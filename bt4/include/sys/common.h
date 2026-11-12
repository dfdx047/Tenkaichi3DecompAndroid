#ifndef SYS_COMMON_H
#define SYS_COMMON_H

#include "types.h"

#define COMMON_FILE_COUNT 3

/* File ids of the always-loaded data (partition 1). */
#define COMMON_FILE_FIRST 2    /* ids 2, 3, 4: reloaded into fixed buffers by Common_Reload */
#define COMMON_FILE_BOOT 5     /* read once by Common_LoadBoot */
#define COMMON_FILE_SND_BANK 0x14B /* sound bank handed to Snd_LoadBank(1, ...) */

/* Always-loaded resources (gCommonRes, 0x70 bytes from the heap; only the first 0x20 are used here). */
typedef struct CommonRes {
    /* 0x00 */ void *boot;                     /* file 5 */
    /* 0x04 */ void *data[COMMON_FILE_COUNT];  /* files 2, 3, 4 */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 size[COMMON_FILE_COUNT];    /* byte sizes of files 2, 3, 4 */
    /* 0x20 */ u8 unk20[0x50];
} CommonRes;

extern CommonRes *gCommonRes;

void Common_LoadBoot(void);
void Common_Reload(void);
void Common_Init(void);

#endif
