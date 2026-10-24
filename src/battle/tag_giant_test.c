/* Real loaded giant models: geometry, query volumes and root-motion roundtrips.
   No fixture resources or alternate fighter model are used. */
#include "common.h"
#include "battle/tag_giant.h"
#include "battle/tag_options.h"
#include "battle/tag_team.h"
#include "battle/btl_char_mgr.h"
#include "battle/btl_char_move.h"
#include "battle/bobj_a.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BobjObj *BtlChar_GetObj(BtlMgrChr *);
extern void BtlChar_PoseToObj(BtlMgrChr *,s32),BtlChar_ObjToPose(BtlMgrChr *);
extern void BtlAct_Request(BtlMgrChr *,s32),BtlAct_ClearQueue(BtlMgrChr *);
extern void Port_TagTestCheck(s32,s32);
extern void Port_TagTestPositions(u32,u32);
extern void BtlObjPose_CalcMatrices(BobjObj *);
static s32 near(f32 a,f32 b) {f32 d=a-b;return d<0.01f && d>-0.01f;}

void TagGiant_TestTick(s32 tick) {
    static f32 radius;
    static s32 hitSeen;
    BtlMgrChr *c=BtlChar_Get(0);
    BobjObj *obj=BtlChar_GetObj(c);
    s32 i,j;
    if(tick==30) {
        Port_TagTestCheck(((u8 *)obj->mdl.param)[2]==4,300);
        Port_TagTestCheck(TagGiant_ObjectScale(obj)==1,301);
        radius=obj->body.radius;
        Port_TagOptionSet(TAG_OPT_GIANTS,1);
        Port_TagOptionSet(TAG_OPT_GIANT_SIZE,200);
    }
    if(tick==32) {
        BtlMovePose *p=(BtlMovePose *)&c->pose;
        Vec4 before=p->pos;
        BobjBodyPart *part;
        Port_TagTestCheck(near(obj->body.radius,radius*2) &&
            near(obj->body.size[0],obj->mdl.model->size[0]*2),302);
        Port_TagTestCheck(TagGiant_ObjectScale(BtlChar_GetObj(BtlChar_Get(1)))==1,303);
        for(i=0;i<100;i++) {
            BtlChar_PoseToObj(c,0);BtlObjPose_CalcMatrices(obj);BtlChar_ObjToPose(c);
        }
        Port_TagTestPositions((u32)&before,(u32)&p->pos);
        Port_TagTestCheck(near(before.x,p->pos.x) && near(before.y,p->pos.y) && near(before.z,p->pos.z),304);
        BtlObjBody_Update(obj,1);
        for(part=obj->body.parts;;part++) {
            for(i=0;i<3;i++) {
                Vec4 a=part->box.axis[i];
                Port_TagTestCheck(near(a.x*a.x+a.y*a.y+a.z*a.z,1),305);
            }
            Port_TagTestCheck(near(part->box.unk10.x,part->box.local[0].x*2),306);
            if(part->flags&1) break;
        }
        Port_TagTestCheck(TagGiant_Damage(0,1000)==1500 && TagGiant_Damage(1,1000)==1000,307);
        Port_TagTestCheck(TagGiant_Armor(0)==1 && near(TagGiant_Camera(c->objId),1.5f),308);
        BtlAct_ClearQueue(c);BtlAct_Request(c,0x44);
    }
    if(tick>32 && tick<150) {
        BobjWork *w=obj->work;
        if(w->atkSphereCount || w->atkObbCount) {
            hitSeen=1;
            for(i=0;i<w->atkSphereCount;i++)
                Port_TagTestCheck(near(w->atkSphere[i].radius,obj->hit.r0*2),309);
            for(i=0;i<w->atkObbCount;i++) for(j=0;j<3;j++) {
                Vec4 a=w->atkObb[i].axis[j];
                Port_TagTestCheck(near(a.x*a.x+a.y*a.y+a.z*a.z,1),310);
            }
        }
    }
    if(tick==150) {
        Port_TagTestCheck(hitSeen,311);
        Port_TagOptionSet(TAG_OPT_GIANTS,0);
    }
    if(tick==153) {
        Port_TagTestCheck(near(obj->body.radius,radius) &&
            near(obj->body.size[0],obj->mdl.model->size[0]),312);
        Port_TagTestCheck(TagGiant_Damage(0,1000)==1000 && TagGiant_Armor(0)==0 &&
            TagGiant_Camera(c->objId)==1,313);
        Port_TagTestCheck(1,99);
    }
}
