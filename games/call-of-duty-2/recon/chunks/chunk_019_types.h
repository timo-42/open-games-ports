/*
 * chunk_019 types: flex-generated script scanner (0x080892d8..0x0808b9dd),
 * server kick / map-rotation / restart commands (0x0808baf8..0x0808cecc).
 * Layouts recovered from consistent offsets in cod2_lnxded 1.3.
 */
#ifndef COD2_CHUNK_019_TYPES_H
#define COD2_CHUNK_019_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* flex 2.5.x yy_buffer_state, 0x30 bytes (yy_bs_lineno/yy_bs_column variant). */
struct yy_buffer_state {
    FILE *input_file;           /* +0x00 */
    byte *ch_buf;               /* +0x04 */
    byte *buf_pos;              /* +0x08 */
    int buf_size;               /* +0x0c */
    int n_chars;                /* +0x10 */
    int is_our_buffer;          /* +0x14 */
    int is_interactive;         /* +0x18 */
    int at_bol;                 /* +0x1c */
    int bs_lineno;              /* +0x20 */
    int bs_column;              /* +0x24 */
    int fill_buffer;            /* +0x28 */
    int buffer_status;          /* +0x2c: 0 new, 1 normal, 2 eof pending */
};
COD2_ASSERT_SIZE(yy_buffer_state, 0x30);
COD2_ASSERT_OFFSET(yy_buffer_state, ch_buf, 0x04);
COD2_ASSERT_OFFSET(yy_buffer_state, buf_pos, 0x08);
COD2_ASSERT_OFFSET(yy_buffer_state, buf_size, 0x0c);
COD2_ASSERT_OFFSET(yy_buffer_state, n_chars, 0x10);
COD2_ASSERT_OFFSET(yy_buffer_state, is_our_buffer, 0x14);
COD2_ASSERT_OFFSET(yy_buffer_state, is_interactive, 0x18);
COD2_ASSERT_OFFSET(yy_buffer_state, at_bol, 0x1c);
COD2_ASSERT_OFFSET(yy_buffer_state, bs_lineno, 0x20);
COD2_ASSERT_OFFSET(yy_buffer_state, bs_column, 0x24);
COD2_ASSERT_OFFSET(yy_buffer_state, fill_buffer, 0x28);
COD2_ASSERT_OFFSET(yy_buffer_state, buffer_status, 0x2c);

union lex_value_u {
    int i;
    float f;
};

/* dvar_s: name, flags/type/modified header, 4-byte value at +8. */

/* Partial client_s; stride 0xb1064 (observed). Only accessed fields are named. */

typedef void (*xcommand_t)(void);

/* ---- flex scanner state (addresses in comments) ---- */
extern int g_parse_result;                          /* 0x083deec0 */
extern byte g_lex_param;                            /* 0x083deecc */
extern union lex_value_u g_yylval;                  /* 0x08a6f4cc */
extern int g_parse_last_token;                      /* 0x08a6f4d4 */

/* flex DFA tables */

/* ---- server globals ---- */
extern struct dvar_s *g_dvar_sv_running;            /* 0x081a218c */
extern struct dvar_s *g_dvar_sv_maxclients;         /* 0x0849f74c */
extern struct dvar_s *g_dvar_0849f768;              /* 0x0849f768 */
extern struct dvar_s *g_dvar_g_gametype;            /* 0x0849f778 */
extern struct dvar_s *g_dvar_sv_mapRotation;        /* 0x0849f7a0 */
extern struct dvar_s *g_dvar_sv_mapRotationCurrent; /* 0x0849f7a4 */
extern struct dvar_s *g_dvar_084a8780;              /* 0x084a8780 */
extern char g_saved_gametype[64];                   /* 0x0849f6f4 */

/* ---- out-of-chunk callees (placeholders, rewritten by the merge queue) ---- */

/* command handlers that sit inside this address range but were not split into functions */

#endif
