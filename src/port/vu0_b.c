/*
 * Portable reference of the vector / matrix library, second half: original 0x121008..0x122940.
 *
 * NOT part of the matching build. One Ref_<Name> per original function, in address order. Each mirrors the original
 * instruction by instruction: the same single-precision operations in the same order and association, the same
 * components written, the same behaviour when the output aliases an input (inputs are read into locals where the
 * original loads them into registers, and the output is stored where the original stores it).
 *
 * Conventions of the original that the code below relies on:
 *   vf0 = (0, 0, 0, 1) hard-wired. vf1 = (0, 0, 1, 0), vf2 = (0, 1, 0, 0), vf3 = (1, 0, 0, 0): set once at boot by
 *   0x120088 and used as constants (the dot product multiplies its third term by vf3.x; the projections take the
 *   1.0 of "q" from vf1.z). They are read from gRefVu0 here, so they must hold those values.
 *   ACC: "vmula" writes ACC = a * b, "vmadda" ACC = ACC + a * b, "vmadd" dest = ACC + a * b, "vmsub" dest = ACC - a * b,
 *   "vadda" / "vsuba" ACC = a +/- b. Every multiply and every add is a separate rounded operation (no fused multiply-add).
 *   Q: "vdiv Q, a, b" Q = a / b; "vsqrt Q, a" Q = sqrt(|a|). Division by zero gives +/-max, never infinity.
 *   A destination mask (.xyz, .x ...) limits which components of the destination REGISTER change; "sqc2" then always
 *   stores all four components, so a ".xyz" routine writes out.w too: with whatever the destination register held
 *   (normally the first input's w).
 *
 * Arithmetic model. By default this file shares the first half's state and primitives (include/port/vu0_a.h:
 * RefVu0_Add / Sub / Mul / Div / SqrtBits), so the PS2's number behaviour is decided in one place. With
 * REF_VU0_B_STANDALONE it is self-contained: add / sub / mul are plain IEEE single operations in the host's rounding
 * mode and only what changes ordinary results is modelled (division by zero, float-to-integer saturation, the
 * max / min ordering); adding REF_VU0_STRICT sends every operation through the VU0 number model as emulators
 * implement it: denormal inputs
 * are zero, an input with exponent 255 is +/-max, results are rounded toward zero, overflow gives +/-max, underflow
 * gives +/-0. (Known limit of the strict model: it does not reproduce the hardware multiplier's rare 1-ulp-low
 * results nor the adder's missing sticky bits; see the report.)
 *
 * Compile with -ffp-contract=off -fno-strict-aliasing.
 */

#include <float.h>
#include <math.h>
#include <string.h>

#include "port/vu0_b.h"
#ifdef REF_VU0_EXTERN_ARITH
/* The port compiles this file without the compiler's built-in functions, which made every float <-> bit pattern
   conversion below (a memcpy of four bytes) a call into the C library: several per float operation. */
#define memcpy(d, s, n) __builtin_memcpy(d, s, n)
#endif

/* ------------------------------------------------------------------------------------------------------------------
 * VU0 state and imports: shared with the first half (default) or local (REF_VU0_B_STANDALONE). See vu0_b.h.
 * ---------------------------------------------------------------------------------------------------------------- */

#ifdef REF_VU0_B_STANDALONE
/* Initialised to what Vu0_Init leaves, so the routines work before it is called. */
RefVu0BState gRefVu0B = {
    {
        {0.0f, 0.0f, 0.0f, 1.0f}, /* vf0 */
        {0.0f, 0.0f, 1.0f, 0.0f}, /* vf1 */
        {0.0f, 1.0f, 0.0f, 0.0f}, /* vf2 */
        {1.0f, 0.0f, 0.0f, 0.0f}, /* vf3 */
        /* vf4-15: scratch */
        {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f}, /* vf16-19: current matrix = identity */
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    },
    {{0.0f, 0.0f, 0.0f, 0.0f}},
    0,
};
#define VF(n) (gRefVu0B.vf[n])
#define VU_MEM(i) (gRefVu0B.mem[(i) & 0xFF])
#define VU_VI15 (gRefVu0B.vi15)
#define A_CUR_ROTATE_ZXY(angles) Ref_func_00120F00(angles)
#define A_MTX_ROTATE_ZXY(out, m, angles) Ref_func_001204B8(out, m, angles)
#else
#include "port/vu0_a.h"
#define VF(n) (*(RefVec4f *)&gRefVu0.vf[n])
#define VU_MEM(i) (*(RefVec4f *)&gRefVu0.mem[(i) & 0xFF])
#define VU_VI15 (gRefVu0.vi[15])
#define A_CUR_ROTATE_ZXY(angles) Ref_Vu0Cur_RotateZXY((RefVec4 *)(angles))
#define A_MTX_ROTATE_ZXY(out, m, angles) Ref_Mtx_RotateZXY((RefMtx44 *)(out), (RefMtx44 *)(m), (RefVec4 *)(angles))
#endif

#define VF0W (VF(0).w) /* 1.0f */
#define VF1Z (VF(1).z) /* 1.0f */
#define VF3X (VF(3).x) /* 1.0f */

/* ------------------------------------------------------------------------------------------------------------------
 * Number model
 * ---------------------------------------------------------------------------------------------------------------- */

static uint32_t f_bits(float f) {
    uint32_t u;

    memcpy(&u, &f, sizeof(u));
    return u;
}

static float f_from(uint32_t u) {
    float f;

    memcpy(&f, &u, sizeof(f));
    return f;
}

#ifndef REF_VU0_B_STANDALONE
/* Shared build: the first half's primitives decide the number model. */
#ifdef REF_VU0_EXTERN_ARITH /* the port: the arithmetic of port/src/softfloat_ps2.c, in place */
#include "softfloat_ps2_inl.h"
static float f_add(float a, float b) { return f_from(Sf_AddBits(f_bits(a), f_bits(b))); }
static float f_sub(float a, float b) { return f_from(Sf_AddBits(f_bits(a), f_bits(b) ^ 0x80000000u)); }
static float f_mul(float a, float b) { return f_from(Sf_MulBits(f_bits(a), f_bits(b))); }
#else
static float f_add(float a, float b) { return RefVu0_Add(a, b); }
static float f_sub(float a, float b) { return RefVu0_Sub(a, b); }
static float f_mul(float a, float b) { return RefVu0_Mul(a, b); }
#endif
static float f_div(float a, float b) { return RefVu0_Div(a, b); }
static float f_sqrt(float a) { return f_from(RefVu0_SqrtBits(f_bits(a))); }
static int f_lt(float a, float b) { return RefVu0_LtBits(f_bits(a), f_bits(b)); }
#else
static int f_lt(float a, float b) { return a < b; }
/* +/-max, the value the VU0 and the EE FPU produce where IEEE would produce infinity or NaN. */
#define REF_VU0_MAX_BITS 0x7F7FFFFFu

static float f_max_signed(uint32_t signSource) {
    return f_from((signSource & 0x80000000u) | REF_VU0_MAX_BITS);
}

#ifdef REF_VU0_STRICT
/* An operand as the VU0 sees it: exponent 0 is zero, exponent 255 is +/-max. */
static double f_in(float f) {
    uint32_t u = f_bits(f);

    if ((u & 0x7F800000u) == 0) {
        return (double)f_from(u & 0x80000000u);
    }
    if ((u & 0x7F800000u) == 0x7F800000u) {
        return (double)f_max_signed(u);
    }
    return (double)f;
}

/* A result: rounded toward zero to 24 bits, overflow clamped, underflow flushed. */
static float f_out(double d) {
    float f = (float)d;

    if (f != f) {
        return f_max_signed(0);
    }
    if (fabs((double)f) > fabs(d)) {
        f = nextafterf(f, 0.0f);
    }
    if (fabsf(f) > FLT_MAX) {
        return f_max_signed(f_bits(f));
    }
    if (fabsf(f) < FLT_MIN) {
        return f_from(f_bits(f) & 0x80000000u);
    }
    return f;
}

static float f_add(float a, float b) { return f_out(f_in(a) + f_in(b)); }
static float f_sub(float a, float b) { return f_out(f_in(a) - f_in(b)); }
static float f_mul(float a, float b) { return f_out(f_in(a) * f_in(b)); }
#else
static float f_add(float a, float b) { return a + b; }
static float f_sub(float a, float b) { return a - b; }
static float f_mul(float a, float b) { return a * b; }
#endif

/*
 * vdiv / the FPU's div.s: a / b. A zero divisor gives +/-max with the sign of sign(a) xor sign(b), for 0 / 0 too
 * (IEEE: infinity or NaN).
 */
static float f_div(float a, float b) {
#ifdef REF_VU0_STRICT
    double da = f_in(a);
    double db = f_in(b);

    if (db == 0.0) {
        return f_max_signed(f_bits(a) ^ f_bits(b));
    }
    return f_out(da / db);
#else
    if (b == 0.0f) {
        return f_max_signed(f_bits(a) ^ f_bits(b));
    }
    return a / b;
#endif
}

/* vsqrt: the square root of the absolute value (a negative operand only raises a flag). */
static float f_sqrt(float a) {
#ifdef REF_VU0_STRICT
    return f_out(sqrt(fabs(f_in(a))));
#else
    return sqrtf(fabsf(a));
#endif
}

#endif /* REF_VU0_B_STANDALONE */

/*
 * vftoi0 / vftoi4 / vftoi12: scale by 2^shift, truncate toward zero, saturate to 0x7FFFFFFF / 0x80000000 (a C cast
 * of an out-of-range float is undefined, the VU0 result is not).
 */
static int32_t f_toi(float f, int shift) {
#if defined(REF_VU0_B_STANDALONE) && defined(REF_VU0_STRICT)
    double d = f_in(f) * (double)(1 << shift);
#else
    double d = (double)f * (double)(1 << shift);
#endif

    if (d != d) {
        return (f_bits(f) & 0x80000000u) ? (int32_t)0x80000000u : 0x7FFFFFFF;
    }
    if (d >= 2147483648.0) {
        return 0x7FFFFFFF;
    }
    if (d <= -2147483648.0) {
        return (int32_t)0x80000000u;
    }
    return (int32_t)d;
}

/* vmax / vmini compare the bit patterns as sign-magnitude integers: -0 < +0, no NaN case. */
static int64_t f_key(float f) {
    uint32_t u = f_bits(f);

    return (u & 0x80000000u) ? -(int64_t)(u & 0x7FFFFFFFu) - 1 : (int64_t)u;
}

static float f_maxf(float a, float b) { return f_key(a) >= f_key(b) ? a : b; }
static float f_minf(float a, float b) { return f_key(a) <= f_key(b) ? a : b; }

/* ------------------------------------------------------------------------------------------------------------------
 * Shared sequences
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * vmulax.xyzw ACC, r0, v.x / vmadday.xyzw ACC, r1, v.y / vmaddaz.xyzw ACC, r2, v.z / vmaddw.xyzw out, r3, v.w
 * per component c: ((r0.c * v.x + r1.c * v.y) + r2.c * v.z) + r3.c * v.w
 */
static RefVec4f mtx_apply(const RefVec4f *r, RefVec4f v) {
    RefVec4f acc;
    RefVec4f o;

#if defined(REF_VU0_EXTERN_ARITH) && !defined(REF_VU0_B_STANDALONE)
    {   /* the port: the same sequence with the processor's arithmetic where that is exact (port/src/plat_fastvec.c) */
        extern int Port_FastMtxApply(const uint32_t *r, const uint32_t *v, uint32_t *out);
        if (!mode(0) && !mode(2) && Port_FastMtxApply((const uint32_t *)r, (const uint32_t *)&v, (uint32_t *)&o)) {
            return o;
        }
    }
#endif

    acc.x = f_mul(r[0].x, v.x);
    acc.y = f_mul(r[0].y, v.x);
    acc.z = f_mul(r[0].z, v.x);
    acc.w = f_mul(r[0].w, v.x);
    acc.x = f_add(acc.x, f_mul(r[1].x, v.y));
    acc.y = f_add(acc.y, f_mul(r[1].y, v.y));
    acc.z = f_add(acc.z, f_mul(r[1].z, v.y));
    acc.w = f_add(acc.w, f_mul(r[1].w, v.y));
    acc.x = f_add(acc.x, f_mul(r[2].x, v.z));
    acc.y = f_add(acc.y, f_mul(r[2].y, v.z));
    acc.z = f_add(acc.z, f_mul(r[2].z, v.z));
    acc.w = f_add(acc.w, f_mul(r[2].w, v.z));
    o.x = f_add(acc.x, f_mul(r[3].x, v.w));
    o.y = f_add(acc.y, f_mul(r[3].y, v.w));
    o.z = f_add(acc.z, f_mul(r[3].z, v.w));
    o.w = f_add(acc.w, f_mul(r[3].w, v.w));
    return o;
}

/* Loads a matrix argument into four registers (lqc2 x 4), so later stores cannot change it. */
static void mtx_load(RefVec4f *r, const RefMtx44f *m) {
    r[0] = m->row[0];
    r[1] = m->row[1];
    r[2] = m->row[2];
    r[3] = m->row[3];
}

/*
 * vmul.xyz t, a, b / vadday.x ACC, t, t.y / vmaddz.x t, vf3, t.z
 * (a.x * b.x + a.y * b.y) + vf3.x * (a.z * b.z), vf3.x = 1.0
 */
static float dot3(RefVec4f a, RefVec4f b) {
    float tx = f_mul(a.x, b.x);
    float ty = f_mul(a.y, b.y);
    float tz = f_mul(a.z, b.z);
    float acc = f_add(tx, ty);

    return f_add(acc, f_mul(VF3X, tz));
}

/*
 * The perspective step shared by every projection:
 *   vdiv Q, vf0.w, p.w / vwaitq / vmulq.xyz p, p, Q      Q = 1.0 / p.w; p.xyz = p.xyz * Q (three multiplies by the
 *   reciprocal, not three divisions); p.w is left undivided.
 */
static float persp(RefVec4f *p) {
    float q = f_div(VF0W, p->w);

    p->x = f_mul(p->x, q);
    p->y = f_mul(p->y, q);
    p->z = f_mul(p->z, q);
    return q;
}

/*
 * The on-screen test:
 *   vsub.xyzw vf11, vf0, vf0          vf11 = 0
 *   vf12.xy = 4096.0 (0x45800000, built in t0 and moved with qmtc2)
 *   ctc2 $zero, $vi16                 clears the sticky status flags
 *   vsub.xyw vf0, p, vf11             p.x - 0, p.y - 0, p.w - 0: results discarded, flags kept
 *   vsub.xy vf0, vf12, p              4096 - p.x, 4096 - p.y
 *   cfc2 v0, $vi16 / andi 0xC0 / sltiu 1
 * returns 1 when no result was zero (sticky Z) or had its sign bit set (sticky S): 0 < x < 4096, 0 < y < 4096, w > 0,
 * all strict. x and y are the divided values, w is the undivided one. The sign flag follows the sign BIT, so -0
 * fails like +0 does.
 */
static int on_screen(RefVec4f p) {
    float d;
    int bad = 0;

    d = f_sub(p.x, 0.0f);
    bad |= ((f_bits(d) & 0x7FFFFFFFu) == 0) | (int)(f_bits(d) >> 31);
    d = f_sub(p.y, 0.0f);
    bad |= ((f_bits(d) & 0x7FFFFFFFu) == 0) | (int)(f_bits(d) >> 31);
    d = f_sub(p.w, 0.0f);
    bad |= ((f_bits(d) & 0x7FFFFFFFu) == 0) | (int)(f_bits(d) >> 31);
    d = f_sub(4096.0f, p.x);
    bad |= ((f_bits(d) & 0x7FFFFFFFu) == 0) | (int)(f_bits(d) >> 31);
    d = f_sub(4096.0f, p.y);
    bad |= ((f_bits(d) & 0x7FFFFFFFu) == 0) | (int)(f_bits(d) >> 31);
    return bad == 0;
}

/* vftoi4.xy o, p / vftoi0.zw o, p: x, y in 12.4 fixed point, z and w as integers. */
static RefVec4i to_gs(RefVec4f p) {
    RefVec4i o;

    o.x = f_toi(p.x, 4);
    o.y = f_toi(p.y, 4);
    o.z = f_toi(p.z, 0);
    o.w = f_toi(p.w, 0);
    return o;
}

/* vmove.z uv, vf1 / vmulq.xyz o, uv, Q: (uv.x * Q, uv.y * Q, 1.0 * Q); o.w is the caller's business. */
static void stq_xyz(RefVec4f *o, RefVec4f uv, float q) {
    uv.z = VF1Z;
    o->x = f_mul(uv.x, q);
    o->y = f_mul(uv.y, q);
    o->z = f_mul(uv.z, q);
}

/* Mirrors of first-half routines that this half calls (exact, they are a few instructions each). */

/* 0x120098 Mtx_StoreIdentity: sqc2 vf3, vf2, vf1, vf0. */
static void a_store_identity(RefMtx44f *m) {
    m->row[0] = VF(3);
    m->row[1] = VF(2);
    m->row[2] = VF(1);
    m->row[3] = VF(0);
}

/* 0x120A98: vmove vf16, vf3 / vf17, vf2 / vf18, vf1 / vf19, vf0. */
static void a_cur_identity(void) {
    VF(16) = VF(3);
    VF(17) = VF(2);
    VF(18) = VF(1);
    VF(19) = VF(0);
}

/* 0x120AB0: vsqi vf16..vf19, (vi15++). VU0 data memory is 256 quadwords and the address wraps. */
static void a_cur_push(void) {
    VU_MEM(VU_VI15++) = VF(16);
    VU_MEM(VU_VI15++) = VF(17);
    VU_MEM(VU_VI15++) = VF(18);
    VU_MEM(VU_VI15++) = VF(19);
}

/* 0x120AC8: vlqd vf19..vf16, (--vi15). */
static void a_cur_pop(void) {
    VF(19) = VU_MEM(--VU_VI15);
    VF(18) = VU_MEM(--VU_VI15);
    VF(17) = VU_MEM(--VU_VI15);
    VF(16) = VU_MEM(--VU_VI15);
}

/* 0x120B80: lqc2 vf16..vf19 from a matrix. */
static void a_cur_load(const RefMtx44f *m) {
    mtx_load(&VF(16), m);
}

/* 0x120FC8: out = current matrix * v, four components. */
static void a_cur_mul_vec4(RefVec4f *out, const RefVec4f *v) {
    *out = mtx_apply(&VF(16), *v);
}

/* 0x120FE8: the last step is vmaddw.xyz into the register that holds v, so out.w = v.w. */
static void a_cur_mul_vec3(RefVec4f *out, const RefVec4f *v) {
    RefVec4f in = *v;
    RefVec4f p = mtx_apply(&VF(16), in);

    p.w = in.w;
    *out = p;
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x121008 .. 0x121240
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x121008 Vu0Cur_RotEulerMulVec4 (compiled C)
 *   jal 0x120A98 (cur = identity) / jal 0x120F00(angles) (cur rotated Z, X, Y, VU0 polynomial sine) /
 *   j 0x120FC8(out, v)
 * The current matrix is left changed. No callers.
 */
void Ref_Vu0Cur_RotEulerMulVec4(RefVec4f *out, const RefVec4f *angles, const RefVec4f *v) {
    a_cur_identity();
    A_CUR_ROTATE_ZXY(angles);
    a_cur_mul_vec4(out, v);
}

/* 0x121058 Vu0Cur_RotEulerMulVec3 (compiled C): the same with 0x120FE8, so out.w = v.w. No callers. */
void Ref_Vu0Cur_RotEulerMulVec3(RefVec4f *out, const RefVec4f *angles, const RefVec4f *v) {
    a_cur_identity();
    A_CUR_ROTATE_ZXY(angles);
    a_cur_mul_vec3(out, v);
}

/*
 * 0x1210A8 Vu0Cur_ProjectInt
 *   lqc2 vf4, 0(a1)
 *   vmulax/vmadday/vmaddaz ACC, vf16..18, vf4 / vmaddw.xyzw vf5, vf19, vf4w
 *   vdiv Q, vf0w, vf5w / vwaitq / vmulq.xyz vf5, vf5, Q
 *   vftoi0.xyzw vf6, vf5 / sqc2 vf6, 0(a0)
 * out = trunc(x / w, y / w, z / w, w) with the division done as a multiply by 1 / w.
 */
void Ref_Vu0Cur_ProjectInt(RefVec4i *out, const RefVec4f *v) {
    RefVec4f p = mtx_apply(&VF(16), *v);

    persp(&p);
    out->x = f_toi(p.x, 0);
    out->y = f_toi(p.y, 0);
    out->z = f_toi(p.z, 0);
    out->w = f_toi(p.w, 0);
}

/*
 * 0x1210D8 Vu0Cur_ProjectPoint
 *   as 0x1210A8 up to vmulq, then the on-screen test (see on_screen), then
 *   vftoi4.xy vf6, vf5 / vftoi0.zw vf6, vf5 / sqc2 vf6, 0(a0)
 * The point is stored whether it is on screen or not.
 */
int Ref_Vu0Cur_ProjectPoint(RefVec4i *out, const RefVec4f *v) {
    RefVec4f p = mtx_apply(&VF(16), *v);

    persp(&p);
    *out = to_gs(p);
    return on_screen(p);
}

/*
 * 0x121140 Vu0Cur_ProjectPoints (hand-written loop around 0x1210D8's body)
 *   lqc2 vf4, 0(a1)
 * loop: project; addi a1, 16; lqc2 vf4, 0(a1) (the NEXT point, loaded before this one is stored, and one past the end
 *   on the last pass); test; sqc2 vf6, 0(a0); if off screen: return 0; a2--; a0 += 16; loop while a2 != 0
 * Returns 1 when every point was on screen. Stops after storing the first point that is off screen: the rest of the
 * output is not written. n <= 0 is not handled (the count is tested after the decrement).
 */
int Ref_Vu0Cur_ProjectPoints(RefVec4i *out, const RefVec4f *v, int n) {
    RefVec4f in = *v;
    RefVec4f p;

    for (;;) {
        p = mtx_apply(&VF(16), in);
        persp(&p);
        v++;
        if (n != 1) {
            in = *v; /* the original also reads one element past the end */
        }
        *out = to_gs(p);
        if (!on_screen(p)) {
            return 0;
        }
        if (--n == 0) {
            return 1;
        }
        out++;
    }
}

/*
 * 0x1211C8 Vu0Cur_ProjectPointStq
 *   lqc2 vf4, 0(a2) / lqc2 vf8, 0(a3)
 *   project / vmove.z vf8, vf1 / vmulq.xyz vf8, vf8, Q
 *   test / sqc2 vf8, 0(a1) / sqc2 vf6, 0(a0)
 * stq = (uv.x * Q, uv.y * Q, 1 * Q, uv.w); stq is stored before out. No callers.
 */
int Ref_Vu0Cur_ProjectPointStq(RefVec4i *out, RefVec4f *stq, const RefVec4f *v, const RefVec4f *uv) {
    RefVec4f t = *uv;
    RefVec4f p = mtx_apply(&VF(16), *v);
    float q = persp(&p);

    stq_xyz(&t, t, q);
    *stq = t;
    *out = to_gs(p);
    return on_screen(p);
}

/*
 * 0x121240 Vu0Cur_ProjectPointsStq (hand-written loop; the count is the fifth argument, in t0)
 *   loop: project; vmove.z vf8, vf1; next point loaded; vmulq.xyz vf7, vf8, Q; next uv loaded;
 *         test; sqc2 vf7, 0(a1); sqc2 vf6, 0(a0); stop when off screen or the count runs out
 * ORIGINAL QUIRK: the stq goes through vf7, of which only x, y, z are written, so stq.w is whatever vf7.w held when
 * the routine was entered (the single-point version stores uv.w). Render data only.
 */
int Ref_Vu0Cur_ProjectPointsStq(RefVec4i *out, RefVec4f *stq, const RefVec4f *v, const RefVec4f *uv, int n) {
    RefVec4f in = *v;
    RefVec4f t = *uv;
    RefVec4f p;
    float q;

    for (;;) {
        p = mtx_apply(&VF(16), in);
        q = persp(&p);
        stq_xyz(&VF(7), t, q); /* vf7.w stale */
        v++;
        uv++;
        if (n != 1) {
            in = *v;
            t = *uv;
        }
        *stq = VF(7);
        *out = to_gs(p);
        if (!on_screen(p)) {
            return 0;
        }
        if (--n == 0) {
            return 1;
        }
        out++;
        stq++;
    }
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x1212D8 .. 0x121438: the three camera matrices in vf20-23, vf24-27, vf28-31
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * SetMul: lqc2 vf4..7 = a rows, vf8..11 = b rows; for each row i of b:
 *   vmulax ACC, vf4, b[i].x / vmadday ACC, vf5, b[i].y / vmaddaz ACC, vf6, b[i].z / vmaddw reg[i], vf7, b[i].w
 */
static void reg_set_mul(RefVec4f *reg, const RefMtx44f *a, const RefMtx44f *b) {
    RefVec4f ra[4];
    RefVec4f rb[4];

    mtx_load(ra, a);
    mtx_load(rb, b);
    reg[0] = mtx_apply(ra, rb[0]);
    reg[1] = mtx_apply(ra, rb[1]);
    reg[2] = mtx_apply(ra, rb[2]);
    reg[3] = mtx_apply(ra, rb[3]);
}

/* 0x1212D8 Vu0Clip_LoadMtx: lqc2 vf20..23, 0x00..0x30(a0). */
void Ref_Vu0Clip_LoadMtx(const RefMtx44f *m) {
    mtx_load(&VF(20), m);
}

/* 0x1212F0 Vu0Clip_StoreMtx: sqc2 vf20..23. */
void Ref_Vu0Clip_StoreMtx(RefMtx44f *m) {
    mtx_load(m->row, (const RefMtx44f *)&VF(20));
}

/* 0x121308 Vu0Clip_SetMulMtx: vf20..23 = a * b. */
void Ref_Vu0Clip_SetMulMtx(const RefMtx44f *a, const RefMtx44f *b) {
    reg_set_mul(&VF(20), a, b);
}

/* 0x121370 Vu0Screen_LoadMtx: lqc2 vf24..27. */
void Ref_Vu0Screen_LoadMtx(const RefMtx44f *m) {
    mtx_load(&VF(24), m);
}

/* 0x121388 Vu0Screen_StoreMtx: sqc2 vf24..27. */
void Ref_Vu0Screen_StoreMtx(RefMtx44f *m) {
    mtx_load(m->row, (const RefMtx44f *)&VF(24));
}

/* 0x1213A0 Vu0Screen_SetMulMtx: vf24..27 = a * b. */
void Ref_Vu0Screen_SetMulMtx(const RefMtx44f *a, const RefMtx44f *b) {
    reg_set_mul(&VF(24), a, b);
}

/* 0x121408 Vu0View_LoadMtx: lqc2 vf28..31. */
void Ref_Vu0View_LoadMtx(const RefMtx44f *m) {
    mtx_load(&VF(28), m);
}

/* 0x121420 Vu0View_StoreMtx: sqc2 vf28..31. */
void Ref_Vu0View_StoreMtx(RefMtx44f *m) {
    mtx_load(m->row, (const RefMtx44f *)&VF(28));
}

/* 0x121438 Vu0View_SetMulMtx: vf28..31 = a * b. No callers. */
void Ref_Vu0View_SetMulMtx(const RefMtx44f *a, const RefMtx44f *b) {
    reg_set_mul(&VF(28), a, b);
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x1214A0 .. 0x121948
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x1214A0 Vu0View_SetRotTrans (compiled C; matching source in src/sys/vu0_b_c.c)
 * Builds M = identity, then for X, Z, Y: r = rotation matrix from libm sinf / cosf, current matrix = M, and the two
 * rows of M that the rotation changes are replaced by current * (the same rows of r). Row 3 = (M * trans).xyz with
 * w = trans.w (Mtx_MulVec3). The result goes to vf28-31. The current matrix is pushed and popped around it.
 *   X: r[1] = (0, c, -s, 0), r[2] = (0, s, c, 0)
 *   Z: r[0] = (c, -s, 0, 0), r[1] = (s, c, 0, 0)
 *   Y: r[0] = (c, 0, s, 0),  r[2] = (-s, 0, c, 0)      (the opposite sign of Vu0Cur_RotateYXZ's Y)
 * sinf is called before cosf each time. The first Vu0View_StoreMtx into M is dead (overwritten by the identity).
 */
void Ref_Vu0View_SetRotTrans(const RefVec4f *rot, const RefVec4f *trans) {
    RefMtx44f m;
    RefMtx44f r;
    float s;
    float c;

    a_cur_push();
    Ref_Vu0View_StoreMtx(&m);
    a_store_identity(&m);
    a_store_identity(&r);
    s = Ref_sinf(rot->x);
    c = Ref_cosf(rot->x);
    r.row[1].y = c;
    r.row[1].z = -s;
    r.row[2].y = s;
    r.row[2].z = c;
    a_cur_load(&m);
    a_cur_mul_vec4(&m.row[1], &r.row[1]);
    a_cur_mul_vec4(&m.row[2], &r.row[2]);
    a_store_identity(&r);
    s = Ref_sinf(rot->z);
    c = Ref_cosf(rot->z);
    r.row[0].x = c;
    r.row[0].y = -s;
    r.row[1].x = s;
    r.row[1].y = c;
    a_cur_load(&m);
    a_cur_mul_vec4(&m.row[0], &r.row[0]);
    a_cur_mul_vec4(&m.row[1], &r.row[1]);
    a_store_identity(&r);
    s = Ref_sinf(rot->y);
    c = Ref_cosf(rot->y);
    r.row[0].x = c;
    r.row[0].z = s;
    r.row[2].x = -s;
    r.row[2].z = c;
    a_cur_load(&m);
    a_cur_mul_vec4(&m.row[0], &r.row[0]);
    a_cur_mul_vec4(&m.row[2], &r.row[2]);
    Ref_Mtx_MulVec3(&m.row[3], &m, trans);
    Ref_Vu0View_LoadMtx(&m);
    a_cur_pop();
}

/*
 * 0x121640 Vu0View_ApplyTransRot (compiled C; matching source in src/sys/vu0_b_c.c). No callers.
 * M = vf28-31; row 3 = (M * trans).xyz, w = trans.w; then the rotations Y, Z, X as in 0x1214A0; back to vf28-31.
 */
void Ref_Vu0View_ApplyTransRot(const RefVec4f *rot, const RefVec4f *trans) {
    RefMtx44f m;
    RefMtx44f r;
    float s;
    float c;

    a_cur_push();
    Ref_Vu0View_StoreMtx(&m);
    Ref_Mtx_MulVec3(&m.row[3], &m, trans);
    a_store_identity(&r);
    s = Ref_sinf(rot->y);
    c = Ref_cosf(rot->y);
    r.row[0].x = c;
    r.row[0].z = s;
    r.row[2].x = -s;
    r.row[2].z = c;
    a_cur_load(&m);
    a_cur_mul_vec4(&m.row[0], &r.row[0]);
    a_cur_mul_vec4(&m.row[2], &r.row[2]);
    a_store_identity(&r);
    s = Ref_sinf(rot->z);
    c = Ref_cosf(rot->z);
    r.row[0].x = c;
    r.row[0].y = -s;
    r.row[1].x = s;
    r.row[1].y = c;
    a_cur_load(&m);
    a_cur_mul_vec4(&m.row[0], &r.row[0]);
    a_cur_mul_vec4(&m.row[1], &r.row[1]);
    a_store_identity(&r);
    s = Ref_sinf(rot->x);
    c = Ref_cosf(rot->x);
    r.row[1].y = c;
    r.row[1].z = -s;
    r.row[2].y = s;
    r.row[2].z = c;
    a_cur_load(&m);
    a_cur_mul_vec4(&m.row[1], &r.row[1]);
    a_cur_mul_vec4(&m.row[2], &r.row[2]);
    Ref_Vu0View_LoadMtx(&m);
    a_cur_pop();
}

/*
 * 0x1217D0 Vu0Cur_RotateYXZ (compiled C with inline VU0; matching source in src/sys/vu0_b_c.c)
 * Three times: m = identity with two rows of a rotation (libm cosf first, then sinf); lqc2 vf8 / vf9 = those rows;
 *   vf7 (or vf4) = cur * vf8, vf4 (or vf7) = cur * vf9 (full four-term products, both from the OLD current matrix);
 *   vmove into the two rows of the current matrix.
 *   Y: rows 0, 2 = (c, 0, -s, 0), (s, 0, c, 0)
 *   X: rows 1, 2 = (0, c, -s, 0), (0, s, c, 0)
 *   Z: rows 0, 1 = (c, -s, 0, 0), (s, c, 0, 0)
 * The matrix m is rebuilt from the identity each time, so the row's other components are exact 0 / 1.
 */
void Ref_Vu0Cur_RotateYXZ(const RefVec4f *rot) {
    RefMtx44f m;
    RefVec4f a;
    RefVec4f b;
    float s;
    float c;

    a_store_identity(&m);
    c = Ref_cosf(rot->y);
    m.row[0].x = c;
    s = Ref_sinf(rot->y);
    m.row[2].x = s;
    m.row[2].z = c;
    m.row[0].z = -s;
    a = mtx_apply(&VF(16), m.row[0]);
    b = mtx_apply(&VF(16), m.row[2]);
    VF(16) = a;
    VF(18) = b;

    a_store_identity(&m);
    c = Ref_cosf(rot->x);
    m.row[1].y = c;
    s = Ref_sinf(rot->x);
    m.row[2].y = s;
    m.row[2].z = c;
    m.row[1].z = -s;
    a = mtx_apply(&VF(16), m.row[1]);
    b = mtx_apply(&VF(16), m.row[2]);
    VF(17) = a;
    VF(18) = b;

    a_store_identity(&m);
    c = Ref_cosf(rot->z);
    m.row[0].x = c;
    s = Ref_sinf(rot->z);
    m.row[1].x = s;
    m.row[1].y = c;
    m.row[0].y = -s;
    a = mtx_apply(&VF(16), m.row[0]);
    b = mtx_apply(&VF(16), m.row[1]);
    VF(16) = a;
    VF(17) = b;
}

/* 0x121910 .. 0x121948: eight empty functions (jr ra / nop). No callers. */
void Ref_Vu0_Stub0(void) {}
void Ref_Vu0_Stub1(void) {}
void Ref_Vu0_Stub2(void) {}
void Ref_Vu0_Stub3(void) {}
void Ref_Vu0_Stub4(void) {}
void Ref_Vu0_Stub5(void) {}
void Ref_Vu0_Stub6(void) {}
void Ref_Vu0_Stub7(void) {}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x121950 .. 0x121D48: the polygon clipper
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x121950 ClipVtx_Set
 *   lq t0, 0(a1) / lq t1, 0(a2) / lq t2, 0(a3) / sq t0, 0(a0) / sq t1, 16(a0) / sq t2, 32(a0)
 * Bit copies (no arithmetic); all three are read before anything is written.
 */
void Ref_ClipVtx_Set(RefClipVtx *vtx, const RefVec4f *pos, const RefVec4f *uv, const RefVec4f *col) {
    RefVec4f p = *pos;
    RefVec4f t = *uv;
    RefVec4f c = *col;

    vtx->pos = p;
    vtx->uv = t;
    vtx->col = c;
}

/* 0x121970 ClipVtx_Copy: lq x 3 from src, sq x 3 to dst. */
void Ref_ClipVtx_Copy(RefClipVtx *dst, const RefClipVtx *src) {
    RefClipVtx t = *src;

    *dst = t;
}

/* 0x121990 ClipVtx_SetArray (count in t0): n times 0x121950 with all four pointers advancing; n == 0 does nothing. */
void Ref_ClipVtx_SetArray(RefClipVtx *vtx, const RefVec4f *pos, const RefVec4f *uv, const RefVec4f *col, int n) {
    if (n == 0) {
        return;
    }
    do {
        Ref_ClipVtx_Set(vtx++, pos++, uv++, col++);
    } while (--n != 0);
}

/* 0x1219D8 ClipVtx_CopyArray: n times 0x121970, ascending; n == 0 does nothing. */
void Ref_ClipVtx_CopyArray(RefClipVtx *dst, const RefClipVtx *src, int n) {
    if (n == 0) {
        return;
    }
    do {
        Ref_ClipVtx_Copy(dst++, src++);
    } while (--n != 0);
}

/*
 * 0x121A10 ClipPoly_ClipPlane (compiled C; matching source in src/sys/vu0_b_c.c)
 * Sutherland-Hodgman against one plane, in place. Local buffers hold 9 vertices: the caller must keep n <= 9 and the
 * result can have n + 1 vertices, so 9 in is already one too many when the plane cuts (no check in the original).
 * "Outside" is dist < 0 with an FPU compare, so dist == 0 (and -0) is inside.
 */
int Ref_ClipPoly_ClipPlane(RefClipVtx *poly, const RefVec4f *plane, int n) {
    RefClipVtx buf[9];
    RefVec4f dist[9];
    int out[12];
    RefClipVtx *dst;
    RefClipVtx *cur;
    RefClipVtx *next;
    int i;
    int j;
    int nOut;
    float t;

    if (n == 0) {
        return 0;
    }
    Ref_ClipPlane_DistArray(dist, poly, plane, n);
    nOut = 0;
    for (i = 0; i < n; i++) {
        out[i] = f_lt(dist[i].x, 0.0f); /* FPU c.lt.s */
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
            Ref_ClipVtx_Copy(dst, cur);
            dst++;
            if (out[j] != 0) {
                t = Ref_ClipPlane_EdgeParam(plane, cur, next, dist[i].x);
                Ref_ClipVtx_Lerp(dst, next, cur, t);
                dst++;
            }
        } else if (out[j] == 0) {
            t = Ref_ClipPlane_EdgeParam(plane, cur, next, dist[i].x);
            Ref_ClipVtx_Lerp(dst, next, cur, t);
            dst++;
        }
    }
    nOut = (int)(dst - buf);
    Ref_ClipVtx_CopyArray(poly, buf, nOut);
    return nOut;
}

/*
 * 0x121C18 ClipVtx_Lerp
 *   mfc1 t0, f12 / qmtc2 t0, vf13            vf13.x = t
 *   vsubx.w vf14, vf0, vf13x                 vf14.w = 1.0 - t
 *   lqc2 vf4..vf9 = a.pos, b.pos, a.uv, b.uv, a.col, b.col
 *   vmulax.xyzw ACC, a.q, vf13x / vmaddw.xyzw o.q, b.q, vf14w      for the three quadwords
 * out = a * t + b * (1 - t), four components of each of pos, uv, col. All six inputs are loaded before the stores.
 */
static RefVec4f lerp4(RefVec4f a, RefVec4f b, float t, float u) {
    RefVec4f o;

    o.x = f_add(f_mul(a.x, t), f_mul(b.x, u));
    o.y = f_add(f_mul(a.y, t), f_mul(b.y, u));
    o.z = f_add(f_mul(a.z, t), f_mul(b.z, u));
    o.w = f_add(f_mul(a.w, t), f_mul(b.w, u));
    return o;
}

void Ref_ClipVtx_Lerp(RefClipVtx *out, const RefClipVtx *a, const RefClipVtx *b, float t) {
    float u = f_sub(VF0W, t);
    RefClipVtx va = *a;
    RefClipVtx vb = *b;

    out->pos = lerp4(va.pos, vb.pos, t, u);
    out->uv = lerp4(va.uv, vb.uv, t, u);
    out->col = lerp4(va.col, vb.col, t, u);
}

/*
 * 0x121C68 ClipPlane_EdgeParam
 *   lqc2 vf6, 0(a2) / lqc2 vf5, 0(a1) / lqc2 vf4, 0(a0)
 *   vsub.xyz vf5, vf5, vf6                   d = a.pos - b.pos
 *   vmul.xyz vf5, vf4, vf5 / vadday.x ACC, vf5, vf5y / vmaddz.x vf5, vf3, vf5z      dot(plane, d)
 *   qmfc2 t0, vf5 / mtc1 t0, f0 / div.s f0, f12, f0
 * dist / dot(plane.xyz, a - b), the division on the FPU (zero divisor: +/-max).
 */
float Ref_ClipPlane_EdgeParam(const RefVec4f *plane, const RefClipVtx *a, const RefClipVtx *b, float dist) {
    RefVec4f d;

    d.x = f_sub(a->pos.x, b->pos.x);
    d.y = f_sub(a->pos.y, b->pos.y);
    d.z = f_sub(a->pos.z, b->pos.z);
    d.w = a->pos.w;
    return f_div(dist, dot3(*plane, d));
}

/*
 * 0x121C98 ClipPlane_DistArray (hand-written loop)
 *   lqc2 vf4, 0(a2)                          plane
 *   loop: vmul.xyz vf5, vf4, vf5 / vadday.x ACC, vf5, vf5y / vmaddaz.x ACC, vf3, vf5z / vmaddw.x vf6, vf3, vf4w
 *         next position loaded (0x30 further), sqc2 vf6, 0(a0)
 * dist[i].x = ((n.x p.x + n.y p.y) + 1.0 * (n.z p.z)) + 1.0 * n.w. Only x of vf6 is written: dist[i].y, z, w receive
 * whatever vf6 held (the only caller reads x).
 */
void Ref_ClipPlane_DistArray(RefVec4f *dist, const RefClipVtx *poly, const RefVec4f *plane, int n) {
    RefVec4f pl;
    RefVec4f p;
    float tx;
    float ty;
    float tz;
    float acc;

    if (n == 0) {
        return;
    }
    pl = *plane;
    p = poly->pos;
    do {
        tx = f_mul(pl.x, p.x);
        ty = f_mul(pl.y, p.y);
        tz = f_mul(pl.z, p.z);
        acc = f_add(tx, ty);
        acc = f_add(acc, f_mul(VF3X, tz));
        VF(6).x = f_add(acc, f_mul(VF3X, pl.w));
        poly++;
        if (n != 1) {
            p = poly->pos;
        }
        *dist++ = VF(6); /* y, z, w stale */
    } while (--n != 0);
}

/*
 * 0x121CD8 ClipPoly_ProjectMtx (count in t0) / 0x121D48 ClipPoly_ProjectCur (hand-written loops)
 *   lqc2 vf8 = poly.pos, vf14 = poly.uv
 *   loop: p = M * pos; vdiv Q; vmove.z vf14, vf1; next pos loaded; vwaitq; vmulq.xyz vf9, vf9, Q;
 *         vmulq.xyz vf13, vf14, Q; next uv loaded; vftoi4.xy / vftoi0.zw vf10, vf9; sqc2 vf13, 0(a1); sqc2 vf10, 0(a0)
 * No on-screen test and no return value. ORIGINAL QUIRK: stq.w is whatever vf13.w held (never written).
 */
static void clip_project(RefVec4i *scr, RefVec4f *stq, const RefVec4f *rows, const RefClipVtx *poly, int n) {
    RefVec4f pos = poly->pos;
    RefVec4f uv = poly->uv;
    RefVec4f p;
    float q;

    do {
        p = mtx_apply(rows, pos);
        q = persp(&p);
        stq_xyz(&VF(13), uv, q); /* vf13.w stale */
        poly++;
        if (n != 1) {
            pos = poly->pos;
            uv = poly->uv;
        }
        *stq++ = VF(13);
        *scr++ = to_gs(p);
    } while (--n != 0);
}

void Ref_ClipPoly_ProjectMtx(RefVec4i *scr, RefVec4f *stq, const RefMtx44f *m, const RefClipVtx *poly, int n) {
    RefVec4f rows[4];

    mtx_load(rows, m);
    clip_project(scr, stq, rows, poly, n);
}

void Ref_ClipPoly_ProjectCur(RefVec4i *scr, RefVec4f *stq, const RefClipVtx *poly, int n) {
    clip_project(scr, stq, &VF(16), poly, n);
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x121DA8 .. 0x122030
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x121DA8 Vu0_Init (compiled C)
 *   Rand_SeedFloat(0.1234141f): the constant 0x3DFCC088 at 0x2FC32C; seeds the R register and libc srand
 *   0x120088: vmr32 vf1, vf0 / vmr32 vf2, vf1 / vmr32 vf3, vf2        (vmr32: x = y, y = z, z = w, w = x)
 *   0x120A80: vmove vf19, vf0 / vmr32 vf18, vf19 / vmr32 vf17, vf18 / vmr32 vf16, vf17 / vi15 = 0
 *   0x120B18: vi15 = 0
 *   Rand_Init()
 */
void Ref_Vu0_Init(void) {
    Ref_Rand_SeedFloat(0.1234140992f);
    VF(0).x = 0.0f;
    VF(0).y = 0.0f;
    VF(0).z = 0.0f;
    VF(0).w = 1.0f;
    VF(1).x = VF(0).y;
    VF(1).y = VF(0).z;
    VF(1).z = VF(0).w;
    VF(1).w = VF(0).x;
    VF(2).x = VF(1).y;
    VF(2).y = VF(1).z;
    VF(2).z = VF(1).w;
    VF(2).w = VF(1).x;
    VF(3).x = VF(2).y;
    VF(3).y = VF(2).z;
    VF(3).z = VF(2).w;
    VF(3).w = VF(2).x;
    VF(19) = VF(0);
    VF(18) = VF(1);
    VF(17) = VF(2);
    VF(16) = VF(3);
    VU_VI15 = 0;
    Ref_Rand_Init();
}

/*
 * 0x121DE0 Vu0_CheckState (compiled C): Mtx_StoreIdentity(local); memcmp(local, identity constant at 0x2EC260, 0x40);
 * 0x120B48() (stack depth != 0). Both results are discarded: no effect.
 */
void Ref_Vu0_CheckState(void) {
    RefMtx44f m;

    a_store_identity(&m);
    (void)m;
}

/*
 * 0x121E18 Vec4_SetZeroW1: sqc2 vf0, 0(a0)      v = (0, 0, 0, 1)
 * Callers' comments call this "zero"; w is 1.
 */
void Ref_Vec4_SetZeroW1(RefVec4f *v) {
    *v = VF(0);
}

/* 0x121E20 Vec4_SetZero: sq $zero, 0(a0)       v = (+0, +0, +0, +0) */
void Ref_Vec4_SetZero(RefVec4f *v) {
    v->x = 0.0f;
    v->y = 0.0f;
    v->z = 0.0f;
    v->w = 0.0f;
}

/* 0x121E28 Vec4_Set (compiled C): swc1 f15, 12 / f12, 0 / f13, 4 / f14, 8. */
void Ref_Vec4_Set(RefVec4f *v, float x, float y, float z, float w) {
    v->x = x;
    v->y = y;
    v->z = z;
    v->w = w;
}

/* 0x121E40 Vec3_Set (compiled C): x, y, z stored; w not touched. */
void Ref_Vec3_Set(RefVec4f *v, float x, float y, float z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

/*
 * 0x121E50 Vec3_Normalize (the disassembler shows raw words; decoded)
 *   lqc2 vf4, 0(a1)
 *   vmul.xyz vf5, vf4, vf4 / vadday.x ACC, vf5, vf5y / vmaddz.x vf5, vf3, vf5z      (x x + y y) + 1.0 * (z z)
 *   vsqrt Q, vf5x / vwaitq / vaddq.x vf5, vf0, Q                                  len = 0 + sqrt
 *   vdiv Q, vf0w, vf5x / vsub.xyzw vf6, vf0, vf0 / vwaitq                          Q = 1.0 / len; vf6 = 0
 *   vmulq.xyz vf6, vf4, Q / sqc2 vf6, 0(a0)
 * out = (x * (1 / len), y * (1 / len), z * (1 / len), 0): a reciprocal and three multiplies, NOT three divisions,
 * and w is set to 0 (1.0 - 1.0). A zero vector gives len = 0, Q = +max, and 0 * max = 0: the result is (0, 0, 0, 0),
 * where IEEE arithmetic would give NaN.
 */
void Ref_Vec3_Normalize(RefVec4f *out, const RefVec4f *v) {
    RefVec4f in = *v;
    float len = f_add(VF(0).x, f_sqrt(dot3(in, in)));
    float q = f_div(VF0W, len);
    RefVec4f o;

    o.w = f_sub(VF0W, VF0W);
    o.x = f_mul(in.x, q);
    o.y = f_mul(in.y, q);
    o.z = f_mul(in.z, q);
    *out = o;
}

/* 0x121E90 Vec4_Swap: lq t0, 0(a0) / lq t1, 0(a1) / sq t0, 0(a1) / sq t1, 0(a0). No callers. */
void Ref_Vec4_Swap(RefVec4f *a, RefVec4f *b) {
    RefVec4f ta = *a;
    RefVec4f tb = *b;

    *b = ta;
    *a = tb;
}

/* 0x121EA8 Vec4_Add: lqc2 vf4, a / lqc2 vf5, b / vadd.xyzw vf4, vf4, vf5 / sqc2 vf4. Four components. */
void Ref_Vec4_Add(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_add(va.x, vb.x);
    va.y = f_add(va.y, vb.y);
    va.z = f_add(va.z, vb.z);
    va.w = f_add(va.w, vb.w);
    *out = va;
}

/* 0x121EC0 Vec3_Add: vadd.xyz vf4, vf4, vf5. out.w = a.w. */
void Ref_Vec3_Add(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_add(va.x, vb.x);
    va.y = f_add(va.y, vb.y);
    va.z = f_add(va.z, vb.z);
    *out = va;
}

/* 0x121ED8 Vec4_Sub: vsub.xyzw vf4, vf4, vf5. Four components (two points with w = 1 give w = 0). */
void Ref_Vec4_Sub(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_sub(va.x, vb.x);
    va.y = f_sub(va.y, vb.y);
    va.z = f_sub(va.z, vb.z);
    va.w = f_sub(va.w, vb.w);
    *out = va;
}

/* 0x121EF0 Vec3_Sub: vsub.xyz vf4, vf4, vf5. out.w = a.w. */
void Ref_Vec3_Sub(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_sub(va.x, vb.x);
    va.y = f_sub(va.y, vb.y);
    va.z = f_sub(va.z, vb.z);
    *out = va;
}

/* 0x121F08 Vec4_Mul: vmul.xyzw vf4, vf4, vf5. Per-component product, four components. */
void Ref_Vec4_Mul(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_mul(va.x, vb.x);
    va.y = f_mul(va.y, vb.y);
    va.z = f_mul(va.z, vb.z);
    va.w = f_mul(va.w, vb.w);
    *out = va;
}

/* 0x121F20 Vec3_Mul: vmul.xyz vf4, vf4, vf5. out.w = a.w. No callers. */
void Ref_Vec3_Mul(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_mul(va.x, vb.x);
    va.y = f_mul(va.y, vb.y);
    va.z = f_mul(va.z, vb.z);
    *out = va;
}

/* 0x121F38 Vec4_Scale: lqc2 vf4, v / vf5.x = s / vmulx.xyzw vf4, vf4, vf5x. Four components: w is scaled too. */
void Ref_Vec4_Scale(RefVec4f *out, const RefVec4f *v, float s) {
    RefVec4f va = *v;

    va.x = f_mul(va.x, s);
    va.y = f_mul(va.y, s);
    va.z = f_mul(va.z, s);
    va.w = f_mul(va.w, s);
    *out = va;
}

/* 0x121F50 Vec3_Scale: vmulx.xyz vf4, vf4, vf5x. out.w = v.w. */
void Ref_Vec3_Scale(RefVec4f *out, const RefVec4f *v, float s) {
    RefVec4f va = *v;

    va.x = f_mul(va.x, s);
    va.y = f_mul(va.y, s);
    va.z = f_mul(va.z, s);
    *out = va;
}

/*
 * 0x121F68 Vec4_Div
 *   lqc2 vf4, v / vf5.x = d / vdiv Q, vf0w, vf5x / vwaitq / vmulq.xyzw vf4, vf4, Q
 * out = v * (1.0 / d), four components: a reciprocal then multiplies (different bits from v / d). d == 0: Q = +/-max.
 */
void Ref_Vec4_Div(RefVec4f *out, const RefVec4f *v, float d) {
    RefVec4f va = *v;
    float q = f_div(VF0W, d);

    va.x = f_mul(va.x, q);
    va.y = f_mul(va.y, q);
    va.z = f_mul(va.z, q);
    va.w = f_mul(va.w, q);
    *out = va;
}

/* 0x121F88 Vec3_Div: vmulq.xyz. out.w = v.w. */
void Ref_Vec3_Div(RefVec4f *out, const RefVec4f *v, float d) {
    RefVec4f va = *v;
    float q = f_div(VF0W, d);

    va.x = f_mul(va.x, q);
    va.y = f_mul(va.y, q);
    va.z = f_mul(va.z, q);
    *out = va;
}

/* 0x121FA8 Vec4_Copy: lq t0, 0(a1) / sq t0, 0(a0). A bit copy through an integer register. */
void Ref_Vec4_Copy(RefVec4f *dst, const RefVec4f *src) {
    RefVec4f t = *src;

    *dst = t;
}

/*
 * 0x121FB8 Vec3_Copy
 *   lqc2 vf4, 0(a1) / lqc2 vf5, 0(a0) / vmove.xyz vf5, vf4 / sqc2 vf5, 0(a0)
 * x, y, z copied; dst.w is read and written back unchanged.
 */
void Ref_Vec3_Copy(RefVec4f *dst, const RefVec4f *src) {
    RefVec4f s = *src;

    dst->x = s.x;
    dst->y = s.y;
    dst->z = s.z;
}

/*
 * 0x121FD0 Mtx_MulVec4
 *   lqc2 vf8, v / lqc2 vf4..7 = m rows
 *   vmulax.xyzw ACC, vf4, vf8x / vmadday ACC, vf5, vf8y / vmaddaz ACC, vf6, vf8z / vmaddw.xyzw vf8, vf7, vf8w
 * out = ((row0 * v.x + row1 * v.y) + row2 * v.z) + row3 * v.w, four components. out may be v or a row of m.
 */
void Ref_Mtx_MulVec4(RefVec4f *out, const RefMtx44f *m, const RefVec4f *v) {
    RefVec4f in = *v;
    RefVec4f rows[4];

    mtx_load(rows, m);
    *out = mtx_apply(rows, in);
}

/*
 * 0x122000 Mtx_MulVec3
 *   as 0x121FD0 into vf9, then vmove.xyz vf8, vf9 / sqc2 vf8
 * The full four-term product (the translation row times v.w included); out.xyz = product, out.w = v.w.
 */
void Ref_Mtx_MulVec3(RefVec4f *out, const RefMtx44f *m, const RefVec4f *v) {
    RefVec4f in = *v;
    RefVec4f rows[4];
    RefVec4f p;

    mtx_load(rows, m);
    p = mtx_apply(rows, in);
    p.w = in.w;
    *out = p;
}

/*
 * 0x122030 Vec4_RotateEuler (compiled C)
 *   0x1204B8(local, identity constant at 0x2EC260, angles): Z by angles.z, X by angles.x, Y by angles.y with the VU0
 *   polynomial sine / cosine; then Mtx_MulVec4(out, local, v).
 */
void Ref_Vec4_RotateEuler(RefVec4f *out, const RefVec4f *angles, const RefVec4f *v) {
    static const RefMtx44f identity = {{
        {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}};
    RefMtx44f m;

    A_MTX_ROTATE_ZXY(&m, &identity, angles);
    Ref_Mtx_MulVec4(out, &m, v);
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x122088 .. 0x1222D8
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x122088 Vec3_Dot
 *   lqc2 vf4, a / lqc2 vf5, b / vmul.xyz vf5, vf4, vf5 / vadday.x ACC, vf5, vf5y / vmaddz.x vf5, vf3, vf5z
 *   qmfc2 t0, vf5 / mtc1 t0, f0
 * (a.x b.x + a.y b.y) + vf3.x * (a.z b.z)
 */
float Ref_Vec3_Dot(const RefVec4f *a, const RefVec4f *b) {
    return dot3(*a, *b);
}

/*
 * 0x1220B0 Vec3_Cross
 *   lqc2 vf4, a / lqc2 vf5, b
 *   vopmula.xyz ACC, vf4, vf5       ACC = (a.y b.z, a.z b.x, a.x b.y)
 *   vopmsub.xyz vf5, vf5, vf4       vf5 = ACC - (b.y a.z, b.z a.x, b.x a.y)
 *   vsub.w vf5, vf5, vf5            w = b.w - b.w = 0
 */
void Ref_Vec3_Cross(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f va = *a;
    RefVec4f vb = *b;
    RefVec4f o;

    o.x = f_sub(f_mul(va.y, vb.z), f_mul(vb.y, va.z));
    o.y = f_sub(f_mul(va.z, vb.x), f_mul(vb.z, va.x));
    o.z = f_sub(f_mul(va.x, vb.y), f_mul(vb.x, va.y));
    o.w = f_sub(vb.w, vb.w);
    *out = o;
}

/* 0x1220D0 Vec4_ToFixed12: vftoi12.xyzw. trunc(c * 4096), saturating. No callers. */
void Ref_Vec4_ToFixed12(RefVec4i *out, const RefVec4f *v) {
    RefVec4f in = *v;

    out->x = f_toi(in.x, 12);
    out->y = f_toi(in.y, 12);
    out->z = f_toi(in.z, 12);
    out->w = f_toi(in.w, 12);
}

/* 0x1220E0 Vec4_ToFixed4: vftoi4.xyzw. trunc(c * 16). */
void Ref_Vec4_ToFixed4(RefVec4i *out, const RefVec4f *v) {
    RefVec4f in = *v;

    out->x = f_toi(in.x, 4);
    out->y = f_toi(in.y, 4);
    out->z = f_toi(in.z, 4);
    out->w = f_toi(in.w, 4);
}

/* 0x1220F0 Vec4_ToInt: vftoi0.xyzw. trunc(c). */
void Ref_Vec4_ToInt(RefVec4i *out, const RefVec4f *v) {
    RefVec4f in = *v;

    out->x = f_toi(in.x, 0);
    out->y = f_toi(in.y, 0);
    out->z = f_toi(in.z, 0);
    out->w = f_toi(in.w, 0);
}

/* 0x122100 Vec4_ToFixed4XY: vftoi4.xy / vftoi0.zw. No callers. */
void Ref_Vec4_ToFixed4XY(RefVec4i *out, const RefVec4f *v) {
    *out = to_gs(*v);
}

/*
 * 0x122118 Vec4_Clamp
 *   vf5.x = lo / vf6.x = hi / lqc2 vf4, v / vmaxx.xyzw vf4, vf4, vf5x / vminix.xyzw vf4, vf4, vf6x
 * min(max(c, lo), hi): with lo > hi the result is hi.
 */
void Ref_Vec4_Clamp(RefVec4f *out, const RefVec4f *v, float lo, float hi) {
    RefVec4f va = *v;

    va.x = f_minf(f_maxf(va.x, lo), hi);
    va.y = f_minf(f_maxf(va.y, lo), hi);
    va.z = f_minf(f_maxf(va.z, lo), hi);
    va.w = f_minf(f_maxf(va.w, lo), hi);
    *out = va;
}

/* 0x122140 Vec3_Clamp: the same with .xyz. out.w = v.w. */
void Ref_Vec3_Clamp(RefVec4f *out, const RefVec4f *v, float lo, float hi) {
    RefVec4f va = *v;

    va.x = f_minf(f_maxf(va.x, lo), hi);
    va.y = f_minf(f_maxf(va.y, lo), hi);
    va.z = f_minf(f_maxf(va.z, lo), hi);
    *out = va;
}

/*
 * 0x122168 Vec4_Lerp
 *   vf6.x = t / lqc2 vf4, a / lqc2 vf5, b / vsubx.w vf7, vf0, vf6x      vf7.w = 1.0 - t
 *   vmulax.xyzw ACC, vf4, vf6x / vmaddw.xyzw vf6, vf5, vf7w
 * out = a * t + b * (1 - t): t = 1 gives a, t = 0 gives b. Four components.
 */
void Ref_Vec4_Lerp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float t) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    *out = lerp4(va, vb, t, f_sub(VF0W, t));
}

/* 0x122190 Vec3_Lerp: .xyz, then vmove.w vf6, vf4: out.w = a.w. */
void Ref_Vec3_Lerp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float t) {
    RefVec4f va = *a;
    RefVec4f vb = *b;
    RefVec4f o = lerp4(va, vb, t, f_sub(VF0W, t));

    o.w = va.w;
    *out = o;
}

/*
 * 0x1221B8 Vec3_Length (raw words in the disassembly; decoded)
 *   lqc2 vf4, v / vmul.xyz vf4, vf4, vf4 / vadday.x ACC, vf4, vf4y / vmaddz.x vf4, vf3, vf4z
 *   vsqrt Q, vf4x / vwaitq / cfc2 t0, $vi22 (Q) / mtc1 t0, f0
 * The VU0 square root of (x x + y y) + 1.0 * (z z); not the FPU's sqrt.s and not libm.
 */
float Ref_Vec3_Length(const RefVec4f *v) {
    RefVec4f in = *v;

    return f_sqrt(dot3(in, in));
}

/* 0x1221E0 Vec3_LengthSq: the same without the root. */
float Ref_Vec3_LengthSq(const RefVec4f *v) {
    RefVec4f in = *v;

    return dot3(in, in);
}

/*
 * 0x122200 Vec3_Dist (raw words in the disassembly; decoded)
 *   lqc2 vf4, a / lqc2 vf5, b / vsub.xyzw vf4, vf4, vf5 / then as Vec3_Length
 */
float Ref_Vec3_Dist(const RefVec4f *a, const RefVec4f *b) {
    RefVec4f d;

    d.x = f_sub(a->x, b->x);
    d.y = f_sub(a->y, b->y);
    d.z = f_sub(a->z, b->z);
    d.w = f_sub(a->w, b->w);
    return f_sqrt(dot3(d, d));
}

/* 0x122230 Vec3_DistSq: the same without the root. */
float Ref_Vec3_DistSq(const RefVec4f *a, const RefVec4f *b) {
    RefVec4f d;

    d.x = f_sub(a->x, b->x);
    d.y = f_sub(a->y, b->y);
    d.z = f_sub(a->z, b->z);
    d.w = f_sub(a->w, b->w);
    return dot3(d, d);
}

/*
 * 0x122258 Vec3_DirToEuler (compiled C; matching source in src/sys/vu0_b_c.c)
 * out.y is stored first, then dir is read again: with out == dir the pitch uses the new y.
 * out.w is not written.
 */
void Ref_Vec3_DirToEuler(RefVec4f *out, const RefVec4f *dir) {
    RefVec4f flat;

    out->y = Ref_atan2f(dir->x, dir->z);
    flat.w = 0.0f; /* not initialised in the original; Vec3_Length does not read it */
    Ref_Vec3_Set(&flat, dir->x, 0.0f, dir->z);
    out->x = -Ref_atan2f(dir->y, Ref_Vec3_Length(&flat));
    out->z = 0.0f;
}

/* 0x1222D8 Vec3_DiffToEuler (compiled C): Vec3_Sub(local, a, b); Vec3_DirToEuler(out, local). No callers. */
void Ref_Vec3_DiffToEuler(RefVec4f *out, const RefVec4f *a, const RefVec4f *b) {
    RefVec4f d;

    Ref_Vec3_Sub(&d, a, b);
    Ref_Vec3_DirToEuler(out, &d);
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x122310 .. 0x1224E0: the projections with the matrix as an argument
 * ---------------------------------------------------------------------------------------------------------------- */

/* 0x122310 Mtx_ProjectInt: 0x1210A8 with lqc2 vf4..7 = m rows instead of vf16..19. */
void Ref_Mtx_ProjectInt(RefVec4i *out, const RefMtx44f *m, const RefVec4f *v) {
    RefVec4f in = *v;
    RefVec4f rows[4];
    RefVec4f p;

    mtx_load(rows, m);
    p = mtx_apply(rows, in);
    persp(&p);
    out->x = f_toi(p.x, 0);
    out->y = f_toi(p.y, 0);
    out->z = f_toi(p.z, 0);
    out->w = f_toi(p.w, 0);
}

/* 0x122350 Mtx_ProjectPoint: 0x1210D8 with the matrix argument. */
int Ref_Mtx_ProjectPoint(RefVec4i *out, const RefMtx44f *m, const RefVec4f *v) {
    RefVec4f in = *v;
    RefVec4f rows[4];
    RefVec4f p;

    mtx_load(rows, m);
    p = mtx_apply(rows, in);
    persp(&p);
    *out = to_gs(p);
    return on_screen(p);
}

/* 0x1223C8 Mtx_ProjectPoints (hand-written loop): 0x121140 with the matrix argument (loaded once). */
int Ref_Mtx_ProjectPoints(RefVec4i *out, const RefMtx44f *m, const RefVec4f *v, int n) {
    RefVec4f in = *v;
    RefVec4f rows[4];
    RefVec4f p;

    mtx_load(rows, m);
    for (;;) {
        p = mtx_apply(rows, in);
        persp(&p);
        v++;
        if (n != 1) {
            in = *v;
        }
        *out = to_gs(p);
        if (!on_screen(p)) {
            return 0;
        }
        if (--n == 0) {
            return 1;
        }
        out++;
    }
}

/*
 * 0x122458 Mtx_ProjectPointStq (uv pointer is the fifth argument, in t0)
 *   lqc2 vf8, v / lqc2 vf13, uv / project / vmove.z vf13, vf1 / vmulq.xyz vf13, vf13, Q
 *   test / sqc2 vf13, 0(a1) / sqc2 vf10, 0(a0)
 * stq = (uv.x Q, uv.y Q, Q, uv.w). Only ONE vnop separates the last vmulq from the flag clear here (two elsewhere);
 * if the flags of that vmulq land after the clear on hardware, a zero or negative uv.x * Q / uv.y * Q would also make
 * this return 0. Modelled like the others. 1 caller (environment-map vertices).
 */
int Ref_Mtx_ProjectPointStq(RefVec4i *out, RefVec4f *stq, const RefMtx44f *m, const RefVec4f *v, const RefVec4f *uv) {
    RefVec4f in = *v;
    RefVec4f t = *uv;
    RefVec4f rows[4];
    RefVec4f p;
    float q;

    mtx_load(rows, m);
    p = mtx_apply(rows, in);
    q = persp(&p);
    stq_xyz(&t, t, q);
    *stq = t;
    *out = to_gs(p);
    return on_screen(p);
}

/*
 * 0x1224E0 Mtx_ProjectPointsStq (hand-written loop; uv in t0, count in t1)
 *   as 0x121240 with the matrix argument; the stq goes through vf14 (vmulq.xyz vf14, vf13, Q).
 * ORIGINAL QUIRK: stq.w is whatever vf14.w held.
 */
int Ref_Mtx_ProjectPointsStq(RefVec4i *out, RefVec4f *stq, const RefMtx44f *m, const RefVec4f *v, const RefVec4f *uv, int n) {
    RefVec4f in = *v;
    RefVec4f t = *uv;
    RefVec4f rows[4];
    RefVec4f p;
    float q;

    mtx_load(rows, m);
    for (;;) {
        p = mtx_apply(rows, in);
        q = persp(&p);
        stq_xyz(&VF(14), t, q); /* vf14.w stale */
        v++;
        uv++;
        if (n != 1) {
            in = *v;
            t = *uv;
        }
        *stq = VF(14);
        *out = to_gs(p);
        if (!on_screen(p)) {
            return 0;
        }
        if (--n == 0) {
            return 1;
        }
        out++;
        stq++;
    }
}

/* 0x122588 Vu0_Stub8: empty. No callers. */
void Ref_Vu0_Stub8(void) {}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x122590 .. 0x122668
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x122590 Vec3_AddSub
 *   lqc2 vf4, a / vf5, b / vf6, c / vadda.xyz ACC, vf4, vf5 / vmsubw.xyz vf4, vf6, vf0w
 * (a + b) - c * 1.0 on x, y, z; out.w = a.w. No callers.
 */
void Ref_Vec3_AddSub(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, const RefVec4f *c) {
    RefVec4f va = *a;
    RefVec4f vb = *b;
    RefVec4f vc = *c;

    va.x = f_sub(f_add(va.x, vb.x), f_mul(vc.x, VF0W));
    va.y = f_sub(f_add(va.y, vb.y), f_mul(vc.y, VF0W));
    va.z = f_sub(f_add(va.z, vb.z), f_mul(vc.z, VF0W));
    *out = va;
}

/*
 * 0x1225B0 Vec3_SubAdd
 *   vsuba.xyz ACC, vf4, vf5 / vmaddw.xyz vf4, vf6, vf0w
 * (a - b) + c * 1.0; out.w = a.w. No callers.
 */
void Ref_Vec3_SubAdd(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, const RefVec4f *c) {
    RefVec4f va = *a;
    RefVec4f vb = *b;
    RefVec4f vc = *c;

    va.x = f_add(f_sub(va.x, vb.x), f_mul(vc.x, VF0W));
    va.y = f_add(f_sub(va.y, vb.y), f_mul(vc.y, VF0W));
    va.z = f_add(f_sub(va.z, vb.z), f_mul(vc.z, VF0W));
    *out = va;
}

/*
 * 0x1225D0 Vec3_ScaleAdd
 *   lqc2 vf4, dir / vf6.x = s / lqc2 vf5, base / vmulax.xyz ACC, vf4, vf6x / vmaddw.xyz vf5, vf5, vf0w
 * dir * s + base * 1.0 on x, y, z; out.w = base.w (the SECOND vector's w).
 */
void Ref_Vec3_ScaleAdd(RefVec4f *out, const RefVec4f *dir, const RefVec4f *base, float s) {
    RefVec4f vd = *dir;
    RefVec4f vb = *base;

    vb.x = f_add(f_mul(vd.x, s), f_mul(vb.x, VF0W));
    vb.y = f_add(f_mul(vd.y, s), f_mul(vb.y, VF0W));
    vb.z = f_add(f_mul(vd.z, s), f_mul(vb.z, VF0W));
    *out = vb;
}

/* 0x1225F0 Vec3_ScaleSub: vmsubw.xyz vf5, vf5, vf0w: dir * s - base * 1.0; out.w = base.w. No callers. */
void Ref_Vec3_ScaleSub(RefVec4f *out, const RefVec4f *dir, const RefVec4f *base, float s) {
    RefVec4f vd = *dir;
    RefVec4f vb = *base;

    vb.x = f_sub(f_mul(vd.x, s), f_mul(vb.x, VF0W));
    vb.y = f_sub(f_mul(vd.y, s), f_mul(vb.y, VF0W));
    vb.z = f_sub(f_mul(vd.z, s), f_mul(vb.z, VF0W));
    *out = vb;
}

/*
 * 0x122610 Vec3_Add4 (d is the fifth argument, in t0)
 *   vadda.xyz ACC, vf4, vf5 / vmaddaw.xyz ACC, vf6, vf0w / vmaddw.xyz vf4, vf7, vf0w
 * ((a + b) + c * 1.0) + d * 1.0 on x, y, z; out.w = a.w.
 */
void Ref_Vec3_Add4(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, const RefVec4f *c, const RefVec4f *d) {
    RefVec4f va = *a;
    RefVec4f vb = *b;
    RefVec4f vc = *c;
    RefVec4f vd = *d;

    va.x = f_add(f_add(f_add(va.x, vb.x), f_mul(vc.x, VF0W)), f_mul(vd.x, VF0W));
    va.y = f_add(f_add(f_add(va.y, vb.y), f_mul(vc.y, VF0W)), f_mul(vd.y, VF0W));
    va.z = f_add(f_add(f_add(va.z, vb.z), f_mul(vc.z, VF0W)), f_mul(vd.z, VF0W));
    *out = va;
}

/*
 * 0x122638 Vec4_AddClamp
 *   vadd.xyzw vf4, vf4, vf5 / vmaxx.xyzw vf4, vf4, vf6x (lo) / vminix.xyzw vf4, vf4, vf7x (hi)
 * No callers.
 */
void Ref_Vec4_AddClamp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float lo, float hi) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_minf(f_maxf(f_add(va.x, vb.x), lo), hi);
    va.y = f_minf(f_maxf(f_add(va.y, vb.y), lo), hi);
    va.z = f_minf(f_maxf(f_add(va.z, vb.z), lo), hi);
    va.w = f_minf(f_maxf(f_add(va.w, vb.w), lo), hi);
    *out = va;
}

/* 0x122668 Vec3_AddClamp: the same with .xyz; out.w = a.w (not clamped). No callers. */
void Ref_Vec3_AddClamp(RefVec4f *out, const RefVec4f *a, const RefVec4f *b, float lo, float hi) {
    RefVec4f va = *a;
    RefVec4f vb = *b;

    va.x = f_minf(f_maxf(f_add(va.x, vb.x), lo), hi);
    va.y = f_minf(f_maxf(f_add(va.y, vb.y), lo), hi);
    va.z = f_minf(f_maxf(f_add(va.z, vb.z), lo), hi);
    *out = va;
}

/* ------------------------------------------------------------------------------------------------------------------
 * 0x122698 .. 0x122868: rotations of a vector (compiled C; matching source in src/sys/vu0_b_c.c)
 * ---------------------------------------------------------------------------------------------------------------- */

/*
 * 0x122698 Vec3_RotateAxis
 *   cs = Mathf_Cos(angle); sn = Mathf_Sin(angle)             (cos first; both wrap the angle and call libm sinf)
 *   a = Vec4_Scale(v, cs)
 *   b = Vec3_Cross(axis, v); b = Vec4_Scale(b, sn)
 *   c = Vec4_Scale(axis, Vec3_Dot(axis, v) * (1.0f - cs))    (FPU sub and mul)
 *   out = Vec4_Add(a, b); out = Vec4_Add(out, c); out.w = v.w
 * ORIGINAL BUG (aliasing): v.w is read AFTER out was written. With out == v (most callers) the "restored" w is the
 * sum's own w: (v.w * cs + 0 * sn) + axis.w * k. The axis is not normalised here.
 */
void Ref_Vec3_RotateAxis(RefVec4f *out, const RefVec4f *v, const RefVec4f *axis, float angle) {
    RefVec4f a;
    RefVec4f b;
    RefVec4f c;
    float cs;
    float sn;

    cs = Ref_Mathf_Cos(angle);
    sn = Ref_Mathf_Sin(angle);
    Ref_Vec4_Scale(&a, v, cs);
    Ref_Vec3_Cross(&b, axis, v);
    Ref_Vec4_Scale(&b, &b, sn);
    Ref_Vec4_Scale(&c, axis, f_mul(Ref_Vec3_Dot(axis, v), f_sub(1.0f, cs)));
    Ref_Vec4_Add(out, &a, &b);
    Ref_Vec4_Add(out, out, &c);
    out->w = v->w;
}

/*
 * 0x122790 Vec3_RotateX
 *   a = Vec4_Scale(v, cs); b = (0, (-v.z) * sn, v.y * sn, 0); c = (v.x * (1.0f - cs), 0, 0, 0)
 *   out = Vec4_Add(a, b); out = Vec4_Add(out, c); out.w = v.w
 * b and c are built from v before out is written. ORIGINAL BUG (aliasing): with out == v the final w is
 * (v.w * cs + 0) + 0, i.e. w is multiplied by cos(angle) on every call.
 */
void Ref_Vec3_RotateX(RefVec4f *out, const RefVec4f *v, float angle) {
    RefVec4f a;
    RefVec4f b;
    RefVec4f c;
    float cs;
    float sn;

    cs = Ref_Mathf_Cos(angle);
    sn = Ref_Mathf_Sin(angle);
    Ref_Vec4_Scale(&a, v, cs);
    b.x = 0.0f;
    b.y = f_mul(-v->z, sn);
    b.z = f_mul(v->y, sn);
    b.w = 0.0f;
    c.x = f_mul(v->x, f_sub(1.0f, cs));
    c.y = 0.0f;
    c.z = 0.0f;
    c.w = 0.0f;
    Ref_Vec4_Add(out, &a, &b);
    Ref_Vec4_Add(out, out, &c);
    out->w = v->w;
}

/*
 * 0x122868 Vec3_RotateY
 *   b = (v.z * sn, 0, (-v.x) * sn, 0); c = (0, v.y * (1.0f - cs), 0, 0); otherwise as Vec3_RotateX, same aliasing bug.
 */
void Ref_Vec3_RotateY(RefVec4f *out, const RefVec4f *v, float angle) {
    RefVec4f a;
    RefVec4f b;
    RefVec4f c;
    float cs;
    float sn;

    cs = Ref_Mathf_Cos(angle);
    sn = Ref_Mathf_Sin(angle);
    Ref_Vec4_Scale(&a, v, cs);
    b.x = f_mul(v->z, sn);
    b.y = 0.0f;
    b.z = f_mul(-v->x, sn);
    b.w = 0.0f;
    c.x = 0.0f;
    c.y = f_mul(v->y, f_sub(1.0f, cs));
    c.z = 0.0f;
    c.w = 0.0f;
    Ref_Vec4_Add(out, &a, &b);
    Ref_Vec4_Add(out, out, &c);
    out->w = v->w;
}

/* ------------------------------------------------------------------------------------------------------------------
 * The VU0 R register. No routine of 0x121008..0x122940 touches it; Vu0_Init seeds it through Rand_SeedFloat
 * (0x11F7D8, vrinit) and Rand_Float01 (0x11F830) steps it. The update rule below is the one emulators implement
 * (it is not derivable from the game binary).
 *   vrinit R, x      R = 0x3F800000 | (bits(x) & 0x007FFFFF)
 *   vrnext dest, R   b = ((R >> 4) ^ (R >> 22)) & 1; R = 0x3F800000 | (((R << 1) | b) & 0x007FFFFF); dest = R
 *   vrget dest, R    dest = R                                 (not used by the game)
 *   vrxor R, x       R = 0x3F800000 | ((R ^ bits(x)) & 0x007FFFFF)      (not used by the game)
 * R read as a float is always in [1, 2).
 * ---------------------------------------------------------------------------------------------------------------- */

void Ref_Vu0R_Init(uint32_t *r, float seed) {
    *r = 0x3F800000u | (f_bits(seed) & 0x007FFFFFu);
}

float Ref_Vu0R_Next(uint32_t *r) {
    uint32_t b = ((*r >> 4) ^ (*r >> 22)) & 1u;

    *r = 0x3F800000u | (((*r << 1) | b) & 0x007FFFFFu);
    return f_from(*r);
}

/* Rand_Float01 (0x11F830): seven steps, vrinit from the value just produced, seven steps, minus 1.0 (vsubw.x, vf0.w). */
float Ref_Rand_Float01_Model(uint32_t *r) {
    float f = 0.0f;
    int i;

    for (i = 0; i < 7; i++) {
        f = Ref_Vu0R_Next(r);
    }
    Ref_Vu0R_Init(r, f);
    for (i = 0; i < 7; i++) {
        f = Ref_Vu0R_Next(r);
    }
    return f_sub(f, VF0W);
}
