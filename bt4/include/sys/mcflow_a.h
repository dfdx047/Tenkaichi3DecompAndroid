#ifndef SYS_MCFLOW_A_H
#define SYS_MCFLOW_A_H

#include "types.h"

/*
 * Memory-card dialog flows (0x1198D8..0x11EC10, src/sys/mcflow_a.c). Proper file name: sys/mc_flow.c.
 *
 * One work area (gMcFlow, 0x39F90 bytes on heap 2) drives the confirmation dialog (Dialog_*, sys/dialog.c)
 * through one of eight state machines, one per kind of card operation. The card itself is handled by the
 * layer below (0x115478..0x1198D8, stem stgm_a), which is only called here.
 *
 * Use: McFlow_Init(); McFlow_SetDoneCb() / McFlow_SetModeCb(); McFlow_Start(mode); then McFlow_Update()
 * once per frame until it returns 0; McFlow_Term(). The dialog must exist (Dialog_Init) before.
 *
 * Every flow runs as: state 0x23 opens the window, the flow's own states follow, state 0x25 closes the
 * window, state 0x26 waits for the close animation, then state 0 (idle) and a callback.
 */

/* McFlow_Start modes */
#define MCFLOW_MODE_SAVE        0 /* save the system data, asking with message 0xD */
#define MCFLOW_MODE_LOAD        1 /* load the system data after a question */
#define MCFLOW_MODE_BOOT_LOAD   2 /* load the system data without a question */
#define MCFLOW_MODE_NEW_SAVE    3 /* create the system data from defaults */
#define MCFLOW_MODE_SAVE_ALT    4 /* as 0, asking with message 0x20 */
#define MCFLOW_MODE_REPLAY_SAVE 5 /* save the replay into slot `slot` */
#define MCFLOW_MODE_REPLAY_LOAD 6 /* load the replay of slot `slot` */
#define MCFLOW_MODE_REPLAY_SCAN 7 /* read the headers of the seven replay files */
#define MCFLOW_MODE_REPLAY_ASK  8 /* one question (message 0x3D) */
#define MCFLOW_MODE_REPLAY_ERR  9 /* as 7, entered at the "card removed" message */

/* McFlow.flags */
#define MCFLOW_FLAG_DONE     0x008 /* the operation succeeded: selects callback 0 */
#define MCFLOW_FLAG_FOUND    0x010 /* replay scan: at least one valid replay */
#define MCFLOW_FLAG_EMPTY    0x020 /* replay scan: at least one slot without a usable file */
#define MCFLOW_FLAG_NO_DIR   0x040 /* replay scan: at least one slot whose directory check did not return 1 */
#define MCFLOW_FLAG_REMOVED  0x080 /* the card went away: mode callback 3 was run, the scan flow takes over */
#define MCFLOW_FLAG_REFUSED  0x100 /* the player answered "no": selects callback 2 */

#define MCFLOW_NO_CHARA 0xA4 /* empty team member */
#define MCFLOW_REPLAY_SLOTS 7

/* Header in front of a replay file (0x38 bytes). */
typedef struct McFlowHeader {
    /* 0x00 */ u32 sum[2];       /* checksum of the whole file from 0x08 */
    /* 0x08 */ u32 hdrSum[2];    /* checksum of 0x10..0x38 */
    /* 0x10 */ s32 chara[2][5];  /* the two teams (MCFLOW_NO_CHARA = empty) */
} McFlowHeader; /* size 0x38 */

/* The part of the card layer's request block that is filled here. */
typedef struct McFlowReq {
    /* 0x000 */ u8 *icon;        /* icon data inside the resource file */
    /* 0x004 */ s32 unk004[2];
    /* 0x00C */ s32 iconSize;
    /* 0x010 */ u8 unk010[0x308];
    /* 0x318 */ union {
        char text[0x100];        /* icon.sys title, Shift-JIS */
        u64 align;               /* the strcpy into it is done in 8-byte pieces */
    } title;
    /* 0x418 */ s32 unk418;
    /* 0x41C */ s32 titleBreak;  /* byte offset of the line break in the title */
} McFlowReq; /* size 0x420 */

/* The system save as this code sees it (SaveData, include/sys/save.h). Its first 8 bytes are its checksum. */
typedef struct McFlowSave {
    /* 0x0000 */ u64 unk0[0x4000 / 8];
} McFlowSave; /* size 0x4000 */

/* Local view of the replay block (BtlReplay, include/battle/battle_setup.h). */
typedef struct McFlowReplayMember {
    /* 0x00 */ s32 chara;
    /* 0x04 */ u8 unk04[0x60];
} McFlowReplayMember; /* size 0x64 */

typedef struct McFlowReplayTeam {
    /* 0x000 */ u32 count;
    /* 0x004 */ McFlowReplayMember member[5];
    /* 0x1F8 */ u8 unk1F8[0x78];
} McFlowReplayTeam; /* size 0x270 */

typedef struct McFlowReplay {
    /* 0x00000 */ u64 unk0[0xC0 / 8];
    /* 0x000C0 */ McFlowReplayTeam team[2];
    /* 0x005A0 */ u8 unk5A0[0x1ABA8 - 0x5A0];
} McFlowReplay; /* size 0x1ABA8 */

/* A replay file as it is on the card. */
typedef struct McFlowReplayFile {
    /* 0x00000 */ McFlowHeader hdr;
    /* 0x00038 */ McFlowReplay data;
    /* 0x1ABE0 */ u8 unk1ABE0[0x20];
} McFlowReplayFile; /* size 0x1AC00 */

typedef void (*McFlowCb)(s32 arg);

typedef struct McFlow {
    /* 0x00000 */ void *pack;      /* the packed resource file (common file base + 0x1D) */
    /* 0x00004 */ u32 *res;        /* unpacked: offset table; word 1 system-save icon, word 2 replay icon */
    /* 0x00008 */ s32 unk08;
    /* 0x0000C */ s32 flags;       /* MCFLOW_FLAG_* */
    /* 0x00010 */ s32 unk10;
    /* 0x00014 */ s32 cardGone;    /* boot load: 1 after the card was seen missing, 4 when it is too full */
    /* 0x00018 */ s32 unk18;
    /* 0x0001C */ s32 next;        /* state entered after 0x23 (flows 0 / 4, 7 / 9) */
    /* 0x00020 */ s32 mode;        /* MCFLOW_MODE_* */
    /* 0x00024 */ s32 state;       /* 0 = idle */
    /* 0x00028 */ s32 choice;      /* last Dialog_Input result: 1 yes, -2 no, -1 cancel, 0 none */
    /* 0x0002C */ s32 timer;       /* frames */
    /* 0x00030 */ s32 msg;         /* message index given to Dialog_SetMsg every frame */
    /* 0x00034 */ s32 unk34;
    /* 0x00038 */ McFlowSave saveBuf;         /* the system data is read into this */
    /* 0x04038 */ McFlowReplayFile repSave;   /* built by McFlow_Init from the current replay */
    /* 0x1EC38 */ McFlowReplayFile repLoad;   /* a replay is read into this */
    /* 0x39838 */ McFlowHeader slotHdr[MCFLOW_REPLAY_SLOTS]; /* the scan reads each file's header into this */
    /* 0x399C0 */ McFlowReq req;
    /* 0x39DE0 */ void (*flow)(void);         /* the state machine McFlow_Update runs */
    /* 0x39DE4 */ McFlowCb doneCb[4];         /* 0 done, 1 not done, 2 refused; 3 unused */
    /* 0x39DF4 */ s32 doneArg[4];
    /* 0x39E04 */ McFlowCb modeCb[12][4];     /* the same per mode; 3 = "card removed" */
    /* 0x39EC4 */ s32 modeArg[12][4];
    /* 0x39F84 */ s32 unk39F84;
    /* 0x39F88 */ s32 slot;        /* replay slot of flows 5 / 6 */
    /* 0x39F8C */ s32 index;       /* replay slot the scan is at */
} McFlow; /* size 0x39F90 */

extern McFlow *gMcFlow;

void McFlow_ClearSlotInfo(s32 kind, s32 slot);
void McFlow_CopySlotInfo(s32 kind, s32 slot);
void McFlow_SetupRequest(s32 kind, s32 slot);
void McFlow_CallDone(void);
void McFlow_CallModeDone(void);
void McFlow_CallModeRemoved(void);
void McFlow_UpdateSave(void);
void McFlow_UpdateLoad(void);
void McFlow_UpdateBootLoad(void);
void McFlow_UpdateNewSave(void);
void McFlow_UpdateReplayScan(void);
void McFlow_UpdateReplaySave(void);
void McFlow_UpdateReplayLoad(void);
void McFlow_UpdateReplayAsk(void);
void McFlow_Init(void);
void McFlow_Term(void);
void McFlow_Start(s32 mode);
void McFlow_SetDoneCb(s32 idx, McFlowCb cb, s32 arg);
void McFlow_SetModeCb(s32 mode, s32 idx, McFlowCb cb, s32 arg);
void McFlow_SetSlot(s32 slot);
s32 McFlow_PollCard(void);
s32 McFlow_Update(void);

#endif
