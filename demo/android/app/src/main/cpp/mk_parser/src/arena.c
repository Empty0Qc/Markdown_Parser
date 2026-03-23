/* arena.c — Arena allocator implementation (M2) */

#include "arena.h"

#include <stdlib.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────────────────────── */

#define MK_ARENA_BLOCK_SIZE  (64u * 1024u)   /* 64 KiB per block             */
#define MK_ARENA_ALIGN       (sizeof(void *)) /* natural pointer alignment    */

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static inline size_t align_up(size_t n) {
    return (n + MK_ARENA_ALIGN - 1) & ~(MK_ARENA_ALIGN - 1);
}

static inline char *block_data(MkArenaBlock *b) {
    return (char *)(b + 1);
}

/* ── Default system allocator ─────────────────────────────────────────────── */

static void *default_alloc(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}

static void default_free(void *ctx, void *ptr) {
    (void)ctx;
    free(ptr);
}

/* ── Block operations ─────────────────────────────────────────────────────── */

static MkArenaBlock *block_new(MkArena *a, size_t min_size) {
    size_t cap = MK_ARENA_BLOCK_SIZE;
    if (min_size > cap)
        cap = align_up(min_size);

    MkArenaBlock *b = a->alloc_fn(a->alloc_ctx, sizeof(MkArenaBlock) + cap);
    if (!b) return NULL;

    b->next = NULL;
    b->cap  = cap;
    b->used = 0;
    return b;
}

/* Try to bump-allocate from b. Returns NULL if not enough space. */
static void *block_bump(MkArenaBlock *b, size_t size) {
    size_t aligned = align_up(size);
    if (b->used + aligned > b->cap) return NULL;
    void *ptr  = block_data(b) + b->used;
    b->used   += aligned;
    return ptr;
}

/* Allocate from a chain, growing with a new block if needed. */
static void *chain_alloc(MkArena *a,
                         MkArenaBlock **head,
                         MkArenaBlock **tail,
                         size_t size)
{
    /* Fast path: current tail has space */
    if (*tail) {
        void *ptr = block_bump(*tail, size);
        if (ptr) return ptr;
    }

    /* Slow path: need a new block */
    MkArenaBlock *nb = block_new(a, size);
    if (!nb) return NULL;

    if (*tail) (*tail)->next = nb;
    else       *head         = nb;
    *tail = nb;

    return block_bump(nb, size);
}

/* Free an entire block chain. */
static void chain_free(MkArena *a, MkArenaBlock *head) {
    while (head) {
        MkArenaBlock *next = head->next;
        a->free_fn(a->alloc_ctx, head);
        head = next;
    }
}

/* ── Arena lifecycle ──────────────────────────────────────────────────────── */

static MkArena *arena_create(MkAllocFn alloc_fn, MkFreeFn free_fn, void *ctx) {
    MkArena *a = alloc_fn(ctx, sizeof(MkArena));
    if (!a) return NULL;
    memset(a, 0, sizeof(MkArena));
    a->alloc_fn  = alloc_fn;
    a->free_fn   = free_fn;
    a->alloc_ctx = ctx;
    return a;
}

MkArena *mk_arena_new(void) {
    return arena_create(default_alloc, default_free, NULL);
}

MkArena *mk_arena_new_custom(MkAllocFn alloc, MkFreeFn free_fn, void *ctx) {
    if (!alloc || !free_fn) return NULL;
    return arena_create(alloc, free_fn, ctx);
}

void mk_arena_free(MkArena *a) {
    if (!a) return;
    chain_free(a, a->stable_head);
    chain_free(a, a->scratch_head);
    a->free_fn(a->alloc_ctx, a);
}

/* ── Stable segment ───────────────────────────────────────────────────────── */

void *mk_arena_stable_alloc(MkArena *a, size_t size) {
    if (!a || !size) return NULL;
    return chain_alloc(a, &a->stable_head, &a->stable_tail, size);
}

/* ── Scratch segment ──────────────────────────────────────────────────────── */

void *mk_arena_scratch_alloc(MkArena *a, size_t size) {
    if (!a || !size) return NULL;
    return chain_alloc(a, &a->scratch_head, &a->scratch_tail, size);
}

void mk_arena_scratch_mark(MkArena *a) {
    if (!a) return;
    a->cp_block = a->scratch_tail;
    a->cp_used  = a->scratch_tail ? a->scratch_tail->used : 0;
}

void mk_arena_scratch_rollback(MkArena *a) {
    if (!a) return;

    if (a->cp_block) {
        /* Free every block after the checkpoint block */
        chain_free(a, a->cp_block->next);
        a->cp_block->next = NULL;
        a->cp_block->used = a->cp_used;
        a->scratch_tail   = a->cp_block;
    } else {
        /* Checkpoint was before the first scratch block: free all */
        chain_free(a, a->scratch_head);
        a->scratch_head = NULL;
        a->scratch_tail = NULL;
    }

    /* Leave checkpoint intact so mark/rollback can be called repeatedly */
}

void mk_arena_scratch_commit(MkArena *a) {
    if (!a || !a->scratch_head) return;

    /* Transfer scratch block chain to stable — O(1), zero copy */
    if (a->stable_tail) a->stable_tail->next = a->scratch_head;
    else                a->stable_head        = a->scratch_head;
    a->stable_tail = a->scratch_tail;

    a->scratch_head = NULL;
    a->scratch_tail = NULL;
    a->cp_block     = NULL;
    a->cp_used      = 0;
}

void mk_arena_reset_scratch(MkArena *a) {
    if (!a) return;
    chain_free(a, a->scratch_head);
    a->scratch_head = NULL;
    a->scratch_tail = NULL;
    a->cp_block     = NULL;
    a->cp_used      = 0;
}

/* ── String helpers ───────────────────────────────────────────────────────── */

char *mk_arena_strdup_stable(MkArena *a, const char *src, size_t len) {
    if (!a || !src) return NULL;
    char *dst = mk_arena_stable_alloc(a, len + 1);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

char *mk_arena_strdup_scratch(MkArena *a, const char *src, size_t len) {
    if (!a || !src) return NULL;
    char *dst = mk_arena_scratch_alloc(a, len + 1);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}
