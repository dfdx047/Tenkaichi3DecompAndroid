/* Free For All has no team line-up. Choose a repeatable, well-spaced set of
   terrain-safe points across the arena, independent of native side indices. */
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_session.h"
#include "battle/tag_lobby.h"
#include "battle/tag_position.h"
#include "battle/stg_a.h"
#include "battle/btl_cam.h"
#include "battle/eft_det_a.h"
extern EftDetResult gStgGroundResult;
extern f32 sinf(f32),cosf(f32);
extern f32 BtlCharApi_GetRadius(s32);
extern f32 BtlCharApi_GetHeight(s32);
extern f32 Stg_CalcYaw(f32,f32,f32,f32);
extern s32 BtlChar_IsStage4Or27(void);
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
extern BtlMgrChr *BtlChar_Get(s32);

static Vec4 placed[TAG_TEAM_CAPACITY];
static s32 ready;
static f32 practiceFacing;
void TagSpawn_Reset(void) {ready=0;}

/* Leave room to move and turn the follow camera, not merely enough room for
   the standing collision cylinder. Score the tightest surrounding wall. */
f32 TagSpawn_Clearance(s32 actor,Vec4 p) {
    s32 k;f32 nearest=1;
    f32 reach=BtlCharApi_GetRadius(BattleSide_GetObjId(actor))*4+180;
    p.y-=BtlCharApi_GetHeight(BattleSide_GetObjId(actor))*.65f;
    for(k=0;k<12;k++) {
        Vec4 end=p,hit;f32 fraction=1;
        end.x+=cosf(k*.523598776f)*reach;
        end.z+=sinf(k*.523598776f)*reach;
        if(BtlCam_TraceStage(&hit,&end,&p,&fraction,NULL)) {
            f32 x=hit.x-p.x,z=hit.z-p.z,d=(x*x+z*z)/(reach*reach);
            if(d<nearest)nearest=d;
        }
    }
    return nearest;
}

/* The lobby's six labelled stations form one straight, level row. Search the
   real expanded terrain rather than using coordinates from the original map:
   South City's buildings and the Lookout's rim must not cut through the row. */
static s32 practice_row(f32 limit) {
    Vec4 trial[TL_DUMMIES];
    f32 best=-1,water=0;int hasWater=BtlStage_GetWaterLevel(&water);
    s32 x,z,heading,k;
    for(z=-4;z<=4;z++)for(x=-4;x<=4;x++)for(heading=0;heading<8;heading++) {
        f32 angle=heading*.392699082f,dx=cosf(angle),dz=sinf(angle);
        f32 cx=x*limit*.12f,cz=z*limit*.12f,score=0;
        s32 valid=1;
        for(k=0;k<TL_DUMMIES;k++) {
            volatile s32 signedIndex=k;
            f32 offset=((f32)signedIndex-2.5f)*100.0f;
            Vec4 p={cx+dx*offset,0,cz+dz*offset,1};StgGroundHit ground;
            p.y=BtlStage_GetTop()+1;BtlStage_ProbeGround(-1,&p,&ground);
            if(!(ground.y>p.y && ground.y<=BtlStage_GetBottom()) || gStgGroundResult.obj ||
               (hasWater && ground.y>water-2)){valid=0;break;}
            p.y=ground.y;
            if(!TagPosition_SafeSpot(TL_MEMBERS-1+k,&p) ||
               (k && __builtin_fabsf(p.y-trial[0].y)>8)){valid=0;break;}
            trial[k]=p;
            score+=TagSpawn_Clearance(TL_MEMBERS-1+k,p);
        }
        if(!valid)continue;
        score+=1-(cx*cx+cz*cz)/(limit*limit);
        if(score>best) {
            best=score;
            for(k=0;k<TL_DUMMIES;k++)placed[TL_MEMBERS-1+k]=trial[k];
            /* All stations face the same open side, toward the arena centre. */
            if(cx*dz-cz*dx<0){dx=-dx;dz=-dz;}
            practiceFacing=Stg_CalcYaw(cx,cz,cx+dz*100,cz-dx*100);
        }
    }
    return best>=0;
}

static s32 build_plan(void) {
    f32 body[TAG_TEAM_CAPACITY],limit=BtlStage_GetInnerRadius();
    f32 water=0;
    s32 hasWater=BtlStage_GetWaterLevel(&water);
    s32 a,b,k,n=TagTeam_Count();u32 occupied=0;
    /* The native tournament Ready phase clamps fighters into this ring.
       Scattering outside it would collapse several starts onto one corner
       and could eliminate fighters as soon as the round begins. */
    if(BtlChar_IsStage4Or27() && limit>185)limit=185;
    if(limit<=0)return 0;
    if(TagLobby_Practice()) {
        if(!practice_row(limit))return 0;
        for(a=TL_MEMBERS-1;a<TL_ACTORS;a++)occupied|=1u<<a;
    }
    for(a=0;a<n;a++) {
        s32 obj=BattleSide_GetObjId(a);
        body[a]=obj>=0?BtlCharApi_GetRadius(obj):15;
        if(body[a]<10) body[a]=10;
    }
    for(a=0;a<n;a++) {
        f32 best=-1;
        s32 grounded=0,clearBody=0;
        if(occupied&(1u<<a))continue;
        /* A golden-angle sample covers the whole disk rather than making
           another ring or two rows. Maximise clearance from earlier spawns.
           No random draws: every actor, intro and rematch gets the same set. */
        for(k=0;k<(TagLobby_Practice()?257:193);k++) {
            volatile s32 signedIndex=k,signedBand=(k*73+a*11)%193;
            f32 index=signedIndex,band=signedBand;
            f32 angle=index*2.39996323f+Battle_GetStage()*.31f;
            /* Small rings need more of their usable area for ten bodies. */
            f32 radius=(limit-body[a]-2)*(.02f+(limit<250?.96f:.76f)*band/192.0f);
            f32 score=limit*limit;
            Vec4 p={0,0,0,1};
            StgGroundHit ground;
            s32 clear=1;
            if(radius<=0)continue;
            p.x=cosf(angle)*radius;p.z=sinf(angle)*radius;
            if(TagLobby_Practice() && k>=193) {
                f32 dx=(placed[14].x-placed[9].x)/500.0f,dz=(placed[14].z-placed[9].z)/500.0f;
                f32 cx=(placed[9].x+placed[14].x)*.5f,cz=(placed[9].z+placed[14].z)*.5f;
                f32 side=(f32)((k-193)%9-4)*65.0f,front=150+(k-193)/9*55.0f;
                /* Start guests facing the stations from their open side.
                   Every candidate still passes the normal terrain/body checks. */
                if(cx*dz-cz*dx<0){dx=-dx;dz=-dz;}
                p.x=cx+dx*side+dz*front;p.z=cz+dz*side-dx*front;
            }
            p.y=BtlStage_GetTop()+1;
            BtlStage_ProbeGround(-1,&p,&ground);
            if(!(ground.y>p.y && ground.y<=BtlStage_GetBottom()))continue;
            /* A destructible prop may collapse during the Ready phase. Start
               on terrain that remains underneath the fighter afterward. */
            if(gStgGroundResult.obj)continue;
            grounded++;
            /* Positive Y points down. A valid terrain hit can be the seabed;
               use dry land for starts, including fighters that cannot fly. */
            if(hasWater && ground.y>water-2)continue;
            p.y=ground.y;
            if(!TagPosition_SafeSpot(a,&p))continue;
            clearBody++;
            for(b=0;b<n;b++) if(occupied&(1u<<b)) {
                f32 x=p.x-placed[b].x,z=p.z-placed[b].z;
                f32 d=x*x+z*z,minimum=body[a]+body[b]+15;
                if(d<minimum*minimum) {clear=0;break;}
                if(d<score)score=d;
            }
            /* Seed away from the centre; subsequent picks spread out around
               it. The centre remains available for larger rosters. */
            if(!a) {
                f32 preferred=radius-limit*.58f;
                score=limit*limit-preferred*preferred;
            }
            /* A good open position beats an isolated point against a wall.
               Narrow stages still get the clearest available valid plan. */
            {
                f32 room=TagSpawn_Clearance(a,p);
                /* Once there is comfortable turning room, spread out rather
                   than packing everybody into the widest flat clearing. */
                score=(score/(limit*limit))+(room>=.64f?2:room*.75f);
                if(TagLobby_Practice() && k>=193) {
                    f32 cx=(placed[9].x+placed[14].x)*.5f,cz=(placed[9].z+placed[14].z)*.5f;
                    f32 x=p.x-cx,z=p.z-cz;
                    score+=5-(x*x+z*z)/1000000.0f;
                }
            }
            if(clear && score>best) {placed[a]=p;best=score;}
        }
        /* Existing start placement is the fallback for unsupported geometry;
           never return a partly filled array or an unchecked point. */
        if(best<0) {
            extern s32 Port_TagSpawnTest(void);
            extern void Port_TagSpawnFailure(s32,s32,s32,s32);
            if(Port_TagSpawnTest())Port_TagSpawnFailure(Battle_GetStage(),a,grounded,clearBody);
            return 0;
        }
        occupied|=1u<<a;
    }
    return 1;
}

s32 TagPosition_FreeForAllStart(s32 actor,Vec4 *pos,Vec4 *rot) {
    s32 a,n=TagTeam_Count();
    if(!TagTeam_Active() || (TagSession_Mode()!=TAG_MODE_FREE_FOR_ALL && !TagLobby_Practice()) ||
       (u32)actor>=(u32)n) return 0;
    /* Native camera collision deliberately skips traces during loading.
       Do not cache such unchecked points for the playable round. */
    if(!BtlStage_IsReady() || (Battle_GetWork()->flags&BATTLE_FLAG_LOADING))return 0;
    /* Stage collision probes are needed once per round, not once per actor
       or each time a CPU asks for its starting location. */
    if(!ready) ready=build_plan()?1:-1;
    if(ready<0)return 0;
    *pos=placed[actor];
    if(TagLobby_Practice() && actor>=TL_MEMBERS-1) {rot->y=practiceFacing;return 1;}
    if(TagLobby_Practice()) {rot->y=Stg_CalcYaw(pos->x,pos->z,(placed[9].x+placed[14].x)*.5f,(placed[9].z+placed[14].z)*.5f);return 1;}
    {
        s32 nearest=-1;
        f32 best=1.0e20f;
        for(a=0;a<n;a++) if(a!=actor) {
            f32 x=pos->x-placed[a].x,z=pos->z-placed[a].z,d=x*x+z*z;
            if(d<best) {nearest=a;best=d;}
        }
        if(nearest>=0)rot->y=Stg_CalcYaw(pos->x,pos->z,placed[nearest].x,placed[nearest].z);
    }
    return 1;
}

/* A later wave enters around the survivors' current positions. Reuse native
   terrain probes, but never move the surviving team or spawn inside it. */
s32 TagPosition_ReinforcementStart(s32 actor,u32 occupied,Vec4 *pos,Vec4 *rot) {
    s32 k,b,found=0,nearest=-1,hasWater;f32 best=-1,water=0,closest=1e20f;
    f32 body=BtlCharApi_GetRadius(BattleSide_GetObjId(actor));
    f32 limit=BtlStage_GetInnerRadius();
    if(BtlChar_IsStage4Or27() && limit>185)limit=185;
    hasWater=BtlStage_GetWaterLevel(&water);
    for(k=0;k<193;k++) {
        f32 angle=k*2.39996323f+actor*.7f;
        f32 radius=(limit-body-2)*(.04f+.72f*((k*73)%193)/192.0f);
        Vec4 p={0,0,0,1};StgGroundHit ground;f32 score=limit*limit;s32 clear=1;
        if(radius<=0)continue;
        p.x=cosf(angle)*radius;p.z=sinf(angle)*radius;p.y=BtlStage_GetTop()+1;
        BtlStage_ProbeGround(-1,&p,&ground);
        if(!(ground.y>p.y && ground.y<=BtlStage_GetBottom()) || gStgGroundResult.obj ||
           (hasWater && ground.y>water-2))continue;
        p.y=ground.y;if(!TagPosition_SafeSpot(actor,&p))continue;
        for(b=0;b<TagTeam_Count();b++)if(b!=actor && (occupied&(1u<<b))) {
            BtlMovePose *q=(BtlMovePose *)&BtlChar_Get(b)->pose;
            f32 x=p.x-q->pos.x,z=p.z-q->pos.z,d=x*x+z*z;
            f32 gap=body+BtlCharApi_GetRadius(BattleSide_GetObjId(b))+45;
            if(d<gap*gap){clear=0;break;}
            if(d<score)score=d;
        }
        if(clear) {
            score=score/(limit*limit)+TagSpawn_Clearance(actor,p)*2;
            if(score>best){best=score;*pos=p;found=1;}
        }
    }
    if(!found)return 0;
    for(b=0;b<TagTeam_Count();b++)if((occupied&(1u<<b)) && TagTeam_IsEnemy(actor,b)) {
        BtlMovePose *q=(BtlMovePose *)&BtlChar_Get(b)->pose;
        f32 x=pos->x-q->pos.x,z=pos->z-q->pos.z,d=x*x+z*z;
        if(d<closest){closest=d;nearest=b;}
    }
    if(nearest>=0){BtlMovePose *q=(BtlMovePose *)&BtlChar_Get(nearest)->pose;
        rot->y=Stg_CalcYaw(pos->x,pos->z,q->pos.x,q->pos.z);}
    return 1;
}
