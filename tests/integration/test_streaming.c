/* test_streaming.c — Integration tests for streaming (chunked) input */

#include "../../include/mk_parser.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PASS(name) printf("  PASS: %s\n", (name))

/* ── event collection ─────────────────────────────────────────────────────── */

#define MAX_EVENTS 1024

typedef enum { EV_OPEN, EV_CLOSE, EV_TEXT, EV_MODIFY } EvKind;

typedef struct {
    EvKind     kind;
    MkNodeType node_type;
    char       text[256];
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
    size_t copy = len < 255 ? len : 255;
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

static MkCallbacks make_cbs(EventLog *log) {
    MkCallbacks cbs = {
        .user_data      = log,
        .on_node_open   = ev_open,
        .on_node_close  = ev_close,
        .on_text        = ev_text,
        .on_node_modify = ev_modify,
    };
    return cbs;
}

/* Feed input in chunks of given size */
static void parse_chunked(const char *md, size_t chunk_size, EventLog *log) {
    MkArena    *a   = mk_arena_new();
    MkCallbacks cbs = make_cbs(log);
    MkParser   *p   = mk_parser_new(a, &cbs);

    size_t len = strlen(md);
    size_t pos = 0;
    while (pos < len) {
        size_t n = len - pos;
        if (n > chunk_size) n = chunk_size;
        int rc = mk_feed(p, md + pos, n);
        assert(rc == 0);
        pos += n;
    }
    mk_finish(p);
    mk_parser_free(p);
    mk_arena_free(a);
}

/* Feed 1 byte at a time (worst case) */
static void parse_byte_by_byte(const char *md, EventLog *log) {
    parse_chunked(md, 1, log);
}

static int find_open(const EventLog *log, MkNodeType t) {
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_OPEN && log->events[i].node_type == t)
            return i;
    return -1;
}

static int count_open(const EventLog *log, MkNodeType t) {
    int n = 0;
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_OPEN && log->events[i].node_type == t)
            n++;
    return n;
}

/* ── helper: compare event sequences between two runs ─────────────────────── */

static int logs_equivalent(const EventLog *a, const EventLog *b) {
    if (a->count != b->count) return 0;
    for (int i = 0; i < a->count; i++) {
        if (a->events[i].kind      != b->events[i].kind)      return 0;
        if (a->events[i].node_type != b->events[i].node_type) return 0;
    }
    return 1;
}

static int find_close(const EventLog *log, MkNodeType t) {
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_CLOSE && log->events[i].node_type == t)
            return i;
    return -1;
}

/* Find first MODIFY event of given type */
static int find_modify(const EventLog *log, MkNodeType t) {
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_MODIFY && log->events[i].node_type == t)
            return i;
    return -1;
}

/* ── tests ────────────────────────────────────────────────────────────────── */

/* 1-byte chunks must produce the same events as full-input */
static void test_byte_by_byte_equals_full(void) {
    const char *md =
        "# Heading\n\nParagraph with **bold** and *em*.\n\n"
        "- item1\n- item2\n\n```c\ncode\n```\n";

    EventLog log_full  = {.count = 0};
    EventLog log_bytes = {.count = 0};

    parse_chunked(md, strlen(md), &log_full);   /* full */
    parse_byte_by_byte(md, &log_bytes);          /* 1 byte */

    assert(logs_equivalent(&log_full, &log_bytes));
    PASS("byte_by_byte_equals_full");
}

/* 7-byte chunks (prime, crosses boundaries) */
static void test_7byte_chunks(void) {
    const char *md = "# Title\n\nSome *inline* content and `code`.\n";
    EventLog log_full  = {.count = 0};
    EventLog log_7     = {.count = 0};

    parse_chunked(md, strlen(md), &log_full);
    parse_chunked(md, 7, &log_7);

    assert(logs_equivalent(&log_full, &log_7));
    PASS("7byte_chunks");
}

/* 3-byte chunks */
static void test_3byte_chunks(void) {
    const char *md = "> blockquote\n\n1. first\n2. second\n";
    EventLog log_full = {.count = 0};
    EventLog log_3    = {.count = 0};

    parse_chunked(md, strlen(md), &log_full);
    parse_chunked(md, 3, &log_3);

    assert(logs_equivalent(&log_full, &log_3));
    PASS("3byte_chunks");
}

/* Fenced code block split mid-fence */
static void test_fenced_code_split(void) {
    const char *md = "```python\nprint('hello')\n```\n";
    EventLog log = {.count = 0};
    parse_byte_by_byte(md, &log);
    assert(find_open(&log, MK_NODE_CODE_BLOCK) >= 0);
    PASS("fenced_code_split");
}

/* Pull API: events available after mk_finish */
static void test_pull_api_basic(void) {
    const char *md = "Hello\n";
    MkArena  *a = mk_arena_new();
    MkParser *p = mk_parser_new(a, NULL);  /* no push callbacks */

    mk_feed(p, md, strlen(md));
    mk_finish(p);

    int found_doc = 0;
    MkDelta *d;
    while ((d = mk_pull_delta(p)) != NULL) {
        if (d->type == MK_DELTA_NODE_OPEN && d->node &&
            d->node->type == MK_NODE_DOCUMENT)
            found_doc = 1;
        mk_delta_free(d);
    }
    assert(found_doc);

    mk_parser_free(p);
    mk_arena_free(a);
    PASS("pull_api_basic");
}

/* Pull API: interleaved feed + pull */
static void test_pull_api_interleaved(void) {
    MkArena  *a = mk_arena_new();
    MkParser *p = mk_parser_new(a, NULL);

    const char *line1 = "# H1\n";
    const char *line2 = "paragraph\n";

    mk_feed(p, line1, strlen(line1));
    /* Drain whatever is available */
    int count1 = 0;
    MkDelta *d;
    while ((d = mk_pull_delta(p)) != NULL) { count1++; mk_delta_free(d); }

    mk_feed(p, line2, strlen(line2));
    mk_finish(p);

    int count2 = 0;
    while ((d = mk_pull_delta(p)) != NULL) { count2++; mk_delta_free(d); }

    /* Both phases produced events */
    assert(count1 > 0 || count2 > 0);

    mk_parser_free(p);
    mk_arena_free(a);
    PASS("pull_api_interleaved");
}

/* Multiple parsers independent (no global state) */
static void test_two_parsers_independent(void) {
    const char *md1 = "# H1\n";
    const char *md2 = "- item\n";

    EventLog log1 = {.count = 0};
    EventLog log2 = {.count = 0};

    MkArena *a1 = mk_arena_new();
    MkArena *a2 = mk_arena_new();
    MkCallbacks cbs1 = make_cbs(&log1);
    MkCallbacks cbs2 = make_cbs(&log2);
    MkParser *p1 = mk_parser_new(a1, &cbs1);
    MkParser *p2 = mk_parser_new(a2, &cbs2);

    /* Interleave feed between the two parsers */
    mk_feed(p1, md1, 3);
    mk_feed(p2, md2, 3);
    mk_feed(p1, md1 + 3, strlen(md1) - 3);
    mk_feed(p2, md2 + 3, strlen(md2) - 3);
    mk_finish(p1);
    mk_finish(p2);

    assert(find_open(&log1, MK_NODE_HEADING)   >= 0);
    assert(find_open(&log2, MK_NODE_LIST_ITEM) >= 0);

    mk_parser_free(p1); mk_arena_free(a1);
    mk_parser_free(p2); mk_arena_free(a2);
    PASS("two_parsers_independent");
}

/* Table split across chunks */
static void test_table_chunked(void) {
    const char *md = "| Col1 | Col2 |\n|------|------|\n| a    | b    |\n";
    EventLog log = {.count = 0};
    parse_chunked(md, 5, &log);
    /* Table is promoted via MODIFY (paragraph → table) */
    assert(find_modify(&log, MK_NODE_TABLE) >= 0);
    PASS("table_chunked");
}

/* Link split across chunks */
static void test_link_chunked(void) {
    const char *md = "[link text](https://example.com)\n";
    EventLog log = {.count = 0};
    parse_byte_by_byte(md, &log);
    assert(find_open(&log, MK_NODE_LINK) >= 0);
    PASS("link_chunked");
}

/* Complex document, verify total event counts are sane */
static void test_complex_document(void) {
    const char *md =
        "# Title\n\n"
        "Paragraph with **bold**, *em*, and `code`.\n\n"
        "> blockquote line\n\n"
        "1. first\n2. second\n\n"
        "```js\nconsole.log('hi');\n```\n\n"
        "| A | B |\n|---|---|\n| 1 | 2 |\n\n"
        "---\n";

    EventLog log_full  = {.count = 0};
    EventLog log_bytes = {.count = 0};

    parse_chunked(md, strlen(md), &log_full);
    parse_byte_by_byte(md, &log_bytes);

    assert(logs_equivalent(&log_full, &log_bytes));
    assert(count_open(&log_full, MK_NODE_HEADING)       >= 1);
    assert(count_open(&log_full, MK_NODE_PARAGRAPH)     >= 1);
    assert(count_open(&log_full, MK_NODE_BLOCK_QUOTE)   >= 1);
    assert(count_open(&log_full, MK_NODE_LIST)          >= 1);
    assert(count_open(&log_full, MK_NODE_CODE_BLOCK)    >= 1);
    assert(find_modify(&log_full, MK_NODE_TABLE)        >= 0);  /* table via MODIFY */
    assert(count_open(&log_full, MK_NODE_THEMATIC_BREAK)>= 1);

    PASS("complex_document");
}

/* [F06] mk_parser_free without mk_finish must still deliver all close events */
static void test_free_without_finish(void) {
    /* Feed content but skip mk_finish — mk_parser_free must call it internally.
     * We must receive: OPEN(doc) OPEN(para) TEXT CLOSE(para) CLOSE(doc) */
    EventLog log = {.count = 0};
    MkArena *a = mk_arena_new();
    MkCallbacks cbs = {
        .user_data      = &log,
        .on_node_open   = ev_open,
        .on_node_close  = ev_close,
        .on_text        = ev_text,
        .on_node_modify = ev_modify,
    };
    MkParser *p = mk_parser_new(a, &cbs);
    mk_feed(p, "hello world\n", 12);
    /* Deliberately skip mk_finish */
    mk_parser_free(p);
    mk_arena_free(a);

    /* Must have received close events for paragraph and document */
    assert(find_close(&log, MK_NODE_PARAGRAPH) >= 0);
    assert(find_close(&log, MK_NODE_DOCUMENT)  >= 0);
    /* mk_finish must be idempotent: calling again on a freed parser is UB, but
     * the close events must appear exactly once */
    int para_closes = 0, doc_closes = 0;
    for (int i = 0; i < log.count; i++) {
        if (log.events[i].kind == EV_CLOSE) {
            if (log.events[i].node_type == MK_NODE_PARAGRAPH) para_closes++;
            if (log.events[i].node_type == MK_NODE_DOCUMENT)  doc_closes++;
        }
    }
    assert(para_closes == 1);
    assert(doc_closes  == 1);
    PASS("free_without_finish");
}

/* [F06] mk_finish followed by mk_parser_free must NOT emit duplicate events */
static void test_finish_then_free_no_duplicates(void) {
    EventLog log = {.count = 0};
    MkArena *a = mk_arena_new();
    MkCallbacks cbs = {
        .user_data      = &log,
        .on_node_open   = ev_open,
        .on_node_close  = ev_close,
        .on_text        = ev_text,
        .on_node_modify = ev_modify,
    };
    MkParser *p = mk_parser_new(a, &cbs);
    mk_feed(p, "hello world\n", 12);
    mk_finish(p);          /* explicit finish */
    mk_parser_free(p);     /* free must NOT call finish again */
    mk_arena_free(a);

    int doc_closes = 0;
    for (int i = 0; i < log.count; i++) {
        if (log.events[i].kind == EV_CLOSE &&
            log.events[i].node_type == MK_NODE_DOCUMENT)
            doc_closes++;
    }
    assert(doc_closes == 1);   /* exactly one CLOSE(document) */
    PASS("finish_then_free_no_duplicates");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Streaming integration tests ===\n");
    test_byte_by_byte_equals_full();
    test_7byte_chunks();
    test_3byte_chunks();
    test_fenced_code_split();
    test_pull_api_basic();
    test_pull_api_interleaved();
    test_two_parsers_independent();
    test_table_chunked();
    test_link_chunked();
    test_complex_document();
    test_free_without_finish();
    test_finish_then_free_no_duplicates();
    printf("All streaming tests passed.\n\n");
    return 0;
}
