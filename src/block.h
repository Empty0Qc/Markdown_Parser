/* block.h — Block-level parser internals (M4) */
#ifndef MK_BLOCK_H
#define MK_BLOCK_H

#include "../include/mk_parser.h"
#include "arena.h"
#include "ast.h"

#define MK_BLOCK_STACK_MAX  64
#define MK_BLOCK_LINE_MAX   8192
#define MK_TABLE_COL_MAX    32

/* ── Mutable text accumulation buffer (malloc-backed) ─────────────────────── */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} MkTextBuf;

/* ── One entry on the open-block stack ───────────────────────────────────── */

typedef struct MkBlockFrame {
    MkNode   *node;

    /* Fenced code block */
    char      fence_char;       /* '`' or '~'                             */
    int       fence_len;        /* opening fence length (≥3)              */

    /* List item */
    int       list_indent;      /* column of content after marker+space   */
    int       list_ordered;
    int       list_start;

    /* HTML block */
    int       html_type;        /* 1–6 per CommonMark spec                */

    /* Inline content accumulation (paragraph, heading, code block text)  */
    MkTextBuf text;

    /* Promotion flags: paragraph might become one of these               */
    int       setext_candidate; /* 1 if single-line para with text        */
    int       table_candidate;  /* 1 if first line looks like a table row */
    int       last_line_hard_break; /* previous line had 2+ trailing spaces */

    /* List tightness tracking */
    int       blank_seen;       /* blank line seen inside this frame      */
} MkBlockFrame;

/* ── Block parser ─────────────────────────────────────────────────────────── */

typedef struct MkBlockParser {
    MkArena     *arena;
    MkCallbacks  cbs;

    MkBlockFrame stack[MK_BLOCK_STACK_MAX];
    int          top;           /* stack[0] = Document, stack[top] = leaf */

    /* Streaming line buffer */
    char         line_buf[MK_BLOCK_LINE_MAX];
    size_t       line_len;

    size_t       src_offset;
    int          last_blank;    /* was last processed line blank?         */
    int          line_overflow; /* 1 while current line is being truncated */
} MkBlockParser;

/* ── Public interface (used by MkParser in M6) ───────────────────────────── */

void mk_block_init   (MkBlockParser *bp, MkArena *arena, const MkCallbacks *cbs);
void mk_block_cleanup(MkBlockParser *bp);
int  mk_block_feed   (MkBlockParser *bp, const char *data, size_t len);
int  mk_block_finish (MkBlockParser *bp);

/* Returns the root Document node (valid after mk_block_init) */
MkNode *mk_block_root(MkBlockParser *bp);

#endif /* MK_BLOCK_H */
