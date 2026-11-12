#include "common.h"

/*
 * Integer vector helpers, 0x11FA10..0x11FE80: the head of the vector / matrix library (config/symbols/vu0_a.txt).
 * A vector is four s32. The IVec3 forms leave the fourth word of the destination untouched, except IVec3_Clamp.
 * IVec4_Swap, IVec4_Copy and the three VU0 conversions are hand-written assembly in the original and are kept as
 * top-level assembly here (same bytes, checked by fdiff). Portable reference versions of the whole library half
 * are in src/port/vu0_a.c.
 */

typedef struct IVec4 {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 w;
} IVec4; /* size 0x10 */

/* v = (0, 0, 0, 1). */
void IVec4_SetZeroW1(IVec4 *v) {
    v->w = 1;
    v->y = 0;
    v->z = 0;
    v->x = 0;
}

/* v = (0, 0, 0, 0). */
void IVec4_SetZero(IVec4 *v) {
    v->x = 0;
    v->y = 0;
    v->z = 0;
    v->w = 0;
}

/* v = (x, y, z, w). */
void IVec4_Set(IVec4 *v, s32 x, s32 y, s32 z, s32 w) {
    v->x = x;
    v->y = y;
    v->z = z;
    v->w = w;
}

/* v = (x, y, z), w untouched. */
void IVec3_Set(IVec4 *v, s32 x, s32 y, s32 z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

/* void IVec4_Swap(IVec4 *a, IVec4 *b): swaps two vectors with 128-bit moves. Hand-written assembly ($t0 / $t1); the
 * compiler's own 128-bit moves use $v0 / $v1:
 *     IVecQword t0 = *(IVecQword *)a, t1 = *(IVecQword *)b; *(IVecQword *)b = t0; *(IVecQword *)a = t1; */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl IVec4_Swap\n"
    ".type IVec4_Swap, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "IVec4_Swap:\n"
    "    lq $8, 0($4)\n"
    "    lq $9, 0($5)\n"
    "    sq $8, 0($5)\n"
    "    jr $31\n"
    "    sq $9, 0($4)\n"
    ".set pop\n"
    ".size IVec4_Swap, . - IVec4_Swap\n"
);

/* out = a + b on four components. */
void IVec4_Add(IVec4 *out, IVec4 *a, IVec4 *b) {
    out->x = a->x + b->x;
    out->y = a->y + b->y;
    out->z = a->z + b->z;
    out->w = a->w + b->w;
}

/* out = a + b on x, y, z. */
void IVec3_Add(IVec4 *out, IVec4 *a, IVec4 *b) {
    out->x = a->x + b->x;
    out->y = a->y + b->y;
    out->z = a->z + b->z;
}

/* out = a - b on four components. */
void IVec4_Sub(IVec4 *out, IVec4 *a, IVec4 *b) {
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
    out->w = a->w - b->w;
}

/* out = a - b on x, y, z. */
void IVec3_Sub(IVec4 *out, IVec4 *a, IVec4 *b) {
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}

/* out = a * b per component (low 32 bits), four components. */
void IVec4_Mul(IVec4 *out, IVec4 *a, IVec4 *b) {
    out->x = a->x * b->x;
    out->y = a->y * b->y;
    out->z = a->z * b->z;
    out->w = a->w * b->w;
}

/* out = a * b per component on x, y, z. */
void IVec3_Mul(IVec4 *out, IVec4 *a, IVec4 *b) {
    out->x = a->x * b->x;
    out->y = a->y * b->y;
    out->z = a->z * b->z;
}

/* out = a * s on four components. */
void IVec4_Scale(IVec4 *out, IVec4 *a, s32 s) {
    out->x = a->x * s;
    out->y = a->y * s;
    out->z = a->z * s;
    out->w = a->w * s;
}

/* out = a * s on x, y, z. */
void IVec3_Scale(IVec4 *out, IVec4 *a, s32 s) {
    out->x = a->x * s;
    out->y = a->y * s;
    out->z = a->z * s;
}

/* out = a / s (signed, truncating) on four components. */
void IVec4_Div(IVec4 *out, IVec4 *a, s32 s) {
    out->x = a->x / s;
    out->y = a->y / s;
    out->z = a->z / s;
    out->w = a->w / s;
}

/* out = a / s (signed, truncating) on x, y, z. */
void IVec3_Div(IVec4 *out, IVec4 *a, s32 s) {
    out->x = a->x / s;
    out->y = a->y / s;
    out->z = a->z / s;
}

/* void IVec4_Copy(IVec4 *out, IVec4 *in): out = in, one 128-bit move. Hand-written assembly ($t0). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl IVec4_Copy\n"
    ".type IVec4_Copy, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "IVec4_Copy:\n"
    "    lq $8, 0($5)\n"
    "    jr $31\n"
    "    sq $8, 0($4)\n"
    ".set pop\n"
    ".size IVec4_Copy, . - IVec4_Copy\n"
);

/* out = in on x, y, z. */
void IVec3_Copy(IVec4 *out, IVec4 *in) {
    out->x = in->x;
    out->y = in->y;
    out->z = in->z;
}

/* void IVec4_ToFloat12(f32 *out, IVec4 *in): out = (f32)in / 4096 on four components (VU0 vitof12). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl IVec4_ToFloat12\n"
    ".type IVec4_ToFloat12, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "IVec4_ToFloat12:\n"
    "    lqc2 $vf4, 0($5)\n"
    "    vitof12.xyzw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0($4)\n"
    ".set pop\n"
    ".size IVec4_ToFloat12, . - IVec4_ToFloat12\n"
);

/* void IVec4_ToFloat4(f32 *out, IVec4 *in): out = (f32)in / 16 on four components (VU0 vitof4). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl IVec4_ToFloat4\n"
    ".type IVec4_ToFloat4, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "IVec4_ToFloat4:\n"
    "    lqc2 $vf4, 0($5)\n"
    "    vitof4.xyzw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0($4)\n"
    ".set pop\n"
    ".size IVec4_ToFloat4, . - IVec4_ToFloat4\n"
);

/* void IVec4_ToFloat(f32 *out, IVec4 *in): out = (f32)in on four components (VU0 vitof0). */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl IVec4_ToFloat\n"
    ".type IVec4_ToFloat, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "IVec4_ToFloat:\n"
    "    lqc2 $vf4, 0($5)\n"
    "    vitof0.xyzw $vf5, $vf4\n"
    "    jr $31\n"
    "    sqc2 $vf5, 0($4)\n"
    ".set pop\n"
    ".size IVec4_ToFloat, . - IVec4_ToFloat\n"
);

/* out = in clamped to [lo, hi] (signed) on four components; lo wins when in < lo. */
void IVec4_Clamp(IVec4 *out, IVec4 *in, s32 lo, s32 hi) {
    out->x = in->x < lo ? lo : (hi < in->x ? hi : in->x);
    out->y = in->y < lo ? lo : (hi < in->y ? hi : in->y);
    out->z = in->z < lo ? lo : (hi < in->z ? hi : in->z);
    out->w = in->w < lo ? lo : (hi < in->w ? hi : in->w);
}

/* out = in clamped to [lo, hi] (signed) on x, y, z; out.w = in.w. */
void IVec3_Clamp(IVec4 *out, IVec4 *in, s32 lo, s32 hi) {
    out->x = in->x < lo ? lo : (hi < in->x ? hi : in->x);
    out->y = in->y < lo ? lo : (hi < in->y ? hi : in->y);
    out->z = in->z < lo ? lo : (hi < in->z ? hi : in->z);
    out->w = in->w;
}

/* Empty; no callers. */
void IVec_Stub(void) {
}
