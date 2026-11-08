/*
 * Compares the quick ways of port/src/plat_fastvec.c with the long way (softfloat_ps2_inl.h), bit for bit:
 * Port_FastMtxApply, Port_FastAdd / Mul, the game's __addsf3 / __subsf3 / __mulsf3 and the vector unit's
 * Port_Vu0Add4 / Mul4 / Madd4.
 *     cc -O2 -Iport/src port/tools/fastvec_check.c port/src/plat_fastvec.c -o /tmp/fastvec_check && /tmp/fastvec_check [millions]
 * Add -DFASTVEC_GENERIC to both on a PC to check the vector-type version that arm64 (NEON) builds; or build it for
 * the phone with the NDK's clang (--target=aarch64-linux-android29) and run it there over adb.
 * Prints how often the quick way gave up, too: wherever it answers, the answer must be the long way's.
 */
#include <stdio.h>
#include <string.h>
#include "softfloat_ps2_inl.h"

extern int Port_FastMtxApply(const uint32_t *r, const uint32_t *v, uint32_t *out);
extern uint32_t Port_FastAdd(uint32_t a, uint32_t b);
extern uint32_t Port_FastMul(uint32_t a, uint32_t b);
extern uint32_t __addsf3(uint32_t a, uint32_t b), __subsf3(uint32_t a, uint32_t b), __mulsf3(uint32_t a, uint32_t b);
extern uint64_t Port_FastIsqrt48(uint64_t n);
extern void Port_Vu0Add4(const uint32_t *a, const uint32_t *b, uint32_t *out);
extern void Port_Vu0Mul4(const uint32_t *a, const uint32_t *b, uint32_t *out);
extern void Port_Vu0Madd4(const uint32_t *acc, const uint32_t *a, const uint32_t *b, uint32_t *out);
extern int Port_FastMtxMul(const uint32_t *a, const uint32_t *b, uint32_t *out, uint32_t *acc, uint32_t *last, uint32_t *any);

static uint64_t sState = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) {
    sState ^= sState << 13; sState ^= sState >> 7; sState ^= sState << 17;
    return (uint32_t)(sState >> 16);
}
static const uint32_t kEdge[] = {
    0x00000000, 0x80000000, 0x00000001, 0x007FFFFF, 0x00800000, 0x00800001, 0x3F800000, 0xBF800000, 0x3F7FFFFF,
    0x3F800001, 0x7F7FFFFF, 0x7F800000, 0x7FC00000, 0xFF7FFFFF, 0x80800000, 0x1F800000, 0x5F800000, 0x00FFFFFF,
};
/* kind 0: any bits; 1: exponents near 1.0 (what the game mostly has); 2: with edges and zeros mixed in */
static uint32_t operand(int kind) {
    uint32_t u = rnd();
    if (kind == 1) { return (u & 0x807FFFFF) | (uint32_t)(127 - 20 + rnd() % 41) << 23; }
    if (kind == 2) { uint32_t k = rnd() % 4; return k == 0 ? kEdge[rnd() % (sizeof(kEdge) / 4)] : k == 1 ? (u & SIGN) : (u & 0x807FFFFF) | (uint32_t)(127 - 30 + rnd() % 61) << 23; }
    if (kind == 3) { return (u & 0x80700000) | (uint32_t)(127 - 3 + rnd() % 7) << 23; } /* few bits: sums cancel */
    return u;
}
static void long_way(const uint32_t *r, const uint32_t *v, uint32_t *o) {
    int c;
    for (c = 0; c < 4; c++) {
        uint32_t a = Sf_MulBits(r[c], v[0]);
        a = Sf_AddBits(a, Sf_MulBits(r[4 + c], v[1]));
        a = Sf_AddBits(a, Sf_MulBits(r[8 + c], v[2]));
        o[c] = Sf_AddBits(a, Sf_MulBits(r[12 + c], v[3]));
    }
}
/* Mtx_Mul's sixteen instructions one by one (src/port/vu0_a.c Ref_Mtx_Mul and setFlags): the product, the
   accumulator after the last vmaddaz, the zero / sign flags of the last instruction and of all of them ORed */
static unsigned zs(const uint32_t *r) {
    unsigned f = 0;
    int c;
    for (c = 0; c < 4; c++) { f |= ((r[c] & 0x7FFFFFFF) == 0) | ((r[c] >> 31) << 1); }
    return f;
}
static void mtxmul_long(const uint32_t *a, const uint32_t *b, uint32_t *out, uint32_t *acc, uint32_t *last, uint32_t *any) {
    int i, c;
    unsigned seen = 0, cur = 0;
    for (i = 0; i < 4; i++) {
        uint32_t t[4];
        for (c = 0; c < 4; c++) { t[c] = Sf_MulBits(a[c], b[i * 4]); }
        seen |= zs(t);
        for (c = 0; c < 4; c++) { t[c] = Sf_AddBits(t[c], Sf_MulBits(a[4 + c], b[i * 4 + 1])); }
        seen |= zs(t);
        for (c = 0; c < 4; c++) { t[c] = Sf_AddBits(t[c], Sf_MulBits(a[8 + c], b[i * 4 + 2])); }
        seen |= zs(t);
        if (i == 3) { memcpy(acc, t, 16); }
        for (c = 0; c < 4; c++) { out[i * 4 + c] = Sf_AddBits(t[c], Sf_MulBits(a[12 + c], b[i * 4 + 3])); }
        cur = zs(out + i * 4);
        seen |= cur;
    }
    *last = cur;
    *any = seen;
}
/* the square root's loop (src/port/vu0_a.c RefVu0_SqrtBits) */
static uint64_t isqrt_loop(uint64_t n) {
    uint64_t r = 0, bit;
    for (bit = (uint64_t)1 << 46; bit != 0; bit >>= 2) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else { r >>= 1; }
    }
    return r;
}
int main(int argc, char **argv) {
    unsigned long long mtxOk = 0, n = (argc > 1 ? strtoull(argv[1], NULL, 10) : 50) * 1000000ull, i, bad = 0, gaveUp[4] = {0, 0, 0, 0}, per[4] = {0, 0, 0, 0};
    { /* the square root: every mantissa, both exponent parities (all the values RefVu0_SqrtBits gives the loop) */
        uint64_t m;
        int odd;
        for (odd = 0; odd < 2; odd++) for (m = 0x800000; m < 0x1000000; m++) {
            uint64_t x = (m << odd) << 23;
            if (Port_FastIsqrt48(x) != isqrt_loop(x) && bad++ < 10) { printf("sqrt %llx\n", (unsigned long long)x); }
        }
    }
    for (i = 0; i < n; i++) {
        uint32_t r[16], v[4], a[4], b[4];
        int kind = (int)(i & 3), k;
        for (k = 0; k < 16; k++) { r[k] = operand(kind); }
        for (k = 0; k < 4; k++) { v[k] = operand(kind); }
        for (k = 0; k < 16; k++) { /* the single operations, on the same operands and on close pairs */
            uint32_t x = r[k], y = k < 8 ? r[15 - k] : (x ^ (rnd() & SIGN)) + (rnd() % 17) - 8 + ((rnd() % 5) << 23);
            if (Port_FastAdd(x, y) != Sf_AddBits(x, y) && bad++ < 10) { printf("add %08x %08x: quick %08x long %08x\n", x, y, Port_FastAdd(x, y), Sf_AddBits(x, y)); }
            if (Port_FastMul(x, y) != Sf_MulBits(x, y) && bad++ < 10) { printf("mul %08x %08x: quick %08x long %08x\n", x, y, Port_FastMul(x, y), Sf_MulBits(x, y)); }
            /* the game's own operations (the FPU: fpu_add's operand cut) */
            if (__addsf3(x, y) != fpu_add(x, y) && bad++ < 10) { printf("__addsf3 %08x %08x: %08x long %08x\n", x, y, __addsf3(x, y), fpu_add(x, y)); }
            if (__subsf3(x, y) != fpu_add(x, y ^ SIGN) && bad++ < 10) { printf("__subsf3 %08x %08x\n", x, y); }
            if (__mulsf3(x, y) != mul_core(x, y, 0) && bad++ < 10) { printf("__mulsf3 %08x %08x\n", x, y); }
        }
        { /* the vector unit's four-component operations on the matrix rows */
            uint32_t q[4], l[4];
            int c;
            Port_Vu0Add4(r, r + 4, q);
            for (c = 0; c < 4; c++) { l[c] = Sf_AddBits(r[c], r[4 + c]); }
            if (memcmp(q, l, 16) != 0 && bad++ < 10) { printf("vadd %08x %08x: %08x long %08x\n", r[0], r[4], q[0], l[0]); }
            Port_Vu0Mul4(r + 8, v, q);
            for (c = 0; c < 4; c++) { l[c] = Sf_MulBits(r[8 + c], v[c]); }
            if (memcmp(q, l, 16) != 0 && bad++ < 10) { printf("vmul %08x %08x: %08x long %08x\n", r[8], v[0], q[0], l[0]); }
            Port_Vu0Madd4(r + 12, r, v, q);
            for (c = 0; c < 4; c++) { l[c] = Sf_AddBits(r[12 + c], Sf_MulBits(r[c], v[c])); }
            if (memcmp(q, l, 16) != 0 && bad++ < 10) { printf("vmadd %08x %08x %08x: %08x long %08x\n", r[12], r[0], v[0], q[0], l[0]); }
        }
        { /* Mtx_Mul */
            uint32_t m2[16], o1[16], o2[16], c1[4], c2[4], l1, l2, y1, y2;
            for (k = 0; k < 16; k++) { m2[k] = operand(kind); }
            mtxmul_long(r, m2, o1, c1, &l1, &y1);
            if (Port_FastMtxMul(r, m2, o2, c2, &l2, &y2)) {
                mtxOk++;
                if ((memcmp(o1, o2, 64) != 0 || memcmp(c1, c2, 16) != 0 || l1 != l2 || y1 != y2) && bad++ < 10) {
                    printf("mtxmul differs (kind %d): out %08x/%08x acc %08x/%08x flags %u %u / %u %u\n", kind, o1[0], o2[0], c1[0], c2[0], l1, y1, l2, y2);
                }
            }
        }
        long_way(r, v, a);
        per[kind]++;
        if (!Port_FastMtxApply(r, v, b)) { gaveUp[kind]++; continue; }
        if (memcmp(a, b, sizeof(a)) != 0 && bad++ < 10) {
            printf("differs (kind %d): long %08x %08x %08x %08x quick %08x %08x %08x %08x\n", kind, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
        }
    }
    printf("%llu million matrices, %llu differences; gave up: any bits %.1f%%, near 1.0 %.2f%%, edges %.1f%%, few bits %.2f%%\n", n / 1000000ull, bad,
           100.0 * gaveUp[0] / per[0], 100.0 * gaveUp[1] / per[1], 100.0 * gaveUp[2] / per[2], 100.0 * gaveUp[3] / per[3]);
    printf("matrix products answered quickly: %.1f%%\n", 100.0 * mtxOk / n);
    return bad != 0;
}
