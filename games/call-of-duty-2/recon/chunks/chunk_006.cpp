#include <setjmp.h>
#include <time.h>
#include <strings.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_006_types.h"

/*
 * Chunk 006 (0x0805ef22 .. 0x08061ccc): spatial-tree traces, scalar/vector helpers, command
 * buffer (Cbuf), command table (Cmd), console print, error handling and the event queue.
 * Callees outside this chunk keep their fn_08xxxxxx placeholder names.
 */

// 0x0805ef22  size=106  FUN_0805ef22  status=translated  naming=unknown
// Builds a 0x28-byte query (start, end, invDelta, contentsMask), computes invDelta (Box_ComputeInverseExtents), then runs
// the tree query Area_TraceSegmentRecords from node 1. Name is descriptive: result 1 means nothing blocked (see Area_TraceSegmentRecords).
unsigned int Collision_IsSegmentClear(const float *start, const float *end, unsigned int contentsMask)
{
    SegQueryB query;

    query.contentsMask = contentsMask;
    Vec3_Copy_0805fbec(start, query.start);
    Vec3_Copy_0805fbec(end, query.end);
    Box_ComputeInverseExtents((float *)&query);
    return Area_TraceSegmentRecords(&query, 1, query.start, query.end);
}

// 0x0805ef8c  size=795  FUN_0805ef8c  status=translated  naming=unknown
// Recursive walk of the node tree for an axis-extent (box) sweep; calls SV_ClipMoveToEntity_Box for every entry whose mask
// intersects and clips the segment against each split plane widened by the box half extent.
void SpatialTree_TraceBoxHits(struct BoxHitTrace *tr, unsigned short nodeIndex, const float *start4, const float *end4, float *bestFraction)
{
    float start[4];
    float mid[4];
    unsigned short cur;
    cod2_area_node_s *node;
    unsigned short entryIndex;
    svEntity_s *entry;
    float d0, d1, halfExtent, diff, absDiff, sel, inv, t0, t1;
    int side;

    cur = nodeIndex;
    Vec4_Copy_0805fc1c(start4, start);
    for (;;) {
        node = &g_0818901c_areaNodes[cur];
        if ((tr->contentsMask & node->maskB) == 0)
            return;
        entryIndex = node->entryHead;
        while (entryIndex != 0) {
            entry = cm6_entry(entryIndex);
            if ((entry->linkcontents & tr->contentsMask) != 0)
                SV_ClipMoveToEntity_Box(tr, entry, (trace_s *)bestFraction);
            entryIndex = entry->nextEntityInWorldSector;
        }
        d0 = start[node->axis] - node->dist;
        d1 = end4[node->axis] - node->dist;
        halfExtent = tr->extents[node->axis];
        if (!(SpatialTree_FloatMin(d0, d1) < halfExtent)) {
            cur = node->child[0];
            continue;
        }
        if (!(-halfExtent < SpatialTree_FloatMax(d0, d1))) {
            cur = node->child[1];
            continue;
        }
        if (*bestFraction <= start[3])
            break;
        diff = d1 - d0;
        if (diff != 0.0f || diff != diff) {
            absDiff = Float_Abs_0805fb26(diff);
            sel = SpatialTree_FloatSelect(diff, -d0, d0);
            inv = 1.0 / absDiff;
            t0 = (sel - halfExtent) * inv;
            t1 = (sel + halfExtent) * inv;
            side = Float_IsNonNegative_0805fb40(diff);
        } else {
            side = 0;
            t1 = 1.0f;
            t0 = 0.0f;
        }
        t1 = SpatialTree_FloatMin(t1, 1.0f);
        mid[0] = start[0] + (end4[0] - start[0]) * t1;
        mid[1] = start[1] + (end4[1] - start[1]) * t1;
        mid[2] = start[2] + (end4[2] - start[2]) * t1;
        mid[3] = start[3] + (end4[3] - start[3]) * t1;
        SpatialTree_TraceBoxHits(tr, node->child[side], start, mid, bestFraction);
        t0 = SpatialTree_FloatMax(t0, 0.0f);
        start[0] = start[0] + (end4[0] - start[0]) * t0;
        start[1] = start[1] + (end4[1] - start[1]) * t0;
        start[2] = start[2] + (end4[2] - start[2]) * t0;
        start[3] = start[3] + (end4[3] - start[3]) * t0;
        cur = node->child[1 - side];
    }
}

// 0x0805f2a8  size=106  FUN_0805f2a8  status=translated  naming=unknown
// Root wrapper: start/end taken from the context (+0x24/+0x30) with fractions 0 and *bestFraction.
void SpatialTree_TraceBoxHitsFromRoot(struct BoxHitTrace *tr, float *bestFraction)
{
    float start4[4];
    float end4[4];

    Vec3_Copy_0805fbec(tr->start, start4);
    Vec3_Copy_0805fbec(tr->end, end4);
    start4[3] = 0.0f;
    end4[3] = *bestFraction;
    SpatialTree_TraceBoxHits(tr, 1, start4, end4, bestFraction);
}

// 0x0805f312  size=744  FUN_0805f312  status=translated  naming=unknown
// Same traversal as SpatialTree_TraceBoxHits but returns the first non-zero SV_ClipBoxHitsEntity result.
int SpatialTree_TraceBoxFirstHit(struct BoxQueryTrace *tr, unsigned short nodeIndex, const float *start3, const float *end3)
{
    float start[3];
    float mid[3];
    unsigned short cur;
    cod2_area_node_s *node;
    unsigned short entryIndex;
    svEntity_s *entry;
    float d0, d1, halfExtent, diff, absDiff, sel, inv, t0, t1;
    int side;
    int result;

    cur = nodeIndex;
    Vec3_Copy_0805fbec(start3, start);
    for (;;) {
        node = &g_0818901c_areaNodes[cur];
        if ((tr->contentsMask & node->maskB) == 0)
            return 0;
        entryIndex = node->entryHead;
        while (entryIndex != 0) {
            entry = cm6_entry(entryIndex);
            result = SV_ClipBoxHitsEntity(tr, entry);
            if (result != 0)
                return result;
            entryIndex = entry->nextEntityInWorldSector;
        }
        d0 = start[node->axis] - node->dist;
        d1 = end3[node->axis] - node->dist;
        halfExtent = tr->extents[node->axis];
        if (!(SpatialTree_FloatMin(d0, d1) < halfExtent)) {
            cur = node->child[0];
            continue;
        }
        if (!(-halfExtent < SpatialTree_FloatMax(d0, d1))) {
            cur = node->child[1];
            continue;
        }
        diff = d1 - d0;
        if (diff != 0.0f || diff != diff) {
            absDiff = Float_Abs_0805fb26(diff);
            sel = SpatialTree_FloatSelect(diff, -d0, d0);
            inv = 1.0 / absDiff;
            t0 = (sel - halfExtent) * inv;
            t1 = (sel + halfExtent) * inv;
            side = Float_IsNonNegative_0805fb40(diff);
        } else {
            side = 0;
            t1 = 1.0f;
            t0 = 0.0f;
        }
        t1 = SpatialTree_FloatMin(t1, 1.0f);
        mid[0] = start[0] + (end3[0] - start[0]) * t1;
        mid[1] = start[1] + (end3[1] - start[1]) * t1;
        mid[2] = start[2] + (end3[2] - start[2]) * t1;
        result = SpatialTree_TraceBoxFirstHit(tr, node->child[side], start, mid);
        if (result != 0)
            return result;
        t0 = SpatialTree_FloatMax(t0, 0.0f);
        start[0] = start[0] + (end3[0] - start[0]) * t0;
        start[1] = start[1] + (end3[1] - start[1]) * t0;
        start[2] = start[2] + (end3[2] - start[2]) * t0;
        cur = node->child[1 - side];
    }
}

// 0x0805f5fa  size=53  FUN_0805f5fa  status=translated  naming=unknown
// Root wrapper using the context's own start (+0x24) and end (+0x30).
int SpatialTree_TraceBoxFirstHitFromRoot(struct BoxQueryTrace *tr)
{
    return SpatialTree_TraceBoxFirstHit(tr, 1, tr->start, tr->end);
}

// 0x0805f630  size=498  FUN_0805f630  status=translated  naming=unknown
// Zero-extent segment traversal (no box widening); SV_ClipMoveToEntity_Line is called for every entry of every visited node.
void SpatialTree_TraceSegmentHits(struct SegHitTrace *tr, unsigned short nodeIndex, const float *start4, const float *end4, float *bestFraction)
{
    float start[4];
    float mid[4];
    unsigned short cur;
    cod2_area_node_s *node;
    unsigned short entryIndex;
    svEntity_s *entry;
    float d0, d1, t;
    int side;

    cur = nodeIndex;
    Vec4_Copy_0805fc1c(start4, start);
    for (;;) {
        for (;;) {
            node = &g_0818901c_areaNodes[cur];
            if ((tr->contentsMask & node->maskB) == 0)
                return;
            entryIndex = node->entryHead;
            while (entryIndex != 0) {
                entry = cm6_entry(entryIndex);
                SV_ClipMoveToEntity_Line(tr, entry, (trace_s *)bestFraction);
                entryIndex = entry->nextEntityInWorldSector;
            }
            d0 = start[node->axis] - node->dist;
            d1 = end4[node->axis] - node->dist;
            if (d0 * d1 < 0.0f)
                break;
            side = Float_IsNonNegative_0805fb40(SpatialTree_FloatMin(d0, d1));
            cur = node->child[1 - side];
        }
        if (*bestFraction <= start[3])
            break;
        t = d0 / (d0 - d1);
        mid[0] = start[0] + (end4[0] - start[0]) * t;
        mid[1] = start[1] + (end4[1] - start[1]) * t;
        mid[2] = start[2] + (end4[2] - start[2]) * t;
        mid[3] = start[3] + (end4[3] - start[3]) * t;
        side = Float_IsNonNegative_0805fb40(d1);
        SpatialTree_TraceSegmentHits(tr, node->child[side], start, mid, bestFraction);
        cur = node->child[1 - side];
        Vec4_Copy_0805fc1c(mid, start);
    }
}

// 0x0805f822  size=103  FUN_0805f822  status=translated  naming=unknown
// Root wrapper: start = ctx+0, end = ctx+0xc, fractions 0 and *bestFraction.
void SpatialTree_TraceSegmentHitsFromRoot(struct SegHitTrace *tr, float *bestFraction)
{
    float start4[4];
    float end4[4];

    Vec3_Copy_0805fbec(tr->start, start4);
    Vec3_Copy_0805fbec(tr->end, end4);
    start4[3] = 0.0f;
    end4[3] = *bestFraction;
    SpatialTree_TraceSegmentHits(tr, 1, start4, end4, bestFraction);
}

// 0x0805f88a  size=575  FUN_0805f88a  status=translated  naming=unknown
// Segment query that splits at planes, recurses into both halves, then tests this node's own entries
// (post-order) with SV_ClipLineHitsEntity; returns the first non-zero result.
int SpatialTree_QuerySegment(struct SegQueryA *tr, unsigned short nodeIndex, const float *start3, const float *end3)
{
    cod2_area_node_s *node;
    float mid[3];
    float d0, d1, t;
    int side;
    int result;
    unsigned short entryIndex;

    node = &g_0818901c_areaNodes[nodeIndex];
    if ((tr->contentsMask & node->maskB) == 0)
        return 0;
    d0 = start3[node->axis] - node->dist;
    d1 = end3[node->axis] - node->dist;
    if (0.0f <= d0 * d1) {
        side = Float_IsNonNegative_0805fb40(SpatialTree_FloatMin(d0, d1));
        result = SpatialTree_QuerySegment(tr, node->child[1 - side], start3, end3);
        if (result != 0)
            return result;
    } else {
        t = d0 / (d0 - d1);
        mid[0] = start3[0] + (end3[0] - start3[0]) * t;
        mid[1] = start3[1] + (end3[1] - start3[1]) * t;
        mid[2] = start3[2] + (end3[2] - start3[2]) * t;
        side = Float_IsNonNegative_0805fb40(d1);
        result = SpatialTree_QuerySegment(tr, node->child[side], start3, mid);
        if (result != 0)
            return result;
        result = SpatialTree_QuerySegment(tr, node->child[1 - side], mid, end3);
        if (result != 0)
            return result;
    }
    entryIndex = node->entryHead;
    for (;;) {
        if (entryIndex == 0)
            return 0;
        result = SV_ClipLineHitsEntity(tr, cm6_entry(entryIndex));
        if (result != 0)
            break;
        entryIndex = cm6_entry(entryIndex)->nextEntityInWorldSector;
    }
    return result;
}

// 0x0805faca  size=50  FUN_0805faca  status=translated  naming=unknown
int SpatialTree_QuerySegmentFromRoot(struct SegQueryA *tr)
{
    return SpatialTree_QuerySegment(tr, 1, tr->start, tr->end);
}

// 0x0805fafc  size=41  FUN_0805fafc  status=translated  naming=unknown
// Returns ifNonNegative when 0.0 <= x else ifNegative (x87 float select).
float SpatialTree_FloatSelect(float x, float ifNonNegative, float ifNegative)
{
    float result;

    if (0.0f <= x)
        result = ifNonNegative;
    else
        result = ifNegative;
    return result;
}

// 0x0805fb26  size=25  FUN_0805fb26  status=translated  naming=unknown
float Float_Abs_0805fb26(float x)
{
    return fabs(x);
}

// 0x0805fb40  size=23  FUN_0805fb40  status=translated  naming=unknown
bool Float_IsNonNegative_0805fb40(float x)
{
    return 0.0f <= x;
}

// 0x0805fb58  size=36  FUN_0805fb58  status=translated  naming=unknown
// max(a, b) built from SpatialTree_FloatSelect on (a - b).
float SpatialTree_FloatMax(float a, float b)
{
    return SpatialTree_FloatSelect(a - b, a, b);
}

// 0x0805fb7c  size=36  FUN_0805fb7c  status=translated  naming=unknown
// min(a, b) built from SpatialTree_FloatSelect on (b - a).
float SpatialTree_FloatMin(float a, float b)
{
    return SpatialTree_FloatSelect(b - a, a, b);
}

// 0x0805fba0  size=31  FUN_0805fba0  status=translated  naming=unknown
// Copies two 32-bit words (bit copy, not float load/store).
void Vec2_Copy_0805fba0(const float *in, float *out)
{
    const volatile unsigned int *src = (const volatile unsigned int *)in;
    volatile unsigned int *dst = (volatile unsigned int *)out;

    dst[0] = src[0];
    dst[1] = src[1];
}

// 0x0805fbc0  size=44  FUN_0805fbc0  status=translated  naming=unknown
void Vec2_Subtract(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

// 0x0805fbec  size=47  FUN_0805fbec  status=translated  naming=unknown
void Vec3_Copy_0805fbec(const float *in, float *out)
{
    const volatile unsigned int *src = (const volatile unsigned int *)in;
    volatile unsigned int *dst = (volatile unsigned int *)out;

    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// 0x0805fc1c  size=63  FUN_0805fc1c  status=translated  naming=unknown
void Vec4_Copy_0805fc1c(const float *in, float *out)
{
    const volatile unsigned int *src = (const volatile unsigned int *)in;
    volatile unsigned int *dst = (volatile unsigned int *)out;

    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// 0x0805fc5c  size=5  FUN_0805fc5c  status=translated  naming=unknown
// Empty function (prologue + epilogue only).
void SpatialTree_NoOp_End(void)
{
}

// 0x0805fc9a  size=111  FUN_0805fc9a  status=translated  naming=inferred
// Q3 Cbuf_Init counterpart: sets each command buffer to 0x10000 bytes of storage, empty. Called from the
// error path for drop errors, as Q3 Com_Error does.
void Cbuf_Init(void)
{
    int i;

    for (i = 0; i < 1; i++) {
        g_cmd_text_buffers[i].data = &g_cmd_text_storage[i * 0x10000];
        g_cmd_text_buffers[i].maxsize = 0x10000;
        g_cmd_text_buffers[i].cursize = 0;
    }
}

// 0x0805fd0a  size=119  FUN_0805fd0a  status=translated  naming=observed
// String 'Cbuf_AddText: overflow\n' at 0x081402e0.
void Cbuf_AddText(const char *text)
{
    size_t length;

    length = strlen(text);
    if ((int)(g_cmd_text->cursize + length) < g_cmd_text->maxsize) {
        memcpy(g_cmd_text->data + g_cmd_text->cursize, text, length);
        g_cmd_text->cursize = g_cmd_text->cursize + length;
    } else {
        Com_Printf("Cbuf_AddText: overflow\n");
    }
}

// 0x0805fd82  size=185  FUN_0805fd82  status=translated  naming=observed
// String 'Cbuf_InsertText overflowed\n' at 0x081402f8. Shifts the pending text up and prepends text + '\n'.
void Cbuf_InsertText(const char *text)
{
    size_t length;
    int total;
    int i;

    length = strlen(text);
    total = length + 1;
    if (g_cmd_text->maxsize < total + g_cmd_text->cursize) {
        Com_Printf("Cbuf_InsertText overflowed\n");
    } else {
        i = g_cmd_text->cursize;
        while (i = i - 1, -1 < i) {
            g_cmd_text->data[total + i] = g_cmd_text->data[i];
        }
        memcpy(g_cmd_text->data, text, length);
        g_cmd_text->data[total - 1] = 10;
        g_cmd_text->cursize = total + g_cmd_text->cursize;
    }
}

// 0x0805fe3c  size=122  FUN_0805fe3c  status=translated  naming=observed
// String '\025Cbuf_ExecuteText: bad exec_when' at 0x08140320. 0 = now, 1 = insert, 2 = append.
void Cbuf_ExecuteText(int exec_when, const char *text)
{
    if (exec_when == 1) {
        Cbuf_InsertText(text);
    } else {
        if (exec_when < 2) {
            if (exec_when == 0) {
                if (text == 0 || *text == '\0') {
                    Cbuf_ExecuteThunk();
                    return;
                }
                Cmd_ExecuteString((char *)text);
                return;
            }
        } else if (exec_when == 2) {
            Cbuf_AddText(text);
            return;
        }
        Com_Error(0, "\025Cbuf_ExecuteText: bad exec_when");
    }
}

// 0x0805feb6  size=330  FUN_0805feb6  status=translated  naming=inferred
// Q3 Cbuf_Execute: honours the wait counter, extracts one line (split on ';' outside quotes, '\n', '\r',
// capped at 0xfff bytes), removes it from the buffer and runs Cmd_ExecuteString.
void Cbuf_Execute(void)
{
    char line[4104];
    bool inQuotes;
    byte *text;
    int i;

    for (;;) {
        if (g_cmd_text->cursize == 0)
            return;
        if (g_cmd_wait != 0) {
            g_cmd_wait = g_cmd_wait - 1;
            return;
        }
        text = g_cmd_text->data;
        inQuotes = false;
        for (i = 0; i < g_cmd_text->cursize; i++) {
            if (text[i] == '"')
                inQuotes = !inQuotes;
            if ((!inQuotes && text[i] == ';') || text[i] == '\n' || text[i] == '\r')
                break;
        }
        if (0xffe < i)
            i = 0xfff;
        memcpy(line, text, i);
        line[i] = 0;
        if (i == g_cmd_text->cursize) {
            g_cmd_text->cursize = 0;
        } else {
            i = i + 1;
            g_cmd_text->cursize = g_cmd_text->cursize - i;
            memmove(text, text + i, g_cmd_text->cursize);
        }
        Cmd_ExecuteString(line);
    }
}

// 0x08060000  size=13  FUN_08060000  status=translated  naming=unknown
// Thin wrapper over Cbuf_Execute (separate symbol, called from Cbuf_ExecuteText and elsewhere).
void Cbuf_ExecuteThunk(void)
{
    Cbuf_Execute();
}

// 0x0806000e  size=92  FUN_0806000e  status=translated  naming=inferred
// Reads a script file (FS_ReadFile = file read), prints 'execing %s\n', inserts it into the buffer and frees it.
// Returns whether the file was found.
bool Cmd_ExecFile(const char *filename)
{
    void *buffer;

    FS_ReadFile(filename, &buffer);
    if (buffer != 0) {
        Com_Printf("execing %s\n", filename);
        Cbuf_InsertText((const char *)buffer);
        FS_FreeFile(buffer);
    }
    return buffer != 0;
}

// 0x0806006a  size=139  FUN_0806006a  status=translated  naming=inferred
// 'exec' command: strings 'exec <filename> : execute a script file\n' and "couldn't exec %s\n"; registered as
// 'exec' by Cmd_Init.
void Cmd_Exec_f(void)
{
    char filename[0x40 + 8];
    char found;

    if (Cmd_Argc() == 2) {
        Q_strncpyz(filename, Cmd_Argv(1), 0x40);
        COM_DefaultExtension(filename, 0x40, ".cfg");
        found = Cmd_ExecFile(filename);
        if (found == '\0')
            Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
    } else {
        Com_Printf("exec <filename> : execute a script file\n");
    }
}

// 0x080601e0  size=10  FUN_080601e0  status=translated  naming=inferred
// Q3 Cmd_Argc.
int Cmd_Argc(void)
{
    return cmd_argc;
}

// 0x080601ea  size=10  FUN_080601ea  status=translated  naming=inferred
// Duplicate of Cmd_Argc (identical body, separate symbol).
int Cmd_Argc_alt(void)
{
    return cmd_argc;
}

// 0x080601f4  size=44  FUN_080601f4  status=translated  naming=inferred
// Q3 Cmd_Argv: unsigned bounds check, returns the empty string for out-of-range.
const char *Cmd_Argv(unsigned int arg)
{
    const char *result;

    if (arg < (unsigned int)cmd_argc)
        result = cmd_argv[arg];
    else
        result = "";
    return result;
}

// 0x08060220  size=44  FUN_08060220  status=translated  naming=inferred
// Duplicate of Cmd_Argv.
const char *Cmd_Argv_alt(unsigned int arg)
{
    const char *result;

    if (arg < (unsigned int)cmd_argc)
        result = cmd_argv[arg];
    else
        result = "";
    return result;
}

// 0x0806024c  size=43  FUN_0806024c  status=translated  naming=inferred
// Q3 Cmd_ArgvBuffer: Q_strncpyz(buffer, Cmd_Argv(arg), bufferLength).
void Cmd_ArgvBuffer(int arg, char *buffer, int bufferLength)
{
    const char *value;

    value = Cmd_Argv(arg);
    Q_strncpyz(buffer, value, bufferLength);
}

// 0x08060278  size=43  FUN_08060278  status=translated  naming=inferred
// Duplicate of Cmd_ArgvBuffer using Cmd_Argv_alt.
void Cmd_ArgvBuffer_alt(int arg, char *buffer, int bufferLength)
{
    const char *value;

    value = Cmd_Argv_alt(arg);
    Q_strncpyz(buffer, value, bufferLength);
}

// 0x080602a4  size=119  FUN_080602a4  status=translated  naming=inferred
// Q3 Cmd_ArgsFrom: joins arguments arg..argc-1 with single spaces into the 0x400-byte static buffer.
const char *Cmd_ArgsFrom(int arg)
{
    int i;

    cmd_args[0] = 0;
    for (i = arg; i < cmd_argc; i++) {
        I_strncat(cmd_args, 0x400, cmd_argv[i]);
        if (i != cmd_argc - 1)
            I_strncat(cmd_args, 0x400, " ");
    }
    return cmd_args;
}

// 0x08060346  size=650  FUN_08060346  status=translated  naming=inferred
// Q3 Cmd_TokenizeString2 counterpart. Splits text into at most 0x200 tokens written to outBuffer, pointers in
// argv. Skips whitespace, '//' (ends parsing) and '/* */' comments, supports "quoted" tokens with \" escape.
// 'limit' counts down per token: when it reaches 0 the remainder of the text becomes the last token (a start
// limit of 0 never triggers). Returns 0 when 0x200 tokens were filled. Chars are signed (>= 0x80 counts as space).
int Cmd_TokenizeStringInternal(char *text, int limit, char **argv, char *outBuffer)
{
    int argc;
    char *p;
    char *q;

    argc = 0;
    p = text;
    for (;;) {
        if (argc == 0x200)
            return 0;
        limit = limit - 1;
        if (limit == 0)
            break;
        for (;;) {
            while (*p != '\0' && *p < '!')
                p++;
            if (*p == '\0')
                return argc;
            if (*p == '/' && p[1] == '/')
                return argc;
            if (*p != '/' || p[1] != '*')
                break;
            while (*p != '\0' && (*p != '*' || p[1] != '/'))
                p++;
            if (*p == '\0')
                return argc;
            p += 2;
        }
        if (*p == '"') {
            argv[argc] = outBuffer;
            argc++;
            q = p;
            while (p = q + 1, *p != '\0' && *p != '"') {
                if (*p == '\\' && q[2] == '"')
                    p = q + 2;
                *outBuffer = *p;
                outBuffer++;
                q = p;
            }
            *outBuffer = '\0';
            outBuffer++;
            if (*p == '\0')
                return argc;
            p = q + 2;
            if (*p == '\0')
                return argc;
            if (*p < '!')
                p = q + 3;
        } else {
            argv[argc] = outBuffer;
            argc++;
            while (' ' < *p && *p != '"' && (*p != '/' || p[1] != '/') && (*p != '/' || p[1] != '*')) {
                *outBuffer = *p;
                outBuffer++;
                p++;
            }
            *outBuffer = '\0';
            outBuffer++;
            if (*p == '\0')
                return argc;
            if (*p < '!')
                p++;
        }
    }
    if (*p == '\0')
        return argc;
    argv[argc] = outBuffer;
    while (*p != '\0') {
        *outBuffer = *p;
        outBuffer++;
        p++;
    }
    *outBuffer = '\0';
    return argc + 1;
}

// 0x080605d0  size=47  FUN_080605d0  status=translated  naming=inferred
// Tokenizes into the global cmd_argv/cmd_tokenized and stores cmd_argc.
void Cmd_TokenizeStringN(char *text, int limit)
{
    cmd_argc = Cmd_TokenizeStringInternal(text, limit, cmd_argv, cmd_tokenized);
}

// 0x08060600  size=47  FUN_08060600  status=translated  naming=inferred
// Duplicate of Cmd_TokenizeStringN.
void Cmd_TokenizeStringN_alt(char *text, int limit)
{
    cmd_argc = Cmd_TokenizeStringInternal(text, limit, cmd_argv, cmd_tokenized);
}

// 0x08060630  size=27  FUN_08060630  status=translated  naming=inferred
// Q3 Cmd_TokenizeString: limit 0 (unlimited).
void Cmd_TokenizeString(char *text)
{
    Cmd_TokenizeStringN(text, 0);
}

// 0x0806064c  size=27  FUN_0806064c  status=translated  naming=inferred
// Duplicate of Cmd_TokenizeString.
void Cmd_TokenizeString_alt(char *text)
{
    Cmd_TokenizeStringN_alt(text, 0);
}

// 0x08060668  size=77  FUN_08060668  status=translated  naming=inferred
// Linear search of the command list by exact (case-sensitive) name.
struct cmd_function_s *Cmd_FindCommand(const char *name)
{
    cmd_function_s *cmd;

    cmd = cmd_functions;
    for (;;) {
        if (cmd == 0)
            return 0;
        if (strcmp(name, cmd->name) == 0)
            break;
        cmd = cmd->next;
    }
    return cmd;
}

// 0x080606b6  size=119  FUN_080606b6  status=translated  naming=observed
// String 'Cmd_AddCommand: %s already defined\n' at 0x08140440. Note: when the command exists and a function is
// given, only the message is printed (the function is not replaced); a null function on an existing name is silent.
void Cmd_AddCommand(const char *name, void (*function)(void))
{
    cmd_function_s *cmd;

    if (Cmd_FindCommand(name) == 0) {
        cmd = (cmd_function_s *)Com_MallocZeroed(0x14);
        cmd->name = CopyString(name);
        cmd->function = function;
        cmd->next = cmd_functions;
        cmd_functions = cmd;
    } else if (function != 0) {
        Com_Printf("Cmd_AddCommand: %s already defined\n", name);
    }
}

// 0x0806072e  size=110  FUN_0806072e  status=translated  naming=inferred
// Q3 Cmd_RemoveCommand: unlinks the node by exact name, frees name and node.
void Cmd_RemoveCommand(const char *name)
{
    cmd_function_s *cmd;
    cmd_function_s **link;

    link = &cmd_functions;
    for (;;) {
        cmd = *link;
        if (cmd == 0)
            return;
        if (strcmp(name, cmd->name) == 0)
            break;
        link = &cmd->next;
    }
    *link = cmd->next;
    if (cmd->name != 0)
        Com_Free_A((void *)cmd->name);
    Com_Free_A(cmd);
}

// 0x0806079c  size=40  FUN_0806079c  status=translated  naming=unknown
// Stores two words into the command record at +8/+0xc. No null check on the lookup result (crashes for
// unknown names).
void Cmd_SetAutoComplete(const char *name, void *aux8, void *auxC)
{
    cmd_function_s *cmd;

    cmd = Cmd_FindCommand(name);
    cmd->autoCompleteDir = (const char *)aux8;
    cmd->autoCompleteExt = (const char *)auxC;
}

// 0x080607c4  size=67  FUN_080607c4  status=translated  naming=inferred
// Frees every command node and its name.
void Cmd_Shutdown(void)
{
    cmd_function_s *cmd;

    while (cmd = cmd_functions, cmd_functions != 0) {
        cmd_functions = cmd_functions->next;
        Com_Free_A((void *)cmd->name);
        Com_Free_A(cmd);
    }
}

// 0x080608a6  size=293  FUN_080608a6  status=translated  naming=inferred
// Q3 Cmd_ExecuteString: tokenize, find command case-insensitively (moved to the list head on a hit), call its
// function. Otherwise: text starting 'pb_sv_' goes to PB_DispatchEvent3(0xe,-1,len+1,text); other text goes to
// Dvar_Command (dvar command) and, if not handled while com_sv_running is set, to SV_GameConsoleCommand.
void Cmd_ExecuteString(char *text)
{
    cmd_function_s *cmd;
    cmd_function_s **link;
    size_t length;

    Cmd_TokenizeString(text);
    if (Cmd_Argc() != 0) {
        link = &cmd_functions;
        while (*link != 0) {
            cmd = *link;
            if (Q_stricmp(cmd_argv[0], cmd->name) == 0) {
                *link = cmd->next;
                cmd->next = cmd_functions;
                cmd_functions = cmd;
                if (cmd->function != 0) {
                    cmd->function();
                    return;
                }
                break;
            }
            link = &cmd->next;
        }
        if (Q_stricmpn(text, "pb_", 3) == 0) {
            if (Q_stricmpn(text + 3, "sv_", 3) == 0) {
                length = strlen(text);
                PB_DispatchEvent3(0xe, -1, length + 1, (int)text);
            }
        } else {
            if (Dvar_Command() == 0 && com_sv_running != 0 && com_sv_running->current.enabled != '\0')
                SV_GameConsoleCommand();
        }
    }
}

// 0x080609cc  size=19  FUN_080609cc  status=translated  naming=unknown
// Wrapper over Cmd_ExecuteString.
void Cmd_ExecuteStringThunk(char *text)
{
    Cmd_ExecuteString(text);
}

// 0x08060a82  size=88  FUN_08060a82  status=translated  naming=inferred
// Q3 Cmd_Init: strings 'cmdlist', 'exec', 'vstr', 'wait' registered with the matching handlers.
void Cmd_Init(void)
{
    Cmd_AddCommand("cmdlist", Cmd_List_f);
    Cmd_AddCommand("exec", Cmd_Exec_f);
    Cmd_AddCommand("vstr", Cmd_Vstr_f);
    Cmd_AddCommand("wait", Cmd_Wait_f);
}

// 0x08060adc  size=46  FUN_08060adc  status=translated  naming=unknown
// fread wrapper.
size_t Com_Fread(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t result;

    result = fread(ptr, size, nmemb, (FILE *)stream);
    return result;
}

// 0x08060b0a  size=40  FUN_08060b0a  status=translated  naming=unknown
// fwrite wrapper; the result is discarded.
size_t Com_Fwrite(const void *ptr, size_t size, size_t nmemb, void *stream)
{
    return fwrite(ptr, size, nmemb, (FILE *)stream);   /* tail value */
}

// 0x08060b32  size=32  FUN_08060b32  status=translated  naming=unknown
// fopen wrapper.
void *Com_Fopen(const char *path, const char *mode)
{
    FILE *file;

    file = fopen(path, mode);
    return file;
}

// 0x08060b52  size=19  FUN_08060b52  status=translated  naming=unknown
// fclose wrapper.
void Com_Fclose(void *stream)
{
    fclose((FILE *)stream);
}

// 0x08060b66  size=39  FUN_08060b66  status=translated  naming=unknown
// fseek wrapper.
int Com_Fseek(void *stream, long offset, int whence)
{
    int result;

    result = fseek((FILE *)stream, offset, whence);
    return result;
}

// 0x08060b90  size=10  FUN_08060b90  status=translated  naming=unknown
// Returns 0 (stub); called from Com_ErrorRecover.
int Com_ReturnZero(void)
{
    return 0;
}

// 0x08060b9c  size=5  FUN_08060b9c  status=translated  naming=unknown
// Empty function.
void Noop_08060b9c(void *unusedField)
{
}

// 0x08060ba2  size=57  FUN_08060ba2  status=translated  naming=inferred
// Q3 Com_BeginRedirect: all three arguments must be non-null; terminates buffer with 0.
void Com_BeginRedirect(char *buffer, int buffersize, void (*flush)(char *))
{
    if (buffer != 0 && buffersize != 0 && flush != 0) {
        rd_buffer = buffer;
        rd_buffersize = buffersize;
        rd_flush = flush;
        *buffer = 0;
    }
}

// 0x08060bdc  size=62  FUN_08060bdc  status=translated  naming=inferred
// Q3 Com_EndRedirect: flushes the buffer if a flush function exists, clears the redirect state.
void Com_EndRedirect(void)
{
    if (rd_flush != 0)
        rd_flush(rd_buffer);
    rd_buffer = 0;
    rd_buffersize = 0;
    rd_flush = 0;
}

// 0x08060c1a  size=5  FUN_08060c1a  status=translated  naming=unknown
// Empty function; callers pass one int argument (0), ignored.
void Noop_08060c1a(int unused)
{
}

// 0x08060c20  size=458  FUN_08060c20  status=translated  naming=inferred
// Q3 Com_Printf core (Com_PrintMessage in later IW engines). Strings 'console_mp_server.log',
// 'logfile opened on %s\n'. channel 4 suppresses console/redirect output. With a redirect buffer active the text
// is appended to it (flushing first if it would overflow); otherwise it goes to the console and, when com_logfile
// is set, to the log file (opened on demand; flushed when com_logfile > 1).
void Com_PrintMessage(int channel, char *msg)
{
    size_t length;
    size_t bufferLength;
    char *timeText;
    time_t now;
    struct tm *local;

    PB_ConsoleCapture(msg, 0x1000);
    if (rd_buffer == 0) {
        if (*msg == '^' && msg[1] != '\0')
            msg = msg + 2;
        if (channel != 4)
            Sys_Print(msg);
        if (com_logfile != 0 && com_logfile->current.integer != 0) {
            Sys_EnterCriticalSection(0);
            if (FS_HasSearchPaths() != 0) {
                if (com_logfileHandle == 0 && com_openingLogfile == 0) {
                    com_openingLogfile = 1;
                    time(&now);
                    local = localtime(&now);
                    com_logfileHandle = FS_FOpenFileWriteText("console_mp_server.log");
                    timeText = asctime(local);
                    Com_Printf("logfile opened on %s\n", timeText);
                    com_openingLogfile = 0;
                }
                if (com_logfileHandle != 0) {
                    length = strlen(msg);
                    FS_Write(msg, length, com_logfileHandle);
                    if (1 < com_logfile->current.integer)
                        FS_Flush(com_logfileHandle);
                }
            }
            Sys_LeaveCriticalSection(0);
        }
    } else if (channel != 4) {
        Sys_EnterCriticalSection(5);
        length = strlen(msg);
        bufferLength = strlen(rd_buffer);
        if (rd_buffersize - 1U < bufferLength + length) {
            rd_flush(rd_buffer);
            *rd_buffer = '\0';
        }
        I_strncat(rd_buffer, rd_buffersize, msg);
        Sys_LeaveCriticalSection(5);
    }
}

// 0x08060dea  size=79  FUN_08060dea  status=translated  naming=inferred
// Q3 Com_Printf: vsnprintf into a 0x1000 buffer (last byte forced to 0), then Com_PrintMessage(0, text).
void Com_Printf(const char *fmt, ...)
{
    char buffer[0x1000];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, 0x1000, fmt, args);
    va_end(args);
    buffer[0xfff] = 0;
    Com_PrintMessage(0, buffer);
}

// 0x08060e3a  size=101  FUN_08060e3a  status=translated  naming=inferred
// Q3 Com_DPrintf: prints only when the com_developer dvar is non-zero.
void Com_DPrintf(const char *fmt, ...)
{
    char buffer[0x1000];
    va_list args;

    if (com_developer != 0 && com_developer->current.integer != 0) {
        va_start(args, fmt);
        vsnprintf(buffer, 0x1000, fmt, args);
        va_end(args);
        buffer[0xfff] = 0;
        Com_Printf("%s", buffer);
    }
}

// 0x08060ea0  size=24  FUN_08060ea0  status=translated  naming=inferred
// Called with 'EXE_SERVERQUIT'-style strings: SV_Shutdown is the server shutdown taking a final message
// (Q3 SV_Shutdown), then Com_Restart_Subsystems.
void Com_ServerShutdown(const char *msg)
{
    SV_Shutdown((char *)msg);
    Com_Restart_Subsystems();
}

// 0x08060eb8  size=19  FUN_08060eb8  status=translated  naming=unknown
// Duplicate wrapper over Com_ServerShutdown.
void Com_ServerShutdownThunk(const char *msg)
{
    Com_ServerShutdown(msg);
}

// 0x08060ecc  size=18  FUN_08060ecc  status=translated  naming=unknown
// Runs Hunk_ClearTempMemory then Hunk_ClearTempMemoryHigh; invoked before server shutdown in Com_Quit_f and Com_ErrorRecover.
void Com_PreShutdownHooks(void)
{
    Hunk_ClearTempMemory();
    Hunk_ClearTempMemoryHigh();
}

// 0x08060ede  size=13  FUN_08060ede  status=translated  naming=unknown
// Stores a word into global 0x081a2270.
void Com_SetExtraTimeScale(int value)
{
    com_extraTimeScale = value;
}

// 0x08060eec  size=87  FUN_08060eec  status=translated  naming=unknown
// Case-insensitive lookup of s in the table at 0x0817d780 (EXE_SERVER_DISCONNECTED, EXE_DISCONNECTED,
// EXE_SERVERISFULL, XBOXLIVE_*), terminated by an empty string. Returns 1 when found.
int Com_IsKnownErrorString(const char *s)
{
    int i;

    i = 0;
    for (;;) {
        if (*com_knownErrorStrings[i] == '\0')
            return 0;
        if (strcasecmp(com_knownErrorStrings[i], s) == 0)
            break;
        i++;
    }
    return 1;
}

// 0x08060f44  size=94  FUN_08060f44  status=translated  naming=observed
// Strings 'com_errorMessage' and 'com_errorTitle': registers both string dvars (default "", flags 0x1040) and
// sets com_errorMessage to msg.
void Com_RegisterErrorDvars(const char *msg)
{
    com_errorMessage_dvar = Dvar_RegisterString("com_errorMessage", "", 0x1040);
    com_errorTitle_dvar = Dvar_RegisterString("com_errorTitle", "", 0x1040);
    Dvar_SetString(com_errorMessage_dvar, msg);
}

// 0x08060fa2  size=385  FUN_08060fa2  status=translated  naming=unknown
// Post-longjmp error recovery (called from the Com_Init-side handler at 0x080627f4): resets subsystems, stores the
// message in the error dvars, rate-limits repeated errors (4 within 100 ms each escalate to code 0), then shuts the
// server down. Strings 'cl_paused', 'EXE_DISCONNECTEDFROMOWNLISTENSERVER',
// '********************\nERROR: %s\n********************\n'.
void Com_ErrorRecover(void)
{
    char savedMessage[4108];
    int now;
    char shouldQuit;

    Com_PoolReset();
    Dvar_SetSetterGate(0);
    Com_PreShutdownHooks();
    Com_ReturnZero();
    Dvar_SetOrCreateInt("cl_paused", 0);
    FS_PureServerSetLoadedIwds("", "");
    strcpy(savedMessage, com_errorMessage);
    if (com_errorCode != 3)
        Com_RegisterErrorDvars(com_errorMessage);
    if (com_errorCode != 3)
        Scr_ClearInitState();
    Com_UnloadBspIfLoaded();
    CMod_ClearBspBase();
    Com_ResetParseSessions();
    FS_ResetLoadStack();
    if (com_errorCode == 1)
        Cbuf_Init();
    now = Sys_Milliseconds();
    if (now - com_lastErrorTime < 100) {
        com_errorCount = com_errorCount + 1;
        if (3 < com_errorCount)
            com_errorCode = 0;
    } else {
        com_errorCount = 0;
    }
    com_lastErrorTime = now;
    if (com_errorCode != 2 && com_errorCode != 1 && com_errorCode != 3)
        Sys_Error("%s", com_errorMessage);
    if (com_errorCode == 2) {
        Com_ServerShutdown("EXE_DISCONNECTEDFROMOWNLISTENSERVER");
    } else {
        Com_Printf("********************\nERROR: %s\n********************\n", com_errorMessage);
        Com_ServerShutdown(savedMessage);
        if (com_errorCode == 1) {
            shouldQuit = Com_GetQuitOnErrorFlag();
            if (shouldQuit != '\0')
                Com_Quit_f();
        }
    }
    g_081a21c4 = 0;
    com_errorEntered = 0;
}

// 0x08061124  size=208  FUN_08061124  status=translated  naming=observed
// String 'recursive error after: %s'. Q3 Com_Error: formats into the 0x1000-byte error buffer, maps error codes
// 4, 5 and 6 to 1, stores the code and longjmps to the handler registered in slot 2. Never returns.
void Com_Error(int code, const char *fmt, ...)
{
    va_list args;
    struct __jmp_buf_tag *env;

    Sys_EnterCriticalSection(2);
    if (com_errorEntered != 0)
        Sys_Error("recursive error after: %s", com_errorMessage);
    com_errorEntered = 1;
    va_start(args, fmt);
    vsnprintf(com_errorMessage, 0x1000, fmt, args);
    va_end(args);
    com_errorMessage[0xfff] = 0;
    if (code != 1) {
        if (code == 4 || code == 6) {
            code = 1;
        } else if (code == 5) {
            code = 1;
        } else {
            g_081a21c4 = 0;
        }
    }
    com_errorCode = code;
    Sys_LeaveCriticalSection(2);
    env = (struct __jmp_buf_tag *)Sys_GetValue(2);
    longjmp(env, -1);
}

// 0x080611f4  size=74  FUN_080611f4  status=translated  naming=unknown
// If an error is already being handled, longjmps again to the slot-2 handler.
void Com_CheckErrorEntered(void)
{
    int entered;
    struct __jmp_buf_tag *env;

    Sys_EnterCriticalSection(2);
    entered = com_errorEntered;
    Sys_LeaveCriticalSection(2);
    if (entered != 0) {
        env = (struct __jmp_buf_tag *)Sys_GetValue(2);
        longjmp(env, -1);
    }
}

// 0x0806123e  size=95  FUN_0806123e  status=translated  naming=inferred
// Q3 Com_Quit_f: strings 'quitting...\n', 'EXE_SERVERQUIT'. Sys_Quit is the final exit call.
void Com_Quit_f(void)
{
    Com_Printf("quitting...\n");
    Sys_EnterCriticalSection(2);
    if (com_errorEntered == 0) {
        Com_PreShutdownHooks();
        SV_Shutdown("EXE_SERVERQUIT");
        Com_ShutdownSubsystems();
        Com_CloseLogFile();
        FS_Shutdown(); /* original pushes an unused argument 1 */
        FS_ClearListA();
        FS_ClearListB();
    }
    Sys_Quit();
}

// 0x0806129e  size=95  FUN_0806129e  status=translated  naming=inferred
// Q3 Com_ParseCommandLine: the line is split at '+' or '\n' into up to 32 console lines (each separator is
// overwritten with 0 and the next line starts after it).
void Com_ParseCommandLine(char *commandLine)
{
    com_consoleLines[0] = commandLine;
    com_numConsoleLines = 1;
    for (;;) {
        if (*commandLine == '\0')
            return;
        if (*commandLine == '+' || *commandLine == '\n') {
            if (com_numConsoleLines == 0x20)
                return;
            com_consoleLines[com_numConsoleLines] = commandLine + 1;
            com_numConsoleLines = com_numConsoleLines + 1;
            *commandLine = '\0';
        }
        commandLine = commandLine + 1;
    }
}

// 0x080612fe  size=150  FUN_080612fe  status=translated  naming=inferred
// Q3 Com_SafeMode: a startup line whose command is 'safe' or 'dvar_restart' is blanked and reports 1;
// otherwise the stored safemode flag.
int Com_SafeMode(void)
{
    int i;

    i = 0;
    for (;;) {
        if (com_numConsoleLines <= i)
            return com_safemode;
        Cmd_TokenizeString(com_consoleLines[i]);
        if (Q_stricmp(Cmd_Argv(0), "safe") == 0)
            break;
        if (Q_stricmp(Cmd_Argv(0), "dvar_restart") == 0)
            break;
        i = i + 1;
    }
    *com_consoleLines[i] = 0;
    return 1;
}

// 0x08061394  size=15  FUN_08061394  status=translated  naming=inferred
// Sets the flag returned by Com_SafeMode.
void Com_SetSafeModeFlag(void)
{
    com_safemode = 1;
}

// 0x080613a4  size=176  FUN_080613a4  status=translated  naming=inferred
// Q3 Com_StartupVariable: for each startup line that is 'set' / 'seta' (and, if match is given, whose arg 1
// equals match exactly) run the dvar set command handler (Cmd_Set_f for set, Cmd_SetA_f for seta).
void Com_StartupVariable(const char *match)
{
    const char *command;
    int i;

    i = 0;
    do {
        if (com_numConsoleLines <= i)
            return;
        Cmd_TokenizeString(com_consoleLines[i]);
        if (match == 0 || strcmp(Cmd_Argv(1), match) == 0) {
            command = Cmd_Argv(0);
            if (strcasecmp(command, "set") == 0) {
                Cmd_Set_f();
            } else {
                command = Cmd_Argv(0);
                if (strcasecmp(command, "seta") == 0)
                    Cmd_SetA_f();
            }
        }
        i = i + 1;
    } while (true);
}

// 0x08061454  size=150  FUN_08061454  status=translated  naming=inferred
// Q3 Com_AddStartupCommands: appends every non-empty startup line plus '\n' to the command buffer; returns 1
// if any line did not start with 'set'.
int Com_AddStartupCommands(void)
{
    int added;
    int i;

    added = 0;
    for (i = 0; i < com_numConsoleLines; i++) {
        if (com_consoleLines[i] != 0 && *com_consoleLines[i] != '\0') {
            if (Q_stricmpn(com_consoleLines[i], "set", 3) != 0)
                added = 1;
            Cbuf_AddText(com_consoleLines[i]);
            Cbuf_AddText("\n");
        }
    }
    return added;
}

// 0x080614ea  size=334  FUN_080614ea  status=translated  naming=inferred
// Q3 Info_Print: walks "\key\value\key\value", prints keys padded with spaces to 20 columns (keys of 20+ chars
// are printed whole) then the value; prints 'MISSING VALUE\n' when the string ends after a key.
void Info_Print(const char *s)
{
    char value[512];
    char key[512];
    char *o;
    const char *next;

    if (*s == '\\')
        s = s + 1;
    for (;;) {
        if (*s == '\0')
            return;
        o = key;
        while (*s != '\0' && *s != '\\') {
            *o = *s;
            o = o + 1;
            s = s + 1;
        }
        if (o - key < 0x14) {
            memset(o, 0x20, 0x14 - (o - key));
            key[0x14] = 0;
        } else {
            *o = '\0';
        }
        Com_Printf("%s", key);
        if (*s == '\0')
            break;
        o = value;
        next = s;
        while (s = next + 1, *s != '\0' && *s != '\\') {
            *o = *s;
            o = o + 1;
            next = s;
        }
        *o = '\0';
        if (*s != '\0')
            s = next + 2;
        Com_Printf("%s\n", value);
    }
    Com_Printf("MISSING VALUE\n");
}

// 0x08061638  size=56  FUN_08061638  status=translated  naming=inferred
// Clears the 256-entry pushed event queue (0x1800 bytes) and both counters.
void Com_InitPushEvent(void)
{
    memset(com_pushedEvents, 0, 0x1800);
    com_pushedEventsHead = 0;
    com_pushedEventsTail = 0;
}

// 0x08061670  size=176  FUN_08061670  status=translated  naming=observed
// String 'WARNING: Com_PushEvent overflow\n'. Appends to the ring; when 256 entries are pending the oldest is
// dropped (its pointer payload freed) and a warning is printed once until the queue drains.
void Com_PushEvent(const struct sysEvent_s *ev)
{
    sysEvent_s *slot;

    slot = &com_pushedEvents[com_pushedEventsHead & 0xff];
    if ((int)(com_pushedEventsHead - com_pushedEventsTail) < 0x100) {
        com_pushEventOverflowed = 0;
    } else {
        if (com_pushEventOverflowed == 0) {
            com_pushEventOverflowed = 1;
            Com_Printf("WARNING: Com_PushEvent overflow\n");
        }
        if (slot->evPtr != 0)
            Com_Free_A(slot->evPtr);
        com_pushedEventsTail = com_pushedEventsTail + 1;
    }
    slot->evTime = ev->evTime;
    slot->evType = ev->evType;
    slot->evValue = ev->evValue;
    slot->evValue2 = ev->evValue2;
    slot->evPtrLength = ev->evPtrLength;
    slot->evPtr = ev->evPtr;
    com_pushedEventsHead = com_pushedEventsHead + 1;
}

// 0x08061720  size=123  FUN_08061720  status=translated  naming=unknown
// Discards all pending pushed events, freeing each event's pointer payload.
void Com_FlushPushedEvents(void)
{
    unsigned int index;

    while ((int)com_pushedEventsTail < com_pushedEventsHead) {
        index = com_pushedEventsTail & 0xff;
        com_pushedEventsTail = com_pushedEventsTail + 1;
        if (com_pushedEvents[index].evPtr != 0)
            Com_Free_A(com_pushedEvents[index].evPtr);
    }
}

// 0x0806179c  size=141  FUN_0806179c  status=translated  naming=inferred
// Q3 Com_GetEvent: pops the oldest pushed event, else asks the system layer (Sys_GetEvent). Returns the 0x18-byte
// event through the hidden struct-return pointer.
void Com_GetEvent(struct sysEvent_s *sret)
{
    unsigned int index;

    index = com_pushedEventsTail;
    if ((int)com_pushedEventsTail < com_pushedEventsHead) {
        com_pushedEventsTail = com_pushedEventsTail + 1;
        *sret = com_pushedEvents[index & 0xff];
        return;
    }
    *sret = Sys_GetEvent();
}

// scope guard reproducing the C++ cleanup (landing pad at 0x08061a82) of the temporary buffer in Com_EventLoop
struct ScopedBufGuard {
    ScopedBuf16 *buf;
    explicit ScopedBufGuard(ScopedBuf16 *b) : buf(b) {}
    ~ScopedBufGuard() { Com_PoolRestoreFrom2((int *)buf); }
};

// 0x0806182a  size=600  FUN_0806182a  status=translated  naming=observed
// String 'Com_EventLoop: oversize packet\n'; "\025Com_EventLoop: bad event type %i". Q3 Com_EventLoop: an
// empty event first drains loopback packets for socket 1 (into a 0x20000-byte temporary message buffer) and
// returns that event's time. Type 4 = console command text, appended to the command buffer plus "\n"; type 5 =
// packet whose payload follows a 0x14-byte netadr. Packets are delivered via SV_ReadPackets only while
// com_sv_running is set. Returns the time of the terminating empty event.
int Com_EventLoop(void)
{
    ScopedBuf16 scratch;
    msg_s msg;
    netadr_s from;
    netadr_s evFrom;
    sysEvent_s ev;
    byte *scratchData;
    int evTime;

    Com_PoolAllocInto2((int *)&scratch, 0x20000);
    ScopedBufGuard guard(&scratch);
    scratchData = (byte *)Com_PoolPtrFromOffset((int *)&scratch);
    MSG_Init(&msg, scratchData, 0x20000);
    for (;;) {
        Com_GetEvent(&ev);
        switch (ev.evType) {
        case 0:
            while (NET_PollLoopPacket(1, &from, &msg) != 0) {
                Noop_08060c1a(0);
                if (com_sv_running->current.enabled != '\0')
                    SV_ReadPackets(from, &msg);
            }
            evTime = ev.evTime;
            return evTime;
        case 4:
            Noop_08060c1a(0);
            Cbuf_AddText((const char *)ev.evPtr);
            Com_Free_A(ev.evPtr);
            Cbuf_AddText("\n");
            break;
        case 5:
            memcpy(&evFrom, ev.evPtr, sizeof(netadr_s));
            msg.cursize = ev.evPtrLength - 0x14;
            if ((unsigned int)msg.maxsize < (unsigned int)msg.cursize) {
                Com_Free_A(ev.evPtr);
                Com_Printf("Com_EventLoop: oversize packet\n");
            } else {
                memcpy(msg.data, (char *)ev.evPtr + 0x14, msg.cursize);
                Com_Free_A(ev.evPtr);
                if (com_sv_running->current.enabled != '\0') {
                    Noop_08060c1a(0);
                    SV_ReadPackets(evFrom, &msg);
                }
            }
            break;
        default:
            Com_Error(0, "\025Com_EventLoop: bad event type %i", ev.evType);
            break;
        }
    }
}

// 0x08061ab8  size=196  FUN_08061ab8  status=translated  naming=unknown
// Event drain variant without the network: type 4 console text is appended with "\n", type 5 payloads are freed,
// anything but 0/4/5 raises the same 'bad event type' error. Returns 0 if the first event was the empty
// terminator, otherwise 1.
int Com_ProcessConsoleEvents(void)
{
    sysEvent_s ev;
    int processedAny;

    processedAny = 0;
    for (;;) {
        Com_GetEvent(&ev);
        if (ev.evType == 4) {
            Cbuf_AddText((const char *)ev.evPtr);
            Com_Free_A(ev.evPtr);
            Cbuf_AddText("\n");
        } else {
            if (ev.evType < 5) {
                if (ev.evType == 0)
                    return processedAny;
            } else if (ev.evType == 5) {
                Com_Free_A(ev.evPtr);
                processedAny = 1;
                continue;
            }
            Com_Error(0, "\025Com_EventLoop: bad event type %i", ev.evType);
        }
        processedAny = 1;
    }
}

// 0x08061b7c  size=85  FUN_08061b7c  status=translated  naming=inferred
// Pulls events from the system layer (Sys_GetEvent) and pushes each non-empty one into the pushed-event queue
// until an empty event arrives; returns that event's time (Q3 Com_PumpEvents/Sys_GetEvent loop).
int Com_PumpSysEvents(void)
{
    sysEvent_s ev;

    do {
        ev = Sys_GetEvent();
        if (ev.evType != 0)
            Com_PushEvent(&ev);
    } while (ev.evType != 0);
    return ev.evTime;
}
