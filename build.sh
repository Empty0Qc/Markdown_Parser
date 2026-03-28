#!/usr/bin/env bash
set -euo pipefail

TARGET=${1:-native}
BUILD_TYPE=${BUILD_TYPE:-Release}
ROOT="$(cd "$(dirname "$0")" && pwd)"

usage() {
    echo "Usage: $0 [native|wasm|android|ios|all|bench|npm-pack]"
    echo "  native    — build for current host (default)"
    echo "  wasm      — build for WebAssembly (requires Emscripten)"
    echo "  android   — build for Android arm64-v8a (requires ANDROID_NDK)"
    echo "  ios       — build for iOS device + simulator (requires Xcode)"
    echo "  all       — build all targets"
    echo "  bench     — build native + run throughput benchmark"
    echo "  npm-pack  — build WASM + validate npm package (dry-run)"
    exit 1
}

build_native() {
    echo "▶ Building native..."
    cmake -B "$ROOT/build/native" \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
          -DMK_BUILD_TESTS=ON
    cmake --build "$ROOT/build/native" --parallel
    echo "✓ Native build: build/native/"
}

build_wasm() {
    echo "▶ Building WASM..."
    command -v emcmake >/dev/null || { echo "Error: emcmake not found. Source emsdk_env.sh first."; exit 1; }
    emcmake cmake -B "$ROOT/build/wasm" \
                  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
                  -DMK_BUILD_TESTS=OFF
    cmake --build "$ROOT/build/wasm" --parallel
    echo "✓ WASM build: build/wasm/"
}

build_android() {
    echo "▶ Building Android (arm64-v8a)..."
    : "${ANDROID_NDK:?ANDROID_NDK env variable required}"
    cmake -B "$ROOT/build/android" \
          -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/android.cmake" \
          -DANDROID_ABI=arm64-v8a \
          -DANDROID_PLATFORM=android-21 \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
          -DMK_BUILD_TESTS=OFF
    cmake --build "$ROOT/build/android" --parallel
    echo "✓ Android build: build/android/"
}

build_ios() {
    echo "▶ Building iOS (device + simulators)..."
    command -v xcodebuild >/dev/null || { echo "Error: xcodebuild not found. Install Xcode."; exit 1; }

    # Device (arm64)
    cmake -B "$ROOT/build/ios-device" \
          -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/ios.cmake" \
          -DPLATFORM=OS64 \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    cmake --build "$ROOT/build/ios-device" --parallel

    # Simulator arm64 (Apple Silicon)
    cmake -B "$ROOT/build/ios-sim-arm64" \
          -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/ios.cmake" \
          -DPLATFORM=SIMULATORARM64 \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    cmake --build "$ROOT/build/ios-sim-arm64" --parallel

    echo "✓ iOS builds: build/ios-device/, build/ios-sim-arm64/"
    echo "  Note: xcframework packaging handled by bindings/ios/"
}

bench() {
    echo "▶ Building native (bench)..."
    cmake -B "$ROOT/build/native" \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
          -DMK_BUILD_TESTS=ON
    cmake --build "$ROOT/build/native" --parallel \
          --target mk_parser_bench_parser
    echo "▶ Running benchmark..."
    "$ROOT/build/native/tests/mk_parser_bench_parser"
}

npm_pack() {
    echo "▶ Building WASM for npm pack..."
    build_wasm
    echo "▶ Copying WASM artifacts..."
    cp "$ROOT/build/wasm/mk_parser.js"   "$ROOT/bindings/js/mk_parser.js"
    cp "$ROOT/build/wasm/mk_parser.wasm" "$ROOT/bindings/js/mk_parser.wasm"
    echo "▶ Running npm pack --dry-run..."
    cd "$ROOT/bindings/js" && npm pack --dry-run
}

case "$TARGET" in
    native)   build_native  ;;
    wasm)     build_wasm    ;;
    android)  build_android ;;
    ios)      build_ios     ;;
    bench)    bench         ;;
    npm-pack) npm_pack      ;;
    all)
        build_native
        build_wasm
        build_android
        build_ios
        ;;
    *) usage ;;
esac
