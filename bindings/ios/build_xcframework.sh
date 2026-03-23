#!/usr/bin/env bash
# build_xcframework.sh — Build MkParser.xcframework (M8d)
#
# Produces a universal xcframework containing:
#   - arm64 device slice   (iphoneos)
#   - arm64 simulator slice (iphonesimulator)
#   - x86_64 simulator slice (iphonesimulator)
#
# Usage:
#   bash bindings/ios/build_xcframework.sh [Release|Debug]
#
# Output:
#   build/ios/MkParser.xcframework
#
# Requirements:
#   - Xcode Command Line Tools installed
#   - cmake/toolchains/ios.cmake present (set MIN_IOS_VERSION via cmake var)

set -euo pipefail

CONFIG="${1:-Release}"
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/ios"
XCF_DIR="$BUILD_DIR/MkParser.xcframework"

CMAKE="$(command -v cmake)"
TOOLCHAIN="$REPO_ROOT/cmake/toolchains/ios.cmake"
SOURCES=(
    "$REPO_ROOT/src/arena.c"
    "$REPO_ROOT/src/ast.c"
    "$REPO_ROOT/src/parser.c"
    "$REPO_ROOT/src/block.c"
    "$REPO_ROOT/src/inline_parser.c"
    "$REPO_ROOT/src/plugin.c"
    "$REPO_ROOT/src/getters.c"
)

# Map CMake PLATFORM names → xcodebuild -sdk values
declare -A PLATFORMS=(
    ["OS64"]="iphoneos"
    ["SIMULATORARM64"]="iphonesimulator"
    ["SIMULATOR64"]="iphonesimulator"
)

declare -A ARCHS=(
    ["OS64"]="arm64"
    ["SIMULATORARM64"]="arm64"
    ["SIMULATOR64"]="x86_64"
)

SLICES=()

for PLATFORM in OS64 SIMULATORARM64 SIMULATOR64; do
    SDK="${PLATFORMS[$PLATFORM]}"
    ARCH="${ARCHS[$PLATFORM]}"
    SLICE_DIR="$BUILD_DIR/slice_${PLATFORM}"

    echo "=== Building $PLATFORM ($ARCH / $SDK) ==="
    "$CMAKE" -S "$REPO_ROOT/bindings/ios" \
             -B "$SLICE_DIR" \
             -G Xcode \
             -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
             -DPLATFORM="$PLATFORM" \
             -DCMAKE_BUILD_TYPE="$CONFIG" \
             -DMK_ROOT="$REPO_ROOT" \
             -DCMAKE_INSTALL_PREFIX="$SLICE_DIR/install"

    "$CMAKE" --build "$SLICE_DIR" --config "$CONFIG"
    "$CMAKE" --install "$SLICE_DIR" --config "$CONFIG"

    SLICES+=(-library "$SLICE_DIR/install/lib/libmk_parser_ios.a"
             -headers "$SLICE_DIR/install/include")
done

# ── Merge simulator arm64 + x86_64 into fat lib ──────────────────────────────
echo "=== Merging simulator slices ==="
SIM_FAT="$BUILD_DIR/sim_fat"
mkdir -p "$SIM_FAT/lib"
lipo -create \
    "$BUILD_DIR/slice_SIMULATORARM64/install/lib/libmk_parser_ios.a" \
    "$BUILD_DIR/slice_SIMULATOR64/install/lib/libmk_parser_ios.a" \
    -output "$SIM_FAT/lib/libmk_parser_ios.a"
cp -R "$BUILD_DIR/slice_SIMULATORARM64/install/include" "$SIM_FAT/"

# ── Create xcframework ────────────────────────────────────────────────────────
echo "=== Creating xcframework ==="
rm -rf "$XCF_DIR"
xcodebuild -create-xcframework \
    -library "$BUILD_DIR/slice_OS64/install/lib/libmk_parser_ios.a" \
    -headers "$BUILD_DIR/slice_OS64/install/include" \
    -library "$SIM_FAT/lib/libmk_parser_ios.a" \
    -headers "$SIM_FAT/include" \
    -output "$XCF_DIR"

echo ""
echo "✅  MkParser.xcframework built:"
echo "    $XCF_DIR"
