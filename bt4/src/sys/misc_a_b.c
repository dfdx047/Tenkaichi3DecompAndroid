#include "common.h"
#include "sys/misc_a.h"

/* Character passwords, current format: 0x253ED8-0x254A20. Called from the menu overlay: ChrPass_Decode (0x3ABDCC),
 * ChrPass_Encode (0x3AE9F8, 0x3AEA5C), ChrPass_GetText (0x3AE868). Same code as misc_a.c with its own state. */

extern u32 strlen(const char *s);
extern s32 rand(void);

extern u32 gChrPassMtState[PASS_MT_N];
extern u8 gChrPassBits[PASS_BUF_SIZE];
extern char gChrPassText[0x30];

s32 gChrPassMtIndex = 0;

/* The 64 characters of the current format, in value order: the older set with the vowels and 'i' replaced by digits. */
#define CHRPASS_CHARSET                                                                                              \
    "Q", "R", "S", "T", "2", "V", "W", "X", "1", "J", "K", "L", "M", "N", "5", "P", "0", "B", "C", "D", "3", "F",    \
    "G", "H", "Y", "Z", "4", "b", "c", "d", "9", "f", "w", "x", "y", "z", "!", "#", "$", "%", "6", "p", "q", "r",    \
    "s", "t", "8", "v", "g", "h", "7", "j", "k", "l", "m", "n", "&", "@", "-", "+", "*", "(", ")", "?"

/* Fills the twister state from one word (MT19937 init_genrand) and leaves the read index at 1. */
void ChrPass_MtSeed(u32 seed) {
    s32 i;

    gChrPassMtState[0] = seed;
    for (i = 1; i < PASS_MT_N; i++) {
        gChrPassMtState[i] = 1812433253 * (gChrPassMtState[i - 1] ^ (gChrPassMtState[i - 1] >> 30)) + i;
    }
    gChrPassMtIndex = 1;
}

/* Fills the twister state from an array of words (MT19937 init_by_array). */
void ChrPass_MtSeedByArray(u32 *key, s32 keyLen) {
    s32 i = 1;
    s32 j = 0;
    s32 k = PASS_MT_N > keyLen ? PASS_MT_N : keyLen;

    ChrPass_MtSeed(19650218);
    for (; k; k--) {
        gChrPassMtState[i] =
            (gChrPassMtState[i] ^ ((gChrPassMtState[i - 1] ^ (gChrPassMtState[i - 1] >> 30)) * 1664525)) + key[j] + j;
        i++;
        j++;
        if (i >= PASS_MT_N) {
            gChrPassMtState[0] = gChrPassMtState[PASS_MT_N - 1];
            i = 1;
        }
        if (j >= keyLen) {
            j = 0;
        }
    }
    for (k = PASS_MT_N - 1; k; k--) {
        gChrPassMtState[i] =
            (gChrPassMtState[i] ^ ((gChrPassMtState[i - 1] ^ (gChrPassMtState[i - 1] >> 30)) * 1566083941ULL)) - i;
        i++;
        if (i >= PASS_MT_N) {
            gChrPassMtState[0] = gChrPassMtState[PASS_MT_N - 1];
            i = 1;
        }
    }
}

/* Returns the next state word, tempered as MT19937 does. There is no refill: the state is never twisted. */
u32 ChrPass_MtNext(void) {
    u32 y;

    y = gChrPassMtState[gChrPassMtIndex++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680;
    y ^= (y << 15) & 0xEFC60000;
    y ^= y >> 18;
    return y;
}

/* Zeroes the bit buffer. */
void ChrPass_ClearBits(void) {
    s32 i;

    for (i = 0; i < PASS_BUF_SIZE; i++) {
        gChrPassBits[i] = 0;
    }
}

/* Writes the low `count` bits of `value` at bit position `pos` (least significant bit first). */
void ChrPass_PutBits(u32 value, s32 pos, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        s32 bit = (value >> i) & 1;
        s32 p = i + pos;

        if (bit) {
            gChrPassBits[p / 8] |= 1 << (p % 8);
        } else {
            gChrPassBits[p / 8] &= ~(1 << (p % 8));
        }
    }
}

/* Returns the bit at position `pos`. */
s32 ChrPass_GetBit(s32 pos) {
    return (gChrPassBits[pos / 8] >> (pos % 8)) & 1;
}

/* Reads `count` bits at bit position `pos` (least significant bit first). */
u32 ChrPass_GetBits(s32 pos, s32 count) {
    u32 value;
    s32 i;

    value = 0;
    for (i = 0; i < count; i++) {
        value |= ChrPass_GetBit(i + pos) << i;
    }
    return value;
}

/* The divisor reaches the division as a variable: the original divides by a register holding 6. */
static inline s32 ChrPass_Div(s32 a, s32 b) {
    return a / b;
}

/* Writes the first `bitCount` bits of the buffer as text, 6 bits per character. */
void ChrPass_BitsToText(s32 bitCount) {
    char *charset[PASS_CHARSET] = { CHRPASS_CHARSET };
    s32 n;
    s32 i;
    u32 value;

    for (n = 0; n < ChrPass_Div(bitCount, 6); n++) {
        value = 0;
        for (i = 0; i < 6; i++) {
            value |= ChrPass_GetBit(n * 6 + i) << i;
        }
        gChrPassText[n] = *charset[value];
        gChrPassText[n + 1] = 0;
    }
}

/* Reads text into the bit buffer, 6 bits per character; characters outside the set are skipped. */
void ChrPass_TextToBits(char *text) {
    char *charset[PASS_CHARSET] = { CHRPASS_CHARSET };
    char *p = text;
    s32 pos = 0;
    s32 i;

    while (*p != 0) {
        for (i = 0; i < PASS_CHARSET; i++) {
            if (*p == *charset[i]) {
                ChrPass_PutBits(i, pos, 6);
                pos += 6;
                break;
            }
        }
        p++;
    }
}

/* Empty (stripped debug output). */
void ChrPass_Nop(void) {
}

/* Seeds the key stream from a password's 32-bit seed. */
static inline void ChrPass_SetKey(u32 seed) {
    u32 key[4] = { seed, seed * 2, seed * 3, seed * 4 };

    ChrPass_MtSeedByArray(key, 4);
}

/* Decodes a 34-character password. Returns 1 when the length, both bit counts and the parity all check. */
s32 ChrPass_Decode(ChrPassData *out, char *text) {
    s32 i;
    s32 sum;
    u32 seed;
    s32 n;

    if (strlen(text) != 34) {
        return 0;
    }
    ChrPass_ClearBits();
    ChrPass_TextToBits(text);
    seed = ChrPass_GetBits(0, 32);
    ChrPass_SetKey(seed);
    for (i = 4; i < 26; i++) {
        gChrPassBits[i] ^= ChrPass_MtNext();
    }
    sum = 0;
    for (i = 0; i < 0x77; i++) {
        sum += ChrPass_GetBit(i);
    }
    if (sum != ChrPass_GetBits(0x77, 8)) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < 0xC4; i++) {
        sum += ChrPass_GetBit(i);
    }
    if (sum != ChrPass_GetBits(0xC4, 8)) {
        return 0;
    }
    /* The encoder writes a second copy of the low 4 bits at 0xCC, but 34 characters carry only bits 0..0xCB:
     * this test re-reads the low half of the byte just compared and can never fail. */
    if ((sum & 0xF) != ChrPass_GetBits(0xC4, 4)) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        if (gChrPassBits[i + 16] != (gChrPassBits[i] ^ gChrPassBits[i + 8])) {
            return 0;
        }
    }
    out->charId = ChrPass_GetBits(0x20, 8);
    for (n = 0; n < 8; n++) {
        out->item[n] = ChrPass_GetBits(0x28 + n * 9, 9);
    }
    out->extraSlots = ChrPass_GetBits(0x70, 4);
    out->unk28 = ChrPass_GetBits(0x74, 1);
    return 1;
}

/* Encodes `in` as a 34-character password in the text buffer, with a seed from two rand() draws. */
s32 ChrPass_Encode(ChrPassData *in) {
    s32 i;
    s32 sum;
    u32 seed;
    s32 hi;
    s32 lo;
    s32 n;

    hi = rand();
    lo = rand();
    seed = (hi << 16) | (lo & 0xFFFF);
    ChrPass_SetKey(seed);
    ChrPass_ClearBits();
    ChrPass_PutBits(seed, 0, 32);
    ChrPass_PutBits(in->charId, 0x20, 8);
    for (n = 0; n < 8; n++) {
        ChrPass_PutBits(in->item[n], 0x28 + n * 9, 9);
    }
    ChrPass_PutBits(in->extraSlots, 0x70, 4);
    ChrPass_PutBits(in->unk28, 0x74, 1);
    sum = 0;
    for (i = 0; i < 0x80; i++) {
        sum += ChrPass_GetBit(i);
    }
    ChrPass_PutBits(sum, 0x77, 8);
    for (i = 0; i < 8; i++) {
        gChrPassBits[i + 16] = gChrPassBits[i] ^ gChrPassBits[i + 8];
    }
    sum = 0;
    for (i = 0; i < 0xC4; i++) {
        sum += ChrPass_GetBit(i);
    }
    ChrPass_PutBits(sum, 0xC4, 8);
    ChrPass_PutBits(sum, 0xCC, 4);
    for (i = 4; i < 26; i++) {
        gChrPassBits[i] ^= ChrPass_MtNext();
    }
    ChrPass_BitsToText(0xD0);
    return 1;
}

/* Returns the text buffer the encoder writes. */
char *ChrPass_GetText(void) {
    return gChrPassText;
}

/* Empty (stripped debug output). */
void ChrPass_Nop2(void) {
}

/* Fills `out` with random content from rand() (debug helper; no caller). */
void ChrPass_Random(ChrPassData *out) {
    s32 i;

    out->charId = rand() % 129;
    for (i = 0; i < 8; i++) {
        out->item[i] = rand() % 200;
    }
    out->extraSlots = rand() % 16;
}

/* Stripped debug text. */
char *ChrPass_GetDebugText(void) {
    return "No Debug\n";
}
