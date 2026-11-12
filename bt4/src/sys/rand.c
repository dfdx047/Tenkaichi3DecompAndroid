#include "common.h"
#include "sys/rand.h"

extern void *memset(void *dst, s32 c, u32 n);
extern void srand(u32 seed);
extern s32 rand(void);
extern long func_002BB390(void);

extern u32 gRandState[RAND_N];
extern s32 gRandIndex;
extern u32 gRandMag01[2];

/* Seeds the twister state from one 32-bit value (MT19937 init_genrand). */
void Rand_Seed(u32 seed) {
    gRandState[0] = seed;
    for (gRandIndex = 1; gRandIndex < RAND_N; gRandIndex++) {
        gRandState[gRandIndex] =
            1812433253ULL * (gRandState[gRandIndex - 1] ^ (gRandState[gRandIndex - 1] >> 30)) + gRandIndex;
    }
}

/* Seeds the twister state from an array of words (MT19937 init_by_array). */
void Rand_SeedByArray(u32 *key, s32 keyLen) {
    s32 i;
    s32 j;
    s32 k;

    Rand_Seed(19650218);
    i = 1;
    j = 0;
    k = RAND_N > keyLen ? RAND_N : keyLen;
    for (; k; k--) {
        gRandState[i] = (gRandState[i] ^ ((gRandState[i - 1] ^ (gRandState[i - 1] >> 30)) * 1664525ULL)) + key[j] + j;
        i++;
        j++;
        if (i >= RAND_N) {
            gRandState[0] = gRandState[RAND_N - 1];
            i = 1;
        }
        if (j >= keyLen) {
            j = 0;
        }
    }
    for (k = RAND_N - 1; k; k--) {
        gRandState[i] = (gRandState[i] ^ ((gRandState[i - 1] ^ (gRandState[i - 1] >> 30)) * 1566083941ULL)) - i;
        i++;
        if (i >= RAND_N) {
            gRandState[0] = gRandState[RAND_N - 1];
            i = 1;
        }
    }
    gRandState[0] = 0x80000000;
}

/* Returns the next 32-bit random number (MT19937 genrand_int32).
 * The refill lacks the reference code's second loop (kk = N-M .. N-2), so words 227..622 of the state
 * are never regenerated: this is not a true MT19937 sequence. */
u32 Rand_Next(void) {
    u32 y;

    if (gRandIndex >= RAND_N) {
        s32 kk;

        if (gRandIndex == RAND_N + 1) {
            Rand_Seed(5489);
        }
        for (kk = 0; kk < RAND_N - RAND_M; kk++) {
            y = (gRandState[kk] & RAND_UPPER_MASK) | (gRandState[kk + 1] & RAND_LOWER_MASK);
            gRandState[kk] = gRandState[kk + RAND_M] ^ (y >> 1) ^ gRandMag01[y & 1];
        }
        y = (gRandState[RAND_N - 1] & RAND_UPPER_MASK) | (gRandState[0] & RAND_LOWER_MASK);
        gRandState[RAND_N - 1] = gRandState[RAND_M - 1] ^ (y >> 1) ^ gRandMag01[y & 1];
        gRandIndex = 0;
    }
    y = gRandState[gRandIndex++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680;
    y ^= (y << 15) & 0xEFC60000;
    y ^= y >> 18;
    return y;
}

/* Seeds libc rand from the hardware timer, then seeds the twister with four rand() words. */
void Rand_Init(void) {
    u32 key[4];
    s32 i;

    memset(key, 0, sizeof(key));
    srand(func_002BB390());
    for (i = 0; i < 4; i++) {
        key[i] = rand();
    }
    Rand_SeedByArray(key, 4);
}

/* Returns a random number in [0, n), or 0 when n is 0. */
u32 Rand_Range(u32 n) {
    if (n == 0) {
        return 0;
    }
    return Rand_Next() % n;
}
