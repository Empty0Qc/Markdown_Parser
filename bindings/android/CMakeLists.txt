# M8c — Android JNI binding
#
# Build with the Android NDK CMake toolchain:
#   cmake -B build/android \
#         -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
#         -DANDROID_ABI=arm64-v8a \
#         -DANDROID_PLATFORM=android-21 \
#         -DMK_BUILD_ANDROID=ON
#
# Or via build.sh:  ./build.sh android

cmake_minimum_required(VERSION 3.22)
project(mk_parser_android C)

# ── Sources ───────────────────────────────────────────────────────────────────

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

set(MK_JNI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/mk_jni.c
)

# ── Shared library target ─────────────────────────────────────────────────────

add_library(mk_parser_jni SHARED
    ${MK_CORE_SOURCES}
    ${MK_JNI_SOURCES}
)

target_include_directories(mk_parser_jni PRIVATE
    ${MK_ROOT}/include
    ${MK_ROOT}/src
)

# Link Android log library (available in NDK sysroot)
find_library(log-lib log)
target_link_libraries(mk_parser_jni ${log-lib})

# Strip release builds for minimal .so size
target_compile_options(mk_parser_jni PRIVATE
    -O2
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)
target_link_options(mk_parser_jni PRIVATE
    -Wl,--gc-sections
    -Wl,-s
)

# ── Multi-ABI install layout ──────────────────────────────────────────────────
# When driven by Gradle via externalNativeBuild, each ABI is a separate
# CMake invocation; the .so lands in the correct jniLibs/<ABI>/ directory
# automatically.  The install() rule below is for manual packaging only.

install(TARGETS mk_parser_jni
    LIBRARY DESTINATION "jniLibs/${ANDROID_ABI}"
)

install(FILES ${MK_ROOT}/include/mk_parser.h
    DESTINATION include
)
