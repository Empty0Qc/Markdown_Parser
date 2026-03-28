/* parser.c — MkParser lifecycle + Push/Pull API (M6)
 *
 * MkParser is a thin wrapper around MkBlockParser that:
 *   1. Wires internal callbacks to build a delta queue
 *   2. Forwards events to user push-callbacks immediately
 *   3. Exposes a Pull API (mk_pull_delta) for event-loop / FFI consumers
 *
 * Memory model:
 *   - MkParser itself: malloc'd, freed by mk_parser_free
 *   - MkDelta nodes:   malloc'd, freed by mk_delta_free (or on mk_parser_free)
 *   - Delta text:      copied into stable arena, lives until mk_arena_free
 *   - AST nodes:       arena-owned, freed when arena is freed
 */

#include "arena.h"
#include "ast.h"
#include "block.h"
#include "inline_parser.h"
#include "plugin.h"
#include <string.h>

#include <stdlib.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Internal MkParser struct
 * ════════════════════════════════════════════════════════════════════════════ */

struct MkParser {
    MkArena      *arena;
    MkCallbacks   user_cbs;    /* user-provided push callbacks (may be NULL) */
    MkBlockParser bp;          /* the underlying block (+ inline) parser      */

    /* Pull delta queue (singly-linked, FIFO) */
    MkDelta      *dq_head;     /* next to dequeue */
    MkDelta      *dq_tail;     /* last enqueued   */

    int           finished;    /* 1 after mk_finish has been called (idempotent guard) */

    /* Plugin slots (M7 fills these in) */
    const MkParserPlugin    *parser_plugins[16];
    const MkTransformPlugin *transform_plugins[16];
    int                      n_parser_plugins;
    int                      n_transform_plugins;

    /* Bitmap: trigger_map[c] != 0 iff any registered plugin triggers on c.
     * Built incrementally in mk_register_parser_plugin; used in the hot path
     * instead of iterating all plugins and calling strchr per character. */
    uint8_t                  trigger_map[256];
};

/* ════════════════════════════════════════════════════════════════════════════
 * Delta queue helpers
 * ════════════════════════════════════════════════════════════════════════════ */

static MkDelta *delta_alloc(MkParser *p, MkDeltaType type, MkNode *node) {
    MkDelta *d = mk_arena_stable_alloc(p->arena, sizeof(MkDelta));
    if (!d) return NULL;
    d->type     = type;
    d->node     = node;
    d->text     = NULL;
    d->text_len = 0;
    d->next     = NULL;
    return d;
}

static void dq_push(MkParser *p, MkDelta *d) {
    if (!d) return;
    if (p->dq_tail) p->dq_tail->next = d;
    else            p->dq_head        = d;
    p->dq_tail = d;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Internal callbacks — always enqueue AND forward to user push callbacks
 * ════════════════════════════════════════════════════════════════════════════ */

static void internal_open(void *ud, MkNode *n) {
    MkParser *p = (MkParser *)ud;
    dq_push(p, delta_alloc(p, MK_DELTA_NODE_OPEN, n));
    if (p->user_cbs.on_node_open)
        p->user_cbs.on_node_open(p->user_cbs.user_data, n);
}

static void internal_close(void *ud, MkNode *n) {
    MkParser *p = (MkParser *)ud;
    /* Transform plugins see the fully-built node before it's announced */
    mk_plugin_node_complete(p, n, p->arena);
    dq_push(p, delta_alloc(p, MK_DELTA_NODE_CLOSE, n));
    if (p->user_cbs.on_node_close)
        p->user_cbs.on_node_close(p->user_cbs.user_data, n);
}

static void internal_text(void *ud, MkNode *n, const char *text, size_t len) {
    MkParser   *p    = (MkParser *)ud;
    /* Copy into stable arena — text may point into a transient line buffer */
    const char *copy = mk_arena_strdup_stable(p->arena, text, len);
    MkDelta    *d    = delta_alloc(p, MK_DELTA_TEXT, n);
    if (d) {
        d->text     = copy;
        d->text_len = len;
        dq_push(p, d);
    }
    if (p->user_cbs.on_text)
        p->user_cbs.on_text(p->user_cbs.user_data, n, text, len);
}

static void internal_modify(void *ud, MkNode *n) {
    MkParser *p = (MkParser *)ud;
    dq_push(p, delta_alloc(p, MK_DELTA_NODE_MODIFY, n));
    if (p->user_cbs.on_node_modify)
        p->user_cbs.on_node_modify(p->user_cbs.user_data, n);
}

static void internal_error(void *ud, MkErrorCode code, const char *msg) {
    MkParser *p = (MkParser *)ud;
    if (p->user_cbs.on_error)
        p->user_cbs.on_error(p->user_cbs.user_data, code, msg);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Parser lifecycle
 * ════════════════════════════════════════════════════════════════════════════ */

MkParser *mk_parser_new(MkArena *arena, const MkCallbacks *callbacks) {
    if (!arena) return NULL;

    MkParser *p = malloc(sizeof(MkParser));
    if (!p) return NULL;
    memset(p, 0, sizeof(MkParser));

    p->arena = arena;
    if (callbacks) p->user_cbs = *callbacks;

    /* Wire the block parser to our internal callbacks */
    MkCallbacks icbs = {
        .user_data      = p,
        .on_node_open   = internal_open,
        .on_node_close  = internal_close,
        .on_text        = internal_text,
        .on_node_modify = internal_modify,
        .on_error       = internal_error,
    };
    mk_block_init(&p->bp, arena, &icbs);

    return p;
}

void mk_parser_free(MkParser *p) {
    if (!p) return;
    /* Flush any pending open blocks — safe to call even if mk_finish was
     * already called (finished flag prevents double close). */
    if (!p->finished) mk_finish(p);
    mk_block_cleanup(&p->bp);
    /* Delta nodes are arena-allocated — they are freed when the arena is freed.
     * Just clear the queue pointers so the parser is in a clean state. */
    p->dq_head = NULL;
    p->dq_tail = NULL;
    free(p);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Push API
 * ════════════════════════════════════════════════════════════════════════════ */

int mk_feed(MkParser *p, const char *data, size_t len) {
    if (!p || !data) return -1;
    return mk_block_feed(&p->bp, data, len);
}

int mk_finish(MkParser *p) {
    if (!p) return -1;
    if (p->finished) return 0;   /* idempotent: no-op if already finished */
    p->finished = 1;
    return mk_block_finish(&p->bp);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Pull API
 * ════════════════════════════════════════════════════════════════════════════ */

MkDelta *mk_pull_delta(MkParser *p) {
    if (!p || !p->dq_head) return NULL;
    MkDelta *d   = p->dq_head;
    p->dq_head   = d->next;
    if (!p->dq_head) p->dq_tail = NULL;
    d->next = NULL;
    return d;
}

void mk_delta_free(MkDelta *d) {
    /* Delta nodes are arena-allocated; both the node and its text live in
     * the stable arena and are freed when the arena is freed.
     * This function is kept for API compatibility but is now a no-op. */
    (void)d;
}

/* Discard all pending (unconsumed) deltas without freeing arena memory. */
void mk_drain_deltas(MkParser *p) {
    if (!p) return;
    p->dq_head = NULL;
    p->dq_tail = NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ════════════════════════════════════════════════════════════════════════════ */

MkNode *mk_get_root(MkParser *p) {
    if (!p) return NULL;
    return mk_block_root(&p->bp);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Plugin registration  (M7 will populate the vtables)
 * ════════════════════════════════════════════════════════════════════════════ */

int mk_register_parser_plugin(MkParser *p, const MkParserPlugin *plugin) {
    if (!p || !plugin) return -1;
    if (p->n_parser_plugins >= 16) return -1;
    p->parser_plugins[p->n_parser_plugins++] = plugin;
    /* Update trigger bitmap so the hot path avoids per-plugin strchr */
    if (plugin->inline_triggers) {
        for (const char *t = plugin->inline_triggers; *t; t++)
            p->trigger_map[(unsigned char)*t] = 1;
    }
    return 0;
}

int mk_register_transform_plugin(MkParser *p, const MkTransformPlugin *plugin) {
    if (!p || !plugin) return -1;
    if (p->n_transform_plugins >= 16) return -1;
    p->transform_plugins[p->n_transform_plugins++] = plugin;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Plugin accessor shims (called by plugin.c via extern declarations)
 * ════════════════════════════════════════════════════════════════════════════ */

/* Fast trigger lookup used by plugin.c instead of iterating all plugins */
int mk_parser_trigger_map_test(MkParser *p, unsigned char c) {
    return p ? (int)p->trigger_map[c] : 0;
}

const MkParserPlugin *mk_parser_plugin_at(MkParser *p, int i) {
    if (!p || i < 0 || i >= p->n_parser_plugins) return NULL;
    return p->parser_plugins[i];
}

const MkTransformPlugin *mk_transform_plugin_at(MkParser *p, int i) {
    if (!p || i < 0 || i >= p->n_transform_plugins) return NULL;
    return p->transform_plugins[i];
}

int mk_parser_plugin_count(MkParser *p) {
    return p ? p->n_parser_plugins : 0;
}

int mk_transform_plugin_count(MkParser *p) {
    return p ? p->n_transform_plugins : 0;
}
