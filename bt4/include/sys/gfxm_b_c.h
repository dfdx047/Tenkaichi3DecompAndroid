#ifndef SYS_GFXM_B_C_H
#define SYS_GFXM_B_C_H

#include "types.h"

/* First 8 bytes of a FOD file: 'F' 'O' 'D' 0x11, then "LIT" and a NUL. */
typedef struct FlashHeader {
    /* 0x00 */ u8 magic[4];
    /* 0x04 */ char kind[4];
} FlashHeader;

/* Header of one tag (may sit at any byte offset, so it is always copied out first). */
typedef struct FlashTag {
    /* 0x00 */ u8 code;  /* 0 ends the tag list */
    /* 0x01 */ u8 unk01;
    /* 0x02 */ u16 count; /* number of records in the tag */
    /* 0x04 */ u32 size;  /* bytes of data after these 8 */
} FlashTag;

/* 3x3 matrix read from the bit stream: [0] [4] scale, [1] [3] skew, [2] [5] translation, [8] = 1. */
typedef struct FlashMtx {
    f32 m[9];
} FlashMtx;

s32 Flash_CheckHeader(u8 *data);
u8 *Flash_FindTag(u8 *data, u8 code);
u8 *Flash_SkipRecords(u8 *tag, u16 count);
u8 *Flash_GetRecord(u8 *data, u8 code, u16 index);
s32 Flash_GetRecordCount(u8 *data, u8 code);
u8 *Flash_SkipNamed(u8 *p, u32 count);
s32 Flash_FindName(u8 *p, char *name);
void Flash_ReadString(char *dst, u8 *p, s32 *ofs);
u32 Flash_ReadBits(u8 *data, u32 *bitPos, u8 bits);
s32 Flash_ReadSBits(u8 *data, u32 *bitPos, u8 bits);
void Flash_MtxIdentity(FlashMtx *m);
void Flash_ReadMtx(u8 *data, FlashMtx *m, s32 *ofs);

#endif
