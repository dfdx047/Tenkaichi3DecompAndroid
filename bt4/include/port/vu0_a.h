#ifndef PORT_VU0_A_H
#define PORT_VU0_A_H

/*
 * Portable C reference of the vector / matrix library, first half (main executable 0x11FA10..0x121008).
 * NOT part of the matching build. Every routine mirrors the original instruction by instruction on a model of the
 * VU0 register file (gRefVu0), so component masks, aliasing, register residue and the matrix stack behave as on the
 * console. All float arithmetic goes through the RefVu0_xxxBits primitives (src/port/vu0_a.c), which is the one
 * place where the PS2's non-IEEE behaviour is decided.
 *
 * Names are the symbol names of config/symbols/vu0_a.txt with a Ref_ prefix.
 */

#include <stdint.h>

/* Four words; float and integer views of the same storage (lqc2 / sqc2 / lq / sq move raw bits). */
typedef union RefVec4 {
    uint32_t u[4];
    int32_t i[4];
    float f[4];
} RefVec4;

/* 4x4 matrix, four rows of 0x10 bytes (row 3 = translation). */
typedef struct RefMtx44 {
    RefVec4 r[4];
} RefMtx44;

/* The part of the machine the library touches. */
typedef struct RefVu0State {
    RefVec4 vf[32];    /* vf0 = (0,0,0,1) hard-wired; vf1-3 = unit vectors after Ref_Vu0_InitAxisRegs; vf16-19 = current matrix */
    RefVec4 acc;       /* ACC */
    uint32_t q;        /* Q (bits) */
    uint16_t vi[16];   /* vi15 = matrix stack pointer, in rows */
    uint32_t status;   /* status flags (control register 16): bit 0 Z, bit 1 S, bit 6 sticky Z, bit 7 sticky S */
    RefVec4 mem[256];  /* VU0 data memory, 4 KB: 64 matrices */
    RefVec4 gpr[8];    /* EE $t0..$t7, 128 bits each: the hand-written routines move rows through them */
} RefVu0State;

extern RefVu0State gRefVu0;

/* ---- arithmetic primitives (bit patterns in, bit pattern out); see the notes at the top of vu0_a.c ---- */
uint32_t RefVu0_AddBits(uint32_t a, uint32_t b);
uint32_t RefVu0_SubBits(uint32_t a, uint32_t b);
uint32_t RefVu0_MulBits(uint32_t a, uint32_t b);
uint32_t RefVu0_DivBits(uint32_t a, uint32_t b);  /* a / b */
uint32_t RefVu0_SqrtBits(uint32_t a);             /* sqrt(|a|) */
uint32_t RefVu0_ItofBits(int32_t v, int fracBits);
int RefVu0_LtBits(uint32_t a, uint32_t b);        /* a < b */
float RefVu0_Add(float a, float b);
float RefVu0_Sub(float a, float b);
float RefVu0_Mul(float a, float b);
float RefVu0_Div(float a, float b);

/* Clears the model and sets vf0. Not a game routine. */
void Ref_Vu0_ResetState(void);

/* ---- integer vectors, 0x11FA10..0x11FE80 ---- */
void Ref_IVec4_SetZeroW1(RefVec4 *v);
void Ref_IVec4_SetZero(RefVec4 *v);
void Ref_IVec4_Set(RefVec4 *v, int32_t x, int32_t y, int32_t z, int32_t w);
void Ref_IVec3_Set(RefVec4 *v, int32_t x, int32_t y, int32_t z);
void Ref_IVec4_Swap(RefVec4 *a, RefVec4 *b);
void Ref_IVec4_Add(RefVec4 *out, RefVec4 *a, RefVec4 *b);
void Ref_IVec3_Add(RefVec4 *out, RefVec4 *a, RefVec4 *b);
void Ref_IVec4_Sub(RefVec4 *out, RefVec4 *a, RefVec4 *b);
void Ref_IVec3_Sub(RefVec4 *out, RefVec4 *a, RefVec4 *b);
void Ref_IVec4_Mul(RefVec4 *out, RefVec4 *a, RefVec4 *b);
void Ref_IVec3_Mul(RefVec4 *out, RefVec4 *a, RefVec4 *b);
void Ref_IVec4_Scale(RefVec4 *out, RefVec4 *a, int32_t s);
void Ref_IVec3_Scale(RefVec4 *out, RefVec4 *a, int32_t s);
void Ref_IVec4_Div(RefVec4 *out, RefVec4 *a, int32_t s);
void Ref_IVec3_Div(RefVec4 *out, RefVec4 *a, int32_t s);
void Ref_IVec4_Copy(RefVec4 *out, RefVec4 *in);
void Ref_IVec3_Copy(RefVec4 *out, RefVec4 *in);
void Ref_IVec4_ToFloat12(RefVec4 *out, RefVec4 *in);
void Ref_IVec4_ToFloat4(RefVec4 *out, RefVec4 *in);
void Ref_IVec4_ToFloat(RefVec4 *out, RefVec4 *in);
void Ref_IVec4_Clamp(RefVec4 *out, RefVec4 *in, int32_t lo, int32_t hi);
void Ref_IVec3_Clamp(RefVec4 *out, RefVec4 *in, int32_t lo, int32_t hi);
void Ref_IVec_Stub(void);

/* ---- screen-range tests, 0x11FE80..0x11FFE8 ---- */
int32_t Ref_IVec4_InGsRange(RefVec4 *p);
int32_t Ref_IVec4_InGsRange3(RefVec4 *a, RefVec4 *b, RefVec4 *c);
int32_t Ref_IVec4_InGsRange4(RefVec4 *a, RefVec4 *b, RefVec4 *c, RefVec4 *d);

/* ---- sine / cosine and constants, 0x11FFE8..0x120098 ---- */
void Ref_Vu0_SinCos(float angle); /* result in gRefVu0.vf[12]: x = sin, y = cos */
void Ref_Vu0_InitAxisRegs(void);

/* ---- matrices in memory, 0x120098..0x120A80 ---- */
void Ref_Mtx_StoreIdentity(RefMtx44 *m);
void Ref_Mtx_StoreIdentityRot(RefMtx44 *m);
void Ref_Mtx_ClearTrans(RefMtx44 *m);
void Ref_Mtx_GetTrans(RefVec4 *v, RefMtx44 *m);
void Ref_Mtx_AxisZToEuler(RefVec4 *angles, RefMtx44 *m);
void Ref_Mtx_SetTrans(RefMtx44 *m, RefVec4 *v);
void Ref_Mtx_Translate(RefMtx44 *dst, RefMtx44 *src, RefVec4 *v);
void Ref_Mtx_TranslateLocal(RefMtx44 *dst, RefMtx44 *src, RefVec4 *v);
void Ref_Mtx_Mul(RefMtx44 *dst, RefMtx44 *a, RefMtx44 *b);
void Ref_Mtx_Copy(RefMtx44 *dst, RefMtx44 *src);
void Ref_Mtx_Transpose(RefMtx44 *dst, RefMtx44 *src);
void Ref_Mtx_InverseRT(RefMtx44 *dst, RefMtx44 *src);
void Ref_Mtx_RotateZ(RefMtx44 *dst, RefMtx44 *src, float angle);
void Ref_Mtx_RotateX(RefMtx44 *dst, RefMtx44 *src, float angle);
void Ref_Mtx_RotateY(RefMtx44 *dst, RefMtx44 *src, float angle);
/* 0x240C68..0x240DB8: the stage module's in-place rotations of the current matrix (vf16..vf19). */
void Ref_StgVu_RotateZ(float angle);
void Ref_StgVu_RotateX(float angle);
void Ref_StgVu_RotateY(float angle);
void Ref_Mtx_RotateZXY(RefMtx44 *dst, RefMtx44 *src, RefVec4 *angles);
void Ref_Mtx_RotateXYZ(RefMtx44 *dst, RefMtx44 *src, RefVec4 *angles);
void Ref_Mtx_ScaleDiag(RefMtx44 *dst, RefMtx44 *src, RefVec4 *v);
void Ref_Mtx_ScaleDiagUniform(RefMtx44 *dst, RefMtx44 *src, float s);
void Ref_Mtx_RotateZXYTranslate(RefMtx44 *dst, RefMtx44 *src, RefVec4 *angles, RefVec4 *pos);
void Ref_Mtx_MakeCamera(RefMtx44 *m, RefVec4 *pos, RefVec4 *zdir, RefVec4 *ydir);
void Ref_Mtx_SetRows(RefMtx44 *m, RefVec4 *r0, RefVec4 *r1, RefVec4 *r2, RefVec4 *r3);
void Ref_Mtx_MakeNormalLight(RefMtx44 *m, RefVec4 *l0, RefVec4 *l1, RefVec4 *l2);
void Ref_Mtx_MakeViewScreen(RefMtx44 *m, float scrz, float ax, float ay, float cx, float cy, float zmin, float zmax,
                            float nearz, float farz);
void Ref_Mtx_MakeDropShadow(RefMtx44 *m, RefVec4 *lp, float a, float b, float c, int32_t mode);

/* ---- the current matrix (vf16-19) and its stack, 0x120A80..0x121008 ---- */
void Ref_Vu0Cur_Init(void);
void Ref_Vu0Cur_LoadIdentity(void);
void Ref_Vu0Cur_Push(void);
void Ref_Vu0Cur_Pop(void);
void Ref_Vu0Cur_PopN(int32_t n);
void Ref_Vu0Cur_ResetStack(void);
int32_t Ref_Vu0Cur_GetStackDepth(void);
int32_t Ref_Vu0Cur_IsStackUsed(void);
void Ref_Vu0Cur_LoadIdentityRot(void);
void Ref_Vu0Cur_ClearTrans(void);
void Ref_Vu0Cur_LoadMtx(RefMtx44 *m);
void Ref_Vu0Cur_StoreMtx(RefMtx44 *m);
void Ref_Vu0Cur_GetTrans(RefVec4 *v);
void Ref_Vu0Cur_AxisZToEuler(RefVec4 *angles);
void Ref_Vu0Cur_SetTrans(RefVec4 *v);
void Ref_Vu0Cur_Translate(RefVec4 *v);
void Ref_Vu0Cur_TranslateLocal(RefVec4 *v);
void Ref_Vu0Cur_MulMtx(RefMtx44 *m);
void Ref_Vu0Cur_MulMtxRev(RefMtx44 *m);
void Ref_Vu0Cur_Transpose(void);
void Ref_Vu0Cur_InverseRT(void);
void Ref_Vu0Cur_RotateZ(float angle);
void Ref_Vu0Cur_RotateX(float angle);
void Ref_Vu0Cur_RotateY(float angle);
void Ref_Vu0Cur_RotateZXY(RefVec4 *angles);
void Ref_Vu0Cur_RotateXYZ(RefVec4 *angles);
void Ref_Vu0Cur_ScaleDiag(RefVec4 *v);
void Ref_Vu0Cur_ScaleDiagUniform(float s);
void Ref_Vu0Cur_RotateZXYTranslate(RefVec4 *angles, RefVec4 *pos);
void Ref_Vu0Cur_MulVec4(RefVec4 *out, RefVec4 *v);
void Ref_Vu0Cur_MulVec3(RefVec4 *out, RefVec4 *v);

#endif
