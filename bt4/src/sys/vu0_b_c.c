#include "common.h"
#include "sys/math3d.h"
#include "sys/mathf.h"
#include "sys/randf.h"

/*
 * The whole second half of the vector / matrix library, 0x121008..0x122940, in address order with no gap.
 *
 * 25 functions are compiled C. The other 67 are hand-written VU0 assembly in the original and are kept as top-level
 * assembly blocks (same bytes, checked by fdiff), as in src/sys/vu0_a_c*.c; three of them use encodings the
 * disassembler does not know and are written as words. src/port/vu0_b.c has the exact portable equivalent of every
 * routine.
 */

/* The polygon clipper's vertex: position, texture coordinate, colour. */
typedef struct ClipVtx {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 uv;
    /* 0x20 */ Vec4 col;
} ClipVtx; /* size 0x30 */

extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern s32 memcmp(const void *a, const void *b, u32 n);
extern void Rand_Init(void);

/* First half of the library (config/symbols/vu0_a.txt). */
extern void Vu0_InitAxisRegs(void);                /* vf1-3 = unit vectors */
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_RotateZXY(Mtx44 *out, const Mtx44 *m, Vec4 *angles); /* out = m rotated by Euler angles */
extern void Vu0Cur_Init(void);                /* current matrix = identity, stack pointer = 0 */
extern void Vu0Cur_LoadIdentity(void);                /* current matrix = identity */
extern void Vu0Cur_Push(void);                /* push the current matrix */
extern void Vu0Cur_Pop(void);                /* pop the current matrix */
extern void Vu0Cur_ResetStack(void);                /* stack pointer = 0 */
extern s32 Vu0Cur_IsStackUsed(void);                 /* stack depth != 0 */
extern void Vu0Cur_LoadMtx(Mtx44 *m);            /* current matrix = m */
extern void Vu0Cur_RotateZXY(Vec4 *angles);        /* rotates the current matrix by Euler angles */
extern void Vu0Cur_MulVec4(void *out, void *v);  /* out = current matrix * v, four components */
extern void Vu0Cur_MulVec3(void *out, void *v);  /* the same, x, y, z only */

/* Assembly routines of this half. */
extern void Vu0View_LoadMtx(Mtx44 *m);
extern void Vu0View_StoreMtx(Mtx44 *m);
extern void ClipVtx_Copy(ClipVtx *dst, ClipVtx *src);
extern void ClipVtx_CopyArray(ClipVtx *dst, ClipVtx *src, s32 n);
extern void ClipVtx_Lerp(ClipVtx *out, ClipVtx *a, ClipVtx *b, f32 t);
extern f32 ClipPlane_EdgeParam(Vec4 *plane, ClipVtx *a, ClipVtx *b, f32 dist);
extern void ClipPlane_DistArray(Vec4 *dist, ClipVtx *poly, Vec4 *plane, s32 n);
extern void Mtx_MulVec3(void *out, Mtx44 *m, Vec4 *v);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *m, Vec4 *v);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *v, f32 s);

extern const Mtx44 D_002EC260; /* the identity matrix */
extern f32 D_002FC32C;         /* 0.1234141f, the boot seed */

void Vec3_Set(Vec4 *v, f32 x, f32 y, f32 z);
void Vec3_DirToEuler(Vec4 *out, Vec4 *dir);

/* 0x121008: out = (identity rotated by the Euler angles) * v, through the current matrix, which is not restored. */
void Vu0Cur_RotEulerMulVec4(Vec4 *out, Vec4 *angles, Vec4 *v) {
    Vu0Cur_LoadIdentity();
    Vu0Cur_RotateZXY(angles);
    Vu0Cur_MulVec4(out, v);
}

/* 0x121058: the same, writing x, y, z only. */
void Vu0Cur_RotEulerMulVec3(Vec4 *out, Vec4 *angles, Vec4 *v) {
    Vu0Cur_LoadIdentity();
    Vu0Cur_RotateZXY(angles);
    Vu0Cur_MulVec3(out, v);
}

/* 0x1210A8..0x1214A0: Vu0Cur_Project*, Vu0Clip_*, Vu0Screen_*, Vu0View_*Mtx. */

/* 0x1210A8: Vu0Cur_ProjectInt, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Cur_ProjectInt in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ProjectInt\n"
    ".type Vu0Cur_ProjectInt, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Cur_ProjectInt:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyzw $vf5, $vf19, $vf4w\n"
    "    vdiv $Q, $vf0w, $vf5w\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf5, $vf5, $Q\n"
    "    vftoi0.xyzw $vf6, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Cur_ProjectInt, . - Vu0Cur_ProjectInt\n"
);

/* 0x1210D8: Vu0Cur_ProjectPoint, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Cur_ProjectPoint in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ProjectPoint\n"
    ".type Vu0Cur_ProjectPoint, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Cur_ProjectPoint:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyzw $vf5, $vf19, $vf4w\n"
    "    vdiv $Q, $vf0w, $vf5w\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf5, $vf5, $Q\n"
    "    vnop\n"
    "    vnop\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf5, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf5\n"
    "    vftoi4.xy $vf6, $vf5\n"
    "    vftoi0.zw $vf6, $vf5\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Vu0Cur_ProjectPoint, . - Vu0Cur_ProjectPoint\n"
);

/* 0x121140: Vu0Cur_ProjectPoints, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Cur_ProjectPoints in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ProjectPoints\n"
    ".type Vu0Cur_ProjectPoints, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Cur_ProjectPoints:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    ".L00121144:\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyzw $vf5, $vf19, $vf4w\n"
    "    vdiv $Q, $vf0w, $vf5w\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    addi $5, $5, 0x10 /* handwritten instruction */\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf5, $vf5, $Q\n"
    "    vnop\n"
    "    vnop\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf5, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf5\n"
    "    vftoi4.xy $vf6, $vf5\n"
    "    vftoi0.zw $vf6, $vf5\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    bne $0, $2, .L001211BC\n"
    "    nop\n"
    "    addi $6, $6, -0x1 /* handwritten instruction */\n"
    "    bne $0, $6, .L00121144\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    ".L001211BC:\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Cur_ProjectPoints, . - Vu0Cur_ProjectPoints\n"
);

/* 0x1211C8: Vu0Cur_ProjectPointStq, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Cur_ProjectPointStq in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ProjectPointStq\n"
    ".type Vu0Cur_ProjectPointStq, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Cur_ProjectPointStq:\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyzw $vf5, $vf19, $vf4w\n"
    "    vdiv $Q, $vf0w, $vf5w\n"
    "    vmove.z $vf8, $vf1\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf5, $vf5, $Q\n"
    "    vmulq.xyz $vf8, $vf8, $Q\n"
    "    vnop\n"
    "    vnop\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf5, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf5\n"
    "    vftoi4.xy $vf6, $vf5\n"
    "    vftoi0.zw $vf6, $vf5\n"
    "    sqc2 $vf8, 0x0($5)\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Vu0Cur_ProjectPointStq, . - Vu0Cur_ProjectPointStq\n"
);

/* 0x121240: Vu0Cur_ProjectPointsStq, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Cur_ProjectPointsStq in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ProjectPointsStq\n"
    ".type Vu0Cur_ProjectPointsStq, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Cur_ProjectPointsStq:\n"
    "    daddu $3, $8, $0\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    lqc2 $vf8, 0x0($7)\n"
    ".L0012124C:\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyzw $vf5, $vf19, $vf4w\n"
    "    vdiv $Q, $vf0w, $vf5w\n"
    "    vmove.z $vf8, $vf1\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    addi $6, $6, 0x10 /* handwritten instruction */\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf5, $vf5, $Q\n"
    "    vmulq.xyz $vf7, $vf8, $Q\n"
    "    addi $7, $7, 0x10 /* handwritten instruction */\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf5, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf5\n"
    "    vftoi4.xy $vf6, $vf5\n"
    "    vftoi0.zw $vf6, $vf5\n"
    "    sqc2 $vf7, 0x0($5)\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    bne $0, $2, .L001212D0\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    "    addi $3, $3, -0x1 /* handwritten instruction */\n"
    "    bne $0, $3, .L0012124C\n"
    "    addi $5, $5, 0x10 /* handwritten instruction */\n"
    ".L001212D0:\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Vu0Cur_ProjectPointsStq, . - Vu0Cur_ProjectPointsStq\n"
);

/* 0x1212D8: Vu0Clip_LoadMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Clip_LoadMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Clip_LoadMtx\n"
    ".type Vu0Clip_LoadMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Clip_LoadMtx:\n"
    "    lqc2 $vf20, 0x0($4)\n"
    "    lqc2 $vf21, 0x10($4)\n"
    "    lqc2 $vf22, 0x20($4)\n"
    "    jr $31\n"
    "    lqc2 $vf23, 0x30($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Clip_LoadMtx, . - Vu0Clip_LoadMtx\n"
);

/* 0x1212F0: Vu0Clip_StoreMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Clip_StoreMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Clip_StoreMtx\n"
    ".type Vu0Clip_StoreMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Clip_StoreMtx:\n"
    "    sqc2 $vf20, 0x0($4)\n"
    "    sqc2 $vf21, 0x10($4)\n"
    "    sqc2 $vf22, 0x20($4)\n"
    "    jr $31\n"
    "    sqc2 $vf23, 0x30($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Clip_StoreMtx, . - Vu0Clip_StoreMtx\n"
);

/* 0x121308: Vu0Clip_SetMulMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Clip_SetMulMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Clip_SetMulMtx\n"
    ".type Vu0Clip_SetMulMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Clip_SetMulMtx:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x10($4)\n"
    "    lqc2 $vf6, 0x20($4)\n"
    "    lqc2 $vf7, 0x30($4)\n"
    "    lqc2 $vf8, 0x0($5)\n"
    "    lqc2 $vf9, 0x10($5)\n"
    "    lqc2 $vf10, 0x20($5)\n"
    "    lqc2 $vf11, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf20, $vf7, $vf8w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf9x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf9y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf9z\n"
    "    vmaddw.xyzw $vf21, $vf7, $vf9w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf10x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf10y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf10z\n"
    "    vmaddw.xyzw $vf22, $vf7, $vf10w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf11x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf11y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf11z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf23, $vf7, $vf11w\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Clip_SetMulMtx, . - Vu0Clip_SetMulMtx\n"
);

/* 0x121370: Vu0Screen_LoadMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Screen_LoadMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Screen_LoadMtx\n"
    ".type Vu0Screen_LoadMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Screen_LoadMtx:\n"
    "    lqc2 $vf24, 0x0($4)\n"
    "    lqc2 $vf25, 0x10($4)\n"
    "    lqc2 $vf26, 0x20($4)\n"
    "    jr $31\n"
    "    lqc2 $vf27, 0x30($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Screen_LoadMtx, . - Vu0Screen_LoadMtx\n"
);

/* 0x121388: Vu0Screen_StoreMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Screen_StoreMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Screen_StoreMtx\n"
    ".type Vu0Screen_StoreMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Screen_StoreMtx:\n"
    "    sqc2 $vf24, 0x0($4)\n"
    "    sqc2 $vf25, 0x10($4)\n"
    "    sqc2 $vf26, 0x20($4)\n"
    "    jr $31\n"
    "    sqc2 $vf27, 0x30($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Screen_StoreMtx, . - Vu0Screen_StoreMtx\n"
);

/* 0x1213A0: Vu0Screen_SetMulMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0Screen_SetMulMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Screen_SetMulMtx\n"
    ".type Vu0Screen_SetMulMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0Screen_SetMulMtx:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x10($4)\n"
    "    lqc2 $vf6, 0x20($4)\n"
    "    lqc2 $vf7, 0x30($4)\n"
    "    lqc2 $vf8, 0x0($5)\n"
    "    lqc2 $vf9, 0x10($5)\n"
    "    lqc2 $vf10, 0x20($5)\n"
    "    lqc2 $vf11, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf24, $vf7, $vf8w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf9x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf9y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf9z\n"
    "    vmaddw.xyzw $vf25, $vf7, $vf9w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf10x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf10y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf10z\n"
    "    vmaddw.xyzw $vf26, $vf7, $vf10w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf11x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf11y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf11z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf27, $vf7, $vf11w\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0Screen_SetMulMtx, . - Vu0Screen_SetMulMtx\n"
);

/* 0x121408: Vu0View_LoadMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0View_LoadMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0View_LoadMtx\n"
    ".type Vu0View_LoadMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0View_LoadMtx:\n"
    "    lqc2 $vf28, 0x0($4)\n"
    "    lqc2 $vf29, 0x10($4)\n"
    "    lqc2 $vf30, 0x20($4)\n"
    "    jr $31\n"
    "    lqc2 $vf31, 0x30($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0View_LoadMtx, . - Vu0View_LoadMtx\n"
);

/* 0x121420: Vu0View_StoreMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0View_StoreMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0View_StoreMtx\n"
    ".type Vu0View_StoreMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0View_StoreMtx:\n"
    "    sqc2 $vf28, 0x0($4)\n"
    "    sqc2 $vf29, 0x10($4)\n"
    "    sqc2 $vf30, 0x20($4)\n"
    "    jr $31\n"
    "    sqc2 $vf31, 0x30($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0View_StoreMtx, . - Vu0View_StoreMtx\n"
);

/* 0x121438: Vu0View_SetMulMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vu0View_SetMulMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0View_SetMulMtx\n"
    ".type Vu0View_SetMulMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vu0View_SetMulMtx:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x10($4)\n"
    "    lqc2 $vf6, 0x20($4)\n"
    "    lqc2 $vf7, 0x30($4)\n"
    "    lqc2 $vf8, 0x0($5)\n"
    "    lqc2 $vf9, 0x10($5)\n"
    "    lqc2 $vf10, 0x20($5)\n"
    "    lqc2 $vf11, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf28, $vf7, $vf8w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf9x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf9y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf9z\n"
    "    vmaddw.xyzw $vf29, $vf7, $vf9w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf10x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf10y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf10z\n"
    "    vmaddw.xyzw $vf30, $vf7, $vf10w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf11x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf11y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf11z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf31, $vf7, $vf11w\n"
    "    nop\n"
    ".set pop\n"
    ".size Vu0View_SetMulMtx, . - Vu0View_SetMulMtx\n"
);


/* 0x1214A0: vf28-31 = rotation X, Z, Y (libm sinf / cosf) with row 3 = rotation * trans. */
void Vu0View_SetRotTrans(Vec4 *rot, Vec4 *trans) {
    Mtx44 m;
    Mtx44 r;
    f32 s;
    f32 c;

    Vu0Cur_Push();
    Vu0View_StoreMtx(&m);
    Mtx_StoreIdentity(&m);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->x);
    c = cosf(rot->x);
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->z);
    c = cosf(rot->z);
    r.m[0][0] = c;
    r.m[0][1] = -s;
    r.m[1][0] = s;
    r.m[1][1] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->y);
    c = cosf(rot->y);
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Mtx_MulVec3(m.m[3], &m, trans);
    Vu0View_LoadMtx(&m);
    Vu0Cur_Pop();
}

/* 0x121640: vf28-31: row 3 = M * trans, then rotations Y, Z, X applied. No callers. */
void Vu0View_ApplyTransRot(Vec4 *rot, Vec4 *trans) {
    Mtx44 m;
    Mtx44 r;
    f32 s;
    f32 c;

    Vu0Cur_Push();
    Vu0View_StoreMtx(&m);
    Mtx_MulVec3(m.m[3], &m, trans);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->y);
    c = cosf(rot->y);
    r.m[0][0] = c;
    r.m[0][2] = s;
    r.m[2][0] = -s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->z);
    c = cosf(rot->z);
    r.m[0][0] = c;
    r.m[0][1] = -s;
    r.m[1][0] = s;
    r.m[1][1] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[0], r.m[0]);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Mtx_StoreIdentity(&r);
    s = sinf(rot->x);
    c = cosf(rot->x);
    r.m[1][1] = c;
    r.m[1][2] = -s;
    r.m[2][1] = s;
    r.m[2][2] = c;
    Vu0Cur_LoadMtx(&m);
    Vu0Cur_MulVec4(m.m[1], r.m[1]);
    Vu0Cur_MulVec4(m.m[2], r.m[2]);
    Vu0View_LoadMtx(&m);
    Vu0Cur_Pop();
}

/*
 * Two rows of a rotation matrix through the current matrix (vf16-19): ra = cur * a, rb = cur * b, then the two results
 * replace two rows of the current matrix.
 */
#define VU0_CUR_MUL2(a, b, ra, rb, da, db) \
    __asm__ volatile( \
        "lqc2 $vf8, %0\n" \
        "lqc2 $vf9, %1\n" \
        "vmulax.xyzw $ACC, $vf16, $vf8x\n" \
        "vmadday.xyzw $ACC, $vf17, $vf8y\n" \
        "vmaddaz.xyzw $ACC, $vf18, $vf8z\n" \
        "vmaddw.xyzw " ra ", $vf19, $vf8w\n" \
        "vmulax.xyzw $ACC, $vf16, $vf9x\n" \
        "vmadday.xyzw $ACC, $vf17, $vf9y\n" \
        "vmaddaz.xyzw $ACC, $vf18, $vf9z\n" \
        "vmaddw.xyzw " rb ", $vf19, $vf9w\n" \
        "vmove.xyzw " da ", $vf7\n" \
        "vmove.xyzw " db ", $vf4\n" \
        : : "m"(a), "m"(b))

/*
 * 0x1217D0: current matrix = current * Ry(rot.y) * Rx(rot.x) * Rz(rot.z), each rotation applied to the two rows it
 * changes; libm cosf / sinf. Compiled C with inline VU0 assembly.
 */
void Vu0Cur_RotateYXZ(Vec4 *rot) {
    Mtx44 m;

    Mtx_StoreIdentity(&m);
    m.m[0][0] = cosf(rot->y);
    m.m[2][0] = sinf(rot->y);
    m.m[2][2] = m.m[0][0];
    m.m[0][2] = -m.m[2][0];
    VU0_CUR_MUL2(m.m[0][0], m.m[2][0], "$vf7", "$vf4", "$vf16", "$vf18");
    Mtx_StoreIdentity(&m);
    m.m[1][1] = cosf(rot->x);
    m.m[2][1] = sinf(rot->x);
    m.m[2][2] = m.m[1][1];
    m.m[1][2] = -m.m[2][1];
    VU0_CUR_MUL2(m.m[1][0], m.m[2][0], "$vf7", "$vf4", "$vf17", "$vf18");
    Mtx_StoreIdentity(&m);
    m.m[0][0] = cosf(rot->z);
    m.m[1][0] = sinf(rot->z);
    m.m[1][1] = m.m[0][0];
    m.m[0][1] = -m.m[1][0];
    __asm__ volatile(
        "lqc2 $vf8, %0\n"
        "lqc2 $vf9, %1\n"
        "vmulax.xyzw $ACC, $vf16, $vf8x\n"
        "vmadday.xyzw $ACC, $vf17, $vf8y\n"
        "vmaddaz.xyzw $ACC, $vf18, $vf8z\n"
        "vmaddw.xyzw $vf4, $vf19, $vf8w\n"
        "vmulax.xyzw $ACC, $vf16, $vf9x\n"
        "vmadday.xyzw $ACC, $vf17, $vf9y\n"
        "vmaddaz.xyzw $ACC, $vf18, $vf9z\n"
        "vmaddw.xyzw $vf7, $vf19, $vf9w\n"
        "vmove.xyzw $vf16, $vf4\n"
        "vmove.xyzw $vf17, $vf7\n"
        : : "m"(m.m[0][0]), "m"(m.m[1][0]));
}

/* 0x121910..0x121950: eight empty functions, no callers. */
void Vu0_Stub0(void) {
}

void Vu0_Stub1(void) {
}

void Vu0_Stub2(void) {
}

void Vu0_Stub3(void) {
}

void Vu0_Stub4(void) {
}

void Vu0_Stub5(void) {
}

void Vu0_Stub6(void) {
}

void Vu0_Stub7(void) {
}

/* 0x121950..0x121A10: ClipVtx_Set, ClipVtx_Copy, ClipVtx_SetArray, ClipVtx_CopyArray. */

/* 0x121950: ClipVtx_Set, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipVtx_Set in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipVtx_Set\n"
    ".type ClipVtx_Set, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipVtx_Set:\n"
    "    lq $8, 0x0($5)\n"
    "    lq $9, 0x0($6)\n"
    "    lq $10, 0x0($7)\n"
    "    sq $8, 0x0($4)\n"
    "    sq $9, 0x10($4)\n"
    "    jr $31\n"
    "    sq $10, 0x20($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipVtx_Set, . - ClipVtx_Set\n"
);

/* 0x121970: ClipVtx_Copy, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipVtx_Copy in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipVtx_Copy\n"
    ".type ClipVtx_Copy, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipVtx_Copy:\n"
    "    lq $8, 0x0($5)\n"
    "    lq $9, 0x10($5)\n"
    "    lq $10, 0x20($5)\n"
    "    sq $8, 0x0($4)\n"
    "    sq $9, 0x10($4)\n"
    "    jr $31\n"
    "    sq $10, 0x20($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipVtx_Copy, . - ClipVtx_Copy\n"
);

/* 0x121990: ClipVtx_SetArray, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipVtx_SetArray in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipVtx_SetArray\n"
    ".type ClipVtx_SetArray, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipVtx_SetArray:\n"
    "    daddu $2, $8, $0\n"
    "    beq $0, $2, .L001219CC\n"
    "    nop\n"
    ".L0012199C:\n"
    "    lq $8, 0x0($5)\n"
    "    lq $9, 0x0($6)\n"
    "    lq $10, 0x0($7)\n"
    "    addi $5, $5, 0x10 /* handwritten instruction */\n"
    "    addi $6, $6, 0x10 /* handwritten instruction */\n"
    "    addi $7, $7, 0x10 /* handwritten instruction */\n"
    "    sq $8, 0x0($4)\n"
    "    sq $9, 0x10($4)\n"
    "    sq $10, 0x20($4)\n"
    "    addi $2, $2, -0x1 /* handwritten instruction */\n"
    "    bne $0, $2, .L0012199C\n"
    "    addi $4, $4, 0x30 /* handwritten instruction */\n"
    ".L001219CC:\n"
    "    jr $31\n"
    "    nop\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipVtx_SetArray, . - ClipVtx_SetArray\n"
);

/* 0x1219D8: ClipVtx_CopyArray, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipVtx_CopyArray in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipVtx_CopyArray\n"
    ".type ClipVtx_CopyArray, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipVtx_CopyArray:\n"
    "    beq $0, $6, .L00121A08\n"
    "    nop\n"
    ".L001219E0:\n"
    "    lq $8, 0x0($5)\n"
    "    lq $9, 0x10($5)\n"
    "    lq $10, 0x20($5)\n"
    "    addi $5, $5, 0x30 /* handwritten instruction */\n"
    "    sq $8, 0x0($4)\n"
    "    sq $9, 0x10($4)\n"
    "    sq $10, 0x20($4)\n"
    "    addi $6, $6, -0x1 /* handwritten instruction */\n"
    "    bne $0, $6, .L001219E0\n"
    "    addi $4, $4, 0x30 /* handwritten instruction */\n"
    ".L00121A08:\n"
    "    jr $31\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipVtx_CopyArray, . - ClipVtx_CopyArray\n"
);


/*
 * 0x121A10: clips a convex polygon of n vertices (at most 9 in, 9 out) against one plane, in place, and returns the
 * new vertex count. A vertex is outside when dot(plane.xyz, pos) + plane.w < 0. Each edge that crosses the plane gets
 * a vertex at t = dist(cur) / dot(plane, cur - next), interpolated as next * t + cur * (1 - t) on all three quadwords.
 */
s32 ClipPoly_ClipPlane(ClipVtx *poly, Vec4 *plane, s32 n) {
    ClipVtx buf[9];
    Vec4 dist[9];
    s32 out[12];
    ClipVtx *dst;
    ClipVtx *cur;
    ClipVtx *next;
    s32 i;
    s32 j;
    s32 nOut;
    u32 cnt;
    f32 t;

    if (n == 0) {
        return 0;
    }
    ClipPlane_DistArray(dist, poly, plane, n);
    nOut = 0;
    for (i = 0; i < n; i++) {
        out[i] = dist[i].x < 0.0f;
        nOut += out[i];
    }
    if (nOut == 0) {
        return n;
    }
    if (n == nOut) {
        return 0;
    }
    dst = buf;
    j = 1;
    for (i = 0; i < n; i++, j++) {
        cur = &poly[i];
        if (j >= n) {
            j = 0;
        }
        next = &poly[j];
        if (out[i] == 0) {
            ClipVtx_Copy(dst, cur);
            dst++;
            if (out[j] != 0) {
                t = ClipPlane_EdgeParam(plane, cur, next, dist[i].x);
                ClipVtx_Lerp(dst, next, cur, t);
                dst++;
            }
        } else if (out[j] == 0) {
            t = ClipPlane_EdgeParam(plane, cur, next, dist[i].x);
            ClipVtx_Lerp(dst, next, cur, t);
            dst++;
        }
    }
    cnt = (u32)((u8 *)dst - (u8 *)buf) / sizeof(ClipVtx);
    ClipVtx_CopyArray(poly, buf, cnt);
    return cnt;
}

/* 0x121C18..0x121DA8: ClipVtx_Lerp, ClipPlane_EdgeParam, ClipPlane_DistArray, ClipPoly_ProjectMtx / Cur. */

/* 0x121C18: ClipVtx_Lerp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipVtx_Lerp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipVtx_Lerp\n"
    ".type ClipVtx_Lerp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipVtx_Lerp:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf13\n"
    "    vsubx.w $vf14, $vf0, $vf13x\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    lqc2 $vf6, 0x10($5)\n"
    "    lqc2 $vf7, 0x10($6)\n"
    "    lqc2 $vf8, 0x20($5)\n"
    "    lqc2 $vf9, 0x20($6)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf13x\n"
    "    vmaddw.xyzw $vf10, $vf5, $vf14w\n"
    "    vmulax.xyzw $ACC, $vf6, $vf13x\n"
    "    vmaddw.xyzw $vf11, $vf7, $vf14w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf13x\n"
    "    vmaddw.xyzw $vf12, $vf9, $vf14w\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    sqc2 $vf11, 0x10($4)\n"
    "    jr $31\n"
    "    sqc2 $vf12, 0x20($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipVtx_Lerp, . - ClipVtx_Lerp\n"
);

/* 0x121C68: ClipPlane_EdgeParam, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipPlane_EdgeParam in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipPlane_EdgeParam\n"
    ".type ClipPlane_EdgeParam, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipPlane_EdgeParam:\n"
    "    lqc2 $vf6, 0x0($6)\n"
    "    lqc2 $vf5, 0x0($5)\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    vsub.xyz $vf5, $vf5, $vf6\n"
    "    vmul.xyz $vf5, $vf4, $vf5\n"
    "    vadday.x $ACC, $vf5, $vf5y\n"
    "    vmaddz.x $vf5, $vf3, $vf5z\n"
    "    qmfc2.ni $8, $vf5\n"
    "    mtc1 $8, $f0\n"
    "    jr $31\n"
    "    div.s $f0, $f12, $f0\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipPlane_EdgeParam, . - ClipPlane_EdgeParam\n"
);

/* 0x121C98: ClipPlane_DistArray, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipPlane_DistArray in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipPlane_DistArray\n"
    ".type ClipPlane_DistArray, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipPlane_DistArray:\n"
    "    beq $0, $7, .L00121CD0\n"
    "    nop\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    lqc2 $vf5, 0x0($5)\n"
    ".L00121CA8:\n"
    "    vmul.xyz $vf5, $vf4, $vf5\n"
    "    vadday.x $ACC, $vf5, $vf5y\n"
    "    vmaddaz.x $ACC, $vf3, $vf5z\n"
    "    vmaddw.x $vf6, $vf3, $vf4w\n"
    "    addi $5, $5, 0x30 /* handwritten instruction */\n"
    "    lqc2 $vf5, 0x0($5)\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    addi $7, $7, -0x1 /* handwritten instruction */\n"
    "    bne $0, $7, .L00121CA8\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    ".L00121CD0:\n"
    "    jr $31\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipPlane_DistArray, . - ClipPlane_DistArray\n"
);

/* 0x121CD8: ClipPoly_ProjectMtx, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipPoly_ProjectMtx in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipPoly_ProjectMtx\n"
    ".type ClipPoly_ProjectMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipPoly_ProjectMtx:\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    lqc2 $vf14, 0x10($7)\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    lqc2 $vf5, 0x10($6)\n"
    "    lqc2 $vf6, 0x20($6)\n"
    "    lqc2 $vf7, 0x30($6)\n"
    ".L00121CF0:\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vmove.z $vf14, $vf1\n"
    "    addi $7, $7, 0x30 /* handwritten instruction */\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vmulq.xyz $vf13, $vf14, $Q\n"
    "    lqc2 $vf14, 0x10($7)\n"
    "    vftoi4.xy $vf10, $vf9\n"
    "    vftoi0.zw $vf10, $vf9\n"
    "    sqc2 $vf13, 0x0($5)\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    addi $8, $8, -0x1 /* handwritten instruction */\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    "    bne $0, $8, .L00121CF0\n"
    "    addi $5, $5, 0x10 /* handwritten instruction */\n"
    "    jr $31\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipPoly_ProjectMtx, . - ClipPoly_ProjectMtx\n"
);

/* 0x121D48: ClipPoly_ProjectCur, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_ClipPoly_ProjectCur in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl ClipPoly_ProjectCur\n"
    ".type ClipPoly_ProjectCur, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "ClipPoly_ProjectCur:\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    lqc2 $vf14, 0x10($6)\n"
    ".L00121D50:\n"
    "    vmulax.xyzw $ACC, $vf16, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf19, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vmove.z $vf14, $vf1\n"
    "    addi $6, $6, 0x30 /* handwritten instruction */\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vmulq.xyz $vf13, $vf14, $Q\n"
    "    lqc2 $vf14, 0x10($6)\n"
    "    vftoi4.xy $vf10, $vf9\n"
    "    vftoi0.zw $vf10, $vf9\n"
    "    sqc2 $vf13, 0x0($5)\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    addi $7, $7, -0x1 /* handwritten instruction */\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    "    bne $0, $7, .L00121D50\n"
    "    addi $5, $5, 0x10 /* handwritten instruction */\n"
    "    jr $31\n"
    "    nop\n"
    ".set pop\n"
    ".size ClipPoly_ProjectCur, . - ClipPoly_ProjectCur\n"
);


/* 0x121DA8: boot-time set-up of the VU0 state and of both random generators. */
void Vu0_Init(void) {
    Rand_SeedFloat(D_002FC32C);
    Vu0_InitAxisRegs();
    Vu0Cur_Init();
    Vu0Cur_ResetStack();
    Rand_Init();
}

/* 0x121DE0: the remains of an assert: compares a stored identity with the constant one and reads the stack depth. */
void Vu0_CheckState(void) {
    Mtx44 m;

    Mtx_StoreIdentity(&m);
    memcmp(&m, &D_002EC260, sizeof(Mtx44));
    Vu0Cur_IsStackUsed();
}

/* 0x121E18..0x121E28: Vec4_SetZeroW1, Vec4_SetZero. */

/* 0x121E18: Vec4_SetZeroW1, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_SetZeroW1 in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_SetZeroW1\n"
    ".type Vec4_SetZeroW1, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_SetZeroW1:\n"
    "    jr $31\n"
    "    sqc2 $vf0, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_SetZeroW1, . - Vec4_SetZeroW1\n"
);

/* 0x121E20: Vec4_SetZero, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_SetZero in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_SetZero\n"
    ".type Vec4_SetZero, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_SetZero:\n"
    "    jr $31\n"
    "    sq $0, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_SetZero, . - Vec4_SetZero\n"
);


/* 0x121E28: v = (x, y, z, w). */
void Vec4_Set(Vec4 *v, f32 x, f32 y, f32 z, f32 w) {
    v->x = x;
    v->y = y;
    v->z = z;
    v->w = w;
}

/* 0x121E40: sets x, y, z and leaves w. */
void Vec3_Set(Vec4 *v, f32 x, f32 y, f32 z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

/* 0x121E50..0x122030: Vec3_Normalize .. Mtx_MulVec3. */

/* 0x121E50: Vec3_Normalize, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Normalize in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Normalize\n"
    ".type Vec3_Normalize, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Normalize:\n"
    "    .word 0xD8A40000 /* lqc2 $vf4,0(a1) */\n"
    "    .word 0x4BC4216A /* vmul.xyz $vf5xyz,$vf4xyz,$vf4xyz */\n"
    "    .word 0x4B05283D /* vadday.x $ACCx,$vf5x,$vf5y */\n"
    "    .word 0x4B05194A /* vmaddz.x $vf5x,$vf3x,$vf5z */\n"
    "    .word 0x4A2503BD /* vsqrt $Q,$vf5x */\n"
    "    .word 0x4A0003BF /* vwaitq */\n"
    "    .word 0x4B000160 /* vaddq.x $vf5x,$vf0x,$Q */\n"
    "    .word 0x4A0002FF /* vnop */\n"
    "    .word 0x4A0002FF /* vnop */\n"
    "    .word 0x4A6503BC /* vdiv $Q,$vf0w,$vf5x */\n"
    "    .word 0x4BE001AC /* vsub.xyzw $vf6xyzw,$vf0xyzw,$vf0xyzw */\n"
    "    .word 0x4A0003BF /* vwaitq */\n"
    "    .word 0x4BC0219C /* vmulq.xyz $vf6xyz,$vf4xyz,$Q */\n"
    "    .word 0x03E00008 /* jr ra */\n"
    "    .word 0xF8860000 /* sqc2 $vf6,0(a0) */\n"
    "    .word 0x00000000 /* nop */\n"
    ".set pop\n"
    ".size Vec3_Normalize, . - Vec3_Normalize\n"
);

/* 0x121E90: Vec4_Swap, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Swap in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Swap\n"
    ".type Vec4_Swap, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Swap:\n"
    "    lq $8, 0x0($4)\n"
    "    lq $9, 0x0($5)\n"
    "    sq $8, 0x0($5)\n"
    "    jr $31\n"
    "    sq $9, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Swap, . - Vec4_Swap\n"
);

/* 0x121EA8: Vec4_Add, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Add in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Add\n"
    ".type Vec4_Add, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Add:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vadd.xyzw $vf4, $vf4, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Add, . - Vec4_Add\n"
);

/* 0x121EC0: Vec3_Add, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Add in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Add\n"
    ".type Vec3_Add, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Add:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vadd.xyz $vf4, $vf4, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Add, . - Vec3_Add\n"
);

/* 0x121ED8: Vec4_Sub, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Sub in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Sub\n"
    ".type Vec4_Sub, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Sub:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vsub.xyzw $vf4, $vf4, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Sub, . - Vec4_Sub\n"
);

/* 0x121EF0: Vec3_Sub, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Sub in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Sub\n"
    ".type Vec3_Sub, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Sub:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vsub.xyz $vf4, $vf4, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Sub, . - Vec3_Sub\n"
);

/* 0x121F08: Vec4_Mul, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Mul in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Mul\n"
    ".type Vec4_Mul, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Mul:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vmul.xyzw $vf4, $vf4, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Mul, . - Vec4_Mul\n"
);

/* 0x121F20: Vec3_Mul, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Mul in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Mul\n"
    ".type Vec3_Mul, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Mul:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vmul.xyz $vf4, $vf4, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Mul, . - Vec3_Mul\n"
);

/* 0x121F38: Vec4_Scale, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Scale in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Scale\n"
    ".type Vec4_Scale, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Scale:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf5\n"
    "    vmulx.xyzw $vf4, $vf4, $vf5x\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_Scale, . - Vec4_Scale\n"
);

/* 0x121F50: Vec3_Scale, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Scale in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Scale\n"
    ".type Vec3_Scale, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Scale:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf5\n"
    "    vmulx.xyz $vf4, $vf4, $vf5x\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    ".set pop\n"
    ".size Vec3_Scale, . - Vec3_Scale\n"
);

/* 0x121F68: Vec4_Div, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Div in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Div\n"
    ".type Vec4_Div, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Div:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf5\n"
    "    vdiv $Q, $vf0w, $vf5x\n"
    "    vwaitq\n"
    "    vmulq.xyzw $vf4, $vf4, $Q\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_Div, . - Vec4_Div\n"
);

/* 0x121F88: Vec3_Div, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Div in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Div\n"
    ".type Vec3_Div, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Div:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf5\n"
    "    vdiv $Q, $vf0w, $vf5x\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf4, $vf4, $Q\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    ".set pop\n"
    ".size Vec3_Div, . - Vec3_Div\n"
);

/* 0x121FA8: Vec4_Copy, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Copy in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Copy\n"
    ".type Vec4_Copy, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Copy:\n"
    "    lq $8, 0x0($5)\n"
    "    jr $31\n"
    "    sq $8, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Copy, . - Vec4_Copy\n"
);

/* 0x121FB8: Vec3_Copy, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Copy in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Copy\n"
    ".type Vec3_Copy, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Copy:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($4)\n"
    "    vmove.xyz $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Copy, . - Vec3_Copy\n"
);

/* 0x121FD0: Mtx_MulVec4, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_MulVec4 in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_MulVec4\n"
    ".type Mtx_MulVec4, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_MulVec4:\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf8, $vf7, $vf8w\n"
    "    jr $31\n"
    "    sqc2 $vf8, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Mtx_MulVec4, . - Mtx_MulVec4\n"
);

/* 0x122000: Mtx_MulVec3, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_MulVec3 in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_MulVec3\n"
    ".type Mtx_MulVec3, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_MulVec3:\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vmove.xyz $vf8, $vf9\n"
    "    jr $31\n"
    "    sqc2 $vf8, 0x0($4)\n"
    ".set pop\n"
    ".size Mtx_MulVec3, . - Mtx_MulVec3\n"
);


/* 0x122030: out = (identity rotated by the Euler angles) * v, all four components. */
void Vec4_RotateEuler(Vec4 *out, Vec4 *angles, Vec4 *v) {
    Mtx44 m;

    Mtx_RotateZXY(&m, &D_002EC260, angles);
    Mtx_MulVec4(out, &m, v);
}

/* 0x122088..0x122258: Vec3_Dot .. Vec3_DistSq. */

/* 0x122088: Vec3_Dot, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Dot in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Dot\n"
    ".type Vec3_Dot, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Dot:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x0($5)\n"
    "    vmul.xyz $vf5, $vf4, $vf5\n"
    "    vadday.x $ACC, $vf5, $vf5y\n"
    "    vmaddz.x $vf5, $vf3, $vf5z\n"
    "    qmfc2.ni $8, $vf5\n"
    "    mtc1 $8, $f0\n"
    "    jr $31\n"
    "    nop\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Dot, . - Vec3_Dot\n"
);

/* 0x1220B0: Vec3_Cross, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Cross in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Cross\n"
    ".type Vec3_Cross, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Cross:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vopmula.xyz $ACC, $vf4, $vf5\n"
    "    vopmsub.xyz $vf5, $vf5, $vf4\n"
    "    vsub.w $vf5, $vf5, $vf5\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Cross, . - Vec3_Cross\n"
);

/* 0x1220D0: Vec4_ToFixed12, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_ToFixed12 in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_ToFixed12\n"
    ".type Vec4_ToFixed12, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_ToFixed12:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vftoi12.xyzw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_ToFixed12, . - Vec4_ToFixed12\n"
);

/* 0x1220E0: Vec4_ToFixed4, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_ToFixed4 in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_ToFixed4\n"
    ".type Vec4_ToFixed4, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_ToFixed4:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vftoi4.xyzw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_ToFixed4, . - Vec4_ToFixed4\n"
);

/* 0x1220F0: Vec4_ToInt, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_ToInt in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_ToInt\n"
    ".type Vec4_ToInt, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_ToInt:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vftoi0.xyzw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    ".set pop\n"
    ".size Vec4_ToInt, . - Vec4_ToInt\n"
);

/* 0x122100: Vec4_ToFixed4XY, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_ToFixed4XY in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_ToFixed4XY\n"
    ".type Vec4_ToFixed4XY, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_ToFixed4XY:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vftoi4.xy $vf5, $vf4\n"
    "    vftoi0.zw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_ToFixed4XY, . - Vec4_ToFixed4XY\n"
);

/* 0x122118: Vec4_Clamp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Clamp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Clamp\n"
    ".type Vec4_Clamp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Clamp:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf5\n"
    "    mfc1 $8, $f13\n"
    "    qmtc2.ni $8, $vf6\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vmaxx.xyzw $vf4, $vf4, $vf5x\n"
    "    vminix.xyzw $vf4, $vf4, $vf6x\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Clamp, . - Vec4_Clamp\n"
);

/* 0x122140: Vec3_Clamp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Clamp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Clamp\n"
    ".type Vec3_Clamp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Clamp:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf5\n"
    "    mfc1 $8, $f13\n"
    "    qmtc2.ni $8, $vf6\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vmaxx.xyz $vf4, $vf4, $vf5x\n"
    "    vminix.xyz $vf4, $vf4, $vf6x\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Clamp, . - Vec3_Clamp\n"
);

/* 0x122168: Vec4_Lerp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_Lerp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_Lerp\n"
    ".type Vec4_Lerp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_Lerp:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf6\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vsubx.w $vf7, $vf0, $vf6x\n"
    "    vmulax.xyzw $ACC, $vf4, $vf6x\n"
    "    vmaddw.xyzw $vf6, $vf5, $vf7w\n"
    "    jr $31\n"
    "    sqc2 $vf6, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_Lerp, . - Vec4_Lerp\n"
);

/* 0x122190: Vec3_Lerp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Lerp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Lerp\n"
    ".type Vec3_Lerp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Lerp:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf6\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vsubx.w $vf7, $vf0, $vf6x\n"
    "    vmulax.xyz $ACC, $vf4, $vf6x\n"
    "    vmaddw.xyz $vf6, $vf5, $vf7w\n"
    "    vmove.w $vf6, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf6, 0x0($4)\n"
    ".set pop\n"
    ".size Vec3_Lerp, . - Vec3_Lerp\n"
);

/* 0x1221B8: Vec3_Length, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Length in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Length\n"
    ".type Vec3_Length, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Length:\n"
    "    .word 0xD8840000 /* lqc2 $vf4,0(a0) */\n"
    "    .word 0x4BC4212A /* vmul.xyz $vf4xyz,$vf4xyz,$vf4xyz */\n"
    "    .word 0x4B04203D /* vadday.x $ACCx,$vf4x,$vf4y */\n"
    "    .word 0x4B04190A /* vmaddz.x $vf4x,$vf3x,$vf4z */\n"
    "    .word 0x4A2403BD /* vsqrt $Q,$vf4x */\n"
    "    .word 0x4A0003BF /* vwaitq */\n"
    "    .word 0x4848B000 /* cfc2 t0,$vi22 */\n"
    "    .word 0x44880000 /* mtc1 t0,$f0 */\n"
    "    .word 0x03E00008 /* jr ra */\n"
    "    .word 0x00000000 /* nop */\n"
    ".set pop\n"
    ".size Vec3_Length, . - Vec3_Length\n"
);

/* 0x1221E0: Vec3_LengthSq, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_LengthSq in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_LengthSq\n"
    ".type Vec3_LengthSq, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_LengthSq:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    vmul.xyz $vf4, $vf4, $vf4\n"
    "    vadday.x $ACC, $vf4, $vf4y\n"
    "    vmaddz.x $vf4, $vf3, $vf4z\n"
    "    qmfc2.ni $8, $vf4\n"
    "    mtc1 $8, $f0\n"
    "    jr $31\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_LengthSq, . - Vec3_LengthSq\n"
);

/* 0x122200: Vec3_Dist, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Dist in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Dist\n"
    ".type Vec3_Dist, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Dist:\n"
    "    .word 0xD8840000 /* lqc2 $vf4,0(a0) */\n"
    "    .word 0xD8A50000 /* lqc2 $vf5,0(a1) */\n"
    "    .word 0x4BE5212C /* vsub.xyzw $vf4xyzw,$vf4xyzw,$vf5xyzw */\n"
    "    .word 0x4BC4212A /* vmul.xyz $vf4xyz,$vf4xyz,$vf4xyz */\n"
    "    .word 0x4B04203D /* vadday.x $ACCx,$vf4x,$vf4y */\n"
    "    .word 0x4B04190A /* vmaddz.x $vf4x,$vf3x,$vf4z */\n"
    "    .word 0x4A2403BD /* vsqrt $Q,$vf4x */\n"
    "    .word 0x4A0003BF /* vwaitq */\n"
    "    .word 0x4848B000 /* cfc2 t0,$vi22 */\n"
    "    .word 0x44880000 /* mtc1 t0,$f0 */\n"
    "    .word 0x03E00008 /* jr ra */\n"
    "    .word 0x00000000 /* nop */\n"
    ".set pop\n"
    ".size Vec3_Dist, . - Vec3_Dist\n"
);

/* 0x122230: Vec3_DistSq, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_DistSq in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_DistSq\n"
    ".type Vec3_DistSq, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_DistSq:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x0($5)\n"
    "    vsub.xyzw $vf4, $vf4, $vf5\n"
    "    vmul.xyz $vf4, $vf4, $vf4\n"
    "    vadday.x $ACC, $vf4, $vf4y\n"
    "    vmaddz.x $vf4, $vf3, $vf4z\n"
    "    qmfc2.ni $8, $vf4\n"
    "    mtc1 $8, $f0\n"
    "    jr $31\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_DistSq, . - Vec3_DistSq\n"
);


/* 0x122258: yaw and pitch of a direction: out = (-atan2f(y, length(x, 0, z)), atan2f(x, z), 0); out.w is left. */
void Vec3_DirToEuler(Vec4 *out, Vec4 *dir) {
    Vec4 flat;

    out->y = atan2f(dir->x, dir->z);
    Vec3_Set(&flat, dir->x, 0.0f, dir->z);
    out->x = -atan2f(dir->y, Vec3_Length(&flat));
    out->z = 0.0f;
}

/* 0x1222D8: Vec3_DirToEuler of a - b. No callers. */
void Vec3_DiffToEuler(Vec4 *out, Vec4 *a, Vec4 *b) {
    Vec4 d;

    Vec3_Sub(&d, a, b);
    Vec3_DirToEuler(out, &d);
}

/* 0x122310..0x122588: Mtx_Project*. */

/* 0x122310: Mtx_ProjectInt, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_ProjectInt in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ProjectInt\n"
    ".type Mtx_ProjectInt, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_ProjectInt:\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vftoi0.xyzw $vf10, $vf9\n"
    "    jr $31\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Mtx_ProjectInt, . - Mtx_ProjectInt\n"
);

/* 0x122350: Mtx_ProjectPoint, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_ProjectPoint in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ProjectPoint\n"
    ".type Mtx_ProjectPoint, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_ProjectPoint:\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vnop\n"
    "    vnop\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf9, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf9\n"
    "    vftoi4.xy $vf10, $vf9\n"
    "    vftoi0.zw $vf10, $vf9\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Mtx_ProjectPoint, . - Mtx_ProjectPoint\n"
);

/* 0x1223C8: Mtx_ProjectPoints, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_ProjectPoints in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ProjectPoints\n"
    ".type Mtx_ProjectPoints, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_ProjectPoints:\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    ".L001223DC:\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    addi $6, $6, 0x10 /* handwritten instruction */\n"
    "    lqc2 $vf8, 0x0($6)\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vnop\n"
    "    vnop\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf9, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf9\n"
    "    vftoi4.xy $vf10, $vf9\n"
    "    vftoi0.zw $vf10, $vf9\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    bne $0, $2, .L00122450\n"
    "    addi $7, $7, -0x1 /* handwritten instruction */\n"
    "    bne $0, $7, .L001223DC\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    ".L00122450:\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Mtx_ProjectPoints, . - Mtx_ProjectPoints\n"
);

/* 0x122458: Mtx_ProjectPointStq, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_ProjectPointStq in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ProjectPointStq\n"
    ".type Mtx_ProjectPointStq, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_ProjectPointStq:\n"
    "    daddu $3, $8, $0\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    lqc2 $vf13, 0x0($3)\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    lqc2 $vf5, 0x10($6)\n"
    "    lqc2 $vf6, 0x20($6)\n"
    "    lqc2 $vf7, 0x30($6)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vmove.z $vf13, $vf1\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vmulq.xyz $vf13, $vf13, $Q\n"
    "    vnop\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf9, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf9\n"
    "    vftoi4.xy $vf10, $vf9\n"
    "    vftoi0.zw $vf10, $vf9\n"
    "    sqc2 $vf13, 0x0($5)\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Mtx_ProjectPointStq, . - Mtx_ProjectPointStq\n"
);

/* 0x1224E0: Mtx_ProjectPointsStq, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Mtx_ProjectPointsStq in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ProjectPointsStq\n"
    ".type Mtx_ProjectPointsStq, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Mtx_ProjectPointsStq:\n"
    "    daddu $3, $8, $0\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    lqc2 $vf13, 0x0($3)\n"
    "    lqc2 $vf4, 0x0($6)\n"
    "    lqc2 $vf5, 0x10($6)\n"
    "    lqc2 $vf6, 0x20($6)\n"
    "    lqc2 $vf7, 0x30($6)\n"
    ".L001224FC:\n"
    "    vmulax.xyzw $ACC, $vf4, $vf8x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf8y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf8z\n"
    "    vmaddw.xyzw $vf9, $vf7, $vf8w\n"
    "    vdiv $Q, $vf0w, $vf9w\n"
    "    vmove.z $vf13, $vf1\n"
    "    vsub.xyzw $vf11, $vf0, $vf0\n"
    "    lui $8, (0x45800000 >> 16)\n"
    "    dsll $8, $8, 16\n"
    "    ori $8, $8, 0x4580\n"
    "    dsll $8, $8, 16\n"
    "    qmtc2.ni $8, $vf12\n"
    "    addi $7, $7, 0x10 /* handwritten instruction */\n"
    "    addi $3, $3, 0x10 /* handwritten instruction */\n"
    "    lqc2 $vf8, 0x0($7)\n"
    "    vwaitq\n"
    "    vmulq.xyz $vf9, $vf9, $Q\n"
    "    vmulq.xyz $vf14, $vf13, $Q\n"
    "    lqc2 $vf13, 0x0($3)\n"
    "    ctc2.ni $0, $vi16\n"
    "    vsub.xyw $vf0, $vf9, $vf11\n"
    "    vsub.xy $vf0, $vf12, $vf9\n"
    "    vftoi4.xy $vf10, $vf9\n"
    "    vftoi0.zw $vf10, $vf9\n"
    "    sqc2 $vf14, 0x0($5)\n"
    "    sqc2 $vf10, 0x0($4)\n"
    "    cfc2.ni $2, $vi16\n"
    "    andi $2, $2, 0xC0\n"
    "    bne $0, $2, .L00122580\n"
    "    addi $4, $4, 0x10 /* handwritten instruction */\n"
    "    addi $9, $9, -0x1 /* handwritten instruction */\n"
    "    bne $0, $9, .L001224FC\n"
    "    addi $5, $5, 0x10 /* handwritten instruction */\n"
    ".L00122580:\n"
    "    jr $31\n"
    "    sltiu $2, $2, 0x1\n"
    ".set pop\n"
    ".size Mtx_ProjectPointsStq, . - Mtx_ProjectPointsStq\n"
);


/* 0x122588: empty, no callers. */
void Vu0_Stub8(void) {
}

/* 0x122590..0x122698: Vec3_AddSub .. Vec3_AddClamp. */

/* 0x122590: Vec3_AddSub, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_AddSub in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_AddSub\n"
    ".type Vec3_AddSub, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_AddSub:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    lqc2 $vf6, 0x0($7)\n"
    "    vadda.xyz $ACC, $vf4, $vf5\n"
    "    vmsubw.xyz $vf4, $vf6, $vf0w\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_AddSub, . - Vec3_AddSub\n"
);

/* 0x1225B0: Vec3_SubAdd, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_SubAdd in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_SubAdd\n"
    ".type Vec3_SubAdd, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_SubAdd:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    lqc2 $vf6, 0x0($7)\n"
    "    vsuba.xyz $ACC, $vf4, $vf5\n"
    "    vmaddw.xyz $vf4, $vf6, $vf0w\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_SubAdd, . - Vec3_SubAdd\n"
);

/* 0x1225D0: Vec3_ScaleAdd, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_ScaleAdd in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_ScaleAdd\n"
    ".type Vec3_ScaleAdd, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_ScaleAdd:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf6\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vmulax.xyz $ACC, $vf4, $vf6x\n"
    "    vmaddw.xyz $vf5, $vf5, $vf0w\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    ".set pop\n"
    ".size Vec3_ScaleAdd, . - Vec3_ScaleAdd\n"
);

/* 0x1225F0: Vec3_ScaleSub, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_ScaleSub in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_ScaleSub\n"
    ".type Vec3_ScaleSub, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_ScaleSub:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf6\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    vmulax.xyz $ACC, $vf4, $vf6x\n"
    "    vmsubw.xyz $vf5, $vf5, $vf0w\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0x0($4)\n"
    ".set pop\n"
    ".size Vec3_ScaleSub, . - Vec3_ScaleSub\n"
);

/* 0x122610: Vec3_Add4, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_Add4 in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_Add4\n"
    ".type Vec3_Add4, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_Add4:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    lqc2 $vf6, 0x0($7)\n"
    "    lqc2 $vf7, 0x0($8)\n"
    "    vadda.xyz $ACC, $vf4, $vf5\n"
    "    vmaddaw.xyz $ACC, $vf6, $vf0w\n"
    "    vmaddw.xyz $vf4, $vf7, $vf0w\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_Add4, . - Vec3_Add4\n"
);

/* 0x122638: Vec4_AddClamp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec4_AddClamp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec4_AddClamp\n"
    ".type Vec4_AddClamp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec4_AddClamp:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf6\n"
    "    mfc1 $8, $f13\n"
    "    qmtc2.ni $8, $vf7\n"
    "    vadd.xyzw $vf4, $vf4, $vf5\n"
    "    vmaxx.xyzw $vf4, $vf4, $vf6x\n"
    "    vminix.xyzw $vf4, $vf4, $vf7x\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec4_AddClamp, . - Vec4_AddClamp\n"
);

/* 0x122668: Vec3_AddClamp, hand-written VU0 assembly in the original (described in config/symbols/vu0_b.txt; C reference
 * Ref_Vec3_AddClamp in src/port/vu0_b.c). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vec3_AddClamp\n"
    ".type Vec3_AddClamp, @function\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    "Vec3_AddClamp:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    lqc2 $vf5, 0x0($6)\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2.ni $8, $vf6\n"
    "    mfc1 $8, $f13\n"
    "    qmtc2.ni $8, $vf7\n"
    "    vadd.xyz $vf4, $vf4, $vf5\n"
    "    vmaxx.xyz $vf4, $vf4, $vf6x\n"
    "    vminix.xyz $vf4, $vf4, $vf7x\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    "    nop\n"
    ".set pop\n"
    ".size Vec3_AddClamp, . - Vec3_AddClamp\n"
);


/*
 * 0x122698: rotates v about a unit axis (Rodrigues): v cos + (axis x v) sin + axis (axis . v)(1 - cos). The three
 * terms are full four-component vectors; w is then replaced by v->w, read AFTER out was written, so with out == v the
 * result's w is the sum's w.
 */
void Vec3_RotateAxis(Vec4 *out, Vec4 *v, Vec4 *axis, f32 angle) {
    Vec4 a;
    Vec4 b;
    Vec4 c;
    f32 cs;
    f32 sn;

    cs = Mathf_Cos(angle);
    sn = Mathf_Sin(angle);
    Vec4_Scale(&a, v, cs);
    Vec3_Cross(&b, axis, v);
    Vec4_Scale(&b, &b, sn);
    Vec4_Scale(&c, axis, Vec3_Dot(axis, v) * (1.0f - cs));
    Vec4_Add(out, &a, &b);
    Vec4_Add(out, out, &c);
    out->w = v->w;
}

/* 0x122790: rotates v about the X axis; same w behaviour as Vec3_RotateAxis. */
void Vec3_RotateX(Vec4 *out, Vec4 *v, f32 angle) {
    Vec4 a;
    Vec4 b;
    Vec4 c;
    f32 cs;
    f32 sn;

    cs = Mathf_Cos(angle);
    sn = Mathf_Sin(angle);
    Vec4_Scale(&a, v, cs);
    b.x = 0.0f;
    b.y = -v->z * sn;
    b.z = v->y * sn;
    b.w = 0.0f;
    c.x = v->x * (1.0f - cs);
    c.y = 0.0f;
    c.z = 0.0f;
    c.w = 0.0f;
    Vec4_Add(out, &a, &b);
    Vec4_Add(out, out, &c);
    out->w = v->w;
}

/* 0x122868: rotates v about the Y axis; same w behaviour as Vec3_RotateAxis. */
void Vec3_RotateY(Vec4 *out, Vec4 *v, f32 angle) {
    Vec4 a;
    Vec4 b;
    Vec4 c;
    f32 cs;
    f32 sn;

    cs = Mathf_Cos(angle);
    sn = Mathf_Sin(angle);
    Vec4_Scale(&a, v, cs);
    b.x = v->z * sn;
    b.y = 0.0f;
    b.z = -v->x * sn;
    b.w = 0.0f;
    c.x = 0.0f;
    c.y = v->y * (1.0f - cs);
    c.z = 0.0f;
    c.w = 0.0f;
    Vec4_Add(out, &a, &b);
    Vec4_Add(out, out, &c);
    out->w = v->w;
}
