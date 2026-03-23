/* arena.h — Arena allocator internals (M2)
 *
 * Two-segment design:
 *   stable  — completed nodes, grows monotonically, freed in chunks
 *   scratch — pending/speculative nodes, supports checkpoint + rollback
 */
#ifndef MK_ARENA_H
#define MK_ARENA_H

#include "../include/mk_parser.h"

/* ── Block (linked list node) ─────────────────────────────────────────────── */

typedef struct MkArenaBlock {
    struct MkArenaBlock *next;
    size_t               cap;   /* usable bytes: data[0..cap-1] */
    size_t               used;
    /* cap bytes of data follow immediately in memory */
} MkArenaBlock;

/* ── Arena struct (opaque in public header) ───────────────────────────────── */

struct MkArena {
    MkAllocFn     alloc_fn;
    MkFreeFn      free_fn;
    void         *alloc_ctx;

    /* Stable segment: completed nodes */
    MkArenaBlock *stable_head;
    MkArenaBlock *stable_tail;

    /* Scratch segment: pending / speculative nodes */
    MkArenaBlock *scratch_head;
    MkArenaBlock *scratch_tail;

    /* Checkpoint for rollback */
    MkArenaBlock *cp_block;  /* scratch_tail at mark time (NULL = before head) */
    size_t        cp_used;   /* scratch_tail->used at mark time                */
};

/* ── Internal allocation API ──────────────────────────────────────────────── */

/* Allocate from stable segment (never rolled back) */
void *mk_arena_stable_alloc(MkArena *arena, size_t size);

/* Allocate from scratch segment (can be rolled back) */
void *mk_arena_scratch_alloc(MkArena *arena, size_t size);

/* Save a rollback point in scratch */
void  mk_arena_scratch_mark(MkArena *arena);

/* Discard all scratch allocations since last mark */
void  mk_arena_scratch_rollback(MkArena *arena);

/* Commit scratch to stable (O(1) block-chain transfer, no copy) */
void  mk_arena_scratch_commit(MkArena *arena);

/* Duplicate a string into the stable segment */
char *mk_arena_strdup_stable(MkArena *arena, const char *src, size_t len);

/* Duplicate a string into the scratch segment */
char *mk_arena_strdup_scratch(MkArena *arena, const char *src, size_t len);

#endif /* MK_ARENA_H */
