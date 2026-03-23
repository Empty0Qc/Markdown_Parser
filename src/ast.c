/* ast.c — AST node definitions and tree operations (M3) */

#include "ast.h"

#include <string.h>

/* ── Node allocation ──────────────────────────────────────────────────────── */

MkNode *mk_node_alloc(MkArena *arena, size_t size,
                      MkNodeType type, int use_scratch)
{
    MkNode *node = use_scratch
        ? mk_arena_scratch_alloc(arena, size)
        : mk_arena_stable_alloc(arena, size);
    if (!node) return NULL;

    memset(node, 0, size);
    node->type  = type;
    node->flags = MK_NODE_FLAG_PENDING;  /* cleared on completion */
    return node;
}

/* ── Tree operations ──────────────────────────────────────────────────────── */

void mk_node_append_child(MkNode *parent, MkNode *child) {
    child->parent       = parent;
    child->prev_sibling = parent->last_child;
    child->next_sibling = NULL;

    if (parent->last_child)
        parent->last_child->next_sibling = child;
    else
        parent->first_child = child;

    parent->last_child = child;
}

void mk_node_prepend_child(MkNode *parent, MkNode *child) {
    child->parent       = parent;
    child->prev_sibling = NULL;
    child->next_sibling = parent->first_child;

    if (parent->first_child)
        parent->first_child->prev_sibling = child;
    else
        parent->last_child = child;

    parent->first_child = child;
}

void mk_node_detach(MkNode *node) {
    if (!node->parent) return;

    if (node->prev_sibling)
        node->prev_sibling->next_sibling = node->next_sibling;
    else
        node->parent->first_child = node->next_sibling;

    if (node->next_sibling)
        node->next_sibling->prev_sibling = node->prev_sibling;
    else
        node->parent->last_child = node->prev_sibling;

    node->parent       = NULL;
    node->prev_sibling = NULL;
    node->next_sibling = NULL;
}

/* ── Traversal ────────────────────────────────────────────────────────────── */

void mk_node_traverse(MkNode *root, MkTraverseFn fn, void *user_data) {
    MkNode *node = root;

    while (node) {
        MkTraversalAction action = fn(node, 1 /* entering */, user_data);

        if (action == MK_TRAVERSE_STOP) return;

        if (action == MK_TRAVERSE_CONTINUE && node->first_child) {
            node = node->first_child;
            continue;
        }

        /* Ascend until we find a next sibling */
        while (node) {
            if (node == root) {
                fn(node, 0 /* leaving */, user_data);
                return;
            }
            fn(node, 0 /* leaving */, user_data);
            if (node->next_sibling) {
                node = node->next_sibling;
                break;
            }
            node = node->parent;
        }
    }
}

/* ── Delta helpers ────────────────────────────────────────────────────────── */

MkDelta *mk_delta_new(MkArena *arena, MkDeltaType type, MkNode *node) {
    MkDelta *d = mk_arena_stable_alloc(arena, sizeof(MkDelta));
    if (!d) return NULL;
    memset(d, 0, sizeof(MkDelta));
    d->type = type;
    d->node = node;
    return d;
}

MkDelta *mk_delta_new_text(MkArena *arena, MkNode *node,
                           const char *text, size_t len)
{
    MkDelta *d = mk_delta_new(arena, MK_DELTA_TEXT, node);
    if (!d) return NULL;
    /* Text points into the arena; caller ensures lifetime */
    d->text     = text;
    d->text_len = len;
    return d;
}

/* ── Debug utilities ──────────────────────────────────────────────────────── */

const char *mk_node_type_name(MkNodeType type) {
    switch (type) {
    /* Block */
    case MK_NODE_DOCUMENT:          return "Document";
    case MK_NODE_HEADING:           return "Heading";
    case MK_NODE_PARAGRAPH:         return "Paragraph";
    case MK_NODE_CODE_BLOCK:        return "CodeBlock";
    case MK_NODE_BLOCK_QUOTE:       return "BlockQuote";
    case MK_NODE_LIST:              return "List";
    case MK_NODE_LIST_ITEM:         return "ListItem";
    case MK_NODE_THEMATIC_BREAK:    return "ThematicBreak";
    case MK_NODE_HTML_BLOCK:        return "HtmlBlock";
    case MK_NODE_TABLE:             return "Table";
    case MK_NODE_TABLE_HEAD:        return "TableHead";
    case MK_NODE_TABLE_ROW:         return "TableRow";
    case MK_NODE_TABLE_CELL:        return "TableCell";
    /* Inline */
    case MK_NODE_TEXT:              return "Text";
    case MK_NODE_SOFT_BREAK:        return "SoftBreak";
    case MK_NODE_HARD_BREAK:        return "HardBreak";
    case MK_NODE_EMPHASIS:          return "Emphasis";
    case MK_NODE_STRONG:            return "Strong";
    case MK_NODE_STRIKETHROUGH:     return "Strikethrough";
    case MK_NODE_INLINE_CODE:       return "InlineCode";
    case MK_NODE_LINK:              return "Link";
    case MK_NODE_IMAGE:             return "Image";
    case MK_NODE_AUTO_LINK:         return "AutoLink";
    case MK_NODE_HTML_INLINE:       return "HtmlInline";
    case MK_NODE_TASK_LIST_ITEM:    return "TaskListItem";
    default:
        if (type >= MK_NODE_CUSTOM)  return "Custom";
        return "Unknown";
    }
}
