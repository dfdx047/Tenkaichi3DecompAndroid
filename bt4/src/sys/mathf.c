#include "common.h"
#include "sys/mathf.h"

/*
 * Scalar float maths helpers, 0x11F548..0x11F7D8.
 *
 * Two families of sine / cosine exist and they do not return the same bits:
 *   Mathf_Sin / Mathf_Cos          wrap the angle, then call libm sinf (cos is sin(angle + pi/2)).
 *   Mathf_SinFast / Mathf_CosFast  wrap the angle, then evaluate a degree-9 odd polynomial on VU0.
 * The wrap (Mathf_WrapAngle) is done by repeated float addition / subtraction of 2*pi, not by fmod, so the result
 * carries one rounding per turn removed, and an angle so large that adding 2*pi does not change it never ends.
 *
 * Mathf_WrapAngle, Mathf_SinFast and Mathf_Sqrt are assembly (VU0 or hand-written) and stay INCLUDE_ASM; an
 * equivalent that assembles to the same bytes is kept above each one.
 */

extern f32 sinf(f32 x);
extern f32 tanf(f32 x);
extern f32 asinf(f32 x);
extern f32 acosf(f32 x);
extern f32 atanf(f32 x);

/* Polynomial coefficients of Mathf_SinFast, highest power first: x^9, x^7, x^5, x^3. */
extern f32 gMathfSinCoef[4];

/*
 * Brings an angle into (-half, half] in steps of 2 * half. Hand-written assembly: the first two instructions are
 * dead (f0 = 0, f0 = -half) and the lower bound actually tested is +2 * half, so what runs is
 *
 *     f32 range = half + half;
 *     while (angle < range) angle += range;
 *     while (half < angle) angle -= range;
 *     return angle;
 *
 * i.e. every angle is first pushed up to at least 2 * half and then brought back down.
 */
#ifndef PORT /* PC build: port/src/mathf_pc.c */
__asm__(
    ".text\n"
    ".align 3\n"
    ".globl Mathf_WrapAngle\n"
    ".type Mathf_WrapAngle, @function\n"
    ".set push\n"
    ".set noreorder\n"
    "Mathf_WrapAngle:\n"
    "    mtc1 $0, $f0\n"
    "    neg.s $f0, $f13\n"
    "    add.s $f0, $f13, $f13\n"
    "1:  c.lt.s $f12, $f0\n"
    "    bc1f 2f\n"
    "    nop\n"
    "    b 1b\n"
    "    add.s $f12, $f12, $f0\n"
    "2:  c.lt.s $f13, $f12\n"
    "    bc1f 3f\n"
    "    nop\n"
    "    b 2b\n"
    "    sub.s $f12, $f12, $f0\n"
    "3:  jr $31\n"
    "    mov.s $f0, $f12\n"
    ".set pop\n"
    ".size Mathf_WrapAngle, . - Mathf_WrapAngle\n"
);
#endif

/* sinf of the angle wrapped to (-pi, pi]. */
f32 Mathf_Sin(f32 angle) {
    return sinf(Mathf_WrapAngle(angle, MATHF_PI));
}

/*
 * Polynomial sine on VU0 of the angle wrapped to (-pi, pi]:
 *     x + c[3] x^3 + c[2] x^5 + c[1] x^7 + c[0] x^9
 * with x^3 formed as (c * x) * x^2 and each further power by one more multiply by x^2; the four terms are added to
 * x in the order x^3, x^5, x^7, x^9. VU0 inline assembly.
 */
#ifndef PORT /* PC build: port/src/mathf_pc.c */
f32 Mathf_SinFast(f32 angle) {
    f32 x = Mathf_WrapAngle(angle, MATHF_PI);
    f32 result;

    __asm__ volatile(
        "lqc2 $vf5, 0(%2)\n"
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vmr32.w $vf4, $vf4\n"
        "vaddx.x $vf6, $vf0, $vf4x\n"
        "vmul.x $vf4, $vf4, $vf4\n"
        "vmulx.yzw $vf6, $vf6, $vf0x\n"
        "vmulw.xyzw $vf7, $vf5, $vf4w\n"
        "vmulx.xyzw $vf7, $vf7, $vf4x\n"
        "vmulx.xyz $vf7, $vf7, $vf4x\n"
        "vaddw.x $vf6, $vf6, $vf7w\n"
        "vmulx.xy $vf7, $vf7, $vf4x\n"
        "vaddz.x $vf6, $vf6, $vf7z\n"
        "vmulx.x $vf7, $vf7, $vf4x\n"
        "vaddy.x $vf6, $vf6, $vf7y\n"
        "vaddx.x $vf6, $vf6, $vf7x\n"
        "qmfc2 $8, $vf6\n"
        "mtc1 $8, %0\n"
        : "=f"(result)
        : "f"(x), "r"(gMathfSinCoef));
    return result;
}
#endif

/* Cosine through libm: Mathf_Sin(angle + pi/2). */
f32 Mathf_Cos(f32 angle) {
    return Mathf_Sin(angle + MATHF_HALF_PI);
}

/* Cosine through the polynomial: Mathf_SinFast(angle + pi/2). */
f32 Mathf_CosFast(f32 angle) {
    return Mathf_SinFast(angle + MATHF_HALF_PI);
}

/* tanf of the angle wrapped to [-pi, pi] in steps of 2*pi. */
f32 Mathf_Tan(f32 angle) {
    while (angle < -MATHF_PI) {
        angle += MATHF_TWO_PI;
    }
    while (angle > MATHF_PI) {
        angle -= MATHF_TWO_PI;
    }
    return tanf(angle);
}

/* Square root on VU0 (vsqrt, result read from the Q register). VU0 assembly. */
#ifndef PORT /* PC build: port/src/mathf_pc.c */
f32 Mathf_Sqrt(f32 x) {
    f32 result;

    __asm__ volatile(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vsqrt $Q, $vf4x\n"
        "vwaitq\n"
        ".word 0x4848B000\n" /* cfc2 $8, $vi22 (written as a word: gas would add a hazard nop behind it) */
        "mtc1 $8, %0\n"
        : "=f"(result)
        : "f"(x));
    return result;
}
#endif

/* out[0] = Mathf_Sin(angle), out[1] = Mathf_Cos(angle). */
void Mathf_SinCos(f32 *out, f32 angle) {
    out[0] = Mathf_Sin(angle);
    out[1] = Mathf_Cos(angle);
}

/* asinf with the argument clamped to [-1, 1]. */
f32 Mathf_Asin(f32 x) {
    return asinf(x < -1.0f ? -1.0f : x > 1.0f ? 1.0f : x);
}

/* acosf with the argument clamped to [-1, 1]. */
f32 Mathf_Acos(f32 x) {
    return acosf(x < -1.0f ? -1.0f : x > 1.0f ? 1.0f : x);
}

/* atanf. */
f32 Mathf_Atan(f32 x) {
    return atanf(x);
}
