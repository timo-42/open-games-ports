#ifndef COD2_CHUNK_018_TYPES_H
#define COD2_CHUNK_018_TYPES_H

#include "cod2_types.h"

/*
 * chunk 018: 0x08085744..0x080892d8
 * Contains small cursor/stack accessor helpers and one Bison-generated LALR(1)
 * parser (yyparse, 0x08085932). Table names follow Bison 1.x conventions.
 */

/* Semantic value stack element: 8 bytes.  yyvsp[k].value / yyvsp[k].pos.
 * Observed: yylval (0x08a6f4cc) is copied as two dwords; reduction actions pass
 * .value as the node handle and .pos (converted by ScrVal_StoreWordAlt) to node builders. */
struct yy_stype_t {
    int value;
    int pos;
};
COD2_ASSERT_SIZE(yy_stype_t, 8);
COD2_ASSERT_OFFSET(yy_stype_t, value, 0);
COD2_ASSERT_OFFSET(yy_stype_t, pos, 4);

/* New-signature declarations (cod2_prototypes.h is regenerated from the chunk JSON by the merge lane). */
unsigned short Cursor_ReadShortZx(const char **cursor);
int Cursor_ReadShort(const char **cursor);
int Cursor_ReadInt(const char **cursor);

#endif
