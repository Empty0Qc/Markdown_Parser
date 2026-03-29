/* inline_parser.c — Inline-level parser state machine (M5)
 *
 * Strategy: greedy recursive-descent with a delimiter stack for emphasis.
 *
 * Priority order (highest first):
 *   1. Backslash escape  (\*)
 *   2. Code span         (`code`)
 *   3. Autolink          (<url> / <email>)
 *   4. HTML inline       (<tag ...>)
 *   5. Image             (![alt](src))
 *   6. Link              ([text](url))
 *   7. Strong/Emphasis   (** * __ _)
 *   8. Strikethrough     (~~)
 *   9. Hard/Soft break   (two-spaces+\n / \n)
 *  10. Plain text
 *
 * GFM task list item ([ ] / [x]) detected at the start of a list-item's
 * inline content.
 */

#include "inline_parser.h"
#include "../src/plugin.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Internal parser state
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    MkArena           *arena;
    const MkCallbacks *cbs;
    const char        *src;
    size_t             len;
    MkParser          *parser;  /* NULL when used without MkParser (no plugins) */
} IP;

/* ── Node emit helpers ────────────────────────────────────────────────────── */

static void iemit_open(const IP *ip, MkNode *n) {
    if (ip->cbs && ip->cbs->on_node_open)
        ip->cbs->on_node_open(ip->cbs->user_data, n);
}

static void iemit_close(const IP *ip, MkNode *n) {
    n->flags &= ~MK_NODE_FLAG_PENDING;
    if (ip->cbs && ip->cbs->on_node_close)
        ip->cbs->on_node_close(ip->cbs->user_data, n);
}

static void iemit_text(const IP *ip, MkNode *n, const char *text, size_t len) {
    if (ip->cbs && ip->cbs->on_text)
        ip->cbs->on_text(ip->cbs->user_data, n, text, len);
}

static void append_text(const IP *ip, MkNode *parent,
                        const char *s, size_t len)
{
    if (!len) return;
    MkTextNode *tn = MK_NODE_NEW(ip->arena, MkTextNode, MK_NODE_TEXT, 0);
    if (!tn) return;
    tn->text     = mk_arena_strdup_stable(ip->arena, s, len);
    tn->text_len = len;
    mk_node_append_child(parent, &tn->base);
    iemit_open (ip, &tn->base);
    iemit_text (ip, &tn->base, s, len);
    iemit_close(ip, &tn->base);
}

/* Append a single character as text (e.g. an unrecognised escape). */
static void append_char(const IP *ip, MkNode *parent, char c) {
    append_text(ip, parent, &c, 1);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Forward declaration
 * ════════════════════════════════════════════════════════════════════════════ */

/* Parse text[pos..end) into parent's children.  Returns new position. */
static size_t parse_seq(const IP *ip, size_t pos, size_t end, MkNode *parent);

/* ════════════════════════════════════════════════════════════════════════════
 * Helper: find a matching close for a bracket/paren, respecting nesting
 * ════════════════════════════════════════════════════════════════════════════ */

/* Scan forward from pos for matching close_char (skipping nested pairs).
 * Returns position of close_char, or 0 if not found. */
static size_t find_matching(const char *s, size_t pos, size_t end,
                            char open_c, char close_c)
{
    int depth = 1;
    while (pos < end) {
        if      (s[pos] == open_c)  depth++;
        else if (s[pos] == close_c) { if (--depth == 0) return pos; }
        pos++;
    }
    return 0; /* not found */
}

/* ════════════════════════════════════════════════════════════════════════════
 * Link destination + title parser  "(<href> "title")"
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *href;  size_t href_len;
    const char *title; size_t title_len;
    size_t      end;   /* position after closing ')' */
} LinkDest;

static int parse_link_dest(const char *s, size_t pos, size_t end,
                           LinkDest *out)
{
    if (pos >= end || s[pos] != '(') return 0;
    pos++;

    /* Skip leading whitespace */
    while (pos < end && (s[pos] == ' ' || s[pos] == '\t')) pos++;

    /* href: either <...> or run of non-space non-control chars */
    size_t href_s, href_e;
    if (pos < end && s[pos] == '<') {
        href_s = ++pos;
        while (pos < end && s[pos] != '>' && s[pos] != '\n') pos++;
        if (pos >= end || s[pos] != '>') return 0;
        href_e = pos++;
    } else {
        href_s = pos;
        int depth = 0;
        while (pos < end && s[pos] != ' ' && s[pos] != '\t') {
            if (s[pos] == '\\' && pos + 1 < end) {
                pos += 2;  /* skip backslash-escaped character — don't count brackets */
                continue;
            }
            if (s[pos] == '(') { depth++; pos++; continue; }
            if (s[pos] == ')') { if (!depth) break; depth--; pos++; continue; }
            pos++;
        }
        href_e = pos;
    }

    while (pos < end && (s[pos] == ' ' || s[pos] == '\t')) pos++;

    /* Optional title: "..." '...' (...) */
    const char *title = NULL;
    size_t      tlen  = 0;
    if (pos < end && (s[pos] == '"' || s[pos] == '\'' || s[pos] == '(')) {
        char close_q = (s[pos] == '(') ? ')' : s[pos];
        pos++;
        size_t ts = pos;
        while (pos < end && s[pos] != close_q) pos++;
        if (pos < end) {
            title = s + ts;
            tlen  = pos - ts;
            pos++; /* skip close quote */
        }
    }

    while (pos < end && (s[pos] == ' ' || s[pos] == '\t')) pos++;
    if (pos >= end || s[pos] != ')') return 0;

    out->href      = s + href_s;
    out->href_len  = href_e - href_s;
    out->title     = title;
    out->title_len = tlen;
    out->end       = pos + 1;
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 * try_* functions: attempt to parse a construct at pos.
 * Return new position on success, 0 on failure (caller treats as literal).
 * ════════════════════════════════════════════════════════════════════════════ */

/* ── Backslash escape ─────────────────────────────────────────────────────── */

static size_t try_escape(const IP *ip, size_t pos, size_t end, MkNode *parent) {
    static const char escapable[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    if (pos + 1 >= end) return 0;
    char next = ip->src[pos + 1];
    if (next == '\n') {
        /* Hard break via backslash */
        MkNode *hb = mk_node_alloc(ip->arena, sizeof(MkNode), MK_NODE_HARD_BREAK, 0);
        mk_node_append_child(parent, hb);
        iemit_open(ip, hb);
        iemit_close(ip, hb);
        return pos + 2;
    }
    if (strchr(escapable, next)) {
        append_char(ip, parent, next);
        return pos + 2;
    }
    return 0;
}

/* ── Code span ────────────────────────────────────────────────────────────── */

static size_t try_code_span(const IP *ip, size_t pos, size_t end, MkNode *parent) {
    const char *s = ip->src;

    /* Count opening backticks */
    int bticks = 0;
    while (pos + (size_t)bticks < end && s[pos + (size_t)bticks] == '`') bticks++;
    size_t open_end = pos + (size_t)bticks;

    /* Find matching closing run of exactly bticks backticks */
    size_t p = open_end;
    while (p < end) {
        if (s[p] == '`') {
            int cnt = 0;
            while (p + (size_t)cnt < end && s[p + (size_t)cnt] == '`') cnt++;
            if (cnt == bticks) {
                /* Found: content is s[open_end .. p) */
                const char *code = s + open_end;
                size_t      clen = p - open_end;

                /* Strip one leading/trailing space if both present and not all-spaces */
                if (clen >= 2 && code[0] == ' ' && code[clen-1] == ' ') {
                    int all = 1;
                    for (size_t k = 0; k < clen && all; k++)
                        if (code[k] != ' ') all = 0;
                    if (!all) { code++; clen -= 2; }
                }

                MkInlineCodeNode *n = MK_NODE_NEW(ip->arena, MkInlineCodeNode,
                                                   MK_NODE_INLINE_CODE, 0);
                if (!n) return 0;
                n->text     = mk_arena_strdup_stable(ip->arena, code, clen);
                n->text_len = clen;
                mk_node_append_child(parent, &n->base);
                iemit_open (ip, &n->base);
                iemit_text (ip, &n->base, n->text, n->text_len);
                iemit_close(ip, &n->base);
                return p + (size_t)bticks;
            }
            p += (size_t)cnt;
        } else {
            p++;
        }
    }
    return 0; /* unmatched — treat backticks as literal */
}

/* ── Autolink / HTML inline ───────────────────────────────────────────────── */

static int is_url_char(char c) {
    return !isspace((unsigned char)c) && c != '<' && c != '>';
}

static size_t try_autolink_or_html(const IP *ip, size_t pos, size_t end,
                                   MkNode *parent)
{
    const char *s = ip->src;
    if (pos >= end || s[pos] != '<') return 0;
    size_t p = pos + 1;

    /* Autolink: <scheme:path> or <email> */
    size_t start = p;
    while (p < end && is_url_char(s[p]) && s[p] != '>') p++;
    if (p < end && s[p] == '>') {
        size_t url_len = p - start;
        if (url_len > 0) {
            const char *url = s + start;
            int is_email = (memchr(url, '@', url_len) != NULL);
            /* Basic scheme check: contains ':' before any space */
            int is_url = (memchr(url, ':', url_len) != NULL && !is_email);
            if (is_url || is_email) {
                MkAutoLinkNode *n = MK_NODE_NEW(ip->arena, MkAutoLinkNode,
                                                MK_NODE_AUTO_LINK, 0);
                if (!n) return 0;
                n->url      = mk_arena_strdup_stable(ip->arena, url, url_len);
                n->url_len  = url_len;
                n->is_email = is_email;
                mk_node_append_child(parent, &n->base);
                iemit_open (ip, &n->base);
                iemit_close(ip, &n->base);
                return p + 1;
            }
        }
    }

    /* HTML inline tag: <tag ...> or </tag> or <!-- --> or <? ?> etc. */
    p = pos + 1;
    if (p >= end) return 0;

    /* Comment: <!-- ... --> */
    if (p + 2 < end && s[p] == '!' && s[p+1] == '-' && s[p+2] == '-') {
        p += 3;
        while (p + 2 < end) {
            if (s[p] == '-' && s[p+1] == '-' && s[p+2] == '>') {
                p += 3;
                goto emit_html_inline;
            }
            p++;
        }
        return 0;
    }

    /* Processing instruction: <? ... ?> */
    if (p < end && s[p] == '?') {
        p++;
        while (p + 1 < end) {
            if (s[p] == '?' && s[p+1] == '>') { p += 2; goto emit_html_inline; }
            p++;
        }
        return 0;
    }

    /* CDATA: <![CDATA[ ... ]]> */
    if (p + 8 < end && memcmp(s + p, "![CDATA[", 8) == 0) {
        p += 8;
        while (p + 2 < end) {
            if (s[p] == ']' && s[p+1] == ']' && s[p+2] == '>') { p += 3; goto emit_html_inline; }
            p++;
        }
        return 0;
    }

    /* Open/close tag: <tagname> or </tagname> or <tag attr="v"> */
    if (s[p] == '/') p++;
    if (p >= end || !isalpha((unsigned char)s[p])) return 0;
    while (p < end && (isalnum((unsigned char)s[p]) || s[p] == '-')) p++;
    /* Scan to closing > */
    while (p < end && s[p] != '>') {
        if (s[p] == '\n') return 0; /* tags can't span lines */
        p++;
    }
    if (p >= end) return 0;
    p++; /* include '>' */

emit_html_inline:;
    MkHtmlInlineNode *h = MK_NODE_NEW(ip->arena, MkHtmlInlineNode,
                                      MK_NODE_HTML_INLINE, 0);
    if (!h) return 0;
    h->raw     = mk_arena_strdup_stable(ip->arena, s + pos, p - pos);
    h->raw_len = p - pos;
    mk_node_append_child(parent, &h->base);
    iemit_open (ip, &h->base);
    /* Keep push-event contract consistent with HTML_BLOCK:
     * consumers that only listen to on_text should still receive raw HTML. */
    iemit_text (ip, &h->base, h->raw, h->raw_len);
    iemit_close(ip, &h->base);
    return p;
}

/* ── Image  ![alt](src "title") ──────────────────────────────────────────── */

static size_t try_image(const IP *ip, size_t pos, size_t end, MkNode *parent) {
    const char *s = ip->src;
    if (pos + 1 >= end || s[pos] != '!' || s[pos+1] != '[') return 0;

    /* Find matching ']' */
    size_t close_br = find_matching(s, pos + 2, end, '[', ']');
    if (!close_br) return 0;

    LinkDest dest;
    if (!parse_link_dest(s, close_br + 1, end, &dest)) return 0;

    const char *alt   = s + pos + 2;
    size_t      alen  = close_br - (pos + 2);

    MkImageNode *n = MK_NODE_NEW(ip->arena, MkImageNode, MK_NODE_IMAGE, 0);
    if (!n) return 0;
    n->src       = mk_arena_strdup_stable(ip->arena, dest.href,  dest.href_len);
    n->src_len   = dest.href_len;
    n->alt       = mk_arena_strdup_stable(ip->arena, alt, alen);
    n->alt_len   = alen;
    n->title     = dest.title ? mk_arena_strdup_stable(ip->arena, dest.title, dest.title_len) : NULL;
    n->title_len = dest.title_len;

    mk_node_append_child(parent, &n->base);
    iemit_open (ip, &n->base);
    iemit_close(ip, &n->base);
    return dest.end;
}

/* ── Link  [text](url "title") ───────────────────────────────────────────── */

static size_t try_link(const IP *ip, size_t pos, size_t end, MkNode *parent) {
    const char *s = ip->src;
    if (pos >= end || s[pos] != '[') return 0;

    /* Find matching ']' skipping nested brackets */
    size_t close_br = find_matching(s, pos + 1, end, '[', ']');
    if (!close_br) return 0;

    LinkDest dest;
    if (!parse_link_dest(s, close_br + 1, end, &dest)) return 0;

    MkLinkNode *n = MK_NODE_NEW(ip->arena, MkLinkNode, MK_NODE_LINK, 0);
    if (!n) return 0;
    n->href      = mk_arena_strdup_stable(ip->arena, dest.href,  dest.href_len);
    n->href_len  = dest.href_len;
    n->title     = dest.title ? mk_arena_strdup_stable(ip->arena, dest.title, dest.title_len) : NULL;
    n->title_len = dest.title_len;

    mk_node_append_child(parent, &n->base);
    iemit_open(ip, &n->base);
    /* Recursively parse link text as children */
    parse_seq(ip, pos + 1, close_br, &n->base);
    iemit_close(ip, &n->base);
    return dest.end;
}

/* ── Emphasis and Strong  * ** _ __ ──────────────────────────────────────── */

/*
 * Greedy delimiter matching:
 *   - Count run of c at pos → `count`
 *   - strong if count >= 2 (consume 2), else emphasis (consume 1)
 *   - Scan forward for a closing run of c with length >= consumed
 *   - On match: wrap content in Emphasis or Strong, recurse
 *   - On no match: return 0 (caller emits literal)
 *
 * Handles *** by nesting: Strong wrapping Emphasis.
 */
static size_t try_emphasis(const IP *ip, size_t pos, size_t end, MkNode *parent) {
    const char *s = ip->src;
    char c = s[pos];
    if (c != '*' && c != '_') return 0;

    /* Count delimiter run */
    size_t run_start = pos;
    int    run_len   = 0;
    while (pos + (size_t)run_len < end && s[pos + (size_t)run_len] == c)
        run_len++;

    size_t run_end = run_start + (size_t)run_len;

    /* ── Left-flanking (opener) check ──────────────────────────────────────
     * A left-flanking delimiter run must:
     *   (1) not be followed by Unicode whitespace
     *   (2) either: not followed by Unicode punctuation
     *        OR:    followed by punctuation AND preceded by space/punctuation
     * ── For '_': additionally must not be preceded by alphanumeric ──────── */
    if (run_end < end && isspace((unsigned char)s[run_end])) return 0;
    if (run_end < end && ispunct((unsigned char)s[run_end])) {
        /* followed by punct — ok only if preceded by space or punct */
        int pre_ok = (run_start == 0
                      || isspace((unsigned char)s[run_start - 1])
                      || ispunct((unsigned char)s[run_start - 1]));
        if (!pre_ok) return 0;
    }
    if (c == '_' && run_start > 0 && isalnum((unsigned char)s[run_start - 1])) return 0;

    int strong  = (run_len >= 2);
    int consume = strong ? 2 : 1;
    size_t content_start = pos + (size_t)consume;

    /* Scan for a closing run */
    size_t p = content_start;
    while (p < end) {
        if (s[p] == c) {
            int crun = 0;
            while (p + (size_t)crun < end && s[p + (size_t)crun] == c) crun++;

            /* ── Right-flanking (closer) check ─────────────────────────────
             * Must not be preceded by whitespace.
             * If preceded by punctuation, must be followed by space/punctuation.
             * For '_': must not be followed by alphanumeric. */
            if (p == 0 || isspace((unsigned char)s[p - 1])) { p += (size_t)crun; continue; }
            if (ispunct((unsigned char)s[p - 1])) {
                size_t after = p + (size_t)crun;
                int fol_ok = (after >= end
                              || isspace((unsigned char)s[after])
                              || ispunct((unsigned char)s[after]));
                if (!fol_ok) { p += (size_t)crun; continue; }
            }
            if (c == '_' && p + (size_t)crun < end
                && isalnum((unsigned char)s[p + (size_t)crun])) { p += (size_t)crun; continue; }

            if (crun >= consume) {
                MkNodeType ntype = strong ? MK_NODE_STRONG : MK_NODE_EMPHASIS;
                MkNode *n = mk_node_alloc(ip->arena, sizeof(MkNode), ntype, 0);
                if (!n) return 0;
                mk_node_append_child(parent, n);
                iemit_open(ip, n);
                parse_seq(ip, content_start, p, n);
                iemit_close(ip, n);

                size_t new_pos = p + (size_t)consume;

                /* Leftover closers? Emit as literal text */
                if (crun > consume)
                    append_text(ip, parent, s + new_pos, (size_t)(crun - consume));

                return new_pos;
            }
        }
        p++;
    }
    return 0; /* no matching closer */
}

/* ── Strikethrough  ~~text~~ ──────────────────────────────────────────────── */

static size_t try_strikethrough(const IP *ip, size_t pos, size_t end,
                                MkNode *parent)
{
    const char *s = ip->src;
    if (pos + 1 >= end || s[pos] != '~' || s[pos+1] != '~') return 0;
    size_t p = pos + 2;
    while (p + 1 < end) {
        if (s[p] == '~' && s[p+1] == '~') {
            MkNode *n = mk_node_alloc(ip->arena, sizeof(MkNode),
                                      MK_NODE_STRIKETHROUGH, 0);
            if (!n) return 0;
            mk_node_append_child(parent, n);
            iemit_open(ip, n);
            parse_seq(ip, pos + 2, p, n);
            iemit_close(ip, n);
            return p + 2;
        }
        p++;
    }
    return 0;
}

/* ── Linebreak  (2+ spaces + \n → hard, \n → soft) ───────────────────────── */

static size_t try_linebreak(const IP *ip, size_t pos, size_t end,
                            MkNode *parent)
{
    (void)end;
    if (ip->src[pos] != '\n') return 0;
    /* Check if preceded by 2+ spaces: look back in parent's last text node */
    /* Simple heuristic: check src[pos-1] and src[pos-2] */
    int hard = (pos >= 2
                && ip->src[pos-1] == ' '
                && ip->src[pos-2] == ' ');
    MkNodeType t  = hard ? MK_NODE_HARD_BREAK : MK_NODE_SOFT_BREAK;
    MkNode    *br = mk_node_alloc(ip->arena, sizeof(MkNode), t, 0);
    if (!br) return 0;
    mk_node_append_child(parent, br);
    iemit_open (ip, br);
    iemit_close(ip, br);
    return pos + 1;
}

/* ── GFM task list item  [ ] or [x] at line start ────────────────────────── */

/*
 * Check if text starts with a task-list marker.
 * Only valid when the parent is (or is inside) a ListItem.
 * Returns 2 (unchecked) or 3 (checked) if detected, 0 otherwise.
 */
static size_t try_task_list(const IP *ip, size_t pos, size_t end,
                            MkNode *parent)
{
    /* Must be at the very start of the inline content */
    if (pos != 0) return 0;
    const char *s = ip->src;
    if (end < 3) return 0;
    if (s[0] != '[') return 0;
    char mid = s[1];
    if (mid != ' ' && mid != 'x' && mid != 'X') return 0;
    if (s[2] != ']') return 0;
    /* Must be followed by space or end */
    if (end > 3 && s[3] != ' ' && s[3] != '\t') return 0;

    /* Verify an ancestor is a ListItem */
    MkNode *p = parent;
    int in_list = 0;
    while (p) {
        if (p->type == MK_NODE_LIST_ITEM) { in_list = 1; break; }
        p = p->parent;
    }
    if (!in_list) return 0;

    MkTaskListItemNode *t = MK_NODE_NEW(ip->arena, MkTaskListItemNode,
                                        MK_NODE_TASK_LIST_ITEM, 0);
    if (!t) return 0;
    t->checked = (mid == 'x' || mid == 'X');
    mk_node_append_child(parent, &t->base);
    iemit_open (ip, &t->base);
    iemit_close(ip, &t->base);
    /* Skip marker + optional space */
    return (end > 3 && (s[3] == ' ' || s[3] == '\t')) ? 4 : 3;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Main recursive scanner
 * ════════════════════════════════════════════════════════════════════════════ */

static size_t parse_seq(const IP *ip, size_t pos, size_t end, MkNode *parent) {
    size_t text_start = pos;

    /* Task list check at very start */
    {
        size_t adv = try_task_list(ip, pos, end, parent);
        if (adv) { pos = adv; text_start = pos; }
    }

    while (pos < end) {
        char c = ip->src[pos];

        /* Fast path: accumulate plain text characters.
         * Also stop on any plugin trigger character. */
        if (c != '\\' && c != '`' && c != '*' && c != '_'
            && c != '~' && c != '['  && c != '!' && c != '<' && c != '\n'
            && !mk_plugin_is_inline_trigger(ip->parser, c))
        {
            pos++;
            continue;
        }

        /* Flush accumulated plain text before trying special construct */
        if (pos > text_start)
            append_text(ip, parent, ip->src + text_start, pos - text_start);
        text_start = pos;

        /* [F07] Delimiter-run fast-skip: for * and _ runs, if there is no
         * subsequent c character in the remaining span, no emphasis can ever
         * match.  Emit the entire run as literal text in O(1) instead of
         * retrying at each position in the run (which would be O(run_len²)).
         * This turns the worst-case adversarial run from O(N²) to O(N). */
        if (c == '*' || c == '_') {
            size_t rend = pos + 1;
            while (rend < end && ip->src[rend] == c) rend++;
            if (!memchr(ip->src + rend, (unsigned char)c, end - rend)) {
                append_text(ip, parent, ip->src + pos, rend - pos);
                pos = text_start = rend;
                continue;
            }
        }

        size_t new_pos = 0;

        switch (c) {
        case '\\': new_pos = try_escape           (ip, pos, end, parent); break;
        case '`':  new_pos = try_code_span        (ip, pos, end, parent); break;
        case '<':  new_pos = try_autolink_or_html (ip, pos, end, parent); break;
        case '!':  new_pos = try_image            (ip, pos, end, parent); break;
        case '[':  new_pos = try_link             (ip, pos, end, parent); break;
        case '*':
        case '_':  new_pos = try_emphasis         (ip, pos, end, parent); break;
        case '~':  new_pos = try_strikethrough    (ip, pos, end, parent); break;
        case '\n': new_pos = try_linebreak        (ip, pos, end, parent); break;
        default:   break;
        }

        if (new_pos > pos) {
            /* Standard construct: returned an absolute position */
            pos = text_start = new_pos;
        } else {
            /* Try plugins: they receive a relative slice, return relative offset */
            size_t plugin_adv = mk_plugin_try_inline(ip->parser, ip->arena, ip->cbs,
                                                     ip->src + pos, end - pos, parent);
            if (plugin_adv > 0) {
                pos = text_start = pos + plugin_adv;
            } else {
                /* No match: treat as literal character */
                pos++;
            }
        }
    }

    /* Flush remaining plain text */
    if (pos > text_start)
        append_text(ip, parent, ip->src + text_start, pos - text_start);

    return pos;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Inline AST walker — fires push callbacks in document order.
 *
 * Used by the two-pass mk_inline_parse: phase 1 builds the AST silently
 * (cbs=NULL), phase 2 walks and fires events.  This guarantees correct
 * event ordering even when emphasis nodes are created around already-parsed
 * children.
 * ════════════════════════════════════════════════════════════════════════════ */

static void inline_walk(const MkCallbacks *cbs, MkNode *n) {
    if (!n || !cbs) return;

    if (cbs->on_node_open) cbs->on_node_open(cbs->user_data, n);

    switch (n->type) {
    case MK_NODE_TEXT: {
        MkTextNode *tn = (MkTextNode *)n;
        if (cbs->on_text && tn->text)
            cbs->on_text(cbs->user_data, n, tn->text, tn->text_len);
        break;
    }
    case MK_NODE_INLINE_CODE: {
        MkInlineCodeNode *ic = (MkInlineCodeNode *)n;
        if (cbs->on_text && ic->text)
            cbs->on_text(cbs->user_data, n, ic->text, ic->text_len);
        break;
    }
    case MK_NODE_HTML_INLINE: {
        MkHtmlInlineNode *hi = (MkHtmlInlineNode *)n;
        if (cbs->on_text && hi->raw)
            cbs->on_text(cbs->user_data, n, hi->raw, hi->raw_len);
        break;
    }
    default:
        for (MkNode *child = n->first_child; child; child = child->next_sibling)
            inline_walk(cbs, child);
        break;
    }

    if (cbs->on_node_close) cbs->on_node_close(cbs->user_data, n);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public entry point
 * ════════════════════════════════════════════════════════════════════════════ */

void mk_inline_parse(MkArena           *arena,
                     const MkCallbacks *cbs,
                     MkNode            *parent,
                     const char        *text,
                     size_t             len,
                     MkParser          *parser)
{
    if (!text || !len || !parent) return;

    /* Phase 1: build the inline AST silently (no callbacks).
     * Using cbs=NULL makes iemit_open/close/text all no-ops.
     * This ensures correct event ordering when emphasis nodes wrap
     * content that was parsed before the closing delimiter was found. */
    IP ip = { arena, NULL, text, len, parser };
    parse_seq(&ip, 0, len, parent);

    /* Phase 2: walk the built subtree and fire events in document order. */
    if (cbs) {
        for (MkNode *child = parent->first_child; child; child = child->next_sibling)
            inline_walk(cbs, child);
    }
}
