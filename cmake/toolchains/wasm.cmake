# Emscripten toolchain for WebAssembly
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/wasm.cmake ...
# Requires: EMSDK environment variable set, or emcmake wrapper

set(CMAKE_SYSTEM_NAME      Emscripten)
set(CMAKE_SYSTEM_VERSION   1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# Emscripten compilers (resolved via PATH when using emcmake)
find_program(EMCC   emcc   REQUIRED)
find_program(EMCXX  em++   REQUIRED)

set(CMAKE_C_COMPILER   ${EMCC})
set(CMAKE_CXX_COMPILER ${EMCXX})

set(CMAKE_EXECUTABLE_SUFFIX    ".js")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

# Tell CMake not to try to run test executables
set(CMAKE_CROSSCOMPILING_EMULATOR "node")

# Common Emscripten flags (override per-target as needed)
set(MK_WASM_FLAGS
    "-s MODULARIZE=1"
    "-s EXPORT_NAME=MkParserModule"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue']"
    "--no-entry"
)
