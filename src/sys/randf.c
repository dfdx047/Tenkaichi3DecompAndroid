#include "common.h"
#include "sys/randf.h"

/*
 * Random helpers, 0x11F7D8..0x11F968. Neither uses the twister in src/sys/rand.c.
 *
 *   Rand_Float01 / Rand_FloatRange  the VU0 R register (the PS2's 23-bit shift-register generator)
 *   Rand_Libc / Rand_IntRange       the C library rand()
 *
 * Rand_SeedFloat seeds both from one float. Rand_SeedFloat and Rand_Float01 contain VU0 instructions and stay
 * INCLUDE_ASM; an equivalent that assembles to the same bytes is kept above each one.
 */

extern void srand(u32 seed);
extern s32 rand(void);

/* Seeds the VU0 R register with the float's bit pattern and libc rand with (u32)(seed * 10000000.0f). */
#ifndef PORT /* PC build: port/src/mathf_pc.c */
void Rand_SeedFloat(f32 seed) {
    __asm__ volatile(
        "mfc1 $8, %0\n"
        "qmtc2 $8, $vf5\n"
        "vrinit $R, $vf5x\n"
        :
        : "f"(seed));
    srand(seed * 10000000.0f);
}
#endif

/*
 * Returns a float in [0, 1): steps the R register 7 times, re-seeds it from the value just produced, steps it 7
 * more times, and subtracts 1.0 from the last value (which lies in [1, 2)). VU0 assembly.
 */
#ifndef PORT /* PC build: port/src/mathf_pc.c */
f32 Rand_Float01(void) {
    f32 result;

    __asm__ volatile(
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrinit $R, $vf5x\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vrnext.x $vf5, $R\n"
        "vsubw.x $vf5, $vf5, $vf0w\n"
        "qmfc2 $8, $vf5\n"
        "mtc1 $8, %0\n"
        : "=f"(result));
    return result;
}
#endif

/* Returns a float between a and b (either order): lo + Rand_Float01() * (hi - lo); b itself when a == b. */
f32 Rand_FloatRange(f32 a, f32 b) {
    f32 tmp;

    if (b == a) {
        return b;
    }
    if (b < a) {
        tmp = a;
        a = b;
        b = tmp;
    }
    return a + Rand_Float01() * (b - a);
}

/* libc rand(). */
s32 Rand_Libc(void) {
    return rand();
}

/* Returns an integer in [a, b] (either order): lo + rand() % (hi - lo + 1); a itself when a == b. */
s32 Rand_IntRange(s32 a, s32 b) {
    s32 tmp;

    if (b == a) {
        return a;
    }
    if (b < a) {
        tmp = a;
        a = b;
        b = tmp;
    }
    return a + Rand_Libc() % (b - a + 1);
}
