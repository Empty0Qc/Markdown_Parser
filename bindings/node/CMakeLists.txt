# M8b — Node.js native binding (N-API)
# Builds mk_parser_native.node via node-gyp or CMake.
#
# To build via CMake (requires Node.js headers at ${NODE_INCLUDE_DIR}):
#   cmake -B build/node \
#         -DNODE_INCLUDE_DIR=$(node -e "process.stdout.write(require('path').join(process.execPath,'../../include/node'))") \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/node

cmake_minimum_required(VERSION 3.20)

message(STATUS "Configuring Node.js N-API binding")

# Prefer node-gyp (invoked via npm); this CMake target is a fallback.
find_program(NODE_GYP node-gyp)

if(NODE_GYP)
    message(STATUS "node-gyp found: ${NODE_GYP}  — use `npm run build` in bindings/node/")
else()
    message(STATUS "node-gyp not found; attempting CMake-only build")

    # Locate Node.js include directory
    if(NOT NODE_INCLUDE_DIR)
        find_program(NODE_BIN node)
        if(NODE_BIN)
            execute_process(
                COMMAND ${NODE_BIN} -e
                    "process.stdout.write(require('path').join(process.execPath,'../../include/node'))"
                OUTPUT_VARIABLE NODE_INCLUDE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif()
    endif()

    if(NOT NODE_INCLUDE_DIR OR NOT EXISTS "${NODE_INCLUDE_DIR}/node_api.h")
        message(WARNING "Node.js headers not found — skipping N-API binding build")
    else()
        add_library(mk_parser_native SHARED mk_napi.c)
        set_target_properties(mk_parser_native PROPERTIES
            PREFIX       ""
            SUFFIX       ".node"
            OUTPUT_NAME  "mk_parser_native"
        )
        target_include_directories(mk_parser_native PRIVATE
            ${NODE_INCLUDE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../src
        )
        target_link_libraries(mk_parser_native PRIVATE mk_parser_core)
        target_compile_definitions(mk_parser_native PRIVATE NAPI_VERSION=8)
    endif()
endif()
