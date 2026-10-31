#include "common.h"
#include "battle/cpu_level.h"
#include "battle/btl_ai_int.h"
#include "battle/btl_ai_seq_a.h"

/*
 * CPU player, first object: 0x1B4140..0x1B6D50 (the former btl_ai_seq_a.c, 0x1B4140..0x1B6008, merged at
 * integration in front of the code that was here before: step handlers 14..23 and BtlAi_RunSeq, see the separator
 * comment further down). The rule conditions that follow are btl_ai_cond.c.
 *
 * First part: the generic scripted action ("sequence"), 0x1B4140..0x1B6008. The object itself starts at 0x1B3F78
 * (BtlAiSeq_Reset / BtlAiSeq_PushRule, eft_det_b_c.c).
 *
 * An action id on top of the sequence stack that BtlAi_RunSeq does not run itself (3, 7, 0x19, 0x1A, 0x36) is a
 * script in the common AI data: a list of steps (BtlAiSeqStep: the AI buttons to hold / press / tap, the state
 * class the fighter is in once the input has taken effect, and the handler that says when the step is over), and
 * per action a flag word, a timeout in seconds and a "pick" function. Steps carry a group number; the action
 * plays one step of group 0, then one of group 1, and so on, and ends when a group has no step. The pick function
 * chooses among the alternatives of the group and usually also sets the step's parameter (seq->timer) from the
 * argument byte of the rule that queued the action.
 *
 * Phases (gBtlAiStateFuncs): 0 init -> 1 start -> 2 run -> (step finished: 1 again, next group) ... -> 3 end.
 * Init, start and run call each other directly, so a new action sends its first input on the frame it starts;
 * a finished step costs one frame without input (the run phase returns before BtlAiPad_Set).
 *
 * The stick is never used here: every BtlAiPad_Set of this file passes x = y = 0. Directions are the four
 * direction button bits of the step.
 *
 * Random numbers: only Rand_Range, in the pick functions (see each one); the phases and the step handlers 0..13
 * draw none, except through AiThink_RollActRate when the last action of a plan ends.
 */

typedef struct BtlAiSeqData {
    /* 0x00 */ s32 size[0x29];
    /* 0xA4 */ BtlAiSeqActTable *act;
    /* 0xA8 */ void *rules[8];
    /* 0xC8 */ u8 *profile[32];
} BtlAiSeqData;

#define AI_DATA ((BtlAiSeqData *)gBtlAi->data)
#define SEQA(ai) ((BtlAiSeqA *)&(ai)->seq)
#define ABS(x) ((x) < 0 ? -(x) : (x))

extern BtlAiPickFunc gBtlAiPickFuncs[16];
extern BtlAiStepFunc gBtlAiStepFuncs[24];

extern void BtlAiPad_Set(BtlAiWork *ai, s32 hold, s32 press, s32 once, s16 special, f32 x, f32 y);
extern s32 BtlAiSense_IsBehindOpponent(BtlAiWork *ai);
extern s32 AiThink_RollActRate(BtlAiWork *ai, s32 cond);
extern s32 AiThink_GetBlastStep(BtlAiWork *ai);
extern f32 BtlCharApi_GetAltitude(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern f32 BtlStage_GetTop(void);
extern f32 BtlStage_GetInnerRadius(void);
extern void BtlCharApi_GetPos(s32 objId, BtlAiVec *out);
extern void BtlCharApi_GetRot(s32 objId, BtlAiVec *out);
extern f32 BtlCharApi_GetChargeRate(s32 objId);
extern s32 BtlCharApi_AnyHasFlag128(void);
extern s32 BtlCharApi_GetUnkD74Diff(s32 objId);
extern s32 BtlCharApi_PickFusionSlot(s32 objId);
extern s32 BtlCharApi_PickTransformSlot(s32 objId);
extern s32 BtlCharApi_GetActiveMember(s32 objId);
extern s32 BtlCharApi_GetMemberCount(s32 objId);
extern s32 BtlCharApi_GetMemberHpPercent(s32 objId, s32 member);
extern s32 BtlCharApi_GetMemberKiPercent(s32 objId, s32 member);
extern s32 BtlCharApi_GetSwitchTarget(s32 objId);
extern s32 BtlCharApi_IsBlockedByOpponent(s32 objId);
extern u64 BtlCharApi_GetUnk1288(s32 objId);
extern s32 BtlCharApi_CheckUnkCAC(s32 objId);
extern s32 BtlCharApi_IsBlastPassing(s32 objId);
extern s32 BtlCharApi_GetBlastShots(s32 objId);
extern f32 Vec3_Length(BtlAiVec *v);
extern void Vec3_Normalize(BtlAiVec *dst, BtlAiVec *src);
extern void Vec3_Scale(BtlAiVec *dst, BtlAiVec *src, f32 s);
extern void Vec3_Add(BtlAiVec *dst, BtlAiVec *a, BtlAiVec *b);
extern void Vec3_Sub(BtlAiVec *dst, BtlAiVec *a, BtlAiVec *b);
extern f32 Vec3_Dot(BtlAiVec *a, BtlAiVec *b);
extern void Vec3_RotateY(BtlAiVec *dst, BtlAiVec *src, f32 angle); /* rotate about Y */
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);

/* ---- Helpers of the phases ---- */

/* 1 when the action's flag word says it has to end now: the fighter is in state class 1..7 (flag 1, unless step
 * handler 15 set BTLAI_SEQ_NO_HIT_END), it is in state class 0x20 or in a clash (flag 8), or the opponent is using
 * a technique that has a kind byte (flag 0x10). Also called by the dispatchers of actions 7, 0x19, 0x1A, 0x36. */
s32 BtlAiSeq_IsInterrupted(BtlAiWork *ai, s32 id) {
    BtlAiSeqBody *body = &AI_DATA->act->body;
    u16 flags = body->flags[id];
    s8 cls = body->stateClass[BtlCharApi_GetUnk974(ai->objId)];
    s32 kind = BtlCharApi_GetOppSkillKind(ai->objId);
    BtlAiSeqA *seq = SEQA(ai);

    if ((u16)(flags & BTLAI_SEQF_END_WHEN_HIT) && !(seq->flags & BTLAI_SEQ_NO_HIT_END) && (u32)(cls - 1) < 7) {
        return 1;
    }
    if (flags & BTLAI_SEQF_END_IN_CLASH) {
        if (cls == 0x20 || BtlCharApi_IsInClashA(ai->objId) != 0) {
            return 1;
        }
    }
    if ((flags & BTLAI_SEQF_END_OPP_SKILL) && kind != -1) {
        return 1;
    }
    return 0;
}

/* mode 0: the fighter is closer to the stage ceiling than its height + 10; mode 1: it is less than 10 above the
 * ground (on stages 4 and 27 also when pose bit 0x80 is clear). */
s32 BtlAiSeq_CheckHeight(BtlAiWork *ai, s32 mode) {
    BtlAiVec pos;
    f32 alt = BtlCharApi_GetAltitude(ai->objId);
    f32 height = BtlCharApi_GetHeight(ai->objId);
    f32 limit = BtlStage_GetTop() + height + 10.0f;

    BtlCharApi_GetPos(ai->objId, &pos);
    switch (mode) {
    case 0:
        if (pos.y < limit) {
            return 1;
        }
        break;
    case 1:
        if (BtlChar_IsStage4Or27() != 0 && BtlCharApi_TestPoseBit80(ai->objId, 1) == 0) {
            return 1;
        }
        if (alt < 10.0f) {
            return 1;
        }
        break;
    }
    return 0;
}

/* End test of the run phase that depends on the action id; non-zero ends the action.
 * 9, 11, 21: on stages 4 / 27, when the fighter's pose bit 0x80 is clear or its y is above -80 (+Y is down).
 * 13: on stages 4 / 27, when the opponent's pose bit 0x80 is clear.
 * 15: BtlAiSeq_CheckHeight with the chosen step as the mode (step 0 = going up, step 1 = going down).
 * 16 / 17: BtlAiSeq_CheckHeight(0) / (1): the ceiling / the ground is reached.
 * 29: on stages 4 / 27, when the fighter's pose bit 0x80 is clear; otherwise, when it is the only action, the
 *     fighter is in state class 12 and the pad's special-3 timer has run out, the "special 3 done" mark is
 *     cleared so that the step's special 3 is sent again. */
s32 BtlAiSeq_CheckStageAbort(BtlAiWork *ai, s32 id) {
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiOutput *out = &ai->out;
    s8 cls = AI_DATA->act->body.stateClass[BtlCharApi_GetUnk974(ai->objId)];
    s32 special = BtlChar_IsStage4Or27();
    s32 pose[2];
    BtlAiVec pos;

    pose[0] = BtlCharApi_TestPoseBit80(ai->objId, 1);
    pose[1] = BtlCharApi_TestPoseBit80(TagTeam_Target(ai->objId), 1);
    BtlCharApi_GetPos(ai->objId, &pos);
    switch (id) {
    case 9:
    case 11:
    case 21:
        if (special != 0) {
            if (pose[0] == 0) {
                return 1;
            }
            if (-80.0f < pos.y) {
                return 1;
            }
        }
        break;
    case 13:
        if (special != 0) {
            if (pose[1] == 0) {
                return 1;
            }
        }
        break;
    case 15:
        if (BtlAiSeq_CheckHeight(ai, seq->step) != 0) {
            return 1;
        }
        break;
    case 16:
        if (BtlAiSeq_CheckHeight(ai, 0) != 0) {
            return 1;
        }
        break;
    case 17:
        if (BtlAiSeq_CheckHeight(ai, 1) != 0) {
            return 1;
        }
        break;
    case 29:
        if (special != 0) {
            if (pose[0] == 0) {
                return 1;
            }
            if (seq->depth != 1) {
                return 0;
            }
            if (cls != 12) {
                return 0;
            }
            if (out->timer > 0) {
                return 0;
            }
            ai->out.toggle[3] &= ~4;
        }
        break;
    }
    return 0;
}

/* ---- The four phases (gBtlAiStateFuncs) ---- */

/* Phase 0. Ends the action at once when BtlAiSeq_IsInterrupted says so. An action with flag 2 waits here for the
 * fighter to be in state class 0, sending nothing, except in state class 10 where it presses DASH (alternate
 * frames) to stop the dash. Otherwise falls through to the start phase. */
void BtlAiSeq_PhaseInit(BtlAiWork *ai) {
    BtlAiOutput *out = &ai->out;
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiSeqEntry *top = &SEQ_TOP(seq);
    BtlAiSeqBody *body = &AI_DATA->act->body;
    u16 flags = body->flags[top->id];
    s8 cls = body->stateClass[BtlCharApi_GetUnk974(ai->objId)];

    seq->group = 0;
    BtlAiPad_Clear(out, 0);
    if (BtlAiSeq_IsInterrupted(ai, top->id) != 0) {
        seq->phase = 3;
        return;
    }
    if (flags & BTLAI_SEQF_WAIT_IDLE) {
        if (cls != 0) {
            if (cls == 10) {
                BtlAiPad_Clear(out, 1);
                BtlAiPad_Set(ai, 0, 2, 0, 0, 0.0f, 0.0f);
            }
            return;
        }
    }
    seq->phase = 1;
    BtlAiSeq_PhaseStart(ai);
}

/* Phase 1: chooses the step of the current group. Clears the pad marks, the step parameter and the flags 0x40,
 * 0x200, 0x400, 0x800 and 0x1000, records the fighter's state, attack charge and the fighter distance, counts the
 * steps of the group and lets the action's pick function choose one; no step or a negative pick ends the action.
 * Action 10 stores the fighter distance in status.unk24 when its step 0 was chosen, every other action clears it.
 * Falls through to the run phase. */
void BtlAiSeq_PhaseStart(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiStatus *status = &ai->status;
    BtlAiSeqEntry *top = &SEQ_TOP(seq);
    BtlAiSeqActTable *act = AI_DATA->act;
    BtlAiSeqBody *body = &act->body;
    BtlAiSeqStep *steps = &act->steps[body->first[top->id]];
    u8 count = body->count[top->id];
    s8 pick = body->pick[top->id];
    s32 n;
    s32 i;
    s32 sel;

    BtlAiPad_Clear(&ai->out, 0);
    seq->timer = 0;
    seq->waitTimer = 0;
    seq->flags &= ~0x1E40;
    seq->startState = BtlCharApi_GetUnk974(ai->objId);
    seq->charge = BtlCharApi_GetChargeRate(ai->objId);
    seq->startDist = gBtlAi->dist;
    seq->dirX = 0.0f;
    seq->dirZ = 0.0f;
    n = 0;
    for (i = 0; i < count; i++) {
        if (seq->group == steps[i].group) {
            n++;
        }
    }
    if (n == 0) {
        seq->phase = 3;
        return;
    }
    sel = gBtlAiPickFuncs[pick](ai, n, top->arg);
    if (sel < 0) {
        seq->phase = 3;
        return;
    }
    n = 0;
    for (i = 0; i < count; i++) {
        if (seq->group == steps[i].group) {
            if (sel == n) {
                seq->step = i;
                break;
            }
            n++;
        }
    }
    seq->phase = 2;
    if (top->id == 10) {
        if (seq->step == 0) {
            *(f32 *)&status->unk24 = gBtlAi->dist;
        }
    } else {
        *(f32 *)&status->unk24 = 0.0f;
    }
    BtlAiSeq_PhaseRun(ai);
}

/* Phase 2, every frame, in this order:
 * 1. raise BTLAI_SEQ_TOOK_EFFECT once the fighter is in the step's state class in another state than at the start;
 * 2. the step handler: finished = back to phase 1 with the next group, nothing is sent this frame;
 * 3. BtlAiSeq_IsInterrupted: end;
 * 4. while the input has not taken effect, count frames (not while any fighter has flag 0x128) and end the action
 *    after timeout * 30 of them;
 * 5. BtlAiSeq_CheckStageAbort: end;
 * 6. send the step's hold / press / once bits and its special (nothing at all with BTLAI_SEQ_RELEASE). */
void BtlAiSeq_PhaseRun(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiSeqEntry *top = &SEQ_TOP(seq);
    BtlAiSeqActTable *act = AI_DATA->act;
    BtlAiSeqBody *body = &act->body;
    s8 timeout = body->timeout[top->id];
    BtlAiSeqStep *step = &act->steps[body->first[top->id]];
    s32 state[2];
    s8 cls[2];

    state[0] = BtlCharApi_GetUnk974(ai->objId);
    state[1] = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    cls[0] = body->stateClass[state[0]];
    cls[1] = body->stateClass[state[1]];
    step = &step[seq->step];
    if (!(seq->flags & BTLAI_SEQ_TOOK_EFFECT) && cls[0] == step->cls && seq->startState != state[0]) {
        seq->flags |= BTLAI_SEQ_TOOK_EFFECT;
    }
    if (gBtlAiStepFuncs[step->handler](ai) != 0) {
        seq->phase = 1;
        seq->group++;
        return;
    }
    if (BtlAiSeq_IsInterrupted(ai, top->id) != 0) {
        seq->phase = 3;
        return;
    }
    if (timeout > 0 && !(seq->flags & BTLAI_SEQ_TOOK_EFFECT)) {
        if (BtlCharApi_AnyHasFlag128() == 0) {
            seq->waitTimer++;
        }
        if (timeout * 30 < seq->waitTimer) {
            seq->phase = 3;
            return;
        }
    }
    if (BtlAiSeq_CheckStageAbort(ai, top->id) != 0) {
        seq->phase = 3;
        return;
    }
    if (seq->flags & BTLAI_SEQ_RELEASE) {
        BtlAiPad_Set(ai, 0, 0, 0, 0, 0.0f, 0.0f);
    } else {
        BtlAiPad_Set(ai, step->hold, step->press, step->once, step->special, 0.0f, 0.0f);
    }
}

/* After the last action of a plan: decides whether action 0x43 follows it. Needs the powered-up mode (fighter
 * flag 6), the plan's first action to be one of 0x22, 0x24, 0x26, 0x29, 0x2A, 0x2B, 0x32, 0x35 (or 0x2D / 0x2E
 * with BtlCharApi_GetUnkD74Diff == 0), and a successful AiThink_RollActRate(ai, 0x4C), which draws. The same ten
 * ids are the ones step handler 23 maps to a slot number. */
s32 BtlAiSeq_RollPowerUpChain(BtlAiWork *ai) {
    BtlAiSeqEntry *first = &ai->seq.stack[0];
    s32 diff = BtlCharApi_GetUnkD74Diff(ai->objId);

    if (BtlSide_IsPoweredUp(ai->objId) == 0) {
        return 0;
    }
    switch (first->id) {
    case 0x2D:
    case 0x2E:
        if (diff != 0) {
            return 0;
        }
        break;
    case 0x22:
    case 0x24:
    case 0x26:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x32:
    case 0x35:
        break;
    default:
        return 0;
    }
    return AiThink_RollActRate(ai, 0x4C) != 0;
}

/* Phase 3: pops the action. In state class 10 (except for action 14) it first presses DASH until the dash has
 * stopped. When the stack is empty it either chains action 0x43 (keeping the old id in seq->prevId) or clears
 * BTLAI_SEQ_CHAIN; the next action starts at phase 0 on the next frame. */
void BtlAiSeq_PhaseEnd(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    s8 cls = AI_DATA->act->body.stateClass[BtlCharApi_GetUnk974(ai->objId)];
    BtlAiSeqEntry *top = &SEQ_TOP(seq);

    if (cls == 10 && top->id != 14) {
        BtlAiPad_Clear(&ai->out, 1);
        BtlAiPad_Set(ai, 0, 2, 0, 0, 0.0f, 0.0f);
        return;
    }
    seq->depth--;
    if (seq->depth <= 0) {
        if (BtlAiSeq_RollPowerUpChain(ai) != 0) {
            seq->phase = 0;
            top = &seq->stack[0];
            seq->prevId = top->id;
            top->id = 0x43;
            top->arg = 0;
            seq->depth = 1;
            return;
        }
        seq->flags &= ~BTLAI_SEQ_CHAIN;
    }
    seq->phase = 0;
}

/* ---- Pick functions (gBtlAiPickFuncs) ---- */

/* Pick 0: any alternative, evenly. One draw: Rand_Range(n). */
s32 BtlAiPick_Random(BtlAiWork *ai, s32 n, s32 arg) {
    return Rand_Range(n);
}

/* Pick 1: the alternative the rule names. */
s32 BtlAiPick_ByArg(BtlAiWork *ai, s32 n, s32 arg) {
    if (arg < n) {
        return arg;
    }
    return -1;
}

/* Pick 2: arg = seconds * 10 + alternative; the step parameter is seconds * 30 frames. Alternative 9 draws
 * Rand_Range(n). An alternative the group does not have ends the action. */
s32 BtlAiPick_ArgSeconds(BtlAiWork *ai, s32 n, s32 arg) {
    BtlAiSeqA *seq = SEQA(ai);
    s32 sel;

    if (arg < 0) {
        sel = -arg % 10;
    } else {
        sel = arg % 10;
    }
    if (sel == 9) {
        sel = BtlAiPick_Random(ai, n, 0);
    }
    if (sel >= n) {
        return -1;
    }
    seq->timer = arg / 10 * 30;
    return sel;
}

/* Pick 3: arg = parameter * 10 + alternative. A negative arg draws the parameter: Rand_Range(|arg| / 10) + 1.
 * Then Rand_Range(100) is always drawn. A group that does not have exactly five alternatives plays alternative 0.
 * With five: alternative 8 replaces the parameter by -1 (30%), 10 (30%) or Rand_Range(10) (40%), and the
 * alternatives 5, 8 and 9 mean "any": Rand_Range(5). So one to four draws. */
s32 BtlAiPick_ArgFrames(BtlAiWork *ai, s32 n, s32 arg) {
    BtlAiSeqA *seq = SEQA(ai);
    s32 sel;
    s32 t;
    s32 roll;

    if (arg < 0) {
        sel = -arg % 10;
    } else {
        sel = arg % 10;
    }
    if (arg < 0) {
        t = Rand_Range(-arg / 10) + 1;
    } else {
        t = arg / 10;
    }
    roll = Rand_Range(100);
    seq->timer = t;
    if (n != 5) {
        return 0;
    }
    if (sel == 8) {
        if (roll < 30) {
            seq->timer = -1;
        } else if (roll < 60) {
            seq->timer = 10;
        } else {
            seq->timer = Rand_Range(10);
        }
    }
    if ((u32)(sel - 8) < 2 || sel == 5) {
        sel = BtlAiPick_Random(ai, n, 0);
    }
    return sel;
}

/* Pick 4: how far to charge ki, as the step parameter in percent: arg 0 = 100, 1 = 101 (until powered up), 2 /
 * 3 = the dearer / cheaper of the fighter's first two skill costs (work + 0x14 / + 0x10, set by AiThink_ReadKit).
 * Always alternative 0. No draw. */
s32 BtlAiPick_KiTarget(BtlAiWork *ai, s32 n, s32 arg) {
    BtlAiSeqA *seq = SEQA(ai);

    switch (arg) {
    case 0:
        seq->timer = 100;
        break;
    case 1:
        seq->timer = 101;
        break;
    case 2:
        seq->timer = ai->unk10[1];
        break;
    case 3:
        seq->timer = ai->unk10[0];
        break;
    }
    return 0;
}

/* Pick 5: the planned skill slot (plan.unk8) is the alternative; none (-1) or slot 2 ends the action. The step
 * parameter is 100 (30%), 0 (30%) or Rand_Range(100) (40%). Rand_Range(100) is drawn before the slot is tested,
 * so one or two draws. */
s32 BtlAiPick_SkillCharge(BtlAiWork *ai, s32 n, s32 arg) {
    BtlAiSeqA *seq;
    BtlAiPlan *plan = &ai->plan;
    s32 roll;

    seq = SEQA(ai);
    roll = Rand_Range(100);
    if (plan->unk8 == -1) {
        return -1;
    }
    if (plan->unk8 == 2) {
        return -1;
    }
    if (roll < 30) {
        seq->timer = 100;
    } else if (roll < 60) {
        seq->timer = 0;
    } else {
        seq->timer = Rand_Range(100);
    }
    return plan->unk8;
}

/* Pick 6: a reaction delay in frames as the step parameter. arg 1: b + b * r with b = half of { 45, 33.75, 22.5,
 * 11.25, 0 }[level / 6] and r = Rand_Range(100) / 100, i.e. 22..44 frames at the lowest levels and 0 at the
 * highest; arg 2: 10 + (29 - level) * 20 / 29 (30 frames at level 0, 10 at level 29; 0 for the dummy). The draw
 * is made for every arg. Always alternative 0. */
s32 BtlAiPick_LevelDelay(BtlAiWork *ai, s32 n, s32 arg) {
    f32 tbl[5] = { 45.0f, 33.75f, 22.5f, 11.25f, 0.0f };
    BtlAiSeqA *seq = SEQA(ai);
    f32 r = (s32)Rand_Range(100) * 0.01f;
    s32 level = ai->level>29?29:ai->level;
    f32 base = tbl[CpuLevel_TableIndex(level)] * 0.5f;

    switch (arg) {
    case 1:
        seq->timer = base + base * r;
        break;
    case 2:
        if (level < 0) {
            seq->timer = 0;
        } else {
            seq->timer = (29 - level) * (20.0f / 29.0f) + 10.0f;
        }
        break;
    }
    return 0;
}

/* Pick 7: one Rand_Range(100) against three level-scaled weights of the AI type's profile (bytes 0x15C + 0x10 +
 * step * 3 .. at level 0, 0x41C + .. at level 29), where step is AiThink_GetBlastStep (0, 1 in state class 15, or
 * the place in the rush chain for states 0x3C..0x3F). A group with one alternative clears BTLAI_SEQ_COMBO_SECOND
 * and plays it (the draw is still made). Alternative 1 at step 0 sets that flag; at step 1 with the flag set the result
 * is always alternative 0; alternative 2 becomes 0 when situation bit 61 is set (the opponent's parameter byte
 * +2 is 4). When the roll is above all three weights: alternative 0. */
s32 BtlAiPick_ComboBranch(BtlAiWork *ai, s32 n, s32 arg) {
    s32 roll;
    BtlAiStatus *status;
    BtlAiSeqA *seq = SEQA(ai);
    s32 sum = 0;
    u8 *prof;
    s32 kind;
    u8 *hi;
    u8 *lo;
    s32 i;
    s32 col;

    status = &ai->status;
    prof = AI_DATA->profile[ai->type];
    kind = AiThink_GetBlastStep(ai);
    roll = Rand_Range(100);
    if (n == 1) {
        seq->flags &= ~BTLAI_SEQ_COMBO_SECOND;
        return 0;
    }
    i = 0;
    lo = prof + 0x15C;
    hi = prof + 0x41C;
    col = kind * 3 + 0x10;
    for (; i < 3; i++) {
        sum += BtlAi_ScaleByLevel(ai->level, lo[col], hi[col]);
        col++;
        if (kind == 1 && (seq->flags & BTLAI_SEQ_COMBO_SECOND) && i == 1) {
            return 0;
        }
        if (roll < sum) {
            if (kind == 0 && i == 1) {
                seq->flags |= BTLAI_SEQ_COMBO_SECOND;
            }
            if (i == 2 && (status->flags & 0x2000000000000000)) {
                return 0;
            }
            return i;
        }
    }
    return 0;
}

/* Pick 8: the fusion slot BtlCharApi_PickFusionSlot proposes (it draws Rand_Range; none = alternative 0). */
s32 BtlAiPick_FusionSlot(BtlAiWork *ai, s32 n, s32 arg) {
    s32 slot = BtlCharApi_PickFusionSlot(ai->objId);

    if (slot == -1) {
        slot = 0;
    }
    return slot;
}

/* Pick 9: the transformation slot BtlCharApi_PickTransformSlot proposes (it draws; none = alternative 0). */
s32 BtlAiPick_TransformSlot(BtlAiWork *ai, s32 n, s32 arg) {
    s32 slot = BtlCharApi_PickTransformSlot(ai->objId);

    if (slot == -1) {
        slot = 0;
    }
    return slot;
}

/* Pick 10: which team member to switch to (the step parameter). Members other than the active one with health
 * left are scored health% * 0.7 + ki% * 0.3 and sorted best first; Rand_Range(100) then takes the best with 60%
 * of two, 50 / 35 / 15% of three, 40 / 30 / 30% of four (the fourth is never taken); with any other number of
 * candidates (none, or more than four) the parameter is 0. A single candidate is taken as it is with alternative
 * 0; otherwise the alternative is a second draw, Rand_Range(n). */
s32 BtlAiPick_SwitchMember(BtlAiWork *ai, s32 n, s32 arg) {
    s32 score[8];
    s32 member[8];
    BtlAiSeqA *seq = SEQA(ai);
    s32 found = 0;
    s32 cur = BtlCharApi_GetActiveMember(ai->objId);
    s32 roll = Rand_Range(100);
    s32 count = BtlCharApi_GetMemberCount(ai->objId);
    s32 i;
    s32 j;
    s32 t;
    s32 sel;

    for (i = 0; i < count; i++) {
        if (cur != i && BtlCharApi_GetMemberHpPercent(ai->objId, i) != 0) {
            s32 hp = BtlCharApi_GetMemberHpPercent(ai->objId, i);
            s32 ki = BtlCharApi_GetMemberKiPercent(ai->objId, i);

            score[found] = (s32)(hp * 0.7f) + (s32)(ki * 0.3f);
            member[found] = i;
            found++;
        }
    }
    if (found == 1) {
        seq->timer = member[0];
        return 0;
    }
    for (i = 0; i < found - 1; i++) {
        for (j = found - 1; j > i; j--) {
            if (score[j] > score[j - 1]) {
                t = score[j];
                score[j] = score[j - 1];
                score[j - 1] = t;
                t = member[j];
                member[j] = member[j - 1];
                member[j - 1] = t;
            }
        }
    }
    switch (found) {
    case 2:
        if (roll < 60) {
            sel = member[0];
        } else {
            sel = member[1];
        }
        break;
    case 3:
        if (roll < 50) {
            sel = member[0];
        } else if (roll < 85) {
            sel = member[1];
        } else {
            sel = member[2];
        }
        break;
    case 4:
        if (roll < 40) {
            sel = member[0];
        } else if (roll < 70) {
            sel = member[1];
        } else {
            sel = member[2];
        }
        break;
    default:
        sel = 0;
        break;
    }
    seq->timer = sel;
    return BtlAiPick_Random(ai, n, 0);
}

/* Pick 11: the alternative by the (negative) level of a training dummy: -2 gives 0, -3 gives 1, and so on. */
s32 BtlAiPick_DummyMode(BtlAiWork *ai, s32 n, s32 arg) {
    s32 level = ai->level;

    if (!(level < -1)) {
        return 0;
    }
    return -level - 2;
}

/* Pick 12: the button the fighter's open prompt asks for (BtlCharApi_GetPromptButtons): UP, DOWN, LEFT, RIGHT,
 * BLAST = alternatives 0..4; no prompt ends the action. No draw: the CPU always answers correctly. */
s32 BtlAiPick_PromptButton(BtlAiWork *ai, s32 n, s32 arg) {
    s32 prompt = BtlCharApi_GetPromptButtons(ai->objId);

    if (prompt & 0x10) {
        return 0;
    }
    if (prompt & 0x20) {
        return 1;
    }
    if (prompt & 0x40) {
        return 2;
    }
    if (prompt & 0x80) {
        return 3;
    }
    if (prompt & 4) {
        return 4;
    }
    return -1;
}

/* Pick 13: the step parameter of step handler 8 by the rule's argument (1 -> 0, 2 -> 15, 3 -> -1); any other
 * argument ends the action. Always alternative 0. */
s32 BtlAiPick_GuardMode(BtlAiWork *ai, s32 n, s32 arg) {
    BtlAiSeqA *seq = SEQA(ai);

    switch (arg) {
    case 1:
        seq->timer = 0;
        return 0;
    case 2:
        seq->timer = 15;
        return 0;
    case 3:
        seq->timer = -1;
        return 0;
    }
    return -1;
}

/* Pick 14: a direction. arg = seconds * 10 + alternative as in pick 2. A group of one plays it (without setting
 * the time). A group of 4 or 8 ignores the alternative in arg and draws: each alternative weighs 10, Rand_Range
 * (n * 10). But when the fighter is further from the stage centre than half the stage radius (or on stages 4 /
 * 27), n probes of length 15 are made from the fighter, starting along its facing and turning 45 degrees each
 * time (also with four alternatives), and the alternative whose probe ends furthest from the centre weighs 3
 * instead of 10, with eight alternatives also its two neighbours: Rand_Range(33) or Rand_Range(59). Other group
 * sizes take the alternative from arg. */
s32 BtlAiPick_Direction(BtlAiWork *ai, s32 n, s32 arg) {
    BtlAiVec pos;
    BtlAiVec rot;
    BtlAiVec dir;
    BtlAiVec probe;
    BtlAiSeqA *seq = SEQA(ai);
    s32 sel = arg < 0 ? -arg % 10 : arg % 10;
    s32 sum = 0;
    s32 special = BtlChar_IsStage4Or27();
    s32 best;
    s32 roll;
    s32 i;
    s32 range;
    f32 radius;
    f32 far;
    f32 len;

    if (n == 1) {
        return 0;
    }
    if (n == 8 || n == 4) {
        best = -2;
        radius = BtlStage_GetInnerRadius();
        range = n * 10;
        BtlCharApi_GetPos(ai->objId, &pos);
        len = Vec3_Length(&pos);
        far = 0.0f;
        if (radius * 0.5f < len || special != 0) {
            BtlCharApi_GetRot(ai->objId, &rot);
            dir.x = sinf(rot.y);
            dir.y = far;
            dir.z = cosf(rot.y);
            dir.w = far;
            Vec3_Normalize(&dir, &dir);
            for (i = 0; i < n; i++) {
                Vec3_Scale(&probe, &dir, 15.0f);
                Vec3_Add(&probe, &pos, &probe);
                len = Vec3_Length(&probe);
                if (far < len) {
                    far = len;
                    best = i;
                }
                Vec3_RotateY(&dir, &dir, 0.7853981f);
            }
            range = n == 8 ? 0x3B : 0x21;
        }
        roll = Rand_Range(range);
        for (i = 0; i < n; i++) {
            if (n == 8) {
                if (best != -2 && (best == i || (best + 1) % n == i || (best + 7) % n == i)) {
                    sum += 3;
                } else {
                    sum += 10;
                }
            } else {
                if (best == i) {
                    sum += 3;
                } else {
                    sum += 10;
                }
            }
            if (roll < sum) {
                sel = i;
                break;
            }
        }
    }
    if (sel >= n) {
        return -1;
    }
    seq->timer = arg / 10 * 30;
    return sel;
}

/* Pick 15: the rule's argument is the step parameter. Always alternative 0. */
s32 BtlAiPick_SetTimer(BtlAiWork *ai, s32 n, s32 arg) {
    ai->seq.timer = arg;
    return 0;
}

/* ---- Step handlers 0..13 (gBtlAiStepFuncs). Non-zero = step finished. ---- */

/* Step handler 0, the default: finished when the fighter is in the state class of the chosen step, in a state
 * other than the one it started in (or in class 0) whose flag bit 1 is clear. Finished at once when the action's
 * first step has no class (-1); action 0x2E is finished as soon as the input took effect. */
s32 BtlAiStep_ReachClass(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiSeqEntry *top = &SEQ_TOP(seq);
    BtlAiSeqActTable *act = AI_DATA->act;
    BtlAiSeqBody *body = &act->body;
    BtlAiSeqStep *step = &act->steps[body->first[top->id]];
    s32 state = BtlCharApi_GetUnk974(ai->objId);
    s8 cls = body->stateClass[state];
    u8 flags = body->stateFlags[state];

    if (step->cls == -1) {
        return 1;
    }
    if (seq->startState == state && cls != 0) {
        return 0;
    }
    if (top->id == 0x2E && (seq->flags & BTLAI_SEQ_TOOK_EFFECT)) {
        return 1;
    }
    if (flags & 2) {
        return 0;
    }
    step = &step[seq->step];
    return cls == step->cls;
}

/* Step handler 1 (ki charge): finished when ki has reached the step parameter in percent of 100000 (101: when
 * the powered-up mode is on), or as soon as BtlAiSense_IsBehindOpponent is true for the opponent's side, i.e. this
 * fighter is turned away from the opponent. */
s32 BtlAiStep_ChargeKi(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    s32 ki = BtlSide_GetKi(ai->objId);
    s32 want = (f32)seq->timer / 100.0f * 100000.0f;

    if (BtlAiSense_IsBehindOpponent(&gBtlAi->work[TagTeam_Target(ai->objId)]) != 0) {
        return 1;
    }
    if (seq->timer == 101) {
        if (BtlSide_IsPoweredUp(ai->objId) != 0) {
            return 1;
        }
    } else if (!(ki < want)) {
        return 1;
    }
    return 0;
}

/* Step handler 2 (approach): finished in state class 0, when the opponent blocks the way
 * (BtlCharApi_IsBlockedByOpponent) or when bit 29 of the fighter word +0x1288 is set. Refreshes seq->startDist. */
s32 BtlAiStep_UntilBlocked(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiSeqActTable *act = AI_DATA->act;
    s32 blocked = BtlCharApi_IsBlockedByOpponent(ai->objId);
    s8 cls = act->body.stateClass[BtlCharApi_GetUnk974(ai->objId)];
    u64 flag = BtlCharApi_GetUnk1288(ai->objId) & 0x20000000;

    if (cls == 0) {
        return 1;
    }
    seq->startDist = gBtlAi->dist;
    if (flag) {
        return 1;
    }
    return blocked;
}

/* Step handler 3: finished after the step parameter's number of frames (not counted while any fighter has flag
 * 0x128). */
s32 BtlAiStep_Countdown(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);

    if (BtlCharApi_AnyHasFlag128() != 0) {
        return 0;
    }
    seq->timer--;
    return seq->timer < 1;
}

/* Step handler 4: handler 2 or handler 7. */
s32 BtlAiStep_BlockedOrCharged(BtlAiWork *ai) {
    s32 a = BtlAiStep_UntilBlocked(ai);
    s32 b = BtlAiStep_Charged(ai);
    s32 ret = 0;

    if (a != 0 || b != 0) {
        ret = 1;
    }
    return ret;
}

/* Step handler 5: handler 2 and handler 7. */
s32 BtlAiStep_BlockedAndCharged(BtlAiWork *ai) {
    s32 a = BtlAiStep_UntilBlocked(ai);
    s32 b = BtlAiStep_Charged(ai);

    return a != 0 && b != 0;
}

/* Step handler 6: finished when the fighter's state class is not 1..7. */
s32 BtlAiStep_NotHit(BtlAiWork *ai) {
    s8 cls = AI_DATA->act->body.stateClass[BtlCharApi_GetUnk974(ai->objId)];

    if ((u32)(cls - 1) < 7) {
        return 0;
    }
    return 1;
}

/* Step handler 7 (held attack): finished when BtlCharApi_GetChargeRate * 10 has reached the step parameter; with
 * -1, one frame after the charge is full (1.0). Also finished when the charge went down, or is 0 after the input
 * took effect. */
s32 BtlAiStep_Charged(BtlAiWork *ai) {
    BtlAiSeqA *seq;
    f32 rate = BtlCharApi_GetChargeRate(ai->objId);

    seq = SEQA(ai);
    if ((seq->flags & BTLAI_SEQ_TOOK_EFFECT) && rate == 0.0f) {
        return 1;
    }
    if (rate < seq->charge) {
        return 1;
    }
    seq->charge = rate;
    if (seq->timer == -1) {
        if (rate == 1.0f) {
            if ((seq->flags & BTLAI_SEQ_FULL_CHARGE) == 0) {
                seq->flags |= BTLAI_SEQ_FULL_CHARGE;
            } else {
                return 1;
            }
        }
        return 0;
    }
    if ((f32)seq->timer <= rate * 10.0f) {
        return 1;
    }
    return 0;
}

/* Step handler 8: the guard step. What ends it depends on the step parameter (pick 13): -1 = the opponent's
 * technique is not of kind 1 or 2; 15 (any other value) = no foreign blast is flying past; 0 = the opponent has
 * stopped attacking (BtlCharApi_CheckUnkCAC) and its state has flag bit 1 clear. For action 0x45 it also picks
 * the step to play from the opponent's state class (0x10 -> 1, 0x11 -> 2, else 0) on every frame. */
/* FAKE MATCH (permuter): two constants held in variables. `hitCls = 0x10` is set at the top (in front of the
 * flag), and `none = 0` is set behind the id block and compared with the timer. Without them 2 of 74 instructions
 * differ: `sltiu v0,v0,1` (the "kind is not 1 or 2" result) and `li a1,0x10` in front of / in the delay slot of
 * the `bne id,0x45` branch come out exchanged. What happens (RTL dumps): written as a literal, the 0x10 is created
 * in the next block and the first scheduling pass moves it up with priority 2 while the flag has priority 1; as a
 * variable it is an instruction of the first block (that alone gives 5 differences, the same instructions in
 * another order). `none = 0` is one more instruction in the timer block that the timer branch depends on until
 * reload replaces the pseudo by the constant; it changes the order in which the instructions of that block are
 * moved up (`li a2,-1`). Only the compare `timer != none` needs it; `none` at its declaration, in front of the
 * id block, or used only in the later returns does not work, nor does `hitCls` set behind the flag.
 * What the two stand for is unknown (a dependent of the flag in the first block that is gone afterwards would do
 * the same). Natural forms that were tried and do not match: `ret = !(...)`, an if / else pair, a switch (on the
 * kind and on the timer), a ternary, the test inside the `timer == -1` branch, an inline helper returning the
 * flag or picking the step or deciding the result, a result variable with one `return`, `kind - 1` and the range
 * flag as locals, the class chain nested the other way, 0x11 in a variable as well.
 * Behaviour is identical in all of them. */
s32 BtlAiStep_GuardUntilSafe(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    BtlAiSeqActTable *act = AI_DATA->act;
    s32 state = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    s8 cls = act->body.stateClass[state];
    s32 attacking = BtlCharApi_CheckUnkCAC(TagTeam_Target(ai->objId));
    s32 passing = BtlCharApi_IsBlastPassing(ai->objId);
    BtlAiSeqEntry *top = &SEQ_TOP(seq);
    u8 flags = act->body.stateFlags[state];
    s32 kind = BtlCharApi_GetOppSkillKind(ai->objId);
    s32 hitCls = 0x10;
    s32 ret = 0;
    s32 none;

    if (!((u32)(kind - 1) < 2)) {
        ret = 1;
    }
    if (top->id == 0x45) {
        if (cls == hitCls) {
            seq->step = 1;
        } else if (cls == 0x11) {
            seq->step = 2;
        } else {
            seq->step = 0;
        }
    }
    none = 0;
    if (seq->timer == -1) {
        return ret;
    }
    if (seq->timer != none) {
        return passing == 0;
    }
    if (flags & 2) {
        return 0;
    }
    return attacking == 0;
}

/* Step handler 9: finished 30 frames after the opponent stopped attacking (BtlCharApi_CheckUnkCAC; the count
 * restarts while it attacks, and stands still while any fighter has flag 0x128); never while the opponent's
 * state has flag bit 1. With no attack at all it ends when the step parameter has been counted below 0. */
s32 BtlAiStep_GuardCountdown(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    u8 flags = AI_DATA->act->body.stateFlags[BtlCharApi_GetUnk974(TagTeam_Target(ai->objId))];
    s32 attacking = BtlCharApi_CheckUnkCAC(TagTeam_Target(ai->objId));

    if (flags & 2) {
        return 0;
    }
    if (attacking != 0) {
        seq->timer = 30;
    }
    if (BtlCharApi_AnyHasFlag128() != 0) {
        return 0;
    }
    seq->timer--;
    if (seq->timer < 0) {
        return 1;
    }
    return 0;
}

/* Step handler 10: handler 0, and no ki blast shots left to fire. */
s32 BtlAiStep_ReachClassNoShots(BtlAiWork *ai) {
    s32 done = BtlAiStep_ReachClass(ai);
    s32 shots = BtlCharApi_GetBlastShots(ai->objId);

    return done != 0 && shots == 0;
}

/* Step handler 11: finished on the second frame (the buttons are sent once). */
s32 BtlAiStep_OneFrame(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);

    if (seq->timer == -1) {
        return 1;
    }
    seq->timer = -1;
    return 0;
}

/* Step handler 12: finished when the fighter faces the opponent (facing . direction to the opponent > 0, on
 * the ground plane). While the facing did not change since the last frame the pad marks are cleared, so "once"
 * buttons are sent again. */
s32 BtlAiStep_FacingOpponent(BtlAiWork *ai) {
    BtlAiVec rot;
    BtlAiVec face;
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec dir;
    BtlAiSeqA *seq = SEQA(ai);

    BtlCharApi_GetRot(ai->objId, &rot);
    face.x = sinf(rot.y);
    face.y = 0.0f;
    face.z = cosf(rot.y);
    face.w = 0.0f;
    Vec3_Normalize(&face, &face);
    if (seq->dirX == face.x && seq->dirZ == face.z) {
        BtlAiPad_Clear(&ai->out, 0);
    }
    seq->dirZ = face.z;
    seq->dirX = face.x;
    BtlCharApi_GetPos(ai->objId, &pos);
    BtlCharApi_GetPos(TagTeam_Target(ai->objId), &opp);
    Vec3_Sub(&rot, &opp, &pos);
    rot.y = 0.0f;
    Vec3_Normalize(&dir, &rot);
    if (0.0f < Vec3_Dot(&face, &dir)) {
        return 1;
    }
    return 0;
}

/* Step handler 13: finished when the fighter's switch target is the member pick 10 chose. */
s32 BtlAiStep_SwitchQueued(BtlAiWork *ai) {
    BtlAiSeqA *seq = SEQA(ai);
    s32 target = BtlCharApi_GetSwitchTarget(ai->objId);
    s32 want = seq->timer;

    return want == target;
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part, 0x1B6008..0x1B6D50 (the file as it was before the first part was decompiled): step handlers
 * 14..23, the per-frame sequence runner and the input hand-over. Same header, no cast macros needed.
 *
 * Per frame (BtlAiMgr_Update, 0x1BB620, for each CPU-controlled fighter): sense (0x1BFF70), think (0x1BAC30: the
 * rule lists of the AI data, tested with the BtlAiCond_* functions), BtlAi_RunSeq, BtlAi_SendInput.
 *
 * The object's .rodata starts at 0x2ED8C0 with the jump table of BtlAiSeq_CheckStageAbort (0x1B4300); the object
 * itself starts at 0x1B3F78 (eft_det_b_c.c, whose table D_002ED8A0 is in front of it). Where the object ends: see
 * the note at the top of btl_ai_cond.c.
 * ------------------------------------------------------------------------------------------------------------ */

/* ---- Step handlers 14..23 of the table at 0x2C4708 (first object). Non-zero = step finished. ---- */

/* Step handler 14: compares a fighter value of both sides (kind 1 or 2 in seq->timer), picks one of two AI-type
 * rates for it and, on a successful roll, moves seq->step on (modulo 4). */
s32 BtlAiStep_Unk14(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    s8 cls = gBtlAi->data->act->actClass[BtlCharApi_GetUnk974(ai->objId)];
    s32 roll = Rand_Range(100);
    u8 *prof = gBtlAi->data->profile[ai->type];
    u8 *lo = prof + 0x2AC;
    u8 *hi = prof + 0x56C;
    s32 val[2];
    s32 a;
    s32 b;

    switch (seq->timer) {
    case 1:
        a = BtlAi_ScaleByLevel(ai->level, lo[4], hi[4]);
        b = BtlAi_ScaleByLevel(ai->level, lo[3], hi[3]);
        val[0] = BtlCharApi_GetClashCountB(ai->objId);
        val[1] = BtlCharApi_GetClashCountB(TagTeam_Target(ai->objId));
        break;
    case 2:
        a = BtlAi_ScaleByLevel(ai->level, lo[6], hi[6]);
        b = BtlAi_ScaleByLevel(ai->level, lo[5], hi[5]);
        val[0] = BtlCharApi_GetClashCountA(ai->objId);
        val[1] = BtlCharApi_GetClashCountA(TagTeam_Target(ai->objId));
        break;
    default:
        return 1;
    }
    if (!(val[0] > val[1])) {
        a = b;
    }
    if (roll < a) {
        seq->step = (seq->step + 1) % 4;
    }
    switch (seq->timer) {
    case 1:
        if (cls == 0x20) {
            break;
        }
        return 1;
    case 2:
        if (BtlCharApi_IsInClashA(ai->objId)) {
            break;
        }
        return 1;
    default:
        return 0;
    }
    return 0;
}

/* Step handler 15: runs BtlAiStep_ReachClass; once that is done, checks the queued move and arms the 300-frame cooldown. */
s32 BtlAiStep_Unk15(BtlAiWork *ai) {
    BtlAiPlan *plan = &ai->plan;
    s32 busy = BtlAiStep_ReachClass(ai);
    BtlAiChrMoves *p = BtlCharApi_GetMoveTable(ai->objId);
    s32 step;
    BtlAiSeq *seq;

    seq = &ai->seq;
    step = seq->step;
    if ((u32)step >= 2) {
        return 1;
    }
    if (busy != 0) {
        return 1;
    }
    if (p->unk9E[step] == 4) {
        seq->flags |= 0x400;
    }
    switch (p->unk10[step]) {
    case 0xC:
    case 0x33:
    case 0x37:
        if (BtlCharApi_IsMoveSlotActive(ai->objId, step) != 0) {
            return 1;
        }
        break;
    }
    plan->cooldown = 300;
    return 0;
}

/* Step handler 16: waits up to 60 frames for the opponent to come within twice move.dist[1]. */
s32 BtlAiStep_WaitNear2(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiMoveWork *move = &ai->move;

    seq->timer++;
    if (seq->timer > 60) {
        return 1;
    }
    if (move->dist[1] + move->dist[1] < gBtlAi->dist) {
        return 0;
    }
    return 1;
}

/* Step handler 17: waits for a skill (plan.unk8) to become affordable or usable against the opponent's action. */
/* Matching notes:
 * - the shared `return 0` sits directly behind the stage 4 / 27 block and the class tail is the END of the
 *   function: a `goto` over the `return 0` (early returns put `move v0,zero` into the flag update);
 * - `if (flags & 0x40) { ... }` falls into the shared `return 0`;
 * - `if (TestPoseBit80() == 0) { seq->step = 2; return 0; }` (v0 is still the call's 0);
 * - the unsigned copy of cls[1] is a local read in front of the `cls[1] == kind` test;
 * - the tail is nested under `if (kind == 2)` and `if (cls[1] != 0x1C)`, both falling out to the final
 *   `return 1`. With `if (kind != 2) return 1;` the code differs only in one delay slot (`ld s1` instead of
 *   `nop` at 0x1B6528): the return value is then loaded in front of that branch, the last jump pass deletes the
 *   same load in front of the `cls[0]` branch as redundant, and the delay-slot pass gives that branch the first
 *   epilogue load at once. In the original that branch first holds `li v0,1`, loses it as redundant in the
 *   first relax pass and gets `ld s0` only in the second round, after the range-test branch took its own copy
 *   of `ld s0`, which the last relax pass deletes: an empty slot that is never refilled.
 * Differential test against the original bytes: build/scratch_cleanup3_S/t_unk17.py. */
s32 BtlAiStep_FireSkill(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiPlan *plan = &ai->plan;
    BtlAiChrSkills *p = BtlCharApi_GetSkillTable(ai->objId);
    BtlAiActTable *act = gBtlAi->data->act;
    s32 busy = BtlAiStep_ReachClass(ai);
    f32 rate = BtlCharApi_GetUnkE44B(ai->objId);
    u32 *top = &SEQ_TOP(seq).id;
    s32 action[2];
    s8 cls[2];
    BtlAiActBody *tbl;
    s8 kind;
    u8 uc;

    if (plan->unk8 == -1) {
        return 1;
    }
    action[0] = BtlCharApi_GetUnk974(ai->objId);
    action[1] = BtlCharApi_GetUnk974(TagTeam_Target(ai->objId));
    tbl = (BtlAiActBody *)((u8 *)act + 8);
    cls[0] = tbl->actClass[action[0]];
    cls[1] = tbl->actClass[action[1]];
    if (busy == 0 && !(seq->flags & 0x1000)) {
        if (seq->flags & 0x40) {
            if (p->cost[plan->unk8] > BtlSide_GetKi(ai->objId)) {
                return 1;
            }
            seq->flags = (seq->flags ^ 0x800) & ~0x40;
        }
    } else {
        seq->flags |= 0x1000;
        if (*top == 0x3F) {
            return 1;
        }
        seq->unk58 = 0;
        if (!(p->unk165[plan->unk8] == 5 && BtlChar_IsStage4Or27() != 0)) {
            goto tail;
        }
        if (cls[0] != 0x16) {
            return 1;
        }
        seq->step = 3;
        if (BtlCharApi_TestPoseBit80(ai->objId, 1) == 0) {
            seq->step = 2;
            return 0;
        }
    }
    return 0;
tail:
    kind = p->unk13E[plan->unk8];
    if (kind == 2) {
        if (cls[0] != 0x16) {
            return 1;
        }
        uc = cls[1];
        if (cls[1] == kind) {
            return 0;
        }
        if (cls[1] != 0x1C) {
            if (cls[1] != cls[0] && cls[1] != 0x17) {
                if (cls[1] == 0x19) {
                    return 1;
                }
                if (cls[1] == 0x18) {
                    return 1;
                }
                if ((u32)(uc - 0xF) < 3) {
                    return 1;
                }
                if ((s32)(rate * 100.0f) < seq->timer) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

/* Step handler 18: counts seq->timer down, reloading it by level and distance, then runs BtlAiStep_NotHit. */
s32 BtlAiStep_Unk18(BtlAiWork *ai) {
    s32 near[5] = { 15, 7, 5, 3, 3 };
    s32 far[5] = { 15, 10, 7, 5, 3 };
    BtlAiSeq *seq = &ai->seq;
    s32 dist = BtlCharApi_GetHp(ai->objId);
    s32 t = seq->timer;

    seq->step = 1;
    seq->timer = t - 1;
    if ((u32)(t - 2) >= 15) {
        if (dist < 10000) {
            seq->timer = near[LEVEL_IDX(ai)];
        } else {
            seq->timer = far[LEVEL_IDX(ai)];
        }
        seq->step = 0;
    }
    return BtlAiStep_NotHit(ai);
}

/* Step handler 19: same countdown, done when BtlCharApi_GetStunTimer is not positive. */
s32 BtlAiStep_Unk19(BtlAiWork *ai) {
    s32 near[5] = { 15, 7, 5, 3, 3 };
    s32 far[5] = { 15, 10, 7, 5, 3 };
    BtlAiSeq *seq = &ai->seq;
    s32 dist = BtlCharApi_GetHp(ai->objId);
    s32 t = seq->timer;

    seq->step = 1;
    seq->timer = t - 1;
    if ((u32)(t - 2) >= 15) {
        if (dist < 10000) {
            seq->timer = near[LEVEL_IDX(ai)];
        } else {
            seq->timer = far[LEVEL_IDX(ai)];
        }
        seq->step = 0;
    }
    return BtlCharApi_GetStunTimer(ai->objId) < 1;
}

/* Step handler 20: done when BtlCharApi_IsOppSkillFlag4 is zero. */
s32 BtlAiStep_Unk20(BtlAiWork *ai) {
    return BtlCharApi_IsOppSkillFlag4(ai->objId) == 0;
}

/* Step handler 21: same countdown, done when BtlCharApi_TestFlagBE is zero. */
s32 BtlAiStep_Unk21(BtlAiWork *ai) {
    s32 near[5] = { 15, 7, 5, 3, 3 };
    s32 far[5] = { 15, 10, 7, 5, 3 };
    BtlAiSeq *seq = &ai->seq;
    s32 dist = BtlCharApi_GetHp(ai->objId);
    s32 t = seq->timer;

    seq->step = 1;
    seq->timer = t - 1;
    if ((u32)(t - 2) >= 15) {
        if (dist < 10000) {
            seq->timer = near[LEVEL_IDX(ai)];
        } else {
            seq->timer = far[LEVEL_IDX(ai)];
        }
        seq->step = 0;
    }
    return BtlCharApi_TestFlagBE(ai->objId) == 0;
}

/* Step handler 22: like 16 with three times the range, then BtlAiStep_ReachClass. */
s32 BtlAiStep_WaitNear3(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    BtlAiMoveWork *move = &ai->move;

    seq->timer++;
    if (seq->timer > 60) {
        return 1;
    }
    if (move->dist[1] * 3.0f < gBtlAi->dist) {
        return 0;
    }
    return BtlAiStep_ReachClass(ai);
}

/* Step handler 23: after BtlAiStep_ReachClass, on stage 4 or 27, picks the next step from a skill slot lookup. */
s32 BtlAiStep_Unk23(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;
    s32 busy = BtlAiStep_ReachClass(ai);
    BtlAiChrSkills *p = BtlCharApi_GetSkillTable(ai->objId);
    s8 cls = gBtlAi->data->act->actClass[BtlCharApi_GetUnk974(ai->objId)];
    s32 n;

    if (busy == 0) {
        return 0;
    }
    if (BtlChar_IsStage4Or27() == 0) {
        return 1;
    }
    switch (seq->unk7C) {
    case 0x32:
        n = 7;
        break;
    case 0x2A:
        n = 4;
        break;
    case 0x24:
        n = 2;
        break;
    case 0x35:
        n = 8;
        break;
    case 0x26:
        n = 6;
        break;
    case 0x22:
        n = 1;
        break;
    case 0x2B:
        n = 5;
        break;
    case 0x29:
        n = 3;
        break;
    case 0x2D:
    case 0x2E:
        n = 0;
        break;
    default:
        return 0;
    }
    if (p->unk165[BtlCharApi_GetParamByte8F(ai->objId, n)] != 5) {
        return 1;
    }
    seq->step = 0;
    if (BtlCharApi_TestPoseBit80(ai->objId, 1) == 0) {
        seq->step = 1;
    }
    return cls == 0;
}

/* ---- Top level. The object boundary is somewhere between here and BtlAi_ScaleByLevel. ---- */

/* Runs sequence 3: a one-shot chosen by the entry's kind byte. */
void BtlAi_RunInstant(BtlAiWork *ai) {
    BtlAiSeq *seq = &ai->seq;

    switch (SEQ_TOP(seq).arg) {
    case 0:
        BtlCharApi_MarkIncomingBlast(ai->objId);
        break;
    case 1:
        seq->flags |= 0x80;
        break;
    case 2:
        BtlAi_NoteOpponent(ai, 0x20000);
        break;
    }
}

/* Once per frame: clears the output, runs the sequence on top of the stack, then finishes the output. */
void BtlAi_RunSeq(BtlAiWork *ai) {
    BtlAiOutput *out = &ai->out;
    BtlAiSeq *seq = &ai->seq;

    BtlAiPad_Clear(out, 1);
    if (seq->depth > 0) {
        switch (SEQ_TOP(seq).id) {
        case 3:
            BtlAi_RunInstant(ai);
            seq->depth--;
            break;
        case 7:
            BtlAiMove_Dispatch(ai);
            break;
        case 0x19:
            BtlAiCombo_Dispatch(ai);
            break;
        case 0x1A:
            BtlAiFollow_Dispatch(ai);
            break;
        case 0x36:
            BtlAiAct36_Dispatch(ai);
            break;
        default:
            gBtlAiStateFuncs[seq->phase](ai);
            break;
        }
        BtlAiPad_EndFrame(out);
    }
}

/* Hands this frame's buttons and stick to the fighter (the same path a pad feeds). */
void BtlAi_SendInput(BtlAiWork *ai) {
    BtlCharApi_SetInjectedInput(ai->objId, ai->out.buttons, ai->out.stickX, ai->out.stickY);
}

/* Interpolates between lo (level 0) and hi (level 29). Last function of this object: its callers in the next one
 * (btl_ai_cond.c) only compile to the original bytes when it is NOT defined in their translation unit. */
s32 BtlAi_ScaleByLevel(s32 level, s32 lo, s32 hi) {
    if (level < 0) {
        level = 0;
    }
    return lo + (s32)((f32)(hi - lo) / 29.0f * (f32)level + 0.01f);
}
