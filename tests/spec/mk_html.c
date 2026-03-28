/* mk_html.c — CommonMark-compatible HTML serializer
 *
 * Converts mk_parser push events into HTML output matching CommonMark 0.31
 * spec expectations as closely as possible.
 *
 * Coverage:
 *   Blocks:  heading, paragraph, fenced/indented code block, blockquote,
 *            list (ordered/unordered, tight/loose), list item, thematic break,
 *            html block, table, table head/row/cell
 *   Inlines: text, soft break, hard break, emphasis, strong, strikethrough,
 *            inline code, link, image, autolink, html inline, task list item
 */
#include "mk_html.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ── dynamic string buffer ─────────────────────────────────────────────────── */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} Buf;

static int buf_grow(Buf *b, size_t need) {
    if (b->len + need < b->cap) return 1;
    size_t ncap = b->cap ? b->cap : 256;
    while (ncap < b->len + need) ncap *= 2;
    char *nd = realloc(b->data, ncap);
    if (!nd) return 0;
    b->data = nd;
    b->cap  = ncap;
    return 1;
}

static void buf_append(Buf *b, const char *s, size_t len) {
    if (!buf_grow(b, len + 1)) return;
    memcpy(b->data + b->len, s, len);
    b->len += len;
    b->data[b->len] = '\0';
}

static void buf_str(Buf *b, const char *s) { buf_append(b, s, strlen(s)); }

static void buf_char(Buf *b, char c) { buf_append(b, &c, 1); }

/* HTML-escape a string (for text content and attribute values). */
static void buf_escape(Buf *b, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '&':  buf_str(b, "&amp;");  break;
            case '<':  buf_str(b, "&lt;");   break;
            case '>':  buf_str(b, "&gt;");   break;
            case '"':  buf_str(b, "&quot;"); break;
            default:   buf_char(b, (char)c); break;
        }
    }
}

/* ── context stack ──────────────────────────────────────────────────────────── */

#define CTX_STACK_MAX 64

typedef enum {
    CTX_NONE = 0,
    CTX_LIST_TIGHT,   /* inside tight list — suppress <p> around item content */
    CTX_LIST_LOOSE,   /* inside loose list */
    CTX_LIST_ITEM,    /* inside a list item */
    CTX_HEADING,
    CTX_PARAGRAPH,
    CTX_CODE_BLOCK,
    CTX_BLOCKQUOTE,
    CTX_TABLE_HEAD,   /* thead row */
    CTX_TABLE_BODY,   /* tbody row */
    CTX_TABLE_CELL,
    CTX_IMAGE,        /* collecting alt text */
    CTX_TASK_ITEM,    /* list item that is a task — skip [ ] / [x] prefix */
} CtxKind;

typedef struct {
    CtxKind kind;
    int     data;   /* tight flag, heading level, etc. */
} CtxFrame;

/* ── serializer state ──────────────────────────────────────────────────────── */

struct MkHtmlState {
    Buf       buf;
    CtxFrame  stack[CTX_STACK_MAX];
    int       sp;         /* stack pointer (top is stack[sp-1]) */
    int       in_tight;   /* > 0 if inside a tight list */
    /* temp buf for image alt accumulation */
    Buf       alt_buf;
};

/* ── stack helpers ──────────────────────────────────────────────────────────── */

static void push_ctx(MkHtmlState *s, CtxKind kind, int data) {
    if (s->sp >= CTX_STACK_MAX) return;
    s->stack[s->sp].kind = kind;
    s->stack[s->sp].data = data;
    s->sp++;
}

static CtxKind top_ctx(MkHtmlState *s) {
    return s->sp > 0 ? s->stack[s->sp - 1].kind : CTX_NONE;
}

static int top_data(MkHtmlState *s) {
    return s->sp > 0 ? s->stack[s->sp - 1].data : 0;
}

static void pop_ctx(MkHtmlState *s) {
    if (s->sp > 0) s->sp--;
}

/* Search stack for a kind (most recent). */
static int find_ctx(MkHtmlState *s, CtxKind kind) {
    for (int i = s->sp - 1; i >= 0; i--) {
        if (s->stack[i].kind == kind) return i;
    }
    return -1;
}

/* ── node open callback ──────────────────────────────────────────────────────── */

static void on_open(void *ud, MkNode *node) {
    MkHtmlState *s = (MkHtmlState *)ud;
    Buf *b = &s->buf;

    switch (node->type) {

    case MK_NODE_DOCUMENT:
        break;

    case MK_NODE_HEADING: {
        int level = mk_node_heading_level(node);
        char tag[8]; snprintf(tag, sizeof(tag), "<h%d>", level);
        buf_str(b, tag);
        push_ctx(s, CTX_HEADING, level);
        break;
    }

    case MK_NODE_PARAGRAPH:
        /* In tight list items, suppress <p>.
         * Otherwise, record the buffer position BEFORE writing <p> so
         * on_modify can truncate back to here for setext/table promotion. */
        if (s->in_tight > 0 && find_ctx(s, CTX_LIST_ITEM) >= 0) {
            push_ctx(s, CTX_PARAGRAPH, -1); /* -1 = suppressed */
        } else {
            int saved = (int)b->len;   /* position before <p> */
            buf_str(b, "<p>");
            push_ctx(s, CTX_PARAGRAPH, saved);
        }
        break;

    case MK_NODE_CODE_BLOCK: {
        const char *lang     = mk_node_code_lang(node);
        size_t      lang_len = mk_node_code_lang_len(node);
        if (lang && lang_len > 0) {
            buf_str(b, "<pre><code class=\"language-");
            buf_escape(b, lang, lang_len);
            buf_str(b, "\">");
        } else {
            buf_str(b, "<pre><code>");
        }
        push_ctx(s, CTX_CODE_BLOCK, 0);
        break;
    }

    case MK_NODE_BLOCK_QUOTE:
        buf_str(b, "<blockquote>\n");
        push_ctx(s, CTX_BLOCKQUOTE, 0);
        break;

    case MK_NODE_LIST: {
        int ordered = mk_node_list_ordered(node);
        int tight   = !(node->flags & MK_NODE_FLAG_TIGHT) ? 0 : 1;
        /* NOTE: MK_NODE_FLAG_TIGHT marks tight lists */
        tight = (node->flags & MK_NODE_FLAG_TIGHT) ? 1 : 0;
        if (ordered) {
            int start = mk_node_list_start(node);
            if (start != 1)
                buf_str(b, "<ol start=\""), buf_append(b, NULL, 0);
            if (start != 1) {
                char tmp[32]; snprintf(tmp, sizeof(tmp), "<ol start=\"%d\">\n", start);
                buf_str(b, tmp);
            } else {
                buf_str(b, "<ol>\n");
            }
        } else {
            buf_str(b, "<ul>\n");
        }
        push_ctx(s, tight ? CTX_LIST_TIGHT : CTX_LIST_LOOSE, ordered);
        if (tight) s->in_tight++;
        break;
    }

    case MK_NODE_LIST_ITEM: {
        int tight = (top_ctx(s) == CTX_LIST_TIGHT);
        /* check task state */
        MkTaskState ts = mk_node_list_item_task_state(node);
        if (ts != MK_TASK_NONE) {
            buf_str(b, "<li>");
            const char *checkbox = (ts == MK_TASK_CHECKED)
                ? "<input checked=\"\" disabled=\"\" type=\"checkbox\"> "
                : "<input disabled=\"\" type=\"checkbox\"> ";
            buf_str(b, checkbox);
            push_ctx(s, CTX_TASK_ITEM, tight);
        } else {
            buf_str(b, "<li>");
            push_ctx(s, CTX_LIST_ITEM, tight);
        }
        break;
    }

    case MK_NODE_THEMATIC_BREAK:
        buf_str(b, "<hr />\n");
        break;

    case MK_NODE_HTML_BLOCK:
        /* raw content emitted via on_text; nothing to open */
        break;

    case MK_NODE_TABLE:
        buf_str(b, "<table>\n");
        break;

    case MK_NODE_TABLE_HEAD:
        buf_str(b, "<thead>\n");   /* <tr> comes from on_open(TABLE_ROW) */
        push_ctx(s, CTX_TABLE_HEAD, 0);
        break;

    case MK_NODE_TABLE_ROW:
        buf_str(b, "<tr>\n");
        push_ctx(s, CTX_TABLE_BODY, 0);
        break;

    case MK_NODE_TABLE_CELL: {
        int is_head = (find_ctx(s, CTX_TABLE_HEAD) >= 0);
        MkAlign align = mk_node_table_cell_align(node);
        if (is_head) {
            switch (align) {
                case MK_ALIGN_LEFT:   buf_str(b, "<th align=\"left\">"); break;
                case MK_ALIGN_CENTER: buf_str(b, "<th align=\"center\">"); break;
                case MK_ALIGN_RIGHT:  buf_str(b, "<th align=\"right\">"); break;
                default:              buf_str(b, "<th>"); break;
            }
        } else {
            switch (align) {
                case MK_ALIGN_LEFT:   buf_str(b, "<td align=\"left\">"); break;
                case MK_ALIGN_CENTER: buf_str(b, "<td align=\"center\">"); break;
                case MK_ALIGN_RIGHT:  buf_str(b, "<td align=\"right\">"); break;
                default:              buf_str(b, "<td>"); break;
            }
        }
        push_ctx(s, CTX_TABLE_CELL, is_head);
        break;
    }

    /* Inline nodes */
    case MK_NODE_EMPHASIS:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "<em>");
        break;

    case MK_NODE_STRONG:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "<strong>");
        break;

    case MK_NODE_STRIKETHROUGH:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "<del>");
        break;

    case MK_NODE_INLINE_CODE:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "<code>");
        break;

    case MK_NODE_LINK: {
        const char *href      = mk_node_link_href(node);
        size_t      href_len  = mk_node_link_href_len(node);
        const char *title     = mk_node_link_title(node);
        size_t      title_len = mk_node_link_title_len(node);
        buf_str(b, "<a href=\"");
        if (href) buf_escape(b, href, href_len);
        if (title && title_len > 0) {
            buf_str(b, "\" title=\"");
            buf_escape(b, title, title_len);
        }
        buf_str(b, "\">");
        break;
    }

    case MK_NODE_IMAGE: {
        const char *src       = mk_node_image_src(node);
        size_t      src_len   = mk_node_image_src_len(node);
        const char *title     = mk_node_image_title(node);
        size_t      title_len = mk_node_image_title_len(node);
        /* Stash src and title; alt is collected from child text events */
        /* We'll write the tag on close; for now push context */
        push_ctx(s, CTX_IMAGE, 0);
        /* Store src in alt_buf temporarily using a marker scheme:
         * Format: "SRC\0TITLE\0" */
        s->alt_buf.len = 0;
        if (s->alt_buf.data) s->alt_buf.data[0] = '\0';
        /* pack: first store src+NUL, then title+NUL, then alt will follow */
        buf_append(&s->alt_buf, src ? src : "", src ? src_len : 0);
        buf_char(&s->alt_buf, '\0');
        buf_append(&s->alt_buf, title ? title : "", title ? title_len : 0);
        buf_char(&s->alt_buf, '\0');
        /* Mark where alt text starts */
        break;
    }

    case MK_NODE_AUTO_LINK: {
        const char *url      = mk_node_autolink_url(node);
        size_t      url_len  = mk_node_autolink_url_len(node);
        int         is_email = mk_node_autolink_is_email(node);
        buf_str(b, "<a href=\"");
        if (is_email) buf_str(b, "mailto:");
        if (url) buf_escape(b, url, url_len);
        buf_str(b, "\">");
        if (url) buf_escape(b, url, url_len);
        buf_str(b, "</a>");
        break;
    }

    case MK_NODE_TASK_LIST_ITEM:
        /* emitted as part of LIST_ITEM handling above */
        break;

    default:
        break;
    }
}

/* ── node close callback ────────────────────────────────────────────────────── */

static void on_close(void *ud, MkNode *node) {
    MkHtmlState *s = (MkHtmlState *)ud;
    Buf *b = &s->buf;

    switch (node->type) {

    case MK_NODE_DOCUMENT:
        break;

    case MK_NODE_HEADING: {
        int level = top_data(s);
        pop_ctx(s);
        char tag[9]; snprintf(tag, sizeof(tag), "</h%d>\n", level);
        buf_str(b, tag);
        break;
    }

    case MK_NODE_PARAGRAPH: {
        int data = top_data(s);   /* -1 = suppressed, >=0 = buf position before <p> */
        pop_ctx(s);
        if (data >= 0) buf_str(b, "</p>\n");
        break;
    }

    case MK_NODE_CODE_BLOCK:
        pop_ctx(s);
        buf_str(b, "</code></pre>\n");
        break;

    case MK_NODE_BLOCK_QUOTE:
        pop_ctx(s);
        buf_str(b, "</blockquote>\n");
        break;

    case MK_NODE_LIST: {
        int ordered = top_data(s);
        int tight   = (top_ctx(s) == CTX_LIST_TIGHT);
        if (tight && s->in_tight > 0) s->in_tight--;
        pop_ctx(s);
        buf_str(b, ordered ? "</ol>\n" : "</ul>\n");
        break;
    }

    case MK_NODE_LIST_ITEM:
    case MK_NODE_TASK_LIST_ITEM:
        pop_ctx(s);
        buf_str(b, "</li>\n");
        break;

    case MK_NODE_THEMATIC_BREAK:
        break;

    case MK_NODE_HTML_BLOCK:
        break;

    case MK_NODE_TABLE:
        buf_str(b, "</tbody>\n</table>\n");
        break;

    case MK_NODE_TABLE_HEAD:
        pop_ctx(s);
        buf_str(b, "</thead>\n<tbody>\n");   /* </tr> comes from on_close(TABLE_ROW) */
        break;

    case MK_NODE_TABLE_ROW:
        pop_ctx(s);
        buf_str(b, "</tr>\n");
        break;

    case MK_NODE_TABLE_CELL: {
        int is_head = top_data(s);
        pop_ctx(s);
        buf_str(b, is_head ? "</th>\n" : "</td>\n");
        break;
    }

    case MK_NODE_EMPHASIS:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "</em>");
        break;

    case MK_NODE_STRONG:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "</strong>");
        break;

    case MK_NODE_STRIKETHROUGH:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "</del>");
        break;

    case MK_NODE_INLINE_CODE:
        if (top_ctx(s) != CTX_IMAGE)
            buf_str(b, "</code>");
        break;

    case MK_NODE_LINK:
        buf_str(b, "</a>");
        break;

    case MK_NODE_IMAGE: {
        pop_ctx(s); /* CTX_IMAGE */
        /* Extract src, title, alt from alt_buf */
        const char *src   = s->alt_buf.data;
        const char *title = src ? (src + strlen(src) + 1) : "";
        const char *alt   = title ? (title + strlen(title) + 1) : "";
        buf_str(b, "<img src=\"");
        buf_escape(b, src ? src : "", src ? strlen(src) : 0);
        buf_str(b, "\" alt=\"");
        buf_escape(b, alt, strlen(alt));
        if (title && *title) {
            buf_str(b, "\" title=\"");
            buf_escape(b, title, strlen(title));
        }
        buf_str(b, "\" />");
        /* reset alt buf */
        s->alt_buf.len = 0;
        if (s->alt_buf.data) s->alt_buf.data[0] = '\0';
        break;
    }

    case MK_NODE_SOFT_BREAK:
        buf_char(b, '\n');
        break;

    case MK_NODE_HARD_BREAK:
        buf_str(b, "<br />\n");
        break;

    case MK_NODE_AUTO_LINK:
        break;

    default:
        break;
    }
}

/* ── text callback ──────────────────────────────────────────────────────────── */

static void on_text(void *ud, MkNode *node, const char *text, size_t len) {
    MkHtmlState *s = (MkHtmlState *)ud;
    Buf *b = &s->buf;

    switch (node->type) {
    case MK_NODE_HTML_BLOCK:
    case MK_NODE_HTML_INLINE:
        /* raw output — no escaping */
        buf_append(b, text, len);
        break;

    case MK_NODE_CODE_BLOCK:
    case MK_NODE_INLINE_CODE:
        buf_escape(b, text, len);
        break;

    case MK_NODE_IMAGE:
        /* Accumulate alt text into alt_buf after the two NUL-separated fields */
        {
            /* find offset of alt start: after src\0title\0 */
            const char *src   = s->alt_buf.data;
            if (!src) { buf_append(&s->alt_buf, text, len); break; }
            size_t src_len   = strlen(src);
            const char *ttl  = src + src_len + 1;
            size_t ttl_len   = strlen(ttl);
            size_t prefix    = src_len + 1 + ttl_len + 1;
            /* ensure buf is at least prefix bytes */
            while (s->alt_buf.len < prefix) buf_char(&s->alt_buf, '\0');
            /* now append alt text */
            buf_append(&s->alt_buf, text, len);
        }
        break;

    case MK_NODE_TEXT:
    case MK_NODE_EMPHASIS:
    case MK_NODE_STRONG:
    case MK_NODE_STRIKETHROUGH:
    case MK_NODE_LINK:
    case MK_NODE_PARAGRAPH:
    default:
        if (top_ctx(s) == CTX_IMAGE) {
            /* accumulate alt text */
            const char *src  = s->alt_buf.data;
            if (!src) { buf_append(&s->alt_buf, text, len); break; }
            size_t src_len  = strlen(src);
            const char *ttl = src + src_len + 1;
            size_t ttl_len  = strlen(ttl);
            size_t prefix   = src_len + 1 + ttl_len + 1;
            while (s->alt_buf.len < prefix) buf_char(&s->alt_buf, '\0');
            buf_escape(&s->alt_buf, text, len);
        } else {
            buf_escape(b, text, len);
        }
        break;
    }
}

/* ── modify callback ──────────────────────────────────────────────────────── */

static void on_modify(void *ud, MkNode *node) {
    MkHtmlState *s = (MkHtmlState *)ud;
    Buf *b = &s->buf;

    /* Setext heading promotion: paragraph → heading.
     * Table promotion: paragraph → table.
     * In both cases on_open(PARAGRAPH) fired earlier; we must undo its <p>. */
    if (top_ctx(s) != CTX_PARAGRAPH) return;

    int saved_pos = top_data(s);   /* buf position before <p> was written */
    pop_ctx(s);

    /* Truncate buffer back to before <p> (no-op if suppressed, saved_pos<0) */
    if (saved_pos >= 0) {
        b->len = (size_t)saved_pos;
        if (b->data) b->data[b->len] = '\0';
    }

    if (node->type == MK_NODE_HEADING) {
        int level = mk_node_heading_level(node);
        char tag[8]; snprintf(tag, sizeof(tag), "<h%d>", level);
        buf_str(b, tag);
        push_ctx(s, CTX_HEADING, level);
    } else if (node->type == MK_NODE_TABLE) {
        /* on_open(TABLE) is never fired for promoted tables; emit it here */
        buf_str(b, "<table>\n");
        /* TABLE_HEAD/TABLE_ROW/TABLE_CELL events manage the rest */
    }
}

/* ── public API ──────────────────────────────────────────────────────────────── */

MkHtmlState *mk_html_new(void) {
    MkHtmlState *s = calloc(1, sizeof(*s));
    return s;
}

void mk_html_free(MkHtmlState *s) {
    if (!s) return;
    free(s->buf.data);
    free(s->alt_buf.data);
    free(s);
}

MkCallbacks mk_html_callbacks(MkHtmlState *s) {
    MkCallbacks cb;
    cb.user_data     = s;
    cb.on_node_open  = on_open;
    cb.on_node_close = on_close;
    cb.on_text       = on_text;
    cb.on_node_modify = on_modify;
    return cb;
}

const char *mk_html_result(MkHtmlState *s, size_t *out_len) {
    if (out_len) *out_len = s->buf.len;
    return s->buf.data ? s->buf.data : "";
}

void mk_html_reset(MkHtmlState *s) {
    s->buf.len  = 0;
    if (s->buf.data)     s->buf.data[0]     = '\0';
    s->alt_buf.len = 0;
    if (s->alt_buf.data) s->alt_buf.data[0] = '\0';
    s->sp       = 0;
    s->in_tight = 0;
}

char *mk_html_parse(const char *markdown, size_t md_len, size_t *out_len) {
    MkHtmlState *s = mk_html_new();
    if (!s) return NULL;

    MkCallbacks cb   = mk_html_callbacks(s);
    MkArena    *arena = mk_arena_new();
    MkParser   *parser = mk_parser_new(arena, &cb);
    if (!arena || !parser) {
        mk_html_free(s);
        if (parser) mk_parser_free(parser);
        if (arena)  mk_arena_free(arena);
        return NULL;
    }

    mk_feed(parser, markdown, md_len);
    mk_finish(parser);

    /* drain pull queue */
    MkDelta *d;
    while ((d = mk_pull_delta(parser)) != NULL) mk_delta_free(d);

    size_t len = 0;
    const char *raw = mk_html_result(s, &len);
    char *result = malloc(len + 1);
    if (result) {
        memcpy(result, raw, len);
        result[len] = '\0';
    }
    if (out_len) *out_len = len;

    mk_parser_free(parser);
    mk_arena_free(arena);
    mk_html_free(s);
    return result;
}
