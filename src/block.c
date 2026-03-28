/* block.c — Block-level parser state machine (M4)
 *
 * Algorithm follows CommonMark spec §4 (block structure):
 *   1. Buffer streaming input into complete lines
 *   2. Per line: consume continuation markers for open containers
 *   3. Try to open new container/leaf blocks
 *   4. Accumulate content in current leaf block
 *
 * Inline content is accumulated raw and passed to mk_inline_parse()
 * (implemented in M5; stub provided here).
 */

#include "block.h"
#include "inline_parser.h"
#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Text buffer helpers (malloc-backed: mutable during parsing)
 * ════════════════════════════════════════════════════════════════════════════ */

#define TEXT_INIT_CAP 256

static int textbuf_append(MkTextBuf *tb, const char *s, size_t n) {
    if (!n) return 0;
    if (tb->len + n > tb->cap) {
        size_t cap = tb->cap ? tb->cap : TEXT_INIT_CAP;
        while (cap < tb->len + n) cap <<= 1;
        char *p = realloc(tb->data, cap);
        if (!p) return -1;
        tb->data = p;
        tb->cap  = cap;
    }
    memcpy(tb->data + tb->len, s, n);
    tb->len += n;
    return 0;
}

static void textbuf_clear(MkTextBuf *tb) { tb->len = 0; }

static void textbuf_free(MkTextBuf *tb) {
    free(tb->data);
    tb->data = NULL;
    tb->len = tb->cap = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Line-scanning helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* Count leading spaces; tabs expand to next 4-stop. Returns indent width. */
static int count_indent(const char *s, size_t len, size_t *out_pos) {
    int  ind = 0;
    size_t i = 0;
    while (i < len) {
        if      (s[i] == ' ')  { ind++;              i++; }
        else if (s[i] == '\t') { ind = (ind+4)&~3;   i++; }
        else break;
    }
    if (out_pos) *out_pos = i;
    return ind;
}

static int is_blank(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (s[i] != ' ' && s[i] != '\t') return 0;
    return 1;
}

/* Remove trailing spaces/tabs in-place; returns new length. */
static size_t rtrim(const char *s, size_t len) {
    while (len && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
    return len;
}

/* ── Specific block type detectors ────────────────────────────────────────── */

/* HTML block detection (CommonMark §4.6 types 1–6).
 * Returns the HTML block type (1–6) if line starts an HTML block, or 0.
 * Type 1: <script / <pre / <style (case-insensitive)
 * Type 2: <!--
 * Type 3: <?
 * Type 4: <! followed by ASCII letter
 * Type 5: <![CDATA[
 * Type 6: one of the recognised block-level tags
 */
static int detect_html_block(const char *s, size_t len) {
    if (len < 2 || s[0] != '<') return 0;

    /* Type 2: <!-- */
    if (len >= 4 && s[1] == '!' && s[2] == '-' && s[3] == '-') return 2;

    /* Type 3: <? */
    if (s[1] == '?') return 3;

    /* Type 4: <! + ASCII letter */
    if (s[1] == '!' && len >= 3 && isalpha((unsigned char)s[2])) {
        /* Distinguish CDATA (type 5) */
        if (len >= 9 && memcmp(s, "<![CDATA[", 9) == 0) return 5;
        return 4;
    }

    /* Types 1 and 6 require a tag name */
    if (s[1] != '/' && !isalpha((unsigned char)s[1])) return 0;

    /* Extract tag name (after optional '/') */
    size_t i = (s[1] == '/') ? 2 : 1;
    size_t tag_start = i;
    while (i < len && (isalnum((unsigned char)s[i]) || s[i] == '-')) i++;
    size_t tag_len = i - tag_start;
    if (!tag_len) return 0;

    /* Case-insensitive tag name compare helper */
    char tag[32];
    if (tag_len >= sizeof(tag)) return 0;
    for (size_t j = 0; j < tag_len; j++)
        tag[j] = (char)tolower((unsigned char)s[tag_start + j]);
    tag[tag_len] = '\0';

    /* Type 1: <script, <pre, <style */
    if (strcmp(tag, "script") == 0 ||
        strcmp(tag, "pre")    == 0 ||
        strcmp(tag, "style")  == 0) return 1;

    /* Type 6: block-level tags that don't need a blank line to close */
    static const char *t6[] = {
        "address","article","aside","base","basefont","blockquote","body",
        "caption","center","col","colgroup","dd","details","dialog","dir",
        "div","dl","dt","fieldset","figcaption","figure","footer","form",
        "frame","frameset","h1","h2","h3","h4","h5","h6","head","header",
        "hr","html","iframe","legend","li","link","main","menu","menuitem",
        "meta","nav","noframes","ol","optgroup","option","p","param",
        "section","source","summary","table","tbody","td","tfoot","th",
        "thead","title","tr","track","ul",
        NULL
    };
    for (int k = 0; t6[k]; k++)
        if (strcmp(tag, t6[k]) == 0) return 6;

    return 0;
}

/* ATX heading: "## title ##" → level + trimmed title span.
 * Returns 0 if not a heading, else 1–6. */
static int detect_atx(const char *s, size_t len,
                      const char **txt, size_t *tlen)
{
    size_t i = 0;
    int level = 0;
    while (i < len && s[i] == '#' && level < 6) { level++; i++; }
    if (!level) return 0;
    if (i < len && s[i] != ' ' && s[i] != '\t') return 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;

    /* Trim trailing closing hashes */
    size_t end = rtrim(s, len);
    size_t j = end;
    while (j > i && s[j-1] == '#') j--;
    if (j < end) {
        if (j == i) {
            end = i;
        } else if (s[j-1] == ' ' || s[j-1] == '\t') {
            end = j;
            end = rtrim(s, end);
        }
    }
    *txt  = s + i;
    *tlen = end > i ? end - i : 0;
    return level;
}

/* Thematic break: 3+ of (- * _) with optional spaces, nothing else. */
static int detect_thematic(const char *s, size_t len) {
    if (!len) return 0;
    char c = s[0];
    if (c != '-' && c != '*' && c != '_') return 0;
    int cnt = 0;
    for (size_t i = 0; i < len; i++) {
        if      (s[i] == c)                        cnt++;
        else if (s[i] == ' ' || s[i] == '\t')     ;
        else                                       return 0;
    }
    return cnt >= 3;
}

/* Setext underline: line of = or - only. Returns 1 (h1) or 2 (h2), else 0. */
static int detect_setext(const char *s, size_t len) {
    if (!len) return 0;
    char c = s[0];
    if (c != '=' && c != '-') return 0;
    size_t i = 0;
    while (i < len && s[i] == c) i++;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i != len) return 0;
    return c == '=' ? 1 : 2;
}

/* Fenced code: ≥3 backticks or tildes + optional info string.
 * Returns fence length or 0. Sets out_char and info span. */
static int detect_fence(const char *s, size_t len, char *out_char,
                        const char **info, size_t *ilen)
{
    if (!len) return 0;
    char c = s[0];
    if (c != '`' && c != '~') return 0;
    int flen = 0;
    size_t i = 0;
    while (i < len && s[i] == c) { flen++; i++; }
    if (flen < 3) return 0;

    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    size_t is = i, ie = rtrim(s, len);
    if (c == '`')
        for (size_t j = is; j < ie; j++)
            if (s[j] == '`') return 0; /* backtick in info not allowed */

    if (out_char) *out_char = c;
    if (info)     *info     = s + is;
    if (ilen)     *ilen     = ie > is ? ie - is : 0;
    return flen;
}

/* Blockquote marker: optional spaces + '>'. Advances past optional space.
 * Returns 1 if found, sets rest/rlen to content after marker. */
static int detect_bq(const char *s, size_t len,
                     const char **rest, size_t *rlen)
{
    size_t i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t') && i < 3) i++;
    if (i >= len || s[i] != '>') return 0;
    i++;
    if (i < len && s[i] == ' ') i++;
    if (rest)  *rest  = s + i;
    if (rlen)  *rlen  = len - i;
    return 1;
}

/* List marker: bullet (- * +) or ordered (digits + '.' or ')').
 * Returns 1 if found. content_col = column of content start. */
static int detect_list_marker(const char *s, size_t len,
                               char *out_marker, int *ordered,
                               int *start, int *content_col)
{
    if (!len) return 0;

    /* Bullet */
    if (s[0] == '-' || s[0] == '*' || s[0] == '+') {
        if (len < 2 || (s[1] != ' ' && s[1] != '\t')) return 0;
        if (out_marker)   *out_marker   = s[0];
        if (ordered)      *ordered      = 0;
        if (start)        *start        = 0;
        if (content_col)  *content_col  = (s[1] == '\t') ? 5 : 2;
        return 1;
    }

    /* Ordered */
    size_t i = 0;
    int    n = 0;
    while (i < len && isdigit((unsigned char)s[i]) && i < 9)
        n = n * 10 + (s[i++] - '0');
    if (!i || i >= len) return 0;
    char d = s[i];
    if (d != '.' && d != ')') return 0;
    i++;
    if (i >= len || (s[i] != ' ' && s[i] != '\t')) return 0;

    if (out_marker)   *out_marker   = d;
    if (ordered)      *ordered      = 1;
    if (start)        *start        = n;
    if (content_col)  *content_col  = (int)i + 1;
    return 1;
}

/* Table separator row: |:---|:---:|---:| Returns column count or 0. */
static size_t detect_table_sep(const char *s, size_t len,
                               MkAlign *aligns, size_t max_cols)
{
    size_t i = 0, cols = 0;
    if (i < len && s[i] == '|') i++;

    while (i < len) {
        while (i < len && s[i] == ' ') i++;
        if (i >= len || s[i] == '\0') break;

        int lc = 0, rc = 0;
        if (i < len && s[i] == ':') { lc = 1; i++; }
        int dashes = 0;
        while (i < len && s[i] == '-') { dashes++; i++; }
        if (!dashes) return 0;
        if (i < len && s[i] == ':') { rc = 1; i++; }
        while (i < len && s[i] == ' ') i++;

        if (i < len && s[i] != '|' && s[i] != '\0') return 0;
        if (i < len && s[i] == '|') i++;

        if (cols < max_cols) {
            if      (lc && rc) aligns[cols] = MK_ALIGN_CENTER;
            else if (lc)       aligns[cols] = MK_ALIGN_LEFT;
            else if (rc)       aligns[cols] = MK_ALIGN_RIGHT;
            else               aligns[cols] = MK_ALIGN_NONE;
        }
        cols++;
    }
    return cols;
}

/* ── Table row parsing: split on |, return cell strings ──────────────────── */

typedef struct { const char *s; size_t len; } StrSpan;

static size_t split_table_row(const char *s, size_t len,
                              StrSpan *cells, size_t max)
{
    size_t i = 0, n = 0;
    if (i < len && s[i] == '|') i++;

    while (i < len && n < max) {
        size_t start = i;
        while (i < len && s[i] != '|') i++;
        cells[n].s   = s + start;
        cells[n].len = rtrim(s + start, i - start);
        /* ltrim */
        while (cells[n].len && (cells[n].s[0] == ' ' || cells[n].s[0] == '\t'))
            { cells[n].s++; cells[n].len--; }
        n++;
        if (i < len) i++; /* skip '|' */
    }
    /* Drop last empty cell from trailing pipe */
    if (n && !cells[n-1].len) n--;
    return n;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Event emission
 * ════════════════════════════════════════════════════════════════════════════ */

static void emit_open(MkBlockParser *bp, MkNode *n) {
    if (bp->cbs.on_node_open) bp->cbs.on_node_open(bp->cbs.user_data, n);
}

static void emit_close(MkBlockParser *bp, MkNode *n) {
    n->flags &= ~MK_NODE_FLAG_PENDING;
    if (bp->cbs.on_node_close) bp->cbs.on_node_close(bp->cbs.user_data, n);
}

static void emit_modify(MkBlockParser *bp, MkNode *n) {
    if (bp->cbs.on_node_modify) bp->cbs.on_node_modify(bp->cbs.user_data, n);
}

static void emit_text(MkBlockParser *bp, MkNode *n,
                      const char *text, size_t len) {
    if (bp->cbs.on_text) bp->cbs.on_text(bp->cbs.user_data, n, text, len);
}

/* ════════════════════════════════════════════════════════════════════════════
 * M5 inline parser stub
 * (replaced by real implementation in M5)
 * ════════════════════════════════════════════════════════════════════════════ */

static void parse_inline_content(MkBlockParser *bp, MkNode *parent,
                                 const char *text, size_t len)
{
    if (!len) return;
    /* bp->cbs.user_data is MkParser* when using the high-level API (NULL otherwise) */
    MkParser *parser = (MkParser *)bp->cbs.user_data;
    mk_inline_parse(bp->arena, &bp->cbs, parent, text, len, parser);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Block stack operations
 * ════════════════════════════════════════════════════════════════════════════ */

static MkBlockFrame *push_frame(MkBlockParser *bp, MkNode *node) {
    if (bp->top + 1 >= MK_BLOCK_STACK_MAX) return NULL;
    MkBlockFrame *f = &bp->stack[++bp->top];
    memset(f, 0, sizeof(*f));
    f->node = node;
    return f;
}

/* Close the innermost frame: flush text, emit close, pop. */
static void pop_frame(MkBlockParser *bp) {
    if (bp->top <= 0) return;
    MkBlockFrame *f = &bp->stack[bp->top];

    /* Flush accumulated inline text (not for blocks that use raw text) */
    MkNodeType nt = f->node->type;
    if (f->text.len &&
        nt != MK_NODE_CODE_BLOCK && nt != MK_NODE_HTML_BLOCK) {
        parse_inline_content(bp, f->node, f->text.data, f->text.len);
        textbuf_clear(&f->text);
    }

    emit_close(bp, f->node);
    textbuf_free(&f->text);
    bp->top--;
}

/* Close frames from bp->top down to (but not including) target_depth. */
static void pop_to(MkBlockParser *bp, int target_depth) {
    while (bp->top > target_depth)
        pop_frame(bp);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Node allocation shortcuts
 * ════════════════════════════════════════════════════════════════════════════ */

static MkNode *alloc_node(MkBlockParser *bp, size_t sz, MkNodeType t) {
    MkNode *n = mk_node_alloc(bp->arena, sz, t, 0);
    return n;
}

#define ALLOC(bp, T, TYPE) ((T*)alloc_node((bp), sizeof(T), (TYPE)))

/* ════════════════════════════════════════════════════════════════════════════
 * Table construction helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* Build TableHead → TableRow → TableCell nodes from a raw header line.
 * aligns[col] tells cell alignment. */
static void build_table_header(MkBlockParser *bp, MkNode *table_node,
                               const char *row_text, size_t row_len,
                               MkAlign *aligns, size_t col_count)
{
    MkNode *thead = alloc_node(bp, sizeof(MkNode), MK_NODE_TABLE_HEAD);
    mk_node_append_child(table_node, thead);
    emit_open(bp, thead);

    MkNode *trow = alloc_node(bp, sizeof(MkNode), MK_NODE_TABLE_ROW);
    mk_node_append_child(thead, trow);
    emit_open(bp, trow);

    StrSpan cells[MK_TABLE_COL_MAX];
    size_t  ncells = split_table_row(row_text, row_len, cells, MK_TABLE_COL_MAX);

    for (size_t c = 0; c < ncells; c++) {
        MkTableCellNode *cell = ALLOC(bp, MkTableCellNode, MK_NODE_TABLE_CELL);
        cell->col_index = c;
        cell->align     = c < col_count ? aligns[c] : MK_ALIGN_NONE;
        mk_node_append_child(trow, &cell->base);
        emit_open(bp, &cell->base);
        parse_inline_content(bp, &cell->base, cells[c].s, cells[c].len);
        emit_close(bp, &cell->base);
    }

    emit_close(bp, trow);
    emit_close(bp, thead);
}

/* Append a body row to a table node. */
static void build_table_row(MkBlockParser *bp, MkNode *table_node,
                            const char *row_text, size_t row_len,
                            MkAlign *aligns, size_t col_count)
{
    MkNode *trow = alloc_node(bp, sizeof(MkNode), MK_NODE_TABLE_ROW);
    mk_node_append_child(table_node, trow);
    emit_open(bp, trow);

    StrSpan cells[MK_TABLE_COL_MAX];
    size_t  ncells = split_table_row(row_text, row_len, cells, MK_TABLE_COL_MAX);

    for (size_t c = 0; c < ncells; c++) {
        MkTableCellNode *cell = ALLOC(bp, MkTableCellNode, MK_NODE_TABLE_CELL);
        cell->col_index = c;
        cell->align     = c < col_count ? aligns[c] : MK_ALIGN_NONE;
        mk_node_append_child(trow, &cell->base);
        emit_open(bp, &cell->base);
        parse_inline_content(bp, &cell->base, cells[c].s, cells[c].len);
        emit_close(bp, &cell->base);
    }

    emit_close(bp, trow);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Special-state line handlers
 * (Fenced code and HTML blocks consume lines wholesale)
 * ════════════════════════════════════════════════════════════════════════════ */

static int handle_fenced_content(MkBlockParser *bp,
                                 const char *line, size_t len)
{
    MkBlockFrame *f = &bp->stack[bp->top];

    /* Check for closing fence: same char, ≥ same length, no content after */
    size_t  pos;
    int     ind = count_indent(line, len, &pos);
    (void)ind;
    const char *s = line + pos;
    size_t      sl = len - pos;

    int close_len = 0;
    size_t i = 0;
    while (i < sl && s[i] == f->fence_char) { close_len++; i++; }
    if (close_len >= f->fence_len) {
        while (i < sl && (s[i] == ' ' || s[i] == '\t')) i++;
        if (i == sl) {
            /* Closing fence matched */
            pop_frame(bp);
            return 0;
        }
    }

    /* Regular code line: emit streaming text (no inline parsing) */
    emit_text(bp, f->node, line, len);
    emit_text(bp, f->node, "\n", 1);
    return 0;
}

static int handle_html_content(MkBlockParser *bp,
                               const char *line, size_t len)
{
    MkBlockFrame *f = &bp->stack[bp->top];
    int t = f->html_type;
    int end = 0;

    /* Check end conditions per HTML block type */
    if      (t == 1 && (memmem(line, len, "</script>", 9) ||
                        memmem(line, len, "</pre>",    6) ||
                        memmem(line, len, "</style>",  8))) end = 1;
    else if (t == 2 && memmem(line, len, "-->",  3))        end = 1;
    else if (t == 3 && memmem(line, len, "?>",   2))        end = 1;
    else if (t == 4 && memmem(line, len, ">",    1))        end = 1;
    else if (t == 5 && memmem(line, len, "]]>",  3))        end = 1;
    else if (t == 6 && is_blank(line, len))                 end = 1;

    if (!end) {
        emit_text(bp, f->node, line, len);
        emit_text(bp, f->node, "\n", 1);
        return 0;
    }
    /* Emit final line if not blank, then close */
    if (!is_blank(line, len)) {
        emit_text(bp, f->node, line, len);
    }
    pop_frame(bp);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Blank line handler
 * ════════════════════════════════════════════════════════════════════════════ */

static int handle_blank(MkBlockParser *bp) {
    MkBlockFrame *cur = &bp->stack[bp->top];

    /* Mark blank seen for list tightness tracking */
    cur->blank_seen = 1;

    /* Paragraph ends on blank line */
    if (cur->node->type == MK_NODE_PARAGRAPH) {
        pop_frame(bp);
        return 0;
    }

    /* Indented code block ends on blank line */
    if (cur->node->type == MK_NODE_CODE_BLOCK && cur->fence_char == 0) {
        pop_frame(bp);
        return 0;
    }

    /* GFM table ends on blank line */
    if (cur->node->type == MK_NODE_TABLE) {
        pop_frame(bp);
        return 0;
    }

    bp->last_blank = 1;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Main line processor
 * ════════════════════════════════════════════════════════════════════════════ */

static int process_line(MkBlockParser *bp, const char *line, size_t len) {
    /* Detect hard-break BEFORE rtrim: two or more trailing spaces */
    int hard_break = 0;
    if (len >= 2 && line[len-1] == ' ' && line[len-2] == ' ')
        hard_break = 1;

    len = rtrim(line, len);

    /* ── Special states that swallow the whole line ── */
    {
        MkBlockFrame *top = &bp->stack[bp->top];

        if (top->node->type == MK_NODE_CODE_BLOCK && top->fence_char != 0)
            return handle_fenced_content(bp, line, len);

        if (top->node->type == MK_NODE_HTML_BLOCK)
            return handle_html_content(bp, line, len);
    }

    /* ── Blank line ── */
    if (is_blank(line, len))
        return handle_blank(bp);

    bp->last_blank = 0;

    /* ── Scan leading indent and content start ── */
    size_t      pos;
    int         indent = count_indent(line, len, &pos);
    const char *s  = line + pos;
    size_t      sl = len  - pos;

    /* ── Container continuation: blockquote, list item ── */
    /* Walk stack from outermost to innermost container and try to continue.
     * We do a simple one-level lookahead; lazy continuation is allowed for
     * paragraphs inside blockquotes. */

    for (int d = 1; d <= bp->top; d++) {
        MkBlockFrame *f = &bp->stack[d];

        if (f->node->type == MK_NODE_BLOCK_QUOTE) {
            /* Blockquote continuation needs '>' prefix */
            const char *rest; size_t rlen;
            if (detect_bq(line, len, &rest, &rlen)) {
                /* Rebase analysis on the content after '>' */
                indent = count_indent(rest, rlen, &pos);
                s  = rest + pos;
                sl = rlen - pos;
            } else if (bp->stack[bp->top].node->type != MK_NODE_PARAGRAPH) {
                /* Not lazy: close everything above the blockquote's parent */
                pop_to(bp, d - 1);
                break;
            }
        }

        if (f->node->type == MK_NODE_LIST_ITEM) {
            /* List item continuation: content must be indented past list_indent */
            if (indent < f->list_indent) {
                pop_to(bp, d - 1);
                break;
            }
            /* Strip the list item's indent */
            size_t stripped = 0;
            const char *p = line;
            size_t plen = len;
            int counted = 0;
            while (stripped < plen && counted < f->list_indent) {
                if (p[stripped] == ' ')      counted++;
                else if (p[stripped] == '\t') counted = (counted+4)&~3;
                stripped++;
            }
            indent -= f->list_indent;
            s   = p + stripped;
            sl  = plen - stripped;
            pos = stripped;
        }
    }

    /* Re-derive current leaf */
    MkBlockFrame *cur = &bp->stack[bp->top];

    /* ── Close bare List if next line is not a list marker ── */
    if (cur->node->type == MK_NODE_LIST) {
        char mk2; int ord2, start2, ccol2;
        if (!detect_list_marker(s, sl, &mk2, &ord2, &start2, &ccol2)) {
            pop_frame(bp);
            cur = &bp->stack[bp->top];
        }
    }

    /* ── Setext heading promotion ── */
    if (cur->node->type == MK_NODE_PARAGRAPH && cur->setext_candidate) {
        int slevel = detect_setext(s, sl);
        if (slevel) {
            /* Promote paragraph to heading */
            MkHeadingNode *h = ALLOC(bp, MkHeadingNode, MK_NODE_HEADING);
            h->level = slevel;

            /* Swap the paragraph node on the stack */
            MkNode *para        = cur->node;
            MkNode *para_parent = para->parent;   /* save before detach */
            mk_node_detach(para);
            mk_node_append_child(para_parent, &h->base);
            cur->node = &h->base;

            /* Emit: close the old paragraph open (we'll emit heading instead) */
            /* Actually we held back: paragraph was emitted open already.
             * We emit modify to signal type change. */
            h->base.flags = para->flags;
            emit_modify(bp, &h->base);

            /* Flush text as heading inline content */
            if (cur->text.len)
                parse_inline_content(bp, &h->base,
                                     cur->text.data, cur->text.len);
            textbuf_clear(&cur->text);

            pop_to(bp, bp->top - 1);
            emit_close(bp, &h->base);
            return 0;
        }
    }

    /* ── GFM table promotion ── */
    if (cur->node->type == MK_NODE_PARAGRAPH && cur->table_candidate) {
        MkAlign aligns[MK_TABLE_COL_MAX];
        size_t col_count = detect_table_sep(s, sl, aligns, MK_TABLE_COL_MAX);
        if (col_count) {
            /* Promote paragraph to table */
            MkTableNode *tbl = ALLOC(bp, MkTableNode, MK_NODE_TABLE);
            tbl->col_count  = col_count;
            tbl->col_aligns = mk_arena_stable_alloc(
                bp->arena, col_count * sizeof(MkAlign));
            if (tbl->col_aligns)
                memcpy(tbl->col_aligns, aligns, col_count * sizeof(MkAlign));

            MkNode *para        = cur->node;
            MkNode *para_parent = para->parent;   /* save before detach */
            mk_node_detach(para);
            mk_node_append_child(para_parent, &tbl->base);
            cur->node = &tbl->base;
            emit_modify(bp, &tbl->base);

            /* Build header from accumulated paragraph text */
            if (cur->text.len)
                build_table_header(bp, &tbl->base,
                                   cur->text.data, cur->text.len,
                                   aligns, col_count);
            textbuf_clear(&cur->text);
            cur->setext_candidate = 0;
            cur->table_candidate  = 0;
            /* Don't pop; we stay in the table frame */
            return 0;
        }
    }

    /* ── Append table body row ── */
    if (cur->node->type == MK_NODE_TABLE) {
        /* Blank line handled above → table already closed.
         * Any non-separator, non-blank line is a body row. */
        MkTableNode *tbl = (MkTableNode *)cur->node;
        if (!is_blank(s, sl))
            build_table_row(bp, cur->node, s, sl,
                            tbl->col_aligns, tbl->col_count);
        return 0;
    }

    /* ══════════════════════════════════════════════════════════════════════
     * Try to open new block structures
     * ══════════════════════════════════════════════════════════════════════ */

    /* HTML block — interrupts paragraph only for type 1–6 that are block-level */
    {
        int htype = detect_html_block(s, sl);
        if (htype) {
            if (cur->node->type == MK_NODE_PARAGRAPH) pop_frame(bp);
            cur = &bp->stack[bp->top];

            MkHtmlBlockNode *hb = ALLOC(bp, MkHtmlBlockNode, MK_NODE_HTML_BLOCK);
            hb->html_type = htype;
            mk_node_append_child(cur->node, &hb->base);
            emit_open(bp, &hb->base);

            MkBlockFrame *hf = push_frame(bp, &hb->base);
            if (hf) hf->html_type = htype;

            /* First line of the HTML block is already content */
            emit_text(bp, &hb->base, line, len);
            emit_text(bp, &hb->base, "\n", 1);

            /* Types 1–5 may end on the same line */
            int end = 0;
            if      (htype == 1 && (memmem(line, len, "</script>", 9) ||
                                    memmem(line, len, "</pre>",    6) ||
                                    memmem(line, len, "</style>",  8))) end = 1;
            else if (htype == 2 && memmem(line, len, "-->",  3))        end = 1;
            else if (htype == 3 && memmem(line, len, "?>",   2))        end = 1;
            else if (htype == 4 && memmem(line, len, ">",    1))        end = 1;
            else if (htype == 5 && memmem(line, len, "]]>",  3))        end = 1;
            if (end) pop_frame(bp);
            return 0;
        }
    }

    /* ATX heading — always interrupts paragraph */
    {
        const char *txt; size_t tlen;
        int level = detect_atx(s, sl, &txt, &tlen);
        if (level) {
            pop_to(bp, (cur->node->type == MK_NODE_PARAGRAPH) ? bp->top - 1 : bp->top);

            MkHeadingNode *h = ALLOC(bp, MkHeadingNode, MK_NODE_HEADING);
            h->level = level;
            mk_node_append_child(bp->stack[bp->top].node, &h->base);
            emit_open(bp, &h->base);
            if (tlen) parse_inline_content(bp, &h->base, txt, tlen);
            emit_close(bp, &h->base);
            return 0;
        }
    }

    /* Thematic break — interrupts paragraph */
    if (detect_thematic(s, sl)) {
        if (cur->node->type == MK_NODE_PARAGRAPH) pop_frame(bp);
        MkNode *tb = alloc_node(bp, sizeof(MkNode), MK_NODE_THEMATIC_BREAK);
        mk_node_append_child(bp->stack[bp->top].node, tb);
        emit_open(bp, tb);
        emit_close(bp, tb);
        return 0;
    }

    /* Fenced code block — interrupts paragraph */
    {
        char       fc; const char *info; size_t ilen;
        int flen = detect_fence(s, sl, &fc, &info, &ilen);
        if (flen) {
            if (cur->node->type == MK_NODE_PARAGRAPH) pop_frame(bp);

            MkCodeBlockNode *cb = ALLOC(bp, MkCodeBlockNode, MK_NODE_CODE_BLOCK);
            cb->fenced = 1;
            if (ilen) {
                cb->lang     = mk_arena_strdup_stable(bp->arena, info, ilen);
                cb->lang_len = ilen;
            }
            mk_node_append_child(bp->stack[bp->top].node, &cb->base);
            emit_open(bp, &cb->base);

            MkBlockFrame *f = push_frame(bp, &cb->base);
            if (f) {
                f->fence_char = fc;
                f->fence_len  = flen;
            }
            return 0;
        }
    }

    /* Block quote */
    {
        const char *rest; size_t rlen;
        if (detect_bq(line, len, &rest, &rlen)) {
            /* Open new blockquote unless already inside one at this depth */
            if (cur->node->type != MK_NODE_BLOCK_QUOTE) {
                if (cur->node->type == MK_NODE_PARAGRAPH) pop_frame(bp);
                MkNode *bq = alloc_node(bp, sizeof(MkNode), MK_NODE_BLOCK_QUOTE);
                mk_node_append_child(bp->stack[bp->top].node, bq);
                emit_open(bp, bq);
                push_frame(bp, bq);
            }
            /* Recurse with content after '>' */
            return process_line(bp, rest, rlen);
        }
    }

    /* List item */
    {
        char mk; int ord, start, ccol;
        if (detect_list_marker(s, sl, &mk, &ord, &start, &ccol)) {
            /* Open list if needed, or continue existing same-type list */
            MkBlockFrame *pcur = &bp->stack[bp->top];
            int in_list = (pcur->node->type == MK_NODE_LIST_ITEM);

            if (!in_list) {
                if (pcur->node->type == MK_NODE_PARAGRAPH) pop_frame(bp);
                if (bp->stack[bp->top].node->type != MK_NODE_LIST) {
                    MkListNode *lst = ALLOC(bp, MkListNode, MK_NODE_LIST);
                    lst->ordered = ord;
                    lst->start   = start;
                    mk_node_append_child(bp->stack[bp->top].node, &lst->base);
                    emit_open(bp, &lst->base);
                    push_frame(bp, &lst->base);
                }
            } else {
                /* Close previous list item */
                pop_frame(bp);
            }

            MkListItemNode *li = ALLOC(bp, MkListItemNode, MK_NODE_LIST_ITEM);
            mk_node_append_child(bp->stack[bp->top].node, &li->base);
            emit_open(bp, &li->base);
            MkBlockFrame *lf = push_frame(bp, &li->base);
            if (lf) {
                lf->list_indent  = indent + ccol;
                lf->list_ordered = ord;
            }

            /* First-line content after marker: open paragraph directly.
             * Do NOT recurse into process_line — the continuation check
             * would fire for the freshly-pushed ListItem frame. */
            const char *after = s + ccol;
            size_t      alen  = sl > (size_t)ccol ? sl - ccol : 0;
            if (alen) {
                MkNode *para = alloc_node(bp, sizeof(MkNode), MK_NODE_PARAGRAPH);
                mk_node_append_child(bp->stack[bp->top].node, para);
                emit_open(bp, para);
                MkBlockFrame *pf = push_frame(bp, para);
                if (pf) {
                    textbuf_append(&pf->text, after, alen);
                    pf->setext_candidate = 1;
                    pf->table_candidate  = (memchr(after, '|', alen) != NULL);
                }
            }
            return 0;
        }
    }

    /* Indented code block (4 spaces; not inside paragraph) */
    if (indent >= 4 && cur->node->type != MK_NODE_PARAGRAPH) {
        if (cur->node->type != MK_NODE_CODE_BLOCK || cur->fence_char != 0) {
            MkCodeBlockNode *cb = ALLOC(bp, MkCodeBlockNode, MK_NODE_CODE_BLOCK);
            cb->fenced = 0;
            mk_node_append_child(bp->stack[bp->top].node, &cb->base);
            emit_open(bp, &cb->base);
            MkBlockFrame *cf = push_frame(bp, &cb->base);
            if (cf) cf->fence_char = 0; /* indented: no fence char */
        }
        /* Strip 4 spaces of indent */
        const char *cline = (indent >= 4) ? (line + pos - (indent - 4)) : line;
        size_t      clen2 = len  - (size_t)(cline - line);
        MkBlockFrame *cf  = &bp->stack[bp->top];
        textbuf_append(&cf->text, cline, clen2);
        textbuf_append(&cf->text, "\n", 1);
        emit_text(bp, cf->node, cline, clen2);
        return 0;
    }

    /* ── Parser plugin block try ── */
    {
        MkParser *parser = (MkParser *)bp->cbs.user_data;
        MkNode   *plug_node = NULL;
        size_t    consumed  = mk_plugin_try_block(parser, bp->arena, &bp->cbs,
                                                  s, sl, &plug_node);
        if (consumed && plug_node) {
            if (cur->node->type == MK_NODE_PARAGRAPH) pop_frame(bp);
            cur = &bp->stack[bp->top];
            mk_node_append_child(cur->node, plug_node);
            emit_open(bp, plug_node);
            emit_close(bp, plug_node);
            return 0;
        }
    }

    /* ── Paragraph / continuation ── */
    cur = &bp->stack[bp->top];

    if (cur->node->type == MK_NODE_PARAGRAPH) {
        /* Append to existing paragraph.
         * Use "  \n" (two spaces + newline) as hard-break sentinel so the
         * inline parser can emit MK_NODE_HARD_BREAK; plain "\n" → soft break. */
        if (cur->text.len) {
            if (cur->last_line_hard_break)
                textbuf_append(&cur->text, "  \n", 3);
            else
                textbuf_append(&cur->text, "\n", 1);
        }
        cur->last_line_hard_break = hard_break;
        textbuf_append(&cur->text, s, sl);

        /* Detect if this single-line paragraph might become table header */
        if (cur->table_candidate) cur->table_candidate = 0; /* 2nd+ line: can't be table */
        cur->setext_candidate = 1; /* could become setext heading */
    } else {
        /* Open a new paragraph */
        MkNode *para = alloc_node(bp, sizeof(MkNode), MK_NODE_PARAGRAPH);
        mk_node_append_child(bp->stack[bp->top].node, para);
        emit_open(bp, para);
        MkBlockFrame *pf = push_frame(bp, para);
        if (pf) {
            textbuf_append(&pf->text, s, sl);
            pf->setext_candidate = 1;
            /* Table candidate: line contains '|' */
            pf->table_candidate       = (memchr(s, '|', sl) != NULL);
            pf->last_line_hard_break  = hard_break;
        }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public API
 * ════════════════════════════════════════════════════════════════════════════ */

void mk_block_init(MkBlockParser *bp, MkArena *arena, const MkCallbacks *cbs) {
    memset(bp, 0, sizeof(*bp));
    bp->arena = arena;
    if (cbs) bp->cbs = *cbs;

    /* Root Document node */
    MkNode *doc = mk_node_alloc(arena, sizeof(MkNode), MK_NODE_DOCUMENT, 0);
    doc->flags &= ~MK_NODE_FLAG_PENDING;
    bp->stack[0].node = doc;
    bp->top = 0;
    emit_open(bp, doc);
}

void mk_block_cleanup(MkBlockParser *bp) {
    for (int i = 0; i <= bp->top; i++)
        textbuf_free(&bp->stack[i].text);
}

int mk_block_feed(MkBlockParser *bp, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\r') continue;
        if (c == '\n') {
            int rc = process_line(bp, bp->line_buf, bp->line_len);
            bp->line_len      = 0;
            bp->line_overflow = 0;
            bp->src_offset++;
            if (rc) return rc;
        } else {
            if (bp->line_len < MK_BLOCK_LINE_MAX - 1) {
                bp->line_buf[bp->line_len++] = c;
            } else if (!bp->line_overflow) {
                bp->line_overflow = 1;
                if (bp->cbs.on_error)
                    bp->cbs.on_error(bp->cbs.user_data, MK_ERR_LINE_TOO_LONG,
                        "input line exceeds MK_BLOCK_LINE_MAX bytes; excess bytes truncated");
            }
        }
        bp->src_offset++;
    }
    return 0;
}

int mk_block_finish(MkBlockParser *bp) {
    if (bp->line_len) {
        int rc = process_line(bp, bp->line_buf, bp->line_len);
        bp->line_len = 0;
        if (rc) return rc;
    }
    pop_to(bp, 0);
    emit_close(bp, bp->stack[0].node);
    return 0;
}

MkNode *mk_block_root(MkBlockParser *bp) {
    return bp->stack[0].node;
}
