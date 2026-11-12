/* Host side of the PS2 float test: the same inputs through the PC build's float model (softfloat_ps2.c and the
   vector-library primitives), printed in the same format as test.c. Compared with PCSX2's output by check.py. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "port/vu0_a.h"
uint32_t __addsf3(uint32_t, uint32_t), __subsf3(uint32_t, uint32_t), __mulsf3(uint32_t, uint32_t), __divsf3(uint32_t, uint32_t);
uint32_t __floatsisf(int32_t); int32_t __fixsfsi(uint32_t);
static uint32_t seed = 12345;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed; }
static uint32_t fpu_sqrt(uint32_t a) { float x; uint32_t r; a &= 0x7FFFFFFF; memcpy(&x, &a, 4); x = sqrtf(x); memcpy(&r, &x, 4); return r; }
int main(void) {
    int i;
    for (i = 0; i < 1500; i++) {
        uint32_t a = rnd(), b = rnd(), ea, eb, p;
        int32_t n;
        ea = 100 + (rnd() >> 8) % 56;
        eb = (i % 3 == 0) ? ea - (rnd() >> 8) % 30 : 100 + (rnd() >> 8) % 56;
        a = (a & 0x807FFFFF) | (ea << 23);
        b = (b & 0x807FFFFF) | (eb << 23);
        if (i % 7 == 0) b = (a & 0xFFFFFFF0) ^ ((rnd() >> 8) & 0x8000000F);
        n = (int32_t)rnd() >> ((rnd() >> 8) % 24);
        printf("%08x %08x %08x ", a, b, (uint32_t)n);
        printf("%08x %08x %08x %08x %08x ", __addsf3(a, b), __subsf3(a, b), __mulsf3(a, b), __divsf3(a, b), fpu_sqrt(a));
        printf("%08x %08x %08x ", __addsf3(a, __mulsf3(a, b)), __floatsisf(n), (uint32_t)__fixsfsi((a & 0x807FFFFF) | ((120 + (ea & 31)) << 23)));
        p = RefVu0_MulBits(a, b);
        printf("%08x %08x %08x %08x %08x %08x %08x %08x \n", RefVu0_AddBits(a, b), RefVu0_SubBits(a, b), p, RefVu0_DivBits(a, b),
               RefVu0_SqrtBits(a), RefVu0_AddBits(p, p), RefVu0_ItofBits(n, 0), (uint32_t)__fixsfsi(a));
    }
    return 0;
}
