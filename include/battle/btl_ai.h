#ifndef BATTLE_BTL_AI_H
#define BATTLE_BTL_AI_H

#include "types.h"

/*
 * CPU player: the one definition of the AI manager block (0xA60 bytes) and of everything inside it. Used by
 * btl_ai_seq.c / btl_ai_cond.c (0x1B4140..0x1B80F8: step handlers, sequence runner, rule conditions) and by
 * btl_ai_mgr.c (0x1BB128..: manager, reset, per-frame driver, the move action; its helper types and prototypes
 * are in battle/btl_ai_mgr.h). Offsets used by matching code are marked (m); the rest is read from the
 * disassembly of neighbouring functions.
 */

/* A vector as btl_ai_mgr.c's locals hold it: Vec4 with the 8-byte (or more) alignment the original type had
   (the struct copies are ld/sd pairs). sys/math3d.h's Vec4 is 4-byte aligned and gives ldl/ldr. */
typedef struct BtlAiVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) BtlAiVec; /* size 0x10 */

/* A path point. Same four floats as Vec4, but the original copied these with unaligned loads
   (ldl/ldr), so the type had 4-byte alignment, unlike the stack vectors. */
typedef struct BtlAiMovePoint {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} BtlAiMovePoint; /* size 0x10 */

/* One entry of the sequence stack. */
typedef struct BtlAiSeqEntry {
    /* 0x00 */ u32 id;   /* (m) sequence id: index of BtlAiActTable.seqFlags; 3, 7, 0x19, 0x1A and 0x36 are run by
                            BtlAi_RunSeq itself */
    /* 0x04 */ s8 arg;   /* (m) argument; for sequence 3: 0, 1 or 2; for the move action: mode * 10 + type */
    /* 0x05 */ u8 pad05[3];
} BtlAiSeqEntry; /* size 8 */

/* Sequence runner state: work + 0x28. Reset by BtlAiSeq_Reset. */
typedef struct BtlAiSeq {
    /* 0x00 */ s32 flags;      /* (m) BTLAI_ACT_* (the move action); 0x40, 0x80, 0x400, 0x800, 0x1000 are set by the
                                  step handlers and conditions */
    /* 0x04 */ s32 depth;      /* (m) entries on the stack; 0 = nothing to run. The top entry is at
                                  (u8 *)seq + depth * 8 + 0xC, i.e. stack[depth - 1] */
    /* 0x08 */ s32 phase;      /* (m) index into gBtlAiStateFuncs: the four phases of a generic sequence */
    /* 0x0C */ s32 stepCount;  /* steps finished (written by the run phase, 0x1B4830) */
    /* 0x10 */ s32 step;       /* (m) index of the current step of the sequence script; step handlers set it to
                                  choose what runs next */
    /* 0x14 */ BtlAiSeqEntry stack[8];
    /* 0x54 */ s32 timer;      /* (m) per-step counter / parameter */
    /* 0x58 */ s32 unk58;      /* (m) cleared by step handler 17 */
    /* 0x5C */ s32 unk5C[3];
    /* 0x68 */ s32 ruleSet;    /* which of the eight rule lists the thinker is evaluating (0x1BAC30) */
    /* 0x6C */ s32 unk6C[2];
    /* 0x74 */ f32 dirX;
    /* 0x78 */ f32 dirZ;
    /* 0x7C */ s32 unk7C;      /* (m) switched on by step handler 23 (0x22..0x35) */
    /* 0x80 */ s32 unk80;
    /* 0x84 */ s32 roll;       /* (m) last percent roll (0..99) of a condition: debug trace, never read here */
    /* 0x88 */ s32 threshold;  /* (m) what that roll was compared with */
    /* 0x8C */ s32 unk8C[3];
} BtlAiSeq; /* size 0x98 */

#define BTLAI_ACT_PATH 0x02          /* following the path instead of heading straight for the target */
#define BTLAI_ACT_BLOCKED_AHEAD 0x04 /* the stage blocks the segment from the fighter to a point ahead of it */
#define BTLAI_ACT_BLOCKED 0x08       /* the stage blocks the segment from the fighter to the move target */
#define BTLAI_ACT_BLOCKED_ID 0x10    /* ... and the blocking thing has an id (BtlAiHit.id != -1) */

/* Path built by StgNav_FindPath (move + 0x20). The last point is the next one to reach. */
typedef struct BtlAiMovePath {
    /* 0x000 */ BtlAiMovePoint pts[16];
    /* 0x100 */ s32 count;
} BtlAiMovePath; /* size 0x104 */

/* Move action state: work + 0xC0. Step handlers 16 and 22 compare 2x / 3x dist[1] with BtlAi.dist. */
typedef struct BtlAiMoveWork {
    /* 0x000 */ f32 dist[5];     /* distance to keep from the opponent, by move type. Set by AiThink_ResetSide (n/m):
                                    [0] = both fighters' radius (BtlCharApi_GetRadius) summed, [1] = d, [2] = d * 5,
                                    [3] = [4] = d * 10 with d = BtlCharApi_GetCloseRange(side) - radius(side) */
    /* 0x014 */ u8 unk014[0x0C];
    /* 0x020 */ BtlAiMovePath path;
    /* 0x124 */ u8 unk124[0x0C];
    /* 0x130 */ s32 pathTimer;   /* frames until the path may be rebuilt (60 after a build) */
    /* 0x134 */ s32 type;        /* entry arg % 10: 0..4, selects dist[] and the steering */
    /* 0x138 */ u8 unk138[0x08];
    /* 0x140 */ BtlAiVec target;
    /* 0x150 */ f32 remain;      /* distance to the next path point / to the opponent when the action started */
    /* 0x154 */ s32 mode;        /* entry arg / 10 when that is 1 or 2 */
    /* 0x158 */ s32 dir;         /* result of BtlAiMove_PickDir (0..4). Written here, not read here */
    /* 0x15C */ s32 flags;       /* bit 0: end the action (set outside this file) */
    /* 0x160 */ u8 unk160[0x14];
    /* 0x174 */ s32 stuckTimer;  /* frames since the action started or a path point was reached; ends at 61 */
    /* 0x178 */ u8 unk178[0x30];
} BtlAiMoveWork; /* size 0x1A8 */

/* The virtual pad, what the CPU "presses" this frame: work + 0x268. Cleared by BtlAiPad_Clear, filled by
   BtlAiPad_Set. */
typedef struct BtlAiOutput {
    /* 0x00 */ u32 buttons;    /* (m) battle button word (BTLB_* in btl_input.h), cleared every frame */
    /* 0x04 */ f32 stickX;     /* (m) */
    /* 0x08 */ f32 stickY;     /* (m) */
    /* 0x0C */ s32 toggle[14]; /* each flipped 0 <-> 1 every frame by 0x1BCD70 (alternating presses) */
    /* 0x44 */ s32 toggle44;   /* flipped as well */
    /* 0x48 */ s32 unk48;      /* cleared every frame */
    /* 0x4C */ s32 unk4C;      /* cleared every frame */
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 timer;      /* counted down every frame */
} BtlAiOutput; /* size 0x58 */

/* Situation: work + 0x2C0. */
typedef struct BtlAiStatus {
    /* 0x00 */ u64 flags;      /* (m) situation bits; a rule only runs when the bit of its group is set */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 oppClass;   /* (m) class of the opponent's current action (BtlAiActTable.actClass) */
    /* 0x10 */ s32 oppAction;  /* (m) opponent's current action id */
    /* 0x14 */ s32 react;      /* (m) reaction bits, set by the conditions through BtlAi_NoteOpponent */
    /* 0x18 */ s32 timer18;    /* (m) set to 90 by condition 33 (arg 0) */
    /* 0x1C */ s32 timer1C;    /* (m) set to 90 by condition 33 (arg 1) */
    /* 0x20 */ s32 timer20;    /* (m) set to 90 by condition 34 */
    /* 0x24 */ s32 unk24;
} BtlAiStatus; /* size 0x28 */

/* Rule evaluation and plan: work + 0x2E8. */
typedef struct BtlAiPlan {
    /* 0x00 */ s32 unk0;       /* result of 0x1B9A08 for the rule being tested */
    /* 0x04 */ s32 unk4;       /* 0..5: which rule list follows list 0 ({1, 2, 3, 5, 6, 7}); a rule of kind 4 sets it */
    /* 0x08 */ s32 unk8;       /* (m) skill slot; -1 = none */
    /* 0x0C */ s32 cond;       /* (m) condition id being tested: index into D_002EDA70 */
    /* 0x10 */ s32 condNo;     /* its position in the rule (0..7) */
    /* 0x14 */ struct {
        s32 roll;
        s32 unk4;
    } rolls[8];                /* eight Rand_Range(100) rolls drawn for every rule group that is tested */
    /* 0x54 */ s32 unk54;      /* (m) written by condition 17 */
    /* 0x58 */ s8 rate[14];    /* (m) per virtual button (0..13): BtlAi_GetPairRate of the AI type's profile */
    /* 0x66 */ u8 unk66[0x3E];
    /* 0xA4 */ s32 cooldown;   /* (m) counted down once per frame; step handler 15 sets 300 */
    /* 0xA8 */ u8 ruleNo;      /* index of the rule being tested */
    /* 0xA9 */ u8 unkA9;
    /* 0xAA */ u8 unkAA;
    /* 0xAB */ u8 unkAB;
    /* 0xAC */ u8 scratch[0x188]; /* (m) cleared whenever the AI type or level is set */
    /* 0x234 */ u8 unk234[4];
} BtlAiPlan; /* size 0x238 */

/* Per-fighter AI work: gBtlAi->work[side]. */
typedef struct BtlAiWork {
    /* 0x000 */ s32 objId;     /* (m) fighter / side number (0 or 1); the opponent is objId ^ 1 */
    /* 0x004 */ s32 type;      /* (m) BattleMember.aiType: index into BtlAiData.profile[] */
    /* 0x008 */ s32 level;     /* (m) BattleMember.cpuLevel: 0..29, -1 = passive dummy */
    /* 0x00C */ s32 unkC;      /* (m) bits 0..2 are tested by conditions 14..16 */
    /* 0x010 */ s32 unk10[2];
    /* 0x018 */ u8 *param;     /* (m) the character's own AI parameters (fighter + 0x934); NULL = no AI */
    /* 0x01C */ s32 unk1C[2];
    /* 0x024 */ s32 flags;     /* (m) bit 0: param is a heap copy owned by the work (Heap_Free on reset) */
    /* 0x028 */ BtlAiSeq seq;
    /* 0x0C0 */ BtlAiMoveWork move;
    /* 0x268 */ BtlAiOutput out;
    /* 0x2C0 */ BtlAiStatus status;
    /* 0x2E8 */ BtlAiPlan plan;
} BtlAiWork; /* size 0x520 */

/* Action tables inside the common AI data. */
typedef struct BtlAiActTable {
    /* 0x000 */ u8 unk0[0x288];
    /* 0x288 */ u16 seqFlags[0x80]; /* (m) per sequence id; bit 2 is tested by condition 18 */
    /* 0x388 */ s8 actClass[1];     /* (m) per fighter action id (fighter + 0x974); 0, 10 and 11 are tested by the move action */
} BtlAiActTable;

/* The common AI data: member 4 of common file 2 (gCommonRes->data[0]), pointers fixed up by 0x1BAD50. */
typedef struct BtlAiData {
    /* 0x00 */ s32 size[0x29];      /* byte sizes of the 1 + 8 + 32 sections */
    /* 0xA4 */ BtlAiActTable *act;  /* (m) */
    /* 0xA8 */ void *rules[8];      /* rule lists: { s16 count; ...; rule *list }, rules of 0x1C bytes */
    /* 0xC8 */ u8 *profile[32];     /* (m) per AI type: rate bytes at +0x2A8.. (level 0) and +0x568.. (level 29); the
                                       button rate tables are at +4 and +0x2C4 */
} BtlAiData;

/* AI manager: 0xA60 bytes from the heap. */
typedef struct BtlAi {
    /* 0x000 */ BtlAiData *data;    /* (m) */
    /* 0x004 */ f32 dist;           /* (m) between the two fighters' centres, refreshed every frame */
    /* 0x008 */ f32 radiusSum;      /* both fighters' BtlCharApi_GetRadius */
    /* 0x00C */ s32 sight;          /* (m) BTLAI_SIGHT_*: stage line test between the two fighters */
    /* 0x010 */ BtlAiWork work[PORT_FIGHTER_CAPACITY];
    /* 0xA50 */ s32 frame;          /* frames the AI has run */
    /* 0xA54 */ s32 flags;          /* (m) bit 0: data is a heap block owned by the manager (Heap_Free on reset) */
    /* 0xA58 */ s32 unkA58[2];
} BtlAi; /* size 0xA60 */

#define BTLAI_SIGHT_BLOCKED 1
#define BTLAI_SIGHT_BLOCKED_ID 2

/* Rule condition: tests the situation; arg is the byte that follows the condition id in the rule. */
typedef s32 (*BtlAiCondFunc)(BtlAiWork *ai, u8 arg);
/* Sequence phase (init, start, run, end). */
typedef void (*BtlAiStateFunc)(BtlAiWork *ai);
/* Step handler: returns non-zero when the step is finished. */
typedef s32 (*BtlAiStepFunc)(BtlAiWork *ai);

void BtlAi_RunSeq(BtlAiWork *ai);
void BtlAi_SendInput(BtlAiWork *ai);
s32 BtlAi_ScaleByLevel(s32 level, s32 lo, s32 hi);
void BtlAi_NoteOpponent(BtlAiWork *ai, s32 react);
s32 BtlAi_GetPairRate(BtlAiWork *ai, u32 kind, s32 off, s8 *base_lo, s8 *base_hi, s32 byGauge);
s32 BtlAi_GetQuadRate(BtlAiWork *ai, u32 kind, s32 off, s8 *base_lo, s8 *base_hi, s32 byGauge);

#endif
