/* bench_parser.c — multi-scenario parser throughput benchmark
 *
 * Tests 3 document sizes × 5 chunk sizes, runs each 3× and takes the median.
 * Outputs an aligned Markdown table to stdout.
 *
 * Build:  cmake --build build/native --target mk_parser_bench_parser
 * Run:    ./build.sh bench
 */
#include "mk_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── document templates ────────────────────────────────────────────────────── */

static const char *DOC_UNIT =
    "# Heading Level One\n"
    "\n"
    "Paragraph with **bold**, *italic*, `code`, [link](https://example.com),"
    " ~~strike~~, and a bit more text to pad things out.\n"
    "\n"
    "- item one\n"
    "- item two with **nested bold**\n"
    "- item three\n"
    "\n"
    "| Name  | Value | Description          |\n"
    "| ----- | ----- | -------------------- |\n"
    "| foo   | 123   | first entry          |\n"
    "| bar   | 456   | second entry         |\n"
    "| baz   | 789   | third entry          |\n"
    "\n"
    "```c\n"
    "int main(void) {\n"
    "    printf(\"hello, world\\n\");\n"
    "    return 0;\n"
    "}\n"
    "```\n"
    "\n"
    "> Blockquote with *emphasis* and a second sentence for length.\n"
    "\n"
    "---\n"
    "\n";

/* ── timing ────────────────────────────────────────────────────────────────── */

static double now_sec(void) {
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#else
    /* fallback for platforms without CLOCK_MONOTONIC */
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

/* ── comparison for qsort ─────────────────────────────────────────────────── */

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/* ── single run: parse `total_len` bytes in `chunk_sz`-byte feeds ─────────── */

static double bench_run(const char *buf, size_t total_len, size_t chunk_sz) {
    MkArena *arena = mk_arena_new();
    MkParser *parser = mk_parser_new(arena, NULL);
    if (!arena || !parser) {
        fprintf(stderr, "parser setup failed\n");
        if (parser) mk_parser_free(parser);
        if (arena)  mk_arena_free(arena);
        return -1.0;
    }

    double start = now_sec();

    for (size_t pos = 0; pos < total_len; pos += chunk_sz) {
        size_t n = total_len - pos;
        if (n > chunk_sz) n = chunk_sz;
        if (mk_feed(parser, buf + pos, n) != 0) {
            fprintf(stderr, "mk_feed error at offset %zu\n", pos);
            mk_parser_free(parser);
            mk_arena_free(arena);
            return -1.0;
        }
    }
    if (mk_finish(parser) != 0) {
        fprintf(stderr, "mk_finish error\n");
        mk_parser_free(parser);
        mk_arena_free(arena);
        return -1.0;
    }

    double elapsed = now_sec() - start;

    /* drain queue so memory is accounted correctly */
    MkDelta *d;
    while ((d = mk_pull_delta(parser)) != NULL) mk_delta_free(d);

    mk_parser_free(parser);
    mk_arena_free(arena);
    return elapsed;
}

/* ── median of RUNS runs ──────────────────────────────────────────────────── */
#define RUNS 3

static double bench_median(const char *buf, size_t total_len, size_t chunk_sz) {
    double times[RUNS];
    for (int r = 0; r < RUNS; r++) {
        times[r] = bench_run(buf, total_len, chunk_sz);
        if (times[r] < 0.0) return -1.0;
    }
    qsort(times, RUNS, sizeof(double), cmp_double);
    return times[RUNS / 2]; /* median */
}

/* ── build document of given approximate byte count ──────────────────────── */

static char *make_doc(size_t target_bytes, size_t *out_len) {
    size_t unit_len = strlen(DOC_UNIT);
    size_t repeats  = target_bytes / unit_len;
    if (repeats < 1) repeats = 1;
    size_t total = unit_len * repeats;

    char *buf = malloc(total + 1);
    if (!buf) return NULL;

    char *dst = buf;
    for (size_t i = 0; i < repeats; i++) {
        memcpy(dst, DOC_UNIT, unit_len);
        dst += unit_len;
    }
    buf[total] = '\0';
    *out_len = total;
    return buf;
}

/* ── helpers ──────────────────────────────────────────────────────────────── */

static const char *human_size(size_t bytes, char *buf, size_t bufsz) {
    if (bytes >= 1024 * 1024)
        snprintf(buf, bufsz, "%.0f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(buf, bufsz, "%.0f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, bufsz, "%zu B", bytes);
    return buf;
}

/* ── chunk label ──────────────────────────────────────────────────────────── */

static const char *chunk_label(size_t chunk_sz, size_t doc_len,
                                char *buf, size_t bufsz) {
    if (chunk_sz >= doc_len)
        snprintf(buf, bufsz, "full");
    else if (chunk_sz >= 1024)
        snprintf(buf, bufsz, "%zu KB", chunk_sz / 1024);
    else
        snprintf(buf, bufsz, "%zu B", chunk_sz);
    return buf;
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    /* document target sizes */
    static const size_t DOC_TARGETS[] = {
        2   * 1024,   /* ~2 KB  — small   */
        50  * 1024,   /* ~50 KB — medium  */
        500 * 1024    /* ~500 KB — large  */
    };
    static const int N_DOCS = (int)(sizeof(DOC_TARGETS) / sizeof(DOC_TARGETS[0]));

    /* chunk sizes — "full" represented by SIZE_MAX, resolved per-doc */
    static const size_t CHUNK_SIZES[] = { 1, 17, 128, 1024, (size_t)-1 };
    static const int N_CHUNKS = (int)(sizeof(CHUNK_SIZES) / sizeof(CHUNK_SIZES[0]));

    printf("mk_parser throughput benchmark  (median of %d runs)\n\n", RUNS);
    printf("| %-9s | %-7s | %10s | %12s |\n",
           "doc_size", "chunk", "MB/s", "elapsed_ms");
    printf("| %-9s | %-7s | %10s | %12s |\n",
           "---------", "-------", "----------", "------------");

    for (int d = 0; d < N_DOCS; d++) {
        size_t doc_len = 0;
        char  *buf     = make_doc(DOC_TARGETS[d], &doc_len);
        if (!buf) {
            fprintf(stderr, "allocation failed for doc %d\n", d);
            return 1;
        }

        char doc_label[32];
        human_size(doc_len, doc_label, sizeof(doc_label));

        for (int c = 0; c < N_CHUNKS; c++) {
            size_t chunk_sz = CHUNK_SIZES[c];
            if (chunk_sz > doc_len) chunk_sz = doc_len; /* "full" */

            double elapsed = bench_median(buf, doc_len, chunk_sz);
            if (elapsed < 0.0) {
                free(buf);
                return 1;
            }

            char chunk_lbl[16];
            chunk_label(chunk_sz, doc_len, chunk_lbl, sizeof(chunk_lbl));

            double mbps = (elapsed > 0.0)
                ? (double)doc_len / (1024.0 * 1024.0) / elapsed
                : 0.0;
            double ms   = elapsed * 1000.0;

            printf("| %-9s | %-7s | %10.1f | %12.3f |\n",
                   doc_label, chunk_lbl, mbps, ms);
        }

        free(buf);
    }

    printf("\n");
    return 0;
}
