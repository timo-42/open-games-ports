# Subsystem analysis: collision (cod2_lnxded 1.3)

Target: `cod2_lnxded`, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` (ELF32 i386).
Scope: clip-map loading from the `.d3dbsp` map, world/entity traces, brushes and terrain patches, point contents, the 2D entity "area" tree and the temp-box/static-model hooks.
Companion file: `collision.structs.json` (canonical struct layouts, per-struct conflicts).

Evidence tags used throughout: **[O]** observed (present in disassembly, a string, or directly readable pseudocode), **[I]** inferred (from structure, callers or lineage), **[U]** unknown.
Lineage notes (id Tech 3, CoD4) are naming hints only; this document describes what this binary does.

Method and limits: functions were selected from `analysis/symbols/chunk_*.json` by address range `0x08051e4c..0x0805fc5c` (all chunks 002-006 and 100 members; 228 functions) plus 30 related functions elsewhere (`SV_*` world clip functions in chunk 023, `G_Trace*` in chunk 049, `PM_*Trace*`/ground probes in chunk 041, `Trace_*` in chunk 045, `Gfx_GetBspVariant`, `Ent_GetTraceMask`), 258 in total. Subsystem tags were noisy: the same TU is tagged `common/clipmap`, `common/bsp`, `collision/*`, `math/*`, `common/math`, `alloc`, `math/util`. Disassembly was checked with `objdump -d` only (no REA/Ghidra session). No dynamic execution was done, so every behavioural statement about run-time values is static analysis.

## 1. Purpose and architecture

All world collision of the dedicated server lives in one contiguous code region `0x08051e4c-0x0805fc5c` (compiled from several source files, evidenced by repeated per-TU copies of the same vector helpers). It implements, in this order:

1. **BSP file image** (`BspFile_*`, 0x08051e4c-0x08052074): reads `maps/mp/<name>.<variant>` into a malloc'd buffer, byte-swaps (identity on x86) the 0x140-byte header, keeps the whole file in memory only while the clip map is built. [O]
2. **Clip-map loader** (`CM_LoadMap` .. `CMod_Load*`, 0x080521a2, 0x080527e4-0x08054d70): converts lumps into the global `cm` (clipMap, 0x110 bytes at `0x08188dc0`), builds a per-leaf brush kd-tree, builds the temp-box brush/leaf, allocates per-thread trace scratch. [O]
3. **Point queries**: plane-BSP descent (`cm_plane_tree_find_leaf`), box-vs-leaf collection, point contents through the per-leaf brush tree. [O]
4. **Moving-box trace** (`Sweep_TraceBox*`, `CM_RecursiveHullCheck`, `cm_leaf_surface_trace`, `cm_trace_node_ray`, `cm_brush_trace_planes`, `CM_TraceThroughAabbTree*`, triangle/edge/border sweeps): one world-vs-box/capsule trace. Plane BSP -> leaf -> {leaf brush kd-tree -> brushes, terrain AABB tree -> partitions -> triangles/borders}. [O]
5. **Position test** (start == end path, `cm_trace_leaves`, `cm_leaf_trace`, `cm_brush_box_test`, `CM_PositionTest*`). [O]
6. **Boolean/"sight" trace** with a hit-id hint (`Sweep_TraceEntityDispatch` family, `CM_TraceBrushBounds`, `CM_TraceKdTree`, `CM_TraceLeafItems`). [O]/[I]
7. **Temp-box/capsule model** for entities without a brush model: handle `0x3ff` (`cm_scratch_stage_segment`, `cm_model_get`, `Sweep_ClipBoxHalves`, `cm_trace_aabb_bounds`). [O]
8. **Static (misc_model) collision**: parsed from the BSP entity string, line-trace only (`cm_load_static_models`, `cm_static_model_*`). [O]
9. **Entity world tree ("area"/world sector)**: a 2D (x,y) lazily-split binary partition tree of up to 0x400 nodes with entity lists (svEntity records) and static-model lists, plus `CM_AreaEntities`, sweep and boolean query traversals (`Area_*`, `CM_AreaEntities*`, `SpatialTree_*`, `Collision_IsSegmentClear`). [O]
10. **Server glue** (not strictly in the region): `SV_Trace`, `SV_TraceIsClear`, `SV_TraceAccumulateHit`, `SV_BoxEntityHit`, `SV_PointContents`, `SV_LinkEntity`, `SV_UnlinkEntity`, `SV_ClipHandleForEntity`, `SV_ClipMoveToEntity_*`, `SV_Clip*HitsEntity` (chunk 023, 0x0809c544-0x0809e368); `G_Trace*` wrappers (chunk 049, 0x0810a528..). [O]

Two parallel "trace families" exist, both [O]:

| family | entry | result | used by |
| --- | --- | --- | --- |
| moving trace | `Sweep_TraceBoxInit` 0x0805b894 / `Sweep_TraceBoxWithOffsetInit` 0x0805bb76 (and the non-`Init` variants 0x0805b414 / 0x0805b8f6 which accumulate into an existing `trace_t`) | full `trace_t` (fraction, normal, surfaceFlags, contents, material) | `SV_Trace`, `SV_ClipMoveToEntity_Box/Line`, `Trace_BoxWorldThenEntities`, `FX_InitSampleRecord`, `SV_EntityRelevantToView` |
| boolean | `Sweep_TraceEntityDispatch` 0x0805cefe / `Sweep_TraceEntityWithOffset` 0x0805d328 | int: hit id (brush index+1 or numBrushes+aabbTreeIndex+1) or 0 | `SV_TraceIsClear`, `SV_TraceAccumulateHit`, `SV_BoxEntityHit`, `SV_ClipBoxHitsEntity`, `SV_ClipLineHitsEntity` |

Identification of the boolean family as a CoD2 "sight trace with oldHitNum hint" is **[I]** (return value encoding is [O]; name is lineage). The first argument `brush_hint` is the previous hit id; when `type==0` and `0 < hint <= numBrushes` the brush is tested first (`CM_TraceBrushBounds`) before the BSP walk. [O]

### 1.1 Three different trees (the chunks conflate them under "kd tree"/"node")

| tree | element | location | build |
| --- | --- | --- | --- |
| plane BSP | `cnode_t` 8 bytes `{plane*, short children[2]}`, leaves are `cLeaf_t` 0x2c | `cm.nodes` @ cm+0x20 (`g_08188de0`), `cm.leafs` @ cm+0x28 (`g_08188de8`) | file lumps 25/26 [O] |
| per-leaf brush tree | `cbrushnode_t` 0x14 (axis, count, contents, split|brush list, range, relative child offsets) | `cm.brushNodes` @ cm+0x30 (`g_08188df0`) | built at load by `CMod_PartionLeafBrushes_r` (not in file) [O] |
| terrain AABB tree | `CollisionAabbTree` 0x20 -> `CollisionPartition` 0xc -> `CollisionTriangle` 0x48 / `CollisionBorder` 0x1c | `cm.aabbTrees` @ cm+0x70 (`g_08188e30`) | file lumps 29-34 [O] |
| world sector (area) tree | `cm_area_node_t` 0x18, 1-based, 0x400 entries | `0x0818901c` | built at run time by `Area_LinkEntity`/`Area_LinkBoxRecord` [O] |

## 2. Entry points and call flow

### 2.1 Load (once per map start / restart)

`SV_SpawnServer` 0x08093520 (observed call sequence in `chunk_021.cpp`, confirmed by call-site scan: exactly one caller each):

1. `Gfx_GetBspVariant()` 0x080a36fe -> `"d3dbsp"` when dvar `gfx_driver` is empty else `"<driver>bsp"` (strings at 0x1019bf/0x104fa1/0x104fa7). [O]
2. `Com_sprintf(mapbuf, 0x40, "maps/mp/%s.%s", mapname, variant)`. [O]
3. `BspFile_Load(mapbuf)` 0x08051e88: `FS_OpenFileRead`, `Com_Malloc(len)`, `FS_Read`, requires len >= 0x140, `Com_BlockChecksum(buf,len)`, requires ident `0x50534249` ("IBSP") and version `4`; errors via `Com_Error(1, "EXE_ERR_COULDNT_LOAD\x15%s")` / `"EXE_ERR_WRONG_MAP_VERSION_NUM\x15%s"`. Stores image `0x08188da8`, length `0x08188dac`, checksum `0x08188db0`. [O]
4. `CM_LoadMap(mapbuf, &checksum)` 0x080521a2: if `cm.name` is NULL or `strcasecmp(cm.name,name)!=0` -> `CM_LoadMapData(name)` (= `CM_LoadMapFromBsp(name,1)` + `cm_load_static_models()`) then `CM_InitAllSlotBuffers()`; always returns `cm.checksum` (= BSP file checksum). Same-name call is a no-op load. [O]
5. `BspFile_Free()` frees the image (the clip map copies everything it needs; `cm.name`, planes etc. are in the Hunk). [O]
6. `Area_ResetAndRelinkAll()` 0x0805dc92 -> `Area_Init()` + `Area_RelinkAllBoxes()`. [O]

Other lifecycle: `CM_ClearMap` 0x0805220a (memset 0x110) from `Com_Restart_Subsystems`/`Com_ShutdownSubsystems`; `BspFile_FreeIfLoaded` from `Com_ErrorRecover`. [O]

### 2.2 `CM_LoadMapFromBsp` 0x08054d70 internal order [O]

`memset(&cm,0,0x110)`, `memset(&cm_numPlanes,0,0xc)` (clears `cm_numPlanes`, `cm_planes`, `cmod_base` at 0x08188ed8/edc/ee0), copy name (Hunk, tag "CM_LoadMapFromBsp"), `BspFile_Get(NULL,&cm.checksum)`, then:
`CMod_LoadMaterials(l0)` -> `CMod_LoadPlanes(l4)` -> `CMod_LoadBrushRelated` { `CMod_LoadBrushes(l6,l5)`, `CMod_LoadLeafBrushes(l27)`, `CMod_LoadCollisionAabbTrees(l34)`, `CMod_LoadLeafs(l26,loadClusters)`, `CMod_LoadSubmodels(l35)`, `ScrBuf_Reset`, set `cm.brushNodes = top-0x14`, `CMod_LoadLeafBrushNodes(l26)`, `CMod_LoadSubmodelBrushNodes(l35)`, `CM_InitBoxHull`, copy node pool to permanent Hunk with a reserved zero node 0 } -> `CMod_LoadNodes(l25)` -> `CMod_LoadLeafSurfaces(l28)` -> `CMod_LoadCollisionVerts(l29)` -> `Edges(l30)` -> `Triangles(l31)` -> `Borders(l32)` -> `Partitions(l33)` -> (loadClusters ? `CMod_LoadVisibility(l36)` : error if lump 36 non-empty "In single player, do not compile the bsp with visibility") -> `CMod_LoadEntityString(l37)`.
`CM_LoadMapData` always passes `loadClusters=1`, so the single-player branch is unreachable in this binary (only caller is `CM_LoadMapData`). [O]

### 2.3 Queries (public API of the region)

| address | recon name | role (this binary) | callers [O] |
| --- | --- | --- | --- |
| 0x0805271c | `CM_ModelBounds` | mins/maxs of `cm_model_get(h)` | `Area_Init`, `SV_InitEntityBoundsAndLink` |
| 0x08058b90/0x08058c30 | `cm_plane_tree_find_leaf[_root]` | point -> leaf (CM_PointLeafnum) | `SV_EntityVisibleToClient`, `SV_BuildClientListForPoint`, `SV_BuildCachedEntityListForPoint`, `cm_point_contents` |
| 0x08058d6a | `cm_plane_tree_query_box` | box -> leaf list (CM_BoxLeafnums); cap given by caller (0x80 in `SV_LinkEntity`) | `SV_LinkEntity`, `SV_BuildCachedEntityListForPoint` |
| 0x080526f8 | `CM_GetRecordShort28` | `cm.leafs[i].cluster` (CM_LeafCluster) | `SV_LinkEntity` and 3 visibility functions |
| 0x08059186 | `cm_table_entry_address` | PVS row pointer: `cluster<0 || cluster>=numClusters || !vised ? cm.visibility : cm.visibility + cluster*clusterBytes` (CM_ClusterPVS) | 3 visibility functions |
| 0x08058f8c | `cm_point_contents(p, model)` | CM_PointContents: world via plane BSP leaf, else model's embedded leaf; returns 0 outside leaf bounds (strict) | `SV_PointContents`, `Trace_ContentsWithEntities` |
| 0x08059098 | `cm_model_point_contents` | CM_TransformedPointContents (translate, optional rotate by `Angles_ToVectors`, right vector negated) | same |
| 0x080526ee | `CM_GetCount_08188e58` | returns `cm.entityString` pointer (not a count) | `SV_InitMapState`, `SV_ResetTokenCursor` |
| 0x080526e4 | `CM_GetCount_08188e34` | `cm.numSubModels` | none found |
| 0x0805b894 / 0x0805bb76 | `Sweep_TraceBoxInit` / `...WithOffsetInit` | CM_BoxTrace / CM_TransformedBoxTrace with result init (memset 0x24, fraction=1.0) | see table above |
| 0x0805cefe / 0x0805d328 | `Sweep_TraceEntityDispatch` / `...WithOffset` | boolean sight trace | see table above |
| 0x080592e2 | `cm_scratch_stage_segment(mins,maxs,contents)` | CM_TempBoxModel; returns handle `0x3ff` | `SV_ClipHandleForEntity`, `Trace_SweepEntities` |
| 0x08059358 | `cm_model_get(h)` | `h < numSubModels ? &cm.cmodels[h] : per-thread tempModel` | 7 callers |
| 0x0805e390 / 0x0805dca4 | `Area_LinkEntity` / `Area_UnlinkEntity` | world sector link/unlink | `SV_LinkEntity`, `SV_UnlinkEntity` |
| 0x0805e986 | `CM_AreaEntities(mins,maxs,list,max,mask)` | entities (by number) whose absmin/absmax overlap and `contents & mask`; message `"CM_AreaEntities: MAXCOUNT\n"` via `Com_DPrintf` on overflow | `G_RadiusDamage`, `G_MoverPush`, `G_TempEntity`, `G_CollectUseCandidates`, `Entity_TouchLinked`, `Ent_AreaEffectApply[Flagged]`, `G_SetObjectiveIcon`, `SV_PointContents` |
| 0x0805ec2c | `Area_SweepBoxFromPoints` | line sweep against static models, clips `result->fraction` | `SV_Trace` only (when `extraCheck`) |
| 0x0805ef22 | `Collision_IsSegmentClear` | boolean segment vs static models | `SV_TraceIsClear` only |
| 0x0805f2a8/0x0805f5fa/0x0805f822/0x0805faca | `SpatialTree_*FromRoot` | entity world-sector traversals (see 5.7) | `SV_Trace`, `SV_TraceIsClear`, `SV_TraceAccumulateHit` |

### 2.4 Moving-box trace call flow [O]

```
SV_Trace (0x0809d884)
  Sweep_TraceBoxInit(result,start,end,mins,maxs,type=0,mask)          0x0805b894
    Sweep_TraceBoxDispatch                                             0x0805b414
      cm_model_get(type); Sweep_BuildSegment (inlined)  -> fills traceWork (0xb8 bytes)
      cm_scratch_acquire(&tr+0xa0): ++slot.counter, copy 6 words of the per-thread slot into tr+0xa0..0xb7
      start != end:
        type==0     : CM_RecursiveHullCheck(tr,0,start4,end4,result)   -> leaf: cm_leaf_surface_trace
        type==0x3ff : if (tr.tempBrush->contents & mask) Sweep_ClipBoxHalves   (capsule vs temp box)
        else        : cm_leaf_surface_trace(tr, &model->leaf, result)
      start == end (position test):
        type==0     : cm_trace_leaves (box+1 unit -> leaf list -> cm_leaf_trace per leaf until allsolid)
        type==0x3ff : if (tr.tempBrush->contents & mask) cm_trace_aabb_bounds
        else        : if (!result->allsolid) cm_leaf_trace(tr,&model->leaf,result)
  entityNum := (fraction != 1.0) ? 0x3fe : 0x3ff
  if fraction != 0:
     [extraCheck] Area_SweepBoxFromPoints(result,start,end,mask)         (static models, line only)
     zero-extent box -> SpatialTree_TraceSegmentHitsFromRoot  -> SV_ClipMoveToEntity_Line per entity
     else            -> SpatialTree_TraceBoxHitsFromRoot      -> SV_ClipMoveToEntity_Box  per entity
```

`cm_leaf_surface_trace` 0x0805a6ae: if `fraction != 0` and (`mask & leaf.brushContents` -> `cm_leaf_ray_trace` -> `cm_trace_node_ray` over the leaf brush tree -> `cm_brush_trace_planes`) did not end at fraction 0, and `mask & leaf.terrainContents`: for each `CollisionAabbTree` in `[firstCollAabbIndex, +collAabbCount)` call `CM_TraceThroughAabbTree` (material contents filter, `CM_CullBox` pruning, partition de-dup via `partitionChecks`, then `cm_wedge_sphere_sweep` (capsule, called twice if plane.z<0, second time with `-offsetZ`) / `cm_wedge_edge_sweep` (point trace) / `cm_wall_circle_sweep` (borders; only when delta.xy != 0 and offsetZ != 0)). [O]

## 3. Global state

| address | symbol (proposed) | layout / meaning | tag |
| --- | --- | --- | --- |
| 0x08188da8 / dac / db0 | `bspImage`, `bspLength`, `bspChecksum` | file image while loading; cleared by `BspFile_Free` | O |
| 0x08188dc0 (0x110) | `cm` (clipMap_t) | see `collision.structs.json` (`clipMap_t`); contiguous with the per-field globals the chunks name `g_08188dxx` | O |
| 0x08188de0 / de8 / df0 | `cm.nodes`, `cm.leafs`, `cm.brushNodes` | cm+0x20, +0x28, +0x30 | O |
| 0x08188dc4 / dc8 | `cm.numStaticModels`, `cm.staticModels` | cm+0x04, +0x08 (chunk_003 left unknown; chunk_004 names the globals) | O |
| 0x08188dd0 | `cm.materials` | cm+0x10, allocation start + 0x48 (index -1 valid, zeroed spare entry used by the box brush `axialMaterialNum=-1`) | O |
| 0x08188e08/e10/e30/e38/e40 | `cm.verts/edges/aabbTrees/cmodels/brushes` | cm+0x48,+0x50,+0x70,+0x78,+0x80 | O |
| 0x08188e3c | `cm.numBrushes` (u16 at cm+0x7c) | read as int by chunk_005 (`g_08188e3c`); the high half (pad) is zero from memset | O (note) |
| 0x08188e44/e48/e4c/e50 | `numClusters`, `clusterBytes`, `visibility`, `vised` | cm+0x84..0x90 | O |
| 0x08188e58 | `cm.entityString` | pointer to the entity lump copy | O |
| 0x08188e5c / e60 | `cm.box_brush` (pointer), `cm.box_model` (0x48 bytes) | copied per slot by `CM_InitSlotBuffers` | O/I |
| 0x08188ecc | `cm.checksum` | file checksum (BSP) | O |
| 0x08188ed8/edc/ee0/ee4 | `cm_numPlanes`, `cm_planes`, `cmod_base`, `cmod_lumpBuffer` | planes array (0x14 stride, Hunk) ; `cmod_base` only non-NULL during load; lumpBuffer used by the dead `CMod_ReadLump` | O |
| 0x08188eec / ef0 / ef8 | winding live count / peak / float scratch | only touched by the dead winding code | O |
| 0x085b7660 (2 x 0x18) | `cm_slots[2]` (`cm_slot_t`) | per-thread trace scratch; registered through `Com_SetThreadBlocks` 0x080b805e -> `Sys_SetValue(3, slots+tid*0x18)`; read through `Sys_GetValue(3)` | O |
| 0x08189000 / 0c | world mins[3] / maxs[3] | `CM_ModelBounds(0,...)` in `Area_Init` | O |
| 0x08189018 | byte flag | when non-zero `Area_LinkIntoSubtree` does nothing; no writer found in this region (U for who sets it) | O/U |
| 0x0818901a | area free-list head (u16) | init 2 | O |
| 0x0818901c (0x400 x 0x18 = 0x6000) | area nodes | memset range `0x08189000..0x0818f01c` (size 0x601c) in `Area_Init`; node 0 null, 1 root | O |
| 0x0818f016 | `areaNodes[0x3ff].parent` | free-list terminator (chunk_005 calls it an "init word") | I |
| 0x08442618 (0x174 stride) | svEntity array; `0x084424a4` is the same array 1-based | collision-relevant fields in `svEntity_s (collision view)` | O |
| 0x081869c0 (2 x 0xc) | PM trace dispatch table | see 7.2 | O |
| 0x08297484 | `ScrBuf` offset | temp-hunk growing buffer used for brush nodes (chunk_014) | O |
| 0x0814cfc8 | zero vector (rodata) | | O |

Memory model: persistent allocations use `Hunk_Alloc` (`CM_HunkAlloc` ignores its tag/line arguments); `CM_HunkAllocTemp` -> `Hunk_AllocateTempMemoryHigh`; `CM_HunkClearLow` -> `Hunk_ClearTempMemory`; brush nodes use the `ScrBuf_*` growing temp buffer (`Hunk_ReallocateTempMemory`); the winding code uses `Com_MallocZeroed`/`Com_Free_A`. [O]

## 4. Data formats read

### 4.1 BSP container [O]
Little-endian, `ident = 'IBSP'`, `version = 4`, then 39 lump entries `{uint32 length, uint32 offset}` (**length first**, header 0x140 bytes). The whole file is read into memory (not streamed) and offsets are file-absolute. `CM_ReplaceBspLump` (dead) additionally accepts version `0x3d`.

### 4.2 Lumps consumed by collision [O]

| idx | content | file stride | consumer | in-memory |
| --- | --- | --- | --- | --- |
| 0 | materials | 0x48 `{char name[64]; int surfaceFlags; int contents}` | `CMod_LoadMaterials` | dMaterial_t, +1 spare leading element |
| 4 | planes | 0x10 `{float normal[3]; float dist}` | `CMod_LoadPlanes` | cplane_t 0x14, type 0..2 only if component == 1.0 exactly, signbits |
| 5 | brush sides | 8 `{u32 a; i32 materialNum}` | `CMod_LoadBrushes` | per brush: 6 axial pairs `{float coord, int material}` (min x,max x,min y,max y,min z,max z) then `numsides-6` extra `{planeIndex, material}` |
| 6 | brushes | 4 `{short numSides; short materialNum}` | `CMod_LoadBrushes` | cbrush_t; `contents = material.contents & 0xdffffffb`; requires numSides >= 6 |
| 25 | nodes | 0x24 `{planeIdx, child0, child1, +6 ints unread}` | `CMod_LoadNodes` | cnode_t 8 |
| 26 | leafs | 0x24 `{cluster@0, ?@4, firstAabb@8, aabbCount@0xc, firstLeafBrush@0x10, numLeafBrushes@0x14, 3 ints unread}` | `CMod_LoadLeafs`, `CMod_LoadLeafBrushNodes` | cLeaf_t 0x2c (cluster only if loadClusters) |
| 27 | leaf brushes | 4 | `CMod_LoadLeafBrushes` | uint16, throws `"brushIndex exceeded"` if > 0xffff; one spare element allocated for the box brush index |
| 28 | leaf surfaces | 4 | `CMod_LoadLeafSurfaces` | int[]; no reader found in the region (U) |
| 29 | collision verts | 0x10 `{4 unread, xyz}` | | CollisionVertex 0xc |
| 30 | collision edges | 0x38 `{4 unread, 12 floats, float length}` | | CollisionEdge 0x30, axis[2] *= 1/length |
| 31 | collision triangles | 0x48 | | CollisionTriangle 0x48 |
| 32 | collision borders | 0x1c | | CollisionBorder |
| 33 | collision partitions | 0xc `{u16 ?, u8 triCount, u8 borderCount, int firstTri, int firstBorder}` | | pointers |
| 34 | collision aabb trees | 0x20 | | CollisionAabbTree |
| 35 | submodels | 0x30 `{mins[3], maxs[3], ?@0x18..0x1f, firstAabb@0x20, aabbCount@0x24, firstBrush@0x28, numBrushes@0x2c}` | `CMod_LoadSubmodels`, `CMod_LoadSubmodelBrushNodes` | cmodel_t; mins-1 / maxs+1; max 0x3ff submodels ("MAX_SUBMODELS exceeded"); index 0 (world) gets no leaf data |
| 36 | visibility | `{int numClusters; int clusterBytes; data}` | `CMod_LoadVisibility` | empty lump -> all-ones table of `(numClusters+31)&~31` bytes; non-empty sets `vised=1` |
| 37 | entity string | text | `CMod_LoadEntityString` | verbatim copy (numEntityChars=length, no NUL added) |

Each loader checks `length % stride` and errors with `"<fn>: funny lump size"`; many narrowing checks error with `"... exceeded"` (u16/short overflows). Exact strings are at rodata 0xf75e1-0xf7f74 (checked with `strings`). [O]

### 4.3 Entity string use [O]
`cm_load_static_models` 0x080585fe parses `cm.entityString` twice with `Com_Parse`: first counts `classname == "misc_model"` entities whose `model` satisfies `String_IsXModelPath`; second allocates `count * 0x50` (tag "CM_CreateStaticModel") and fills origin (`origin`), angles (`angles`), scale (`modelscale_vec` x y z or `modelscale` uniform, default 1). The name passed to `cm_static_model_init` is `model + 7` while the existence test uses `model` (skips a 7-character prefix, presumably `"xmodel/"` [I]). Key/value are copied with unbounded `strcpy` into 64-byte buffers (overflow possible on long tokens, [O]). Errors: `"Invalid static model name\n"`, `"Static model [%s] has x/y/z scale of 0.0\n"` (all `Com_Error(1, ...)`). A model whose load fails (handle 0) is silently dropped and the count decremented.

## 5. Behaviours a native port must reproduce

### 5.1 Clip map construction
* Lump reads are whole-file in memory; byte order little-endian; all validation errors are fatal `Com_Error(1,...)` with the exact strings (matters for diagnostic parity only). [O]
* **Material indices**: `cm.materials[-1]` must exist (zeroed). `axialMaterialNum` is stored as int16 (so -1 for the box brush). [O]
* **Brush contents mask** `0xdffffffb` applied when copying from material (clears bits 0x20000000 and 0x00000004). [O]
* **Plane type** classification uses exact `== 1.0` on the normal components only (never -1.0); `signbits` from `< 0.0`. Axial plane tests then compute `point[type]-dist`; others a dot product with z,y,x summation order. [O]
* **Leaf bounds** = union of leaf brush bounds, expanded by 0.125 per side (`CMod_BuildLeafBrushTree`); leaves without brushes keep `leafBrushNode=0` and zero mins/maxs. `cm_point_contents` treats `node_index==0` as no contents. [O]
* **Brush kd tree build** must be reproduced because it is not in the file and node order defines relative child offsets (u16, error `"child exceeded"` otherwise): for each axis 0..2, each brush's min and max face is a candidate split `d`; `CMod_ScoreBrushSplit` counts brushes entirely above (`d <= b.mins`) and below (`b.maxs <= d`), score = `min(highFaceMin - mins[axis], maxs[axis] - lowFaceMax) * min(countLow,countHigh)` (0 if `count<1`), and the split plane is *re-centred* to `(lowFaceMax+highFaceMin)/2` through `*dist`; first strictly-greater score wins (`bestScore < score`). If no axis scores > 0 -> leaf node (`leafBrushCount = n`, contents = OR). Otherwise straddlers (`b.mins < d < b.maxs`) are partitioned first and become a subtree at `node+1` (`leafBrushCount=-1`), then high side (`d <= b.mins`) = `child[0]` and low side (`b.maxs <= d`) = `child[1]`; child bounds are clipped by `d ± gap` where `gap` is the smallest distance to a brush face on that side; node `range = gap`; node contents = OR of children and straddle subtree. In-place reordering of the shared `cm.leafBrushes` array is part of the algorithm (leaf slices must not overlap, [U] whether the compiler guarantees it). Node 0 is reserved (indices start at 1; `numBrushNodes = used + 1`). [O]
* The temp-box brush is `cm.brushes[numBrushes]` with `contents=0xffffffff`, no sides, `axialMaterialNum = -1`; its leaf node references `leafBrushes[numLeafBrushes] = numBrushes`. This box leaf is copied into every per-thread tempModel but the traces for handle `0x3ff` never use the leaf (they use the capsule path); the box leaf/brush look vestigial. [I]
* `Area_LinkEntity` for a temp-box handle uses `tempModel.leaf.brushContents|terrainContents`, which is `0xffffffff|0` from the copied box leaf (CM_TempBoxModel only rewrites the brush's contents and the model's mins/maxs), so non-bmodel entities link into the area tree with an all-ones mask and are filtered later by `ent->contents`/`svEntity+0x160`. [I]

### 5.2 Trace setup (`Sweep_BuildSegment` is an inlined helper in two places, the recon invented the name)
Inputs are a box `mins/maxs`. Centre `c=(mins+maxs)/2`, `size=maxs-c`, start/end shifted by `c`, midpoint, half-delta, |half-delta|, `invDelta` (0 when `|delta| < ~epsilon`, via `Box_ComputeInverseExtents`), `lengthSq`, `length`. **radius = min(size.x, size.z)** (note: x, not min(x,y)), `skin = size.z - radius`, `offset = (radius, radius, radius+skin)`. Swept bounds: x,y expanded by `radius`; z expanded by `radius+skin`, with the larger z written back into `end[2]` (**mutating end.z to max(start.z,end.z)** in the binary after bounds are computed, observed in the pseudocode comment and in `0x805b6xx`). `isPoint = (size.x+size.y+size.z == 0)` only when `start != end` (else forced 0). `axialCullOnly = boundsVolume < size.x*size.y*size.z*16*length`. Then `cm_scratch_acquire(&tr+0xa0)` and `contents = mask`. [O]

### 5.3 World BSP walk (`CM_RecursiveHullCheck` 0x0805b058)
Segment is carried as a 4-vector `(x,y,z,t)`; at each node: axial plane -> `d = p[type]-dist`, `extent = size[type] + 0.125`; non-axial -> dot minus dist, `extent = isPoint ? 0.125 : 2048.0`; both sides tested with `min(d0,d1) < extent` / `max > -extent`; split with `|d1-d0| > 4.7683716e-07` (2^-21); near fraction `cut_lo=(num-extent)/|dd|`, far `cut_hi=(num+extent)/|dd|` clamped at 1; early out when `result->fraction <= seg.t`. Same logic (vec3) for the boolean walk with first-non-zero return. The constants 0.125 and 4.76837e-07 are [O]. [O]

### 5.4 Brush trace (`cm_brush_trace_planes` 0x08059dc0) - differs from stock Q3
Two passes: (1) six implicit axial faces first (min faces with `sign=-1`, max faces `+1`) using `trace.offset` and per-face `axialMaterialNum[group][axis]`; (2) real sides with `reach = dist + radius + |normal.z * skin|` (capsule inflation). Enter plane uses `0.125` standoff (`fraction = (d0-0.125)/(d0-d1)`); `startsolid` is set unconditionally when no entering plane, `allsolid` + `fraction=0` only when the start lies inside (`inside` flag). Hit writes `surfaceFlags = material+0x40`, `contents = brush.contents`, `material = &cm.materials[idx]`. Position-test counterpart `cm_brush_box_test` marks `allsolid, startsolid, fraction 0` and writes `brush.contents` when no side separates. [O]

### 5.5 Terrain
* `CM_CullBox`: SAT between the sweep (midpoint, halfDeltaAbs) and the box (origin, halfSize + size) on the 3 axes, plus the 3 cross-product axes when `axialCullOnly == 0`. [O]
* Partition de-dup: `partitionChecks[partitionIndex] == (uint16)checkcount` skip (WORD compare, zero-extended `movzx` at 0x8055b81); per-slot counter is a 32-bit int incremented per trace (wrap at 65536 revisits stale partitions; [I] harmless in practice). Edges/vertices use the full 32-bit stamp. [O]
* Capsule vs triangle: plane offset by `radius+0.125`, z-offset `offsetZ` applied to start/end (`-offsetZ` second pass if plane.z<0); contact on face (barycentric `s>=0,t>=0,s+t<=1`), vertex spheres and edge cylinders; walkable-slope hack: when `plane.n.z >= 0.7`, `hit.normal.z >= 0` and `< 0.7` and the start is above the end, the plane normal is substituted. Borders are 2D walls with `zBase + zSlope*t`. Point traces use only `cm_wedge_edge_sweep` (plane enter with 0.125 standoff, barycentric tolerance -0.001..1.001). [O]
* dot products are summed **z, y, x** in x87 extended precision and cast to float when stored; many traces compare in 80-bit. Bit-exact reproduction requires `long double`/x87 semantics or accepting last-bit differences. [O]

### 5.6 Entities without a clip model (handle 0x3ff) and `trace_t.entityNum`
`SV_ClipHandleForEntity` returns `cm_scratch_stage_segment(ent->mins, ent->maxs, ent->contents)` for non-bmodels, which writes `mins/maxs` into the per-thread tempModel, `contents` into the tempBrush+0xc and returns `0x3ff`. For `0x3ff` the traces are **not** brush traces: a moving trace uses `Sweep_ClipBoxHalves` (two spheres + vertical capsule, `cm_trace_endpoint_sphere` 0x0805a768, `cm_trace_vertical_capsule` 0x0805a95e; radius `min(half x, half z)`, cylinder half-height `half z - radius`), and the position test uses `cm_trace_aabb_bounds`. Entity hits set `entityNum`, `contents = ent->contents`, `surfaceFlags = 0`, `material = NULL`; world hit sets `entityNum 0x3fe` when `fraction != 1.0`, `0x3ff` otherwise. [O]
Defect to avoid when porting from `chunk_005.cpp`: `Sweep_TraceBoxDispatch`/`Sweep_TraceEntityDispatch` treat the `0x3ff` contents pointer as a NULL local (`entity_ref = 0`) so the `0x3ff` branch is dead in the reconstruction; in the binary it is `tr+0xb0` (the per-thread tempBrush). Disassembly 0x805b702-0x805b7e2 (`mov eax,[ebp-0x28]; ... [eax+0xc] & mask`, `ebp-0x28 == tr+0xb0` because `tr` starts at `ebp-0xd8`). [O]

### 5.7 World-sector (area) tree and entity traversals
* Two entities lists per node, in sorted order by entity number: svEntity records (1-based, `node+8`) and static-model records (`node+0x0a`, sorted by index). Node 1 is the root; splitting is 2D, axis = longer of x/y extent (`axis = (ext.x <= ext.y)` -> y on tie), split at the midpoint, children created lazily by `Area_AllocSplitNode` only when the extent is > 512 on that axis and the free list is non-empty (`return 0` otherwise). Children are `[0]` = coordinate greater than split, `[1]` = lesser. `Area_LinkEntity` descends until the entity straddles a plane or the child is missing, ORs `contents` into every visited node's `entityContents`, then `Area_LinkIntoSubtree` pushes down entities/static models wholly on one side (unless the byte at 0x08189018 is set). `Area_UnlinkEntity` frees emptied nodes back to the free list and recomputes `entityContents` up to the root. [O]
* Linking uses 2D bounds (`Vec2_Copy` of mins/maxs xy into svEntity+0x164/+0x16c), `contents = tempModel/cmodel leaf.brushContents | terrainContents`; entities with `contents == 0` are unlinked (`Area_LinkEntity` early out). [O]
* `CM_AreaEntities_r` visits a node when `mask & node.entityContents`; per-entity test is full 3D vs `gentity.absmin/absmax` (+0x120/+0x12c of the game entity) and `gentity.contents (+0x11c) & mask`; number = (svEntity - base)/0x174 - 1; overflow at `maxcount` -> debug print and stop. [O]
* `SpatialTree_TraceBoxHits` (0x0805ef8c): per node, gate `mask & node.entityContents`, then for each entry with `entry.contents(+0x160) & mask` call `SV_ClipMoveToEntity_Box`; the split-plane test widens by `tr->extents[axis]` at +0x18, which `SV_Trace` fills with half-extent **+1**. `SpatialTree_TraceSegmentHits` (0x0805f630) calls `SV_ClipMoveToEntity_Line` for every entry of every visited node (the mask test is inside that function) and splits at planes with no widening. `SpatialTree_TraceBoxFirstHit` / `SpatialTree_QuerySegment` are the boolean variants (first non-zero `SV_ClipBoxHitsEntity`/`SV_ClipLineHitsEntity`); `QuerySegment` recurses into both halves before testing the node's own entries (post-order). [O]
* Static models participate **only in line traces**: `Area_SweepBoxFromPoints` (moving, extraCheck != 0) and `Area_TraceSegmentRecords`/`Collision_IsSegmentClear` (boolean, extraCheck != 0). They use `XModel_TraceLine`, hit flags `0x3fe`, and only when `XModel_GetContents(model) & mask`. Box traces never see static models. [O]

### 5.8 Point contents and clusters
* `cm_point_contents(p, 0)`: leaf by plane-BSP descent; if `leaf.node_index==0` -> 0; strict bounds test `p > mins && p < maxs` (else 0); then brush tree walk ORing `contents` of brushes whose AABB contains p (inclusive) and for which every side satisfies `dot(n,p) <= dist` (inclusive); tree child select `p[axis] <= split -> child[1]` else `child[0]`; straddle subtree (`count<0`) visited first. [O]
* Terrain contents are **not** part of point contents (aabb trees ignored). [O]
* `SV_PointContents` = world OR `cm_model_point_contents` for each area entity (ignores `passEntityNum`), result masked. [O]
* Visibility: `cm_table_entry_address(cluster)` returns the base row (all-ones when the lump is empty, otherwise **row 0's PVS** because the base is `cm.visibility`) for cluster<0 or >= numClusters. This is the observable behaviour for `cluster == -1` leaves. [O]

## 6. Dvars and commands

The collision region registers **no dvars and no console commands** (no call to any `Dvar_*`/`Cmd_*` function in the region; call-target scan of `0x08051e4c-0x0805fc5c` lists only libc, `Com_Error`, `Com_DPrintf`, `Com_BlockChecksum`, `FS_*`, `Hunk_*`, `Com_Parse`, `va`, XModel and math helpers). [O]
Indirect configuration: `gfx_driver` (read by `Gfx_GetBspVariant`, selects `<driver>bsp` vs `d3dbsp`). `g_playerCollisionEjectSpeed` (string at 0x112684) belongs to the game-side `G_PlayerCollisionPush` (chunk 046), not analysed here. [O]
Hard-coded limits: 0x400 area nodes, 512-unit minimum split, 0x3ff submodels, 0x80-leaf query in `SV_LinkEntity`, 0x400 leaf list in `cm_trace_leaves`, 16 clusters per svEntity, 0x40 winding points. [O]

## 7. Inter-subsystem dependencies

### 7.1 Inbound (who calls collision) [O]
`SV_SpawnServer` (load), `Com_Restart_Subsystems`/`Com_ShutdownSubsystems`/`Com_ErrorRecover` (lifecycle), `SV_LinkEntity`/`SV_UnlinkEntity`, `SV_Trace*`, `G_TraceBox`/`G_TracePointEx`/`G_TraceIsClear*` (chunk 049), game code using `CM_AreaEntities`, snapshot visibility (`SV_BuildClientListForPoint`, `SV_BuildCachedEntityListForPoint`, `SV_EntityVisibleToClient`), `SV_InitMapState`/`SV_ResetTokenCursor` (entity string pointer), `FX_InitSampleRecord` (client FX code linked into the server), `SV_EntityRelevantToView`.

### 7.2 PM/bg_pmove dispatch (chunks 041, 045) [O]
`PM_TraceRaw` 0x080e1308 calls `g_081869c0[pm->traceMode].trace(...)`. Table rows (12 bytes) observed in `.data`: row 0 `{0x080f3cfe Trace_BoxWorldThenEntities, 0x080f3dc0 Trace_ContentsWithEntities, 0}`, row 1 `{0x0810a528 G_TraceBox, 0x0809e368 SV_PointContents, 0x080f7b38 G_HandleClientEvent}`. The server's gameplay path uses row 1 (SV_Trace); row 0 traces the world through `Sweep_TraceBoxInit` and then a caller-maintained entity list (`g_08628178` count, `g_08628180` pointers); who sets `traceMode` to 0 is [U]. `PM_Trace` retries without contents `0x2000000` when the (byte at trace+0x23 = **startsolid**, see structs) flag is set and the hit contents had that bit, adding the entity to the touch list. `PM_AddTouchEnt` ignores entity 0x3fe, caps at 32. `Ent_GetTraceMask` defaults to `0x491`.

### 7.3 Outbound [O]
`FS_*` (map file), `Hunk_*`/`ScrBuf_*`/`Com_Malloc*`, `Com_Parse` (entity string), `XModel_FindOrLoad` (with the two Hunk reserve callbacks `CM_HunkReserveA/B` 0x08058148/0x0805815c which are plain `Hunk_Alloc` wrappers), `XModel_ComputeBounds`, `XModel_TraceLine`, `XModel_GetContents`, `XModelRef_GetBlock44`, `String_IsXModelPath`, `Angles_ToVectors`, `AnglesToAxis`, `Mat3_Invert`, `Vec3_RotateByTranspose`, `BoxOnPlaneSide` (leaf collection), `Sys_GetValue(3)`, `SV_GentityNumFromSvEntity` (area code; the symbol table lists it as returning void/"result discarded", but the callers use the returned gentity: it computes `(sv - 0x08442618)/0x174` then `SV_GentityNum`), `SV_ClipMoveToEntity_*`/`SV_Clip*HitsEntity` (callbacks from the `SpatialTree_*` code), `Com_Error`, `Com_DPrintf`.

## 8. Dead or unreferenced code in this region [O]
No `call` instruction targets and no absolute address in `.data`/`.rodata` (only `.eh_frame` FDE entries) for: `BaseWindingForPlane` 0x0805785a, `ClipWinding` 0x08057b1e, `CM_CloneBlock` 0x08057ad6 (only referenced by the former), `cm_vertex_block_bounds` 0x08057766, `Ray_IntersectTriangle` 0x08056006, `CM_ReplaceBspLump` 0x0805228e (its open-for-write uses `FS_ReturnZeroStub` so it always errors "Failed to open file %s for writing"), `CMod_ReadLump` 0x08055076, `CMod_FreeLump` 0x080551e6, `CM_GetCount_08188e34` 0x080526e4, `CM_PlaneFromIndex` 0x0805505e (no caller). A port does not need them for server behaviour. `CM_TraceThroughAabbTreeMasked` is only used by the boolean family.

## 9. Naming problems (wrong / inconsistent names across chunks)

| # | problem | evidence |
| --- | --- | --- |
| N1 | **Same table, different names**: chunk_004 `cm_surface_s`/`g_08188e30`, chunk_005 `cod2_leaf_item_s`, chunk_003 `CollisionAabbTree` are one array (`cm+0x70`); chunk_004/005 call the 0x20-byte records "surfaces"/"items". | `cm_leaf_surface_trace` passes `&g_08188e30[first_surface+i]` to `CM_TraceThroughAabbTree` whose first line indexes `cm.materials[tree->materialIndex]` (chunk_003) |
| N2 | chunk_004 `g_08188dd0` "model array (second table)" and `cm_model_s`-typed `model_index` = **cm.materials** (0x48 stride) | `cm_brush_trace_planes` reads `+0x40` (surfaceFlags) from it; `CMod_LoadMaterials` sets `cm.materials` (cm+0x10 = 0x08188dd0) |
| N3 | chunk_005 `cod2_area_box_s`/"brush box records" (`Area_*Box*`, `g_08188dc8`) = chunk_004 `cm_static_model_s` (cm+8, count cm+4); +4 is `XModel*` not `contents` | `XModel_GetContents(box->contents)`, `cm_static_model_trace(box,...)`; `Area_LinkBoxRecord`; string tag "CM_CreateStaticModel" |
| N4 | chunk_005 `cod2_area_link_s`/chunk_006 `SpatialEntry`/chunk_023 `svEntity_s` are one struct; chunk_005's `mins[3]@0x164,maxs[3]@0x16c` overlap; chunk_023's `int worldSector@0` vs u16 `node,next` | `Vec2_Copy` in `Area_LinkEntity`; array base `0x084424a4+0x174 == 0x08442618` |
| N5 | `cod2_box_s`/"box" in chunk_005 is a **segment** (start,end,invDelta) | callers pass start/end |
| N6 | trace result `+0x22/+0x23`: chunk_003, 041, 045 `startsolid/allsolid`; chunk_004 `all_solid/start_solid` (correct per asm). Consequence: chunk_041 `PM_Trace`/`PM_CorrectAllSolid` test startsolid, chunk_003 `CM_PositionTestInLeaf` tests allsolid. | asm 0x805a15c, 0x805a169; `cmp byte [eax+0x23]` at 0x80e13b8, 0x80e3562 |
| N7 | chunk_023 `trace_s` fields `dist@0x10`, `field_18@0x18` = surfaceFlags, material pointer; chunk_045 `trace_s` has `endpos@4`, `normal@0x10` (conflicts with all others) | `SV_ClipMoveToEntity_Line` zeroes 0x10/0x18/0x1e/0x20 on entity hit; `cm_brush_trace_planes` writes normal at +4 |
| N8 | `CM_GetCount_08188e58` returns the entity-string pointer, not a count; `CM_GetCount_08188e34` is `numSubModels` | cm+0x98, cm+0x74 |
| N9 | `CM_GetRecordShort28` = leaf cluster (CM_LeafCluster); `cm_table_entry_address` = CM_ClusterPVS; `cm_scratch_stage_segment` = CM_TempBoxModel; `cm_scratch_acquire`/`cm_scratch_get_pair` are per-thread slot access; `cm_block_alloc/free` = AllocWinding/FreeWinding (string "FreeWinding: freed a freed winding") | callers and string at 0x8140020 |
| N10 | chunk_002 `BspFile_Load` names lump words `off`/`len` swapped (first word is length) | `CMod_ReadLump`, `CM_ReplaceBspLump` |
| N11 | chunk_002 `cm_map_s` "stride 0x2c rows reached through g_08188de8" = `cm.leafs` | cm+0x28 |
| N12 | `CM_PositionTestInLeaf` 0x08055f50 (chunk_003) only tests the terrain aabb trees; the brush position test is `cm_leaf_trace`/`cm_leaf_box_brush_trace`/`cm_node_trace`/`cm_brush_box_test` (chunk_004). Same word "PositionTest" for halves of one operation; and `CM_TraceThroughAabbTree[Masked]` (chunk_003) is the target of calls typed differently in chunks 004/005. | call graph |
| N13 | Duplicate symbol names for different addresses: `Float_Max` 0x08055216 / 0x0805d5ce / 0x0805fb58 (+ `Float_MaxAlias` 0x0805da56), `Float_Min` three times, `Float_Sqrt` 0x080553b2 / 0x0805d596, `Float_Abs` x3, `Vec3_Copy` x3 (+ `VectorCopy_*`, `cm_vec3_copy*`), `LittleLong` 0x0805208a / 0x0805536c, `Vec4_Copy` x2 | `analysis/symbols/chunk_*.json`; these are per-TU copies of identical inline helpers, a flat C++ port needs address suffixes or a single shared definition |
| N14 | `Area_*`/`CM_AreaEntities*` (chunk_005) and `SpatialTree_*` (chunk_006) traverse the same tree; chunk_006 `maskA/maskB/entryHead/entryHeadAlt` = chunk_005 `brush_contents(static models)/entity_contents/entity_head/brush_head` (brush == static model) | node layout 0x18 |
| N15 | `CM_TraceKdTree`, `Sweep_TraceLeafKdTree`, `cod2_kdnode_s` call the brush node tree a kd-tree (it is a one-axis-per-node partition with straddle lists; "kd" is acceptable, "kdnode"=cbrushnode_t) while chunk_004 `cm_node_s` is the same struct; do not confuse with the plane BSP | struct reconciliation |
| N16 | `CM_HunkReserveA/B` are `XModel_FindOrLoad` allocation callbacks, not collision functions; tagged `collision/memory` | only referenced via `cm_static_model_load_handle` |
| N17 | `symbols.json` rows `Sys_SetValue` address `0x0080d6bae` (extra zero) vs `Sys_GetValue` `0x080d6bc0` | `analysis/symbols/chunk_038.json` |
| N18 | `Sweep_*` names (chunk_005) are descriptive only: `Sweep_TraceBoxInit` = CM_BoxTrace-with-init, `Sweep_TraceBoxDispatch` = CM_BoxTrace body, `Sweep_ClipBoxHalves` = capsule-vs-capsule temp-model trace, `Sweep_TraceEntityDispatch` = sight trace; `cm_trace_aabb_bounds` = capsule position test for the temp model; `cm_trace_endpoint_sphere`/`cm_trace_vertical_capsule` = cap/body tests of that capsule | call graph and asm |
| N19 | `g_0818f016` "area init word" is `areaNodes[0x3ff].parent` | memset range, init loop `i<0x3ff` |

Chunk-local type names are all different for the same objects (`cm_trace_s`/`cod2_trace_s`/`traceWork_t`, `cm_hit_s`/`cod2_trace_result_s`/`trace_t`, `cm_leaf_s`/`cod2_leaf_s`/`cLeaf_t`); see `collision.structs.json` for the reconciled layouts and each contradiction.

## 10. Reconstruction defects found (chunks vs binary)

| id | where | defect | evidence |
| --- | --- | --- | --- |
| R1 | `chunk_005.cpp Sweep_BuildSegment` | `cm_scratch_acquire(work)` with `unsigned char work[16]`; the real call passes `&tr+0xa0` and the callee writes 24 bytes, filling the slot copy that the traces read (checkcount, stamp arrays, partitionChecks, tempBrush, tempModel). A literal compile overruns the stack and leaves tr+0xa0.. uninitialised | asm 0x805b6bf-0x805b6cd; callee 0x08059270 copies 6 words |
| R2 | `chunk_005.cpp Sweep_TraceBoxDispatch`, `Sweep_TraceEntityDispatch` | `entity_ref` is a never-assigned local (NULL) so the `0x3ff` capsule paths are never taken; real source is `tr.tempBrush` (`tr+0xb0`) | asm at 0x805b6fc-0x805b7e2 |
| R3 | `chunk_005.cpp Area_TraceSegmentRecords` | computes `mid = seg + (start3 - seg) * frac`; Ghidra pseudocode `.local/cod2/decomp/functions/0x805ecd4.c` lines 62-64 use `param_4` (the end point) | pseudocode |
| R4 | `chunk_004.cpp cm_trace_vertical_capsule` | the "already inside" branch reads an uninitialised local (`local_14`), recon substitutes 0.0 (documented in the chunk as OPEN) | chunk comment |
| R5 | `chunk_003_types.h traceWork_t.checkcount` | `short` at 0xa0; binary reads WORD with `movzx` for partitions and 32-bit for stamps | asm 0x8055b81 |
| R6 | `chunk_005_types.h` `cod2_area_link_s` comment `mins[3]/maxs[3]` | impossible overlap, see N4 | |

## 11. Open questions [U]

1. Who sets the byte at `0x08189018` (disables `Area_LinkIntoSubtree`)? No writer inside the region; may be a debug/benchmark switch.
2. `cm.leafSurfaces` (lump 28) and `cm.numLeafSurfaces` have no reader in the region; do other subsystems (snapshot/visibility, DObj) use them?
3. Meaning of file bytes not read: leaf `@4`, `@0x18..0x23`; node `+0xc..0x23`; collision-vert/edge leading 4 bytes; submodel `@0x18..0x1f`; partition `@0..1`. Possibly bounds, area ids or padding.
4. `trace_t` words at `0x1e`/`0x20` (copied from the DObj trace record at `cmtrace_s+0x14/+0x16`): probably the hit part/surface-type ids; unconfirmed.
5. Does the compiler guarantee that per-leaf `leafBrushes` slices do not overlap (the brush tree build reorders them in place)? A shared slice would make results depend on leaf order.
6. `trace->fraction` ties and ordering between brush, terrain, entity and static-model hits (not exercised statically).
7. Origin of `traceMode == 0` rows and users of `Trace_BoxWorldThenEntities`/`Trace_SweepEntities` on a dedicated server (probably shared client prediction code).
8. `XModel_TraceLine` (chunk 034) detailed result convention (`rc >= 0` = hit, sign bit used by `cm_static_model_test`) and whether it writes `hit->normal` in model space (the code rotates back).
9. Exact numeric behaviour: how much of the x87 80-bit intermediate behaviour (`long double` dot products, `fucompp`-based NaN-aware compares like `diff != diff`) is observable in practice; `Float_*` helpers are mostly branches on `0.0 <= x` that treat NaN as the false arm.
10. Behaviour of `Hunk_Alloc` zeroing: the reserved brush node 0, partition `pad`, and `Area` free list rely on zeroed memory; `cm_load_static_models` relies on it for `next` link initialisation.
11. Whether `CM_LoadMap`'s same-name short-circuit can ever be hit on a restart (`SV_SpawnServer` calls `BspFile_Load` then `CM_LoadMap`; `Com_Restart_Subsystems` calls `CM_ClearMap`, so a `map_restart` through the fast path may reuse the loaded clip map); not traced.
