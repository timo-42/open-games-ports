/*
 * Chunk 003 (0x0805271c..0x080562e6): collision-map (clipMap) loader, brush-node
 * tree builder, aabb-tree trace helpers and small vector/float/endian helpers.
 * Layouts are recovered from consistent field offsets in this chunk only.
 */
#ifndef COD2_CHUNK_003_TYPES_H
#define COD2_CHUNK_003_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include <float.h>

/* ---- BSP file structures (little-endian file, host is little-endian) ---- */

typedef struct bspLump_s {
	uint32 filelen;
	uint32 fileofs;
} bspLump_t;
COD2_ASSERT_SIZE(bspLump_t, 0x8);

/* Header read by CMod_ReadLump is 0x140 bytes = 8 + 39 lumps * 8. */
typedef struct bspHeader_s {
	int ident;
	int version;
	bspLump_t lumps[39];
} bspHeader_t;
COD2_ASSERT_SIZE(bspHeader_t, 0x140);

/* Lump indices used by this chunk (index = (header offset - 8) / 8). */
enum {
	CM_LUMP_MATERIALS = 0,
	CM_LUMP_PLANES = 4,
	CM_LUMP_BRUSHSIDES = 5,
	CM_LUMP_BRUSHES = 6,
	CM_LUMP_NODES = 25,
	CM_LUMP_LEAFS = 26,
	CM_LUMP_LEAFBRUSHES = 27,
	CM_LUMP_LEAFSURFACES = 28,
	CM_LUMP_COLLISIONVERTS = 29,
	CM_LUMP_COLLISIONEDGES = 30,
	CM_LUMP_COLLISIONTRIANGLES = 31,
	CM_LUMP_COLLISIONBORDERS = 32,
	CM_LUMP_COLLISIONPARTITIONS = 33,
	CM_LUMP_COLLISIONAABBTREES = 34,
	CM_LUMP_SUBMODELS = 35,
	CM_LUMP_VISIBILITY = 36,
	CM_LUMP_ENTITIES = 37
};

/* ---- in-memory collision structures ---- */

typedef struct dMaterial_s {
	char name[64];
	int surfaceFlags;	/* 0x40 */
	int contents;		/* 0x44 */
} dMaterial_t;
COD2_ASSERT_OFFSET(dMaterial_t, surfaceFlags, 0x40);
COD2_ASSERT_OFFSET(dMaterial_t, contents, 0x44);
COD2_ASSERT_SIZE(dMaterial_t, 0x48);

/* cplane_s: promoted to cod2_types.h */
COD2_ASSERT_OFFSET(cplane_t, dist, 0xc);
COD2_ASSERT_OFFSET(cplane_t, type, 0x10);
COD2_ASSERT_OFFSET(cplane_t, signbits, 0x11);
COD2_ASSERT_SIZE(cplane_t, 0x14);

typedef struct cbrushside_s {
	cplane_t *plane;
	int materialNum;
} cbrushside_t;
COD2_ASSERT_SIZE(cbrushside_t, 0x8);

typedef struct cbrush_s {
	float mins[3];
	int contents;
	float maxs[3];
	int numsides;
	cbrushside_t *sides;
	short axialMaterialNum[2][3];
} cbrush_t;
COD2_ASSERT_OFFSET(cbrush_t, contents, 0xc);
COD2_ASSERT_OFFSET(cbrush_t, maxs, 0x10);
COD2_ASSERT_OFFSET(cbrush_t, numsides, 0x1c);
COD2_ASSERT_OFFSET(cbrush_t, sides, 0x20);
COD2_ASSERT_OFFSET(cbrush_t, axialMaterialNum, 0x24);
COD2_ASSERT_SIZE(cbrush_t, 0x30);

typedef struct cnode_s {
	cplane_t *plane;
	short children[2];
} cnode_t;
COD2_ASSERT_OFFSET(cnode_t, children, 0x4);
COD2_ASSERT_SIZE(cnode_t, 0x8);

typedef struct cLeaf_s {
	unsigned short firstCollAabbIndex;
	unsigned short collAabbCount;
	int brushContents;
	int terrainContents;
	float mins[3];
	float maxs[3];
	int leafBrushNode;
	short cluster;
	short pad;
} cLeaf_t;
COD2_ASSERT_OFFSET(cLeaf_t, collAabbCount, 0x2);
COD2_ASSERT_OFFSET(cLeaf_t, brushContents, 0x4);
COD2_ASSERT_OFFSET(cLeaf_t, terrainContents, 0x8);
COD2_ASSERT_OFFSET(cLeaf_t, mins, 0xc);
COD2_ASSERT_OFFSET(cLeaf_t, maxs, 0x18);
COD2_ASSERT_OFFSET(cLeaf_t, leafBrushNode, 0x24);
COD2_ASSERT_OFFSET(cLeaf_t, cluster, 0x28);
COD2_ASSERT_SIZE(cLeaf_t, 0x2c);

typedef struct cmodel_s {
	float mins[3];
	float maxs[3];
	float radius;
	cLeaf_t leaf;
} cmodel_t;
COD2_ASSERT_OFFSET(cmodel_t, maxs, 0xc);
COD2_ASSERT_OFFSET(cmodel_t, radius, 0x18);
COD2_ASSERT_OFFSET(cmodel_t, leaf, 0x1c);
COD2_ASSERT_SIZE(cmodel_t, 0x48);

/* Brush-node tree element built by CMod_PartionLeafBrushes_r (0x14 bytes). */
typedef struct cbrushnode_s {
	byte axis;
	byte pad;
	short leafBrushCount;	/* -1 for an interior node */
	unsigned int contents;
	union {
		float dist;			/* interior node: split distance */
		unsigned short *leafBrushes;	/* leaf node: brush index list */
	} u;
	float range;			/* gap to the nearest brush on either side */
	unsigned short children[2];	/* child offsets relative to this node */
} cbrushnode_t;
COD2_ASSERT_OFFSET(cbrushnode_t, leafBrushCount, 0x2);
COD2_ASSERT_OFFSET(cbrushnode_t, contents, 0x4);
COD2_ASSERT_OFFSET(cbrushnode_t, u, 0x8);
COD2_ASSERT_OFFSET(cbrushnode_t, range, 0xc);
COD2_ASSERT_OFFSET(cbrushnode_t, children, 0x10);
COD2_ASSERT_SIZE(cbrushnode_t, 0x14);

typedef struct CollisionAabbTree_s {
	float origin[3];	/* box centre (read by CM_CullBox as origin) */
	float halfSize[3];
	unsigned short materialIndex;
	unsigned short childCount;
	union {
		int firstChildIndex;
		int partitionIndex;
	} u;
} CollisionAabbTree;
COD2_ASSERT_OFFSET(CollisionAabbTree, halfSize, 0xc);
COD2_ASSERT_OFFSET(CollisionAabbTree, materialIndex, 0x18);
COD2_ASSERT_OFFSET(CollisionAabbTree, childCount, 0x1a);
COD2_ASSERT_OFFSET(CollisionAabbTree, u, 0x1c);
COD2_ASSERT_SIZE(CollisionAabbTree, 0x20);

typedef struct CollisionVertex_s {
	float xyz[3];
} CollisionVertex;
COD2_ASSERT_SIZE(CollisionVertex, 0xc);

typedef struct CollisionEdge_s {
	float origin[3];
	float axis[3][3];	/* axis[2] is rescaled by 1/length at load */
} CollisionEdge;
COD2_ASSERT_OFFSET(CollisionEdge, axis, 0xc);
COD2_ASSERT_SIZE(CollisionEdge, 0x30);

typedef struct CollisionTriangle_s {
	float plane[4];
	float svec[4];
	float tvec[4];
	int verts[3];		/* indices into cm.verts, negative = none */
	int edges[3];		/* indices into cm.edges, negative = none */
} CollisionTriangle;
COD2_ASSERT_OFFSET(CollisionTriangle, svec, 0x10);
COD2_ASSERT_OFFSET(CollisionTriangle, tvec, 0x20);
COD2_ASSERT_OFFSET(CollisionTriangle, verts, 0x30);
COD2_ASSERT_OFFSET(CollisionTriangle, edges, 0x3c);
COD2_ASSERT_SIZE(CollisionTriangle, 0x48);

typedef struct CollisionBorder_s {
	float distEq[3];
	float zBase;
	float zSlope;
	float start;
	float length;
} CollisionBorder;
COD2_ASSERT_SIZE(CollisionBorder, 0x1c);

typedef struct CollisionPartition_s {
	byte triCount;
	byte borderCount;
	byte pad[2];
	CollisionTriangle *triangles;
	CollisionBorder *borders;
} CollisionPartition;
COD2_ASSERT_OFFSET(CollisionPartition, borderCount, 0x1);
COD2_ASSERT_OFFSET(CollisionPartition, triangles, 0x4);
COD2_ASSERT_OFFSET(CollisionPartition, borders, 0x8);
COD2_ASSERT_SIZE(CollisionPartition, 0xc);

/* Global collision map at 0x08188dc0 (0x110 bytes, zeroed by CM_LoadMapFromBsp). */
typedef struct clipMap_s {
	char *name;				/* 0x00 */
	int unknown_004[2];
	int numMaterials;			/* 0x0c */
	dMaterial_t *materials;			/* 0x10, points one element past the allocation start */
	int numBrushSides;			/* 0x14 */
	cbrushside_t *brushSides;		/* 0x18 */
	int numNodes;				/* 0x1c */
	cnode_t *nodes;				/* 0x20 */
	int numLeafs;				/* 0x24 */
	cLeaf_t *leafs;				/* 0x28 */
	int numBrushNodes;			/* 0x2c */
	cbrushnode_t *brushNodes;		/* 0x30 */
	int numLeafBrushes;			/* 0x34 */
	unsigned short *leafBrushes;		/* 0x38 */
	int numLeafSurfaces;			/* 0x3c */
	int *leafSurfaces;			/* 0x40 */
	int numVerts;				/* 0x44 */
	CollisionVertex *verts;			/* 0x48 */
	int numEdges;				/* 0x4c */
	CollisionEdge *edges;			/* 0x50 */
	int numTriangles;			/* 0x54 */
	CollisionTriangle *triangles;		/* 0x58 */
	int numBorders;				/* 0x5c */
	CollisionBorder *borders;		/* 0x60 */
	int numPartitions;			/* 0x64 */
	CollisionPartition *partitions;		/* 0x68 */
	int numAabbTrees;			/* 0x6c */
	CollisionAabbTree *aabbTrees;		/* 0x70 */
	int numSubModels;			/* 0x74 */
	cmodel_t *cmodels;			/* 0x78 */
	unsigned short numBrushes;		/* 0x7c */
	unsigned short pad_07e;
	cbrush_t *brushes;			/* 0x80 */
	int numClusters;			/* 0x84 */
	int clusterBytes;			/* 0x88 */
	byte *visibility;			/* 0x8c */
	int vised;				/* 0x90 */
	int numEntityChars;			/* 0x94 */
	char *entityString;			/* 0x98 */
	cbrush_t *box_brush;			/* 0x9c */
	int unknown_0a0[7];
	cLeaf_t box_leaf;			/* 0xbc */
	int unknown_0e8[9];
	int checksum;				/* 0x10c */
} clipMap_t;
COD2_ASSERT_OFFSET(clipMap_t, numMaterials, 0xc);
COD2_ASSERT_OFFSET(clipMap_t, materials, 0x10);
COD2_ASSERT_OFFSET(clipMap_t, numBrushSides, 0x14);
COD2_ASSERT_OFFSET(clipMap_t, numNodes, 0x1c);
COD2_ASSERT_OFFSET(clipMap_t, numLeafs, 0x24);
COD2_ASSERT_OFFSET(clipMap_t, numBrushNodes, 0x2c);
COD2_ASSERT_OFFSET(clipMap_t, numLeafBrushes, 0x34);
COD2_ASSERT_OFFSET(clipMap_t, numLeafSurfaces, 0x3c);
COD2_ASSERT_OFFSET(clipMap_t, numVerts, 0x44);
COD2_ASSERT_OFFSET(clipMap_t, numEdges, 0x4c);
COD2_ASSERT_OFFSET(clipMap_t, numTriangles, 0x54);
COD2_ASSERT_OFFSET(clipMap_t, numBorders, 0x5c);
COD2_ASSERT_OFFSET(clipMap_t, numPartitions, 0x64);
COD2_ASSERT_OFFSET(clipMap_t, numAabbTrees, 0x6c);
COD2_ASSERT_OFFSET(clipMap_t, numSubModels, 0x74);
COD2_ASSERT_OFFSET(clipMap_t, cmodels, 0x78);
COD2_ASSERT_OFFSET(clipMap_t, numBrushes, 0x7c);
COD2_ASSERT_OFFSET(clipMap_t, brushes, 0x80);
COD2_ASSERT_OFFSET(clipMap_t, numClusters, 0x84);
COD2_ASSERT_OFFSET(clipMap_t, vised, 0x90);
COD2_ASSERT_OFFSET(clipMap_t, entityString, 0x98);
COD2_ASSERT_OFFSET(clipMap_t, box_brush, 0x9c);
COD2_ASSERT_OFFSET(clipMap_t, box_leaf, 0xbc);
COD2_ASSERT_OFFSET(clipMap_t, checksum, 0x10c);
COD2_ASSERT_SIZE(clipMap_t, 0x110);

/* Trace work state; only fields read by this chunk are named. */
typedef struct traceWork_s {
	float start[3];			/* 0x00 */
	float unknown_00c[6];
	float delta[3];			/* 0x24 */
	float unknown_030[2];
	float midpoint[3];		/* 0x38 */
	float halfDelta[3];		/* 0x44 */
	float halfDeltaAbs[3];		/* 0x50 */
	float size[3];			/* 0x5c */
	float unknown_068[6];
	unsigned int contents;		/* 0x80 */
	int isPoint;			/* 0x84 */
	int axialCullOnly;		/* 0x88 */
	float radius;			/* 0x8c */
	float offsetZ;			/* 0x90 */
	float unknown_094[3];
	short checkcount;		/* 0xa0 */
	byte unknown_0a2[10];
	unsigned short *partitionChecks;	/* 0xac */
} traceWork_t;
COD2_ASSERT_OFFSET(traceWork_t, delta, 0x24);
COD2_ASSERT_OFFSET(traceWork_t, midpoint, 0x38);
COD2_ASSERT_OFFSET(traceWork_t, halfDelta, 0x44);
COD2_ASSERT_OFFSET(traceWork_t, halfDeltaAbs, 0x50);
COD2_ASSERT_OFFSET(traceWork_t, size, 0x5c);
COD2_ASSERT_OFFSET(traceWork_t, contents, 0x80);
COD2_ASSERT_OFFSET(traceWork_t, isPoint, 0x84);
COD2_ASSERT_OFFSET(traceWork_t, axialCullOnly, 0x88);
COD2_ASSERT_OFFSET(traceWork_t, radius, 0x8c);
COD2_ASSERT_OFFSET(traceWork_t, offsetZ, 0x90);
COD2_ASSERT_OFFSET(traceWork_t, checkcount, 0xa0);
COD2_ASSERT_OFFSET(traceWork_t, partitionChecks, 0xac);

#endif
