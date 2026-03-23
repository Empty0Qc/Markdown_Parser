/* test_m6.c — M6 Push/Pull API integration test */
#include "mk_parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ── helpers ──────────────────────────────────────────────────────────────── */

static int g_push_events = 0;

static void push_open (void *ud, MkNode *n)                            { (void)ud; (void)n; g_push_events++; }
static void push_close(void *ud, MkNode *n)                            { (void)ud; (void)n; g_push_events++; }
static void push_text (void *ud, MkNode *n, const char *t, size_t l)   { (void)ud; (void)n; (void)t; (void)l; g_push_events++; }
static void push_mod  (void *ud, MkNode *n)                            { (void)ud; (void)n; g_push_events++; }

static const char *INPUT =
    "# Title\n"
    "\n"
    "Hello **world**.\n";

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 1 — Push-only: callbacks fire, delta queue is populated
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_push(void) {
    MkArena *arena = mk_arena_new();
    MkCallbacks cbs = {
        .user_data     = NULL,
        .on_node_open  = push_open,
        .on_node_close = push_close,
        .on_text       = push_text,
        .on_node_modify= push_mod,
    };

    g_push_events = 0;
    MkParser *p = mk_parser_new(arena, &cbs);
    assert(p != NULL);

    mk_feed(p, INPUT, strlen(INPUT));
    mk_finish(p);

    assert(g_push_events > 0);
    printf("  push_events=%d\n", g_push_events);

    /* Root should be Document */
    MkNode *root = mk_get_root(p);
    assert(root != NULL);
    assert(root->type == MK_NODE_DOCUMENT);

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_push: OK\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 2 — Pull-only: no callbacks, drain delta queue manually
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_pull(void) {
    MkArena  *arena = mk_arena_new();
    MkParser *p     = mk_parser_new(arena, NULL);   /* no push callbacks */
    assert(p != NULL);

    mk_feed(p, INPUT, strlen(INPUT));
    mk_finish(p);

    /* Drain the delta queue */
    int n_open = 0, n_close = 0, n_text = 0, n_modify = 0;
    MkDelta *d;
    while ((d = mk_pull_delta(p)) != NULL) {
        switch (d->type) {
        case MK_DELTA_NODE_OPEN:   n_open++;   break;
        case MK_DELTA_NODE_CLOSE:  n_close++;  break;
        case MK_DELTA_TEXT:        n_text++;   break;
        case MK_DELTA_NODE_MODIFY: n_modify++; break;
        }
        mk_delta_free(d);
    }

    printf("  open=%d close=%d text=%d modify=%d\n",
           n_open, n_close, n_text, n_modify);

    /* Every open must have a matching close */
    assert(n_open == n_close);
    assert(n_text > 0);

    /* Queue should now be empty */
    assert(mk_pull_delta(p) == NULL);

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_pull: OK\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 3 — Mixed: push callbacks fire AND pull queue accumulates
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_mixed(void) {
    MkArena *arena = mk_arena_new();
    MkCallbacks cbs = {
        .user_data     = NULL,
        .on_node_open  = push_open,
        .on_node_close = push_close,
        .on_text       = push_text,
        .on_node_modify= push_mod,
    };

    g_push_events = 0;
    MkParser *p = mk_parser_new(arena, &cbs);
    assert(p != NULL);

    mk_feed(p, INPUT, strlen(INPUT));
    mk_finish(p);

    int push_count = g_push_events;
    assert(push_count > 0);

    /* Count deltas in queue — should equal push_events */
    int pull_count = 0;
    MkDelta *d;
    while ((d = mk_pull_delta(p)) != NULL) { pull_count++; mk_delta_free(d); }

    printf("  push_count=%d pull_count=%d\n", push_count, pull_count);
    assert(push_count == pull_count);

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_mixed: OK\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test 4 — Chunked feeding matches single-shot feeding
 * ═══════════════════════════════════════════════════════════════════════════ */
static void test_chunked(void) {
    /* Single shot */
    MkArena  *a1 = mk_arena_new();
    MkParser *p1 = mk_parser_new(a1, NULL);
    mk_feed(p1, INPUT, strlen(INPUT));
    mk_finish(p1);
    int total1 = 0;
    { MkDelta *d; while ((d = mk_pull_delta(p1)) != NULL) { total1++; mk_delta_free(d); } }

    /* Chunked (5 bytes at a time) */
    MkArena  *a2 = mk_arena_new();
    MkParser *p2 = mk_parser_new(a2, NULL);
    const char *src = INPUT;
    size_t      rem = strlen(INPUT);
    while (rem) {
        size_t chunk = rem < 5 ? rem : 5;
        mk_feed(p2, src, chunk);
        src += chunk; rem -= chunk;
    }
    mk_finish(p2);
    int total2 = 0;
    { MkDelta *d; while ((d = mk_pull_delta(p2)) != NULL) { total2++; mk_delta_free(d); } }

    printf("  single=%d chunked=%d\n", total1, total2);
    assert(total1 == total2);

    mk_parser_free(p1); mk_arena_free(a1);
    mk_parser_free(p2); mk_arena_free(a2);
    printf("  test_chunked: OK\n");
}

int main(void) {
    printf("M6 Push/Pull API tests\n");
    printf("--- test_push ---\n");   test_push();
    printf("--- test_pull ---\n");   test_pull();
    printf("--- test_mixed ---\n");  test_mixed();
    printf("--- test_chunked ---\n");test_chunked();
    printf("\nAll M6 tests passed.\n");
    return 0;
}
