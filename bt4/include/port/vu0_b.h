#ifndef PORT_VU0_B_H
#define PORT_VU0_B_H

/*
 * Portable reference of the vector / matrix library, second half (original 0x121008..0x122940).
 *
 * NOT part of the matching build. Every function is named Ref_<original symbol> and mirrors the original's
 * arithmetic operation by operation; see the comment above each one in src/port/vu0_b.c. Build with
 * -ffp-contract=off (no fused multiply-add), -fno-strict-aliasing and without x87 excess precision.
 */

#include <stdint.h>

/*
 * Types. The names differ from include/port/vu0_a.h on purpose (that header defines RefVec4 as a union of float /
 * integer arrays and may still change); the layouts are identical, so a pointer to one can be cast to the other.
 */
typedef struct RefVec4f {
    float x, y, z, w;
} RefVec4f; /* 0x10 */

typedef struct RefVec4i {
    int32_t x, y, z, w;
} RefVec4i; /* 0x10: what the float-to-integer routines write */

typedef struct RefMtx44f {
    RefVec4f row[4];
} RefMtx44f; /* 0x40: v' = row0 * v.x + row1 * v.y + row2 * v.z + row3 * v.w */

/* The polygon clipper's vertex. */
typedef struct RefClipVtx {
    RefVec4f pos; /* 0x00 */
    RefVec4f uv;  /* 0x10 */
    RefVec4f col; /* 0x20 */
} RefClipVtx;     /* 0x30 */

/*
 * VU0 state that outlives a call: vf0 = (0,0,0,1); vf1..3 = (0,0,1,0), (0,1,0,0), (1,0,0,0), set at boot (0x120088)
 * and read by the routines as constants; vf16..19 the current matrix; vf20..23, vf24..27, vf28..31 the three camera
 * matrices; VU0 data memory (the matrix stack) and vi15 (its top, in quadwords). The other registers are scratch: a
 * few routines store a component they never wrote ("stale"), which the reference reads from the same state.
 *
 * By default vu0_b.c shares the state, the arithmetic primitives (RefVu0_Add / Sub / Mul / Div / SqrtBits) and the
 * rotation routines of the first half through include/port/vu0_a.h: it uses only gRefVu0.vf[], gRefVu0.vi[15],
 * gRefVu0.mem[], Ref_Vu0Cur_RotateZXY and Ref_Mtx_RotateZXY from it.
 * With REF_VU0_B_STANDALONE it uses the state below and its own primitives (REF_VU0_STRICT selects the VU0 number
 * model), and the two rotation routines must be supplied under the names Ref_func_00120F00 / Ref_func_001204B8.
 */
#ifdef REF_VU0_B_STANDALONE
typedef struct RefVu0BState {
    RefVec4f vf[32];
    RefVec4f mem[256];
    uint16_t vi15;
} RefVu0BState;
extern RefVu0BState gRefVu0B;
extern void Ref_func_00120F00(const RefVec4f *angles);                                  /* Vu0Cur_RotateZXY */
extern void Ref_func_001204B8(RefMtx44f *out, const RefMtx44f *m, const RefVec4f *angles); /* Mtx_RotateZXY */
#endif

/* The game's libm (newlib, software) and src/sys/mathf.c, src/sys/randf.c, src/sys/rand.c: the port must supply the same bits. */
extern float Ref_sinf(float x);
extern float Ref_cosf(float x);
extern float Ref_atan2f(float y, float x);
extern float Ref_Mathf_Sin(float angle);
extern float Ref_Mathf_Cos(float angle);
extern void Ref_Rand_SeedFloat(float seed);
extern void Ref_Rand_Init(void);

/* 0x121008 .. 0x121240 */
void Ref_Vu0Cur_RotEulerMulVec4(RefVec4f *out, const RefVec4f *angles, const RefVec4f *v);
void Ref_Vu0Cur_RotEulerMulVec3(RefVec4f *out, const RefVec4f *angles, const RefVec4f *v);
void Ref_Vu0Cur_ProjectInt(RefVec4i *out, const RefVec4f *v);
int Ref_Vu0Cur_ProjectPoint(RefVec4i *out, const RefVec4f *v);
int Ref_Vu0Cur_ProjectPoints(RefVec4i *out, const RefVec4f *v, int n);
int Ref_Vu0Cur_ProjectPointStq(RefVec4i *out, RefVec4f *stq, const RefVec4f *v, const RefVec4f *uv);
int Ref_Vu0Cur_ProjectPointsStq(RefVec4i *out, RefVec4f *stq, const RefVec4f *v, const RefVec4f *uv, int n);

/* 0x1212D8 .. 0x121438 */
void Ref_Vu0Clip_LoadMtx(const RefMtx44f *m);
void Ref_Vu0Clip_StoreMtx(RefMtx44f *m);
void Ref_Vu0Clip_SetMulMtx(const RefMtx44f *a, const RefMtx44f *b);
void Ref_Vu0Screen_LoadMtx(const RefMtx44f *m);
void Ref_Vu0Screen_StoreMtx(RefMtx44f *m);
void Ref_Vu0Screen_SetMulMtx(const RefMtx44f *a, const RefMtx44f *b);
void Ref_Vu0View_LoadMtx(const RefMtx44f *m);
void Ref_Vu0View_StoreMtx(RefMtx44f *m);
void Ref_Vu0View_SetMulMtx(const RefMtx44f *a, const RefMtx44f *b);

/* 0x1214A0 .. 0x121948 */
void Ref_Vu0View_SetRotTrans(const RefVec4f *rot, const RefVec4f *trans);
void Ref_Vu0View_ApplyTransRot(const RefVec4f *rot, const RefVec4f *trans);
void Ref_Vu0Cur_RotateYXZ(const RefVec4f *rot);
void Ref_Vu0_Stub0(void);
void Ref_Vu0_Stub1(void);
void Ref_Vu0_Stub2(void);
void Ref_Vu0_Stub3(void);
void Ref_Vu0_Stub4(void);
void Ref_Vu0_Stub5(void);
void Ref_Vu0_Stub6(void);
void Ref_Vu0_Stub7(void);

/* 0x121950 .. 0x121D48 */
void Ref_ClipVtx_Set(RefClipVtx *vtx, const RefVec4f *pos, const RefVec4f *uv, const RefVec4f *col);
void Ref_ClipVtx_Copy(RefClipVtx *dst, const RefClipVtx *src);
void Ref_ClipVtx_SetArray(RefClipVtx *vtx, const RefVec4f *pos, const RefVec4f *uv, const RefVec4f *col, int n);
void Ref_ClipVtx_CopyArray(RefClipVtx *dst, const RefClipVtx *src, int n);
int Ref_ClipPoly_ClipPlane(RefClipVtx *poly, const RefVec4f *plane, int n);
void Ref_ClipVtx_Lerp(RefClipVtx *out, const RefClipVtx *a, const RefClipVtx *b, float t);
float Ref_ClipPlane_EdgeParam(const RefVec4f *plane, const RefClipVtx *a, const RefClipVtx *b, float dist);
void Ref_ClipPlane_DistArray(RefVec4f *dist, const RefClipVtx *poly, const RefVec4f *plane, int n);
void Ref_ClipPoly_ProjectMtx(RefVec4i *scr, RefVec4f *stq, const RefMtx44f *m, const RefClipVtx *poly, int n);
void Ref_ClipPoly_ProjectCur(RefVec4i *scr, RefVec4f *stq, const RefClipVtx *poly, int n);

/* 0x121DA8 .. 0x122030 */
void Ref_Vu0_Init(void);
void Ref_Vu0_CheckState(void);
void Ref_Vec4_SetZeroW1(RefVec4f *v);
void Ref_Vec4_SetZero(RefVec4f *v);
void Ref_Vec4_Set(RefVec4f *v, float x, float y, float z, float w);
void Ref_Vec3_Set(RefVec4f *v, float x, float y, float z);
void Ref_Vec3_Normalize(RefVec4f *out, const RefVec4f *v);
void Ref_Vec4_Swap(RefVec4f *a, RefVec4f *b);
void Ref_Vec4_Add(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec3_Add(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec4_Sub(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec3_Sub(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec4_Mul(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec3_Mul(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec4_Scale(RefVec4f *out, const RefVec4f *v, float s);
void Ref_Vec3_Scale(RefVec4f *out, const RefVec4f *v, float s);
void Ref_Vec4_Div(RefVec4f *out, const RefVec4f *v, float d);
void Ref_Vec3_Div(RefVec4f *out, const RefVec4f *v, float d);
void Ref_Vec4_Copy(RefVec4f *dst, const RefVec4f *src);
void Ref_Vec3_Copy(RefVec4f *dst, const RefVec4f *src);
void Ref_Mtx_MulVec4(RefVec4f *out, const RefMtx44f *m, const RefVec4f *v);
void Ref_Mtx_MulVec3(RefVec4f *out, const RefMtx44f *m, const RefVec4f *v);
void Ref_Vec4_RotateEuler(RefVec4f *out, const RefVec4f *angles, const RefVec4f *v);

/* 0x122088 .. 0x1222D8 */
float Ref_Vec3_Dot(const RefVec4f *a, const RefVec4f *b);
void Ref_Vec3_Cross(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);
void Ref_Vec4_ToFixed12(RefVec4i *out, const RefVec4f *v);
void Ref_Vec4_ToFixed4(RefVec4i *out, const RefVec4f *v);
void Ref_Vec4_ToInt(RefVec4i *out, const RefVec4f *v);
void Ref_Vec4_ToFixed4XY(RefVec4i *out, const RefVec4f *v);
void Ref_Vec4_Clamp(RefVec4f *out, const RefVec4f *v, float lo, float hi);
void Ref_Vec3_Clamp(RefVec4f *out, const RefVec4f *v, float lo, float hi);
void Ref_Vec4_Lerp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float t);
void Ref_Vec3_Lerp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float t);
float Ref_Vec3_Length(const RefVec4f *v);
float Ref_Vec3_LengthSq(const RefVec4f *v);
float Ref_Vec3_Dist(const RefVec4f *a, const RefVec4f *b);
float Ref_Vec3_DistSq(const RefVec4f *a, const RefVec4f *b);
void Ref_Vec3_DirToEuler(RefVec4f *out, const RefVec4f *dir);
void Ref_Vec3_DiffToEuler(RefVec4f *out, const RefVec4f *a, const RefVec4f *b);

/* 0x122310 .. 0x122868 */
void Ref_Mtx_ProjectInt(RefVec4i *out, const RefMtx44f *m, const RefVec4f *v);
int Ref_Mtx_ProjectPoint(RefVec4i *out, const RefMtx44f *m, const RefVec4f *v);
int Ref_Mtx_ProjectPoints(RefVec4i *out, const RefMtx44f *m, const RefVec4f *v, int n);
int Ref_Mtx_ProjectPointStq(RefVec4i *out, RefVec4f *stq, const RefMtx44f *m, const RefVec4f *v, const RefVec4f *uv);
int Ref_Mtx_ProjectPointsStq(RefVec4i *out, RefVec4f *stq, const RefMtx44f *m, const RefVec4f *v, const RefVec4f *uv, int n);
void Ref_Vu0_Stub8(void);
void Ref_Vec3_AddSub(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, const RefVec4f *c);
void Ref_Vec3_SubAdd(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, const RefVec4f *c);
void Ref_Vec3_ScaleAdd(RefVec4f *out, const RefVec4f *dir, const RefVec4f *base, float s);
void Ref_Vec3_ScaleSub(RefVec4f *out, const RefVec4f *dir, const RefVec4f *base, float s);
void Ref_Vec3_Add4(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, const RefVec4f *c, const RefVec4f *d);
void Ref_Vec4_AddClamp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float lo, float hi);
void Ref_Vec3_AddClamp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float lo, float hi);
void Ref_Vec3_RotateAxis(RefVec4f *out, const RefVec4f *v, const RefVec4f *axis, float angle);
void Ref_Vec3_RotateX(RefVec4f *out, const RefVec4f *v, float angle);
void Ref_Vec3_RotateY(RefVec4f *out, const RefVec4f *v, float angle);

/* The VU0 R register (random numbers) as src/sys/randf.c uses it; model from emulator documentation, see vu0_b.c. */
void Ref_Vu0R_Init(uint32_t *r, float seed);
float Ref_Vu0R_Next(uint32_t *r);
float Ref_Rand_Float01_Model(uint32_t *r);

#endif
