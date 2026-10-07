/* The core of softfloat_ps2.c (the PS2's add and multiply on bit patterns), as inline functions: the vector
   library's reference (src/port/vu0_a.c, vu0_b.c) includes this too, so that a vector operation does its four
   multiplies in place instead of through three calls each. One definition, so one number model. */
#ifndef PORT_SOFTFLOAT_PS2_INL_H
#define PORT_SOFTFLOAT_PS2_INL_H
#include <stdint.h>
#include <stdlib.h>

#ifndef SIGN
#define SIGN 0x80000000u
#endif
#define FMAX 0x7F7FFFFFu
#define EXP(x) (((x) >> 23) & 0xFF)
#define MANT(x) (((x) & 0x7FFFFF) | 0x800000)

/* An operand with exponent 255 (infinity / NaN patterns) counts as the largest number. */
static inline uint32_t in(uint32_t a) {
    return EXP(a) == 255 ? (a & SIGN) | FMAX : a;
}

/* Packs sign, exponent and a mantissa in [2^23, 2^24); out of range gives +/-FLT_MAX or +/-0. */
static inline uint32_t pack(uint32_t sign, int32_t e, uint32_t m) {
    if (e >= 255) {
        return sign | FMAX;
    }
    if (e <= 0) {
        return sign;
    }
    return sign | ((uint32_t)e << 23) | (m & 0x7FFFFF);
}

/* Rounds a mantissa with 32 extra bits (and "more below" in sticky) to nearest even, renormalising. */
static inline uint32_t round_pack(uint32_t sign, int32_t e, uint64_t m, int sticky, int nearest) {
    uint32_t hi = (uint32_t)(m >> 32);
    uint32_t lo = (uint32_t)m;

    if (nearest && (lo & 0x80000000u) && ((lo & 0x7FFFFFFFu) || sticky || (hi & 1))) {
        hi++;
        if (hi == 0x1000000) {
            hi >>= 1;
            e++;
        }
    }
    return pack(sign, e, hi);
}

/* a + b, exact, then truncated toward zero (nearest = 0) or rounded to nearest even. */
#ifdef SF_FAST_CALLS
/* The port's files: the truncating add and multiply are done by the processor where that is exact
   (port/src/plat_fastvec.c, which is compiled with hardware floating point and falls back to the code here). */
extern uint32_t Port_FastAdd(uint32_t a, uint32_t b);
extern uint32_t Port_FastMul(uint32_t a, uint32_t b);
static inline uint32_t add_core_long(uint32_t a, uint32_t b, int nearest);
static inline uint32_t mul_core_long(uint32_t a, uint32_t b, int nearest);
static inline uint32_t add_core(uint32_t a, uint32_t b, int nearest) {
    return nearest ? add_core_long(a, b, nearest) : Port_FastAdd(a, b);
}
static inline uint32_t mul_core(uint32_t a, uint32_t b, int nearest) {
    return nearest ? mul_core_long(a, b, nearest) : Port_FastMul(a, b);
}
#define add_core add_core_long
#define mul_core mul_core_long
#endif
static inline uint32_t add_core(uint32_t a, uint32_t b, int nearest) {
    uint64_t ma, mb;
    uint32_t sign;
    int32_t e, d;
    int sticky = 0;

    a = in(a);
    b = in(b);
    if (EXP(a) == 0 && EXP(b) == 0) {
        return a & b & SIGN;
    }
    if (EXP(b) == 0) {
        return a;
    }
    if (EXP(a) == 0) {
        return b;
    }
    if ((a & 0x7FFFFFFF) < (b & 0x7FFFFFFF)) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    sign = a & SIGN;
    e = (int32_t)EXP(a);
    d = e - (int32_t)EXP(b);
    ma = (uint64_t)MANT(a) << 32;
    mb = (uint64_t)MANT(b) << 32;
    if ((a ^ b) & SIGN) {
        /* the bits of b shifted out below 2^-32 make the exact difference smaller: one unit there, then truncate */
        uint64_t lost = d > 32;

        sticky = (int)lost;
        mb = d >= 64 ? 0 : mb >> d;
        ma -= mb + lost;
        if (ma == 0) {
            return 0; /* x - x is +0 */
        }
        while (!(ma & ((uint64_t)1 << 55))) {
            ma <<= 1;
            e--;
        }
    } else {
        sticky = d > 32;
        ma += d >= 64 ? 0 : mb >> d;
        if (ma & ((uint64_t)1 << 56)) {
            sticky |= (int)(ma & 1);
            ma >>= 1;
            e++;
        }
    }
    return round_pack(sign, e, ma, sticky, nearest);
}

/* a * b, truncated toward zero or rounded to nearest even. */
static inline uint32_t mul_core(uint32_t a, uint32_t b, int nearest) {
    uint32_t sign = (a ^ b) & SIGN;
    uint64_t m;
    int32_t e;

    a = in(a);
    b = in(b);
    if (EXP(a) == 0 || EXP(b) == 0) {
        return sign;
    }
    m = (uint64_t)MANT(a) * MANT(b);
    e = (int32_t)EXP(a) + (int32_t)EXP(b) - 127;
    if (m & ((uint64_t)1 << 47)) {
        e++;
        m <<= 8;  /* 24 result bits above bit 32 */
    } else {
        m <<= 9;
    }
    return round_pack(sign, e, m, 0, nearest);
}

#ifdef SF_FAST_CALLS
#undef add_core
#undef mul_core
#endif

/* Experiment switches (environment, read once): BT3_VU_NEAREST=1 rounds the vector unit's add / multiply /
   divide to nearest; BT3_FPU_NEAREST=1 does the same for the FPU's add / multiply. Default: toward zero. */
static inline int mode(int which) {
    static const char *const name[4] = {"BT3_VU_NEAREST", "BT3_FPU_NEAREST", "BT3_VU_ADDHACK", "BT3_FPU_NOADDHACK"};
    static int m[4] = {-1, -1, -1, -1};

    if (m[which] < 0) {
        const char *e = getenv(name[which]);
        m[which] = e != NULL && e[0] == '1';
    }
    return m[which];
}
/*
 * The FPU's add.s / sub.s. The PS2 adder keeps only ONE bit of the smaller operand below the larger operand's
 * last place and no sticky bits; PCSX2 reproduces this (its FPU_ADD_SUB step) by clearing the smaller operand's
 * low bits before an ordinary truncating add: exponent difference d of 1..24 clears d - 1 bits, 25 or more
 * leaves only the sign. (The vector unit's adds do not get this treatment there.)
 */
static inline uint32_t fpu_add(uint32_t a, uint32_t b) {
    int32_t d = (int32_t)EXP(a) - (int32_t)EXP(b);

    if (mode(3)) {
        return add_core(a, b, mode(1));
    }
    if (d >= 25) {
        b &= SIGN;
    } else if (d > 0) {
        b &= 0xFFFFFFFFu << (d - 1);
    } else if (d <= -25) {
        a &= SIGN;
    } else if (d < 0) {
        a &= 0xFFFFFFFFu << (-d - 1);
    }
    return add_core(a, b, mode(1));
}

/* the vector unit's add and multiply */
static inline uint32_t Sf_AddBits(uint32_t a, uint32_t b) { return mode(2) ? fpu_add(a, b) : add_core(a, b, mode(0)); }
static inline uint32_t Sf_MulBits(uint32_t a, uint32_t b) { return mul_core(a, b, mode(0)); }

#endif
