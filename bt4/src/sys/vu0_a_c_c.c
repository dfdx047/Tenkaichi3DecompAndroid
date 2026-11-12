#include "common.h"
#include "sys/math3d.h"

/*
 * Matrix builders of the vector / matrix library, 0x1204B8..0x120A80 (config/symbols/vu0_a.txt): the compiled C
 * part of Sony's libvu0 under the game's names. The two diagonal-scale routines in the middle are hand-written VU0
 * assembly in the original and are kept as top-level assembly (same bytes, checked by fdiff).
 * Portable reference versions are in src/port/vu0_a.c.
 */

extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_Translate(Mtx44 *dst, Mtx44 *src, Vec4 *v);
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
extern void Mtx_Transpose(Mtx44 *dst, Mtx44 *src);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_SetZeroW1(Vec4 *v);

/* dst = src rotated about Z by angles.z, then X by angles.x, then Y by angles.y (sceVu0RotMatrix). */
void Mtx_RotateZXY(Mtx44 *dst, Mtx44 *src, Vec4 *angles) {
    Mtx_RotateZ(dst, src, angles->z);
    Mtx_RotateX(dst, dst, angles->x);
    Mtx_RotateY(dst, dst, angles->y);
}

/* dst = src rotated about X by angles.x, then Y by angles.y, then Z by angles.z. */
void Mtx_RotateXYZ(Mtx44 *dst, Mtx44 *src, Vec4 *angles) {
    Mtx_RotateX(dst, src, angles->x);
    Mtx_RotateY(dst, dst, angles->y);
    Mtx_RotateZ(dst, dst, angles->z);
}

/* void Mtx_ScaleDiag(Mtx44 *dst, Mtx44 *src, Vec4 *v): dst = src with m[0][0] *= v.x, m[1][1] *= v.y, m[2][2] *= v.z.
 * Only the diagonal is multiplied: a scale only when the 3x3 part of src is diagonal. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ScaleDiag\n"
    ".type Mtx_ScaleDiag, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Mtx_ScaleDiag:\n"
    "    lqc2 $vf8, 0($6)\n"
    "    lqc2 $vf4, 0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    "    vmulx.x $vf4, $vf4, $vf8x\n"
    "    vmuly.y $vf5, $vf5, $vf8y\n"
    "    vmulz.z $vf6, $vf6, $vf8z\n"
    "    sqc2 $vf4, 0($4)\n"
    "    sqc2 $vf5, 0x10($4)\n"
    "    sqc2 $vf6, 0x20($4)\n"
    "    jr $31\n"
    "    sqc2 $vf7, 0x30($4)\n"
    ".set pop\n"
    ".size Mtx_ScaleDiag, . - Mtx_ScaleDiag\n"
);

/* void Mtx_ScaleDiagUniform(Mtx44 *dst, Mtx44 *src, f32 s): the same with one factor. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mtx_ScaleDiagUniform\n"
    ".type Mtx_ScaleDiagUniform, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Mtx_ScaleDiagUniform:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2 $8, $vf8\n"
    "    lqc2 $vf4, 0($5)\n"
    "    lqc2 $vf5, 0x10($5)\n"
    "    lqc2 $vf6, 0x20($5)\n"
    "    lqc2 $vf7, 0x30($5)\n"
    "    vmulx.x $vf4, $vf4, $vf8x\n"
    "    vmulx.y $vf5, $vf5, $vf8x\n"
    "    vmulx.z $vf6, $vf6, $vf8x\n"
    "    sqc2 $vf4, 0($4)\n"
    "    sqc2 $vf5, 0x10($4)\n"
    "    sqc2 $vf6, 0x20($4)\n"
    "    jr $31\n"
    "    sqc2 $vf7, 0x30($4)\n"
    ".set pop\n"
    ".size Mtx_ScaleDiagUniform, . - Mtx_ScaleDiagUniform\n"
);

/* Mtx_RotateZXY into dst, then Mtx_Translate(dst, src, pos): the second call copies the rows of src again, so the
 * rotation survives only when dst == src (original bug; no callers). */
void Mtx_RotateZXYTranslate(Mtx44 *dst, Mtx44 *src, Vec4 *angles, Vec4 *pos) {
    Mtx_RotateZXY(dst, src, angles);
    Mtx_Translate(dst, src, pos);
}

/* World-to-view matrix of a camera at pos looking along zdir with ydir up (sceVu0CameraMatrix). */
void Mtx_MakeCamera(Mtx44 *m, Vec4 *pos, Vec4 *zdir, Vec4 *ydir) {
    Mtx44 m0;
    Vec4 xd;

    Mtx_StoreIdentity(&m0);
    Vec3_Cross(&xd, ydir, zdir);
    Vec3_Normalize((Vec4 *)m0.m[0], &xd);
    Vec3_Normalize((Vec4 *)m0.m[2], zdir);
    Vec3_Cross((Vec4 *)m0.m[1], (Vec4 *)m0.m[2], (Vec4 *)m0.m[0]);
    Mtx_Translate(&m0, &m0, pos);
    Mtx_InverseRT(m, &m0);
}

/* m = the four rows given (sceVu0LightColorMatrix: three light colours and the ambient colour). */
void Mtx_SetRows(Mtx44 *m, Vec4 *r0, Vec4 *r1, Vec4 *r2, Vec4 *r3) {
    Vec4_Copy((Vec4 *)m->m[0], r0);
    Vec4_Copy((Vec4 *)m->m[1], r1);
    Vec4_Copy((Vec4 *)m->m[2], r2);
    Vec4_Copy((Vec4 *)m->m[3], r3);
}

/* m = transpose of the matrix whose rows are the normalised negated light directions (sceVu0NormalLightMatrix). */
void Mtx_MakeNormalLight(Mtx44 *m, Vec4 *l0, Vec4 *l1, Vec4 *l2) {
    Mtx44 mt;
    Vec4 t;

    Vec4_Scale(&t, l0, -1.0f);
    Vec3_Normalize((Vec4 *)mt.m[0], &t);
    Vec4_Scale(&t, l1, -1.0f);
    Vec3_Normalize((Vec4 *)mt.m[1], &t);
    Vec4_Scale(&t, l2, -1.0f);
    Vec3_Normalize((Vec4 *)mt.m[2], &t);
    Vec4_SetZeroW1((Vec4 *)mt.m[3]);
    Mtx_Transpose(m, &mt);
}

/* View-to-screen matrix (sceVu0ViewScreenMatrix): perspective with projection distance scrz, scale (ax, ay),
 * centre (cx, cy) and the depth range [nearz, farz] mapped to [zmax, zmin]. */
void Mtx_MakeViewScreen(Mtx44 *m, f32 scrz, f32 ax, f32 ay, f32 cx, f32 cy, f32 zmin, f32 zmax, f32 nearz, f32 farz) {
    f32 az;
    f32 cz;
    Mtx44 mp;
    Mtx44 mt;

    cz = (-zmax * nearz + zmin * farz) / (-nearz + farz);
    az = farz * nearz * (-zmin + zmax) / (-nearz + farz);
    Mtx_StoreIdentity(&mp);
    mp.m[0][0] = scrz;
    mp.m[1][1] = scrz;
    mp.m[2][2] = 0.0f;
    mp.m[3][3] = 0.0f;
    mp.m[3][2] = 1.0f;
    mp.m[2][3] = 1.0f;
    Mtx_StoreIdentity(&mt);
    mt.m[0][0] = ax;
    mt.m[1][1] = ay;
    mt.m[2][2] = az;
    mt.m[3][0] = cx;
    mt.m[3][1] = cy;
    mt.m[3][2] = cz;
    Mtx_Mul(m, &mt, &mp);
}

/* Projection onto the plane a x + b y + c z = 1 from a point light at lp (mode != 0) or along the direction lp
 * (mode == 0) (sceVu0DropShadowMatrix). Plain FPU arithmetic. */
void Mtx_MakeDropShadow(Mtx44 *m, Vec4 *lp, f32 a, f32 b, f32 c, s32 mode) {
    if (mode) {
        f32 x = lp->x, y = lp->y, z = lp->z;
        f32 d = 1.0f - (a * x + b * y + c * z);

        m->m[0][0] = a * x + d, m->m[1][0] = b * x, m->m[2][0] = c * x, m->m[3][0] = -x;
        m->m[0][1] = a * y, m->m[1][1] = b * y + d, m->m[2][1] = c * y, m->m[3][1] = -y;
        m->m[0][2] = a * z, m->m[1][2] = b * z, m->m[2][2] = c * z + d, m->m[3][2] = -z;
        m->m[0][3] = a, m->m[1][3] = b, m->m[2][3] = c, m->m[3][3] = d - 1.0f;
    } else {
        f32 p = lp->x, q = lp->y, r = lp->z;
        f32 n = a * p + b * q + c * r;
        f32 nr = -1.0f / n;

        m->m[0][0] = nr * (a * p - n), m->m[1][0] = nr * (b * p), m->m[2][0] = nr * (c * p), m->m[3][0] = nr * (-p);
        m->m[0][1] = nr * (a * q), m->m[1][1] = nr * (b * q - n), m->m[2][1] = nr * (c * q), m->m[3][1] = nr * (-q);
        m->m[0][2] = nr * (a * r), m->m[1][2] = nr * (b * r), m->m[2][2] = nr * (c * r - n), m->m[3][2] = nr * (-r);
        m->m[0][3] = 0.0f, m->m[1][3] = 0.0f, m->m[2][3] = 0.0f, m->m[3][3] = nr * (-n);
    }
}

/*
 * The current matrix (VU0 registers vf16-19) and its stack in VU0 data memory, 0x120A80..0x121008. Hand-written VU0
 * assembly in the original, kept as top-level assembly (same bytes, checked by fdiff); the compiled C functions
 * between them are C.
 */

extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Vec3_DirToEuler(Vec4 *out, Vec4 *dir);
extern Vec4 gVu0AxisZW; /* (0, 0, 1, 1) */

void Vu0Cur_Pop(void);
s32 Vu0Cur_GetStackDepth(void);
void Vu0Cur_StoreMtx(Mtx44 *m);
void Vu0Cur_Translate(Vec4 *v);
void Vu0Cur_RotateZ(f32 angle);
void Vu0Cur_RotateX(f32 angle);
void Vu0Cur_RotateY(f32 angle);

/* void Vu0Cur_Init(void): current matrix = identity built from vf0 (three vmr32), stack pointer vi15 = 0. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_Init\n"
    ".type Vu0Cur_Init, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_Init:\n"
    "    vmove.xyzw $vf19, $vf0\n"
    "    vmr32.xyzw $vf18, $vf19\n"
    "    vmr32.xyzw $vf17, $vf18\n"
    "    vmr32.xyzw $vf16, $vf17\n"
    "    jr $31\n"
    "    viaddi $vi15, $vi0, 0x0\n"
    ".set pop\n"
    ".size Vu0Cur_Init, . - Vu0Cur_Init\n"
);

/* void Vu0Cur_LoadIdentity(void): current matrix = identity (rows vf3, vf2, vf1, vf0). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_LoadIdentity\n"
    ".type Vu0Cur_LoadIdentity, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_LoadIdentity:\n"
    "    vmove.xyzw $vf16, $vf3\n"
    "    vmove.xyzw $vf17, $vf2\n"
    "    vmove.xyzw $vf18, $vf1\n"
    "    jr $31\n"
    "    vmove.xyzw $vf19, $vf0\n"
    ".set pop\n"
    ".size Vu0Cur_LoadIdentity, . - Vu0Cur_LoadIdentity\n"
);

/* void Vu0Cur_Push(void): VU0 memory[vi15++] = row, for rows 0..3. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_Push\n"
    ".type Vu0Cur_Push, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_Push:\n"
    "    vsqi.xyzw $vf16, ($vi15++)\n"
    "    vsqi.xyzw $vf17, ($vi15++)\n"
    "    vsqi.xyzw $vf18, ($vi15++)\n"
    "    jr $31\n"
    "    vsqi.xyzw $vf19, ($vi15++)\n"
    ".set pop\n"
    ".size Vu0Cur_Push, . - Vu0Cur_Push\n"
);

/* void Vu0Cur_Pop(void): row = VU0 memory[--vi15], for rows 3..0. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_Pop\n"
    ".type Vu0Cur_Pop, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_Pop:\n"
    "    vlqd.xyzw $vf19, (--$vi15)\n"
    "    vlqd.xyzw $vf18, (--$vi15)\n"
    "    vlqd.xyzw $vf17, (--$vi15)\n"
    "    jr $31\n"
    "    vlqd.xyzw $vf16, (--$vi15)\n"
    ".set pop\n"
    ".size Vu0Cur_Pop, . - Vu0Cur_Pop\n"
);

/* Pops n matrices; the current matrix ends as the last one popped. */
void Vu0Cur_PopN(s32 n) {
    s32 i;

    for (i = 0; i < n; i++) {
        Vu0Cur_Pop();
    }
}

/* void Vu0Cur_ResetStack(void): stack pointer vi15 = 0; the current matrix is untouched. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ResetStack\n"
    ".type Vu0Cur_ResetStack, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_ResetStack:\n"
    "    jr $31\n"
    "    viaddi $vi15, $vi0, 0x0\n"
    ".set pop\n"
    ".size Vu0Cur_ResetStack, . - Vu0Cur_ResetStack\n"
);

/* Number of matrices on the stack: the stack pointer (a signed 16-bit count of rows) / 4. */
s32 Vu0Cur_GetStackDepth(void) {
    register s32 rows __asm__("$3");

    __asm__ volatile(".word 0x48437800" /* cfc2 $3, $vi15 (as a word: gas would add a hazard nop behind it) */ : "=r"(rows));
    return (s16)rows / 4;
}

/* 1 when the stack is not empty. */
s32 Vu0Cur_IsStackUsed(void) {
    return Vu0Cur_GetStackDepth() != 0;
}

/* void Vu0Cur_LoadIdentityRot(void): rows 0-2 = identity, row 3 untouched. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_LoadIdentityRot\n"
    ".type Vu0Cur_LoadIdentityRot, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_LoadIdentityRot:\n"
    "    vmove.xyzw $vf16, $vf3\n"
    "    vmove.xyzw $vf17, $vf2\n"
    "    jr $31\n"
    "    vmove.xyzw $vf18, $vf1\n"
    ".set pop\n"
    ".size Vu0Cur_LoadIdentityRot, . - Vu0Cur_LoadIdentityRot\n"
);

/* void Vu0Cur_ClearTrans(void): row 3 = (0, 0, 0, 1). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ClearTrans\n"
    ".type Vu0Cur_ClearTrans, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_ClearTrans:\n"
    "    jr $31\n"
    "    vmove.xyzw $vf19, $vf0\n"
    ".set pop\n"
    ".size Vu0Cur_ClearTrans, . - Vu0Cur_ClearTrans\n"
);

/* void Vu0Cur_LoadMtx(Mtx44 *m): current matrix = m. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_LoadMtx\n"
    ".type Vu0Cur_LoadMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_LoadMtx:\n"
    "    lqc2 $vf16, 0x0($4)\n"
    "    lqc2 $vf17, 0x10($4)\n"
    "    lqc2 $vf18, 0x20($4)\n"
    "    jr $31\n"
    "    lqc2 $vf19, 0x30($4)\n"
    ".set pop\n"
    ".size Vu0Cur_LoadMtx, . - Vu0Cur_LoadMtx\n"
);

/* void Vu0Cur_StoreMtx(Mtx44 *m): m = current matrix. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_StoreMtx\n"
    ".type Vu0Cur_StoreMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_StoreMtx:\n"
    "    sqc2 $vf16, 0x0($4)\n"
    "    sqc2 $vf17, 0x10($4)\n"
    "    sqc2 $vf18, 0x20($4)\n"
    "    jr $31\n"
    "    sqc2 $vf19, 0x30($4)\n"
    ".set pop\n"
    ".size Vu0Cur_StoreMtx, . - Vu0Cur_StoreMtx\n"
);

/* void Vu0Cur_GetTrans(Vec4 *v): v = row 3. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_GetTrans\n"
    ".type Vu0Cur_GetTrans, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_GetTrans:\n"
    "    jr $31\n"
    "    sqc2 $vf19, 0x0($4)\n"
    ".set pop\n"
    ".size Vu0Cur_GetTrans, . - Vu0Cur_GetTrans\n"
);

/* angles = Vec3_DirToEuler of the current matrix times (0, 0, 1, 1) = row 2 + row 3: unlike Mtx_AxisZToEuler the
 * translation is not removed first (original bug; no callers). */
void Vu0Cur_AxisZToEuler(Vec4 *angles) {
    Vec4 dir;
    Mtx44 mt;

    Vu0Cur_StoreMtx(&mt);
    Mtx_MulVec4(&dir, &mt, &gVu0AxisZW);
    Vec3_DirToEuler(angles, &dir);
}

/* void Vu0Cur_SetTrans(Vec4 *v): row 3 = v (all four components). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_SetTrans\n"
    ".type Vu0Cur_SetTrans, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_SetTrans:\n"
    "    jr $31\n"
    "    lqc2 $vf19, 0x0($4)\n"
    ".set pop\n"
    ".size Vu0Cur_SetTrans, . - Vu0Cur_SetTrans\n"
);

/* void Vu0Cur_Translate(Vec4 *v): row 3 x, y, z += v. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_Translate\n"
    ".type Vu0Cur_Translate, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_Translate:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    jr $31\n"
    "    vadd.xyz $vf19, $vf19, $vf4\n"
    ".set pop\n"
    ".size Vu0Cur_Translate, . - Vu0Cur_Translate\n"
);

/* void Vu0Cur_TranslateLocal(Vec4 *v): row 3 = row0 * v.x + row1 * v.y + row2 * v.z + row3 * v.w. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_TranslateLocal\n"
    ".type Vu0Cur_TranslateLocal, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_TranslateLocal:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf19, $vf19, $vf4w\n"
    ".set pop\n"
    ".size Vu0Cur_TranslateLocal, . - Vu0Cur_TranslateLocal\n"
);

/* void Vu0Cur_MulMtx(Mtx44 *m): row[i] = sum over k of old row[k] * m[i][k] (Mtx_Mul(cur, cur, m)). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_MulMtx\n"
    ".type Vu0Cur_MulMtx, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_MulMtx:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x10($4)\n"
    "    lqc2 $vf6, 0x20($4)\n"
    "    lqc2 $vf7, 0x30($4)\n"
    "    vmove.xyzw $vf8, $vf16\n"
    "    vmove.xyzw $vf9, $vf17\n"
    "    vmove.xyzw $vf10, $vf18\n"
    "    vmove.xyzw $vf11, $vf19\n"
    "    vmulax.xyzw $ACC, $vf8, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf10, $vf4z\n"
    "    vmaddw.xyzw $vf16, $vf11, $vf4w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf5x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf5y\n"
    "    vmaddaz.xyzw $ACC, $vf10, $vf5z\n"
    "    vmaddw.xyzw $vf17, $vf11, $vf5w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf6x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf6y\n"
    "    vmaddaz.xyzw $ACC, $vf10, $vf6z\n"
    "    vmaddw.xyzw $vf18, $vf11, $vf6w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf7x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf7y\n"
    "    vmaddaz.xyzw $ACC, $vf10, $vf7z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf19, $vf11, $vf7w\n"
    ".set pop\n"
    ".size Vu0Cur_MulMtx, . - Vu0Cur_MulMtx\n"
);

/* void Vu0Cur_MulMtxRev(Mtx44 *m): row[i] = sum over k of m.row[k] * old row[i][k] (Mtx_Mul(cur, m, cur)). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_MulMtxRev\n"
    ".type Vu0Cur_MulMtxRev, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_MulMtxRev:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    lqc2 $vf5, 0x10($4)\n"
    "    lqc2 $vf6, 0x20($4)\n"
    "    lqc2 $vf7, 0x30($4)\n"
    "    vmulax.xyzw $ACC, $vf4, $vf16x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf16y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf16z\n"
    "    vmaddw.xyzw $vf16, $vf7, $vf16w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf17x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf17y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf17z\n"
    "    vmaddw.xyzw $vf17, $vf7, $vf17w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf18x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf18y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf18z\n"
    "    vmaddw.xyzw $vf18, $vf7, $vf18w\n"
    "    vmulax.xyzw $ACC, $vf4, $vf19x\n"
    "    vmadday.xyzw $ACC, $vf5, $vf19y\n"
    "    vmaddaz.xyzw $ACC, $vf6, $vf19z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf19, $vf7, $vf19w\n"
    ".set pop\n"
    ".size Vu0Cur_MulMtxRev, . - Vu0Cur_MulMtxRev\n"
);

/* void Vu0Cur_Transpose(void). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_Transpose\n"
    ".type Vu0Cur_Transpose, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_Transpose:\n"
    "    qmfc2 $8, $vf16\n"
    "    qmfc2 $9, $vf17\n"
    "    qmfc2 $10, $vf18\n"
    "    qmfc2 $11, $vf19\n"
    "    pextlw $12, $9, $8\n"
    "    pextuw $13, $9, $8\n"
    "    pextlw $14, $11, $10\n"
    "    pextuw $15, $11, $10\n"
    "    pcpyld $8, $14, $12\n"
    "    pcpyud $9, $12, $14\n"
    "    pcpyld $10, $15, $13\n"
    "    pcpyud $11, $13, $15\n"
    "    qmtc2 $8, $vf16\n"
    "    qmtc2 $9, $vf17\n"
    "    qmtc2 $10, $vf18\n"
    "    jr $31\n"
    "    qmtc2 $11, $vf19\n"
    ".set pop\n"
    ".size Vu0Cur_Transpose, . - Vu0Cur_Transpose\n"
);

/* void Vu0Cur_InverseRT(void): Mtx_InverseRT on the current matrix. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_InverseRT\n"
    ".type Vu0Cur_InverseRT, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_InverseRT:\n"
    "    qmfc2 $8, $vf16\n"
    "    qmfc2 $9, $vf17\n"
    "    qmfc2 $10, $vf18\n"
    "    vmove.xyzw $vf4, $vf19\n"
    "    vmove.xyz $vf4, $vf0\n"
    "    qmfc2 $11, $vf4\n"
    "    pextlw $12, $9, $8\n"
    "    pextuw $13, $9, $8\n"
    "    pextlw $14, $11, $10\n"
    "    pextuw $15, $11, $10\n"
    "    pcpyld $8, $14, $12\n"
    "    pcpyud $9, $12, $14\n"
    "    pcpyld $10, $15, $13\n"
    "    qmtc2 $8, $vf6\n"
    "    qmtc2 $9, $vf7\n"
    "    qmtc2 $10, $vf8\n"
    "    vmulax.xyz $ACC, $vf6, $vf19x\n"
    "    vmadday.xyz $ACC, $vf7, $vf19y\n"
    "    vmaddz.xyz $vf4, $vf8, $vf19z\n"
    "    vsub.xyz $vf4, $vf0, $vf4\n"
    "    qmtc2 $8, $vf16\n"
    "    qmtc2 $9, $vf17\n"
    "    qmtc2 $10, $vf18\n"
    "    jr $31\n"
    "    vmove.xyzw $vf19, $vf4\n"
    ".set pop\n"
    ".size Vu0Cur_InverseRT, . - Vu0Cur_InverseRT\n"
);

/* void Vu0Cur_RotateZ(f32 angle): Mtx_RotateZ on the current matrix. Keeps $ra in $a2 around Vu0_SinCos. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_RotateZ\n"
    ".type Vu0Cur_RotateZ, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_RotateZ:\n"
    "    daddu $6, $31, $zero\n"
    "    jal Vu0_SinCos\n"
    "    nop\n"
    "    daddu $31, $6, $zero\n"
    "    vsub.zw $vf8, $vf0, $vf0\n"
    "    vaddy.x $vf8, $vf0, $vf12y\n"
    "    vaddx.y $vf8, $vf0, $vf12x\n"
    "    vsub.zw $vf9, $vf0, $vf0\n"
    "    vsubx.x $vf9, $vf0, $vf12x\n"
    "    vaddy.y $vf9, $vf0, $vf12y\n"
    "    vmulax.xyzw $ACC, $vf8, $vf16x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf16y\n"
    "    vmaddaz.xyzw $ACC, $vf1, $vf16z\n"
    "    vmaddw.xyzw $vf16, $vf0, $vf16w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf17x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf17y\n"
    "    vmaddaz.xyzw $ACC, $vf1, $vf17z\n"
    "    vmaddw.xyzw $vf17, $vf0, $vf17w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf18x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf18y\n"
    "    vmaddaz.xyzw $ACC, $vf1, $vf18z\n"
    "    vmaddw.xyzw $vf18, $vf0, $vf18w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf19x\n"
    "    vmadday.xyzw $ACC, $vf9, $vf19y\n"
    "    vmaddaz.xyzw $ACC, $vf1, $vf19z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf19, $vf0, $vf19w\n"
    ".set pop\n"
    ".size Vu0Cur_RotateZ, . - Vu0Cur_RotateZ\n"
);

/* void Vu0Cur_RotateX(f32 angle): Mtx_RotateX on the current matrix. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_RotateX\n"
    ".type Vu0Cur_RotateX, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_RotateX:\n"
    "    daddu $6, $31, $zero\n"
    "    jal Vu0_SinCos\n"
    "    nop\n"
    "    daddu $31, $6, $zero\n"
    "    vsub.xw $vf8, $vf0, $vf0\n"
    "    vaddy.y $vf8, $vf0, $vf12y\n"
    "    vaddx.z $vf8, $vf0, $vf12x\n"
    "    vsub.xw $vf9, $vf0, $vf0\n"
    "    vsubx.y $vf9, $vf0, $vf12x\n"
    "    vaddy.z $vf9, $vf0, $vf12y\n"
    "    vmulax.xyzw $ACC, $vf3, $vf16x\n"
    "    vmadday.xyzw $ACC, $vf8, $vf16y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf16z\n"
    "    vmaddw.xyzw $vf16, $vf0, $vf16w\n"
    "    vmulax.xyzw $ACC, $vf3, $vf17x\n"
    "    vmadday.xyzw $ACC, $vf8, $vf17y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf17z\n"
    "    vmaddw.xyzw $vf17, $vf0, $vf17w\n"
    "    vmulax.xyzw $ACC, $vf3, $vf18x\n"
    "    vmadday.xyzw $ACC, $vf8, $vf18y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf18z\n"
    "    vmaddw.xyzw $vf18, $vf0, $vf18w\n"
    "    vmulax.xyzw $ACC, $vf3, $vf19x\n"
    "    vmadday.xyzw $ACC, $vf8, $vf19y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf19z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf19, $vf0, $vf19w\n"
    ".set pop\n"
    ".size Vu0Cur_RotateX, . - Vu0Cur_RotateX\n"
);

/* void Vu0Cur_RotateY(f32 angle): Mtx_RotateY on the current matrix. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_RotateY\n"
    ".type Vu0Cur_RotateY, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_RotateY:\n"
    "    daddu $6, $31, $zero\n"
    "    jal Vu0_SinCos\n"
    "    nop\n"
    "    daddu $31, $6, $zero\n"
    "    vsub.yw $vf8, $vf0, $vf0\n"
    "    vaddy.x $vf8, $vf0, $vf12y\n"
    "    vsubx.z $vf8, $vf0, $vf12x\n"
    "    vsub.yw $vf9, $vf0, $vf0\n"
    "    vaddx.x $vf9, $vf0, $vf12x\n"
    "    vaddy.z $vf9, $vf0, $vf12y\n"
    "    vmulax.xyzw $ACC, $vf8, $vf16x\n"
    "    vmadday.xyzw $ACC, $vf2, $vf16y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf16z\n"
    "    vmaddw.xyzw $vf16, $vf0, $vf16w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf17x\n"
    "    vmadday.xyzw $ACC, $vf2, $vf17y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf17z\n"
    "    vmaddw.xyzw $vf17, $vf0, $vf17w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf18x\n"
    "    vmadday.xyzw $ACC, $vf2, $vf18y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf18z\n"
    "    vmaddw.xyzw $vf18, $vf0, $vf18w\n"
    "    vmulax.xyzw $ACC, $vf8, $vf19x\n"
    "    vmadday.xyzw $ACC, $vf2, $vf19y\n"
    "    vmaddaz.xyzw $ACC, $vf9, $vf19z\n"
    "    jr $31\n"
    "    vmaddw.xyzw $vf19, $vf0, $vf19w\n"
    ".set pop\n"
    ".size Vu0Cur_RotateY, . - Vu0Cur_RotateY\n"
);

/* Rotates the current matrix about Z by angles.z, then X by angles.x, then Y by angles.y. */
void Vu0Cur_RotateZXY(Vec4 *angles) {
    Vu0Cur_RotateZ(angles->z);
    Vu0Cur_RotateX(angles->x);
    Vu0Cur_RotateY(angles->y);
}

/* Rotates the current matrix about X, then Y, then Z. */
void Vu0Cur_RotateXYZ(Vec4 *angles) {
    Vu0Cur_RotateX(angles->x);
    Vu0Cur_RotateY(angles->y);
    Vu0Cur_RotateZ(angles->z);
}

/* void Vu0Cur_ScaleDiag(Vec4 *v): m[0][0] *= v.x, m[1][1] *= v.y, m[2][2] *= v.z (diagonal only). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ScaleDiag\n"
    ".type Vu0Cur_ScaleDiag, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_ScaleDiag:\n"
    "    lqc2 $vf4, 0x0($4)\n"
    "    vmulx.x $vf16, $vf16, $vf4x\n"
    "    vmuly.y $vf17, $vf17, $vf4y\n"
    "    jr $31\n"
    "    vmulz.z $vf18, $vf18, $vf4z\n"
    ".set pop\n"
    ".size Vu0Cur_ScaleDiag, . - Vu0Cur_ScaleDiag\n"
);

/* void Vu0Cur_ScaleDiagUniform(f32 s): the three diagonal elements *= s. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_ScaleDiagUniform\n"
    ".type Vu0Cur_ScaleDiagUniform, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_ScaleDiagUniform:\n"
    "    mfc1 $8, $f12\n"
    "    qmtc2 $8, $vf4\n"
    "    vmulx.x $vf16, $vf16, $vf4x\n"
    "    vmulx.y $vf17, $vf17, $vf4x\n"
    "    jr $31\n"
    "    vmulx.z $vf18, $vf18, $vf4x\n"
    ".set pop\n"
    ".size Vu0Cur_ScaleDiagUniform, . - Vu0Cur_ScaleDiagUniform\n"
);

/* Vu0Cur_RotateZXY(angles), then Vu0Cur_Translate(pos). */
void Vu0Cur_RotateZXYTranslate(Vec4 *angles, Vec4 *pos) {
    Vu0Cur_RotateZXY(angles);
    Vu0Cur_Translate(pos);
}

/* void Vu0Cur_MulVec4(Vec4 *out, Vec4 *v): out = row0 * v.x + row1 * v.y + row2 * v.z + row3 * v.w. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_MulVec4\n"
    ".type Vu0Cur_MulVec4, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_MulVec4:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyzw $vf4, $vf19, $vf4w\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    ".set pop\n"
    ".size Vu0Cur_MulVec4, . - Vu0Cur_MulVec4\n"
);

/* void Vu0Cur_MulVec3(Vec4 *out, Vec4 *v): the same sum, x, y, z only; out.w = v.w. */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Vu0Cur_MulVec3\n"
    ".type Vu0Cur_MulVec3, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Vu0Cur_MulVec3:\n"
    "    lqc2 $vf4, 0x0($5)\n"
    "    vmulax.xyzw $ACC, $vf16, $vf4x\n"
    "    vmadday.xyzw $ACC, $vf17, $vf4y\n"
    "    vmaddaz.xyzw $ACC, $vf18, $vf4z\n"
    "    vmaddw.xyz $vf4, $vf19, $vf4w\n"
    "    jr $31\n"
    "    sqc2 $vf4, 0x0($4)\n"
    ".set pop\n"
    ".size Vu0Cur_MulVec3, . - Vu0Cur_MulVec3\n"
);
