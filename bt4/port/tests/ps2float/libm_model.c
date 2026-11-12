/* Host side of the libm test: the same inputs through the PC build's maths library (the newlib sources of
   port/third_party/newlib_libm on the software float model). Built with the game's flags by check_libm.py. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed = 2024;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed; }
static float f(uint32_t u) { float x; memcpy(&x, &u, 4); return x; }
static uint32_t u(float x) { uint32_t v; memcpy(&v, &x, 4); return v; }
int main(void) {
    int i;
    for (i = 0; i < 1500; i++) {
        uint32_t a = (rnd() & 0x807FFFFF) | ((112 + (rnd() >> 8) % 20) << 23);
        uint32_t b = (rnd() & 0x807FFFFF) | ((112 + (rnd() >> 8) % 20) << 23);
        uint32_t c = (rnd() & 0x807FFFFF) | ((110 + (rnd() >> 8) % 17) << 23);
        printf("%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", a, b, c, u(sinf(f(a))), u(cosf(f(a))),
               u(tanf(f(a))), u(atanf(f(a))), u(atan2f(f(a), f(b))), u(asinf(f(c))), u(acosf(f(c))),
               u(sqrtf(f(a & 0x7FFFFFFF))), u(powf(f(a & 0x7FFFFFFF), f(c))), u(floorf(f(a))));
    }
    return 0;
}
