cmake_minimum_required(VERSION 3.20)

project(mk_parser
    VERSION     0.1.0
    LANGUAGES   C
    DESCRIPTION "Incremental Markdown parser for AI streaming scenarios"
)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

include(cmake/options.cmake)

# ── Core library ─────────────────────────────────────────────────────────────

set(MK_CORE_SOURCES
    src/arena.c
    src/ast.c
    src/parser.c
    src/block.c
    src/inline_parser.c
    src/plugin.c
    src/getters.c
)

if(MK_SHARED)
    add_library(mk_parser_core SHARED ${MK_CORE_SOURCES})
else()
    add_library(mk_parser_core STATIC ${MK_CORE_SOURCES})
endif()

target_include_directories(mk_parser_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(mk_parser_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<C_COMPILER_ID:MSVC>:/W4 /WX>
)

# ── Platform bindings ─────────────────────────────────────────────────────────

if(EMSCRIPTEN)
    add_subdirectory(bindings/wasm)
elseif(ANDROID)
    add_subdirectory(bindings/android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_subdirectory(bindings/ios)
else()
    # Native: Node.js napi binding (optional, requires node headers)
    find_program(NODE node)
    if(NODE)
        add_subdirectory(bindings/node)
    endif()
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────

if(MK_BUILD_TESTS
    AND NOT EMSCRIPTEN
    AND NOT ANDROID
    AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_testing()
    add_subdirectory(tests)
endif()
