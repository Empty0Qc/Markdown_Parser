/* test_arena.c — Unit tests for Arena allocator (M2) */

#include "../../include/mk_parser.h"
#include "../../src/arena.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define PASS(name) printf("  PASS: %s\n", (name))

/* ── helpers ──────────────────────────────────────────────────────────────── */

static int g_custom_alloc_count = 0;
static int g_custom_free_count  = 0;

static void *custom_alloc(void *ctx, size_t size) {
    (void)ctx;
    g_custom_alloc_count++;
    return malloc(size);
}

static void custom_free(void *ctx, void *ptr) {
    (void)ctx;
    g_custom_free_count++;
    free(ptr);
}

/* ── tests ────────────────────────────────────────────────────────────────── */

static void test_new_free(void) {
    MkArena *a = mk_arena_new();
    assert(a != NULL);
    mk_arena_free(a);
    PASS("arena_new_free");
}

static void test_custom_allocator(void) {
    g_custom_alloc_count = 0;
    g_custom_free_count  = 0;
    MkArena *a = mk_arena_new_custom(custom_alloc, custom_free, NULL);
    assert(a != NULL);
    /* alloc at least once (the arena struct itself) */
    assert(g_custom_alloc_count >= 1);
    mk_arena_free(a);
    assert(g_custom_free_count >= 1);
    PASS("arena_custom_allocator");
}

static void test_stable_alloc_alignment(void) {
    MkArena *a = mk_arena_new();
    for (int i = 1; i <= 32; i++) {
        void *p = mk_arena_stable_alloc(a, (size_t)i);
        assert(p != NULL);
        /* returned pointer must be pointer-aligned */
        assert(((uintptr_t)p % sizeof(void*)) == 0);
    }
    mk_arena_free(a);
    PASS("stable_alloc_alignment");
}

static void test_scratch_alloc(void) {
    MkArena *a = mk_arena_new();
    void *p = mk_arena_scratch_alloc(a, 64);
    assert(p != NULL);
    memset(p, 0xAB, 64);
    mk_arena_free(a);
    PASS("scratch_alloc");
}

static void test_scratch_mark_rollback(void) {
    MkArena *a = mk_arena_new();

    void *before = mk_arena_scratch_alloc(a, 8);
    assert(before != NULL);
    memset(before, 1, 8);

    mk_arena_scratch_mark(a);

    void *during = mk_arena_scratch_alloc(a, 128);
    assert(during != NULL);
    memset(during, 2, 128);

    mk_arena_scratch_rollback(a);

    /* After rollback, new alloc should reuse the freed space — pointer may
     * equal 'during'. More importantly, the arena must not crash. */
    void *after = mk_arena_scratch_alloc(a, 128);
    assert(after != NULL);
    memset(after, 3, 128);

    /* 'before' memory must still be intact */
    unsigned char *b = (unsigned char *)before;
    for (int i = 0; i < 8; i++) assert(b[i] == 1);

    mk_arena_free(a);
    PASS("scratch_mark_rollback");
}

static void test_scratch_commit(void) {
    MkArena *a = mk_arena_new();

    mk_arena_scratch_mark(a);
    void *p = mk_arena_scratch_alloc(a, 256);
    assert(p != NULL);
    memset(p, 0x55, 256);

    mk_arena_scratch_commit(a);

    /* After commit, stable alloc should succeed and not corrupt previous data */
    void *q = mk_arena_stable_alloc(a, 8);
    assert(q != NULL);

    /* Original committed data still readable */
    unsigned char *pp = (unsigned char *)p;
    for (int i = 0; i < 256; i++) assert(pp[i] == 0x55);

    mk_arena_free(a);
    PASS("scratch_commit");
}

static void test_strdup_stable(void) {
    MkArena *a = mk_arena_new();
    const char *src = "hello, world";
    char *dup = mk_arena_strdup_stable(a, src, strlen(src));
    assert(dup != NULL);
    assert(strcmp(dup, src) == 0);
    assert(dup != src);  /* must be a copy */
    mk_arena_free(a);
    PASS("strdup_stable");
}

static void test_strdup_scratch(void) {
    MkArena *a = mk_arena_new();
    const char *src = "scratch string";
    char *dup = mk_arena_strdup_scratch(a, src, strlen(src));
    assert(dup != NULL);
    assert(strcmp(dup, src) == 0);
    mk_arena_free(a);
    PASS("strdup_scratch");
}

static void test_large_allocation(void) {
    /* Exceeds default 64KB block — must trigger a new block */
    MkArena *a = mk_arena_new();
    const size_t big = 128 * 1024;
    void *p = mk_arena_stable_alloc(a, big);
    assert(p != NULL);
    memset(p, 0xCC, big);
    mk_arena_free(a);
    PASS("large_allocation");
}

static void test_many_small_allocs(void) {
    MkArena *a = mk_arena_new();
    enum { N = 1000 };
    void *ptrs[N];
    for (int i = 0; i < N; i++) {
        ptrs[i] = mk_arena_stable_alloc(a, 17);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], (unsigned char)i, 17);
    }
    /* Verify first alloc still intact after many more allocs */
    unsigned char *first = (unsigned char *)ptrs[0];
    for (int i = 0; i < 17; i++) assert(first[i] == 0);
    mk_arena_free(a);
    PASS("many_small_allocs");
}

static void test_reset_scratch(void) {
    MkArena *a = mk_arena_new();
    void *p = mk_arena_scratch_alloc(a, 64);
    assert(p != NULL);
    mk_arena_reset_scratch(a);
    /* After reset, new scratch alloc must succeed */
    void *q = mk_arena_scratch_alloc(a, 64);
    assert(q != NULL);
    mk_arena_free(a);
    PASS("reset_scratch");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Arena unit tests ===\n");
    test_new_free();
    test_custom_allocator();
    test_stable_alloc_alignment();
    test_scratch_alloc();
    test_scratch_mark_rollback();
    test_scratch_commit();
    test_strdup_stable();
    test_strdup_scratch();
    test_large_allocation();
    test_many_small_allocs();
    test_reset_scratch();
    printf("All arena tests passed.\n\n");
    return 0;
}
