/* getters.c — Public node attribute accessor implementations
 *
 * All functions accept NULL / wrong-type nodes and return safe defaults.
 * No allocation, no side effects — pure reads through safe casts.
 */

#include "../include/mk_parser.h"
#include "ast.h"

/* ── MK_NODE_HEADING ────────────────────────────────────────────────────── */

int mk_node_heading_level(const MkNode *n) {
    if (!n || n->type != MK_NODE_HEADING) return 0;
    return ((const MkHeadingNode *)n)->level;
}

/* ── MK_NODE_CODE_BLOCK ─────────────────────────────────────────────────── */

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

/* ── MK_NODE_LIST ───────────────────────────────────────────────────────── */

int mk_node_list_ordered(const MkNode *n) {
    if (!n || n->type != MK_NODE_LIST) return 0;
    return ((const MkListNode *)n)->ordered;
}

int mk_node_list_start(const MkNode *n) {
    if (!n || n->type != MK_NODE_LIST) return 1;
    return ((const MkListNode *)n)->start;
}

/* ── MK_NODE_LIST_ITEM ──────────────────────────────────────────────────── */

MkTaskState mk_node_list_item_task_state(const MkNode *n) {
    if (!n || n->type != MK_NODE_LIST_ITEM) return MK_TASK_NONE;
    return ((const MkListItemNode *)n)->task_state;
}

/* ── MK_NODE_HTML_BLOCK ─────────────────────────────────────────────────── */

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

/* ── MK_NODE_TABLE ──────────────────────────────────────────────────────── */

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

/* ── MK_NODE_TABLE_CELL ─────────────────────────────────────────────────── */

MkAlign mk_node_table_cell_align(const MkNode *n) {
    if (!n || n->type != MK_NODE_TABLE_CELL) return MK_ALIGN_NONE;
    return ((const MkTableCellNode *)n)->align;
}

size_t mk_node_table_cell_col_index(const MkNode *n) {
    if (!n || n->type != MK_NODE_TABLE_CELL) return 0;
    return ((const MkTableCellNode *)n)->col_index;
}

/* ── MK_NODE_TEXT ───────────────────────────────────────────────────────── */

const char *mk_node_text_content(const MkNode *n) {
    if (!n || n->type != MK_NODE_TEXT) return NULL;
    return ((const MkTextNode *)n)->text;
}

size_t mk_node_text_content_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_TEXT) return 0;
    return ((const MkTextNode *)n)->text_len;
}

/* ── MK_NODE_INLINE_CODE ────────────────────────────────────────────────── */

const char *mk_node_inline_code_text(const MkNode *n) {
    if (!n || n->type != MK_NODE_INLINE_CODE) return NULL;
    return ((const MkInlineCodeNode *)n)->text;
}

size_t mk_node_inline_code_text_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_INLINE_CODE) return 0;
    return ((const MkInlineCodeNode *)n)->text_len;
}

/* ── MK_NODE_LINK ───────────────────────────────────────────────────────── */

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

/* ── MK_NODE_IMAGE ──────────────────────────────────────────────────────── */

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

/* ── MK_NODE_AUTO_LINK ──────────────────────────────────────────────────── */

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

/* ── MK_NODE_HTML_INLINE ────────────────────────────────────────────────── */

const char *mk_node_html_inline_raw(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_INLINE) return NULL;
    return ((const MkHtmlInlineNode *)n)->raw;
}

size_t mk_node_html_inline_raw_len(const MkNode *n) {
    if (!n || n->type != MK_NODE_HTML_INLINE) return 0;
    return ((const MkHtmlInlineNode *)n)->raw_len;
}

/* ── MK_NODE_TASK_LIST_ITEM ─────────────────────────────────────────────── */

int mk_node_task_list_item_checked(const MkNode *n) {
    if (!n || n->type != MK_NODE_TASK_LIST_ITEM) return 0;
    return ((const MkTaskListItemNode *)n)->checked;
}

/* ── MK_NODE_CUSTOM ─────────────────────────────────────────────────────── */

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
