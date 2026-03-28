# M11 — Test suite (Owner: shared, scaffolding by GPT)
# See docs/HANDOFF_GPT.md for full requirements.

message(STATUS "Configuring mk_parser test suite")

# ── helper macro ──────────────────────────────────────────────────────────────

macro(mk_add_test target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    target_link_libraries(${target} PRIVATE mk_parser_core)
    add_test(NAME ${target} COMMAND ${target})
endmacro()

# ── existing tests ─────────────────────────────────────────────────────────────

mk_add_test(mk_parser_smoke     smoke.c)
mk_add_test(mk_parser_test_m6   test_m6.c)
mk_add_test(mk_parser_test_m7   test_m7.c)

# ── unit tests (M11) ───────────────────────────────────────────────────────────

mk_add_test(mk_test_arena   unit/test_arena.c)
mk_add_test(mk_test_ast     unit/test_ast.c)
mk_add_test(mk_test_block   unit/test_block.c)
mk_add_test(mk_test_inline  unit/test_inline.c)

# ── integration tests (M11) ───────────────────────────────────────────────────

mk_add_test(mk_test_streaming  integration/test_streaming.c)

# ── benchmark (not a CTest, just a build target) ──────────────────────────────

add_executable(mk_parser_bench_parser bench_parser.c)
target_include_directories(mk_parser_bench_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(mk_parser_bench_parser PRIVATE mk_parser_core)

# ── CommonMark spec compliance test ───────────────────────────────────────────

add_executable(mk_test_spec
    spec/test_spec.c
    spec/mk_html.c
)
target_include_directories(mk_test_spec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/spec
)
target_link_libraries(mk_test_spec PRIVATE mk_parser_core)
# Pass spec.json path so the binary can find it regardless of CWD
target_compile_definitions(mk_test_spec PRIVATE
    MK_SPEC_JSON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/spec/spec.json"
)
add_test(NAME mk_test_spec
    COMMAND mk_test_spec "${CMAKE_CURRENT_SOURCE_DIR}/spec/spec.json"
)
