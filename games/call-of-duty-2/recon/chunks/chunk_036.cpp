#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_036_types.h"

/* Return codes used by the statically linked zlib. */
enum { Z_OK_ = 0, Z_STREAM_END_ = 1, Z_NEED_DICT_ = 2, Z_STREAM_ERROR_ = -2, Z_DATA_ERROR_ = -3,
       Z_MEM_ERROR_ = -4, Z_BUF_ERROR_ = -5, Z_VERSION_ERROR_ = -6 };

/* Free-space helper shared by the sliding window code: bytes writable at q without passing read. */
#define WINDOW_AVAIL(s, q) ((q) < (s)->read ? (uint32)((s)->read - (q) - 1) : (uint32)((s)->end - (q)))

// 0x080cbd40  size=101  FUN_080cbd40  status=translated  naming=inferred
// zlib inflate_codes_new: allocates 0x1c bytes through z->zalloc(opaque, 1, 0x1c) and stores lbits/dbits/ltree/dtree.
inflate_codes_state_s *inflate_codes_new(byte lbits, byte dbits, inflate_huft *tl, inflate_huft *td, z_stream_s *z)
{
    inflate_codes_state_s *c = (inflate_codes_state_s *)z->zalloc(z->opaque, 1, sizeof(inflate_codes_state_s));
    if (c != NULL) {
        c->mode = CODES_START;
        c->lbits = lbits;
        c->dbits = dbits;
        c->ltree = tl;
        c->dtree = td;
    }
    return c;
}

// 0x080cbda5  size=3439  FUN_080cbda5  status=translated  naming=inferred
// zlib inflate_codes (infcodes.c): strings "invalid literal/length code" / "invalid distance code", calls inflate_fast and inflate_flush.
#define IC_LOADIN  { p = z->next_in; n = z->avail_in; b = s->bitb; k = s->bitk; }
#define IC_LOADOUT { q = s->write; m = WINDOW_AVAIL(s, q); }
#define IC_UPDATE  { s->bitb = b; s->bitk = k; z->avail_in = n; \
                     z->total_in += (uint32)(p - z->next_in); z->next_in = p; s->write = q; }
#define IC_LEAVE   { IC_UPDATE; return inflate_flush(s, z, r); }
#define IC_NEEDBITS(j) { while (k < (uint32)(j)) { if (n == 0) IC_LEAVE; r = Z_OK_; n--; \
                         b |= (uint32)(*p++) << k; k += 8; } }
#define IC_DUMPBITS(j) { b >>= (j); k -= (j); }
#define IC_WRAP    { if (q == s->end && s->read != s->window) { q = s->window; m = WINDOW_AVAIL(s, q); } }
#define IC_FLUSH   { s->write = q; r = inflate_flush(s, z, r); IC_LOADOUT; }
#define IC_NEEDOUT { if (m == 0) { IC_WRAP; if (m == 0) { IC_FLUSH; IC_WRAP; if (m == 0) IC_LEAVE; } } r = Z_OK_; }

int inflate_codes(inflate_blocks_state_s *s, z_stream_s *z, int r)
{
    inflate_codes_state_s *c = s->sub.decode.codes;
    byte *p;
    uint32 n;
    uint32 b;
    uint32 k;
    byte *q;
    uint32 m;
    inflate_huft *t;
    uint32 e;
    byte *f;

    IC_LOADIN;
    IC_LOADOUT;

    for (;;) {
        switch (c->mode) {
        case CODES_START:
            if (m >= 258 && n >= 10) {
                IC_UPDATE;
                r = inflate_fast(c->lbits, c->dbits, c->ltree, c->dtree, s, z);
                IC_LOADIN;
                IC_LOADOUT;
                if (r != Z_OK_) {
                    c->mode = (r == Z_STREAM_END_) ? CODES_WASH : CODES_BADCODE;
                    break;
                }
            }
            c->sub.code.need = c->lbits;
            c->sub.code.tree = c->ltree;
            c->mode = CODES_LEN;
            /* fall through */
        case CODES_LEN:
            IC_NEEDBITS(c->sub.code.need);
            t = c->sub.code.tree + (inflate_mask[c->sub.code.need] & b);
            IC_DUMPBITS(t->bits);
            e = t->exop;
            if (e == 0) {
                c->sub.lit = t->base;
                c->mode = CODES_LIT;
                break;
            }
            if (e & 16) {
                c->sub.copy.get = e & 15;
                c->len = t->base;
                c->mode = CODES_LENEXT;
                break;
            }
            if ((e & 64) == 0) {
                c->sub.code.need = e;
                c->sub.code.tree = t + t->base;
                break;
            }
            if (e & 32) {
                c->mode = CODES_WASH;
                break;
            }
            c->mode = CODES_BADCODE;
            z->msg = "invalid literal/length code";
            r = Z_DATA_ERROR_;
            IC_LEAVE;
        case CODES_LENEXT:
            e = c->sub.copy.get;
            IC_NEEDBITS(e);
            c->len += inflate_mask[e] & b;
            IC_DUMPBITS(e);
            c->sub.code.need = c->dbits;
            c->sub.code.tree = c->dtree;
            c->mode = CODES_DIST;
            /* fall through */
        case CODES_DIST:
            IC_NEEDBITS(c->sub.code.need);
            t = c->sub.code.tree + (inflate_mask[c->sub.code.need] & b);
            IC_DUMPBITS(t->bits);
            e = t->exop;
            if (e & 16) {
                c->sub.copy.get = e & 15;
                c->sub.copy.dist = t->base;
                c->mode = CODES_DISTEXT;
                break;
            }
            if ((e & 64) == 0) {
                c->sub.code.need = e;
                c->sub.code.tree = t + t->base;
                break;
            }
            c->mode = CODES_BADCODE;
            z->msg = "invalid distance code";
            r = Z_DATA_ERROR_;
            IC_LEAVE;
        case CODES_DISTEXT:
            e = c->sub.copy.get;
            IC_NEEDBITS(e);
            c->sub.copy.dist += inflate_mask[e] & b;
            IC_DUMPBITS(e);
            c->mode = CODES_COPY;
            /* fall through */
        case CODES_COPY:
            f = q - c->sub.copy.dist;
            while (f < s->window)
                f += s->end - s->window;
            while (c->len != 0) {
                IC_NEEDOUT;
                *q++ = *f++;
                m--;
                if (f == s->end)
                    f = s->window;
                c->len--;
            }
            c->mode = CODES_START;
            break;
        case CODES_LIT:
            IC_NEEDOUT;
            *q++ = (byte)c->sub.lit;
            m--;
            c->mode = CODES_START;
            break;
        case CODES_WASH:
            if (k > 7) {
                k -= 8;
                n++;
                p--;
            }
            IC_FLUSH;
            if (s->read != s->write)
                IC_LEAVE;
            c->mode = CODES_END;
            /* fall through */
        case CODES_END:
            r = Z_STREAM_END_;
            IC_LEAVE;
        case CODES_BADCODE:
            r = Z_DATA_ERROR_;
            IC_LEAVE;
        default:
            r = Z_STREAM_ERROR_;
            IC_LEAVE;
        }
    }
}

// 0x080ccb14  size=32  FUN_080ccb14  status=translated  naming=inferred
// zlib inflate_codes_free: z->zfree(z->opaque, c).
void inflate_codes_free(inflate_codes_state_s *c, z_stream_s *z)
{
    z->zfree(z->opaque, c);
}

// 0x080ccb34  size=1805  FUN_080ccb34  status=translated  naming=inferred
// zlib inflate_fast (inffast.c): same two error strings as inflate_codes, called from it with 258/10 slack.
#define IF_GRABBITS(j) { while (k < (uint32)(j)) { n--; b |= (uint32)(*p++) << k; k += 8; } }
#define IF_UNGRAB { uint32 cnt = z->avail_in - n; if ((k >> 3) < cnt) cnt = k >> 3; n += cnt; p -= cnt; k -= cnt << 3; }
#define IF_UPDATE { s->bitb = b; s->bitk = k; z->avail_in = n; \
                    z->total_in += (uint32)(p - z->next_in); z->next_in = p; s->write = q; }

int inflate_fast(uint32 bl, uint32 bd, inflate_huft *tl, inflate_huft *td, inflate_blocks_state_s *s, z_stream_s *z)
{
    inflate_huft *t;
    uint32 e;
    uint32 b;
    uint32 k;
    byte *p;
    uint32 n;
    byte *q;
    uint32 m;
    uint32 ml;
    uint32 md;
    uint32 c;
    uint32 d;
    byte *r;

    p = z->next_in;
    n = z->avail_in;
    b = s->bitb;
    k = s->bitk;
    q = s->write;
    m = WINDOW_AVAIL(s, q);
    ml = inflate_mask[bl];
    md = inflate_mask[bd];

    do {
        IF_GRABBITS(20);
        t = tl + (b & ml);
        e = t->exop;
        if (e == 0) {
            b >>= t->bits;
            k -= t->bits;
            *q++ = (byte)t->base;
            m--;
            continue;
        }
        for (;;) {
            b >>= t->bits;
            k -= t->bits;
            if (e & 16) {
                e &= 15;
                c = t->base + (inflate_mask[e] & b);
                b >>= e;
                k -= e;
                IF_GRABBITS(15);
                t = td + (b & md);
                e = t->exop;
                for (;;) {
                    b >>= t->bits;
                    k -= t->bits;
                    if (e & 16)
                        break;
                    if (e & 64) {
                        z->msg = "invalid distance code";
                        IF_UNGRAB;
                        IF_UPDATE;
                        return Z_DATA_ERROR_;
                    }
                    t += (inflate_mask[e] & b) + t->base;
                    e = t->exop;
                }
                e &= 15;
                IF_GRABBITS(e);
                d = inflate_mask[e] & b;
                b >>= e;
                k -= e;
                m -= c;
                r = q - (d + t->base);
                if (r < s->window) {
                    do {
                        r += s->end - s->window;
                    } while (r < s->window);
                    e = (uint32)(s->end - r);
                    if (e < c) {
                        c -= e;
                        do {
                            *q++ = *r++;
                        } while (--e != 0);
                        r = s->window;
                        do {
                            *q++ = *r++;
                        } while (--c != 0);
                    } else {
                        *q++ = *r++;
                        *q++ = *r++;
                        c -= 2;
                        do {
                            *q++ = *r++;
                        } while (--c != 0);
                    }
                } else {
                    *q++ = *r++;
                    *q++ = *r++;
                    c -= 2;
                    do {
                        *q++ = *r++;
                    } while (--c != 0);
                }
                break;
            }
            if ((e & 64) == 0) {
                t += (inflate_mask[e] & b) + t->base;
                e = t->exop;
                if (e == 0) {
                    b >>= t->bits;
                    k -= t->bits;
                    *q++ = (byte)t->base;
                    m--;
                    break;
                }
                continue;
            }
            if (e & 32) {
                IF_UNGRAB;
                IF_UPDATE;
                return Z_STREAM_END_;
            }
            z->msg = "invalid literal/length code";
            IF_UNGRAB;
            IF_UPDATE;
            return Z_DATA_ERROR_;
        }
    } while (m >= 258 && n >= 10);

    IF_UNGRAB;
    IF_UPDATE;
    return Z_OK_;
}

// 0x080cd244  size=149  FUN_080cd244  status=translated  naming=inferred
// zlib inflateReset: zeroes total_in/total_out/msg, picks METHOD or BLOCKS (nowrap) and resets the block state.
int inflateReset(z_stream_s *z)
{
    inflate_state_s *state;
    if (z == NULL || z->state == NULL)
        return Z_STREAM_ERROR_;
    state = (inflate_state_s *)z->state;
    z->total_out = 0;
    z->total_in = 0;
    z->msg = NULL;
    state = (inflate_state_s *)z->state;
    state->mode = state->nowrap ? INFLATE_BLOCKS : INFLATE_METHOD;
    inflate_blocks_reset(((inflate_state_s *)z->state)->blocks, z, NULL);
    return Z_OK_;
}

// 0x080cd2d9  size=124  FUN_080cd2d9  status=translated  naming=inferred
// zlib inflateEnd: frees the block state and the stream state through z->zfree.
int inflateEnd(z_stream_s *z)
{
    if (z == NULL || z->state == NULL || z->zfree == NULL)
        return Z_STREAM_ERROR_;
    if (((inflate_state_s *)z->state)->blocks != NULL)
        inflate_blocks_free(((inflate_state_s *)z->state)->blocks, z);
    z->zfree(z->opaque, z->state);
    z->state = NULL;
    return Z_OK_;
}

// 0x080cd355  size=407  FUN_080cd355  status=translated  naming=inferred
// zlib inflateInit2_: version[0]=='1', stream_size==0x38 (sizeof z_stream), default zcalloc/zcfree, window bits 8..15.
int inflateInit2_(z_stream_s *z, int w, const char *version, int stream_size)
{
    inflate_state_s *state;
    check_fn checkfn;

    if (version == NULL || version[0] != '1' || stream_size != (int)sizeof(z_stream_s))
        return Z_VERSION_ERROR_;
    if (z == NULL)
        return Z_STREAM_ERROR_;
    z->msg = NULL;
    if (z->zalloc == NULL) {
        z->zalloc = zcalloc;
        z->opaque = NULL;
    }
    if (z->zfree == NULL)
        z->zfree = zcfree;
    z->state = z->zalloc(z->opaque, 1, sizeof(inflate_state_s));
    if (z->state == NULL)
        return Z_MEM_ERROR_;
    ((inflate_state_s *)z->state)->blocks = NULL;
    ((inflate_state_s *)z->state)->nowrap = 0;
    if (w < 0) {
        w = -w;
        ((inflate_state_s *)z->state)->nowrap = 1;
    }
    if (w < 8 || w > 15) {
        inflateEnd(z);
        return Z_STREAM_ERROR_;
    }
    ((inflate_state_s *)z->state)->wbits = (uint32)w;
    state = (inflate_state_s *)z->state;
    if (state->nowrap == 0)
        checkfn = adler32;
    else
        checkfn = NULL;
    state->blocks = inflate_blocks_new(z, checkfn, 1u << w);
    if (state->blocks == NULL) {
        inflateEnd(z);
        return Z_MEM_ERROR_;
    }
    inflateReset(z);
    return Z_OK_;
}

// 0x080cd4ec  size=41  FUN_080cd4ec  status=translated  naming=inferred
// zlib inflateInit_: inflateInit2_ with the maximum window bits (15).
int inflateInit_(z_stream_s *z, const char *version, int stream_size)
{
    return inflateInit2_(z, 15, version, stream_size);
}

// 0x080cd515  size=1458  FUN_080cd515  status=translated  naming=observed
// zlib inflate(): strings "unknown compression method", "invalid window size", "incorrect header check", "need dictionary", "incorrect data check".
#define INF_NEEDBYTE { if (z->avail_in == 0) return r; r = f; }
#define INF_NEXTBYTE (z->avail_in--, z->total_in++, *z->next_in++)

int inflate(z_stream_s *z, int flush)
{
    int f;
    int r;
    uint32 b;
    inflate_state_s *state;

    if (z == NULL || z->state == NULL || z->next_in == NULL)
        return Z_STREAM_ERROR_;
    f = (flush == 4) ? Z_BUF_ERROR_ : Z_OK_;
    r = Z_BUF_ERROR_;
    for (;;) {
        state = (inflate_state_s *)z->state;
        switch (state->mode) {
        case INFLATE_METHOD:
            INF_NEEDBYTE;
            state->sub = INF_NEXTBYTE;
            if ((state->sub & 0xf) != 8) {
                state->mode = INFLATE_BAD;
                z->msg = "unknown compression method";
                state->sub = 5;
                break;
            }
            if (state->wbits < (state->sub >> 4) + 8) {
                state->mode = INFLATE_BAD;
                z->msg = "invalid window size";
                state->sub = 5;
                break;
            }
            state->mode = INFLATE_FLAG;
            /* fall through */
        case INFLATE_FLAG:
            INF_NEEDBYTE;
            b = INF_NEXTBYTE;
            if ((state->sub * 0x100 + b) % 0x1f != 0) {
                state->mode = INFLATE_BAD;
                z->msg = "incorrect header check";
                state->sub = 5;
                break;
            }
            if ((b & 0x20) == 0) {
                state->mode = INFLATE_BLOCKS;
                break;
            }
            state->mode = INFLATE_DICT4;
            /* fall through */
        case INFLATE_DICT4:
            INF_NEEDBYTE;
            state->need = (uint32)INF_NEXTBYTE << 24;
            state->mode = INFLATE_DICT3;
            /* fall through */
        case INFLATE_DICT3:
            INF_NEEDBYTE;
            state->need += (uint32)INF_NEXTBYTE * 0x10000;
            state->mode = INFLATE_DICT2;
            /* fall through */
        case INFLATE_DICT2:
            INF_NEEDBYTE;
            state->need += (uint32)INF_NEXTBYTE * 0x100;
            state->mode = INFLATE_DICT1;
            /* fall through */
        case INFLATE_DICT1:
            if (z->avail_in == 0)
                return r;
            state->need += (uint32)INF_NEXTBYTE;
            z->adler = state->need;
            state->mode = INFLATE_DICT0;
            return Z_NEED_DICT_;
        case INFLATE_DICT0:
            state->mode = INFLATE_BAD;
            z->msg = "need dictionary";
            state->sub = 0;
            return Z_STREAM_ERROR_;
        case INFLATE_BLOCKS:
            r = inflate_blocks(state->blocks, z, r);
            if (r == Z_DATA_ERROR_) {
                state->mode = INFLATE_BAD;
                state->sub = 0;
                r = Z_DATA_ERROR_;
                break;
            }
            if (r == Z_OK_)
                r = f;
            if (r != Z_STREAM_END_)
                return r;
            r = f;
            inflate_blocks_reset(state->blocks, z, (unsigned long *)&state->sub);
            if (state->nowrap == 0) {
                state->mode = INFLATE_CHECK4;
                /* fall through to CHECK4 */
            } else {
                state->mode = INFLATE_DONE;
                break;
            }
            /* fall through */
        case INFLATE_CHECK4:
            INF_NEEDBYTE;
            state->need = (uint32)INF_NEXTBYTE << 24;
            state->mode = INFLATE_CHECK3;
            /* fall through */
        case INFLATE_CHECK3:
            INF_NEEDBYTE;
            state->need += (uint32)INF_NEXTBYTE * 0x10000;
            state->mode = INFLATE_CHECK2;
            /* fall through */
        case INFLATE_CHECK2:
            INF_NEEDBYTE;
            state->need += (uint32)INF_NEXTBYTE * 0x100;
            state->mode = INFLATE_CHECK1;
            /* fall through */
        case INFLATE_CHECK1:
            INF_NEEDBYTE;
            state->need += (uint32)INF_NEXTBYTE;
            if (state->sub == state->need) {
                state->mode = INFLATE_DONE;
                return Z_STREAM_END_;
            }
            state->mode = INFLATE_BAD;
            z->msg = "incorrect data check";
            state->sub = 5;
            break;
        case INFLATE_DONE:
            return Z_STREAM_END_;
        case INFLATE_BAD:
            return Z_DATA_ERROR_;
        default:
            return Z_STREAM_ERROR_;
        }
    }
}

// 0x080cdac7  size=219  FUN_080cdac7  status=translated  naming=inferred
// zlib inflateSetDictionary: requires mode DICT0, checks adler32 of the dictionary against z->adler.
int inflateSetDictionary(z_stream_s *z, const byte *dictionary, uint32 dictLength)
{
    uint32 length = dictLength;
    inflate_state_s *state;

    if (z == NULL || z->state == NULL || ((inflate_state_s *)z->state)->mode != INFLATE_DICT0)
        return Z_STREAM_ERROR_;
    if (adler32(1, dictionary, dictLength) != z->adler)
        return Z_DATA_ERROR_;
    z->adler = 1;
    state = (inflate_state_s *)z->state;
    if ((dictLength >> state->wbits) != 0) {
        length = (1u << state->wbits) - 1;
        dictionary += dictLength - length;
    }
    inflate_set_dictionary(((inflate_state_s *)z->state)->blocks, dictionary, length);
    ((inflate_state_s *)z->state)->mode = INFLATE_BLOCKS;
    return Z_OK_;
}

// 0x080cdba2  size=336  FUN_080cdba2  status=translated  naming=inferred
// zlib inflateSync: scans for the 00 00 ff ff stored-block marker (pattern at 0x8150b9c), then resets while keeping total_in/total_out.
int inflateSync(z_stream_s *z)
{
    uint32 n;
    byte *p;
    uint32 m;
    uint32 r;
    uint32 w;
    inflate_state_s *state;

    if (z == NULL || z->state == NULL)
        return Z_STREAM_ERROR_;
    state = (inflate_state_s *)z->state;
    if (state->mode != INFLATE_BAD) {
        state->mode = INFLATE_BAD;
        state->sub = 0;
    }
    n = z->avail_in;
    if (n == 0)
        return Z_BUF_ERROR_;
    p = z->next_in;
    m = ((inflate_state_s *)z->state)->sub;
    while (n != 0 && m < 4) {
        if (*p == inflateSync_mark[m])
            m++;
        else if (*p != 0)
            m = 0;
        else
            m = 4 - m;
        p++;
        n--;
    }
    z->total_in += (uint32)(p - z->next_in);
    z->next_in = p;
    z->avail_in = n;
    ((inflate_state_s *)z->state)->sub = m;
    if (m != 4)
        return Z_DATA_ERROR_;
    r = z->total_in;
    w = z->total_out;
    inflateReset(z);
    z->total_in = r;
    z->total_out = w;
    ((inflate_state_s *)z->state)->mode = INFLATE_BLOCKS;
    return Z_OK_;
}

// 0x080cdcf2  size=67  FUN_080cdcf2  status=translated  naming=inferred
// zlib inflateSyncPoint: forwards to the block layer's sync-point query.
int inflateSyncPoint(z_stream_s *z)
{
    if (z == NULL || z->state == NULL || ((inflate_state_s *)z->state)->blocks == NULL)
        return Z_STREAM_ERROR_;
    return inflate_blocks_sync_point(((inflate_state_s *)z->state)->blocks);
}

// 0x080cdd38  size=2094  FUN_080cdd38  status=translated  naming=inferred
// zlib huft_build (inftrees.c): builds decoding tables; returns -3 (over-subscribed / table overflow), -5 (incomplete), 0.
int huft_build(const uint32 *b, uint32 n, uint32 s, const uint32 *d, const uint32 *e,
               inflate_huft **t, uint32 *m, inflate_huft *hp, uint32 *hn, uint32 *v)
{
    uint32 a;
    uint32 c[16 + 1];
    uint32 f;
    int g;
    int h;
    uint32 i;
    uint32 j;
    int k;
    int l;
    uint32 mask;
    uint32 *p;
    inflate_huft *q;
    inflate_huft r;
    inflate_huft *u[15];
    int w;
    uint32 x[16 + 1];
    uint32 *xp;
    int y;
    uint32 z;

    /* count the code lengths */
    for (i = 0; i < 17; i++)
        c[i] = 0;
    p = (uint32 *)b;
    i = n;
    do {
        c[*p]++;
        p++;
    } while (--i != 0);
    if (c[0] == n) {
        *t = NULL;
        *m = 0;
        return Z_OK_;
    }

    /* find min and max length, bound *m by those */
    l = (int)*m;
    for (j = 1; j <= 15; j++)
        if (c[j] != 0)
            break;
    k = (int)j;
    if ((uint32)l < j)
        l = (int)j;
    for (i = 15; i != 0; i--)
        if (c[i] != 0)
            break;
    g = (int)i;
    if ((uint32)l > i)
        l = (int)i;
    *m = (uint32)l;

    /* adjust last length count to fill out codes, if needed */
    for (y = 1 << j; j < i; j++, y <<= 1)
        if ((y -= (int)c[j]) < 0)
            return Z_DATA_ERROR_;
    if ((y -= (int)c[i]) < 0)
        return Z_DATA_ERROR_;
    c[i] += (uint32)y;

    /* generate starting offsets into the value table for each length */
    x[1] = j = 0;
    p = c + 1;
    xp = x + 2;
    while (--i != 0)
        *xp++ = (j += *p++);

    /* make a table of values in order of bit lengths */
    p = (uint32 *)b;
    i = 0;
    do {
        if ((j = *p++) != 0)
            v[x[j]++] = i;
    } while (++i < n);
    n = x[g];

    /* generate the Huffman codes and for each, make the table entries */
    x[0] = i = 0;
    p = v;
    h = -1;
    w = -l;
    u[0] = NULL;
    q = NULL;
    z = 0;

    for (; k <= g; k++) {
        a = c[k];
        while (a-- != 0) {
            /* here i is the Huffman code of length k bits for value *p */
            while (k > w + l) {
                h++;
                w += l;

                /* compute minimum size table less than or equal to l bits */
                z = (uint32)(g - w);
                z = (z > (uint32)l) ? (uint32)l : z;
                j = (uint32)(k - w);
                f = 1u << j;
                if (f > a + 1) {
                    f -= a + 1;
                    xp = c + k;
                    if (j < z) {
                        while (++j < z) {
                            if ((f <<= 1) <= *++xp)
                                break;
                            f -= *xp;
                        }
                    }
                }
                z = 1u << j;

                /* allocate new table */
                if (*hn + z > 1440)
                    return Z_DATA_ERROR_;
                u[h] = q = hp + *hn;
                *hn += z;

                /* connect to last table, if there is one */
                if (h != 0) {
                    x[h] = i;
                    r.bits = (byte)l;
                    r.exop = (byte)j;
                    j = i >> (w - l);
                    r.base = (uint32)(q - u[h - 1]) - j;
                    u[h - 1][j] = r;
                } else {
                    *t = q;
                }
            }

            /* set up table entry in r */
            r.bits = (byte)(k - w);
            if (p >= v + n) {
                r.exop = 128 + 64;
            } else if (*p < s) {
                r.exop = (byte)(*p < 256 ? 0 : 32 + 64);
                r.base = *p++;
            } else {
                r.exop = (byte)((byte)e[*p - s] + 16 + 64);
                r.base = d[*p++ - s];
            }

            /* fill code-like entries with r */
            f = 1u << (k - w);
            for (j = i >> w; j < z; j += f)
                q[j] = r;

            /* backwards increment the k-bit code i */
            for (j = 1u << (k - 1); (i & j) != 0; j >>= 1)
                i ^= j;
            i ^= j;

            /* backup over finished tables */
            mask = (1u << w) - 1;
            while ((i & mask) != x[h]) {
                h--;
                w -= l;
                mask = (1u << w) - 1;
            }
        }
    }

    /* Return Z_BUF_ERROR if we were given an incomplete table */
    return (y != 0 && g != 1) ? Z_BUF_ERROR_ : Z_OK_;
}

// 0x080ce566  size=233  FUN_080ce566  status=translated  naming=observed
// zlib inflate_trees_bits: strings "oversubscribed dynamic bit lengths tree" / "incomplete dynamic bit lengths tree".
int inflate_trees_bits(uint32 *c, uint32 *bb, inflate_huft **tb, inflate_huft *hp, z_stream_s *z)
{
    int r;
    uint32 hn = 0;
    uint32 *v;

    v = (uint32 *)z->zalloc(z->opaque, 19, sizeof(uint32));
    if (v == NULL)
        return Z_MEM_ERROR_;
    r = huft_build(c, 19, 19, NULL, NULL, tb, bb, hp, &hn, v);
    if (r == Z_DATA_ERROR_) {
        z->msg = "oversubscribed dynamic bit lengths tree";
    } else if (r == Z_BUF_ERROR_ || *bb == 0) {
        z->msg = "incomplete dynamic bit lengths tree";
        r = Z_DATA_ERROR_;
    }
    z->zfree(z->opaque, v);
    return r;
}

// 0x080ce64f  size=483  FUN_080ce64f  status=translated  naming=observed
// zlib inflate_trees_dynamic: strings "oversubscribed distance tree", "incomplete distance tree", "empty distance tree with lengths", "oversubscribed literal/length tree", "incomplete literal/length tree".
int inflate_trees_dynamic(uint32 nl, uint32 nd, uint32 *c, uint32 *bl, uint32 *bd,
                          inflate_huft **tl, inflate_huft **td, inflate_huft *hp, z_stream_s *z)
{
    int r;
    uint32 hn = 0;
    uint32 *v;

    v = (uint32 *)z->zalloc(z->opaque, 288, sizeof(uint32));
    if (v == NULL)
        return Z_MEM_ERROR_;

    r = huft_build(c, nl, 257, cplens, cplext, tl, bl, hp, &hn, v);
    if (r != Z_OK_ || *bl == 0) {
        if (r == Z_DATA_ERROR_) {
            z->msg = "oversubscribed literal/length tree";
        } else if (r != Z_MEM_ERROR_) {
            z->msg = "incomplete literal/length tree";
            r = Z_DATA_ERROR_;
        }
        z->zfree(z->opaque, v);
        return r;
    }

    r = huft_build(c + nl, nd, 0, cpdist, cpdext, td, bd, hp, &hn, v);
    if (r != Z_OK_ || (*bd == 0 && nl > 0x101)) {
        if (r == Z_DATA_ERROR_) {
            z->msg = "oversubscribed distance tree";
        } else if (r == Z_BUF_ERROR_) {
            z->msg = "incomplete distance tree";
            r = Z_DATA_ERROR_;
        } else if (r != Z_MEM_ERROR_) {
            z->msg = "empty distance tree with lengths";
            r = Z_DATA_ERROR_;
        }
        z->zfree(z->opaque, v);
        return r;
    }

    z->zfree(z->opaque, v);
    return Z_OK_;
}

// 0x080ce832  size=48  FUN_080ce832  status=translated  naming=inferred
// zlib inflate_trees_fixed: returns the precomputed fixed tables (bl=9, bd=5 at 0x8183240/44, tl at 0x8183260, td at 0x8184260).
int inflate_trees_fixed(uint32 *bl, uint32 *bd, inflate_huft **tl, inflate_huft **td)
{
    *bl = fixed_bl;
    *bd = fixed_bd;
    *tl = fixed_tl;
    *td = fixed_td;
    return Z_OK_;
}

// 0x080ce864  size=511  FUN_080ce864  status=translated  naming=inferred
// zlib inflate_flush (infutil.c): copies the window [read, write) to next_out, updates the running check via checkfn.
int inflate_flush(inflate_blocks_state_s *s, z_stream_s *z, int r)
{
    uint32 n;
    byte *p;
    byte *q;

    q = z->next_out;
    p = s->read;

    n = (uint32)((p <= s->write ? s->write : s->end) - p);
    if (n > z->avail_out)
        n = z->avail_out;
    if (n != 0 && r == Z_BUF_ERROR_)
        r = Z_OK_;

    z->avail_out -= n;
    z->total_out += n;

    if (s->checkfn != NULL) {
        s->check = s->checkfn(s->check, p, n);
        z->adler = s->check;
    }

    if (q != NULL) {
        memcpy(q, p, n);
        q += n;
    }
    p += n;

    if (p == s->end) {
        p = s->window;
        if (s->write == s->end)
            s->write = s->window;

        n = (uint32)(s->write - p);
        if (n > z->avail_out)
            n = z->avail_out;
        if (n != 0 && r == Z_BUF_ERROR_)
            r = Z_OK_;

        z->avail_out -= n;
        z->total_out += n;

        if (s->checkfn != NULL) {
            s->check = s->checkfn(s->check, p, n);
            z->adler = s->check;
        }

        if (q != NULL) {
            memcpy(q, p, n);
            q += n;
        }
        p += n;
    }

    z->next_out = q;
    s->read = p;
    return r;
}

// 0x080cea64  size=5  FUN_080cea64  status=translated  naming=inferred
// zlib tr_static_init: empty in this build (static trees are precomputed tables).
void tr_static_init(void)
{
}

// 0x080cea69  size=152  FUN_080cea69  status=translated  naming=inferred
// zlib _tr_init: hooks the three tree descriptors to the static descriptors at 0x81843a4/b8/cc and clears the bit buffer.
void _tr_init(deflate_state_s *s)
{
    tr_static_init();

    s->l_desc.dyn_tree = s->dyn_ltree;
    s->l_desc.stat_desc = &static_l_desc;

    s->d_desc.dyn_tree = s->dyn_dtree;
    s->d_desc.stat_desc = &static_d_desc;

    s->bl_desc.dyn_tree = s->bl_tree;
    s->bl_desc.stat_desc = &static_bl_desc;

    s->bi_buf = 0;
    s->bi_valid = 0;
    s->last_eob_len = 8;

    init_block(s);
}

// 0x080ceb01  size=189  FUN_080ceb01  status=translated  naming=inferred
// zlib init_block: zeroes 286/30/19 frequencies, sets the end-of-block count to 1 and clears the block counters.
void init_block(deflate_state_s *s)
{
    int n;

    for (n = 0; n < 286; n++)
        s->dyn_ltree[n].fc = 0;
    for (n = 0; n < 30; n++)
        s->dyn_dtree[n].fc = 0;
    for (n = 0; n < 19; n++)
        s->bl_tree[n].fc = 0;

    s->dyn_ltree[256].fc = 1;
    s->static_len = 0;
    s->opt_len = 0;
    s->matches = 0;
    s->last_lit = 0;
}

// 0x080cebbe  size=450  FUN_080cebbe  status=translated  naming=inferred
// zlib pqdownheap: sift-down on s->heap ordered by frequency, ties broken by subtree depth.
#define TREE_SMALLER(tree, n, m, depth) \
    ((tree)[n].fc < (tree)[m].fc || ((tree)[n].fc == (tree)[m].fc && (depth)[n] <= (depth)[m]))

void pqdownheap(deflate_state_s *s, ct_data *tree, int k)
{
    int v = s->heap[k];
    int j = k << 1;

    while (j <= s->heap_len) {
        if (j < s->heap_len && TREE_SMALLER(tree, s->heap[j + 1], s->heap[j], s->depth))
            j++;
        if (TREE_SMALLER(tree, v, s->heap[j], s->depth))
            break;
        s->heap[k] = s->heap[j];
        k = j;
        j <<= 1;
    }
    s->heap[k] = v;
}

// 0x080ced80  size=828  FUN_080ced80  status=translated  naming=inferred
// zlib gen_bitlen: computes code lengths from the Huffman tree, limiting to max_length and updating opt_len/static_len.
void gen_bitlen(deflate_state_s *s, tree_desc *desc)
{
    ct_data *tree = desc->dyn_tree;
    int max_code = desc->max_code;
    const ct_data *stree = desc->stat_desc->static_tree;
    const int *extra = desc->stat_desc->extra_bits;
    int base = desc->stat_desc->extra_base;
    uint32 max_length = (uint32)desc->stat_desc->max_length;
    int h;
    int n;
    int m;
    uint32 bits;
    int xbits;
    uint16 f;
    int overflow = 0;

    for (bits = 0; (int)bits < 16; bits++)
        s->bl_count[bits] = 0;

    tree[s->heap[s->heap_max]].dl = 0;

    for (h = s->heap_max + 1; h < DEFLATE_HEAP_SIZE; h++) {
        n = s->heap[h];
        bits = (uint32)tree[tree[n].dl].dl + 1;
        if ((int)max_length < (int)bits) {
            bits = max_length;
            overflow++;
        }
        tree[n].dl = (uint16)bits;
        if (n > max_code)
            continue;

        s->bl_count[bits]++;
        xbits = 0;
        if (n >= base)
            xbits = extra[n - base];
        f = tree[n].fc;
        s->opt_len += (uint32)f * (bits + (uint32)xbits);
        if (stree != NULL)
            s->static_len += (uint32)f * ((uint32)stree[n].dl + (uint32)xbits);
    }
    if (overflow == 0)
        return;

    do {
        bits = max_length;
        do {
            bits--;
        } while (s->bl_count[bits] == 0);
        s->bl_count[bits]--;
        s->bl_count[bits + 1] = (uint16)(s->bl_count[bits + 1] + 2);
        s->bl_count[max_length]--;
        overflow -= 2;
    } while (overflow > 0);

    for (bits = max_length; bits != 0; bits--) {
        uint32 cnt = s->bl_count[bits];
        while (cnt != 0) {
            m = s->heap[--h];
            if (m > max_code)
                continue;
            if (tree[m].dl != bits) {
                s->opt_len += (uint32)tree[m].fc * (bits - (uint32)tree[m].dl);
                tree[m].dl = (uint16)bits;
            }
            cnt--;
        }
    }
}

// 0x080cf0bc  size=182  FUN_080cf0bc  status=translated  naming=inferred
// zlib gen_codes: assigns canonical Huffman codes (bit-reversed through the bi_reverse callee) from bl_count.
void gen_codes(ct_data *tree, int max_code, const uint16 *bl_count)
{
    uint16 next_code[16];
    uint16 code = 0;
    int bits;
    int n;

    for (bits = 1; bits < 16; bits++) {
        code = (uint16)((bl_count[bits - 1] + code) << 1);
        next_code[bits] = code;
    }
    for (n = 0; n <= max_code; n++) {
        uint32 len = tree[n].dl;
        if (len == 0)
            continue;
        tree[n].fc = bi_reverse(next_code[len]++, (int)len);
    }
}

// 0x080cf172  size=927  FUN_080cf172  status=translated  naming=inferred
// zlib build_tree: builds the Huffman tree for one tree_desc via the heap (leaves are n < elems, internal nodes start at elems).
void build_tree(deflate_state_s *s, tree_desc *desc)
{
    ct_data *tree = desc->dyn_tree;
    const ct_data *stree = desc->stat_desc->static_tree;
    int elems = desc->stat_desc->elems;
    int n;
    int m;
    int max_code = -1;
    int node;

    s->heap_len = 0;
    s->heap_max = DEFLATE_HEAP_SIZE;

    for (n = 0; n < elems; n++) {
        if (tree[n].fc != 0) {
            s->heap[++s->heap_len] = max_code = n;
            s->depth[n] = 0;
        } else {
            tree[n].dl = 0;
        }
    }

    while (s->heap_len < 2) {
        node = (max_code < 2) ? ++max_code : 0;
        s->heap[++s->heap_len] = node;
        tree[node].fc = 1;
        s->depth[node] = 0;
        s->opt_len--;
        if (stree != NULL)
            s->static_len -= stree[node].dl;
    }
    desc->max_code = max_code;

    for (n = s->heap_len / 2; n >= 1; n--)
        pqdownheap(s, tree, n);

    node = elems;
    do {
        n = s->heap[1];
        s->heap[1] = s->heap[s->heap_len--];
        pqdownheap(s, tree, 1);
        m = s->heap[1];

        s->heap[--s->heap_max] = n;
        s->heap[--s->heap_max] = m;

        tree[node].fc = (uint16)(tree[n].fc + tree[m].fc);
        s->depth[node] = (byte)(((s->depth[n] >= s->depth[m]) ? s->depth[n] : s->depth[m]) + 1);
        tree[n].dl = (uint16)node;
        tree[m].dl = tree[n].dl;

        s->heap[1] = node++;
        pqdownheap(s, tree, 1);
    } while (s->heap_len >= 2);

    s->heap[--s->heap_max] = s->heap[1];

    gen_bitlen(s, desc);
    gen_codes(tree, max_code, s->bl_count);
}

// 0x080cf511  size=403  FUN_080cf511  status=translated  naming=inferred
// zlib scan_tree: run-length scans a literal/distance length table and counts bl_tree symbol frequencies (16/17/18 are repeat codes).
void scan_tree(deflate_state_s *s, ct_data *tree, int max_code)
{
    int n;
    int prevlen = -1;
    int curlen;
    int nextlen = tree[0].dl;
    int count = 0;
    int max_count = 7;
    int min_count = 4;

    if (nextlen == 0) {
        max_count = 138;
        min_count = 3;
    }
    tree[max_code + 1].dl = 0xffff;

    for (n = 0; n <= max_code; n++) {
        curlen = nextlen;
        nextlen = tree[n + 1].dl;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            s->bl_tree[curlen].fc = (uint16)(s->bl_tree[curlen].fc + count);
        } else if (curlen != 0) {
            if (curlen != prevlen)
                s->bl_tree[curlen].fc++;
            s->bl_tree[16].fc++;
        } else if (count <= 10) {
            s->bl_tree[17].fc++;
        } else {
            s->bl_tree[18].fc++;
        }
        count = 0;
        prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138;
            min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6;
            min_count = 3;
        } else {
            max_count = 7;
            min_count = 4;
        }
    }
}

// 0x080cf6a4  size=2502  FUN_080cf6a4  status=translated  naming=inferred
// zlib send_tree: emits the run-length coded length table through bl_tree codes; send_bits is inlined 16-bit bit-buffer code.
static inline void tr_send_bits(deflate_state_s *s, uint32 value, int length)
{
    if (s->bi_valid > 16 - length) {
        s->bi_buf = (uint16)(s->bi_buf | ((uint16)value << s->bi_valid));
        s->pending_buf[s->pending++] = (byte)(s->bi_buf & 0xff);
        s->pending_buf[s->pending++] = (byte)((uint16)s->bi_buf >> 8);
        s->bi_buf = (uint16)((uint16)value >> (16 - s->bi_valid));
        s->bi_valid += length - 16;
    } else {
        s->bi_buf = (uint16)(s->bi_buf | ((uint16)value << s->bi_valid));
        s->bi_valid += length;
    }
}

void send_tree(deflate_state_s *s, ct_data *tree, int max_code)
{
    int n;
    int prevlen = -1;
    int curlen;
    int nextlen = tree[0].dl;
    int count = 0;
    int max_count = 7;
    int min_count = 4;

    if (nextlen == 0) {
        max_count = 138;
        min_count = 3;
    }

    for (n = 0; n <= max_code; n++) {
        curlen = nextlen;
        nextlen = tree[n + 1].dl;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            do {
                tr_send_bits(s, s->bl_tree[curlen].fc, s->bl_tree[curlen].dl);
            } while (--count != 0);
        } else if (curlen != 0) {
            if (curlen != prevlen) {
                tr_send_bits(s, s->bl_tree[curlen].fc, s->bl_tree[curlen].dl);
                count--;
            }
            tr_send_bits(s, s->bl_tree[16].fc, s->bl_tree[16].dl);
            tr_send_bits(s, (uint32)(count - 3), 2);
        } else if (count <= 10) {
            tr_send_bits(s, s->bl_tree[17].fc, s->bl_tree[17].dl);
            tr_send_bits(s, (uint32)(count - 3), 3);
        } else {
            tr_send_bits(s, s->bl_tree[18].fc, s->bl_tree[18].dl);
            tr_send_bits(s, (uint32)(count - 11), 7);
        }
        count = 0;
        prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138;
            min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6;
            min_count = 3;
        } else {
            max_count = 7;
            min_count = 4;
        }
    }
}
