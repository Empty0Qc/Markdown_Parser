# iOS toolchain (requires CMake 3.14+ with native iOS support)
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ios.cmake \
#         -DPLATFORM=OS64 ...           # physical device (arm64)
#   cmake ... -DPLATFORM=SIMULATORARM64 # Apple Silicon simulator
#   cmake ... -DPLATFORM=SIMULATOR64    # Intel simulator
#
# Packaging: use build.sh ios to produce xcframework

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum iOS version")

if(NOT DEFINED PLATFORM)
    set(PLATFORM "OS64")
endif()

if(PLATFORM STREQUAL "OS64")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_SYSROOT        "iphoneos")
elseif(PLATFORM STREQUAL "SIMULATORARM64")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_SYSROOT        "iphonesimulator")
elseif(PLATFORM STREQUAL "SIMULATOR64")
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    set(CMAKE_OSX_SYSROOT        "iphonesimulator")
else()
    message(FATAL_ERROR "Unknown PLATFORM=${PLATFORM}. Use OS64 | SIMULATORARM64 | SIMULATOR64")
endif()

message(STATUS "iOS build: PLATFORM=${PLATFORM}, ARCH=${CMAKE_OSX_ARCHITECTURES}")

# Disable test execution for cross-compiled target
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
