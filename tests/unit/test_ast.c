/* test_ast.c — Unit tests for AST node structures and tree operations (M3) */

#include "../../include/mk_parser.h"
#include "../../src/ast.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define PASS(name) printf("  PASS: %s\n", (name))

/* ── helpers ──────────────────────────────────────────────────────────────── */

static MkArena *g_arena;

static void setup(void)    { g_arena = mk_arena_new(); assert(g_arena); }
static void teardown(void) { mk_arena_free(g_arena); g_arena = NULL; }

/* ── node allocation ──────────────────────────────────────────────────────── */

static void test_alloc_heading(void) {
    setup();
    MkHeadingNode *h = MK_NODE_NEW(g_arena, MkHeadingNode, MK_NODE_HEADING, 0);
    assert(h != NULL);
    assert(h->base.type == MK_NODE_HEADING);
    h->level = 3;
    assert(h->level == 3);
    teardown();
    PASS("alloc_heading");
}

static void test_alloc_all_block_types(void) {
    setup();
    MkNodeType types[] = {
        MK_NODE_DOCUMENT, MK_NODE_HEADING, MK_NODE_PARAGRAPH,
        MK_NODE_CODE_BLOCK, MK_NODE_BLOCK_QUOTE, MK_NODE_LIST,
        MK_NODE_LIST_ITEM, MK_NODE_THEMATIC_BREAK, MK_NODE_HTML_BLOCK,
        MK_NODE_TABLE, MK_NODE_TABLE_HEAD, MK_NODE_TABLE_ROW,
        MK_NODE_TABLE_CELL,
    };
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        MkNode *n = mk_node_alloc(g_arena, sizeof(MkNode), types[i], 0);
        assert(n != NULL);
        assert(n->type == types[i]);
    }
    teardown();
    PASS("alloc_all_block_types");
}

static void test_alloc_all_inline_types(void) {
    setup();
    MkNodeType types[] = {
        MK_NODE_TEXT, MK_NODE_SOFT_BREAK, MK_NODE_HARD_BREAK,
        MK_NODE_EMPHASIS, MK_NODE_STRONG, MK_NODE_STRIKETHROUGH,
        MK_NODE_INLINE_CODE, MK_NODE_LINK, MK_NODE_IMAGE,
        MK_NODE_AUTO_LINK, MK_NODE_HTML_INLINE, MK_NODE_TASK_LIST_ITEM,
    };
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        MkNode *n = mk_node_alloc(g_arena, sizeof(MkNode), types[i], 0);
        assert(n != NULL);
        assert(n->type == types[i]);
    }
    teardown();
    PASS("alloc_all_inline_types");
}

/* ── tree operations ──────────────────────────────────────────────────────── */

static void test_append_child(void) {
    setup();
    MkNode *parent = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_DOCUMENT, 0);
    MkNode *child1 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);
    MkNode *child2 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_HEADING, 0);

    mk_node_append_child(parent, child1);
    assert(parent->first_child == child1);
    assert(parent->last_child  == child1);
    assert(child1->parent      == parent);
    assert(child1->next_sibling == NULL);
    assert(child1->prev_sibling == NULL);

    mk_node_append_child(parent, child2);
    assert(parent->first_child == child1);
    assert(parent->last_child  == child2);
    assert(child1->next_sibling == child2);
    assert(child2->prev_sibling == child1);

    teardown();
    PASS("append_child");
}

static void test_prepend_child(void) {
    setup();
    MkNode *parent = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_DOCUMENT, 0);
    MkNode *child1 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);
    MkNode *child2 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_HEADING, 0);

    mk_node_prepend_child(parent, child1);
    mk_node_prepend_child(parent, child2); /* child2 should become first */

    assert(parent->first_child  == child2);
    assert(parent->last_child   == child1);
    assert(child2->next_sibling == child1);
    assert(child1->prev_sibling == child2);

    teardown();
    PASS("prepend_child");
}

static void test_detach_middle(void) {
    setup();
    MkNode *parent = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_DOCUMENT, 0);
    MkNode *c1 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);
    MkNode *c2 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_HEADING, 0);
    MkNode *c3 = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_CODE_BLOCK, 0);

    mk_node_append_child(parent, c1);
    mk_node_append_child(parent, c2);
    mk_node_append_child(parent, c3);

    mk_node_detach(c2);

    assert(parent->first_child == c1);
    assert(parent->last_child  == c3);
    assert(c1->next_sibling    == c3);
    assert(c3->prev_sibling    == c1);
    assert(c2->parent          == NULL);

    teardown();
    PASS("detach_middle");
}

static void test_detach_only_child(void) {
    setup();
    MkNode *parent = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_DOCUMENT, 0);
    MkNode *child  = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);

    mk_node_append_child(parent, child);
    mk_node_detach(child);

    assert(parent->first_child == NULL);
    assert(parent->last_child  == NULL);
    assert(child->parent       == NULL);

    teardown();
    PASS("detach_only_child");
}

/* ── traversal ────────────────────────────────────────────────────────────── */

typedef struct { int enter_count; int exit_count; } TraverseCtx;

static MkTraversalAction count_cb(MkNode *node, int entering, void *ud) {
    (void)node;
    TraverseCtx *ctx = (TraverseCtx *)ud;
    if (entering) ctx->enter_count++;
    else          ctx->exit_count++;
    return MK_TRAVERSE_CONTINUE;
}

static void test_traverse_counts(void) {
    setup();
    /* Build: doc → [p1, p2 → [text]] */
    MkNode *doc  = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_DOCUMENT,  0);
    MkNode *p1   = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);
    MkNode *p2   = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);
    MkNode *text = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_TEXT,      0);

    mk_node_append_child(doc, p1);
    mk_node_append_child(doc, p2);
    mk_node_append_child(p2, text);

    TraverseCtx ctx = {0, 0};
    mk_node_traverse(doc, count_cb, &ctx);

    /* 4 nodes, each entered and exited once */
    assert(ctx.enter_count == 4);
    assert(ctx.exit_count  == 4);

    teardown();
    PASS("traverse_counts");
}

static MkTraversalAction skip_cb(MkNode *node, int entering, void *ud) {
    int *count = (int *)ud;
    if (entering) (*count)++;
    if (entering && node->type == MK_NODE_BLOCK_QUOTE)
        return MK_TRAVERSE_SKIP;
    return MK_TRAVERSE_CONTINUE;
}

static void test_traverse_skip(void) {
    setup();
    MkNode *doc  = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_DOCUMENT,    0);
    MkNode *bq   = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_BLOCK_QUOTE, 0);
    MkNode *p    = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH,   0);

    mk_node_append_child(doc, bq);
    mk_node_append_child(bq, p);  /* should be skipped */

    int count = 0;
    mk_node_traverse(doc, skip_cb, &count);

    /* doc + bq entered; p skipped */
    assert(count == 2);

    teardown();
    PASS("traverse_skip");
}

/* ── getter API sanity ────────────────────────────────────────────────────── */

static void test_getters_heading(void) {
    setup();
    MkHeadingNode *h = MK_NODE_NEW(g_arena, MkHeadingNode, MK_NODE_HEADING, 0);
    h->level = 2;
    assert(mk_node_heading_level(&h->base) == 2);
    assert(mk_node_heading_level(NULL) == 0);
    teardown();
    PASS("getter_heading_level");
}

static void test_getters_link(void) {
    setup();
    MkLinkNode *lk = MK_NODE_NEW(g_arena, MkLinkNode, MK_NODE_LINK, 0);
    lk->href      = mk_arena_strdup_stable(g_arena, "https://example.com", 19);
    lk->href_len  = 19;
    lk->title     = NULL;
    lk->title_len = 0;

    assert(strcmp(mk_node_link_href(&lk->base), "https://example.com") == 0);
    assert(mk_node_link_href_len(&lk->base) == 19);
    assert(mk_node_link_title(&lk->base)    == NULL);

    /* Wrong type returns NULL */
    MkNode *p = mk_node_alloc(g_arena, sizeof(MkNode), MK_NODE_PARAGRAPH, 0);
    assert(mk_node_link_href(p) == NULL);

    teardown();
    PASS("getter_link");
}

static void test_getters_table(void) {
    setup();
    MkTableNode *t = MK_NODE_NEW(g_arena, MkTableNode, MK_NODE_TABLE, 0);
    t->col_count  = 3;
    t->col_aligns = mk_arena_stable_alloc(g_arena, 3 * sizeof(MkAlign));
    t->col_aligns[0] = MK_ALIGN_LEFT;
    t->col_aligns[1] = MK_ALIGN_CENTER;
    t->col_aligns[2] = MK_ALIGN_RIGHT;

    assert(mk_node_table_col_count(&t->base) == 3);
    assert(mk_node_table_col_align(&t->base, 0) == MK_ALIGN_LEFT);
    assert(mk_node_table_col_align(&t->base, 1) == MK_ALIGN_CENTER);
    assert(mk_node_table_col_align(&t->base, 2) == MK_ALIGN_RIGHT);
    assert(mk_node_table_col_align(&t->base, 99) == MK_ALIGN_NONE);

    teardown();
    PASS("getter_table");
}

/* ── node_type_name ───────────────────────────────────────────────────────── */

static void test_type_name(void) {
    assert(strcmp(mk_node_type_name(MK_NODE_HEADING),   "Heading")   == 0);
    assert(strcmp(mk_node_type_name(MK_NODE_PARAGRAPH), "Paragraph") == 0);
    assert(mk_node_type_name((MkNodeType)9999) != NULL); /* never NULL */
    PASS("node_type_name");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== AST unit tests ===\n");
    test_alloc_heading();
    test_alloc_all_block_types();
    test_alloc_all_inline_types();
    test_append_child();
    test_prepend_child();
    test_detach_middle();
    test_detach_only_child();
    test_traverse_counts();
    test_traverse_skip();
    test_getters_heading();
    test_getters_link();
    test_getters_table();
    test_type_name();
    printf("All AST tests passed.\n\n");
    return 0;
}
