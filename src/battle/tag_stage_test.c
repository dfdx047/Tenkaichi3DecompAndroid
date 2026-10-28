#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_stage.h"
#include "battle/tag_position.h"
#include "battle/stg_a.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_cam.h"
#include "battle/stg_c.h"
#include "battle/eft_det_a.h"
extern EftDetResult gStgGroundResult;
extern s32 Port_TagStageTest(void),Battle_GetStage(void);
extern void Port_TagStageProbe(s32,s32,u32),Port_TagTestCheck(s32,s32);
extern s32 Port_TagSpawnTest(void);
extern void Port_TagSpawnTrace(s32,s32,u32);
extern BtlMgrChr *BtlChar_Get(s32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern void BtlAct_ClearQueue(BtlMgrChr *),BtlAct_Request(BtlMgrChr *,s32);
extern f32 sinf(f32),cosf(f32);
static f32 firstRadius;
static f32 cameraPhase;
static Vec4 underwaterStart[10];
/* Scatter over durable terrain, not surrounding water or breakable props.
   Islands and the space platform occupy less than the arena cylinder. */
static void scatter_extent(f32 limit,f32 *wide,f32 *narrow) {
    f32 water,minX=limit,maxX=-limit,minZ=limit,maxZ=-limit;
    s32 x,z,hasWater=BtlStage_GetWaterLevel(&water);
    *wide=*narrow=limit;
    for(z=-8;z<=8;z++)for(x=-8;x<=8;x++) {
        Vec4 p={x*limit*.1f,BtlStage_GetTop()+1,z*limit*.1f,1};
        StgGroundHit h;
        if(p.x*p.x+p.z*p.z>limit*limit*.64f)continue;
        BtlStage_ProbeGround(-1,&p,&h);
        if(h.y<=p.y || (hasWater&&h.y>water-2) || h.y>BtlStage_GetBottom() || gStgGroundResult.obj)continue;
        if(p.x<minX)minX=p.x;if(p.x>maxX)maxX=p.x;
        if(p.z<minZ)minZ=p.z;if(p.z>maxZ)maxZ=p.z;
    }
    if(maxX>minX && maxZ>minZ) {
        f32 a=maxX-minX,b=maxZ-minZ;
        *wide=a>b?a:b;*narrow=a<b?a:b;
        if(*wide>limit)*wide=limit;
        if(*narrow>limit)*narrow=limit;
    }
}
void TagStage_TestTick(s32 tick) {
    s32 i;
    if(Port_TagStageTest()==4) {
        extern void TagEffects_TestTick(s32);
        TagEffects_TestTick(tick);return;
    }
    if(Port_TagStageTest()==5) {
        extern void TagBlind_RenderTest(s32);
        TagBlind_RenderTest(tick);return;
    }
    if(tick==1) cameraPhase=0;
    if(Port_TagSpawnTest() && tick==1) {
        extern s32 TagRevive_Enabled(void),TagRevive_Nearby(s32);
        Port_TagTestCheck(!TagRevive_Enabled() && TagRevive_Nearby(0)==-1,697);
    }
    if(Port_TagSpawnTest() && (tick==1||tick==40)) {
        s32 a,b;
        Vec4 spawns[10],rot;
        f32 extent=BtlStage_GetInnerRadius(),minX=extent,maxX=-extent,minZ=extent,maxZ=-extent;
        extern f32 BtlCharApi_GetRadius(s32);
        extern s32 BtlChar_IsStage4Or27(void);
        if(BtlChar_IsStage4Or27() && extent>185)extent=185;
        for(a=0;a<TagTeam_Count();a++) {
            Vec4 actual=((BtlMovePose *)&BtlChar_Get(a)->pose)->pos;
            Port_TagTestCheck(TagPosition_FreeForAllStart(a,&spawns[a],&rot),690);
            if(Battle_GetStage()==2) {
                extern f32 TagSpawn_Clearance(s32,Vec4);
                Port_TagTestCheck(TagSpawn_Clearance(a,spawns[a])>=.64f,698);
            }
            if(!TagPosition_SafeSpot(a,&spawns[a])) {
                s32 k,obj=BattleSide_GetObjId(a);
                extern f32 BtlCharApi_GetHeight(s32);
                f32 r=BtlCharApi_GetRadius(obj),h=BtlCharApi_GetHeight(obj);
                Vec4 p=spawns[a],values={r,h,extent,1};StgGroundHit floor;
                Port_TagSpawnTrace(910,a,(u32)&p);Port_TagSpawnTrace(911,a,(u32)&values);
                p.y-=r*4+10;BtlStage_ProbeGround(-1,&p,&floor);
                values.x=p.y;values.y=floor.y;values.z=BtlStage_GetBottom();
                Port_TagSpawnTrace(912,a,(u32)&values);
                for(k=0;k<8;k++) {
                    Vec4 to=spawns[a],from,hit;
                    to.y-=h*(k<4?.3f:.8f);from=to;
                    if(k&1)from.z+=(k&2)?-r:r;else from.x+=(k&2)?-r:r;
                    if(BtlCam_TraceStage(&hit,&from,&to,NULL,NULL))Port_TagSpawnTrace(920+k,a,(u32)&hit);
                }
                Port_TagTestCheck(0,691);
            }
            {
                f32 water;
                if(BtlStage_GetWaterLevel(&water))
                    Port_TagTestCheck(spawns[a].y<=water-2,695);
                if(tick==1 && TagTeam_InputPlayer(a)>=0 && BtlStage_GetWaterLevel(&water))
                    Port_TagTestCheck(actual.y<=water-2,696);
            }
            /* CPUs already move during the native Ready phase. Human slots
               are untouched at Fight and can verify the placed positions. */
            if(tick==1 && TagTeam_InputPlayer(a)>=0) {
                f32 x=actual.x-spawns[a].x,z=actual.z-spawns[a].z;
                f32 tolerance=2*BtlCharApi_GetRadius(BattleSide_GetObjId(a));
                if(tolerance<8)tolerance=8;
                /* Ready has already simulated a full second of body/ground
                   correction. Allow one body width on slopes, while still
                   rejecting an old team position or a stacked ring corner. */
                if(x*x+z*z>=tolerance*tolerance) {
                    Port_TagSpawnTrace(900,a,(u32)&actual);
                    Port_TagSpawnTrace(901,a,(u32)&spawns[a]);
                }
                Port_TagTestCheck(x*x+z*z<tolerance*tolerance,692);
            }
            for(b=0;b<a;b++) {
                f32 x=spawns[a].x-spawns[b].x,z=spawns[a].z-spawns[b].z;
                f32 r=BtlCharApi_GetRadius(BattleSide_GetObjId(a))+BtlCharApi_GetRadius(BattleSide_GetObjId(b));
                Port_TagTestCheck(x*x+z*z>(r+10)*(r+10),693);
            }
            if(spawns[a].x<minX)minX=spawns[a].x;
            if(spawns[a].x>maxX)maxX=spawns[a].x;
            if(spawns[a].z<minZ)minZ=spawns[a].z;
            if(spawns[a].z>maxZ)maxZ=spawns[a].z;
            Port_TagSpawnTrace(tick,a,(u32)&spawns[a]);
        }
        /* The playable islands need not fill the cylinder equally in X/Z.
           Require wide distribution, not a square-shaped terrain footprint. */
        {
            f32 wide,narrow;
            scatter_extent(extent,&wide,&narrow);
            Port_TagTestCheck((maxX-minX>wide*.5f || maxZ-minZ>wide*.5f) &&
                (TagTeam_Count()<4 || (maxX-minX>narrow*.25f && maxZ-minZ>narrow*.25f)),694);
        }
    }
    if(tick==1 && Battle_GetStage()==0) Port_TagTestCheck(TagStage_PanBlur()==1,603);
    if(tick==1 && Battle_GetStage()==2) Port_TagTestCheck(TagStage_PanBlur()==0,604);
    if(tick==1 || tick==10 || tick==40) {
        f32 v[6];
        Vec4 p={30,0,30,1};
        StgGroundHit hit;
        v[0]=TagStage_Scale();v[1]=BtlStage_GetRadius();
        v[2]=BtlStage_GetTop();v[3]=BtlStage_GetBottom();
        BtlStage_GetWaterLevel(&v[4]);
        p.x*=v[0];p.z*=v[0];
        p.y=v[2]+1;BtlStage_ProbeGround(-1,&p,&hit);v[5]=hit.y;
        Port_TagStageProbe(tick,Battle_GetStage(),(u32)v);
        Port_TagTestCheck(v[0]>=1 && v[0]<=3 && v[1]>0 && v[2]<v[3],600);
        if(tick==1) firstRadius=v[1];
        else Port_TagTestCheck(v[1]==firstRadius,601);
    }
    if(Port_TagStageTest()==1) {
        /* Use the pause/results rematch route. Resetting here, after draw
           lists were built, would leave freed effect models in those lists. */
        if(tick==12) Battle_GetWork()->flags|=BATTLE_FLAG_RESTART;
        if(tick==45) Port_TagTestCheck(1,99);
        return;
    }
    if(tick==10) for(i=0;i<TagTeam_Count();i++) {
        BtlMgrChr *c=BtlChar_Get(i);
        f32 water;
        /* Retain a signed conversion in the soft-float game build. Widening
           a proven nonnegative index uses an unavailable 64-bit helper. */
        volatile s32 signedIndex=i;
        f32 index=signedIndex;
        Vec4 p={index*80.0f-120,0,index*35.0f,1},r={0,0,0,0};
        if(BtlStage_GetWaterLevel(&water)) {
            s32 x,z,found=0;
            /* Pick actual submerged terrain, not the central island where
               the normal placement routine correctly pushes us above ground. */
            for(z=-4;z<=4&&!found;z++) for(x=-4;x<=4&&!found;x++) {
                StgGroundHit h;
                p.x=x*200.0f*TagStage_Scale()+index*25;
                p.z=z*200.0f*TagStage_Scale()+index*25;
                p.y=BtlStage_GetTop()+1;
                BtlStage_ProbeGround(-1,&p,&h);
                if(h.y>water+200 && h.y<BtlStage_GetBottom()) found=1;
            }
            Port_TagTestCheck(found,602);
            p.y=water+160;
        } else p.y=-100;
        BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
        BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
        underwaterStart[i]=p;
    }
    /* Keep the camera underwater long enough to inspect the finished GPU
       frame, including the native water pass and radial blur in bottom views. */
    if(tick>10) {
        for(i=0;i<TagTeam_Count();i++) BtlChar_Get(i)->freezeNext=2;
        cameraPhase+=0.08f;
        if(Port_TagStageTest()==3) for(i=0;i<TagTeam_Count();i++) {
            /* Move the real camera through the normal follow path, so pan
               feedback is exercised as well as a stationary water pass. */
            Vec4 p=underwaterStart[i],r={0,0,0,0};
            f32 t=cameraPhase;
            BtlMgrChr *c=BtlChar_Get(i);
            /* Hit-stop also freezes the fighter's follow-camera solver. */
            c->freeze=c->freezeNext=c->freezeDelay=0;
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            p.x+=96*sinf(t);
            p.z+=96*(cosf(t)-1);
            BtlChar_Place(c,&p,&r,BtlStage_FindZoneNear(-1,&p));
        }
        for(i=0;i<TagTeam_ViewCount();i++) {
            StgBlur_SetColor0Rgba(i,128,128,128,0);
            StgBlur_SetColor1Rgba(i,128,128,128,15);
            StgBlur_SetColor2Rgba(i,128,128,128,30);
        }
    }
}
