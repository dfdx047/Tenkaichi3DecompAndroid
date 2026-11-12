#ifndef SYS_MISC_A_H
#define SYS_MISC_A_H

#include "types.h"

/* Two password codecs for customised characters, used only by the menu overlay (DBZP).
 * Both pack fields into a 32-byte bit buffer (least significant bit first), add a bit-count
 * checksum and a parity block, XOR the buffer from byte 4 on with a key stream, and write it
 * as text with 6 bits per character. Bits 0..31 are the seed and stay in clear.
 * The key stream is the low byte of successive words of an MT19937 state seeded with
 * init_by_array({seed, seed*2, seed*3, seed*4}), read from index 1 and tempered; the state is
 * never twisted. Each codec has its own copy of the state: neither touches gRandState.
 * The seed of a new password is two libc rand() draws: (rand() << 16) | (rand() & 0xFFFF). */

#define PASS_MT_N 624
#define PASS_BUF_SIZE 32   /* bytes in the bit buffer */
#define PASS_CHARSET 64    /* 6 bits per character */

/* --- older format (src/sys/misc_a.c): 32 and 20 character passwords ------------------------ */

/* Content of a 32-character password (0x30 bytes; the menu clears 0x30 before decoding). */
typedef struct OldPassChar {
    /* 0x00 */ s32 charId;   /* 8 bits; the random filler draws 0..128 */
    /* 0x04 */ s32 item[7];  /* 10 bits each; the random filler draws 0..565 */
    /* 0x20 */ s32 unk20;    /* 3 bits; random filler 0..2 */
    /* 0x24 */ s32 unk24;    /* 1 bit */
    /* 0x28 */ s32 unk28;    /* 4 bits; random filler 0..6 */
    /* 0x2C */ s32 unk2C;    /* 1 bit; never set by the random filler */
} OldPassChar;

/* Content of a 20-character password (0x1C bytes). Nothing calls this half. */
typedef struct OldPassShort {
    /* 0x00 */ s32 charId;   /* 8 bits */
    /* 0x04 */ s32 unk04;    /* not in the password: decode writes 0, the random filler 0..128 */
    /* 0x08 */ s32 amount;   /* stored divided by 100 in 20 bits; clamped to 104857500 */
    /* 0x0C */ s32 unk0C;    /* 2 bits; random filler 0..2 */
    /* 0x10 */ s32 unk10;    /* 1 bit */
    /* 0x14 */ s32 unk14;    /* 4 bits; random filler 0..6 */
    /* 0x18 */ s32 unk18;    /* 1 bit; never set by the random filler */
} OldPassShort;

void OldPass_MtSeed(u32 seed);
void OldPass_MtSeedByArray(u32 *key, s32 keyLen);
u32 OldPass_MtNext(void);
void OldPass_ClearBits(void);
void OldPass_PutBits(u32 value, s32 pos, s32 count);
s32 OldPass_GetBit(s32 pos);
u32 OldPass_GetBits(s32 pos, s32 count);
void OldPass_BitsToText(s32 bitCount);
void OldPass_TextToBits(char *text);
void OldPass_Nop(void);
s32 OldPass_DecodeChar(OldPassChar *out, char *text);
s32 OldPass_DecodeShort(OldPassShort *out, char *text);
s32 OldPass_EncodeChar(OldPassChar *in);
s32 OldPass_EncodeShort(OldPassShort *in);
char *OldPass_GetText(void);
void OldPass_RandomChar(OldPassChar *out);
void OldPass_RandomShort(OldPassShort *out);
char *OldPass_GetDebugTextA(void);
char *OldPass_GetDebugTextB(void);

/* --- current format (src/sys/misc_a_b.c): 34 character passwords ---------------------------- */

/* Content of a 34-character password (0x2C bytes; the menu clears 0x2C before decoding). */
typedef struct ChrPassData {
    /* 0x00 */ s32 charId;   /* 8 bits; the menu accepts 0..160 */
    /* 0x04 */ s32 item[8];  /* 9 bits each, 0 = empty; the menu copies them from / to u16[8] */
    /* 0x24 */ s32 extraSlots; /* 4 bits; the menu's check adds it to the character's own slot count (must stay below 8) */
    /* 0x28 */ s32 unk28;    /* 1 bit; never set by the random filler */
} ChrPassData;

void ChrPass_MtSeed(u32 seed);
void ChrPass_MtSeedByArray(u32 *key, s32 keyLen);
u32 ChrPass_MtNext(void);
void ChrPass_ClearBits(void);
void ChrPass_PutBits(u32 value, s32 pos, s32 count);
s32 ChrPass_GetBit(s32 pos);
u32 ChrPass_GetBits(s32 pos, s32 count);
void ChrPass_BitsToText(s32 bitCount);
void ChrPass_TextToBits(char *text);
void ChrPass_Nop(void);
s32 ChrPass_Decode(ChrPassData *out, char *text);
s32 ChrPass_Encode(ChrPassData *in);
char *ChrPass_GetText(void);
void ChrPass_Nop2(void);
void ChrPass_Random(ChrPassData *out);
char *ChrPass_GetDebugText(void);

#endif
