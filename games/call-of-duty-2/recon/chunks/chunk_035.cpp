#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_035_types.h"

/* Chunk 035: XModel LOD accessors, then the statically linked zlib 1.1.4 (deflate + inflate_blocks). */

#define MIN_MATCH 3
#define MAX_MATCH 258
#define MIN_LOOKAHEAD (MAX_MATCH + MIN_MATCH + 1)
#define MAX_DIST(s) ((s)->w_size - MIN_LOOKAHEAD)
#define TOO_FAR 4096
#define ZLIB_VERSION_STRING "1.1.4"

enum block_state { need_more = 0, block_done = 1, finish_started = 2, finish_done = 3 };

#define UPDATE_HASH(s, h, c) ((h) = (((h) << (s)->hash_shift) ^ (c)) & (s)->hash_mask)
#define INSERT_STRING(s, str, match_head)                                                         \
    (UPDATE_HASH(s, (s)->ins_h, (s)->window[(str) + (MIN_MATCH - 1)]),                            \
     (s)->prev[(str) & (s)->w_mask] = (match_head) = (s)->head[(s)->ins_h],                       \
     (s)->head[(s)->ins_h] = (ush)(str))

#define d_code(dist) ((dist) < 256 ? _dist_code[dist] : _dist_code[256 + ((dist) >> 7)])

#define TALLY_LIT(s, c, flush)                                                                    \
    {                                                                                             \
        Byte cc_ = (c);                                                                           \
        (s)->d_buf[(s)->last_lit] = 0;                                                            \
        (s)->l_buf[(s)->last_lit++] = cc_;                                                        \
        (s)->dyn_ltree[cc_].freq_or_code++;                                                       \
        (flush) = ((s)->last_lit == (s)->lit_bufsize - 1);                                        \
    }
#define TALLY_DIST(s, distance, lc, flush)                                                        \
    {                                                                                             \
        Byte len_ = (Byte)(lc);                                                                   \
        ush dist_ = (ush)(distance);                                                              \
        (s)->d_buf[(s)->last_lit] = dist_;                                                        \
        (s)->l_buf[(s)->last_lit++] = len_;                                                       \
        dist_--;                                                                                  \
        (s)->dyn_ltree[_length_code[len_] + 256 + 1].freq_or_code++;                                \
        (s)->dyn_dtree[d_code(dist_)].freq_or_code++;                                             \
        (flush) = ((s)->last_lit == (s)->lit_bufsize - 1);                                        \
    }

#define FLUSH_BLOCK_ONLY(s, eof)                                                                  \
    {                                                                                             \
        tr_flush_block((s), ((s)->block_start >= 0L ? (byte *)&(s)->window[(unsigned)(s)->block_start] \
                                                 : (byte *)0),                                    \
                    (uLong)((long)(s)->strstart - (s)->block_start), (eof));                      \
        (s)->block_start = (s)->strstart;                                                         \
        flush_pending((s)->strm);                                                                 \
    }
#define FLUSH_BLOCK(s, eof)                                                                       \
    {                                                                                             \
        FLUSH_BLOCK_ONLY(s, eof);                                                                 \
        if ((s)->strm->avail_out == 0)                                                            \
            return (eof) ? finish_started : need_more;                                            \
    }

#define ERR_RETURN(strm, err) return ((strm)->msg = z_errmsg[2 - (err)], (err))

void putShortMSB(deflate_state_s *s, uInt b);
void flush_pending(z_stream_s *strm);
void lm_init(deflate_state_s *s);
void fill_window(deflate_state_s *s);
uInt longest_match(deflate_state_s *s, uInt cur_match);
int read_buf(z_stream_s *strm, Byte *buf, uInt size);
int deflate_stored(deflate_state_s *s, int flush);
int deflate_fast(deflate_state_s *s, int flush);
int deflate_slow(deflate_state_s *s, int flush);

int deflateInit2_(z_stream_s *strm, int level, int method, int windowBits, int memLevel, int strategy,
                  const char *version, int stream_size);
int deflateReset(z_stream_s *strm);
int deflateEnd(z_stream_s *strm);
int deflate(z_stream_s *strm, int flush);
void inflate_blocks_reset(inflate_blocks_state_s *s, z_stream_s *z, uLong *c);

// 0x080c79fe  size=15  FUN_080c79fe  status=translated  naming=unknown
// Reads a byte at +0x8c of the model; its meaning is not established, name is descriptive only.
unsigned char XModel_GetFlags(const XModel *model)
{
    return model->byte8c;
}

// 0x080c7a5e  size=64  FUN_080c7a5e  status=translated  naming=inferred
// Caller 0x080bad96 prints "model '%s' lod %i has %i surfaces" with the returned value; LOD record at model+4+lod*0x14.
int XModel_GetLodSurfaces(XModel *model, unsigned int *outLeadingWord, int lod, unsigned int **outSurfRecords)
{
    XModelLod *info = &model->lod[lod];
    *outLeadingWord = info->surfData[0];
    *outSurfRecords = info->surfData + 1;
    return (int)info->numSurfs;
}

// 0x080c7a9e  size=12  FUN_080c7a9e  status=translated  naming=inferred
// Short at +0x7c is the loop bound for the LOD tables walked by 0x080c7ae2 and 0x080c7b32.
int XModel_GetNumLods(const XModel *model)
{
    return (int)model->numLods;
}

// 0x080c7aaa  size=13  FUN_080c7aaa  status=translated  naming=unknown
// Returns the leading short of the record the object's first word points at (caller 0x080c59da).
int XModelRef_GetHeaderShort(const XModelRef *ref)
{
    return (int)ref->header->firstShort;
}

// 0x080c7ab8  size=13  FUN_080c7ab8  status=translated  naming=unknown
// Address 0x44 bytes into the block the object's first word points at (callers 0x08058194, 0x0805829e).
unsigned char *XModelRef_GetBlock44(const XModelRef *ref)
{
    return (unsigned char *)ref->header + 0x44;
}

// 0x080c7ac6  size=27  FUN_080c7ac6  status=translated  naming=unknown
// Like XModelRef_GetBlock44 plus index/2 bytes.
unsigned char *XModelRef_GetBlock44Entry(const XModelRef *ref, unsigned int index)
{
    return (unsigned char *)ref->header + 0x44 + (index >> 1);
}

// 0x080c7ae2  size=80  FUN_080c7ae2  status=translated  naming=inferred
// Distance cutoff of the last LOD: global override table entry when valid, else model lod[n-1].dist.
float XModel_GetLastLodDistance(const XModel *model)
{
    int last = XModel_GetNumLods(model) - 1;
    float dist;
    if (g_xmodelLodDistanceOverrides[last].valid == 0)
        dist = model->lod[last].dist;
    else
        dist = g_xmodelLodDistanceOverrides[last].dist;
    return dist;
}

// 0x080c7b32  size=164  FUN_080c7b32  status=translated  naming=inferred
// First LOD whose cutoff is 0 or beyond the given distance; -1 if none.
int XModel_GetLodIndexForDistance(const XModel *model, float distance)
{
    int numLods = XModel_GetNumLods(model);
    int i;
    for (i = 0; i < numLods; i++) {
        float cutoff;
        if (g_xmodelLodDistanceOverrides[i].valid == 0)
            cutoff = model->lod[i].dist;
        else
            cutoff = g_xmodelLodDistanceOverrides[i].dist;
        if (cutoff == 0.0f || distance < cutoff)
            return i;
    }
    return -1;
}

// 0x080c7bd6  size=43  FUN_080c7bd6  status=translated  naming=inferred
// Stores an override distance for a LOD; valid flag is (dist >= 0), i.e. cleared for negative or NaN.
void XModel_SetLodDistanceOverride(int lod, float dist)
{
    g_xmodelLodDistanceOverrides[lod].dist = dist;
    g_xmodelLodDistanceOverrides[lod].valid = (0.0f <= dist);
}

// 0x080c7c04  size=587  FUN_080c7c04  status=translated  naming=inferred
// zlib adler32 (BASE 65521, NMAX 5552, 16-way unrolled); sibling of the "1.1.4" strings below.
uLong adler32(uLong adler, const Byte *buf, uInt len)
{
    uLong s1 = adler & 0xffff;
    uLong s2 = adler >> 16;
    if (buf == 0)
        return 1;
    while (len != 0) {
        uInt k = len < 5552 ? len : 5552;
        len -= k;
        while (k >= 16) {
            s1 += buf[0];  s2 += s1;
            s1 += buf[1];  s2 += s1;
            s1 += buf[2];  s2 += s1;
            s1 += buf[3];  s2 += s1;
            s1 += buf[4];  s2 += s1;
            s1 += buf[5];  s2 += s1;
            s1 += buf[6];  s2 += s1;
            s1 += buf[7];  s2 += s1;
            s1 += buf[8];  s2 += s1;
            s1 += buf[9];  s2 += s1;
            s1 += buf[10]; s2 += s1;
            s1 += buf[11]; s2 += s1;
            s1 += buf[12]; s2 += s1;
            s1 += buf[13]; s2 += s1;
            s1 += buf[14]; s2 += s1;
            s1 += buf[15]; s2 += s1;
            buf += 16;
            k -= 16;
        }
        while (k != 0) {
            s1 += *buf++;
            s2 += s1;
            k--;
        }
        s1 %= 65521;
        s2 %= 65521;
    }
    return (s2 << 16) | s1;
}

// 0x080c7e50  size=225  FUN_080c7e50  status=translated  naming=inferred
// zlib compress2: string "1.1.4" and deflateInit_/deflate(Z_FINISH)/deflateEnd call sequence.
int compress2(Byte *dest, uLong *destLen, const Byte *source, uLong sourceLen, int level);
int deflateInit_(z_stream_s *strm, int level, const char *version, int stream_size);

int compress2(Byte *dest, uLong *destLen, const Byte *source, uLong sourceLen, int level)
{
    z_stream_s stream;
    int err;

    stream.next_in = (byte *)source;
    stream.avail_in = (uInt)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen)
        return Z_BUF_ERROR;

    stream.zalloc = 0;
    stream.zfree = 0;
    stream.opaque = 0;

    err = deflateInit_(&stream, level, ZLIB_VERSION_STRING, (int)sizeof(z_stream_s));
    if (err != Z_OK)
        return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}

// 0x080c7f31  size=48  FUN_080c7f31  status=translated  naming=inferred
// zlib compress: compress2 with Z_DEFAULT_COMPRESSION (-1).
int compress(Byte *dest, uLong *destLen, const Byte *source, uLong sourceLen)
{
    return compress2(dest, destLen, source, sourceLen, -1);
}

// 0x080c7f64  size=72  FUN_080c7f64  status=translated  naming=inferred
// zlib deflateInit_: deflateInit2_(level, Z_DEFLATED, MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY).
int deflateInit_(z_stream_s *strm, int level, const char *version, int stream_size)
{
    return deflateInit2_(strm, level, 8, 15, 8, 0, version, stream_size);
}

// 0x080c7fac  size=818  FUN_080c7fac  status=translated  naming=inferred
// zlib deflateInit2_: version/size check (0x38), 0x16b8-byte state, "insufficient memory" failure path.
int deflateInit2_(z_stream_s *strm, int level, int method, int windowBits, int memLevel, int strategy,
                  const char *version, int stream_size)
{
    deflate_state_s *s;
    int noheader = 0;
    uInt *overlay;

    if (version == 0 || version[0] != zlib_version[0] || stream_size != (int)sizeof(z_stream_s))
        return Z_VERSION_ERROR;
    if (strm == 0)
        return Z_STREAM_ERROR;

    strm->msg = 0;
    if (strm->zalloc == 0) {
        strm->zalloc = zcalloc;
        strm->opaque = 0;
    }
    if (strm->zfree == 0)
        strm->zfree = zcfree;

    if (level == -1)
        level = 6;

    if (windowBits < 0) {
        noheader = 1;
        windowBits = -windowBits;
    }
    if (memLevel < 1 || memLevel > 9 || method != 8 || windowBits < 9 || windowBits > 15 || level < 0 ||
        level > 9 || strategy < 0 || strategy > 2)
        return Z_STREAM_ERROR;

    s = (deflate_state_s *)strm->zalloc(strm->opaque, 1, sizeof(deflate_state_s));
    if (s == 0)
        return Z_MEM_ERROR;
    DEFLATE_STATE(strm) = s;
    s->strm = strm;

    s->noheader = noheader;
    s->w_bits = windowBits;
    s->w_size = 1 << s->w_bits;
    s->w_mask = s->w_size - 1;

    s->hash_bits = memLevel + 7;
    s->hash_size = 1 << s->hash_bits;
    s->hash_mask = s->hash_size - 1;
    s->hash_shift = (s->hash_bits + MIN_MATCH - 1) / MIN_MATCH;

    s->window = (Byte *)strm->zalloc(strm->opaque, s->w_size, 2);
    s->prev = (ush *)strm->zalloc(strm->opaque, s->w_size, 2);
    s->head = (ush *)strm->zalloc(strm->opaque, s->hash_size, 2);

    s->lit_bufsize = 1 << (memLevel + 6);

    overlay = (uInt *)strm->zalloc(strm->opaque, s->lit_bufsize, 4);
    s->pending_buf = (Byte *)overlay;
    s->pending_buf_size = (uLong)s->lit_bufsize * 4;

    if (s->window == 0 || s->prev == 0 || s->head == 0 || s->pending_buf == 0) {
        strm->msg = z_errmsg[6];
        deflateEnd(strm);
        return Z_MEM_ERROR;
    }
    s->d_buf = (ush *)((Byte *)overlay + (s->lit_bufsize & ~1u));
    s->l_buf = s->pending_buf + s->lit_bufsize * 3;

    s->level = level;
    s->strategy = strategy;
    s->method = (Byte)method;

    return deflateReset(strm);
}

// 0x080c82de  size=460  FUN_080c82de  status=translated  naming=inferred
// zlib deflateSetDictionary: adler32 of dictionary, window preload and hash priming.
int deflateSetDictionary(z_stream_s *strm, const Byte *dictionary, uInt dictLength)
{
    deflate_state_s *s;
    uInt length = dictLength;
    uInt n;
    uInt hash_head;

    if (strm == 0 || DEFLATE_STATE(strm) == 0 || dictionary == 0 || DEFLATE_STATE(strm)->status != INIT_STATE)
        return Z_STREAM_ERROR;

    s = DEFLATE_STATE(strm);
    strm->adler = adler32(strm->adler, dictionary, dictLength);

    if (length < MIN_MATCH)
        return Z_OK;
    if (length > MAX_DIST(s)) {
        length = MAX_DIST(s);
        dictionary += dictLength - length;
    }
    memcpy(s->window, dictionary, length);
    s->strstart = length;
    s->block_start = (long)length;

    s->ins_h = s->window[0];
    UPDATE_HASH(s, s->ins_h, s->window[1]);
    for (n = 0; n <= length - MIN_MATCH; n++) {
        INSERT_STRING(s, n, hash_head);
    }
    return Z_OK;
}

// 0x080c84aa  size=235  FUN_080c84aa  status=translated  naming=inferred
// zlib deflateReset: zeroes totals, Z_UNKNOWN data_type, calls tree init then lm_init.
int deflateReset(z_stream_s *strm)
{
    deflate_state_s *s;

    if (strm == 0 || DEFLATE_STATE(strm) == 0 || strm->zalloc == 0 || strm->zfree == 0)
        return Z_STREAM_ERROR;

    strm->total_out = 0;
    strm->total_in = 0;
    strm->msg = 0;
    strm->data_type = 2;

    s = DEFLATE_STATE(strm);
    s->pending = 0;
    s->pending_out = s->pending_buf;

    if (s->noheader < 0)
        s->noheader = 0;
    s->status = s->noheader ? BUSY_STATE : INIT_STATE;
    strm->adler = 1;
    s->last_flush = Z_NO_FLUSH;

    _tr_init(s);
    lm_init(s);
    return Z_OK;
}

// 0x080c8595  size=331  FUN_080c8595  status=translated  naming=inferred
// zlib deflateParams: flushes with Z_PARTIAL_FLUSH when the compress function changes and input was consumed.
int deflateParams(z_stream_s *strm, int level, int strategy)
{
    deflate_state_s *s;
    int err = Z_OK;

    if (strm == 0 || DEFLATE_STATE(strm) == 0)
        return Z_STREAM_ERROR;
    s = DEFLATE_STATE(strm);

    if (level == -1)
        level = 6;
    if (level < 0 || level > 9 || strategy < 0 || strategy > 2)
        return Z_STREAM_ERROR;

    if (configuration_table[s->level].func != configuration_table[level].func && strm->total_in != 0)
        err = deflate(strm, Z_PARTIAL_FLUSH);

    if (s->level != level) {
        s->level = level;
        s->max_lazy_match = configuration_table[level].max_lazy;
        s->good_match = configuration_table[level].good_length;
        s->nice_match = configuration_table[level].nice_length;
        s->max_chain_length = configuration_table[level].max_chain;
    }
    s->strategy = strategy;
    return err;
}

// 0x080c86e0  size=57  FUN_080c86e0  status=translated  naming=inferred
// zlib putShortMSB: appends a 16-bit value big-endian to the pending buffer.
void putShortMSB(deflate_state_s *s, uInt b)
{
    s->pending_buf[s->pending++] = (Byte)(b >> 8);
    s->pending_buf[s->pending++] = (Byte)(b & 0xff);
}

// 0x080c8719  size=206  FUN_080c8719  status=translated  naming=inferred
// zlib flush_pending: copy pending bytes into next_out, bounded by avail_out.
void flush_pending(z_stream_s *strm)
{
    deflate_state_s *s = DEFLATE_STATE(strm);
    uInt len = s->pending;

    if (len > strm->avail_out)
        len = strm->avail_out;
    if (len == 0)
        return;

    memcpy(strm->next_out, s->pending_out, len);
    strm->next_out += len;
    s->pending_out += len;
    strm->total_out += len;
    strm->avail_out -= len;
    s->pending -= len;
    if (s->pending == 0)
        s->pending_out = s->pending_buf;
}

// 0x080c87e7  size=1001  FUN_080c87e7  status=translated  naming=inferred
// zlib deflate(): states 0x2a/0x71/0x29a, "stream error"/"buffer error" messages, level function dispatch.
int deflate(z_stream_s *strm, int flush)
{
    int old_flush;
    deflate_state_s *s;

    if (strm == 0 || DEFLATE_STATE(strm) == 0 || flush > Z_FINISH || flush < 0)
        return Z_STREAM_ERROR;
    s = DEFLATE_STATE(strm);

    if (strm->next_out == 0 || (strm->next_in == 0 && strm->avail_in != 0) ||
        (s->status == FINISH_STATE && flush != Z_FINISH))
        ERR_RETURN(strm, Z_STREAM_ERROR);
    if (strm->avail_out == 0)
        ERR_RETURN(strm, Z_BUF_ERROR);

    s->strm = strm;
    old_flush = s->last_flush;
    s->last_flush = flush;

    if (s->status == INIT_STATE) {
        uInt header = (8 + ((s->w_bits - 8) << 4)) << 8;
        uInt level_flags = (s->level - 1) >> 1;

        if (level_flags > 3)
            level_flags = 3;
        header |= (level_flags << 6);
        if (s->strstart != 0)
            header |= 0x20;
        s->status = BUSY_STATE;
        header += 31 - (header % 31);
        putShortMSB(s, header);

        if (s->strstart != 0) {
            putShortMSB(s, (uInt)(strm->adler >> 16));
            putShortMSB(s, (uInt)(strm->adler & 0xffff));
        }
        strm->adler = 1;
    }

    if (s->pending != 0) {
        flush_pending(strm);
        if (strm->avail_out == 0) {
            s->last_flush = -1;
            return Z_OK;
        }
    } else if (strm->avail_in == 0 && flush <= old_flush && flush != Z_FINISH) {
        ERR_RETURN(strm, Z_BUF_ERROR);
    }

    if (s->status == FINISH_STATE && strm->avail_in != 0)
        ERR_RETURN(strm, Z_BUF_ERROR);

    if (strm->avail_in != 0 || s->lookahead != 0 || (flush != Z_NO_FLUSH && s->status != FINISH_STATE)) {
        int bstate = configuration_table[s->level].func(s, flush);

        if (bstate == finish_started || bstate == finish_done)
            s->status = FINISH_STATE;
        if (bstate == need_more || bstate == finish_started) {
            if (strm->avail_out == 0)
                s->last_flush = -1;
            return Z_OK;
        }
        if (bstate == block_done) {
            if (flush == Z_PARTIAL_FLUSH) {
                tr_align(s);
            } else {
                tr_stored_block(s, (byte *)0, 0L, 0);
                if (flush == Z_FULL_FLUSH) {
                    /* CLEAR_HASH */
                    s->head[s->hash_size - 1] = 0;
                    memset(s->head, 0, (s->hash_size - 1) * sizeof(*s->head));
                }
            }
            flush_pending(strm);
            if (strm->avail_out == 0) {
                s->last_flush = -1;
                return Z_OK;
            }
        }
    }

    if (flush != Z_FINISH)
        return Z_OK;
    if (s->noheader)
        return Z_STREAM_END;

    putShortMSB(s, (uInt)(strm->adler >> 16));
    putShortMSB(s, (uInt)(strm->adler & 0xffff));
    flush_pending(strm);
    s->noheader = -1;
    return s->pending != 0 ? Z_OK : Z_STREAM_END;
}

// 0x080c8bd0  size=316  FUN_080c8bd0  status=translated  naming=inferred
// zlib deflateEnd: frees pending_buf/head/prev/window/state; Z_DATA_ERROR when status was BUSY_STATE.
int deflateEnd(z_stream_s *strm)
{
    int status;

    if (strm == 0 || DEFLATE_STATE(strm) == 0)
        return Z_STREAM_ERROR;

    status = DEFLATE_STATE(strm)->status;
    if (status != INIT_STATE && status != BUSY_STATE && status != FINISH_STATE)
        return Z_STREAM_ERROR;

    if (DEFLATE_STATE(strm)->pending_buf)
        strm->zfree(strm->opaque, DEFLATE_STATE(strm)->pending_buf);
    if (DEFLATE_STATE(strm)->head)
        strm->zfree(strm->opaque, DEFLATE_STATE(strm)->head);
    if (DEFLATE_STATE(strm)->prev)
        strm->zfree(strm->opaque, DEFLATE_STATE(strm)->prev);
    if (DEFLATE_STATE(strm)->window)
        strm->zfree(strm->opaque, DEFLATE_STATE(strm)->window);

    strm->zfree(strm->opaque, DEFLATE_STATE(strm));
    DEFLATE_STATE(strm) = 0;

    return status == BUSY_STATE ? Z_DATA_ERROR : Z_OK;
}

// 0x080c8d0c  size=732  FUN_080c8d0c  status=translated  naming=inferred
// zlib deflateCopy: struct copy of z_stream_s (14 words) and deflate_state_s (0x16b8), fresh buffers, rebased pointers.
int deflateCopy(z_stream_s *dest, z_stream_s *source)
{
    deflate_state_s *ds;
    deflate_state_s *ss;
    uInt *overlay;

    if (source == 0 || dest == 0 || DEFLATE_STATE(source) == 0)
        return Z_STREAM_ERROR;

    ss = DEFLATE_STATE(source);
    *dest = *source;

    ds = (deflate_state_s *)dest->zalloc(dest->opaque, 1, sizeof(deflate_state_s));
    if (ds == 0)
        return Z_MEM_ERROR;
    DEFLATE_STATE(dest) = ds;
    memcpy(ds, ss, sizeof(deflate_state_s));
    ds->strm = dest;

    ds->window = (Byte *)dest->zalloc(dest->opaque, ds->w_size, 2);
    ds->prev = (ush *)dest->zalloc(dest->opaque, ds->w_size, 2);
    ds->head = (ush *)dest->zalloc(dest->opaque, ds->hash_size, 2);
    overlay = (uInt *)dest->zalloc(dest->opaque, ds->lit_bufsize, 4);
    ds->pending_buf = (Byte *)overlay;

    if (ds->window == 0 || ds->prev == 0 || ds->head == 0 || ds->pending_buf == 0) {
        deflateEnd(dest);
        return Z_MEM_ERROR;
    }
    memcpy(ds->window, ss->window, ds->w_size * 2);
    memcpy(ds->prev, ss->prev, ds->w_size * 2);
    memcpy(ds->head, ss->head, ds->hash_size * 2);
    memcpy(ds->pending_buf, ss->pending_buf, (uInt)ds->pending_buf_size);

    ds->pending_out = ds->pending_buf + (ss->pending_out - ss->pending_buf);
    ds->d_buf = (ush *)((Byte *)overlay + (ds->lit_bufsize & ~1u));
    ds->l_buf = ds->pending_buf + ds->lit_bufsize * 3;

    ds->l_desc.dyn_tree = ds->dyn_ltree;
    ds->d_desc.dyn_tree = ds->dyn_dtree;
    ds->bl_desc.dyn_tree = ds->bl_tree;
    return Z_OK;
}

// 0x080c8fe8  size=180  FUN_080c8fe8  status=translated  naming=inferred
// zlib read_buf: copies up to size bytes from next_in, updating adler32 unless noheader.
int read_buf(z_stream_s *strm, Byte *buf, uInt size)
{
    uInt len = strm->avail_in;

    if (len > size)
        len = size;
    if (len == 0)
        return 0;

    strm->avail_in -= len;
    if (!DEFLATE_STATE(strm)->noheader)
        strm->adler = adler32(strm->adler, strm->next_in, len);
    memcpy(buf, strm->next_in, len);
    strm->next_in += len;
    strm->total_in += len;
    return (int)len;
}

// 0x080c909c  size=269  FUN_080c909c  status=translated  naming=inferred
// zlib lm_init: window_size, CLEAR_HASH, level parameters from configuration_table, zero match state.
void lm_init(deflate_state_s *s)
{
    s->window_size = (uLong)2L * s->w_size;

    /* CLEAR_HASH */
    s->head[s->hash_size - 1] = 0;
    memset(s->head, 0, (s->hash_size - 1) * sizeof(*s->head));

    s->max_lazy_match = configuration_table[s->level].max_lazy;
    s->good_match = configuration_table[s->level].good_length;
    s->nice_match = configuration_table[s->level].nice_length;
    s->max_chain_length = configuration_table[s->level].max_chain;

    s->strstart = 0;
    s->block_start = 0L;
    s->lookahead = 0;
    s->prev_length = MIN_MATCH - 1;
    s->match_length = MIN_MATCH - 1;
    s->match_available = 0;
    s->ins_h = 0;
}

// 0x080c91a9  size=650  FUN_080c91a9  status=translated  naming=inferred
// zlib longest_match: hash chain walk with 8-way unrolled compare, MAX_MATCH 258, scan[2] skipped.
uInt longest_match(deflate_state_s *s, uInt cur_match)
{
    uInt chain_length = s->max_chain_length;
    Byte *scan = s->window + s->strstart;
    Byte *match;
    int len;
    int best_len = (int)s->prev_length;
    int nice_match = s->nice_match;
    uInt limit = s->strstart > MAX_DIST(s) ? s->strstart - MAX_DIST(s) : 0;
    ush *prev = s->prev;
    uInt wmask = s->w_mask;
    Byte *strend = s->window + s->strstart + MAX_MATCH;
    Byte scan_end1 = scan[best_len - 1];
    Byte scan_end = scan[best_len];

    if (s->prev_length >= s->good_match)
        chain_length >>= 2;
    if ((uInt)nice_match > s->lookahead)
        nice_match = (int)s->lookahead;

    do {
        match = s->window + cur_match;

        if (match[best_len] != scan_end || match[best_len - 1] != scan_end1 || *match != *scan ||
            *++match != scan[1])
            continue;

        scan += 2;
        match++;
        do {
        } while (*++scan == *++match && *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match && scan < strend);

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;

        if (len > best_len) {
            s->match_start = cur_match;
            best_len = len;
            if (len >= nice_match)
                break;
            scan_end1 = scan[best_len - 1];
            scan_end = scan[best_len];
        }
    } while ((cur_match = prev[cur_match & wmask]) > limit && --chain_length != 0);

    if ((uInt)best_len <= s->lookahead)
        return (uInt)best_len;
    return s->lookahead;
}

// 0x080c9433  size=593  FUN_080c9433  status=translated  naming=inferred
// zlib fill_window: slides the window by w_size when strstart >= 2*w_size - MIN_LOOKAHEAD, then read_buf.
void fill_window(deflate_state_s *s)
{
    uInt n, m;
    ush *p;
    uInt more;
    uInt wsize = s->w_size;

    do {
        more = (uInt)(s->window_size - (uLong)s->lookahead - (uLong)s->strstart);

        if (more == 0 && s->strstart == 0 && s->lookahead == 0) {
            more = wsize;
        } else if (more == (uInt)(-1)) {
            more--;
        } else if (s->strstart >= wsize + MAX_DIST(s)) {
            memcpy(s->window, s->window + wsize, wsize);
            s->match_start -= wsize;
            s->strstart -= wsize;
            s->block_start -= (long)wsize;

            n = s->hash_size;
            p = &s->head[n];
            do {
                m = *--p;
                *p = (ush)(m >= wsize ? m - wsize : 0);
            } while (--n);

            n = wsize;
            p = &s->prev[n];
            do {
                m = *--p;
                *p = (ush)(m >= wsize ? m - wsize : 0);
            } while (--n);
            more += wsize;
        }
        if (s->strm->avail_in == 0)
            return;

        n = (uInt)read_buf(s->strm, s->window + s->strstart + s->lookahead, more);
        s->lookahead += n;

        if (s->lookahead >= MIN_MATCH) {
            s->ins_h = s->window[s->strstart];
            UPDATE_HASH(s, s->ins_h, s->window[s->strstart + 1]);
        }
    } while (s->lookahead < MIN_LOOKAHEAD && s->strm->avail_in != 0);
}

// 0x080c9684  size=663  FUN_080c9684  status=translated  naming=inferred
// zlib deflate_stored (level 0): max block 0xffff or pending_buf_size-5.
int deflate_stored(deflate_state_s *s, int flush)
{
    uLong max_block_size = 0xffff;
    uLong max_start;

    if (max_block_size > s->pending_buf_size - 5)
        max_block_size = s->pending_buf_size - 5;

    for (;;) {
        if (s->lookahead <= 1) {
            fill_window(s);
            if (s->lookahead == 0 && flush == Z_NO_FLUSH)
                return need_more;
            if (s->lookahead == 0)
                break;
        }
        s->strstart += s->lookahead;
        s->lookahead = 0;

        max_start = s->block_start + max_block_size;
        if (s->strstart == 0 || (uLong)s->strstart >= max_start) {
            s->lookahead = (uInt)(s->strstart - max_start);
            s->strstart = (uInt)max_start;
            FLUSH_BLOCK(s, 0);
        }
        if (s->strstart - (uInt)s->block_start >= MAX_DIST(s)) {
            FLUSH_BLOCK(s, 0);
        }
    }
    FLUSH_BLOCK(s, flush == Z_FINISH);
    return flush == Z_FINISH ? finish_done : block_done;
}

// 0x080c991b  size=1471  FUN_080c991b  status=translated  naming=inferred
// zlib deflate_fast (levels 1-3): inline tally, max_insert_length is max_lazy_match.
int deflate_fast(deflate_state_s *s, int flush)
{
    uInt hash_head = 0;
    int bflush;

    for (;;) {
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)
                return need_more;
            if (s->lookahead == 0)
                break;
        }
        if (s->lookahead >= MIN_MATCH) {
            INSERT_STRING(s, s->strstart, hash_head);
        }
        if (hash_head != 0 && s->strstart - hash_head <= MAX_DIST(s)) {
            if (s->strategy != Z_HUFFMAN_ONLY)
                s->match_length = longest_match(s, hash_head);
        }
        if (s->match_length >= MIN_MATCH) {
            TALLY_DIST(s, s->strstart - s->match_start, s->match_length - MIN_MATCH, bflush);

            s->lookahead -= s->match_length;

            if (s->match_length <= s->max_lazy_match && s->lookahead >= MIN_MATCH) {
                s->match_length--;
                do {
                    s->strstart++;
                    INSERT_STRING(s, s->strstart, hash_head);
                } while (--s->match_length != 0);
                s->strstart++;
            } else {
                s->strstart += s->match_length;
                s->match_length = 0;
                s->ins_h = s->window[s->strstart];
                UPDATE_HASH(s, s->ins_h, s->window[s->strstart + 1]);
            }
        } else {
            TALLY_LIT(s, s->window[s->strstart], bflush);
            s->lookahead--;
            s->strstart++;
        }
        if (bflush)
            FLUSH_BLOCK(s, 0);
    }
    FLUSH_BLOCK(s, flush == Z_FINISH);
    return flush == Z_FINISH ? finish_done : block_done;
}

// 0x080c9eda  size=1861  FUN_080c9eda  status=translated  naming=inferred
// zlib deflate_slow (levels 4-9): lazy matching, TOO_FAR 4096 rule, hash_head persists across iterations.
int deflate_slow(deflate_state_s *s, int flush)
{
    uInt hash_head = 0;
    int bflush;

    for (;;) {
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)
                return need_more;
            if (s->lookahead == 0)
                break;
        }
        if (s->lookahead >= MIN_MATCH) {
            INSERT_STRING(s, s->strstart, hash_head);
        }

        s->prev_length = s->match_length;
        s->prev_match = s->match_start;
        s->match_length = MIN_MATCH - 1;

        if (hash_head != 0 && s->prev_length < s->max_lazy_match &&
            s->strstart - hash_head <= MAX_DIST(s)) {
            if (s->strategy != Z_HUFFMAN_ONLY)
                s->match_length = longest_match(s, hash_head);

            if (s->match_length <= 5 &&
                (s->strategy == Z_FILTERED ||
                 (s->match_length == MIN_MATCH && s->strstart - s->match_start > TOO_FAR)))
                s->match_length = MIN_MATCH - 1;
        }
        if (s->prev_length >= MIN_MATCH && s->match_length <= s->prev_length) {
            uInt max_insert = s->strstart + s->lookahead - MIN_MATCH;

            TALLY_DIST(s, s->strstart - 1 - s->prev_match, s->prev_length - MIN_MATCH, bflush);

            s->lookahead -= s->prev_length - 1;
            s->prev_length -= 2;
            do {
                if (++s->strstart <= max_insert) {
                    INSERT_STRING(s, s->strstart, hash_head);
                }
            } while (--s->prev_length != 0);
            s->match_available = 0;
            s->match_length = MIN_MATCH - 1;
            s->strstart++;

            if (bflush)
                FLUSH_BLOCK(s, 0);
        } else if (s->match_available) {
            TALLY_LIT(s, s->window[s->strstart - 1], bflush);
            if (bflush) {
                FLUSH_BLOCK_ONLY(s, 0);
            }
            s->strstart++;
            s->lookahead--;
            if (s->strm->avail_out == 0)
                return need_more;
        } else {
            s->match_available = 1;
            s->strstart++;
            s->lookahead--;
        }
    }
    if (s->match_available) {
        TALLY_LIT(s, s->window[s->strstart - 1], bflush);
        s->match_available = 0;
    }
    FLUSH_BLOCK(s, flush == Z_FINISH);
    return flush == Z_FINISH ? finish_done : block_done;
}

// 0x080ca620  size=208  FUN_080ca620  status=translated  naming=inferred
// zlib inflate_blocks_reset: frees btree lengths / codes state, rewinds window, restarts checksum.
void inflate_blocks_reset(inflate_blocks_state_s *s, z_stream_s *z, uLong *c)
{
    if (c != 0)
        *c = s->check;
    if (s->mode == IB_BTREE || s->mode == IB_DTREE)
        z->zfree(z->opaque, s->sub.trees.blens);
    if (s->mode == IB_CODES)
        inflate_codes_free(s->sub.decode.codes, z);
    s->mode = IB_TYPE;
    s->bitk = 0;
    s->bitb = 0;
    s->read = s->write = s->window;
    if (s->checkfn != 0)
        z->adler = s->check = (*s->checkfn)(0L, (const Byte *)0, 0);
}

// 0x080ca6f0  size=322  FUN_080ca6f0  status=translated  naming=inferred
// zlib inflate_blocks_new: 0x40-byte state, 8*0x5a0 hufts (MANY=1440), w-byte window.
inflate_blocks_state_s *inflate_blocks_new(z_stream_s *z, check_func c, uInt w)
{
    inflate_blocks_state_s *s;

    s = (inflate_blocks_state_s *)z->zalloc(z->opaque, 1, sizeof(inflate_blocks_state_s));
    if (s == 0)
        return s;
    s->hufts = (inflate_huft *)z->zalloc(z->opaque, sizeof(inflate_huft), 1440);
    if (s->hufts == 0) {
        z->zfree(z->opaque, s);
        return 0;
    }
    s->window = (Byte *)z->zalloc(z->opaque, 1, w);
    if (s->window == 0) {
        z->zfree(z->opaque, s->hufts);
        z->zfree(z->opaque, s);
        return 0;
    }
    s->end = s->window + w;
    s->checkfn = c;
    s->mode = IB_TYPE;
    inflate_blocks_reset(s, z, 0);
    return s;
}

/* inflate_blocks macros: local copies of bit buffer / input / window state. */
#define LOAD                                                                                      \
    {                                                                                             \
        p = z->next_in;                                                                           \
        n = z->avail_in;                                                                          \
        b = s->bitb;                                                                              \
        k = s->bitk;                                                                              \
        q = s->write;                                                                             \
        m = (uInt)(q < s->read ? s->read - q - 1 : s->end - q);                                   \
    }
#define UPDATE                                                                                    \
    {                                                                                             \
        s->bitb = b;                                                                              \
        s->bitk = k;                                                                              \
        z->avail_in = n;                                                                          \
        z->total_in += p - z->next_in;                                                            \
        z->next_in = (byte *)p;                                                                           \
        s->write = q;                                                                             \
    }
#define LEAVE                                                                                     \
    {                                                                                             \
        UPDATE return inflate_flush(s, z, r);                                                       \
    }
#define NEEDBYTE                                                                                  \
    {                                                                                             \
        if (n)                                                                                    \
            r = Z_OK;                                                                             \
        else                                                                                      \
            LEAVE                                                                                 \
    }
#define NEXTBYTE (n--, *p++)
#define NEEDBITS(j)                                                                               \
    {                                                                                             \
        while (k < (j)) {                                                                         \
            NEEDBYTE;                                                                             \
            b |= ((uLong)NEXTBYTE) << k;                                                          \
            k += 8;                                                                               \
        }                                                                                         \
    }
#define DUMPBITS(j)                                                                               \
    {                                                                                             \
        b >>= (j);                                                                                \
        k -= (j);                                                                                 \
    }
#define WAVAIL (uInt)(q < s->read ? s->read - q - 1 : s->end - q)
#define WRAP                                                                                      \
    {                                                                                             \
        if (q == s->end && s->read != s->window) {                                                \
            q = s->window;                                                                        \
            m = WAVAIL;                                                                           \
        }                                                                                         \
    }
#define FLUSH                                                                                     \
    {                                                                                             \
        s->write = q;                                                                             \
        r = inflate_flush(s, z, r);                                                                 \
        q = s->write;                                                                             \
        m = WAVAIL;                                                                               \
    }
#define NEEDOUT                                                                                   \
    {                                                                                             \
        if (m == 0) {                                                                             \
            WRAP                                                                                  \
            if (m == 0) {                                                                         \
                FLUSH                                                                             \
                WRAP                                                                              \
                if (m == 0)                                                                       \
                    LEAVE                                                                         \
            }                                                                                     \
        }                                                                                         \
        r = Z_OK;                                                                                 \
    }

// 0x080ca832  size=5191  FUN_080ca832  status=translated  naming=inferred
// zlib inflate_blocks: strings "invalid block type", "invalid stored block lengths",
// "too many length or distance symbols", "invalid bit length repeat"; mode values 0..9.
int inflate_blocks(inflate_blocks_state_s *s, z_stream_s *z, int r)
{
    uInt t;
    uLong b;
    uInt k;
    const Byte *p;
    uInt n;
    Byte *q;
    uInt m;

    LOAD

    for (;;) {
        switch (s->mode) {
        case IB_TYPE:
            NEEDBITS(3);
            t = (uInt)b & 7;
            s->last = t & 1;
            switch (t >> 1) {
            case 0:
                DUMPBITS(3);
                t = k & 7;
                DUMPBITS(t);
                s->mode = IB_LENS;
                break;
            case 1: {
                uInt bl, bd;
                inflate_huft *tl, *td;

                inflate_trees_fixed(&bl, &bd, &tl, &td);
                s->sub.decode.codes = inflate_codes_new(bl, bd, tl, td, z);
                if (s->sub.decode.codes == 0) {
                    r = Z_MEM_ERROR;
                    LEAVE
                }
                DUMPBITS(3);
                s->mode = IB_CODES;
                break;
            }
            case 2:
                DUMPBITS(3);
                s->mode = IB_TABLE;
                break;
            case 3:
                DUMPBITS(3);
                s->mode = IB_BAD;
                z->msg = "invalid block type";
                r = Z_DATA_ERROR;
                LEAVE
            }
            break;
        case IB_LENS:
            NEEDBITS(32);
            if ((((~b) >> 16) & 0xffff) != (b & 0xffff)) {
                s->mode = IB_BAD;
                z->msg = "invalid stored block lengths";
                r = Z_DATA_ERROR;
                LEAVE
            }
            s->sub.left = (uInt)b & 0xffff;
            b = k = 0;
            s->mode = s->sub.left ? IB_STORED : (s->last ? IB_DRY : IB_TYPE);
            break;
        case IB_STORED:
            if (n == 0)
                LEAVE
            NEEDOUT
            t = s->sub.left;
            if (t > n)
                t = n;
            if (t > m)
                t = m;
            memcpy(q, p, t);
            p += t;
            n -= t;
            q += t;
            m -= t;
            if ((s->sub.left -= t) != 0)
                break;
            s->mode = s->last ? IB_DRY : IB_TYPE;
            break;
        case IB_TABLE:
            NEEDBITS(14);
            s->sub.trees.table = t = (uInt)b & 0x3fff;
            if ((t & 0x1f) > 29 || ((t >> 5) & 0x1f) > 29) {
                s->mode = IB_BAD;
                z->msg = "too many length or distance symbols";
                r = Z_DATA_ERROR;
                LEAVE
            }
            t = 258 + (t & 0x1f) + ((t >> 5) & 0x1f);
            s->sub.trees.blens = (uInt *)z->zalloc(z->opaque, t, 4);
            if (s->sub.trees.blens == 0) {
                r = Z_MEM_ERROR;
                LEAVE
            }
            DUMPBITS(14);
            s->sub.trees.index = 0;
            s->mode = IB_BTREE;
            /* fall through */
        case IB_BTREE:
            while (s->sub.trees.index < 4 + (s->sub.trees.table >> 10)) {
                NEEDBITS(3);
                s->sub.trees.blens[inflate_border[s->sub.trees.index++]] = (uInt)b & 7;
                DUMPBITS(3);
            }
            while (s->sub.trees.index < 19)
                s->sub.trees.blens[inflate_border[s->sub.trees.index++]] = 0;
            s->sub.trees.bb = 7;
            t = (uInt)inflate_trees_bits(s->sub.trees.blens, &s->sub.trees.bb, &s->sub.trees.tb, s->hufts, z);
            if ((int)t != Z_OK) {
                r = (int)t;
                if (r == Z_DATA_ERROR) {
                    z->zfree(z->opaque, s->sub.trees.blens);
                    s->mode = IB_BAD;
                }
                LEAVE
            }
            s->sub.trees.index = 0;
            s->mode = IB_DTREE;
            /* fall through */
        case IB_DTREE:
            while (t = s->sub.trees.table,
                   s->sub.trees.index < 258 + (t & 0x1f) + ((t >> 5) & 0x1f)) {
                inflate_huft *h;
                uInt i, j, c;

                t = s->sub.trees.bb;
                NEEDBITS(t);
                h = s->sub.trees.tb + ((uInt)b & inflate_mask[t]);
                t = h->bits;
                c = h->base;
                if (c < 16) {
                    DUMPBITS(t);
                    s->sub.trees.blens[s->sub.trees.index++] = c;
                } else {
                    i = c == 18 ? 7 : c - 14;
                    j = c == 18 ? 11 : 3;
                    NEEDBITS(t + i);
                    DUMPBITS(t);
                    j += (uInt)b & inflate_mask[i];
                    DUMPBITS(i);
                    i = s->sub.trees.index;
                    t = s->sub.trees.table;
                    if (i + j > 258 + (t & 0x1f) + ((t >> 5) & 0x1f) || (c == 16 && i < 1)) {
                        z->zfree(z->opaque, s->sub.trees.blens);
                        s->mode = IB_BAD;
                        z->msg = "invalid bit length repeat";
                        r = Z_DATA_ERROR;
                        LEAVE
                    }
                    c = c == 16 ? s->sub.trees.blens[i - 1] : 0;
                    do {
                        s->sub.trees.blens[i++] = c;
                    } while (--j);
                    s->sub.trees.index = i;
                }
            }
            {
                uInt bl, bd;
                inflate_huft *tl, *td;
                inflate_codes_state_s *codes;

                s->sub.trees.tb = 0;
                bl = 9;
                bd = 6;
                t = s->sub.trees.table;
                t = (uInt)inflate_trees_dynamic(257 + (t & 0x1f), 1 + ((t >> 5) & 0x1f), s->sub.trees.blens, &bl,
                                      &bd, &tl, &td, s->hufts, z);
                if (t != Z_OK) {
                    if ((int)t == Z_DATA_ERROR) {
                        z->zfree(z->opaque, s->sub.trees.blens);
                        s->mode = IB_BAD;
                    }
                    r = (int)t;
                    LEAVE
                }
                z->zfree(z->opaque, s->sub.trees.blens);
                codes = inflate_codes_new(bl, bd, tl, td, z);
                if (codes == 0) {
                    r = Z_MEM_ERROR;
                    LEAVE
                }
                s->sub.decode.codes = codes;
            }
            s->mode = IB_CODES;
            /* fall through */
        case IB_CODES:
            UPDATE
            if ((r = inflate_codes(s, z, r)) != Z_STREAM_END)
                return inflate_flush(s, z, r);
            r = Z_OK;
            inflate_codes_free(s->sub.decode.codes, z);
            LOAD
            if (!s->last) {
                s->mode = IB_TYPE;
                break;
            }
            s->mode = IB_DRY;
            /* fall through */
        case IB_DRY:
            FLUSH
            if (s->read != s->write)
                LEAVE
            s->mode = IB_DONE;
            /* fall through */
        case IB_DONE:
            r = Z_STREAM_END;
            LEAVE
        case IB_BAD:
            r = Z_DATA_ERROR;
            LEAVE
        default:
            r = Z_STREAM_ERROR;
            LEAVE
        }
    }
}

// 0x080cbc79  size=117  FUN_080cbc79  status=translated  naming=inferred
// zlib inflate_blocks_free: reset, then free window, hufts and the state.
int inflate_blocks_free(inflate_blocks_state_s *s, z_stream_s *z)
{
    inflate_blocks_reset(s, z, 0);
    z->zfree(z->opaque, s->window);
    z->zfree(z->opaque, s->hufts);
    z->zfree(z->opaque, s);
    return Z_OK;
}

// 0x080cbcee  size=65  FUN_080cbcee  status=translated  naming=inferred
// zlib inflate_set_dictionary: copy into the window and set read = write = window + n.
void inflate_set_dictionary(inflate_blocks_state_s *s, const Byte *d, uInt n)
{
    memcpy(s->window, d, n);
    s->read = s->write = s->window + n;
}

// 0x080cbd2f  size=17  FUN_080cbd2f  status=translated  naming=inferred
// zlib inflate_blocks_sync_point: true when in the stored-length state (mode == LENS).
int inflate_blocks_sync_point(inflate_blocks_state_s *s)
{
    return s->mode == IB_LENS;
}
