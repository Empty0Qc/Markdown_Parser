/* inline_parser.h — Inline-level parser internals (M5) */
#ifndef MK_INLINE_PARSER_H
#define MK_INLINE_PARSER_H

#include "../include/mk_parser.h"
#include "arena.h"
#include "ast.h"

/*
 * Parse inline Markdown content from `text[0..len)` and append resulting
 * AST nodes as children of `parent`.
 *
 * Handles: code spans, emphasis, strong, strikethrough, links, images,
 *          autolinks, HTML inline, hard/soft breaks, escape sequences,
 *          GFM task list items.
 *
 * Called by the block parser (M4) when a block that holds inline content
 * (Paragraph, Heading, TableCell, …) is closed.
 */
/*
 * parser may be NULL (when MkBlockParser is used directly without MkParser).
 * When non-NULL, registered MkParserPlugins are tried for inline constructs.
 */
void mk_inline_parse(MkArena           *arena,
                     const MkCallbacks *cbs,
                     MkNode            *parent,
                     const char        *text,
                     size_t             len,
                     MkParser          *parser);

#endif /* MK_INLINE_PARSER_H */
