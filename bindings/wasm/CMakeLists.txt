# M8a — WASM binding (Emscripten)

cmake_minimum_required(VERSION 3.20)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "bindings/wasm must be built with emcmake")
endif()

message(STATUS "Configuring WASM binding")

# ── Exported C functions ───────────────────────────────────────────────────────

set(WASM_EXPORTS
    _mk_wasm_create
    _mk_wasm_feed
    _mk_wasm_finish
    _mk_wasm_destroy
    # node attribute getters
    _mk_wasm_node_type
    _mk_wasm_heading_level
    _mk_wasm_code_lang
    _mk_wasm_code_fenced
    _mk_wasm_list_ordered
    _mk_wasm_list_start
    _mk_wasm_table_col_count
    _mk_wasm_table_col_align
    _mk_wasm_cell_align
    _mk_wasm_cell_col_index
    _mk_wasm_task_checked
    _mk_wasm_list_item_task
    _mk_wasm_autolink_email
    _mk_wasm_link_href
    _mk_wasm_link_title
    _mk_wasm_image_src
    _mk_wasm_image_alt
    _mk_wasm_image_title
    _mk_wasm_autolink_url
    _mk_wasm_html_raw
    _mk_wasm_html_inline_raw
    _mk_wasm_text_content
    _mk_wasm_inline_code
    # malloc/free for JS-side buffer management
    _malloc
    _free
)
list(JOIN WASM_EXPORTS "," WASM_EXPORTS_STR)

# ── WASM executable target ─────────────────────────────────────────────────────

add_executable(mk_parser_wasm
    mk_wasm.c
)

target_include_directories(mk_parser_wasm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

target_link_libraries(mk_parser_wasm PRIVATE mk_parser_core)

target_link_options(mk_parser_wasm PRIVATE
    # Output as ES module with JS glue
    "-sEXPORTED_FUNCTIONS=[${WASM_EXPORTS_STR}]"
    "-sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,UTF8ToString,HEAPU8,malloc,free]"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createMkParser"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=2097152"  # 2 MB
    "-sENVIRONMENT=web,node"
    "-sFILESYSTEM=0"            # no file system needed
    "-O2"
)

set_target_properties(mk_parser_wasm PROPERTIES
    OUTPUT_NAME "mk_parser"
    SUFFIX      ".js"
)

# Copy the JS API wrapper to the output directory
add_custom_command(TARGET mk_parser_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/mk_wasm_api.js
        ${CMAKE_CURRENT_BINARY_DIR}/mk_wasm_api.js
    COMMENT "Copying mk_wasm_api.js to build output"
)
