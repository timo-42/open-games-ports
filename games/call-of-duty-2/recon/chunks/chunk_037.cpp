#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_037_types.h"

/*
 * Chunk 037: zlib 1.1.4 deflate tree/bit-output layer, zlib uncompress(), a minizip-style
 * ZIP reader, and the tty console helpers. Every function below is translated from the
 * disassembly/pseudocode of this binary; names are zlib / minizip counterparts (inferred).
 */

/* ---- bit-output helpers ------------------------------------------------- */
/* The binary inlines zlib's put_short / send_bits / send_code macros with Buf_size 16. */

static inline void deflate_put_short(deflate_state_s *s, uint32 w)
{
    s->pending_buf[s->pending] = (byte)(w & 0xff);
    s->pending++;
    s->pending_buf[s->pending] = (byte)((w >> 8) & 0xff);
    s->pending++;
}

static inline void deflate_send_bits(deflate_state_s *s, uint32 value, int length)
{
    if (s->bi_valid > 16 - length) {
        s->bi_buf = (uint16)(s->bi_buf | (value << s->bi_valid));
        deflate_put_short(s, s->bi_buf);
        s->bi_buf = (uint16)((uint16)value >> (16 - s->bi_valid));
        s->bi_valid += length - 16;
    } else {
        s->bi_buf = (uint16)(s->bi_buf | (value << s->bi_valid));
        s->bi_valid += length;
    }
}

static inline void deflate_send_code(deflate_state_s *s, uint32 c, const ct_data *tree)
{
    deflate_send_bits(s, tree[c].fc, tree[c].dl);
}

/* zlib d_code(dist - 1) using this binary's table layout: the table base sits one byte
 * before zlib_dist_code_low, so the low half is indexed with the unmodified distance. */
static inline uint32 deflate_d_code(uint32 dist)
{
    uint32 dist_m1 = dist - 1;
    if (dist_m1 < 0x100) {
        return zlib_dist_code_low[dist];
    }
    return zlib_dist_code_high[dist_m1 >> 7];
}

/* Forward declarations for in-chunk callees defined further down. */
void bi_flush(deflate_state_s *s);
void bi_windup(deflate_state_s *s);
void copy_block(deflate_state_s *s, byte *buf, int len, int header);
void compress_block(deflate_state_s *s, const ct_data *ltree, const ct_data *dtree);
void set_data_type(deflate_state_s *s);
void tr_stored_block(deflate_state_s *s, byte *buf, uint32 stored_len, int last);
void tr_align(deflate_state_s *s);
void tr_flush_block(deflate_state_s *s, byte *buf, uint32 stored_len, int eof);
bool tr_tally(deflate_state_s *s, uint32 dist, uint32 lc);
void send_all_trees(deflate_state_s *s, int lcodes, int dcodes, int blcodes);
int build_bl_tree(deflate_state_s *s);
uint32 bi_reverse(uint32 code, int len);
int unzlocal_getShort(void *fp, uint32 *out);
int unzlocal_getLong(void *fp, uint32 *out);
int unzlocal_strcmpcasenosensitive_internal(const char *fileName1, const char *fileName2);
int unzStringFileNameCompare(const char *fileName1, const char *fileName2, int iCaseSensitivity);
uint32 unzlocal_SearchCentralDir(void *fp);
unz_s *unzReOpen(const char *path, unz_s *file_to_reopen);
unz_s *unzOpen(const char *path);
int unzClose(unz_s *file);
int unzGetGlobalInfo(unz_s *file, unz_global_info_s *pglobal_info);
void DosDateToTmuDate(uint32 ulDosDate, tm_unz_s *ptm);
int unzlocal_GetCurrentFileInfoInternal(unz_s *file, unz_file_info_s *pfile_info, uint32 *pfile_info_internal,
                                        char *szFileName, uint32 fileNameBufferSize, void *extraField,
                                        uint32 extraFieldBufferSize, char *szComment, uint32 commentBufferSize);
int unzGetCurrentFileInfo(unz_s *file, unz_file_info_s *pfile_info, char *szFileName, uint32 fileNameBufferSize,
                          void *extraField, uint32 extraFieldBufferSize, char *szComment, uint32 commentBufferSize);
int unzGoToFirstFile(unz_s *file);
int unzGoToNextFile(unz_s *file);
int unzGetOffset(unz_s *file, uint32 *pos);
int unzSetOffset(unz_s *file, uint32 pos);
int unzLocateFile(unz_s *file, const char *szFileName, int iCaseSensitivity);
int unzlocal_CheckCurrentFileCoherencyHeader(unz_s *s, uint32 *piSizeVar, uint32 *poffset_local_extrafield,
                                             uint32 *psize_local_extrafield);
int unzOpenCurrentFile(unz_s *file);
int unzReadCurrentFile(unz_s *file, void *buf, uint32 len);
int unzTellCurrentFile(unz_s *file);
int unzeof(unz_s *file);
uint32 unzReadLocalExtraField(unz_s *file, void *buf, uint32 len);
int unzCloseCurrentFile(unz_s *file);
uint32 unzGetGlobalComment(unz_s *file, char *szComment, uint32 commentBufferSize);
int Dir_HasNonDotEntries(const char *path);

/* ======================================================================== */

// 0x080d006a  size=191  FUN_080d006a  status=translated  naming=inferred
// zlib 1.1.4 trees.c build_bl_tree: scan_tree(ltree), scan_tree(dtree), build_tree(bl_desc), bl_order table at 0x815108c; no string, name from the structure
int build_bl_tree(deflate_state_s *s)
{
    int max_blindex;

    scan_tree(s, s->dyn_ltree, s->l_desc.max_code);
    scan_tree(s, s->dyn_dtree, s->d_desc.max_code);
    build_tree(s, &s->bl_desc);

    max_blindex = 18;
    while (2 < max_blindex && s->bl_tree[bl_order[max_blindex]].dl == 0) {
        max_blindex--;
    }
    /* 3 * (max_blindex + 1) + 5 + 5 + 4 */
    s->opt_len += (uint32)(max_blindex * 3 + 17);
    return max_blindex;
}

// 0x080d0129  size=1188  FUN_080d0129  status=translated  naming=inferred
// zlib 1.1.4 trees.c send_all_trees: writes lcodes-257 (5 bits), dcodes-1 (5), blcodes-4 (4), the bit-length code lengths in bl_order, then send_tree for ltree and dtree
void send_all_trees(deflate_state_s *s, int lcodes, int dcodes, int blcodes)
{
    int rank;

    deflate_send_bits(s, (uint32)(lcodes - 257), 5);
    deflate_send_bits(s, (uint32)(dcodes - 1), 5);
    deflate_send_bits(s, (uint32)(blcodes - 4), 4);
    for (rank = 0; rank < blcodes; rank++) {
        deflate_send_bits(s, s->bl_tree[bl_order[rank]].dl, 3);
    }
    send_tree(s, s->dyn_ltree, lcodes - 1);
    send_tree(s, s->dyn_dtree, dcodes - 1);
}

// 0x080d05cd  size=302  FUN_080d05cd  status=translated  naming=inferred
// zlib 1.1.4 trees.c _tr_stored_block: 3-bit block header (STORED_BLOCK << 1 | last), then copy_block with header
void tr_stored_block(deflate_state_s *s, byte *buf, uint32 stored_len, int last)
{
    deflate_send_bits(s, (uint32)last, 3);
    copy_block(s, buf, (int)stored_len, 1);
}

// 0x080d06fb  size=1134  FUN_080d06fb  status=translated  naming=inferred
// zlib 1.1.4 trees.c _tr_align: static-tree empty block (END_BLOCK code 0, length 7 from static_ltree[256]), flushed, sent twice when last_eob_len is short
void tr_align(deflate_state_s *s)
{
    deflate_send_bits(s, 2, 3);     /* STATIC_TREES << 1 */
    deflate_send_bits(s, 0, 7);     /* static_ltree[END_BLOCK]: code 0, len 7 */
    bi_flush(s);
    if (1 + s->last_eob_len + 10 - s->bi_valid < 9) {
        deflate_send_bits(s, 2, 3);
        deflate_send_bits(s, 0, 7);
        bi_flush(s);
    }
    s->last_eob_len = 7;
}

// 0x080d0b69  size=914  FUN_080d0b69  status=translated  naming=inferred
// zlib 1.1.4 trees.c _tr_flush_block: builds trees when level > 0, picks stored / static / dynamic block by length, then init_block and optional bi_windup
void tr_flush_block(deflate_state_s *s, byte *buf, uint32 stored_len, int eof)
{
    uint32 opt_lenb;
    uint32 static_lenb;
    int max_blindex = 0;

    if (s->level > 0) {
        if (s->data_type == ZLIB_DATA_UNKNOWN) {
            set_data_type(s);
        }
        build_tree(s, &s->l_desc);
        build_tree(s, &s->d_desc);
        max_blindex = build_bl_tree(s);
        opt_lenb = (s->opt_len + 10u) >> 3;
        static_lenb = (s->static_len + 10u) >> 3;
        if (static_lenb <= opt_lenb) {
            opt_lenb = static_lenb;
        }
    } else {
        opt_lenb = stored_len + 5;
        static_lenb = opt_lenb;
    }

    if (opt_lenb < stored_len + 4 || buf == NULL) {
        if (static_lenb == opt_lenb) {
            deflate_send_bits(s, (uint32)(2 + eof), 3);   /* STATIC_TREES << 1 | eof */
            compress_block(s, static_ltree, static_dtree);
        } else {
            deflate_send_bits(s, (uint32)(4 + eof), 3);   /* DYN_TREES << 1 | eof */
            send_all_trees(s, s->l_desc.max_code + 1, s->d_desc.max_code + 1, max_blindex + 1);
            compress_block(s, s->dyn_ltree, s->dyn_dtree);
        }
    } else {
        tr_stored_block(s, buf, stored_len, eof);
    }

    init_block(s);
    if (eof != 0) {
        bi_windup(s);
    }
}

// 0x080d0efb  size=380  FUN_080d0efb  status=translated  naming=inferred
// zlib 1.1.4 trees.c _tr_tally: stores one literal or (distance, length) pair and bumps the frequency counts; returns true when the literal buffer is full
bool tr_tally(deflate_state_s *s, uint32 dist, uint32 lc)
{
    s->d_buf[s->last_lit] = (uint16)dist;
    s->l_buf[s->last_lit] = (byte)lc;
    s->last_lit++;

    if (dist == 0) {
        s->dyn_ltree[lc].fc++;
    } else {
        s->matches++;
        s->dyn_ltree[_length_code[lc] + 257].fc++;
        s->dyn_dtree[deflate_d_code(dist)].fc++;
    }
    return s->last_lit == s->lit_bufsize - 1;
}

// 0x080d1077  size=1963  FUN_080d1077  status=translated  naming=inferred
// zlib 1.1.4 trees.c compress_block: emits the buffered literals and matches with the given trees, then END_BLOCK and last_eob_len
void compress_block(deflate_state_s *s, const ct_data *ltree, const ct_data *dtree)
{
    uint32 lx = 0;

    if (s->last_lit != 0) {
        do {
            uint32 dist = s->d_buf[lx];
            uint32 lc = s->l_buf[lx];
            uint32 code;
            uint32 extra;

            lx++;
            if (dist == 0) {
                deflate_send_code(s, lc, ltree);
            } else {
                code = _length_code[lc];
                deflate_send_code(s, code + 257, ltree);
                extra = (uint32)extra_lbits[code];
                if (extra != 0) {
                    deflate_send_bits(s, lc - (uint32)base_length[code], (int)extra);
                }
                dist--;                                   /* distance - 1 for the code lookup below */
                code = deflate_d_code(dist + 1);
                deflate_send_code(s, code, dtree);
                extra = (uint32)extra_dbits[code];
                if (extra != 0) {
                    deflate_send_bits(s, dist - (uint32)base_dist[code], (int)extra);
                }
            }
        } while (lx < s->last_lit);
    }

    deflate_send_code(s, 256, ltree);
    s->last_eob_len = ltree[256].dl;
}

// 0x080d1822  size=154  FUN_080d1822  status=translated  naming=inferred
// zlib 1.1.4 trees.c set_data_type: classifies the block as binary or ascii from literal frequencies (bytes 0..6 and 128..255 count as binary; 7..127 as ascii)
void set_data_type(deflate_state_s *s)
{
    uint32 bin_freq = 0;
    uint32 ascii_freq = 0;
    int n = 0;

    for (; n < 7; n++) {
        bin_freq += s->dyn_ltree[n].fc;
    }
    for (; n < 128; n++) {
        ascii_freq += s->dyn_ltree[n].fc;
    }
    for (; n < 256; n++) {
        bin_freq += s->dyn_ltree[n].fc;
    }
    s->data_type = (byte)(bin_freq <= (ascii_freq >> 2));
}

// 0x080d18bc  size=46  FUN_080d18bc  status=translated  naming=inferred
// zlib 1.1.4 trees.c bi_reverse: reverses the low len bits of code
uint32 bi_reverse(uint32 code, int len)
{
    uint32 res_prev = 0;
    uint32 res = 0;
    uint32 bit = 0;

    do {
        res_prev = res;
        bit = code & 1;
        code >>= 1;
        len--;
        res = (res_prev | bit) << 1;
    } while (0 < len);
    return (res_prev & 0x7fffffff) | bit;
}

// 0x080d18ea  size=195  FUN_080d18ea  status=translated  naming=inferred
// zlib 1.1.4 trees.c bi_flush: writes out whole bytes from the 16-bit bit buffer, keeps the remainder
void bi_flush(deflate_state_s *s)
{
    if (s->bi_valid == 16) {
        deflate_put_short(s, s->bi_buf);
        s->bi_buf = 0;
        s->bi_valid = 0;
    } else if (s->bi_valid > 7) {
        s->pending_buf[s->pending] = (byte)(s->bi_buf & 0xff);
        s->pending++;
        s->bi_buf = (uint16)(s->bi_buf >> 8);
        s->bi_valid -= 8;
    }
}

// 0x080d19ad  size=151  FUN_080d19ad  status=translated  naming=inferred
// zlib 1.1.4 trees.c bi_windup: flushes the bit buffer to a byte boundary and clears it
void bi_windup(deflate_state_s *s)
{
    if (s->bi_valid > 8) {
        deflate_put_short(s, s->bi_buf);
    } else if (s->bi_valid > 0) {
        s->pending_buf[s->pending] = (byte)(s->bi_buf & 0xff);
        s->pending++;
    }
    s->bi_buf = 0;
    s->bi_valid = 0;
}

// 0x080d1a44  size=205  FUN_080d1a44  status=translated  naming=inferred
// zlib 1.1.4 trees.c copy_block: byte-aligns, optionally writes LEN and NLEN, then copies len raw bytes into pending output
void copy_block(deflate_state_s *s, byte *buf, int len, int header)
{
    bi_windup(s);
    s->last_eob_len = 8;
    if (header != 0) {
        deflate_put_short(s, (uint32)len);
        deflate_put_short(s, (uint32)~len);
    }
    while (len-- != 0) {
        s->pending_buf[s->pending] = *buf;
        s->pending++;
        buf++;
    }
}

// 0x080d1b14  size=231  FUN_080d1b14  status=translated  naming=inferred
// zlib 1.1.4 uncompress(): inflateInit_ with version "1.1.4", inflate(Z_FINISH) into the caller's buffer, total_out to *destLen, inflateEnd
int zlib_uncompress(byte *dest, uint32 *destLen, const byte *source, uint32 sourceLen)
{
    z_stream_s stream;
    int err;
    int ret;

    stream.next_in = (byte *)source;
    stream.avail_in = sourceLen;
    if (stream.avail_in != sourceLen) {
        return ZLIB_BUF_ERROR;
    }
    stream.next_out = dest;
    stream.avail_out = *destLen;
    if (stream.avail_out != *destLen) {
        return ZLIB_BUF_ERROR;
    }
    stream.zalloc = NULL;
    stream.zfree = NULL;

    err = inflateInit_(&stream, "1.1.4", 0x38);
    if (err != 0) {
        return err;
    }
    ret = inflate(&stream, ZLIB_FINISH);
    if (ret == ZLIB_STREAM_END) {
        *destLen = stream.total_out;
        err = inflateEnd(&stream);
    } else {
        inflateEnd(&stream);
        if (ret == 0) {
            ret = ZLIB_BUF_ERROR;
        }
        err = ret;
    }
    return err;
}

// 0x080d1bfc  size=56  FUN_080d1bfc  status=translated  naming=inferred
// minizip unzip.c unzlocal_getShort: reads a little-endian 16-bit ZIP field, sign-extended into *out; always returns 0
int unzlocal_getShort(void *fp, uint32 *out)
{
    int16 value;

    Com_Fread(&value, 2, 1, fp);
    *out = (uint32)(int)value;
    return 0;
}

// 0x080d1c34  size=55  FUN_080d1c34  status=translated  naming=inferred
// minizip unzip.c unzlocal_getLong: reads a little-endian 32-bit ZIP field into *out; always returns 0
int unzlocal_getLong(void *fp, uint32 *out)
{
    uint32 value;

    Com_Fread(&value, 4, 1, fp);
    *out = value;
    return 0;
}

// 0x080d1c6b  size=165  FUN_080d1c6b  status=translated  naming=inferred
// minizip unzip.c strcmpcasenosensitive_internal: ASCII case-folded string compare (folds 'a'..'z' only), returns -1, 0 or 1
int unzlocal_strcmpcasenosensitive_internal(const char *fileName1, const char *fileName2)
{
    for (;;) {
        char c1 = *fileName1;
        char c2 = *fileName2;

        if ('`' < c1 && c1 < '{') {
            c1 = (char)(c1 - 0x20);
        }
        if ('`' < c2 && c2 < '{') {
            c2 = (char)(c2 - 0x20);
        }
        if (c1 == '\0') {
            return (c2 == '\0') ? 0 : -1;
        }
        if (c2 == '\0') {
            return 1;
        }
        if (c1 < c2) {
            return -1;
        }
        fileName1++;
        fileName2++;
        if (c2 < c1) {
            return 1;
        }
    }
}

// 0x080d1d10  size=74  FUN_080d1d10  status=translated  naming=inferred
// minizip unzip.c unzStringFileNameCompare: iCaseSensitivity 0 or 1 selects strcmp, any other value selects the case-folded compare
int unzStringFileNameCompare(const char *fileName1, const char *fileName2, int iCaseSensitivity)
{
    if (iCaseSensitivity == 0) {
        iCaseSensitivity = 1;
    }
    if (iCaseSensitivity == 1) {
        return strcmp(fileName1, fileName2);
    }
    return unzlocal_strcmpcasenosensitive_internal(fileName1, fileName2);
}

// 0x080d1d5a  size=414  FUN_080d1d5a  status=translated  naming=inferred
// minizip unzip.c unzlocal_SearchCentralDir: scans back up to 0xffff bytes from EOF in 1 KiB steps for the end-of-central-directory signature "PK\5\6"; returns its file offset or 0
uint32 unzlocal_SearchCentralDir(void *fp)
{
    uint32 file_size;
    uint32 max_back = 0xffff;
    uint32 block_start;
    uint32 read_len;
    int found = 0;
    int seek_pos;
    int idx;
    int pos;
    byte *buf;

    if (Com_Fseek(fp, 0, 2) != 0) {
        return 0;
    }
    file_size = (uint32)ftell((FILE *)fp);
    if (file_size < 0xffff) {
        max_back = file_size;
    }

    buf = (byte *)malloc(0x404);
    if (buf == NULL) {
        return 0;
    }

    block_start = 4;
    while (block_start < max_back) {
        if (max_back < block_start + 0x400) {
            block_start = max_back;
        } else {
            block_start += 0x400;
        }
        seek_pos = (int)(file_size - block_start);
        read_len = file_size - (uint32)seek_pos;
        if (read_len > 0x404) {
            read_len = 0x404;
        }
        if (Com_Fseek(fp, seek_pos, 0) != 0) {
            break;
        }
        if (Com_Fread(buf, read_len, 1, fp) != 1) {
            break;
        }
        idx = (int)read_len - 3;
        for (;;) {
            pos = idx;
            idx = pos - 1;
            if (pos < 1) {
                break;
            }
            if (buf[idx] == 'P' && buf[pos] == 'K' && buf[pos + 1] == 5 && buf[pos + 2] == 6) {
                found = idx + seek_pos;
                break;
            }
        }
        if (found != 0) {
            break;
        }
    }

    free(buf);
    return (uint32)found;
}

// 0x080d1ef8  size=113  FUN_080d1ef8  status=translated  naming=inferred
// minizip unzip.c unzReOpen: opens the archive path again and clones the state of an already open unzFile (0x80 bytes) with the new handle and no open entry
unz_s *unzReOpen(const char *path, unz_s *file_to_reopen)
{
    void *fp = Com_Fopen(path, "rb");
    unz_s *s;

    if (fp == NULL) {
        return NULL;
    }
    s = (unz_s *)malloc(0x80);
    Com_Memcpy(s, file_to_reopen, 0x80);
    s->file = fp;
    s->pfile_in_zip_read = NULL;
    return s;
}

// 0x080d1f69  size=741  FUN_080d1f69  status=translated  naming=inferred
// minizip unzip.c unzOpen: opens "rb", locates the end-of-central-directory record, validates the disk and entry counts and offsets, then clones the 0x80-byte state and positions on the first entry
unz_s *unzOpen(const char *path)
{
    unz_s us;
    unz_s *ret = NULL;
    int err = 0;
    uint32 sig_ignored;
    uint32 disk_this;
    uint32 disk_cd;
    uint32 entries_total;
    uint32 central_pos;
    void *fp;

    fp = Com_Fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    memset(&us, 0, sizeof(us));

    central_pos = unzlocal_SearchCentralDir(fp);
    if (central_pos == 0) {
        err = UNZ_ERRNO;
    }
    if (Com_Fseek(fp, (long)central_pos, 0) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(fp, &sig_ignored) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(fp, &disk_this) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(fp, &disk_cd) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(fp, &us.gi.number_entry) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(fp, &entries_total) != 0) {
        err = UNZ_ERRNO;
    }
    if (entries_total != us.gi.number_entry || disk_cd != 0 || disk_this != 0) {
        err = UNZ_BADZIPFILE;
    }
    if (unzlocal_getLong(fp, &us.size_central_dir) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(fp, &us.offset_central_dir) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(fp, &us.gi.size_comment) != 0) {
        err = UNZ_ERRNO;
    }
    if (central_pos < us.offset_central_dir + us.size_central_dir && err == 0) {
        err = UNZ_BADZIPFILE;
    }

    if (err == 0) {
        us.file = fp;
        us.byte_before_the_zipfile = central_pos - (us.offset_central_dir + us.size_central_dir);
        us.central_pos = central_pos;
        us.pfile_in_zip_read = NULL;

        ret = (unz_s *)malloc(0x80);
        memcpy(ret, &us, 0x80);
        unzGoToFirstFile(ret);
    } else {
        Com_Fclose(fp);
        ret = NULL;
    }
    return ret;
}

// 0x080d224e  size=83  FUN_080d224e  status=translated  naming=inferred
// minizip unzip.c unzClose: closes the open entry (if any), closes the archive handle, frees the state
int unzClose(unz_s *file)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->pfile_in_zip_read != NULL) {
        unzCloseCurrentFile(file);
    }
    Com_Fclose(file->file);
    free(file);
    return 0;
}

// 0x080d22a1  size=56  FUN_080d22a1  status=translated  naming=inferred
// minizip unzip.c unzGetGlobalInfo: copies number_entry and size_comment of the archive
int unzGetGlobalInfo(unz_s *file, unz_global_info_s *pglobal_info)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    pglobal_info->number_entry = file->gi.number_entry;
    pglobal_info->size_comment = file->gi.size_comment;
    return 0;
}

// 0x080d22d9  size=116  FUN_080d22d9  status=translated  naming=inferred
// minizip unzip.c DosDateToTmuDate: unpacks a 32-bit MS-DOS date/time (years from 1980)
void DosDateToTmuDate(uint32 ulDosDate, tm_unz_s *ptm)
{
    ptm->tm_hour = (ulDosDate & 0xf800) >> 11;
    ptm->tm_min = (ulDosDate & 0x7e0) >> 5;
    ptm->tm_sec = (ulDosDate & 0x1f) * 2;
    ptm->tm_mday = (ulDosDate >> 16) & 0x1f;
    ptm->tm_mon = ((ulDosDate >> 16) & 0x1e0) / 0x20 - 1;
    ptm->tm_year = (ulDosDate >> 25) + 1980;
}

// 0x080d234d  size=1236  FUN_080d234d  status=translated  naming=inferred
// minizip unzip.c unzlocal_GetCurrentFileInfoInternal: seeks to the current central-directory entry, reads its fields into pfile_info, copies name/extra/comment into the caller's buffers (skipping what does not fit), and returns the offset of the local header
int unzlocal_GetCurrentFileInfoInternal(unz_s *file, unz_file_info_s *pfile_info, uint32 *pfile_info_internal,
                                        char *szFileName, uint32 fileNameBufferSize, void *extraField,
                                        uint32 extraFieldBufferSize, char *szComment, uint32 commentBufferSize)
{
    unz_file_info_s info;
    int err = 0;
    uint32 sig = 0;
    uint32 offset_local = 0;
    uint32 remaining;
    uint32 chunk;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }

    memset(&info, 0, sizeof(info));
    if (Com_Fseek(file->file, (long)(file->byte_before_the_zipfile + file->pos_in_central_dir), 0) != 0) {
        err = UNZ_ERRNO;
    }
    if (err == 0) {
        if (unzlocal_getLong(file->file, &sig) == 0) {
            if (sig != ZIP_CENTRAL_HEADER_SIG) {
                err = UNZ_BADZIPFILE;
            }
        } else {
            err = UNZ_ERRNO;
        }
    }
    if (unzlocal_getShort(file->file, &info.version) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.version_needed) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.flag) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.compression_method) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(file->file, &info.dosDate) != 0) {
        err = UNZ_ERRNO;
    }
    DosDateToTmuDate(info.dosDate, &info.tmu_date);
    if (unzlocal_getLong(file->file, &info.crc) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(file->file, &info.compressed_size) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(file->file, &info.uncompressed_size) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.size_filename) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.size_file_extra) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.size_file_comment) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.disk_num_start) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(file->file, &info.internal_fa) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(file->file, &info.external_fa) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(file->file, &offset_local) != 0) {
        err = UNZ_ERRNO;
    }

    /* file name */
    remaining = info.size_filename;
    if (err == 0 && szFileName != NULL) {
        if (info.size_filename < fileNameBufferSize) {
            szFileName[info.size_filename] = 0;
            chunk = info.size_filename;
        } else {
            chunk = fileNameBufferSize;
        }
        if (info.size_filename != 0 && fileNameBufferSize != 0) {
            if (Com_Fread(szFileName, chunk, 1, file->file) != 1) {
                err = UNZ_ERRNO;
            }
        }
        remaining = info.size_filename - chunk;
    }

    /* extra field */
    if (err == 0 && extraField != NULL) {
        chunk = (info.size_file_extra < extraFieldBufferSize) ? info.size_file_extra : extraFieldBufferSize;
        if (remaining != 0) {
            if (Com_Fseek(file->file, (long)remaining, 1) == 0) {
                remaining = 0;
            } else {
                err = UNZ_ERRNO;
            }
        }
        if (info.size_file_extra != 0 && extraFieldBufferSize != 0) {
            if (Com_Fread(extraField, chunk, 1, file->file) != 1) {
                err = UNZ_ERRNO;
            }
        }
        remaining = remaining + (info.size_file_extra - chunk);
    } else {
        remaining = remaining + info.size_file_extra;
    }

    /* comment */
    if (err == 0 && szComment != NULL) {
        if (info.size_file_comment < commentBufferSize) {
            szComment[info.size_file_comment] = 0;
            chunk = info.size_file_comment;
        } else {
            chunk = commentBufferSize;
        }
        if (remaining != 0) {
            if (Com_Fseek(file->file, (long)remaining, 1) != 0) {
                err = UNZ_ERRNO;
            }
        }
        if (info.size_file_comment != 0 && commentBufferSize != 0) {
            if (Com_Fread(szComment, chunk, 1, file->file) != 1) {
                err = UNZ_ERRNO;
            }
        }
    }

    if (err == 0 && pfile_info != NULL) {
        memcpy(pfile_info, &info, sizeof(info));
    }
    if (err == 0 && pfile_info_internal != NULL) {
        *pfile_info_internal = offset_local;
    }
    return err;
}

// 0x080d2821  size=76  FUN_080d2821  status=translated  naming=inferred
// minizip unzip.c unzGetCurrentFileInfo: the public wrapper with no internal-info output
int unzGetCurrentFileInfo(unz_s *file, unz_file_info_s *pfile_info, char *szFileName, uint32 fileNameBufferSize,
                          void *extraField, uint32 extraFieldBufferSize, char *szComment, uint32 commentBufferSize)
{
    return unzlocal_GetCurrentFileInfoInternal(file, pfile_info, NULL, szFileName, fileNameBufferSize, extraField,
                                               extraFieldBufferSize, szComment, commentBufferSize);
}

// 0x080d286d  size=168  FUN_080d286d  status=translated  naming=inferred
// minizip unzip.c unzGoToFirstFile: positions on the first central-directory entry and loads its info
int unzGoToFirstFile(unz_s *file)
{
    int err;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    file->pos_in_central_dir = file->offset_central_dir;
    file->num_file = 0;
    err = unzlocal_GetCurrentFileInfoInternal(file, &file->cur_file_info, &file->cur_file_info_internal.offset_curfile,
                                              NULL, 0, NULL, 0, NULL, 0);
    file->current_file_ok = (err == 0);
    return err;
}

// 0x080d2915  size=238  FUN_080d2915  status=translated  naming=inferred
// minizip unzip.c unzGoToNextFile: advances past the current entry (header 0x2e + name + extra + comment) and loads it; UNZ_END_OF_LIST_OF_FILE at the end
int unzGoToNextFile(unz_s *file)
{
    int err;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->current_file_ok == 0) {
        return UNZ_END_OF_LIST_OF_FILE;
    }
    if (file->num_file + 1 == file->gi.number_entry) {
        return UNZ_END_OF_LIST_OF_FILE;
    }
    file->pos_in_central_dir = file->pos_in_central_dir + file->cur_file_info.size_file_comment +
                               file->cur_file_info.size_filename + file->cur_file_info.size_file_extra + 0x2e;
    file->num_file++;
    err = unzlocal_GetCurrentFileInfoInternal(file, &file->cur_file_info, &file->cur_file_info_internal.offset_curfile,
                                              NULL, 0, NULL, 0, NULL, 0);
    file->current_file_ok = (err == 0);
    return err;
}

// 0x080d2a03  size=50  FUN_080d2a03  status=translated  naming=inferred
// minizip unzip.c unzGetOffset: returns the current central-directory position
int unzGetOffset(unz_s *file, uint32 *pos)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    *pos = file->pos_in_central_dir;
    return 0;
}

// 0x080d2a35  size=146  FUN_080d2a35  status=translated  naming=inferred
// minizip unzip.c unzSetOffset: moves to a central-directory position and reloads that entry's info
int unzSetOffset(unz_s *file, uint32 pos)
{
    int err;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    file->pos_in_central_dir = pos;
    err = unzlocal_GetCurrentFileInfoInternal(file, &file->cur_file_info, &file->cur_file_info_internal.offset_curfile,
                                              NULL, 0, NULL, 0, NULL, 0);
    file->current_file_ok = (err == 0);
    return 0;
}

// 0x080d2ac7  size=303  FUN_080d2ac7  status=translated  naming=inferred
// minizip unzip.c unzLocateFile: walks the central directory for a name (names of 255 bytes or more are rejected); restores the position when not found
int unzLocateFile(unz_s *file, const char *szFileName, int iCaseSensitivity)
{
    char szCurrentFileName[0x110];
    uint32 num_file_saved;
    uint32 pos_saved;
    int err;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (strlen(szFileName) >= 0x100) {
        return UNZ_PARAMERROR;
    }
    if (file->current_file_ok == 0) {
        return UNZ_END_OF_LIST_OF_FILE;
    }

    num_file_saved = file->num_file;
    pos_saved = file->pos_in_central_dir;

    err = unzGoToFirstFile(file);
    while (err == 0) {
        unzGetCurrentFileInfo(file, NULL, szCurrentFileName, 0x100, NULL, 0, NULL, 0);
        if (unzStringFileNameCompare(szCurrentFileName, szFileName, iCaseSensitivity) == 0) {
            return 0;
        }
        err = unzGoToNextFile(file);
    }

    file->num_file = num_file_saved;
    file->pos_in_central_dir = pos_saved;
    return err;
}

// 0x080d2bf6  size=680  FUN_080d2bf6  status=translated  naming=inferred
// minizip unzip.c unzlocal_CheckCurrentFileCoherencyHeader: re-reads the local header of the current entry and cross-checks it with the central directory; reports the variable-size block and the local extra-field location
int unzlocal_CheckCurrentFileCoherencyHeader(unz_s *s, uint32 *piSizeVar, uint32 *poffset_local_extrafield,
                                             uint32 *psize_local_extrafield)
{
    int err = 0;
    uint32 sig;
    uint32 version_needed;
    uint32 flags;
    uint32 value;
    uint32 size_filename_local;
    uint32 size_extra_local;

    *piSizeVar = 0;
    *poffset_local_extrafield = 0;
    *psize_local_extrafield = 0;

    if (Com_Fseek(s->file, (long)(s->byte_before_the_zipfile + s->cur_file_info_internal.offset_curfile), 0) != 0) {
        return UNZ_ERRNO;
    }

    if (unzlocal_getLong(s->file, &sig) == 0) {
        if (sig != ZIP_LOCAL_HEADER_SIG) {
            err = UNZ_BADZIPFILE;
        }
    } else {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(s->file, &version_needed) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(s->file, &flags) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(s->file, &value) == 0) {
        if (err == 0 && value != s->cur_file_info.compression_method) {
            err = UNZ_BADZIPFILE;
        }
    } else {
        err = UNZ_ERRNO;
    }
    if (err == 0 && s->cur_file_info.compression_method != 0 && s->cur_file_info.compression_method != 8) {
        err = UNZ_BADZIPFILE;
    }

    if (unzlocal_getLong(s->file, &value) != 0) {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(s->file, &value) == 0) {
        if (err == 0 && value != s->cur_file_info.crc && (flags & 8) == 0) {
            err = UNZ_BADZIPFILE;
        }
    } else {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(s->file, &value) == 0) {
        if (err == 0 && value != s->cur_file_info.compressed_size && (flags & 8) == 0) {
            err = UNZ_BADZIPFILE;
        }
    } else {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getLong(s->file, &value) == 0) {
        if (err == 0 && value != s->cur_file_info.uncompressed_size && (flags & 8) == 0) {
            err = UNZ_BADZIPFILE;
        }
    } else {
        err = UNZ_ERRNO;
    }
    if (unzlocal_getShort(s->file, &size_filename_local) == 0) {
        if (err == 0 && size_filename_local != s->cur_file_info.size_filename) {
            err = UNZ_BADZIPFILE;
        }
    } else {
        err = UNZ_ERRNO;
    }
    *piSizeVar = size_filename_local + *piSizeVar;

    if (unzlocal_getShort(s->file, &size_extra_local) != 0) {
        err = UNZ_ERRNO;
    }
    *poffset_local_extrafield = size_filename_local + s->cur_file_info_internal.offset_curfile + 0x1e;
    *psize_local_extrafield = size_extra_local;
    *piSizeVar = size_extra_local + *piSizeVar;

    return err;
}

// 0x080d2e9e  size=502  FUN_080d2e9e  status=translated  naming=inferred
// minizip unzip.c unzOpenCurrentFile: validates the local header, allocates the 100-byte read state with a 16 KiB buffer, and starts inflate (raw deflate, windowBits -15) for compressed entries
int unzOpenCurrentFile(unz_s *file)
{
    file_in_zip_read_info_s *pfi;
    uint32 iSizeVar = 0;
    uint32 offset_local_extrafield = 0;
    uint32 size_local_extrafield = 0;
    int err = 0;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->current_file_ok == 0) {
        return UNZ_PARAMERROR;
    }
    if (file->pfile_in_zip_read != NULL) {
        unzCloseCurrentFile(file);
    }

    if (unzlocal_CheckCurrentFileCoherencyHeader(file, &iSizeVar, &offset_local_extrafield,
                                                 &size_local_extrafield) != 0) {
        return UNZ_BADZIPFILE;
    }

    pfi = (file_in_zip_read_info_s *)malloc(0x64);
    if (pfi == NULL) {
        return UNZ_INTERNALERROR;
    }
    pfi->read_buffer = (byte *)malloc(0x4000);
    pfi->offset_local_extrafield = offset_local_extrafield;
    pfi->size_local_extrafield = size_local_extrafield;
    pfi->pos_local_extrafield = 0;

    if (pfi->read_buffer == NULL) {
        free(pfi);
        return UNZ_INTERNALERROR;
    }

    pfi->stream_initialised = 0;
    /* The original computes UNZ_BADZIPFILE for a method other than 0 or 8, stores it in a dead local and still opens the entry. */
    pfi->compression_method = file->cur_file_info.compression_method;
    pfi->file = file->file;
    pfi->byte_before_the_zipfile = file->byte_before_the_zipfile;
    pfi->stream.total_out = 0;

    if (file->cur_file_info.compression_method != 0) {
        pfi->stream.zalloc = NULL;
        pfi->stream.zfree = NULL;
        pfi->stream.opaque = NULL;
        if (inflateInit2_(&pfi->stream, -15, "1.1.4", 0x38) == 0) {
            pfi->stream_initialised = 1;
        }
    }

    pfi->rest_read_compressed = file->cur_file_info.compressed_size;
    pfi->rest_read_uncompressed = file->cur_file_info.uncompressed_size;
    pfi->pos_in_zipfile = iSizeVar + file->cur_file_info_internal.offset_curfile + 0x1e;
    pfi->stream.avail_in = 0;

    file->pfile_in_zip_read = pfi;
    return err;
}

// 0x080d3094  size=752  FUN_080d3094  status=translated  naming=inferred
// minizip unzip.c unzReadCurrentFile: returns bytes read from the open entry; stored entries are copied directly, deflated ones go through inflate(Z_SYNC_FLUSH) with 16 KiB refills
int unzReadCurrentFile(unz_s *file, void *buf, uint32 len)
{
    file_in_zip_read_info_s *pfi;
    int result = 0;
    int err = 0;
    uint32 copied = 0;
    uint32 chunk;
    uint32 total_before;
    uint32 i;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    pfi = file->pfile_in_zip_read;
    if (pfi == NULL) {
        return UNZ_PARAMERROR;
    }
    if (pfi->read_buffer == NULL) {
        return UNZ_END_OF_LIST_OF_FILE;
    }
    if (len == 0) {
        return 0;
    }

    pfi->stream.next_out = (byte *)buf;
    pfi->stream.avail_out = len;
    if (pfi->rest_read_uncompressed < len) {
        pfi->stream.avail_out = pfi->rest_read_uncompressed;
    }

    for (;;) {
        for (;;) {
            if (pfi->stream.avail_out == 0) {
                goto finish;
            }
            if (pfi->stream.avail_in == 0 && pfi->rest_read_compressed != 0) {
                chunk = (pfi->rest_read_compressed < 0x4000) ? pfi->rest_read_compressed : 0x4000;
                if (chunk == 0) {
                    return 0;
                }
                if (Com_Fseek(pfi->file, (long)(pfi->byte_before_the_zipfile + pfi->pos_in_zipfile), 0) != 0) {
                    return UNZ_ERRNO;
                }
                if (Com_Fread(pfi->read_buffer, chunk, 1, pfi->file) != 1) {
                    return UNZ_ERRNO;
                }
                pfi->pos_in_zipfile += chunk;
                pfi->rest_read_compressed -= chunk;
                pfi->stream.next_in = pfi->read_buffer;
                pfi->stream.avail_in = chunk;
            }
            if (pfi->compression_method != 0) {
                break;
            }
            /* stored entry: copy straight through */
            chunk = (pfi->stream.avail_out < pfi->stream.avail_in) ? pfi->stream.avail_out : pfi->stream.avail_in;
            for (i = 0; i < chunk; i++) {
                pfi->stream.next_out[i] = pfi->stream.next_in[i];
            }
            pfi->rest_read_uncompressed -= chunk;
            pfi->stream.avail_in -= chunk;
            pfi->stream.avail_out -= chunk;
            pfi->stream.next_out += chunk;
            pfi->stream.next_in += chunk;
            pfi->stream.total_out += chunk;
            copied += chunk;
        }

        total_before = pfi->stream.total_out;
        err = inflate(&pfi->stream, ZLIB_SYNC_FLUSH);
        pfi->rest_read_uncompressed -= (pfi->stream.total_out - total_before);
        copied += (pfi->stream.total_out - total_before);
        if (err == ZLIB_STREAM_END) {
            return (int)copied;
        }
        if (err != 0) {
            break;
        }
    }

finish:
    result = (err == 0) ? (int)copied : err;
    return result;
}

// 0x080d3384  size=65  FUN_080d3384  status=translated  naming=inferred
// minizip unzip.c unzTellCurrentFile: uncompressed bytes already delivered from the open entry
int unzTellCurrentFile(unz_s *file)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }
    return (int)file->pfile_in_zip_read->stream.total_out;
}

// 0x080d33c5  size=81  FUN_080d33c5  status=translated  naming=inferred
// minizip unzip.c unzeof: 1 when the open entry has no uncompressed bytes left, 0 otherwise
int unzeof(unz_s *file)
{
    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->pfile_in_zip_read == NULL) {
        return UNZ_PARAMERROR;
    }
    if (file->pfile_in_zip_read->rest_read_uncompressed == 0) {
        return 1;
    }
    return 0;
}

// 0x080d3416  size=240  FUN_080d3416  status=translated  naming=inferred
// minizip unzip.c unzReadLocalExtraField: with buf NULL returns the unread local extra-field length; otherwise reads from it (the original reads the whole remainder into buf) and returns the count copied
uint32 unzReadLocalExtraField(unz_s *file, void *buf, uint32 len)
{
    file_in_zip_read_info_s *pfi;
    uint32 result;
    uint32 n = 0;

    if (file == NULL) {
        return (uint32)UNZ_PARAMERROR;
    }
    pfi = file->pfile_in_zip_read;
    if (pfi == NULL) {
        return (uint32)UNZ_PARAMERROR;
    }
    result = pfi->size_local_extrafield - pfi->pos_local_extrafield;
    if (buf != NULL) {
        n = (result < len) ? result : len;
        if (n == 0) {
            result = 0;
        } else {
            if (Com_Fseek(pfi->file, (long)(pfi->pos_local_extrafield + pfi->offset_local_extrafield), 0) == 0) {
                if (Com_Fread(buf, result, 1, pfi->file) == 1) {
                    result = n;
                } else {
                    result = 0xffffffff;
                }
            } else {
                result = 0xffffffff;
            }
        }
    }
    return result;
}

// 0x080d3506  size=145  FUN_080d3506  status=translated  naming=inferred
// minizip unzip.c unzCloseCurrentFile: frees the read buffer, ends inflate if it was started, frees the state
int unzCloseCurrentFile(unz_s *file)
{
    file_in_zip_read_info_s *pfi;

    if (file == NULL) {
        return UNZ_PARAMERROR;
    }
    pfi = file->pfile_in_zip_read;
    if (pfi == NULL) {
        return UNZ_PARAMERROR;
    }
    free(pfi->read_buffer);
    pfi->read_buffer = NULL;
    if (pfi->stream_initialised != 0) {
        inflateEnd(&pfi->stream);
    }
    pfi->stream_initialised = 0;
    free(pfi);
    file->pfile_in_zip_read = NULL;
    return 0;
}

// 0x080d3597  size=204  FUN_080d3597  status=translated  naming=inferred
// minizip unzip.c unzGetGlobalComment: reads the archive comment at end-of-central-directory + 22; returns its length or -1 on a failed read
uint32 unzGetGlobalComment(unz_s *file, char *szComment, uint32 commentBufferSize)
{
    uint32 n;
    uint32 result;

    if (file == NULL) {
        return (uint32)UNZ_PARAMERROR;
    }
    n = commentBufferSize;
    if (file->gi.size_comment < commentBufferSize) {
        n = file->gi.size_comment;
    }
    if (Com_Fseek(file->file, (long)(file->central_pos + 0x16), 0) == 0) {
        if (n != 0) {
            *szComment = 0;
            if (Com_Fread(szComment, n, 1, file->file) != 1) {
                return 0xffffffff;
            }
        }
        if (szComment != NULL && file->gi.size_comment < commentBufferSize) {
            szComment[file->gi.size_comment] = 0;
        }
        result = n;
    } else {
        result = 0xffffffff;
    }
    return result;
}

// 0x080d3664  size=10  FUN_080d3664  status=translated  naming=inferred
// zlib 1.1.4 zlibVersion: returns the library version string
const char *zlibVersion(void)
{
    return "1.1.4";
}

// 0x080d366e  size=20  FUN_080d366e  status=translated  naming=inferred
// zlib 1.1.4 zError: maps a zlib return code to its message through the z_errmsg table (index 2 - err)
const char *zError(int err)
{
    return z_errmsg[2 - err];
}

// 0x080d3682  size=23  FUN_080d3682  status=translated  naming=inferred
// zlib 1.1.4 zcalloc allocator hook: items * size bytes through the zeroed allocator; the result is returned unchanged
void *zcalloc(void *opaque, uint32 items, uint32 size)
{
    (void)opaque;
    return Com_MallocZeroed(items * size);
}

// 0x080d3699  size=19  FUN_080d3699  status=translated  naming=inferred
// zlib 1.1.4 zcfree deallocator hook: frees the block through the allocator wrapper
void zcfree(void *opaque, void *ptr)
{
    (void)opaque;
    Com_Free_A(ptr);
}

// 0x080d36ac  size=148  FUN_080d36ac  status=translated  naming=inferred
// directory probe: 1 if the directory opens and holds an entry other than "." and "..", else 0 (strings "." and ".." identify the skipped entries)
int Dir_HasNonDotEntries(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    int result = 0;

    dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    for (;;) {
        entry = readdir(dir);
        if (entry == NULL) {
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        result = 1;
        break;
    }
    closedir(dir);
    return result;
}

// 0x080d3740  size=10  FUN_080d3740  status=translated  naming=unknown
// returns 0; no string or caller evidence identifies the platform hook it stands in for
int Tty_StubReturnZero(void)
{
    return 0;
}

// 0x080d374a  size=10  FUN_080d374a  status=translated  naming=unknown
// returns 1; no string or caller evidence identifies the platform hook it stands in for
int Tty_StubReturnOne(void)
{
    return 1;
}

// 0x080d3754  size=10  FUN_080d3754  status=translated  naming=unknown
// returns 0; second instance of the same stub shape as Tty_StubReturnZero
int Tty_StubReturnZeroAlt(void)
{
    return 0;
}

// 0x080d375e  size=5  FUN_080d375e  status=translated  naming=unknown
// empty body; no caller evidence identifies it
void Tty_StubNoopA(void)
{
}

// 0x080d3764  size=5  FUN_080d3764  status=translated  naming=unknown
// empty body; second instance of the same stub shape as Tty_StubNoopA
void Sys_In_Restart_f(void)
{
}

// 0x080d376a  size=41  FUN_080d376a  status=translated  naming=inferred
// reads stdin one byte at a time until read() reports an error (-1); a zero-byte read at EOF is not treated as the end
void Tty_DrainStdin(void)
{
    char c;
    ssize_t n;

    do {
        n = read(0, &c, 1);
    } while (n != -1);
}

// 0x080d3794  size=101  FUN_080d3794  status=translated  naming=inferred
// writes the terminal sequence backspace, space, backspace to stdout to erase one echoed character
void Tty_EraseChar(void)
{
    char c;

    c = 8;
    write(1, &c, 1);
    c = 0x20;
    write(1, &c, 1);
    c = 8;
    write(1, &c, 1);
}

// 0x080d37fa  size=72  FUN_080d37fa  status=translated  naming=inferred
// hides the echoed input: on the first nested hide, erases the tty_con.cursor echoed characters; increments the hide depth
void Tty_HideInput(void)
{
    int i;

    if (tty_hideDepth == 0 && tty_con.cursor > 0) {
        for (i = 0; i < tty_con.cursor; i++) {
            Tty_EraseChar();
        }
    }
    tty_hideDepth = tty_hideDepth + 1;
}

// 0x080d3842  size=91  FUN_080d3842  status=translated  naming=inferred
// unhides the echoed input: decrements the hide depth and, when it returns to zero, rewrites the tty_con.cursor buffered characters
void Tty_ShowInput(void)
{
    int i;

    tty_hideDepth = tty_hideDepth - 1;
    if (tty_hideDepth == 0 && tty_con.cursor != 0) {
        for (i = 0; i < tty_con.cursor; i++) {
            write(1, tty_con.buffer + i, 1);
        }
    }
}

// 0x080d389e  size=57  FUN_080d389e  status=translated  naming=observed
// string "Shutdown tty console\n" at 0x80d389e's call site identifies the shutdown; restores the saved termios with TCSADRAIN when the console is active
void Tty_Shutdown(void)
{
    if (ttycon_on != 0) {
        Com_Printf("Shutdown tty console\n");
        tcsetattr(0, 1, (struct termios *)tty_tc);
    }
}
