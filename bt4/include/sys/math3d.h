#ifndef SYS_MATH3D_H
#define SYS_MATH3D_H

#include "types.h"

/* A vector as the VU0 routines (Vec3_xxx / Vec4_xxx at 0x121E28..) take it: always four floats.
   Locals of this type sit 0x10 apart on the stack, and "Vec3" routines just ignore w. */
typedef struct Vec4 {
    /* 0x00 */ float x;
    /* 0x04 */ float y;
    /* 0x08 */ float z;
    /* 0x0C */ float w;
} Vec4; /* size 0x10 */

#ifdef PORT
/* A Vec4 passed by value, as the effect code's own vector types are (EftXVec and friends: 16-byte aligned). The
   declarations that call those functions from other files use this, not Vec4: on AArch64 a 16-byte aligned argument
   goes in an even pair of registers, a 4-byte aligned one in the next free pair, so the two do not match there. */
typedef struct PortVec4 {
    float x, y, z, w;
} __attribute__((aligned(16))) PortVec4;
#endif

/* Unit quaternion, vector part first. Copied with Vec4_Copy. */
typedef struct Quat {
    /* 0x00 */ float x;
    /* 0x04 */ float y;
    /* 0x08 */ float z;
    /* 0x0C */ float w;
} Quat; /* size 0x10 */

/* 4x4 matrix, m[row][col], row-vector convention (v' = v * M): the rotation built by
   Quat_ToMtx has m[0][1] = 2(xy + wz). Translation would be row 3. */
typedef struct Mtx44 {
    /* 0x00 */ float m[4][4];
} Mtx44; /* size 0x40 */

void Vec3_Hermite(Vec4 *out, Vec4 *p0, Vec4 *p1, Vec4 *t0, Vec4 *t1, float t);
void Quat_SetIdentity(Quat *q);
void Quat_FromAxisAngle(Quat *out, float x, float y, float z, float angle);
void Quat_FromVectors(Quat *out, Vec4 *from, Vec4 *to, float t);
float Quat_LengthSq(Quat *q);
float Quat_Length(Quat *q);
void Quat_SlerpIdentity(Quat *out, Quat *q, float t);
float Quat_GetAngle(Quat *q);
void Quat_Conjugate(Quat *out, Quat *q);
void Quat_Inverse(Quat *out, Quat *q);
void Quat_Mul(Quat *out, Quat *a, Quat *b);
void Quat_Slerp(Quat *out, Quat *a, Quat *b, float t);
void Quat_ConjugateBy(Quat *out, Quat *q, Quat *r);
void Quat_LimitAngle(Quat *out, Quat *q, float maxAngle);
void Quat_Normalize(Quat *out, Quat *q);
u64 Quat_Pack(Quat *q);
void Quat_Unpack(Quat *out, u64 packed);
void Quat_ToMtx(Mtx44 *out, Quat *q);
void Quat_FromEuler(Quat *out, Vec4 *angles);
void Mtx_ToEuler(Vec4 *out, Mtx44 *m);
void Quat_ToEuler(Vec4 *out, Quat *q);
void Quat_FromMtx(Quat *out, Mtx44 *m);

#endif
