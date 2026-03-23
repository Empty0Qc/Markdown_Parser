/* ast.h — AST node type definitions and tree operations (M3)
 *
 * All specialized node structs follow the same pattern:
 *   - MkNode base is always the FIRST field (enables safe cast to/from MkNode*)
 *   - Extra fields follow
 *   - Strings point into the arena (never freed individually)
 */
#ifndef MK_AST_H
#define MK_AST_H

#include "../include/mk_parser.h"
#include "arena.h"

/* MkAlign and MkTaskState are defined in include/mk_parser.h (public API) */

/* ════════════════════════════════════════════════════════════════════════════
 * Block node types
 * ════════════════════════════════════════════════════════════════════════════ */

/* MK_NODE_DOCUMENT — no extra fields, use MkNode* directly */

typedef struct MkHeadingNode {
    MkNode base;
    int    level;           /* 1–6 */
} MkHeadingNode;

/* MK_NODE_PARAGRAPH — no extra fields */

typedef struct MkCodeBlockNode {
    MkNode      base;
    const char *lang;       /* language tag, arena-owned, may be NULL */
    size_t      lang_len;
    int         fenced;     /* 1 = fenced (```), 0 = indented */
} MkCodeBlockNode;

/* MK_NODE_BLOCK_QUOTE — no extra fields */

typedef struct MkListNode {
    MkNode base;
    int    ordered;         /* 0 = bullet, 1 = ordered */
    int    start;           /* starting number for ordered lists (usually 1) */
    /* MK_NODE_FLAG_TIGHT in base.flags when no blank lines between items */
} MkListNode;

typedef struct MkListItemNode {
    MkNode      base;
    MkTaskState task_state; /* MK_TASK_NONE if not a GFM task list item */
} MkListItemNode;

/* MK_NODE_THEMATIC_BREAK — no extra fields */

typedef struct MkHtmlBlockNode {
    MkNode      base;
    int         html_type;  /* CommonMark HTML block type 1–6 */
    const char *raw;        /* raw HTML content, arena-owned */
    size_t      raw_len;
} MkHtmlBlockNode;

typedef struct MkTableNode {
    MkNode   base;
    size_t   col_count;
    MkAlign *col_aligns;    /* array of col_count alignments, arena-owned */
} MkTableNode;

/* MK_NODE_TABLE_HEAD — no extra fields */
/* MK_NODE_TABLE_ROW  — no extra fields */

typedef struct MkTableCellNode {
    MkNode  base;
    MkAlign align;
    size_t  col_index;
} MkTableCellNode;

/* ════════════════════════════════════════════════════════════════════════════
 * Inline node types
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct MkTextNode {
    MkNode      base;
    const char *text;       /* arena-owned */
    size_t      text_len;
} MkTextNode;

/* MK_NODE_SOFT_BREAK — no extra fields */
/* MK_NODE_HARD_BREAK — no extra fields */
/* MK_NODE_EMPHASIS   — no extra fields (children are the content) */
/* MK_NODE_STRONG     — no extra fields */
/* MK_NODE_STRIKETHROUGH — no extra fields */

typedef struct MkInlineCodeNode {
    MkNode      base;
    const char *text;       /* arena-owned */
    size_t      text_len;
} MkInlineCodeNode;

typedef struct MkLinkNode {
    MkNode      base;
    const char *href;       /* arena-owned, never NULL */
    size_t      href_len;
    const char *title;      /* arena-owned, may be NULL */
    size_t      title_len;
} MkLinkNode;

typedef struct MkImageNode {
    MkNode      base;
    const char *src;        /* arena-owned */
    size_t      src_len;
    const char *alt;        /* arena-owned, may be NULL */
    size_t      alt_len;
    const char *title;      /* arena-owned, may be NULL */
    size_t      title_len;
} MkImageNode;

typedef struct MkAutoLinkNode {
    MkNode      base;
    const char *url;        /* arena-owned */
    size_t      url_len;
    int         is_email;   /* 1 if mailto: auto-link */
} MkAutoLinkNode;

typedef struct MkHtmlInlineNode {
    MkNode      base;
    const char *raw;        /* arena-owned */
    size_t      raw_len;
} MkHtmlInlineNode;

typedef struct MkTaskListItemNode {
    MkNode      base;
    int         checked;    /* 1 = [x], 0 = [ ] */
} MkTaskListItemNode;

/* ════════════════════════════════════════════════════════════════════════════
 * Delta (event emitted to the Pull queue or Push callbacks)
 * ════════════════════════════════════════════════════════════════════════════ */

/* MkDelta is defined in mk_parser.h (public header) */

/* ── Custom node (created by MkParserPlugin) ──────────────────────────────── */

typedef struct MkCustomNode {
    MkNode      base;
    const char *plugin_name;  /* which plugin created this, arena-owned */
    const char *raw;          /* raw source content, arena-owned         */
    size_t      raw_len;
} MkCustomNode;

/* ════════════════════════════════════════════════════════════════════════════
 * Node allocation helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/*
 * Allocate a node of the given C struct type from the arena.
 * use_scratch=1 → scratch arena (pending), 0 → stable arena (complete).
 *
 * Example:
 *   MkHeadingNode *h = MK_NODE_NEW(arena, MkHeadingNode, MK_NODE_HEADING, 1);
 */
#define MK_NODE_NEW(arena, struct_type, node_type, use_scratch)           \
    ((struct_type *)mk_node_alloc(                                        \
        (arena), sizeof(struct_type), (node_type), (use_scratch)))

MkNode *mk_node_alloc(MkArena *arena, size_t size,
                      MkNodeType type, int use_scratch);

/* ════════════════════════════════════════════════════════════════════════════
 * Tree operations
 * ════════════════════════════════════════════════════════════════════════════ */

/* Append child as the last child of parent. */
void mk_node_append_child(MkNode *parent, MkNode *child);

/* Prepend child as the first child of parent. */
void mk_node_prepend_child(MkNode *parent, MkNode *child);

/* Remove node from its parent (does not free memory). */
void mk_node_detach(MkNode *node);

/* ════════════════════════════════════════════════════════════════════════════
 * Traversal
 * ════════════════════════════════════════════════════════════════════════════ */

typedef enum MkTraversalAction {
    MK_TRAVERSE_CONTINUE = 0, /* visit children, then siblings    */
    MK_TRAVERSE_SKIP,         /* skip children, continue siblings */
    MK_TRAVERSE_STOP,         /* halt entire traversal            */
} MkTraversalAction;

/*
 * Depth-first traversal.
 * callback is called twice per node: entering=1 (pre-order), entering=0 (post-order).
 * Return MK_TRAVERSE_SKIP on entering=1 to skip the node's children.
 */
typedef MkTraversalAction (*MkTraverseFn)(MkNode *node, int entering,
                                          void *user_data);

void mk_node_traverse(MkNode *root, MkTraverseFn fn, void *user_data);

/* ════════════════════════════════════════════════════════════════════════════
 * Delta helpers
 * ════════════════════════════════════════════════════════════════════════════ */

MkDelta *mk_delta_new(MkArena *arena, MkDeltaType type, MkNode *node);

MkDelta *mk_delta_new_text(MkArena *arena, MkNode *node,
                           const char *text, size_t len);

/* ════════════════════════════════════════════════════════════════════════════
 * Debug utilities
 * ════════════════════════════════════════════════════════════════════════════ */

const char *mk_node_type_name(MkNodeType type);

#endif /* MK_AST_H */
