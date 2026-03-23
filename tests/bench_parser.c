/* bench_parser.c — lightweight parser throughput benchmark */
#include "mk_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *DOC_CHUNK =
    "# Heading\n"
    "\n"
    "Paragraph with **bold**, `code`, [link](https://example.com), and ~~strike~~.\n"
    "\n"
    "- item one\n"
    "- item two\n"
    "- item three\n"
    "\n"
    "| Name | Value |\n"
    "| ---- | ----- |\n"
    "| foo  | 123   |\n"
    "| bar  | 456   |\n"
    "\n"
    "```c\n"
    "int main(void) { return 0; }\n"
    "```\n"
    "\n";

static double now_seconds(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(void) {
    enum { REPEAT = 400 };

    size_t chunk_len = strlen(DOC_CHUNK);
    size_t total_len = chunk_len * (size_t)REPEAT;
    char *buffer = malloc(total_len + 1);
    if (!buffer) {
        fprintf(stderr, "benchmark allocation failed\n");
        return 1;
    }

    char *dst = buffer;
    for (int i = 0; i < REPEAT; i++) {
        memcpy(dst, DOC_CHUNK, chunk_len);
        dst += chunk_len;
    }
    buffer[total_len] = '\0';

    MkArena *arena = mk_arena_new();
    MkParser *parser = mk_parser_new(arena, NULL);
    if (!arena || !parser) {
        fprintf(stderr, "parser setup failed\n");
        free(buffer);
        if (parser) mk_parser_free(parser);
        if (arena) mk_arena_free(arena);
        return 1;
    }

    const size_t feed_chunk = 17;
    double start = now_seconds();
    for (size_t pos = 0; pos < total_len; pos += feed_chunk) {
        size_t n = total_len - pos;
        if (n > feed_chunk) n = feed_chunk;
        if (mk_feed(parser, buffer + pos, n) != 0) {
            fprintf(stderr, "mk_feed failed at offset %zu\n", pos);
            free(buffer);
            mk_parser_free(parser);
            mk_arena_free(arena);
            return 1;
        }
    }
    if (mk_finish(parser) != 0) {
        fprintf(stderr, "mk_finish failed\n");
        free(buffer);
        mk_parser_free(parser);
        mk_arena_free(arena);
        return 1;
    }
    double elapsed = now_seconds() - start;

    size_t deltas = 0;
    MkDelta *delta = NULL;
    while ((delta = mk_pull_delta(parser)) != NULL) {
        deltas++;
        mk_delta_free(delta);
    }

    printf("bench_parser\n");
    printf("  bytes=%zu\n", total_len);
    printf("  feed_chunk=%zu\n", feed_chunk);
    printf("  deltas=%zu\n", deltas);
    printf("  elapsed_sec=%.6f\n", elapsed);
    if (elapsed > 0.0) {
        printf("  bytes_per_sec=%.2f\n", (double)total_len / elapsed);
    }

    free(buffer);
    mk_parser_free(parser);
    mk_arena_free(arena);
    return 0;
}
