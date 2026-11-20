#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_003_types.h"

/* Chunk 003: collision-map (cm) loader and aabb-tree trace helpers. */

/* Node index arithmetic as compiled: ((byte diff) >> 2) * inverse(5) == diff / 0x14. */
static unsigned int CM_BrushNodeDiff(const cbrushnode_t *a, const cbrushnode_t *b)
{
	return (unsigned int)(((int)((const char *)a - (const char *)b) >> 2) * 0xCCCCCCCDu);
}

// 0x0805271c  size=61  FUN_0805271c  status=translated  naming=inferred
// Copies the mins/maxs of the cmodel selected by a handle (Q3 CM_ModelBounds shape: callee 0x08059358 indexes g_08188dc0_clipmap.cmodels with 0x48 stride).
void CM_ModelBounds(int handle, float *mins, float *maxs)
{
	cmodel_t *model;

	model = cm_model_get(handle);
	VectorCopy_080527aa((const uint32 *)model->mins, (uint32 *)mins);
	VectorCopy_080527aa((const uint32 *)model->maxs, (uint32 *)maxs);
}

// 0x0805275a  size=19  FUN_0805275a  status=translated  naming=unknown
// Allocation wrapper forwarding only the size to Hunk_Alloc; call sites also push a tag string and a number that the callee ignores.
void *CM_HunkAlloc(int size, ...)
{
	return Hunk_Alloc(size);
}

// 0x0805276e  size=5  FUN_0805276e  status=translated  naming=unknown
// Empty function (ret only); called before the temp-buffer reset in CMod_LoadBrushRelated.
void CM_NoOp_BeforeTempBufferReset(void)
{
}

// 0x08052774  size=5  FUN_08052774  status=translated  naming=unknown
// Empty function (ret only); called before the leaf brush tree is built in CMod_BuildLeafBrushTree.
void CM_NoOp_BeforeLeafBrushTreeBuild(void)
{
}

// 0x0805277a  size=19  FUN_0805277a  status=translated  naming=unknown
// Wrapper forwarding the size to Hunk_AllocateTempMemoryHigh (which bumps a 16-byte aligned counter and errors when the pool is exhausted); extra tag argument ignored.
void *CM_HunkAllocTemp(int size, ...)
{
	return (void *)Hunk_AllocateTempMemoryHigh(size);
}

// 0x0805278e  size=13  FUN_0805278e  status=translated  naming=unknown
// Wrapper for Hunk_ClearTempMemory.
void CM_HunkClearLow(void)
{
	Hunk_ClearTempMemory();
}

// 0x0805279c  size=13  FUN_0805279c  status=translated  naming=unknown
// Wrapper for Hunk_ClearTempMemoryHigh (resets the temp counter to its saved value); paired with CM_HunkAllocTemp use.
void CM_HunkClearTemp(void)
{
	Hunk_ClearTempMemoryHigh();
}

// 0x080527aa  size=47  FUN_080527aa  status=translated  naming=inferred
// Copies three 32-bit words (VectorCopy shape) without going through the FPU.
void VectorCopy_080527aa(const uint32 *src, uint32 *dst)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

// 0x080527da  size=8  FUN_080527da  status=translated  naming=unknown
// Returns its argument unchanged.
int Identity_080527da(int v)
{
	return v;
}

// 0x080527e4  size=327  FUN_080527e4  status=translated  naming=observed
// String "CMod_LoadMaterials" is the allocation tag and the 0x48-byte records are copied from the BSP lump.
void CMod_LoadMaterials(const bspLump_t *l)
{
	byte *src;
	unsigned int count;
	byte *mem;
	dMaterial_t *m;
	int i;

	src = (byte *)(l->fileofs + cmod_base);
	if (l->filelen % 0x48 != 0) {
		Com_Error(1, "\025CMod_LoadMaterials: funny lump size");
	}
	count = l->filelen / 0x48;
	if (count == 0) {
		Com_Error(1, "\025Map with no materials");
	}
	mem = (byte *)CM_HunkAlloc(count * 0x48 + 0x48, "CMod_LoadMaterials", 0x15);
	g_08188dc0_clipmap.materials = (dMaterial_t *)(mem + 0x48);
	g_08188dc0_clipmap.numMaterials = count;
	Com_Memcpy(g_08188dc0_clipmap.materials, src, count * 0x48);
	if (CMod_LittleLong(1) != 1) {
		m = g_08188dc0_clipmap.materials;
		for (i = 0; i < (int)count; i++) {
			m->contents = CMod_LittleLong(m->contents);
			m->surfaceFlags = CMod_LittleLong(m->surfaceFlags);
			m++;
		}
	}
}

// 0x0805292c  size=64  FUN_0805292c  status=translated  naming=inferred
// Takes 0x14 bytes from the TempMemory_Reserve bump buffer, zeroes them and sets the split-distance slot to -FLT_MAX: a blank cbrushnode_t.
cbrushnode_t *CM_AllocBrushNode(void)
{
	cbrushnode_t *node;

	node = (cbrushnode_t *)TempMemory_Reserve(0x14);
	memset(node, 0, 0x14);
	node->u.dist = -3.4028235e+38f;
	return node;
}

// 0x0805296c  size=582  FUN_0805296c  status=translated  naming=observed
// String "CMod_LoadSubmodels" tags the allocation and the error messages.
void CMod_LoadSubmodels(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	int i;
	int axis;
	cmodel_t *model;
	float absMax[3];
	float absMin[3];
	float extent[3];
	unsigned int v;

	file = cmod_base + l->fileofs;
	if (l->filelen % 0x30 != 0) {
		Com_Error(1, "\025CMod_LoadSubmodels: funny lump size");
	}
	count = l->filelen / 0x30;
	if (count == 0) {
		Com_Error(1, "\025Map with no models");
	}
	g_08188dc0_clipmap.cmodels = (cmodel_t *)CM_HunkAlloc(count * 0x48, "CMod_LoadSubmodels", 0x16);
	g_08188dc0_clipmap.numSubModels = count;
	if (0x3ff < (int)count) {
		Com_Error(1, "\025MAX_SUBMODELS exceeded");
	}
	for (i = 0; i < (int)count; i++) {
		model = &g_08188dc0_clipmap.cmodels[i];
		for (axis = 0; axis < 3; axis++) {
			model->mins[axis] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + axis * 4))) - 1;
			model->maxs[axis] = 1 + Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + axis * 4 + 0xc)));
			absMax[axis] = Float_Abs_080551fc(model->maxs[axis]);
			absMin[axis] = Float_Abs_080551fc(model->mins[axis]);
			extent[axis] = Float_Max(absMin[axis], absMax[axis]);
		}
		model->radius = VectorLength(extent);
		if (i != 0) {
			v = CMod_LittleLong(*(const int *)(file + 0x24));
			model->leaf.collAabbCount = (unsigned short)v;
			if (model->leaf.collAabbCount != v) {
				Com_Error(1, "\025CMod_LoadSubmodels: collAabbCount exceeded");
			}
			v = CMod_LittleLong(*(const int *)(file + 0x20));
			model->leaf.firstCollAabbIndex = (unsigned short)v;
			if (model->leaf.firstCollAabbIndex != v) {
				Com_Error(1, "\025CMod_LoadSubmodels: firstCollAabbIndex exceeded");
			}
		}
		file += 0x30;
	}
}

// 0x08052bb2  size=356  FUN_08052bb2  status=translated  naming=unknown
// Scores a candidate split plane (*dist on axis) for a brush list: counts brushes fully on each side, reports the midpoint of the closest facing faces back through *dist and returns min(gap to bounds) * min(count).
float CMod_ScoreBrushSplit(const unsigned short *brushes, int numBrushes, int axis,
			   const float *mins, const float *maxs, float *dist)
{
	int countHigh;
	int countLow;
	float lowFaceMax;
	float highFaceMin;
	int i;
	const cbrush_t *b;
	int count;
	float score;

	countHigh = -1;
	countLow = -1;
	lowFaceMax = -3.4028235e+38f;
	highFaceMin = 3.4028235e+38f;
	for (i = 0; i < numBrushes; i++) {
		b = &g_08188dc0_clipmap.brushes[brushes[i]];
		if (*dist <= b->mins[axis]) {
			countHigh = countHigh + 1;
			if (b->mins[axis] < highFaceMin) {
				highFaceMin = b->mins[axis];
			}
		} else if (b->maxs[axis] <= *dist) {
			countLow = countLow + 1;
			if (lowFaceMax < b->maxs[axis]) {
				lowFaceMax = b->maxs[axis];
			}
		}
	}
	count = Int_Min(countLow, countHigh);
	*dist = (lowFaceMax + highFaceMin) * 0.5;
	if (count < 1) {
		score = 0.0f;
	} else {
		score = Float_Min(highFaceMin - mins[axis], maxs[axis] - lowFaceMax) * (float)count;
	}
	return score;
}

// 0x08052d16  size=1217  FUN_08052d16  status=translated  naming=observed
// String "CMod_PartionLeafBrushes_r" (sic) is the temp-allocation tag; recursive split of a brush index list into a cbrushnode_t tree.
cbrushnode_t *CMod_PartionLeafBrushes_r(unsigned short *brushes, int numBrushes,
					const float *mins, const float *maxs)
{
	cbrushnode_t *node;
	float bestScore;
	unsigned int bestAxis;
	float bestDist;
	byte axis;
	int i;
	unsigned int idx;
	cbrush_t *b;
	float dist;
	float score;
	int outCount;
	size_t listBytes;
	unsigned short *tmp;
	cbrushnode_t *straddle;
	cbrushnode_t *child;
	float gap;
	int side;
	float childMins[3];
	float childMaxs[3];
	unsigned int childOfs;

	node = CM_AllocBrushNode();
	bestScore = 0.0f;
	bestAxis = 0xffffffff;
	bestDist = 0.0f;
	for (axis = 0; axis < 3; axis++) {
		for (i = 0; i < numBrushes; i++) {
			idx = brushes[i];
			b = &g_08188dc0_clipmap.brushes[idx];
			dist = b->mins[axis];
			score = CMod_ScoreBrushSplit(brushes, numBrushes, axis, mins, maxs, &dist);
			if (bestScore < score) {
				bestAxis = axis;
				bestDist = dist;
				bestScore = score;
			}
			dist = b->maxs[axis];
			score = CMod_ScoreBrushSplit(brushes, numBrushes, axis, mins, maxs, &dist);
			if (bestScore < score) {
				bestAxis = axis;
				bestDist = dist;
				bestScore = score;
			}
		}
	}
	if ((int)bestAxis < 0) {
		node->leafBrushCount = (short)numBrushes;
		if (node->leafBrushCount != numBrushes) {
			Com_Error(1, "\025CMod_PartionLeafBrushes_r: leafBrushCount exceeded");
		}
		for (i = 0; i < numBrushes; i++) {
			node->contents = g_08188dc0_clipmap.brushes[brushes[i]].contents | node->contents;
		}
		node->u.leafBrushes = brushes;
		return node;
	}
	listBytes = numBrushes * 2;
	tmp = (unsigned short *)CM_HunkAllocTemp(listBytes, "CMod_PartionLeafBrushes_r");
	memcpy(tmp, brushes, listBytes);
	outCount = 0;
	for (i = 0; i < numBrushes; i++) {
		idx = tmp[i];
		b = &g_08188dc0_clipmap.brushes[idx];
		if (b->mins[bestAxis] < bestDist && bestDist < b->maxs[bestAxis]) {
			brushes[outCount] = (unsigned short)idx;
			outCount = outCount + 1;
		}
	}
	if (outCount != 0) {
		straddle = CMod_PartionLeafBrushes_r(brushes, outCount, mins, maxs);
		node->leafBrushCount = (short)0xffff;
		node->contents = straddle->contents;
		brushes = brushes + outCount;
	}
	gap = 3.4028235e+38f;
	node->axis = (byte)bestAxis;
	node->u.dist = bestDist;
	for (side = 0; side < 2; side++) {
		outCount = 0;
		for (i = 0; i < numBrushes; i++) {
			idx = tmp[i];
			b = &g_08188dc0_clipmap.brushes[idx];
			if (side == 0) {
				if (bestDist <= b->mins[bestAxis]) {
					gap = Float_Min(gap, b->mins[bestAxis] - bestDist);
					brushes[outCount] = (short)idx;
					outCount = outCount + 1;
				}
			} else if (b->maxs[bestAxis] <= bestDist) {
				gap = Float_Min(gap, bestDist - b->maxs[bestAxis]);
				brushes[outCount] = (short)idx;
				outCount = outCount + 1;
			}
		}
		VectorCopy_080552b2((const uint32 *)mins, (uint32 *)childMins);
		VectorCopy_080552b2((const uint32 *)maxs, (uint32 *)childMaxs);
		if (side == 0) {
			childMins[bestAxis] = bestDist + gap;
		} else {
			childMaxs[bestAxis] = bestDist - gap;
		}
		child = CMod_PartionLeafBrushes_r(brushes, outCount, childMins, childMaxs);
		childOfs = CM_BrushNodeDiff(child, node);
		node->children[side] = (unsigned short)childOfs;
		if (node->children[side] != childOfs) {
			Com_Error(1, "\025CMod_PartionLeafBrushes_r: child exceeded");
		}
		node->contents = child->contents | node->contents;
		brushes = brushes + outCount;
	}
	node->range = gap;
	return node;
}

// 0x080531d8  size=458  FUN_080531d8  status=translated  naming=inferred
// Computes the padded bounds of a brush index list into leaf->mins/maxs, builds its brush-node tree and stores the node index in leaf->leafBrushNode. Named from its role between CMod_LoadLeafBrushNodes and CMod_PartionLeafBrushes_r.
void CMod_BuildLeafBrushTree(unsigned short *leafBrushes, int numLeafBrushes, cLeaf_t *leaf)
{
	float mins[3];
	float maxs[3];
	int i;
	int axis;
	const cbrush_t *b;
	cbrushnode_t *root;

	if (numLeafBrushes != 0) {
		Vec3_Set_0805528e((uint32 *)mins, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff);
		Vec3_Set_0805528e((uint32 *)maxs, 0xff7fffff, 0xff7fffff, 0xff7fffff);
		for (i = 0; i < numLeafBrushes; i++) {
			b = &g_08188dc0_clipmap.brushes[leafBrushes[i]];
			for (axis = 0; axis < 3; axis++) {
				if (b->mins[axis] < mins[axis]) {
					mins[axis] = b->mins[axis];
				}
				if (maxs[axis] < b->maxs[axis]) {
					maxs[axis] = b->maxs[axis];
				}
			}
		}
		VectorCopy_080552b2((const uint32 *)mins, (uint32 *)leaf->mins);
		VectorCopy_080552b2((const uint32 *)maxs, (uint32 *)leaf->maxs);
		for (axis = 0; axis < 3; axis++) {
			leaf->mins[axis] = leaf->mins[axis] - 0.125;
			leaf->maxs[axis] = leaf->maxs[axis] + 0.125;
		}
		CM_NoOp_BeforeLeafBrushTreeBuild();
		root = CMod_PartionLeafBrushes_r(leafBrushes, numLeafBrushes, mins, maxs);
		leaf->leafBrushNode = CM_BrushNodeDiff(root, g_08188dc0_clipmap.brushNodes);
		CM_HunkClearTemp();
	}
}

// 0x080533a2  size=98  FUN_080533a2  status=translated  naming=unknown
// OR of the material contents of every aabb tree referenced by the leaf's (firstCollAabbIndex, collAabbCount) range.
unsigned int CMod_LeafTerrainContents(const cLeaf_t *leaf)
{
	int i;
	unsigned int contents;

	contents = 0;
	for (i = 0; i < (int)leaf->collAabbCount; i++) {
		contents = contents |
			   g_08188dc0_clipmap.materials[g_08188dc0_clipmap.aabbTrees[leaf->firstCollAabbIndex + i].materialIndex].contents;
	}
	return contents;
}

// 0x08053404  size=339  FUN_08053404  status=translated  naming=observed
// String "CMod_LoadSubmodelBrushNodes" tags the allocation; builds per-submodel (index >= 1) brush lists and trees.
void CMod_LoadSubmodelBrushNodes(const bspLump_t *l)
{
	const byte *file;
	int i;
	cmodel_t *model;
	int count;
	unsigned short *list;
	unsigned int contents;
	int j;
	int firstBrush;
	unsigned int brushIndex;

	file = cmod_base + l->fileofs;
	for (i = 0; i < g_08188dc0_clipmap.numSubModels; i++) {
		if (i != 0) {
			model = &g_08188dc0_clipmap.cmodels[i];
			count = CMod_LittleLong(*(const int *)(file + 0x2c));
			list = (unsigned short *)CM_HunkAlloc(count * 2, "CMod_LoadSubmodelBrushNodes", 0x16);
			contents = 0;
			for (j = 0; j < count; j++) {
				firstBrush = CMod_LittleLong(*(const int *)(file + 0x28));
				brushIndex = firstBrush + j;
				list[j] = (unsigned short)brushIndex;
				if (list[j] != brushIndex) {
					Com_Error(1, "\025CMod_LoadSubmodelBrushNodes: leafBrushes exceeded");
				}
				contents = contents | g_08188dc0_clipmap.brushes[brushIndex].contents;
			}
			model->leaf.brushContents = contents;
			model->leaf.terrainContents = CMod_LeafTerrainContents(&model->leaf);
			CMod_BuildLeafBrushTree(list, count, &model->leaf);
		}
		file += 0x30;
	}
}

// 0x08053558  size=355  FUN_08053558  status=translated  naming=observed
// String "CMod_LoadNodes" tags the allocation; 0x24-byte file nodes become 8-byte cnode_t (plane pointer + two short children).
void CMod_LoadNodes(const bspLump_t *l)
{
	const int *file;
	unsigned int count;
	cnode_t *node;
	int i;
	int j;
	int v;

	file = (const int *)(l->fileofs + cmod_base);
	if (l->filelen % 0x24 != 0) {
		Com_Error(1, "\025MOD_LoadBmodel: funny lump size");
	}
	count = l->filelen / 0x24;
	if (count == 0) {
		Com_Error(1, "\025Map has no nodes");
	}
	g_08188dc0_clipmap.nodes = (cnode_t *)CM_HunkAlloc(count << 3, "CMod_LoadNodes", 0x15);
	g_08188dc0_clipmap.numNodes = count;
	node = g_08188dc0_clipmap.nodes;
	for (i = 0; i < (int)count; i++) {
		v = CMod_LittleLong(file[0]);
		node->plane = (cplane_t *)(v * 0x14 + (int)cm_planes);
		for (j = 0; j < 2; j++) {
			v = CMod_LittleLong(file[j + 1]);
			node->children[j] = (short)v;
			if (node->children[j] != v) {
				Com_Error(1, "\025CMod_LoadNodes: children exceeded");
			}
		}
		node++;
		file += 9;
	}
}

// 0x080536bc  size=1027  FUN_080536bc  status=translated  naming=observed
// Strings "CMod_LoadBrushes" / "CMod_LoadBrushSides" tag the allocations. File brushes are (short numSides, short materialNum); the side lump holds 6 axial (float, material) pairs per brush followed by the extra sides.
void CMod_LoadBrushes(const bspLump_t *brushLump, const bspLump_t *sideLump)
{
	const short *rec;
	unsigned int numBrushesFile;
	const uint32 *sf;
	int numSides;
	cbrushside_t *sideCursor;
	cbrush_t *brush;
	int i;
	int axis;
	int k;
	int mat;
	short sv;
	int stray;

	rec = (const short *)(brushLump->fileofs + cmod_base);
	if ((brushLump->filelen & 3) != 0) {
		Com_Error(1, "\025CMod_LoadBrushes: funny lump size");
	}
	numBrushesFile = brushLump->filelen >> 2;
	sf = (const uint32 *)(sideLump->fileofs + cmod_base);
	if ((sideLump->filelen & 7) != 0) {
		Com_Error(1, "\025CMod_LoadBrushes: funny lump size");
	}
	numSides = (sideLump->filelen >> 3) + numBrushesFile * -6;
	if (numSides < 0) {
		Com_Error(1, "\025CMod_LoadBrushes: bad side count");
	}
	if (numSides == 0) {
		g_08188dc0_clipmap.brushSides = 0;
	} else {
		g_08188dc0_clipmap.brushSides = (cbrushside_t *)CM_HunkAlloc(numSides * 8, "CMod_LoadBrushSides", 0x16);
	}
	sideCursor = g_08188dc0_clipmap.brushSides;
	g_08188dc0_clipmap.numBrushSides = numSides;
	g_08188dc0_clipmap.brushes = (cbrush_t *)CM_HunkAlloc((numBrushesFile + 1) * 0x30, "CMod_LoadBrushes", 0x16);
	g_08188dc0_clipmap.numBrushes = (unsigned short)numBrushesFile;
	if ((numBrushesFile & 0xffff) != numBrushesFile) {
		Com_Error(1, "\025CMod_LoadBrushes: g_08188dc0_clipmap.numBrushes exceeded");
	}
	brush = g_08188dc0_clipmap.brushes;
	for (i = 0; i < (int)numBrushesFile; i++) {
		sv = LittleShort(*rec);
		brush->numsides = sv + -6;
		if (brush->numsides < 0) {
			Com_Error(1, "\025CMod_LoadBrushes: brush has less than 6 sides");
		}
		if (brush->numsides == 0) {
			brush->sides = 0;
		} else {
			brush->sides = sideCursor;
		}
		for (axis = 0; axis < 3; axis++) {
			for (k = 0; k < 2; k++) {
				if (k == 0) {
					brush->mins[axis] = Swap_BitsToFloat(Swap_FloatBits(sf));
				} else {
					brush->maxs[axis] = Swap_BitsToFloat(Swap_FloatBits(sf));
				}
				mat = CMod_LittleLong(sf[1]);
				if (mat < 0 || g_08188dc0_clipmap.numMaterials <= mat) {
					Com_Error(1, "\025CMod_LoadBrushes: bad materialNum: %i", mat);
				}
				brush->axialMaterialNum[k][axis] = (short)mat;
				if (brush->axialMaterialNum[k][axis] != mat) {
					Com_Error(1, "\025CMod_LoadBrushes: axialMaterialNum exceeded");
				}
				sf += 2;
			}
		}
		for (axis = 0; axis < brush->numsides; axis++) {
			sideCursor->plane = (cplane_t *)(CMod_LittleLong(sf[0]) * 0x14 + (int)cm_planes);
			sideCursor->materialNum = CMod_LittleLong(sf[1]);
			if (sideCursor->materialNum < 0 || g_08188dc0_clipmap.numMaterials <= sideCursor->materialNum) {
				stray = sideCursor->materialNum;
				Com_Error(1, "\025CMod_LoadBrushes: bad materialNum: %i", stray);
			}
			sf += 2;
			sideCursor++;
		}
		mat = (int)LittleShort(rec[1]);
		if (mat < 0 || g_08188dc0_clipmap.numMaterials <= mat) {
			Com_Error(1, "\025CMod_LoadBrushes: bad materialNum: %i", mat);
		}
		brush->contents = g_08188dc0_clipmap.materials[mat].contents & 0xdffffffb;
		brush++;
		rec += 2;
	}
}

// 0x08053ac0  size=451  FUN_08053ac0  status=translated  naming=observed
// String "CMod_LoadLeafs" tags the allocation; 0x24-byte file leaves become 0x2c-byte cLeaf_t, cluster only read when loadClusters is set.
void CMod_LoadLeafs(const bspLump_t *l, char loadClusters)
{
	const int *file;
	unsigned int count;
	cLeaf_t *leaf;
	int cluster;
	int i;
	unsigned int v;

	file = (const int *)(l->fileofs + cmod_base);
	if (l->filelen % 0x24 != 0) {
		Com_Error(1, "\025CMod_LoadLeafs: funny lump size");
	}
	count = l->filelen / 0x24;
	if (count == 0) {
		Com_Error(1, "\025Map with no leafs");
	}
	g_08188dc0_clipmap.leafs = (cLeaf_t *)CM_HunkAlloc(count * 0x2c, "CMod_LoadLeafs", 0x15);
	cluster = 0;
	g_08188dc0_clipmap.numLeafs = count;
	leaf = g_08188dc0_clipmap.leafs;
	for (i = 0; i < (int)count; i++) {
		if (loadClusters != 0) {
			cluster = CMod_LittleLong(file[0]);
			leaf->cluster = (short)cluster;
			if (leaf->cluster != cluster) {
				Com_Error(1, "\025CMod_LoadLeafs: cluster exceeded");
			}
		}
		v = CMod_LittleLong(file[2]);
		leaf->firstCollAabbIndex = (unsigned short)v;
		if (leaf->firstCollAabbIndex != v) {
			Com_Error(1, "\025CMod_LoadLeafs: firstCollAabbIndex exceeded");
		}
		v = CMod_LittleLong(file[3]);
		leaf->collAabbCount = (unsigned short)v;
		if (leaf->collAabbCount != v) {
			Com_Error(1, "\025CMod_LoadLeafs: collAabbCount exceeded");
		}
		if (loadClusters != 0 && g_08188dc0_clipmap.numClusters <= cluster) {
			g_08188dc0_clipmap.numClusters = cluster + 1;
		}
		file += 9;
		leaf++;
	}
}

// 0x08053c84  size=254  FUN_08053c84  status=translated  naming=inferred
// For every leaf: ORs the contents of its leaf brushes, computes terrain contents and builds its brush tree from the leaf-brush slice (firstLeafBrush at file+0x10, count at +0x14).
void CMod_LoadLeafBrushNodes(const bspLump_t *leafLump)
{
	const byte *file;
	cLeaf_t *leaf;
	int i;
	int count;
	int first;
	unsigned int contents;
	int j;

	file = (const byte *)(leafLump->fileofs + cmod_base);
	leaf = g_08188dc0_clipmap.leafs;
	for (i = 0; i < g_08188dc0_clipmap.numLeafs; i++) {
		count = CMod_LittleLong(*(const int *)(file + 0x14));
		first = CMod_LittleLong(*(const int *)(file + 0x10));
		contents = 0;
		for (j = 0; j < count; j++) {
			contents = contents | g_08188dc0_clipmap.brushes[g_08188dc0_clipmap.leafBrushes[j + first]].contents;
		}
		leaf->brushContents = contents;
		leaf->terrainContents = CMod_LeafTerrainContents(leaf);
		CMod_BuildLeafBrushTree(g_08188dc0_clipmap.leafBrushes + first, count, leaf);
		file += 0x24;
		leaf++;
	}
}

// 0x08053d82  size=438  FUN_08053d82  status=translated  naming=observed
// String "CMod_LoadPlanes" tags the allocation; file planes (normal[3], dist) get a type byte (axis index or 3) and signbits.
void CMod_LoadPlanes(byte *base, const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	cplane_t *plane;
	int i;
	int j;
	byte signbits;
	byte type;

	file = base + l->fileofs;
	if ((l->filelen & 0xf) != 0) {
		Com_Error(1, "\025MOD_LoadBmodel: funny lump size");
	}
	count = l->filelen >> 4;
	if (count == 0) {
		Com_Error(1, "\025Map with no planes");
	}
	cm_planes = (cplane_t *)CM_HunkAlloc(count * 0x14, "CMod_LoadPlanes", 0x15);
	cm_numPlanes = count;
	plane = cm_planes;
	for (i = 0; i < (int)count; i++) {
		signbits = 0;
		for (j = 0; j < 3; j++) {
			plane->normal[j] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + j * 4)));
			if (plane->normal[j] < 0.0) {
				signbits = signbits | (byte)(1 << ((byte)j & 0x1f));
			}
		}
		plane->dist = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + 0xc)));
		if (plane->normal[0] != 1.0) {
			if (plane->normal[1] != 1.0) {
				if (plane->normal[2] == 1.0) {
					type = 2;
				} else {
					type = 3;
				}
			} else {
				type = 1;
			}
		} else {
			type = 0;
		}
		plane->type = type;
		plane->signbits = signbits;
		file += 0x10;
		plane++;
	}
}

// 0x08053f38  size=217  FUN_08053f38  status=translated  naming=observed
// String "CMod_LoadLeafBrushes" tags the allocation; ints narrowed to unsigned short brush indices (one spare element allocated).
void CMod_LoadLeafBrushes(const bspLump_t *l)
{
	const int *file;
	unsigned int count;
	unsigned short *out;
	int i;
	unsigned int v;

	file = (const int *)(l->fileofs + cmod_base);
	if ((l->filelen & 3) != 0) {
		Com_Error(1, "\025CMod_LoadLeafBrushes: funny lump size");
	}
	count = l->filelen >> 2;
	g_08188dc0_clipmap.leafBrushes = (unsigned short *)CM_HunkAlloc(count * 2 + 2, "CMod_LoadLeafBrushes", 0x16);
	g_08188dc0_clipmap.numLeafBrushes = count;
	out = g_08188dc0_clipmap.leafBrushes;
	for (i = 0; i < (int)count; i++) {
		v = CMod_LittleLong(*file);
		*out = (unsigned short)v;
		if (*out != v) {
			Com_Error(1, "\025CMod_LoadLeafBrushes: brushIndex exceeded");
		}
		file++;
		out++;
	}
}

// 0x08054012  size=179  FUN_08054012  status=translated  naming=observed
// String "CMod_LoadLeafSurfaces" tags the allocation; byte-swapped int array copy.
void CMod_LoadLeafSurfaces(const bspLump_t *l)
{
	const int *file;
	unsigned int count;
	int *out;
	int i;

	file = (const int *)(l->fileofs + cmod_base);
	if ((l->filelen & 3) != 0) {
		Com_Error(1, "\025CMod_LoadLeafSurfaces: funny lump size");
	}
	count = l->filelen >> 2;
	g_08188dc0_clipmap.leafSurfaces = (int *)CM_HunkAlloc(count << 2, "CMod_LoadLeafSurfaces", 0x18);
	g_08188dc0_clipmap.numLeafSurfaces = count;
	out = g_08188dc0_clipmap.leafSurfaces;
	for (i = 0; i < (int)count; i++) {
		*out = CMod_LittleLong(*file);
		file++;
		out++;
	}
}

// 0x080540c6  size=253  FUN_080540c6  status=translated  naming=observed
// String "CMod_LoadCollisionVerts" tags the allocation; 0x10-byte file records (first 4 bytes skipped) become 3-float vertices.
void CMod_LoadCollisionVerts(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	CollisionVertex *out;
	int i;

	file = (const byte *)(l->fileofs + cmod_base);
	if ((l->filelen & 0xf) != 0) {
		Com_Error(1, "\025CMod_LoadCollisionVerts: funny lump size");
	}
	count = l->filelen >> 4;
	g_08188dc0_clipmap.verts = (CollisionVertex *)CM_HunkAlloc(count * 0xc, "CMod_LoadCollisionVerts", 0x18);
	g_08188dc0_clipmap.numVerts = count;
	out = g_08188dc0_clipmap.verts;
	for (i = 0; i < (int)count; i++) {
		out->xyz[0] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + 4)));
		out->xyz[1] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + 8)));
		out->xyz[2] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + 0xc)));
		file += 0x10;
		out++;
	}
}

// 0x080541c4  size=607  FUN_080541c4  status=translated  naming=observed
// String "CMod_LoadCollisionEdges" tags the allocation; 0x38-byte file records (first 4 bytes skipped, 12 floats then a length) become 0x30-byte edges whose third axis is scaled by 1/length.
void CMod_LoadCollisionEdges(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	CollisionEdge *out;
	int i;
	int k;
	float length;

	file = (const byte *)(l->fileofs + cmod_base);
	if (l->filelen % 0x38 != 0) {
		Com_Error(1, "\025CMod_LoadCollisionEdges: funny lump size");
	}
	count = l->filelen / 0x38;
	g_08188dc0_clipmap.edges = (CollisionEdge *)CM_HunkAlloc(count * 0x30, "CMod_LoadCollisionEdges", 0x18);
	g_08188dc0_clipmap.numEdges = count;
	out = g_08188dc0_clipmap.edges;
	for (i = 0; i < (int)count; i++) {
		float *f = (float *)out;
		for (k = 0; k < 12; k++) {
			f[k] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + 4 + k * 4)));
		}
		length = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + 0x34)));
		VectorScale(out->axis[2], 1.0f / length, out->axis[2]);
		file += 0x38;
		out++;
	}
}

// 0x08054424  size=619  FUN_08054424  status=translated  naming=observed
// String "CMod_LoadCollisionTriangles" tags the allocation; 0x48-byte records: 12 floats (plane, s-vec, t-vec) then 3 vertex indices and 3 edge indices.
void CMod_LoadCollisionTriangles(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	CollisionTriangle *out;
	int i;
	int k;

	file = (const byte *)(l->fileofs + cmod_base);
	if (l->filelen % 0x48 != 0) {
		Com_Error(1, "\025CMod_LoadCollisionTriangles: funny lump size");
	}
	count = l->filelen / 0x48;
	g_08188dc0_clipmap.triangles = (CollisionTriangle *)CM_HunkAlloc(count * 0x48, "CMod_LoadCollisionTriangles", 0x18);
	g_08188dc0_clipmap.numTriangles = count;
	out = g_08188dc0_clipmap.triangles;
	for (i = 0; i < (int)count; i++) {
		float *f = (float *)out;
		for (k = 0; k < 12; k++) {
			f[k] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + k * 4)));
		}
		for (k = 0; k < 3; k++) {
			out->edges[k] = CMod_LittleLong(*(const int *)(file + 0x3c + k * 4));
			out->verts[k] = CMod_LittleLong(*(const int *)(file + 0x30 + k * 4));
		}
		file += 0x48;
		out++;
	}
}

// 0x08054690  size=401  FUN_08054690  status=translated  naming=observed
// String "CMod_LoadCollisionBorders" tags the allocation; 0x1c-byte records of 7 floats.
void CMod_LoadCollisionBorders(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	CollisionBorder *out;
	int i;
	int k;

	file = (const byte *)(l->fileofs + cmod_base);
	if (l->filelen % 0x1c != 0) {
		Com_Error(1, "\025CMod_LoadCollisionBorders: funny lump size");
	}
	count = l->filelen / 0x1c;
	g_08188dc0_clipmap.borders = (CollisionBorder *)CM_HunkAlloc(count * 0x1c, "CMod_LoadCollisionBorders", 0x18);
	g_08188dc0_clipmap.numBorders = count;
	out = g_08188dc0_clipmap.borders;
	for (i = 0; i < (int)count; i++) {
		float *f = (float *)out;
		for (k = 0; k < 7; k++) {
			f[k] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + k * 4)));
		}
		file += 0x1c;
		out++;
	}
}

// 0x08054822  size=303  FUN_08054822  status=translated  naming=observed
// String "CMod_LoadCollisionPartitions" tags the allocation; 0xc-byte records (counts at +2/+3, triangle and border start indices at +4/+8) are resolved to pointers.
void CMod_LoadCollisionPartitions(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	CollisionPartition *out;
	int i;

	file = (const byte *)(l->fileofs + cmod_base);
	if (l->filelen % 0xc != 0) {
		Com_Error(1, "\025CMod_LoadCollisionPartitions: funny lump size");
	}
	count = l->filelen / 0xc;
	g_08188dc0_clipmap.partitions = (CollisionPartition *)CM_HunkAlloc(count * 0xc, "CMod_LoadCollisionPartitions", 0x18);
	g_08188dc0_clipmap.numPartitions = count;
	out = g_08188dc0_clipmap.partitions;
	for (i = 0; i < (int)count; i++) {
		out->triCount = file[2];
		out->borderCount = file[3];
		out->triangles = (CollisionTriangle *)(CMod_LittleLong(*(const int *)(file + 4)) * 0x48 + (int)g_08188dc0_clipmap.triangles);
		out->borders = (CollisionBorder *)(CMod_LittleLong(*(const int *)(file + 8)) * 0x1c + (int)g_08188dc0_clipmap.borders);
		file += 0xc;
		out++;
	}
}

// 0x08054952  size=398  FUN_08054952  status=translated  naming=observed
// String "CMod_LoadCollisionAabbTrees" tags the allocation; 0x20-byte records: 6 floats, material index, child count, child/partition index.
void CMod_LoadCollisionAabbTrees(const bspLump_t *l)
{
	const byte *file;
	unsigned int count;
	CollisionAabbTree *out;
	int i;
	int k;

	file = (const byte *)(l->fileofs + cmod_base);
	if ((l->filelen & 0x1f) != 0) {
		Com_Error(1, "\025CMod_LoadCollisionAabbTrees: funny lump size");
	}
	count = l->filelen >> 5;
	g_08188dc0_clipmap.aabbTrees = (CollisionAabbTree *)CM_HunkAlloc(count << 5, "CMod_LoadCollisionAabbTrees", 0x18);
	g_08188dc0_clipmap.numAabbTrees = count;
	out = g_08188dc0_clipmap.aabbTrees;
	for (i = 0; i < (int)count; i++) {
		float *f = (float *)out;
		for (k = 0; k < 6; k++) {
			f[k] = Swap_BitsToFloat(Swap_FloatBits((const uint32 *)(file + k * 4)));
		}
		out->materialIndex = LittleShort(*(const short *)(file + 0x18));
		out->childCount = LittleShort(*(const short *)(file + 0x1a));
		out->u.firstChildIndex = CMod_LittleLong(*(const int *)(file + 0x1c));
		file += 0x20;
		out++;
	}
}

// 0x08054ae0  size=90  FUN_08054ae0  status=translated  naming=observed
// String "CMod_LoadEntityString" tags the allocation; copies the entity lump verbatim.
void CMod_LoadEntityString(const bspLump_t *l)
{
	g_08188dc0_clipmap.numEntityChars = l->filelen;
	g_08188dc0_clipmap.entityString = (char *)CM_HunkAlloc(l->filelen, "CMod_LoadEntityString", 9);
	Com_Memcpy(g_08188dc0_clipmap.entityString, l->fileofs + cmod_base, l->filelen);
}

// 0x08054b3a  size=239  FUN_08054b3a  status=translated  naming=observed
// String "CMod_LoadVisibility" tags the allocation; empty lump gives an all-ones visibility table, else (numClusters, clusterBytes, data) is copied.
void CMod_LoadVisibility(const bspLump_t *l)
{
	int len;
	const int *p;

	len = l->filelen;
	if (len == 0) {
		g_08188dc0_clipmap.clusterBytes = (g_08188dc0_clipmap.numClusters + 0x1fU) & 0xffffffe0;
		g_08188dc0_clipmap.visibility = (byte *)CM_HunkAlloc(g_08188dc0_clipmap.clusterBytes, "CMod_LoadVisibility", 9);
		Com_Memset(g_08188dc0_clipmap.visibility, 0xff, g_08188dc0_clipmap.clusterBytes);
	} else {
		p = (const int *)(l->fileofs + cmod_base);
		g_08188dc0_clipmap.vised = 1;
		g_08188dc0_clipmap.numClusters = CMod_LittleLong(p[0]);
		g_08188dc0_clipmap.clusterBytes = CMod_LittleLong(p[1]);
		g_08188dc0_clipmap.visibility = (byte *)CM_HunkAlloc(len + -8, "CMod_LoadVisibility", 9);
		Com_Memcpy(g_08188dc0_clipmap.visibility, p + 2, len + -8);
	}
}

// 0x08054c2a  size=325  FUN_08054c2a  status=translated  naming=observed
// String "CMod_LoadBrushRelated" tags the final allocation; loads brushes, leaf brushes, aabb trees, leafs and submodels, builds the brush-node trees in the temp buffer and copies them (with a reserved leading node) to permanent memory.
void CMod_LoadBrushRelated(bspHeader_t *header, char loadClusters)
{
	int diff;
	void *top;
	byte *mem;

	CMod_LoadBrushes(&header->lumps[CM_LUMP_BRUSHES], &header->lumps[CM_LUMP_BRUSHSIDES]);
	CMod_LoadLeafBrushes(&header->lumps[CM_LUMP_LEAFBRUSHES]);
	CMod_LoadCollisionAabbTrees(&header->lumps[CM_LUMP_COLLISIONAABBTREES]);
	CMod_LoadLeafs(&header->lumps[CM_LUMP_LEAFS], loadClusters);
	CMod_LoadSubmodels(&header->lumps[CM_LUMP_SUBMODELS]);
	CM_NoOp_BeforeTempBufferReset();
	TempMemory_Reset();
	top = (void *)TempMemory_Reserve(0);
	g_08188dc0_clipmap.brushNodes = (cbrushnode_t *)((int)top + -0x14);
	CMod_LoadLeafBrushNodes(&header->lumps[CM_LUMP_LEAFS]);
	CMod_LoadSubmodelBrushNodes(&header->lumps[CM_LUMP_SUBMODELS]);
	CM_InitBoxHull();
	g_08188dc0_clipmap.brushNodes = (cbrushnode_t *)((int)g_08188dc0_clipmap.brushNodes + 0x14);
	top = (void *)TempMemory_Reserve(0);
	diff = (int)((int)top - (int)g_08188dc0_clipmap.brushNodes) >> 2;
	g_08188dc0_clipmap.numBrushNodes = diff * -0x33333333 + 1;
	mem = (byte *)CM_HunkAlloc(g_08188dc0_clipmap.numBrushNodes * 0x14, "CMod_LoadBrushRelated", 0x16);
	memcpy(mem + 0x14, g_08188dc0_clipmap.brushNodes, diff * 4);
	g_08188dc0_clipmap.brushNodes = (cbrushnode_t *)mem;
	CM_HunkClearLow();
}

// 0x08054d70  size=412  FUN_08054d70  status=translated  naming=observed
// String "CM_LoadMapFromBsp" tags the name allocation; also the "do not compile the bsp with visibility" error.
void CM_LoadMapFromBsp(const char *name, char loadClusters)
{
	size_t len;
	bspHeader_t *header;

	Com_Memset(&g_08188dc0_clipmap, 0, 0x110);
	Com_Memset(&cm_numPlanes, 0, 0xc);
	len = strlen(name);
	g_08188dc0_clipmap.name = (char *)CM_HunkAlloc(len + 1, "CM_LoadMapFromBsp", 0x15);
	strcpy(g_08188dc0_clipmap.name, name);
	header = (bspHeader_t *)Com_GetBspHeader(0, &g_08188dc0_clipmap.checksum);
	cmod_base = (byte *)header;
	CMod_LoadMaterials(&header->lumps[CM_LUMP_MATERIALS]);
	CMod_LoadPlanes(cmod_base, &header->lumps[CM_LUMP_PLANES]);
	CMod_LoadBrushRelated(header, loadClusters);
	CMod_LoadNodes(&header->lumps[CM_LUMP_NODES]);
	CMod_LoadLeafSurfaces(&header->lumps[CM_LUMP_LEAFSURFACES]);
	CMod_LoadCollisionVerts(&header->lumps[CM_LUMP_COLLISIONVERTS]);
	CMod_LoadCollisionEdges(&header->lumps[CM_LUMP_COLLISIONEDGES]);
	CMod_LoadCollisionTriangles(&header->lumps[CM_LUMP_COLLISIONTRIANGLES]);
	CMod_LoadCollisionBorders(&header->lumps[CM_LUMP_COLLISIONBORDERS]);
	CMod_LoadCollisionPartitions(&header->lumps[CM_LUMP_COLLISIONPARTITIONS]);
	if (loadClusters == 0) {
		if (header->lumps[CM_LUMP_VISIBILITY].filelen != 0) {
			Com_Error(1, "In single player, do not compile the bsp with visibility");
		}
	} else {
		CMod_LoadVisibility(&header->lumps[CM_LUMP_VISIBILITY]);
	}
	CMod_LoadEntityString(&header->lumps[CM_LUMP_ENTITIES]);
	cmod_base = 0;
}

// 0x08054f0c  size=321  FUN_08054f0c  status=translated  naming=inferred
// Builds the extra "box" brush (index numBrushes, contents -1, no sides), an all-encompassing box leaf and a one-brush leaf node referencing it (Q3 CM_InitBoxHull shape).
void CM_InitBoxHull(void)
{
	cbrushnode_t *node;

	g_08188dc0_clipmap.box_brush = &g_08188dc0_clipmap.brushes[g_08188dc0_clipmap.numBrushes];
	g_08188dc0_clipmap.box_brush->numsides = 0;
	g_08188dc0_clipmap.box_brush->sides = 0;
	g_08188dc0_clipmap.box_brush->contents = 0xffffffff;
	g_08188dc0_clipmap.box_leaf.brushContents = 0xffffffff;
	g_08188dc0_clipmap.box_leaf.terrainContents = 0;
	Vec3_Set_0805528e((uint32 *)g_08188dc0_clipmap.box_leaf.mins, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff);
	Vec3_Set_0805528e((uint32 *)g_08188dc0_clipmap.box_leaf.maxs, 0xff7fffff, 0xff7fffff, 0xff7fffff);
	g_08188dc0_clipmap.box_brush->axialMaterialNum[0][0] = (short)0xffff;
	g_08188dc0_clipmap.box_brush->axialMaterialNum[0][1] = (short)0xffff;
	g_08188dc0_clipmap.box_brush->axialMaterialNum[0][2] = (short)0xffff;
	g_08188dc0_clipmap.box_brush->axialMaterialNum[1][0] = (short)0xffff;
	g_08188dc0_clipmap.box_brush->axialMaterialNum[1][1] = (short)0xffff;
	g_08188dc0_clipmap.box_brush->axialMaterialNum[1][2] = (short)0xffff;
	node = CM_AllocBrushNode();
	g_08188dc0_clipmap.box_leaf.leafBrushNode = CM_BrushNodeDiff(node, g_08188dc0_clipmap.brushNodes);
	node->leafBrushCount = 1;
	node->u.leafBrushes = g_08188dc0_clipmap.leafBrushes + g_08188dc0_clipmap.numLeafBrushes;
	g_08188dc0_clipmap.leafBrushes[g_08188dc0_clipmap.numLeafBrushes] = g_08188dc0_clipmap.numBrushes;
}

// 0x0805504e  size=15  FUN_0805504e  status=translated  naming=unknown
// Clears the BSP image base pointer.
void CMod_ClearBspBase(void)
{
	cmod_base = 0;
}

// 0x0805505e  size=24  FUN_0805505e  status=translated  naming=unknown
// Returns &cm_planes[index].
cplane_t *CM_PlaneFromIndex(int index)
{
	return (cplane_t *)(index * 0x14 + (int)cm_planes);
}

// 0x08055076  size=368  FUN_08055076  status=translated  naming=inferred
// Re-opens the map file (g_08188dc0_clipmap.name), reads the 0x140-byte header, byte-swaps it and loads one lump into a fresh buffer; error key "EXE_ERR_COULDNT_LOAD" at 0x813ffd9. Callee roles (open/read/seek/close/alloc) inferred from argument shapes.
int CMod_ReadLump(int lumpIndex, void **outData)
{
	int len;
	int file;
	unsigned int i;
	int header[82];

	FS_OpenFileRead(g_08188dc0_clipmap.name, &file, 0);
	if (file == 0) {
		Com_Error(1, "EXE_ERR_COULDNT_LOAD\025%s", g_08188dc0_clipmap.name);
	}
	FS_Read(header, 0x140, file);
	for (i = 0; i < 0x50; i++) {
		header[i] = CMod_LittleLong(header[i]);
	}
	len = header[lumpIndex * 2 + 2];
	if (len == 0) {
		FS_FCloseFile(file);
		len = 0;
	} else {
		FS_Seek(file, header[lumpIndex * 2 + 3] + -0x140, 0);
		cmod_lumpBuffer = Hunk_AllocateTempMemory(len);
		FS_Read(cmod_lumpBuffer, len, file);
		FS_FCloseFile(file);
		*outData = cmod_lumpBuffer;
	}
	return len;
}

// 0x080551e6  size=21  FUN_080551e6  status=translated  naming=inferred
// Releases the buffer allocated by CMod_ReadLump.
void CMod_FreeLump(void)
{
	Hunk_FreeTempMemory(cmod_lumpBuffer);
}

// 0x080551fc  size=25  FUN_080551fc  status=translated  naming=inferred
// fabs for a float (per-TU copy of the vector-math helper).
float Float_Abs_080551fc(float v)
{
	return fabs(v);
}

// 0x08055216  size=36  FUN_08055216  status=translated  naming=inferred
// max(a, b): selects a when a - b >= 0, else b.
float Float_Max(float a, float b)
{
	return Float_SelectNonNeg(a - b, a, b);
}

// 0x0805523a  size=36  FUN_0805523a  status=translated  naming=inferred
// min(a, b): selects a when b - a >= 0, else b.
float Float_Min(float a, float b)
{
	return Float_SelectNonNeg(b - a, a, b);
}

// 0x0805525e  size=38  FUN_0805525e  status=translated  naming=inferred
// Integer min(a, b): selects a when b - a >= 0, else b.
int Int_Min(int a, int b)
{
	return Int_SelectNonNeg(b - a, a, b);
}

// 0x08055284  size=10  FUN_08055284  status=translated  naming=inferred
// Little-endian host "LittleFloat" shape: returns the 32-bit pattern at *p.
uint32 Swap_FloatBits(const uint32 *p)
{
	return *p;
}

// 0x0805528e  size=35  FUN_0805528e  status=translated  naming=unknown
// Stores three 32-bit patterns into a vec3.
void Vec3_Set_0805528e(uint32 *v, uint32 x, uint32 y, uint32 z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

// 0x080552b2  size=47  FUN_080552b2  status=translated  naming=inferred
// VectorCopy (three 32-bit words).
void VectorCopy_080552b2(const uint32 *src, uint32 *dst)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

// 0x080552e2  size=56  FUN_080552e2  status=translated  naming=inferred
// VectorScale: out = scale * v.
void VectorScale(const float *v, float scale, float *out)
{
	out[0] = scale * v[0];
	out[1] = scale * v[1];
	out[2] = scale * v[2];
}

// 0x0805531a  size=62  FUN_0805531a  status=translated  naming=inferred
// VectorLength: sqrt of the sum of squares, via Float_Sqrt.
float VectorLength(const float *v)
{
	return Float_Sqrt(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
}

// 0x08055358  size=19  FUN_08055358  status=translated  naming=inferred
// Little-endian host "LittleShort": sign-extends the short.
int LittleShort(short v)
{
	return (int)v;
}

// 0x0805536c  size=8  FUN_0805536c  status=translated  naming=inferred
// Little-endian host "CMod_LittleLong": identity.
int CMod_LittleLong(int v)
{
	return v;
}

// 0x08055374  size=19  FUN_08055374  status=translated  naming=inferred
// Reinterprets a 32-bit pattern as float (the pattern is passed through the stack and read back by Float_Identity).
float Swap_BitsToFloat(uint32 bits)
{
	union {
		uint32 u;
		float f;
	} x;

	x.u = bits;
	return Float_Identity(x.f);
}

// 0x08055388  size=41  FUN_08055388  status=translated  naming=inferred
// Returns ifNonNeg when 0.0 <= cond, otherwise ifNeg (NaN selects ifNeg).
float Float_SelectNonNeg(float cond, float ifNonNeg, float ifNeg)
{
	float result;

	if (0.0 <= cond) {
		result = ifNonNeg;
	} else {
		result = ifNeg;
	}
	return result;
}

// 0x080553b2  size=31  FUN_080553b2  status=translated  naming=inferred
// sqrt computed in double precision and narrowed to float.
float Float_Sqrt(float v)
{
	double d;
	float f;

	d = sqrt((double)v);
	f = (float)d;
	return f;
}

// 0x080553d2  size=31  FUN_080553d2  status=translated  naming=inferred
// Returns ifNeg when cond < 0, otherwise ifNonNeg.
int Int_SelectNonNeg(int cond, int ifNonNeg, int ifNeg)
{
	int result;

	if (cond < 0) {
		result = ifNeg;
	} else {
		result = ifNonNeg;
	}
	return result;
}

// 0x080553f2  size=17  FUN_080553f2  status=translated  naming=unknown
// Returns its float argument unchanged.
float Float_Identity(float v)
{
	return v;
}

// 0x08055404  size=434  FUN_08055404  status=translated  naming=inferred
// Separating-axis style reject of the trace volume against an aabb (origin, halfSize): 1 = culled, 0 = may intersect. Name from the CoD4 CM_CullBox shape (three axis tests plus three cross-product tests unless axialCullOnly).
int CM_CullBox(const traceWork_t *tw, const float *origin, const float *halfSize)
{
	float d[3];
	float e[3];
	int result;

	VectorSubtract(tw->midpoint, origin, d);
	VectorAdd(halfSize, tw->size, e);
	if ((long double)e[0] + (long double)tw->halfDeltaAbs[0] < (long double)Float_Abs_0805618c(d[0])) {
		result = 1;
	} else if ((long double)e[1] + (long double)tw->halfDeltaAbs[1] < (long double)Float_Abs_0805618c(d[1])) {
		result = 1;
	} else if ((long double)e[2] + (long double)tw->halfDeltaAbs[2] < (long double)Float_Abs_0805618c(d[2])) {
		result = 1;
	} else if (tw->axialCullOnly == 0) {
		if ((long double)e[2] * (long double)tw->halfDeltaAbs[1] +
			    (long double)e[1] * (long double)tw->halfDeltaAbs[2] <
		    (long double)Float_Abs_0805618c(tw->halfDelta[1] * d[2] - tw->halfDelta[2] * d[1])) {
			result = 1;
		} else if ((long double)e[0] * (long double)tw->halfDeltaAbs[2] +
				   (long double)e[2] * (long double)tw->halfDeltaAbs[0] <
			   (long double)Float_Abs_0805618c(tw->halfDelta[2] * d[0] - tw->halfDelta[0] * d[2])) {
			result = 1;
		} else if ((long double)e[1] * (long double)tw->halfDeltaAbs[0] +
				   (long double)e[0] * (long double)tw->halfDeltaAbs[1] <
			   (long double)Float_Abs_0805618c(tw->halfDelta[0] * d[1] - tw->halfDelta[1] * d[0])) {
			result = 1;
		} else {
			result = 0;
		}
	} else {
		result = 0;
	}
	return result;
}

// 0x080555b6  size=1338  FUN_080555b6  status=translated  naming=inferred
// Tests whether the trace sphere/capsule start position overlaps a collision triangle (face interior, then vertex and edge distance tests); on overlap sets fraction=0 and both solid flags. Name from CoD4 CM_PositionTestCapsuleInTriangle shape; not string-confirmed.
void CM_PositionTestCapsuleInTriangle(const traceWork_t *tw, const CollisionTriangle *tri,
				      trace_t *trace)
{
	float offsetZ;
	float pos[3];
	float radius;
	float dist;
	float negRadius;
	float distEnd;
	float t;
	float s;
	float tt;
	float u;
	float v;
	unsigned int flags;
	float proj[3];
	float s2;
	float t2;
	int i;
	int vi;
	int ei;
	float diff[3];
	const CollisionEdge *edge;
	float f;
	long double dotv;

	offsetZ = tw->offsetZ;
	if (tri->plane[2] < 0.0) {
		offsetZ = -offsetZ;
	}
	VectorCopy_080561a6((const uint32 *)tw->start, (uint32 *)pos);
	pos[2] = pos[2] - offsetZ;
	radius = tw->radius;
	dotv = DotProduct(pos, tri->plane);
	dist = (float)(dotv - (long double)tri->plane[3]);
	/* x87 fucompp/jae: the body runs when the compare is false OR unordered (NaN), so test the negation. */
	if (!(dist >= radius)) {
		negRadius = -radius;
		if (dist <= negRadius) {
			distEnd = dist + (offsetZ + offsetZ) * tri->plane[2];
			if (!(negRadius >= distEnd)) {
				t = (negRadius - dist) / tri->plane[2];
				dotv = DotProduct(pos, tri->svec);
				s = (float)(dotv - (long double)tri->svec[3]);
				dotv = DotProduct(pos, tri->tvec);
				tt = (float)(dotv - (long double)tri->tvec[3]);
				u = s + t * tri->svec[2];
				if (!(u >= 0.0 && (v = tt + t * tri->tvec[2], v >= 0.0) && 1.0 >= u + v)) {
					if (distEnd < radius) {
						t = offsetZ + offsetZ;
					} else {
						t = (radius - dist) / tri->plane[2];
					}
					s = s + t * tri->svec[2];
					if (!(0.0 > s) && (tt = tt + t * tri->tvec[2], !(0.0 > tt)) && !(s + tt > 1.0)) {
						trace->startsolid = 1;
						trace->allsolid = 1;
						trace->fraction = 0.0f;
					}
				} else {
					trace->startsolid = 1;
					trace->allsolid = 1;
					trace->fraction = 0.0f;
				}
			}
		} else {
			flags = 0;
			VectorMA(pos, -dist, tri->plane, proj);
			dotv = DotProduct(proj, tri->svec);
			s2 = (float)(dotv - (long double)tri->svec[3]);
			dotv = DotProduct(proj, tri->tvec);
			t2 = (float)(dotv - (long double)tri->tvec[3]);
			if (1.0 < s2 + t2) {
				flags = flags | 1;
			}
			if (s2 < 0.0) {
				flags = flags | 2;
			}
			if (t2 < 0.0) {
				flags = flags | 4;
			}
			if (flags == 0) {
				trace->startsolid = 1;
				trace->allsolid = 1;
				trace->fraction = 0.0f;
			} else {
				for (i = 0; i < 3; i++) {
					if ((((int)flags >> ((byte)i & 0x1f)) & 1U) == 0) {
						vi = tri->verts[i];
						if (-1 < vi) {
							VectorSubtract(pos, g_08188dc0_clipmap.verts[vi].xyz, diff);
							if ((long double)VectorLengthSquared(diff) <
							    (long double)tw->radius * (long double)tw->radius) {
								trace->startsolid = 1;
								trace->allsolid = 1;
								trace->fraction = 0.0f;
								return;
							}
						}
					} else {
						ei = tri->edges[i];
						if (-1 < ei) {
							edge = &g_08188dc0_clipmap.edges[ei];
							VectorSubtract(pos, edge->origin, diff);
							dotv = DotProduct(diff, edge->axis[2]);
							f = (float)dotv;
							if ((long double)Float_Abs_0805618c(f - 0.5) <= (long double)0.5) {
								f = f / VectorLengthSquared(edge->axis[2]);
								VectorMA(diff, -f, edge->axis[2], diff);
								if ((long double)VectorLengthSquared(diff) <
								    (long double)tw->radius * (long double)tw->radius) {
									trace->startsolid = 1;
									trace->allsolid = 1;
									trace->fraction = 0.0f;
									return;
								}
							}
						}
					}
				}
			}
		}
	}
}

// 0x08055af0  size=603  FUN_08055af0  status=translated  naming=inferred
// Recursive swept trace through the aabb tree: culls with CM_CullBox, at a leaf (childCount 0) tests each not-yet-checked partition's triangles (point or capsule variant by tw->isPoint) and borders, else recurses over the children.
void CM_TraceThroughAabbTree_r(traceWork_t *tw, CollisionAabbTree *tree, trace_t *trace)
{
	char culled;
	int partIndex;
	CollisionPartition *part;
	int i;
	int childIdx;
	CollisionAabbTree *child;

	culled = CM_CullBox(tw, tree->origin, tree->halfSize);
	if (culled == 0) {
		if (tree->childCount == 0) {
			partIndex = tree->u.partitionIndex;
			if ((unsigned int)tw->partitionChecks[partIndex] != (int)tw->checkcount) {
				tw->partitionChecks[partIndex] = tw->checkcount;
				part = &g_08188dc0_clipmap.partitions[partIndex];
				if (tw->isPoint == 0) {
					for (i = 0; i < (int)(unsigned int)part->triCount; i++) {
						CollisionTriangle *tri = &part->triangles[i];
						cm_wedge_sphere_sweep(tw, tri, tw->offsetZ, trace);
						if (tri->plane[2] < 0.0) {
							cm_wedge_sphere_sweep(tw, tri, -tw->offsetZ, trace);
						}
					}
					if ((tw->delta[0] != 0.0 || tw->delta[1] != 0.0) && tw->offsetZ != 0.0) {
						for (childIdx = 0; childIdx < (int)(unsigned int)part->borderCount; childIdx++) {
							cm_wall_circle_sweep(tw, &part->borders[childIdx], trace);
						}
					}
				} else {
					for (i = 0; i < (int)(unsigned int)part->triCount; i++) {
						cm_wedge_edge_sweep(tw, &part->triangles[i], trace);
					}
				}
			}
		} else {
			child = &g_08188dc0_clipmap.aabbTrees[tree->u.firstChildIndex];
			for (i = 0; i < (int)(unsigned int)tree->childCount; i++) {
				CM_TraceThroughAabbTree_r(tw, child, trace);
				child++;
			}
		}
	}
}

// 0x08055d4c  size=299  FUN_08055d4c  status=translated  naming=inferred
// Position-test counterpart of CM_TraceThroughAabbTree_r: same cull and partition-check logic but tests each triangle with CM_PositionTestCapsuleInTriangle.
void CM_PositionTestInAabbTree_r(traceWork_t *tw, CollisionAabbTree *tree, trace_t *trace)
{
	char culled;
	int partIndex;
	CollisionPartition *part;
	int i;
	CollisionAabbTree *child;

	culled = CM_CullBox(tw, tree->origin, tree->halfSize);
	if (culled == 0) {
		if (tree->childCount == 0) {
			partIndex = tree->u.partitionIndex;
			if ((unsigned int)tw->partitionChecks[partIndex] != (int)tw->checkcount) {
				tw->partitionChecks[partIndex] = tw->checkcount;
				part = &g_08188dc0_clipmap.partitions[partIndex];
				for (i = 0; i < (int)(unsigned int)part->triCount; i++) {
					CM_PositionTestCapsuleInTriangle(tw, &part->triangles[i], trace);
				}
			}
		} else {
			child = &g_08188dc0_clipmap.aabbTrees[tree->u.firstChildIndex];
			for (i = 0; i < (int)(unsigned int)tree->childCount; i++) {
				CM_PositionTestInAabbTree_r(tw, child, trace);
				child++;
			}
		}
	}
}

// 0x08055e78  size=138  FUN_08055e78  status=translated  naming=inferred
// Traces one aabb tree if its material contents match tw->contents; when the trace fraction shrinks, records the material's surface flags, contents and pointer in the trace.
void CM_TraceThroughAabbTree(traceWork_t *tw, CollisionAabbTree *tree, trace_t *trace)
{
	dMaterial_t *mat;
	uint32 oldFraction;
	float oldFractionF;

	mat = &g_08188dc0_clipmap.materials[tree->materialIndex];
	if ((mat->contents & tw->contents) != 0) {
		memcpy(&oldFraction, &trace->fraction, 4);
		memcpy(&oldFractionF, &oldFraction, 4);
		CM_TraceThroughAabbTree_r(tw, tree, trace);
		if (trace->fraction < oldFractionF) {
			trace->surfaceFlags = mat->surfaceFlags;
			trace->contents = mat->contents;
			trace->material = mat;
		}
	}
}

// 0x08055f02  size=78  FUN_08055f02  status=translated  naming=inferred
// Calls CM_TraceThroughAabbTree only when the tree's material contents intersect tw->contents (the callee repeats the same test).
void CM_TraceThroughAabbTreeMasked(traceWork_t *tw, CollisionAabbTree *tree, trace_t *trace)
{
	if ((g_08188dc0_clipmap.materials[tree->materialIndex].contents & tw->contents) != 0) {
		CM_TraceThroughAabbTree(tw, tree, trace);
	}
}

// 0x08055f50  size=181  FUN_08055f50  status=translated  naming=inferred
// Position test over all aabb trees of a leaf whose material contents match; stops at the first tree that sets the trace's 0x22 flag and copies that material's surface flags, contents and pointer into the trace.
void CM_PositionTestInTerrainLeaf(traceWork_t *tw, const cLeaf_t *leaf, trace_t *trace)
{
	int i;
	CollisionAabbTree *tree;
	dMaterial_t *mat;

	i = 0;
	while (true) {
		if ((int)(unsigned int)leaf->collAabbCount <= i) {
			return;
		}
		tree = &g_08188dc0_clipmap.aabbTrees[leaf->firstCollAabbIndex + i];
		mat = &g_08188dc0_clipmap.materials[tree->materialIndex];
		if ((mat->contents & tw->contents) != 0) {
			CM_PositionTestInAabbTree_r(tw, tree, trace);
			if (trace->allsolid != 0) {
				break;
			}
		}
		i = i + 1;
	}
	trace->surfaceFlags = mat->surfaceFlags;
	trace->contents = mat->contents;
	trace->material = mat;
}

// 0x0805618c  size=25  FUN_0805618c  status=translated  naming=inferred
// fabs for a float (per-TU copy).
float Float_Abs_0805618c(float v)
{
	return fabs(v);
}

// 0x080561a6  size=47  FUN_080561a6  status=translated  naming=inferred
// VectorCopy (three 32-bit words), per-TU copy.
void VectorCopy_080561a6(const uint32 *src, uint32 *dst)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

// 0x080561d6  size=68  FUN_080561d6  status=translated  naming=inferred
// VectorAdd: out = a + b.
void VectorAdd(const float *a, const float *b, float *out)
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

// 0x0805621a  size=68  FUN_0805621a  status=translated  naming=inferred
// VectorSubtract: out = a - b.
void VectorSubtract(const float *a, const float *b, float *out)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

// 0x0805625e  size=83  FUN_0805625e  status=translated  naming=inferred
// VectorMA: out = a + scale * dir.
void VectorMA(const float *a, float scale, const float *dir, float *out)
{
	out[0] = a[0] + scale * dir[0];
	out[1] = a[1] + scale * dir[1];
	out[2] = a[2] + scale * dir[2];
}

// 0x080562b2  size=51  FUN_080562b2  status=translated  naming=inferred
// DotProduct, summed z, y, x in extended precision.
long double DotProduct(const float *a, const float *b)
{
	/* 0x080562b2: fld a[0]; fmul b[0]; (+ a[1]*b[1]) (+ a[2]*b[2]); ret -- summed x, y, z and returned in st(0)
	 * without rounding to float (a float return type made g++ spill it through a float slot) */
	return ((long double)a[0] * (long double)b[0] + (long double)a[1] * (long double)b[1]) +
	       (long double)a[2] * (long double)b[2];
}

// 0x080562e6  size=51  FUN_080562e6  status=translated  naming=inferred
// Squared length (v . v), summed x, y, z in extended precision and returned unrounded in st(0) (0x080562e6).
long double VectorLengthSquared(const float *v)
{
	return ((long double)v[0] * (long double)v[0] + (long double)v[1] * (long double)v[1]) +
	       (long double)v[2] * (long double)v[2];
}
