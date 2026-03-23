option(MK_BUILD_TESTS  "Build test suite"    ON)
option(MK_BUILD_DEMO   "Build web demo"      OFF)
option(MK_SHARED       "Build shared library instead of static" OFF)

# Detect platform automatically if not cross-compiling
if(EMSCRIPTEN)
    message(STATUS "mk_parser: target = WASM (Emscripten)")
elseif(ANDROID)
    message(STATUS "mk_parser: target = Android (ABI=${ANDROID_ABI})")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(STATUS "mk_parser: target = iOS")
else()
    message(STATUS "mk_parser: target = Native (${CMAKE_SYSTEM_NAME})")
endif()
