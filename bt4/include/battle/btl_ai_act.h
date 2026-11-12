#ifndef BATTLE_BTL_AI_ACT_H
#define BATTLE_BTL_AI_ACT_H

#include "types.h"

/*
 * CPU opponent: virtual pad, attack actions and "sense" (src/battle/btl_ai_act.c, 0x1BC8A8..0x1C0058).
 *
 * LOCAL VIEW. include/battle/btl_ai_mgr.h (BtlAiMgr*) and include/battle/btl_ai.h (BtlAi*) describe the same heap
 * block; this header does not include either and uses its own type prefix (AiAct*). Offsets marked (m) are used by
 * matching code in btl_ai_act.c; the others are copied from those two headers.
 */

/* Stack vector: four floats, 8-byte aligned (copied with ld/sd pairs). */
typedef struct AiActVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) AiActVec; /* size 0x10 */

/* One entry of the action stack. */
typedef struct AiActEntry {
    /* 0x00 */ s32 id;  /* (m) action id */
    /* 0x04 */ s8 arg;  /* (m) */
    /* 0x05 */ u8 pad05[3];
} AiActEntry; /* size 8 */

/* Action runner state: side + 0x28. */
typedef struct AiActSeq {
    /* 0x00 */ s32 flags;   /* (m) bit 0x80 becomes situation bit 59 and is cleared in state classes 1..7;
                                   0x400 is tested by BtlAiSeq_IsInterrupted */
    /* 0x04 */ s32 depth;   /* (m) entries on the stack; the running one is stack[depth - 1] */
    /* 0x08 */ s32 phase;   /* (m) 0 init, 1 start, 2 run, 3 end */
    /* 0x0C */ s32 unk0C[2];
    /* 0x14 */ AiActEntry stack[8]; /* (m) */
    /* 0x54 */ u8 unk54[0x44];
} AiActSeq; /* size 0x98 */

/* Move work: side + 0xC0. Only what this file touches. */
typedef struct AiActMove {
    /* 0x000 */ f32 dist[5];     /* (m) [1] and [2] are the close / middle range limits used by sense */
    /* 0x014 */ u8 unk014[0x148];
    /* 0x15C */ s32 flags;       /* (m) bit 0: "not moving", makes the move action end */
    /* 0x160 */ AiActVec lastPos; /* (m) fighter position on the last frame a movement button was sent */
    /* 0x170 */ s32 stillTimer;  /* (m) frames the fighter moved less than its radius while a movement button was
                                        sent; bit 0 of flags is set from 31 on */
    /* 0x174 */ s32 stuckTimer;
    /* 0x178 */ u8 unk178[8];
} AiActMove; /* size 0x180 */

/* Attack work of the three attack actions: side + 0x240. */
typedef struct AiActAtk {
    /* 0x00 */ s32 hold;    /* (m) AI button bits sent as "hold" every frame of the run phase */
    /* 0x04 */ s32 press;   /* (m) AI button bits sent as "press" (every other frame) */
    /* 0x08 */ f32 charge;  /* (m) how long RUSH / BLAST stay held: compared with BtlCharApi_GetChargeRate(side) */
    /* 0x0C */ s32 state;   /* (m) the fighter's state id when the input was chosen */
    /* 0x10 */ s32 flags;   /* (m) AIACT_ATK_* */
    /* 0x14 */ s32 waitTimer;  /* (m) 15: frames to wait for the fighter's state to change */
    /* 0x18 */ s32 idleTimer;  /* (m) 15: frames spent in state class 0 before giving up (option 10) */
    /* 0x1C */ s32 oppKind; /* (m) 0, 1 the opponent is in state class 13/14, 2 the opponent is in class 0x1A.
                                   Read from the OTHER side by BtlAiAtk_RollCharge */
    /* 0x20 */ s32 step;    /* (m) rush step: state - 0x37 or state - 0x3C */
    /* 0x24 */ s32 choice;  /* (m) option picked by BtlAiAtk_PickOption (0..10, 11 none, -1 before). Debug only */
} AiActAtk; /* size 0x28 */

#define AIACT_ATK_WAIT_CLS11 1 /* the run phase ends when the state class becomes 11 */
#define AIACT_ATK_IDLE 2       /* option 10: send nothing, end after idleTimer frames in class 0 */
#define AIACT_ATK_CHAIN 4      /* follow-up action: when it ends, start action 0x19 */

/* The virtual pad: side + 0x268. */
typedef struct AiActPad {
    /* 0x00 */ s32 buttons;   /* (m) battle button word (BTLB_* of btl_input.h) */
    /* 0x04 */ f32 stickX;    /* (m) */
    /* 0x08 */ f32 stickY;    /* (m) */
    /* 0x0C */ s32 mark[14];  /* (m) per AI button bit: bit 0 flips every frame (a "press" is sent only while it
                                     is 0), bit 1 = the "once" request was already sent, bit 2 (mark[3] only) =
                                     special 3 was already done */
    /* 0x44 */ s32 flip;      /* (m) flips every frame; picks between the two buttons of special 1 / 2 */
    /* 0x48 */ f32 accX;      /* (m) stick accumulator of this frame */
    /* 0x4C */ f32 accY;      /* (m) */
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 timer;     /* (m) set to 2 by special 3, counted down every frame; no reader in this file */
} AiActPad; /* size 0x58 */

/* Situation: side + 0x2C0. */
typedef struct AiActStatus {
    /* 0x00 */ u64 flags;     /* (m) situation bits rebuilt every frame by BtlAiSense_Update (AIACT_SIT_*) */
    /* 0x08 */ s32 downTimer; /* (m) 4 while BtlCharApi_CheckUnkCAC is 1, then counted down */
    /* 0x0C */ s32 oppClass;  /* (m) the opponent's state class when bit 29 was last raised */
    /* 0x10 */ s32 oppAction; /* (m) */
    /* 0x14 */ s32 react;     /* (m) persistent bits. Bits 0, 1 and 3 are rebuilt every frame; bits 5..20 are
                                     cleared when the opponent's state class differs from oppClass */
    /* 0x18 */ s32 timer18;   /* (m) */
    /* 0x1C */ s32 timer1C;   /* (m) */
    /* 0x20 */ s32 timer20;   /* (m) */
    /* 0x24 */ f32 unk24;     /* (m) a distance; half of it is compared with the fighter distance */
} AiActStatus; /* size 0x28 */

/* One side's CPU controller. */
typedef struct AiActSide {
    /* 0x000 */ s32 side;      /* (m) object id; side ^ 1 is the opponent */
    /* 0x004 */ s32 aiType;    /* (m) */
    /* 0x008 */ s32 cpuLevel;  /* (m) 0..29, negative for the dummy */
    /* 0x00C */ u8 unk00C[0x0C];
    /* 0x018 */ u8 *param;     /* (m) the character's own AI parameters (AIACT_PRM_*) */
    /* 0x01C */ u8 unk01C[0x0C];
    /* 0x028 */ AiActSeq act;
    /* 0x0C0 */ AiActMove move;
    /* 0x240 */ AiActAtk atk;
    /* 0x268 */ AiActPad pad;
    /* 0x2C0 */ AiActStatus st;
    /* 0x2E8 */ u8 unk2E8[0x238];
} AiActSide; /* size 0x520 */

/* Tables inside the common AI data. */
typedef struct AiActTables {
    /* 0x000 */ u8 unk000[0x288];
    /* 0x288 */ u16 actFlags[0x80];   /* by action id */
    /* 0x388 */ s8 stateClass[0x200]; /* (m) by fighter state id */
    /* 0x588 */ u8 stateFlags[1];     /* (m) by fighter state id; bit 0 makes sense stop early */
} AiActTables;

/* The same tables seen from +8, the form about half of the functions use. */
typedef struct AiActTables8 {
    /* 0x000 */ u8 unk000[0x380];
    /* 0x380 */ u8 stateClass[0x200]; /* (m) */
    /* 0x580 */ u8 stateFlags[1];     /* (m) */
} AiActTables8;

typedef struct AiActData {
    /* 0x00 */ s32 size[0x29];
    /* 0xA4 */ AiActTables *tables;   /* (m) */
    /* 0xA8 */ void *rules[8];
    /* 0xC8 */ u8 *typeTbl[32];       /* (m) per aiType; bytes +0x2AD / +0x56D are read here */
} AiActData;

typedef struct AiActMgr {
    /* 0x000 */ AiActData *data;      /* (m) */
    /* 0x004 */ f32 dist;             /* (m) distance between the fighters */
    /* 0x008 */ f32 radiusSum;
    /* 0x00C */ s32 sight;            /* (m) bit 0: the stage blocks the line between the fighters */
    /* 0x010 */ AiActSide side[2];    /* (m) */
    /* 0xA50 */ s32 frame;
    /* 0xA54 */ s32 flags;
    /* 0xA58 */ u8 padA58[8];
} AiActMgr; /* size 0xA60 */

/* AI button bits (BtlAiPad_Set); BtlAiPad_AddButtons turns them into BTLB_*. */
#define AIACT_BTN_GUARD 0x0001
#define AIACT_BTN_DASH 0x0002
#define AIACT_BTN_BLAST 0x0004
#define AIACT_BTN_RUSH 0x0008
#define AIACT_BTN_UP 0x0010
#define AIACT_BTN_DOWN 0x0020
#define AIACT_BTN_LEFT 0x0040
#define AIACT_BTN_RIGHT 0x0080
#define AIACT_BTN_LOCKON 0x0100
#define AIACT_BTN_CHARGE 0x0200
#define AIACT_BTN_400 0x0400 /* no button of its own; CHARGE | 400 held with R3 pressed gives L3+R3 */
#define AIACT_BTN_ASCEND 0x0800
#define AIACT_BTN_DESCEND 0x1000
#define AIACT_BTN_R3 0x2000

typedef void (*AiActPhaseFunc)(AiActSide *s);

void BtlAiMove_Dispatch(AiActSide *s);
void BtlAiPad_Clear(AiActPad *pad, s32 keep);
void BtlAiPad_AddButtons(AiActPad *pad, s32 bits);
void BtlAiPad_TrackStill(AiActSide *s);
void BtlAiPad_Set(AiActSide *s, s32 hold, s32 press, s32 once, s16 special, f32 x, f32 y);
void BtlAiPad_EndFrame(AiActPad *pad);
void BtlAiAtk_Reset(s32 side, AiActAtk *atk);
f32 BtlAiAtk_RollCharge(AiActSide *s, s8 base, s8 range);
void BtlAiAtk_SetInput(AiActSide *s, u32 kind);
s32 BtlAiAtk_PickOption(AiActSide *s, s8 *lo, s8 *hi);
s32 BtlAiAtk_PickLastStep(AiActSide *s, s8 *lo, s8 *hi);
void BtlAiCombo_Init(AiActSide *s);
void BtlAiCombo_Start(AiActSide *s);
void BtlAiAtk_Run(AiActSide *s);
void BtlAiAtk_End(AiActSide *s);
void BtlAiCombo_Dispatch(AiActSide *s);
void BtlAiFollow_Init(AiActSide *s);
void BtlAiFollow_Start(AiActSide *s);
void BtlAiFollow_End(AiActSide *s);
void BtlAiFollow_Dispatch(AiActSide *s);
void BtlAiAct36_Init(AiActSide *s);
void BtlAiAct36_Start(AiActSide *s);
void BtlAiAct36_Dispatch(AiActSide *s);
s32 BtlAiSense_GetRange(AiActSide *s);
s32 BtlAiSense_CheckBit56(AiActSide *s);
s32 BtlAiSense_CheckBit57(AiActSide *s);
s32 BtlAiSense_CheckBit58(AiActSide *s);
s32 BtlAiSense_CheckBit60(AiActSide *s);
s32 BtlAiSense_CheckBit61(AiActSide *s);
s32 BtlAiSense_IsBehindOpponent(AiActSide *s);
s32 BtlAiSense_CheckBit32(AiActSide *s);
s32 BtlAiSense_CheckSwitch(AiActSide *s);
s32 BtlAiSense_CheckAct10(AiActSide *s);
s32 BtlAiSense_Basic(AiActSide *s);
void BtlAiSense_NoteOppState(AiActSide *s);
s32 BtlAiSense_IsBusy(AiActSide *s);
s32 BtlAiSense_CheckBit49(AiActSide *s);
s32 BtlAiSense_CheckBit50(AiActSide *s);
s32 BtlAiSense_CheckBit51(AiActSide *s);
s32 BtlAiSense_CheckBit43(AiActSide *s);
s32 BtlAiSense_CheckBit40(AiActSide *s);
s32 BtlAiSense_CheckBit41(AiActSide *s);
s32 BtlAiSense_CheckBit28(AiActSide *s);
s32 BtlAiSense_CheckBit29(AiActSide *s);
s32 BtlAiSense_CheckBit35(AiActSide *s);
s32 BtlAiSense_CheckBit31(AiActSide *s);
s32 BtlAiSense_CheckBit33(AiActSide *s);
s32 BtlAiSense_CheckBit34(AiActSide *s);
s32 BtlAiSense_CheckBit36(AiActSide *s);
s32 BtlAiSense_CheckBit37(AiActSide *s);
s32 BtlAiSense_CheckBit38(AiActSide *s);
s32 BtlAiSense_CheckBit39(AiActSide *s);
void BtlAiSense_Reactions(AiActSide *s);
s32 BtlAiSense_IsSteep(AiActSide *s);
void BtlAiSense_Derived(AiActSide *s);
void BtlAiSense_Update(AiActSide *s);

#endif
