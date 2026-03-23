/* plugin.h — Plugin system internals (M7) */
#ifndef MK_PLUGIN_H
#define MK_PLUGIN_H

#include "../include/mk_parser.h"
#include "arena.h"
#include "ast.h"

/*
 * Check if character c is a trigger for any inline plugin registered
 * on the given parser.  Returns 1 if yes, 0 if no (or parser is NULL).
 */
int mk_plugin_is_inline_trigger(MkParser *parser, char c);

/*
 * Try all inline plugins at the current position.
 * src/len: remaining inline text from current position.
 * parent:  node to append created child to.
 * Returns: bytes consumed (0 = no plugin matched).
 * Side-effect: appends and emits the created node on success.
 */
size_t mk_plugin_try_inline(MkParser          *parser,
                             MkArena           *arena,
                             const MkCallbacks *cbs,
                             const char        *src,
                             size_t             len,
                             MkNode            *parent);

/*
 * Try all block plugins on the current line.
 * src/len: current line content (post-indent-strip).
 * out:     set to the created node on success.
 * Returns: bytes consumed (0 = no plugin matched).
 */
size_t mk_plugin_try_block(MkParser          *parser,
                            MkArena           *arena,
                            const MkCallbacks *cbs,
                            const char        *src,
                            size_t             len,
                            MkNode           **out);

/*
 * Call all transform plugins' on_node_complete for the given node.
 */
void mk_plugin_node_complete(MkParser *parser, MkNode *node, MkArena *arena);

/*
 * Accessor: return the i-th registered parser/transform plugin.
 * Used internally; returns NULL if i is out of range.
 */
const MkParserPlugin    *mk_parser_plugin_at   (MkParser *p, int i);
const MkTransformPlugin *mk_transform_plugin_at(MkParser *p, int i);
int                      mk_parser_plugin_count   (MkParser *p);
int                      mk_transform_plugin_count(MkParser *p);

#endif /* MK_PLUGIN_H */
