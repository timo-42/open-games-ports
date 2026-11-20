/*
 * chunk_004: collision-model (CM) trace, BSP/plane-tree queries, static models and
 * entity-string model loading for cod2_lnxded 1.3 (ELF32 i386, GNU C++98).
 *
 * Provenance per function: address, size, Ghidra label, status and naming evidence.
 * Float arithmetic follows the float-typed locals of the decompiled code. Where the
 * original keeps an intermediate in x87 extended precision, the reconstruction
 * uses the float or double type that the pseudocode shows; the difference is an
 * acknowledged limitation (see chunk notes).
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_004_types.h"

#include <strings.h>

/* ---- Local helpers (bit-exact access where the binary copies raw words) ---- */

/* Shared prototype takes word pointers (chunk 003 VectorCopy_080561a6 copies 3 raw words). */
static inline void cm4_vcopy(const float *src, float *dst)
{
    VectorCopy_080561a6((const uint32 *)src, (uint32 *)dst);
}

/* neg_eps = -eps: the binary flips the sign bit of the stored word (xor eax,0x80000000). */
static inline void cm4_store_negated(volatile float *dst, float x)
{
    union { uint32 u; float f; } v;
    v.f = x;
    v.u ^= 0x80000000u;
    *dst = v.f;
}

/* Raw word copy (mov eax,[slot]; mov [dst],eax): never passes through the x87 stack, so NaN payloads stay intact. */
static inline void cm4_copy_bits(float *dst, const volatile float *src)
{
    *(volatile uint32 *)dst = *(const volatile uint32 *)src;
}

/* Vec3_Set_08057654 called with raw argument words (the binary pushes the float words with integer moves, so an
 * sNaN in the wall normal survives; passing floats would run them through fld/fstp and quiet them). */
typedef void (*cm4_set_bits_fn)(float *, uint32, uint32, uint32);
static inline void cm4_vec3_set_bits(float *dst, const float *x, const float *y, uint32 zbits)
{
    ((cm4_set_bits_fn)Vec3_Set_08057654)(dst, *(const uint32 *)x, *(const uint32 *)y, zbits);
}

static inline float cm4_bits_to_float(uint32 bits)
{
    union { uint32 u; float f; } v;
    v.u = bits;
    return v.f;
}

static inline uint32 cm4_float_to_bits(float f)
{
    union { uint32 u; float f; } v;
    v.f = f;
    return v.u;
}

/* Decompiler "int" that holds an address. */
static inline float *cm4_fptr(int addr)
{
    return (float *)(unsigned long)(uint32)addr;
}

/* Non-zero or NaN, as the binary's NAN() tests. */
static inline int cm4_nonzero_or_nan(float x)
{
    return (x != 0.0f) || (x != x);
}

/* Forward declarations for chunk-internal callees. */

// 0x0805631a  size=374  FUN_0805631a  status=translated  naming=unknown
// Evidence: no strings; wedge plane tests with a 0.125 offset and a
// 0.001/1.001 tolerance band. Name is a descriptive label only.
void cm_wedge_edge_sweep(struct traceWork_s *trace, struct CollisionTriangle_s *wedge, struct trace_s *hit)
{
    volatile float end_dist;
    volatile float start_dist;
    volatile float enter_t;
    volatile float point_t;
    volatile float side1;
    volatile float side2;
    vec3_t point;

    end_dist = DotProduct(trace->end, wedge->planes[0].normal) - wedge->planes[0].dist;
    /* binary tests: fucompp + jae/ja, so unordered (NaN) falls through into the body */
    if (!(end_dist >= 0.0f)) {
        start_dist = DotProduct(trace->start, wedge->planes[0].normal) - wedge->planes[0].dist;
        if (!(0.0f >= start_dist)) {
            enter_t = (start_dist - 0.125f) / (start_dist - end_dist);
            enter_t = Float_Max_080575c0(enter_t, 0.0f);
            if (!(enter_t >= hit->fraction)) {
                point_t = start_dist / (start_dist - end_dist);
                VectorMA(trace->start, point_t, trace->delta, point);
                side1 = DotProduct(point, wedge->planes[1].normal) - wedge->planes[1].dist;
                if (!(-0.001f > side1) && !(side1 > 1.001f)) {
                    side2 = DotProduct(point, wedge->planes[2].normal) - wedge->planes[2].dist;
                    if (!(-0.001f > side2) && !(side1 + side2 > 1.001f)) {
                        hit->fraction = enter_t;
                        cm4_vcopy(wedge->planes[0].normal, hit->normal);
                    }
                }
            }
        }
    }
}

// 0x08056490  size=2998  FUN_08056490  status=translated  naming=unknown
// Sphere (radius + 0.125 epsilon) sweep against a wedge: plane, vertex and
// edge contacts. Name is a descriptive label only.
void cm_wedge_sphere_sweep(struct traceWork_s *trace, struct CollisionTriangle_s *wedge, float offset, struct trace_s *hit)
{
    /*
     * Branch conditions follow the binary's x87 compares (fucompp; sahf; ja/jae): an unordered (NaN)
     * operand makes ja/jae fall through, so every test is written as the exact jump condition (or its
     * negation). Float locals are volatile because the binary spills each one to a float stack slot.
     * Constants come from .rodata as floats (0.125f, 1e-4f, 0.5f, 0.7f).
     */
    vec3_t end_pos;     /* local_bc */
    vec3_t start_pos;   /* local_ac */
    vec3_t hit_pos;     /* local_3c */
    vec3_t rel;         /* local_5c */
    volatile float eps_r;        /* [ebp-0x14] */
    volatile float end_dist;     /* [ebp-0x1c] */
    volatile float start_dist;   /* [ebp-0x18] */
    volatile float neg_eps_r;    /* [ebp-0x10] */
    volatile float diff;         /* [ebp-0xcc] */
    volatile float t;            /* [ebp-0x20] */
    volatile float side1;        /* [ebp-0x3c] */
    volatile float side2;        /* [ebp-0x40] */
    volatile float far_t;        /* [ebp-0xbc] */
    volatile float two_offset;   /* [ebp-0xc0] */
    volatile float raw1;         /* [ebp-0xc4] */
    volatile float raw2;         /* [ebp-0xc8] */
    int flags;          /* local_48 */
    int k;

    cm4_vcopy(trace->end, end_pos);
    end_pos[2] = end_pos[2] - offset;
    eps_r = trace->radius + 0.125f;
    end_dist = DotProduct(end_pos, wedge->planes[0].normal) - wedge->planes[0].dist;
    if (!(end_dist >= eps_r)) {
        cm4_vcopy(trace->start, start_pos);
        start_pos[2] = start_pos[2] - offset;
        start_dist = DotProduct(start_pos, wedge->planes[0].normal) - wedge->planes[0].dist;
        diff = start_dist - end_dist;
        if (!(0.0001f >= diff)) {
            cm4_store_negated(&neg_eps_r, eps_r);
            if (neg_eps_r >= start_dist) {
                two_offset = offset + offset;
                far_t = two_offset * wedge->planes[0].normal[2] + start_dist;
                if (!(neg_eps_r >= far_t)) {
                    t = (neg_eps_r - start_dist) / wedge->planes[0].normal[2];
                    raw1 = DotProduct(start_pos, wedge->planes[1].normal) - wedge->planes[1].dist;
                    raw2 = DotProduct(start_pos, wedge->planes[2].normal) - wedge->planes[2].dist;
                    side1 = t * wedge->planes[1].normal[2] + raw1;
                    side2 = t * wedge->planes[2].normal[2] + raw2;
                    if (!((side1 >= 0.0f) && (side2 >= 0.0f) && (1.0f >= side1 + side2))) {
                        if (eps_r > far_t) {
                            t = offset + offset;
                        } else {
                            t = (eps_r - start_dist) / wedge->planes[0].normal[2];
                        }
                        raw1 = DotProduct(start_pos, wedge->planes[1].normal) - wedge->planes[1].dist;
                        raw2 = DotProduct(start_pos, wedge->planes[2].normal) - wedge->planes[2].dist;
                        side1 = t * wedge->planes[1].normal[2] + raw1;
                        side2 = t * wedge->planes[2].normal[2] + raw2;
                        if (!(0.0f > side1) && !(0.0f > side2) && !(side1 + side2 > 1.0f)) {
                            cm4_vcopy(wedge->planes[0].normal, hit->normal);
                            hit->fraction = 0.0f;
                            hit->startsolid = 1;
                        }
                    } else {
                        cm4_vcopy(wedge->planes[0].normal, hit->normal);
                        hit->fraction = 0.0f;
                        hit->startsolid = 1;
                    }
                }
            } else {
                if (0.0f >= start_dist - eps_r) {
                    t = 0.0f;
                    cm4_vcopy(start_pos, hit_pos);
                } else {
                    t = (start_dist - eps_r) / diff; /* fdiv dword [ebp-0xcc]: the stored float difference (0x08056782) */
                    if (t > hit->fraction) {
                        return;
                    }
                    VectorMA(start_pos, t, trace->delta, hit_pos);
                }
                side1 = DotProduct(hit_pos, wedge->planes[1].normal) - wedge->planes[1].dist;
                side2 = DotProduct(hit_pos, wedge->planes[2].normal) - wedge->planes[2].dist;
                flags = (side1 + side2 > 1.0f) ? 1 : 0;
                if (0.0f > side1) {
                    flags = flags | 2;
                }
                if (0.0f > side2) {
                    flags = flags | 4;
                }
                if (flags == 0) {
                    cm4_vcopy(wedge->planes[0].normal, hit->normal);
                    cm4_copy_bits(&hit->fraction, &t);
                    if (trace->radius > start_dist) {
                        hit->startsolid = 1;
                    }
                } else {
                    for (k = 0; k <= 2; k = k + 1) {
                        if (((flags >> (k & 0x1f)) & 1) == 0) {
                            int vi = wedge->vertex_index[k];
                            if (-1 < vi && trace->vertex_stamps[vi] != trace->visit_stamp) {
                                volatile float dv;
                                volatile float rel_len_sq;
                                volatile float disc_base;
                                volatile float disc;
                                volatile float a_coef;
                                trace->vertex_stamps[vi] = trace->visit_stamp;
                                VectorSubtract(start_pos, g_08188e08_verts[vi], rel);
                                dv = DotProduct(trace->delta, rel);
                                if (!(dv >= 0.0f)) {
                                    rel_len_sq = DotProduct(rel, rel);
                                    disc_base = rel_len_sq - eps_r * eps_r;
                                    if (0.0f >= disc_base) {
                                        volatile float inv_len;
                                        inv_len = (float)(1.0 / Float_Sqrt_080575a0(rel_len_sq));
                                        Vec3_Scale_08057678(rel, inv_len, hit->normal);
                                        if (wedge->planes[0].normal[2] >= 0.7f && hit->normal[2] >= 0.0f &&
                                            0.7f > hit->normal[2] && start_pos[2] > end_pos[2]) {
                                            cm4_vcopy(wedge->planes[0].normal, hit->normal);
                                        }
                                        hit->fraction = 0.0f;
                                        if (trace->radius * trace->radius > rel_len_sq) {
                                            hit->startsolid = 1;
                                        }
                                        return;
                                    }
                                    a_coef = trace->delta_len_sq;
                                    disc = dv * dv - a_coef * disc_base;
                                    if (!(0.0f > disc)) {
                                        t = (-Float_Sqrt_080575a0(disc) - dv) / a_coef;
                                        if (hit->fraction > t) {
                                            VectorMA(rel, t, trace->delta, hit->normal);
                                            Vec3_Scale_08057678(hit->normal, (float)(1.0 / eps_r), hit->normal);
                                            if (wedge->planes[0].normal[2] >= 0.7f && hit->normal[2] >= 0.0f &&
                                                0.7f > hit->normal[2] && start_pos[2] > end_pos[2]) {
                                                cm4_vcopy(wedge->planes[0].normal, hit->normal);
                                            }
                                            cm4_copy_bits(&hit->fraction, &t);
                                        }
                                    }
                                }
                            }
                        } else {
                            int ei = wedge->edge_index[k];
                            if (-1 < ei && trace->edge_stamps[ei] != trace->visit_stamp) {
                                CollisionEdge_s *edge;
                                float v7[2];
                                float v6[2];
                                volatile float dv;
                                volatile float v7_len_sq;
                                volatile float disc_base;
                                volatile float disc;
                                volatile float v6_len_sq;
                                volatile float n_dot;
                                trace->edge_stamps[ei] = trace->visit_stamp;
                                edge = &g_08188e10_edges[ei];
                                VectorSubtract(start_pos, (const float *)edge, rel);
                                v7[0] = DotProduct(rel, edge->p0);
                                v7[1] = DotProduct(rel, edge->p1);
                                v6[0] = DotProduct(trace->delta, edge->p0);
                                v6[1] = DotProduct(trace->delta, edge->p1);
                                dv = Vec2_Dot_08057610(v6, v7);
                                if (!(dv >= 0.0f)) {
                                    v7_len_sq = Vec2_Dot_08057610(v7, v7);
                                    disc_base = v7_len_sq - eps_r * eps_r;
                                    if (0.0f >= disc_base) {
                                        n_dot = DotProduct(rel, edge->normal);
                                        if (!(Float_Abs_0805618c(n_dot - 0.5f) > 0.5f)) {
                                            Vec3_Scale_08057678(edge->p0, v7[0], hit->normal);
                                            VectorMA(hit->normal, v7[1], edge->p1, hit->normal);
                                            Vec3_Normalize(hit->normal);
                                            if (wedge->planes[0].normal[2] >= 0.7f && hit->normal[2] >= 0.0f &&
                                                0.7f > hit->normal[2] && start_pos[2] > end_pos[2]) {
                                                cm4_vcopy(wedge->planes[0].normal, hit->normal);
                                            }
                                            hit->fraction = 0.0f;
                                            if (trace->radius * trace->radius > v7_len_sq) {
                                                hit->startsolid = 1;
                                            }
                                            return;
                                        }
                                    } else {
                                        v6_len_sq = Vec2_LengthSq_08057632(v6);
                                        disc = dv * dv - v6_len_sq * disc_base;
                                        if (!(0.0f >= disc)) {
                                            t = (-Float_Sqrt_080575a0(disc) - dv) / v6_len_sq;
                                            if (hit->fraction > t) {
                                                VectorMA(rel, t, trace->delta, rel);
                                                n_dot = DotProduct(rel, edge->normal);
                                                if (!(Float_Abs_0805618c(n_dot - 0.5f) > 0.5f)) {
                                                    volatile float s3c = (t * v6[0] + v7[0]) / eps_r;
                                                    volatile float s38 = (t * v6[1] + v7[1]) / eps_r;
                                                    Vec3_Scale_08057678(edge->p0, s3c, hit->normal);
                                                    VectorMA(hit->normal, s38, edge->p1, hit->normal);
                                                    if (wedge->planes[0].normal[2] >= 0.7f && hit->normal[2] >= 0.0f &&
                                                        0.7f > hit->normal[2] && start_pos[2] > end_pos[2]) {
                                                        cm4_vcopy(wedge->planes[0].normal, hit->normal);
                                                    }
                                                    cm4_copy_bits(&hit->fraction, &t);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// 0x08057046  size=1369  FUN_08057046  status=translated  naming=unknown
// Circle (radius + 0.125) sweep against a 2D wall with a z slope.
void cm_wall_circle_sweep(struct traceWork_s *trace, struct CollisionBorder_s *wall, struct trace_s *hit)
{
    /* conditions are the binary's exact x87 jump conditions (ja/jae fall through on NaN); float locals are
     * volatile because the binary spills each one to a float slot; constants are the .rodata floats */
    volatile float eps_r;        /* local_10 */
    volatile float dv;           /* local_18 */
    volatile float dist0;        /* local_14 */
    volatile float t;            /* local_1c */
    volatile float side;         /* local_20 */
    volatile float z_here;       /* local_40 */
    float v6[2];        /* local_6c, local_68 */
    float v4[2];        /* local_4c, local_48 */
    volatile float proj;         /* local_50 */
    volatile float proj_len_sq;  /* local_54 */
    volatile float disc_c;       /* local_5c */
    volatile float disc;         /* local_58 */
    volatile float z_pt;         /* local_64 */
    volatile float tmp;          /* lVar2 */
    volatile float tsum;
    vec3_t pt;          /* local_3c */

    dv = Vec2_Dot_08057610(trace->delta, wall->normal);
    if (!(dv >= 0.0f)) {
        eps_r = trace->radius + 0.125f;
        dist0 = Vec2_Dot_08057610(trace->start, wall->normal) - wall->dist;
        t = (eps_r - dist0) / dv;
        if (!(t >= hit->fraction) && !(-eps_r > t * trace->slope_scale)) {
            VectorMA(trace->start, t, trace->delta, pt);
            side = (wall->normal[1] * pt[0] - wall->normal[0] * pt[1]) - wall->tangent_min;
            if (side < 0.0) {
                v6[0] = wall->normal[0] * wall->dist + wall->normal[1] * wall->tangent_min;
                v6[1] = wall->normal[1] * wall->dist - wall->normal[0] * wall->tangent_min;
                Vec2_Subtract_080575e4(trace->start, v6, v4);
                proj = Vec2_Dot_08057610(trace->delta, v4);
                if (0.0f <= proj) {
                    return;
                }
                proj_len_sq = Vec2_LengthSq_08057632(v4);
                disc_c = proj_len_sq - eps_r * eps_r;
                if (disc_c < 0.0) {
                    z_pt = wall->z_base;
                    tmp = cm_fabs(z_pt - trace->start[2]);
                    if ((double)trace->tolerance < tmp) {
                        return;
                    }
                    cm4_vec3_set_bits(hit->normal, &wall->normal[0], &wall->normal[1], 0);
                    hit->fraction = 0.0f;
                    if (trace->radius * trace->radius > proj_len_sq) {
                        hit->startsolid = 1;
                    }
                    return;
                }
                disc = proj * proj - trace->delta_len_sq * disc_c;
                if (disc < 0.0) {
                    return;
                }
                tmp = Float_Sqrt_080575a0(disc);
                t = ((double)(-proj) - tmp) / trace->delta_len_sq;
                if (hit->fraction <= t) {
                    return;
                }
                if (t <= 0.0) {
                    return;
                }
                VectorMA(trace->start, t, trace->delta, pt);
                side = 0.0f;
            } else if (wall->tangent_max < side) {
                v6[0] = (wall->tangent_min + wall->tangent_max) * wall->normal[1] + wall->normal[0] * wall->dist;
                v6[1] = wall->normal[1] * wall->dist - (wall->tangent_min + wall->tangent_max) * wall->normal[0];
                Vec2_Subtract_080575e4(trace->start, v6, v4);
                proj = Vec2_Dot_08057610(trace->delta, v4);
                if (0.0f <= proj) {
                    return;
                }
                proj_len_sq = Vec2_LengthSq_08057632(v4);
                disc_c = proj_len_sq - eps_r * eps_r;
                if (disc_c < 0.0) {
                    z_pt = wall->z_base + wall->tangent_max * wall->z_slope;
                    tmp = cm_fabs(trace->start[2] - z_pt);
                    if ((double)trace->tolerance < tmp) {
                        return;
                    }
                    cm4_vec3_set_bits(hit->normal, &wall->normal[0], &wall->normal[1], 0);
                    hit->fraction = 0.0f;
                    if (trace->radius * trace->radius > proj_len_sq) {
                        hit->startsolid = 1;
                    }
                    return;
                }
                disc = proj * proj - trace->delta_len_sq * disc_c;
                if (disc < 0.0) {
                    return;
                }
                tmp = Float_Sqrt_080575a0(disc);
                t = ((double)(-proj) - tmp) / trace->delta_len_sq;
                if (hit->fraction <= t) {
                    return;
                }
                if (t <= 0.0) {
                    return;
                }
                VectorMA(trace->start, t, trace->delta, pt);
                cm4_copy_bits((float *)&side, &wall->tangent_max);
            } else if (t < 0.0) {
                t = 0.0f;
            }
            z_here = wall->z_base + side * wall->z_slope;
            if (!(cm_fabs(pt[2] - z_here) > trace->tolerance)) {
                cm4_copy_bits(&hit->fraction, &t);
                cm4_vec3_set_bits(hit->normal, &wall->normal[0], &wall->normal[1], 0);
            }
        }
    }
}

// 0x080575a0  size=31  FUN_080575a0  status=translated  naming=unknown
// Evidence: imports sqrt; body is (float)sqrt((double)x).
float Float_Sqrt_080575a0(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x080575c0  size=36  FUN_080575c0  status=translated  naming=unknown
// Ghidra shows void, but the binary tail-jumps into 0x080576b0 and returns its
// result in st(0); the caller reads it as longdouble, so this returns float.
float Float_Max_080575c0(float a, float b)
{
    return Float_SelectNonNegative_080576b0(a - b, a, b);
}

// 0x080575e4  size=44  FUN_080575e4  status=translated  naming=unknown
void Vec2_Subtract_080575e4(float *a, float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

// 0x08057610  size=33  FUN_08057610  status=translated  naming=unknown
float Vec2_Dot_08057610(float *a, float *b)
{
    return a[1] * b[1] + a[0] * b[0];
}

// 0x08057632  size=33  FUN_08057632  status=translated  naming=unknown
float Vec2_LengthSq_08057632(float *a)
{
    return a[1] * a[1] + a[0] * a[0];
}

// 0x08057654  size=35  FUN_08057654  status=translated  naming=unknown
void Vec3_Set_08057654(float *dst, float x, float y, float z)
{
    /* the binary stores the three stack argument words raw (mov eax,[ebp+0xc]; mov [edx],eax) */
    volatile uint32 *d = (volatile uint32 *)dst;   /* volatile: keeps gcc from turning the copy into fld/fstp */
    d[0] = *(const volatile uint32 *)&x;
    d[1] = *(const volatile uint32 *)&y;
    d[2] = *(const volatile uint32 *)&z;
}

// 0x08057678  size=56  FUN_08057678  status=translated  naming=unknown
void Vec3_Scale_08057678(float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x080576b0  size=41  FUN_080576b0  status=translated  naming=unknown
// Returns a when x >= 0, otherwise b.
float Float_SelectNonNegative_080576b0(float x, float a, float b)
{
    float r;
    if (0.0 <= x) {
        r = a;
    } else {
        r = b;
    }
    return r;
}

// 0x080576dc  size=72  FUN_080576dc  status=translated  naming=inferred
// Counted block allocator: count*12+4 bytes, live count and peak count kept.
void *AllocWinding(uint32 count)
{
    g_08188eec_liveWindingCount = g_08188eec_liveWindingCount + 1;
    if (g_08188ef0_peakWindingCount < g_08188eec_liveWindingCount) {
        g_08188ef0_peakWindingCount = g_08188eec_liveWindingCount;
    }
    return Com_MallocZeroed(count * 0xc + 4);
}

// 0x08057724  size=65  FUN_08057724  status=translated  naming=inferred
// Frees a AllocWinding block; the header word is set to the 0xDEADDEAD marker.
void FreeWinding(int32 *block)
{
    if (block[0] == -0x21522153) {
        Com_Error(0, g_08140020);
    }
    block[0] = -0x21522153;
    g_08188eec_liveWindingCount = g_08188eec_liveWindingCount - 1;
    Com_Free_A(block);
}

// 0x08057766  size=243  FUN_08057766  status=translated  naming=unknown
// Axis-aligned bounds over a block whose first word is a vertex count and whose
// vertices are three float words each. Initial bounds are +/-131072.
void cm_vertex_block_bounds(int32 *block, float *mins, float *maxs)
{
    int32 count;
    int32 v;
    int axis;

    mins[0] = 131072.0f;
    mins[1] = 131072.0f;
    mins[2] = 131072.0f;
    maxs[0] = -131072.0f;
    maxs[1] = -131072.0f;
    maxs[2] = -131072.0f;
    count = block[0];
    for (v = 0; v < count; v = v + 1) {
        for (axis = 0; axis < 3; axis = axis + 1) {
            float value = cm4_bits_to_float((uint32)block[axis + v * 3 + 1]);
            if (value < mins[axis]) {
                mins[axis] = value;
            }
            if (maxs[axis] < value) {
                maxs[axis] = value;
            }
        }
    }
}

// 0x08057fb6  size=25  FUN_08057fb6  status=translated  naming=inferred
float cm_fabs(float x)
{
    return (float)fabs((double)x);
}

// 0x08057fd0  size=47  FUN_08057fd0  status=translated  naming=unknown
// Raw 3-word copy (undefined4 in the binary).
void Vec3_CopyBits_08057fd0(float *src, float *dst)
{
    memcpy(dst, src, 3 * sizeof(uint32));
}

// 0x08058000  size=68  FUN_08058000  status=translated  naming=unknown
void Vec3_Add_08058000(float *a, float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x08058044  size=68  FUN_08058044  status=translated  naming=unknown
void Vec3_Sub_08058044(float *a, float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08058088  size=56  FUN_08058088  status=translated  naming=unknown
// Same body as Vec3_Scale_08057678 (distinct call site).
void Vec3_Scale_08058088(float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x080580c0  size=83  FUN_080580c0  status=translated  naming=unknown
// out = base + s * dir.
void Vec3_MA_080580c0(float *base, float s, float *dir, float *out)
{
    out[0] = base[0] + s * dir[0];
    out[1] = base[1] + s * dir[1];
    out[2] = base[2] + s * dir[2];
}

// 0x08058114  size=51  FUN_08058114  status=translated  naming=unknown
// Dot product; summation order z, y, x as in the binary.
float Vec3_Dot_08058114(float *a, float *b)
{
    return a[2] * b[2] + a[1] * b[1] + a[0] * b[0];
}

/* Raw byte pointer from a decompiler-style int address. */
static inline unsigned char *cm4_bytes(int addr)
{
    return (unsigned char *)(unsigned long)(uint32)addr;
}

/* Model table entry from an int address (scratch model path of cm_model_get). */
static inline cmodel_s *cm4_model_from_int(int addr)
{
    return (cmodel_s *)(unsigned long)(uint32)addr;
}

/* Sets the hit as fully solid: all_solid, start_solid and fraction 0. */
static inline void cm4_mark_all_solid(trace_s *hit)
{
    hit->allsolid = 1;
    hit->startsolid = 1;
    hit->fraction = 0.0f;
}

// 0x08058170  size=35  FUN_08058170  status=translated  naming=inferred
// Loads a model handle by name through the model registry (XModel_FindOrLoad with two
// callbacks). Ghidra shows void, but the caller uses the return value and the
// binary returns the call result in eax.
int cm_static_model_load_handle(char *name)
{
    return (int)XModel_FindOrLoad(name, (void *)CM_HunkReserveA, (void *)CM_HunkReserveB);
}

// 0x08058194  size=266  FUN_08058194  status=translated  naming=unknown
// Transforms start and end into model space, traces via the model, and on a hit
// writes the normal back to world space with the flag word 0x3fe.
void cm_static_model_trace(struct cm_static_model_s *model, struct trace_s *hit, float *start, float *end, uint32 arg5)
{
    vec3_t rel;
    vec3_t start_local;
    vec3_t end_local;
    vec3_t normal_world;
    int local_10;
    int rc;

    Vec3_Sub_08058396(start, model->origin, rel);
    Vec3_RotateByTranspose(rel, &model->axes[0][0], start_local);
    Vec3_Sub_08058396(end, model->origin, rel);
    Vec3_RotateByTranspose(rel, &model->axes[0][0], end_local);
    local_10 = (int)XModelRef_GetBlock44((const struct XModelRef *)model->handle);
    rc = XModel_TraceLine((struct XModel *)model->handle, (struct XModelTrace *)hit, (struct DObjBone *)local_10, start_local, end_local, arg5);
    if (-1 < rc) {
        hit->entityNum = 0x3fe;
        Vec3_MulMat3RowsAlt(hit->normal, &model->axes[0][0], normal_world);
        Vec3_Normalize(normal_world);
        Vec3_CopyBits_08058366(normal_world, hit->normal);
    }
}

// 0x0805829e  size=200  FUN_0805829e  status=translated  naming=unknown
// Boolean-style test: traces with a local hit seeded to fraction 1.0 and returns
// the sign bit of the model result (1 when negative).
uint32 cm_static_model_test(struct cm_static_model_s *model, float *start, float *end, uint32 arg4)
{
    vec3_t rel;
    vec3_t start_local;
    vec3_t end_local;
    uint32 hit_words[12];
    trace_s *hit = (trace_s *)hit_words;
    int local_10;
    uint32 rc;

    Vec3_Sub_08058396(start, model->origin, rel);
    Vec3_RotateByTranspose(rel, &model->axes[0][0], start_local);
    Vec3_Sub_08058396(end, model->origin, rel);
    Vec3_RotateByTranspose(rel, &model->axes[0][0], end_local);
    hit_words[0] = cm4_float_to_bits(1.0f);
    local_10 = (int)XModelRef_GetBlock44((const struct XModelRef *)model->handle);
    rc = (uint32)XModel_TraceLine((struct XModel *)model->handle, (struct XModelTrace *)hit, (struct DObjBone *)local_10, start_local, end_local, arg4);
    return rc >> 31;
}

// 0x08058366  size=47  FUN_08058366  status=translated  naming=unknown
// Raw 3-word copy (undefined4 in the binary).
void Vec3_CopyBits_08058366(float *src, float *dst)
{
    memcpy(dst, src, 3 * sizeof(uint32));
}

// 0x08058396  size=68  FUN_08058396  status=translated  naming=unknown
void Vec3_Sub_08058396(float *a, float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080583dc  size=276  FUN_080583dc  status=translated  naming=inferred
// Stores the origin, builds the scaled axis frame from the angles and scale, and
// updates the model bounds (inferred: static model placement).
void cm_static_model_set_transform(struct cm_static_model_s *model, float *origin, float *angles, float *scale)
{
    float axes9[9];
    int rc;

    Vec3_CopyBits_08058ae4(origin, model->origin);
    AnglesToAxis(angles, axes9);
    Vec3_Scale_08058b58(axes9 + 0, scale[0], axes9 + 0);
    Vec3_Scale_08058b58(axes9 + 3, scale[1], axes9 + 3);
    Vec3_Scale_08058b58(axes9 + 6, scale[2], axes9 + 6);
    Mat3_Invert(axes9, &model->axes[0][0]);
    rc = XModel_ComputeBounds((const struct XModel *)model->handle, axes9, model->bounds_mins, model->bounds_maxs);
    if (rc != 0) {
        Vec3_Add_08058b14(model->bounds_mins, origin, model->bounds_mins);
        Vec3_Add_08058b14(model->bounds_maxs, origin, model->bounds_maxs);
    }
}

// 0x080584f0  size=269  FUN_080584f0  status=translated  naming=inferred
// Validates the scale components (warns on zero), loads the model by name and
// applies the transform. Returns true when the model handle is non-zero.
int cm_static_model_init(struct cm_static_model_s *model, char *name, float *origin, float *angles, float *scale)
{
    int handle;

    if ((name == NULL) || (*name == '\0')) {
        Com_Error(1, g_msgInvalidStaticModelName);
    }
    if (scale[0] == 0.0f) {
        Com_Error(1, g_msgStaticModelZeroScaleX, name);
    }
    if (scale[1] == 0.0f) {
        Com_Error(1, g_msgStaticModelZeroScaleY, name);
    }
    if (scale[2] == 0.0f) {
        Com_Error(1, g_msgStaticModelZeroScaleZ, name);
    }
    handle = cm_static_model_load_handle(name);
    if (handle != 0) {
        model->handle = handle;
        cm_static_model_set_transform(model, origin, angles, scale);
    }
    return handle != 0;
}

// 0x080585fe  size=1254  FUN_080585fe  status=translated  naming=observed
// Anchor: string "CM_CreateStaticModel" (allocation tag, line 0x17).
// Two passes over the entity string at g_08188e58_entityString: pass 1 counts
// classname "misc_model" entities that name a loadable model; pass 2 allocates
// the 0x50-byte static model array and initialises each entry.
// QUIRK (preserved): the model name handed to cm_static_model_init is
// model_buf + 7, while the existence check uses model_buf. The key and value
// buffers are copied with unbounded strcpy, as in the binary.
void cm_load_static_models(void)
{
    const char *cursor;
    char *token;
    int entry;
    int has_misc;
    char key[64];
    char value[64];
    char model_buf[72];
    float origin[3];
    float angles[3];
    float scale[3];

    cursor = (const char *)g_08188e58_entityString;
    g_08188dc4_numStaticModels = 0;
    g_08188dc8_staticModels = NULL;
    for (;;) {
        token = Com_Parse(&cursor);
        if (cursor == 0 || *token != '{') {
            break;
        }
        model_buf[0] = '\0';
        has_misc = 0;
        for (;;) {
            token = Com_Parse(&cursor);
            if (!(cursor != 0 && *token != '}')) {
                break;
            }
            strcpy(key, token);
            token = Com_Parse(&cursor);
            if (cursor == 0) {
                break;
            }
            strcpy(value, token);
            if (strcasecmp(key, "classname") == 0) {
                if (strcasecmp(value, "misc_model") == 0) {
                    has_misc = 1;
                }
            } else if (strcasecmp(key, "model") == 0) {
                strcpy(model_buf, value);
            }
        }
        if (has_misc && String_IsXModelPath(model_buf) != 0) {
            g_08188dc4_numStaticModels = g_08188dc4_numStaticModels + 1;
        }
    }

    if (g_08188dc4_numStaticModels != 0) {
        g_08188dc8_staticModels = (cm_static_model_s *)CM_HunkAlloc(g_08188dc4_numStaticModels * 0x50, "CM_CreateStaticModel", 0x17);
        cursor = (const char *)g_08188e58_entityString;
        entry = 0;
        for (;;) {
            token = Com_Parse(&cursor);
            if (!(cursor != 0 && *token == '{')) {
                break;
            }
            model_buf[0] = '\0';
            origin[0] = 0.0f;
            origin[1] = 0.0f;
            origin[2] = 0.0f;
            angles[0] = 0.0f;
            angles[1] = 0.0f;
            angles[2] = 0.0f;
            scale[0] = 1.0f;
            scale[1] = 1.0f;
            scale[2] = 1.0f;
            has_misc = 0;
            for (;;) {
                token = Com_Parse(&cursor);
                if (!(cursor != 0 && *token != '}')) {
                    break;
                }
                strcpy(key, token);
                token = Com_Parse(&cursor);
                if (cursor == 0) {
                    break;
                }
                strcpy(value, token);
                if (strcasecmp(key, "classname") == 0) {
                    if (strcasecmp(value, "misc_model") == 0) {
                        has_misc = 1;
                    }
                } else if (strcasecmp(key, "model") == 0) {
                    strcpy(model_buf, value);
                } else if (strcasecmp(key, "origin") == 0) {
                    sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
                } else if (strcasecmp(key, "angles") == 0) {
                    sscanf(value, "%f %f %f", &angles[0], &angles[1], &angles[2]);
                } else if (strcasecmp(key, "modelscale_vec") == 0) {
                    sscanf(value, "%f %f %f", &scale[0], &scale[1], &scale[2]);
                } else if (strcasecmp(key, "modelscale") == 0) {
                    scale[0] = (float)atof(value);
                    scale[1] = scale[0];
                    scale[2] = scale[0];
                }
            }
            if (has_misc && String_IsXModelPath(model_buf) != 0) {
                if (cm_static_model_init(&g_08188dc8_staticModels[entry], model_buf + 7, origin, angles, scale) == 0) {
                    g_08188dc4_numStaticModels = g_08188dc4_numStaticModels - 1;
                } else {
                    entry = entry + 1;
                }
            }
        }
    }
}

// 0x08058ae4  size=47  FUN_08058ae4  status=translated  naming=unknown
// Raw 3-word copy (undefined4 in the binary).
void Vec3_CopyBits_08058ae4(float *src, float *dst)
{
    memcpy(dst, src, 3 * sizeof(uint32));
}

// 0x08058b14  size=68  FUN_08058b14  status=translated  naming=unknown
void Vec3_Add_08058b14(float *a, float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x08058b58  size=56  FUN_08058b58  status=translated  naming=unknown
// Same body as Vec3_Scale_08057678 (in-place friendly).
void Vec3_Scale_08058b58(float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x08058b90  size=159  FUN_08058b90  status=translated  naming=unknown
// Walks the plane tree (8-byte nodes) to a leaf. Returns -1 - leaf index.
int CM_PointLeafnum_r(float *point, int node)
{
    while (-1 < node) {
        cnode_s *pn = &g_08188de0_nodes[node];
        cplane_s *plane = pn->plane;
        float d;
        if (plane->type < 3) {
            d = point[plane->type] - plane->dist;
        } else {
            d = Vec3_Dot_0805923c(plane->normal, point) - plane->dist;
        }
        if (d < 0.0f) {
            node = pn->back;
        } else {
            node = pn->front;
        }
    }
    return -1 - node;
}

// 0x08058c30  size=27  FUN_08058c30  status=translated  naming=unknown
// Root query. Ghidra shows void; the binary tail-calls CM_PointLeafnum_r
// and the callers use its result, so this returns int.
int CM_PointLeafnum(float *point)
{
    return CM_PointLeafnum_r(point, 0);
}

// 0x08058c4c  size=118  FUN_08058c4c  status=translated  naming=unknown
// Appends a leaf index to the query list (or sets overflow); records the last leaf
// when it has surface data (aux != -1).
void CM_StoreLeafs(struct cm_leaf_list_s *list, int node)
{
    node = -1 - node;
    if (g_08188de8_leafs[node].aux != -1) {
        list->last_leaf = node;
    }
    if (list->count < list->capacity) {
        list->items[list->count] = node;
        list->count = list->count + 1;
    } else {
        list->overflow = 1;
    }
}

// 0x08058cc2  size=168  FUN_08058cc2  status=translated  naming=unknown
// Box-vs-plane traversal of the plane tree; collects leaves that the box touches.
void CM_BoxLeafnums_r(struct cm_leaf_list_s *list, int node)
{
    while (-1 < node) {
        cnode_s *pn = &g_08188de0_nodes[node];
        int side = BoxOnPlaneSide(list->box_mins, list->box_maxs, pn->plane);
        if (side == 1) {
            node = pn->front;
        } else if (side == 2) {
            node = pn->back;
        } else {
            CM_BoxLeafnums_r(list, pn->front);
            node = pn->back;
        }
    }
    CM_StoreLeafs(list, node);
}

// 0x08058d6a  size=113  FUN_08058d6a  status=translated  naming=unknown
// Query entry point: builds a leaf list over the box [mins, maxs] with the given
// item array and capacity. Returns the count; writes the last leaf to *last_leaf_out.
int CM_BoxLeafnums(float *mins, float *maxs, int32 *items, int32 capacity, int32 *last_leaf_out)
{
    cm_leaf_list_s list;

    list.count = 0;
    list.capacity = capacity;
    list.overflow = 0;
    list.items = items;
    list.last_leaf = 0;
    Vec3_CopyBits_080591c8(mins, list.box_mins);
    Vec3_CopyBits_080591c8(maxs, list.box_maxs);
    CM_BoxLeafnums_r(&list, 0);
    *last_leaf_out = list.last_leaf;
    return list.count;
}

// 0x08058ddc  size=432  FUN_08058ddc  status=translated  naming=unknown
// Point query over the BSP node tree: OR of the contents of every brush that
// contains the point (AABB, then brush planes).
uint32 cm_node_point_contents(float *point, struct cbrushnode_s *node)
{
    uint32 contents = 0;

    for (;;) {
        int child_sel;
        if (node->count != 0) {
            if (0 < node->count) {
                int i = 0;
                for (;;) {
                    const cbrush_s *brush;
                    int inside;
                    int k;
                    int side_i;
                    if (node->count <= i) {
                        return contents;
                    }
                    brush = &g_08188e40_brushes[node->brush_refs[i]];
                    inside = 1;
                    for (k = 0; k < 3; k = k + 1) {
                        if ((point[k] < brush->mins[k]) || (brush->maxs[k] < point[k])) {
                            inside = 0;
                            break;
                        }
                    }
                    if (inside) {
                        for (side_i = 0; side_i < brush->side_count; side_i = side_i + 1) {
                            const cbrushside_s *side = &brush->sides[side_i];
                            if (side->plane->dist < Vec3_Dot_0805923c(side->plane->normal, point)) {
                                inside = 0;
                                break;
                            }
                        }
                    }
                    if (inside) {
                        contents = contents | brush->contents;
                    }
                    i = i + 1;
                }
            }
            contents = contents | cm_node_point_contents(point, node + 1);
        }
        child_sel = (point[node->axis] <= node->split) ? 2 : 0;
        node = node + node->child_offset[child_sel / 2];
    }
}

// 0x08058f8c  size=268  FUN_08058f8c  status=translated  naming=unknown
// Point contents for the world (model_index 0, plane-tree leaf) or for a model
// (embedded leaf). Returns 0 when the point is outside the leaf bounds.
uint32 CM_PointContents(float *point, int model_index)
{
    const cLeaf_s *leaf;
    int k;

    if (model_index == 0) {
        int leaf_index = CM_PointLeafnum_r(point, 0);
        leaf = &g_08188de8_leafs[leaf_index];
    } else {
        leaf = &cm_model_get(model_index)->leaf;
    }
    if (leaf->node_index == 0) {
        return 0;
    }
    for (k = 0; k < 3; k = k + 1) {
        if (point[k] <= leaf->mins[k]) {
            return 0;
        }
        if (leaf->maxs[k] <= point[k]) {
            return 0;
        }
    }
    return cm_node_point_contents(point, &g_08188df0_brushNodes[leaf->node_index]);
}

// 0x08059098  size=237  FUN_08059098  status=translated  naming=unknown
// Point contents in a model's local frame: subtracts the origin, rotates by the
// model angles when any is non-zero or NaN, then queries the model contents.
uint32 CM_TransformedPointContents(const float *point, int model_index, const float *origin, const float *angles)
{
    vec3_t local_pt;
    vec3_t fwd;
    vec3_t right;
    vec3_t up;
    vec3_t tmp;

    Vec3_Sub_080591f8(point, origin, local_pt);
    if (cm4_nonzero_or_nan(angles[0]) || cm4_nonzero_or_nan(angles[1]) || cm4_nonzero_or_nan(angles[2])) {
        float right_dot;
        Angles_ToVectors(angles, fwd, right, up);
        Vec3_CopyBits_080591c8(local_pt, tmp);
        local_pt[0] = Vec3_Dot_0805923c(tmp, fwd);
        right_dot = Vec3_Dot_0805923c(tmp, right);
        local_pt[1] = -right_dot;
        local_pt[2] = Vec3_Dot_0805923c(tmp, up);
    }
    return CM_PointContents(local_pt, model_index);   /* tail value: the binary returns the callee's eax */
}

// 0x08059186  size=66  FUN_08059186  status=translated  naming=unknown
// Table entry address: base + index * stride, or the base when the index is out of
// range or the table is disabled.
const unsigned char *CM_ClusterPVS(int index)
{
    if ((index < 0) || (g_08188e44_numClusters <= index) || (g_08188e50_vised == 0)) {
        return (const unsigned char *)g_08188e4c_visibility;
    }
    return (const unsigned char *)(index * g_08188e48_clusterBytes + g_08188e4c_visibility);
}

// 0x080591c8  size=47  FUN_080591c8  status=translated  naming=unknown
// Raw 3-word copy (undefined4 in the binary).
void Vec3_CopyBits_080591c8(float *src, float *dst)
{
    memcpy(dst, src, 3 * sizeof(uint32));
}

// 0x080591f8  size=68  FUN_080591f8  status=translated  naming=unknown
void Vec3_Sub_080591f8(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0805923c  size=51  FUN_0805923c  status=translated  naming=unknown
// Dot product; summation order z, y, x as in the binary.
float Vec3_Dot_0805923c(float *a, float *b)
{
    return a[2] * b[2] + a[1] * b[1] + a[0] * b[0];
}

// 0x08059270  size=68  FUN_08059270  status=translated  naming=unknown
// Increments the slot-3 counter of the scratch table and copies six words.
void cm_scratch_acquire(int *state)
{
    int *slot = (int *)Sys_GetValue(3);
    slot[0] = slot[0] + 1;
    state[0] = slot[0];
    state[1] = slot[1];
    state[2] = slot[2];
    state[3] = slot[3];
    state[4] = slot[4];
    state[5] = slot[5];
}

// 0x080592b4  size=45  FUN_080592b4  status=translated  naming=unknown
// Returns the two scratch words (+0x10 and +0x14) of table slot 3.
void cm_scratch_get_pair(int *a, int *b)
{
    int *slot = (int *)Sys_GetValue(3);
    *a = slot[4];
    *b = slot[5];
}

// 0x080592e2  size=118  FUN_080592e2  status=translated  naming=unknown
// Stages a segment (a, b) into the two scratch blocks and stores a tag word.
// Returns 0x3ff.
uint32 CM_TempBoxModel(float *a, float *b, uint32 tag)
{
    int local_c;
    int local_8;

    cm_scratch_get_pair(&local_8, &local_c);
    Vec3_Copy(a, cm4_fptr(local_c));
    Vec3_Copy(b, cm4_fptr(local_c + 0xc));
    Vec3_Copy(a, cm4_fptr(local_8));
    Vec3_Copy(b, cm4_fptr(local_8 + 0x10));
    *(uint32 *)(cm4_bytes(local_8) + 0xc) = tag;
    return 0x3ff;
}

// 0x08059358  size=70  FUN_08059358  status=translated  naming=unknown
// Model entry: table element when the index is below the model count, otherwise
// the scratch model.
struct cmodel_s *cm_model_get(int index)
{
    int local_8;
    int local_c;

    if (index < g_08188e34_numSubModels) {
        return &g_08188e38_cmodels[index];
    }
    cm_scratch_get_pair(&local_8, &local_c);
    return cm4_model_from_int(local_c);
}

// 0x0805939e  size=34  FUN_0805939e  status=translated  naming=unknown
// Surface mask (embedded leaf +0x08) OR brush mask (+0x04).
uint32 cm_model_surface_mask(int index)
{
    cmodel_s *model = cm_model_get(index);
    return model->leaf.surface_mask | model->leaf.brush_mask;
}

// 0x080593c0  size=34  FUN_080593c0  status=translated  naming=unknown
// Model header float at +0x18.
float cm_model_radius(int index)
{
    cmodel_s *model = cm_model_get(index);
    return model->radius;
}

// 0x080593e2  size=107  FUN_080593e2  status=translated  naming=unknown
// In-place 3x3 matrix times vector: v = M * v, with M rows at m, m+3, m+6.
void Mat3_TransformVecInPlace_080593e2(float *v, float *m)
{
    float tmp[3];

    Vec3_Copy(v, tmp);
    v[0] = Vec3_Dot(m, tmp);
    v[1] = Vec3_Dot(m + 3, tmp);
    v[2] = Vec3_Dot(m + 6, tmp);
}

// 0x0805944e  size=107  FUN_0805944e  status=translated  naming=unknown
// 3x3 transpose: out[c][r] = in[r][c].
void Mat3_TransposeBits_0805944e(float *in, float *out)
{
    int c;
    int r;

    for (c = 0; c < 3; c = c + 1) {
        for (r = 0; r < 3; r = r + 1) {
            *(volatile uint32 *)&out[c * 3 + r] = *(const volatile uint32 *)&in[r * 3 + c];   /* raw word copy */
        }
    }
}

// 0x080594ba  size=60  FUN_080594ba  status=translated  naming=unknown
// Angle vectors (forward, right, up) with the right vector negated.
void Angles_ToVectorsNegRight_080594ba(float *angles, float *axes)
{
    Angles_ToVectors(angles, axes, axes + 3, axes + 6);
    Vec3_Negate((uint32 *)(axes + 3));
}

// 0x080594f6  size=319  FUN_080594f6  status=translated  naming=unknown
// Brush-vs-trace-box overlap, then per-plane separation with the trace radius and
// tolerance. When no plane separates, the brush is treated as solid (all_solid,
// start_solid, fraction 0).
void cm_brush_box_test(struct traceWork_s *trace, struct cbrush_s *brush, struct trace_s *hit)
{
    int remaining;
    const cbrushside_s *side;

    if ((trace->bounds_mins[0] <= brush->maxs[0]) && (trace->bounds_mins[1] <= brush->maxs[1]) &&
        (trace->bounds_mins[2] <= brush->maxs[2]) &&
        (brush->mins[0] <= trace->bounds_maxs[0]) && (brush->mins[1] <= trace->bounds_maxs[1]) &&
        (brush->mins[2] <= trace->bounds_maxs[2])) {
        side = brush->sides;
        remaining = brush->side_count;
        while (remaining != 0) {
            const cplane_s *plane = side->plane;
            float margin = Sweep_FloatAbs(plane->normal[2] * trace->tolerance);
            float reach = plane->dist + trace->radius + margin;
            float sep = (float)(Vec3_Dot(trace->start, plane->normal) - reach);
            if (0.0 < sep) {
                return;
            }
            remaining = remaining - 1;
            side = side + 1;
        }
        hit->allsolid = 1;
        hit->startsolid = 1;
        hit->fraction = 0.0f;
        hit->contents = (int)brush->contents;
    }
}

// 0x08059636  size=408  FUN_08059636  status=translated  naming=unknown
// Recursive BSP trace over node children: leaf brush lists and split planes.
void cm_node_trace(struct traceWork_s *trace, struct cbrushnode_s *node, struct trace_s *hit)
{
    for (;;) {
        if ((trace->content_mask & node->content_mask) == 0) {
            return;
        }
        if (node->count != 0) {
            if (0 < node->count) {
                int i = 0;
                while (i < node->count) {
                    const cbrush_s *brush = &g_08188e40_brushes[node->brush_refs[i]];
                    if ((trace->content_mask & brush->contents) != 0) {
                        cm_brush_box_test(trace, (cbrush_s *)brush, hit);
                        if (hit->allsolid != 0) {
                            break;
                        }
                    }
                    i = i + 1;
                }
                return;
            }
            cm_node_trace(trace, node + 1, hit);
            if (hit->allsolid != 0) {
                return;
            }
        }
        if (node->split < trace->bounds_mins[node->axis]) {
            node = node + node->child_offset[0];
        } else {
            if (node->split <= trace->bounds_maxs[node->axis]) {
                cm_node_trace(trace, node + node->child_offset[0], hit);
                if (hit->allsolid != 0) {
                    return;
                }
            }
            node = node + node->child_offset[1];
        }
    }
}

// 0x080597ce  size=170  FUN_080597ce  status=translated  naming=unknown
// Leaf bounds vs trace box; when they overlap on all axes, traces the leaf's node
// subtree. Returns the all_solid flag.
uint8 cm_leaf_box_brush_trace(struct traceWork_s *trace, struct cLeaf_s *leaf, struct trace_s *hit)
{
    int c = 0;

    for (;;) {
        if (2 < c) {
            cm_node_trace(trace, &g_08188df0_brushNodes[leaf->node_index], hit);
            return hit->allsolid;
        }
        if (trace->bounds_maxs[c] <= leaf->mins[c]) {
            break;
        }
        if (leaf->maxs[c] <= trace->bounds_mins[c]) {
            return 0;
        }
        c = c + 1;
    }
    return 0;
}

// 0x08059878  size=102  FUN_08059878  status=translated  naming=unknown
// Leaf trace: brush-mask test through cm_leaf_box_brush_trace, then the surface
// trace when the surface mask matches.
void cm_leaf_trace(struct traceWork_s *trace, struct cLeaf_s *leaf, struct trace_s *hit)
{
    int r;

    if (((trace->content_mask & leaf->brush_mask) == 0) ||
        ((r = cm_leaf_box_brush_trace(trace, leaf, hit)) == 0)) {
        if ((trace->content_mask & leaf->surface_mask) != 0) {
            CM_PositionTestInTerrainLeaf(trace, leaf, hit);
        }
    }
}

// 0x080598de  size=878  FUN_080598de  status=translated  naming=unknown
// Sphere-style test of the trace's top and bottom points (offset by the tolerance)
// against the expanded bounds centre and its half-extent circle.
void cm_trace_aabb_bounds(struct traceWork_s *trace, struct trace_s *hit)
{
    vec3_t top;
    vec3_t bottom;
    float center[3];
    float half_dev[6];
    float min_half;
    float fVar3;
    float fVar4;
    float fVar1;
    float fVar2;
    vec3_t center_hi;
    vec3_t center_lo;
    vec3_t diff;
    const cm_aabb_s *bounds = trace->bounds_ref;
    int k;

    Vec3_CopyBits_080591c8(trace->start, top);
    top[2] = top[2] + trace->tolerance;
    Vec3_CopyBits_080591c8(trace->start, bottom);
    bottom[2] = bottom[2] - trace->tolerance;
    for (k = 0; k < 3; k = k + 1) {
        center[k] = (bounds->mins[k] + bounds->maxs[k]) * 0.5f;
        half_dev[k] = bounds->mins[k] - center[k];
        half_dev[k + 3] = bounds->maxs[k] - center[k];
    }
    if (half_dev[5] < half_dev[3]) {
        min_half = half_dev[5];
    } else {
        min_half = half_dev[3];
    }
    fVar3 = half_dev[5] - min_half;
    fVar4 = (trace->radius + min_half) * (trace->radius + min_half);
    Vec3_CopyBits_080591c8(center, center_hi);
    center_hi[2] = center_hi[2] + fVar3;
    Vec3_Subtract(center_hi, top, diff);
    if (Vec3_LengthSquared(diff) < fVar4) {
        cm4_mark_all_solid(hit);
    }
    Vec3_Subtract(center_hi, bottom, diff);
    if (Vec3_LengthSquared(diff) < fVar4) {
        cm4_mark_all_solid(hit);
    }
    Vec3_CopyBits_080591c8(center, center_lo);
    center_lo[2] = center_lo[2] - fVar3;
    Vec3_Subtract(center_lo, top, diff);
    if (Vec3_LengthSquared(diff) < fVar4) {
        cm4_mark_all_solid(hit);
    }
    Vec3_Subtract(center_lo, bottom, diff);
    if (Vec3_LengthSquared(diff) < fVar4) {
        cm4_mark_all_solid(hit);
    }
    fVar1 = trace->box_bias[2];
    fVar2 = trace->radius;
    if (Sweep_FloatAbs(trace->start[2] - center[2]) <= (fVar3 + fVar1) - fVar2) {
        center_hi[2] = 0.0f;
        top[2] = 0.0f;
        Vec3_Subtract(top, center_hi, diff);
        if (Vec3_LengthSquared(diff) < fVar4) {
            cm4_mark_all_solid(hit);
        }
    }
}

// 0x08059c4c  size=371  FUN_08059c4c  status=translated  naming=unknown
// Expands the trace box by its bias and by one unit, collects the plane-tree leaves
// it touches and traces each leaf until solid.
void cm_trace_leaves(struct traceWork_s *trace, struct trace_s *hit)
{
    cm_leaf_list_s list;
    int32 items[1026];
    int i;
    int k;

    if (hit->allsolid == 0) {
        Vec3_Subtract(trace->start, trace->box_bias, list.box_mins);
        Vec3_Add(trace->start, trace->box_bias, list.box_maxs);
        for (k = 0; k < 3; k = k + 1) {
            list.box_mins[k] = list.box_mins[k] - 1.0;
            list.box_maxs[k] = list.box_maxs[k] + 1.0;
        }
        list.count = 0;
        list.capacity = 0x400;
        list.overflow = 0;
        list.items = items;
        list.last_leaf = 0;
        CM_BoxLeafnums_r(&list, 0);
        if (list.count != 0) {
            i = 0;
            while ((i < list.count) && (hit->allsolid == 0)) {
                cm_leaf_trace(trace, &g_08188de8_leafs[items[i]], hit);
                i = i + 1;
            }
        }
    }
}

// 0x08059dc0  size=1056  FUN_08059dc0  status=translated  naming=unknown
// Brush trace. Pass 1 tests the brush's min and max faces (axis-aligned, tagged by
// face_tag); pass 2 tests the brush planes. The best entry plane is kept as
// best_side; a synthetic plane stands in for the axis faces of pass 1.
void cm_brush_trace_planes(struct traceWork_s *trace, struct cbrush_s *brush, struct trace_s *hit)
{
    float best_t = 0.0f;                 /* local_1c */
    float limit;                         /* local_20 */
    int inside = 1;                      /* local_2c */
    cbrushside_s *best_side = NULL;   /* local_38 */
    cplane_s face_plane;               /* local_7c (axis face normal) */
    cbrushside_s face_side;           /* local_54 / local_50 */
    float sign = -1.0f;                  /* local_44 */
    const float *box_pt = brush->mins;   /* local_48 */
    int group = 0;                       /* local_80 */
    int axis;

    limit = hit->fraction;
    for (;;) {
        for (axis = 0; axis < 3; axis = axis + 1) {
            float d0 = (trace->start[axis] - box_pt[axis]) * sign - trace->half_extent[axis];
            float d1 = (trace->end[axis] - box_pt[axis]) * sign - trace->half_extent[axis];
            if (0.0 < d0) {
                float lim = Sweep_FloatMin(d0, 0.125);
                float ft;
                if (lim <= d1) {
                    return;
                }
                ft = (d0 - 0.125) * trace->inv_delta[axis] * sign;
                if (limit <= ft) {
                    return;
                }
                if (0.0 < d1) {
                    inside = 0;
                }
                if (best_t < ft) {
                    best_t = ft;
                } else if (best_side != NULL) {
                    continue;
                }
                face_plane.normal[0] = 0.0f;
                face_plane.normal[1] = 0.0f;
                face_plane.normal[2] = 0.0f;
                face_plane.normal[axis] = sign;
                face_side.plane = &face_plane;
                face_side.model_index = brush->face_tag[group][axis];
                best_side = &face_side;
            } else if (0.0 < d1) {
                float ft = d0 * trace->inv_delta[axis] * sign;
                if (ft <= best_t) {
                    return;
                }
                inside = 0;
                limit = Sweep_FloatMin(limit, ft);
            }
        }
        if (group != 0) {
            break;
        }
        sign = 1.0f;
        box_pt = brush->maxs;
        group = 1;
    }

    {
        cbrushside_s *side = brush->sides;
        int remaining = brush->side_count;

        for (;;) {
            if (remaining == 0) {
                hit->contents = (int)brush->contents;
                if (best_side == NULL) {
                    hit->startsolid = 1;
                    if (inside != 0) {
                        hit->allsolid = 1;
                        hit->fraction = 0.0f;
                    }
                } else {
                    dMaterial_s *model = &g_08188dd0_materials[best_side->model_index];
                    hit->fraction = best_t;
                    Vec3_Copy(best_side->plane->normal, hit->normal);
                    hit->surfaceFlags = model->surfaceFlags;
                    hit->material = model;
                }
                return;
            }
            {
                float margin = Sweep_FloatAbs(side->plane->normal[2] * trace->tolerance);
                float reach = side->plane->dist + trace->radius + margin;
                float d0 = (float)(Vec3_Dot(trace->start, side->plane->normal) - reach);
                float d1 = (float)(Vec3_Dot(trace->end, side->plane->normal) - reach);
                float fdelta;
                float fmin;
                if (0.0 < d0) {
                    float lim = Sweep_FloatMin(d0, 0.125);
                    if (lim <= d1) {
                        return;
                    }
                    if (0.0 < d1) {
                        inside = 0;
                    }
                    fdelta = d0 - d1;
                    fmin = d0 - 0.125;
                    if (best_t * fdelta < fmin) {
                        best_t = fmin / fdelta;
                        if (limit <= best_t) {
                            return;
                        }
                    } else if (best_side != NULL) {
                        remaining = remaining - 1;
                        side = side + 1;
                        continue;
                    }
                    best_side = side;
                } else if (0.0 < d1) {
                    fdelta = d0 - d1;
                    if (limit * fdelta < d0) {
                        limit = d0 / fdelta;
                        if (limit <= best_t) {
                            return;
                        }
                    }
                    inside = 0;
                }
            }
            remaining = remaining - 1;
            side = side + 1;
        }
    }
}

// 0x0805a1e0  size=968  FUN_0805a1e0  status=translated  naming=unknown
// Recursive ray traversal of the node tree between two points (seg_start, seg_end),
// splitting the segment at the node planes. seg[3] carries the segment parameter.
void cm_trace_node_ray(struct traceWork_s *trace, struct cbrushnode_s *node, float *seg_start, float *seg_end, struct trace_s *hit)
{
    float seg[7];       /* local_4c; seg[3] is the parameter */
    float mid[4];       /* local_6c */
    float d_start;      /* local_20 */
    float d_end;        /* local_24 */
    float reach;        /* local_1c */
    float dmax;         /* local_30 */
    float dmin;         /* local_2c */
    float fVar1;
    float span;         /* local_28 */
    float inv_len;      /* local_50 */
    float frac_a;       /* local_5c */
    float frac_b;       /* local_58 */
    int child_sel;      /* local_54 */
    int k;

    Vec4_Copy(seg_start, seg);
    for (;;) {
        for (;;) {
            for (;;) {
                if ((trace->content_mask & node->content_mask) == 0) {
                    return;
                }
                if (node->count != 0) {
                    if (0 < node->count) {
                        int i;
                        for (i = 0; i < node->count; i = i + 1) {
                            const cbrush_s *brush = &g_08188e40_brushes[node->brush_refs[i]];
                            if ((trace->content_mask & brush->contents) != 0) {
                                cm_brush_trace_planes(trace, (cbrush_s *)brush, hit);
                            }
                        }
                        return;
                    }
                    cm_trace_node_ray(trace, node + 1, seg, seg_end, hit);
                }
                d_start = seg[node->axis] - node->split;
                d_end = seg_end[node->axis] - node->split;
                reach = (trace->box_bias[node->axis] + 0.125f) - node->extent;
                dmax = Sweep_FloatMax(d_start, d_end);
                dmin = Sweep_FloatMin(d_start, d_end);
                /* binary: fucompp; jae -> unordered (NaN) falls to the straddle test, so test the negation */
                if (!(dmin >= reach)) {
                    break;
                }
                if (dmax <= -reach) {
                    return;
                }
                node = node + node->child_offset[0];
            }
            if (!(-reach >= dmax)) {
                break;
            }
            node = node + node->child_offset[1];
        }
        if (hit->fraction <= seg[3]) {
            break;
        }
        fVar1 = d_end - d_start;
        {
            float len = Sweep_FloatAbs(fVar1);
            if (4.7683716e-07 < len) {
                span = Sweep_FloatSelect(fVar1, -d_start, d_start);
                inv_len = (float)(1.0 / len);
                frac_a = (span - reach) * inv_len;
                frac_b = (span + reach) * inv_len;
                child_sel = Sweep_FloatIsNonNegative(fVar1);
            } else {
                child_sel = 0;
                frac_b = 1.0f;
                frac_a = 0.0f;
            }
        }
        frac_b = Sweep_FloatMin(frac_b, 1.0f);
        mid[0] = seg[0] + (seg_end[0] - seg[0]) * frac_b;
        mid[1] = seg[1] + (seg_end[1] - seg[1]) * frac_b;
        mid[2] = seg[2] + (seg_end[2] - seg[2]) * frac_b;
        mid[3] = seg[3] + (seg_end[3] - seg[3]) * frac_b;
        cm_trace_node_ray(trace, node + node->child_offset[child_sel], seg, mid, hit);
        frac_a = Sweep_FloatMax(frac_a, 0.0f);
        for (k = 0; k < 4; k = k + 1) {
            seg[k] = seg[k] + (seg_end[k] - seg[k]) * frac_a;
        }
        node = node + node->child_offset[1 - child_sel];
    }
}

// 0x0805a5a8  size=262  FUN_0805a5a8  status=translated  naming=unknown
// Leaf ray test: the leaf bounds grown by the trace box bias must be hit by the
// sweep; then the leaf's node subtree is traced. Returns true when the fraction
// remains exactly 0.
bool cm_leaf_ray_trace(struct traceWork_s *trace, struct cLeaf_s *leaf, struct trace_s *hit)
{
    vec3_t box_min;
    vec3_t box_max;
    float seg_a[4];
    float seg_b[4];
    int hit_r;

    Vec3_Subtract(leaf->mins, trace->box_bias, box_min);
    Vec3_Add(leaf->maxs, trace->box_bias, box_max);
    hit_r = Box_SegmentSlabTest((const struct cod2_box_s *)trace, box_min, box_max, hit->fraction);
    if (hit_r == 0) {
        Vec3_Copy(trace->start, seg_a);
        Vec3_Copy(trace->end, seg_b);
        seg_a[3] = 0.0f;
        seg_b[3] = hit->fraction;
        cm_trace_node_ray(trace, &g_08188df0_brushNodes[leaf->node_index], seg_a, seg_b, hit);
        return hit->fraction == 0.0f;
    }
    return false;
}

// 0x0805a6ae  size=185  FUN_0805a6ae  status=translated  naming=unknown
// Leaf surface trace: runs the leaf ray test when the brush mask matches, then the
// leaf's surface entries (0x20 stride) while the fraction is non-zero.
void cm_leaf_surface_trace(struct traceWork_s *trace, struct cLeaf_s *leaf, struct trace_s *hit)
{
    if (hit->fraction != 0.0f) {
        if ((((trace->content_mask & leaf->brush_mask) == 0) || (!cm_leaf_ray_trace(trace, leaf, hit))) &&
            ((trace->content_mask & leaf->surface_mask) != 0)) {
            int i = 0;
            while ((i < (int)leaf->surface_count) && (hit->fraction != 0.0f)) {
                CM_TraceThroughAabbTree(trace, &g_08188e30_aabbTrees[leaf->first_surface + i], hit);
                i = i + 1;
            }
        }
    }
}

// 0x0805a768  size=502  FUN_0805a768  status=translated  naming=unknown
// Sphere of radius (radius + trace radius) about point c: quadratic in the sweep
// direction against the segment a->b. Returns 1 for no contact, 0 for a contact.
int cm_trace_endpoint_sphere(struct traceWork_s *trace, float *a, float *b, float *c, float radius, struct trace_s *hit)
{
    vec3_t d;
    vec3_t nrm;
    float r2;
    float qa;
    float qb;
    float qc;
    float disc;
    float len;
    float tmp;
    float fVar1;
    int ret;

    Vec3_Subtract(a, c, d);
    r2 = (radius + trace->radius) * (radius + trace->radius);
    qc = Vec3_Dot(d, d) - r2;
    if (qc <= 0.0f) {
        hit->fraction = 0.0f;
        hit->startsolid = 1;
        Vec3_NormalizeCopy(d, hit->normal);
        hit->contents = trace->surface_ref->value;
        Vec3_Subtract(b, c, d);
        if (Vec3_LengthSquared(d) <= r2) {
            hit->allsolid = 1;
        }
        ret = 0;
    } else {
        qb = Vec3_Dot(trace->delta, d);
        if (0.0f <= qb) {
            ret = 1;
        } else {
            qa = trace->delta_len_sq;
            disc = qb * qb - qa * qc;
            if (disc < 0.0f) {
                ret = 1;
            } else {
                len = Vec3_NormalizeCopy(d, nrm);
                fVar1 = -qb;
                tmp = (float)(((0.125 * len) / qb) + ((fVar1 - Sweep_FloatSqrt(disc)) / qa));
                if (tmp < hit->fraction) {
                    hit->fraction = Sweep_FloatMax(tmp, 0.0f);
                    Vec3_Copy(nrm, hit->normal);
                    hit->contents = trace->surface_ref->value;
                    ret = 0;
                } else {
                    ret = 1;
                }
            }
        }
    }
    return ret;
}

// 0x0805a95e  size=691  FUN_0805a95e  status=partial  naming=unknown
// Vertical capsule test: the centre-to-start vector against (radius + trace radius)
// with a z-range check against the trace box bias.
// OPEN: the first (already-inside) branch compares against local_14, which the
// binary never writes on that path (uninitialised stack read). This translation
// uses 0.0 for that read; see the chunk open questions.
int cm_trace_vertical_capsule(struct traceWork_s *trace, float *center, float height, float radius, struct trace_s *hit)
{
    vec3_t d;
    vec3_t nrm;
    float r2;
    float qa;
    float qb;
    float qc;
    float disc;
    float len;
    float tmp;
    float t;
    float zdist;
    float fVar2;
    int ret;

    Vec3_Subtract(trace->start, center, d);
    r2 = (radius + trace->radius) * (radius + trace->radius);
    qc = Vec2_Dot(d, d) - r2;
    if (qc <= 0.0f) {
        height = (trace->box_bias[2] - trace->radius) + height;
        /* d[2] is the decompiler's "local_14": the z of d (stack slot [ebp-0x10]) */
        if (Sweep_FloatAbs(d[2]) > height) {
            ret = 1;
        } else {
            hit->fraction = 0.0f;
            hit->startsolid = 1;
            d[2] = 0.0f;
            Vec3_NormalizeCopy(d, hit->normal);
            hit->contents = trace->surface_ref->value;
            Vec3_Subtract(trace->end, center, d);
            if (height >= Sweep_FloatAbs(d[2])) {
                hit->allsolid = 1;
            }
            ret = 0;
        }
    } else {
        qb = Vec2_Dot(trace->delta, d);
        if (0.0f <= qb) {
            ret = 1;
        } else {
            qa = Vec2_Dot(trace->delta, trace->delta);
            disc = qb * qb - qa * qc;
            if (disc < 0.0f) {
                ret = 1;
            } else {
                d[2] = 0.0f;
                len = Vec3_NormalizeCopy(d, nrm);
                tmp = (float)((len * 0.125) / qb);
                fVar2 = -qb;
                t = (float)(((fVar2 - Sweep_FloatSqrt(disc)) / qa) + tmp);
                if (t < hit->fraction) {
                    zdist = Sweep_FloatAbs(((t - tmp) * trace->delta[2] + trace->start[2]) - center[2]);
                    if ((double)((trace->box_bias[2] - trace->radius) + height) < zdist) {
                        ret = 1;
                    } else {
                        hit->fraction = Sweep_FloatMax(t, 0.0f);
                        Vec3_Copy(nrm, hit->normal);
                        hit->contents = trace->surface_ref->value;
                        ret = 0;
                    }
                } else {
                    ret = 1;
                }
            }
        }
    }
    return ret;
}
