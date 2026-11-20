/*
 * chunk_019: flex scanner for the script/config parser, server kick and map commands.
 * Recovered from cod2_lnxded 1.3 (SHA-256 05e774a3...a3a7).
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_019_types.h"
#include <errno.h>
#include <unistd.h>
#include <strings.h>

#define YY_CURRENT_BUFFER (g_yy_buffer_stack ? g_yy_buffer_stack[g_yy_buffer_stack_top] : (struct yy_buffer_state *)0)
#define YY_CUR_BUF_UNCHECKED (g_yy_buffer_stack[g_yy_buffer_stack_top])
#define LEX_ADVANCE() do { g_lex_token_pos = g_lex_column; g_lex_error_pos = g_lex_column; g_lex_column += g_yyleng; } while (0)


// 0x080892d8  size=48  FUN_080892d8  status=translated  naming=unknown
// Stores the matched identifier text via ScrStr_Intern(kind 0xd) into the lexer value slot; name is descriptive.
void Lex_StoreIdentifier(const char *text, int length)
{
    g_yylval.i = ScrStr_Intern(text, 0, length + 1, 0xd);
}

// 0x08089308  size=317  FUN_08089308  status=translated  naming=observed
// String "max string length exceeded: \"%s\"" is printed here; backslash escapes n/r/t decoded, other escapes pass the char.
int Lex_StoreEscapedString(const char *text, int length)
{
    char buffer[0x2000 + 4];
    char *out;

    if (length >= 0x2000) {
        ScrCompile_ErrorInFile(g_lex_error_pos, "max string length exceeded: \"%s\"", text);
        return 0;
    }
    out = buffer;
    while (length != 0) {
        if (*text == '\\') {
            char c;
            if (length == 1)
                break;
            c = text[1];
            if (c == 'r')
                *out = '\r';
            else if (c == 'n')
                *out = '\n';
            else if (c == 't')
                *out = '\t';
            else
                *out = c;
            out++;
            length -= 2;
            text += 2;
        } else {
            length--;
            *out = *text;
            out++;
            text++;
        }
    }
    *out = '\0';
    g_yylval.i = ScrStr_InternCStringEx(buffer, g_lex_param, 0xd);
    return 1;
}

// 0x08089445  size=35  FUN_08089445  status=translated  naming=inferred
// sscanf "%d" into the lexer value (integer literal rule).
void Lex_ParseInt(const char *text)
{
    sscanf(text, "%d", &g_yylval.i);
}

// 0x08089468  size=35  FUN_08089468  status=translated  naming=inferred
// sscanf "%f" into the lexer value (float literal rule).
void Lex_ParseFloat(const char *text)
{
    sscanf(text, "%f", &g_yylval.f);
}

// 0x0808948b  size=5274  FUN_0808948b  status=translated  naming=inferred
// flex-generated yylex(): strings 'fatal flex scanner internal error--no action found', DFA tables yy_accept/ec/meta/base/def/nxt/chk.
// Token numbers are the parser's numeric token codes (names not recovered).
int yylex(void)
{
    int yy_current_state;
    int yy_act;
    int token;
    int tok_start;
    int rc;
    int yy_amount_of_matched_text;
    int next_state;
    byte *yy_cp;
    byte *yy_bp;
    byte yy_c;

    if (g_yy_init != 0) {
        g_yy_init = 0;
        if (g_yy_start == 0)
            g_yy_start = 1;
        if (g_yyin == 0)
            g_yyin = stdin;
        if (g_yyout == 0)
            g_yyout = stdout;
        if (g_yy_buffer_stack == 0 || g_yy_buffer_stack[g_yy_buffer_stack_top] == 0) {
            yyensure_buffer_stack();
            g_yy_buffer_stack[g_yy_buffer_stack_top] = yy_create_buffer(g_yyin, 0x4000);
        }
        yy_load_buffer_state();
    }

new_scan:
    yy_cp = g_yy_c_buf_p;
    *g_yy_c_buf_p = g_yy_hold_char;
    yy_bp = g_yy_c_buf_p;
    yy_current_state = g_yy_start;
match:
    do {
        yy_c = (byte)((unsigned int*)&g_yy_ec)[*yy_cp];
        if (((short*)&g_yy_accept)[yy_current_state] != 0) {
            g_yy_last_accepting_state = yy_current_state;
            g_yy_last_accepting_cpos = yy_cp;
        }
        while (((short*)&g_yy_chk)[((short*)&g_yy_base)[yy_current_state] + yy_c] != yy_current_state) {
            yy_current_state = ((short*)&g_yy_def)[yy_current_state];
            if (yy_current_state > 0xff)
                yy_c = (byte)((unsigned int*)&g_yy_meta)[yy_c];
        }
        yy_current_state = ((short*)&g_yy_nxt)[((short*)&g_yy_base)[yy_current_state] + yy_c];
        yy_cp++;
    } while (((short*)&g_yy_base)[yy_current_state] != 0x1af);
find_action:
    yy_act = ((short*)&g_yy_accept)[yy_current_state];
    if (yy_act == 0) {
        yy_cp = g_yy_last_accepting_cpos;
        yy_act = ((short*)&g_yy_accept)[g_yy_last_accepting_state];
    }
    g_yytext = yy_bp;
    g_yyleng = (int)(yy_cp - yy_bp);
    g_yy_hold_char = *yy_cp;
    *yy_cp = 0;
    g_yy_c_buf_p = yy_cp;
do_action:
    tok_start = g_lex_column;
    switch (yy_act) {
    case 0:
        *yy_cp = g_yy_hold_char;
        yy_cp = g_yy_last_accepting_cpos;
        yy_current_state = g_yy_last_accepting_state;
        goto find_action;
    case 1:
        LEX_ADVANCE();
        goto new_scan;
    case 2:
        LEX_ADVANCE();
        g_yy_start = 3;
        goto new_scan;
    case 3:
    case 4:
    case 5:
        LEX_ADVANCE();
        goto new_scan;
    case 6:
        LEX_ADVANCE();
        g_yy_start = 5;
        goto new_scan;
    case 7:
        LEX_ADVANCE();
        rc = Lex_StoreEscapedString((char *)g_yytext + 1, g_yyleng - 2);
        token = (rc == 0) ? 0x102 : 0x104;
        break;
    case 8:
        LEX_ADVANCE();
        rc = Lex_StoreEscapedString((char *)g_yytext + 2, g_yyleng - 3);
        token = (rc == 0) ? 0x102 : 0x105;
        break;
    case 0x9:
        LEX_ADVANCE();
        token = 0x106;
        break;
    case 0xa:
        LEX_ADVANCE();
        token = 0x107;
        break;
    case 0xb:
        LEX_ADVANCE();
        token = 0x108;
        break;
    case 0xc:
        LEX_ADVANCE();
        token = 0x109;
        break;
    case 0xd:
        LEX_ADVANCE();
        token = 0x10a;
        break;
    case 0xe:
        LEX_ADVANCE();
        token = 0x10b;
        break;
    case 0xf:
        LEX_ADVANCE();
        token = 0x10c;
        break;
    case 0x10:
        LEX_ADVANCE();
        token = 0x10d;
        break;
    case 0x11:
        LEX_ADVANCE();
        token = 0x10e;
        break;
    case 0x12:
        LEX_ADVANCE();
        token = 0x10f;
        break;
    case 0x13:
        LEX_ADVANCE();
        token = 0x110;
        break;
    case 0x14:
        LEX_ADVANCE();
        token = 0x111;
        break;
    case 0x15:
        LEX_ADVANCE();
        token = 0x112;
        break;
    case 0x16:
        LEX_ADVANCE();
        token = 0x113;
        break;
    case 0x17:
        LEX_ADVANCE();
        token = 0x114;
        break;
    case 0x18:
        LEX_ADVANCE();
        token = 0x115;
        break;
    case 0x19:
        LEX_ADVANCE();
        token = 0x116;
        break;
    case 0x1a:
        LEX_ADVANCE();
        token = 0x117;
        break;
    case 0x1b:
        LEX_ADVANCE();
        token = 0x118;
        break;
    case 0x1c:
        LEX_ADVANCE();
        token = 0x119;
        break;
    case 0x1d:
        LEX_ADVANCE();
        token = 0x11a;
        break;
    case 0x1e:
        LEX_ADVANCE();
        token = 0x11b;
        break;
    case 0x1f:
        LEX_ADVANCE();
        token = 0x11c;
        break;
    case 0x20:
        LEX_ADVANCE();
        token = 0x11d;
        break;
    case 0x21:
        LEX_ADVANCE();
        token = 0x11e;
        break;
    case 0x22:
        LEX_ADVANCE();
        token = 0x11f;
        break;
    case 0x23:
        LEX_ADVANCE();
        Lex_ParseInt((char *)g_yytext);
        token = 0x120;
        break;
    case 0x24:
        LEX_ADVANCE();
        Lex_ParseFloat((char *)g_yytext);
        token = 0x121;
        break;
    case 0x25:
        LEX_ADVANCE();
        token = 0x123;
        break;
    case 0x26:
        LEX_ADVANCE();
        token = 0x122;
        break;
    case 0x27:
        LEX_ADVANCE();
        token = 0x127;
        break;
    case 0x28:
        LEX_ADVANCE();
        token = 0x124;
        break;
    case 0x29:
        LEX_ADVANCE();
        token = 0x126;
        break;
    case 0x2a:
        LEX_ADVANCE();
        token = 0x125;
        break;
    case 0x2b:
        LEX_ADVANCE();
        token = 0x128;
        break;
    case 0x2c:
        LEX_ADVANCE();
        token = 0x129;
        break;
    case 0x2d:
        LEX_ADVANCE();
        token = 0x12a;
        break;
    case 0x2e:
        LEX_ADVANCE();
        token = 0x12b;
        break;
    case 0x2f:
        LEX_ADVANCE();
        token = 0x12c;
        break;
    case 0x30:
        LEX_ADVANCE();
        token = 0x12d;
        break;
    case 0x31:
        LEX_ADVANCE();
        token = 0x12e;
        break;
    case 0x32:
        LEX_ADVANCE();
        token = 0x12f;
        break;
    case 0x33:
        LEX_ADVANCE();
        token = 0x130;
        break;
    case 0x34:
        LEX_ADVANCE();
        token = 0x131;
        break;
    case 0x35:
        LEX_ADVANCE();
        token = 0x132;
        break;
    case 0x36:
        LEX_ADVANCE();
        token = 0x133;
        break;
    case 0x37:
        LEX_ADVANCE();
        token = 0x134;
        break;
    case 0x38:
        LEX_ADVANCE();
        token = 0x135;
        break;
    case 0x39:
        LEX_ADVANCE();
        token = 0x136;
        break;
    case 0x3a:
        LEX_ADVANCE();
        token = 0x137;
        break;
    case 0x3b:
        LEX_ADVANCE();
        token = 0x138;
        break;
    case 0x3c:
        LEX_ADVANCE();
        token = 0x139;
        break;
    case 0x3d:
        LEX_ADVANCE();
        token = 0x13a;
        break;
    case 0x3e:
        LEX_ADVANCE();
        token = 0x13b;
        break;
    case 0x3f:
        LEX_ADVANCE();
        token = 0x13c;
        break;
    case 0x40:
        LEX_ADVANCE();
        token = 0x13d;
        break;
    case 0x41:
        LEX_ADVANCE();
        token = 0x13e;
        break;
    case 0x42:
        LEX_ADVANCE();
        token = 0x13f;
        break;
    case 0x43:
        LEX_ADVANCE();
        token = 0x140;
        break;
    case 0x44:
        LEX_ADVANCE();
        token = 0x141;
        break;
    case 0x45:
        LEX_ADVANCE();
        token = 0x142;
        break;
    case 0x46:
        LEX_ADVANCE();
        token = 0x143;
        break;
    case 0x47:
        LEX_ADVANCE();
        token = 0x144;
        break;
    case 0x48:
        LEX_ADVANCE();
        token = 0x146;
        break;
    case 0x49:
        LEX_ADVANCE();
        token = 0x147;
        break;
    case 0x4a:
        LEX_ADVANCE();
        token = 0x148;
        break;
    case 0x4b:
        LEX_ADVANCE();
        token = 0x149;
        break;
    case 0x4c:
        LEX_ADVANCE();
        token = 0x14a;
        break;
    case 0x4d:
        LEX_ADVANCE();
        token = 0x14b;
        break;
    case 0x4e:
        LEX_ADVANCE();
        token = 0x14c;
        break;
    case 0x4f:
        LEX_ADVANCE();
        token = 0x14d;
        break;
    case 0x50:
        LEX_ADVANCE();
        token = 0x14e;
        break;
    case 0x51:
        LEX_ADVANCE();
        token = 0x14f;
        break;
    case 0x52:
        LEX_ADVANCE();
        token = 0x150;
        break;
    case 0x53:
        LEX_ADVANCE();
        token = 0x151;
        break;
    case 0x54:
        LEX_ADVANCE();
        token = 0x152;
        break;
    case 0x55:
        LEX_ADVANCE();
        token = 0x153;
        break;
    case 0x56:
        LEX_ADVANCE();
        token = 0x154;
        break;
    case 0x57:
        LEX_ADVANCE();
        token = 0x155;
        break;
    case 0x58:
        LEX_ADVANCE();
        token = 0x156;
        break;
    case 0x59:
        LEX_ADVANCE();
        token = 0x157;
        break;
    case 0x5a:
        LEX_ADVANCE();
        Lex_StoreIdentifier((char *)g_yytext, g_yyleng);
        token = 0x103;
        break;
    case 0x5b:
        LEX_ADVANCE();
        Lex_StoreIdentifier((char *)g_yytext, g_yyleng);
        token = 0x145;
        break;
    case 0x5c:
        LEX_ADVANCE();
        ScrCompile_ErrorInFile(tok_start, "bad token '%s'", g_yytext);
        token = 0x102;
        break;
    case 0x5d:
        LEX_ADVANCE();
        fwrite(g_yytext, g_yyleng, 1, g_yyout);
        goto new_scan;
    case 0x5e: /* YY_END_OF_BUFFER */
        yy_amount_of_matched_text = (int)(yy_cp - g_yytext) - 1;
        *yy_cp = g_yy_hold_char;
        if (YY_CUR_BUF_UNCHECKED->buffer_status == 0) {
            g_yy_n_chars = YY_CUR_BUF_UNCHECKED->n_chars;
            YY_CUR_BUF_UNCHECKED->input_file = g_yyin;
            YY_CUR_BUF_UNCHECKED->buffer_status = 1;
        }
        if (g_yy_c_buf_p <= YY_CUR_BUF_UNCHECKED->ch_buf + g_yy_n_chars) {
            /* a NUL inside the buffer, not the end-of-buffer sentinel */
            g_yy_c_buf_p = g_yytext + yy_amount_of_matched_text;
            yy_current_state = yy_get_previous_state();
            next_state = yy_try_NUL_trans(yy_current_state);
            yy_bp = g_yytext;
            if (next_state == 0) {
                yy_cp = g_yy_c_buf_p;
                goto find_action;
            }
            g_yy_c_buf_p++;
            yy_cp = g_yy_c_buf_p;
            yy_current_state = next_state;
            goto match;
        }
        rc = yy_get_next_buffer();
        if (rc == 1) {
            g_yy_did_buffer_switch_on_eof = 0;
            if (yywrap() != 0) {
                g_yy_c_buf_p = g_yytext;
                yy_act = (g_yy_start - 1) / 2 + 0x5f;
                goto do_action;
            }
            if (g_yy_did_buffer_switch_on_eof == 0)
                yyrestart(g_yyin);
            goto new_scan;
        }
        if (rc < 2) {
            if (rc != 0)
                goto new_scan;
            g_yy_c_buf_p = g_yytext + yy_amount_of_matched_text;
            yy_current_state = yy_get_previous_state();
            yy_cp = g_yy_c_buf_p;
            yy_bp = g_yytext;
            goto match;
        }
        if (rc != 2)
            goto new_scan;
        g_yy_c_buf_p = YY_CUR_BUF_UNCHECKED->ch_buf + g_yy_n_chars;
        yy_current_state = yy_get_previous_state();
        yy_cp = g_yy_c_buf_p;
        yy_bp = g_yytext;
        goto find_action;
    case 0x5f:
    case 0x60:
    case 0x61:
        token = 0;
        break;
    default:
        yy_fatal_error("fatal flex scanner internal error--no action found");
        goto new_scan;
    }
    return token;
}

// 0x0808a925  size=855  FUN_0808a925  status=translated  naming=inferred
// flex yy_get_next_buffer(); strings 'fatal flex scanner internal error--end of buffer missed' and 'fatal error - scanner input buffer overflow'.
// Returns 0 continue scan, 1 end of file, 2 last match. The refill loop only iterates while the free-space count is exactly zero (as compiled).
int yy_get_next_buffer(void)
{
    byte *dest = YY_CUR_BUF_UNCHECKED->ch_buf;
    byte *source = g_yytext;
    int number_to_move;
    int i;
    unsigned int num_to_read;
    int ret_val;
    int size_after_move;
    struct yy_buffer_state *b;

    if (YY_CUR_BUF_UNCHECKED->ch_buf + g_yy_n_chars + 1 < g_yy_c_buf_p)
        yy_fatal_error("fatal flex scanner internal error--end of buffer missed");

    if (YY_CUR_BUF_UNCHECKED->fill_buffer == 0) {
        if (g_yy_c_buf_p - g_yytext == 1)
            return 1;
        return 2;
    }

    number_to_move = (int)(g_yy_c_buf_p - g_yytext) - 1;
    for (i = 0; i < number_to_move; i++) {
        *dest = *source;
        dest++;
        source++;
    }

    if (YY_CUR_BUF_UNCHECKED->buffer_status == 2) {
        g_yy_n_chars = 0;
        YY_CUR_BUF_UNCHECKED->n_chars = 0;
    } else {
        size_after_move = YY_CUR_BUF_UNCHECKED->buf_size - number_to_move;
        while ((num_to_read = (unsigned int)(size_after_move - 1)) == 0) {
            int yy_c_buf_p_offset;
            b = YY_CURRENT_BUFFER;
            yy_c_buf_p_offset = (int)(g_yy_c_buf_p - b->ch_buf);
            if (b->is_our_buffer == 0) {
                b->ch_buf = 0;
            } else {
                if (b->buf_size * 2 < 1)
                    b->buf_size = (int)((unsigned int)b->buf_size >> 3) + b->buf_size;
                else
                    b->buf_size = b->buf_size * 2;
                b->ch_buf = (byte *)yyrealloc(b->ch_buf, b->buf_size + 2);
            }
            if (b->ch_buf == 0)
                yy_fatal_error("fatal error - scanner input buffer overflow");
            g_yy_c_buf_p = b->ch_buf + yy_c_buf_p_offset;
            size_after_move = YY_CUR_BUF_UNCHECKED->buf_size - number_to_move;
        }
        if (num_to_read > 0x2000)
            num_to_read = 0x2000;
        g_yy_n_chars = Scr_ReadSourceLine((char *)(YY_CUR_BUF_UNCHECKED->ch_buf + number_to_move), num_to_read);
        YY_CUR_BUF_UNCHECKED->n_chars = g_yy_n_chars;
    }

    if (g_yy_n_chars == 0) {
        if (number_to_move == 0) {
            ret_val = 1;
            yyrestart(g_yyin);
        } else {
            ret_val = 2;
            YY_CUR_BUF_UNCHECKED->buffer_status = 2;
        }
    } else {
        ret_val = 0;
    }
    g_yy_n_chars += number_to_move;
    YY_CUR_BUF_UNCHECKED->ch_buf[g_yy_n_chars] = 0;
    YY_CUR_BUF_UNCHECKED->ch_buf[g_yy_n_chars + 1] = 0;
    g_yytext = YY_CUR_BUF_UNCHECKED->ch_buf;
    return ret_val;
}

// 0x0808ac7c  size=216  FUN_0808ac7c  status=translated  naming=inferred
// flex yy_get_previous_state(): re-runs the DFA over the already matched text (NUL byte maps to class 1).
int yy_get_previous_state(void)
{
    byte yy_c;
    byte *yy_cp;
    int yy_current_state = g_yy_start;

    for (yy_cp = g_yytext; yy_cp < g_yy_c_buf_p; yy_cp++) {
        if (*yy_cp == 0)
            yy_c = 1;
        else
            yy_c = (byte)((unsigned int*)&g_yy_ec)[*yy_cp];
        if (((short*)&g_yy_accept)[yy_current_state] != 0) {
            g_yy_last_accepting_state = yy_current_state;
            g_yy_last_accepting_cpos = yy_cp;
        }
        while (((short*)&g_yy_chk)[((short*)&g_yy_base)[yy_current_state] + yy_c] != yy_current_state) {
            yy_current_state = ((short*)&g_yy_def)[yy_current_state];
            if (yy_current_state > 0xff)
                yy_c = (byte)((unsigned int*)&g_yy_meta)[yy_c];
        }
        yy_current_state = ((short*)&g_yy_nxt)[((short*)&g_yy_base)[yy_current_state] + yy_c];
    }
    return yy_current_state;
}

// 0x0808ad54  size=188  FUN_0808ad54  status=translated  naming=inferred
// flex yy_try_NUL_trans(): one DFA step on class 1; returns 0 when the next state is 0xff (jam).
int yy_try_NUL_trans(int current_state)
{
    int next_state;
    byte yy_c = 1;

    if (((short*)&g_yy_accept)[current_state] != 0) {
        g_yy_last_accepting_state = current_state;
        g_yy_last_accepting_cpos = g_yy_c_buf_p;
    }
    while (((short*)&g_yy_chk)[((short*)&g_yy_base)[current_state] + yy_c] != current_state) {
        current_state = ((short*)&g_yy_def)[current_state];
        if (current_state > 0xff)
            yy_c = (byte)((unsigned int*)&g_yy_meta)[yy_c];
    }
    next_state = ((short*)&g_yy_nxt)[((short*)&g_yy_base)[current_state] + yy_c];
    if (next_state == 0xff)
        next_state = 0;
    return next_state;
}

// 0x0808ae10  size=360  FUN_0808ae10  status=translated  naming=inferred
// flex yyunput(c, yy_bp); string 'flex scanner push-back overflow'.
void yyunput(byte c, byte *yy_bp)
{
    byte *yy_cp = g_yy_c_buf_p;

    *g_yy_c_buf_p = g_yy_hold_char;
    if (yy_cp < YY_CUR_BUF_UNCHECKED->ch_buf + 2) {
        byte *dest = YY_CUR_BUF_UNCHECKED->ch_buf + YY_CUR_BUF_UNCHECKED->buf_size + 2;
        byte *source = YY_CUR_BUF_UNCHECKED->ch_buf + g_yy_n_chars + 2;
        while (YY_CUR_BUF_UNCHECKED->ch_buf < source) {
            dest--;
            source--;
            *dest = *source;
        }
        yy_cp += dest - source;
        yy_bp += dest - source;
        g_yy_n_chars = YY_CUR_BUF_UNCHECKED->buf_size;
        YY_CUR_BUF_UNCHECKED->n_chars = g_yy_n_chars;
        if (yy_cp < YY_CUR_BUF_UNCHECKED->ch_buf + 2)
            yy_fatal_error("flex scanner push-back overflow");
    }
    g_yy_c_buf_p = yy_cp - 1;
    *g_yy_c_buf_p = c;
    g_yytext = yy_bp;
    g_yy_hold_char = *g_yy_c_buf_p;
}

// 0x0808af78  size=269  FUN_0808af78  status=translated  naming=inferred
// flex input(): returns the next byte or -1 at end of input.
int yyinput(void)
{
    byte c;
    int rc;
    byte *saved_text;
    byte *saved_c_buf_p;

    *g_yy_c_buf_p = g_yy_hold_char;
    saved_text = g_yytext;
    saved_c_buf_p = g_yy_c_buf_p;
    if (*g_yy_c_buf_p == 0) {
        if (g_yy_c_buf_p < YY_CUR_BUF_UNCHECKED->ch_buf + g_yy_n_chars) {
            *g_yy_c_buf_p = 0;
        } else {
            g_yy_c_buf_p++;
            rc = yy_get_next_buffer();
            if (rc == 1) {
            at_eof:
                if (yywrap() != 0)
                    return -1;
                if (g_yy_did_buffer_switch_on_eof == 0)
                    yyrestart(g_yyin);
                return yyinput();
            }
            if (rc < 2) {
                if (rc == 0)
                    g_yy_c_buf_p = saved_c_buf_p + (g_yytext - saved_text);
            } else if (rc == 2) {
                yyrestart(g_yyin);
                goto at_eof;
            }
        }
    }
    c = *g_yy_c_buf_p;
    *g_yy_c_buf_p = 0;
    g_yy_c_buf_p++;
    g_yy_hold_char = *g_yy_c_buf_p;
    return c;
}

// 0x0808b085  size=152  FUN_0808b085  status=translated  naming=inferred
// flex yyrestart(input_file).
void yyrestart(FILE *input_file)
{
    if (g_yy_buffer_stack == 0 || g_yy_buffer_stack[g_yy_buffer_stack_top] == 0) {
        yyensure_buffer_stack();
        g_yy_buffer_stack[g_yy_buffer_stack_top] = yy_create_buffer(g_yyin, 0x4000);
    }
    yy_init_buffer(YY_CURRENT_BUFFER, input_file);
    yy_load_buffer_state();
}

// 0x0808b11d  size=206  FUN_0808b11d  status=translated  naming=inferred
// flex yy_switch_to_buffer(new_buffer).
void yy_switch_to_buffer(struct yy_buffer_state *new_buffer)
{
    yyensure_buffer_stack();
    if (YY_CURRENT_BUFFER == new_buffer)
        return;
    if (YY_CURRENT_BUFFER != 0) {
        *g_yy_c_buf_p = g_yy_hold_char;
        YY_CUR_BUF_UNCHECKED->buf_pos = g_yy_c_buf_p;
        YY_CUR_BUF_UNCHECKED->n_chars = g_yy_n_chars;
    }
    g_yy_buffer_stack[g_yy_buffer_stack_top] = new_buffer;
    yy_load_buffer_state();
    g_yy_did_buffer_switch_on_eof = 1;
}

// 0x0808b1eb  size=106  FUN_0808b1eb  status=translated  naming=inferred
// flex yy_load_buffer_state().
void yy_load_buffer_state(void)
{
    g_yy_n_chars = YY_CUR_BUF_UNCHECKED->n_chars;
    g_yy_c_buf_p = YY_CUR_BUF_UNCHECKED->buf_pos;
    g_yytext = g_yy_c_buf_p;
    g_yyin = YY_CUR_BUF_UNCHECKED->input_file;
    g_yy_hold_char = *g_yy_c_buf_p;
}

// 0x0808b255  size=130  FUN_0808b255  status=translated  naming=observed
// String 'out of dynamic memory in yy_create_buffer()'.
struct yy_buffer_state *yy_create_buffer(FILE *file, int size)
{
    struct yy_buffer_state *b = (struct yy_buffer_state *)yyalloc(0x30);
    if (b == 0)
        yy_fatal_error("out of dynamic memory in yy_create_buffer()");
    b->buf_size = size;
    b->ch_buf = (byte *)yyalloc(b->buf_size + 2);
    if (b->ch_buf == 0)
        yy_fatal_error("out of dynamic memory in yy_create_buffer()");
    b->is_our_buffer = 1;
    yy_init_buffer(b, file);
    return b;
}

// 0x0808b2d7  size=117  FUN_0808b2d7  status=translated  naming=inferred
// flex yy_delete_buffer(b).
void yy_delete_buffer(struct yy_buffer_state *b)
{
    if (b == 0)
        return;
    if (b == YY_CURRENT_BUFFER)
        g_yy_buffer_stack[g_yy_buffer_stack_top] = 0;
    if (b->is_our_buffer != 0)
        yyfree(b->ch_buf);
    yyfree(b);
}

// 0x0808b34c  size=182  FUN_0808b34c  status=translated  naming=inferred
// flex yy_init_buffer(b, file); preserves errno across isatty().
void yy_init_buffer(struct yy_buffer_state *b, FILE *file)
{
    int saved_errno = errno;

    yy_flush_buffer(b);
    b->input_file = file;
    b->fill_buffer = 1;
    if (b != YY_CURRENT_BUFFER) {
        b->bs_lineno = 1;
        b->bs_column = 0;
    }
    b->is_interactive = 0;
    if (file != 0) {
        if (isatty(fileno(file)) > 0)
            b->is_interactive = 1;
    }
    errno = saved_errno;
}

// 0x0808b402  size=125  FUN_0808b402  status=translated  naming=inferred
// flex yy_flush_buffer(b).
void yy_flush_buffer(struct yy_buffer_state *b)
{
    if (b == 0)
        return;
    b->n_chars = 0;
    b->ch_buf[0] = 0;
    b->ch_buf[1] = 0;
    b->buf_pos = b->ch_buf;
    b->at_bol = 1;
    b->buffer_status = 0;
    if (b == YY_CURRENT_BUFFER)
        yy_load_buffer_state();
}

// 0x0808b47f  size=204  FUN_0808b47f  status=translated  naming=inferred
// flex yypush_buffer_state(new_buffer).
void yypush_buffer_state(struct yy_buffer_state *new_buffer)
{
    if (new_buffer == 0)
        return;
    yyensure_buffer_stack();
    if (YY_CURRENT_BUFFER != 0) {
        *g_yy_c_buf_p = g_yy_hold_char;
        YY_CUR_BUF_UNCHECKED->buf_pos = g_yy_c_buf_p;
        YY_CUR_BUF_UNCHECKED->n_chars = g_yy_n_chars;
    }
    if (YY_CURRENT_BUFFER != 0)
        g_yy_buffer_stack_top++;
    g_yy_buffer_stack[g_yy_buffer_stack_top] = new_buffer;
    yy_load_buffer_state();
    g_yy_did_buffer_switch_on_eof = 1;
}

// 0x0808b54b  size=187  FUN_0808b54b  status=translated  naming=inferred
// flex yypop_buffer_state().
void yypop_buffer_state(void)
{
    if (YY_CURRENT_BUFFER == 0)
        return;
    yy_delete_buffer(YY_CURRENT_BUFFER);
    g_yy_buffer_stack[g_yy_buffer_stack_top] = 0;
    if (g_yy_buffer_stack_top != 0)
        g_yy_buffer_stack_top--;
    if (YY_CURRENT_BUFFER != 0) {
        yy_load_buffer_state();
        g_yy_did_buffer_switch_on_eof = 1;
    }
}

// 0x0808b606  size=203  FUN_0808b606  status=translated  naming=inferred
// flex yyensure_buffer_stack(): allocates 1 slot, then grows by 8 slots when the top reaches max-1.
void yyensure_buffer_stack(void)
{
    if (g_yy_buffer_stack == 0) {
        g_yy_buffer_stack = (struct yy_buffer_state **)yyalloc(4);
        memset(g_yy_buffer_stack, 0, 4);
        g_yy_buffer_stack_max = 1;
        g_yy_buffer_stack_top = 0;
    } else if (g_yy_buffer_stack_max - 1U <= g_yy_buffer_stack_top) {
        int num_to_alloc = (int)g_yy_buffer_stack_max + 8;
        g_yy_buffer_stack = (struct yy_buffer_state **)yyrealloc(g_yy_buffer_stack, num_to_alloc * 4);
        memset((byte *)g_yy_buffer_stack + g_yy_buffer_stack_max * 4, 0, 0x20);
        g_yy_buffer_stack_max = num_to_alloc;
    }
}

// 0x0808b6d1  size=205  FUN_0808b6d1  status=translated  naming=observed
// String 'out of dynamic memory in yy_scan_buffer()'. Requires two trailing NUL bytes.
struct yy_buffer_state *yy_scan_buffer(byte *base, unsigned int size)
{
    struct yy_buffer_state *b;

    if (size < 2 || base[size - 2] != 0 || base[size - 1] != 0)
        return 0;
    b = (struct yy_buffer_state *)yyalloc(0x30);
    if (b == 0)
        yy_fatal_error("out of dynamic memory in yy_scan_buffer()");
    b->buf_size = (int)(size - 2);
    b->ch_buf = base;
    b->buf_pos = base;
    b->is_our_buffer = 0;
    b->input_file = 0;
    b->n_chars = b->buf_size;
    b->is_interactive = 0;
    b->at_bol = 1;
    b->fill_buffer = 0;
    b->buffer_status = 0;
    yy_switch_to_buffer(b);
    return b;
}

// 0x0808b79e  size=34  FUN_0808b79e  status=translated  naming=inferred
// flex yy_scan_string(str) -> yy_scan_bytes(str, strlen(str)); return value discarded.
void yy_scan_string(const char *str)
{
    size_t len = strlen(str);
    yy_scan_bytes(str, (int)len);
}

// 0x0808b7c0  size=165  FUN_0808b7c0  status=translated  naming=observed
// Strings 'out of dynamic memory in yy_scan_bytes()' and 'bad buffer in yy_scan_bytes()'.
struct yy_buffer_state *yy_scan_bytes(const char *bytes, int len)
{
    byte *buf;
    int i;
    struct yy_buffer_state *b;

    buf = (byte *)yyalloc(len + 2);
    if (buf == 0)
        yy_fatal_error("out of dynamic memory in yy_scan_bytes()");
    for (i = 0; i < len; i++)
        buf[i] = bytes[i];
    buf[len + 1] = 0;
    buf[len] = 0;
    b = yy_scan_buffer(buf, len + 2);
    if (b == 0)
        yy_fatal_error("bad buffer in yy_scan_bytes()");
    b->is_our_buffer = 1;
    return b;
}

// 0x0808b865  size=46  FUN_0808b865  status=translated  naming=inferred
// flex yy_fatal_error(msg): fprintf(stderr, "%s\n", msg); exit(2).
void yy_fatal_error(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(2);
}

// 0x0808b89d  size=10  FUN_0808b89d  status=translated  naming=inferred
// flex yyget_in().
FILE *yyget_in(void)
{
    return g_yyin;
}

// 0x0808b8a7  size=10  FUN_0808b8a7  status=translated  naming=inferred
// flex yyget_out().
FILE *yyget_out(void)
{
    return g_yyout;
}

// 0x0808b8b1  size=10  FUN_0808b8b1  status=translated  naming=inferred
// flex yyget_leng().
int yyget_leng(void)
{
    return g_yyleng;
}

// 0x0808b8bb  size=10  FUN_0808b8bb  status=translated  naming=inferred
// flex yyget_text().
byte *yyget_text(void)
{
    return g_yytext;
}

// 0x0808b8c5  size=13  FUN_0808b8c5  status=translated  naming=inferred
// flex yyset_lineno(n): stores to the global at 0x0817d9c4.
void yyset_lineno(int line_number)
{
    g_yylineno = line_number;
}

// 0x0808b8d2  size=13  FUN_0808b8d2  status=translated  naming=inferred
// flex yyset_in().
void yyset_in(FILE *in_str)
{
    g_yyin = in_str;
}

// 0x0808b8df  size=13  FUN_0808b8df  status=translated  naming=inferred
// flex yyset_out().
void yyset_out(FILE *out_str)
{
    g_yyout = out_str;
}

// 0x0808b8ec  size=10  FUN_0808b8ec  status=translated  naming=inferred
// flex yyget_debug().
int yyget_debug(void)
{
    return g_yy_flex_debug;
}

// 0x0808b8f6  size=13  FUN_0808b8f6  status=translated  naming=inferred
// flex yyset_debug().
void yyset_debug(int bdebug)
{
    g_yy_flex_debug = bdebug;
}

// 0x0808b903  size=154  FUN_0808b903  status=translated  naming=inferred
// flex yylex_destroy(): pops/deletes every buffer, frees the stack, returns 0.
int yylex_destroy(void)
{
    while (YY_CURRENT_BUFFER != 0) {
        yy_delete_buffer(YY_CURRENT_BUFFER);
        g_yy_buffer_stack[g_yy_buffer_stack_top] = 0;
        yypop_buffer_state();
    }
    yyfree(g_yy_buffer_stack);
    g_yy_buffer_stack = 0;
    return 0;
}

// 0x0808b99d  size=19  FUN_0808b99d  status=translated  naming=inferred
// flex yyalloc() = malloc (result discarded by the decompiler; the original returns it in eax).
void *yyalloc(size_t size)
{
    return malloc(size);
}

// 0x0808b9b0  size=26  FUN_0808b9b0  status=translated  naming=inferred
// flex yyrealloc() = realloc.
void *yyrealloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x0808b9ca  size=19  FUN_0808b9ca  status=translated  naming=inferred
// flex yyfree() = free.
void yyfree(void *ptr)
{
    free(ptr);
}

// 0x0808b9dd  size=78  FUN_0808b9dd  status=translated  naming=observed
// Strings 'unexpected end of file found' and 'bad syntax'; token 0x102 is the lexer's error token. Always returns 0.
int yyerror(const char *msg)
{
    (void)msg; /* the bison callers push the message; the callee ignores it (0x0808b9dd) */
    if (g_parse_last_token == 0)
        ScrCompile_ErrorInFile(g_lex_error_pos, "unexpected end of file found");
    else if (g_parse_last_token != 0x102)
        ScrCompile_ErrorInFile(g_lex_error_pos, "bad syntax");
    return 0;
}

// 0x0808ba2b  size=194  FUN_0808ba2b  status=translated  naming=unknown
// Resets lexer counters, installs a stack-allocated yy_buffer_state (ch_buf = ((byte*)&g_lex_char_buf), size 0x4000, not heap-owned),
// starts in start-condition 3, runs the parser yyparse and returns its result in *out_result. Descriptive name.
// The stack buffer stays referenced by the buffer stack after return (as in the original).
void Lex_RunParser(int *out_result, char param)
{
    struct yy_buffer_state buffer;

    memset(&buffer, 0, 0x30);
    g_lex_column = -1;
    g_lex_error_pos = 0;
    g_lex_param = (byte)param;
    g_lex_reserved = 0;
    g_yy_init = 1;
    buffer.buf_size = 0x4000;
    buffer.ch_buf = ((byte*)&g_lex_char_buf);
    buffer.is_our_buffer = 0;
    yy_init_buffer(&buffer, 0);
    yyensure_buffer_stack();
    g_yy_buffer_stack[g_yy_buffer_stack_top] = &buffer;
    g_yy_start = 3;
    yyparse();
    *out_result = g_parse_result;
}

// 0x0808baed  size=10  FUN_0808baed  status=translated  naming=inferred
// flex yywrap(): always 1.
int yywrap(void)
{
    return 1;
}

// 0x0808baf8  size=279  FUN_0808baf8  status=translated  naming=inferred
// Q3 SV_GetPlayerByHandle counterpart; strings 'No player specified.\n', 'Player %s is not on the server\n'.
// Matches Argv(1) against each active client's name, then against the colour-stripped name.
struct client_s *SV_GetPlayerByName(void)
{
    char clean[68];
    const char *name;
    int i;
    struct client_s *cl;

    if (g_dvar_sv_running->current.enabled == 0)
        return 0;
    if (Cmd_Argc_alt() < 2) {
        Com_Printf("No player specified.\n");
        return 0;
    }
    name = Cmd_Argv_alt(1);
    cl = g_svs_clients;
    for (i = 0; i < g_dvar_sv_maxclients->current.integer; i++) {
        if (cl->state != 0) {
            if (Q_stricmp(cl->name, name) == 0)
                return cl;
            Q_strncpyz(clean, cl->name, 0x40);
            Q_CleanStr(clean);
            if (Q_stricmp(clean, name) == 0)
                return cl;
        }
        cl++;
    }
    Com_Printf("Player %s is not on the server\n", name);
    return 0;
}

// 0x0808bc10  size=289  FUN_0808bc10  status=translated  naming=inferred
// Client lookup by slot number; strings 'Bad slot number: %s\n', 'Bad client slot: %i\n', 'Client %i is not active\n'.
struct client_s *SV_GetPlayerByNum(void)
{
    const char *arg;
    int i;
    int slot;
    struct client_s *cl;

    if (g_dvar_sv_running->current.enabled == 0)
        return 0;
    if (Cmd_Argc_alt() < 2) {
        Com_Printf("No player specified.\n");
        return 0;
    }
    arg = Cmd_Argv_alt(1);
    for (i = 0; arg[i] != '\0'; i++) {
        if (arg[i] < '0' || '9' < arg[i]) {
            Com_Printf("Bad slot number: %s\n", arg);
            return 0;
        }
    }
    slot = atoi(arg);
    if (slot < 0 || g_dvar_sv_maxclients->current.integer <= slot) {
        Com_Printf("Bad client slot: %i\n", slot);
        return 0;
    }
    cl = &g_svs_clients[slot];
    if (cl->state == 0) {
        Com_Printf("Client %i is not active\n", slot);
        return 0;
    }
    return cl;
}

// 0x0808bd32  size=19  FUN_0808bd32  status=translated  naming=unknown
// Thin forwarding wrapper around MapNameFromPath (used by the map/devmap command handler at 0x0808bd46).
char *SV_MapNameFromPath(char *arg)
{
    return MapNameFromPath(arg);
}

// 0x0808be3e  size=618  FUN_0808be3e  status=translated  naming=observed
// String 'SV_MapRestart_f: dropped client %i - denied!\n'. fast_restart != 0 for the fast path; otherwise (or if the
// maxclients dvar was modified / the gametype changed) performs a full map load of the current "mapname".
void SV_MapRestart_f(int fast_restart)
{
    int flag;
    int i;
    int k;
    struct client_s *cl;
    const char *denied;
    char mapname[68];

    if (g_dvar_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
        return;
    }
    SV_ValidateGametype();
    Q_strncpyz(g_saved_gametype, g_dvar_g_gametype->current.string, 0x40);
    flag = Level_GetIntValue();
    if (g_dvar_sv_maxclients->modified == 0 &&
        strcasecmp(g_saved_gametype, g_dvar_g_gametype->current.string) == 0 &&
        fast_restart != 0) {
        if (com_frameTime != g_sv_restartTime) {
            SV_ClearAllDvarFlag0400();
            SV_ClearPoolState();
            g_svs_snapFlags = g_svs_snapFlags ^ 4;
            g_sv_serverIdCounter = ((g_sv_serverIdCounter + 1) & 0xf) + (g_sv_serverIdCounter & 0xf0);
            Dvar_SetInt(g_dvar_0849f768, g_sv_serverIdCounter);
            g_sv_restartTime = com_frameTime;
            g_sv_state = 1;
            g_sv_restarting = 1;
            SV_RestartGameProgs(flag);
            for (k = 0; k < 3; k++) {
                g_svs_time = g_svs_time + 100;
                SV_RunGameFrame();
            }
            for (i = 0; i < g_dvar_sv_maxclients->current.integer; i++) {
                cl = &g_svs_clients[i];
                if (cl->state > 1) {
                    int c = (flag == 0) ? 0x42 : 0x6e;
                    SV_QueueReliableCommand(cl, 1, va("%c", c));
                    denied = ClientConnect(i, cl->scriptId);
                    if (denied == 0) {
                        if (cl->state == 4)
                            SV_ClientEnterWorld(cl, (const int *)&cl->lastUsercmd);
                    } else {
                        SV_DropClient(cl, denied);
                        Com_Printf("SV_MapRestart_f: dropped client %i - denied!\n", i);
                    }
                }
            }
            g_sv_state = 2;
            g_sv_restarting = 0;
        }
    } else {
        Level_SetIntValue(0);
        Q_strncpyz(mapname, Dvar_GetString("mapname"), 0x40);
        FS_NormalizeSeparators(mapname);
        SV_SpawnServer(mapname);
    }
}

// 0x0808c0a8  size=20  FUN_0808c0a8  status=translated  naming=observed
// "map_restart" command callback (registered in SV_AddOperatorCommands): SV_MapRestart_f(0).
void SV_MapRestart_cmd(void)
{
    SV_MapRestart_f(0);
}

// 0x0808c0bc  size=20  FUN_0808c0bc  status=translated  naming=observed
// "fast_restart" command callback: SV_MapRestart_f(1).
void SV_FastRestart_cmd(void)
{
    SV_MapRestart_f(1);
}

// 0x0808c0d0  size=98  FUN_0808c0d0  status=translated  naming=inferred
// Pops the next whitespace token off sv_mapRotationCurrent (Com_Parse advances the text pointer); writes the
// remainder (or "" at the end) back to the dvar and returns the token or NULL.
char *SV_MapRotation_NextToken(void)
{
    const char *text = g_dvar_sv_mapRotationCurrent->current.string;
    char *token = Com_Parse(&text);

    if (text == 0) {
        Dvar_SetString(g_dvar_sv_mapRotationCurrent, "");
        token = 0;
    } else {
        Dvar_SetString(g_dvar_sv_mapRotationCurrent, text);
    }
    return token;
}

// 0x0808c132  size=483  FUN_0808c132  status=translated  naming=observed
// Strings 'map_rotate...' and the sv_mapRotation / sv_mapRotationCurrent dumps; command "map_rotate".
void SV_MapRotate_f(void)
{
    char *token;
    const char *arg;

    Com_Printf("map_rotate...\n\n");
    Com_Printf("\"sv_mapRotation\" is:\"%s\"\n\n", g_dvar_sv_mapRotation->current.string);
    Com_Printf("\"sv_mapRotationCurrent\" is:\"%s\"\n\n", g_dvar_sv_mapRotationCurrent->current.string);
    if (*g_dvar_sv_mapRotationCurrent->current.string == '\0')
        Dvar_SetString(g_dvar_sv_mapRotationCurrent, g_dvar_sv_mapRotation->current.string);
    token = SV_MapRotation_NextToken();
    if (token == 0) {
        Dvar_SetString(g_dvar_sv_mapRotationCurrent, g_dvar_sv_mapRotation->current.string);
        token = SV_MapRotation_NextToken();
    }
    for (;;) {
        if (token == 0) {
            Com_Printf("No map specified in sv_mapRotation - forcing map_restart.\n");
            SV_FastRestart_cmd();
            return;
        }
        if (strcasecmp(token, "gametype") == 0) {
            token = SV_MapRotation_NextToken();
            if (token == 0) {
                Com_Printf("No gametype specified after 'gametype' keyword in sv_mapRotation - forcing map_restart.\n");
                SV_FastRestart_cmd();
                return;
            }
            Com_Printf("Setting g_gametype: %s.\n", token);
            if (g_dvar_sv_running->current.enabled != 0 &&
                strcasecmp(g_dvar_g_gametype->current.string, token) != 0)
                Level_SetIntValue(0);
            Dvar_SetString(g_dvar_g_gametype, token);
        } else if (strcasecmp(token, "map") == 0) {
            arg = SV_MapRotation_NextToken();
            if (arg == 0) {
                Com_Printf("No map specified after 'map' keyword in sv_mapRotation - forcing map_restart.\n");
                SV_FastRestart_cmd();
                return;
            }
            Com_Printf("Setting map: %s.\n", arg);
            Cbuf_ExecuteText(0, va("map %s\n", arg));
            return;
        } else {
            Com_Printf("Unknown keyword '%s' in sv_mapRotation.\n", token);
        }
        token = SV_MapRotation_NextToken();
    }
}

// 0x0808c316  size=166  FUN_0808c316  status=translated  naming=inferred
// Kicks one client (strings 'EXE_PLAYERKICKED', 'EXE_CANNOTKICKHOSTPLAYER'); optionally copies the cleaned name to out_name.
// Returns the client's field at +0xae704 (0 when the host/loopback client refuses the kick).
int SV_KickClient(struct client_s *cl, char *out_name, int out_size)
{
    int result;

    if (cl->netchan.remoteAddress.type == 2) {
        SV_SendServerCommand(0, 0, "%c \"EXE_CANNOTKICKHOSTPLAYER\"", 0x65);
        return 0;
    }
    if (out_name != 0) {
        Q_strncpyz(out_name, cl->name, out_size);
        Q_CleanStr(out_name);
    }
    result = cl->guid;
    SV_DropClient(cl, "EXE_PLAYERKICKED");
    cl->lastPacketTime = g_svs_time;
    return result;
}

// 0x0808c3bc  size=276  FUN_0808c3bc  status=translated  naming=inferred
// Kick-by-name handler body; strings 'Usage: %s <player name>\n%s all = kick everyone\n', 'Server is not running.\n'.
// "all" kicks every active client (the by-name lookup still prints its not-on-server message first, as compiled).
int SV_Kick_f(char *out_name, int out_size)
{
    struct client_s *cl;
    struct client_s *c;
    const char *arg;
    int i;

    if (g_dvar_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
        return 0;
    }
    if (Cmd_Argc_alt() != 2) {
        arg = Cmd_Argv_alt(0);
        Com_Printf("Usage: %s <player name>\n%s all = kick everyone\n", arg, arg);
        return 0;
    }
    cl = SV_GetPlayerByName();
    if (cl == 0) {
        arg = Cmd_Argv_alt(1);
        if (Q_stricmp(arg, "all") == 0) {
            c = g_svs_clients;
            for (i = 0; i < g_dvar_sv_maxclients->current.integer; i++) {
                if (c->state != 0)
                    SV_KickClient(c, 0, 0);
                c++;
            }
        }
        return 0;
    }
    return SV_KickClient(cl, out_name, out_size);
}

// 0x0808c4d0  size=141  FUN_0808c4d0  status=translated  naming=inferred
// Kick-by-slot handler body (command "clientkick"); string 'Usage: %s <client number>\n'.
int SV_KickNum_f(char *out_name, int out_size)
{
    struct client_s *cl;

    if (g_dvar_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
        return 0;
    }
    if (Cmd_Argc_alt() != 2) {
        Com_Printf("Usage: %s <client number>\n", Cmd_Argv_alt(0));
        return 0;
    }
    cl = SV_GetPlayerByNum();
    if (cl == 0)
        return 0;
    return SV_KickClient(cl, out_name, out_size);
}

// 0x0808cb98  size=15  FUN_0808cb98  status=translated  naming=inferred
// "heartbeat" command callback: forces the next master heartbeat by writing INT_MIN to the heartbeat time.
void SV_Heartbeat_f(void)
{
    g_svs_nextHeartbeatTime = (int)0x80000000;
}

// 0x0808cca6  size=544  FUN_0808cca6  status=translated  naming=inferred
// Registers the server operator commands once (guard flag at 0x083e2f08). Handlers at 0x0808c55e..0x0808cc98 were not
// split into functions by the decompile and are referenced through fn_ placeholders.
void SV_AddOperatorCommands(void)
{
    if (g_server_commands_registered != 0)
        return;
    g_server_commands_registered = 1;
    Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
    Cmd_AddCommand("onlykick", SV_OnlyKick_f);
    Cmd_AddCommand("banUser", SV_BanUser_f);
    Cmd_AddCommand("banClient", SV_BanClient_f);
    Cmd_AddCommand("kick", SV_TempBanUser_f);
    Cmd_AddCommand("tempBanUser", SV_TempBanUser_f);
    Cmd_AddCommand("tempBanClient", SV_TempBanClient_f);
    Cmd_AddCommand("unbanUser", SV_UnbanUser_f);
    Cmd_AddCommand("clientkick", SV_ClientKick_f);
    Cmd_AddCommand("status", SV_Status_f);
    Cmd_AddCommand("serverinfo", SV_ServerInfo_f);
    Cmd_AddCommand("systeminfo", SV_SystemInfo_f);
    Cmd_AddCommand("dumpuser", SV_DumpUser_f);
    Cmd_AddCommand("map_restart", SV_MapRestart_cmd);
    Cmd_AddCommand("fast_restart", SV_FastRestart_cmd);
    Cmd_AddCommand("map", SV_Map_f);
    Cmd_SetAutoComplete("map", (void *)"maps/mp", (void *)"d3dbsp");
    Cmd_AddCommand("map_rotate", SV_MapRotate_f);
    Cmd_AddCommand("gameCompleteStatus", SV_GameCompleteStatus_f);
    Cmd_AddCommand("devmap", SV_Map_f);
    Cmd_SetAutoComplete("devmap", (void *)"maps/mp", (void *)"d3dbsp");
    Cmd_AddCommand("killserver", SV_KillServer_f);
    if (g_dvar_084a8780->current.integer != 0)
        SV_AddSayTellCommands();
    Cmd_AddCommand("scriptUsage", SV_ScriptUsage_f);
    Cmd_AddCommand("stringUsage", SV_StringUsage_f);
}

// 0x0808cec6  size=5  FUN_0808cec6  status=translated  naming=unknown
// Empty function (single ret).
void SV_ShutdownStub(void)
{
}

// 0x0808cecc  size=48  FUN_0808cecc  status=translated  naming=inferred
// Registers the "say" and "tell" commands (strings 'say', 'tell', '%s: %s\n' follow in rodata).
void SV_AddSayTellCommands(void)
{
    Cmd_AddCommand("say", SV_ConSay_f);
    Cmd_AddCommand("tell", SV_ConTell_f);
}
