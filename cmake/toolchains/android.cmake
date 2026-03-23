# Android NDK toolchain
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/android.cmake \
#         -DANDROID_ABI=arm64-v8a \
#         -DANDROID_PLATFORM=android-21 \
#         -DANDROID_NDK=$ANDROID_NDK ...
#
# Supported ABIs: arm64-v8a | armeabi-v7a | x86_64

if(NOT DEFINED ANDROID_NDK)
    if(DEFINED ENV{ANDROID_NDK})
        set(ANDROID_NDK $ENV{ANDROID_NDK})
    elseif(DEFINED ENV{ANDROID_NDK_HOME})
        set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
    else()
        message(FATAL_ERROR "ANDROID_NDK not set. Export ANDROID_NDK env variable.")
    endif()
endif()

# Delegate to NDK's own toolchain file
include("${ANDROID_NDK}/build/cmake/android.toolchain.cmake")

if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()

if(NOT DEFINED ANDROID_PLATFORM)
    set(ANDROID_PLATFORM "android-21")
endif()

# Pure C — no STL needed
set(ANDROID_STL "none")

message(STATUS "Android build: ABI=${ANDROID_ABI}, platform=${ANDROID_PLATFORM}")
