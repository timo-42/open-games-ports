/*
 * chunk_004 types: collision-model (CM) structures for cod2_lnxded 1.3.
 * Target: SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7
 *
 * Layouts were recovered from consistent offsets in the Ghidra pseudocode and
 * spot-checked with objdump. Field names describe the role observed in this
 * chunk; semantics not confirmed by a string or callee are marked "unknown".
 * Sizes are only asserted where the stride or the full extent is known.
 * Owned by the chunk-004 worker; the merge lane promotes shared types.
 */
#ifndef COD2_CHUNK_004_TYPES_H
#define COD2_CHUNK_004_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* Forward declarations */
typedef struct traceWork_s traceWork_s;
typedef struct cLeaf_s cLeaf_s;
typedef struct cbrushnode_s cbrushnode_s;
typedef struct cbrush_s cbrush_s;
typedef struct cmodel_s cmodel_s;
typedef struct cm_static_model_s cm_static_model_s;

/* Plane-tree node: 8-byte stride. front/back are absolute node indices; negative means leaf (-1 - leaf). */
typedef struct cnode_s {
    cplane_s *plane;   /* 0x00 */
    int16 front;         /* 0x04 */
    int16 back;          /* 0x06 */
} cnode_s;
COD2_ASSERT_SIZE(cnode_s, 0x08);

/* Brush side: plane pointer plus owning model index (tag). 8-byte stride. */
typedef struct cbrushside_s {
    cplane_s *plane;   /* 0x00 */
    int32 model_index;   /* 0x04 index into the 0x48-byte model table */
} cbrushside_s;
COD2_ASSERT_SIZE(cbrushside_s, 0x08);

/* Brush: axis-aligned bounds, contents mask, planes and per-face model tags. 0x30 stride. */
struct cbrush_s {
    vec3_t mins;               /* 0x00 */
    uint32 contents;           /* 0x0c */
    vec3_t maxs;               /* 0x10 */
    int32 side_count;          /* 0x1c */
    cbrushside_s *sides;    /* 0x20 */
    int16 face_tag[2][3];      /* 0x24: [min/max group][axis] */
};
COD2_ASSERT_SIZE(cbrush_s, 0x30);
COD2_ASSERT_OFFSET(cbrush_s, contents, 0x0c);
COD2_ASSERT_OFFSET(cbrush_s, maxs, 0x10);
COD2_ASSERT_OFFSET(cbrush_s, side_count, 0x1c);
COD2_ASSERT_OFFSET(cbrush_s, sides, 0x20);
COD2_ASSERT_OFFSET(cbrush_s, face_tag, 0x24);

/* Leaf: surface range, masks, bounds and an embedded node index. 0x2c stride. */
struct cLeaf_s {
    uint16 first_surface;   /* 0x00 index into the 0x20-byte surface table */
    uint16 surface_count;   /* 0x02 */
    uint32 brush_mask;      /* 0x04 */
    uint32 surface_mask;    /* 0x08 */
    vec3_t mins;            /* 0x0c */
    vec3_t maxs;            /* 0x18 */
    int32 node_index;       /* 0x24 0 = none, otherwise index into the 0x14-byte node table */
    int16 aux;              /* 0x28 -1 = invalid */
    byte pad_2a[2];         /* 0x2a */
};
COD2_ASSERT_SIZE(cLeaf_s, 0x2c);
COD2_ASSERT_OFFSET(cLeaf_s, brush_mask, 0x04);
COD2_ASSERT_OFFSET(cLeaf_s, surface_mask, 0x08);
COD2_ASSERT_OFFSET(cLeaf_s, mins, 0x0c);
COD2_ASSERT_OFFSET(cLeaf_s, maxs, 0x18);
COD2_ASSERT_OFFSET(cLeaf_s, node_index, 0x24);
COD2_ASSERT_OFFSET(cLeaf_s, aux, 0x28);

/* BSP node: 0x14 stride. count > 0: leaf brush-ref list; count < 0: subtree continues at +0x14. */
struct cbrushnode_s {
    byte axis;                  /* 0x00 split axis, 0..2 */
    byte pad_01;               /* 0x01 */
    int16 count;                /* 0x02 */
    uint32 content_mask;        /* 0x04 */
    union {
        float split;            /* 0x08 when internal */
        uint16 *brush_refs;     /* 0x08 when count > 0 */
    };
    float extent;               /* 0x0c half extent along axis (unknown purpose) */
    uint16 child_offset[2];     /* 0x10 [0] taken when point > split, [1] otherwise; relative node offsets */
};
COD2_ASSERT_SIZE(cbrushnode_s, 0x14);
COD2_ASSERT_OFFSET(cbrushnode_s, content_mask, 0x04);
COD2_ASSERT_OFFSET(cbrushnode_s, extent, 0x0c);
COD2_ASSERT_OFFSET(cbrushnode_s, child_offset, 0x10);

/* Surface entry: 0x20 stride. Contents not recovered. */
typedef struct CollisionAabbTree_s {
    uint32 unknown[8];
} CollisionAabbTree_s;
COD2_ASSERT_SIZE(CollisionAabbTree_s, 0x20);

/* Surface reference used by hit reporting: value at +0x0c. */
typedef struct cm_surface_ref_s {
    uint32 unknown_00[3];
    int value;               /* 0x0c raw word copied to trace_s.contents */
} cm_surface_ref_s;
COD2_ASSERT_OFFSET(cm_surface_ref_s, value, 0x0c);

/* Wedge edge plane: normal + distance, 16-byte stride. */
typedef struct cm_edge_plane_s {
    vec3_t normal;   /* 0x00 */
    float dist;      /* 0x0c */
} cm_edge_plane_s;
COD2_ASSERT_SIZE(cm_edge_plane_s, 0x10);

/* Wedge (triangle prism): three planes, three vertex indices, three edge indices. 0x48 bytes. */
typedef struct CollisionTriangle_s {
    cm_edge_plane_s planes[3];  /* 0x00 */
    int32 vertex_index[3];      /* 0x30 -1 = none */
    int32 edge_index[3];        /* 0x3c -1 = none */
} CollisionTriangle_s;
COD2_ASSERT_SIZE(CollisionTriangle_s, 0x48);
COD2_ASSERT_OFFSET(CollisionTriangle_s, vertex_index, 0x30);
COD2_ASSERT_OFFSET(CollisionTriangle_s, edge_index, 0x3c);

/* Edge: 0x30 stride. */
typedef struct CollisionEdge_s {
    uint32 unknown_00[3];   /* 0x00 */
    vec3_t p0;              /* 0x0c */
    vec3_t p1;              /* 0x18 */
    vec3_t normal;          /* 0x24 */
} CollisionEdge_s;
COD2_ASSERT_SIZE(CollisionEdge_s, 0x30);
COD2_ASSERT_OFFSET(CollisionEdge_s, p0, 0x0c);
COD2_ASSERT_OFFSET(CollisionEdge_s, p1, 0x18);
COD2_ASSERT_OFFSET(CollisionEdge_s, normal, 0x24);

/* 2D wall used by the circle-wall sweep. 0x1c bytes. */
typedef struct CollisionBorder_s {
    float normal[2];        /* 0x00 */
    float dist;             /* 0x08 */
    float z_base;           /* 0x0c */
    float z_slope;          /* 0x10 */
    float tangent_min;      /* 0x14 */
    float tangent_max;      /* 0x18 */
} CollisionBorder_s;
COD2_ASSERT_SIZE(CollisionBorder_s, 0x1c);

/* Axis-aligned box: mins at 0x00, maxs at 0x0c. */
typedef struct cm_aabb_s {
    vec3_t mins;   /* 0x00 */
    vec3_t maxs;   /* 0x0c */
} cm_aabb_s;
COD2_ASSERT_SIZE(cm_aabb_s, 0x18);

/* Trace state. Only observed offsets are named; 0x38..0x5b, 0x84..0x8b and 0xac are unknown. */
struct traceWork_s {
    vec3_t start;                    /* 0x00 */
    vec3_t end;                      /* 0x0c */
    vec3_t inv_delta;                /* 0x18 per-axis 1/delta */
    vec3_t delta;                    /* 0x24 */
    float slope_scale;               /* 0x30 */
    float delta_len_sq;              /* 0x34 */
    byte unknown_38[0x24];           /* 0x38 */
    vec3_t box_bias;                 /* 0x5c */
    vec3_t bounds_mins;              /* 0x68 */
    vec3_t bounds_maxs;              /* 0x74 */
    uint32 content_mask;             /* 0x80 */
    byte unknown_84[8];              /* 0x84 */
    float radius;                    /* 0x8c */
    float tolerance;                 /* 0x90 */
    vec3_t half_extent;              /* 0x94 */
    int32 visit_stamp;               /* 0xa0 */
    int32 *edge_stamps;              /* 0xa4 */
    int32 *vertex_stamps;            /* 0xa8 */
    byte unknown_ac[4];              /* 0xac */
    cm_surface_ref_s *surface_ref;   /* 0xb0 */
    cm_aabb_s *bounds_ref;           /* 0xb4 */
};
COD2_ASSERT_OFFSET(traceWork_s, end, 0x0c);
COD2_ASSERT_OFFSET(traceWork_s, inv_delta, 0x18);
COD2_ASSERT_OFFSET(traceWork_s, delta, 0x24);
COD2_ASSERT_OFFSET(traceWork_s, slope_scale, 0x30);
COD2_ASSERT_OFFSET(traceWork_s, delta_len_sq, 0x34);
COD2_ASSERT_OFFSET(traceWork_s, box_bias, 0x5c);
COD2_ASSERT_OFFSET(traceWork_s, bounds_mins, 0x68);
COD2_ASSERT_OFFSET(traceWork_s, bounds_maxs, 0x74);
COD2_ASSERT_OFFSET(traceWork_s, content_mask, 0x80);
COD2_ASSERT_OFFSET(traceWork_s, radius, 0x8c);
COD2_ASSERT_OFFSET(traceWork_s, tolerance, 0x90);
COD2_ASSERT_OFFSET(traceWork_s, half_extent, 0x94);
COD2_ASSERT_OFFSET(traceWork_s, visit_stamp, 0xa0);
COD2_ASSERT_OFFSET(traceWork_s, edge_stamps, 0xa4);
COD2_ASSERT_OFFSET(traceWork_s, vertex_stamps, 0xa8);
COD2_ASSERT_OFFSET(traceWork_s, surface_ref, 0xb0);
COD2_ASSERT_OFFSET(traceWork_s, bounds_ref, 0xb4);

/* Material record (cm.materials, 0x48 stride). Layout owned by chunk 003; same tag as canon expects. */
struct dMaterial_s {
    char name[64];       /* 0x00 */
    int surfaceFlags;    /* 0x40 */
    int contents;        /* 0x44 */
};
COD2_ASSERT_SIZE(dMaterial_s, 0x48);

/* Collision model entry: 0x48 stride; embedded leaf at +0x1c. */
struct cmodel_s {
    uint32 unknown_00[6];    /* 0x00 */
    float radius;            /* 0x18 */
    cLeaf_s leaf;          /* 0x1c */
};
COD2_ASSERT_SIZE(cmodel_s, 0x48);
COD2_ASSERT_OFFSET(cmodel_s, radius, 0x18);
COD2_ASSERT_OFFSET(cmodel_s, leaf, 0x1c);

/* Leaf-collection query box (CM_BoxLeafnums_r) with result list. */
typedef struct cm_leaf_list_s {
    int32 count;           /* 0x00 */
    int32 capacity;        /* 0x04 */
    int32 overflow;        /* 0x08 */
    int32 *items;          /* 0x0c */
    float box_mins[3];     /* 0x10 */
    float box_maxs[3];     /* 0x1c */
    int32 last_leaf;       /* 0x28 */
} cm_leaf_list_s;
COD2_ASSERT_OFFSET(cm_leaf_list_s, box_mins, 0x10);
COD2_ASSERT_OFFSET(cm_leaf_list_s, box_maxs, 0x1c);
COD2_ASSERT_OFFSET(cm_leaf_list_s, last_leaf, 0x28);

/* Static model: 0x50 stride, allocated as "CM_CreateStaticModel". */
struct cm_static_model_s {
    uint32 unknown_00;       /* 0x00 */
    int32 handle;            /* 0x04 */
    vec3_t origin;           /* 0x08 */
    float axes[3][3];        /* 0x14 */
    vec3_t bounds_mins;      /* 0x38 */
    vec3_t bounds_maxs;      /* 0x44 */
};
COD2_ASSERT_SIZE(cm_static_model_s, 0x50);
COD2_ASSERT_OFFSET(cm_static_model_s, handle, 0x04);
COD2_ASSERT_OFFSET(cm_static_model_s, origin, 0x08);
COD2_ASSERT_OFFSET(cm_static_model_s, axes, 0x14);
COD2_ASSERT_OFFSET(cm_static_model_s, bounds_mins, 0x38);
COD2_ASSERT_OFFSET(cm_static_model_s, bounds_maxs, 0x44);

#endif
