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

/* ── Node attribute getters ───────────────────────────────────────────────── */

/* MK_NODE_HEADING */
int mk_node_heading_level(const MkNode *n) {
    if (!n || n->type != MK_NODE_HEADING) return 0;
    return ((const MkHeadingNode *)n)->level;
}

/* MK_NODE_CODE_BLOCK */
const char *mk_node_code_lang(const MkNode *n) {
    if (!n || n->type != MK_NODE_CODE_BLOCK) return NULL;
    return ((const MkCodeBlockNode *)n)->lang;
}
size_t mk_node_code_lang_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_CODE_BLOCK) return 0;
    return ((const MkCodeBlockNode *)n)->lang_len;
}
int mk_node_code_fenced(const MkNode *n) {
    if (!n || n->type != MK_NODE_CODE_BLOCK) return 0;
    return ((const MkCodeBlockNode *)n)->fenced;
}

/* MK_NODE_LIST */
int mk_node_list_ordered(const MkNode *n) {
    if (!n || n->type != MK_NODE_LIST) return 0;
    return ((const MkListNode *)n)->ordered;
}
int mk_node_list_start(const MkNode *n) {
    if (!n || n->type != MK_NODE_LIST) return 1;
    return ((const MkListNode *)n)->start;
}

/* MK_NODE_LIST_ITEM */
MkTaskState mk_node_list_item_task_state(const MkNode *n) {
    if (!n || n->type != MK_NODE_LIST_ITEM) return MK_TASK_NONE;
    return ((const MkListItemNode *)n)->task_state;
}

/* MK_NODE_HTML_BLOCK */
int mk_node_html_block_type(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_BLOCK) return 0;
    return ((const MkHtmlBlockNode *)n)->html_type;
}
const char *mk_node_html_block_raw(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_BLOCK) return NULL;
    return ((const MkHtmlBlockNode *)n)->raw;
}
size_t mk_node_html_block_raw_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_BLOCK) return 0;
    return ((const MkHtmlBlockNode *)n)->raw_len;
}

/* MK_NODE_TABLE */
size_t mk_node_table_col_count(const MkNode *n) {
    if (!n || n->type != MK_NODE_TABLE) return 0;
    return ((const MkTableNode *)n)->col_count;
}
MkAlign mk_node_table_col_align(const MkNode *n, size_t col) {
    if (!n || n->type != MK_NODE_TABLE) return MK_ALIGN_NONE;
    const MkTableNode *t = (const MkTableNode *)n;
    if (col >= t->col_count || !t->col_aligns) return MK_ALIGN_NONE;
    return t->col_aligns[col];
}

/* MK_NODE_TABLE_CELL */
MkAlign mk_node_table_cell_align(const MkNode *n) {
    if (!n || n->type != MK_NODE_TABLE_CELL) return MK_ALIGN_NONE;
    return ((const MkTableCellNode *)n)->align;
}
size_t mk_node_table_cell_col_index(const MkNode *n) {
    if (!n || n->type != MK_NODE_TABLE_CELL) return 0;
    return ((const MkTableCellNode *)n)->col_index;
}

/* MK_NODE_TEXT */
const char *mk_node_text_content(const MkNode *n) {
    if (!n || n->type != MK_NODE_TEXT) return NULL;
    return ((const MkTextNode *)n)->text;
}
size_t mk_node_text_content_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_TEXT) return 0;
    return ((const MkTextNode *)n)->text_len;
}

/* MK_NODE_INLINE_CODE */
const char *mk_node_inline_code_text(const MkNode *n) {
    if (!n || n->type != MK_NODE_INLINE_CODE) return NULL;
    return ((const MkInlineCodeNode *)n)->text;
}
size_t mk_node_inline_code_text_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_INLINE_CODE) return 0;
    return ((const MkInlineCodeNode *)n)->text_len;
}

/* MK_NODE_LINK */
const char *mk_node_link_href(const MkNode *n) {
    if (!n || n->type != MK_NODE_LINK) return NULL;
    return ((const MkLinkNode *)n)->href;
}
size_t mk_node_link_href_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_LINK) return 0;
    return ((const MkLinkNode *)n)->href_len;
}
const char *mk_node_link_title(const MkNode *n) {
    if (!n || n->type != MK_NODE_LINK) return NULL;
    return ((const MkLinkNode *)n)->title;
}
size_t mk_node_link_title_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_LINK) return 0;
    return ((const MkLinkNode *)n)->title_len;
}

/* MK_NODE_IMAGE */
const char *mk_node_image_src(const MkNode *n) {
    if (!n || n->type != MK_NODE_IMAGE) return NULL;
    return ((const MkImageNode *)n)->src;
}
size_t mk_node_image_src_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_IMAGE) return 0;
    return ((const MkImageNode *)n)->src_len;
}
const char *mk_node_image_alt(const MkNode *n) {
    if (!n || n->type != MK_NODE_IMAGE) return NULL;
    return ((const MkImageNode *)n)->alt;
}
size_t mk_node_image_alt_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_IMAGE) return 0;
    return ((const MkImageNode *)n)->alt_len;
}
const char *mk_node_image_title(const MkNode *n) {
    if (!n || n->type != MK_NODE_IMAGE) return NULL;
    return ((const MkImageNode *)n)->title;
}
size_t mk_node_image_title_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_IMAGE) return 0;
    return ((const MkImageNode *)n)->title_len;
}

/* MK_NODE_AUTO_LINK */
const char *mk_node_autolink_url(const MkNode *n) {
    if (!n || n->type != MK_NODE_AUTO_LINK) return NULL;
    return ((const MkAutoLinkNode *)n)->url;
}
size_t mk_node_autolink_url_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_AUTO_LINK) return 0;
    return ((const MkAutoLinkNode *)n)->url_len;
}
int mk_node_autolink_is_email(const MkNode *n) {
    if (!n || n->type != MK_NODE_AUTO_LINK) return 0;
    return ((const MkAutoLinkNode *)n)->is_email;
}

/* MK_NODE_HTML_INLINE */
const char *mk_node_html_inline_raw(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_INLINE) return NULL;
    return ((const MkHtmlInlineNode *)n)->raw;
}
size_t mk_node_html_inline_raw_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_INLINE) return 0;
    return ((const MkHtmlInlineNode *)n)->raw_len;
}

/* MK_NODE_TASK_LIST_ITEM */
int mk_node_task_list_item_checked(const MkNode *n) {
    if (!n || n->type != MK_NODE_TASK_LIST_ITEM) return 0;
    return ((const MkTaskListItemNode *)n)->checked;
}

/* MK_NODE_CUSTOM */
const char *mk_node_custom_plugin_name(const MkNode *n) {
    if (!n || n->type < MK_NODE_CUSTOM) return NULL;
    return ((const MkCustomNode *)n)->plugin_name;
}
const char *mk_node_custom_raw(const MkNode *n) {
    if (!n || n->type < MK_NODE_CUSTOM) return NULL;
    return ((const MkCustomNode *)n)->raw;
}
size_t mk_node_custom_raw_len(const MkNode *n) {
    if (!n || n->type < MK_NODE_CUSTOM) return 0;
    return ((const MkCustomNode *)n)->raw_len;
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
