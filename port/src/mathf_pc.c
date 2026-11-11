/*
 * PC versions of the five functions of src/sys/mathf.c and src/sys/randf.c that are PS2 assembly (COP1 or VU0).
 * Each performs the original's float operations one by one, in the original order, through the PS2 float model of
 * the vector-library reference (RefVu0_xxxBits, src/port/vu0_a.c), so the results do not depend on the host's
 * float unit. The originals are quoted in the game sources next to their `#ifndef PORT`.
 *
 * NOT verified against a console: see docs/port/README.md (maths harness).
 */
#include <stdint.h>
#include <string.h>
#include "port/vu0_a.h"

extern float gMathfSinCoef[4]; /* x^9, x^7, x^5, x^3 */
extern void srand(unsigned seed);

/* The VU0 R register: the 23-bit shift-register generator behind Rand_Float01. Part of the simulation state. */
uint32_t gPortVu0R = 0x3F800000u;

static uint32_t bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static float from(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

/* Mathf_WrapAngle (0x11F548): range = half + half; while (angle < range) angle += range;
   while (half < angle) angle -= range. */
float Mathf_WrapAngle(float angle, float half) {
    uint32_t a = bits(angle), h = bits(half);
    uint32_t range = RefVu0_AddBits(h, h);
    /* BT4's secondary model chains can reach angles for which subtracting one
       turn rounds back to the same float. Reduce only that large-angle path;
       ordinary battle angles retain the original sequence of PS2 operations. */
    if ((a & 0x7FFFFFFFu) > 0x47800000u) {
        extern float fmodf(float, float);
        a = bits(fmodf(from(a), from(range)));
    }

    while (RefVu0_LtBits(a, range)) {
        a = RefVu0_AddBits(a, range);
    }
    while (RefVu0_LtBits(h, a)) {
        a = RefVu0_SubBits(a, range);
    }
    return from(a);
}

/* Mathf_SinFast (0x11F5C8). vf5 = coefficients (x: x^9 ... w: x^3), vf4.w = x, vf4.x = x * x, vf6.x = x. */
float Mathf_SinFast(float angle) {
    uint32_t x = bits(Mathf_WrapAngle(angle, 3.14159265f));
    uint32_t x2 = RefVu0_MulBits(x, x);            /* vmul.x vf4, vf4, vf4 */
    uint32_t r = x;                                /* vaddx.x vf6, vf0, vf4x */
    uint32_t t[4];
    int i;

    for (i = 0; i < 4; i++) {
        t[i] = RefVu0_MulBits(bits(gMathfSinCoef[i]), x); /* vmulw.xyzw vf7, vf5, vf4w */
        t[i] = RefVu0_MulBits(t[i], x2);                   /* vmulx.xyzw vf7, vf7, vf4x */
    }
    for (i = 0; i < 3; i++) {
        t[i] = RefVu0_MulBits(t[i], x2);           /* vmulx.xyz */
    }
    r = RefVu0_AddBits(r, t[3]);                   /* vaddw.x vf6, vf6, vf7w */
    for (i = 0; i < 2; i++) {
        t[i] = RefVu0_MulBits(t[i], x2);           /* vmulx.xy */
    }
    r = RefVu0_AddBits(r, t[2]);                   /* vaddz.x */
    t[0] = RefVu0_MulBits(t[0], x2);               /* vmulx.x */
    r = RefVu0_AddBits(r, t[1]);                   /* vaddy.x */
    r = RefVu0_AddBits(r, t[0]);                   /* vaddx.x */
    return from(r);
}

/* Mathf_Sqrt (0x11F6D8): vsqrt, square root of the absolute value. */
float Mathf_Sqrt(float x) {
    return from(RefVu0_SqrtBits(bits(x)));
}

/* Rand_SeedFloat (0x11F7D8): vrinit from the float's bits, and libc srand((u32)(seed * 10000000.0f)). */
void Rand_SeedFloat(float seed) {
    float scaled = from(RefVu0_MulBits(bits(seed), bits(10000000.0f)));

    gPortVu0R = 0x3F800000u | (bits(seed) & 0x007FFFFFu);
    /* The PS2 conversion truncates and saturates; the only seed the game passes (0.1234141f) is far from both. */
    srand(scaled >= 4294967296.0f ? 0xFFFFFFFFu : scaled <= 0.0f ? 0u : (unsigned)scaled);
}

static uint32_t rnext(void) {
    uint32_t b = ((gPortVu0R >> 4) ^ (gPortVu0R >> 22)) & 1u;

    gPortVu0R = 0x3F800000u | (((gPortVu0R << 1) | b) & 0x007FFFFFu);
    return gPortVu0R;
}

/* Rand_Float01 (0x11F830): seven steps, re-seed from the value produced, seven steps, minus 1.0. */
float Rand_Float01(void) {
    uint32_t f = 0;
    int i;

    for (i = 0; i < 7; i++) {
        f = rnext();
    }
    gPortVu0R = 0x3F800000u | (f & 0x007FFFFFu);
    for (i = 0; i < 7; i++) {
        f = rnext();
    }
    return from(RefVu0_SubBits(f, 0x3F800000u));
}
