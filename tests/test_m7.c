/* test_m7.c — M7 Plugin system test
 *
 * Tests:
 *   1. Math inline plugin  ($...$  → MK_NODE_CUSTOM "math")
 *   2. Block plugin        (!! line → MK_NODE_CUSTOM "alert")
 *   3. Transform plugin    (counts nodes after they complete)
 */
#include "mk_parser.h"
#include "../src/ast.h"   /* MkCustomNode */

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Plugin 1 — Math inline: $expr$
 * ════════════════════════════════════════════════════════════════════════════ */

static size_t math_try_inline(MkParser *parser, MkArena *arena,
                               const char *src, size_t len, MkNode **out)
{
    (void)parser;
    if (!len || src[0] != '$') return 0;
    /* Find closing $ */
    size_t p = 1;
    while (p < len && src[p] != '$' && src[p] != '\n') p++;
    if (p >= len || src[p] != '$') return 0;  /* no closing $ on same line */

    MkCustomNode *n = MK_NODE_NEW(arena, MkCustomNode, MK_NODE_CUSTOM, 0);
    if (!n) return 0;
    n->plugin_name = "math";
    n->raw         = mk_arena_strdup_stable(arena, src + 1, p - 1);
    n->raw_len     = p - 1;
    *out = &n->base;
    return p + 1;   /* consume including closing $ */
}

static const MkParserPlugin math_plugin = {
    .name            = "math",
    .inline_triggers = "$",
    .try_block       = NULL,
    .try_inline      = math_try_inline,
};

/* ════════════════════════════════════════════════════════════════════════════
 * Plugin 2 — Alert block: !! message
 * ════════════════════════════════════════════════════════════════════════════ */

static size_t alert_try_block(MkParser *parser, MkArena *arena,
                               const char *src, size_t len, MkNode **out)
{
    (void)parser;
    if (len < 3 || src[0] != '!' || src[1] != '!') return 0;
    /* Content after "!! " */
    size_t      cstart = (len > 2 && src[2] == ' ') ? 3 : 2;
    MkCustomNode *n = MK_NODE_NEW(arena, MkCustomNode, MK_NODE_CUSTOM, 0);
    if (!n) return 0;
    n->plugin_name = "alert";
    n->raw         = mk_arena_strdup_stable(arena, src + cstart, len - cstart);
    n->raw_len     = len - cstart;
    *out = &n->base;
    return len;
}

static const MkParserPlugin alert_plugin = {
    .name            = "alert",
    .inline_triggers = NULL,
    .try_block       = alert_try_block,
    .try_inline      = NULL,
};

/* ════════════════════════════════════════════════════════════════════════════
 * Plugin 3 — Transform: count completed nodes
 * ════════════════════════════════════════════════════════════════════════════ */

static int g_completed_nodes = 0;

static void count_complete(MkNode *node, MkArena *arena) {
    (void)arena;
    (void)node;
    g_completed_nodes++;
}

static const MkTransformPlugin counter_plugin = {
    .name              = "counter",
    .on_node_complete  = count_complete,
};

/* ════════════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════════════ */

static int g_custom_count = 0;

static void on_open(void *ud, MkNode *n) {
    (void)ud;
    if (n->type == MK_NODE_CUSTOM) {
        MkCustomNode *cn = (MkCustomNode *)n;
        printf("  [custom plugin=%s raw=%.*s]\n",
               cn->plugin_name, (int)cn->raw_len, cn->raw);
        g_custom_count++;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 1 — Math inline
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_math_inline(void) {
    const char *input =
        "Euler: $e^{i\\pi}+1=0$ and Pythagoras: $a^2+b^2=c^2$.\n";

    MkArena *arena = mk_arena_new();
    MkCallbacks cbs = { .user_data = NULL, .on_node_open = on_open };
    MkParser *p = mk_parser_new(arena, &cbs);
    assert(p);
    mk_register_parser_plugin(p, &math_plugin);

    g_custom_count = 0;
    mk_feed(p, input, strlen(input));
    mk_finish(p);

    printf("  math custom nodes found: %d\n", g_custom_count);
    assert(g_custom_count == 2);

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_math_inline: OK\n");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 2 — Alert block
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_alert_block(void) {
    const char *input =
        "Normal paragraph.\n"
        "\n"
        "!! Watch out for edge cases!\n"
        "\n"
        "More text.\n";

    MkArena *arena = mk_arena_new();
    MkCallbacks cbs = { .user_data = NULL, .on_node_open = on_open };
    MkParser *p = mk_parser_new(arena, &cbs);
    assert(p);
    mk_register_parser_plugin(p, &alert_plugin);

    g_custom_count = 0;
    mk_feed(p, input, strlen(input));
    mk_finish(p);

    printf("  alert custom nodes found: %d\n", g_custom_count);
    assert(g_custom_count == 1);

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_alert_block: OK\n");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 3 — Transform plugin (node counter)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_transform(void) {
    const char *input = "# Hi\n\nParagraph **bold**.\n";

    MkArena *arena = mk_arena_new();
    MkParser *p = mk_parser_new(arena, NULL);
    assert(p);
    mk_register_transform_plugin(p, &counter_plugin);

    g_completed_nodes = 0;
    mk_feed(p, input, strlen(input));
    mk_finish(p);

    printf("  completed nodes: %d\n", g_completed_nodes);
    /* Document, Heading, Text(Hi), Paragraph, Text(Paragraph ), Strong,
       Text(bold), Text(.) = 8 */
    assert(g_completed_nodes >= 6);  /* at least this many */

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_transform: OK\n");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 4 — Both inline plugins + transform together (smoke)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_combined(void) {
    const char *input =
        "# Title\n"
        "\n"
        "Inline math: $x^2$ here.\n"
        "\n"
        "!! Alert block!\n"
        "\n"
        "Normal $y=mx+b$ text.\n";

    MkArena *arena = mk_arena_new();
    MkCallbacks cbs = { .user_data = NULL, .on_node_open = on_open };
    MkParser *p = mk_parser_new(arena, &cbs);
    assert(p);
    mk_register_parser_plugin(p, &math_plugin);
    mk_register_parser_plugin(p, &alert_plugin);
    mk_register_transform_plugin(p, &counter_plugin);

    g_custom_count = 0;
    g_completed_nodes = 0;
    mk_feed(p, input, strlen(input));
    mk_finish(p);

    printf("  combined: custom=%d completed=%d\n",
           g_custom_count, g_completed_nodes);
    assert(g_custom_count == 3);   /* $x^2$, alert block, $y=mx+b$ */
    assert(g_completed_nodes > 0);

    mk_parser_free(p);
    mk_arena_free(arena);
    printf("  test_combined: OK\n");
}

int main(void) {
    printf("M7 Plugin system tests\n");
    printf("--- test_math_inline ---\n");  test_math_inline();
    printf("--- test_alert_block ---\n");  test_alert_block();
    printf("--- test_transform ---\n");    test_transform();
    printf("--- test_combined ---\n");     test_combined();
    printf("\nAll M7 tests passed.\n");
    return 0;
}
