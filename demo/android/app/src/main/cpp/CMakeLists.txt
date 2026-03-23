# CMakeLists.txt — Android native build for mk-parser demo
#
# Source files are vendored into cpp/mk_parser/ so the build never needs to
# reach outside the project tree (avoids iCloud Drive permission issues).

cmake_minimum_required(VERSION 3.22)
project(mk_parser_demo C)

set(MK_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/mk_parser")

# ── mk_parser sources ─────────────────────────────────────────────────────────

set(MK_CORE_SOURCES
    ${MK_LOCAL}/src/arena.c
    ${MK_LOCAL}/src/ast.c
    ${MK_LOCAL}/src/parser.c
    ${MK_LOCAL}/src/block.c
    ${MK_LOCAL}/src/inline_parser.c
    ${MK_LOCAL}/src/plugin.c
    ${MK_LOCAL}/src/getters.c
)

set(MK_JNI_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/mk_jni.c)

# ── Shared library ─────────────────────────────────────────────────────────────

add_library(mk_parser_jni SHARED
    ${MK_CORE_SOURCES}
    ${MK_JNI_SOURCE}
)

target_include_directories(mk_parser_jni PRIVATE
    ${MK_LOCAL}/include
    ${MK_LOCAL}/src
)

find_library(log-lib log)
target_link_libraries(mk_parser_jni ${log-lib})

target_compile_options(mk_parser_jni PRIVATE
    -O2
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)
target_link_options(mk_parser_jni PRIVATE -Wl,--gc-sections)
