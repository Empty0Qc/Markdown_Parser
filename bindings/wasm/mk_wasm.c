/* mk_wasm.c — Emscripten C glue layer for mk_parser (M8a)
 *
 * Strategy β: event-streaming over callbacks.
 * Each delta event is serialized and forwarded to a JavaScript callback
 * registered via mk_wasm_set_callbacks().
 *
 * Node attribute access uses the public getter API (include/mk_parser.h).
 * All functions exported with EMSCRIPTEN_KEEPALIVE / cwrap.
 *
 * Build with:
 *   emcmake cmake -B build/wasm -DMK_BUILD_TESTS=OFF
 *   cmake --build build/wasm
 * Produces: build/wasm/mk_parser.js + mk_parser.wasm
 */

#include "../../include/mk_parser.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  define MK_EXPORT EMSCRIPTEN_KEEPALIVE
#else
/* Allow the file to be compiled natively for inspection/testing */
#  define MK_EXPORT
#  define EM_ASM_INT(code, ...) 0
#  define EM_ASM(code, ...) ((void)0)
#endif

/* ── JS callback bridge ───────────────────────────────────────────────────── */
/*
 * JavaScript sets up callbacks via mk_wasm_set_callbacks().
 * The C side calls back into JS using EM_ASM for each event.
 *
 * Event integer codes:
 *   0 = NODE_OPEN
 *   1 = NODE_CLOSE
 *   2 = TEXT
 *   3 = NODE_MODIFY
 */

/* Per-parser user context: holds the JS-side callback function indices */
typedef struct WasmCtx {
    MkArena  *arena;
    MkParser *parser;
    /* JS function table indices (set by mk_wasm_set_callbacks) */
    int on_node_open;   /* JS fn: (node_ptr, node_type) */
    int on_node_close;  /* JS fn: (node_ptr, node_type) */
    int on_text;        /* JS fn: (node_ptr, text_ptr, text_len) */
    int on_node_modify; /* JS fn: (node_ptr, node_type) */
} WasmCtx;

/* Global single-parser context (extend to array for multi-instance) */
static WasmCtx g_ctx;

/* ── Push callback implementations ───────────────────────────────────────── */

static void wasm_on_open(void *ud, MkNode *n) {
    (void)ud;
#ifdef __EMSCRIPTEN__
    EM_ASM({ Module._wasm_on_event(0, $0, $1); },
           (int)(intptr_t)n, (int)n->type);
#else
    (void)n;
#endif
}

static void wasm_on_close(void *ud, MkNode *n) {
    (void)ud;
#ifdef __EMSCRIPTEN__
    EM_ASM({ Module._wasm_on_event(1, $0, $1); },
           (int)(intptr_t)n, (int)n->type);
#else
    (void)n;
#endif
}

static void wasm_on_text(void *ud, MkNode *n,
                         const char *text, size_t len) {
    (void)ud;
#ifdef __EMSCRIPTEN__
    EM_ASM({ Module._wasm_on_event(2, $0, $1, $2); },
           (int)(intptr_t)n, (int)(intptr_t)text, (int)len);
#else
    (void)n; (void)text; (void)len;
#endif
}

static void wasm_on_modify(void *ud, MkNode *n) {
    (void)ud;
#ifdef __EMSCRIPTEN__
    EM_ASM({ Module._wasm_on_event(3, $0, $1); },
           (int)(intptr_t)n, (int)n->type);
#else
    (void)n;
#endif
}

/* ── Public WASM API ──────────────────────────────────────────────────────── */

/* Create a new parser instance. Returns opaque handle (pointer as int). */
MK_EXPORT int mk_wasm_create(void) {
    MkArena *arena = mk_arena_new();
    if (!arena) return 0;

    MkCallbacks cbs = {
        .user_data      = NULL,
        .on_node_open   = wasm_on_open,
        .on_node_close  = wasm_on_close,
        .on_text        = wasm_on_text,
        .on_node_modify = wasm_on_modify,
    };
    MkParser *parser = mk_parser_new(arena, &cbs);
    if (!parser) { mk_arena_free(arena); return 0; }

    g_ctx.arena  = arena;
    g_ctx.parser = parser;
    return 1; /* single-instance for now */
}

/* Feed a chunk of markdown text. data = pointer to WASM linear memory. */
MK_EXPORT int mk_wasm_feed(const char *data, int len) {
    if (!g_ctx.parser || !data || len <= 0) return -1;
    return mk_feed(g_ctx.parser, data, (size_t)len);
}

/* Signal end of stream. */
MK_EXPORT int mk_wasm_finish(void) {
    if (!g_ctx.parser) return -1;
    return mk_finish(g_ctx.parser);
}

/* Destroy the parser and free all memory. */
MK_EXPORT void mk_wasm_destroy(void) {
    if (g_ctx.parser) { mk_parser_free(g_ctx.parser); g_ctx.parser = NULL; }
    if (g_ctx.arena)  { mk_arena_free(g_ctx.arena);   g_ctx.arena  = NULL; }
}

/* ── Node attribute getters (used from JS via ccall/cwrap) ───────────────── */

MK_EXPORT int         mk_wasm_node_type       (int ptr) { return (int)((MkNode*)(intptr_t)ptr)->type; }
MK_EXPORT int         mk_wasm_heading_level   (int ptr) { return mk_node_heading_level   ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_code_fenced     (int ptr) { return mk_node_code_fenced     ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_list_ordered    (int ptr) { return mk_node_list_ordered    ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_list_start      (int ptr) { return mk_node_list_start      ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_table_col_count (int ptr) { return (int)mk_node_table_col_count((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_table_col_align (int ptr, int col) { return (int)mk_node_table_col_align((MkNode*)(intptr_t)ptr, (size_t)col); }
MK_EXPORT int         mk_wasm_cell_align      (int ptr) { return (int)mk_node_table_cell_align    ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_cell_col_index  (int ptr) { return (int)mk_node_table_cell_col_index((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_task_checked    (int ptr) { return mk_node_task_list_item_checked    ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_list_item_task  (int ptr) { return (int)mk_node_list_item_task_state ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int         mk_wasm_autolink_email  (int ptr) { return mk_node_autolink_is_email         ((MkNode*)(intptr_t)ptr); }

/* String getters: return pointer to arena-owned null-terminated string.
 * JS side uses UTF8ToString(ptr) to convert. Returns 0 if not applicable. */
MK_EXPORT int mk_wasm_code_lang    (int ptr) { return (int)(intptr_t)mk_node_code_lang      ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_link_href    (int ptr) { return (int)(intptr_t)mk_node_link_href       ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_link_title   (int ptr) { return (int)(intptr_t)mk_node_link_title      ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_image_src    (int ptr) { return (int)(intptr_t)mk_node_image_src       ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_image_alt    (int ptr) { return (int)(intptr_t)mk_node_image_alt       ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_image_title  (int ptr) { return (int)(intptr_t)mk_node_image_title     ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_autolink_url (int ptr) { return (int)(intptr_t)mk_node_autolink_url    ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_html_raw     (int ptr) { return (int)(intptr_t)mk_node_html_block_raw  ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_html_inline_raw(int ptr){ return (int)(intptr_t)mk_node_html_inline_raw((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_text_content (int ptr) { return (int)(intptr_t)mk_node_text_content    ((MkNode*)(intptr_t)ptr); }
MK_EXPORT int mk_wasm_inline_code  (int ptr) { return (int)(intptr_t)mk_node_inline_code_text((MkNode*)(intptr_t)ptr); }
