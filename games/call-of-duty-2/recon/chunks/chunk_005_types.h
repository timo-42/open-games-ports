/*
 * Chunk 005 types: collision trace, BSP/kd-tree and area-link structures for
 * cod2_lnxded 1.3 (0x0805ac12..0x0805ecd4).
 *
 * Layouts are recovered from consistent offset use across the chunk and from
 * the callee expectations. Fields whose meaning is not established are kept as
 * named padding (unk_xx) with the offset in the name.
 */
#ifndef COD2_CHUNK_005_TYPES_H
#define COD2_CHUNK_005_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* BSP interior node, stride 8. child >= 0 is a node index, child < 0 is leaf -1-child. */
typedef struct cnode_s {
    cplane_s *plane;        /* 0x00 */
    int16 child[2];             /* 0x04 */
} cnode_s;
COD2_ASSERT_SIZE(cnode_s, 8);
COD2_ASSERT_OFFSET(cnode_s, child, 0x04);

/* Brush side: plane plus surface flags, stride 8. */
typedef struct cbrushside_s {
    cplane_s *plane;        /* 0x00 */
    int surface_flags;          /* 0x04 */
} cbrushside_s;
COD2_ASSERT_SIZE(cbrushside_s, 8);

/* Collision brush, stride 0x30. */
typedef struct cbrush_s {
    vec3_t mins;                /* 0x00 */
    uint32 contents;            /* 0x0c */
    vec3_t maxs;                /* 0x10 */
    int num_sides;              /* 0x1c */
    cbrushside_s *sides;   /* 0x20 */
    byte unk_24[0x0c];          /* 0x24 */
} cbrush_s;
COD2_ASSERT_SIZE(cbrush_s, 0x30);
COD2_ASSERT_OFFSET(cbrush_s, contents, 0x0c);
COD2_ASSERT_OFFSET(cbrush_s, num_sides, 0x1c);

/* Leaf record, stride 0x2c. Leaves reference 0x20-byte items and a kd-tree root. */
typedef struct cLeaf_s {
    uint16 first_item;          /* 0x00 index into the 0x20-byte item table */
    uint16 item_count;          /* 0x02 */
    uint32 mask_items_a;        /* 0x04 tested against trace contents */
    uint32 mask_items_b;        /* 0x08 tested against trace contents */
    vec3_t mins;                /* 0x0c */
    vec3_t maxs;                /* 0x18 */
    int kd_node;                /* 0x24 index into the 0x14-byte kd-node table */
    byte unk_28[4];             /* 0x28 */
} cLeaf_s;
COD2_ASSERT_SIZE(cLeaf_s, 0x2c);
COD2_ASSERT_OFFSET(cLeaf_s, mins, 0x0c);
COD2_ASSERT_OFFSET(cLeaf_s, kd_node, 0x24);

/* Leaf item, stride 0x20. Contents not recovered in this chunk. */
typedef struct CollisionAabbTree_s {
    byte raw[0x20];
} CollisionAabbTree_s;
COD2_ASSERT_SIZE(CollisionAabbTree_s, 0x20);

/* kd-tree node, stride 0x14. Interior nodes split on axis; leaf nodes carry a brush list. */
typedef struct cbrushnode_s {
    byte axis;                  /* 0x00 */
    byte unk_01;                /* 0x01 */
    int16 brush_count;          /* 0x02 >0 leaf brush list, <0 continues at next record */
    uint32 contents;            /* 0x04 */
    union {
        float split;            /* 0x08 interior split distance */
        uint16 *brush_list;     /* 0x08 leaf brush index list */
    } u8;
    float radius;               /* 0x0c */
    uint16 child[2];            /* 0x10 relative record offsets, in 0x14 units */
} cbrushnode_s;
COD2_ASSERT_SIZE(cbrushnode_s, 0x14);
COD2_ASSERT_OFFSET(cbrushnode_s, contents, 0x04);
COD2_ASSERT_OFFSET(cbrushnode_s, radius, 0x0c);
COD2_ASSERT_OFFSET(cbrushnode_s, child, 0x10);

/* Trace/sweep state. The first 0x18 bytes hold the segment start and end (box corners),
 * then derived quantities. */
typedef struct traceWork_s {
    vec3_t start;               /* 0x00 */
    vec3_t end;                 /* 0x0c */
    vec3_t inv;                 /* 0x18 reciprocal of (start - end) per axis, 0 when degenerate */
    vec3_t delta;               /* 0x24 end - start */
    float length;               /* 0x30 */
    float length_sq;            /* 0x34 */
    vec3_t mid;                 /* 0x38 */
    vec3_t half;                /* 0x44 */
    vec3_t abs_half;            /* 0x50 */
    vec3_t extents;             /* 0x5c */
    vec3_t box2_mins;           /* 0x68 */
    vec3_t box2_maxs;           /* 0x74 */
    uint32 contents;            /* 0x80 */
    int is_point;               /* 0x84 nonzero when extents sum is not zero */
    uint32 flags_88;            /* 0x88 */
    float radius;               /* 0x8c */
    float skin;                 /* 0x90 */
    vec3_t offset;              /* 0x94 */
    byte unk_a0[0x14];          /* 0xa0 */
    const float *world_bounds;  /* 0xb4 pointer to a 6-float world box */
} traceWork_s;
COD2_ASSERT_OFFSET(traceWork_s, end, 0x0c);
COD2_ASSERT_OFFSET(traceWork_s, inv, 0x18);
COD2_ASSERT_OFFSET(traceWork_s, delta, 0x24);
COD2_ASSERT_OFFSET(traceWork_s, length, 0x30);
COD2_ASSERT_OFFSET(traceWork_s, length_sq, 0x34);
COD2_ASSERT_OFFSET(traceWork_s, mid, 0x38);
COD2_ASSERT_OFFSET(traceWork_s, half, 0x44);
COD2_ASSERT_OFFSET(traceWork_s, abs_half, 0x50);
COD2_ASSERT_OFFSET(traceWork_s, extents, 0x5c);
COD2_ASSERT_OFFSET(traceWork_s, box2_mins, 0x68);
COD2_ASSERT_OFFSET(traceWork_s, box2_maxs, 0x74);
COD2_ASSERT_OFFSET(traceWork_s, contents, 0x80);
COD2_ASSERT_OFFSET(traceWork_s, is_point, 0x84);
COD2_ASSERT_OFFSET(traceWork_s, flags_88, 0x88);
COD2_ASSERT_OFFSET(traceWork_s, radius, 0x8c);
COD2_ASSERT_OFFSET(traceWork_s, skin, 0x90);
COD2_ASSERT_OFFSET(traceWork_s, offset, 0x94);
COD2_ASSERT_OFFSET(traceWork_s, world_bounds, 0xb4);
COD2_ASSERT_SIZE(traceWork_s, 0xb8);

/* Axis-aligned box with reciprocal extents (0x24 bytes). */
typedef struct cod2_box_s {
    vec3_t mins;                /* 0x00 */
    vec3_t maxs;                /* 0x0c */
    vec3_t inv;                 /* 0x18 */
} cod2_box_s;
COD2_ASSERT_SIZE(cod2_box_s, 0x24);

/* Area query with a leading contents mask, used by the area sweep. */
typedef struct cod2_area_query_box_s {
    uint32 mask;                /* 0x00 */
    cod2_box_s box;             /* 0x04 */
} cod2_area_query_box_s;
COD2_ASSERT_OFFSET(cod2_area_query_box_s, box, 0x04);
COD2_ASSERT_SIZE(cod2_area_query_box_s, 0x28);

/* Area query used by the entity list collector. */
typedef struct cod2_area_query_s {
    uint32 unk_00;              /* 0x00 */
    const float *mins;          /* 0x04 */
    const float *maxs;          /* 0x08 */
    int *list;                  /* 0x0c */
    int count;                  /* 0x10 */
    int maxcount;               /* 0x14 */
    uint32 mask;                /* 0x18 */
} cod2_area_query_s;
COD2_ASSERT_OFFSET(cod2_area_query_s, mins, 0x04);
COD2_ASSERT_OFFSET(cod2_area_query_s, list, 0x0c);
COD2_ASSERT_OFFSET(cod2_area_query_s, count, 0x10);
COD2_ASSERT_OFFSET(cod2_area_query_s, mask, 0x18);

/* Box with a trailing contents mask, used by the entity-segment test. */
typedef struct SegQueryB {
    cod2_box_s box;             /* 0x00 */
    uint32 mask;                /* 0x24 */
} SegQueryB;
COD2_ASSERT_OFFSET(SegQueryB, mask, 0x24);

/* Area node, stride 0x18. Index 0 is the null node; node 1 is the root. */
typedef struct cod2_area_node_s {
    uint32 brush_contents;      /* 0x00 OR of contents of linked brush boxes */
    uint32 entity_contents;     /* 0x04 OR of contents of linked entities */
    uint16 entity_head;         /* 0x08 1-based entity link index, 0 = empty */
    uint16 brush_head;          /* 0x0a 1-based brush box index, 0 = empty */
    float split_dist;           /* 0x0c */
    uint16 split_axis;          /* 0x10 */
    uint16 parent;              /* 0x12 parent index; free-list link when unused */
    uint16 child[2];            /* 0x14 front, 0x16 back */
} cod2_area_node_s;
COD2_ASSERT_SIZE(cod2_area_node_s, 0x18);
COD2_ASSERT_OFFSET(cod2_area_node_s, split_dist, 0x0c);
COD2_ASSERT_OFFSET(cod2_area_node_s, split_axis, 0x10);
COD2_ASSERT_OFFSET(cod2_area_node_s, parent, 0x12);
COD2_ASSERT_OFFSET(cod2_area_node_s, child, 0x14);

/* Brush box record, stride 0x50, 1-based index. */
typedef struct cm_static_model_s {
    uint16 next;                /* 0x00 */
    byte unk_02[2];             /* 0x02 */
    struct XModel *handle;      /* 0x04 model handle (field was "contents"; XModel_GetContents(handle) yields the mask) */
    byte unk_08[0x30];          /* 0x08 */
    vec3_t mins;                /* 0x38 */
    vec3_t maxs;                /* 0x44 */
} cm_static_model_s;
COD2_ASSERT_SIZE(cm_static_model_s, 0x50);
COD2_ASSERT_OFFSET(cm_static_model_s, handle, 0x04);
COD2_ASSERT_OFFSET(cm_static_model_s, mins, 0x38);
COD2_ASSERT_OFFSET(cm_static_model_s, maxs, 0x44);

#endif
