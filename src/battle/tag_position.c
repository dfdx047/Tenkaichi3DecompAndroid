/* Optional local staging for the native authored placement requests. A rush's
   actual participants share an offset; ordinary movement, saved poses, intros
   and stage transitions continue through their original placement functions. */
#include "common.h"
#include "battle/tag_position.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/btl_char_coll.h"
#include "battle/btl_seq.h"
#include "battle/stg_a.h"
#include "battle/btl_cam.h"
#include "battle/battle_setup.h"
extern void *memset(void *,s32,u32);
extern f32 sqrtf(f32);
extern BtlMgrChr *BtlChar_Get(s32);
extern void BtlChar_Place(BtlMgrChr *,Vec4 *,Vec4 *,s32);
extern f32 BtlCharApi_GetRadius(s32),BtlCharApi_GetHeight(s32);
extern s32 BtlCharApi_IsInRushSequence(s32);

typedef struct Position {
    s32 active,owner,kind,quiet,placed;
    Vec4 home,homeRot,offset,staged;
} Position;
static Position rows[PORT_FIGHTER_CAPACITY];
static s32 stage=-1;

static s32 authored(s32 i) {
    s32 a=BtlChar_Get(i)->action;
    return (a>=236&&a<=243) || (a>=253&&a<=315);
}
static s32 valid(Vec4 p) {
    /* Also excludes NaNs without depending on host floating point libraries. */
    return p.x>-100000 && p.x<100000 && p.y>-100000 && p.y<100000 &&
           p.z>-100000 && p.z<100000;
}
static s32 safe(s32 actor,Vec4 *p,s32 clamp) {
    s32 n;Vec4 probe,hit,from,to;StgGroundHit floor;
    /* Stage placements can run while fighter structs are being reset in
       order. The loader's object map is already complete at that point. */
    s32 obj=BattleSide_GetObjId(actor);
    f32 radius=BtlCharApi_GetRadius(obj);
    f32 height=BtlCharApi_GetHeight(obj);
    f32 limit=BtlStage_GetInnerRadius()-radius-2,dist;
    f32 top=BtlStage_GetTop()+height+2,bottom=BtlStage_GetBottom();
    if(!valid(*p)||limit<=0||top>=bottom)return 0;
    dist=sqrtf(p->x*p->x+p->z*p->z);
    if(dist>limit) {
        if(!clamp)return 0;
        p->x*=limit/dist;p->z*=limit/dist;
    }
    if(p->y<top) {if(!clamp)return 0;p->y=top;}
    if(p->y>bottom)p->y=bottom;
    /* Start just above the fighter's feet, not at the top of the stage: the
       underside of an arch must not move a fighter onto its roof. */
    probe=*p;probe.y-=radius*4+10;
    BtlStage_ProbeGround(-1,&probe,&floor);
    if(!(floor.y>probe.y && floor.y<=bottom))return 0;
    if(p->y>floor.y)p->y=floor.y;
    /* A footpoint alone can be valid while the torso is inside a wall. Check
       the body's horizontal footprint at two heights using native collision. */
    for(n=0;n<8;n++) {
        from=to=*p;from.y=to.y=p->y-height*(n<4?.3f:.8f);
        if(n&1)from.z+=(n&2)?-radius:radius;
        else from.x+=(n&2)?-radius:radius;
        if(BtlCam_TraceStage(&hit,&from,&to,NULL,NULL))return 0;
    }
    return 1;
}
s32 TagPosition_SafeSpot(s32 actor,Vec4 *pos) {
    return safe(actor,pos,0);
}
static s32 owner(s32 actor) {
    BtlCollThrow *a=&((BtlCollChr *)BtlChar_Get(actor))->thr,*b;
    s32 at=a->atkSide,de=a->defSide;
    if((u32)at>=(u32)TagTeam_Count() || (u32)de>=(u32)TagTeam_Count() ||
       at==de || (actor!=at&&actor!=de) || !TagTeam_IsEnemy(at,de))return actor;
    b=&((BtlCollChr *)BtlChar_Get(actor==at?de:at))->thr;
    if(b->atkSide!=at||b->defSide!=de ||
       !BtlCharApi_IsInRushSequence(BtlChar_Get(at)->objId) ||
       !BtlCharApi_IsInRushSequence(BtlChar_Get(de)->objId))return actor;
    return at;
}
static void seed(s32 i,s32 root) {
    Position *p=&rows[i];BtlMovePose *pose=(BtlMovePose *)&BtlChar_Get(i)->pose;
    if(p->active)return;
    memset(p,0,sizeof(*p));p->active=1;p->owner=root;
    p->home=pose->pos;p->homeRot=pose->rot;
}
void TagPosition_Reset(void) {memset(rows,0,sizeof(rows));stage=-1;}
static void stage_check(void) {
    s32 current=Battle_GetStage();
    if(current!=stage) {TagPosition_Reset();stage=current;}
}
s32 TagPosition_Offset(s32 actor,Vec4 *out) {
    Position *p;
    if(!TagTeam_Active() || (u32)actor>=(u32)TagTeam_Count() || !rows[actor].active)return 0;
    p=&rows[rows[actor].owner];
    if(!p->kind)return 0;
    *out=p->offset;return 1;
}
void TagPosition_Place(s32 actor,Vec4 *pos,Vec4 *rot,s32 area,s32 kind) {
    BtlMgrChr *c=BtlChar_Get(actor);
    Position *p,*root;Vec4 candidate,oldOffset,shift;
    s32 r,i;
    if(!TagTeam_Active() || !Port_TagOption(TAG_OPT_LOCAL_CINEMATICS) ||
       !BtlSeq_IsFighting() || !BtlStage_IsReady() || !authored(actor) ||
       (Battle_GetWork()->flags&BATTLE_FLAG_LOADING) || !valid(*pos)) {
        BtlChar_Place(c,pos,rot,area);return;
    }
    stage_check();
    r=owner(actor);seed(r,r);seed(actor,r);p=&rows[actor];p->owner=r;root=&rows[r];
    oldOffset=root->offset;
    if(root->kind!=kind) {
        /* Each authored staging kind starts from the pre-move root, even when
           the lower-index victim's update runs before the attacker's setter. */
        root->offset.x=root->home.x-pos->x;
        root->offset.y=root->home.y-pos->y;
        root->offset.z=root->home.z-pos->z;root->offset.w=0;
        root->kind=kind;
    }
    candidate=*pos;
    candidate.x+=root->offset.x;candidate.y+=root->offset.y;candidate.z+=root->offset.z;
    if(!safe(actor,&candidate,1)) {
        /* Try the original safe pre-move location before falling back to the
           game's authored position. Never keep an invalid translated pose. */
        candidate=p->home;
        if(!safe(actor,&candidate,1))candidate=*pos;
    }
    root->offset.x=candidate.x-pos->x;root->offset.y=candidate.y-pos->y;root->offset.z=candidate.z-pos->z;
    for(i=0;i<TagTeam_Count();i++) if(i!=actor&&rows[i].active&&rows[i].owner==r&&rows[i].placed) {
        Vec4 q=rows[i].staged,check;
        q.x+=root->offset.x;q.y+=root->offset.y;q.z+=root->offset.z;check=q;
        if(!safe(i,&check,0) || check.y<q.y-.01f) {
            /* No common clear local staging space. Keep the authored layout
               for the whole pair instead of separating its participants. */
            root->offset.x=root->offset.y=root->offset.z=0;candidate=*pos;break;
        }
    }
    shift.x=root->offset.x-oldOffset.x;shift.y=root->offset.y-oldOffset.y;shift.z=root->offset.z-oldOffset.z;
    if(shift.x*shift.x+shift.y*shift.y+shift.z*shift.z>.0001f) {
        /* Move already placed partners by the same correction, keeping the
           native paired animations together instead of stretching a grab. */
        for(i=0;i<TagTeam_Count();i++) if(i!=actor&&rows[i].active&&rows[i].owner==r&&rows[i].placed) {
            BtlMovePose *q=(BtlMovePose *)&BtlChar_Get(i)->pose;Vec4 v=q->pos;
            v.x+=shift.x;v.y+=shift.y;v.z+=shift.z;
            BtlChar_Place(BtlChar_Get(i),&v,&q->rot,BtlStage_FindZoneNear(-1,&v));
        }
    }
    p->placed=1;p->quiet=0;p->staged=*pos;
    BtlChar_Place(c,&candidate,rot,BtlStage_FindZoneNear(-1,&candidate));
}
void TagPosition_Update(void) {
    s32 i,j;
    if(!TagTeam_Active() || !BtlStage_IsReady() || !BtlSeq_IsFighting() ||
       (Battle_GetWork()->flags&BATTLE_FLAG_LOADING)) {TagPosition_Reset();return;}
    stage_check();
    for(i=0;i<TagTeam_Count();i++) if(rows[i].active && rows[i].owner==i) {
        s32 running=0;
        for(j=0;j<TagTeam_Count();j++) if(rows[j].active&&rows[j].owner==i&&authored(j))running=1;
        if(running){rows[i].quiet=0;continue;}
        if(++rows[i].quiet<2)continue;
        for(j=0;j<TagTeam_Count();j++) if(rows[j].active&&rows[j].owner==i) {
            BtlMgrChr *c=BtlChar_Get(j);BtlMovePose *q=(BtlMovePose *)&c->pose;
            Vec4 pos=q->pos,rot=q->rot;
            if(!safe(j,&pos,1)) {
                pos=rows[j].home;rot=rows[j].homeRot;
                if(!safe(j,&pos,1)) {
                    BtlStage_GetStartPlace(j,&pos,&rot,0);
                    if(!safe(j,&pos,1)){rows[j].active=0;continue;}
                }
            }
            if(!valid(q->pos) || (pos.x-q->pos.x)*(pos.x-q->pos.x)+(pos.y-q->pos.y)*(pos.y-q->pos.y)+
               (pos.z-q->pos.z)*(pos.z-q->pos.z)>.0001f)
                BtlChar_Place(c,&pos,&rot,BtlStage_FindZoneNear(-1,&pos));
            rows[j].active=0;
        }
    }
}
