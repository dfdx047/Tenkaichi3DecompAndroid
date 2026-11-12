#ifndef BATTLE_STGM_A_B_H
#define BATTLE_STGM_A_B_H

#include "types.h"

/*
 * Memory card operations (src/battle/stgm_a_b.c, 0x116B98..0x1198D8). Proper home: src/sys/mcard.c.
 *
 * A thin layer over Sony's libmc. Every operation is a function the caller invokes once per frame until it
 * returns 1; it advances one library call (or one sceMcSync poll) per invocation. Two globals carry the state:
 * gMcCardCmd (which operation owns the step counter) and gMcCardStep. The user of this layer is the save / load
 * flow at 0x1198D8.. (src/sys/mcflow_a.c).
 */

/* Per-port record. */
typedef struct McCardPort {
    /* 0x00 */ s32 flags;    /* MCCARD_F_* results of the compare / verify operations */
    /* 0x04 */ s32 ret;      /* return value of the last library call */
    /* 0x08 */ s32 result;   /* result of the last sceMcSync */
    /* 0x0C */ s32 type;     /* sceMcGetInfo: card type */
    /* 0x10 */ s32 free;     /* sceMcGetInfo: free clusters */
    /* 0x14 */ s32 format;   /* sceMcGetInfo: formatted */
    /* 0x18 */ s32 fd;       /* open file */
    /* 0x1C */ s32 size;     /* file size (McCard_LoadFile) or clusters still needed (McCard_CalcNeed) */
    /* 0x20 */ void *buf;    /* McCard_LoadFile: the heap block the file was read into */
} McCardPort; /* size 0x24 */

#define MCCARD_F_DIFF 1      /* the size read back differs from the expected one */
#define MCCARD_F_SAME 2      /* McCard_ReadFile read exactly the requested size */

/* gMcCardCmd values: the operation the step counter belongs to. */
#define MCCARD_CMD_INFO 10
#define MCCARD_CMD_CHANGE 11
#define MCCARD_CMD_FORMAT 12
#define MCCARD_CMD_CREATE 14
#define MCCARD_CMD_SAVE 15
#define MCCARD_CMD_READ 16
#define MCCARD_CMD_LOAD 17
#define MCCARD_CMD_CHECK 18
#define MCCARD_CMD_DIR 22
#define MCCARD_CMD_FIND 23
#define MCCARD_CMD_INFO_SYNC 24
#define MCCARD_CMD_INFO_PROBE 25
#define MCCARD_CMD_NEED 26

/* One save (a directory on the card with icon.sys, an icon and the data file). */
typedef struct McCardFile {
    /* 0x000 */ void *icon;          /* icon file image */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ s32 iconSize;
    /* 0x010 */ s32 unk10;
    /* 0x014 */ s32 unk14;
    /* 0x018 */ char dir[0x80];      /* "BASLUS-21678" + name (+ two digits) */
    /* 0x098 */ char sysPath[0x80];  /* "/<dir>/icon.sys" */
    /* 0x118 */ char iconPath[3][0x80]; /* "/<dir>/dbzsm.ico" or dbzsmr.ico; only [0] is used */
    /* 0x298 */ char dataPath[0x80]; /* "/<dir>/<dir>" */
    /* 0x318 */ char title[0x44];    /* icon.sys title (Shift-JIS) */
    /* 0x35C */ char iconName[3][0x40]; /* list / copy / delete icon names for icon.sys */
    /* 0x41C */ u16 titleBreak;      /* byte offset of the title's line break */
} McCardFile;

/* icon.sys as Sony defines it (sceMcIconSys). */
typedef struct McCardIconSys {
    /* 0x000 */ char head[4];        /* "PS2D" */
    /* 0x004 */ u16 unk4;
    /* 0x006 */ u16 titleBreak;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ s32 transRate;
    /* 0x010 */ s32 bgColor[4][4];
    /* 0x050 */ f32 lightDir[3][4];
    /* 0x080 */ f32 lightColor[3][4];
    /* 0x0B0 */ f32 ambient[4];
    /* 0x0C0 */ char title[0x44];
    /* 0x104 */ char viewName[0x40];
    /* 0x144 */ char copyName[0x40];
    /* 0x184 */ char delName[0x40];
    /* 0x1C4 */ u8 unk1C4[0x200];
} McCardIconSys; /* size 0x3C4 */

s32 McCard_GetStep(void);
void McCard_ResetStep(void);
void McCard_Init(void);
s32 McCard_GetInfo(s32 port);
s32 McCard_GetInfoSync(s32 port);
s32 McCard_Probe(s32 port);
s32 McCard_CheckChange(s32 port);
s32 McCard_Format(s32 port);
s32 McCard_CreateSave(s32 port, McCardFile *file);
s32 McCard_WriteSave(s32 port, void *data, s32 size, McCardFile *file);
s32 McCard_ReadFile(s32 port, void *buf, s32 size, McCardFile *file);
s32 McCard_LoadFile(s32 port, McCardFile *file);
s32 McCard_CheckFiles(s32 port, McCardFile *file);
void McCardFile_SetNames(McCardFile *file, char *name, s32 num, s32 unused, s32 replay);
s32 McCard_IsFatalError(s32 code);
s32 McCard_ReadDir(s32 port, s32 maxEntries, void *table);
s32 McCard_FindSystemSave(s32 port);
s32 McCard_FindSave(s32 port, McCardFile *file);
s32 McCard_Checksum(u32 *buf, s32 size, s32 write);
s32 McCard_CalcNeed(s32 port, McCardFile *file, s32 replay);

#endif
