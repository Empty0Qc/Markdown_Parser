/* test_inline.c — Unit tests for Inline parser (M5) */

#include "../../include/mk_parser.h"
#include "../../src/ast.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define PASS(name) printf("  PASS: %s\n", (name))

/* ── event collection (same pattern as test_block.c) ─────────────────────── */

#define MAX_EVENTS 512

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

static void parse_full(const char *md, EventLog *log) {
    MkArena *a = mk_arena_new();
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

/* Check if any TEXT event contains the given substring */
static int text_contains(const EventLog *log, const char *sub) {
    for (int i = 0; i < log->count; i++)
        if (log->events[i].kind == EV_TEXT && strstr(log->events[i].text, sub))
            return 1;
    return 0;
}

/* ── tests ────────────────────────────────────────────────────────────────── */

static void test_plain_text(void) {
    EventLog log = {.count = 0};
    parse_full("Hello world\n", &log);
    assert(text_contains(&log, "Hello world"));
    PASS("plain_text");
}

static void test_emphasis_star(void) {
    EventLog log = {.count = 0};
    parse_full("*em*\n", &log);
    assert(find_open(&log, MK_NODE_EMPHASIS) >= 0);
    PASS("emphasis_star");
}

static void test_emphasis_underscore(void) {
    EventLog log = {.count = 0};
    parse_full("_em_\n", &log);
    assert(find_open(&log, MK_NODE_EMPHASIS) >= 0);
    PASS("emphasis_underscore");
}

static void test_strong_star(void) {
    EventLog log = {.count = 0};
    parse_full("**strong**\n", &log);
    assert(find_open(&log, MK_NODE_STRONG) >= 0);
    PASS("strong_star");
}

static void test_strong_underscore(void) {
    EventLog log = {.count = 0};
    parse_full("__strong__\n", &log);
    assert(find_open(&log, MK_NODE_STRONG) >= 0);
    PASS("strong_underscore");
}

static void test_strikethrough(void) {
    EventLog log = {.count = 0};
    parse_full("~~strike~~\n", &log);
    assert(find_open(&log, MK_NODE_STRIKETHROUGH) >= 0);
    PASS("strikethrough");
}

static void test_inline_code(void) {
    EventLog log = {.count = 0};
    parse_full("`code`\n", &log);
    assert(find_open(&log, MK_NODE_INLINE_CODE) >= 0);
    PASS("inline_code");
}

static void test_link(void) {
    EventLog log = {.count = 0};
    parse_full("[text](https://example.com)\n", &log);
    assert(find_open(&log, MK_NODE_LINK) >= 0);
    PASS("link");
}

static void test_image(void) {
    EventLog log = {.count = 0};
    parse_full("![alt](img.png)\n", &log);
    assert(find_open(&log, MK_NODE_IMAGE) >= 0);
    PASS("image");
}

static void test_autolink(void) {
    EventLog log = {.count = 0};
    parse_full("<https://example.com>\n", &log);
    assert(find_open(&log, MK_NODE_AUTO_LINK) >= 0);
    PASS("autolink");
}

static void test_autolink_email(void) {
    EventLog log = {.count = 0};
    parse_full("<user@example.com>\n", &log);
    assert(find_open(&log, MK_NODE_AUTO_LINK) >= 0);
    PASS("autolink_email");
}

static void test_html_inline(void) {
    EventLog log = {.count = 0};
    parse_full("text <em>html</em> text\n", &log);
    assert(find_open(&log, MK_NODE_HTML_INLINE) >= 0);
    PASS("html_inline");
}

static void test_hard_break(void) {
    EventLog log = {.count = 0};
    parse_full("line1  \nline2\n", &log); /* two trailing spaces = hard break */
    assert(find_open(&log, MK_NODE_HARD_BREAK) >= 0);
    PASS("hard_break");
}

static void test_soft_break(void) {
    EventLog log = {.count = 0};
    parse_full("line1\nline2\n", &log); /* single newline = soft break */
    assert(find_open(&log, MK_NODE_SOFT_BREAK) >= 0);
    PASS("soft_break");
}

static void test_task_list_item(void) {
    EventLog log = {.count = 0};
    parse_full("- [ ] unchecked\n- [x] checked\n", &log);
    assert(count_open(&log, MK_NODE_TASK_LIST_ITEM) == 2);
    PASS("task_list_item");
}

static void test_nested_em_strong(void) {
    EventLog log = {.count = 0};
    parse_full("***both***\n", &log);
    /* Should produce at least emphasis or strong */
    int has_em = find_open(&log, MK_NODE_EMPHASIS) >= 0;
    int has_st = find_open(&log, MK_NODE_STRONG)   >= 0;
    assert(has_em || has_st);
    PASS("nested_em_strong");
}

static void test_code_span_multiple_backticks(void) {
    EventLog log = {.count = 0};
    parse_full("``foo ` bar``\n", &log);
    assert(find_open(&log, MK_NODE_INLINE_CODE) >= 0);
    PASS("code_span_multiple_backticks");
}

static void test_link_with_title(void) {
    EventLog log = {.count = 0};
    parse_full("[text](https://example.com \"title\")\n", &log);
    assert(find_open(&log, MK_NODE_LINK) >= 0);
    PASS("link_with_title");
}

/* [F15] Link dest with backslash-escaped parentheses.
 * [link](\(foo\)) — href should be \(foo\), NOT misparse as [link]( */
static char g_captured_href[256];
static void capture_link_href(void *ud, MkNode *n) {
    (void)ud;
    if (n->type == MK_NODE_LINK) {
        const char *h = mk_node_link_href(n);
        if (h) {
            size_t l = mk_node_link_href_len(n);
            if (l >= sizeof(g_captured_href)) l = sizeof(g_captured_href) - 1;
            memcpy(g_captured_href, h, l);
            g_captured_href[l] = '\0';
        }
    }
}

static void test_link_escaped_parens(void) {
    g_captured_href[0] = '\0';
    MkArena *a = mk_arena_new();
    MkCallbacks cbs = {
        .user_data    = NULL,
        .on_node_open = capture_link_href,
    };
    MkParser *p = mk_parser_new(a, &cbs);
    mk_feed(p, "[link](\\(foo\\))\n", 17);
    mk_finish(p);
    mk_parser_free(p);
    mk_arena_free(a);

    /* href must be \(foo\) — the escaped parens are part of the URL */
    assert(strcmp(g_captured_href, "\\(foo\\)") == 0);
    PASS("link_escaped_parens");
}

/* Balanced unescaped parens in link dest should also work */
static void test_link_balanced_parens(void) {
    g_captured_href[0] = '\0';
    MkArena *a = mk_arena_new();
    MkCallbacks cbs = {
        .user_data    = NULL,
        .on_node_open = capture_link_href,
    };
    MkParser *p = mk_parser_new(a, &cbs);
    mk_feed(p, "[link](foo(bar)baz)\n", 20);
    mk_finish(p);
    mk_parser_free(p);
    mk_arena_free(a);

    /* href must be foo(bar)baz — balanced parens included */
    assert(strcmp(g_captured_href, "foo(bar)baz") == 0);
    PASS("link_balanced_parens");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Inline parser unit tests ===\n");
    test_plain_text();
    test_emphasis_star();
    test_emphasis_underscore();
    test_strong_star();
    test_strong_underscore();
    test_strikethrough();
    test_inline_code();
    test_link();
    test_image();
    test_autolink();
    test_autolink_email();
    test_html_inline();
    test_hard_break();
    test_soft_break();
    test_task_list_item();
    test_nested_em_strong();
    test_code_span_multiple_backticks();
    test_link_with_title();
    test_link_escaped_parens();
    test_link_balanced_parens();
    printf("All inline tests passed.\n\n");
    return 0;
}
