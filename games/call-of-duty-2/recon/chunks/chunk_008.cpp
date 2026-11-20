/*
 * chunk_008.cpp -- cod2_lnxded 1.3 reconstruction, chunk 008 (0x08065f94..0x08069c98).
 *
 * Every function is translated from the binary's observable behaviour. Names
 * are inferred from the Quake III / IW engine structure where noted in each
 * provenance comment; placeholders fn_08xxxxxx stand for out-of-chunk callees.
 * Float-to-int conversions that the binary performs with x87 "fistp" under
 * round-toward-zero (every ROUND() in this chunk) are written as C casts.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_008_types.h"

/* In-chunk prototypes (definitions follow in address order). */
void FS_SetServerReferencedIwds(const char *checksumList, const char *nameList);
char *MapNameFromPath(const char *path);
void Huff_offsetPutBit(int bit, byte *fout, int32 *offset);
int Huff_offsetGetBit(const byte *fin, int32 *offset);
void Huff_putBit(int bit, byte *fout);
int Huff_getBit(const byte *fin);
node_s **Huff_allocPtr(huffman_s *h);
void Huff_freePtr(huffman_s *h, node_s **slot);
void Huff_swapTreePos(huffman_s *h, node_s *a, node_s *b);
void Huff_swapListPos(node_s *a, node_s *b);
void Huff_increment(huffman_s *h, node_s *node);
void Huff_addRef(huffman_s *h, uint32 chIn);
int Huff_Receive(node_s *node, int *ch, const byte *fin);
void Huff_offsetReceive(node_s *node, int *ch, const byte *fin, int32 *offset);
void Huff_recursiveBitTransmit(node_s *node, node_s *child, byte *fout);
void Huff_transmit(huffman_s *h, int ch, byte *fout);
void Huff_offsetTransmitKnown(huffman_s *h, int ch, byte *fout, int32 *offset);
void Huff_Init(huff_s *huff);
void NoOp_080670c8(void);
void NoOp_080670ce(void);
void MD4_Init(md4_ctx_s *ctx);
void MD4_Update(md4_ctx_s *ctx, const byte *input, uint32 inputLen);
void MD4_Final(byte digest[16], md4_ctx_s *ctx);
void MD4_Transform(uint32 state[4], const byte block[64]);
void MD4_Encode(byte *output, const uint32 *input, uint32 len);
void MD4_Decode(uint32 *output, const byte *input, uint32 len);
uint32 Com_BlockChecksum(const void *buffer, int length);
uint32 Com_BlockChecksumKey(const void *buffer, int length, int key);
void MSG_Init(msg_s *msg, byte *data, int32 maxsize);
void MSG_BeginReading(msg_s *msg);
int MSG_BitCount(const msg_s *msg);
void MSG_WriteBits(msg_s *msg, uint32 value, int bits);
void MSG_WriteBit0(msg_s *msg);
void MSG_WriteBit1(msg_s *msg);
int MSG_ReadBits(msg_s *msg, int bits);
int MSG_ReadBit(msg_s *msg);
int Huff_CompressBytes(const byte *in, byte *out, int count);
int Huff_DecompressBytes(const byte *in, byte *out, int compressedLen);
void MSG_WriteByte(msg_s *msg, int c);
void MSG_WriteData(msg_s *msg, const void *data, int length);
void MSG_WriteShort(msg_s *msg, int c);
void MSG_WriteLong(msg_s *msg, int c);
void MSG_WriteInt64(msg_s *msg, uint32 lo, uint32 hi);
void MSG_WriteString(msg_s *msg, const char *s);
void MSG_WriteBigString(msg_s *msg, const char *s);
void MSG_WriteAngle8(msg_s *msg, float angle);
void MSG_WriteAngle16(msg_s *msg, float angle);
int MSG_ReadByte(msg_s *msg);
int MSG_ReadShort(msg_s *msg);
int MSG_ReadLong(msg_s *msg);
uint64 MSG_ReadInt64(msg_s *msg);
char *MSG_ReadString(msg_s *msg);
char *MSG_ReadBigString(msg_s *msg);
char *MSG_ReadStringLine(msg_s *msg);
float MSG_ReadAngle16(msg_s *msg);
void MSG_ReadData(msg_s *msg, void *dst, int length);
void MSG_WriteDeltaKey(msg_s *msg, uint32 key, uint32 oldValue, uint32 newValue, int bits);
uint32 MSG_ReadDeltaKey(msg_s *msg, uint32 key, uint32 oldValue, int bits);
void MSG_WriteKeyedBits(msg_s *msg, uint32 key, uint32 value, int bits);
uint32 MSG_ReadKeyedBits(msg_s *msg, uint32 key, int bits);
void MSG_WriteDeltaKeyByte(msg_s *msg, uint32 key, int8 oldValue, uint32 newValue);
uint32 MSG_ReadDeltaKeyByte(msg_s *msg, uint8 key, uint32 oldValue);
void MSG_WriteDeltaKeyShort(msg_s *msg, uint32 key, int16 oldValue, uint32 newValue);
int MSG_ReadDeltaKeyShort(msg_s *msg, uint16 key, int oldValue);
void CopyReliableCommand(const char *src, char *dst, int dstSize);
void CL_CreateCmd(const playerState_s *ps, usercmd_s *cmd);
uint32 UserCmd_MoveClass(int forward, int right);
void UserCmd_MoveFromClass(uint32 cls, int8 *forward, int8 *right);
void MSG_WriteDeltaUsercmdKey(msg_s *msg, uint32 key, const usercmd_s *from, const usercmd_s *to);
void MSG_ReadDeltaUsercmdKey(msg_s *msg, uint32 key, const usercmd_s *from, usercmd_s *to);
void MSG_WriteDeltaField(msg_s *msg, const void *from, const void *to, const netField_s *field);
void MSG_WriteDeltaFieldGroup(msg_s *msg, const void *from, const void *to, int force, int count, const netField_s *fields);
void MSG_WriteDeltaStruct(msg_s *msg, const void *from, const void *to, int force, int numFields, int numBits, const netField_s *fields, int print);
void MSG_WriteDeltaEntity(msg_s *msg, const void *from, const void *to, int force);
void MSG_WriteDeltaSharedEntity(msg_s *msg, const void *from, const void *to, int force);
void MSG_WriteDeltaClientState(msg_s *msg, const void *from, const void *to, int force);
void MSG_ReadDeltaField(msg_s *msg, const void *from, void *to, const netField_s *field, int print);

/* ------------------------------------------------------------------------ */
// 0x08065f94  size=270  FUN_08065f94  status=translated  naming=inferred
// Evidence: string "iwd sum/name mismatch" is the error path of an iwd checksum-list parser; the parser stores
// Cmd_Argv atoi() values as checksums and string hashes as names. No string names the function itself.
void FS_SetServerReferencedIwds(const char *checksumList, const char *nameList)
{
    int32 count;
    int32 i;

    Cmd_TokenizeString((char *)checksumList);
    count = Cmd_Argc();
    if (count > 0x400) {
        count = 0x400;
    }
    FS_ClearListB();
    for (i = 0; i < count; i++) {
        const char *arg = Cmd_Argv(i);
        fs_referencedIwdSums[i] = atoi(arg);
    }

    if (nameList == NULL || *nameList == '\0') {
        if (count != 0) {
            Com_Error(1, "iwd sum/name mismatch");
        }
    } else {
        int32 nameCount;
        Cmd_TokenizeString((char *)nameList);
        nameCount = Cmd_Argc();
        if (nameCount > 0x400) {
            nameCount = 0x400;
        }
        if (count != nameCount) {
            Com_Error(1, "iwd sum/name mismatch");
        }
        for (i = 0; i < nameCount; i++) {
            char *hash = CopyString(Cmd_Argv(i));
            fs_referencedIwdNames[i] = hash;
        }
    }
    fs_numReferencedIwds = count;
}

/* ------------------------------------------------------------------------ */
// 0x080660a2  size=182  FUN_080660a2  status=translated  naming=inferred
// Evidence: literal "maps/mp/" prefix and "bsp" extension are stripped; '%' becomes '_'. Returns a shared buffer.
char *MapNameFromPath(const char *path)
{
    size_t len;
    int32 i;

    if (Q_stricmpn(path, "maps/mp/", 8) == 0) {
        path = path + 8;
    }
    len = strlen(path);
    if (strcasecmp(path + (len - 3), "bsp") == 0) {
        len = len - 7;
    }
    memcpy(g_081e9500, path, len);
    g_081e9500[len] = 0;
    for (i = 0; i < (int32)len; i++) {
        if (g_081e9500[i] == '%') {
            g_081e9500[i] = '_';
        }
    }
    return g_081e9500;
}

/* ------------------------------------------------------------------------ */
/* Adaptive Huffman coder. Naming: Quake III huffman.c counterpart           */
/* (inferred from node layout, NYT/INTERNAL symbols and the loc[] table).   */
/* ------------------------------------------------------------------------ */

// 0x08066158  size=112  FUN_08066158  status=translated  naming=inferred
// Evidence: writes one bit at an offset held in the caller's cursor and updates it; Huffman bit writer with Quake III structure.
void Huff_offsetPutBit(int bit, byte *fout, int32 *offset)
{
    huff_bitCursor = *offset;
    if ((huff_bitCursor & 7) == 0) {
        fout[huff_bitCursor >> 3] = 0;
    }
    fout[huff_bitCursor >> 3] = (byte)(bit << (huff_bitCursor & 7)) | fout[huff_bitCursor >> 3];
    huff_bitCursor = huff_bitCursor + 1;
    *offset = huff_bitCursor;
}

// 0x080661c8  size=71  FUN_080661c8  status=translated  naming=inferred
// Evidence: reads one bit at a cursor held by the caller, paired with Huff_offsetPutBit.
int Huff_offsetGetBit(const byte *fin, int32 *offset)
{
    int32 bitPos = *offset;
    byte value = fin[bitPos >> 3];
    huff_bitCursor = bitPos + 1;
    *offset = huff_bitCursor;
    return ((int)(uint32)value >> (bitPos & 7)) & 1;
}

// 0x08066210  size=105  FUN_08066210  status=translated  naming=inferred
// Evidence: bit writer using the global bit cursor huff_bitCursor; the bit argument is a char in the binary.
void Huff_putBit(int bit, byte *fout)
{
    if ((huff_bitCursor & 7) == 0) {
        fout[huff_bitCursor >> 3] = 0;
    }
    fout[huff_bitCursor >> 3] = (byte)((int8)bit << (huff_bitCursor & 7)) | fout[huff_bitCursor >> 3];
    huff_bitCursor = huff_bitCursor + 1;
}

// 0x0806627a  size=51  FUN_0806627a  status=translated  naming=inferred
// Evidence: reads one bit using the global bit cursor, paired with Huff_putBit.
int Huff_getBit(const byte *fin)
{
    int32 index = huff_bitCursor >> 3;
    int32 shift = (byte)huff_bitCursor;
    huff_bitCursor = huff_bitCursor + 1;
    return ((int)(uint32)fin[index] >> (shift & 7)) & 1;
}

// 0x080662ae  size=80  FUN_080662ae  status=translated  naming=inferred
// Evidence: allocates a node-pointer slot from nodePtrs[] or the free list; Quake III AllocNode counterpart.
node_s **Huff_allocPtr(huffman_s *h)
{
    node_s **slot;
    if (h->freelist == NULL) {
        slot = &h->nodePtrs[h->blocPtrs];
        h->blocPtrs = h->blocPtrs + 1;
    } else {
        slot = h->freelist;
        h->freelist = *(node_s ***)slot;
    }
    return slot;
}

// 0x080662fe  size=31  FUN_080662fe  status=translated  naming=inferred
// Evidence: pushes a node-pointer slot back on the free list (inverse of Huff_allocPtr).
void Huff_freePtr(huffman_s *h, node_s **slot)
{
    *(node_s ***)slot = h->freelist;
    h->freelist = slot;
}

// 0x0806631e  size=136  FUN_0806631e  status=translated  naming=inferred
// Evidence: swaps the tree positions of two nodes, fixing parent child pointers and the root (Quake III swap counterpart).
void Huff_swapTreePos(huffman_s *h, node_s *a, node_s *b)
{
    node_s *pa = a->parent;
    node_s *pb = b->parent;

    if (pa == NULL) {
        h->tree = b;
    } else if (pa->left == a) {
        pa->left = b;
    } else {
        pa->right = b;
    }

    if (pb == NULL) {
        h->tree = a;
    } else if (pb->left == b) {
        pb->left = a;
    } else {
        pb->right = a;
    }

    a->parent = pb;
    b->parent = pa;
}

// 0x080663a6  size=192  FUN_080663a6  status=translated  naming=inferred
// Evidence: swaps the weight-list (next/prev) positions of two nodes, repairing neighbour links (Quake III swaplist counterpart).
void Huff_swapListPos(node_s *a, node_s *b)
{
    node_s *tmp;

    tmp = a->next;
    a->next = b->next;
    b->next = tmp;

    tmp = a->prev;
    a->prev = b->prev;
    b->prev = tmp;

    if (a->next == a) {
        a->next = b;
    }
    if (b->next == b) {
        b->next = a;
    }
    if (a->next != NULL) {
        a->next->prev = a;
    }
    if (b->next != NULL) {
        b->next->prev = b;
    }
    if (a->prev != NULL) {
        a->prev->next = a;
    }
    if (b->prev != NULL) {
        b->prev->next = b;
    }
}

// 0x08066466  size=359  FUN_08066466  status=translated  naming=inferred
// Evidence: recursive weight increment that keeps the tree in order; Quake III Huff_increment counterpart by structure (node->next/prev weight blocks, head slots, recursion on parent).
void Huff_increment(huffman_s *h, node_s *node)
{
    if (node == NULL) {
        return;
    }

    if (node->next != NULL && node->next->weight == node->weight) {
        node_s *leader = *node->head;
        if (leader != node->parent) {
            Huff_swapTreePos(h, leader, node);
        }
        Huff_swapListPos(leader, node);
    }

    if (node->prev == NULL || node->prev->weight != node->weight) {
        *node->head = NULL;
        Huff_freePtr(h, node->head);
    } else {
        *node->head = node->prev;
    }

    node->weight = node->weight + 1;

    if (node->next == NULL || node->next->weight != node->weight) {
        node->head = Huff_allocPtr(h);
        *node->head = node;
    } else {
        node->head = node->next->head;
    }

    if (node->parent != NULL) {
        Huff_increment(h, node->parent);
        if (node->prev == node->parent) {
            Huff_swapListPos(node, node->parent);
            if (*node->head == node) {
                *node->head = node->parent;
            }
        }
    }
}

// 0x080665ce  size=645  FUN_080665ce  status=translated  naming=inferred
// Evidence: first transmission of a symbol splits the NYT node into an internal node (0x101) and the new leaf; otherwise increments the leaf (Quake III Huff_addRef counterpart).
void Huff_addRef(huffman_s *h, uint32 chIn)
{
    uint32 ch = chIn & 0xff;

    if (h->loc[ch] == NULL) {
        int32 leafIndex = h->blocNode;
        node_s *leaf = &h->nodeList[leafIndex];
        int32 innerIndex;
        node_s *inner;

        h->blocNode = h->blocNode + 1;
        innerIndex = h->blocNode;
        inner = &h->nodeList[innerIndex];

        inner->symbol = COD2_HUFF_INTERNAL;
        inner->weight = 1;
        inner->next = h->lhead->next;
        if (h->lhead->next == NULL) {
            inner->head = Huff_allocPtr(h);
            *inner->head = inner;
        } else {
            h->lhead->next->prev = inner;
            if (h->lhead->next->weight == 1) {
                inner->head = h->lhead->next->head;
            } else {
                inner->head = Huff_allocPtr(h);
                *inner->head = inner;
            }
        }
        h->lhead->next = inner;
        inner->prev = h->lhead;

        h->blocNode = h->blocNode + 1;

        leaf->symbol = (int32)ch;
        leaf->weight = 1;
        leaf->next = h->lhead->next;
        if (h->lhead->next == NULL) {
            leaf->head = Huff_allocPtr(h);
            *leaf->head = leaf;
        } else {
            h->lhead->next->prev = leaf;
            if (h->lhead->next->weight == 1) {
                leaf->head = h->lhead->next->head;
            } else {
                leaf->head = Huff_allocPtr(h);
                *leaf->head = inner;
            }
        }
        h->lhead->next = leaf;
        leaf->prev = h->lhead;

        leaf->left = NULL;
        leaf->right = NULL;
        inner->left = h->lhead;
        inner->right = leaf;
        if (h->lhead->parent == NULL) {
            h->tree = inner;
        } else if (h->lhead->parent->left == h->lhead) {
            h->lhead->parent->left = inner;
        } else {
            h->lhead->parent->right = inner;
        }
        inner->parent = h->lhead->parent;
        leaf->parent = inner;
        h->lhead->parent = inner;
        h->loc[ch] = leaf;

        Huff_increment(h, inner->parent);
    } else {
        Huff_increment(h, h->loc[ch]);
    }
}

// 0x08066854  size=97  FUN_08066854  status=translated  naming=inferred
// Evidence: walks the tree from the root with bits from the global cursor until a leaf (symbol != 0x101); Quake III Huff_Receive counterpart.
int Huff_Receive(node_s *node, int *ch, const byte *fin)
{
    int result;

    while (node != NULL && node->symbol == COD2_HUFF_INTERNAL) {
        if (Huff_getBit(fin) == 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    if (node == NULL) {
        result = 0;
    } else {
        result = node->symbol;
        *ch = result;
    }
    return result;
}

// 0x080668b6  size=112  FUN_080668b6  status=translated  naming=inferred
// Evidence: offset-carrying variant of Huff_Receive; writes back the bit cursor only when a leaf is reached.
void Huff_offsetReceive(node_s *node, int *ch, const byte *fin, int32 *offset)
{
    huff_bitCursor = *offset;

    while (node != NULL && node->symbol == COD2_HUFF_INTERNAL) {
        if (Huff_getBit(fin) == 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    if (node == NULL) {
        *ch = 0;
    } else {
        *ch = node->symbol;
        *offset = huff_bitCursor;
    }
}

// 0x08066926  size=102  FUN_08066926  status=translated  naming=inferred
// Evidence: recursive emission of the bit path from root to child (Quake III recursiveBitTransmit counterpart).
void Huff_recursiveBitTransmit(node_s *node, node_s *child, byte *fout)
{
    if (node->parent != NULL) {
        Huff_recursiveBitTransmit(node->parent, node, fout);
    }
    if (child != NULL) {
        if (node->right == child) {
            Huff_putBit(1, fout);
        } else {
            Huff_putBit(0, fout);
        }
    }
}

// 0x0806698c  size=132  FUN_0806698c  status=translated  naming=inferred
// Evidence: unknown symbol -> send NYT code then the 8 raw bits; known symbol -> send its tree path (Quake III Huff_transmit counterpart).
void Huff_transmit(huffman_s *h, int ch, byte *fout)
{
    if (h->loc[ch] == NULL) {
        int bit;
        Huff_transmit(h, COD2_HUFF_NYT, fout);
        for (bit = 7; bit >= 0; bit--) {
            Huff_putBit((ch >> bit) & 1, fout);
        }
    } else {
        Huff_recursiveBitTransmit(h->loc[ch], NULL, fout);
    }
}

// 0x08066a10  size=61  FUN_08066a10  status=translated  naming=inferred
// Evidence: tree-path emission for a symbol with an existing loc[] entry, with the bit cursor carried by the caller; no NYT fallback in this variant.
void Huff_offsetTransmitKnown(huffman_s *h, int ch, byte *fout, int32 *offset)
{
    huff_bitCursor = *offset;
    Huff_recursiveBitTransmit(h->loc[ch], NULL, fout);
    *offset = huff_bitCursor;
}

// 0x08066f30  size=405  FUN_08066f30  status=translated  naming=inferred
// Evidence: zeroes both 0x701c-byte halves and seeds each with one NYT node (symbol 0x100, weight 0); Quake III Huff_Init counterpart.
void Huff_Init(huff_s *huff)
{
    huffman_s *comp = &huff->compressor;
    huffman_s *decomp = &huff->decompressor;
    node_s *node;

    memset(comp, 0, 0x701c);
    memset(decomp, 0, 0x701c);

    /* decompressor */
    decomp->loc[COD2_HUFF_NYT] = &decomp->nodeList[decomp->blocNode];
    node = decomp->loc[COD2_HUFF_NYT];
    decomp->blocNode = decomp->blocNode + 1;
    decomp->ltail = node;
    decomp->lhead = node;
    decomp->tree = node;
    decomp->lhead->prev = NULL;
    decomp->lhead->next = NULL;
    decomp->tree->symbol = COD2_HUFF_NYT;
    decomp->tree->weight = 0;
    decomp->tree->right = NULL;
    decomp->tree->left = NULL;
    decomp->tree->parent = NULL;

    /* compressor */
    comp->loc[COD2_HUFF_NYT] = &comp->nodeList[comp->blocNode];
    comp->blocNode = comp->blocNode + 1;
    comp->lhead = comp->loc[COD2_HUFF_NYT];
    comp->tree = comp->loc[COD2_HUFF_NYT];
    comp->tree->symbol = COD2_HUFF_NYT;
    comp->tree->weight = 0;
    comp->lhead->prev = NULL;
    comp->lhead->next = NULL;
    comp->tree->right = NULL;
    comp->tree->left = NULL;
    comp->tree->parent = NULL;
    comp->loc[COD2_HUFF_NYT] = comp->tree;
}


/* ------------------------------------------------------------------------ */
/* MD4 (RFC 1320 structure; constants and round order match the binary).    */
/* ------------------------------------------------------------------------ */
#define COD2_ROTL32(x, s) ((uint32)(((uint32)(x) << (s)) | ((uint32)(x) >> (32 - (s)))))
#define COD2_MD4_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define COD2_MD4_G(x, y, z) (((x) & (y)) | (((x) | (y)) & (z)))
#define COD2_MD4_H(x, y, z) ((x) ^ (y) ^ (z))
#define COD2_MD4_STEP(f, a, b, c, d, xk, s, k) ((a) = COD2_ROTL32((a) + f((b), (c), (d)) + (xk) + (k), s))

// 0x080670c8  size=5  FUN_080670c8  status=translated  naming=unknown
// Evidence: body is a bare return; no string, caller or table identifies it.
void NoOp_080670c8(void)
{
    return;
}

// 0x080670ce  size=5  FUN_080670ce  status=translated  naming=unknown
// Evidence: body is a bare return; no string, caller or table identifies it.
void NoOp_080670ce(void)
{
    return;
}

// 0x080670d4  size=64  FUN_080670d4  status=translated  naming=inferred
// Evidence: standard MD4 initial state 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476; MD4Init counterpart.
void MD4_Init(md4_ctx_s *ctx)
{
    ctx->count[1] = 0;
    ctx->count[0] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

// 0x08067114  size=252  FUN_08067114  status=translated  naming=inferred
// Evidence: MD4Update counterpart: bit-count update, 64-byte block buffering, MD4Transform per full block.
void MD4_Update(md4_ctx_s *ctx, const byte *input, uint32 inputLen)
{
    uint32 index;
    uint32 partLen;
    uint32 i;

    index = (ctx->count[0] >> 3) & 0x3f;
    ctx->count[0] = inputLen * 8 + ctx->count[0];
    if (ctx->count[0] < (inputLen << 3)) {
        ctx->count[1] = ctx->count[1] + 1;
    }
    ctx->count[1] = (inputLen >> 29) + ctx->count[1];

    partLen = 0x40 - index;
    if (inputLen < partLen) {
        i = 0;
    } else {
        memcpy(ctx->buffer + index, input, partLen);
        MD4_Transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 0x3f < inputLen; i = i + 0x40) {
            MD4_Transform(ctx->state, input + i);
        }
        index = 0;
    }
    memcpy(ctx->buffer + index, input + i, inputLen - i);
}

// 0x08067210  size=198  FUN_08067210  status=translated  naming=inferred
// Evidence: MD4Final counterpart: encodes the bit count, pads to 56 mod 64, appends the count, emits the digest, clears the context (memset 0x58).
void MD4_Final(byte digest[16], md4_ctx_s *ctx)
{
    byte bits[8];
    uint32 index;
    int32 padLen;

    MD4_Encode(bits, ctx->count, 8);
    index = (ctx->count[0] >> 3) & 0x3f;
    if (index < 0x38) {
        padLen = 0x38 - index;
    } else {
        padLen = 0x78 - index;
    }
    MD4_Update(ctx, md4_padding, padLen);
    MD4_Update(ctx, bits, 8);
    MD4_Encode(digest, ctx->state, 16);
    memset(ctx, 0, 0x58);
}

// 0x080672d2  size=1804  FUN_080672d2  status=translated  naming=inferred
// Evidence: 48-step MD4 transform (round constants 0, 0x5a827999, 0x6ed9eba1; rotations 3/7/11/19, 3/5/9/13, 3/9/11/15). Counterpart: MD4Transform.
void MD4_Transform(uint32 state[4], const byte block[64])
{
    uint32 x[16];
    uint32 a = state[0];
    uint32 b = state[1];
    uint32 c = state[2];
    uint32 d = state[3];
    int32 i;

    MD4_Decode(x, block, 64);

    /* Round 1: F(b,c,d) = (b & c) | (~b & d) */
    COD2_MD4_STEP(COD2_MD4_F, a, b, c, d, x[0], 3, 0);
    COD2_MD4_STEP(COD2_MD4_F, d, a, b, c, x[1], 7, 0);
    COD2_MD4_STEP(COD2_MD4_F, c, d, a, b, x[2], 11, 0);
    COD2_MD4_STEP(COD2_MD4_F, b, c, d, a, x[3], 19, 0);
    COD2_MD4_STEP(COD2_MD4_F, a, b, c, d, x[4], 3, 0);
    COD2_MD4_STEP(COD2_MD4_F, d, a, b, c, x[5], 7, 0);
    COD2_MD4_STEP(COD2_MD4_F, c, d, a, b, x[6], 11, 0);
    COD2_MD4_STEP(COD2_MD4_F, b, c, d, a, x[7], 19, 0);
    COD2_MD4_STEP(COD2_MD4_F, a, b, c, d, x[8], 3, 0);
    COD2_MD4_STEP(COD2_MD4_F, d, a, b, c, x[9], 7, 0);
    COD2_MD4_STEP(COD2_MD4_F, c, d, a, b, x[10], 11, 0);
    COD2_MD4_STEP(COD2_MD4_F, b, c, d, a, x[11], 19, 0);
    COD2_MD4_STEP(COD2_MD4_F, a, b, c, d, x[12], 3, 0);
    COD2_MD4_STEP(COD2_MD4_F, d, a, b, c, x[13], 7, 0);
    COD2_MD4_STEP(COD2_MD4_F, c, d, a, b, x[14], 11, 0);
    COD2_MD4_STEP(COD2_MD4_F, b, c, d, a, x[15], 19, 0);

    /* Round 2: G(b,c,d) = (b & c) | (b & d) | (c & d) */
    COD2_MD4_STEP(COD2_MD4_G, a, b, c, d, x[0], 3, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, d, a, b, c, x[4], 5, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, c, d, a, b, x[8], 9, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, b, c, d, a, x[12], 13, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, a, b, c, d, x[1], 3, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, d, a, b, c, x[5], 5, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, c, d, a, b, x[9], 9, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, b, c, d, a, x[13], 13, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, a, b, c, d, x[2], 3, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, d, a, b, c, x[6], 5, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, c, d, a, b, x[10], 9, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, b, c, d, a, x[14], 13, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, a, b, c, d, x[3], 3, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, d, a, b, c, x[7], 5, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, c, d, a, b, x[11], 9, 0x5a827999);
    COD2_MD4_STEP(COD2_MD4_G, b, c, d, a, x[15], 13, 0x5a827999);

    /* Round 3: H(b,c,d) = b ^ c ^ d */
    COD2_MD4_STEP(COD2_MD4_H, a, b, c, d, x[0], 3, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, d, a, b, c, x[8], 9, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, c, d, a, b, x[4], 11, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, b, c, d, a, x[12], 15, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, a, b, c, d, x[2], 3, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, d, a, b, c, x[10], 9, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, c, d, a, b, x[6], 11, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, b, c, d, a, x[14], 15, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, a, b, c, d, x[1], 3, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, d, a, b, c, x[9], 9, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, c, d, a, b, x[5], 11, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, b, c, d, a, x[13], 15, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, a, b, c, d, x[3], 3, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, d, a, b, c, x[11], 9, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, c, d, a, b, x[7], 11, 0x6ed9eba1);
    COD2_MD4_STEP(COD2_MD4_H, b, c, d, a, x[15], 15, 0x6ed9eba1);

    state[0] = state[0] + a;
    state[1] = state[1] + b;
    state[2] = state[2] + c;
    state[3] = state[3] + d;

    for (i = 0; i < 16; i++) {
        x[i] = 0;
    }
}

// 0x080679de  size=168  FUN_080679de  status=translated  naming=inferred
// Evidence: little-endian encode of 32-bit words into bytes (MD4Encode counterpart).
void MD4_Encode(byte *output, const uint32 *input, uint32 len)
{
    uint32 j;
    uint32 i = 0;

    for (j = 0; j < len; j = j + 4) {
        output[j] = (byte)input[i];
        output[j + 1] = (byte)(input[i] >> 8);
        output[j + 2] = (byte)(input[i] >> 16);
        output[j + 3] = (byte)(input[i] >> 24);
        i = i + 1;
    }
}

// 0x08067a86  size=124  FUN_08067a86  status=translated  naming=inferred
// Evidence: little-endian decode of bytes into 32-bit words (MD4Decode counterpart).
void MD4_Decode(uint32 *output, const byte *input, uint32 len)
{
    uint32 j;
    uint32 i = 0;

    for (j = 0; j < len; j = j + 4) {
        output[i] = (uint32)input[j] | ((uint32)input[j + 1] << 8) |
                    ((uint32)input[j + 2] << 16) | ((uint32)input[j + 3] << 24);
        i = i + 1;
    }
}

// 0x08067b02  size=92  FUN_08067b02  status=translated  naming=inferred
// Evidence: MD4 over the buffer; returns the XOR of the four digest words (Com_BlockChecksum counterpart).
uint32 Com_BlockChecksum(const void *buffer, int length)
{
    md4_ctx_s ctx;
    byte digest[16];
    uint32 words[4];

    MD4_Init(&ctx);
    MD4_Update(&ctx, (const byte *)buffer, (uint32)length);
    MD4_Final(digest, &ctx);
    MD4_Decode(words, digest, 16);
    return words[3] ^ words[0] ^ words[2] ^ words[1];
}

// 0x08067b5e  size=121  FUN_08067b5e  status=translated  naming=inferred
// Evidence: as Com_BlockChecksum but hashes a 4-byte key first (Com_BlockChecksumKey counterpart).
uint32 Com_BlockChecksumKey(const void *buffer, int length, int key)
{
    md4_ctx_s ctx;
    byte digest[16];
    uint32 words[4];
    int32 keyValue = key;

    MD4_Init(&ctx);
    MD4_Update(&ctx, (const byte *)&keyValue, 4);
    MD4_Update(&ctx, (const byte *)buffer, (uint32)length);
    MD4_Final(digest, &ctx);
    MD4_Decode(words, digest, 16);
    return words[3] ^ words[0] ^ words[2] ^ words[1];
}

/* ------------------------------------------------------------------------ */
/* Bit-level message buffer (msg_s). Naming: Quake III msg.c counterpart.    */
/* ------------------------------------------------------------------------ */

// 0x08067bd8  size=67  FUN_08067bd8  status=translated  naming=inferred
// Evidence: zeroes the 0x18-byte buffer header, stores data and maxsize; MSG_Init counterpart. Calls a one-time init guarded by msg_huffmanInitialized.
void MSG_Init(msg_s *msg, byte *data, int32 maxsize)
{
    if (msg_huffmanInitialized == 0) {
        MSG_InitHuffmanOnce();
    }
    memset(msg, 0, 0x18);
    msg->data = data;
    msg->maxsize = maxsize;
}

// 0x08067c1c  size=34  FUN_08067c1c  status=translated  naming=inferred
// Evidence: resets read cursors (readcount, bit) and the overflow flag; MSG_BeginReading counterpart.
void MSG_BeginReading(msg_s *msg)
{
    msg->overflowed = 0;
    msg->readcount = 0;
    msg->bit = 0;
}

// 0x08067c3e  size=36  FUN_08067c3e  status=translated  naming=unknown
// Evidence: returns cursize*8 minus the unused bits of the last byte, derived from the bit cursor; no string names it.
int MSG_BitCount(const msg_s *msg)
{
    return msg->cursize * 8 - (int32)((8u - (uint32)msg->bit) & 7u);
}

// 0x08067c62  size=198  FUN_08067c62  status=translated  naming=inferred
// Evidence: writes the low 'bits' bits of value LSB first; overflow test is a fixed 4-byte margin; MSG_WriteBits counterpart.
void MSG_WriteBits(msg_s *msg, uint32 value, int bits)
{
    if ((msg->maxsize - msg->cursize) < 4) {
        msg->overflowed = 1;
        return;
    }
    while (bits != 0) {
        int32 oldBit;
        bits = bits - 1;
        oldBit = msg->bit;
        if ((oldBit & 7) == 0) {
            msg->bit = msg->cursize << 3;
            msg->data[msg->cursize] = 0;
            msg->cursize = msg->cursize + 1;
        }
        if ((value & 1) != 0) {
            msg->data[msg->bit >> 3] = (byte)(1 << (oldBit & 7)) | msg->data[msg->bit >> 3];
        }
        msg->bit = msg->bit + 1;
        value = (uint32)((int32)value >> 1);
    }
}

// 0x08067d28  size=93  FUN_08067d28  status=translated  naming=inferred
// Evidence: writes a single 0 bit or sets overflow; MSG_WriteBit0 counterpart.
void MSG_WriteBit0(msg_s *msg)
{
    if (msg->cursize < msg->maxsize) {
        if ((msg->bit & 7) == 0) {
            msg->bit = msg->cursize << 3;
            msg->data[msg->cursize] = 0;
            msg->cursize = msg->cursize + 1;
        }
        msg->bit = msg->bit + 1;
    } else {
        msg->overflowed = 1;
    }
}

// 0x08067d86  size=153  FUN_08067d86  status=translated  naming=inferred
// Evidence: writes a single 1 bit or sets overflow; MSG_WriteBit1 counterpart.
void MSG_WriteBit1(msg_s *msg)
{
    if (msg->cursize < msg->maxsize) {
        int32 oldBit = msg->bit;
        if ((oldBit & 7) == 0) {
            msg->bit = msg->cursize << 3;
            msg->data[msg->cursize] = 0;
            msg->cursize = msg->cursize + 1;
        }
        msg->data[msg->bit >> 3] = (byte)(1 << (oldBit & 7)) | msg->data[msg->bit >> 3];
        msg->bit = msg->bit + 1;
    } else {
        msg->overflowed = 1;
    }
}

// 0x08067e20  size=174  FUN_08067e20  status=translated  naming=inferred
// Evidence: reads 'bits' bits LSB first; returns -1 and sets overflow when the buffer is exhausted; MSG_ReadBits counterpart.
int MSG_ReadBits(msg_s *msg, int bits)
{
    uint32 result = 0;
    int32 i = 0;

    for (;;) {
        int32 oldBit;
        if (bits <= i) {
            return (int)result;
        }
        oldBit = msg->bit;
        if ((oldBit & 7) == 0) {
            if (msg->cursize <= msg->readcount) {
                msg->overflowed = 1;
                return -1;
            }
            msg->bit = msg->readcount << 3;
            msg->readcount = msg->readcount + 1;
        }
        result = result | ((((uint32)msg->data[msg->bit >> 3]) >> (oldBit & 7)) & 1u) << (i & 0x1f);
        msg->bit = msg->bit + 1;
        i = i + 1;
    }
}

// 0x08067ece  size=127  FUN_08067ece  status=translated  naming=inferred
// Evidence: reads one bit; returns -1 and sets overflow on exhaustion; MSG_ReadBit counterpart.
int MSG_ReadBit(msg_s *msg)
{
    uint32 oldBit = (uint32)msg->bit;
    byte value;
    int32 shift;

    if ((oldBit & 7) == 0) {
        if (msg->cursize <= msg->readcount) {
            msg->overflowed = 1;
            return -1;
        }
        msg->bit = msg->readcount << 3;
        msg->readcount = msg->readcount + 1;
    }
    value = msg->data[msg->bit >> 3];
    shift = (int32)(oldBit & 7);
    msg->bit = msg->bit + 1;
    return ((int)(uint32)value >> shift) & 1;
}


// 0x08067f4e  size=84  FUN_08067f4e  status=translated  naming=inferred
// Evidence: encodes a byte block with the compressor tree (no adaptation); returns the number of bytes produced.
int Huff_CompressBytes(const byte *in, byte *out, int count)
{
    int32 bitOffset = 0;
    int32 remaining;

    for (remaining = count; remaining != 0; remaining = remaining - 1) {
        Huff_offsetTransmitKnown(&msgHuff.compressor, *in, out, &bitOffset);
        in = in + 1;
    }
    return (bitOffset + 7) >> 3;
}

// 0x08067fa2  size=105  FUN_08067fa2  status=translated  naming=inferred
// Evidence: decodes bits from the input until compressedLen*8 bits are consumed; Quake III Huff_Decompress counterpart.
int Huff_DecompressBytes(const byte *in, byte *out, int compressedLen)
{
    byte *outStart = out;
    int32 bitLimit = compressedLen << 3;
    int32 bitOffset = 0;
    int ch = 0;

    while (bitOffset < bitLimit) {
        Huff_offsetReceive(msgHuff.decompressor.tree, &ch, in, &bitOffset);
        *out = (byte)ch;
        out = out + 1;
    }
    return (int)(out - outStart);
}

// 0x0806800c  size=56  FUN_0806800c  status=translated  naming=inferred
// Evidence: appends one raw byte after an overflow check; MSG_WriteByte counterpart.
void MSG_WriteByte(msg_s *msg, int c)
{
    if (msg->cursize < msg->maxsize) {
        msg->data[msg->cursize] = (byte)c;
        msg->cursize = msg->cursize + 1;
    } else {
        msg->overflowed = 1;
    }
}

// 0x08068044  size=85  FUN_08068044  status=translated  naming=inferred
// Evidence: appends a raw block (memcpy) after an overflow check; MSG_WriteData counterpart.
void MSG_WriteData(msg_s *msg, const void *data, int length)
{
    int32 start = msg->cursize;
    if (msg->maxsize < start + length) {
        msg->overflowed = 1;
    } else {
        memcpy(msg->data + start, data, (size_t)length);
        msg->cursize = start + length;
    }
}

// 0x0806809a  size=80  FUN_0806809a  status=translated  naming=inferred
// Evidence: writes a 16-bit value through the byte-order helper; MSG_WriteShort counterpart.
void MSG_WriteShort(msg_s *msg, int c)
{
    int32 start = msg->cursize;
    if (msg->maxsize < start + 2) {
        msg->overflowed = 1;
    } else {
        int16 value = LittleShort_0806b1b8(c);
        memcpy(msg->data + start, &value, 2);
        msg->cursize = start + 2;
    }
}

// 0x080680ea  size=78  FUN_080680ea  status=translated  naming=inferred
// Evidence: writes a 32-bit value through the byte-order helper; MSG_WriteLong counterpart.
void MSG_WriteLong(msg_s *msg, int c)
{
    int32 start = msg->cursize;
    if (msg->maxsize < start + 4) {
        msg->overflowed = 1;
    } else {
        int32 value = LittleLong_0806b1cc(c);
        memcpy(msg->data + start, &value, 4);
        msg->cursize = start + 4;
    }
}

// 0x08068138  size=107  FUN_08068138  status=translated  naming=inferred
// Evidence: writes a 64-bit value assembled from two words; MSG_WriteInt64 (name inferred from width).
void MSG_WriteInt64(msg_s *msg, uint32 lo, uint32 hi)
{
    int32 start = msg->cursize;
    if (msg->maxsize < start + 8) {
        msg->overflowed = 1;
    } else {
        uint64 value = BigLong64(((uint64)hi << 32) | lo);
        memcpy(msg->data + start, &value, 8);
        msg->cursize = start + 8;
    }
}

// 0x080681a4  size=178  FUN_080681a4  status=translated  naming=observed
// Evidence: string "MSG_WriteString: MAX_STRING_CHARS" is in this function body.
void MSG_WriteString(msg_s *msg, const char *s)
{
    char buffer[1032];
    size_t len;
    size_t i;

    len = strlen(s);
    if ((int32)len < 0x400) {
        for (i = 0; i < len; i++) {
            buffer[i] = Q_NormalizeApostrophe((int)s[i]);
        }
        buffer[len] = 0;
        MSG_WriteData(msg, buffer, (int)len + 1);
    } else {
        Com_Printf("MSG_WriteString: MAX_STRING_CHARS");
        MSG_WriteData(msg, str_empty, 1);
    }
}

// 0x08068256  size=198  FUN_08068256  status=translated  naming=inferred
// Evidence: string "MSG_WriteString: BIG_INFO_STRING" is in this function body; the big-buffer variant of MSG_WriteString.
void MSG_WriteBigString(msg_s *msg, const char *s)
{
    char buffer[8200];
    size_t len;
    size_t i;

    len = strlen(s);
    if ((int32)len < 0x2000) {
        Q_strncpyz(buffer, s, 0x2000);
        for (i = 0; i < len; i++) {
            buffer[i] = Q_NormalizeApostrophe((int)buffer[i]);
        }
        MSG_WriteData(msg, buffer, (int)len + 1);
    } else {
        Com_Printf("MSG_WriteString: BIG_INFO_STRING");
        MSG_WriteData(msg, str_empty, 1);
    }
}

// 0x0806831c  size=74  FUN_0806831c  status=translated  naming=inferred
// Evidence: 8-bit angle, (angle*256/360) truncated (x87 fistp with RC=truncate in the binary); MSG_WriteAngle8 counterpart.
void MSG_WriteAngle8(msg_s *msg, float angle)
{
    MSG_WriteByte(msg, (int)((angle * 256.0f) / 360.0f) & 0xff);
}

// 0x08068366  size=66  FUN_08068366  status=translated  naming=inferred
// Evidence: 16-bit angle, truncation of angle*182.04445 (float constant at 0x0814142c); MSG_WriteAngle16 counterpart.
void MSG_WriteAngle16(msg_s *msg, float angle)
{
    MSG_WriteShort(msg, (int)(angle * 182.04445f) & 0xffff);
}

// 0x080683a8  size=73  FUN_080683a8  status=translated  naming=inferred
// Evidence: reads one unsigned byte or sets overflow and returns -1; MSG_ReadByte counterpart.
int MSG_ReadByte(msg_s *msg)
{
    int result;
    if (msg->readcount < msg->cursize) {
        result = (int)msg->data[msg->readcount];
        msg->readcount = msg->readcount + 1;
    } else {
        msg->overflowed = 1;
        result = -1;
    }
    return result;
}

// 0x080683f2  size=94  FUN_080683f2  status=translated  naming=inferred
// Evidence: reads a 16-bit value through the byte-order helper, sign-extended; MSG_ReadShort counterpart.
int MSG_ReadShort(msg_s *msg)
{
    int32 start = msg->readcount;
    int result;
    if (msg->cursize < start + 2) {
        msg->overflowed = 1;
        result = -1;
    } else {
        int16 raw;
        int16 value;
        memcpy(&raw, msg->data + start, 2);
        value = LittleShort_0806b1b8((int)raw);
        result = (int)value;
        msg->readcount = start + 2;
    }
    return result;
}

// 0x08068450  size=92  FUN_08068450  status=translated  naming=inferred
// Evidence: reads a 32-bit value through the byte-order helper; MSG_ReadLong counterpart.
int MSG_ReadLong(msg_s *msg)
{
    int32 start = msg->readcount;
    int result;
    if (msg->cursize < start + 4) {
        msg->overflowed = 1;
        result = -1;
    } else {
        int32 raw;
        memcpy(&raw, msg->data + start, 4);
        result = LittleLong_0806b1cc((int)raw);
        msg->readcount = start + 4;
    }
    return result;
}

// 0x080684ac  size=118  FUN_080684ac  status=translated  naming=inferred
// Evidence: reads two 32-bit words and combines them into 64 bits; MSG_ReadInt64 counterpart. Returns 0 on overflow.
uint64 MSG_ReadInt64(msg_s *msg)
{
    int32 start = msg->readcount;
    uint64 result;
    if (msg->cursize < start + 8) {
        msg->overflowed = 1;
        result = 0;
    } else {
        uint32 words[2];
        memcpy(words, msg->data + start, 8);
        result = BigLong64(((uint64)words[1] << 32) | words[0]);
        msg->readcount = start + 8;
    }
    return result;
}

// 0x08068522  size=106  FUN_08068522  status=translated  naming=inferred
// Evidence: reads bytes until NUL into a 1024-byte static buffer, mapping read errors to NUL; MSG_ReadString counterpart.
char *MSG_ReadString(msg_s *msg)
{
    uint32 count = 0;
    int ch;

    for (;;) {
        ch = MSG_ReadByte(msg);
        if (ch == -1) {
            ch = 0;
        }
        if (count < 0x400) {
            msg_readStringBuf[count] = Q_NormalizeApostrophe((int)(char)ch);
        }
        if (ch == 0) {
            break;
        }
        count = count + 1;
    }
    msg_readStringBuf[1023] = 0;
    return msg_readStringBuf;
}

// 0x0806858c  size=121  FUN_0806858c  status=translated  naming=inferred
// Evidence: big-buffer string read (8 KiB limit) that maps '%' to '.'; MSG_ReadBigString counterpart.
char *MSG_ReadBigString(msg_s *msg)
{
    uint32 count = 0;
    int ch;

    for (;;) {
        ch = MSG_ReadByte(msg);
        if (ch == 0x25) {
            ch = 0x2e;
        } else if (ch == -1) {
            ch = 0;
        }
        if (count < 0x2000) {
            msg_readBigStringBuf[count] = Q_NormalizeApostrophe((int)(char)ch);
        }
        if (ch == 0) {
            break;
        }
        count = count + 1;
    }
    msg_readBigStringBuf[8191] = 0;
    return msg_readBigStringBuf;
}

// 0x08068606  size=129  FUN_08068606  status=translated  naming=inferred
// Evidence: line-terminated string read (stops at '\n' or read error), '%' mapped to '.'; MSG_ReadStringLine counterpart.
char *MSG_ReadStringLine(msg_s *msg)
{
    uint32 count = 0;
    int ch;

    for (;;) {
        ch = MSG_ReadByte(msg);
        if (ch == 0x25) {
            ch = 0x2e;
        } else if (ch == 10 || ch == -1) {
            ch = 0;
        }
        if (count < 0x400) {
            msg_readStringLineBuf[count] = Q_NormalizeApostrophe((int)(char)ch);
        }
        if (ch == 0) {
            break;
        }
        count = count + 1;
    }
    msg_readStringLineBuf[1023] = 0;
    return msg_readStringLineBuf;
}

// 0x08068688  size=35  FUN_08068688  status=translated  naming=inferred
// Evidence: reads a 16-bit angle and scales by 360/65536 (float constant 0x08141430 = 0.0054931640625); MSG_ReadAngle16 counterpart.
float MSG_ReadAngle16(msg_s *msg)
{
    int value = MSG_ReadShort(msg);
    return (float)value * 0.0054931640625f;
}

// 0x080686ac  size=111  FUN_080686ac  status=translated  naming=inferred
// Evidence: copies a raw block out after an overflow check (fills 0xff on overflow); MSG_ReadData counterpart.
void MSG_ReadData(msg_s *msg, void *dst, int length)
{
    int32 start = msg->readcount;
    if (msg->cursize < start + length) {
        msg->overflowed = 1;
        memset(dst, -1, (size_t)length);
    } else {
        memcpy(dst, msg->data + start, (size_t)length);
        msg->readcount = start + length;
    }
}

// 0x0806871c  size=68  FUN_0806871c  status=translated  naming=inferred
// Evidence: key-xored changed-bit delta of 'bits' bits; Quake III MSG_WriteDeltaKey counterpart.
void MSG_WriteDeltaKey(msg_s *msg, uint32 key, uint32 oldValue, uint32 newValue, int bits)
{
    if (oldValue == newValue) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        MSG_WriteBits(msg, key ^ newValue, bits);
    }
}

// 0x08068760  size=72  FUN_08068760  status=translated  naming=inferred
// Evidence: reads the MSG_WriteDeltaKey encoding; Quake III MSG_ReadDeltaKey counterpart (mask table msg_lowBitMasks).
uint32 MSG_ReadDeltaKey(msg_s *msg, uint32 key, uint32 oldValue, int bits)
{
    uint32 result;
    if (MSG_ReadBit(msg) == 0) {
        result = oldValue;
    } else {
        result = (uint32)MSG_ReadBits(msg, bits) ^ (msg_lowBitMasks[bits] & key);
    }
    return result;
}

// 0x080687a8  size=36  FUN_080687a8  status=translated  naming=inferred
// Evidence: writes 'bits' bits of (key ^ value); keyed-bit helper paired with MSG_ReadKeyedBits.
void MSG_WriteKeyedBits(msg_s *msg, uint32 key, uint32 value, int bits)
{
    MSG_WriteBits(msg, key ^ value, bits);
}

// 0x080687cc  size=45  FUN_080687cc  status=translated  naming=inferred
// Evidence: reads 'bits' bits and xors them with the masked key; paired with MSG_WriteKeyedBits.
uint32 MSG_ReadKeyedBits(msg_s *msg, uint32 key, int bits)
{
    return (uint32)MSG_ReadBits(msg, bits) ^ (msg_lowBitMasks[bits] & key);
}

// 0x080687fa  size=62  FUN_080687fa  status=translated  naming=inferred
// Evidence: keyed byte delta: changed bit then key-xored byte; Quake III-style delta helper.
void MSG_WriteDeltaKeyByte(msg_s *msg, uint32 key, int8 oldValue, uint32 newValue)
{
    if (oldValue == (int8)newValue) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        MSG_WriteByte(msg, (int)(key ^ newValue));
    }
}

// 0x08068838  size=54  FUN_08068838  status=translated  naming=inferred
// Evidence: reader of MSG_WriteDeltaKeyByte; the key is a byte in this function.
uint32 MSG_ReadDeltaKeyByte(msg_s *msg, uint8 key, uint32 oldValue)
{
    uint32 result;
    if (MSG_ReadBit(msg) == 0) {
        result = oldValue;
    } else {
        byte value = (byte)MSG_ReadByte(msg);
        result = (uint32)(value ^ key);
    }
    return result;
}

// 0x0806886e  size=62  FUN_0806886e  status=translated  naming=inferred
// Evidence: keyed 16-bit delta: changed bit then key-xored short; Quake III-style delta helper.
void MSG_WriteDeltaKeyShort(msg_s *msg, uint32 key, int16 oldValue, uint32 newValue)
{
    if (oldValue == (int16)newValue) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        MSG_WriteShort(msg, (int)(key ^ newValue));
    }
}

// 0x080688ac  size=56  FUN_080688ac  status=translated  naming=inferred
// Evidence: reader of MSG_WriteDeltaKeyShort; returns the sign-extended key-xored short.
int MSG_ReadDeltaKeyShort(msg_s *msg, uint16 key, int oldValue)
{
    int result;
    if (MSG_ReadBit(msg) == 0) {
        result = oldValue;
    } else {
        uint16 value = (uint16)MSG_ReadShort(msg);
        result = (int)(int16)(key ^ value);
    }
    return result;
}

// 0x080688e4  size=194  FUN_080688e4  status=translated  naming=inferred
// Evidence: strings "WARNING: Reliable command is too long" and "WARNING: Empty reliable command" in the body; copies a command with char mapping and '%' -> '.'.
void CopyReliableCommand(const char *src, char *dst, int dstSize)
{
    size_t len = strlen(src);
    char *out = dst;
    int32 i;

    if (dstSize <= (int32)len) {
        Com_Printf("WARNING: Reliable command is too long (%i/%i) and will be truncated: \'%s\'\n",
                    (int)len, dstSize, src);
    }
    if (len == 0) {
        Com_Printf("WARNING: Empty reliable command\n");
    }

    for (i = 0; i < dstSize && src[i] != '\0'; i++) {
        char c = Q_NormalizeApostrophe((int)src[i]);
        *out = c;
        if (*out == '%') {
            *out = '.';
        }
        out = out + 1;
    }

    if (i < dstSize) {
        dst[i] = '\0';
    } else {
        dst[dstSize - 1] = '\0';
    }
}

// 0x080689a6  size=377  FUN_080689a6  status=translated  naming=inferred
// Evidence: builds a 28-byte usercmd from a player state (delta angles, pm flags, view angles); Quake III CL_CreateCmd counterpart.
void CL_CreateCmd(const playerState_s *ps, usercmd_s *cmd)
{
    int32 i;

    memset(cmd, 0, 0x1c);
    cmd->weapon = ps->weapon;
    cmd->offHandIndex = ps->offHandIndex;
    for (i = 0; i < 2; i++) {
        uint32 angle = (uint32)((int32)(ps->viewangles[i] * 182.04445f) & 0xffff);
        cmd->angles[i] = (angle - (uint32)ps->delta_angles[i]) & 0xffff;
    }

    if ((ps->pm_flags & 0x800000) != 0) {
        if ((ps->eFlags & 8) == 0) {
            if ((ps->eFlags & 4) != 0) {
                cmd->buttons = cmd->buttons | 0x200;
            }
        } else {
            cmd->buttons = cmd->buttons | 0x100;
        }
        if (0.0f < ps->leanf) {
            cmd->buttons = cmd->buttons | 0x80;
        } else if (ps->leanf < 0.0f) {
            cmd->buttons = cmd->buttons | 0x40;
        }
        if (ps->fWeaponPosFrac != 0.0f || ps->fWeaponPosFrac != ps->fWeaponPosFrac) {
            cmd->buttons = cmd->buttons | 0x1000;
        }
    }
}

// 0x08068b20  size=70  FUN_08068b20  status=translated  naming=unknown
// Evidence: classifies a signed move pair into 4 flags using thresholds of +/-10 (usercmd forwardmove/rightmove); no string names it.
uint32 UserCmd_MoveClass(int forward, int right)
{
    uint32 cls = 0;

    if (forward < 0xb) {
        if (forward < -10) {
            cls = 2;
        }
    } else {
        cls = 1;
    }
    if (right < 0xb) {
        if (right < -10) {
            cls = cls | 8;
        }
    } else {
        cls = cls | 4;
    }
    return cls;
}

// 0x08068b66  size=89  FUN_08068b66  status=translated  naming=unknown
// Evidence: inverse of UserCmd_MoveClass; maps class flags back to +127 / -127 / 0 move values.
void UserCmd_MoveFromClass(uint32 cls, int8 *forward, int8 *right)
{
    if ((cls & 1) != 0) {
        *forward = 0x7f;
    } else if ((cls & 2) != 0) {
        *forward = (int8)0x81;
    } else {
        *forward = 0;
    }

    if ((cls & 4) != 0) {
        *right = 0x7f;
    } else if ((cls & 8) != 0) {
        *right = (int8)0x81;
    } else {
        *right = 0;
    }
}

// 0x08068bc0  size=990  FUN_08068bc0  status=translated  naming=inferred
// Evidence: delta-codes a usercmd (serverTime, buttons, angles, weapon, move classes) under a key; Quake III MSG_WriteDeltaUsercmdKey counterpart.
void MSG_WriteDeltaUsercmdKey(msg_s *msg, uint32 key, const usercmd_s *from, const usercmd_s *to)
{
    uint32 timeDelta = to->serverTime - from->serverTime;
    uint32 toClass;
    uint32 fromClass;

    if (timeDelta < 0x100) {
        MSG_WriteBit1(msg);
        MSG_WriteByte(msg, (int)timeDelta);
    } else {
        MSG_WriteBit0(msg);
        MSG_WriteLong(msg, to->serverTime);
    }

    toClass = UserCmd_MoveClass((int)to->forwardmove, (int)to->rightmove);
    fromClass = UserCmd_MoveClass((int)from->forwardmove, (int)from->rightmove);

    if ((((int32)from->buttons >> 1) == ((int32)to->buttons >> 1)) &&
        ((int8)from->weapon == (int8)to->weapon) &&
        ((int8)from->offHandIndex == (int8)to->offHandIndex) &&
        ((int32)from->angles[2] == (int32)to->angles[2])) {
        if (((from->angles[0] == to->angles[0]) && (from->angles[1] == to->angles[1])) &&
            (((from->buttons & 1) == (to->buttons & 1)) && (fromClass == toClass))) {
            MSG_WriteKeyedBits(msg, key, 0, 1);
        } else {
            MSG_WriteKeyedBits(msg, key, 1, 1);
            MSG_WriteKeyedBits(msg, key, 0, 1);
            key = key ^ to->serverTime;
            MSG_WriteKeyedBits(msg, key, to->buttons, 1);
            MSG_WriteDeltaKeyShort(msg, key, (int16)from->angles[0], to->angles[0]);
            MSG_WriteDeltaKeyShort(msg, key, (int16)from->angles[1], to->angles[1]);
            MSG_WriteDeltaKey(msg, key, fromClass, toClass, 4);
        }
    } else {
        MSG_WriteKeyedBits(msg, key, 1, 1);
        MSG_WriteKeyedBits(msg, key, 1, 1);
        MSG_WriteKeyedBits(msg, key, to->buttons, 1);
        MSG_WriteDeltaKeyShort(msg, key, (int16)from->angles[0], to->angles[0]);
        MSG_WriteDeltaKeyShort(msg, key, (int16)from->angles[1], to->angles[1]);
        MSG_WriteDeltaKey(msg, key, fromClass, toClass, 4);
        key = key ^ to->serverTime;
        MSG_WriteDeltaKeyShort(msg, key, (int16)from->angles[2], to->angles[2]);
        MSG_WriteDeltaKey(msg, key, (uint32)((int32)from->buttons >> 1),
                          (uint32)((int32)to->buttons >> 1), 0x12);
        MSG_WriteDeltaKey(msg, key, (uint32)(int32)(int8)from->weapon,
                          (uint32)(int32)(int8)to->weapon, 7);
        MSG_WriteDeltaKey(msg, key, (uint32)(int32)(int8)from->offHandIndex,
                          (uint32)(int32)(int8)to->offHandIndex, 7);
    }
}

// 0x08068f9e  size=830  FUN_08068f9e  status=translated  naming=inferred
// Evidence: reader of MSG_WriteDeltaUsercmdKey (same key sequencing and class tables); Quake III MSG_ReadDeltaUsercmdKey counterpart.
void MSG_ReadDeltaUsercmdKey(msg_s *msg, uint32 key, const usercmd_s *from, usercmd_s *to)
{
    *to = *from;

    if (MSG_ReadBit(msg) == 0) {
        to->serverTime = MSG_ReadLong(msg);
    } else {
        to->serverTime = MSG_ReadByte(msg) + from->serverTime;
    }

    if (MSG_ReadKeyedBits(msg, key, 1) != 0) {
        to->buttons = to->buttons & 0xfffffffe;
        if (MSG_ReadKeyedBits(msg, key, 1) == 0) {
            uint32 cls;
            key = key ^ to->serverTime;
            to->buttons = MSG_ReadKeyedBits(msg, key, 1) | to->buttons;
            to->angles[0] = MSG_ReadDeltaKeyShort(msg, (uint16)key, (int)from->angles[0]) & 0xffff;
            to->angles[1] = MSG_ReadDeltaKeyShort(msg, (uint16)key, (int)from->angles[1]) & 0xffff;
            cls = UserCmd_MoveClass((int)from->forwardmove, (int)from->rightmove);
            cls = MSG_ReadDeltaKey(msg, key, cls, 4);
            UserCmd_MoveFromClass(cls, &to->forwardmove, &to->rightmove);
        } else {
            uint32 cls;
            to->buttons = MSG_ReadKeyedBits(msg, key, 1) | to->buttons;
            to->angles[0] = MSG_ReadDeltaKeyShort(msg, (uint16)key, (int)from->angles[0]) & 0xffff;
            to->angles[1] = MSG_ReadDeltaKeyShort(msg, (uint16)key, (int)from->angles[1]) & 0xffff;
            cls = UserCmd_MoveClass((int)from->forwardmove, (int)from->rightmove);
            cls = MSG_ReadDeltaKey(msg, key, cls, 4);
            UserCmd_MoveFromClass(cls, &to->forwardmove, &to->rightmove);
            key = key ^ to->serverTime;
            to->angles[2] = MSG_ReadDeltaKeyShort(msg, (uint16)key, (int)from->angles[2]) & 0xffff;
            to->buttons = to->buttons & 1;
            to->buttons = MSG_ReadDeltaKey(msg, key, (uint32)((int32)from->buttons >> 1), 0x12) * 2 |
                          to->buttons;
            to->weapon = (uint8)MSG_ReadDeltaKey(msg, key, (uint32)(int32)(int8)from->weapon, 7);
            to->offHandIndex = (uint8)MSG_ReadDeltaKey(msg, key, (uint32)(int32)(int8)from->offHandIndex, 7);
        }
    }
}

// 0x080692dc  size=805  FUN_080692dc  status=translated  naming=inferred
// Evidence: Quake III MSG_WriteDeltaField counterpart: netField (name, offset, bits) table driven; bits 0/-99/-100 select special float encodings.
// Re-translated from the disassembly (boot-parity lane): the change test and the integer/zero tests compare the
// raw 32-bit words; only the float encodings convert (fistp with truncation, compared back in x87 precision);
// the long fallback writes the raw word, and the bit-field path shifts the raw integer value.
void MSG_WriteDeltaField(msg_s *msg, const void *from, const void *to, const netField_s *field)
{
    const int32 *fromWord = (const int32 *)((const byte *)from + field->offset);
    const int32 *toWord = (const int32 *)((const byte *)to + field->offset);
    float value;
    int32 truncated;
    int32 intValue;
    int32 absBits;
    int32 remaining;
    int32 lowBits;

    if (*fromWord == *toWord) {
        MSG_WriteBit0(msg);
        return;
    }
    MSG_WriteBit1(msg);

    if (field->bits == 0) {
        value = *(const float *)toWord;
        truncated = (int32)value;
        if (value == 0.0f) {
            MSG_WriteBit0(msg);
            return;
        }
        MSG_WriteBit1(msg);
        if ((long double)truncated == (long double)value && truncated + 0x1000 >= 0 && truncated + 0x1000 <= 0x1fff) {
            MSG_WriteBit0(msg);
            truncated += 0x1000;
            MSG_WriteBits(msg, (uint32)truncated, 5);
            MSG_WriteByte(msg, truncated >> 5);
        } else {
            MSG_WriteBit1(msg);
            MSG_WriteLong(msg, *toWord);
        }
    } else if (field->bits == -99) {
        value = *(const float *)toWord;
        truncated = (int32)value;
        if (value == 0.0f) {
            MSG_WriteBit0(msg);
            return;
        }
        MSG_WriteBit1(msg);
        if ((long double)truncated == (long double)value && truncated + 0x200 >= 0 && truncated + 0x200 <= 0x3ff) {
            MSG_WriteBit0(msg);
            truncated += 0x200;
            MSG_WriteBits(msg, (uint32)truncated, 2);
            MSG_WriteByte(msg, truncated >> 2);
        } else {
            MSG_WriteBit1(msg);
            MSG_WriteLong(msg, *toWord);
        }
    } else if (field->bits == -100) {
        if (*toWord == 0) {
            MSG_WriteBit0(msg);
            return;
        }
        MSG_WriteBit1(msg);
        MSG_WriteAngle16(msg, *(const float *)toWord);
    } else {
        if (*toWord == 0) {
            MSG_WriteBit0(msg);
            return;
        }
        MSG_WriteBit1(msg);
        intValue = *toWord;
        absBits = field->bits;
        if (absBits < 0) {
            absBits = -absBits;
        }
        remaining = absBits;
        lowBits = absBits & 7;
        if (lowBits != 0) {
            MSG_WriteBits(msg, (uint32)intValue, lowBits);
            remaining -= lowBits;
            intValue >>= (byte)lowBits;
        }
        while (remaining != 0) {
            MSG_WriteByte(msg, intValue);
            intValue >>= 8;
            remaining -= 8;
        }
    }
}

// 0x08069602  size=186  FUN_08069602  status=translated  naming=inferred
// Evidence: writes one changed bit for a field group (or nothing when unchanged and not forced), then each field; Quake III field-group counterpart.
void MSG_WriteDeltaFieldGroup(msg_s *msg, const void *from, const void *to, int force,
                              int count, const netField_s *fields)
{
    int32 i;
    int changed = 0;

    if (force == 0) {
        for (i = 0; i < count; i++) {
            if (*(const int32 *)((const byte *)from + fields[i].offset) !=
                *(const int32 *)((const byte *)to + fields[i].offset)) {
                changed = 1;
                break;
            }
        }
        if (!changed) {
            MSG_WriteBit0(msg);
            return;
        }
    }

    MSG_WriteBit1(msg);
    for (i = 0; i < count; i++) {
        MSG_WriteDeltaField(msg, from, to, &fields[i]);
    }
}

// 0x080696bc  size=400  FUN_080696bc  status=translated  naming=inferred
// Evidence: Quake III MSG_WriteDeltaStruct counterpart: 'to' entries carry their number in the first word; removals write the number alone.
void MSG_WriteDeltaStruct(msg_s *msg, const void *from, const void *to, int force,
                          int numFields, int numBits, const netField_s *fields, int print)
{
    int32 lastChanged = 0;
    int32 i;

    if (to == NULL) {
        if (print != 0) {
            MSG_WriteBit1(msg);
        }
        MSG_WriteBits(msg, (uint32)*(const int32 *)from, numBits);
        MSG_WriteBit1(msg);
        return;
    }

    for (i = 0; i < numFields; i++) {
        if (*(const int32 *)((const byte *)from + fields[i].offset) !=
            *(const int32 *)((const byte *)to + fields[i].offset)) {
            lastChanged = i + 1;
        }
    }

    if (lastChanged == 0) {
        if (force != 0) {
            if (print != 0) {
                MSG_WriteBit1(msg);
            }
            MSG_WriteBits(msg, (uint32)*(const int32 *)to, numBits);
            MSG_WriteBit0(msg);
            MSG_WriteBit0(msg);
        }
    } else {
        if (print != 0) {
            MSG_WriteBit1(msg);
        }
        MSG_WriteBits(msg, (uint32)*(const int32 *)to, numBits);
        MSG_WriteBit0(msg);
        MSG_WriteBit1(msg);
        MSG_WriteByte(msg, lastChanged);
        for (i = 0; i < lastChanged; i++) {
            MSG_WriteDeltaField(msg, from, to, &fields[i]);
        }
    }
}

// 0x0806984c  size=78  FUN_0806984c  status=translated  naming=inferred
// Evidence: entity-state delta: 59 fields (table starts with "pos.trTime") at 10 index bits; Quake III MSG_WriteDeltaEntity counterpart.
void MSG_WriteDeltaEntity(msg_s *msg, const void *from, const void *to, int force)
{
    MSG_WriteDeltaStruct(msg, from, to, force, 0x3b, 10, entityStateFields, 0);
}

// 0x0806989a  size=78  FUN_0806989a  status=translated  naming=inferred
// Evidence: shared-entity delta: 68 fields (table starts with "absmin") at 10 index bits; Quake III MSG_WriteDeltaSharedEntity counterpart.
void MSG_WriteDeltaSharedEntity(msg_s *msg, const void *from, const void *to, int force)
{
    MSG_WriteDeltaStruct(msg, from, to, force, 0x44, 10, sharedEntityFields, 0);
}

// 0x080698e8  size=120  FUN_080698e8  status=translated  naming=inferred
// Evidence: player-state delta: 22 fields at 6 index bits, debug print on; a NULL 'from' uses a zeroed 0x5c-byte default. Quake III MSG_WriteDeltaClientState counterpart.
void MSG_WriteDeltaClientState(msg_s *msg, const void *from, const void *to, int force)
{
    byte defaults[0x5c];
    if (from == NULL) {
        from = defaults;
        memset(defaults, 0, 0x5c);
    }
    MSG_WriteDeltaStruct(msg, from, to, force, 0x16, 6, clientStateFields, 1);
}

// 0x08069960  size=823  FUN_08069960  status=translated  naming=inferred
// Evidence: Quake III MSG_ReadDeltaField counterpart; prints "%s:%i " / "%s:%f " field traces when requested.
void MSG_ReadDeltaField(msg_s *msg, const void *from, void *to, const netField_s *field, int print)
{
    /* Re-translated from the disassembly (gameplay lane): unchanged fields, the 32-bit long of the float encodings and
     * every integer field are moved as raw dwords (mov [edx],eax); only the small-integer float encodings convert
     * (fild/fstp) and the angle encoding stores MSG_ReadAngle16's float. The old float-typed version turned integer
     * fields of archived entities (pos.trTime, eType, ...) into float bit patterns. */
    const int32 *fromWord = (const int32 *)((const byte *)from + field->offset);
    int32 *toWord = (int32 *)((byte *)to + field->offset);
    int32 value;

    if (MSG_ReadBit(msg) == 0) {
        *toWord = *fromWord;
        return;
    }

    if (field->bits == 0 || field->bits == -99) {
        if (MSG_ReadBit(msg) == 0) {
            *toWord = 0;
        } else if (MSG_ReadBit(msg) == 0) {
            if (field->bits == 0) {
                value = MSG_ReadBits(msg, 5);
                value = value + (MSG_ReadByte(msg) << 5);
                value = value - 0x1000;
            } else {
                value = MSG_ReadBits(msg, 2);
                value = value + MSG_ReadByte(msg) * 4;
                value = value - 0x200;
            }
            *(float *)toWord = (float)value;
            if (print != 0) {
                Com_Printf("%s:%i ", field->name, value);
            }
        } else {
            *toWord = MSG_ReadLong(msg);
            if (print != 0) {
                Com_Printf("%s:%f ", field->name, (double)*(const float *)toWord);
            }
        }
    } else if (field->bits == -100) {
        if (MSG_ReadBit(msg) == 0) {
            *toWord = 0;
        } else {
            *(float *)toWord = MSG_ReadAngle16(msg);
        }
    } else {
        if (MSG_ReadBit(msg) == 0) {
            *toWord = 0;
            return;
        }
        uint32 negative = (uint32)field->bits >> 31;
        int32 absBits = field->bits;
        int32 low;
        int32 shift;
        if (negative != 0) {
            absBits = -absBits;
        }
        low = absBits & 7;
        if (low == 0) {
            value = 0;
        } else {
            value = MSG_ReadBits(msg, low);
        }
        for (shift = low; shift < absBits; shift = shift + 8) {
            value = value | (int32)((uint32)MSG_ReadByte(msg) << (shift & 0x1f)); /* shl edx,cl */
        }
        if (negative != 0 && ((value >> ((absBits - 1) & 0x1f)) & 1) != 0) {
            value = value | (int32)~((1u << (absBits & 0x1f)) - 1u);
        }
        *toWord = value;
        if (print != 0) {
            Com_Printf("%s:%i ", field->name, *toWord);
        }
    }
}
