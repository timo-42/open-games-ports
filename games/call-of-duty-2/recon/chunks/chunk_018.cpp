#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_018_types.h"

// 0x08085744  size=54  FUN_08085744  status=translated  naming=unknown
// Descriptive name: floor(x + 0.5) computed in double (x87), then truncated (fistp with RC=truncate).
int Float_RoundToInt_08085744(float x)
{
    return (int)floor((double)x + 0.5);
}

// 0x0808577a  size=47  FUN_0808577a  status=translated  naming=unknown
// Descriptive name: copies three dwords src -> dst (integer moves, bit-exact).
void Vec3_CopyBits_0808577a(const uint32 *src, uint32 *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// 0x080857aa  size=19  FUN_080857aa  status=translated  naming=unknown
// rdtsc; only the low dword (eax) is returned, edx is discarded.
uint32 Sys_ReadTimestampLow(void)
{
    uint32 lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return lo;
}

// 0x080857be  size=22  FUN_080857be  status=translated  naming=unknown
// Descriptive name: reads the dword index*4 bytes below the pointer stored in scrVmPub.
int Stack_PeekFromTop(int index)
{
    return *(int *)((char *)scrVmPub.localVars - index * 4);
}

// 0x080857d4  size=22  FUN_080857d4  status=translated  naming=unknown
// Descriptive name: Stack_PeekFromTop with a zero-extended byte index loaded through a pointer.
int Stack_PeekFromTopByte(const char *indexPtr)
{
    return Stack_PeekFromTop(*(const byte *)indexPtr);
}

// 0x080857ea  size=34  FUN_080857ea  status=translated  naming=unknown
// Descriptive name: reads a dword at *cursor and advances the cursor by 4 bytes.
int Cursor_ReadInt(const char **cursor)
{
    int value = *(const int *)*cursor;
    *cursor = *cursor + 4;
    return value;
}

// 0x0808580c  size=32  FUN_0808580c  status=translated  naming=unknown
// Descriptive name: returns the cursor, then advances it by 12 bytes (three dwords).
const float *Cursor_AdvanceVec3(const char **cursor)
{
    const char *old = *cursor;
    *cursor = *cursor + 0xc;
    return (const float *)old;
}

// 0x0808582c  size=35  FUN_0808582c  status=translated  naming=unknown
// Descriptive name: returns the cursor, then advances it by count*4 bytes.
const char *Cursor_AdvanceDwords(const char **cursor, int count)
{
    const char *old = *cursor;
    *cursor = *cursor + count * 4;
    return old;
}

// 0x08085850  size=22  FUN_08085850  status=translated  naming=unknown
// Descriptive name: wrapper over Cursor_ReadShort returning the low 16 bits zero-extended (movzx eax,ax).
unsigned short Cursor_ReadShortZx(const char **cursor)
{
    return (unsigned short)Cursor_ReadShort(cursor);
}

// 0x08085866  size=19  FUN_08085866  status=translated  naming=unknown
// Descriptive name: thin wrapper; returns Cursor_ReadInt unchanged (Ghidra shows void, eax is passed through).
int Cursor_ReadIntAlias1(const char **cursor)
{
    return Cursor_ReadInt(cursor);
}

// 0x0808587a  size=40  FUN_0808587a  status=translated  naming=unknown
// Descriptive name: reads a dword at *cursor, reinterprets as float (fld dword), advances by 4; returned in st0.
float Cursor_ReadFloat(const char **cursor)
{
    float value = *(const float *)*cursor;
    *cursor = *cursor + 4;
    return value;
}

// 0x080858a2  size=19  FUN_080858a2  status=translated  naming=unknown
// Descriptive name: second thin wrapper over Cursor_ReadInt (identical body to 0x08085866).
const char *Cursor_ReadIntAlias2(const char **cursor)
{
    return (const char *)Cursor_ReadInt(cursor);
}

// 0x080858b6  size=37  FUN_080858b6  status=translated  naming=unknown
// Descriptive name: reads a 16-bit value at *cursor (sign-extended result), advances by 2 bytes.
int Cursor_ReadShort(const char **cursor)
{
    short value = *(const short *)*cursor;
    *cursor = *cursor + 2;
    return (int)value;
}

// 0x080858dc  size=38  FUN_080858dc  status=translated  naming=unknown
// Descriptive name: forwards (a, byte g_083deecc, 13) to ScrStr_ReinternLower and returns its result.
int ScrParse_ReinternIdentifierLower(int a)
{
    return ScrStr_ReinternLower(a, (int)g_083deecc, 0xd);
}

// 0x08085902  size=30  FUN_08085902  status=translated  naming=unknown
// Descriptive name: forwards (a, byte g_083deecc) to ScrStr_SetFlagOrDecRef and returns its result.
int ScrParse_ReleaseStringWithLexParam(int a)
{
    /* callee returns void; eax is left as the callee left it (no explicit return in the binary) */
    ScrStr_SetFlagOrDecRef(a, g_083deecc);
}

// 0x08085920  size=18  FUN_08085920  status=translated  naming=inferred
// Bison yydestruct stub: only substitutes the default message "Deleting" (rodata 0x08147a3f) when msg==NULL
// into its own argument slot; no observable effect. Name from Bison convention and its callers' "Error: popping" strings.
void yydestruct(const char *msg, int type, struct yy_stype_t *value)
{
    if (msg == 0)
        msg = "Deleting";
}

// 0x08085932  size=14758  FUN_08085932  status=translated  naming=inferred
// Bison 1.x LALR(1) yyparse(void). Evidence: strings "parser stack overflow", "syntax error",
// "Error: popping", "Error: discarding", "Error: discarding lookahead"; tables yypact/yydefact/yytable/yycheck;
// yylex = yylex, yyerror = yyerror. Reduction actions (cases 2..0x83) call unnamed node builders.
// Initial yyvs[0] = yylval; stacks start at 200 entries, grow by doubling up to 10000 using alloca.
int yyparse(void)
{
    short yyssa[200];
    struct yy_stype_t yyvsa[200];
    short *yyss = yyssa;
    short *yyssp = yyssa;
    struct yy_stype_t *yyvs = yyvsa;
    struct yy_stype_t *yyvsp = yyvsa;
    unsigned int yystacksize = 200;
    int yystate = 0;
    int yyerrstatus = 0;
    int yyn;
    int yylen;
    unsigned int yytoken = 0;
    struct yy_stype_t yyval;
    int t824, t828, t82c, t830, t834, t838, t83c, t840, t844, t848, t84c, t850, t854, t858, t85c, t860, t864, t868, t86c, t870, t874, t878, t87c, t880, t884, t888, t88c, t890, t894, t898, t89c, t8a0, t8a4, t8a8, t8ac, t8b0, t8b4, t8b8, t8bc, t8c0, t8c4, t8c8, t8cc, t8d0, t8d4, t8d8, t8dc, t8e0, t8e4, t8e8, t8ec, t8f0, t8f4, t8f8, t8fc, t900, t904, t908, t90c, t910, t914, t918, t91c, t920, t924, t928, t92c, t930, t934, t938, t93c, t940, t944, t948, t94c, t950, t954, t958, t95c, t960, t964, t968, t96c, t970, t974, t978, t97c, t980, t984, t988, t98c, t990, t994, t998, t99c, t9a0, t9a4, t9a8, t9ac, t9b0, t9b4, t9b8, t9bc, t9c0, t9c4, t9c8, t9cc, t9d0, t9d4, t9d8, t9dc, t9e0, t9e4, t9e8, t9ec, t9f0, t9f4, t9f8, t9fc, ta00, ta04, ta08, ta0c, ta10, ta14, ta18, ta1c, ta20, ta24, ta28, ta2c, ta30, ta34, ta38, ta3c, ta40, ta44, ta48, ta4c, ta50, ta54, ta58, ta5c, ta60, ta64, ta68, ta6c, ta70, ta74, ta78, ta7c, ta80, ta84, ta88, ta8c, ta90, ta94, ta98, ta9c, taa0, taa4, taa8, taac, tab0, tab4, tab8, tabc, tac0, tac4, tac8, tacc, tad0, tad4;
    int iVar4;

    yynerrs = 0;
    yychar = -2;
    yyvsa[0] = yylval;

yynewstate:
    *yyssp = (short)yystate;
    if (yyss + (yystacksize - 1) <= yyssp) {
        int yyoldidx = (int)(yyssp - yyss);
        int yyused = yyoldidx + 1;
        char *yyblock;
        short *yynewss;
        struct yy_stype_t *yynewvs;
        if (yystacksize > 9999)
            goto yyoverflow;
        yystacksize = yystacksize * 2;
        if (yystacksize > 10000)
            yystacksize = 10000;
        yyblock = (char *)__builtin_alloca(yystacksize * 10 + 16);
        yynewss = (short *)yyblock;
        if (yynewss == 0)
            goto yyoverflow;
        memcpy(yynewss, yyss, yyused * 2);
        yynewvs = (struct yy_stype_t *)(((unsigned int)yyblock + yystacksize * 2 + 7) & 0xfffffff8u);
        memcpy(yynewvs, yyvs, yyused * 8);
        yyssp = yynewss + yyoldidx;
        yyvsp = yynewvs + yyused - 1;
        yyss = yynewss;
        yyvs = yynewvs;
        if (yyss + (yystacksize - 1) <= yyssp)
            goto yyabortlab;
    }

    yyn = yypact[yystate];
    if (yyn == -105)
        goto yydefault;

    if (yychar == -2)
        yychar = yylex();
    if (yychar < 1) {
        yytoken = 0;
        yychar = 0;
    } else {
        if (yychar < 0x159)
            yytoken = yytranslate[yychar];
        else
            yytoken = 2;
    }
    yyn = yyn + (int)yytoken;
    if (yyn < 0 || yyn > 0x522 || yycheck[yyn] != (int)yytoken)
        goto yydefault;
    yyn = yytable[yyn];
    if (yyn < 1) {
        if (yyn != 0 && yyn != -95) {
            yyn = -yyn;
            goto yyreduce;
        }
        goto yyerrlab;
    }
    if (yyn == 0x36)
        return 0;
    if (yychar != 0)
        yychar = -2;
    yyvsp[1] = yylval;
    if (yyerrstatus != 0)
        yyerrstatus = yyerrstatus - 1;
    yyvsp = yyvsp + 1;
    yystate = yyn;
    yyssp = yyssp + 1;
    goto yynewstate;

yydefault:
    yyn = yydefact[yystate];
    if (yyn == 0)
        goto yyerrlab;

yyreduce:
    yylen = yyr2[yyn];
    yyval = yyvsp[1 - yylen];
    switch (yyn) {
    case 0x2:
        ScrBox_Make2Alt((void**)(g_scr_parse_result), yyvsp[-1].value, yyvsp[0].value);
        break;
    case 0x3:
        ScrBox_Make2((void**)(g_scr_parse_result), 0x41, yyvsp[0].value);
        break;
    case 0x4:
        ScrBox_Make2((void**)(g_scr_parse_result), 0x52, yyvsp[0].value);
        break;
    case 0x5:
        ScrBox_Make2((void**)(g_scr_parse_result), 0x52, yyvsp[0].value);
        break;
    case 0x6:
        ScrVal_StoreWordAlt((uint32*)(&t830), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 6, yyvsp[0].value, t830);
        break;
    case 0x7:
        ScrVal_StoreWordAlt((uint32*)(&t82c), yyvsp[-1].pos);
        ScrVal_StoreWordAlt((uint32*)(&t828), yyvsp[0].pos);
        ScrVal_StoreWordAlt((uint32*)(&t824), yyvsp[-2].pos);
        ScrBox_Make6((void**)(&yyval.value), 0x2f, yyvsp[-2].value, yyvsp[0].value, t824, t828, t82c);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x8:
        ScrVal_StoreWordAlt((uint32*)(&t834), yyvsp[-1].pos);
        ScrVal_StoreWordAlt((uint32*)(&t838), yyvsp[0].pos);
        ScrVal_StoreWordAlt((uint32*)(&t83c), yyvsp[-2].pos);
        ScrBox_Make6((void**)(&yyval.value), 0x30, yyvsp[-2].value, yyvsp[0].value, t83c, t838, t834);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x9:
        ScrVal_StoreWordAlt((uint32*)(&t840), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t844), 0x66);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t844, t840);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0xa:
        ScrVal_StoreWordAlt((uint32*)(&t848), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t84c), 0x67);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t84c, t848);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0xb:
        ScrVal_StoreWordAlt((uint32*)(&t850), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t854), 0x68);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t854, t850);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0xc:
        ScrVal_StoreWordAlt((uint32*)(&t858), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t85c), 0x69);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t85c, t858);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0xd:
        ScrVal_StoreWordAlt((uint32*)(&t860), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t864), 0x6a);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t864, t860);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0xe:
        ScrVal_StoreWordAlt((uint32*)(&t868), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t86c), 0x6b);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t86c, t868);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0xf:
        ScrVal_StoreWordAlt((uint32*)(&t870), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t874), 0x6c);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t874, t870);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x10:
        ScrVal_StoreWordAlt((uint32*)(&t878), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t87c), 0x6d);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t87c, t878);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x11:
        ScrVal_StoreWordAlt((uint32*)(&t880), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t884), 0x6e);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t884, t880);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x12:
        ScrVal_StoreWordAlt((uint32*)(&t888), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t88c), 0x6f);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t88c, t888);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x13:
        ScrVal_StoreWordAlt((uint32*)(&t890), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t894), 0x70);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t894, t890);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x14:
        ScrVal_StoreWordAlt((uint32*)(&t898), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t89c), 0x71);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t89c, t898);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x15:
        ScrVal_StoreWordAlt((uint32*)(&t8a0), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t8a4), 0x72);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t8a4, t8a0);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x16:
        ScrVal_StoreWordAlt((uint32*)(&t8a8), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t8ac), 0x73);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t8ac, t8a8);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x17:
        ScrVal_StoreWordAlt((uint32*)(&t8b0), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t8b4), 0x74);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t8b4, t8b0);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x18:
        ScrVal_StoreWordAlt((uint32*)(&t8b8), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t8bc), 0x75);
        ScrBox_Make5((void**)(&yyval.value), 0x31, yyvsp[-2].value, yyvsp[0].value, t8bc, t8b8);
        yyval.pos = yyvsp[-1].pos;
        break;
    case 0x19:
        ScrVal_StoreWordAlt((uint32*)(&t8c0), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x32, yyvsp[0].value, t8c0);
        break;
    case 0x1a:
        ScrVal_StoreWordAlt((uint32*)(&t8c4), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x33, yyvsp[0].value, t8c4);
        break;
    case 0x1b:
        ScrBox_Make2((void**)(&yyval.value), 0x41, yyvsp[0].value);
        break;
    case 0x1c:
        ScrBox_Make1((void**)(&yyval.value), 0);
        break;
    case 0x1d:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        yyval.value = yyvsp[0].value;
        break;
    case 0x1e:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        yyval.value = yyvsp[0].value;
        break;
    case 0x1f:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t8c8), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x15, yyvsp[-2].value, yyvsp[0].value, t8c8);
        g_scr_dependency_record_count = g_scr_dependency_record_count + 1;
        break;
    case 0x20:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t8cc), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x14, yyvsp[0].value, t8cc);
        break;
    case 0x21:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t8d0), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x15, yyvsp[-2].value, yyvsp[0].value, t8d0);
        yyval.pos = yyvsp[-1].pos;
        g_scr_dependency_record_count = g_scr_dependency_record_count + 1;
        break;
    case 0x22:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t8d4), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x14, yyvsp[0].value, t8d4);
        break;
    case 0x23:
        ScrVal_StoreWordAlt((uint32*)(&t8d8), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x12, yyvsp[0].value, t8d8);
        break;
    case 0x24:
        ScrVal_StoreWordAlt((uint32*)(&t8dc), yyvsp[-2].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x16, yyvsp[-2].value, t8dc);
        break;
    case 0x25:
        ScrVal_StoreWordAlt((uint32*)(&t8e0), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x1a, yyvsp[0].value, t8e0);
        break;
    case 0x26:
        ScrVal_StoreWordAlt((uint32*)(&t8e4), yyvsp[0].pos);
        ScrVal_StoreWordAlt((uint32*)(&t8e8), yyvsp[-1].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x1e, yyvsp[0].value, t8e8, t8e4);
        yyval.pos = yyvsp[0].pos;
        break;
    case 0x27:
        ScrVal_StoreWordAlt((uint32*)(&t8ec), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x17, yyvsp[-3].value, yyvsp[-1].value, t8ec);
        yyval.pos = yyvsp[-2].pos;
        break;
    case 0x28:
        ScrVal_StoreWordAlt((uint32*)(&t8f0), yyvsp[-2].pos);
        ScrVal_StoreWordAlt((uint32*)(&t8f4), yyvsp[-4].pos);
        ScrBox_Make6((void**)(&yyval.value), 0x18, yyvsp[-4].value, yyvsp[-3].value, yyvsp[-1].value, t8f4, t8f0);
        yyval.pos = yyvsp[-2].pos;
        break;
    case 0x29:
        ScrVal_StoreWordAlt((uint32*)(&t8f8), yyvsp[-2].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x2e, yyvsp[-1].value, t8f8);
        break;
    case 0x2a:
        ScrVal_StoreWordAlt((uint32*)(&t8fc), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 7, yyvsp[0].value, t8fc);
        break;
    case 0x2b:
        ScrVal_StoreWordAlt((uint32*)(&t900), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 8, yyvsp[0].value, t900);
        break;
    case 0x2c:
        ScrVal_StoreWordAlt((uint32*)(&t904), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 9, yyvsp[0].value, t904);
        break;
    case 0x2d:
        ScrVal_StoreWordAlt((uint32*)(&t908), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 10, yyvsp[0].value, t908);
        break;
    case 0x2e:
        ScrVal_StoreWordAlt((uint32*)(&t90c), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0xb, yyvsp[0].value, t90c);
        break;
    case 0x2f:
        ScrVal_StoreWordAlt((uint32*)(&t910), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0xc, yyvsp[0].value, t910);
        break;
    case 0x30:
        ScrBox_Make2((void**)(&yyval.value), 0x13, yyvsp[0].value);
        break;
    case 0x31:
        ScrVal_StoreWordAlt((uint32*)(&t914), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x11, yyvsp[0].value, t914);
        break;
    case 0x32:
        ScrVal_StoreWordAlt((uint32*)(&t918), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x1f, t918);
        break;
    case 0x33:
        ScrVal_StoreWordAlt((uint32*)(&t91c), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x20, t91c);
        break;
    case 0x34:
        ScrVal_StoreWordAlt((uint32*)(&t920), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x22, t920);
        break;
    case 0x35:
        ScrVal_StoreWordAlt((uint32*)(&t924), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x23, t924);
        break;
    case 0x36:
        ScrVal_StoreWordAlt((uint32*)(&t928), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x24, t928);
        break;
    case 0x37:
        ScrVal_StoreWordAlt((uint32*)(&t92c), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x34, yyvsp[-1].value, t92c);
        yyval.pos = yyvsp[0].pos;
        break;
    case 0x38:
        ScrVal_StoreWordAlt((uint32*)(&t930), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x12, yyvsp[0].value, t930);
        break;
    case 0x39:
        ScrVal_StoreWordAlt((uint32*)(&t934), yyvsp[-1].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x42, t934);
        break;
    case 0x3a:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t938), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x43, yyvsp[0].value, t938);
        break;
    case 0x3b:
        ScrVal_StoreWordAlt((uint32*)(&t93c), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x48, t93c);
        break;
    case 0x3c:
        ScrVal_StoreWordAlt((uint32*)(&t940), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x49, t940);
        break;
    case 0x3d:
        ScrVal_StoreWordAlt((uint32*)(&t944), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x4a, t944);
        break;
    case 0x3e:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t948), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0xf, yyvsp[-2].value, yyvsp[0].value, t948);
        yyval.pos = yyvsp[0].pos;
        break;
    case 0x3f:
        ScrVal_StoreWordAlt((uint32*)(&t94c), yyvsp[-1].pos);
        ScrVal_StoreWordAlt((uint32*)(&t950), yyvsp[-3].pos);
        ScrBox_Make5((void**)(&yyval.value), 0xd, yyvsp[-3].value, yyvsp[-1].value, t950, t94c);
        yyval.pos = yyvsp[-2].pos;
        break;
    case 0x40:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t954), yyvsp[0].pos);
        ScrBox_Make3((void**)(&yyval.value), 4, yyvsp[0].value, t954);
        break;
    case 0x41:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&t958), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x4f, yyvsp[0].value, t958);
        break;
    case 0x42:
        ScrVal_StoreWordAlt((uint32*)(&t95c), yyvsp[-2].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x35, yyvsp[-2].value, t95c);
        yyval.pos = yyvsp[0].pos;
        break;
    case 0x43:
        ScrVal_StoreWordAlt((uint32*)(&t960), yyvsp[0].pos);
        ScrVal_StoreWordAlt((uint32*)(&t964), yyvsp[-1].pos);
        ScrBox_Make5((void**)(&yyval.value), 2, yyvsp[-2].value, yyvsp[0].value, t964, t960);
        break;
    case 0x44:
        ScrVal_StoreWordAlt((uint32*)(&t968), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x1b, yyvsp[0].value, t968);
        break;
    case 0x45:
        ScrVal_StoreWordAlt((uint32*)(&t96c), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x1c, t96c);
        break;
    case 0x46:
        ScrVal_StoreWordAlt((uint32*)(&t970), yyvsp[-1].pos);
        ScrVal_StoreWordAlt((uint32*)(&t974), yyvsp[0].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x1d, yyvsp[0].value, t974, t970);
        break;
    case 0x47:
        ScrVal_StoreWordAlt((uint32*)(&t978), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x29, yyvsp[-1].value, t978);
        break;
    case 0x48:
        ScrVal_StoreWordAlt((uint32*)(&t97c), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x2a, yyvsp[-1].value, t97c);
        break;
    case 0x49:
        ScrVal_StoreWordAlt((uint32*)(&t980), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t984), 0x66);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t984, t980);
        break;
    case 0x4a:
        ScrVal_StoreWordAlt((uint32*)(&t988), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t98c), 0x67);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t98c, t988);
        break;
    case 0x4b:
        ScrVal_StoreWordAlt((uint32*)(&t990), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t994), 0x68);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t994, t990);
        break;
    case 0x4c:
        ScrVal_StoreWordAlt((uint32*)(&t998), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t99c), 0x6f);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t99c, t998);
        break;
    case 0x4d:
        ScrVal_StoreWordAlt((uint32*)(&t9a0), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t9a4), 0x70);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t9a4, t9a0);
        break;
    case 0x4e:
        ScrVal_StoreWordAlt((uint32*)(&t9a8), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t9ac), 0x71);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t9ac, t9a8);
        break;
    case 0x4f:
        ScrVal_StoreWordAlt((uint32*)(&t9b0), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t9b4), 0x72);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t9b4, t9b0);
        break;
    case 0x50:
        ScrVal_StoreWordAlt((uint32*)(&t9b8), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t9bc), 0x73);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t9bc, t9b8);
        break;
    case 0x51:
        ScrVal_StoreWordAlt((uint32*)(&t9c0), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t9c4), 0x74);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t9c4, t9c0);
        break;
    case 0x52:
        ScrVal_StoreWordAlt((uint32*)(&t9c8), yyvsp[-1].pos);
        ScrVal_StoreWord((uint32*)(&t9cc), 0x75);
        ScrBox_Make5((void**)(&yyval.value), 0x2b, yyvsp[-2].value, yyvsp[0].value, t9cc, t9c8);
        break;
    case 0x53:
        ScrVal_StoreWordAlt((uint32*)(&t9d0), yyvsp[-3].pos);
        ScrVal_StoreWordAlt((uint32*)(&t9d4), yyvsp[-4].pos);
        ScrBox_Make5((void**)(&yyval.value), 0x37, yyvsp[-4].value, yyvsp[-1].value, t9d4, t9d0);
        break;
    case 0x54:
        ScrVal_StoreWordAlt((uint32*)(&t9d8), yyvsp[-3].pos);
        ScrVal_StoreWordAlt((uint32*)(&t9dc), yyvsp[-4].pos);
        ScrBox_Make5((void**)(&yyval.value), 0x38, yyvsp[-4].value, yyvsp[-1].value, t9dc, t9d8);
        break;
    case 0x55:
        ScrVal_StoreWordAlt((uint32*)(&t9e0), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x39, t9e0);
        break;
    case 0x56:
        ScrVal_StoreWordAlt((uint32*)(&t9e4), yyvsp[-3].pos);
        ScrVal_StoreWordAlt((uint32*)(&t9e8), yyvsp[-4].pos);
        ScrBox_Make5((void**)(&yyval.value), 0x3a, yyvsp[-4].value, yyvsp[-1].value, t9e8, t9e4);
        break;
    case 0x57:
        ScrVal_StoreWordAlt((uint32*)(&t9ec), yyvsp[-1].pos);
        ScrVal_StoreWordAlt((uint32*)(&t9f0), yyvsp[-4].pos);
        ScrBox_Make5((void**)(&yyval.value), 0x3b, yyvsp[-4].value, yyvsp[-1].value, t9f0, t9ec);
        break;
    case 0x58:
        ScrVal_StoreWordAlt((uint32*)(&t9f4), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x3f, t9f4);
        break;
    case 0x59:
        ScrVal_StoreWordAlt((uint32*)(&t9f8), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x40, t9f8);
        break;
    case 0x5a:
        ScrVal_StoreWordAlt((uint32*)(&t9fc), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x4b, t9fc);
        break;
    case 0x5b:
        ScrVal_StoreWordAlt((uint32*)(&ta00), yyvsp[-3].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x4c, yyvsp[-1].value, ta00);
        break;
    case 0x5c:
        ScrVal_StoreWordAlt((uint32*)(&ta04), yyvsp[-3].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x4d, yyvsp[-1].value, ta04);
        break;
    case 0x5d:
        ScrBox_Make2((void**)(&yyval.value), 0x19, yyvsp[0].value);
        break;
    case 0x5f:
        ScrBox_Make1((void**)(&yyval.value), 0);
        break;
    case 0x62:
        ScrVal_StoreWordAlt((uint32*)(&ta08), yyvsp[0].pos);
        ScrVal_StoreWordAlt((uint32*)(&ta0c), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x2c, yyvsp[-1].value, ta0c, ta08);
        break;
    case 0x63:
        ScrVal_StoreWordAlt((uint32*)(&ta10), yyvsp[-2].pos);
        ScrBox_Make5((void**)(&yyval.value), 0x25, yyvsp[-2].value, yyvsp[0].value, ta10, g_scr_parse_result[4]);
        break;
    case 0x64:
        ScrVal_StoreWordAlt((uint32*)(&ta14), yyvsp[-1].pos);
        ScrVal_StoreWordAlt((uint32*)(&ta18), yyvsp[-4].pos);
        ScrBox_Make8((void**)(&yyval.value), 0x26, yyvsp[-4].value, yyvsp[-2].value, yyvsp[0].value, ta18, ta14, g_scr_parse_result[4], g_scr_parse_result[4]);
        break;
    case 0x65:
        ScrVal_StoreWordAlt((uint32*)(&ta1c), yyvsp[-4].pos);
        ScrVal_StoreWordAlt((uint32*)(&ta20), yyvsp[-2].pos);
        ScrBox_Make6((void**)(&yyval.value), 0x27, yyvsp[-2].value, yyvsp[0].value, ta20, ta1c, g_scr_parse_result[4]);
        break;
    case 0x66:
        ScrVal_StoreWordAlt((uint32*)(&ta24), yyvsp[-7].pos);
        ScrVal_StoreWordAlt((uint32*)(&ta28), yyvsp[-4].pos);
        ScrBox_Make9((void**)(&yyval.value), 0x28, yyvsp[-5].value, yyvsp[-4].value, yyvsp[-2].value, yyvsp[0].value, ta28, ta24, g_scr_parse_result[4], g_scr_parse_result[4]);
        break;
    case 0x67:
        ScrVal_StoreWordAlt((uint32*)(&ta2c), yyvsp[-4].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x3c, yyvsp[-4].value, yyvsp[-1].value, ta2c);
        break;
    case 0x68:
        ScrVal_StoreWordAlt((uint32*)(&ta30), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x2d, yyvsp[-1].value, ta30, g_scr_parse_result[4]);
        break;
    case 0x69:
        ScrBox_Make1((void**)(&yyval.value), 0);
        break;
    case 0x6a:
        ScrVal_StoreWordAlt((uint32*)(&ta34), yyvsp[-2].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x3d, yyvsp[-1].value, ta34, g_scr_parse_result[4]);
        break;
    case 0x6b:
        ScrVal_StoreWordAlt((uint32*)(&ta38), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x3e, ta38, g_scr_parse_result[4]);
        break;
    case 0x6d:
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(yyvsp[-1].value), yyvsp[0].value);
        break;
    case 0x6e:
        ScrBox_Make1((void**)(&ta3c), 0);
        ScrList_Create((void**)(&yyval.value), ta3c);
        break;
    case 0x6f:
        ScrVal_StoreWordAlt((uint32*)(&ta44), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta40), yyvsp[0].value, ta44);
        ScrList_PushFront((void**)(&yyval.value), ta40, (struct scr_list_s*)(yyvsp[-2].value));
        break;
    case 0x70:
        ScrBox_Make1((void**)(&ta48), 0);
        ScrVal_StoreWordAlt((uint32*)(&ta50), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta4c), yyvsp[0].value, ta50);
        ScrList_PushFront((void**)(&yyval.value), ta4c, (struct scr_list_s*)(ta48));
        break;
    case 0x71:
        ScrBox_Make1((void**)(&yyval.value), 0);
        break;
    case 0x72:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&ta58), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta54), yyvsp[0].value, ta58);
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(yyvsp[-2].value), ta54);
        break;
    case 0x73:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&ta60), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta5c), yyvsp[0].value, ta60);
        ScrBox_Make1((void**)(&ta68), 0);
        ScrList_Create((void**)(&ta64), ta68);
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(ta64), ta5c);
        break;
    case 0x74:
        ScrBox_Make1((void**)(&ta6c), 0);
        ScrList_Create((void**)(&yyval.value), ta6c);
        break;
    case 0x75:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[0].value);
        yyvsp[0].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&ta74), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta70), yyvsp[0].value, ta74);
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(yyvsp[-2].value), ta70);
        break;
    case 0x76:
        ScrVal_StoreWordAlt((uint32*)(&ta7c), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta78), yyvsp[0].value, ta7c);
        ScrBox_Make1((void**)(&ta84), 0);
        ScrList_Create((void**)(&ta80), ta84);
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(ta80), ta78);
        break;
    case 0x77:
        ScrVal_StoreWordAlt((uint32*)(&ta8c), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta88), yyvsp[0].value, ta8c);
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(yyvsp[-2].value), ta88);
        break;
    case 0x78:
        ScrVal_StoreWordAlt((uint32*)(&ta94), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&ta90), yyvsp[0].value, ta94);
        ScrBox_Make1((void**)(&ta9c), 0);
        ScrList_Create((void**)(&ta98), ta9c);
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(ta98), ta90);
        break;
    case 0x79:
        ScrVal_StoreWordAlt((uint32*)(&taa4), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&taa0), yyvsp[0].value, taa4);
        ScrList_PushFront((void**)(&yyval.value), taa0, (struct scr_list_s*)(yyvsp[-2].value));
        break;
    case 0x7a:
        ScrBox_Make1((void**)(&taa8), 0);
        ScrVal_StoreWordAlt((uint32*)(&tab0), yyvsp[0].pos);
        ScrBox_Make2Alt((void**)(&taac), yyvsp[0].value, tab0);
        ScrList_PushFront((void**)(&yyval.value), taac, (struct scr_list_s*)(taa8));
        break;
    case 0x7b:
        iVar4 = ScrParse_ReinternIdentifierLower(yyvsp[-6].value);
        yyvsp[-6].value = iVar4;
        ScrVal_StoreWordAlt((uint32*)(&tab4), yyvsp[0].pos);
        ScrVal_StoreWordAlt((uint32*)(&tab8), yyvsp[-6].pos);
        ScrBox_Make7((void**)(&yyval.value), 0x44, yyvsp[-6].value, yyvsp[-4].value, yyvsp[-1].value, tab8, tab4, g_scr_parse_result[4]);
        break;
    case 0x7c:
        ScrVal_StoreWordAlt((uint32*)(&tabc), yyvsp[-2].pos);
        ScrVal_StoreWordAlt((uint32*)(&tac0), yyvsp[-4].pos);
        ScrBox_Make4((void**)(&yyval.value), 0x47, yyvsp[-2].value, tac0, tabc);
        break;
    case 0x7d:
        ScrVal_StoreWordAlt((uint32*)(&tac4), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x45, tac4);
        break;
    case 0x7e:
        ScrVal_StoreWordAlt((uint32*)(&tac8), yyvsp[0].pos);
        ScrBox_Make2((void**)(&yyval.value), 0x46, tac8);
        break;
    case 0x7f:
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(yyvsp[-1].value), yyvsp[0].value);
        break;
    case 0x80:
        ScrBox_Make1((void**)(&tacc), 0);
        ScrList_Create((void**)(&yyval.value), tacc);
        break;
    case 0x81:
        ScrVal_StoreWordAlt((uint32*)(&tad0), yyvsp[-1].pos);
        ScrBox_Make3((void**)(&yyval.value), 0x55, yyvsp[0].value, tad0);
        g_scr_dependency_record_count = g_scr_dependency_record_count + 1;
        break;
    case 0x82:
        ScrList_Append((void**)(&yyval.value), (struct scr_list_s*)(yyvsp[-2].value), yyvsp[-1].value);
        break;
    case 0x83:
        ScrBox_Make1((void**)(&tad4), 0);
        ScrList_Create((void**)(&yyval.value), tad4);
        break;
    }
    yyssp = yyssp - yylen;
    yyvsp = yyvsp - yylen + 1;
    *yyvsp = yyval;
    yyn = yyr1[yyn];
    {
        int yyidx = (int)yypgoto[yyn] + (int)*yyssp;
        if (yyidx < 0 || yyidx > 0x522 || yycheck[yyidx] != *yyssp)
            yystate = yydefgoto[yyn];
        else
            yystate = yytable[yyidx];
    }
    yyssp = yyssp + 1;
    goto yynewstate;

yyerrlab:
    if (yyerrstatus == 0) {
        yynerrs = yynerrs + 1;
        yyerror("syntax error");
    }
    if (yyerrstatus == 3) {
        if (yychar < 1) {
            if (yychar == 0) {
                for (;;) {
                    yyvsp = yyvsp - 1;
                    yyssp = yyssp - 1;
                    if (yyssp == yyss)
                        break;
                    yydestruct("Error: popping", yystos[*yyssp], yyvsp);
                }
                goto yyabortlab;
            }
        } else {
            yydestruct("Error: discarding", (int)yytoken, &yylval);
            yychar = -2;
        }
    }
    yyerrstatus = 3;
    for (;;) {
        yyn = yypact[yystate];
        if (yyn != -105) {
            yyn = yyn + 1;
            if (yyn >= 0 && yyn <= 0x522 && yycheck[yyn] == 1) {
                yyn = yytable[yyn];
                if (yyn > 0)
                    break;
            }
        }
        if (yyssp == yyss)
            goto yyabortlab;
        yydestruct("Error: popping", yystos[yystate], yyvsp);
        yyvsp = yyvsp - 1;
        yyssp = yyssp - 1;
        yystate = *yyssp;
    }
    if (yyn == 0x36)
        return 0;
    yyvsp[1] = yylval;
    yyvsp = yyvsp + 1;
    yystate = yyn;
    yyssp = yyssp + 1;
    goto yynewstate;

yyabortlab:
    yydestruct("Error: discarding lookahead", (int)yytoken, &yylval);
    yychar = -2;
    return 1;

yyoverflow:
    yyerror("parser stack overflow");
    return 2;
}
