#ifndef BATTLE_BTL_AI_THINK_H
#define BATTLE_BTL_AI_THINK_H

#include "types.h"

/*
 * CPU opponent: rule conditions 35..117, the rule evaluator, the thinker and the data / side binding
 * (0x1B80F8..0x1BB128, the second half of src/battle/btl_ai_cond.c: one object in the original).
 *
 * LOCAL VIEW. include/battle/btl_ai.h (BtlAi*) and include/battle/btl_ai_mgr.h (BtlAiMgr*) describe the same
 * heap block; they are being merged, so this file does not include either and repeats what it needs under the
 * AiTh prefix. Offsets marked (m) are used by matching code in btl_ai_cond.c.
 */

/* A position as BtlCharApi_GetPos writes it. */
typedef struct AiThVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(8))) AiThVec; /* size 0x10 */

/* One entry of the action stack. */
typedef struct AiThSeqEntry {
    /* 0x00 */ u32 id;
    /* 0x04 */ s8 kind;
} AiThSeqEntry; /* size 8 */

/* Action / sequence state: work + 0x28. */
typedef struct AiThSeq {
    /* 0x00 */ s32 flags;      /* (m) bit 0x100 read by AiThink_GetBlastStep */
    /* 0x04 */ s32 depth;      /* (m) entries on the action stack; the thinker stops as soon as it is non-zero */
    /* 0x08 */ s32 phase;
    /* 0x0C */ s32 stepCount;
    /* 0x10 */ s32 step;
    /* 0x14 */ AiThSeqEntry stack[8];
    /* 0x54 */ s32 timer;
    /* 0x58 */ s32 unk58;
    /* 0x5C */ s32 unk5C[3];
    /* 0x68 */ s32 ruleSet;    /* (m) rule list being evaluated (0..7) */
    /* 0x6C */ s32 firedSet;   /* (m) rule list of the rule that last fired */
    /* 0x70 */ s32 unk70;
    /* 0x74 */ f32 dirX;
    /* 0x78 */ f32 dirZ;
    /* 0x7C */ s32 unk7C;
    /* 0x80 */ s32 firedRule;  /* (m) index of the rule that last fired */
    /* 0x84 */ s32 roll;       /* (m) trace: last roll */
    /* 0x88 */ s32 threshold;  /* (m) trace: what it was compared with */
    /* 0x8C */ s32 firedGroup; /* (m) situation group of the rule that last fired */
    /* 0x90 */ s32 unk90[2];
} AiThSeq; /* size 0x98 */

/* Movement work: work + 0xC0. Only the keep-distances are written here. */
typedef struct AiThRange {
    /* 0x00 */ f32 dist[5];    /* (m) [0] both body radii, [1] d, [2] d * 5, [3] = [4] = d * 10 */
    /* 0x14 */ u8 unk14[0x194];
} AiThRange; /* size 0x1A8 */

/* Situation: work + 0x2C0. Filled by sense (0x1BFF70, outside this file). */
typedef struct AiThStatus {
    /* 0x00 */ u64 flags;      /* (m) bit n set = rules of situation group n may run this frame */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 oppClass;
    /* 0x10 */ s32 oppAction;
    /* 0x14 */ s32 react;
    /* 0x18 */ s32 timer18;
    /* 0x1C */ s32 timer1C;
    /* 0x20 */ s32 timer20;
    /* 0x24 */ s32 unk24;
} AiThStatus; /* size 0x28 */

/* One pre-drawn roll per condition position. */
typedef struct AiThRoll {
    /* 0x00 */ s32 roll;       /* (m) Rand_Range(range), drawn when a new situation group is entered */
    /* 0x04 */ s32 acc;        /* (m) running sum of the rates of the weighted conditions tested at this position */
} AiThRoll; /* size 8 */

/* Rule evaluation state: work + 0x2E8. */
typedef struct AiThPlan {
    /* 0x00 */ s32 cls;        /* (m) class 0..13 of the rule's situation group (AiThink_GetGroupClass), 14 = none */
    /* 0x04 */ s32 next;       /* (m) 0..5: which list follows list 0 ({1, 2, 3, 5, 6, 7}); also selects the weight table */
    /* 0x08 */ s32 slot;       /* (m) chosen move / skill slot, -1 = none */
    /* 0x0C */ s32 cond;       /* (m) condition id being tested (the byte in the rule) */
    /* 0x10 */ s32 condNo;     /* (m) its position in the rule (0..7) */
    /* 0x14 */ AiThRoll rolls[8]; /* (m) */
    /* 0x54 */ s32 sub;        /* (m) 0..3 for the groups 0x1D, 0x24, 0x26, 0x25; written by condition 17 too */
    /* 0x58 */ s8 rate[14];    /* (m) per class: rate of condition 38 (written by BtlAiMgr_BuildRates) */
    /* 0x66 */ u8 pad66[3];
    /* 0x69 */ s8 total[4][14]; /* (m) per weight table and class: sum of the rates of the usable columns. Rows 0..2 are
                                  the weight tables 0..2 by class; row 3 is weight table 5 by `sub` (four entries
                                  written, AiThink_BuildTotals; read by AiThink_GetRollRange with tbl 3) */
    /* 0xA1 */ u8 padA1[3];
    /* 0xA4 */ s32 cooldown;   /* (m) counted down once per frame */
    /* 0xA8 */ u8 ruleNo;      /* (m) index of the rule being tested */
    /* 0xA9 */ u8 unkA9;       /* (m) low byte of the roll of the last basic condition that passed */
    /* 0xAA */ u8 unkAA;       /* (m) group of the last "set next list" rule */
    /* 0xAB */ u8 range;       /* (m) range of the rolls drawn for the current group */
    /* 0xAC */ s32 clsCount[14];    /* (m) statistics: "set next list" rules fired, per class */
    /* 0xE4 */ s32 nextCount[14][6]; /* (m) ... per class and list */
    /* 0x234 */ s32 unk234;
} AiThPlan; /* size 0x238 */

/* Per-fighter AI work: gBtlAi->work[side]. */
typedef struct AiThWork {
    /* 0x000 */ s32 objId;     /* (m) side / battle object id; the opponent is objId ^ 1 */
    /* 0x004 */ s32 type;      /* (m) profile index (BattleMember.aiType) */
    /* 0x008 */ s32 level;     /* (m) 0..29, negative = dummy */
    /* 0x00C */ s32 kit;       /* (m) AITH_KIT_* */
    /* 0x010 */ s32 costMin;   /* (m) smaller of the first two skill costs / 1000 */
    /* 0x014 */ s32 costMax;   /* (m) larger of them / 1000 */
    /* 0x018 */ void *param;   /* (m) the character's own AI parameters (battle object + 0x934); NULL = side not run */
    /* 0x01C */ s32 unk1C[2];
    /* 0x024 */ s32 own;       /* (m) bit 0: param is a heap block owned by the work */
    /* 0x028 */ AiThSeq seq;
    /* 0x0C0 */ AiThRange range;
    /* 0x268 */ u8 out[0x58];
    /* 0x2C0 */ AiThStatus status;
    /* 0x2E8 */ AiThPlan plan;
} AiThWork; /* size 0x520 */

#define AITH_KIT_MOVE6 1 /* one of the two move slots has kind 6 */
#define AITH_KIT_MOVE7 2 /* one has kind 7 (cleared when both are present) */
#define AITH_KIT_UNK4 4  /* BtlCharApi_HasParamBit80(side) is non-zero */

/*
 * A rule. (m) for every field listed; the rest of the record is read by the action starter BtlAiSeq_PushRule.
 */
typedef struct AiThRule {
    /* 0x00 */ s16 group;      /* situation group: bit index into AiThStatus.flags */
    /* 0x02 */ u8 pad02[2];
    /* 0x04 */ u8 cond[8];     /* condition ids; 0xFF ends the list, 0xFE repeats the previous rule's result */
    /* 0x0C */ u8 arg[8];      /* one argument byte per condition */
    /* 0x14 */ u8 kind;        /* 0: stop thinking, 4: choose the next rule list, other: start an action */
    /* 0x15 */ u8 pad15[3];
    /* 0x18 */ u8 param;       /* kind 4: list choice + 0x7F */
    /* 0x19 */ u8 pad19[3];
} AiThRule; /* size 0x1C */

#define AITH_COND_END 0xFF
#define AITH_COND_SAME 0xFE

/* A rule list: one section of the AI data. */
typedef struct AiThRuleList {
    /* 0x00 */ s16 count;      /* (m) */
    /* 0x02 */ u8 pad02[2];
    /* 0x04 */ AiThRule *rules; /* (m) set at bind time to the address right after this header */
} AiThRuleList; /* size 8 */

/* Action table section. */
typedef struct AiThActTable {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ void *unk4;        /* (m) set at bind time to the table at +0x788 */
    /* 0x008 */ u8 unk8[0x280];
    /* 0x288 */ u16 seqFlags[0x80];
    /* 0x388 */ s8 actClass[0x400]; /* (m) per fighter action id */
    /* 0x788 */ u8 unk788[1];
} AiThActTable;

/* The action table seen from its +8, the form AiThCond_ActRateByFlags uses. */
typedef struct AiThActBody {
    /* 0x000 */ u8 unk0[0x380];
    /* 0x380 */ u8 actClass[1];
} AiThActBody;

/*
 * One level end of a profile (0x2C0 bytes). A profile is { s32; AiThRates lo; AiThRates hi; }: lo is used at
 * level 0, hi at level 29, BtlAi_ScaleByLevel interpolates. Offsets (m) through the pointers the code forms.
 */
typedef struct AiThRates {
    /* 0x000 */ s8 basic[14][8];   /* by class; columns by condition 35..39 through D_002EDC70 */
    /* 0x070 */ s8 tblA[14][8];    /* weight table 0, columns D_002EDCE0 */
    /* 0x0E0 */ s8 tblB[14][8];    /* weight table 1, columns D_002EDD50 */
    /* 0x150 */ u8 act[36];        /* conditions 57..77, index through D_002EDDC0 */
    /* 0x174 */ s8 tblC[14][4];    /* weight table 2, columns D_002EDDD8 */
    /* 0x1AC */ s8 tblD[20][8];    /* weight table 3 (by sub), columns D_002EDE10 */
    /* 0x24C */ s8 tblE[14][4];    /* weight table 4, columns D_002EDEB0 */
    /* 0x284 */ s8 tblF[4][8];     /* weight table 5 (by sub), columns D_002EDEE8 */
    /* 0x2A4 */ u8 misc[0x1C];     /* single rates */
} AiThRates; /* size 0x2C0 */

typedef struct AiThProfile {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ AiThRates lo;
    /* 0x2C4 */ AiThRates hi;
} AiThProfile; /* size 0x584 */

/* The AI data: member 4 of common file 2. */
typedef struct AiThData {
    /* 0x00 */ s32 size[41];        /* (m) byte sizes: action table, 8 rule lists, 32 profiles */
    /* 0xA4 */ AiThActTable *act;   /* (m) the pointers are filled by AiThink_BindData */
    /* 0xA8 */ AiThRuleList *rules[8];
    /* 0xC8 */ u8 *profile[32];     /* NULL where the size is 0 */
} AiThData; /* size 0x148; the sections follow */

/* The manager block. */
typedef struct AiThMgr {
    /* 0x000 */ AiThData *data;     /* (m) */
    /* 0x004 */ f32 distance;       /* (m) */
    /* 0x008 */ f32 radiusSum;      /* (m) */
    /* 0x00C */ s32 sight;
    /* 0x010 */ AiThWork work[PORT_FIGHTER_CAPACITY];   /* (m) */
    /* 0xA50 */ s32 frame;          /* (m) */
    /* 0xA54 */ s32 own;            /* (m) bit 0: data is a heap block owned by the manager */
    /* 0xA58 */ s32 unkA58[2];
} AiThMgr; /* size 0xA60 */

/* Part of the fighter data BtlCharApi_GetMoveTable returns: the two "move" slots. */
typedef struct AiThChrMoves {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ s16 id[2];      /* (m) */
    /* 0x14 */ u8 unk14[0x80];
    /* 0x94 */ s8 stock[2];    /* (m) stock units (of 100000) the move needs */
    /* 0x96 */ u8 unk96[8];
    /* 0x9E */ s8 kind[2];     /* (m) */
} AiThChrMoves;

/* Part of the fighter data BtlCharApi_GetSkillTable returns: the three skill slots (the third only with fighter flag 6). */
typedef struct AiThChrSkills {
    /* 0x000 */ u32 state[3];  /* bit 0x100 */
    /* 0x00C */ u32 flags[3];  /* (m) bits 0x2000, 0x8000000 */
    /* 0x018 */ s16 unk18[3];  /* 0x280 is tested */
    /* 0x01E */ s16 power[3];  /* compared with the opponent's current skill */
    /* 0x024 */ u8 unk24[0x6F];
    /* 0x093 */ s8 rank[3];    /* tie-break of the same comparison */
    /* 0x096 */ u8 unk96[0xCF];
    /* 0x165 */ s8 kind[3];    /* (m) */
    /* 0x168 */ u8 unk168[0x34];
    /* 0x19C */ s32 cost[3];   /* (m) */
} AiThChrSkills;

/* Part of a battle object (BtlObj_Get). */
typedef struct AiThObj {
    /* 0x000 */ u8 unk0[0x934];
    /* 0x934 */ void *param;   /* (m) the character's AI parameters ("tpChar->data.pparam_com") */
} AiThObj;

/* Rule condition. */
typedef s32 (*AiThCondFunc)(AiThWork *ai, u8 arg);

s32 AiThink_GetSubRate(AiThWork *ai, s32 col, s8 *lo, s8 *hi);
s32 AiThink_GetSubRate4(AiThWork *ai, s32 col, s8 *lo, s8 *hi);
s32 AiThink_FindBasicColumn(s32 cls, s32 code);
s32 AiThink_TestBasic(AiThWork *ai);
s32 AiThink_FindWeightColumn(AiThPlan *plan);
s32 AiThink_TestWeighted(AiThWork *ai, s32 byGauge);
s32 AiThink_TestRate(AiThWork *ai, s32 cond);
s32 AiThink_GetGroupClass(s32 group);
s32 AiThink_HasMoveKind(AiThWork *ai, s32 kind);
s32 AiThink_HasSkillKind(AiThWork *ai, s32 kind);
s32 AiThink_HasAbility(AiThWork *ai, s32 mask);
s32 AiThink_IsCodeUsable(AiThWork *ai, s32 code);
s32 AiThink_SumPairRates(AiThWork *ai, u32 cls, s8 *lo, s8 *hi, u8 *codes);
s32 AiThink_SumQuadRates(AiThWork *ai, u32 cls, s8 *lo, s8 *hi, u8 *codes);
s32 AiThink_SumSubRates(AiThWork *ai, s32 sub);
void AiThink_BuildTotals(AiThWork *ai);
s32 AiThink_GetRollRange(AiThWork *ai, s32 tbl);
s32 AiThink_GetCond38Range(AiThWork *ai);
s32 AiThink_RollGroupGate(AiThWork *ai);
void AiThink_EvalRules(AiThWork *ai, AiThRuleList *list);
s32 AiThink_GetBlastStep(AiThWork *ai);
void AiThink_TickCooldown(AiThWork *ai);
void AiThink_Think(AiThWork *ai);
void AiThink_BindData(s32 owned);
void AiThink_ReadKit(AiThWork *ai);
void AiThink_ResetSide(s32 side, s32 owned);

#endif
