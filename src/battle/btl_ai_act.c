#include "common.h"
#include "battle/cpu_level.h"
#include "battle/btl_ai_act.h"
#include "sys/rand.h"

/*
 * CPU opponent: the virtual pad, three attack actions and "sense": 0x1BC8A8..0x1C0058.
 *
 * Virtual pad (side + 0x268). BtlAi_RunSeq clears buttons and stick at the start of the frame (BtlAiPad_Clear
 * with keep = 1), runs the action on top of the stack, then calls BtlAiPad_EndFrame. Actions write the pad only
 * through BtlAiPad_Set(side, hold, press, once, special, x, y), any number of times per frame:
 *   hold   AI button bits sent on every call;
 *   press  sent only on frames where the button's mark bit 0 is clear. EndFrame flips that bit every frame, so
 *          a "press" is a button that is down on alternate frames (a fresh press every second frame);
 *   once   sent one time, then mark bit 1 blocks it until the marks are cleared (BtlAiPad_Clear with keep = 0,
 *          which the actions call when they start);
 *   special 1 / 2: the whole button word is replaced by right stick up / down, or by right stick left on the
 *          frames where pad->flip is set; 3: once per mark clear, RUSH is removed and pad->timer set to 2;
 *   CHARGE | bit 0x400 held together with R3 pressed replaces the word by L3+R3;
 *   x, y   added to the frame's accumulator, which is halved: stick = (previous sum + (x, y)) / 2. With one
 *          call per frame the stick is half of what was asked.
 * Every call ends in BtlAiPad_TrackStill.
 *
 * Attack actions. Each is a table of four phase functions indexed by act.phase, behind a dispatcher called by
 * BtlAi_RunSeq for its action id: 0x19 (BtlAiCombo_*), 0x1A (BtlAiFollow_*), 0x36 (BtlAiAct36_*). They share
 * the work at side + 0x240, the run phase BtlAiAtk_Run and the input chooser BtlAiAtk_SetInput.
 *
 * Sense (BtlAiSense_Update, called by BtlAiMgr_Update before the rules are evaluated) rebuilds the 64-bit
 * situation word at side + 0x2C0 from both fighters. The bit list is at BtlAiSense_Update.
 *
 * Randomness: Rand_Range only. BtlAiAtk_RollCharge 1 to 2 draws, BtlAiAtk_SetInput 1 (kinds 3, 5, 8) plus the
 * RollCharge draws (kinds 0..3), BtlAiAtk_PickOption 1, BtlAiAtk_PickLastStep 1, BtlAiFollow_Start 2,
 * BtlAiAct36_Start 1. Sense draws nothing.
 */

/* First byte of the character's AI parameters. */
typedef struct AiActPrm {
    u8 bit0 : 1; /* picks the first or second base of a charge row */
    u8 bit1 : 1; /* the same for action 0x36 */
} AiActPrm;

typedef struct AiActRow {
    s8 v[3]; /* base for parameter bit 0 clear, for bit 0 set, random range */
} AiActRow;

extern AiActMgr *gBtlAi;
extern AiActPhaseFunc gBtlAiMovePhases[4];
extern AiActPhaseFunc gBtlAiComboPhases[4];
extern AiActPhaseFunc gBtlAiFollowPhases[4];
extern AiActPhaseFunc gBtlAiAct36Phases[4];

/* Charge rows, indexed by the fighter parameter bytes 0x84.. (rush step), 0x8A / 2 or 0x8D / 2; row 6 is used
   directly by input 3 and by action 0x36. Original address 0x2EE270: the first thing in this object's .rodata
   (it has to be defined here for the jump table of BtlAiAtk_SetInput to land on its 16-byte boundary). */
static const AiActRow sAiActChargeRows[8] = {
    { { 0, 0, 12 } }, { { 10, 3, 12 } }, { { 10, 3, 12 } }, { { 0, 0, 0 } },
    { { 10, 3, 12 } }, { { 0, 0, 12 } }, { { 10, 3, 12 } }, { { 0, 0, 0 } },
};

extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern void Vec3_Sub(AiActVec *dst, AiActVec *a, AiActVec *b);
extern void Vec3_Normalize(AiActVec *dst, AiActVec *src);
extern f32 Vec3_Length(AiActVec *v);
extern f32 Vec3_Dot(AiActVec *a, AiActVec *b);
extern f32 Vec3_Dist(AiActVec *a, AiActVec *b); /* distance between two points */

extern s32 BtlSeq_GetState(void);
extern s32 BtlAi_ScaleByLevel(s32 level, s32 lo, s32 hi);
extern s32 BtlAiSeq_IsInterrupted(AiActSide *s, s32 actionId); /* the action must stop (by actFlags and state class) */

/* Fighter accessors by object id (btl_char_api.c and the not yet decompiled 0x204E78.. object). */
extern s32 BtlCharApi_GetUnk974(s32 objId);            /* state id */
extern u64 BtlCharApi_GetUnk1288(s32 objId);           /* 64-bit word at fighter +0x1288 */
extern s32 BtlCharApi_CheckUnkCAC(s32 objId);
extern s32 BtlCharApi_GetParamByte84(s32 objId, u32 n);
extern s32 BtlCharApi_GetParamByte8A(s32 objId);
extern s32 BtlCharApi_GetParamByte8D(s32 objId);
extern s32 BtlCharApi_TestFlag05(s32 objId);
extern void BtlCharApi_GetPos(s32 objId, AiActVec *out);   /* position */
extern void BtlCharApi_GetRot(s32 objId, AiActVec *out);   /* rotation (fighter +0x20) */
extern f32 BtlCharApi_GetSpeed(s32 objId);                   /* fighter +0xA8 (a speed) */
extern f32 BtlCharApi_GetChargeRate(s32 objId);                   /* max(0, fighter +0xD78, fighter +0xDEC) */
extern f32 BtlCharApi_GetRadius(s32 objId);                   /* body radius */
extern s32 BtlCharApi_IsInClashA(s32 objId);                   /* action id in 0x130..0x132 */
extern s32 BtlCharApi_IsUnkCACPaired(s32 objId);
extern s32 BtlCharApi_GetAction(s32 objId);                   /* action id (fighter +0x948) */
extern s32 BtlCharApi_GetBlastShots(s32 objId);                   /* fighter +0xDE0 */
extern s32 BtlCharApi_GetBlastRoom(s32 objId);
extern s32 BtlCharApi_GetBlastRoomB(s32 objId);
extern s32 BtlCharApi_TestFlag66(s32 objId);                   /* fighter flag 0x66 */
extern s32 BtlCharApi_FindIncomingBlast(s32 objId, s32 mode);         /* index of an incoming projectile, -1 none */
extern f32 BtlCharApi_GetTechniqueProgress(s32 objId);                   /* progress of the technique in use, -1 none */
extern u32 BtlCharApi_GetAttackAttr(s32 objId);                   /* attribute word of the technique in use */
extern s32 BtlCharApi_GetHp(s32 objId);                   /* health */
extern s32 BtlCharApi_GetHpMax(s32 objId);                   /* health maximum */
extern s32 BtlCharApi_CanTransform(s32 objId);
extern s32 BtlCharApi_CanFuse(s32 objId);
extern s32 BtlCharApi_CanSwitch(s32 objId);
extern s32 BtlCharApi_IsUnk1070Set(s32 objId);                   /* fighter +0x1070 != -30 */
extern s32 BtlCharApi_GetTransformCost(s32 objId);                   /* gauge cost: parameter byte * 100000 */
extern s32 BtlCharApi_IsChangingForm(s32 objId);                   /* action id in 0xEC..0xF2 (changing form) */
extern s32 BtlCharApi_GetParamFlags(s32 objId);                   /* parameter word +0x10 */
extern s32 BtlCharApi_GetOppSkillKind(s32 objId);                   /* kind byte of the opponent's technique, -1 none */
extern s32 BtlCharApi_IsOppSkillFlag4(s32 objId);
extern s32 BtlCharApi_GetOppMoveKind(s32 objId);
extern s32 BtlCharApi_GetStunTimer(s32 objId);                   /* fighter +0xFE0 */
extern s32 BtlCharApi_GetPromptButtons(s32 objId);                   /* buttons a switch prompt accepts, 0 none */
extern s32 BtlCharApi_GetUnk1290(s32 objId);
extern s32 BtlCharApi_IsUnk106CLow(s32 objId);                   /* fighter +0x106C < -29 */
extern s32 BtlCharApi_GetArmorBreakLevel(s32 objId);
extern s32 BtlCharApi_GetParamByte2(s32 objId);                   /* parameter byte +2 */
extern s32 BtlCharApi_TestFlagBE(s32 objId);                   /* fighter flag 0xBE */
extern s32 BtlSide_GetKi(s32 side);                    /* gauge block +0xC */
extern s32 BtlSide_GetBlast(s32 side);                    /* gauge block +0x14 */
extern s32 BtlSide_IsPoweredUp(s32 side);                    /* fighter flag 6 */

/* The running entry of the action stack. */
#define AIACT_TOP(act) ((AiActEntry *)((u8 *)(act) + ((act)->depth << 3) + 0xC))

/* Both fighters' state ids and state classes, the way six functions here fetch them: two local arrays
   (s32 state[2]; s8 cls[2]; index 0 own, 1 opponent). */
#define AIACT_GET_STATES(s, tbl, state, cls)                                   \
    (state)[0] = BtlCharApi_GetUnk974((s)->side);                              \
    (state)[1] = BtlCharApi_GetUnk974(TagTeam_Target((s)->side));                          \
    (cls)[0] = ((AiActTables8 *)((u8 *)(tbl) + 8))->stateClass[(state)[0]];    \
    (cls)[1] = ((AiActTables8 *)((u8 *)(tbl) + 8))->stateClass[(state)[1]]

/* Action 7 (move): ends the action when BtlAiSeq_IsInterrupted says so, else runs its current phase. */
void BtlAiMove_Dispatch(AiActSide *s) {
    AiActSeq *act = &s->act;

    if (act->depth > 0 && BtlAiSeq_IsInterrupted(s, AIACT_TOP(act)->id)) {
        act->depth = 0;
        return;
    }
    gBtlAiMovePhases[act->phase](s);
}

/* Clears this frame's buttons, stick and stick accumulator; with keep == 0 also the marks and the timer. */
void BtlAiPad_Clear(AiActPad *pad, s32 keep) {
    s32 i;

    if (keep == 0) {
        for (i = 15; i >= 0; i--) {
            pad->mark[i] = 0;
        }
        pad->flip = 0;
        pad->timer = 0;
    }
    pad->buttons = 0;
    pad->stickX = 0.0f;
    pad->stickY = 0.0f;
    pad->accX = 0.0f;
    pad->accY = 0.0f;
}

/* Adds AI button bits to the pad as battle button bits. */
void BtlAiPad_AddButtons(AiActPad *pad, s32 bits) {
    if (bits & AIACT_BTN_GUARD) {
        pad->buttons |= 1;
    }
    if (bits & AIACT_BTN_DASH) {
        pad->buttons |= 2;
    }
    if (bits & AIACT_BTN_BLAST) {
        pad->buttons |= 4;
    }
    if (bits & AIACT_BTN_RUSH) {
        pad->buttons |= 8;
    }
    if (bits & AIACT_BTN_UP) {
        pad->buttons |= 0x10;
    }
    if (bits & AIACT_BTN_DOWN) {
        pad->buttons |= 0x20;
    }
    if (bits & AIACT_BTN_LEFT) {
        pad->buttons |= 0x40;
    }
    if (bits & AIACT_BTN_RIGHT) {
        pad->buttons |= 0x80;
    }
    if (bits & AIACT_BTN_LOCKON) {
        pad->buttons |= 0x4000;
    }
    if (bits & AIACT_BTN_CHARGE) {
        pad->buttons |= 0x200;
    }
    if (bits & AIACT_BTN_ASCEND) {
        pad->buttons |= 0x800;
    }
    if (bits & AIACT_BTN_DESCEND) {
        pad->buttons |= 0x1000;
    }
    if (bits & AIACT_BTN_R3) {
        pad->buttons |= 0x2000;
    }
}

/* While DASH, ASCEND or DESCEND is being sent: counts the frames on which the fighter moved less than its
   radius, and raises move flag 0 after 30 of them. Otherwise clears the counter and the flag. */
void BtlAiPad_TrackStill(AiActSide *s) {
    AiActVec pos;
    AiActVec d;
    AiActMove *m = &s->move;
    f32 len;

    if (s->pad.buttons & 0x1802) {
        BtlCharApi_GetPos(s->side, &pos);
        Vec3_Sub(&d, &pos, &m->lastPos);
        len = Vec3_Length(&d);
        if (len < BtlCharApi_GetRadius(s->side)) {
            if (++m->stillTimer > 30) {
                m->flags |= 1;
            }
        }
        m->lastPos = pos;
    } else {
        m->stillTimer = 0;
        m->flags &= ~1;
    }
}

/* Writes the virtual pad (see the header comment of this file). */
void BtlAiPad_Set(AiActSide *s, s32 hold, s32 press, s32 once, s16 special, f32 x, f32 y) {
    AiActPad *pad = &s->pad;
    s32 i;

    for (i = 0; i < 14; i++) {
        if (!(pad->mark[i] & 2)) {
            s32 bits = once & (1 << i);

            if (bits) {
                BtlAiPad_AddButtons(pad, bits);
                pad->mark[i] |= 2;
            }
        }
        if (!(pad->mark[i] & 1)) {
            BtlAiPad_AddButtons(pad, press & (1 << i));
        }
        BtlAiPad_AddButtons(pad, hold & (1 << i));
    }
    if ((hold & 0x600) == 0x600 && (press & AIACT_BTN_R3)) {
        pad->buttons = 0x8000;
    }
    switch (special) {
    case 1:
        if (!(pad->flip & 1)) {
            pad->buttons = 0x10000;
        } else {
            pad->buttons = 0x40000;
        }
        break;
    case 2:
        if (!(pad->flip & 1)) {
            pad->buttons = 0x20000;
        } else {
            pad->buttons = 0x40000;
        }
        break;
    case 3:
        if (!(pad->mark[3] & 4)) {
            pad->timer = 2;
            pad->mark[3] |= 4;
            pad->buttons &= ~8;
        }
        break;
    }
    pad->stickX = (pad->accX + x) * 0.5f;
    pad->stickY = (pad->accY + y) * 0.5f;
    pad->accX = pad->stickX;
    pad->accY = pad->stickY;
    BtlAiPad_TrackStill(s);
}

/* End of the AI frame: counts the timer down and flips every mark's bit 0. */
void BtlAiPad_EndFrame(AiActPad *pad) {
    s32 i;

    if (pad->timer > 0) {
        pad->timer--;
    }
    for (i = 0; i < 14; i++) {
        pad->mark[i] ^= 1;
    }
    pad->flip ^= 1;
}

/* Clears the chosen input and remembers the fighter's state id. */
void BtlAiAtk_Reset(s32 side, AiActAtk *atk) {
    atk->hold = 0;
    atk->press = 0;
    atk->charge = 0.0f;
    atk->state = BtlCharApi_GetUnk974(side);
    atk->flags &= ~7;
    atk->waitTimer = 15;
    atk->idleTimer = 15;
}

/* Rolls how long an attack button is held. One Rand_Range(100) against a chance of 10..50 by level step
   (+20 when the opponent's oppKind is 2). On success: base * 0.1 (or, oppKind 2, a second roll between 1.0 and
   1.1). On failure: Rand_Range(range) * 0.1, 0 when range is 0. */
f32 BtlAiAtk_RollCharge(AiActSide *s, s8 base, s8 range) {
    s32 roll = Rand_Range(100);
    s32 chance[5] = { 10, 20, 30, 40, 50 };
    AiActSide *opp = &gBtlAi->side[TagTeam_Target(s->side)];
    s32 kind = opp->atk.oppKind;
    f32 ret = 0.0f;
    s32 thr;

    if (kind == 2) {
        thr = chance[CpuLevel_TableIndex(s->cpuLevel)] + 20;
    } else {
        thr = chance[CpuLevel_TableIndex(s->cpuLevel)];
    }
    if (roll < thr) {
        if (kind == 2) {
            if ((s32)Rand_Range(100) < thr) {
                if (range > 0) {
                    ret = 1.0f;
                } else {
                    ret = 0.0f;
                }
            } else {
                ret = 1.1f;
            }
        } else {
            ret = base * 0.1f;
        }
    } else if (range != 0) {
        ret = (s32)Rand_Range(range) * 0.1f;
    }
    return ret;
}

/* Writes the input for one of nine attack inputs into the attack work. Does nothing unless the fighter is in a
   rush step (state 0x37..0x3A, 0x3C..0x3F) or in state 0x1B. */
void BtlAiAtk_SetInput(AiActSide *s, u32 kind) {
    AiActAtk *atk = &s->atk;
    s32 action = BtlCharApi_GetAction(s->side);
    s32 state = BtlCharApi_GetUnk974(s->side);
    s32 row = 0;
    u8 bit = s->param[0] & 1;
    f32 t;

    if ((u32)(state - 0x37) < 4 || (u32)(state - 0x3C) < 4) {
        row = BtlCharApi_GetParamByte84(s->side, state < 0x3C ? state - 0x37 : state - 0x3C);
    } else if (state == 0x1B) {
        if (kind == 0) {
            if (action == 0x1F) {
                row = BtlCharApi_GetParamByte8D(s->side) / 2;
            } else {
                row = BtlCharApi_GetParamByte8A(s->side) / 2;
            }
        }
    } else {
        return;
    }
    switch (kind) {
    case 0:
        atk->charge = BtlAiAtk_RollCharge(s, sAiActChargeRows[row].v[bit], sAiActChargeRows[row].v[2]);
        if (atk->charge != 0.0f) {
            atk->hold |= AIACT_BTN_BLAST;
        } else {
            atk->press |= AIACT_BTN_BLAST;
        }
        break;
    case 1:
    case 2:
        t = BtlAiAtk_RollCharge(s, sAiActChargeRows[0].v[bit], sAiActChargeRows[0].v[2]);
        atk->charge = t;
        atk->hold |= kind == 1 ? AIACT_BTN_UP : AIACT_BTN_DOWN;
        if (t == 0.0f) {
            atk->press |= AIACT_BTN_BLAST;
        } else {
            atk->hold |= AIACT_BTN_BLAST;
        }
        break;
    case 3:
        atk->charge = BtlAiAtk_RollCharge(s, sAiActChargeRows[6].v[bit], sAiActChargeRows[6].v[2]);
        if (atk->charge == 0.0f) {
            atk->charge = 0.1f;
        }
        atk->hold |= AIACT_BTN_RUSH;
        {
            s32 dir = Rand_Range(4);

            atk->hold |= 1 << (dir + 4);
        }
        break;
    case 4:
        atk->hold |= AIACT_BTN_GUARD;
        break;
    case 5:
        atk->press |= AIACT_BTN_GUARD;
        atk->hold |= Rand_Range(2) == 0 ? AIACT_BTN_LEFT : AIACT_BTN_RIGHT;
        break;
    case 6:
        atk->press |= AIACT_BTN_DASH;
        break;
    case 7:
        atk->flags |= AIACT_ATK_WAIT_CLS11;
        atk->hold |= AIACT_BTN_UP;
        atk->press |= AIACT_BTN_DASH;
        break;
    case 8:
        atk->press |= AIACT_BTN_DASH;
        atk->hold |= Rand_Range(2) == 0 ? AIACT_BTN_LEFT : AIACT_BTN_RIGHT;
        break;
    }
}

/* Picks one of 11 options from the 12-byte row of the current rush step: one Rand_Range(100), compared with
   the running sum of the level-scaled weights. Returns 0 when the roll is above the whole sum. */
s32 BtlAiAtk_PickOption(AiActSide *s, s8 *lo, s8 *hi) {
    AiActAtk *atk = &s->atk;
    AiActStatus *st = &s->st;
    s32 state = BtlCharApi_GetUnk974(s->side);
    s32 step = state < 0x3C ? state - 0x37 : state - 0x3C;
    s32 roll = Rand_Range(100) + 1;
    s32 sum;

    atk->choice = -1;
    lo += step * 12;
    hi += step * 12;
    sum = BtlAi_ScaleByLevel(s->cpuLevel, lo[0], hi[0]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 0);
        atk->choice = 0;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[1], hi[1]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 1);
        atk->choice = 1;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[2], hi[2]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 2);
        atk->choice = 2;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[3], hi[3]);
    if (sum >= roll) {
        atk->choice = 3;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[4], hi[4]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 3);
        atk->choice = 4;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[5], hi[5]);
    if (sum >= roll) {
        atk->choice = 5;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[6], hi[6]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 5);
        atk->choice = 6;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[7], hi[7]);
    if (sum >= roll) {
        if (st->flags & 0x2000000000000000) {
            BtlAiAtk_SetInput(s, 3);
        } else {
            BtlAiAtk_SetInput(s, 6);
        }
        atk->choice = 7;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[8], hi[8]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 7);
        atk->choice = 8;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[9], hi[9]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 8);
        atk->choice = 9;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[10], hi[10]);
    if (sum >= roll) {
        atk->choice = 10;
        atk->flags |= AIACT_ATK_IDLE;
        return 1;
    }
    atk->choice = 11;
    return 0;
}

/* Rush step 4: one Rand_Range(100) against two level-scaled weights. First: replace the whole action stack by
   action 0x21. Second: press DASH. */
s32 BtlAiAtk_PickLastStep(AiActSide *s, s8 *lo, s8 *hi) {
    AiActEntry *e = &s->act.stack[0];
    AiActSeq *act = &s->act;
    AiActAtk *atk = &s->atk;
    s32 roll = Rand_Range(100) + 1;
    s32 sum;

    sum = BtlAi_ScaleByLevel(s->cpuLevel, lo[0], hi[0]);
    if (sum >= roll) {
        act->phase = 0;
        e->id = 0x21;
        e->arg = 0;
        act->depth = 1;
        return 1;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[1], hi[1]);
    if (sum >= roll) {
        atk->press |= AIACT_BTN_DASH;
        return 1;
    }
    return 0;
}

/* Action 0x19, phase 0: presses RUSH until the fighter's state class is 13 or 14, then starts. */
void BtlAiCombo_Init(AiActSide *s) {
    AiActSeq *act = &s->act;
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(s->side)];

    BtlAiPad_Set(s, 0, AIACT_BTN_RUSH, 0, 0, 0.0f, 0.0f);
    if ((u32)(cls - 13) < 2) {
        act->phase = 1;
        BtlAiCombo_Start(s);
    }
}

/* Action 0x19, phase 1: chooses the input for the current rush step from the character's weight rows. Which
   pair of rows depends on the opponent's state class (0x1A, 13 / 14, other) and on the own class (13 or 14).
   No option picked: press RUSH. */
void BtlAiCombo_Start(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state[2];
    s8 cls[2];
    AiActAtk *atk = &s->atk;
    s32 picked = 0;
    AiActEntry *e = &s->act.stack[0];
    AiActSeq *act = &s->act;
    s32 step;

    AIACT_GET_STATES(s, tbl, state, cls);
    step = state[0] < 0x3C ? state[0] - 0x37 : state[0] - 0x3C;
    BtlAiAtk_Reset(s->side, atk);
    atk->oppKind = 0;
    atk->step = step;
    if (!((u32)((u8)cls[0] - 13) < 2)) {
        act->phase = 3;
        BtlAiAtk_End(s);
        return;
    }
    if (step == 4) {
        if (cls[0] == 13) {
            BtlAiAtk_PickLastStep(s, (s8 *)s->param + 0x44, (s8 *)s->param + 0x13C);
        }
        if (e->id == 0x21) {
            return;
        }
    } else {
        if (cls[1] == 0x1A) {
            atk->oppKind = 2;
            picked = BtlAiAtk_PickOption(s, (s8 *)s->param + 0x88, (s8 *)s->param + 0x180);
        } else if ((u8)(cls[1] - 13) < 2) {
            atk->oppKind = 1;
            picked = BtlAiAtk_PickOption(s, (s8 *)s->param + 0xB8, (s8 *)s->param + 0x1B0);
        } else if (cls[0] == 13) {
            picked = BtlAiAtk_PickOption(s, (s8 *)s->param + 0x14, (s8 *)s->param + 0x10C);
        } else if (cls[0] == 14) {
            picked = BtlAiAtk_PickOption(s, (s8 *)s->param + 0x50, (s8 *)s->param + 0x148);
        }
        if (picked == 0) {
            atk->press |= AIACT_BTN_RUSH;
        }
    }
    act->phase = 2;
    BtlAiPad_Clear(&s->pad, 0);
}

/* Phase 2 of the three attack actions: sends the chosen input until the fighter's state id changes (15 frames
   at most), keeps a held RUSH / BLAST down until BtlCharApi_GetChargeRate reaches atk->charge, then goes back to phase 1
   (action 0x1A: to phase 3). */
void BtlAiAtk_Run(AiActSide *s) {
    AiActSeq *act = &s->act;
    AiActAtk *atk = &s->atk;
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state = BtlCharApi_GetUnk974(s->side);
    f32 held = BtlCharApi_GetChargeRate(s->side);
    AiActEntry *e = AIACT_TOP(act);
    s32 cls = tbl->stateClass[state];

    BtlAiPad_Clear(&s->pad, 1);
    if (atk->flags & AIACT_ATK_IDLE) {
        if (cls != 0) {
            return;
        }
        if (--atk->idleTimer >= 0) {
            return;
        }
        act->phase = 3;
        return;
    }
    if (state != atk->state) {
        if (atk->hold & (AIACT_BTN_BLAST | AIACT_BTN_RUSH)) {
            if (atk->charge <= held || cls == 0) {
                atk->hold = 0;
            }
            if (cls == 8) {
                act->phase = 3;
            }
        } else if (atk->flags & AIACT_ATK_WAIT_CLS11) {
            if (cls == 11) {
                act->phase = 3;
            }
        } else {
            if (e->id == 0x1A) {
                act->phase = 3;
            } else {
                act->phase = 1;
            }
            return;
        }
    } else if (atk->waitTimer > 0) {
        atk->waitTimer--;
    } else {
        act->phase = 3;
    }
    BtlAiPad_Set(s, atk->hold, atk->press, 0, 0, 0.0f, 0.0f);
}

/* Phase 3 of actions 0x19 and 0x36: empties the action stack. */
void BtlAiAtk_End(AiActSide *s) {
    AiActSeq *act = &s->act;

    act->depth = 0;
    act->phase = 0;
}

/* Action 0x19: goes to the end phase when BtlAiSeq_IsInterrupted asks, or when the opponent is not in close range. */
void BtlAiCombo_Dispatch(AiActSide *s) {
    AiActSeq *act = &s->act;

    if (BtlAiSeq_IsInterrupted(s, AIACT_TOP(act)->id)) {
        act->phase = 3;
    }
    if (BtlAiSense_GetRange(s)) {
        act->phase = 3;
    }
    gBtlAiComboPhases[act->phase](s);
}

/* Action 0x1A, phase 0: starts at once. */
void BtlAiFollow_Init(AiActSide *s) {
    s->act.phase = 1;
    BtlAiFollow_Start(s);
}

/* Action 0x1A, phase 1: one Rand_Range(100) against a 5-entry level-scaled row (one pair of rows for fighter
   action 0x1F, another for any other action) and one Rand_Range(3) for a direction. Options: inputs 0, 1, 2,
   "press RUSH (action 0x1F: with UP or DOWN, and chain into action 0x19)", input 0. None: press RUSH. */
void BtlAiFollow_Start(AiActSide *s) {
    AiActSeq *act = &s->act;
    AiActAtk *atk = &s->atk;
    s32 roll = Rand_Range(100) + 1;
    s32 action = BtlCharApi_GetAction(s->side);
    s32 dir = Rand_Range(3);
    s8 *hi;
    s8 *lo;
    s32 sum;

    if (action == 0x1F) {
        lo = (s8 *)s->param + 0x48;
        hi = (s8 *)s->param + 0x140;
    } else {
        lo = (s8 *)s->param + 0x80;
        hi = (s8 *)s->param + 0x178;
    }
    BtlAiAtk_Reset(s->side, atk);
    act->phase = 2;
    BtlAiPad_Clear(&s->pad, 0);
    sum = BtlAi_ScaleByLevel(s->cpuLevel, lo[0], hi[0]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 0);
        return;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[1], hi[1]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 1);
        return;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[2], hi[2]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 2);
        return;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[3], hi[3]);
    if (sum >= roll) {
        atk->press |= AIACT_BTN_RUSH;
        if (action == 0x1F) {
            switch (dir) {
            case 0:
                atk->hold |= AIACT_BTN_UP;
                break;
            case 1:
                atk->hold |= AIACT_BTN_DOWN;
                break;
            }
            atk->flags |= AIACT_ATK_CHAIN;
        }
        return;
    }
    sum += BtlAi_ScaleByLevel(s->cpuLevel, lo[4], hi[4]);
    if (sum >= roll) {
        BtlAiAtk_SetInput(s, 0);
        return;
    }
    atk->press |= AIACT_BTN_RUSH;
}

/* Action 0x1A, phase 3: once the state class is no longer 11, either replaces the stack by action 0x19 (chain
   flag) or empties it. */
void BtlAiFollow_End(AiActSide *s) {
    AiActSeq *act = &s->act;
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(s->side)];
    AiActAtk *atk = &s->atk;
    AiActEntry *e = &s->act.stack[0];

    if (cls != 11) {
        if (atk->flags & AIACT_ATK_CHAIN) {
            act->phase = 0;
            e->id = 0x19;
            e->arg = 0;
            act->depth = 1;
        } else {
            act->depth = 0;
            act->phase = 0;
        }
    }
}

/* Action 0x1A: goes to the end phase when BtlAiSeq_IsInterrupted asks, then runs the phase. */
void BtlAiFollow_Dispatch(AiActSide *s) {
    AiActSeq *act = &s->act;

    if (BtlAiSeq_IsInterrupted(s, AIACT_TOP(act)->id)) {
        act->phase = 3;
    }
    gBtlAiFollowPhases[act->phase](s);
}

/* Action 0x36, phase 0: presses BLAST. Ends unless bit 0x1000 of fighter +0x1288 is set; starts when the
   state class is 0x12. */
void BtlAiAct36_Init(AiActSide *s) {
    AiActSeq *act = &s->act;
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(s->side)];
    u64 w = BtlCharApi_GetUnk1288(s->side);

    BtlAiPad_Set(s, 0, AIACT_BTN_BLAST, 0, 0, 0.0f, 0.0f);
    if (!(w & 0x1000)) {
        act->phase = 3;
    } else if (cls == 0x12) {
        act->phase = 1;
        BtlAiAct36_Start(s);
    }
}

/* Action 0x36, phase 1: one Rand_Range(100) against two level-scaled bytes of the row picked by fighter
   +0xDE0. Outcomes: hold BLAST for a rolled time (needs BtlCharApi_GetBlastRoomB and bit 0x2000 of fighter +0x1288),
   press BLAST (needs BtlCharApi_GetBlastRoom and bit 0x1000), or end. */
void BtlAiAct36_Start(AiActSide *s) {
    AiActSeq *act = &s->act;
    AiActAtk *atk = &s->atk;
    s32 roll = Rand_Range(100) + 1;
    u8 *param = s->param;
    s32 bit = (param[0] & 2) != 0;
    s8 *row;
    s32 canPress;
    s8 *lo;
    s32 canHold;
    s8 *hi;
    u64 w;

    row = (s8 *)param + BtlCharApi_GetBlastShots(s->side) * 2;
    lo = row + 0xE6;
    hi = row + 0x1DE;
    canPress = BtlCharApi_GetBlastRoom(s->side);
    canHold = BtlCharApi_GetBlastRoomB(s->side);
    w = BtlCharApi_GetUnk1288(s->side);
    BtlAiAtk_Reset(s->side, atk);
    act->phase = 2;
    BtlAiPad_Clear(&s->pad, 0);
    if (BtlAi_ScaleByLevel(s->cpuLevel, lo[0], hi[0]) < roll || (canPress == 0 && canHold == 0)) {
        act->phase = 3;
    } else if (BtlAi_ScaleByLevel(s->cpuLevel, lo[1], hi[1]) >= roll && canHold != 0 && (w & 0x2000)) {
        atk->charge = BtlAiAtk_RollCharge(s, sAiActChargeRows[6].v[bit], sAiActChargeRows[6].v[2]);
        atk->hold |= AIACT_BTN_BLAST;
    } else if (canPress == 0 || !(w & 0x1000)) {
        act->phase = 3;
    } else {
        atk->press |= AIACT_BTN_BLAST;
    }
}

/* Action 0x36: goes to the end phase when BtlAiSeq_IsInterrupted asks, then runs the phase. */
void BtlAiAct36_Dispatch(AiActSide *s) {
    AiActSeq *act = &s->act;

    if (BtlAiSeq_IsInterrupted(s, AIACT_TOP(act)->id)) {
        act->phase = 3;
    }
    gBtlAiAct36Phases[act->phase](s);
}

/* Range band from the situation word: 0 close, 1 middle, 2 far, -1 not sensed. */
s32 BtlAiSense_GetRange(AiActSide *s) {
    u64 flags = s->st.flags;

    if (flags & 0x10) {
        return 0;
    }
    if (flags & 0x20) {
        return 1;
    }
    if (flags & 0x40) {
        return 2;
    }
    return -1;
}

/* Situation bit 56: a technique with a gauge cost (BtlCharApi_GetTransformCost) can be used. Outside mode 1's
   BtlCharApi_GetUnk1290: not for 600 frames after a form change, and only when health is at or below a level-scaled
   percentage (parameter bytes 0xF / 0x107). */
s32 BtlAiSense_CheckBit56(AiActSide *s) {
    AiActStatus *st = &s->st;
    s32 cost = BtlCharApi_GetTransformCost(s->side);
    s32 gauge = BtlSide_GetBlast(s->side);
    s32 hp = BtlCharApi_GetHp(s->side);
    f32 ratio = (f32)hp / (f32)BtlCharApi_GetHpMax(s->side);
    s32 limit = BtlAi_ScaleByLevel(s->cpuLevel, (s8)s->param[0xF], (s8)s->param[0x107]);
    s32 frequency=100;
#ifdef PORT
    if (TagTeam_Active()) {
        extern s32 TagTeam_TransformFrequency(void);
        frequency=TagTeam_TransformFrequency();
        if (frequency<=0) return 0;
        limit=limit*frequency/100;
    }
#endif

    if (!BtlCharApi_GetUnk1290(s->side)) {
        if (BtlCharApi_IsChangingForm(s->side)) {
            st->timer18 = 60000/frequency;
            return 0;
        }
        if (--st->timer18 > 0) {
            return 0;
        }
        st->timer18 = -1;
        if ((f32)limit < ratio * 100.0f) {
            return 0;
        }
    }
    if (!BtlCharApi_CanTransform(s->side)) {
        return 0;
    }
    if (cost == 0) {
        return 0;
    }
    if (gauge < cost) {
        return 0;
    }
    return 1;
}

/* Situation bit 57: the same kind of test for BtlCharApi_CanFuse, with the health limit 20 points higher. */
s32 BtlAiSense_CheckBit57(AiActSide *s) {
    AiActStatus *st = &s->st;
    s32 hp = BtlCharApi_GetHp(s->side);
    f32 ratio = (f32)hp / (f32)BtlCharApi_GetHpMax(s->side);
    s32 limit = BtlAi_ScaleByLevel(s->cpuLevel, (s8)s->param[0xF], (s8)s->param[0x107]) + 20;

    if (BtlCharApi_IsChangingForm(s->side)) {
        st->timer1C = 600;
        return 0;
    }
    if (!BtlCharApi_CanFuse(s->side)) {
        return 0;
    }
    if (--st->timer1C > 0) {
        return 0;
    }
    st->timer1C = -1;
    if ((f32)limit < ratio * 100.0f) {
        return 0;
    }
    return 1;
}

/* Situation bit 58: BtlCharApi_CanSwitch is possible, flag 6 is clear, and a score of lost health (70 points) plus
   missing gauge +0xC (30 points) reaches an aiType-and-level threshold. */
s32 BtlAiSense_CheckBit58(AiActSide *s) {
    AiActStatus *st = &s->st;
    s32 hp = BtlCharApi_GetHp(s->side);
    s32 gauge = BtlSide_GetKi(s->side);
    u8 *type = gBtlAi->data->typeTbl[s->aiType];
    s32 limit = BtlAi_ScaleByLevel(s->cpuLevel, type[0x2AD], type[0x56D]);
    s32 max;
    s32 score;
    f32 r;

    if (BtlSide_IsPoweredUp(s->side)) {
        return 0;
    }
    if (!BtlCharApi_CanSwitch(s->side)) {
        return 0;
    }
    if (--st->timer20 > 0) {
        return 0;
    }
    st->timer20 = -1;
    max = BtlCharApi_GetHpMax(s->side);
    r = (f32)hp / (f32)max;
    score = (s32)((1.0f - r) * 70.0f);
    r = (f32)gauge / 100000.0f;
    score += (s32)((1.0f - r) * 30.0f);
    if (score < limit) {
        return 0;
    }
    return 1;
}

/* Situation bit 60: BtlCharApi_GetArmorBreakLevel gives 4 or more. */
s32 BtlAiSense_CheckBit60(AiActSide *s) {
    if (BtlCharApi_GetArmorBreakLevel(s->side) < 4) {
        return 0;
    }
    return 1;
}

/* Situation bit 61: the opponent's parameter byte +2 is 4. */
s32 BtlAiSense_CheckBit61(AiActSide *s) {
    return BtlCharApi_GetParamByte2(TagTeam_Target(s->side)) == 4;
}

/* 1 when the opponent's facing direction and the direction from this fighter to the opponent agree: this
   fighter is behind the opponent. */
s32 BtlAiSense_IsBehindOpponent(AiActSide *s) {
    AiActVec v;
    AiActVec face;
    AiActVec pos;
    AiActVec opp;
    AiActVec dir;

    BtlCharApi_GetRot(TagTeam_Target(s->side), &v);
    face.x = sinf(v.y);
    face.y = 0.0f;
    face.z = cosf(v.y);
    face.w = 0.0f;
    Vec3_Normalize(&face, &face);
    BtlCharApi_GetPos(s->side, &pos);
    BtlCharApi_GetPos(TagTeam_Target(s->side), &opp);
    Vec3_Sub(&v, &opp, &pos);
    v.y = 0.0f;
    Vec3_Normalize(&dir, &v);
    if (0.0f < Vec3_Dot(&face, &dir)) {
        return 1;
    }
    return 0;
}

/* Situation bit 32: the opponent's technique can be answered now. By its kind byte: 1, 2, 5 within reach
   (own radius + the opponent's speed), 3, 4, 6 a projectile of mode 2 is coming, other kinds with
   BtlCharApi_GetOppMoveKind == 0 a projectile of mode 3, otherwise attribute bit 0 and progress in 1..3. Needs fighter
   +0x106C < -29 and react bit 0x40 clear. */
s32 BtlAiSense_CheckBit32(AiActSide *s) {
    AiActStatus *st = &s->st;
    f32 progress = BtlCharApi_GetTechniqueProgress(TagTeam_Target(s->side));
    u32 attr = BtlCharApi_GetAttackAttr(TagTeam_Target(s->side));
    s32 kind = BtlCharApi_GetOppSkillKind(s->side);
    s32 sub = BtlCharApi_GetOppMoveKind(s->side);
    f32 radius = BtlCharApi_GetRadius(s->side);
    f32 speed = BtlCharApi_GetSpeed(TagTeam_Target(s->side));
    s32 ready = BtlCharApi_IsUnk106CLow(s->side);
    s32 ret;

    if ((u32)(kind - 1) < 2 || kind == 5) {
        if (radius + speed < gBtlAi->dist) {
            return 0;
        }
    } else if ((u32)(kind - 3) < 2 || kind == 6) {
        if (BtlCharApi_FindIncomingBlast(s->side, 2) == -1) {
            return 0;
        }
    } else if (sub == 0) {
        if (BtlCharApi_FindIncomingBlast(s->side, 3) == -1) {
            return 0;
        }
    } else {
        if (!(attr & 1)) {
            return 0;
        }
        if (progress < 1.0f || 3.0f <= progress) {
            return 0;
        }
    }
    ret = ready != 0;
    if (st->react & 0x40) {
        ret = 0;
    }
    return ret;
}

/* Situation bit 27: BtlCharApi_GetPromptButtons is non-zero and react bit 0x10000 is clear. */
s32 BtlAiSense_CheckSwitch(AiActSide *s) {
    s32 ret = BtlCharApi_GetPromptButtons(s->side) != 0;

    if (s->st.react & 0x10000) {
        ret = 0;
    }
    return ret;
}

/* While action 10 is at the bottom of the stack: 2 when the fighter distance is between dist[1] and half of
   st->unk24, else 1. 0 otherwise. */
s32 BtlAiSense_CheckAct10(AiActSide *s) {
    AiActStatus *st = &s->st;
    AiActEntry *e = &s->act.stack[0];
    AiActMove *m = &s->move;

    if (s->act.depth == 0 || e->id != 10) {
        return 0;
    }
    if (st->unk24 * 0.5f > gBtlAi->dist) {
        if (gBtlAi->dist > m->dist[1]) {
            return 2;
        }
    }
    return 1;
}

/* First pass of sense. Always returns 0. */
s32 BtlAiSense_Basic(AiActSide *s) {
    AiActStatus *st = &s->st;
    AiActMove *m = &s->move;
    AiActSeq *act = &s->act;
    s32 seq = BtlSeq_GetState();
    f32 ratio = (f32)BtlSide_GetKi(s->side) / 100000.0f;
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(s->side)];
    s32 r;

    if (seq == 3) {
        st->flags |= 2;
    } else if (seq == 2) {
        st->flags |= 4;
    }
    if (!BtlCharApi_TestFlag05(s->side)) {
        st->flags |= 8;
    }
    if (m->dist[1] + BtlCharApi_GetRadius(s->side) * 2.0f > gBtlAi->dist) {
        st->flags |= 0x10;
    } else if (m->dist[2] + BtlCharApi_GetRadius(s->side) * 2.0f > gBtlAi->dist) {
        st->flags |= 0x20;
    } else {
        st->flags |= 0x40;
    }
    st->react |= 1;
    if (BtlAiSense_IsBehindOpponent(s) && (st->flags & 0x10)) {
        st->react &= ~1;
    }
    if (ratio * 100.0f < 20.0f) {
        st->flags |= 0x0080000000000000;
    } else if (ratio * 100.0f < 60.0f) {
        st->flags |= 0x0040000000000000;
    } else if (ratio * 100.0f < 100.0f) {
        st->flags |= 0x0020000000000000;
    } else {
        st->flags |= 0x0010000000000000;
    }
    if (BtlAiSense_CheckBit56(s)) {
        st->flags |= 0x0100000000000000;
    }
    if (BtlAiSense_CheckBit57(s)) {
        st->flags |= 0x0200000000000000;
    }
    if (BtlAiSense_CheckBit58(s)) {
        st->flags |= 0x0400000000000000;
    }
    if (BtlAiSense_CheckBit60(s)) {
        st->flags |= 0x1000000000000000;
    }
    if (BtlAiSense_CheckBit61(s)) {
        st->flags |= 0x2000000000000000;
    }
    if (cls == 0x20) {
        st->flags |= 0x200000;
    }
    if (BtlCharApi_IsInClashA(s->side)) {
        st->flags |= 0x400000;
    }
    if (act->flags & 0x80) {
        st->flags |= 0x0800000000000000;
    }
    if (BtlAiSense_CheckBit32(s)) {
        st->flags |= 0x100000000;
    }
    if (BtlAiSense_CheckSwitch(s)) {
        st->flags |= 0x8000000;
    }
    r = BtlAiSense_CheckAct10(s);
    if (r > 0) {
        if (r == 2) {
            st->flags |= 0x8000000000000000;
        }
        st->react |= 8;
    }
    return 0;
}

/* Sets react bit 4 while the opponent's state id is 0x97 or 0x189. */
void BtlAiSense_NoteOppState(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state[2];
    s8 cls[2];
    AiActStatus *st = &s->st;

    AIACT_GET_STATES(s, tbl, state, cls);
    if (state[1] == 0x97 || state[1] == 0x189) {
        st->react |= 4;
    }
}

/* 1 = nothing more to sense this frame: the state has flag bit 0, or the state class is 15..17 and either
   BtlCharApi_CheckUnkCAC is 1 or fewer than 4 frames passed since it was. */
/* The class is biased by 15 before the `busy` test and compared after it. With the range test in a variable set
   in front of `if (busy)`, the compiler's branch prediction sees only `x == 0` at the second branch (predicted
   not taken, delay slot filled from the fall-through path); with the compare next to its branch it sees an
   unsigned compare (no prediction, slot filled from the target: the original's `beqzl` + `move v0,zero`). */
s32 BtlAiSense_IsBusy(AiActSide *s) {
    AiActTables8 *tbl = (AiActTables8 *)((u8 *)gBtlAi->data->tables + 8);
    s32 state = BtlCharApi_GetUnk974(s->side);
    u8 busy = tbl->stateFlags[state] & 1;
    s32 cls = (s8)tbl->stateClass[state] - 15;
    s32 r = BtlCharApi_CheckUnkCAC(s->side);
    AiActStatus *st = &s->st;

    if (busy) {
        return 1;
    }
    if ((u32)cls < 3) {
        if (r == 1) {
            st->downTimer = 4;
            return 1;
        }
        if (--st->downTimer > 0) {
            return 1;
        }
        st->downTimer = 0;
    }
    return 0;
}

/* Situation bit 49: state class 6 and the bottom action is not 0x50. */
s32 BtlAiSense_CheckBit49(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(s->side)];
    AiActSeq *act = &s->act;
    AiActEntry *e = &s->act.stack[0];

    if (cls == 6) {
        if (act->depth == 0) {
            return 1;
        }
        return e->id != 0x50;
    }
    return 0;
}

/* Fighter +0xFE0 set: 1 when the bottom action is 0x51, else 2 (situation bit 50). 0 when clear. */
s32 BtlAiSense_CheckBit50(AiActSide *s) {
    AiActEntry *e = &s->act.stack[0];

    if (BtlCharApi_GetStunTimer(s->side)) {
        return e->id == 0x51 ? 1 : 2;
    }
    return 0;
}

/* Fighter flag 0xBE set: 1 when the bottom action is 0x52, else 2 (situation bit 51). 0 when clear. */
s32 BtlAiSense_CheckBit51(AiActSide *s) {
    AiActEntry *e = &s->act.stack[0];

    if (BtlCharApi_TestFlagBE(s->side)) {
        return e->id == 0x52 ? 1 : 2;
    }
    return 0;
}

/* Situation bit 43: bit 41 of fighter +0x1288 set, bits 42..44 clear, react bit 0x80000 set. */
s32 BtlAiSense_CheckBit43(AiActSide *s) {
    u64 w = BtlCharApi_GetUnk1288(s->side);
    AiActStatus *st = &s->st;
    u64 other = w & 0x1C0000000000;

    w &= 0x20000000000;
    if (w == 0) {
        return 0;
    }
    if (other != 0) {
        return 0;
    }
    if (st->react & 0x80000) {
        return 1;
    }
    return 0;
}

/* Situation bit 40: the opponent is behind this fighter with flag 6 set and in state class 13 / 14, bit 46
   of fighter +0x1288 is set, and the bottom action is not 0x5C. react bit 0x10 blocks it; react bit 0x100000
   blocks it until the opponent's state id changes. */
s32 BtlAiSense_CheckBit40(AiActSide *s) {
    AiActStatus *st = &s->st;
    AiActSeq *act = &s->act;
    AiActEntry *e = &s->act.stack[0];
    AiActSide *opp = &gBtlAi->side[TagTeam_Target(s->side)];
    AiActTables *tbl = gBtlAi->data->tables;
    s32 oppState = BtlCharApi_GetUnk974(TagTeam_Target(s->side));
    s32 cls = tbl->stateClass[oppState];
    u64 w = BtlCharApi_GetUnk1288(s->side);

    if (!BtlSide_IsPoweredUp(opp->side)) {
        st->react &= ~0x10;
        return 0;
    }
    if (!BtlAiSense_IsBehindOpponent(opp)) {
        st->react &= ~0x10;
        return 0;
    }
    if (!((u32)(cls - 13) < 2)) {
        st->react &= ~0x10;
        return 0;
    }
    if (!(w & 0x400000000000)) {
        return 0;
    }
    if (st->react & 0x10) {
        return 0;
    }
    if (st->react & 0x100000) {
        if (st->oppAction == oppState) {
            return 0;
        }
        st->react &= ~0x100000;
    }
    if (act->depth > 0 && e->id == 0x5C) {
        return 0;
    }
    return 1;
}

/* Situation bit 41: the opponent is behind this fighter, within dist[1], and the bottom action is none of
   6, 0x5C, 0x50. */
s32 BtlAiSense_CheckBit41(AiActSide *s) {
    AiActMove *m = &s->move;
    AiActSeq *act = &s->act;
    AiActEntry *e = &s->act.stack[0];

    if (!BtlAiSense_IsBehindOpponent(&gBtlAi->side[TagTeam_Target(s->side)])) {
        return 0;
    }
    if (m->dist[1] < gBtlAi->dist) {
        return 0;
    }
    if (act->depth > 0) {
        if (e->id == 6) {
            return 0;
        }
        if (e->id == 0x5C) {
            return 0;
        }
        if (e->id == 0x50) {
            return 0;
        }
    }
    return 1;
}

/* Situation bit 28: close range, own state class not 13..23, the opponent in class 13..17 and not turned
   away from this fighter, BtlCharApi_IsUnkCACPaired, react bit 0x20 clear. */
s32 BtlAiSense_CheckBit28(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state[2];
    s8 cls[2];
    AiActStatus *st = &s->st;

    AIACT_GET_STATES(s, tbl, state, cls);
    if (!(st->flags & 0x10)) {
        return 0;
    }
    if ((u8)(cls[0] - 13) < 11) {
        return 0;
    }
    if (BtlAiSense_IsBehindOpponent(&gBtlAi->side[TagTeam_Target(s->side)])) {
        return 0;
    }
    if (!((u32)((u8)cls[1] - 13) < 5)) {
        return 0;
    }
    if (!BtlCharApi_IsUnkCACPaired(s->side)) {
        return 0;
    }
    if (st->react & 0x20) {
        return 0;
    }
    return 1;
}

/* Situation bit 29: close range, the opponent in state class 15..17, react bit 0x200 clear. */
s32 BtlAiSense_CheckBit29(AiActSide *s) {
    s32 cls = gBtlAi->data->tables->stateClass[BtlCharApi_GetUnk974(TagTeam_Target(s->side))];
    AiActStatus *st = &s->st;

    if (st->react & 0x200) {
        return 0;
    }
    if (!((u32)(cls - 15) < 3)) {
        return 0;
    }
    if (st->flags & 0x10) {
        return 1;
    }
    return 0;
}

/* Situation bit 35: the opponent's technique has a non-zero kind byte, its progress is known and not above
   1, fighter +0x1070 is -30, react bit 0x400 clear. */
s32 BtlAiSense_CheckBit35(AiActSide *s) {
    AiActStatus *st = &s->st;
    f32 progress = BtlCharApi_GetTechniqueProgress(TagTeam_Target(s->side));

    if (!BtlCharApi_GetOppSkillKind(s->side)) {
        return 0;
    }
    if (st->react & 0x400) {
        return 0;
    }
    if (progress == -1.0f) {
        return 0;
    }
    if (BtlCharApi_IsUnk1070Set(s->side)) {
        return 0;
    }
    if (1.0f < progress) {
        return 0;
    }
    return 1;
}

/* Situation bit 31: by the opponent's technique progress: kind byte 0 with progress 0..1, or another kind
   with the opponent in state class 0x18 and progress 1..3; react bit 0x80 clear. */
s32 BtlAiSense_CheckBit31(AiActSide *s) {
    AiActStatus *st = &s->st;
    f32 progress = BtlCharApi_GetTechniqueProgress(TagTeam_Target(s->side));
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(TagTeam_Target(s->side))];

    if (!BtlCharApi_GetOppSkillKind(s->side)) {
        if (progress < 0.0f) {
            return 0;
        }
        if (1.0f < progress) {
            return 0;
        }
    } else {
        if (cls != 0x18) {
            return 0;
        }
        if (progress < 1.0f || 3.0f <= progress) {
            return 0;
        }
    }
    if (st->react & 0x80) {
        return 0;
    }
    return 1;
}

/* Situation bit 33: bit 45 of fighter +0x1288 set and react bit 0x100 clear. */
s32 BtlAiSense_CheckBit33(AiActSide *s) {
    AiActStatus *st = &s->st;

    if (BtlCharApi_GetUnk1288(s->side) & 0x200000000000) {
        if (st->react & 0x100) {
        return 0;
    }
    return 1;
    }
    return 0;
}

/* Situation bit 34: own state class 0x1A, the opponent in a rush step, the running action is not 0x47:
   fighter flag 0x66. */
s32 BtlAiSense_CheckBit34(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state[2];
    s8 cls[2];
    AiActSeq *act = &s->act;
    s32 step;

    AIACT_GET_STATES(s, tbl, state, cls);
    step = state[1] < 0x3C ? state[1] - 0x37 : state[1] - 0x3C;
    if (cls[0] == 0x1A) {
        if (!((u32)step < 4)) {
            return 0;
        }
        if (act->depth != 0 && AIACT_TOP(act)->id == 0x47) {
            return 0;
        }
        return BtlCharApi_TestFlag66(s->side);
    }
    return 0;
}

/* Situation bit 36: the opponent's technique kind byte is 1 or 2, react bit 0x800 clear. */
s32 BtlAiSense_CheckBit36(AiActSide *s) {
    AiActStatus *st = &s->st;
    s32 kind = BtlCharApi_GetOppSkillKind(s->side);

    if (kind == -1) {
        return 0;
    }
    if ((u32)(kind - 1) < 2) {
        if (st->react & 0x800) {
        return 0;
    }
    return 1;
    }
    return 0;
}

/* Situation bit 37: the kind byte is 3, react bit 0x1000 clear. */
s32 BtlAiSense_CheckBit37(AiActSide *s) {
    AiActStatus *st = &s->st;
    s32 kind = BtlCharApi_GetOppSkillKind(s->side);

    if (kind == -1) {
        return 0;
    }
    if (kind == 3) {
        if (st->react & 0x1000) {
        return 0;
    }
    return 1;
    }
    return 0;
}

/* Situation bit 38: the kind byte is 0, react bit 0x2000 clear. */
s32 BtlAiSense_CheckBit38(AiActSide *s) {
    AiActStatus *st = &s->st;
    s32 kind = BtlCharApi_GetOppSkillKind(s->side);

    if (kind == -1) {
        return 0;
    }
    if (kind == 0) {
        if (st->react & 0x2000) {
        return 0;
    }
    return 1;
    }
    return 0;
}

/* Situation bit 39: BtlCharApi_IsOppSkillFlag4 and bit 1 of the own parameter word +0x10. */
s32 BtlAiSense_CheckBit39(AiActSide *s) {
    s32 a = BtlCharApi_IsOppSkillFlag4(s->side);
    s32 b = BtlCharApi_GetParamFlags(s->side);
    s32 ret = a != 0;

    if (!(b & 2)) {
        ret = 0;
    }
    return ret;
}

/* Third pass of sense: the reaction bits. */
void BtlAiSense_Reactions(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state[2];
    s8 cls[2];
    AiActSeq *act = &s->act;
    AiActStatus *st = &s->st;
    u64 w = BtlCharApi_GetUnk1288(s->side);
    AiActEntry *e;
    s32 r;

    AIACT_GET_STATES(s, tbl, state, cls);
    r = BtlAiSense_CheckBit51(s);
    if (r != 0) {
        if (r == 2) {
            st->flags |= 0x0008000000000000;
        }
        return;
    }
    r = BtlAiSense_CheckBit50(s);
    if (r != 0) {
        if (r == 2) {
            st->flags |= 0x0004000000000000;
        }
        return;
    }
    if (cls[0] == 2) {
        e = &s->act.stack[0];
        if (!(act->depth > 0 && e->id == 0x55)) {
            st->flags |= 0x40000000000;
        }
    }
    if (cls[0] == 4 && !(st->react & 0x20000)) {
        st->flags |= 0x0001000000000000;
    }
    if (BtlAiSense_CheckBit49(s)) {
        st->flags |= 0x0002000000000000;
    }
    if (BtlAiSense_CheckBit43(s)) {
        st->flags |= 0x80000000000;
    }
    if ((u32)((u8)cls[0] - 1) < 7) {
        act->flags &= ~0x80;
    }
    if (BtlAiSense_CheckBit40(s)) {
        st->flags |= 0x10000000000;
    }
    if (BtlAiSense_CheckBit41(s)) {
        st->flags |= 0x20000000000;
    }
    if (BtlAiSense_CheckBit28(s) == 1) {
        st->flags |= 0x10000000;
        st->react |= 2;
    }
    if (BtlAiSense_CheckBit29(s) == 1) {
        st->flags |= 0x20000000;
        st->react |= 0x200;
        st->oppClass = cls[1];
    }
    if (BtlCharApi_FindIncomingBlast(s->side, 0) != -1) {
        st->flags |= 0x40000000;
    }
    if (BtlAiSense_CheckBit35(s) == 1) {
        st->flags |= 0x800000000;
    }
    if (BtlAiSense_CheckBit31(s) == 1) {
        st->flags |= 0x80000000;
    }
    if (!(st->react & 0x8000)) {
        if (w & 0x40000000000) {
            st->flags |= 0x400000000000;
        }
        if (w & 0x80000000000) {
            st->flags |= 0x800000000000;
        }
    }
    if (w & 0x100000000000) {
        if (!(st->react & 0x4000)) {
            s32 id = 0;

            if (act->depth > 0) {
                id = AIACT_TOP(act)->id;
            }
            if (!((u32)(id - 0x53) < 2)) {
                st->flags |= 0x200000000000;
            }
        }
    }
    if (BtlAiSense_CheckBit33(s) == 1) {
        st->flags |= 0x200000000;
    }
    if (BtlAiSense_CheckBit34(s) == 1) {
        st->flags |= 0x400000000;
    }
    if ((u32)((u8)cls[0] - 1) < 6) {
        st->flags |= 0x100000000000;
    }
    if (BtlAiSense_CheckBit36(s) == 1) {
        st->flags |= 0x1000000000;
    }
    if (BtlAiSense_CheckBit37(s) == 1) {
        if (st->react & 8) {
            st->flags |= 0x4000000000000000;
        }
        st->flags |= 0x2000000000;
    }
    if (BtlAiSense_CheckBit38(s) == 1) {
        st->flags |= 0x4000000000;
    }
    if (BtlAiSense_CheckBit39(s) == 1) {
        st->flags |= 0x8000000000;
    }
}

/* 1 when the fighters are at least 30 apart and the elevation angle between them is 0.7 rad or more. */
s32 BtlAiSense_IsSteep(AiActSide *s) {
    AiActVec pos;
    AiActVec opp;
    f32 a;

    BtlCharApi_GetPos(s->side, &pos);
    BtlCharApi_GetPos(TagTeam_Target(s->side), &opp);
    if (Vec3_Dist(&opp, &pos) < 30.0f) {
        return 0;
    }
    a = -atan2f(opp.y - pos.y, Vec3_Dist(&opp, &pos));
    if (a < 0.0f) {
        if (!(-0.7f < a)) {
            return 1;
        }
    } else {
        if (!(a < 0.7f)) {
            return 1;
        }
    }
    return 0;
}

/* Fourth pass of sense: bits derived from the earlier ones and from the opponent's state class. */
void BtlAiSense_Derived(AiActSide *s) {
    AiActTables *tbl = gBtlAi->data->tables;
    s32 state[2];
    s8 cls[2];
    AiActStatus *st = &s->st;
    u64 w = BtlCharApi_GetUnk1288(s->side);
    s32 oppBusy = BtlCharApi_GetStunTimer(TagTeam_Target(s->side));

    AIACT_GET_STATES(s, tbl, state, cls);
    if (st->react & 1) {
        st->flags |= 0x2000;
    } else {
        st->flags |= 0x4000;
    }
    if (BtlSide_IsPoweredUp(s->side)) {
        st->flags |= 0x8000;
    }
    if (st->flags & 0x10) {
        st->flags |= 0x80;
        if (st->flags & 0x8000) {
            st->flags |= 0x400;
        }
    } else if (st->flags & 0x20) {
        st->flags |= 0x100;
        if (st->flags & 0x8000) {
            st->flags |= 0x800;
        }
    } else if (st->flags & 0x40) {
        st->flags |= 0x200;
        if (st->flags & 0x8000) {
            st->flags |= 0x1000;
        }
    }
    if (BtlAiSense_IsSteep(s)) {
        st->flags |= 0x800000;
    }
    if (gBtlAi->sight & 1) {
        st->flags |= 0x1000000;
    }
    if (w & 0x3FB00809FF) {
        st->flags |= 0x2000000;
    }
    if (cls[1] == 6 || oppBusy > 0) {
        st->flags |= 0x10000;
    }
    if ((u8)(cls[1] - 3) < 5 && cls[1] != 6) {
        if (st->react & 4) {
            st->flags |= 0x40000;
        } else {
            st->flags |= 0x20000;
        }
    } else {
        st->react &= ~4;
    }
    if (st->flags & 0x40) {
        if (cls[1] == 7 || cls[1] == 2) {
            st->flags |= 0x80000;
        }
    }
    if (st->flags & 0x30) {
        if (cls[1] == 2 || cls[1] == 5) {
            st->flags |= 0x100000;
        }
    }
    if (cls[0] == 11) {
        st->flags |= 0x4000000;
    }
}

/*
 * Rebuilds the situation word of one side. Called by BtlAiMgr_Update for each CPU fighter, before the rules.
 * react bits 0, 1 and 3 are cleared first; bits 5..20 are cleared when the opponent's state class is not the
 * one stored in st->oppClass. The later passes are skipped while BtlAiSense_IsBusy.
 */
void BtlAiSense_Update(AiActSide *s) {
    AiActStatus *st = &s->st;
    AiActTables *tbl = gBtlAi->data->tables;
    s32 cls = tbl->stateClass[BtlCharApi_GetUnk974(TagTeam_Target(s->side))];
    s32 react;
    s32 i;

    react = st->react & ~0xB;
    st->flags = 0;
    st->react = react;
    if (cls != st->oppClass) {
        for (i = 5; i < 21; i++) {
            react &= ~(1 << i);
        }
        st->react = react;
    }
    if (BtlAiSense_Basic(s)) {
        return;
    }
    BtlAiSense_NoteOppState(s);
    if (BtlAiSense_IsBusy(s)) {
        return;
    }
    BtlAiSense_Reactions(s);
    BtlAiSense_Derived(s);
}
