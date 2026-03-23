/* test_block.c — Unit tests for Block parser (M4) */

#include "../../include/mk_parser.h"
#include "../../src/ast.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PASS(name) printf("  PASS: %s\n", (name))

/* ── event collection ─────────────────────────────────────────────────────── */

#define MAX_EVENTS 256

typedef enum { EV_OPEN, EV_CLOSE, EV_TEXT, EV_MODIFY } EvKind;

typedef struct {
    EvKind     kind;
    MkNodeType node_type;
    char       text[128];
} Event;

typedef struct {
    Event events[MAX_EVENTS];
    int   count;
} EventLog;

static void ev_open(void *ud, MkNode *n) {
    EventLog *log = (EventLog *)ud;
    if (log->count >= MAX_EVENTS) return;
    log->events[log->count].kind      = EV_OPEN;
    log->events[log->count].node_type = n->type;
    log->count++;
}

static void ev_close(void *ud, MkNode *n) {
    EventLog *log = (EventLog *)ud;
    if (log->count >= MAX_EVENTS) return;
    log->events[log->count].kind      = EV_CLOSE;
    log->events[log->count].node_type = n->type;
    log->count++;
}

static void ev_text(void *ud, MkNode *n, const char *text, size_t len) {
    EventLog *log = (EventLog *)ud;
    if (log->count >= MAX_EVENTS) return;
    log->events[log->count].kind      = EV_TEXT;
    log->events[log->count].node_type = n->type;
    size_t copy = len < 127 ? len : 127;
    memcpy(log->events[log->count].text, text, copy);
    log->events[log->count].text[copy] = '\0';
    log->count++;
}

static void ev_modify(void *ud, MkNode *n) {
    EventLog *log = (EventLog *)ud;
    if (log->count >= MAX_EVENTS) return;
    log->events[log->count].kind      = EV_MODIFY;
    log->events[log->count].node_type = n->type;
    log->count++;
}

static void parse_full(const char *md, EventLog *log) {
    MkArena  *a = mk_arena_new();
    MkCallbacks cbs = {
        .user_data      = log,
        .on_node_open   = ev_open,
        .on_node_close  = ev_close,
        .on_text        = ev_text,
        .on_node_modify = ev_modify,
    };
    MkParser *p = mk_parser_new(a, &cbs);
    mk_feed(p, md, strlen(md));
    mk_finish(p);
    mk_parser_free(p);
    mk_arena_free(a);
}

/* Find first OPEN event of given type, return index or -1 */
static int find_open(const EventLog *log, MkNodeType t) {
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_OPEN && log->events[i].node_type == t)
            return i;
    return -1;
}

/* Count OPEN events of given type */
static int count_open(const EventLog *log, MkNodeType t) {
    int n = 0;
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_OPEN && log->events[i].node_type == t)
            n++;
    return n;
}

/* Find first MODIFY event of given type (used for promoted nodes: table, setext) */
static int find_modify(const EventLog *log, MkNodeType t) {
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_MODIFY && log->events[i].node_type == t)
            return i;
    return -1;
}

/* ── tests ────────────────────────────────────────────────────────────────── */

static void test_paragraph(void) {
    EventLog log = {.count = 0};
    parse_full("Hello, world.\n", &log);
    assert(find_open(&log, MK_NODE_PARAGRAPH) >= 0);
    PASS("paragraph");
}

static void test_atx_headings(void) {
    EventLog log = {.count = 0};
    parse_full("# H1\n## H2\n### H3\n", &log);
    assert(count_open(&log, MK_NODE_HEADING) == 3);
    PASS("atx_headings");
}

static void test_fenced_code_block(void) {
    EventLog log = {.count = 0};
    parse_full("```rust\nfn main() {}\n```\n", &log);
    assert(find_open(&log, MK_NODE_CODE_BLOCK) >= 0);
    PASS("fenced_code_block");
}

static void test_indented_code_block(void) {
    EventLog log = {.count = 0};
    parse_full("    code line\n", &log);
    assert(find_open(&log, MK_NODE_CODE_BLOCK) >= 0);
    PASS("indented_code_block");
}

static void test_blockquote(void) {
    EventLog log = {.count = 0};
    parse_full("> quoted text\n", &log);
    assert(find_open(&log, MK_NODE_BLOCK_QUOTE) >= 0);
    PASS("blockquote");
}

static void test_unordered_list(void) {
    EventLog log = {.count = 0};
    parse_full("- item1\n- item2\n- item3\n", &log);
    assert(find_open(&log, MK_NODE_LIST) >= 0);
    assert(count_open(&log, MK_NODE_LIST_ITEM) == 3);
    PASS("unordered_list");
}

static void test_ordered_list(void) {
    EventLog log = {.count = 0};
    parse_full("1. first\n2. second\n", &log);
    assert(find_open(&log, MK_NODE_LIST) >= 0);
    assert(count_open(&log, MK_NODE_LIST_ITEM) == 2);
    PASS("ordered_list");
}

static void test_thematic_break(void) {
    EventLog log = {.count = 0};
    parse_full("---\n", &log);
    assert(find_open(&log, MK_NODE_THEMATIC_BREAK) >= 0);
    PASS("thematic_break");
}

static void test_html_block(void) {
    EventLog log = {.count = 0};
    /* Type 6 HTML block: opening with a recognised block-level tag */
    parse_full("<div>\ncontent\n</div>\n\n", &log);
    assert(find_open(&log, MK_NODE_HTML_BLOCK) >= 0);
    PASS("html_block");
}

static void test_table_gfm(void) {
    EventLog log = {.count = 0};
    parse_full("| A | B |\n|---|---|\n| 1 | 2 |\n", &log);
    /* Table is announced via MODIFY (paragraph promoted to table) */
    assert(find_modify(&log, MK_NODE_TABLE) >= 0);
    PASS("table_gfm");
}

static void test_setext_heading(void) {
    EventLog log = {.count = 0};
    parse_full("Title\n=====\n", &log);
    /* Setext heading announced via MODIFY (paragraph promoted to heading) */
    assert(find_modify(&log, MK_NODE_HEADING) >= 0);
    PASS("setext_heading");
}

static void test_nested_blockquote(void) {
    EventLog log = {.count = 0};
    /* Multi-line nested blockquote: first line in outer BQ,
     * second line continues outer BQ and opens inner BQ */
    parse_full("> outer\n> > inner\n", &log);
    assert(count_open(&log, MK_NODE_BLOCK_QUOTE) >= 2);
    PASS("nested_blockquote");
}

static void test_multiple_paragraphs(void) {
    EventLog log = {.count = 0};
    parse_full("First.\n\nSecond.\n\nThird.\n", &log);
    assert(count_open(&log, MK_NODE_PARAGRAPH) == 3);
    PASS("multiple_paragraphs");
}

static void test_empty_input(void) {
    EventLog log = {.count = 0};
    parse_full("", &log);
    /* Should not crash, document node emitted */
    assert(find_open(&log, MK_NODE_DOCUMENT) >= 0);
    PASS("empty_input");
}

static void test_document_open_close(void) {
    EventLog log = {.count = 0};
    parse_full("text\n", &log);
    assert(find_open(&log, MK_NODE_DOCUMENT) >= 0);
    /* CLOSE for document must also appear */
    int found_close = 0;
    for (int i = 0; i < log.count; i++)
        if (log.events[i].kind == EV_CLOSE && log.events[i].node_type == MK_NODE_DOCUMENT)
            found_close = 1;
    assert(found_close);
    PASS("document_open_close");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Block parser unit tests ===\n");
    test_paragraph();
    test_atx_headings();
    test_fenced_code_block();
    test_indented_code_block();
    test_blockquote();
    test_unordered_list();
    test_ordered_list();
    test_thematic_break();
    test_html_block();
    test_table_gfm();
    test_setext_heading();
    test_nested_blockquote();
    test_multiple_paragraphs();
    test_empty_input();
    test_document_open_close();
    printf("All block tests passed.\n\n");
    return 0;
}
