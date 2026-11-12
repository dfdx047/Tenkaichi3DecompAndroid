#ifndef SYS_RANDF_H
#define SYS_RANDF_H

#include "types.h"

/* Random helpers on the VU0 R register and on libc rand(), src/sys/randf.c = 0x11F7D8..0x11F968. */

void Rand_SeedFloat(f32 seed);
f32 Rand_Float01(void);
f32 Rand_FloatRange(f32 a, f32 b);
s32 Rand_Libc(void);
s32 Rand_IntRange(s32 a, s32 b);

#endif
