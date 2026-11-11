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

/* The long ways, out of line: inlined into the quick functions they made every call save a dozen registers. */
static __attribute__((noinline)) uint32_t long_mul(uint32_t a, uint32_t b) { return mul_core(a, b, 0); }
static __attribute__((noinline)) uint32_t long_add(uint32_t a, uint32_t b) { return add_core(a, b, 0); }

/* The experiment switches (softfloat_ps2_inl.h mode()), read once: 1 when the game's FPU (fpu) or the vector unit
   (vu) must take the long way. */
static int sSwitches = -1;
static __attribute__((noinline)) int read_switches(void) {
    sSwitches = (mode(1) | mode(3)) | (mode(0) | mode(2)) << 1;
    return sSwitches;
}
static inline int switches(void) {
    int s = sSwitches;
    return __builtin_expect(s < 0, 0) ? read_switches() : s;
}

static inline uint32_t fast_mul(uint32_t a, uint32_t b) {
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
    return long_mul(a, b);
}

static inline uint32_t fast_add(uint32_t a, uint32_t b) {
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
    return long_add(a, b);
}

uint32_t Port_FastMul(uint32_t a, uint32_t b) { return fast_mul(a, b); }
uint32_t Port_FastAdd(uint32_t a, uint32_t b) { return fast_add(a, b); }

/* ------------------------------------------------------------------------------------------------------------
 * The game's own float add, subtract and multiply (the calls -msoft-float makes for every one of them). They were
 * in softfloat_ps2.c, which is software float itself and so had to call Port_FastAdd / Port_FastMul in turn: two
 * calls per operation. Here, one. The same results: fpu_add's operand cut, then the quick way or the long way of
 * softfloat_ps2_inl.h; the experiment switches (BT3_FPU_NEAREST, BT3_FPU_NOADDHACK) take the long way as before.
 * softfloat_ps2.c keeps its own definitions for builds without SF_FAST_CALLS (port/tests/ps2float).
 * ---------------------------------------------------------------------------------------------------------- */
static __attribute__((noinline)) uint32_t slow_fpu_add(uint32_t a, uint32_t b) { return fpu_add(a, b); }
static __attribute__((noinline)) uint32_t slow_fpu_mul(uint32_t a, uint32_t b) { return mul_core(a, b, mode(1)); }

static inline __attribute__((always_inline)) uint32_t game_add(uint32_t a, uint32_t b) {
    int32_t d;

    if (__builtin_expect(switches() & 1, 0)) {
        return slow_fpu_add(a, b);
    }
    d = (int32_t)EXP(a) - (int32_t)EXP(b);
    if (d >= 25) {
        b &= SIGN;
    } else if (d > 0) {
        b &= 0xFFFFFFFFu << (d - 1);
    } else if (d <= -25) {
        a &= SIGN;
    } else if (d < 0) {
        a &= 0xFFFFFFFFu << (-d - 1);
    }
    return fast_add(a, b);
}

uint32_t __addsf3(uint32_t a, uint32_t b) { return game_add(a, b); }
uint32_t __subsf3(uint32_t a, uint32_t b) { return game_add(a, b ^ SIGN); }
uint32_t __mulsf3(uint32_t a, uint32_t b) { return __builtin_expect(switches() & 1, 0) ? slow_fpu_mul(a, b) : fast_mul(a, b); }

/* The compiler's vector types, two components per register: NEON on arm64 (Android), SSE2 on x86. Lane by lane
   they are mul / add / wide / cut above; `bad` collects lanes' flags as all-ones masks. */
typedef double V2d __attribute__((vector_size(16)));
typedef uint64_t V2u __attribute__((vector_size(16)));
typedef int64_t V2i __attribute__((vector_size(16)));

static inline V2u v_bits(V2d x) { V2u u; __builtin_memcpy(&u, &x, 16); return u; }
static inline V2d v_dbl(V2u u) { V2d x; __builtin_memcpy(&x, &u, 16); return x; }

/* a lane's exponent field 0 (zero) or 897..1150 (ordinary single): 0 there, all ones otherwise */
static inline V2u v_out_of_range(V2u u) {
    V2u e = (u >> 52) & 0x7FF;
    return (V2u)((e != 0) & ((e - 897u) > 253u));
}

static inline V2d v_cut(V2d x, V2u *bad) {
    V2u u = v_bits(x) & ~(uint64_t)0x1FFFFFFF;
    *bad |= v_out_of_range(u);
    return v_dbl(u);
}

static inline V2d v_mul(V2d a, V2d b, V2u *bad) {
    return v_cut(a * b, bad);
}

static inline V2d v_add(V2d a, V2d b, V2u *bad) {
    V2u ua = v_bits(a), ub = v_bits(b), ea = (ua >> 52) & 0x7FF, eb = (ub >> 52) & 0x7FF;
    V2u far = (V2u)((ea != 0) & (eb != 0) & ((ea - eb + 28u) > 56u));
    V2u aBig = (V2u)((ua << 1) > (ub << 1));
    V2u big = (aBig & ua) | (~aBig & ub), u;

    big -= ((ua ^ ub) >> 63) << 29;
    u = v_bits(a + b) & ~(uint64_t)0x1FFFFFFF;
    u = (far & big) | (~far & u);
    *bad |= v_out_of_range(u);
    return v_dbl(u);
}

/* two singles as doubles; flags what is not an ordinary number or a true zero (one widening instruction: fcvtl) */
typedef float V2f __attribute__((vector_size(8)));
typedef uint32_t V2w __attribute__((vector_size(8)));
static inline V2d v_wide(const uint32_t *p, V2u *bad) {
    V2w w;
    V2f f;
    V2u u, e;

    __builtin_memcpy(&w, p, 8);
    __builtin_memcpy(&f, p, 8);
    u = __builtin_convertvector(w, V2u);
    e = (u >> 23) & 0xFF;
    *bad |= (V2u)(((e - 1u) > 253u) & ((u & 0x7FFFFFFFu) != 0));
    return __builtin_convertvector(f, V2d);
}

static inline V2d v_splat(double x) { return (V2d){x, x}; }

static inline void v_store(uint32_t *out, V2d lo, V2d hi) { /* exact: 24 bits, in range (fcvtn) */
    V2f a = __builtin_convertvector(lo, V2f), b = __builtin_convertvector(hi, V2f);
    __builtin_memcpy(out, &a, 8);
    __builtin_memcpy(out + 2, &b, 8);
}

static inline int v_any(V2u bad) { return (bad[0] | bad[1]) != 0; }

/*
 * per component c: ((r0.c * v.x + r1.c * v.y) + r2.c * v.z) + r3.c * v.w      (mtx_apply of src/port/vu0_b.c)
 * r: four rows of four singles, v: four singles, out: four singles. Returns 1 if out was written.
 */
#if !defined(__SSE2__) || defined(FASTVEC_GENERIC)
/*
 * The same with the compiler's vector types, two components per register: NEON on arm64 (Android), SSE2 on x86
 * when FASTVEC_GENERIC is defined (port/tools/fastvec_check.c builds it that way too, so this is checked on a PC
 * bit for bit, with the arithmetic that the phone does: IEEE double add / multiply, no fused multiply-add).
 * Measured on a Redmi Note 13 Pro (Cortex-A78): 103 -> 101 ns against the scalar version it replaces.
 */
int Port_FastMtxApply(const uint32_t *r, const uint32_t *v, uint32_t *out) {
    V2u bad = {0, 0};
    V2d vlo = v_wide(v, &bad), vhi = v_wide(v + 2, &bad);
    V2d vx = v_splat(vlo[0]), vy = v_splat(vlo[1]), vz = v_splat(vhi[0]), vw = v_splat(vhi[1]);
    V2d lo, hi;

    lo = v_mul(v_wide(r, &bad), vx, &bad);
    hi = v_mul(v_wide(r + 2, &bad), vx, &bad);
    lo = v_add(lo, v_mul(v_wide(r + 4, &bad), vy, &bad), &bad);
    hi = v_add(hi, v_mul(v_wide(r + 6, &bad), vy, &bad), &bad);
    lo = v_add(lo, v_mul(v_wide(r + 8, &bad), vz, &bad), &bad);
    hi = v_add(hi, v_mul(v_wide(r + 10, &bad), vz, &bad), &bad);
    lo = v_add(lo, v_mul(v_wide(r + 12, &bad), vw, &bad), &bad);
    hi = v_add(hi, v_mul(v_wide(r + 14, &bad), vw, &bad), &bad);
    if (v_any(bad)) {
        return 0;
    }
    v_store(out, lo, hi);
    return 1;
}

#else /* SSE2: the PC */
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

/* ------------------------------------------------------------------------------------------------------------
 * The vector unit's four-component add / multiply / multiply-add (src/port/vu0_a.c op_vadd, op_vmulbc, op_vmaddbc
 * ..., the Vec4 / Vec3 routines of vu0_b.c): out[i] = a[i] + b[i], a[i] * b[i], acc[i] + a[i] * b[i], each operation
 * truncated as Sf_AddBits / Sf_MulBits do. One call for the four components instead of one per component (two
 * for the multiply-add). Measured on a Redmi Note 13 Pro (Cortex-A78): an add of four 30 -> 21 ns, a multiply-add
 * of four 62 -> 42 ns. Component by component, not in NEON registers: the two-lane double version (as
 * Port_FastMtxApply) was SLOWER there (41 / 53 ns), its exactness checks cost more than the arithmetic.
 * ---------------------------------------------------------------------------------------------------------- */
static inline int vu_plain(void) { return !(switches() & 2); } /* (the experiment switches: the usual way) */
static __attribute__((noinline)) void slow_add4(const uint32_t *a, const uint32_t *b, uint32_t *out) {
    int i;
    for (i = 0; i < 4; i++) {
        out[i] = Sf_AddBits(a[i], b[i]);
    }
}
static __attribute__((noinline)) void slow_mul4(const uint32_t *a, const uint32_t *b, uint32_t *out) {
    int i;
    for (i = 0; i < 4; i++) {
        out[i] = Sf_MulBits(a[i], b[i]);
    }
}
static __attribute__((noinline)) void slow_madd4(const uint32_t *acc, const uint32_t *a, const uint32_t *b, uint32_t *out) {
    int i;
    for (i = 0; i < 4; i++) {
        out[i] = Sf_AddBits(acc[i], Sf_MulBits(a[i], b[i]));
    }
}

void Port_Vu0Add4(const uint32_t *a, const uint32_t *b, uint32_t *out) {
    int i;

    if (__builtin_expect(!vu_plain(), 0)) {
        slow_add4(a, b, out);
        return;
    }
    for (i = 0; i < 4; i++) {
        out[i] = fast_add(a[i], b[i]);
    }
}

void Port_Vu0Mul4(const uint32_t *a, const uint32_t *b, uint32_t *out) {
    int i;

    if (__builtin_expect(!vu_plain(), 0)) {
        slow_mul4(a, b, out);
        return;
    }
    for (i = 0; i < 4; i++) {
        out[i] = fast_mul(a[i], b[i]);
    }
}

void Port_Vu0Madd4(const uint32_t *acc, const uint32_t *a, const uint32_t *b, uint32_t *out) {
    int i;

    if (__builtin_expect(!vu_plain(), 0)) {
        slow_madd4(acc, a, b, out);
        return;
    }
    for (i = 0; i < 4; i++) {
        out[i] = fast_add(acc[i], fast_mul(a[i], b[i]));
    }
}

/* The whole-number square root of n < 2^48 (src/port/vu0_a.c RefVu0_SqrtBits: the vector unit's and the FPU's
   sqrt, truncated): the processor's double square root, then put right by one where it rounded up. n and every
   square compared are exact in 64-bit integers. Checked for every n the root can get (port/tools/fastvec_check.c);
   it replaced a loop of 24 steps. */
uint64_t Port_FastIsqrt48(uint64_t n) {
    uint64_t r = (uint64_t)__builtin_sqrt((double)n);

    while (r * r > n) {
        r--;
    }
    while ((r + 1) * (r + 1) <= n) {
        r++;
    }
    return r;
}
