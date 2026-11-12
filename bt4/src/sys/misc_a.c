#include "common.h"
#include "sys/misc_a.h"

/* Character passwords, older format: 0x252F68-0x253ED8. Only OldPass_DecodeChar is called (menu overlay 0x3ABD8C);
 * the encoders, the 20-character format and the random fillers have no caller. */

extern u32 strlen(const char *s);
extern s32 rand(void);

extern u32 gOldPassMtState[PASS_MT_N];
extern u8 gOldPassBits[PASS_BUF_SIZE];
extern char gOldPassText[0x30];

s32 gOldPassMtIndex = 0;

/* The 64 characters of the older format, in value order. */
#define OLDPASS_CHARSET                                                                                              \
    "Q", "R", "S", "T", "U", "V", "W", "X", "I", "J", "K", "L", "M", "N", "O", "P", "A", "B", "C", "D", "E", "F",    \
    "G", "H", "Y", "Z", "a", "b", "c", "d", "e", "f", "w", "x", "y", "z", "!", "#", "$", "%", "o", "p", "q", "r",    \
    "s", "t", "u", "v", "g", "h", "i", "j", "k", "l", "m", "n", "&", "@", "-", "+", "*", "(", ")", "?"

/* Fills the twister state from one word (MT19937 init_genrand) and leaves the read index at 1. */
void OldPass_MtSeed(u32 seed) {
    s32 i;

    gOldPassMtState[0] = seed;
    for (i = 1; i < PASS_MT_N; i++) {
        gOldPassMtState[i] = 1812433253 * (gOldPassMtState[i - 1] ^ (gOldPassMtState[i - 1] >> 30)) + i;
    }
    gOldPassMtIndex = 1;
}

/* Fills the twister state from an array of words (MT19937 init_by_array). */
void OldPass_MtSeedByArray(u32 *key, s32 keyLen) {
    s32 i = 1;
    s32 j = 0;
    s32 k = PASS_MT_N > keyLen ? PASS_MT_N : keyLen;

    OldPass_MtSeed(19650218);
    for (; k; k--) {
        gOldPassMtState[i] =
            (gOldPassMtState[i] ^ ((gOldPassMtState[i - 1] ^ (gOldPassMtState[i - 1] >> 30)) * 1664525)) + key[j] + j;
        i++;
        j++;
        if (i >= PASS_MT_N) {
            gOldPassMtState[0] = gOldPassMtState[PASS_MT_N - 1];
            i = 1;
        }
        if (j >= keyLen) {
            j = 0;
        }
    }
    for (k = PASS_MT_N - 1; k; k--) {
        gOldPassMtState[i] =
            (gOldPassMtState[i] ^ ((gOldPassMtState[i - 1] ^ (gOldPassMtState[i - 1] >> 30)) * 1566083941ULL)) - i;
        i++;
        if (i >= PASS_MT_N) {
            gOldPassMtState[0] = gOldPassMtState[PASS_MT_N - 1];
            i = 1;
        }
    }
}

/* Returns the next state word, tempered as MT19937 does. There is no refill: the state is never twisted. */
u32 OldPass_MtNext(void) {
    u32 y;

    y = gOldPassMtState[gOldPassMtIndex++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680;
    y ^= (y << 15) & 0xEFC60000;
    y ^= y >> 18;
    return y;
}

/* Zeroes the bit buffer. */
void OldPass_ClearBits(void) {
    s32 i;

    for (i = 0; i < PASS_BUF_SIZE; i++) {
        gOldPassBits[i] = 0;
    }
}

/* Writes the low `count` bits of `value` at bit position `pos` (least significant bit first). */
void OldPass_PutBits(u32 value, s32 pos, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        s32 bit = (value >> i) & 1;
        s32 p = i + pos;

        if (bit) {
            gOldPassBits[p / 8] |= 1 << (p % 8);
        } else {
            gOldPassBits[p / 8] &= ~(1 << (p % 8));
        }
    }
}

/* Returns the bit at position `pos`. */
s32 OldPass_GetBit(s32 pos) {
    return (gOldPassBits[pos / 8] >> (pos % 8)) & 1;
}

/* Reads `count` bits at bit position `pos` (least significant bit first). */
u32 OldPass_GetBits(s32 pos, s32 count) {
    u32 value;
    s32 i;

    value = 0;
    for (i = 0; i < count; i++) {
        value |= OldPass_GetBit(i + pos) << i;
    }
    return value;
}

/* The divisor reaches the division as a variable: the original divides by a register holding 6. */
static inline s32 OldPass_Div(s32 a, s32 b) {
    return a / b;
}

/* Writes the first `bitCount` bits of the buffer as text, 6 bits per character. */
void OldPass_BitsToText(s32 bitCount) {
    char *charset[PASS_CHARSET] = { OLDPASS_CHARSET };
    s32 n;
    s32 i;
    u32 value;

    for (n = 0; n < OldPass_Div(bitCount, 6); n++) {
        value = 0;
        for (i = 0; i < 6; i++) {
            value |= OldPass_GetBit(n * 6 + i) << i;
        }
        gOldPassText[n] = *charset[value];
        gOldPassText[n + 1] = 0;
    }
}

/* Reads text into the bit buffer, 6 bits per character; characters outside the set are skipped. */
void OldPass_TextToBits(char *text) {
    char *charset[PASS_CHARSET] = { OLDPASS_CHARSET };
    char *p = text;
    s32 pos = 0;
    s32 i;

    while (*p != 0) {
        for (i = 0; i < PASS_CHARSET; i++) {
            if (*p == *charset[i]) {
                OldPass_PutBits(i, pos, 6);
                pos += 6;
                break;
            }
        }
        p++;
    }
}

/* Empty (stripped debug output). */
void OldPass_Nop(void) {
}

/* Seeds the key stream from a password's 32-bit seed. */
static inline void OldPass_SetKey(u32 seed) {
    u32 key[4] = { seed, seed * 2, seed * 3, seed * 4 };

    OldPass_MtSeedByArray(key, 4);
}

/* Decodes a 32-character password. Returns 1 when the length, bit count and parity all check. */
s32 OldPass_DecodeChar(OldPassChar *out, char *text) {
    s32 i;
    s32 sum;
    u32 seed;
    s32 n;

    if (strlen(text) != 32) {
        return 0;
    }
    OldPass_TextToBits(text);
    seed = OldPass_GetBits(0, 32);
    OldPass_SetKey(seed);
    for (i = 4; i < 24; i++) {
        gOldPassBits[i] ^= OldPass_MtNext();
    }
    sum = 0;
    for (i = 0; i < 0x77; i++) {
        sum += OldPass_GetBit(i);
    }
    if (sum != OldPass_GetBits(0x77, 8)) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        if (gOldPassBits[i + 16] != (gOldPassBits[i] ^ gOldPassBits[i + 8])) {
            return 0;
        }
    }
    out->charId = OldPass_GetBits(0x20, 8);
    for (n = 0; n < 7; n++) {
        out->item[n] = OldPass_GetBits(0x28 + n * 10, 10);
    }
    out->unk20 = OldPass_GetBits(0x6E, 3);
    out->unk24 = OldPass_GetBits(0x71, 1);
    out->unk28 = OldPass_GetBits(0x72, 4);
    out->unk2C = OldPass_GetBits(0x76, 1);
    return 1;
}

/* Decodes a 20-character password. Returns 1 when the length, bit count and parity all check. */
s32 OldPass_DecodeShort(OldPassShort *out, char *text) {
    s32 i;
    s32 sum;
    u32 seed;

    if (strlen(text) != 20) {
        return 0;
    }
    OldPass_TextToBits(text);
    seed = OldPass_GetBits(0, 32);
    OldPass_SetKey(seed);
    for (i = 4; i < 15; i++) {
        gOldPassBits[i] ^= OldPass_MtNext();
    }
    sum = 0;
    for (i = 0; i < 0x44; i++) {
        sum += OldPass_GetBit(i);
    }
    if (sum != OldPass_GetBits(0x44, 8)) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        if (gOldPassBits[i + 10] != (gOldPassBits[i] ^ gOldPassBits[i + 5])) {
            return 0;
        }
    }
    out->charId = OldPass_GetBits(0x20, 8);
    out->unk04 = 0;
    out->amount = OldPass_GetBits(0x28, 20) * 100;
    out->unk0C = OldPass_GetBits(0x3C, 2);
    out->unk10 = OldPass_GetBits(0x3E, 1);
    out->unk14 = OldPass_GetBits(0x3F, 4);
    out->unk18 = OldPass_GetBits(0x43, 1);
    return 1;
}

/* Encodes `in` as a 32-character password in the text buffer, with a seed from two rand() draws. */
s32 OldPass_EncodeChar(OldPassChar *in) {
    s32 i;
    s32 sum;
    u32 seed;
    s32 hi;
    s32 lo;
    s32 n;

    hi = rand();
    lo = rand();
    seed = (hi << 16) | (lo & 0xFFFF);
    OldPass_SetKey(seed);
    OldPass_ClearBits();
    OldPass_PutBits(seed, 0, 32);
    OldPass_PutBits(in->charId, 0x20, 8);
    for (n = 0; n < 7; n++) {
        OldPass_PutBits(in->item[n], 0x28 + n * 10, 10);
    }
    OldPass_PutBits(in->unk20, 0x6E, 3);
    OldPass_PutBits(in->unk24, 0x71, 1);
    OldPass_PutBits(in->unk28, 0x72, 4);
    OldPass_PutBits(in->unk2C, 0x76, 1);
    sum = 0;
    for (i = 0; i < 0x80; i++) {
        sum += OldPass_GetBit(i);
    }
    OldPass_PutBits(sum, 0x77, 8);
    for (i = 0; i < 8; i++) {
        gOldPassBits[i + 16] = gOldPassBits[i] ^ gOldPassBits[i + 8];
    }
    for (i = 4; i < 24; i++) {
        gOldPassBits[i] ^= OldPass_MtNext();
    }
    OldPass_BitsToText(0xC0);
    return 1;
}

/* Encodes `in` as a 20-character password in the text buffer, with a seed from two rand() draws. */
s32 OldPass_EncodeShort(OldPassShort *in) {
    s32 i;
    s32 sum;
    u32 seed;
    s32 hi;
    s32 lo;
    s32 n;

    hi = rand();
    lo = rand();
    seed = (hi << 16) | (lo & 0xFFFF);
    OldPass_SetKey(seed);
    OldPass_ClearBits();
    OldPass_PutBits(seed, 0, 32);
    OldPass_PutBits(in->charId, 0x20, 8);
    if (in->amount > 104857500) {
        in->amount = 104857500;
    }
    OldPass_PutBits(in->amount / 100, 0x28, 20);
    OldPass_PutBits(in->unk0C, 0x3C, 2);
    OldPass_PutBits(in->unk10, 0x3E, 1);
    OldPass_PutBits(in->unk14, 0x3F, 4);
    OldPass_PutBits(in->unk18, 0x43, 1);
    sum = 0;
    for (i = 0; i < 0x44; i++) {
        sum += OldPass_GetBit(i);
    }
    OldPass_PutBits(sum, 0x44, 8);
    for (i = 0; i < 5; i++) {
        gOldPassBits[i + 10] = gOldPassBits[i] ^ gOldPassBits[i + 5];
    }
    for (i = 4; i < 15; i++) {
        gOldPassBits[i] ^= OldPass_MtNext();
    }
    OldPass_BitsToText(0x78);
    return 1;
}

/* Returns the text buffer the encoders write. */
char *OldPass_GetText(void) {
    return gOldPassText;
}

/* Fills `out` with random content from rand() (debug helper; no caller). */
void OldPass_RandomChar(OldPassChar *out) {
    s32 i;

    out->charId = rand() % 129;
    for (i = 0; i < 7; i++) {
        out->item[i] = rand() % 566;
    }
    out->unk20 = rand() % 3;
    out->unk28 = rand() % 7;
    out->unk24 = rand() % 2;
}

/* Fills `out` with random content from rand() (debug helper; no caller). */
void OldPass_RandomShort(OldPassShort *out) {
    out->charId = rand() % 129;
    out->unk04 = rand() % 129;
    out->unk0C = rand() % 3;
    out->amount = (rand() % 0xFFFFF) * 100;
    out->unk14 = rand() % 7;
    out->unk10 = rand() % 2;
}

/* Stripped debug text. */
char *OldPass_GetDebugTextA(void) {
    return "No Debug\n";
}

/* Stripped debug text. */
char *OldPass_GetDebugTextB(void) {
    return "No Debug\n";
}
