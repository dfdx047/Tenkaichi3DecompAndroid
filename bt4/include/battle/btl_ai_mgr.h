#ifndef BATTLE_BTL_AI_MGR_H
#define BATTLE_BTL_AI_MGR_H

#include "types.h"
#include "sys/math3d.h"
#include "battle/btl_ai.h"

/*
 * CPU opponent ("AI") manager and its "move" action: src/battle/btl_ai_mgr.c, 0x1BB128..0x1BC8A8.
 * See the block comment at the top of the C file.
 *
 * The AI block itself (BtlAi, BtlAiWork and what they contain) is defined in battle/btl_ai.h; this header adds
 * what only the manager file uses.
 */

/* Segment handed to the stage line test StgCol_TraceSegment. */
typedef struct BtlAiSegment {
    /* 0x00 */ BtlAiVec from;
    /* 0x10 */ BtlAiVec to;
} BtlAiSegment; /* size 0x20 */

/* Result of the last stage line test (static block gStgColHit, StgCol_GetHit returns it). */
typedef struct BtlAiHit {
    /* 0x00 */ s32 hit;          /* (n/m) 1 when the segment is blocked */
    /* 0x04 */ u8 unk04[0x0C];
    /* 0x10 */ BtlAiVec pos;         /* (n/m) */
    /* 0x20 */ u8 unk20[0x20];   /* (n/m) */
    /* 0x40 */ s32 id;           /* -1: what blocks the segment has no id (inferred: not a breakable object) */
} BtlAiHit;

/* AI button bits as BtlAiPad_Set takes them; BtlAiPad_AddButtons turns them into battle button bits (BTLB_* of
   btl_input.h): 1 GUARD, 2 DASH, 4 BLAST, 8 RUSH, 0x10..0x80 UP DOWN LEFT RIGHT, 0x100 LOCKON, 0x200 CHARGE,
   0x800 ASCEND, 0x1000 DESCEND, 0x2000 R3. Only the ones this file uses are named. */
#define BTLAI_BTN_DASH 0x0002
#define BTLAI_BTN_UP 0x0010
#define BTLAI_BTN_CHARGE 0x0200
#define BTLAI_BTN_ASCEND 0x0800
#define BTLAI_BTN_DESCEND 0x1000

/* Partial view of a fighter (BtlChar_Get): the one field this file reads directly. */
typedef struct BtlAiMgrChr {
    /* 0x000 */ u8 unk000[0x444];
    /* 0x444 */ f32 yaw;         /* angle the stick directions are relative to */
} BtlAiMgrChr;

void BtlAiMgr_Reset(void);
void BtlAiMgr_ResetSide(s32 side);
void BtlAiMgr_Init(void);
void BtlAiMgr_Term(void);
void BtlAiMgr_BuildRates(BtlAiWork *s);
s32 BtlAiMgr_GetType(s32 side);
void BtlAiMgr_SetType(s32 side, s32 aiType);
s32 BtlAiMgr_GetLevel(s32 side);
void BtlAiMgr_SetLevel(s32 side, s32 cpuLevel);
s32 BtlAiMgr_IsIdleState(void);
void BtlAiMgr_UpdateSight(void);
s32 BtlAiMgr_GetFrame(void);
void BtlAiMgr_Update(void);

s32 BtlAiMove_PickDir(BtlAiWork *s, s32 mode);
void BtlAiMove_CalcTarget(BtlAiWork *s);
s32 BtlAiMove_IsNear(BtlAiWork *s, BtlAiVec *p, s32 flat);
s32 BtlAiMove_BuildPath(BtlAiWork *s);
s32 BtlAiMove_Check(BtlAiWork *s);
void BtlAiMove_Steer(BtlAiWork *s, BtlAiVec *to, s32 kind);
void BtlAiMove_Hold(BtlAiWork *s);
void BtlAiMove_Init(BtlAiWork *s);
void BtlAiMove_Start(BtlAiWork *s);
void BtlAiMove_Run(BtlAiWork *s);
void BtlAiMove_End(BtlAiWork *s);

#endif
