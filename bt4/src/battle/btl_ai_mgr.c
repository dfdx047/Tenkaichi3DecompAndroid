extern int TagBattle_Active(void);
#include "common.h"
extern s32 TagBattle_Target(s32 actor);
#include "battle/btl_ai_mgr.h"
#include "battle/battle.h"
#include "battle/tag_team_native.h"
#include "battle/btl_seq.h"
#include "sys/heap.h"
#include "sys/rand.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * CPU opponent manager and the "move" action: 0x1BB128..0x1BC8A8.
 *
 * gBtlAi is one heap block holding the AI data pointer, three values shared by both sides (distance between
 * the fighters, their radius sum, the line-of-sight test) and one 0x520-byte controller per side.
 *
 * BtlAiMgr_Update is the frame entry (Battle_Loop, after Battle_UpdateWork, before the fighters sample
 * input). It does nothing while the battle is paused (flag 0x100), an object load job runs (0x1000), the
 * stage is not ready, or the sequence is not in state 2 (Ready) or 3 (Fight). Otherwise it refreshes the
 * shared values, counts the frame, and for each of the first two fighters that takes AI input
 * (fighter +0x1278) and has parameters, runs four passes that live outside this file: BtlAiSense_Update,
 * AiThink_Think, BtlAi_RunSeq, BtlAi_SendInput.
 *
 * The second half of the file (0x1BB730..) is one "action": a table of four phase functions
 * (gBtlAiMovePhases = {Init, Start, Run, End}) indexed by act.phase and called by the dispatcher
 * BtlAiMove_Dispatch. It moves the fighter to a target computed from the opponent's position: straight when the
 * stage does not block the way, otherwise along a path from StgNav_FindPath. Its only output is the side's
 * virtual pad, through BtlAiPad_Clear / BtlAiPad_Set (AI button bits: BTLAI_BTN_* in the header).
 *
 * Randomness: Rand_Range (the shared MT19937) once in BtlAiMove_PickDir (once per start of the action) and
 * twice in BtlAiMove_CalcTarget when the move type is 4. Outside the simulation state this file reads only
 * battle flags 0x100 / 0x1000, BtlStage_IsReady and the sequence state, all in BtlAiMgr_Update. No pad,
 * sound or file state.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec3_Add(BtlAiVec *dst, BtlAiVec *a, BtlAiVec *b);
extern void Vec3_Sub(BtlAiVec *dst, BtlAiVec *a, BtlAiVec *b);
extern void Vec4_Scale(BtlAiVec *dst, BtlAiVec *src, f32 k);
extern void Vec3_Normalize(BtlAiVec *dst, BtlAiVec *src);
extern f32 Vec3_Length(BtlAiVec *v);
extern f32 Vec3_Dot(BtlAiVec *a, BtlAiVec *b);
extern void Vec4_SetZero(BtlAiVec *v);                         /* v = (0, 0, 0, 0) */
extern void Vec3_RotateY(BtlAiVec *dst, BtlAiVec *src, f32 angle); /* rotate about Y */
extern f32 Mathf_Cos(f32 angle);                        /* cos */
extern f32 Mathf_Sin(f32 angle);                        /* sin */

extern s32 BtlStage_IsReady(void);
extern s32 BtlChar_GetCount(void);
extern BtlAiMgrChr *BtlChar_Get(s32 side);
extern s32 BtlChar_IsStage4Or27(void);                        /* Battle_GetStage() is 4 or 27 */

/* Fighter accessors by object id (0x204xxx..0x208xxx, not decompiled). */
extern void BtlCharApi_GetPos(s32 objId, BtlAiVec *out);       /* position: transform +0 plus +0x20, or object +0x970 */
extern f32 BtlCharApi_GetHeight(s32 objId);                   /* object +0xFF4 (height), 10.0 without object */
extern f32 BtlCharApi_GetGroundY(s32 objId);                   /* fighter transform +0xB4 */
extern f32 BtlCharApi_GetSpeed(s32 objId);                   /* fighter transform +0x98 */
extern f32 BtlCharApi_GetRadius(s32 objId);                   /* body radius: object +0x91C->+0xC, or +0xFF0 * constant */
extern void BtlCharApi_GetVelocity(s32 objId, BtlAiVec *out);       /* fighter transform +0x30 */
extern s32 BtlCharApi_IsClose(s32 objId);                   /* BtlChar_TestFlag(chr, 0x13) */
extern s32 BtlCharApi_TestFlag0F(s32 objId);                   /* BtlChar_TestFlag(chr, 0xF) */
extern s32 BtlCharApi_GetUnk974(s32 objId);                   /* fighter +0x974: current state id */
extern s32 BtlCharApi_IsInputInjected(s32 objId);                   /* fighter +0x1278: input comes from the AI */

/* Stage (0x23Fxxx / 0x242xxx, not decompiled). */
extern f32 BtlStage_GetRadius(void);                        /* stage limit: first float of the stage's +0x40 block */
extern f32 BtlStage_GetInnerRadius(void);                        /* the same - 100 */
extern f32 BtlStage_GetTop(void);                        /* second float of that block */
extern void BtlStage_GetStartPlace(s32 objId, BtlAiVec *out, BtlAiVec *out2, s32 arg);

/* Other AI code. */
extern s32 StgCol_TraceSegment(BtlAiSegment *seg);           /* stage line test; fills the BtlAiHit block */
extern BtlAiHit *StgCol_GetHit(void);
extern void StgNav_FindPath(BtlAiVec *from, BtlAiVec *to, BtlAiMovePath *path);
extern void BtlAiSeq_Reset(BtlAiSeq *act);           /* clears the action state */
extern s32 AiThink_FindBasicColumn(s32 button, s32 arg);
extern void AiThink_BuildTotals(BtlAiWork *s);
extern void AiThink_Think(BtlAiWork *s);
extern void AiThink_BindData(s32 arg);                    /* binds the AI data section of gCommonRes */
extern void AiThink_ResetSide(s32 side, s32 arg);          /* resets one side's controller from its fighter */
extern void BtlAiPad_Clear(BtlAiOutput *out, s32 keep); /* clears the virtual pad */
/* Writes the virtual pad. hold / press / once are AI button bits (BTLAI_BTN_*): hold is passed on every
   frame, press only for a button not marked 1 in the output's per-button word, once only for a button not
   yet marked 2 (and marks it). x, y are averaged with the previous stick. The parameter order is the one
   that makes every call here match. */
extern void BtlAiPad_Set(BtlAiWork *s, s32 hold, s32 press, s32 once, s16 special, f32 x, f32 y);

/* The divisions by 10 in BtlAiMove_Start are real divide instructions with 10 in a register, reloaded for
   each one: that is what a constant passed to an inlined helper gives (a literal gives a multiply). */
static inline s32 BtlAiMove_Div(s32 a, s32 b) {
    return a / b;
}

static inline s32 BtlAiMove_Mod(s32 a, s32 b) {
    return a % b;
}
extern void BtlAiSense_Update(BtlAiWork *s);

extern BtlAi *gBtlAi;
static BtlAiWork sTagAI[8];
static s32 sTagTargetSince[10];
void *BtlAi_ActorWork(s32 actor) { return actor>=2 && actor<10 ? &sTagAI[actor-2] : &gBtlAi->work[actor]; }

/* Frees what the block owns, rebinds the shared AI data and resets both sides from their fighters. */
void BtlAiMgr_Reset(void) {
    BtlAiWork *s;
    s32 i;
    memset(sTagTargetSince,0,sizeof(sTagTargetSince));

    if (gBtlAi->flags & 1) {
        if (gBtlAi->data != NULL) {
            Heap_Free(gBtlAi->data);
            gBtlAi->data = NULL;
        }
    }
    for (i=0;i<TagBattle_Count();++i) {
        s=BtlAi_ActorWork(i);
        if (s->flags & 1) {
            if (s->param != NULL) {
                Heap_Free(s->param);
                s->param = NULL;
            }
        }
    }
    AiThink_BindData(0);
    for (i = 0; i < TagBattle_Count(); i++) {
        AiThink_ResetSide(i, 0);
    }
}

/* Resets one side's controller from its fighter. */
void BtlAiMgr_ResetSide(s32 side) {
    AiThink_ResetSide(side, 0);
}

/* Allocates the manager block if needed and resets it. */
void BtlAiMgr_Init(void) {
    if (gBtlAi == NULL) {
        memset(gBtlAi = Heap_Alloc(sizeof(BtlAi), 0x20, 0, 2), 0, sizeof(BtlAi));
    }
    BtlAiMgr_Reset();
}

/* Frees what the block owns, then the block. */
void BtlAiMgr_Term(void) {
    BtlAiWork *s; s32 i;

    if (gBtlAi->flags & 1) {
        if (gBtlAi->data != NULL) {
            Heap_Free(gBtlAi->data);
            gBtlAi->data = NULL;
        }
    }
    for (i=0;i<TagBattle_Count();++i) {
        s=BtlAi_ActorWork(i);
        if (s->flags & 1) {
            if (s->param != NULL) {
                Heap_Free(s->param);
                s->param = NULL;
            }
        }
    }
    if (gBtlAi != NULL) {
        Heap_Free(gBtlAi);
        gBtlAi = NULL;
    }
}

/* Fills the side's 14 per-button rates from its aiType table. */
void BtlAiMgr_BuildRates(BtlAiWork *s) {
    u8 *tbl = gBtlAi->data->profile[s->type];
    s32 i;

    for (i = 0; i < 14; i++) {
        s32 r;

        s->plan.rate[i] = 0;
        r = AiThink_FindBasicColumn(i, 3);
        if (r != -1) {
            s->plan.rate[i] = BtlAi_GetPairRate(s, i, r, (s8 *)tbl + 4, (s8 *)tbl + 0x2C4, 0);
        }
    }
}

/* Returns a side's aiType. */
s32 BtlAiMgr_GetType(s32 side) {
    return ((BtlAiWork *)BtlAi_ActorWork(side))->type;
}

/* Sets a side's aiType, rebuilds what depends on it and clears the side's work and action state. */
void BtlAiMgr_SetType(s32 side, s32 aiType) {
    BtlAiWork *s = BtlAi_ActorWork(side);

    s->type = aiType;
    BtlAiMgr_BuildRates(s);
    AiThink_BuildTotals(s);
    memset(s->plan.scratch, 0, 0x188);
    BtlAiSeq_Reset(&s->seq);
}

/* Returns a side's CPU level. */
s32 BtlAiMgr_GetLevel(s32 side) {
    return ((BtlAiWork *)BtlAi_ActorWork(side))->level;
}

/* Sets a side's CPU level, rebuilds what depends on it and clears the side's work and action state. */
void BtlAiMgr_SetLevel(s32 side, s32 cpuLevel) {
    BtlAiWork *s = BtlAi_ActorWork(side);

    s->level = cpuLevel;
    BtlAiMgr_BuildRates(s);
    AiThink_BuildTotals(s);
    memset(s->plan.scratch, 0, 0x188);
    BtlAiSeq_Reset(&s->seq);
}

/* Returns 1 when the battle sequence is in neither state 2 (Ready) nor 3 (Fight): the AI does not run. */
s32 BtlAiMgr_IsIdleState(void) {
    s32 state = BtlSeq_GetState();

    if (state == 2 || state == 3) {
        return 0;
    }
    return 1;
}

/* Measures the distance between the two fighters' centres and tests whether the stage blocks the line. */
static void TagAi_UpdateSight(s32 actor) {
    BtlAiVec a;
    BtlAiVec b;
    BtlAiVec d;
    BtlAiSegment seg;

    s32 opponent=TagBattle_Target(actor);
    BtlCharApi_GetPos(actor, &a);
    a.y -= BtlCharApi_GetHeight(actor) * 0.5f;
    BtlCharApi_GetPos(opponent, &b);
    b.y -= BtlCharApi_GetHeight(opponent) * 0.5f;
    Vec3_Sub(&d, &a, &b);
    gBtlAi->dist = Vec3_Length(&d);
    if(TagBattle_Active())gBtlAi->radiusSum=BtlCharApi_GetRadius(actor)+BtlCharApi_GetRadius(opponent);
    seg.from = a;
    seg.to = b;
    gBtlAi->sight = 0;
    if (StgCol_TraceSegment(&seg)) {
        gBtlAi->sight |= BTLAI_SIGHT_BLOCKED;
        if (StgCol_GetHit()->id != -1) {
            gBtlAi->sight |= BTLAI_SIGHT_BLOCKED_ID;
        }
    }
}
void BtlAiMgr_UpdateSight(void) { TagAi_UpdateSight(0); }

/* Returns the number of frames the AI has run. */
s32 BtlAiMgr_GetFrame(void) {
    return gBtlAi->frame;
}

/* Reconsider at staggered intervals, without interrupting an attack, stun,
 * grab or cinematic. The decision uses shared simulation state only. */
static void TagAi_SelectTarget(s32 actor){
    extern s32 BtlCharApi_GetHp(s32),BtlAct_GetCurrent(void *),ChrCam_IsCutActive(void *);
    s32 i,j,old=TagBattle_Target(actor),best=old,frame=gBtlAi->frame,action;
    f32 pos[4],other[4],bestScore=1.0e30f;
    if(!TagBattle_Active()||BtlCharApi_GetHp(actor)<=0||TagBattle_Pair(actor)>=0)return;
    if(frame<2)sTagTargetSince[actor]=frame;
    if((frame+actor*17)%90 && BtlCharApi_GetHp(old)>0)return;
    action=BtlAct_GetCurrent(BtlChar_Get(actor));
    /* Neutral, walking and flying states. All attacks and reactions
     * keep their original opponent until their action has completed. */
    if(action<0xB || action>0x1B || ChrCam_IsCutActive(BtlChar_Get(actor)))return;
    BtlCharApi_GetPos(actor,(BtlAiVec *)pos);
    for(i=0;i<TagBattle_Count();++i){
        s32 candidate=(i+actor+frame/180)%TagBattle_Count(),pressure=0;
        f32 dx,dy,dz,score;
        if(TagBattle_Allied(candidate,actor)||BtlCharApi_GetHp(candidate)<=0||!TagBattle_CanInteract(actor,candidate))continue;
        BtlCharApi_GetPos(candidate,(BtlAiVec *)other);
        dx=pos[0]-other[0];dy=pos[1]-other[1];dz=pos[2]-other[2];
        for(j=0;j<TagBattle_Count();++j)if(j!=actor && BtlCharApi_GetHp(j)>0 &&
            TagBattle_Allied(j,actor)&&TagBattle_Target(j)==candidate)++pressure;
        score=dx*dx+dy*dy+dz*dz+pressure*90000.0f;
        if(candidate==old){
            score-=22500.0f; /* hysteresis: do not oscillate between nearby foes */
            if(frame-sTagTargetSince[actor]>240)score+=160000.0f;
        }
        if(score<bestScore){bestScore=score;best=candidate;}
    }
    if(best!=old && TagBattle_SetTarget(actor,best)){
        sTagTargetSince[actor]=frame;
        /* Rebuild the movement plan against the new opponent. */
        BtlAiSeq_Reset(&((BtlAiWork *)BtlAi_ActorWork(actor))->seq);
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_TARGET_TRACE"))fprintf(stderr,"tag target: frame=%d actor=%d old=%d target=%d alive=%d\n",frame,actor,old,best,BtlCharApi_GetHp(best));
    }
}

/* Frame entry: runs both sides' controllers while the fight is live. */
void BtlAiMgr_Update(void) {
    s32 i;

    if (gBtlAi == NULL) {
        return;
    }
    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    if ((Battle_GetWork()->flags & BATTLE_FLAG_LOAD_OBJECT) && !TagBattle_Active()) {
        return;
    }
    if (!BtlStage_IsReady()) {
        return;
    }
    if (BtlAiMgr_IsIdleState()) {
        return;
    }
    BtlAiMgr_UpdateSight();
    gBtlAi->frame++;
    for (i = 0; i < BtlChar_GetCount() && i < TagBattle_Count(); i++) {
        BtlAiWork *s = BtlAi_ActorWork(i);
        if(TagBattle_Active()){
            extern s32 BtlChar_IsFrozen(void *);
            if(BtlChar_IsFrozen(BtlChar_Get(i)))continue;
        }

        if (BtlCharApi_IsInputInjected(i) && s->param != NULL) {
            if(TagBattle_Active()){TagAi_SelectTarget(i);TagAi_UpdateSight(i);}
            BtlAiSense_Update(s);
            AiThink_Think(s);
            BtlAi_RunSeq(s);
            BtlAi_SendInput(s);
        }
        if (TagBattle_Active() && getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_TARGET_TRACE") && gBtlAi->frame%120==0)
            fprintf(stderr,"tag ai frame=%d slot=%d obj=%d param=%p type=%d level=%d depth=%d flags=%llx buttons=%x stick=%.2f,%.2f\n",gBtlAi->frame,i,s->objId,s->param,s->type,s->level,s->seq.depth,(unsigned long long)s->status.flags,s->out.buttons,s->out.stickX,s->out.stickY);
    }
}

/* Picks at random one of the directions the stage limits allow: 0 always; in mode 2 also 1 when a fighter
   is above the upper limit and 2 when one is below its own limit; 3 / 4 for the two sides, dropped when
   the fighter is near the stage edge and that side leads outwards. */
s32 BtlAiMove_PickDir(BtlAiWork *s, s32 mode) {
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec d;
    BtlAiVec out;
    BtlAiVec r;
    struct {
        s32 v[8];
    } cand;
    typeof(cand) *list;
    s32 n = 1;
    f32 top = BtlStage_GetTop() + 20.0f;
    f32 rad = BtlStage_GetInnerRadius() - 100.0f;

    BtlCharApi_GetPos(s->objId, &pos);
    BtlCharApi_GetPos(TagBattle_Target(s->objId), &opp);
    list = &cand;
    list->v[0] = 0;
    if (mode == 2) {
        if (top < pos.y || top < opp.y) {
            cand.v[1] = 1;
            n = 2;
        }
        if (pos.y < BtlCharApi_GetGroundY(s->objId) - 20.0f || opp.y < BtlCharApi_GetGroundY(TagBattle_Target(s->objId)) - 20.0f) {
            list->v[n++] = 2;
        }
    }
    if (rad < Vec3_Length(&pos)) {
        Vec3_Sub(&d, &opp, &pos);
        d.y = 0.0f;
        Vec3_Normalize(&d, &d);
        Vec3_Normalize(&out, &pos);
        if (0.0f < Vec3_Length(&d) && 0.0f < Vec3_Length(&out)) {
            if (0.0f < Vec3_Dot(&out, &d)) {
                goto both;
            }
            Vec3_RotateY(&r, &d, -1.0471975f);
            Vec3_Normalize(&r, &r);
            if (0.0f < Vec3_Dot(&out, &r)) {
                list->v[n++] = 3;
            }
            Vec3_RotateY(&r, &d, 1.0471975f);
            Vec3_Normalize(&r, &r);
            if (0.0f < Vec3_Dot(&out, &r)) {
                list->v[n++] = 4;
            }
        }
    } else {
    both:
        list->v[n++] = 3;
        list->v[n++] = 4;
    }
    return list->v[Rand_Range(n)];
}

/* Computes the move target from the opponent's position and the move type, clamps it to the stage limit,
   and sets the three "blocked" flags from stage line tests. */
void BtlAiMove_CalcTarget(BtlAiWork *s) {
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec a;
    BtlAiVec dir;
    BtlAiVec ahead;
    BtlAiVec tmp;
    BtlAiSegment seg;
    BtlAiSeq *act = &s->seq;
    BtlAiMoveWork *m = &s->move;
    f32 rad = BtlStage_GetRadius();
    s32 special = BtlChar_IsStage4Or27();
    f32 reach;

    BtlCharApi_GetPos(s->objId, &pos);
    a = pos;
    BtlCharApi_GetPos(TagBattle_Target(s->objId), &opp);
    a.y = pos.y;
    Vec3_Sub(&dir, &a, &opp);
    Vec3_Normalize(&dir, &dir);
    Vec4_Scale(&a, &dir, m->dist[m->type]);
    if (special != 0 && (u32)m->type >= 2) {
        Vec4_SetZero(&m->target);
        m->target.y = pos.y;
    } else if (m->type == 4) {
        BtlStage_GetStartPlace(s->objId, &m->target, &tmp, 0);
        m->target.y -= 20.0f;
        m->target.x = (s32)(Rand_Range(400) - 200);
        m->target.z = (s32)(Rand_Range(400) - 200);
    } else {
        Vec3_Add(&m->target, &opp, &a);
    }
    pos.y -= BtlCharApi_GetHeight(s->objId) * 0.5f;
    act->flags &= ~(BTLAI_ACT_BLOCKED | BTLAI_ACT_BLOCKED_ID);
    if (rad < Vec3_Length(&m->target)) {
        Vec4_Scale(&a, &dir, rad - Vec3_Length(&pos));
        Vec3_Add(&m->target, &pos, &a);
    }
    seg.from = pos;
    seg.to = m->target;
    if (StgCol_TraceSegment(&seg)) {
        act->flags |= BTLAI_ACT_BLOCKED;
        if (StgCol_GetHit()->id != -1) {
            act->flags |= BTLAI_ACT_BLOCKED_ID;
        }
    }
    reach = BtlCharApi_GetRadius(s->objId);
    reach += BtlCharApi_GetSpeed(s->objId) * 3.0f;
    BtlCharApi_GetVelocity(s->objId, &dir);
    Vec3_Normalize(&dir, &dir);
    Vec4_Scale(&a, &dir, reach);
    Vec3_Add(&ahead, &pos, &a);
    if (rad < Vec3_Length(&ahead)) {
        Vec4_Scale(&a, &dir, rad - Vec3_Length(&pos));
        Vec3_Add(&ahead, &pos, &a);
    }
    act->flags &= ~BTLAI_ACT_BLOCKED_AHEAD;
    ahead.y = pos.y;
    seg.from = pos;
    seg.to = ahead;
    if (StgCol_TraceSegment(&seg)) {
        act->flags |= BTLAI_ACT_BLOCKED_AHEAD;
    }
}

/* Returns 1 when the fighter is within 20 units, or within five times fighter transform +0x98, of a point.
   The height difference counts only for move type 1 with flat == 0. */
s32 BtlAiMove_IsNear(BtlAiWork *s, BtlAiVec *p, s32 flat) {
    BtlAiVec pos;
    BtlAiVec d;
    BtlAiMoveWork *m = &s->move;
    f32 len;

    BtlCharApi_GetPos(s->objId, &pos);
    Vec3_Sub(&d, &pos, p);
    if (m->type != 1 || flat != 0) {
        d.y = 0.0f;
    }
    len = Vec3_Length(&d);
    if (len < 20.0f) {
        return 1;
    }
    if (len < BtlCharApi_GetSpeed(s->objId) * 5.0f) {
        return 1;
    }
    return 0;
}

/* Builds the path from the fighter to the target. Returns 1 when the path is empty. */
s32 BtlAiMove_BuildPath(BtlAiWork *s) {
    BtlAiVec pos;
    BtlAiVec d;
    BtlAiMovePath *path = &s->move.path;
    BtlAiMoveWork *m = &s->move;

    BtlCharApi_GetPos(s->objId, &pos);
    StgNav_FindPath(&pos, &s->move.target, path);
    m->pathTimer = 60;
    if (path->count > 0) {
        Vec3_Sub(&d, (BtlAiVec *)&path->pts[path->count - 1], &pos);
        m->remain = Vec3_Length(&d);
        return 0;
    }
    m->remain = 0.0f;
    return 1;
}

/* Decides what the run phase does this frame: 0 recompute the target, 1 arrived, 2 head straight for the
   target, 3 follow the path, 4 wait (fighter state 0x1F..0x22). */
s32 BtlAiMove_Check(BtlAiWork *s) {
    BtlAiVec pos;
    BtlAiVec opp;
    BtlAiVec d;
    BtlAiSegment seg;
    s32 skip = 0;
    BtlAiMoveWork *m = &s->move;
    BtlAiSeq *act = &s->seq;
    BtlAiMovePath *path = &s->move.path;
    s32 state = BtlCharApi_GetUnk974(s->objId);

    if (m->pathTimer > 0) {
        m->pathTimer--;
    }
    if (act->flags & BTLAI_ACT_PATH) {
        if (!(act->flags & BTLAI_ACT_BLOCKED)) {
            return 0;
        }
        if (path->count == 0) {
            return 0;
        }
        BtlCharApi_GetPos(s->objId, &pos);
        if (path->count >= 2) {
            seg.from = pos;
            *(BtlAiMovePoint *)&seg.to = path->pts[path->count - 2];
            skip = StgCol_TraceSegment(&seg) == 0;
        }
        if (skip || BtlAiMove_IsNear(s, (BtlAiVec *)&path->pts[path->count - 1], 1)) {
            path->count--;
            m->stuckTimer = 0;
            if (path->count == 0) {
                return 1;
            }
            if ((u32)(state - 0x1F) < 4) {
                return 4;
            }
            Vec3_Sub(&d, (BtlAiVec *)&path->pts[path->count - 1], &pos);
            m->remain = Vec3_Length(&d);
        }
        if (m->pathTimer == 0) {
            BtlCharApi_GetPos(TagBattle_Target(s->objId), &opp);
            Vec3_Sub(&d, &opp, (BtlAiVec *)path);
            if (50.0f < Vec3_Length(&d)) {
                if (BtlAiMove_BuildPath(s)) {
                    return 0;
                }
            }
        }
        return 3;
    }
    if (BtlAiMove_IsNear(s, &m->target, 0)) {
        if (!(gBtlAi->sight & BTLAI_SIGHT_BLOCKED)) {
            return 1;
        }
    }
    if (!(act->flags & BTLAI_ACT_BLOCKED)) {
        return 2;
    }
    if ((act->flags & BTLAI_ACT_BLOCKED_ID) && m->mode == 2) {
        return 2;
    }
    return BtlAiMove_BuildPath(s) ? 0 : 3;
}

/* Writes the virtual pad to move toward a point: stick direction relative to chr->yaw (clamped to -1..1),
   ASCEND / DESCEND from the height difference, DASH held (fighter flags 0x13 and 0xF both clear) or pressed.
   While fighter flag 0x13 is set, CHARGE is added for move types 2..4 and the stick is reduced to its
   dominant axis. Within dist[2] of the point (kind 1, types 0 and 1) the stick is released and UP held. */
void BtlAiMove_Steer(BtlAiWork *s, BtlAiVec *to, s32 kind) {
    BtlAiVec pos;
    BtlAiVec d;
    BtlAiVec opp;
    s32 *volatile flags; /* volatile: the original kept this pointer in a stack slot and stored it between two loads */
    s32 cls;
    s32 hold = 0;
    BtlAiMoveWork *m = &s->move;
    s32 press = 0;
    BtlAiMgrChr *chr = BtlChar_Get(s->objId);
    BtlAiActTable *tbl;
    s32 side;
    BtlAi *mgr;
    s32 fly;
    f32 dy;
    f32 len;
    f32 yaw;
    f32 x;
    f32 y;

    mgr = gBtlAi;
    side = s->objId;
    flags = &s->seq.flags;
    tbl = mgr->data->act;
    cls = tbl->actClass[BtlCharApi_GetUnk974(side)];
    fly = BtlCharApi_TestFlag0F(s->objId);
    BtlCharApi_GetPos(s->objId, &pos);
    BtlCharApi_GetPos(TagBattle_Target(s->objId), &opp);
    dy = to->y - pos.y;
    Vec3_Sub(&d, to, &pos);
    d.y = d.w = 0.0f;
    len = Vec3_Length(&d);
    if (len < BtlCharApi_GetRadius(s->objId)) {
        len = BtlCharApi_GetRadius(s->objId);
    }
    Vec3_Normalize(&d, &d);
    yaw = chr->yaw;
    x = Mathf_Cos(yaw) * d.x - Mathf_Sin(yaw) * d.z;
    y = -(Mathf_Sin(yaw) * d.x + Mathf_Cos(yaw) * d.z);
    if (1.0f < x) {
        x = 1.0f;
    } else if (x < -1.0f) {
        x = -1.0f;
    }
    if (1.0f < y) {
        y = 1.0f;
    } else if (y < -1.0f) {
        y = -1.0f;
    }
    if (BtlCharApi_IsClose(s->objId) || fly != 0) {
        press = BTLAI_BTN_DASH;
    } else {
        hold = BTLAI_BTN_DASH;
    }
    if (kind == 0) {
        if (dy < -30.0f) {
            if (fly == 0 || !(pos.y < opp.y)) {
                hold |= BTLAI_BTN_ASCEND;
            }
        } else if (30.0f < dy) {
            hold |= BTLAI_BTN_DESCEND;
        }
    }
    if (*flags & BTLAI_ACT_BLOCKED_AHEAD) {
        if (!(hold & BTLAI_BTN_DESCEND)) {
            hold |= BTLAI_BTN_ASCEND;
        }
    }
    if (cls == 11) {
        press &= ~BTLAI_BTN_DASH;
    }
    if (BtlCharApi_TestFlag0F(s->objId) && !BtlCharApi_TestFlag0F(TagBattle_Target(s->objId))) {
        if (pos.y > opp.y) {
            hold |= BTLAI_BTN_ASCEND;
        }
    }
    if (len < m->dist[2] && kind == 1 && (u32)m->type < 2) {
        y = 0.0f;
        hold |= BTLAI_BTN_UP;
        x = y;
    }
    if (BtlCharApi_IsClose(s->objId)) {
        if (!(hold & BTLAI_BTN_ASCEND)) {
            if ((u32)m->type >= 2) {
                hold |= BTLAI_BTN_CHARGE;
            }
            if (((x < 0.0f) ? -x : x) > ((y < 0.0f) ? -y : y)) {
                y = 0.0f;
            } else {
                x = 0.0f;
            }
        }
    }
    BtlAiPad_Set(s, hold, press, 0, 0, x, y);
}

/* Virtual pad for move type 0 on a clear line: no stick, DASH held (plus CHARGE in mode 2, ASCEND when
   blocked ahead), stick Y -1 in state class 10. */
void BtlAiMove_Hold(BtlAiWork *s) {
    BtlAiMoveWork *m = &s->move;
    BtlAiSeq *act = &s->seq;
    f32 y = 0.0f;
    f32 x;
    BtlAiActTable *tbl = gBtlAi->data->act;
    s32 hold;
    s32 cls;
    s32 state = BtlCharApi_GetUnk974(s->objId);

    hold = BTLAI_BTN_DASH;
    if (m->mode == 2) {
        hold = BTLAI_BTN_DASH | BTLAI_BTN_CHARGE;
    }
    if (act->flags & BTLAI_ACT_BLOCKED_AHEAD) {
        hold |= BTLAI_BTN_ASCEND;
    }
    cls = tbl->actClass[state];
    x = y;
    if (cls == 10) {
        y = -1.0f;
    }
    BtlAiPad_Set(s, hold, 0, 0, 0, x, y);
}

/* Phase 0 of the move action: goes to phase 1 at once. */
void BtlAiMove_Init(BtlAiWork *s) {
    s->seq.phase = 1;
    BtlAiMove_Start(s);
}

/* Phase 1: reads mode and type from the action entry, picks a direction, computes the target, then runs. */
void BtlAiMove_Start(BtlAiWork *s) {
    BtlAiSeq *act = &s->seq;
    BtlAiMoveWork *m = &s->move;
    BtlAiSeqEntry *e = &act->stack[act->depth - 1];
    s32 v;

    v = BtlAiMove_Div(e->arg, 10);
    BtlAiPad_Clear(&s->out, 0);
    if (v == 1) {
        goto set;
    }
    if (v == 2) {
    set:
        m->mode = v;
    }
    v = BtlAiMove_Mod(e->arg, 10);
    m->type = v;
    m->dir = BtlAiMove_PickDir(s, m->mode);
    m->remain = gBtlAi->dist;
    act->flags &= ~BTLAI_ACT_PATH;
    m->stuckTimer = 0;
    BtlAiMove_CalcTarget(s);
    act->phase = 2;
    BtlAiMove_Run(s);
}

/* Phase 2: acts on BtlAiMove_Check and ends the action when asked to, after 61 frames without progress, or
   (type 4) once the fighters are further apart than dist[1]. */
void BtlAiMove_Run(BtlAiWork *s) {
    BtlAiMoveWork *m = &s->move;
    BtlAiSeq *act = &s->seq;
    BtlAiMovePath *path = &s->move.path;

    switch (BtlAiMove_Check(s)) {
    case 0:
        BtlAiMove_CalcTarget(s);
        act->phase = 1;
        break;
    case 1:
        act->phase = 3;
        break;
    case 2:
        act->flags &= ~BTLAI_ACT_PATH;
        BtlAiPad_Clear(&s->out, 1);
        if (m->type == 0) {
            BtlAiMove_Hold(s);
        } else {
            BtlAiMove_Steer(s, &m->target, 1);
        }
        break;
    case 3:
        act->flags |= BTLAI_ACT_PATH;
        BtlAiMove_Steer(s, (BtlAiVec *)&path->pts[path->count - 1], 0);
        break;
    case 4:
        BtlAiPad_Set(s, 0, 0, BTLAI_BTN_ASCEND, 0, 0.0f, 0.0f);
        break;
    }
    if (m->flags & 1) {
        act->phase = 3;
        m->flags &= ~1;
    }
    if (++m->stuckTimer > 60) {
        act->phase = 3;
    }
    if (m->type == 4) {
        if (gBtlAi->dist < m->dist[1]) {
            act->phase = 3;
        }
    }
}

/* Phase 3: releases the pad (pressing DASH in state class 10, holding ASCEND in states 0x1F..0x22), and
   pops the action once the fighter is in state class 0. */
void BtlAiMove_End(BtlAiWork *s) {
    BtlAiSeq *act = &s->seq;
    BtlAiActTable *tbl = gBtlAi->data->act;
    s32 state = BtlCharApi_GetUnk974(s->objId);
    s32 cls = tbl->actClass[state];

    BtlAiPad_Clear(&s->out, 1);
    if (cls == 10) {
        BtlAiPad_Set(s, 0, BTLAI_BTN_DASH, 0, 0, 0.0f, 0.0f);
    }
    if ((u32)(state - 0x1F) < 4) {
        BtlAiPad_Set(s, BTLAI_BTN_ASCEND, 0, 0, 0, 0.0f, 0.0f);
    }
    if (cls == 0) {
        act->depth--;
    }
}
