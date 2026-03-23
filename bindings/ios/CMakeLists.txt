# M8d — iOS xcframework binding
#
# Build with the iOS CMake toolchain:
#   cmake -B build/ios \
#         -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchains/ios.cmake \
#         -DPLATFORM=OS64 \
#         -DMK_BUILD_IOS=ON
#
# Or via build.sh:  ./build.sh ios
#
# The xcframework packaging script packages arm64-device + arm64-simulator
# + x86_64-simulator slices into a single MkParser.xcframework bundle.

cmake_minimum_required(VERSION 3.22)
project(mk_parser_ios C)

set(MK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

set(MK_CORE_SOURCES
    ${MK_ROOT}/src/arena.c
    ${MK_ROOT}/src/ast.c
    ${MK_ROOT}/src/parser.c
    ${MK_ROOT}/src/block.c
    ${MK_ROOT}/src/inline_parser.c
    ${MK_ROOT}/src/plugin.c
    ${MK_ROOT}/src/getters.c
)

# ── Static library (xcframework uses static slices) ──────────────────────────

add_library(mk_parser_ios STATIC ${MK_CORE_SOURCES})

target_include_directories(mk_parser_ios PUBLIC
    ${MK_ROOT}/include
)

target_compile_options(mk_parser_ios PRIVATE
    -O2
    -fvisibility=hidden
)

# Public header install (used when embedding in xcframework)
set_target_properties(mk_parser_ios PROPERTIES
    PUBLIC_HEADER ${MK_ROOT}/include/mk_parser.h
)

install(TARGETS mk_parser_ios
    ARCHIVE       DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
