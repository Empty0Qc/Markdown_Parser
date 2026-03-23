/* smoke.c — Quick end-to-end parse test (M4+M5 integration) */
#include "mk_parser.h"
#include "../src/arena.h"
#include "../src/block.h"

#include <stdio.h>
#include <string.h>

static int g_depth = 0;

static void on_open(void *ud, MkNode *n) {
    (void)ud;
    for (int i = 0; i < g_depth; i++) printf("  ");
    printf("<%s", mk_node_type_name(n->type));
    if (n->type == MK_NODE_HEADING)
        printf(" level=%d", ((MkHeadingNode *)n)->level);
    if (n->type == MK_NODE_CODE_BLOCK && ((MkCodeBlockNode *)n)->lang_len)
        printf(" lang=%.*s", (int)((MkCodeBlockNode *)n)->lang_len,
               ((MkCodeBlockNode *)n)->lang);
    printf(">\n");
    g_depth++;
}

static void on_close(void *ud, MkNode *n) {
    (void)ud;
    g_depth--;
    for (int i = 0; i < g_depth; i++) printf("  ");
    printf("</%s>\n", mk_node_type_name(n->type));
}

static void on_text(void *ud, MkNode *n, const char *text, size_t len) {
    (void)ud; (void)n;
    for (int i = 0; i < g_depth; i++) printf("  ");
    printf("[text: %.*s]\n", (int)len, text);
}

static void on_modify(void *ud, MkNode *n) {
    (void)ud;
    for (int i = 0; i < g_depth - 1; i++) printf("  ");
    printf("[MODIFY → %s]\n", mk_node_type_name(n->type));
}

static const char *TEST_INPUT =
    "# Hello *World*\n"
    "\n"
    "A paragraph with **bold** and `code`.\n"
    "\n"
    "- item one\n"
    "- item [two](https://example.com)\n"
    "\n"
    "```python\n"
    "print('hello')\n"
    "```\n"
    "\n"
    "| Name | Age |\n"
    "| ---- | --- |\n"
    "| Alice | 30 |\n"
    "\n"
    "~~strikethrough~~ and ~~done~~\n";

int main(void) {
    MkArena *arena = mk_arena_new();

    MkCallbacks cbs = {
        .user_data     = NULL,
        .on_node_open  = on_open,
        .on_node_close = on_close,
        .on_text       = on_text,
        .on_node_modify = on_modify,
    };

    MkBlockParser bp;
    mk_block_init(&bp, arena, &cbs);

    /* Feed in small chunks to stress-test streaming */
    const char *src = TEST_INPUT;
    size_t      len = strlen(src);
    size_t      chunk = 7;  /* odd chunk size to catch boundary issues */
    for (size_t i = 0; i < len; i += chunk) {
        size_t n = (i + chunk < len) ? chunk : len - i;
        mk_block_feed(&bp, src + i, n);
    }
    mk_block_finish(&bp);

    mk_block_cleanup(&bp);
    mk_arena_free(arena);

    printf("\nOK\n");
    return 0;
}
