/* Chunk 026 (0x080a79c6..0x080ab702): math library (vec/quat/mat/angle/bounds), RNG, com_memory wrappers. */
#ifndef COD2_CHUNK_026_TYPES_H
#define COD2_CHUNK_026_TYPES_H

#include "cod2_types.h"

/* Q3-style collision plane (offsets recovered from BoxOnPlaneSide / Plane_SetSignbits). */
/* cplane_s: promoted to cod2_types.h */
COD2_ASSERT_SIZE(cplane_s, 0x14);
COD2_ASSERT_OFFSET(cplane_s, dist, 0x0c);
COD2_ASSERT_OFFSET(cplane_s, type, 0x10);
COD2_ASSERT_OFFSET(cplane_s, signbits, 0x11);

/* Quaternion + translation record used by Vec3_TransformByQuatTransform / QuatTransform_ToMat3. */
struct QuatTransform_s {
    float quat[4];     /* 0x00 x,y,z,w */
    float origin[3];   /* 0x10 */
    float quatScale;   /* 0x1c factor multiplied into quat xyz (probably 2/|q|^2) */
};
COD2_ASSERT_SIZE(QuatTransform_s, 0x20);
COD2_ASSERT_OFFSET(QuatTransform_s, origin, 0x10);
COD2_ASSERT_OFFSET(QuatTransform_s, quatScale, 0x1c);

#endif
