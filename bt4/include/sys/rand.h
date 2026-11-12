#ifndef SYS_RAND_H
#define SYS_RAND_H

#include "types.h"

/* MT19937 parameters (Matsumoto & Nishimura, 2002 reference code). */
#define RAND_N 624
#define RAND_M 397
#define RAND_MATRIX_A 0x9908B0DFULL
#define RAND_UPPER_MASK 0x80000000ULL
#define RAND_LOWER_MASK 0x7FFFFFFFULL

void Rand_Seed(u32 seed);
void Rand_SeedByArray(u32 *key, s32 keyLen);
u32 Rand_Next(void);
void Rand_Init(void);
u32 Rand_Range(u32 n);

#endif
