#ifndef MK_PARSER_H
#define MK_PARSER_H

/*
 * mk_parser.h — Public API
 *
 * Incremental Markdown parser for AI streaming scenarios.
 * Pure C11, platform-agnostic, no global state.
 *
 * Status: STUB — filled in during M2–M7.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────────────────── */

#define MK_VERSION_MAJOR 0
#define MK_VERSION_MINOR 1
#define MK_VERSION_PATCH 0

/* ── Forward declarations ─────────────────────────────────────────────────── */

typedef struct MkArena   MkArena;
typedef struct MkNode    MkNode;
typedef struct MkParser  MkParser;
typedef struct MkDelta   MkDelta;

/* ── Table cell alignment (also used by MkTableNode) ─────────────────────── */

typedef enum MkAlign {
    MK_ALIGN_NONE   = 0,
    MK_ALIGN_LEFT,
    MK_ALIGN_CENTER,
    MK_ALIGN_RIGHT,
} MkAlign;

/* ── Task list state ──────────────────────────────────────────────────────── */

typedef enum MkTaskState {
    MK_TASK_NONE      = 0,  /* not a task list item */
    MK_TASK_UNCHECKED,      /* [ ]                  */
    MK_TASK_CHECKED,        /* [x] or [X]           */
} MkTaskState;

/* ── Node types ───────────────────────────────────────────────────────────── */

typedef enum MkNodeType {
    /* Block */
    MK_NODE_DOCUMENT = 0,
    MK_NODE_HEADING,
    MK_NODE_PARAGRAPH,
    MK_NODE_CODE_BLOCK,
    MK_NODE_BLOCK_QUOTE,
    MK_NODE_LIST,
    MK_NODE_LIST_ITEM,
    MK_NODE_THEMATIC_BREAK,
    MK_NODE_HTML_BLOCK,
    MK_NODE_TABLE,
    MK_NODE_TABLE_HEAD,
    MK_NODE_TABLE_ROW,
    MK_NODE_TABLE_CELL,
    /* Inline */
    MK_NODE_TEXT,
    MK_NODE_SOFT_BREAK,
    MK_NODE_HARD_BREAK,
    MK_NODE_EMPHASIS,
    MK_NODE_STRONG,
    MK_NODE_STRIKETHROUGH,
    MK_NODE_INLINE_CODE,
    MK_NODE_LINK,
    MK_NODE_IMAGE,
    MK_NODE_AUTO_LINK,
    MK_NODE_HTML_INLINE,
    MK_NODE_TASK_LIST_ITEM,
    /* Custom nodes start here */
    MK_NODE_CUSTOM = 0x1000,
} MkNodeType;

/* ── Delta types ──────────────────────────────────────────────────────────── */

typedef enum MkDeltaType {
    MK_DELTA_NODE_OPEN,    /* a node was opened (streaming, may still grow) */
    MK_DELTA_NODE_CLOSE,   /* a node is complete                            */
    MK_DELTA_TEXT,         /* text content appended to current node         */
    MK_DELTA_NODE_MODIFY,  /* a pending node's attributes changed           */
} MkDeltaType;

/* ── Delta event (Pull API) ───────────────────────────────────────────────── */

struct MkDelta {
    MkDeltaType  type;
    MkNode      *node;
    /* MK_DELTA_TEXT only: text slice (arena-owned, valid until mk_arena_free) */
    const char  *text;
    size_t       text_len;
    /* intrusive singly-linked list (internal use only) */
    MkDelta     *next;
};

/* ── Base node (M3 fills in full definition) ──────────────────────────────── */

/* Node flags */
#define MK_NODE_FLAG_PENDING  (1u << 0)  /* node is still being built     */
#define MK_NODE_FLAG_TIGHT    (1u << 1)  /* tight list (no blank lines)   */

struct MkNode {
    MkNodeType  type;
    uint32_t    flags;
    MkNode     *parent;
    MkNode     *first_child;
    MkNode     *last_child;   /* O(1) append during streaming  */
    MkNode     *next_sibling;
    MkNode     *prev_sibling;
    size_t      src_begin;    /* byte offset in original source */
    size_t      src_end;
    /* type-specific data follows in specialized structs */
};

/* ── Arena (M2 fills in full definition) ─────────────────────────────────── */

typedef void *(*MkAllocFn)(void *ctx, size_t size);
typedef void  (*MkFreeFn) (void *ctx, void *ptr);

/* ── Error codes ──────────────────────────────────────────────────────────── */

typedef enum MkErrorCode {
    MK_ERR_LINE_TOO_LONG = 1,  /* input line exceeded MK_BLOCK_LINE_MAX bytes */
} MkErrorCode;

/* ── Push callbacks ───────────────────────────────────────────────────────── */

typedef struct MkCallbacks {
    void *user_data;
    void (*on_node_open)  (void *user_data, MkNode *node);
    void (*on_node_close) (void *user_data, MkNode *node);
    void (*on_text)       (void *user_data, MkNode *node,
                           const char *text, size_t len);
    void (*on_node_modify)(void *user_data, MkNode *node);
    /* Optional: called when a non-fatal parse error occurs.
     * msg is a static string, valid only during the callback. */
    void (*on_error)      (void *user_data, MkErrorCode code, const char *msg);
} MkCallbacks;

/* ── Plugin vtables (M7 fills in full definition) ─────────────────────────── */

typedef struct MkParserPlugin {
    const char *name;
    /* Characters that may start an inline match (fast-path gate).
     * E.g. "$" for a math plugin.  NULL means never called for inline. */
    const char *inline_triggers;
    size_t (*try_block) (MkParser *, MkArena *,
                         const char *src, size_t len, MkNode **out);
    size_t (*try_inline)(MkParser *, MkArena *,
                         const char *src, size_t len, MkNode **out);
} MkParserPlugin;

typedef struct MkTransformPlugin {
    const char *name;
    void (*on_node_complete)(MkNode *, MkArena *);
} MkTransformPlugin;

/* ── Public API (M2–M7 implement these) ──────────────────────────────────── */

/* Arena */
MkArena  *mk_arena_new(void);
MkArena  *mk_arena_new_custom(MkAllocFn alloc, MkFreeFn free, void *ctx);
void      mk_arena_free(MkArena *arena);
void      mk_arena_reset_scratch(MkArena *arena);

/* Parser lifecycle */
MkParser *mk_parser_new(MkArena *arena, const MkCallbacks *callbacks);
void      mk_parser_free(MkParser *parser);

/* Feed data (Push API) */
int       mk_feed(MkParser *parser, const char *data, size_t len);
int       mk_finish(MkParser *parser);   /* signal end of stream */

/* Pull API */
MkDelta  *mk_pull_delta(MkParser *parser);   /* NULL = no pending delta */
void      mk_delta_free(MkDelta *delta);     /* no-op; arena owns memory */
void      mk_drain_deltas(MkParser *parser); /* discard all pending deltas */

/* Plugin registration */
int       mk_register_parser_plugin   (MkParser *, const MkParserPlugin *);
int       mk_register_transform_plugin(MkParser *, const MkTransformPlugin *);

/* Utilities */
const char *mk_node_type_name(MkNodeType type);
MkNode     *mk_get_root(MkParser *parser);

/* ── Node attribute getters ───────────────────────────────────────────────── */
/* All getters return safe defaults (0 / NULL) when node is NULL or wrong type */

/* MK_NODE_HEADING */
int         mk_node_heading_level(const MkNode *node);

/* MK_NODE_CODE_BLOCK */
const char *mk_node_code_lang    (const MkNode *node);
size_t      mk_node_code_lang_len(const MkNode *node);
int         mk_node_code_fenced  (const MkNode *node);

/* MK_NODE_LIST */
int         mk_node_list_ordered (const MkNode *node);
int         mk_node_list_start   (const MkNode *node);

/* MK_NODE_LIST_ITEM */
MkTaskState mk_node_list_item_task_state(const MkNode *node);

/* MK_NODE_HTML_BLOCK */
int         mk_node_html_block_type(const MkNode *node);
const char *mk_node_html_block_raw (const MkNode *node);
size_t      mk_node_html_block_raw_len(const MkNode *node);

/* MK_NODE_TABLE */
size_t      mk_node_table_col_count(const MkNode *node);
MkAlign     mk_node_table_col_align(const MkNode *node, size_t col);

/* MK_NODE_TABLE_CELL */
MkAlign     mk_node_table_cell_align    (const MkNode *node);
size_t      mk_node_table_cell_col_index(const MkNode *node);

/* MK_NODE_TEXT */
const char *mk_node_text_content    (const MkNode *node);
size_t      mk_node_text_content_len(const MkNode *node);

/* MK_NODE_INLINE_CODE */
const char *mk_node_inline_code_text    (const MkNode *node);
size_t      mk_node_inline_code_text_len(const MkNode *node);

/* MK_NODE_LINK */
const char *mk_node_link_href     (const MkNode *node);
size_t      mk_node_link_href_len (const MkNode *node);
const char *mk_node_link_title    (const MkNode *node);
size_t      mk_node_link_title_len(const MkNode *node);

/* MK_NODE_IMAGE */
const char *mk_node_image_src      (const MkNode *node);
size_t      mk_node_image_src_len  (const MkNode *node);
const char *mk_node_image_alt      (const MkNode *node);
size_t      mk_node_image_alt_len  (const MkNode *node);
const char *mk_node_image_title    (const MkNode *node);
size_t      mk_node_image_title_len(const MkNode *node);

/* MK_NODE_AUTO_LINK */
const char *mk_node_autolink_url     (const MkNode *node);
size_t      mk_node_autolink_url_len (const MkNode *node);
int         mk_node_autolink_is_email(const MkNode *node);

/* MK_NODE_HTML_INLINE */
const char *mk_node_html_inline_raw    (const MkNode *node);
size_t      mk_node_html_inline_raw_len(const MkNode *node);

/* MK_NODE_TASK_LIST_ITEM */
int         mk_node_task_list_item_checked(const MkNode *node);

/* MK_NODE_CUSTOM */
const char *mk_node_custom_plugin_name(const MkNode *node);
const char *mk_node_custom_raw        (const MkNode *node);
size_t      mk_node_custom_raw_len    (const MkNode *node);

#ifdef __cplusplus
}
#endif

#endif /* MK_PARSER_H */
