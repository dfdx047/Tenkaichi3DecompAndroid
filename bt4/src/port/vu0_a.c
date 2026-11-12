/*
 * Portable C reference of the vector / matrix library, first half: main executable 0x11FA10..0x121008.
 * NOT part of the matching build (the matching sources are src/sys/vu0_a_c.c, vu0_a_c_b.c, vu0_a_c_c.c).
 *
 * How to read this file
 * ---------------------
 * Most of the original is hand-written VU0 macro-mode assembly. Each routine here executes the same instructions in
 * the same order on a model of the registers (gRefVu0): one op_xxx call per instruction, with the original address
 * and instruction in the comment beside it. The comment above each routine says what that comes to as a formula.
 * Because it is the instruction sequence itself, the following are right by construction: the order and association
 * of every float operation, which components of the destination are written, what happens when arguments alias,
 * and what is left behind in the scratch registers (some routines of the second half read a register they never
 * set, e.g. the w of the STQ output of the projection routines).
 *
 * Conventions: component masks are M_X .. M_XYZW; vf0 = (0, 0, 0, 1) and cannot be written; vf1 = (0,0,1,0),
 * vf2 = (0,1,0,0), vf3 = (1,0,0,0) once Ref_Vu0_InitAxisRegs has run (the game does it at boot, Vu0_Init 0x121DA8);
 * vf16..vf19 = rows 0..3 of the current matrix; vi15 = matrix stack pointer in rows.
 * "vmadd" is ACC + fs * ft: the product is formed first as a float, then added.
 *
 * Arithmetic model (the port's decision point)
 * --------------------------------------------
 * Every float operation of this file goes through RefVu0_AddBits / SubBits / MulBits / DivBits / SqrtBits /
 * ItofBits / LtBits. Two implementations are provided:
 *
 *   default            integer-only model of the PS2 float unit, the same on every host:
 *                        - an exponent field of 0 is zero (no denormals); an exponent field of 255 is an ordinary
 *                          number (no infinity, no NaN);
 *                        - results are truncated (round toward zero), never rounded to nearest;
 *                        - add / subtract keep ONE guard bit of the smaller operand and no sticky bit, so a
 *                          subtraction can be one unit in the last place above the IEEE round-toward-zero result;
 *                        - overflow gives +/-REF_VU0_FMAX, underflow gives +/-0;
 *                        - x / 0 and 0 / 0 give +/-REF_VU0_FMAX with the sign of the exclusive-or of the signs;
 *                        - square root is taken of the absolute value;
 *                        - x - x is +0; 0 * anything is +/-0.
 *                      NOT modelled: the hardware multiplier drops low partial-product bits, so a product can
 *                      be one unit in the last place low (x * 1.0 is not always x on a console); see mulMantissa.
 *   REF_VU0_NATIVE_FLOAT=1  host float arithmetic with operands and results clamped (exponent 255 -> +/-FLT_MAX,
 *                      denormal -> 0). Rounding is whatever the host is set to; this is what an emulator's fast
 *                      path does and it is NOT bit-exact with the console.
 *
 * The model was not validated against a console or an emulator trace in this work; see the report for the list of
 * points that need such a check. The FPU (COP1) of the EE is treated with the same primitives: it has the same
 * non-IEEE properties (no denormals / infinity / NaN, truncation).
 *
 * Routines of the second half that this half calls (Vec4_Copy, Vec4_Scale, Vec3_Normalize, Vec3_Cross, Mtx_MulVec4,
 * Vec4_SetZeroW1, Vec3_DirToEuler with Vec3_Set / Vec3_Length) are reproduced here as private b_xxx copies so the
 * file stands alone; replace them by the vu0_b reference when both are integrated. Vec3_DirToEuler calls the
 * game's libm atan2f, which is outside this library: REF_ATAN2F is the hook.
 */

#include <string.h>
#include "port/vu0_a.h"
#ifdef REF_VU0_EXTERN_ARITH
/* The port compiles this file without the compiler's built-in functions, which made every float <-> bit pattern
   conversion below (a memcpy of four bytes) a call into the C library: several per float operation. */
#define memcpy(d, s, n) __builtin_memcpy(d, s, n)
#endif

#ifndef REF_VU0_NATIVE_FLOAT
#define REF_VU0_NATIVE_FLOAT 0
#endif
/* Largest magnitude: the result of an overflow and of a division by zero. */
#ifndef REF_VU0_FMAX
#define REF_VU0_FMAX 0x7FFFFFFFu
#endif
#ifndef REF_ATAN2F
#include <math.h>
#define REF_ATAN2F(y, x) atan2f((y), (x))
#endif

#define SIGN 0x80000000u

RefVu0State gRefVu0 = {{{{0u, 0u, 0u, 0x3F800000u}}}};

static uint32_t f2u(float f) {
    uint32_t u;

    memcpy(&u, &f, 4);
    return u;
}

static float u2f(uint32_t u) {
    float f;

    memcpy(&f, &u, 4);
    return f;
}

/* ================================================================================================================
 * Arithmetic primitives
 * ============================================================================================================== */

#if REF_VU0_NATIVE_FLOAT

static float clampIn(uint32_t a) {
    if (((a >> 23) & 0xFF) == 0) {
        return u2f(a & SIGN);
    }
    if (((a >> 23) & 0xFF) == 0xFF) {
        return u2f((a & SIGN) | 0x7F7FFFFFu);
    }
    return u2f(a);
}

static uint32_t clampOut(float f, uint32_t signIfNan) {
    uint32_t r = f2u(f);

    if (((r >> 23) & 0xFF) == 0xFF) {
        return ((r & 0x7FFFFF) ? signIfNan : (r & SIGN)) | 0x7F7FFFFFu;
    }
    if (((r >> 23) & 0xFF) == 0) {
        return r & SIGN;
    }
    return r;
}

uint32_t RefVu0_AddBits(uint32_t a, uint32_t b) {
    return clampOut(clampIn(a) + clampIn(b), a & SIGN);
}

uint32_t RefVu0_MulBits(uint32_t a, uint32_t b) {
    return clampOut(clampIn(a) * clampIn(b), (a ^ b) & SIGN);
}

uint32_t RefVu0_DivBits(uint32_t a, uint32_t b) {
    if (((b >> 23) & 0xFF) == 0) {
        return ((a ^ b) & SIGN) | 0x7F7FFFFFu;
    }
    return clampOut(clampIn(a) / clampIn(b), (a ^ b) & SIGN);
}

uint32_t RefVu0_SqrtBits(uint32_t a) {
    return clampOut(sqrtf(clampIn(a & ~SIGN)), 0);
}

uint32_t RefVu0_ItofBits(int32_t v, int fracBits) {
    return f2u((float)v * (1.0f / (float)(1 << fracBits)));
}

#else /* integer model */

/* REF_VU0_EXTERN_ARITH: add, multiply and divide are supplied by the user of this file (the PC port selects its
   float model in one place, port/src/softfloat_ps2.c); square root, conversion and compare stay here. */
#ifndef REF_VU0_EXTERN_ARITH

/* a + b. One guard bit, no sticky bit, truncation. */
uint32_t RefVu0_AddBits(uint32_t a, uint32_t b) {
    uint32_t ea = (a >> 23) & 0xFF;
    uint32_t eb = (b >> 23) & 0xFF;
    uint32_t ma, mb, m, sign, t;
    int32_t e, d;

    if (ea == 0 && eb == 0) {
        return a & b & SIGN; /* -0 only when both are negative zero */
    }
    if (eb == 0) {
        return a;
    }
    if (ea == 0) {
        return b;
    }
    ma = (a & 0x7FFFFF) | 0x800000;
    mb = (b & 0x7FFFFF) | 0x800000;
    if (ea < eb || (ea == eb && ma < mb)) { /* a = the operand of larger magnitude */
        t = a, a = b, b = t;
        t = ea, ea = eb, eb = t;
        t = ma, ma = mb, mb = t;
    }
    sign = a & SIGN;
    d = (int32_t)(ea - eb);
    ma <<= 1;
    mb = d >= 25 ? 0 : (mb << 1) >> d; /* bits below the guard bit are dropped */
    e = (int32_t)ea;
    if (((a ^ b) & SIGN) == 0) {
        m = ma + mb;
        if (m & (1u << 25)) {
            m >>= 1;
            e++;
        }
    } else {
        m = ma - mb;
        if (m == 0) {
            return 0; /* exact cancellation: +0 */
        }
        while (!(m & (1u << 24))) {
            m <<= 1;
            e--;
        }
    }
    if (e > 255) {
        return sign | REF_VU0_FMAX;
    }
    if (e < 1) {
        return sign;
    }
    return sign | ((uint32_t)e << 23) | ((m >> 1) & 0x7FFFFF);
}

/* 24 x 24 bit mantissa product. The console's multiplier array drops some low partial-product bits, so its product
 * can be one unit lower in bit 15 than the exact one (the reason x * 1.0 is not always x on hardware). That is not
 * modelled: this is the exact product. If the port needs console-identical products, this function is the only
 * place to change (a = mantissa of fs, b = mantissa of ft; the hardware is not symmetric in the two). */
static uint64_t mulMantissa(uint32_t a, uint32_t b) {
    return (uint64_t)a * (uint64_t)b;
}

/* a * b, truncated. Operand order is kept as in the instruction (fs * ft) everywhere in this file. */
uint32_t RefVu0_MulBits(uint32_t a, uint32_t b) {
    uint32_t ea = (a >> 23) & 0xFF;
    uint32_t eb = (b >> 23) & 0xFF;
    uint32_t sign = (a ^ b) & SIGN;
    uint64_t p;
    uint32_t m;
    int32_t e;

    if (ea == 0 || eb == 0) {
        return sign;
    }
    p = mulMantissa((a & 0x7FFFFF) | 0x800000, (b & 0x7FFFFF) | 0x800000);
    e = (int32_t)(ea + eb) - 127;
    if (p & ((uint64_t)1 << 47)) {
        m = (uint32_t)(p >> 24);
        e++;
    } else {
        m = (uint32_t)(p >> 23);
    }
    if (e > 255) {
        return sign | REF_VU0_FMAX;
    }
    if (e < 1) {
        return sign;
    }
    return sign | ((uint32_t)e << 23) | (m & 0x7FFFFF);
}

/* a / b, truncated. */
uint32_t RefVu0_DivBits(uint32_t a, uint32_t b) {
    uint32_t ea = (a >> 23) & 0xFF;
    uint32_t eb = (b >> 23) & 0xFF;
    uint32_t sign = (a ^ b) & SIGN;
    uint64_t q;
    uint32_t m;
    int32_t e;

    if (eb == 0) {
        return sign | REF_VU0_FMAX; /* x / 0 and 0 / 0 */
    }
    if (ea == 0) {
        return sign;
    }
    q = ((uint64_t)((a & 0x7FFFFF) | 0x800000) << 24) / ((b & 0x7FFFFF) | 0x800000);
    e = (int32_t)ea - (int32_t)eb + 127;
    if (q & (1u << 24)) {
        m = (uint32_t)(q >> 1);
    } else {
        m = (uint32_t)q;
        e--;
    }
    if (e > 255) {
        return sign | REF_VU0_FMAX;
    }
    if (e < 1) {
        return sign;
    }
    return sign | ((uint32_t)e << 23) | (m & 0x7FFFFF);
}

#endif /* REF_VU0_EXTERN_ARITH */
#ifdef REF_VU0_EXTERN_ARITH /* the port: the same arithmetic, in place instead of through calls */
#include "softfloat_ps2_inl.h"
#define RefVu0_AddBits Sf_AddBits
#define RefVu0_MulBits Sf_MulBits
#endif

/* sqrt(|a|), truncated. */
uint32_t RefVu0_SqrtBits(uint32_t a) {
    uint32_t ea = (a >> 23) & 0xFF;
    uint64_t n, r, bit;
    int32_t e;

    if (ea == 0) {
        return 0;
    }
    e = (int32_t)ea - 127;
    n = (uint64_t)((a & 0x7FFFFF) | 0x800000);
    if (e & 1) {
        n <<= 1;
        e -= 1;
    }
    n <<= 23; /* 2^46 <= n < 2^48, root in [2^23, 2^24) */
#if defined(REF_VU0_EXTERN_ARITH) && defined(SF_FAST_CALLS)
    {   /* the port: the processor's square root, made the same whole-number root (port/src/plat_fastvec.c) */
        extern uint64_t Port_FastIsqrt48(uint64_t n);
        (void)bit;
        r = Port_FastIsqrt48(n);
    }
#else
    r = 0;
    for (bit = (uint64_t)1 << 46; bit != 0; bit >>= 2) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
    }
#endif
    return ((uint32_t)(e / 2 + 127) << 23) | ((uint32_t)r & 0x7FFFFF);
}

/* (float)v / 2^fracBits, truncated (vitof0 / vitof4 / vitof12). */
uint32_t RefVu0_ItofBits(int32_t v, int fracBits) {
    uint32_t sign = v < 0 ? SIGN : 0;
    uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    int32_t e, n;

    if (u == 0) {
        return 0;
    }
    n = __builtin_clz(u); /* (was a loop of one step per leading zero: 31 steps for 1, every int -> float) */
    u <<= n;
    e = 31 - n;
    return sign | ((uint32_t)(e + 127 - fracBits) << 23) | ((u >> 8) & 0x7FFFFF);
}

#endif /* REF_VU0_NATIVE_FLOAT */

uint32_t RefVu0_SubBits(uint32_t a, uint32_t b) {
    return RefVu0_AddBits(a, b ^ SIGN);
}

/* a < b, comparing the bit patterns as sign and magnitude (+0 == -0). */
int RefVu0_LtBits(uint32_t a, uint32_t b) {
    int32_t x = (a & SIGN) ? -(int32_t)(a & 0x7FFFFFFF) : (int32_t)(a & 0x7FFFFFFF);
    int32_t y = (b & SIGN) ? -(int32_t)(b & 0x7FFFFFFF) : (int32_t)(b & 0x7FFFFFFF);

    return x < y;
}

float RefVu0_Add(float a, float b) {
    return u2f(RefVu0_AddBits(f2u(a), f2u(b)));
}

float RefVu0_Sub(float a, float b) {
    return u2f(RefVu0_SubBits(f2u(a), f2u(b)));
}

float RefVu0_Mul(float a, float b) {
    return u2f(RefVu0_MulBits(f2u(a), f2u(b)));
}

float RefVu0_Div(float a, float b) {
    return u2f(RefVu0_DivBits(f2u(a), f2u(b)));
}

/* ================================================================================================================
 * Register model: one function per instruction form
 * ============================================================================================================== */

enum { X = 0, Y = 1, Z = 2, W = 3 };
enum { T0 = 0, T1, T2, T3, T4, T5, T6, T7 };
#define M_X 8
#define M_Y 4
#define M_Z 2
#define M_W 1
#define M_XY 12
#define M_XW 9
#define M_YW 5
#define M_ZW 3
#define M_XYZ 14
#define M_XYW 13
#define M_YZW 7
#define M_XYZW 15
#define HAS(m, i) ((m) & (8 >> (i)))
#define VF(n) (gRefVu0.vf[n])
#define AT(p, off) ((void *)((char *)(void *)(p) + (off)))

/* Zero / sign flags of a float result on the masked components; bits 6 / 7 are the sticky copies. */
static void setFlags(const uint32_t r[4], int m) {
    uint32_t cur = 0;
    int i;

    for (i = 0; i < 4; i++) {
        if (HAS(m, i)) {
            if ((r[i] & 0x7FFFFFFF) == 0) {
                cur |= 1;
            }
            if (r[i] & SIGN) {
                cur |= 2;
            }
        }
    }
    gRefVu0.status = (gRefVu0.status & 0xFC0) | cur | (cur << 6);
}

static void put(int fd, const uint32_t r[4], int m) {
    int i;

    if (fd == 0) {
        return; /* vf0 is hard-wired */
    }
    for (i = 0; i < 4; i++) {
        if (HAS(m, i)) {
            VF(fd).u[i] = r[i];
        }
    }
}

static void putAcc(const uint32_t r[4], int m) {
    int i;

    for (i = 0; i < 4; i++) {
        if (HAS(m, i)) {
            gRefVu0.acc.u[i] = r[i];
        }
    }
}

/* Four components at once (the op_ routines below). The port: one call into port/src/plat_fastvec.c instead of
   one per component, with the same results component by component as the loops of the other builds. */
#if defined(REF_VU0_EXTERN_ARITH) && defined(SF_FAST_CALLS)
extern void Port_Vu0Add4(const uint32_t *a, const uint32_t *b, uint32_t *out);
extern void Port_Vu0Mul4(const uint32_t *a, const uint32_t *b, uint32_t *out);
extern void Port_Vu0Madd4(const uint32_t *acc, const uint32_t *a, const uint32_t *b, uint32_t *out);
#define add4 Port_Vu0Add4
#define mul4 Port_Vu0Mul4
#define madd4 Port_Vu0Madd4
#else
static void add4(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    int i;
    for (i = 0; i < 4; i++) {
        r[i] = RefVu0_AddBits(a[i], b[i]);
    }
}
static void mul4(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    int i;
    for (i = 0; i < 4; i++) {
        r[i] = RefVu0_MulBits(a[i], b[i]);
    }
}
static void madd4(const uint32_t *acc, const uint32_t *a, const uint32_t *b, uint32_t *r) {
    int i;
    for (i = 0; i < 4; i++) {
        r[i] = RefVu0_AddBits(acc[i], RefVu0_MulBits(a[i], b[i]));
    }
}
#endif
static inline void sub4(const uint32_t *a, const uint32_t *b, uint32_t *r) { /* a - b is a + (-b): RefVu0_SubBits */
    uint32_t n[4] = {b[0] ^ SIGN, b[1] ^ SIGN, b[2] ^ SIGN, b[3] ^ SIGN};
    add4(a, n, r);
}

static void op_lqc2(int ft, const void *p) {
    RefVec4 t;

    memcpy(&t, p, 16);
    put(ft, t.u, M_XYZW);
}

static void op_sqc2(int ft, void *p) {
    memcpy(p, &VF(ft), 16);
}

/* fd = fs + ft / fs - ft / fs * ft */
static void op_vadd(int m, int fd, int fs, int ft) {
    uint32_t r[4];
    add4(VF(fs).u, VF(ft).u, r);
    setFlags(r, m);
    put(fd, r, m);
}

static void op_vsub(int m, int fd, int fs, int ft) {
    uint32_t r[4];
    sub4(VF(fs).u, VF(ft).u, r);
    setFlags(r, m);
    put(fd, r, m);
}

static void op_vmul(int m, int fd, int fs, int ft) {
    uint32_t r[4];
    mul4(VF(fs).u, VF(ft).u, r);
    setFlags(r, m);
    put(fd, r, m);
}

/* fd = fs + ft.c / fs - ft.c / fs * ft.c (one component of ft for all) */
static void op_vaddbc(int m, int fd, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    add4(VF(fs).u, bb, r);
    setFlags(r, m);
    put(fd, r, m);
}

static void op_vsubbc(int m, int fd, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    sub4(VF(fs).u, bb, r);
    setFlags(r, m);
    put(fd, r, m);
}

static void op_vmulbc(int m, int fd, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    mul4(VF(fs).u, bb, r);
    setFlags(r, m);
    put(fd, r, m);
}

/* ACC = fs + ft.c */
static void op_vaddabc(int m, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    add4(VF(fs).u, bb, r);
    setFlags(r, m);
    putAcc(r, m);
}

/* ACC = fs * ft.c */
static void op_vmulabc(int m, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    mul4(VF(fs).u, bb, r);
    setFlags(r, m);
    putAcc(r, m);
}

/* ACC = ACC + fs * ft.c */
static void op_vmaddabc(int m, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    madd4(gRefVu0.acc.u, VF(fs).u, bb, r);
    setFlags(r, m);
    putAcc(r, m);
}

/* fd = ACC + fs * ft.c */
static void op_vmaddbc(int m, int fd, int fs, int ft, int c) {
    uint32_t r[4], b = VF(ft).u[c];
    uint32_t bb[4] = {b, b, b, b};

    madd4(gRefVu0.acc.u, VF(fs).u, bb, r);
    setFlags(r, m);
    put(fd, r, m);
}

/* fd = fs * Q / fs + Q */
static void op_vmulq(int m, int fd, int fs) {
    uint32_t r[4];
    uint32_t qq[4] = {gRefVu0.q, gRefVu0.q, gRefVu0.q, gRefVu0.q};

    mul4(VF(fs).u, qq, r);
    setFlags(r, m);
    put(fd, r, m);
}

static void op_vaddq(int m, int fd, int fs) {
    uint32_t r[4];
    uint32_t qq[4] = {gRefVu0.q, gRefVu0.q, gRefVu0.q, gRefVu0.q};

    add4(VF(fs).u, qq, r);
    setFlags(r, m);
    put(fd, r, m);
}

/* ACC.xyz = (fs.y * ft.z, fs.z * ft.x, fs.x * ft.y) */
static void op_vopmula(int fs, int ft) {
    uint32_t r[4];

    r[X] = RefVu0_MulBits(VF(fs).u[Y], VF(ft).u[Z]);
    r[Y] = RefVu0_MulBits(VF(fs).u[Z], VF(ft).u[X]);
    r[Z] = RefVu0_MulBits(VF(fs).u[X], VF(ft).u[Y]);
    r[W] = 0;
    setFlags(r, M_XYZ);
    putAcc(r, M_XYZ);
}

/* fd.xyz = ACC - (fs.y * ft.z, fs.z * ft.x, fs.x * ft.y) */
static void op_vopmsub(int fd, int fs, int ft) {
    uint32_t r[4];

    r[X] = RefVu0_SubBits(gRefVu0.acc.u[X], RefVu0_MulBits(VF(fs).u[Y], VF(ft).u[Z]));
    r[Y] = RefVu0_SubBits(gRefVu0.acc.u[Y], RefVu0_MulBits(VF(fs).u[Z], VF(ft).u[X]));
    r[Z] = RefVu0_SubBits(gRefVu0.acc.u[Z], RefVu0_MulBits(VF(fs).u[X], VF(ft).u[Y]));
    r[W] = 0;
    setFlags(r, M_XYZ);
    put(fd, r, M_XYZ);
}

/* ft = (fs.y, fs.z, fs.w, fs.x) */
static void op_vmr32(int m, int ft, int fs) {
    uint32_t r[4];

    r[X] = VF(fs).u[Y];
    r[Y] = VF(fs).u[Z];
    r[Z] = VF(fs).u[W];
    r[W] = VF(fs).u[X];
    put(ft, r, m);
}

static void op_vmove(int m, int ft, int fs) {
    uint32_t r[4];

    memcpy(r, VF(fs).u, 16);
    put(ft, r, m);
}

static void itof(int m, int ft, int fs, int frac) {
    uint32_t r[4];
    int i;

    for (i = 0; i < 4; i++) {
        r[i] = RefVu0_ItofBits(VF(fs).i[i], frac);
    }
    put(ft, r, m);
}

static void op_vitof0(int m, int ft, int fs) {
    itof(m, ft, fs, 0);
}

static void op_vitof4(int m, int ft, int fs) {
    itof(m, ft, fs, 4);
}

static void op_vitof12(int m, int ft, int fs) {
    itof(m, ft, fs, 12);
}

/* Q = sqrt(|ft.c|) */
static void op_vsqrt(int ft, int c) {
    gRefVu0.q = RefVu0_SqrtBits(VF(ft).u[c]);
}

/* Q = fs.a / ft.b */
static void op_vdiv(int fs, int a, int ft, int b) {
    gRefVu0.q = RefVu0_DivBits(VF(fs).u[a], VF(ft).u[b]);
}

/* VU0 data memory[vi++] = fs / ft = VU0 data memory[--vi]. The address wraps at 4 KB. */
static void op_vsqi(int fs, int vi) {
    gRefVu0.mem[gRefVu0.vi[vi] & 0xFF] = VF(fs);
    gRefVu0.vi[vi]++;
}

static void op_vlqd(int ft, int vi) {
    gRefVu0.vi[vi]--;
    put(ft, gRefVu0.mem[gRefVu0.vi[vi] & 0xFF].u, M_XYZW);
}

/* EE side: 128-bit general registers $t0..$t7. */
static void ee_lq(int t, const void *p) {
    memcpy(&gRefVu0.gpr[t], p, 16);
}

static void ee_sq(int t, void *p) {
    memcpy(p, &gRefVu0.gpr[t], 16);
}

/* mfc1: low 64 bits = the float's bits sign-extended; bits 64..127 keep what the register held. */
static void ee_mfc1(int t, uint32_t bits) {
    gRefVu0.gpr[t].u[0] = bits;
    gRefVu0.gpr[t].u[1] = (bits & SIGN) ? 0xFFFFFFFFu : 0;
}

/* lui / dsll / ori / dsll: low 64 bits = hi:lo; bits 64..127 keep what the register held. */
static void ee_set64(int t, uint32_t lo, uint32_t hi) {
    gRefVu0.gpr[t].u[0] = lo;
    gRefVu0.gpr[t].u[1] = hi;
}

static void ee_pextlw(int d, int s, int t) {
    RefVec4 r;

    r.u[0] = gRefVu0.gpr[t].u[0];
    r.u[1] = gRefVu0.gpr[s].u[0];
    r.u[2] = gRefVu0.gpr[t].u[1];
    r.u[3] = gRefVu0.gpr[s].u[1];
    gRefVu0.gpr[d] = r;
}

static void ee_pextuw(int d, int s, int t) {
    RefVec4 r;

    r.u[0] = gRefVu0.gpr[t].u[2];
    r.u[1] = gRefVu0.gpr[s].u[2];
    r.u[2] = gRefVu0.gpr[t].u[3];
    r.u[3] = gRefVu0.gpr[s].u[3];
    gRefVu0.gpr[d] = r;
}

static void ee_pcpyld(int d, int s, int t) {
    RefVec4 r;

    r.u[0] = gRefVu0.gpr[t].u[0];
    r.u[1] = gRefVu0.gpr[t].u[1];
    r.u[2] = gRefVu0.gpr[s].u[0];
    r.u[3] = gRefVu0.gpr[s].u[1];
    gRefVu0.gpr[d] = r;
}

static void ee_pcpyud(int d, int s, int t) {
    RefVec4 r;

    r.u[0] = gRefVu0.gpr[s].u[2];
    r.u[1] = gRefVu0.gpr[s].u[3];
    r.u[2] = gRefVu0.gpr[t].u[2];
    r.u[3] = gRefVu0.gpr[t].u[3];
    gRefVu0.gpr[d] = r;
}

static void op_qmtc2(int t, int fd) {
    put(fd, gRefVu0.gpr[t].u, M_XYZW);
}

static void op_qmfc2(int t, int fs) {
    gRefVu0.gpr[t] = VF(fs);
}

/* ================================================================================================================
 * 0x10FFD0 ObjSeam_TransformVtx(vtx, work, mtx, ofs): skins, lights and projects one seam vertex of a model.
 * Hand-written VU0 macro code of the game, translated here instruction for instruction (the comments give the
 * address and the instruction). The screen matrix is in vf24..27 (set by the caller's surroundings); the routine
 * leaves vf1..16 and vf19 changed, as on the PS2 (its caller runs Vu0_InitAxisRegs afterwards).
 *   vtx:  +0x00 position (w = weight of the first of the two nodes), +0x10 normal, +0x20 uv;
 *         written: +0x30 GS XYZ (12.4; w = 0xFFFF8000 when the point is outside), +0x50 uv * Q, +0x60 (shade, 0, 1) * Q
 *   work: +0x80 light matrix, +0xC0 k (w = 0.5); written: +0x00 light * mtx[0], +0x40 light * mtx[1]
 * BT3_SEAM_CHECK=<path of SLUS_216.78>: every call is also run by a small interpreter from the instruction words
 * of the original program, and what the two write (memory and registers) is compared.
 * ============================================================================================================== */
static uint32_t ftoiBits(uint32_t a, int frac) {
    uint32_t m = (a & 0x7FFFFF) | 0x800000, v;
    int e = (int)((a >> 23) & 0xFF) - 127 + frac;

    if (((a >> 23) & 0xFF) == 0 || e < 0) {
        return 0;
    }
    if (e >= 31) {
        return (a & SIGN) ? 0x80000000u : 0x7FFFFFFFu; /* saturates */
    }
    v = e >= 23 ? m << (e - 23) : m >> (23 - e);
    return (a & SIGN) ? 0u - v : v;
}

static void op_vftoi(int m, int ft, int fs, int frac) {
    uint32_t r[4];
    int i;

    for (i = 0; i < 4; i++) {
        r[i] = ftoiBits(VF(fs).u[i], frac);
    }
    put(ft, r, m);
}

static void op_vmfir(int m, int ft, int vi) {
    uint32_t r[4];
    int i;

    for (i = 0; i < 4; i++) {
        r[i] = (uint32_t)(int32_t)(int16_t)gRefVu0.vi[vi];
    }
    put(ft, r, m);
}

static void seam_c(void *a0, void *a1, const void *a2, const void *a3) {
    static const uint32_t k4096[4] = {0x45800000u, 0x45800000u, 0, 0}; /* z, w: the upper half of $v0, not known */
    int i, out;

    put(19, k4096, M_XYZW);                    /* 10FFD0..E0  $v0 = 4096.0f twice; qmtc2 $v0, vf19 */
    gRefVu0.vi[1] = 0x8000;                    /* 10FFE4..E8  ctc2 0x8000, vi1 */
    for (i = 0; i < 8; i++) {
        op_lqc2(1 + i, AT(a2, i * 16));        /* 10FFF0..0C  lqc2 vf1..vf8, 0x00..0x70(a2) */
    }
    for (i = 0; i < 4; i++) {
        op_lqc2(9 + i, AT(a1, 0x80 + i * 16)); /* 110010..1C  lqc2 vf9..vf12, 0x80..0xB0(a1) */
    }
    for (i = 0; i < 3; i++) {                  /* 110020..4C  vf13 + i = light * vf(1 + i) */
        op_vmulabc(M_XYZW, 9, 1 + i, X);
        op_vmaddabc(M_XYZW, 10, 1 + i, Y);
        op_vmaddabc(M_XYZW, 11, 1 + i, Z);
        op_vmaddbc(M_XYZW, 13 + i, 12, 1 + i, W);
    }
    op_sqc2(13, AT(a1, 0x00));                 /* 110050..5C */
    op_sqc2(14, AT(a1, 0x10));
    op_sqc2(15, AT(a1, 0x20));
    op_sqc2(12, AT(a1, 0x30));
    for (i = 0; i < 3; i++) {                  /* 110060..8C  the same with vf5..vf7 */
        op_vmulabc(M_XYZW, 9, 5 + i, X);
        op_vmaddabc(M_XYZW, 10, 5 + i, Y);
        op_vmaddabc(M_XYZW, 11, 5 + i, Z);
        op_vmaddbc(M_XYZW, 13 + i, 12, 5 + i, W);
    }
    op_sqc2(13, AT(a1, 0x40));                 /* 110090..9C */
    op_sqc2(14, AT(a1, 0x50));
    op_sqc2(15, AT(a1, 0x60));
    op_sqc2(12, AT(a1, 0x70));
    op_lqc2(13, AT(a0, 0x10));                 /* 1100A0  normal */
    op_lqc2(16, AT(a0, 0x00));                 /* 1100A4  position */
    for (i = 0; i < 4; i++) {
        op_lqc2(9 + i, AT(a1, i * 16));        /* 1100A8..B4 */
    }
    op_vmulabc(M_XYZW, 9, 13, X);              /* 1100B8 */
    op_vmaddabc(M_XYZW, 10, 13, Y);
    op_vmaddabc(M_XYZW, 11, 13, Z);
    op_vmaddbc(M_XYZW, 14, 12, 0, W);          /* 1100C4  vmaddw vf14, vf12, vf0w */
    for (i = 0; i < 4; i++) {
        op_lqc2(9 + i, AT(a1, 0x40 + i * 16)); /* 1100C8..D4 */
    }
    op_vmulabc(M_XYZW, 9, 13, X);              /* 1100D8 */
    op_vmaddabc(M_XYZW, 10, 13, Y);
    op_vmaddabc(M_XYZW, 11, 13, Z);
    op_vmaddbc(M_XYZW, 15, 12, 0, W);          /* 1100E4 */
    op_vsub(M_XYZW, 14, 14, 15);               /* 1100E8 */
    op_vmulbc(M_XYZW, 14, 14, 16, W);          /* 1100EC  vmulw vf14, vf14, vf16w */
    op_vadd(M_XYZ, 14, 15, 14);                /* 1100F0 */
    op_lqc2(13, AT(a1, 0xC0));                 /* 1100F4 */
    op_vmulbc(M_XYZ, 14, 14, 13, W);           /* 1100F8 */
    op_vaddbc(M_XYZ, 14, 14, 13, W);           /* 1100FC */
    op_vadd(M_X, 15, 0, 14);                   /* 110100 */
    op_vadd(M_Y, 15, 0, 0);                    /* 110104 */
    op_vaddbc(M_Z, 15, 0, 0, W);               /* 110108 */
    op_lqc2(9, AT(a3, 0x00));                  /* 11010C */
    op_lqc2(10, AT(a3, 0x10));
    op_vsub(M_XYZW, 11, 16, 9);                /* 110114 */
    op_vsub(M_XYZW, 12, 16, 10);
    op_vmulabc(M_XYZW, 1, 11, X);              /* 11011C */
    op_vmaddabc(M_XYZW, 2, 11, Y);
    op_vmaddabc(M_XYZW, 3, 11, Z);
    op_vmaddbc(M_XYZW, 13, 4, 0, W);
    op_vmulabc(M_XYZW, 5, 12, X);              /* 11012C */
    op_vmaddabc(M_XYZW, 6, 12, Y);
    op_vmaddabc(M_XYZW, 7, 12, Z);
    op_vmaddbc(M_XYZW, 14, 8, 0, W);
    op_vsub(M_XYZW, 9, 13, 14);                /* 11013C */
    op_vmulbc(M_XYZW, 9, 9, 16, W);
    op_vadd(M_XYZW, 9, 14, 9);
    op_vmulabc(M_XYZW, 24, 9, X);              /* 110148  the screen matrix */
    op_vmaddabc(M_XYZW, 25, 9, Y);
    op_vmaddabc(M_XYZW, 26, 9, Z);
    op_vmaddbc(M_XYZW, 9, 27, 9, W);
    op_vdiv(0, W, 9, W);                       /* 110158  Q = 1 / w */
    op_vmulq(M_XYZ, 9, 9);                     /* 110160 */
    op_vftoi(M_XYZW, 10, 9, 4);                /* 110164  vftoi4 */
    {   /* (the port: the position before it is rounded to the GS's 1/16 pixel, for the triangles drawn from it) */
        /* (the floats as their bits: this file is built with the game's software float, which passes a float
           otherwise than the PC's code takes it) */
        extern void Port_SeamExact(uint32_t x, uint32_t y, uint32_t z, uint32_t fx, uint32_t fy); /* port/src/gs/gs_core.c */
        Port_SeamExact(gRefVu0.vf[10].u[0], gRefVu0.vf[10].u[1], gRefVu0.vf[10].u[2], gRefVu0.vf[9].u[0], gRefVu0.vf[9].u[1]);
    }
    gRefVu0.status = 0;                        /* 110170  ctc2 $zero, vi16 */
    op_vsub(M_XYW, 0, 9, 0);                   /* 110174  flags only: x, y, w - 1 */
    op_vsub(M_XY, 0, 19, 9);                   /* 110178  flags only: 4096 - x, 4096 - y */
    out = (gRefVu0.status & 0xC0) != 0;        /* 11018C..A4  sticky zero or sign: outside */
    if (out) {
        op_vmfir(M_W, 10, 1);                  /* 1101B0  vmfir.w vf10, vi1 */
    }
    op_lqc2(11, AT(a0, 0x20));                 /* 1101B4  uv */
    op_vmulq(M_XYZ, 12, 11);                   /* 1101B8 */
    op_vmulq(M_XYZ, 14, 15);                   /* 1101BC */
    op_sqc2(12, AT(a0, 0x50));                 /* 1101C0 */
    op_sqc2(14, AT(a0, 0x60));
    op_sqc2(10, AT(a0, 0x30));                 /* 1101CC */
}

/* The check: the routine's 128 instruction words, read from the original program, executed one by one. Only the
   instructions this routine uses are known. The arithmetic of each instruction is the library's (the same as
   above), so this checks the translation, not the number model. */
#include <stdio.h>
#include <stdlib.h>
static uint32_t sSeamCode[128];

static int seam_code_load(const char *path) {
    FILE *fp = fopen(path, "rb");
    unsigned char h[0x40], ph[0x20];
    uint32_t phoff, i, n;
    int ok = 0;

    if (fp == NULL) {
        return 0;
    }
    if (fread(h, 1, sizeof(h), fp) == sizeof(h) && h[0] == 0x7F && h[1] == 'E') {
        memcpy(&phoff, h + 0x1C, 4);
        n = h[0x2C] | h[0x2D] << 8;
        for (i = 0; i < n && !ok; i++) {
            uint32_t off, vaddr, filesz;
            fseek(fp, (long)(phoff + i * 0x20), SEEK_SET);
            if (fread(ph, 1, sizeof(ph), fp) != sizeof(ph)) {
                break;
            }
            memcpy(&off, ph + 4, 4);
            memcpy(&vaddr, ph + 8, 4);
            memcpy(&filesz, ph + 16, 4);
            if (vaddr <= 0x10FFD0 && 0x10FFD0 + sizeof(sSeamCode) <= vaddr + filesz) {
                fseek(fp, (long)(off + (0x10FFD0 - vaddr)), SEEK_SET);
                ok = fread(sSeamCode, 4, 128, fp) == 128;
            }
        }
    }
    fclose(fp);
    return ok;
}

static int seam_interp(void *a0, void *a1, const void *a2, const void *a3) {
    void *reg[32];      /* the four pointer arguments, by register number */
    uint64_t g[32];     /* the other general registers */
    int pc = 0, next, steps;

    memset(reg, 0, sizeof(reg));
    memset(g, 0, sizeof(g));
    reg[4] = a0;
    reg[5] = a1;
    reg[6] = (void *)a2;
    reg[7] = (void *)a3;
    for (steps = 0; steps < 1000; steps++) {
        uint32_t w = sSeamCode[pc], op = w >> 26, rs = (w >> 21) & 31, rt = (w >> 16) & 31, rd = (w >> 11) & 31, sa = (w >> 6) & 31;
        int16_t imm = (int16_t)(w & 0xFFFF);
        int slot = 0, target = -1;

        next = pc + 1;
        if (w == 0) {
            /* nop */
        } else if (op == 0x0F) {
            g[rt] = (uint64_t)(int64_t)(int32_t)((uint32_t)(uint16_t)imm << 16);              /* lui */
        } else if (op == 0x0D) {
            g[rt] = g[rs] | (uint16_t)imm;                                                    /* ori */
        } else if (op == 0x0C) {
            g[rt] = g[rs] & (uint16_t)imm;                                                    /* andi */
        } else if (op == 0x09) {
            g[rt] = (uint64_t)(int64_t)(int32_t)((uint32_t)g[rs] + (uint32_t)(int32_t)imm);   /* addiu */
        } else if (op == 0 && (w & 63) == 0x38) {
            g[rd] = g[rt] << sa;                                                              /* dsll */
        } else if (op == 0 && (w & 63) == 0x00) {
            g[rd] = (uint64_t)(int64_t)(int32_t)((uint32_t)g[rt] << sa);                      /* sll */
        } else if (op == 0 && (w & 63) == 0x08) {
            slot = 1;                                                                         /* jr $ra: after the next one */
            target = 128;
        } else if (op == 0x04) {
            slot = 1;                                                                         /* beq */
            target = g[rs] == g[rt] ? pc + 1 + imm : pc + 2;
        } else if (op == 0x36) {
            op_lqc2((int)rt, AT(reg[rs], imm));                                               /* lqc2 */
        } else if (op == 0x3E) {
            op_sqc2((int)rt, AT(reg[rs], imm));                                               /* sqc2 */
        } else if (op == 0x12 && rs == 0x05) {                                                /* qmtc2: 128 bits, the upper 64 unknown */
            uint32_t v[4] = {(uint32_t)g[rt], (uint32_t)(g[rt] >> 32), 0, 0};
            put((int)rd, v, M_XYZW);
        } else if (op == 0x12 && rs == 0x06) {                                                /* ctc2 */
            if (rd == 16) {
                gRefVu0.status = (uint32_t)g[rt];
            } else if (rd < 16) {
                gRefVu0.vi[rd] = (uint16_t)g[rt];
            } else {
                return 0;
            }
        } else if (op == 0x12 && rs == 0x02) {                                                /* cfc2 */
            if (rd != 16) {
                return 0;
            }
            g[rt] = gRefVu0.status;
        } else if (op == 0x12 && (w & 0x02000000)) {
            int m = (int)((w >> 21) & 15), ft = (int)rt, fs = (int)rd, fd = (int)sa, bc = (int)(w & 3), fn = (int)(w & 63);
            if (fn < 0x3C) {
                if (fn < 4) { op_vaddbc(m, fd, fs, ft, bc); }
                else if (fn >= 0x08 && fn < 0x0C) { op_vmaddbc(m, fd, fs, ft, bc); }
                else if (fn >= 0x18 && fn < 0x1C) { op_vmulbc(m, fd, fs, ft, bc); }
                else if (fn == 0x1C) { op_vmulq(m, fd, fs); }
                else if (fn == 0x28) { op_vadd(m, fd, fs, ft); }
                else if (fn == 0x2C) { op_vsub(m, fd, fs, ft); }
                else { return 0; }
            } else {
                int idx = (int)(sa << 2) | bc;
                if (idx >= 0x08 && idx < 0x0C) { op_vmaddabc(m, fs, ft, bc); }
                else if (idx >= 0x18 && idx < 0x1C) { op_vmulabc(m, fs, ft, bc); }
                else if (idx == 0x15) { op_vftoi(m, ft, fs, 4); }
                else if (idx == 0x38) { op_vdiv(fs, (int)((w >> 21) & 3), ft, (int)((w >> 23) & 3)); }
                else if (idx == 0x3D) { op_vmfir(m, ft, fs); }
                else if (idx == 0x3B || idx == 0x2F) { /* vwaitq, vnop */ }
                else { return 0; }
            }
        } else {
            return 0;
        }
        if (slot) { /* the instruction after a branch runs first */
            uint32_t d = sSeamCode[pc + 1];
            if (d == 0) {
                /* nop */
            } else if ((d >> 26) == 0x3E) {
                op_sqc2((int)((d >> 16) & 31), AT(reg[(d >> 21) & 31], (int16_t)(d & 0xFFFF)));
            } else {
                return 0;
            }
            next = target;
        }
        if (next >= 128) {
            return 1;
        }
        pc = next;
    }
    return 0;
}

void Ref_ObjSeam_TransformVtx(void *vtx, void *work, const void *mtx, const void *ofs) {
    static int mode = -1; /* 0 = no check, 1 = check, 2 = the check cannot run */
    static unsigned calls, bad;

    if (mode < 0) {
        const char *path = getenv("BT3_SEAM_CHECK");
        mode = path == NULL ? 0 : seam_code_load(path) ? 1 : 2;
        if (mode == 2) {
            fprintf(stderr, "bt3: seam check: cannot read the routine from %s\n", path);
        }
    }
    if (mode == 1) {
        static RefVu0State before, afterC;
        unsigned char v0[0x70], w0[0xD0], vC[0x70], wC[0xD0];
        int ran, same;

        before = gRefVu0;
        memcpy(v0, vtx, sizeof(v0));
        memcpy(w0, work, sizeof(w0));
        seam_c(vtx, work, mtx, ofs);
        afterC = gRefVu0;
        memcpy(vC, vtx, sizeof(vC));
        memcpy(wC, work, sizeof(wC));
        gRefVu0 = before;
        memcpy(vtx, v0, sizeof(v0));
        memcpy(work, w0, sizeof(w0));
        ran = seam_interp(vtx, work, mtx, ofs);
        same = ran && memcmp(vC, vtx, sizeof(vC)) == 0 && memcmp(wC, work, sizeof(wC)) == 0 &&
               memcmp(afterC.vf, gRefVu0.vf, sizeof(afterC.vf)) == 0 && memcmp(&afterC.acc, &gRefVu0.acc, sizeof(afterC.acc)) == 0 &&
               afterC.q == gRefVu0.q && afterC.status == gRefVu0.status && memcmp(afterC.vi, gRefVu0.vi, sizeof(afterC.vi)) == 0;
        calls++;
        if (!same) {
            bad++;
            if (bad <= 5) {
                fprintf(stderr, "bt3: seam check: call %u differs (%s)\n", calls, ran ? "outputs" : "the interpreter met an instruction it does not know");
            }
        }
        if (calls == 1 || calls % 20000 == 0) {
            fprintf(stderr, "bt3: seam check: %u vertices through the original instructions and the C version, %u differ\n", calls, bad);
        }
        return; /* the interpreter's results are in place */
    }
    seam_c(vtx, work, mtx, ofs);
}

void Ref_Vu0_ResetState(void) {
    memset(&gRefVu0, 0, sizeof(gRefVu0));
    gRefVu0.vf[0].u[W] = 0x3F800000u;
}

/* ================================================================================================================
 * Private copies of second-half routines this half calls (0x121E18..0x122258)
 * ============================================================================================================== */

/* 0x121E18 Vec4_SetZeroW1: v = vf0 = (0, 0, 0, 1). */
static void b_Vec4_SetZeroW1(RefVec4 *a0) {
    op_sqc2(0, a0);                        /* 121E1C  sqc2 vf0, 0x0(a0) */
}

/* 0x121E40 Vec3_Set: x, y, z stored (in the order z, x, y); w untouched. Compiled C. */
static void b_Vec3_Set(RefVec4 *v, uint32_t x, uint32_t y, uint32_t z) {
    v->u[Z] = z;
    v->u[X] = x;
    v->u[Y] = y;
}

/* 0x121E50 Vec3_Normalize: d = (x*x + y*y) + vf3.x * (z*z); len = vsqrt(d); Q = 1 / len (vdiv, vf0.w / len);
 * out.xyz = in.xyz * Q; out.w = 0. A zero vector gives Q = FMAX and out = (0, 0, 0, 0). */
static void b_Vec3_Normalize(RefVec4 *a0, RefVec4 *a1) {
    op_lqc2(4, a1);                        /* 121E50  lqc2 vf4, 0x0(a1) */
    op_vmul(M_XYZ, 5, 4, 4);               /* 121E54  vmul.xyz vf5, vf4, vf4 */
    op_vaddabc(M_X, 5, 5, Y);              /* 121E58  vadday.x ACC, vf5, vf5y */
    op_vmaddbc(M_X, 5, 3, 5, Z);           /* 121E5C  vmaddz.x vf5, vf3, vf5z */
    op_vsqrt(5, X);                        /* 121E60  vsqrt Q, vf5x */
    op_vaddq(M_X, 5, 0);                   /* 121E68  vaddq.x vf5, vf0, Q */
    op_vdiv(0, W, 5, X);                   /* 121E74  vdiv Q, vf0w, vf5x */
    op_vsub(M_XYZW, 6, 0, 0);              /* 121E78  vsub.xyzw vf6, vf0, vf0 */
    op_vmulq(M_XYZ, 6, 4);                 /* 121E80  vmulq.xyz vf6, vf4, Q */
    op_sqc2(6, a0);                        /* 121E88  sqc2 vf6, 0x0(a0) */
}

/* 0x121F38 Vec4_Scale: out = in * s on four components. */
static void b_Vec4_Scale(RefVec4 *a0, RefVec4 *a1, uint32_t f12) {
    op_lqc2(4, a1);                        /* 121F38  lqc2 vf4, 0x0(a1) */
    ee_mfc1(T0, f12);                      /* 121F3C  mfc1 t0, f12 */
    op_qmtc2(T0, 5);                       /* 121F40  qmtc2.ni t0, vf5 */
    op_vmulbc(M_XYZW, 4, 4, 5, X);         /* 121F44  vmulx.xyzw vf4, vf4, vf5x */
    op_sqc2(4, a0);                        /* 121F4C  sqc2 vf4, 0x0(a0) */
}

/* 0x121FA8 Vec4_Copy: one 128-bit move through $t0. */
static void b_Vec4_Copy(RefVec4 *a0, RefVec4 *a1) {
    ee_lq(T0, a1);                         /* 121FA8  lq t0, 0x0(a1) */
    ee_sq(T0, a0);                         /* 121FB0  sq t0, 0x0(a0) */
}

/* 0x121FD0 Mtx_MulVec4: out = ((row0 * v.x + row1 * v.y) + row2 * v.z) + row3 * v.w, four components. */
static void b_Mtx_MulVec4(RefVec4 *a0, RefMtx44 *a1, RefVec4 *a2) {
    op_lqc2(8, a2);                        /* 121FD0  lqc2 vf8, 0x0(a2) */
    op_lqc2(4, a1);                        /* 121FD4  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(a1, 0x10));              /* 121FD8  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(a1, 0x20));              /* 121FDC  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(a1, 0x30));              /* 121FE0  lqc2 vf7, 0x30(a1) */
    op_vmulabc(M_XYZW, 4, 8, X);           /* 121FE4  vmulax.xyzw ACC, vf4, vf8x */
    op_vmaddabc(M_XYZW, 5, 8, Y);          /* 121FE8  vmadday.xyzw ACC, vf5, vf8y */
    op_vmaddabc(M_XYZW, 6, 8, Z);          /* 121FEC  vmaddaz.xyzw ACC, vf6, vf8z */
    op_vmaddbc(M_XYZW, 8, 7, 8, W);        /* 121FF0  vmaddw.xyzw vf8, vf7, vf8w */
    op_sqc2(8, a0);                        /* 121FF8  sqc2 vf8, 0x0(a0) */
}

/* 0x1220B0 Vec3_Cross: out = (a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y), out.w = b.w - b.w = +0. */
static void b_Vec3_Cross(RefVec4 *a0, RefVec4 *a1, RefVec4 *a2) {
    op_lqc2(4, a1);                        /* 1220B0  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, a2);                        /* 1220B4  lqc2 vf5, 0x0(a2) */
    op_vopmula(4, 5);                      /* 1220B8  vopmula.xyz ACC, vf4, vf5 */
    op_vopmsub(5, 5, 4);                   /* 1220BC  vopmsub.xyz vf5, vf5, vf4 */
    op_vsub(M_W, 5, 5, 5);                 /* 1220C0  vsub.w vf5, vf5, vf5 */
    op_sqc2(5, a0);                        /* 1220C8  sqc2 vf5, 0x0(a0) */
}

/* 0x1221B8 Vec3_Length: vsqrt((x*x + y*y) + vf3.x * (z*z)), returned from Q. */
static uint32_t b_Vec3_Length(RefVec4 *a0) {
    op_lqc2(4, a0);                        /* 1221B8  lqc2 vf4, 0x0(a0) */
    op_vmul(M_XYZ, 4, 4, 4);               /* 1221BC  vmul.xyz vf4, vf4, vf4 */
    op_vaddabc(M_X, 4, 4, Y);              /* 1221C0  vadday.x ACC, vf4, vf4y */
    op_vmaddbc(M_X, 4, 3, 4, Z);           /* 1221C4  vmaddz.x vf4, vf3, vf4z */
    op_vsqrt(4, X);                        /* 1221C8  vsqrt Q, vf4x */
    return gRefVu0.q;                      /* 1221D0  cfc2 t0, vi22 ; mtc1 t0, f0 */
}

/* 0x122258 Vec3_DirToEuler (compiled C): out.y = atan2f(dir.x, dir.z); out.x = -atan2f(dir.y, length(dir.x, 0,
 * dir.z)); out.z = 0. out.y is stored before dir is read again (matters only if out == dir). The temporary's w is
 * uninitialised stack in the original (it only reaches vf4.w); 0 here. */
static void b_Vec3_DirToEuler(RefVec4 *out, RefVec4 *dir) {
    RefVec4 tmp;
    uint32_t len;

    memset(&tmp, 0, sizeof(tmp));
    out->f[Y] = REF_ATAN2F(dir->f[X], dir->f[Z]);
    b_Vec3_Set(&tmp, dir->u[X], 0, dir->u[Z]);
    len = b_Vec3_Length(&tmp);
    out->u[Z] = 0;
    out->u[X] = f2u(REF_ATAN2F(dir->f[Y], u2f(len))) ^ SIGN;
}

/* ================================================================================================================
 * 0x11FA10..0x11FE80: integer vectors (compiled C except Swap / Copy / ToFloat)
 * ============================================================================================================== */

/* 0x11FA10: v = (0, 0, 0, 1). */
void Ref_IVec4_SetZeroW1(RefVec4 *v) {
    v->i[X] = 0;
    v->i[W] = 1;
    v->i[Y] = 0;
    v->i[Z] = 0;
}

/* 0x11FA28: v = (0, 0, 0, 0). */
void Ref_IVec4_SetZero(RefVec4 *v) {
    v->i[W] = 0;
    v->i[X] = 0;
    v->i[Y] = 0;
    v->i[Z] = 0;
}

/* 0x11FA40: v = (x, y, z, w). */
void Ref_IVec4_Set(RefVec4 *v, int32_t x, int32_t y, int32_t z, int32_t w) {
    v->i[W] = w;
    v->i[X] = x;
    v->i[Y] = y;
    v->i[Z] = z;
}

/* 0x11FA58: v.xyz = (x, y, z); v.w untouched. */
void Ref_IVec3_Set(RefVec4 *v, int32_t x, int32_t y, int32_t z) {
    v->i[Z] = z;
    v->i[X] = x;
    v->i[Y] = y;
}

/* 0x11FA68: swap, both loaded before either is stored.  lq t0,(a0) ; lq t1,(a1) ; sq t0,(a1) ; sq t1,(a0) */
void Ref_IVec4_Swap(RefVec4 *a, RefVec4 *b) {
    ee_lq(T0, a);                          /* 11FA68  lq t0, 0x0(a0) */
    ee_lq(T1, b);                          /* 11FA6C  lq t1, 0x0(a1) */
    ee_sq(T0, b);                          /* 11FA70  sq t0, 0x0(a1) */
    ee_sq(T1, a);                          /* 11FA78  sq t1, 0x0(a0) */
}

/* 0x11FA80: out = a + b, component by component in the order x, y, z, w (each stored before the next is read). */
void Ref_IVec4_Add(RefVec4 *out, RefVec4 *a, RefVec4 *b) {
    out->u[X] = a->u[X] + b->u[X];
    out->u[Y] = a->u[Y] + b->u[Y];
    out->u[Z] = a->u[Z] + b->u[Z];
    out->u[W] = a->u[W] + b->u[W];
}

/* 0x11FAC8: x, y, z only. */
void Ref_IVec3_Add(RefVec4 *out, RefVec4 *a, RefVec4 *b) {
    out->u[X] = a->u[X] + b->u[X];
    out->u[Y] = a->u[Y] + b->u[Y];
    out->u[Z] = a->u[Z] + b->u[Z];
}

/* 0x11FB00: out = a - b. */
void Ref_IVec4_Sub(RefVec4 *out, RefVec4 *a, RefVec4 *b) {
    out->u[X] = a->u[X] - b->u[X];
    out->u[Y] = a->u[Y] - b->u[Y];
    out->u[Z] = a->u[Z] - b->u[Z];
    out->u[W] = a->u[W] - b->u[W];
}

/* 0x11FB48: x, y, z only. */
void Ref_IVec3_Sub(RefVec4 *out, RefVec4 *a, RefVec4 *b) {
    out->u[X] = a->u[X] - b->u[X];
    out->u[Y] = a->u[Y] - b->u[Y];
    out->u[Z] = a->u[Z] - b->u[Z];
}

/* 0x11FB80: out = a * b per component, low 32 bits. */
void Ref_IVec4_Mul(RefVec4 *out, RefVec4 *a, RefVec4 *b) {
    out->u[X] = a->u[X] * b->u[X];
    out->u[Y] = a->u[Y] * b->u[Y];
    out->u[Z] = a->u[Z] * b->u[Z];
    out->u[W] = a->u[W] * b->u[W];
}

/* 0x11FBC8: x, y, z only. */
void Ref_IVec3_Mul(RefVec4 *out, RefVec4 *a, RefVec4 *b) {
    out->u[X] = a->u[X] * b->u[X];
    out->u[Y] = a->u[Y] * b->u[Y];
    out->u[Z] = a->u[Z] * b->u[Z];
}

/* 0x11FC00: out = a * s, low 32 bits. */
void Ref_IVec4_Scale(RefVec4 *out, RefVec4 *a, int32_t s) {
    out->u[X] = a->u[X] * (uint32_t)s;
    out->u[Y] = a->u[Y] * (uint32_t)s;
    out->u[Z] = a->u[Z] * (uint32_t)s;
    out->u[W] = a->u[W] * (uint32_t)s;
}

/* 0x11FC38: x, y, z only. */
void Ref_IVec3_Scale(RefVec4 *out, RefVec4 *a, int32_t s) {
    out->u[X] = a->u[X] * (uint32_t)s;
    out->u[Y] = a->u[Y] * (uint32_t)s;
    out->u[Z] = a->u[Z] * (uint32_t)s;
}

/* MIPS div: truncating; INT_MIN / -1 gives INT_MIN without a trap. */
static int32_t idiv(int32_t a, int32_t s) {
    if (a == INT32_MIN && s == -1) {
        return INT32_MIN;
    }
    return a / s;
}

/* 0x11FC60: out = a / s. s == 0 executes "break 7" before anything is written (a trap on the console); here the
 * routine returns with out untouched. */
void Ref_IVec4_Div(RefVec4 *out, RefVec4 *a, int32_t s) {
    if (s == 0) {
        return;
    }
    out->i[X] = idiv(a->i[X], s);
    out->i[Y] = idiv(a->i[Y], s);
    out->i[Z] = idiv(a->i[Z], s);
    out->i[W] = idiv(a->i[W], s);
}

/* 0x11FCB0: x, y, z only. */
void Ref_IVec3_Div(RefVec4 *out, RefVec4 *a, int32_t s) {
    if (s == 0) {
        return;
    }
    out->i[X] = idiv(a->i[X], s);
    out->i[Y] = idiv(a->i[Y], s);
    out->i[Z] = idiv(a->i[Z], s);
}

/* 0x11FCF0: out = in, one 128-bit move.  lq t0,(a1) ; sq t0,(a0) */
void Ref_IVec4_Copy(RefVec4 *out, RefVec4 *in) {
    ee_lq(T0, in);                         /* 11FCF0  lq t0, 0x0(a1) */
    ee_sq(T0, out);                        /* 11FCF8  sq t0, 0x0(a0) */
}

/* 0x11FD00: x, y, z copied one at a time. */
void Ref_IVec3_Copy(RefVec4 *out, RefVec4 *in) {
    out->u[X] = in->u[X];
    out->u[Y] = in->u[Y];
    out->u[Z] = in->u[Z];
}

/* 0x11FD20: out = (float)in / 4096, four components. */
void Ref_IVec4_ToFloat12(RefVec4 *out, RefVec4 *in) {
    op_lqc2(4, in);                        /* 11FD20  lqc2 vf4, 0x0(a1) */
    op_vitof12(M_XYZW, 5, 4);              /* 11FD24  vitof12.xyzw vf5, vf4 */
    op_sqc2(5, out);                       /* 11FD2C  sqc2 vf5, 0x0(a0) */
}

/* 0x11FD30: out = (float)in / 16, four components. */
void Ref_IVec4_ToFloat4(RefVec4 *out, RefVec4 *in) {
    op_lqc2(4, in);                        /* 11FD30  lqc2 vf4, 0x0(a1) */
    op_vitof4(M_XYZW, 5, 4);               /* 11FD34  vitof4.xyzw vf5, vf4 */
    op_sqc2(5, out);                       /* 11FD3C  sqc2 vf5, 0x0(a0) */
}

/* 0x11FD40: out = (float)in, four components. */
void Ref_IVec4_ToFloat(RefVec4 *out, RefVec4 *in) {
    op_lqc2(4, in);                        /* 11FD40  lqc2 vf4, 0x0(a1) */
    op_vitof0(M_XYZW, 5, 4);               /* 11FD44  vitof0.xyzw vf5, vf4 */
    op_sqc2(5, out);                       /* 11FD4C  sqc2 vf5, 0x0(a0) */
}

static int32_t iclamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (hi < v ? hi : v);
}

/* 0x11FD50: out = in < lo ? lo : (hi < in ? hi : in), four components, signed. lo wins when lo > hi. */
void Ref_IVec4_Clamp(RefVec4 *out, RefVec4 *in, int32_t lo, int32_t hi) {
    out->i[X] = iclamp(in->i[X], lo, hi);
    out->i[Y] = iclamp(in->i[Y], lo, hi);
    out->i[Z] = iclamp(in->i[Z], lo, hi);
    out->i[W] = iclamp(in->i[W], lo, hi);
}

/* 0x11FDF0: x, y, z clamped; out.w = in.w (the other IVec3 routines leave out.w alone). */
void Ref_IVec3_Clamp(RefVec4 *out, RefVec4 *in, int32_t lo, int32_t hi) {
    out->i[X] = iclamp(in->i[X], lo, hi);
    out->i[Y] = iclamp(in->i[Y], lo, hi);
    out->i[Z] = iclamp(in->i[Z], lo, hi);
    out->i[W] = in->i[W];
}

/* 0x11FE78: empty. */
void Ref_IVec_Stub(void) {
}

/* ================================================================================================================
 * 0x11FE80..0x11FFE8: screen-range tests
 * ============================================================================================================== */

/* 0x11FE80: p = (x, y in 12.4 fixed point, z, w integers). fx = x / 16, fy = y / 16, fw = (float)w.
 * Returns 1 when none of fx - 0, fy - 0, fw - 0, 4096 - fx, 4096 - fy is zero or negative, else 0.
 * z is converted but not tested. Leaves vf4 = 0, vf5 = (4096, 4096, $t0 residue), vf7 = the converted point. */
int32_t Ref_IVec4_InGsRange(RefVec4 *p) {
    uint32_t v0;

    op_lqc2(7, p);                         /* 11FE80  lqc2 vf7, 0x0(a0) */
    op_vsub(M_XYZW, 4, 0, 0);              /* 11FE84  vsub.xyzw vf4, vf0, vf0 */
    ee_set64(T0, 0x45800000u, 0x45800000u); /* 11FE88  lui t0, 0x4580 */
    op_qmtc2(T0, 5);                       /* 11FE98  qmtc2.ni t0, vf5 */
    op_vitof4(M_XY, 7, 7);                 /* 11FE9C  vitof4.xy vf7, vf7 */
    op_vitof0(M_ZW, 7, 7);                 /* 11FEA0  vitof0.zw vf7, vf7 */
    gRefVu0.status = 0;                    /* 11FEA4  ctc2.ni zero, vi16 */
    op_vsub(M_XYW, 0, 7, 4);               /* 11FEA8  vsub.xyw vf0, vf7, vf4 */
    op_vsub(M_XY, 0, 5, 7);                /* 11FEAC  vsub.xy vf0, vf5, vf7 */
    v0 = gRefVu0.status;                   /* 11FEC4  cfc2.ni v0, vi16 */
    v0 &= 0xC0;                            /* 11FEC8  andi v0, v0, 0xC0 */
    return v0 < 1;                         /* 11FED0  sltiu v0, v0, 0x1 */
}

/* 0x11FED8: the same test on three points; 1 only when all pass. */
int32_t Ref_IVec4_InGsRange3(RefVec4 *a, RefVec4 *b, RefVec4 *c) {
    uint32_t v0;

    op_lqc2(7, a);                         /* 11FED8  lqc2 vf7, 0x0(a0) */
    op_lqc2(8, b);                         /* 11FEDC  lqc2 vf8, 0x0(a1) */
    op_lqc2(9, c);                         /* 11FEE0  lqc2 vf9, 0x0(a2) */
    op_vsub(M_XYZW, 4, 0, 0);              /* 11FEE4  vsub.xyzw vf4, vf0, vf0 */
    ee_set64(T0, 0x45800000u, 0x45800000u); /* 11FEE8  lui t0, 0x4580 */
    op_qmtc2(T0, 5);                       /* 11FEF8  qmtc2.ni t0, vf5 */
    op_vitof4(M_XY, 7, 7);                 /* 11FEFC  vitof4.xy vf7, vf7 */
    op_vitof0(M_ZW, 7, 7);                 /* 11FF00  vitof0.zw vf7, vf7 */
    op_vitof4(M_XY, 8, 8);                 /* 11FF04  vitof4.xy vf8, vf8 */
    op_vitof0(M_ZW, 8, 8);                 /* 11FF08  vitof0.zw vf8, vf8 */
    op_vitof4(M_XY, 9, 9);                 /* 11FF0C  vitof4.xy vf9, vf9 */
    op_vitof0(M_ZW, 9, 9);                 /* 11FF10  vitof0.zw vf9, vf9 */
    gRefVu0.status = 0;                    /* 11FF14  ctc2.ni zero, vi16 */
    op_vsub(M_XYW, 0, 7, 4);               /* 11FF18  vsub.xyw vf0, vf7, vf4 */
    op_vsub(M_XY, 0, 5, 7);                /* 11FF1C  vsub.xy vf0, vf5, vf7 */
    op_vsub(M_XYW, 0, 8, 4);               /* 11FF20  vsub.xyw vf0, vf8, vf4 */
    op_vsub(M_XY, 0, 5, 8);                /* 11FF24  vsub.xy vf0, vf5, vf8 */
    op_vsub(M_XYW, 0, 9, 4);               /* 11FF28  vsub.xyw vf0, vf9, vf4 */
    op_vsub(M_XY, 0, 5, 9);                /* 11FF2C  vsub.xy vf0, vf5, vf9 */
    v0 = gRefVu0.status;                   /* 11FF44  cfc2.ni v0, vi16 */
    v0 &= 0xC0;                            /* 11FF48  andi v0, v0, 0xC0 */
    return v0 < 1;                         /* 11FF50  sltiu v0, v0, 0x1 */
}

/* 0x11FF58: meant for four points, but the fourth register is loaded from c again: d is never read, c is tested
 * twice (ORIGINAL BUG, reproduced). */
int32_t Ref_IVec4_InGsRange4(RefVec4 *a, RefVec4 *b, RefVec4 *c, RefVec4 *d) {
    uint32_t v0;

    (void)d;
    op_lqc2(7, a);                         /* 11FF58  lqc2 vf7, 0x0(a0) */
    op_lqc2(8, b);                         /* 11FF5C  lqc2 vf8, 0x0(a1) */
    op_lqc2(9, c);                         /* 11FF60  lqc2 vf9, 0x0(a2) */
    op_lqc2(10, c);                        /* 11FF64  lqc2 vf10, 0x0(a2) */
    op_vsub(M_XYZW, 4, 0, 0);              /* 11FF68  vsub.xyzw vf4, vf0, vf0 */
    ee_set64(T0, 0x45800000u, 0x45800000u); /* 11FF6C  lui t0, 0x4580 */
    op_qmtc2(T0, 5);                       /* 11FF7C  qmtc2.ni t0, vf5 */
    op_vitof4(M_XY, 7, 7);                 /* 11FF80  vitof4.xy vf7, vf7 */
    op_vitof0(M_ZW, 7, 7);                 /* 11FF84  vitof0.zw vf7, vf7 */
    op_vitof4(M_XY, 8, 8);                 /* 11FF88  vitof4.xy vf8, vf8 */
    op_vitof0(M_ZW, 8, 8);                 /* 11FF8C  vitof0.zw vf8, vf8 */
    op_vitof4(M_XY, 9, 9);                 /* 11FF90  vitof4.xy vf9, vf9 */
    op_vitof0(M_ZW, 9, 9);                 /* 11FF94  vitof0.zw vf9, vf9 */
    op_vitof4(M_XY, 10, 10);               /* 11FF98  vitof4.xy vf10, vf10 */
    op_vitof0(M_ZW, 10, 10);               /* 11FF9C  vitof0.zw vf10, vf10 */
    gRefVu0.status = 0;                    /* 11FFA0  ctc2.ni zero, vi16 */
    op_vsub(M_XYW, 0, 7, 4);               /* 11FFA4  vsub.xyw vf0, vf7, vf4 */
    op_vsub(M_XY, 0, 5, 7);                /* 11FFA8  vsub.xy vf0, vf5, vf7 */
    op_vsub(M_XYW, 0, 8, 4);               /* 11FFAC  vsub.xyw vf0, vf8, vf4 */
    op_vsub(M_XY, 0, 5, 8);                /* 11FFB0  vsub.xy vf0, vf5, vf8 */
    op_vsub(M_XYW, 0, 9, 4);               /* 11FFB4  vsub.xyw vf0, vf9, vf4 */
    op_vsub(M_XY, 0, 5, 9);                /* 11FFB8  vsub.xy vf0, vf5, vf9 */
    op_vsub(M_XYW, 0, 10, 4);              /* 11FFBC  vsub.xyw vf0, vf10, vf4 */
    op_vsub(M_XY, 0, 5, 10);               /* 11FFC0  vsub.xy vf0, vf5, vf10 */
    v0 = gRefVu0.status;                   /* 11FFD8  cfc2.ni v0, vi16 */
    v0 &= 0xC0;                            /* 11FFDC  andi v0, v0, 0xC0 */
    return v0 < 1;                         /* 11FFE4  sltiu v0, v0, 0x1 */
}

/* ================================================================================================================
 * 0x11FFE8..0x120098: sine / cosine, unit vectors
 * ============================================================================================================== */

/* gVu0SinCoef 0x2EC250: coefficients of x^9, x^7, x^5, x^3. gVu0HalfPi 0x2FC328. */
static const RefVec4 sSinCoef = {{0x362E9C14u, 0xB94FB21Fu, 0x3C08873Eu, 0xBE2AAAA4u}};
#define HALF_PI_BITS 0x3FC90FDBu

/*
 * 0x11FFE8 Vu0_SinCos (internal; angle in $f12, result in vf12).
 *   neg = angle < 0;  u = neg ? pi/2 + angle : pi/2 - angle        (FPU add.s / sub.s)
 *   u2 = u * u
 *   t3 = (c3 * u) * u2;  t5 = ((c5 * u) * u2) * u2;  t7 = (((c7 * u) * u2) * u2) * u2;  t9 = one more * u2
 *   c = ((((0 + u) + t3) + t5) + t7) + t9;  c = 0 + c               -> cos(angle), kept in vf12.y
 *   s = vsqrt(1 - c * c);  vf12.x = neg ? 0 - (0 + s) : 0 + (0 + s) -> sin(angle)
 * vf12.z / vf12.w = their old value * 0 (a zero whose sign depends on what vf12 held). Correct only for
 * |angle| <= pi: no wrapping is done. Leaves vf4 = (u2, sign extension of u, $t0 residue, u), vf5 = coefficients,
 * vf6 = (t9, t7, t5, t3), vf7 = (s, ?, ?, 1 - c*c), Q = s.
 */
void Ref_Vu0_SinCos(float angle) {
    uint32_t f12 = f2u(angle);
    uint32_t f0;
    int neg;

    f0 = 0;                                /* 11FFF0  mtc1 zero, f0 */
    neg = RefVu0_LtBits(f12, f0);          /* 11FFF4  c.lt.s f12, f0 */
    f0 = HALF_PI_BITS;                     /* 11FFF8  lwc1 f0, gVu0HalfPi */
    if (neg) {                             /* 11FFFC  bc1f */
        f12 = RefVu0_AddBits(f0, f12);     /* 120004  add.s f12, f0, f12   (t1 = 1) */
    } else {
        f12 = RefVu0_SubBits(f0, f12);     /* 120010  sub.s f12, f0, f12   (t1 = 0) */
    }
    ee_mfc1(T0, f12);                      /* 120018  mfc1 t0, f12 */
    op_qmtc2(T0, 4);                       /* 12001C  qmtc2 t0, vf4 */
    op_lqc2(5, &sSinCoef);                 /* 120020  lqc2 vf5, 0x0(v0) */
    op_vmr32(M_W, 4, 4);                   /* 120024  vmr32.w vf4, vf4 */
    op_vaddbc(M_X, 12, 0, 4, X);           /* 120028  vaddx.x vf12, vf0, vf4x */
    op_vmul(M_X, 4, 4, 4);                 /* 12002C  vmul.x vf4, vf4, vf4 */
    op_vmulbc(M_YZW, 12, 12, 0, X);        /* 120030  vmulx.yzw vf12, vf12, vf0x */
    op_vmulbc(M_XYZW, 6, 5, 4, W);         /* 120034  vmulw.xyzw vf6, vf5, vf4w */
    op_vmulbc(M_XYZW, 6, 6, 4, X);         /* 120038  vmulx.xyzw vf6, vf6, vf4x */
    op_vmulbc(M_XYZ, 6, 6, 4, X);          /* 12003C  vmulx.xyz vf6, vf6, vf4x */
    op_vaddbc(M_X, 12, 12, 6, W);          /* 120040  vaddw.x vf12, vf12, vf6w */
    op_vmulbc(M_XY, 6, 6, 4, X);           /* 120044  vmulx.xy vf6, vf6, vf4x */
    op_vaddbc(M_X, 12, 12, 6, Z);          /* 120048  vaddz.x vf12, vf12, vf6z */
    op_vmulbc(M_X, 6, 6, 4, X);            /* 12004C  vmulx.x vf6, vf6, vf4x */
    op_vaddbc(M_X, 12, 12, 6, Y);          /* 120050  vaddy.x vf12, vf12, vf6y */
    op_vaddbc(M_X, 12, 12, 6, X);          /* 120054  vaddx.x vf12, vf12, vf6x */
    op_vaddbc(M_XY, 12, 0, 12, X);         /* 120058  vaddx.xy vf12, vf0, vf12x */
    op_vmul(M_X, 7, 12, 12);               /* 12005C  vmul.x vf7, vf12, vf12 */
    op_vsubbc(M_W, 7, 0, 7, X);            /* 120060  vsubx.w vf7, vf0, vf7x */
    op_vsqrt(7, W);                        /* 120064  vsqrt Q, vf7w */
    op_vaddq(M_X, 7, 0);                   /* 120070  vaddq.x vf7, vf0, Q */
    if (neg) {                             /* 12006C  bnez t1 */
        op_vsubbc(M_X, 12, 0, 7, X);       /* 12007C  vsubx.x vf12, vf0, vf7x */
    } else {
        op_vaddbc(M_X, 12, 0, 7, X);       /* 120078  vaddx.x vf12, vf0, vf7x */
    }
}

/* 0x120088: vf1 = (0, 0, 1, 0), vf2 = (0, 1, 0, 0), vf3 = (1, 0, 0, 0). Every routine that uses vf1-vf3 (identity,
 * rotations, Vec3_Dot / Length / Normalize through vf3.x) depends on this having run. */
void Ref_Vu0_InitAxisRegs(void) {
    op_vmr32(M_XYZW, 1, 0);                /* 120088  vmr32.xyzw vf1, vf0 */
    op_vmr32(M_XYZW, 2, 1);                /* 12008C  vmr32.xyzw vf2, vf1 */
    op_vmr32(M_XYZW, 3, 2);                /* 120094  vmr32.xyzw vf3, vf2 */
}

/* ================================================================================================================
 * 0x120098..0x120A80: matrices in memory
 * ============================================================================================================== */

/* 0x120098: rows = vf3, vf2, vf1, vf0. */
void Ref_Mtx_StoreIdentity(RefMtx44 *m) {
    op_sqc2(3, m);                         /* 120098  sqc2 vf3, 0x0(a0) */
    op_sqc2(2, AT(m, 0x10));               /* 12009C  sqc2 vf2, 0x10(a0) */
    op_sqc2(1, AT(m, 0x20));               /* 1200A0  sqc2 vf1, 0x20(a0) */
    op_sqc2(0, AT(m, 0x30));               /* 1200A8  sqc2 vf0, 0x30(a0) */
}

/* 0x1200B0: rows 0-2 = vf3, vf2, vf1; row 3 untouched. */
void Ref_Mtx_StoreIdentityRot(RefMtx44 *m) {
    op_sqc2(3, m);                         /* 1200B0  sqc2 vf3, 0x0(a0) */
    op_sqc2(2, AT(m, 0x10));               /* 1200B4  sqc2 vf2, 0x10(a0) */
    op_sqc2(1, AT(m, 0x20));               /* 1200BC  sqc2 vf1, 0x20(a0) */
}

/* 0x1200C0: row 3 = (0, 0, 0, 1). */
void Ref_Mtx_ClearTrans(RefMtx44 *m) {
    op_sqc2(0, AT(m, 0x30));               /* 1200C4  sqc2 vf0, 0x30(a0) */
}

/* 0x1200C8: v = row 3, four words. */
void Ref_Mtx_GetTrans(RefVec4 *v, RefMtx44 *m) {
    ee_lq(T0, AT(m, 0x30));                /* 1200C8  lq t0, 0x30(a1) */
    ee_sq(T0, v);                          /* 1200D0  sq t0, 0x0(a0) */
}

/* 0x1200D8 (compiled C with inline assembly): mt = rows 0-2 of m, row 3 = (0, 0, 0, 1);
 * dir = Mtx_MulVec4(mt, (0, 0, 1, 1)) = ((row0 * 0 + row1 * 0) + row2 * 1) + (0,0,0,1) * 1;
 * angles = Vec3_DirToEuler(dir): the pitch and yaw of the Z axis. */
void Ref_Mtx_AxisZToEuler(RefVec4 *angles, RefMtx44 *m) {
    static const RefVec4 axisZW = {{0u, 0u, 0x3F800000u, 0x3F800000u}}; /* gVu0AxisZW 0x2EC2F0 */
    RefMtx44 mt;
    RefVec4 dir;
    RefVec4 v = axisZW;

    ee_lq(T0, m);                          /* 1200EC  lq t0, 0x0(a1) */
    ee_lq(T1, AT(m, 0x10));                /* 1200F0  lq t1, 0x10(a1) */
    ee_lq(T2, AT(m, 0x20));                /* 1200F4  lq t2, 0x20(a1) */
    ee_sq(T0, &mt);                        /* 1200F8  sq t0, 0x0(sp) */
    ee_sq(T1, AT(&mt, 0x10));              /* 1200FC  sq t1, 0x10(sp) */
    ee_sq(T2, AT(&mt, 0x20));              /* 120100  sq t2, 0x20(sp) */
    op_sqc2(0, AT(&mt, 0x30));             /* 120104  sqc2 vf0, 0x30(sp) */
    b_Mtx_MulVec4(&dir, &mt, &v);          /* 120118  jal Mtx_MulVec4 */
    b_Vec3_DirToEuler(angles, &dir);       /* 120124  jal Vec3_DirToEuler */
}

/* 0x120140: row 3 = v, all four words (so m[3][3] = v.w). */
void Ref_Mtx_SetTrans(RefMtx44 *m, RefVec4 *v) {
    ee_lq(T0, v);                          /* 120140  lq t0, 0x0(a1) */
    ee_sq(T0, AT(m, 0x30));                /* 120148  sq t0, 0x30(a0) */
}

/* 0x120150: dst rows 0-2 = src rows 0-2 (stored before v and src row 3 are read); dst row 3 = (src row 3 xyz + v
 * xyz, src row 3 w). */
void Ref_Mtx_Translate(RefMtx44 *dst, RefMtx44 *src, RefVec4 *v) {
    ee_lq(T0, src);                        /* 120150  lq t0, 0x0(a1) */
    ee_lq(T1, AT(src, 0x10));              /* 120154  lq t1, 0x10(a1) */
    ee_lq(T2, AT(src, 0x20));              /* 120158  lq t2, 0x20(a1) */
    ee_sq(T0, dst);                        /* 12015C  sq t0, 0x0(a0) */
    ee_sq(T1, AT(dst, 0x10));              /* 120160  sq t1, 0x10(a0) */
    ee_sq(T2, AT(dst, 0x20));              /* 120164  sq t2, 0x20(a0) */
    op_lqc2(4, v);                         /* 120168  lqc2 vf4, 0x0(a2) */
    op_lqc2(5, AT(src, 0x30));             /* 12016C  lqc2 vf5, 0x30(a1) */
    op_vadd(M_XYZ, 5, 5, 4);               /* 120170  vadd.xyz vf5, vf5, vf4 */
    op_sqc2(5, AT(dst, 0x30));             /* 120178  sqc2 vf5, 0x30(a0) */
}

/* 0x120180: dst rows 0-2 = src rows 0-2; dst row 3 = ((row0 * v.x + row1 * v.y) + row2 * v.z) + row3 * v.w, four
 * components. Everything is loaded before anything is stored. */
void Ref_Mtx_TranslateLocal(RefMtx44 *dst, RefMtx44 *src, RefVec4 *v) {
    op_lqc2(8, v);                         /* 120180  lqc2 vf8, 0x0(a2) */
    op_lqc2(4, src);                       /* 120184  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(src, 0x10));             /* 120188  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(src, 0x20));             /* 12018C  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(src, 0x30));             /* 120190  lqc2 vf7, 0x30(a1) */
    op_vmulabc(M_XYZW, 4, 8, X);           /* 120194  vmulax.xyzw ACC, vf4, vf8x */
    op_vmaddabc(M_XYZW, 5, 8, Y);          /* 120198  vmadday.xyzw ACC, vf5, vf8y */
    op_vmaddabc(M_XYZW, 6, 8, Z);          /* 12019C  vmaddaz.xyzw ACC, vf6, vf8z */
    op_vmaddbc(M_XYZW, 7, 7, 8, W);        /* 1201A0  vmaddw.xyzw vf7, vf7, vf8w */
    op_sqc2(4, dst);                       /* 1201A4  sqc2 vf4, 0x0(a0) */
    op_sqc2(5, AT(dst, 0x10));             /* 1201A8  sqc2 vf5, 0x10(a0) */
    op_sqc2(6, AT(dst, 0x20));             /* 1201AC  sqc2 vf6, 0x20(a0) */
    op_sqc2(7, AT(dst, 0x30));             /* 1201B4  sqc2 vf7, 0x30(a0) */
}

/* 0x1201B8: dst.row[i] = ((a.row0 * b[i].x + a.row1 * b[i].y) + a.row2 * b[i].z) + a.row3 * b[i].w for i = 0..3,
 * four components each. Everything is loaded before anything is stored (dst may alias a or b). */
void Ref_Mtx_Mul(RefMtx44 *dst, RefMtx44 *a, RefMtx44 *b) {
    op_lqc2(4, a);                         /* 1201B8  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(a, 0x10));               /* 1201BC  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(a, 0x20));               /* 1201C0  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(a, 0x30));               /* 1201C4  lqc2 vf7, 0x30(a1) */
    op_lqc2(8, b);                         /* 1201C8  lqc2 vf8, 0x0(a2) */
    op_lqc2(9, AT(b, 0x10));               /* 1201CC  lqc2 vf9, 0x10(a2) */
    op_lqc2(10, AT(b, 0x20));              /* 1201D0  lqc2 vf10, 0x20(a2) */
    op_lqc2(11, AT(b, 0x30));              /* 1201D4  lqc2 vf11, 0x30(a2) */
    op_vmulabc(M_XYZW, 4, 8, X);           /* 1201D8  vmulax.xyzw ACC, vf4, vf8x */
    op_vmaddabc(M_XYZW, 5, 8, Y);          /* 1201DC  vmadday.xyzw ACC, vf5, vf8y */
    op_vmaddabc(M_XYZW, 6, 8, Z);          /* 1201E0  vmaddaz.xyzw ACC, vf6, vf8z */
    op_vmaddbc(M_XYZW, 8, 7, 8, W);        /* 1201E4  vmaddw.xyzw vf8, vf7, vf8w */
    op_vmulabc(M_XYZW, 4, 9, X);           /* 1201E8  vmulax.xyzw ACC, vf4, vf9x */
    op_vmaddabc(M_XYZW, 5, 9, Y);          /* 1201EC  vmadday.xyzw ACC, vf5, vf9y */
    op_vmaddabc(M_XYZW, 6, 9, Z);          /* 1201F0  vmaddaz.xyzw ACC, vf6, vf9z */
    op_vmaddbc(M_XYZW, 9, 7, 9, W);        /* 1201F4  vmaddw.xyzw vf9, vf7, vf9w */
    op_vmulabc(M_XYZW, 4, 10, X);          /* 1201F8  vmulax.xyzw ACC, vf4, vf10x */
    op_vmaddabc(M_XYZW, 5, 10, Y);         /* 1201FC  vmadday.xyzw ACC, vf5, vf10y */
    op_vmaddabc(M_XYZW, 6, 10, Z);         /* 120200  vmaddaz.xyzw ACC, vf6, vf10z */
    op_vmaddbc(M_XYZW, 10, 7, 10, W);      /* 120204  vmaddw.xyzw vf10, vf7, vf10w */
    op_vmulabc(M_XYZW, 4, 11, X);          /* 120208  vmulax.xyzw ACC, vf4, vf11x */
    op_vmaddabc(M_XYZW, 5, 11, Y);         /* 12020C  vmadday.xyzw ACC, vf5, vf11y */
    op_vmaddabc(M_XYZW, 6, 11, Z);         /* 120210  vmaddaz.xyzw ACC, vf6, vf11z */
    op_vmaddbc(M_XYZW, 11, 7, 11, W);      /* 120214  vmaddw.xyzw vf11, vf7, vf11w */
    op_sqc2(8, dst);                       /* 120218  sqc2 vf8, 0x0(a0) */
    op_sqc2(9, AT(dst, 0x10));             /* 12021C  sqc2 vf9, 0x10(a0) */
    op_sqc2(10, AT(dst, 0x20));            /* 120220  sqc2 vf10, 0x20(a0) */
    op_sqc2(11, AT(dst, 0x30));            /* 120228  sqc2 vf11, 0x30(a0) */
}

/* 0x120230: four 128-bit moves, all loaded first. */
void Ref_Mtx_Copy(RefMtx44 *dst, RefMtx44 *src) {
    ee_lq(T0, src);                        /* 120230  lq t0, 0x0(a1) */
    ee_lq(T1, AT(src, 0x10));              /* 120234  lq t1, 0x10(a1) */
    ee_lq(T2, AT(src, 0x20));              /* 120238  lq t2, 0x20(a1) */
    ee_lq(T3, AT(src, 0x30));              /* 12023C  lq t3, 0x30(a1) */
    ee_sq(T0, dst);                        /* 120240  sq t0, 0x0(a0) */
    ee_sq(T1, AT(dst, 0x10));              /* 120244  sq t1, 0x10(a0) */
    ee_sq(T2, AT(dst, 0x20));              /* 120248  sq t2, 0x20(a0) */
    ee_sq(T3, AT(dst, 0x30));              /* 120250  sq t3, 0x30(a0) */
}

/* 0x120258: transpose (bit moves only), all loaded first. */
void Ref_Mtx_Transpose(RefMtx44 *dst, RefMtx44 *src) {
    ee_lq(T0, src);                        /* 120258  lq t0, 0x0(a1) */
    ee_lq(T1, AT(src, 0x10));              /* 12025C  lq t1, 0x10(a1) */
    ee_lq(T2, AT(src, 0x20));              /* 120260  lq t2, 0x20(a1) */
    ee_lq(T3, AT(src, 0x30));              /* 120264  lq t3, 0x30(a1) */
    ee_pextlw(T4, T1, T0);                 /* 120268  pextlw t4, t1, t0 */
    ee_pextuw(T5, T1, T0);                 /* 12026C  pextuw t5, t1, t0 */
    ee_pextlw(T6, T3, T2);                 /* 120270  pextlw t6, t3, t2 */
    ee_pextuw(T7, T3, T2);                 /* 120274  pextuw t7, t3, t2 */
    ee_pcpyld(T0, T6, T4);                 /* 120278  pcpyld t0, t6, t4 */
    ee_pcpyud(T1, T4, T6);                 /* 12027C  pcpyud t1, t4, t6 */
    ee_pcpyld(T2, T7, T5);                 /* 120280  pcpyld t2, t7, t5 */
    ee_pcpyud(T3, T5, T7);                 /* 120284  pcpyud t3, t5, t7 */
    ee_sq(T0, dst);                        /* 120288  sq t0, 0x0(a0) */
    ee_sq(T1, AT(dst, 0x10));              /* 12028C  sq t1, 0x10(a0) */
    ee_sq(T2, AT(dst, 0x20));              /* 120290  sq t2, 0x20(a0) */
    ee_sq(T3, AT(dst, 0x30));              /* 120298  sq t3, 0x30(a0) */
}

/* 0x1202A0: inverse of a rotation + translation matrix.
 *   dst.row[j] = (m[0][j], m[1][j], m[2][j], +0)  for j = 0..2   (the w column of src rows 0-2 is dropped)
 *   s = (dst.row0 * t.x + dst.row1 * t.y) + dst.row2 * t.z, with t = src row 3
 *   dst.row3 = (0 - s.x, 0 - s.y, 0 - s.z, t.w)
 * Everything is loaded before anything is stored. */
void Ref_Mtx_InverseRT(RefMtx44 *dst, RefMtx44 *src) {
    ee_lq(T0, src);                        /* 1202A0  lq t0, 0x0(a1) */
    ee_lq(T1, AT(src, 0x10));              /* 1202A4  lq t1, 0x10(a1) */
    ee_lq(T2, AT(src, 0x20));              /* 1202A8  lq t2, 0x20(a1) */
    op_lqc2(4, AT(src, 0x30));             /* 1202AC  lqc2 vf4, 0x30(a1) */
    op_vmove(M_XYZW, 5, 4);                /* 1202B0  vmove.xyzw vf5, vf4 */
    op_vmove(M_XYZ, 4, 0);                 /* 1202B4  vmove.xyz vf4, vf0 */
    op_qmfc2(T3, 4);                       /* 1202B8  qmfc2.ni t3, vf4 */
    ee_pextlw(T4, T1, T0);                 /* 1202BC  pextlw t4, t1, t0 */
    ee_pextuw(T5, T1, T0);                 /* 1202C0  pextuw t5, t1, t0 */
    ee_pextlw(T6, T3, T2);                 /* 1202C4  pextlw t6, t3, t2 */
    ee_pextuw(T7, T3, T2);                 /* 1202C8  pextuw t7, t3, t2 */
    ee_pcpyld(T0, T6, T4);                 /* 1202CC  pcpyld t0, t6, t4 */
    ee_pcpyud(T1, T4, T6);                 /* 1202D0  pcpyud t1, t4, t6 */
    ee_pcpyld(T2, T7, T5);                 /* 1202D4  pcpyld t2, t7, t5 */
    op_qmtc2(T0, 6);                       /* 1202D8  qmtc2.ni t0, vf6 */
    op_qmtc2(T1, 7);                       /* 1202DC  qmtc2.ni t1, vf7 */
    op_qmtc2(T2, 8);                       /* 1202E0  qmtc2.ni t2, vf8 */
    op_vmulabc(M_XYZ, 6, 5, X);            /* 1202E4  vmulax.xyz ACC, vf6, vf5x */
    op_vmaddabc(M_XYZ, 7, 5, Y);           /* 1202E8  vmadday.xyz ACC, vf7, vf5y */
    op_vmaddbc(M_XYZ, 4, 8, 5, Z);         /* 1202EC  vmaddz.xyz vf4, vf8, vf5z */
    op_vsub(M_XYZ, 4, 0, 4);               /* 1202F0  vsub.xyz vf4, vf0, vf4 */
    ee_sq(T0, dst);                        /* 1202F4  sq t0, 0x0(a0) */
    ee_sq(T1, AT(dst, 0x10));              /* 1202F8  sq t1, 0x10(a0) */
    ee_sq(T2, AT(dst, 0x20));              /* 1202FC  sq t2, 0x20(a0) */
    op_sqc2(4, AT(dst, 0x30));             /* 120304  sqc2 vf4, 0x30(a0) */
}

/* 0x120308: (s, c) = Vu0_SinCos(angle); A = (0 + c, 0 + s, 0, 0); B = (0 - s, 0 + c, 0, 0);
 * dst.row[i] = ((A * r.x + B * r.y) + vf1 * r.z) + vf0 * r.w for each row r of src, four components
 * (so x' = x c - y s, y' = x s + y c, z' = z * 1, w' = w * 1, each with its zero terms added in that order).
 * All rows are loaded before any is stored. */
void Ref_Mtx_RotateZ(RefMtx44 *dst, RefMtx44 *src, float angle) {
    Ref_Vu0_SinCos(angle);                 /* 12030C  jal Vu0_SinCos */
    op_lqc2(4, src);                       /* 120318  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(src, 0x10));             /* 12031C  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(src, 0x20));             /* 120320  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(src, 0x30));             /* 120324  lqc2 vf7, 0x30(a1) */
    op_vsub(M_ZW, 8, 0, 0);                /* 120328  vsub.zw vf8, vf0, vf0 */
    op_vaddbc(M_X, 8, 0, 12, Y);           /* 12032C  vaddy.x vf8, vf0, vf12y */
    op_vaddbc(M_Y, 8, 0, 12, X);           /* 120330  vaddx.y vf8, vf0, vf12x */
    op_vsub(M_ZW, 9, 0, 0);                /* 120334  vsub.zw vf9, vf0, vf0 */
    op_vsubbc(M_X, 9, 0, 12, X);           /* 120338  vsubx.x vf9, vf0, vf12x */
    op_vaddbc(M_Y, 9, 0, 12, Y);           /* 12033C  vaddy.y vf9, vf0, vf12y */
    op_vmulabc(M_XYZW, 8, 4, X);           /* 120340  vmulax.xyzw ACC, vf8, vf4x */
    op_vmaddabc(M_XYZW, 9, 4, Y);          /* 120344  vmadday.xyzw ACC, vf9, vf4y */
    op_vmaddabc(M_XYZW, 1, 4, Z);          /* 120348  vmaddaz.xyzw ACC, vf1, vf4z */
    op_vmaddbc(M_XYZW, 4, 0, 4, W);        /* 12034C  vmaddw.xyzw vf4, vf0, vf4w */
    op_vmulabc(M_XYZW, 8, 5, X);           /* 120350  vmulax.xyzw ACC, vf8, vf5x */
    op_vmaddabc(M_XYZW, 9, 5, Y);          /* 120354  vmadday.xyzw ACC, vf9, vf5y */
    op_vmaddabc(M_XYZW, 1, 5, Z);          /* 120358  vmaddaz.xyzw ACC, vf1, vf5z */
    op_vmaddbc(M_XYZW, 5, 0, 5, W);        /* 12035C  vmaddw.xyzw vf5, vf0, vf5w */
    op_vmulabc(M_XYZW, 8, 6, X);           /* 120360  vmulax.xyzw ACC, vf8, vf6x */
    op_vmaddabc(M_XYZW, 9, 6, Y);          /* 120364  vmadday.xyzw ACC, vf9, vf6y */
    op_vmaddabc(M_XYZW, 1, 6, Z);          /* 120368  vmaddaz.xyzw ACC, vf1, vf6z */
    op_vmaddbc(M_XYZW, 6, 0, 6, W);        /* 12036C  vmaddw.xyzw vf6, vf0, vf6w */
    op_vmulabc(M_XYZW, 8, 7, X);           /* 120370  vmulax.xyzw ACC, vf8, vf7x */
    op_vmaddabc(M_XYZW, 9, 7, Y);          /* 120374  vmadday.xyzw ACC, vf9, vf7y */
    op_vmaddabc(M_XYZW, 1, 7, Z);          /* 120378  vmaddaz.xyzw ACC, vf1, vf7z */
    op_vmaddbc(M_XYZW, 7, 0, 7, W);        /* 12037C  vmaddw.xyzw vf7, vf0, vf7w */
    op_sqc2(4, dst);                       /* 120380  sqc2 vf4, 0x0(a0) */
    op_sqc2(5, AT(dst, 0x10));             /* 120384  sqc2 vf5, 0x10(a0) */
    op_sqc2(6, AT(dst, 0x20));             /* 120388  sqc2 vf6, 0x20(a0) */
    op_sqc2(7, AT(dst, 0x30));             /* 120390  sqc2 vf7, 0x30(a0) */
}

/* 0x120398: A = (0, 0 + c, 0 + s, 0); B = (0, 0 - s, 0 + c, 0); dst.row = ((vf3 * r.x + A * r.y) + B * r.z) + vf0 * r.w
 * (y' = y c - z s, z' = y s + z c). */
void Ref_Mtx_RotateX(RefMtx44 *dst, RefMtx44 *src, float angle) {
    Ref_Vu0_SinCos(angle);                 /* 12039C  jal Vu0_SinCos */
    op_lqc2(4, src);                       /* 1203A8  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(src, 0x10));             /* 1203AC  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(src, 0x20));             /* 1203B0  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(src, 0x30));             /* 1203B4  lqc2 vf7, 0x30(a1) */
    op_vsub(M_XW, 8, 0, 0);                /* 1203B8  vsub.xw vf8, vf0, vf0 */
    op_vaddbc(M_Y, 8, 0, 12, Y);           /* 1203BC  vaddy.y vf8, vf0, vf12y */
    op_vaddbc(M_Z, 8, 0, 12, X);           /* 1203C0  vaddx.z vf8, vf0, vf12x */
    op_vsub(M_XW, 9, 0, 0);                /* 1203C4  vsub.xw vf9, vf0, vf0 */
    op_vsubbc(M_Y, 9, 0, 12, X);           /* 1203C8  vsubx.y vf9, vf0, vf12x */
    op_vaddbc(M_Z, 9, 0, 12, Y);           /* 1203CC  vaddy.z vf9, vf0, vf12y */
    op_vmulabc(M_XYZW, 3, 4, X);           /* 1203D0  vmulax.xyzw ACC, vf3, vf4x */
    op_vmaddabc(M_XYZW, 8, 4, Y);          /* 1203D4  vmadday.xyzw ACC, vf8, vf4y */
    op_vmaddabc(M_XYZW, 9, 4, Z);          /* 1203D8  vmaddaz.xyzw ACC, vf9, vf4z */
    op_vmaddbc(M_XYZW, 4, 0, 4, W);        /* 1203DC  vmaddw.xyzw vf4, vf0, vf4w */
    op_vmulabc(M_XYZW, 3, 5, X);           /* 1203E0  vmulax.xyzw ACC, vf3, vf5x */
    op_vmaddabc(M_XYZW, 8, 5, Y);          /* 1203E4  vmadday.xyzw ACC, vf8, vf5y */
    op_vmaddabc(M_XYZW, 9, 5, Z);          /* 1203E8  vmaddaz.xyzw ACC, vf9, vf5z */
    op_vmaddbc(M_XYZW, 5, 0, 5, W);        /* 1203EC  vmaddw.xyzw vf5, vf0, vf5w */
    op_vmulabc(M_XYZW, 3, 6, X);           /* 1203F0  vmulax.xyzw ACC, vf3, vf6x */
    op_vmaddabc(M_XYZW, 8, 6, Y);          /* 1203F4  vmadday.xyzw ACC, vf8, vf6y */
    op_vmaddabc(M_XYZW, 9, 6, Z);          /* 1203F8  vmaddaz.xyzw ACC, vf9, vf6z */
    op_vmaddbc(M_XYZW, 6, 0, 6, W);        /* 1203FC  vmaddw.xyzw vf6, vf0, vf6w */
    op_vmulabc(M_XYZW, 3, 7, X);           /* 120400  vmulax.xyzw ACC, vf3, vf7x */
    op_vmaddabc(M_XYZW, 8, 7, Y);          /* 120404  vmadday.xyzw ACC, vf8, vf7y */
    op_vmaddabc(M_XYZW, 9, 7, Z);          /* 120408  vmaddaz.xyzw ACC, vf9, vf7z */
    op_vmaddbc(M_XYZW, 7, 0, 7, W);        /* 12040C  vmaddw.xyzw vf7, vf0, vf7w */
    op_sqc2(4, dst);                       /* 120410  sqc2 vf4, 0x0(a0) */
    op_sqc2(5, AT(dst, 0x10));             /* 120414  sqc2 vf5, 0x10(a0) */
    op_sqc2(6, AT(dst, 0x20));             /* 120418  sqc2 vf6, 0x20(a0) */
    op_sqc2(7, AT(dst, 0x30));             /* 120420  sqc2 vf7, 0x30(a0) */
}

/* 0x120428: A = (0 + c, 0, 0 - s, 0); B = (0 + s, 0, 0 + c, 0); dst.row = ((A * r.x + vf2 * r.y) + B * r.z) + vf0 * r.w
 * (x' = x c + z s, z' = z c - x s). */
void Ref_Mtx_RotateY(RefMtx44 *dst, RefMtx44 *src, float angle) {
    Ref_Vu0_SinCos(angle);                 /* 12042C  jal Vu0_SinCos */
    op_lqc2(4, src);                       /* 120438  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(src, 0x10));             /* 12043C  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(src, 0x20));             /* 120440  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(src, 0x30));             /* 120444  lqc2 vf7, 0x30(a1) */
    op_vsub(M_YW, 8, 0, 0);                /* 120448  vsub.yw vf8, vf0, vf0 */
    op_vaddbc(M_X, 8, 0, 12, Y);           /* 12044C  vaddy.x vf8, vf0, vf12y */
    op_vsubbc(M_Z, 8, 0, 12, X);           /* 120450  vsubx.z vf8, vf0, vf12x */
    op_vsub(M_YW, 9, 0, 0);                /* 120454  vsub.yw vf9, vf0, vf0 */
    op_vaddbc(M_X, 9, 0, 12, X);           /* 120458  vaddx.x vf9, vf0, vf12x */
    op_vaddbc(M_Z, 9, 0, 12, Y);           /* 12045C  vaddy.z vf9, vf0, vf12y */
    op_vmulabc(M_XYZW, 8, 4, X);           /* 120460  vmulax.xyzw ACC, vf8, vf4x */
    op_vmaddabc(M_XYZW, 2, 4, Y);          /* 120464  vmadday.xyzw ACC, vf2, vf4y */
    op_vmaddabc(M_XYZW, 9, 4, Z);          /* 120468  vmaddaz.xyzw ACC, vf9, vf4z */
    op_vmaddbc(M_XYZW, 4, 0, 4, W);        /* 12046C  vmaddw.xyzw vf4, vf0, vf4w */
    op_vmulabc(M_XYZW, 8, 5, X);           /* 120470  vmulax.xyzw ACC, vf8, vf5x */
    op_vmaddabc(M_XYZW, 2, 5, Y);          /* 120474  vmadday.xyzw ACC, vf2, vf5y */
    op_vmaddabc(M_XYZW, 9, 5, Z);          /* 120478  vmaddaz.xyzw ACC, vf9, vf5z */
    op_vmaddbc(M_XYZW, 5, 0, 5, W);        /* 12047C  vmaddw.xyzw vf5, vf0, vf5w */
    op_vmulabc(M_XYZW, 8, 6, X);           /* 120480  vmulax.xyzw ACC, vf8, vf6x */
    op_vmaddabc(M_XYZW, 2, 6, Y);          /* 120484  vmadday.xyzw ACC, vf2, vf6y */
    op_vmaddabc(M_XYZW, 9, 6, Z);          /* 120488  vmaddaz.xyzw ACC, vf9, vf6z */
    op_vmaddbc(M_XYZW, 6, 0, 6, W);        /* 12048C  vmaddw.xyzw vf6, vf0, vf6w */
    op_vmulabc(M_XYZW, 8, 7, X);           /* 120490  vmulax.xyzw ACC, vf8, vf7x */
    op_vmaddabc(M_XYZW, 2, 7, Y);          /* 120494  vmadday.xyzw ACC, vf2, vf7y */
    op_vmaddabc(M_XYZW, 9, 7, Z);          /* 120498  vmaddaz.xyzw ACC, vf9, vf7z */
    op_vmaddbc(M_XYZW, 7, 0, 7, W);        /* 12049C  vmaddw.xyzw vf7, vf0, vf7w */
    op_sqc2(4, dst);                       /* 1204A0  sqc2 vf4, 0x0(a0) */
    op_sqc2(5, AT(dst, 0x10));             /* 1204A4  sqc2 vf5, 0x10(a0) */
    op_sqc2(6, AT(dst, 0x20));             /* 1204A8  sqc2 vf6, 0x20(a0) */
    op_sqc2(7, AT(dst, 0x30));             /* 1204B0  sqc2 vf7, 0x30(a0) */
}

/* 0x1204B8 (compiled C): Z by angles.z, then X by angles.x, then Y by angles.y. angles.z is read before dst is
 * written, angles.x and angles.y after. */
void Ref_Mtx_RotateZXY(RefMtx44 *dst, RefMtx44 *src, RefVec4 *angles) {
    Ref_Mtx_RotateZ(dst, src, angles->f[Z]);
    Ref_Mtx_RotateX(dst, dst, angles->f[X]);
    Ref_Mtx_RotateY(dst, dst, angles->f[Y]);
}

/* 0x120508 (compiled C): X, then Y, then Z. */
void Ref_Mtx_RotateXYZ(RefMtx44 *dst, RefMtx44 *src, RefVec4 *angles) {
    Ref_Mtx_RotateX(dst, src, angles->f[X]);
    Ref_Mtx_RotateY(dst, dst, angles->f[Y]);
    Ref_Mtx_RotateZ(dst, dst, angles->f[Z]);
}

/* 0x120558: dst = src with m[0][0] * v.x, m[1][1] * v.y, m[2][2] * v.z; every other element copied. Only the
 * diagonal is multiplied (ORIGINAL BEHAVIOUR, reproduced): a scale only when the 3x3 part of src is diagonal. */
void Ref_Mtx_ScaleDiag(RefMtx44 *dst, RefMtx44 *src, RefVec4 *v) {
    op_lqc2(8, v);                         /* 120558  lqc2 vf8, 0x0(a2) */
    op_lqc2(4, src);                       /* 12055C  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(src, 0x10));             /* 120560  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(src, 0x20));             /* 120564  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(src, 0x30));             /* 120568  lqc2 vf7, 0x30(a1) */
    op_vmulbc(M_X, 4, 4, 8, X);            /* 12056C  vmulx.x vf4, vf4, vf8x */
    op_vmulbc(M_Y, 5, 5, 8, Y);            /* 120570  vmuly.y vf5, vf5, vf8y */
    op_vmulbc(M_Z, 6, 6, 8, Z);            /* 120574  vmulz.z vf6, vf6, vf8z */
    op_sqc2(4, dst);                       /* 120578  sqc2 vf4, 0x0(a0) */
    op_sqc2(5, AT(dst, 0x10));             /* 12057C  sqc2 vf5, 0x10(a0) */
    op_sqc2(6, AT(dst, 0x20));             /* 120580  sqc2 vf6, 0x20(a0) */
    op_sqc2(7, AT(dst, 0x30));             /* 120588  sqc2 vf7, 0x30(a0) */
}

/* 0x120590: the same with m[0][0] * s, m[1][1] * s, m[2][2] * s. */
void Ref_Mtx_ScaleDiagUniform(RefMtx44 *dst, RefMtx44 *src, float s) {
    uint32_t f12 = f2u(s);

    ee_mfc1(T0, f12);                      /* 120590  mfc1 t0, f12 */
    op_qmtc2(T0, 8);                       /* 120594  qmtc2.ni t0, vf8 */
    op_lqc2(4, src);                       /* 120598  lqc2 vf4, 0x0(a1) */
    op_lqc2(5, AT(src, 0x10));             /* 12059C  lqc2 vf5, 0x10(a1) */
    op_lqc2(6, AT(src, 0x20));             /* 1205A0  lqc2 vf6, 0x20(a1) */
    op_lqc2(7, AT(src, 0x30));             /* 1205A4  lqc2 vf7, 0x30(a1) */
    op_vmulbc(M_X, 4, 4, 8, X);            /* 1205A8  vmulx.x vf4, vf4, vf8x */
    op_vmulbc(M_Y, 5, 5, 8, X);            /* 1205AC  vmulx.y vf5, vf5, vf8x */
    op_vmulbc(M_Z, 6, 6, 8, X);            /* 1205B0  vmulx.z vf6, vf6, vf8x */
    op_sqc2(4, dst);                       /* 1205B4  sqc2 vf4, 0x0(a0) */
    op_sqc2(5, AT(dst, 0x10));             /* 1205B8  sqc2 vf5, 0x10(a0) */
    op_sqc2(6, AT(dst, 0x20));             /* 1205BC  sqc2 vf6, 0x20(a0) */
    op_sqc2(7, AT(dst, 0x30));             /* 1205C4  sqc2 vf7, 0x30(a0) */
}

/* 0x1205C8 (compiled C): Mtx_RotateZXY(dst, src, angles), then Mtx_Translate(dst, SRC, pos), which copies the rows
 * of src over the rotation (ORIGINAL BUG, reproduced; harmless only when dst == src). */
void Ref_Mtx_RotateZXYTranslate(RefMtx44 *dst, RefMtx44 *src, RefVec4 *angles, RefVec4 *pos) {
    Ref_Mtx_RotateZXY(dst, src, angles);
    Ref_Mtx_Translate(dst, src, pos);
}

/* 0x120610 (compiled C; sceVu0CameraMatrix): m0 = identity; xd = ydir x zdir; m0.row0 = normalize(xd);
 * m0.row2 = normalize(zdir); m0.row1 = row2 x row0; m0.row3 += pos (xyz); m = InverseRT(m0). */
void Ref_Mtx_MakeCamera(RefMtx44 *m, RefVec4 *pos, RefVec4 *zdir, RefVec4 *ydir) {
    RefMtx44 m0;
    RefVec4 xd;

    Ref_Mtx_StoreIdentity(&m0);
    b_Vec3_Cross(&xd, ydir, zdir);
    b_Vec3_Normalize(&m0.r[0], &xd);
    b_Vec3_Normalize(&m0.r[2], zdir);
    b_Vec3_Cross(&m0.r[1], &m0.r[2], &m0.r[0]);
    Ref_Mtx_Translate(&m0, &m0, pos);
    Ref_Mtx_InverseRT(m, &m0);
}

/* 0x1206C0 (compiled C; sceVu0LightColorMatrix): four Vec4_Copy, rows 0..3 in order. */
void Ref_Mtx_SetRows(RefMtx44 *m, RefVec4 *r0, RefVec4 *r1, RefVec4 *r2, RefVec4 *r3) {
    b_Vec4_Copy(&m->r[0], r0);
    b_Vec4_Copy(&m->r[1], r1);
    b_Vec4_Copy(&m->r[2], r2);
    b_Vec4_Copy(&m->r[3], r3);
}

/* 0x120728 (compiled C; sceVu0NormalLightMatrix): row i of a temporary = normalize(l_i * -1) (w = 0), row 3 =
 * (0, 0, 0, 1); m = its transpose. */
void Ref_Mtx_MakeNormalLight(RefMtx44 *m, RefVec4 *l0, RefVec4 *l1, RefVec4 *l2) {
    RefMtx44 mt;
    RefVec4 t;
    uint32_t minusOne = 0xBF800000u;

    b_Vec4_Scale(&t, l0, minusOne);
    b_Vec3_Normalize(&mt.r[0], &t);
    b_Vec4_Scale(&t, l1, minusOne);
    b_Vec3_Normalize(&mt.r[1], &t);
    b_Vec4_Scale(&t, l2, minusOne);
    b_Vec3_Normalize(&mt.r[2], &t);
    b_Vec4_SetZeroW1(&mt.r[3]);
    Ref_Mtx_Transpose(m, &mt);
}

/* 0x1207E0 (compiled C, FPU; sceVu0ViewScreenMatrix). The ten float operations in the original's order:
 *   cz = ((-zmax * nearz) + (zmin * farz)) / (farz - nearz)
 *   az = ((farz * nearz) * (zmax - zmin)) / (farz - nearz)
 * mp = identity with [0][0] = [1][1] = scrz, [2][2] = 0, [2][3] = 1, [3][2] = 1, [3][3] = 0;
 * mt = identity with [0][0] = ax, [1][1] = ay, [2][2] = az, row 3 = (cx, cy, cz, 1); m = Mtx_Mul(mt, mp). */
void Ref_Mtx_MakeViewScreen(RefMtx44 *m, float scrz, float ax, float ay, float cx, float cy, float zmin, float zmax,
                            float nearz, float farz) {
    RefMtx44 mp;
    RefMtx44 mt;
    uint32_t f0, f17, f18, f19, f20, f21;

    f17 = f2u(zmin);
    f18 = f2u(zmax);
    f19 = f2u(nearz);
    f20 = f18 ^ SIGN;                      /* 1207E8  neg.s f20, f18 */
    f0 = f2u(farz);                        /* 1207EC  lwc1 f0, 0xD0(sp) */
    f18 = RefVu0_SubBits(f18, f17);        /* 1207F0  sub.s f18, f18, f17 */
    f17 = RefVu0_MulBits(f17, f0);         /* 1207F8  mul.s f17, f17, f0 */
    f21 = RefVu0_MulBits(f0, f19);         /* 120800  mul.s f21, f0, f19 */
    f20 = RefVu0_MulBits(f20, f19);        /* 120808  mul.s f20, f20, f19 */
    f0 = RefVu0_SubBits(f0, f19);          /* 120810  sub.s f0, f0, f19 */
    f21 = RefVu0_MulBits(f21, f18);        /* 12081C  mul.s f21, f21, f18 */
    f20 = RefVu0_AddBits(f20, f17);        /* 120824  add.s f20, f20, f17 */
    f21 = RefVu0_DivBits(f21, f0);         /* 120844  div.s f21, f21, f0   az */
    f20 = RefVu0_DivBits(f20, f0);         /* 12085C  div.s f20, f20, f0   cz */
    Ref_Mtx_StoreIdentity(&mp);
    mp.r[1].f[1] = scrz;
    mp.r[0].f[0] = scrz;
    mp.r[2].u[3] = 0x3F800000u;
    mp.r[2].u[2] = 0;
    mp.r[3].u[3] = 0;
    mp.r[3].u[2] = 0x3F800000u;
    Ref_Mtx_StoreIdentity(&mt);
    mt.r[0].f[0] = ax;
    mt.r[1].f[1] = ay;
    mt.r[2].u[2] = f21;
    mt.r[3].f[0] = cx;
    mt.r[3].f[1] = cy;
    mt.r[3].u[2] = f20;
    Ref_Mtx_Mul(m, &mt, &mp);
}

/* 0x1208F0 (compiled C, FPU only; sceVu0DropShadowMatrix). lp is read completely before m is written.
 * mode != 0 (point light at lp = (x, y, z)):
 *   d = 1 - ((a*x + b*y) + c*z)
 *   m = | a*x + d   a*y       a*z       a     |
 *       | b*x       b*y + d   b*z       b     |
 *       | c*x       c*y       c*z + d   c     |
 *       | -x        -y        -z        d - 1 |
 * mode == 0 (parallel light along lp = (p, q, r)):
 *   n = (a*p + b*q) + c*r;  nr = -1 / n
 *   m = | nr*(a*p - n)  nr*(a*q)      nr*(a*r)      0       |
 *       | nr*(b*p)      nr*(b*q - n)  nr*(b*r)      0       |
 *       | nr*(c*p)      nr*(c*q)      nr*(c*r - n)  0       |
 *       | nr*(-p)       nr*(-q)       nr*(-r)       nr*(-n) |
 * n == 0 divides by zero: nr = -FMAX. */
void Ref_Mtx_MakeDropShadow(RefMtx44 *m, RefVec4 *lp, float fa, float fb, float fc, int32_t mode) {
    uint32_t a = f2u(fa), b = f2u(fb), c = f2u(fc);
    uint32_t one = 0x3F800000u;

    if (mode != 0) {
        uint32_t x = lp->u[X], y = lp->u[Y], z = lp->u[Z];
        uint32_t ax = RefVu0_MulBits(a, x);     /* 120904  mul.s f8, f16, f1 */
        uint32_t by = RefVu0_MulBits(b, y);     /* 12090C  mul.s f7, f15, f2 */
        uint32_t cz = RefVu0_MulBits(c, z);     /* 120918  mul.s f6, f14, f3 */
        uint32_t d, bx, cx, ay, az, cy, bz;

        m->r[0].u[3] = a;
        m->r[1].u[3] = b;
        m->r[2].u[3] = c;
        d = RefVu0_AddBits(ax, by);             /* 120930  add.s f0, f8, f7 */
        bx = RefVu0_MulBits(b, x);              /* 120938  mul.s f11, f15, f1 */
        m->r[3].u[0] = x ^ SIGN;
        cx = RefVu0_MulBits(c, x);              /* 120940  mul.s f1, f14, f1 */
        m->r[3].u[1] = y ^ SIGN;
        d = RefVu0_AddBits(d, cz);              /* 120948  add.s f0, f0, f6 */
        ay = RefVu0_MulBits(a, y);              /* 12094C  mul.s f9, f16, f2 */
        m->r[3].u[2] = z ^ SIGN;
        az = RefVu0_MulBits(a, z);              /* 120954  mul.s f5, f16, f3 */
        m->r[1].u[0] = bx;
        cy = RefVu0_MulBits(c, y);              /* 12095C  mul.s f2, f14, f2 */
        m->r[2].u[0] = cx;
        d = RefVu0_SubBits(one, d);             /* 120964  sub.s f0, f4, f0 */
        bz = RefVu0_MulBits(b, z);              /* 120968  mul.s f3, f15, f3 */
        m->r[0].u[1] = ay;
        m->r[0].u[2] = az;
        m->r[2].u[1] = cy;
        m->r[1].u[2] = bz;
        m->r[3].u[3] = RefVu0_SubBits(d, one);  /* 120978  sub.s f4, f0, f4 */
        m->r[0].u[0] = RefVu0_AddBits(ax, d);   /* 12097C  add.s f8, f8, f0 */
        m->r[1].u[1] = RefVu0_AddBits(by, d);   /* 120984  add.s f7, f7, f0 */
        m->r[2].u[2] = RefVu0_AddBits(cz, d);   /* 120988  add.s f6, f6, f0 */
    } else {
        uint32_t p = lp->u[X], q = lp->u[Y], r = lp->u[Z];
        uint32_t ap = RefVu0_MulBits(a, p);     /* 1209A8  mul.s f5, f16, f2 */
        uint32_t bq = RefVu0_MulBits(b, q);     /* 1209B0  mul.s f6, f15, f3 */
        uint32_t cr = RefVu0_MulBits(c, r);     /* 1209BC  mul.s f8, f14, f7 */
        uint32_t cq, n, ar, br, bp, cp, nr, aq;

        m->r[0].u[3] = 0;
        m->r[1].u[3] = 0;
        cq = RefVu0_MulBits(c, q);              /* 1209CC  mul.s f13, f14, f3 */
        m->r[2].u[3] = 0;
        n = RefVu0_AddBits(ap, bq);             /* 1209D4  add.s f0, f5, f6 */
        ar = RefVu0_MulBits(a, r);              /* 1209DC  mul.s f11, f16, f7 */
        br = RefVu0_MulBits(b, r);              /* 1209E0  mul.s f12, f15, f7 */
        n = RefVu0_AddBits(n, cr);              /* 1209E4  add.s f0, f0, f8 */
        bp = RefVu0_MulBits(b, p);              /* 1209E8  mul.s f4, f15, f2 */
        cp = RefVu0_MulBits(c, p);              /* 1209F0  mul.s f2, f14, f2 */
        nr = RefVu0_DivBits(one ^ SIGN, n);     /* 1209FC  div.s f1, f1, f0 */
        ap = RefVu0_SubBits(ap, n);             /* 120A00  sub.s f5, f5, f0 */
        bq = RefVu0_SubBits(bq, n);             /* 120A04  sub.s f6, f6, f0 */
        cr = RefVu0_SubBits(cr, n);             /* 120A08  sub.s f8, f8, f0 */
        aq = RefVu0_MulBits(a, q);              /* 120A0C  mul.s f3, f16, f3 */
        m->r[0].u[0] = RefVu0_MulBits(nr, ap);          /* 120A14  mul.s f5, f1, f5 */
        m->r[3].u[3] = RefVu0_MulBits(nr, n ^ SIGN);    /* 120A18  mul.s f0, f1, f0 */
        m->r[1].u[0] = RefVu0_MulBits(nr, bp);          /* 120A1C  mul.s f4, f1, f4 */
        m->r[2].u[0] = RefVu0_MulBits(nr, cp);          /* 120A20  mul.s f2, f1, f2 */
        m->r[3].u[0] = RefVu0_MulBits(nr, p ^ SIGN);    /* 120A24  mul.s f9, f1, f9 */
        m->r[0].u[1] = RefVu0_MulBits(nr, aq);          /* 120A2C  mul.s f3, f1, f3 */
        m->r[1].u[1] = RefVu0_MulBits(nr, bq);          /* 120A34  mul.s f6, f1, f6 */
        m->r[2].u[1] = RefVu0_MulBits(nr, cq);          /* 120A3C  mul.s f13, f1, f13 */
        m->r[3].u[1] = RefVu0_MulBits(nr, q ^ SIGN);    /* 120A44  mul.s f10, f1, f10 */
        m->r[0].u[2] = RefVu0_MulBits(nr, ar);          /* 120A4C  mul.s f11, f1, f11 */
        m->r[1].u[2] = RefVu0_MulBits(nr, br);          /* 120A54  mul.s f12, f1, f12 */
        m->r[2].u[2] = RefVu0_MulBits(nr, cr);          /* 120A5C  mul.s f8, f1, f8 */
        m->r[3].u[2] = RefVu0_MulBits(nr, r ^ SIGN);    /* 120A64  mul.s f1, f1, f7 */
    }
}

/* ================================================================================================================
 * 0x120A80..0x121008: the current matrix (vf16..vf19) and its stack
 * ============================================================================================================== */

/* 0x120A80: row 3 = vf0; rows 2, 1, 0 by rotating it (vmr32): identity without needing vf1-vf3. vi15 = 0. */
void Ref_Vu0Cur_Init(void) {
    op_vmove(M_XYZW, 19, 0);               /* 120A80  vmove.xyzw vf19, vf0 */
    op_vmr32(M_XYZW, 18, 19);              /* 120A84  vmr32.xyzw vf18, vf19 */
    op_vmr32(M_XYZW, 17, 18);              /* 120A88  vmr32.xyzw vf17, vf18 */
    op_vmr32(M_XYZW, 16, 17);              /* 120A8C  vmr32.xyzw vf16, vf17 */
    gRefVu0.vi[15] = 0;                    /* 120A94  viaddi vi15, vi0, 0x0 */
}

/* 0x120A98: rows = vf3, vf2, vf1, vf0. */
void Ref_Vu0Cur_LoadIdentity(void) {
    op_vmove(M_XYZW, 16, 3);               /* 120A98  vmove.xyzw vf16, vf3 */
    op_vmove(M_XYZW, 17, 2);               /* 120A9C  vmove.xyzw vf17, vf2 */
    op_vmove(M_XYZW, 18, 1);               /* 120AA0  vmove.xyzw vf18, vf1 */
    op_vmove(M_XYZW, 19, 0);               /* 120AA8  vmove.xyzw vf19, vf0 */
}

/* 0x120AB0: memory[vi15] = row 0, [vi15 + 1] = row 1, [+2] = row 2, [+3] = row 3; vi15 += 4. No overflow check:
 * the address wraps after 64 matrices. */
void Ref_Vu0Cur_Push(void) {
    op_vsqi(16, 15);                       /* 120AB0  vsqi.xyzw vf16, (vi15++) */
    op_vsqi(17, 15);                       /* 120AB4  vsqi.xyzw vf17, (vi15++) */
    op_vsqi(18, 15);                       /* 120AB8  vsqi.xyzw vf18, (vi15++) */
    op_vsqi(19, 15);                       /* 120AC0  vsqi.xyzw vf19, (vi15++) */
}

/* 0x120AC8: row 3 = memory[--vi15], then rows 2, 1, 0. No underflow check. */
void Ref_Vu0Cur_Pop(void) {
    op_vlqd(19, 15);                       /* 120AC8  vlqd.xyzw vf19, (--vi15) */
    op_vlqd(18, 15);                       /* 120ACC  vlqd.xyzw vf18, (--vi15) */
    op_vlqd(17, 15);                       /* 120AD0  vlqd.xyzw vf17, (--vi15) */
    op_vlqd(16, 15);                       /* 120AD8  vlqd.xyzw vf16, (--vi15) */
}

/* 0x120AE0 (compiled C): pops n times (nothing when n <= 0). */
void Ref_Vu0Cur_PopN(int32_t n) {
    int32_t i;

    for (i = 0; i < n; i++) {
        Ref_Vu0Cur_Pop();
    }
}

/* 0x120B18: vi15 = 0. */
void Ref_Vu0Cur_ResetStack(void) {
    gRefVu0.vi[15] = 0;                    /* 120B1C  viaddi vi15, vi0, 0x0 */
}

/* 0x120B20 (compiled C around cfc2): (s16)vi15 / 4, truncating toward zero. */
int32_t Ref_Vu0Cur_GetStackDepth(void) {
    return (int16_t)gRefVu0.vi[15] / 4;
}

/* 0x120B48 (compiled C): depth != 0. */
int32_t Ref_Vu0Cur_IsStackUsed(void) {
    return Ref_Vu0Cur_GetStackDepth() != 0;
}

/* 0x120B68: rows 0-2 = vf3, vf2, vf1. */
void Ref_Vu0Cur_LoadIdentityRot(void) {
    op_vmove(M_XYZW, 16, 3);               /* 120B68  vmove.xyzw vf16, vf3 */
    op_vmove(M_XYZW, 17, 2);               /* 120B6C  vmove.xyzw vf17, vf2 */
    op_vmove(M_XYZW, 18, 1);               /* 120B74  vmove.xyzw vf18, vf1 */
}

/* 0x120B78: row 3 = (0, 0, 0, 1). */
void Ref_Vu0Cur_ClearTrans(void) {
    op_vmove(M_XYZW, 19, 0);               /* 120B7C  vmove.xyzw vf19, vf0 */
}

/* 0x120B80: current = m. */
void Ref_Vu0Cur_LoadMtx(RefMtx44 *m) {
    op_lqc2(16, m);                        /* 120B80  lqc2 vf16, 0x0(a0) */
    op_lqc2(17, AT(m, 0x10));              /* 120B84  lqc2 vf17, 0x10(a0) */
    op_lqc2(18, AT(m, 0x20));              /* 120B88  lqc2 vf18, 0x20(a0) */
    op_lqc2(19, AT(m, 0x30));              /* 120B90  lqc2 vf19, 0x30(a0) */
}

/* 0x120B98: m = current. */
void Ref_Vu0Cur_StoreMtx(RefMtx44 *m) {
    op_sqc2(16, m);                        /* 120B98  sqc2 vf16, 0x0(a0) */
    op_sqc2(17, AT(m, 0x10));              /* 120B9C  sqc2 vf17, 0x10(a0) */
    op_sqc2(18, AT(m, 0x20));              /* 120BA0  sqc2 vf18, 0x20(a0) */
    op_sqc2(19, AT(m, 0x30));              /* 120BA8  sqc2 vf19, 0x30(a0) */
}

/* 0x120BB0: v = row 3. */
void Ref_Vu0Cur_GetTrans(RefVec4 *v) {
    op_sqc2(19, v);                        /* 120BB4  sqc2 vf19, 0x0(a0) */
}

/* 0x120BB8 (compiled C): dir = Mtx_MulVec4(current, (0, 0, 1, 1)) = ((row0 * 0 + row1 * 0) + row2 * 1) + row3 * 1;
 * angles = Vec3_DirToEuler(dir). Row 3 is NOT replaced by (0,0,0,1) as Mtx_AxisZToEuler does, so the translation is
 * added to the axis (ORIGINAL BUG, reproduced; no callers). */
void Ref_Vu0Cur_AxisZToEuler(RefVec4 *angles) {
    static const RefVec4 axisZW = {{0u, 0u, 0x3F800000u, 0x3F800000u}};
    RefVec4 dir;
    RefMtx44 mt;
    RefVec4 v = axisZW;

    Ref_Vu0Cur_StoreMtx(&mt);
    b_Mtx_MulVec4(&dir, &mt, &v);
    b_Vec3_DirToEuler(angles, &dir);
}

/* 0x120C10: row 3 = v, four components. */
void Ref_Vu0Cur_SetTrans(RefVec4 *v) {
    op_lqc2(19, v);                        /* 120C14  lqc2 vf19, 0x0(a0) */
}

/* 0x120C18: row 3 xyz += v xyz; row 3 w untouched. Leaves vf4 = v. */
void Ref_Vu0Cur_Translate(RefVec4 *v) {
    op_lqc2(4, v);                         /* 120C18  lqc2 vf4, 0x0(a0) */
    op_vadd(M_XYZ, 19, 19, 4);             /* 120C20  vadd.xyz vf19, vf19, vf4 */
}

/* 0x120C28: row 3 = ((row0 * v.x + row1 * v.y) + row2 * v.z) + row3 * v.w, four components (v.w is used). */
void Ref_Vu0Cur_TranslateLocal(RefVec4 *v) {
    op_lqc2(4, v);                         /* 120C28  lqc2 vf4, 0x0(a0) */
    op_vmulabc(M_XYZW, 16, 4, X);          /* 120C2C  vmulax.xyzw ACC, vf16, vf4x */
    op_vmaddabc(M_XYZW, 17, 4, Y);         /* 120C30  vmadday.xyzw ACC, vf17, vf4y */
    op_vmaddabc(M_XYZW, 18, 4, Z);         /* 120C34  vmaddaz.xyzw ACC, vf18, vf4z */
    op_vmaddbc(M_XYZW, 19, 19, 4, W);      /* 120C3C  vmaddw.xyzw vf19, vf19, vf4w */
}

/* 0x120C40: old = current; current.row[i] = ((old.row0 * m[i].x + old.row1 * m[i].y) + old.row2 * m[i].z) +
 * old.row3 * m[i].w: Mtx_Mul(current, current, m). */
void Ref_Vu0Cur_MulMtx(RefMtx44 *m) {
    op_lqc2(4, m);                         /* 120C40  lqc2 vf4, 0x0(a0) */
    op_lqc2(5, AT(m, 0x10));               /* 120C44  lqc2 vf5, 0x10(a0) */
    op_lqc2(6, AT(m, 0x20));               /* 120C48  lqc2 vf6, 0x20(a0) */
    op_lqc2(7, AT(m, 0x30));               /* 120C4C  lqc2 vf7, 0x30(a0) */
    op_vmove(M_XYZW, 8, 16);               /* 120C50  vmove.xyzw vf8, vf16 */
    op_vmove(M_XYZW, 9, 17);               /* 120C54  vmove.xyzw vf9, vf17 */
    op_vmove(M_XYZW, 10, 18);              /* 120C58  vmove.xyzw vf10, vf18 */
    op_vmove(M_XYZW, 11, 19);              /* 120C5C  vmove.xyzw vf11, vf19 */
    op_vmulabc(M_XYZW, 8, 4, X);           /* 120C60  vmulax.xyzw ACC, vf8, vf4x */
    op_vmaddabc(M_XYZW, 9, 4, Y);          /* 120C64  vmadday.xyzw ACC, vf9, vf4y */
    op_vmaddabc(M_XYZW, 10, 4, Z);         /* 120C68  vmaddaz.xyzw ACC, vf10, vf4z */
    op_vmaddbc(M_XYZW, 16, 11, 4, W);      /* 120C6C  vmaddw.xyzw vf16, vf11, vf4w */
    op_vmulabc(M_XYZW, 8, 5, X);           /* 120C70  vmulax.xyzw ACC, vf8, vf5x */
    op_vmaddabc(M_XYZW, 9, 5, Y);          /* 120C74  vmadday.xyzw ACC, vf9, vf5y */
    op_vmaddabc(M_XYZW, 10, 5, Z);         /* 120C78  vmaddaz.xyzw ACC, vf10, vf5z */
    op_vmaddbc(M_XYZW, 17, 11, 5, W);      /* 120C7C  vmaddw.xyzw vf17, vf11, vf5w */
    op_vmulabc(M_XYZW, 8, 6, X);           /* 120C80  vmulax.xyzw ACC, vf8, vf6x */
    op_vmaddabc(M_XYZW, 9, 6, Y);          /* 120C84  vmadday.xyzw ACC, vf9, vf6y */
    op_vmaddabc(M_XYZW, 10, 6, Z);         /* 120C88  vmaddaz.xyzw ACC, vf10, vf6z */
    op_vmaddbc(M_XYZW, 18, 11, 6, W);      /* 120C8C  vmaddw.xyzw vf18, vf11, vf6w */
    op_vmulabc(M_XYZW, 8, 7, X);           /* 120C90  vmulax.xyzw ACC, vf8, vf7x */
    op_vmaddabc(M_XYZW, 9, 7, Y);          /* 120C94  vmadday.xyzw ACC, vf9, vf7y */
    op_vmaddabc(M_XYZW, 10, 7, Z);         /* 120C98  vmaddaz.xyzw ACC, vf10, vf7z */
    op_vmaddbc(M_XYZW, 19, 11, 7, W);      /* 120CA0  vmaddw.xyzw vf19, vf11, vf7w */
}

/* 0x120CA8: current.row[i] = ((m.row0 * cur[i].x + m.row1 * cur[i].y) + m.row2 * cur[i].z) + m.row3 * cur[i].w:
 * Mtx_Mul(current, m, current). */
void Ref_Vu0Cur_MulMtxRev(RefMtx44 *m) {
    op_lqc2(4, m);                         /* 120CA8  lqc2 vf4, 0x0(a0) */
    op_lqc2(5, AT(m, 0x10));               /* 120CAC  lqc2 vf5, 0x10(a0) */
    op_lqc2(6, AT(m, 0x20));               /* 120CB0  lqc2 vf6, 0x20(a0) */
    op_lqc2(7, AT(m, 0x30));               /* 120CB4  lqc2 vf7, 0x30(a0) */
    op_vmulabc(M_XYZW, 4, 16, X);          /* 120CB8  vmulax.xyzw ACC, vf4, vf16x */
    op_vmaddabc(M_XYZW, 5, 16, Y);         /* 120CBC  vmadday.xyzw ACC, vf5, vf16y */
    op_vmaddabc(M_XYZW, 6, 16, Z);         /* 120CC0  vmaddaz.xyzw ACC, vf6, vf16z */
    op_vmaddbc(M_XYZW, 16, 7, 16, W);      /* 120CC4  vmaddw.xyzw vf16, vf7, vf16w */
    op_vmulabc(M_XYZW, 4, 17, X);          /* 120CC8  vmulax.xyzw ACC, vf4, vf17x */
    op_vmaddabc(M_XYZW, 5, 17, Y);         /* 120CCC  vmadday.xyzw ACC, vf5, vf17y */
    op_vmaddabc(M_XYZW, 6, 17, Z);         /* 120CD0  vmaddaz.xyzw ACC, vf6, vf17z */
    op_vmaddbc(M_XYZW, 17, 7, 17, W);      /* 120CD4  vmaddw.xyzw vf17, vf7, vf17w */
    op_vmulabc(M_XYZW, 4, 18, X);          /* 120CD8  vmulax.xyzw ACC, vf4, vf18x */
    op_vmaddabc(M_XYZW, 5, 18, Y);         /* 120CDC  vmadday.xyzw ACC, vf5, vf18y */
    op_vmaddabc(M_XYZW, 6, 18, Z);         /* 120CE0  vmaddaz.xyzw ACC, vf6, vf18z */
    op_vmaddbc(M_XYZW, 18, 7, 18, W);      /* 120CE4  vmaddw.xyzw vf18, vf7, vf18w */
    op_vmulabc(M_XYZW, 4, 19, X);          /* 120CE8  vmulax.xyzw ACC, vf4, vf19x */
    op_vmaddabc(M_XYZW, 5, 19, Y);         /* 120CEC  vmadday.xyzw ACC, vf5, vf19y */
    op_vmaddabc(M_XYZW, 6, 19, Z);         /* 120CF0  vmaddaz.xyzw ACC, vf6, vf19z */
    op_vmaddbc(M_XYZW, 19, 7, 19, W);      /* 120CF8  vmaddw.xyzw vf19, vf7, vf19w */
}

/* 0x120D00: transpose in place (bit moves). */
void Ref_Vu0Cur_Transpose(void) {
    op_qmfc2(T0, 16);                      /* 120D00  qmfc2.ni t0, vf16 */
    op_qmfc2(T1, 17);                      /* 120D04  qmfc2.ni t1, vf17 */
    op_qmfc2(T2, 18);                      /* 120D08  qmfc2.ni t2, vf18 */
    op_qmfc2(T3, 19);                      /* 120D0C  qmfc2.ni t3, vf19 */
    ee_pextlw(T4, T1, T0);                 /* 120D10  pextlw t4, t1, t0 */
    ee_pextuw(T5, T1, T0);                 /* 120D14  pextuw t5, t1, t0 */
    ee_pextlw(T6, T3, T2);                 /* 120D18  pextlw t6, t3, t2 */
    ee_pextuw(T7, T3, T2);                 /* 120D1C  pextuw t7, t3, t2 */
    ee_pcpyld(T0, T6, T4);                 /* 120D20  pcpyld t0, t6, t4 */
    ee_pcpyud(T1, T4, T6);                 /* 120D24  pcpyud t1, t4, t6 */
    ee_pcpyld(T2, T7, T5);                 /* 120D28  pcpyld t2, t7, t5 */
    ee_pcpyud(T3, T5, T7);                 /* 120D2C  pcpyud t3, t5, t7 */
    op_qmtc2(T0, 16);                      /* 120D30  qmtc2.ni t0, vf16 */
    op_qmtc2(T1, 17);                      /* 120D34  qmtc2.ni t1, vf17 */
    op_qmtc2(T2, 18);                      /* 120D38  qmtc2.ni t2, vf18 */
    op_qmtc2(T3, 19);                      /* 120D40  qmtc2.ni t3, vf19 */
}

/* 0x120D48: Mtx_InverseRT in place (same arithmetic). */
void Ref_Vu0Cur_InverseRT(void) {
    op_qmfc2(T0, 16);                      /* 120D48  qmfc2.ni t0, vf16 */
    op_qmfc2(T1, 17);                      /* 120D4C  qmfc2.ni t1, vf17 */
    op_qmfc2(T2, 18);                      /* 120D50  qmfc2.ni t2, vf18 */
    op_vmove(M_XYZW, 4, 19);               /* 120D54  vmove.xyzw vf4, vf19 */
    op_vmove(M_XYZ, 4, 0);                 /* 120D58  vmove.xyz vf4, vf0 */
    op_qmfc2(T3, 4);                       /* 120D5C  qmfc2.ni t3, vf4 */
    ee_pextlw(T4, T1, T0);                 /* 120D60  pextlw t4, t1, t0 */
    ee_pextuw(T5, T1, T0);                 /* 120D64  pextuw t5, t1, t0 */
    ee_pextlw(T6, T3, T2);                 /* 120D68  pextlw t6, t3, t2 */
    ee_pextuw(T7, T3, T2);                 /* 120D6C  pextuw t7, t3, t2 */
    ee_pcpyld(T0, T6, T4);                 /* 120D70  pcpyld t0, t6, t4 */
    ee_pcpyud(T1, T4, T6);                 /* 120D74  pcpyud t1, t4, t6 */
    ee_pcpyld(T2, T7, T5);                 /* 120D78  pcpyld t2, t7, t5 */
    op_qmtc2(T0, 6);                       /* 120D7C  qmtc2.ni t0, vf6 */
    op_qmtc2(T1, 7);                       /* 120D80  qmtc2.ni t1, vf7 */
    op_qmtc2(T2, 8);                       /* 120D84  qmtc2.ni t2, vf8 */
    op_vmulabc(M_XYZ, 6, 19, X);           /* 120D88  vmulax.xyz ACC, vf6, vf19x */
    op_vmaddabc(M_XYZ, 7, 19, Y);          /* 120D8C  vmadday.xyz ACC, vf7, vf19y */
    op_vmaddbc(M_XYZ, 4, 8, 19, Z);        /* 120D90  vmaddz.xyz vf4, vf8, vf19z */
    op_vsub(M_XYZ, 4, 0, 4);               /* 120D94  vsub.xyz vf4, vf0, vf4 */
    op_qmtc2(T0, 16);                      /* 120D98  qmtc2.ni t0, vf16 */
    op_qmtc2(T1, 17);                      /* 120D9C  qmtc2.ni t1, vf17 */
    op_qmtc2(T2, 18);                      /* 120DA0  qmtc2.ni t2, vf18 */
    op_vmove(M_XYZW, 19, 4);               /* 120DA8  vmove.xyzw vf19, vf4 */
}

/* 0x120DB0: Mtx_RotateZ on the current matrix, row by row in place. */
void Ref_Vu0Cur_RotateZ(float angle) {
    Ref_Vu0_SinCos(angle);                 /* 120DB4  jal Vu0_SinCos */
    op_vsub(M_ZW, 8, 0, 0);                /* 120DC0  vsub.zw vf8, vf0, vf0 */
    op_vaddbc(M_X, 8, 0, 12, Y);           /* 120DC4  vaddy.x vf8, vf0, vf12y */
    op_vaddbc(M_Y, 8, 0, 12, X);           /* 120DC8  vaddx.y vf8, vf0, vf12x */
    op_vsub(M_ZW, 9, 0, 0);                /* 120DCC  vsub.zw vf9, vf0, vf0 */
    op_vsubbc(M_X, 9, 0, 12, X);           /* 120DD0  vsubx.x vf9, vf0, vf12x */
    op_vaddbc(M_Y, 9, 0, 12, Y);           /* 120DD4  vaddy.y vf9, vf0, vf12y */
    op_vmulabc(M_XYZW, 8, 16, X);          /* 120DD8  vmulax.xyzw ACC, vf8, vf16x */
    op_vmaddabc(M_XYZW, 9, 16, Y);         /* 120DDC  vmadday.xyzw ACC, vf9, vf16y */
    op_vmaddabc(M_XYZW, 1, 16, Z);         /* 120DE0  vmaddaz.xyzw ACC, vf1, vf16z */
    op_vmaddbc(M_XYZW, 16, 0, 16, W);      /* 120DE4  vmaddw.xyzw vf16, vf0, vf16w */
    op_vmulabc(M_XYZW, 8, 17, X);          /* 120DE8  vmulax.xyzw ACC, vf8, vf17x */
    op_vmaddabc(M_XYZW, 9, 17, Y);         /* 120DEC  vmadday.xyzw ACC, vf9, vf17y */
    op_vmaddabc(M_XYZW, 1, 17, Z);         /* 120DF0  vmaddaz.xyzw ACC, vf1, vf17z */
    op_vmaddbc(M_XYZW, 17, 0, 17, W);      /* 120DF4  vmaddw.xyzw vf17, vf0, vf17w */
    op_vmulabc(M_XYZW, 8, 18, X);          /* 120DF8  vmulax.xyzw ACC, vf8, vf18x */
    op_vmaddabc(M_XYZW, 9, 18, Y);         /* 120DFC  vmadday.xyzw ACC, vf9, vf18y */
    op_vmaddabc(M_XYZW, 1, 18, Z);         /* 120E00  vmaddaz.xyzw ACC, vf1, vf18z */
    op_vmaddbc(M_XYZW, 18, 0, 18, W);      /* 120E04  vmaddw.xyzw vf18, vf0, vf18w */
    op_vmulabc(M_XYZW, 8, 19, X);          /* 120E08  vmulax.xyzw ACC, vf8, vf19x */
    op_vmaddabc(M_XYZW, 9, 19, Y);         /* 120E0C  vmadday.xyzw ACC, vf9, vf19y */
    op_vmaddabc(M_XYZW, 1, 19, Z);         /* 120E10  vmaddaz.xyzw ACC, vf1, vf19z */
    op_vmaddbc(M_XYZW, 19, 0, 19, W);      /* 120E18  vmaddw.xyzw vf19, vf0, vf19w */
}

/* 0x120E20: Mtx_RotateX on the current matrix. */
void Ref_Vu0Cur_RotateX(float angle) {
    Ref_Vu0_SinCos(angle);                 /* 120E24  jal Vu0_SinCos */
    op_vsub(M_XW, 8, 0, 0);                /* 120E30  vsub.xw vf8, vf0, vf0 */
    op_vaddbc(M_Y, 8, 0, 12, Y);           /* 120E34  vaddy.y vf8, vf0, vf12y */
    op_vaddbc(M_Z, 8, 0, 12, X);           /* 120E38  vaddx.z vf8, vf0, vf12x */
    op_vsub(M_XW, 9, 0, 0);                /* 120E3C  vsub.xw vf9, vf0, vf0 */
    op_vsubbc(M_Y, 9, 0, 12, X);           /* 120E40  vsubx.y vf9, vf0, vf12x */
    op_vaddbc(M_Z, 9, 0, 12, Y);           /* 120E44  vaddy.z vf9, vf0, vf12y */
    op_vmulabc(M_XYZW, 3, 16, X);          /* 120E48  vmulax.xyzw ACC, vf3, vf16x */
    op_vmaddabc(M_XYZW, 8, 16, Y);         /* 120E4C  vmadday.xyzw ACC, vf8, vf16y */
    op_vmaddabc(M_XYZW, 9, 16, Z);         /* 120E50  vmaddaz.xyzw ACC, vf9, vf16z */
    op_vmaddbc(M_XYZW, 16, 0, 16, W);      /* 120E54  vmaddw.xyzw vf16, vf0, vf16w */
    op_vmulabc(M_XYZW, 3, 17, X);          /* 120E58  vmulax.xyzw ACC, vf3, vf17x */
    op_vmaddabc(M_XYZW, 8, 17, Y);         /* 120E5C  vmadday.xyzw ACC, vf8, vf17y */
    op_vmaddabc(M_XYZW, 9, 17, Z);         /* 120E60  vmaddaz.xyzw ACC, vf9, vf17z */
    op_vmaddbc(M_XYZW, 17, 0, 17, W);      /* 120E64  vmaddw.xyzw vf17, vf0, vf17w */
    op_vmulabc(M_XYZW, 3, 18, X);          /* 120E68  vmulax.xyzw ACC, vf3, vf18x */
    op_vmaddabc(M_XYZW, 8, 18, Y);         /* 120E6C  vmadday.xyzw ACC, vf8, vf18y */
    op_vmaddabc(M_XYZW, 9, 18, Z);         /* 120E70  vmaddaz.xyzw ACC, vf9, vf18z */
    op_vmaddbc(M_XYZW, 18, 0, 18, W);      /* 120E74  vmaddw.xyzw vf18, vf0, vf18w */
    op_vmulabc(M_XYZW, 3, 19, X);          /* 120E78  vmulax.xyzw ACC, vf3, vf19x */
    op_vmaddabc(M_XYZW, 8, 19, Y);         /* 120E7C  vmadday.xyzw ACC, vf8, vf19y */
    op_vmaddabc(M_XYZW, 9, 19, Z);         /* 120E80  vmaddaz.xyzw ACC, vf9, vf19z */
    op_vmaddbc(M_XYZW, 19, 0, 19, W);      /* 120E88  vmaddw.xyzw vf19, vf0, vf19w */
}

/* 0x120E90: Mtx_RotateY on the current matrix. */
void Ref_Vu0Cur_RotateY(float angle) {
    Ref_Vu0_SinCos(angle);                 /* 120E94  jal Vu0_SinCos */
    op_vsub(M_YW, 8, 0, 0);                /* 120EA0  vsub.yw vf8, vf0, vf0 */
    op_vaddbc(M_X, 8, 0, 12, Y);           /* 120EA4  vaddy.x vf8, vf0, vf12y */
    op_vsubbc(M_Z, 8, 0, 12, X);           /* 120EA8  vsubx.z vf8, vf0, vf12x */
    op_vsub(M_YW, 9, 0, 0);                /* 120EAC  vsub.yw vf9, vf0, vf0 */
    op_vaddbc(M_X, 9, 0, 12, X);           /* 120EB0  vaddx.x vf9, vf0, vf12x */
    op_vaddbc(M_Z, 9, 0, 12, Y);           /* 120EB4  vaddy.z vf9, vf0, vf12y */
    op_vmulabc(M_XYZW, 8, 16, X);          /* 120EB8  vmulax.xyzw ACC, vf8, vf16x */
    op_vmaddabc(M_XYZW, 2, 16, Y);         /* 120EBC  vmadday.xyzw ACC, vf2, vf16y */
    op_vmaddabc(M_XYZW, 9, 16, Z);         /* 120EC0  vmaddaz.xyzw ACC, vf9, vf16z */
    op_vmaddbc(M_XYZW, 16, 0, 16, W);      /* 120EC4  vmaddw.xyzw vf16, vf0, vf16w */
    op_vmulabc(M_XYZW, 8, 17, X);          /* 120EC8  vmulax.xyzw ACC, vf8, vf17x */
    op_vmaddabc(M_XYZW, 2, 17, Y);         /* 120ECC  vmadday.xyzw ACC, vf2, vf17y */
    op_vmaddabc(M_XYZW, 9, 17, Z);         /* 120ED0  vmaddaz.xyzw ACC, vf9, vf17z */
    op_vmaddbc(M_XYZW, 17, 0, 17, W);      /* 120ED4  vmaddw.xyzw vf17, vf0, vf17w */
    op_vmulabc(M_XYZW, 8, 18, X);          /* 120ED8  vmulax.xyzw ACC, vf8, vf18x */
    op_vmaddabc(M_XYZW, 2, 18, Y);         /* 120EDC  vmadday.xyzw ACC, vf2, vf18y */
    op_vmaddabc(M_XYZW, 9, 18, Z);         /* 120EE0  vmaddaz.xyzw ACC, vf9, vf18z */
    op_vmaddbc(M_XYZW, 18, 0, 18, W);      /* 120EE4  vmaddw.xyzw vf18, vf0, vf18w */
    op_vmulabc(M_XYZW, 8, 19, X);          /* 120EE8  vmulax.xyzw ACC, vf8, vf19x */
    op_vmaddabc(M_XYZW, 2, 19, Y);         /* 120EEC  vmadday.xyzw ACC, vf2, vf19y */
    op_vmaddabc(M_XYZW, 9, 19, Z);         /* 120EF0  vmaddaz.xyzw ACC, vf9, vf19z */
    op_vmaddbc(M_XYZW, 19, 0, 19, W);      /* 120EF8  vmaddw.xyzw vf19, vf0, vf19w */
}

/* 0x120F00 (compiled C): Z by angles.z, X by angles.x, Y by angles.y. */
void Ref_Vu0Cur_RotateZXY(RefVec4 *angles) {
    Ref_Vu0Cur_RotateZ(angles->f[Z]);
    Ref_Vu0Cur_RotateX(angles->f[X]);
    Ref_Vu0Cur_RotateY(angles->f[Y]);
}

/* 0x120F38 (compiled C): X, Y, Z. */
void Ref_Vu0Cur_RotateXYZ(RefVec4 *angles) {
    Ref_Vu0Cur_RotateX(angles->f[X]);
    Ref_Vu0Cur_RotateY(angles->f[Y]);
    Ref_Vu0Cur_RotateZ(angles->f[Z]);
}

/* 0x120F70: m[0][0] *= v.x, m[1][1] *= v.y, m[2][2] *= v.z; nothing else (diagonal only, as Mtx_ScaleDiag). */
void Ref_Vu0Cur_ScaleDiag(RefVec4 *v) {
    op_lqc2(4, v);                         /* 120F70  lqc2 vf4, 0x0(a0) */
    op_vmulbc(M_X, 16, 16, 4, X);          /* 120F74  vmulx.x vf16, vf16, vf4x */
    op_vmulbc(M_Y, 17, 17, 4, Y);          /* 120F78  vmuly.y vf17, vf17, vf4y */
    op_vmulbc(M_Z, 18, 18, 4, Z);          /* 120F80  vmulz.z vf18, vf18, vf4z */
}

/* 0x120F88: m[0][0] *= s, m[1][1] *= s, m[2][2] *= s. */
void Ref_Vu0Cur_ScaleDiagUniform(float s) {
    uint32_t f12 = f2u(s);

    ee_mfc1(T0, f12);                      /* 120F88  mfc1 t0, f12 */
    op_qmtc2(T0, 4);                       /* 120F8C  qmtc2.ni t0, vf4 */
    op_vmulbc(M_X, 16, 16, 4, X);          /* 120F90  vmulx.x vf16, vf16, vf4x */
    op_vmulbc(M_Y, 17, 17, 4, X);          /* 120F94  vmulx.y vf17, vf17, vf4x */
    op_vmulbc(M_Z, 18, 18, 4, X);          /* 120F9C  vmulx.z vf18, vf18, vf4x */
}

/* 0x120FA0 (compiled C). */
void Ref_Vu0Cur_RotateZXYTranslate(RefVec4 *angles, RefVec4 *pos) {
    Ref_Vu0Cur_RotateZXY(angles);
    Ref_Vu0Cur_Translate(pos);
}

/* 0x120FC8: out = ((row0 * v.x + row1 * v.y) + row2 * v.z) + row3 * v.w, four components. */
void Ref_Vu0Cur_MulVec4(RefVec4 *out, RefVec4 *v) {
    op_lqc2(4, v);                         /* 120FC8  lqc2 vf4, 0x0(a1) */
    op_vmulabc(M_XYZW, 16, 4, X);          /* 120FCC  vmulax.xyzw ACC, vf16, vf4x */
    op_vmaddabc(M_XYZW, 17, 4, Y);         /* 120FD0  vmadday.xyzw ACC, vf17, vf4y */
    op_vmaddabc(M_XYZW, 18, 4, Z);         /* 120FD4  vmaddaz.xyzw ACC, vf18, vf4z */
    op_vmaddbc(M_XYZW, 4, 19, 4, W);       /* 120FD8  vmaddw.xyzw vf4, vf19, vf4w */
    op_sqc2(4, out);                       /* 120FE0  sqc2 vf4, 0x0(a0) */
}

/* 0x120FE8: the same sum for x, y, z; out.w = v.w (the register still holds v). */
void Ref_Vu0Cur_MulVec3(RefVec4 *out, RefVec4 *v) {
    op_lqc2(4, v);                         /* 120FE8  lqc2 vf4, 0x0(a1) */
    op_vmulabc(M_XYZW, 16, 4, X);          /* 120FEC  vmulax.xyzw ACC, vf16, vf4x */
    op_vmaddabc(M_XYZW, 17, 4, Y);         /* 120FF0  vmadday.xyzw ACC, vf17, vf4y */
    op_vmaddabc(M_XYZW, 18, 4, Z);         /* 120FF4  vmaddaz.xyzw ACC, vf18, vf4z */
    op_vmaddbc(M_XYZ, 4, 19, 4, W);        /* 120FF8  vmaddw.xyz vf4, vf19, vf4w */
    op_sqc2(4, out);                       /* 121000  sqc2 vf4, 0x0(a0) */
}

/* ================================================================================================================
 * 0x240C68..0x240DB8: StgVu_RotateZ / RotateX / RotateY (hand-written VU0 macro code inside the stage module)
 * ============================================================================================================== */

/* Each multiplies the four rows of the CURRENT matrix (vf16..vf19) by a rotation about one axis, in place:
 *   (s, c) = Vu0_SinCos(angle), in vf12.x / vf12.y; vf8 and vf9 are the two rotated axes, the third is the unit
 *   register (vf1 = z, vf3 = x, vf2 = y); row' = ((P * row.x + Q * row.y) + R * row.z) + vf0 * row.w.
 *   Z: P = vf8 = (c, -s, 0, 0), Q = vf9 = (s, c, 0, 0), R = vf1
 *   X: P = vf3, Q = vf8 = (0, c, -s, 0), R = vf9 = (0, s, c, 0)
 *   Y: P = vf8 = (c, 0, -s, 0), Q = vf2, R = vf9 = (s, 0, c, 0)
 * Transcribed instruction by instruction from asm/cod/140C68.s (the loop is the same four instructions per row). */
void Ref_StgVu_RotateZ(float angle) {
    int r;

    Ref_Vu0_SinCos(angle);
    op_vsub(M_ZW, 8, 0, 0);
    op_vaddbc(M_X, 8, 0, 12, Y);
    op_vsubbc(M_Y, 8, 0, 12, X);
    op_vsub(M_ZW, 9, 0, 0);
    op_vaddbc(M_X, 9, 0, 12, X);
    op_vaddbc(M_Y, 9, 0, 12, Y);
    for (r = 16; r < 20; r++) {
        op_vmulabc(M_XYZW, 8, r, X);
        op_vmaddabc(M_XYZW, 9, r, Y);
        op_vmaddabc(M_XYZW, 1, r, Z);
        op_vmaddbc(M_XYZW, r, 0, r, W);
    }
}

void Ref_StgVu_RotateX(float angle) {
    int r;

    Ref_Vu0_SinCos(angle);
    op_vsub(M_XW, 8, 0, 0);
    op_vaddbc(M_Y, 8, 0, 12, Y);
    op_vsubbc(M_Z, 8, 0, 12, X);
    op_vsub(M_XW, 9, 0, 0);
    op_vaddbc(M_Y, 9, 0, 12, X);
    op_vaddbc(M_Z, 9, 0, 12, Y);
    for (r = 16; r < 20; r++) {
        op_vmulabc(M_XYZW, 3, r, X);
        op_vmaddabc(M_XYZW, 8, r, Y);
        op_vmaddabc(M_XYZW, 9, r, Z);
        op_vmaddbc(M_XYZW, r, 0, r, W);
    }
}

void Ref_StgVu_RotateY(float angle) {
    int r;

    Ref_Vu0_SinCos(angle);
    op_vsub(M_YW, 8, 0, 0);
    op_vaddbc(M_X, 8, 0, 12, Y);
    op_vsubbc(M_Z, 8, 0, 12, X);
    op_vsub(M_YW, 9, 0, 0);
    op_vaddbc(M_X, 9, 0, 12, X);
    op_vaddbc(M_Z, 9, 0, 12, Y);
    for (r = 16; r < 20; r++) {
        op_vmulabc(M_XYZW, 8, r, X);
        op_vmaddabc(M_XYZW, 2, r, Y);
        op_vmaddabc(M_XYZW, 9, r, Z);
        op_vmaddbc(M_XYZW, r, 0, r, W);
    }
}
