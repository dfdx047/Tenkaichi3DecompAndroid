/*
 * The vector library's heaviest sequence with the processor's own arithmetic, bit for bit what the PS2 number
 * model of softfloat_ps2_inl.h gives. (Compiled WITH hardware floating point, unlike the rest: undefined.py.)
 *
 * Why this is exact. The PS2's add and multiply are the exact result cut off toward zero at 24 bits. A product
 * of two 24-bit mantissas has 48 bits and a sum of two such numbers whose exponents are at most 28 apart has at
 * most 53: both fit a double's 53 bits, so the double operation rounds nothing, whatever the rounding mode, and
 * clearing the double's low 29 mantissa bits is the cut. Every case this reasoning does not cover makes the
 * function give up (return 0), and the caller does the whole sequence the long way: operands the PS2 takes for
 * zero without being zero (exponent 0, mantissa not 0), exponent 255, and any result outside the ordinary range
 * (the PS2 gives +-0 or +-the largest number there). Sums of operands further apart have their own rule (add).
 *
 * port/tools/fastvec_check.c compares it with the long way.
 */
#include <stdint.h>
#include <string.h>

typedef struct Lane { double v; } Lane;

static inline uint64_t bits_of(double x) {
    uint64_t u;
    __builtin_memcpy(&u, &x, sizeof(u));
    return u;
}

/* The exact value cut to 24 bits; flags a result outside the ordinary single range. */
static inline double cut(double x, unsigned *bad) {
    uint64_t u = bits_of(x);
    unsigned e = (unsigned)(u >> 52) & 0x7FF;

    *bad |= (e != 0) & ((e - 897u) > 253u); /* single exponent 1..254 <-> double exponent 897..1150; 0: zero */
    u &= ~(uint64_t)0x1FFFFFFF;
    __builtin_memcpy(&x, &u, sizeof(x));
    return x;
}

static inline double mul(double a, double b, unsigned *bad) {
    return cut(a * b, bad);
}

/* Operands more than 28 exponents apart: the small one lies wholly below the large one's last place, so the
   exact sum cut toward zero is the large one itself (same signs) or the single just below it in size (opposite
   signs): one unit off its last place, which in a double with 24 significant bits is 2^29 off the bit pattern
   (and steps down an exponent correctly when the mantissa was all zero). */
static inline double add(double a, double b, unsigned *bad) {
    uint64_t ua = bits_of(a), ub = bits_of(b), big, u;
    unsigned ea = (unsigned)(ua >> 52) & 0x7FF, eb = (unsigned)(ub >> 52) & 0x7FF, e;
    unsigned far = (ea != 0) & (eb != 0) & ((ea - eb + 28u) > 56u);
    double x;

    big = (ua << 1) > (ub << 1) ? ua : ub;
    big -= ((ua ^ ub) >> 63) << 29;
    u = bits_of(a + b) & ~(uint64_t)0x1FFFFFFF;
    u = far ? big : u;
    e = (unsigned)(u >> 52) & 0x7FF;
    *bad |= (e != 0) & ((e - 897u) > 253u);
    __builtin_memcpy(&x, &u, sizeof(x));
    return x;
}

/* A single's bits as a double; flags what is not an ordinary number or a true zero. */
static inline double wide(uint32_t u, unsigned *bad) {
    float f;
    unsigned e = (u >> 23) & 0xFF;

    *bad |= ((e - 1u) > 253u) & ((u & 0x7FFFFFFFu) != 0);
    __builtin_memcpy(&f, &u, sizeof(f));
    return (double)f;
}

/* ------------------------------------------------------------------------------------------------------------
 * One add, one multiply (softfloat_ps2_inl.h sends the port's truncating adds and multiplies here). Whatever the
 * quick way does not cover is done by that header's own code, included here as it is (integers only).
 * ---------------------------------------------------------------------------------------------------------- */
#include "softfloat_ps2_inl.h"

uint32_t Port_FastMul(uint32_t a, uint32_t b) {
    if ((uint32_t)(EXP(a) - 1) < 254u && (uint32_t)(EXP(b) - 1) < 254u) {
        float fa, fb, f;
        uint64_t u;
        unsigned e;
        double x;
        __builtin_memcpy(&fa, &a, 4);
        __builtin_memcpy(&fb, &b, 4);
        x = (double)fa * (double)fb;
        u = bits_of(x) & ~(uint64_t)0x1FFFFFFF;
        e = (unsigned)(u >> 52) & 0x7FF;
        if (e - 897u <= 253u) {
            __builtin_memcpy(&x, &u, 8);
            f = (float)x;
            __builtin_memcpy(&a, &f, 4);
            return a;
        }
    }
    return mul_core(a, b, 0);
}

uint32_t Port_FastAdd(uint32_t a, uint32_t b) {
    if ((uint32_t)(EXP(a) - 1) < 254u && (uint32_t)(EXP(b) - 1) < 254u && (uint32_t)((int32_t)EXP(a) - (int32_t)EXP(b) + 28) <= 56u) {
        float fa, fb, f;
        uint64_t u;
        unsigned e;
        double x;
        __builtin_memcpy(&fa, &a, 4);
        __builtin_memcpy(&fb, &b, 4);
        x = (double)fa + (double)fb;
        u = bits_of(x) & ~(uint64_t)0x1FFFFFFF;
        e = (unsigned)(u >> 52) & 0x7FF;
        if (e - 897u <= 253u) {
            __builtin_memcpy(&x, &u, 8);
            f = (float)x;
            __builtin_memcpy(&a, &f, 4);
            return a;
        }
    }
    return add_core(a, b, 0);
}

/*
 * per component c: ((r0.c * v.x + r1.c * v.y) + r2.c * v.z) + r3.c * v.w      (mtx_apply of src/port/vu0_b.c)
 * r: four rows of four singles, v: four singles, out: four singles. Returns 1 if out was written.
 */
#ifndef __SSE2__
int Port_FastMtxApply(const uint32_t *r, const uint32_t *v, uint32_t *out) {
    unsigned bad = 0;
    double vx = wide(v[0], &bad), vy = wide(v[1], &bad), vz = wide(v[2], &bad), vw = wide(v[3], &bad);
    double acc[4];
    int c;

    for (c = 0; c < 4; c++) {
        double a = mul(wide(r[c], &bad), vx, &bad);
        a = add(a, mul(wide(r[4 + c], &bad), vy, &bad), &bad);
        a = add(a, mul(wide(r[8 + c], &bad), vz, &bad), &bad);
        a = add(a, mul(wide(r[12 + c], &bad), vw, &bad), &bad);
        acc[c] = a;
    }
    if (bad) {
        return 0;
    }
    for (c = 0; c < 4; c++) {
        float f = (float)acc[c]; /* exact: 24 bits, in range */
        __builtin_memcpy(&out[c], &f, sizeof(f));
    }
    return 1;
}
#else
/*
 * The same with SSE2, two components per register (the functions above, lane by lane; `bad` collects lanes'
 * flags as all-ones masks). The check program runs whichever of the two the compiler flags select.
 */
#include <emmintrin.h>

typedef struct Pair { __m128d lo, hi; } Pair; /* components x y, z w */

#define LOW32 _mm_set_epi32(0, -1, 0, -1)

/* double exponent fields in the low half of each lane */
static inline __m128i expo(__m128i u) {
    return _mm_srli_epi64(_mm_slli_epi64(u, 1), 53);
}

static inline __m128i out_of_range(__m128i u) {
    __m128i e = expo(u);
    __m128i zero = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    __m128i off = _mm_or_si128(_mm_cmplt_epi32(e, _mm_set1_epi32(897)), _mm_cmpgt_epi32(e, _mm_set1_epi32(1150)));
    return _mm_and_si128(_mm_andnot_si128(zero, off), LOW32);
}

static inline __m128d cut2(__m128d x, __m128i *bad) {
    __m128i u = _mm_and_si128(_mm_castpd_si128(x), _mm_set1_epi64x(~(long long)0x1FFFFFFF));
    *bad = _mm_or_si128(*bad, out_of_range(u));
    return _mm_castsi128_pd(u);
}

static inline __m128d add2(__m128d a, __m128d b, __m128i *bad) {
    __m128i ua = _mm_castpd_si128(a), ub = _mm_castpd_si128(b);
    __m128i ea = expo(ua), eb = expo(ub), d = _mm_sub_epi32(ea, eb);
    __m128i both = _mm_andnot_si128(_mm_or_si128(_mm_cmpeq_epi32(ea, _mm_setzero_si128()), _mm_cmpeq_epi32(eb, _mm_setzero_si128())),
                                    _mm_or_si128(_mm_cmpgt_epi32(d, _mm_set1_epi32(28)), _mm_cmplt_epi32(d, _mm_set1_epi32(-28))));
    __m128i far = _mm_shuffle_epi32(both, _MM_SHUFFLE(2, 2, 0, 0)); /* the low half's answer for the whole lane */
    __m128d absmask = _mm_castsi128_pd(_mm_set1_epi64x(0x7FFFFFFFFFFFFFFFll));
    __m128i aBig = _mm_castpd_si128(_mm_cmpgt_pd(_mm_and_pd(a, absmask), _mm_and_pd(b, absmask)));
    __m128i big = _mm_or_si128(_mm_and_si128(aBig, ua), _mm_andnot_si128(aBig, ub));
    __m128i u;

    big = _mm_sub_epi64(big, _mm_slli_epi64(_mm_srli_epi64(_mm_xor_si128(ua, ub), 63), 29));
    u = _mm_and_si128(_mm_castpd_si128(_mm_add_pd(a, b)), _mm_set1_epi64x(~(long long)0x1FFFFFFF));
    u = _mm_or_si128(_mm_and_si128(far, big), _mm_andnot_si128(far, u));
    *bad = _mm_or_si128(*bad, out_of_range(u));
    return _mm_castsi128_pd(u);
}

/* four singles as doubles; flags what is not an ordinary number or a true zero */
static inline Pair wide4(const uint32_t *p, __m128i *bad) {
    __m128i u = _mm_loadu_si128((const __m128i *)p);
    __m128i e = _mm_and_si128(_mm_srli_epi32(u, 23), _mm_set1_epi32(0xFF));
    __m128i edge = _mm_or_si128(_mm_cmpeq_epi32(e, _mm_setzero_si128()), _mm_cmpeq_epi32(e, _mm_set1_epi32(0xFF)));
    __m128i zero = _mm_cmpeq_epi32(_mm_and_si128(u, _mm_set1_epi32(0x7FFFFFFF)), _mm_setzero_si128());
    __m128 f = _mm_castsi128_ps(u);
    Pair r;

    *bad = _mm_or_si128(*bad, _mm_andnot_si128(zero, edge));
    r.lo = _mm_cvtps_pd(f);
    r.hi = _mm_cvtps_pd(_mm_movehl_ps(f, f));
    return r;
}

int Port_FastMtxApply(const uint32_t *r, const uint32_t *v, uint32_t *out) {
    __m128i bad = _mm_setzero_si128();
    Pair vv = wide4(v, &bad), r0 = wide4(r, &bad), r1 = wide4(r + 4, &bad), r2 = wide4(r + 8, &bad), r3 = wide4(r + 12, &bad);
    __m128d vx = _mm_unpacklo_pd(vv.lo, vv.lo), vy = _mm_unpackhi_pd(vv.lo, vv.lo);
    __m128d vz = _mm_unpacklo_pd(vv.hi, vv.hi), vw = _mm_unpackhi_pd(vv.hi, vv.hi);
    __m128d lo, hi;

    lo = cut2(_mm_mul_pd(r0.lo, vx), &bad);
    hi = cut2(_mm_mul_pd(r0.hi, vx), &bad);
    lo = add2(lo, cut2(_mm_mul_pd(r1.lo, vy), &bad), &bad);
    hi = add2(hi, cut2(_mm_mul_pd(r1.hi, vy), &bad), &bad);
    lo = add2(lo, cut2(_mm_mul_pd(r2.lo, vz), &bad), &bad);
    hi = add2(hi, cut2(_mm_mul_pd(r2.hi, vz), &bad), &bad);
    lo = add2(lo, cut2(_mm_mul_pd(r3.lo, vw), &bad), &bad);
    hi = add2(hi, cut2(_mm_mul_pd(r3.hi, vw), &bad), &bad);
    if (_mm_movemask_epi8(bad) != 0) {
        return 0;
    }
    _mm_storeu_ps((float *)out, _mm_movelh_ps(_mm_cvtpd_ps(lo), _mm_cvtpd_ps(hi))); /* exact: 24 bits, in range */
    return 1;
}
#endif
