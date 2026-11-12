/* Hidden regression instrumentation only. Check the actual character pipeline,
 * rather than just the stop flag, while an unrelated cinematic is running. */
#include <stdio.h>
#include <stdlib.h>
extern void BtlCharApi_GetBasePos(s32,void *);
static f32 sTagCutBefore[10][4];
static s32 sTagCutAction[10],sTagCutTracked[10],sTagCutChecks,sTagCutFrames,sTagCutAdvanced;
static void TagCutAudit_Begin(s32 owner){
    s32 i;static s32 last=-1;
    if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_TAG_DEFAULT_TRACE"))return;
    for(i=0;i<10;++i)sTagCutTracked[i]=0;
    if(owner<0){if(last>=0)fprintf(stderr,"tag cut audit: released=1 frames=%d actor-checks=%d independent-advances=%d\n",sTagCutFrames,sTagCutChecks,sTagCutAdvanced);last=-1;return;}
    ++sTagCutFrames;last=owner;
    for(i=0;i<BtlChar_GetCount();++i)if(i!=owner && TagBattle_PairAttacker(i)<0){
        BtlMgrChr *c=BtlChar_Get(i);sTagCutTracked[i]=1;BtlCharApi_GetBasePos(c->objId,sTagCutBefore[i]);sTagCutAction[i]=c->action;
    }
}
static void TagCutAudit_End(void){
    s32 i;
    if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_TAG_DEFAULT_TRACE"))return;
    for(i=0;i<BtlChar_GetCount();++i)if(sTagCutTracked[i]){
        BtlMgrChr *c=BtlChar_Get(i);f32 p[4];BtlCharApi_GetBasePos(c->objId,p);
        if(memcmp(p,sTagCutBefore[i],12)||c->action!=sTagCutAction[i]){
            ++sTagCutAdvanced;
        }
        ++sTagCutChecks;
    }
}
