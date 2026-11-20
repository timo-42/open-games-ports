#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_005_types.h"

/*
 * Chunk 005: collision trace (BSP and kd-tree traversal, brush and leaf tests), entity
 * sphere/segment tests, entity area links and the area sweep.
 *
 * Source: Ghidra pseudocode of the 53 chunk functions, checked against the disassembly
 * where the pseudocode was ambiguous (x87 precision, return values carried in st0,
 * frame-slot aliases). Double literals are kept where the binary uses double constants.
 */

/* ---- in-chunk prototypes ---- */
static void Sweep_BuildSegment(traceWork_s *tr, const float *start, const float *end, const float *mins, const float *maxs, uint32 mask);

/* ---- record addressing ---- */

/* Entity link record for 1-based index (record n+1 is entity n). */
static inline gentity_s *cm5_GentityNum(svEntity_s *lk)
{
    return SV_GentityNumFromSvEntity(lk);
}

static svEntity_s *Area_LinkRecord(uint32 index)
{
    return (svEntity_s *)(g_084424a4_areaEntityLinkRecords + index * 0x174);
}

/* Brush box record for 1-based index. */
static cm_static_model_s *Area_BoxRecord(uint32 index)
{
    return g_08188dc8_staticModels + (index - 1);
}

/* ---- scalar and vector helpers ---- */

/* 805d552: returns if_nonneg when test >= 0, else if_neg. */
// 0x0805d552  size=41  FUN_0805d552  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: disassembly: fucompp against 0, returns arg2 when arg1 >= 0 else arg3
float Sweep_FloatSelect(float test, float if_nonneg, float if_neg)
{
    if (0.0 <= test) {
        return if_nonneg;
    }
    return if_neg;
}

/* 805d57c: absolute value. */
// 0x0805d57c  size=25  FUN_0805d57c  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): disassembly: fabs on the x87 stack
float Sweep_FloatAbs(float x)
{
    return fabsf(x);
}

/* 805d596: square root through double, rounded to float. */
// 0x0805d596  size=31  FUN_0805d596  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): disassembly: call sqrt@plt on a widened double
float Sweep_FloatSqrt(float x)
{
    return (float)sqrt((double)x);
}

/* 805d5b6: 1 when x >= 0 (0 for NaN). */
// 0x0805d5b6  size=23  FUN_0805d5b6  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: disassembly: setae on compare with 0
int Sweep_FloatIsNonNegative(float x)
{
    return (0.0 <= x) ? 1 : 0;
}

/* 805d5ce: maximum of a and b, decided by the sign of a - b. */
// 0x0805d5ce  size=36  FUN_0805d5ce  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): tail-calls Sweep_FloatSelect(a - b, a, b): returns the value in st0, so Ghidra's void is an artefact
float Sweep_FloatMax(float a, float b)
{
    float d = a - b;
    return Sweep_FloatSelect(d, a, b);
}

/* 805d5f2: minimum of a and b, decided by the sign of b - a. */
// 0x0805d5f2  size=36  FUN_0805d5f2  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): tail-calls Sweep_FloatSelect(b - a, a, b)
float Sweep_FloatMin(float a, float b)
{
    float d = b - a;
    return Sweep_FloatSelect(d, a, b);
}

/* 805d616: dot product of the x and y components, extended precision. */
// 0x0805d616  size=33  FUN_0805d616  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): x and y products in extended precision
long double Vec2_Dot(const float *a, const float *b)
{
    return (long double)a[1] * (long double)b[1] + (long double)a[0] * (long double)b[0];
}

/* 805d638: zero a vec3. */
// 0x0805d638  size=41  FUN_0805d638  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): zeroes three words
void Vec3_Clear(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

/* 805d662: copy a vec3. */
// 0x0805d662  size=47  FUN_0805d662  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): copies three words
void Vec3_Copy(const float *src, float *dst)
{
    /* the binary copies raw 32-bit words (mov eax,[src]; mov [dst],eax), never via the x87 stack */
    const volatile uint32 *s = (const volatile uint32 *)src;
    volatile uint32 *d = (volatile uint32 *)dst;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

/* 805d692: negate a vec3 by flipping the sign bit of each word. */
// 0x0805d692  size=62  FUN_0805d692  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): flips the sign bit of three words
void Vec3_Negate(uint32 *bits)
{
    bits[0] ^= 0x80000000u;
    bits[1] ^= 0x80000000u;
    bits[2] ^= 0x80000000u;
}

/* 805d6d0: out = a + b. */
// 0x0805d6d0  size=68  FUN_0805d6d0  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): component-wise sum
void Vec3_Add(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

/* 805d714: out = a - b. */
// 0x0805d714  size=68  FUN_0805d714  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): component-wise difference
void Vec3_Subtract(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

/* 805d758: dot product, extended precision, accumulated z, y, x. */
// 0x0805d758  size=51  FUN_0805d758  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): dot product, extended precision, z term first
long double Vec3_Dot(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2]
         + (long double)a[1] * (long double)b[1]
         + (long double)a[0] * (long double)b[0];
}

/* 805d78c: exact equality of two vec3. */
// 0x0805d78c  size=106  FUN_0805d78c  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): exact component equality
int Vec3_Equal(const float *a, const float *b)
{
    if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) {
        return 1;
    }
    return 0;
}

/* 805d7f6: squared length of a vec3, extended precision. */
// 0x0805d7f6  size=51  FUN_0805d7f6  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): self dot product, extended precision
long double Vec3_LengthSquared(const float *v)
{
    return (long double)v[2] * (long double)v[2]
         + (long double)v[1] * (long double)v[1]
         + (long double)v[0] * (long double)v[0];
}

/* 805d82a: copy a vec4. */
// 0x0805d82a  size=63  FUN_0805d82a  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): copies four words
void Vec4_Copy(const float *src, float *dst)
{
    /* raw word copy as in the binary (no x87 round trip) */
    const volatile uint32 *s = (const volatile uint32 *)src;
    volatile uint32 *d = (volatile uint32 *)dst;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = s[3];
}

/* 805d86c: box[6+c] = 1/(box[c] - box[c+3]) per axis, or 0 when that difference is zero. */
// 0x0805d86c  size=115  FUN_0805d86c  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): box[6+c] = 1/(box[c]-box[c+3]), 0 when the difference is zero
void Box_ComputeInverseExtents(float *box)
{
    int c;
    for (c = 0; c < 3; c++) {
        volatile float e = box[c] - box[c + 3]; /* the binary rounds the difference to float (fstp [ebp-0xc]) before 1/e */
        float inv;
        if (e != 0.0f) {
            inv = 1.0f / e;
        } else {
            inv = 0.0f;
        }
        box[6 + c] = inv;
    }
}

/* 805da9e (and the 805da56 / 805da7a wrappers): same selection as Float_SelectNonNegative. */
// 0x0805da9e  size=41  FUN_0805da9e  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): same as Float_SelectNonNegative
float Float_SelectNonNegativeAlias(float test, float if_nonneg, float if_neg)
{
    return Sweep_FloatSelect(test, if_nonneg, if_neg);
}

/* 805da56: maximum of a and b. */
// 0x0805da56  size=36  FUN_0805da56  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): same as Sweep_FloatMax (separate binary function)
float Float_MaxAlias(float a, float b)
{
    float d = a - b;
    return Float_SelectNonNegativeAlias(d, a, b);
}

/* 805da7a: minimum of a and b. */
// 0x0805da7a  size=36  FUN_0805da7a  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): same as Sweep_FloatMin (separate binary function)
float Float_MinAlias(float a, float b)
{
    float d = b - a;
    return Float_SelectNonNegativeAlias(d, a, b);
}

// 0x0805d8e0  size=374  FUN_0805d8e0  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): Liang-Barsky style slab test; returns 1 on miss
/*
 * 805d8e0: slab test of the segment p0..p1 against the box, limited to parameters
 * below max_t. Returns 1 when the segment misses the box, 0 when it overlaps it.
 */
int Box_SegmentSlabTest(const struct cod2_box_s *box, const float *p0, const float *p1, float max_t)
{
    float t_enter = 0.0f;
    float t_leave = max_t;
    float sign = -1.0f;
    const float *origin = p0;
    int c;

    for (;;) {
        for (c = 0; c < 3; c++) {
            float f1 = (box->mins[c] - origin[c]) * sign;
            float f2 = (box->maxs[c] - origin[c]) * sign;
            if (0.0f < f1) {
                if (0.0f < f2) {
                    return 1;
                }
                f1 = f1 * box->inv[c] * sign;
                if (t_leave <= f1) {
                    return 1;
                }
                t_enter = Float_MaxAlias(t_enter, f1);
            } else if (0.0f < f2) {
                f1 = f1 * box->inv[c] * sign;
                if (f1 <= t_enter) {
                    return 1;
                }
                t_leave = Float_MinAlias(t_leave, f1);
            }
        }
        if (sign == 1.0f) {
            return 0;
        }
        sign = 1.0f;
        origin = p1;
    }
}

/* ---- area node allocation and tree maintenance ---- */

// 0x0805dac8  size=228  FUN_0805dac8  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: pops the free list; axis = larger extent; midpoint split; 512-unit minimum
/*
 * 805dac8: pops a node from the free list, sets its split axis (the larger of the two
 * projected extents) and midpoint. Returns 0 when the free list is empty or the box is
 * no wider than 512 on that axis.
 */
uint16 Area_AllocSplitNode(const float *a, const float *b)
{
    uint16 result = g_0818901a_areaNodeFreeHead;
    float extent[2];
    uint16 axis;
    cod2_area_node_s *node;

    if (g_0818901a_areaNodeFreeHead == 0) {
        return 0;
    }
    Vec2_Subtract(b, a, extent);
    axis = (uint16)(extent[0] <= extent[1]);
    if (extent[axis] <= 512.0) {
        return 0;
    }
    node = &g_0818901c_areaNodes[result];
    g_0818901a_areaNodeFreeHead = node->parent;
    node->split_axis = axis;
    node->split_dist = (b[axis] + a[axis]) * 0.5f;
    return result;
}

/* 805dbac: clears the area tables, builds the free list and sets the root split. */
// 0x0805dbac  size=229  FUN_0805dbac  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: clears 0x601c bytes, free list from node 2, root node 1 split
void Area_Init(void)
{
    float extent[2];
    uint16 i;

    memset(g_08189000_areaWorldMins, 0, 0x601c);
    CM_ModelBounds(0, g_08189000_areaWorldMins, g_0818900c_areaWorldMaxs);
    g_0818901a_areaNodeFreeHead = 2;
    for (i = 2; i < 0x3ff; i++) {
        g_0818901c_areaNodes[i].parent = (uint16)(i + 1);
    }
    g_0818f016_areaNodes3ffParent = 0;
    Vec2_Subtract(g_0818900c_areaWorldMaxs, g_08189000_areaWorldMins, extent);
    g_0818901c_areaNodes[1].split_axis = (uint16)(extent[0] <= extent[1]);
    g_0818901c_areaNodes[1].split_dist = (g_0818900c_areaWorldMaxs[g_0818901c_areaNodes[1].split_axis] + g_08189000_areaWorldMins[g_0818901c_areaNodes[1].split_axis]) * 0.5f;
}

/* 805dc92: re-initialise the area tables and relink every brush box. */
// 0x0805dc92  size=18  FUN_0805dc92  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: Area_Init then Area_RelinkAllBoxes
void Area_ResetAndRelinkAll(void)
{
    Area_Init();
    Area_RelinkAllBoxes();
}

// 0x0805dca4  size=552  FUN_0805dca4  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: unlinks from node list, frees empty nodes toward the root, recomputes entity contents
/*
 * 805dca4: removes an entity from its node list, then frees nodes that became empty,
 * walking towards the root, and refreshes the entity contents of the remaining nodes.
 */
void Area_UnlinkEntity(struct svEntity_s *link)
{
    uint16 node_index = link->worldSector;
    cod2_area_node_s *node;
    uint16 cur;
    svEntity_s *prev;
    uint32 mask;

    if (node_index == 0) {
        return;
    }
    node = &g_0818901c_areaNodes[node_index];
    link->worldSector = 0;

    if (Area_LinkRecord(node->entity_head) == link) {
        node->entity_head = link->nextEntityInWorldSector;
    } else {
        cur = node->entity_head;
        for (;;) {
            prev = Area_LinkRecord(cur);
            if (Area_LinkRecord(prev->nextEntityInWorldSector) == link) {
                break;
            }
            cur = prev->nextEntityInWorldSector;
        }
        prev->nextEntityInWorldSector = link->nextEntityInWorldSector;
    }

    for (;;) {
        uint16 up;
        if (!(node->entity_head == 0 && node->brush_head == 0 &&
              node->child[0] == 0 && node->child[1] == 0)) {
            break;
        }
        node->entity_contents = 0;
        if (node->parent == 0) {
            break;
        }
        up = node->parent;
        node->parent = g_0818901a_areaNodeFreeHead;
        g_0818901a_areaNodeFreeHead = node_index;
        if (g_0818901c_areaNodes[up].child[0] == node_index) {
            g_0818901c_areaNodes[up].child[0] = 0;
        } else {
            g_0818901c_areaNodes[up].child[1] = 0;
        }
        node_index = up;
        node = &g_0818901c_areaNodes[node_index];
    }

    for (;;) {
        mask = g_0818901c_areaNodes[node->child[1]].entity_contents | g_0818901c_areaNodes[node->child[0]].entity_contents;
        if (node->entity_head != 0) {
            uint16 walk = node->entity_head;
            for (;;) {
                svEntity_s *lk = Area_LinkRecord(walk);
                gentity_s *ent = cm5_GentityNum(lk);
                mask |= ent->r.contents;
                if (lk->nextEntityInWorldSector == 0) {
                    break;
                }
                walk = lk->nextEntityInWorldSector;
            }
        }
        node->entity_contents = mask;
        if (node->parent == 0) {
            break;
        }
        node = &g_0818901c_areaNodes[node->parent];
    }
}

// 0x0805decc  size=135  FUN_0805decc  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: sorted insert by entity number (record index - 1)
/*
 * 805decc: inserts a link record into a node's entity list. The list is ordered by entity
 * number (record index - 1); the record's node and next fields are set.
 */
void Area_InsertEntityIntoNode(struct svEntity_s *link, uint16 node)
{
    /* the binary does not divide: ((link - 0x8442618) >> 2) * 0x3f4fd3f5 (modular inverse of 0x174/4), truncated to 16 bits */
    uint16 n = (uint16)((uint32)((((int)(((char *)link) - (char *)g_084424a4_areaEntityLinkRecords)) - 0x174) >> 2) * 0x3f4fd3f5u);
    uint16 *slot = &g_0818901c_areaNodes[node].entity_head;

    while ((uint16)(*slot - 1) <= n) {
        slot = &Area_LinkRecord(*slot)->nextEntityInWorldSector;
    }
    link->worldSector = node;
    link->nextEntityInWorldSector = *slot;
    *slot = (uint16)(n + 1);
}

// 0x0805df54  size=128  FUN_0805df54  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: sorted insert by box index
/*
 * 805df54: inserts a brush box into a node's brush list, ordered by box index (0-based
 * index k is stored as k + 1).
 */
void Area_InsertBoxIntoNode(struct cm_static_model_s *box, uint16 node)
{
    uint16 k = (uint16)(box - g_08188dc8_staticModels);
    uint16 *slot = &g_0818901c_areaNodes[node].brush_head;

    while ((uint16)(*slot - 1) <= k) {
        slot = &Area_BoxRecord(*slot)->next;
    }
    box->next = *slot;
    *slot = (uint16)(k + 1);
}

// 0x0805dfd4  size=956  FUN_0805dfd4  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: moves one-sided entities and boxes into children; allocates children
/*
 * 805dfd4: moves the entities and brush boxes of a node that lie wholly on one side of
 * the split plane into the matching child node, allocating children as needed. Nothing
 * is moved while the g_08189018_areaLinkDisabled flag is set.
 */
void Area_LinkIntoSubtree(uint16 node, const float *mins, const float *maxs)
{
    cod2_area_node_s *nd;
    uint16 axis;
    float dist;
    uint16 idx;
    svEntity_s *prev_link;
    cm_static_model_s *prev_box;
    uint16 child;

    if (g_08189018_areaLinkDisabled != 0) {
        return;
    }
    nd = &g_0818901c_areaNodes[node];
    axis = nd->split_axis;
    dist = nd->split_dist;

    prev_link = 0;
    idx = nd->entity_head;
    while (idx != 0) {
        svEntity_s *rec = Area_LinkRecord(idx);
        float rec_min = *(float *)((char *)rec + 0x164 + axis * 4);
        float rec_max = *(float *)((char *)rec + 0x16c + axis * 4);
        int front = 0;
        int back = 0;
        uint16 next_idx;

        if (dist < rec_min) {
            front = 1;
        } else if (rec_max < dist) {
            back = 1;
        }
        if (front || back) {
            child = front ? nd->child[0] : nd->child[1];
            if (child == 0) {
                child = Area_AllocSplitNode(mins, maxs);
                if (child != 0) {
                    if (front) {
                        nd->child[0] = child;
                    } else {
                        nd->child[1] = child;
                    }
                    g_0818901c_areaNodes[child].parent = node;
                }
            }
            if (child != 0) {
                gentity_s *ent;
                next_idx = rec->nextEntityInWorldSector;
                Area_InsertEntityIntoNode(rec, child);
                ent = cm5_GentityNum(rec);
                g_0818901c_areaNodes[child].entity_contents |= ent->r.contents;
                if (prev_link == 0) {
                    nd->entity_head = next_idx;
                } else {
                    prev_link->nextEntityInWorldSector = next_idx;
                }
                idx = next_idx;
                continue;
            }
        }
        idx = rec->nextEntityInWorldSector;
        prev_link = rec;
    }

    prev_box = 0;
    idx = nd->brush_head;
    while (idx != 0) {
        cm_static_model_s *box = Area_BoxRecord(idx);
        int front = 0;
        int back = 0;
        uint16 next_idx;

        if (dist < box->mins[axis]) {
            front = 1;
        } else if (box->maxs[axis] < dist) {
            back = 1;
        }
        if (front || back) {
            child = front ? nd->child[0] : nd->child[1];
            if (child == 0) {
                child = Area_AllocSplitNode(mins, maxs);
                if (child != 0) {
                    if (front) {
                        nd->child[0] = child;
                    } else {
                        nd->child[1] = child;
                    }
                    g_0818901c_areaNodes[child].parent = node;
                }
            }
            if (child != 0) {
                next_idx = box->next;
                Area_InsertBoxIntoNode(box, child);
                g_0818901c_areaNodes[child].brush_contents |= XModel_GetContents(box->handle);
                if (prev_box == 0) {
                    nd->brush_head = next_idx;
                } else {
                    prev_box->next = next_idx;
                }
                idx = next_idx;
                continue;
            }
        }
        idx = box->next;
        prev_box = box;
    }
}

// 0x0805e390  size=588  FUN_0805e390  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: contents from type info block; locate node; relink or refresh stored bounds
/*
 * 805e390: links an entity with the given bounds and type. Its contents come from the
 * type's info block. The search descends from the root; when the entity already sits in
 * the matching node it only refreshes its stored bounds.
 */
void Area_LinkEntity(struct svEntity_s *link, const float *mins, const float *maxs, int type)
{
    unsigned char *info = (unsigned char *)cm_model_get(type);
    uint32 contents = *(uint32 *)(info + 0x24) | *(uint32 *)(info + 0x20);
    uint32 *stored_contents = (uint32 *)((char *)link + 0x160);
    float *stored_mins = (float *)((char *)link + 0x164);
    float *stored_maxs = (float *)((char *)link + 0x16c);
    float bounds[5];
    uint16 node = 0;
    uint16 axis;
    float dist;
    cod2_area_node_s *nd;
    int found;

    if (contents == 0) {
        Area_UnlinkEntity(link);
        return;
    }

    for (;;) {
        Vec2_Copy_0805fba0(g_08189000_areaWorldMins, bounds + 2);
        Vec2_Copy_0805fba0(g_0818900c_areaWorldMaxs, bounds);
        node = 1;
        found = 1;
        for (;;) {
            for (;;) {
                nd = &g_0818901c_areaNodes[node];
                nd->entity_contents |= contents;
                axis = nd->split_axis;
                dist = nd->split_dist;
                if (mins[axis] <= dist) {
                    break;
                }
                bounds[axis + 2] = dist;
                if (nd->child[0] == 0) {
                    found = 0;
                    break;
                }
                node = nd->child[0];
            }
            if (!found) {
                break;
            }
            if (dist <= maxs[axis]) {
                break;
            }
            bounds[axis] = dist;
            if (nd->child[1] == 0) {
                found = 0;
                break;
            }
            node = nd->child[1];
        }

        if (found && node == link->worldSector && ((~contents & *stored_contents) == 0)) {
            *stored_contents = contents;
            Vec2_Copy_0805fba0(mins, stored_mins);
            Vec2_Copy_0805fba0(maxs, stored_maxs);
            return;
        }
        if (link->worldSector != 0) {
            if (!(node == link->worldSector && ((~contents & *stored_contents) == 0))) {
                Area_UnlinkEntity(link);
                continue;
            }
        } else {
            Area_InsertEntityIntoNode(link, node);
        }
        break;
    }

    *stored_contents = contents;
    Vec2_Copy_0805fba0(mins, stored_mins);
    Vec2_Copy_0805fba0(maxs, stored_maxs);
    Area_LinkIntoSubtree(node, bounds + 2, bounds);
}

// 0x0805e5dc  size=321  FUN_0805e5dc  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: descends to the node that contains the box, inserts, splits downwards
/*
 * 805e5dc: links a brush box into the area tree at the deepest node that fully contains
 * it, then splits the node's lists downwards.
 */
void Area_LinkBoxRecord(struct cm_static_model_s *box)
{
    uint32 mask = XModel_GetContents(box->handle);
    float bounds[5];
    uint16 node = 1;
    uint16 axis;
    float dist;
    cod2_area_node_s *nd;

    Vec2_Copy_0805fba0(g_08189000_areaWorldMins, bounds + 2);
    Vec2_Copy_0805fba0(g_0818900c_areaWorldMaxs, bounds);
    for (;;) {
        for (;;) {
            nd = &g_0818901c_areaNodes[node];
            nd->brush_contents |= mask;
            axis = nd->split_axis;
            dist = nd->split_dist;
            if (dist < box->mins[axis]) {
                break;
            }
            if (dist <= box->maxs[axis]) {
                goto place_box;
            }
            bounds[axis] = dist;
            if (nd->child[1] == 0) {
                goto place_box;
            }
            node = nd->child[1];
        }
        bounds[axis + 2] = dist;
        if (nd->child[0] == 0) {
            break;
        }
        node = nd->child[0];
    }
place_box:
    Area_InsertBoxIntoNode(box, node);
    Area_LinkIntoSubtree(node, bounds + 2, bounds);
}

/* 805e71e: relinks every brush box whose contents mask is nonzero. */
// 0x0805e71e  size=80  FUN_0805e71e  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: relinks every box with nonzero contents
void Area_RelinkAllBoxes(void)
{
    cm_static_model_s *box = g_08188dc8_staticModels;
    int i;
    for (i = 0; i < g_08188dc4_numStaticModels; i++) {
        if (XModel_GetContents(box->handle) != 0) {
            Area_LinkBoxRecord(box);
        }
        box = box + 1;
    }
}

// 0x0805e76e  size=535  FUN_0805e76e  status=translated  naming=observed
// naming evidence: string or observed symbol in the function (see symbols/chunk_005.json): string 'CM_AreaEntities: MAXCOUNT\n' at 0x805e76e
/*
 * 805e76e: recursive collector behind CM_AreaEntities. Appends the entity number of each
 * linked entity whose absolute bounds overlap the query box.
 */
void CM_AreaEntities_r(uint32 node_index, struct cod2_area_query_s *query)
{
    cod2_area_node_s *nd = &g_0818901c_areaNodes[node_index & 0xffff];

    if ((query->mask & nd->entity_contents) != 0) {
        uint16 idx = nd->entity_head;
        while (idx != 0) {
            svEntity_s *lk = Area_LinkRecord(idx);
            gentity_s *ent = cm5_GentityNum(lk);
            if ((ent->r.contents & query->mask) != 0 &&
                ent->r.absmin[0] <= query->maxs[0] &&
                query->mins[0] <= ent->r.absmax[0] &&
                ent->r.absmin[1] <= query->maxs[1] &&
                query->mins[1] <= ent->r.absmax[1] &&
                ent->r.absmin[2] <= query->maxs[2] &&
                query->mins[2] <= ent->r.absmax[2]) {
                if (query->count == query->maxcount) {
                    Com_DPrintf("CM_AreaEntities: MAXCOUNT\n");
                    return;
                }
                /* (record index - 1), written as the multiplicative inverse used by the binary */
                query->list[query->count] =
                    (int)((uint32)((((int)(((char *)lk) - (char *)g_084424a4_areaEntityLinkRecords)) - 0x174) >> 2) * 0x3f4fd3f5u);
                query->count = query->count + 1;
            }
            idx = lk->nextEntityInWorldSector;
        }
        if (nd->split_dist < query->maxs[nd->split_axis]) {
            CM_AreaEntities_r(nd->child[0], query);
        }
        if (query->mins[nd->split_axis] < nd->split_dist) {
            CM_AreaEntities_r(nd->child[1], query);
        }
    }
}

/* 805e986: CM_AreaEntities(mins, maxs, list, maxcount, mask) returns the entity count. */
// 0x0805e986  size=67  FUN_0805e986  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): wrapper building the query struct and calling the recursive worker; returns the count; name from the string in 805e76e
int CM_AreaEntities(const float *mins, const float *maxs, int *list, int maxcount, uint32 mask)
{
    cod2_area_query_s q;
    q.unk_00 = 0;
    q.mins = mins;
    q.maxs = maxs;
    q.list = list;
    q.count = 0;
    q.maxcount = maxcount;
    q.mask = mask;
    CM_AreaEntities_r(1, &q);
    return q.count;
}

// 0x0805e9ca  size=609  FUN_0805e9ca  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: sweeps a vec4 segment through the area tree, tests boxes per node, recurses at straddling nodes
/*
 * 805e9ca: sweeps the vec4 segment start4..end4 through the area tree and tests the brush
 * boxes of each node visited against the query box. Recurses at a straddling node.
 */
void Area_SweepBoxRecords(struct cod2_area_query_box_s *query, uint16 node_index, const float *start4, const float *end4, float *frac)
{
    float seg[4];
    float mid[4];
    uint16 e = node_index;
    cod2_area_node_s *nd;
    float side_a = 0.0f;
    float side_b = 0.0f;
    float t;
    int side;
    int k;

    Vec4_Copy(start4, seg);
    for (;;) {
        for (;;) {
            nd = &g_0818901c_areaNodes[e];
            if ((query->mask & nd->brush_contents) == 0) {
                return;
            }
            {
                uint16 bi = nd->brush_head;
                while (bi != 0) {
                    cm_static_model_s *box = Area_BoxRecord(bi);
                    uint32 bmask = XModel_GetContents(box->handle);
                    if ((bmask & query->mask) != 0 &&
                        Box_SegmentSlabTest(&query->box, box->mins, box->maxs, *frac) == 0) {
                        cm_static_model_trace(box, (trace_s *)frac, (float *)query->box.mins, (float *)query->box.maxs, query->mask);
                    }
                    bi = box->next;
                }
            }
            side_a = seg[nd->split_axis] - nd->split_dist;
            side_b = end4[nd->split_axis] - nd->split_dist;
            if (side_a * side_b < 0.0f) {
                break;
            }
            t = SpatialTree_FloatMin(side_a, side_b);
            side = Sweep_FloatIsNonNegative((float)t);
            e = nd->child[1 - side];
        }
        if (*frac <= seg[3]) {
            break;
        }
        t = side_a / (side_a - side_b);
        for (k = 0; k < 4; k++) {
            mid[k] = seg[k] + (end4[k] - seg[k]) * t;
        }
        side = Sweep_FloatIsNonNegative(side_b);
        Area_SweepBoxRecords(query, nd->child[side], seg, mid, frac);
        e = nd->child[1 - side];
        Vec4_Copy(mid, seg);
    }
}

// 0x0805ec2c  size=168  FUN_0805ec2c  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: builds the query box from two corners and sweeps the segment
/*
 * 805ec2c: builds the area query box from two corner points and sweeps the segment
 * between them.
 */
void Area_SweepBoxFromPoints(float *result, const float *a, const float *b, uint32 mask)
{
    cod2_area_query_box_s q;
    /* gameplay-fix(net_extra): the segment endpoints are vec4: start[3] = 0 (mov [ebp-0x3c],0 at 0x0805ec8a) and
     * end[3] = the initial *result dword (mov [ebp-0x4c],[result] at 0x0805eca7). Area_SweepBoxRecords uses [3] as the
     * running trace fraction (`*frac <= seg[3]`), the 3-float arrays passed stack garbage (the contents mask) there. */
    float start[4];
    float end[4];

    q.mask = mask;
    Vec3_Copy_0805fbec(a, q.box.mins);
    Vec3_Copy_0805fbec(b, q.box.maxs);
    Box_ComputeInverseExtents((float *)&q.box);
    Vec3_Copy_0805fbec(q.box.mins, start);
    ((uint32 *)start)[3] = 0;
    Vec3_Copy_0805fbec(q.box.maxs, end);
    ((uint32 *)end)[3] = *(const uint32 *)result;
    Area_SweepBoxRecords(&q, 1, start, end, result);
}

// 0x0805ecd4  size=590  FUN_0805ecd4  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: returns 1 when the segment misses all area boxes and entities, 0 at the first hit
/*
 * 805ecd4: returns 1 when the segment start3..end3 misses every linked box and entity, 0 at
 * the first hit. Recurses at a straddling node.
 */
int Area_TraceSegmentRecords(struct SegQueryB *query, uint16 node_index, const float *start3, const float *end3)
{
    float seg[5];
    uint16 e = node_index;
    cod2_area_node_s *nd;
    float side_a;
    float side_b;
    float t;
    int side;

    Vec3_Copy_0805fbec(start3, seg);
    for (;;) {
        nd = &g_0818901c_areaNodes[e];
        if ((query->mask & nd->brush_contents) == 0) {
            return 1;
        }
        {
            uint16 bi = nd->brush_head;
            while (bi != 0) {
                cm_static_model_s *box = Area_BoxRecord(bi);
                uint32 bmask = XModel_GetContents(box->handle);
                if ((bmask & query->mask) != 0 &&
                    Box_SegmentSlabTest(&query->box, box->mins, box->maxs, 1.0) == 0 &&
                    cm_static_model_test(box, (float *)query, (float *)query->box.maxs, query->mask) == 0) {
                    return 0;
                }
                bi = box->next;
            }
        }
        side_a = seg[nd->split_axis] - nd->split_dist;
        side_b = end3[nd->split_axis] - nd->split_dist;
        if (0.0 <= side_a * side_b) {
            t = SpatialTree_FloatMin(side_a, side_b);
            side = Sweep_FloatIsNonNegative((float)t);
            e = nd->child[1 - side];
        } else {
            float frac = side_a / (side_a - side_b);
            float mid[5];
            int k;
            for (k = 0; k < 3; k++) {
                mid[k] = seg[k] + (start3[k] - seg[k]) * frac;
            }
            side = Sweep_FloatIsNonNegative(side_b);
            if (Area_TraceSegmentRecords(query, nd->child[side], seg, mid) == 0) {
                return 0;
            }
            e = nd->child[1 - side];
            Vec3_Copy_0805fbec(mid, seg);
        }
    }
}

/* ---- BSP and kd-tree trace ---- */

// 0x0805b058  size=835  FUN_0805b058  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): BSP walk with plane pointer and int16 children (node stride 8), leaf = -1-index, result->fraction <= start[3] stop; shape matches id Tech 3 CM_RecursiveHullCheck (not confirmed by string)
/*
 * 805b058 (CM_RecursiveHullCheck): walks the BSP from node_index, clipping the vec4 segment
 * against each splitting plane. Leaves are handed to cm_leaf_surface_trace.
 */
void CM_RecursiveHullCheck(struct traceWork_s *tr, int node_index, const float *start4, const float *end4, struct trace_s *result)
{
    float seg[4];
    cnode_s *node = 0;
    cplane_s *plane;
    float d_start = 0.0f;
    float d_end = 0.0f;
    float extent = 0.0f;
    float t_min;
    float t_max;
    float abs_delta;
    float num;
    float inv_abs;
    float cut_lo;
    float cut_hi;
    float scale;
    float mid[4];
    int side;
    int k;

    Vec4_Copy(start4, seg);
    for (;;) {
        for (;;) {
            for (;;) {
                if (node_index < 0) {
                    cm_leaf_surface_trace(tr, (g_08188de8_leafs + (-1 - node_index)), result);
                    return;
                }
                node = &g_08188de0_nodes[node_index];
                plane = node->plane;
                if (plane->type < 3) {
                    d_start = seg[plane->type] - plane->dist;
                    d_end = end4[plane->type] - plane->dist;
                    extent = tr->extents[plane->type] + 0.125;
                } else {
                    d_start = (float)(Vec3_Dot(seg, plane->normal) - (long double)plane->dist);
                    d_end = (float)(Vec3_Dot(end4, plane->normal) - (long double)plane->dist);
                    if (tr->is_point == 0) {
                        extent = 2048.0;
                    } else {
                        extent = 0.125;
                    }
                }
                t_min = Sweep_FloatMin(d_start, d_end);
                if ((long double)t_min < (long double)extent) {
                    break;
                }
                node_index = node->child[0];
            }
            t_max = Sweep_FloatMax(d_start, d_end);
            if (-(long double)extent < (long double)t_max) {
                break;
            }
            node_index = node->child[1];
        }

        if (result->fraction <= seg[3]) {
            break;
        }
        abs_delta = Sweep_FloatAbs(d_end - d_start);
        if (4.7683716e-07 < abs_delta) {
            num = Sweep_FloatSelect(d_end - d_start, -d_start, d_start);
            inv_abs = 1.0f / abs_delta;
            cut_lo = (num - extent) * inv_abs;
            cut_hi = (num + extent) * inv_abs;
            side = Sweep_FloatIsNonNegative(d_end - d_start);
        } else {
            side = 0;
            cut_hi = 1.0f;
            cut_lo = 0.0f;
        }
        scale = Sweep_FloatMin(cut_hi, 1.0);
        for (k = 0; k < 4; k++) {
            mid[k] = seg[k] + (end4[k] - seg[k]) * scale;
        }
        CM_RecursiveHullCheck(tr, node->child[side], seg, mid, result);
        cut_lo = Sweep_FloatMax(cut_lo, 0.0);
        for (k = 0; k < 4; k++) {
            seg[k] = seg[k] + (end4[k] - seg[k]) * cut_lo;
        }
        node_index = node->child[1 - side];
    }
}

// 0x0805bbe6  size=779  FUN_0805bbe6  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): enter/leave fractions over brush bounds then side planes with radius+skin; returns brush index+1 on hit; shape of id Tech 3 CM_TraceThroughBrush
/*
 * 805bbe6 (CM_TraceBrushBounds): slab-clips the segment against the brush bounds and then
 * against each brush side plane. Returns brush index + 1 on a hit, 0 otherwise.
 */
int CM_TraceBrushBounds(struct traceWork_s *tr, const struct cbrush_s *brush)
{
    float enter = 0.0f;
    float leave = 1.0f;
    float sign = -1.0f;
    const float *corner = brush->mins;
    int pass_done = 0;
    int c;
    cbrushside_s *side;
    int remaining;
    float d_start;
    float d_end;
    float off;

    for (;;) {
        for (c = 0; c < 3; c++) {
            float f1 = (tr->start[c] - corner[c]) * sign - tr->offset[c];
            float f2 = (tr->end[c] - corner[c]) * sign - tr->offset[c];
            if (0.0f < f1) {
                if (0.0f < f2) {
                    return 0;
                }
                f1 = f1 * tr->inv[c] * sign;
                if (leave <= f1) {
                    return 0;
                }
                enter = Sweep_FloatMax(enter, f1);
            } else if (0.0f < f2) {
                f1 = f1 * tr->inv[c] * sign;
                if (f1 <= enter) {
                    return 0;
                }
                leave = Sweep_FloatMin(leave, f1);
            }
        }
        if (pass_done) {
            break;
        }
        sign = 1.0f;
        corner = brush->maxs;
        pass_done = 1;
    }

    side = brush->sides;
    remaining = brush->num_sides;
    do {
        if (remaining == 0) {
            return (int)(brush - g_08188e40_brushes) + 1;
        }
        {
            cplane_s *plane = side->plane;
            float reach = plane->dist + tr->radius + Sweep_FloatAbs(plane->normal[2] * tr->skin);
            d_start = (float)(Vec3_Dot(tr->start, plane->normal) - (long double)reach);
            d_end = (float)(Vec3_Dot(tr->end, plane->normal) - (long double)reach);
            if (0.0f < d_start) {
                if (0.0f < d_end) {
                    return 0;
                }
                off = d_start - d_end;
                if (enter * off < d_start) {
                    enter = d_start / (d_start - d_end);
                    if (leave <= enter) {
                        return 0;
                    }
                }
            } else if (0.0f < d_end) {
                off = d_start - d_end;
                if (leave * off < d_start) {
                    leave = d_start / (d_start - d_end);
                    if (leave <= enter) {
                        return 0;
                    }
                }
            }
        }
        remaining = remaining - 1;
        side = side + 1;
    } while (1);
}

// 0x0805bef2  size=974  FUN_0805bef2  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: kd-tree of 0x14 records with brush lists at leaves and relative child offsets
/*
 * 805bef2 (CM_TraceKdTree): traverses the kd-tree. Leaf nodes test their brush lists; the
 * interior nodes split the segment and recurse into the side that the segment touches.
 */
int CM_TraceKdTree(struct traceWork_s *tr, const struct cbrushnode_s *node, const float *start, const float *end)
{
    float seg[3];
    float d_start;
    float d_end;
    float ext;
    float mx;
    float mn;
    float dir;
    float abs_dir;
    float num;
    float inv_abs;
    float cut_lo;
    float cut_hi;
    float mid[3];
    int side;
    int hit;
    int k;

    Vec3_Copy(start, seg);
    for (;;) {
        if ((tr->contents & node->contents) == 0) {
            return 0;
        }
        if (node->brush_count != 0) {
            if (node->brush_count > 0) {
                int i;
                for (i = 0; ; i++) {
                    const cbrush_s *brush;
                    if (node->brush_count <= i) {
                        return 0;
                    }
                    brush = g_08188e40_brushes + node->u8.brush_list[i];
                    if ((tr->contents & brush->contents) != 0) {
                        hit = CM_TraceBrushBounds(tr, brush);
                        if (hit != 0) {
                            return hit;
                        }
                    }
                }
            }
            hit = CM_TraceKdTree(tr, node + 1, seg, end);
            if (hit != 0) {
                return hit;
            }
        }

        d_start = seg[node->axis] - node->u8.split;
        d_end = end[node->axis] - node->u8.split;
        ext = (tr->extents[node->axis] + 0.125) - node->radius;
        mx = Sweep_FloatMax(d_start, d_end);
        mn = Sweep_FloatMin(d_start, d_end);
        if (ext <= mn) {
            if (mx <= -ext) {
                return 0;
            }
            node = node + node->child[0];
        } else if (mx <= -ext) {
            node = node + node->child[1];
        } else {
            dir = d_end - d_start;
            abs_dir = Sweep_FloatAbs(dir);
            if (4.7683716e-07 < abs_dir) {
                num = Sweep_FloatSelect(dir, -d_start, d_start);
                inv_abs = 1.0f / abs_dir;
                cut_lo = (num - ext) * inv_abs;
                cut_hi = (num + ext) * inv_abs;
                side = Sweep_FloatIsNonNegative(dir);
            } else {
                side = 0;
                cut_hi = 1.0f;
                cut_lo = 0.0f;
            }
            cut_hi = Sweep_FloatMin(cut_hi, 1.0);
            for (k = 0; k < 3; k++) {
                mid[k] = seg[k] + (end[k] - seg[k]) * cut_hi;
            }
            hit = CM_TraceKdTree(tr, node + node->child[side], seg, mid);
            if (hit != 0) {
                return hit;
            }
            cut_lo = Sweep_FloatMax(cut_lo, 0.0);
            for (k = 0; k < 3; k++) {
                seg[k] = seg[k] + (end[k] - seg[k]) * cut_lo;
            }
            node = node + node->child[1 - side];
        }
    }
}

// 0x0805c372  size=206  FUN_0805c372  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: leaf masks, leaf kd test, then 0x20-byte item list; returns item index + brush count + 1
/*
 * 805c372 (CM_TraceLeafItems): leaf test. Runs the leaf bounds test (805c2c0) and, when
 * it does not hit, tests the leaf's 0x20-byte items. Returns item index + brush count + 1
 * on a hit, else 0.
 */
int CM_TraceLeafItems(struct traceWork_s *tr, const struct cLeaf_s *leaf, struct trace_s *result)
{
    int hit = 0;
    int k;

    if ((tr->contents & leaf->mask_items_a) == 0 || (hit = Sweep_TraceLeafKdTree(tr, leaf)) == 0) {
        if ((tr->contents & leaf->mask_items_b) != 0) {
            for (k = 0; k < (int)leaf->item_count; k++) {
                CM_TraceThroughAabbTreeMasked(tr, g_08188e30_aabbTrees + (leaf->first_item + k), result);
                if (result->fraction != 1.0f) {
                    return g_08188e3c_numBrushes + leaf->first_item + k + 1;
                }
            }
        }
        hit = 0;
    }
    return hit;
}

// 0x0805c2c0  size=177  FUN_0805c2c0  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: leaf bounds expanded by extents, slab test, then kd-tree at leaf+0x24
/*
 * 805c2c0: expands the leaf bounds by the trace extents, slab-tests the trace segment
 * against them and, on overlap, traverses the leaf's kd-tree.
 */
int Sweep_TraceLeafKdTree(struct traceWork_s *tr, const struct cLeaf_s *leaf)
{
    float lo[3];
    float hi[3];

    Vec3_Subtract(leaf->mins, tr->extents, lo);
    Vec3_Add(leaf->maxs, tr->extents, hi);
    if (Box_SegmentSlabTest((const cod2_box_s *)tr, lo, hi, 1.0) == 0) {
        return CM_TraceKdTree(tr, g_08188df0_brushNodes + leaf->kd_node, tr->start, tr->end);
    }
    return 0;
}

/* ---- sphere and clip tests ---- */

// 0x0805c440  size=308  FUN_0805c440  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: quadratic sphere-segment test with extended-precision sqrt; returns 1 when clear up to *t
/*
 * 805c440: tests the trace segment (direction at delta, squared length at length_sq)
 * against a sphere of radius radius + tr->radius centred on a. Returns 1 when the
 * trace is clear of the sphere up to parameter *t. The unused argument is not read.
 */
int Sweep_SphereSegmentReject(struct traceWork_s *tr, const float *a, const float *unused, const float *b, float radius, float *t)
{
    float v[3];
    float grad[5];
    float r2;
    float c;
    float bdot;
    float coef;
    float disc;
    float len;
    long double root;
    long double rhs;

    (void)unused;
    Vec3_Subtract(a, b, v);
    r2 = (radius + tr->radius) * (radius + tr->radius);
    c = (float)(Vec3_Dot(v, v) - (long double)r2);
    if (c <= 0.0) {
        return 0;
    }
    bdot = (float)Vec3_Dot(tr->delta, v);
    if (0.0 <= bdot) {
        return 1;
    }
    coef = tr->length_sq;
    disc = bdot * bdot - coef * c;
    if (disc < 0.0) {
        return 1;
    }
    len = Vec3_NormalizeCopy(v, grad);
    root = (long double)Sweep_FloatSqrt(disc);
    rhs = ((long double)0.125 * (long double)bdot) / (long double)len
        + (((long double)(-bdot) - root) / (long double)coef);
    return (*t <= (float)rhs) ? 1 : 0;
}

// 0x0805c574  size=451  FUN_0805c574  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: 2D sphere test with z-range check; decompiler local_14 alias resolved from disassembly to the v.z component
/*
 * 805c574: vertical-range variant of the sphere test. Projects onto the x-y plane, and in the
 * sphere-hit case bounds the trace height at the hit time.
 */
int Sweep_SphereRangeTest(struct traceWork_s *tr, const float *a, float extra, float radius, float *t)
{
    float v[3];
    float grad[5];
    volatile float r2;
    volatile float c;
    volatile float bdot;
    volatile float coef;
    volatile float disc;
    volatile float len;
    volatile float hit_scale;
    volatile float hit_t;
    volatile float z_range;
    long double root;

    Vec3_Subtract(tr->start, a, v);
    r2 = (radius + tr->radius) * (radius + tr->radius);
    c = (float)(Vec2_Dot(v, v) - (long double)r2);
    if (c <= 0.0) {
        volatile float lhs = (tr->extents[2] - tr->radius) + extra;
        return ((long double)lhs < (long double)Sweep_FloatAbs(v[2])) ? 1 : 0;
    }
    bdot = (float)Vec2_Dot(tr->delta, v);
    if (0.0 <= bdot) {
        return 1;
    }
    coef = tr->length_sq;
    disc = bdot * bdot - coef * c;
    if (disc < 0.0) {
        return 1;
    }
    v[2] = 0.0f;   /* mov [ebp-0x10],0: the stack slot of v[2] (decompiler local_14) */
    len = Vec3_NormalizeCopy(v, grad);
    hit_scale = (float)((long double)bdot * 0.125L / (long double)len);   /* x87 extended division, one rounding */
    root = (long double)Sweep_FloatSqrt(disc);
    hit_t = (float)(((long double)(-bdot) - root) / (long double)coef + (long double)hit_scale);
    if (*t <= hit_t) {
        return 1;
    }
    z_range = fabsf(((hit_t - hit_scale) * tr->delta[2] + tr->start[2]) - a[2]);
    {
        volatile float lhs2 = (tr->extents[2] - tr->radius) + extra;   /* [ebp-0x4c], rounded to float */
        return (z_range > lhs2) ? 1 : 0;
    }
}

// 0x0805c738  size=1193  FUN_0805c738  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: world-box reject, two vertical sphere ends, vertical range test; returns 0 miss, -1 hit
/*
 * 805c738 (Sweep_TraceClipToBounds): clips the trace against the world box stored at
 * tr->world_bounds, then tests the two vertical sphere ends (805c440) and the vertical
 * range (805c574). Returns 0 when the trace misses the box, -1 on a hit, else 0.
 */
int Sweep_TraceClipToBounds(struct traceWork_s *tr, struct trace_s *result)
{
    const float *world = tr->world_bounds;
    float s_hi[3];
    float s_lo[3];
    float e_hi[3];
    float e_lo[3];
    float center[3];
    float ext[6];
    float c_hi[3];
    float c_lo[3];
    float half;
    float dz;
    float skin = tr->skin;
    int k;

    if (world[3] + 1.0 < tr->box2_mins[0] || world[4] + 1.0 < tr->box2_mins[1] ||
        world[5] + 1.0 < tr->box2_mins[2] ||
        tr->box2_maxs[0] < world[0] - 1.0 || tr->box2_maxs[1] < world[1] - 1.0 ||
        tr->box2_maxs[2] < world[2] - 1.0) {
        return 0;
    }

    Vec3_Copy(tr->start, s_hi);
    s_hi[2] = s_hi[2] + skin;
    Vec3_Copy(tr->start, s_lo);
    s_lo[2] = s_lo[2] - skin;
    Vec3_Copy(tr->end, e_hi);
    e_hi[2] = e_hi[2] + skin;
    Vec3_Copy(tr->end, e_lo);
    e_lo[2] = e_lo[2] - skin;

    for (k = 0; k < 3; k++) {
        center[k] = (world[k] + world[k + 3]) * 0.5;
        ext[k] = world[k] - center[k];
        ext[k + 3] = world[k + 3] - center[k];
    }
    half = (ext[5] < ext[3]) ? ext[5] : ext[3];
    dz = ext[5] - half;
    Vec3_Copy(center, c_hi);
    c_hi[2] = c_hi[2] + dz;
    Vec3_Copy(center, c_lo);
    c_lo[2] = c_lo[2] - dz;

    if (c_hi[2] < s_lo[2]) {
        if (Sweep_SphereSegmentReject(tr, s_lo, e_lo, c_hi, half, &result->fraction) == 0) {
            return -1;
        }
        if (0.0 <= tr->delta[2]) {
            return 0;
        }
    } else if (s_hi[2] < c_lo[2]) {
        if (Sweep_SphereSegmentReject(tr, s_hi, e_hi, c_lo, half, &result->fraction) == 0) {
            return -1;
        }
        if (tr->delta[2] <= 0.0) {
            return 0;
        }
    }

    if (Sweep_SphereRangeTest(tr, center, dz, half, &result->fraction) == 0) {
        return -1;
    }
    if (c_hi[2] < e_lo[2]) {
        if (s_lo[2] <= c_hi[2] &&
            Sweep_SphereSegmentReject(tr, s_lo, e_lo, c_hi, half, &result->fraction) == 0) {
            return -1;
        }
    } else if (e_hi[2] < c_lo[2] && c_lo[2] <= s_hi[2]) {
        if (Sweep_SphereSegmentReject(tr, s_hi, e_hi, c_lo, half, &result->fraction) == 0) {
            return -1;
        }
    }
    return 0;
}

// 0x0805ac12  size=1093  FUN_0805ac12  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: no strings; clips the swept box against the two world-box halves via 0x805a768/0x805a95e
/*
 * 805ac12 (Sweep_ClipBoxHalves): clips the trace against the two halves of the world box
 * that its vertical extent touches, using the bounded-axis helpers 805a768 and 805a95e.
 */
void Sweep_ClipBoxHalves(struct traceWork_s *tr, struct trace_s *result)
{
    const float *world = tr->world_bounds;
    float s_hi[3];
    float s_lo[3];
    float e_hi[3];
    float e_lo[3];
    float center[3];
    float ext[6];
    float c_hi[3];
    float c_lo[3];
    float half;
    float dz;
    float skin = tr->skin;
    int k;
    int r;

    if (!(tr->box2_mins[0] <= world[3] + 1.0 && tr->box2_mins[1] <= world[4] + 1.0 &&
          tr->box2_mins[2] <= world[5] + 1.0 &&
          world[0] - 1.0 <= tr->box2_maxs[0] && world[1] - 1.0 <= tr->box2_maxs[1] &&
          world[2] - 1.0 <= tr->box2_maxs[2])) {
        return;
    }

    Vec3_Copy(tr->start, s_hi);
    s_hi[2] = s_hi[2] + skin;
    Vec3_Copy(tr->start, s_lo);
    s_lo[2] = s_lo[2] - skin;
    Vec3_Copy(tr->end, e_hi);
    e_hi[2] = e_hi[2] + skin;
    Vec3_Copy(tr->end, e_lo);
    e_lo[2] = e_lo[2] - skin;

    for (k = 0; k < 3; k++) {
        center[k] = (world[k] + world[k + 3]) * 0.5;
        ext[k] = world[k] - center[k];
        ext[k + 3] = world[k + 3] - center[k];
    }
    half = (ext[5] < ext[3]) ? ext[5] : ext[3];
    dz = ext[5] - half;
    Vec3_Copy(center, c_hi);
    c_hi[2] = c_hi[2] + dz;
    Vec3_Copy(center, c_lo);
    c_lo[2] = c_lo[2] - dz;

    if (c_hi[2] < s_lo[2]) {
        r = cm_trace_endpoint_sphere(tr, s_lo, e_lo, c_hi, half, result);
        if (r == 0) {
            return;
        }
        if (0.0 <= tr->delta[2]) {
            return;
        }
    } else if (s_hi[2] < c_lo[2]) {
        r = cm_trace_endpoint_sphere(tr, s_hi, e_hi, c_lo, half, result);
        if (r == 0) {
            return;
        }
        if (tr->delta[2] <= 0.0) {
            return;
        }
    }

    r = cm_trace_vertical_capsule(tr, center, dz, half, result);
    if (r != 0) {
        if (c_hi[2] < e_lo[2]) {
            if (s_lo[2] <= c_hi[2]) {
                cm_trace_endpoint_sphere(tr, s_lo, e_lo, c_hi, half, result);
            }
        } else if (e_hi[2] < c_lo[2] && c_lo[2] <= s_hi[2]) {
            cm_trace_endpoint_sphere(tr, s_hi, e_hi, c_lo, half, result);
        }
    }
}

/* 805b39c: sets the cull flag (tr->flags_88) from the box2 volume versus the extents volume. */
// 0x0805b39c  size=120  FUN_0805b39c  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: writes tr+0x88 from box2 volume vs extents*length volume
void Sweep_UpdateCullFlag(struct traceWork_s *tr)
{
    float size[3];
    volatile float lhs;     /* [ebp-0x1c] */
    volatile float prod;    /* [ebp-0x20]: the binary rounds the extents product to float before scaling */
    volatile float rhs;     /* [ebp-0x24] */

    Vec3_Subtract(tr->box2_maxs, tr->box2_mins, size);
    lhs = size[0] * size[1] * size[2];
    prod = tr->extents[0] * tr->extents[1] * tr->extents[2];
    rhs = prod * 16.0f * tr->length;
    tr->flags_88 = (rhs > lhs) ? 1u : 0u;   /* fucompp; seta */
}

/*
 * Shared segment setup of 805b414 and 805cefe (inlined twice in the binary; identical instruction
 * sequences up to the call of Sweep_UpdateCullFlag): contents mask first, then per axis the box centre,
 * extents, centred start/end, midpoint, delta, half delta and its absolute value; the reciprocal
 * extents, squared length and length; radius = (extents.x > extents.z ? extents.z : extents.x),
 * skin = extents.z - radius; the swept xy bounds and the z bounds widened by skin + radius.
 * Start/end and the caller's arrays are not modified.
 */
static void Sweep_BuildSegment(traceWork_s *tr, const float *start, const float *end, const float *mins, const float *maxs, uint32 mask)
{
    float center[3];
    float radius;
    int k;

    tr->contents = mask;
    for (k = 0; k < 3; k++) {
        center[k] = (mins[k] + maxs[k]) * 0.5f;
        tr->extents[k] = maxs[k] - center[k];
        tr->start[k] = start[k] + center[k];
        tr->end[k] = end[k] + center[k];
        tr->mid[k] = (tr->start[k] + tr->end[k]) * 0.5f;
        tr->delta[k] = tr->end[k] - tr->start[k];
        tr->half[k] = tr->delta[k] * 0.5f;
        tr->abs_half[k] = Sweep_FloatAbs(tr->half[k]);
    }
    Box_ComputeInverseExtents((float *)tr);
    tr->length_sq = (float)Vec3_LengthSquared(tr->delta);
    tr->length = Sweep_FloatSqrt(tr->length_sq);

    if (tr->extents[0] > tr->extents[2]) {
        radius = tr->extents[2];
    } else {
        radius = tr->extents[0];
    }
    tr->radius = radius;
    tr->skin = tr->extents[2] - tr->radius;
    for (k = 0; k < 2; k++) {
        if (tr->end[k] > tr->start[k]) {
            tr->box2_mins[k] = tr->start[k] - tr->radius;
            tr->box2_maxs[k] = tr->end[k] + tr->radius;
        } else {
            tr->box2_mins[k] = tr->end[k] - tr->radius;
            tr->box2_maxs[k] = tr->start[k] + tr->radius;
        }
    }
    if (tr->end[2] > tr->start[2]) {
        tr->box2_mins[2] = tr->start[2] - tr->skin - tr->radius;
        tr->box2_maxs[2] = tr->end[2] + tr->skin + tr->radius;
    } else {
        tr->box2_mins[2] = tr->end[2] - tr->skin - tr->radius;
        tr->box2_maxs[2] = tr->start[2] + tr->skin + tr->radius;
    }
    Sweep_UpdateCullFlag(tr);
}

/* Capsule parameters of the moving case (both callers): point flag and the per-axis offsets. */
static void Sweep_SetCapsule(traceWork_s *tr)
{
    tr->is_point = (tr->extents[0] + tr->extents[1] + tr->extents[2] == 0.0f);
    tr->offset[0] = tr->radius;
    tr->offset[1] = tr->radius;
    tr->offset[2] = tr->radius + tr->skin;
}

// 0x0805b414  size=1152  FUN_0805b414  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: dispatch on type 0 / 0x3ff / other, mirrors 805cefe
/*
 * 805b414 (CM_BoxTraceBody): traces the box mins..maxs swept from start to end against the world
 * (type 0), the temporary box model (type 0x3ff; its record pointer is the +0xb0 word that
 * cm_scratch_acquire copies into the trace work) or the model's leaf (other types).
 * Re-translated from the disassembly (boot-parity lane): the scratch state (+0xa0..+0xb7: check
 * counter, stamp arrays, box model, bounds) is acquired into the trace work itself.
 */
void CM_BoxTraceBody(struct trace_s *result, const float *start, const float *end, const float *mins, const float *maxs, int type, uint32 mask)
{
    traceWork_s tr;
    unsigned char *info;
    const unsigned char *box_model;
    float start4[4];
    float end4[4];

    info = (unsigned char *)cm_model_get(type);
    Sweep_BuildSegment(&tr, start, end, mins, maxs, mask);
    cm_scratch_acquire((int *)tr.unk_a0);
    box_model = *(const unsigned char *const *)(tr.unk_a0 + 0x10); /* trace work +0xb0 */

    if (Vec3_Equal(start, end) != 0) {
        tr.is_point = 0;
        if (type == 0) {
            cm_trace_leaves(&tr, result);
        } else if (type == 0x3ff) {
            if ((*(const uint32 *)(box_model + 0x0c) & tr.contents) != 0) {
                cm_trace_aabb_bounds(&tr, result);
            }
        } else if (((const byte *)result)[0x22] == 0) {
            cm_leaf_trace(&tr, (cLeaf_s *)(info + 0x1c), result);
        }
        return;
    }
    Sweep_SetCapsule(&tr);
    if (type == 0) {
        Vec3_Copy(tr.start, start4);
        start4[3] = 0.0f;
        Vec3_Copy(tr.end, end4);
        end4[3] = result->fraction;
        CM_RecursiveHullCheck(&tr, 0, start4, end4, result);
    } else if (type == 0x3ff) {
        if ((*(const uint32 *)(box_model + 0x0c) & tr.contents) != 0) {
            Sweep_ClipBoxHalves(&tr, result);
        }
    } else {
        cm_leaf_surface_trace(&tr, (cLeaf_s *)(info + 0x1c), result);
    }
}

// 0x0805b894  size=98  FUN_0805b894  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: memset 0x24 then fraction = 1.0
void CM_BoxTrace(struct trace_s *result, const float *start, const float *end, const float *mins, const float *maxs, int type, uint32 mask)
{
    memset(result, 0, 0x24);
    result->fraction = 1.0f;
    CM_BoxTraceBody(result, start, end, mins, maxs, type, mask);
}

// 0x0805b8f6  size=639  FUN_0805b8f6  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: offset subtraction, optional rotation of segment, normal rotated back when fraction decreases
/*
 * 805b8f6 (Sweep_TraceBoxWithOffset): centres the box, subtracts offset from both segment
 * ends, rotates the segment by angles when they are nonzero, traces it, and rotates the
 * result normal back when the fraction decreased.
 */
void Sweep_TraceBoxWithOffset(struct trace_s *result, const float *start, const float *end, const float *mins, const float *maxs, int type, uint32 mask, const float *offset, float *angles)
{
    float center[3];
    float box[6];
    float p0[4];
    float p1[4];
    float mat[12];
    float mat_inv[12];
    int rotated;
    float frac_before;
    int k;

    for (k = 0; k < 3; k++) {
        center[k] = (mins[k] + maxs[k]) * 0.5;
        box[k] = mins[k] - center[k];
        box[k + 3] = maxs[k] - center[k];
        p0[k] = start[k] + center[k];
        p1[k] = end[k] + center[k];
    }
    Vec3_Subtract(p0, offset, p0);
    Vec3_Subtract(p1, offset, p1);

    rotated = (angles[0] != 0.0 || angles[1] != 0.0 || angles[2] != 0.0) ? 1 : 0;
    if (rotated) {
        Angles_ToVectorsNegRight_080594ba(angles, mat);
        Mat3_TransformVecInPlace_080593e2(p0, mat);
        Mat3_TransformVecInPlace_080593e2(p1, mat);
    }
    frac_before = result->fraction;
    CM_BoxTraceBody(result, p0, p1, box, box + 3, type, mask);
    if (rotated && result->fraction < frac_before) {
        Mat3_TransposeBits_0805944e(mat, mat_inv);
        Mat3_TransformVecInPlace_080593e2(result->normal, mat_inv);
    }
}

/* 805bb76: initialises the result and dispatches Sweep_TraceBoxWithOffset. */
// 0x0805bb76  size=112  FUN_0805bb76  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: memset 0x24 then fraction = 1.0 then 805b8f6
void Sweep_TraceBoxWithOffsetInit(struct trace_s *result, const float *start, const float *end, const float *mins, const float *maxs, int type, uint32 mask, const float *offset, float *angles)
{
    memset(result, 0, 0x24);
    result->fraction = 1.0f;
    Sweep_TraceBoxWithOffset(result, start, end, mins, maxs, type, mask, offset, angles);
}

// 0x0805cbe2  size=795  FUN_0805cbe2  status=translated  naming=inferred
// naming evidence: inferred from structure and callers (see symbols/chunk_005.json): vec3 variant of 805b058; leaves go to 805c372 and the first nonzero result is returned
/*
 * 805cbe2 (CM_RecursiveHullCheckEntities): hull walk over the BSP for vec3 segments.
 * Leaves are handed to 805c372; the walk returns the first nonzero leaf result.
 */
int CM_RecursiveHullCheckEntities(struct traceWork_s *tr, int node_index, const float *start3, const float *end3, struct trace_s *result)
{
    float seg[3];
    cnode_s *node = 0;
    cplane_s *plane = 0;
    float d_start = 0.0f;
    float d_end = 0.0f;
    float extent = 0.0f;
    float t_min;
    float t_max;
    float abs_delta;
    float num;
    float inv_abs;
    float cut_lo;
    float cut_hi;
    float scale;
    float mid[3];
    int side = 0;
    int k;
    int hit;

    Vec3_Copy(start3, seg);
    for (;;) {
        for (;;) {
            for (;;) {
                if (node_index < 0) {
                    return CM_TraceLeafItems(tr, g_08188de8_leafs + (-1 - node_index), result);
                }
                node = &g_08188de0_nodes[node_index];
                plane = node->plane;
                if (plane->type < 3) {
                    d_start = seg[plane->type] - plane->dist;
                    d_end = end3[plane->type] - plane->dist;
                    extent = tr->extents[plane->type] + 0.125;
                } else {
                    d_start = (float)(Vec3_Dot(seg, plane->normal) - (long double)plane->dist);
                    d_end = (float)(Vec3_Dot(end3, plane->normal) - (long double)plane->dist);
                    if (tr->is_point == 0) {
                        extent = 2048.0;
                    } else {
                        extent = 0.125;
                    }
                }
                t_min = Sweep_FloatMin(d_start, d_end);
                if ((long double)t_min < (long double)extent) {
                    break;
                }
                node_index = node->child[0];
            }
            t_max = Sweep_FloatMax(d_start, d_end);
            if (-(long double)extent < (long double)t_max) {
                break;
            }
            node_index = node->child[1];
        }

        abs_delta = Sweep_FloatAbs(d_end - d_start);
        if (4.7683716e-07 < abs_delta) {
            num = Sweep_FloatSelect(d_end - d_start, -d_start, d_start);
            inv_abs = 1.0f / abs_delta;
            cut_lo = (num - extent) * inv_abs;
            cut_hi = (num + extent) * inv_abs;
            side = Sweep_FloatIsNonNegative(d_end - d_start);
        } else {
            side = 0;
            cut_hi = 1.0f;
            cut_lo = 0.0f;
        }
        scale = Sweep_FloatMin(cut_hi, 1.0);
        for (k = 0; k < 3; k++) {
            mid[k] = seg[k] + (end3[k] - seg[k]) * scale;
        }
        hit = CM_RecursiveHullCheckEntities(tr, node->child[side], seg, mid, result);
        if (hit != 0) {
            return hit;
        }
        cut_lo = Sweep_FloatMax(cut_lo, 0.0);
        for (k = 0; k < 3; k++) {
            seg[k] = seg[k] + (end3[k] - seg[k]) * cut_lo;
        }
        node_index = node->child[1 - side];
    }
}

// 0x0805cefe  size=1065  FUN_0805cefe  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: type 0: one brush then BSP walk; 0x3ff: box2 clip; other: leaf test
/*
 * 805cefe (Sweep_TraceEntityDispatch): entity-side trace. The type-0 branch first tests
 * one brush (brush_hint - 1) and otherwise walks the BSP; type 0x3ff uses the box2 clip,
 * other types use the leaf test. Returns the hit value.
 */
int Sweep_TraceEntityDispatch(int brush_hint, const float *start, const float *end, const float *mins, const float *maxs, int type, uint32 mask)
{
    traceWork_s tr;
    trace_s res;
    unsigned char *info;
    const unsigned char *box_model;
    int hit;

    info = (unsigned char *)cm_model_get(type);
    res.fraction = 1.0f;                   /* only these three fields of the local trace are initialised */
    ((byte *)&res)[0x23] = 0;
    ((byte *)&res)[0x22] = 0;
    Sweep_BuildSegment(&tr, start, end, mins, maxs, mask);
    Sweep_SetCapsule(&tr);
    cm_scratch_acquire((int *)tr.unk_a0);
    box_model = *(const unsigned char *const *)(tr.unk_a0 + 0x10); /* trace work +0xb0 */

    if (type == 0) {
        hit = 0;
        if (brush_hint > 0) {
            brush_hint--;
            if (brush_hint < (int)(uint16)g_08188e3c_numBrushes) {   /* movzx WORD PTR ds:0x8188e3c */
                hit = CM_TraceBrushBounds(&tr, g_08188e40_brushes + brush_hint);
            }
        }
        if (hit == 0) {
            hit = CM_RecursiveHullCheckEntities(&tr, 0, tr.start, tr.end, &res);
        }
    } else if (type == 0x3ff) {
        if ((*(const uint32 *)(box_model + 0x0c) & tr.contents) == 0) {
            hit = 0;
        } else {
            hit = Sweep_TraceClipToBounds(&tr, &res);
        }
    } else {
        hit = CM_TraceLeafItems(&tr, (const cLeaf_s *)(info + 0x1c), &res);
    }
    return hit;
}

/* 805d328: centres and rotates the entity-side segment, then dispatches Sweep_TraceEntityDispatch. */
// 0x0805d328  size=554  FUN_0805d328  status=translated  naming=unknown
// naming evidence: descriptive name from behaviour only (no strings); see symbols/chunk_005.json: offset and optional rotation, then 805cefe
int Sweep_TraceEntityWithOffset(int brush_hint, const float *start, const float *end, const float *mins, const float *maxs, int type, uint32 mask, const float *offset, float *angles)
{
    float center[3];
    float box[6];
    float p0[4];
    float p1[4];
    float mat[12];
    int rotated;
    int k;

    for (k = 0; k < 3; k++) {
        center[k] = (mins[k] + maxs[k]) * 0.5;
        box[k] = mins[k] - center[k];
        box[k + 3] = maxs[k] - center[k];
        p0[k] = start[k] + center[k];
        p1[k] = end[k] + center[k];
    }
    Vec3_Subtract(p0, offset, p0);
    Vec3_Subtract(p1, offset, p1);

    rotated = (angles[0] != 0.0 || angles[1] != 0.0 || angles[2] != 0.0) ? 1 : 0;
    if (rotated) {
        Angles_ToVectorsNegRight_080594ba(angles, mat);
        Mat3_TransformVecInPlace_080593e2(p0, mat);
        Mat3_TransformVecInPlace_080593e2(p1, mat);
    }
    return Sweep_TraceEntityDispatch(brush_hint, p0, p1, box, box + 3, type, mask);   /* tail value */
}
