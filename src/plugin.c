/* plugin.c — Plugin system implementation (M7) */

#include "plugin.h"

/* ════════════════════════════════════════════════════════════════════════════
 * Forward: access parser internals
 * (MkParser is defined in parser.c; use the accessor shim declared below)
 * ════════════════════════════════════════════════════════════════════════════ */

extern const MkParserPlugin    *mk_parser_plugin_at        (MkParser *p, int i);
extern const MkTransformPlugin *mk_transform_plugin_at     (MkParser *p, int i);
extern int                      mk_parser_plugin_count     (MkParser *p);
extern int                      mk_transform_plugin_count  (MkParser *p);
extern int                      mk_parser_trigger_map_test (MkParser *p, unsigned char c);

/* ════════════════════════════════════════════════════════════════════════════
 * Inline trigger check
 * ════════════════════════════════════════════════════════════════════════════ */

int mk_plugin_is_inline_trigger(MkParser *parser, char c) {
    /* O(1) bitmap lookup — populated when plugins are registered */
    return mk_parser_trigger_map_test(parser, (unsigned char)c);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Inline plugin invocation
 * ════════════════════════════════════════════════════════════════════════════ */

size_t mk_plugin_try_inline(MkParser          *parser,
                             MkArena           *arena,
                             const MkCallbacks *cbs,
                             const char        *src,
                             size_t             len,
                             MkNode            *parent)
{
    if (!parser || !len) return 0;
    int n = mk_parser_plugin_count(parser);
    for (int i = 0; i < n; i++) {
        const MkParserPlugin *pl = mk_parser_plugin_at(parser, i);
        if (!pl || !pl->try_inline) continue;

        MkNode *node  = NULL;
        size_t  eaten = pl->try_inline(parser, arena, src, len, &node);
        if (eaten && node) {
            mk_node_append_child(parent, node);
            if (cbs && cbs->on_node_open)
                cbs->on_node_open(cbs->user_data, node);
            if (cbs && cbs->on_node_close)
                cbs->on_node_close(cbs->user_data, node);
            return eaten;
        }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Block plugin invocation
 * ════════════════════════════════════════════════════════════════════════════ */

size_t mk_plugin_try_block(MkParser          *parser,
                            MkArena           *arena,
                            const MkCallbacks *cbs,
                            const char        *src,
                            size_t             len,
                            MkNode           **out)
{
    (void)cbs;
    if (!parser || !len || !out) return 0;
    int n = mk_parser_plugin_count(parser);
    for (int i = 0; i < n; i++) {
        const MkParserPlugin *pl = mk_parser_plugin_at(parser, i);
        if (!pl || !pl->try_block) continue;

        MkNode *node  = NULL;
        size_t  eaten = pl->try_block(parser, arena, src, len, &node);
        if (eaten && node) {
            *out = node;
            return eaten;
        }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Transform plugin invocation
 * ════════════════════════════════════════════════════════════════════════════ */

void mk_plugin_node_complete(MkParser *parser, MkNode *node, MkArena *arena) {
    if (!parser || !node) return;
    int n = mk_transform_plugin_count(parser);
    for (int i = 0; i < n; i++) {
        const MkTransformPlugin *pl = mk_transform_plugin_at(parser, i);
        if (pl && pl->on_node_complete)
            pl->on_node_complete(node, arena);
    }
}
