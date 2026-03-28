# Android library CMakeLists.txt
# Placed at bindings/android/lib/ so MK_ROOT resolves to the repo root.

cmake_minimum_required(VERSION 3.22)
project(mk_parser_android C)

set(MK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

set(MK_CORE_SOURCES
    ${MK_ROOT}/src/arena.c
    ${MK_ROOT}/src/ast.c
    ${MK_ROOT}/src/parser.c
    ${MK_ROOT}/src/block.c
    ${MK_ROOT}/src/inline_parser.c
    ${MK_ROOT}/src/plugin.c
    ${MK_ROOT}/src/getters.c
)

add_library(mk_parser_jni SHARED
    ${MK_CORE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mk_jni.c
)

target_include_directories(mk_parser_jni PRIVATE
    ${MK_ROOT}/include
    ${MK_ROOT}/src
)

find_library(log-lib log)
target_link_libraries(mk_parser_jni ${log-lib})

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
