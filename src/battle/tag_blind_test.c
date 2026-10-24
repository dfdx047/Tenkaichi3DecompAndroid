#include "common.h"
#include "battle/btl_capi_a.h"
#include "battle/eft_aa.h"
extern BtlCapiChr *BtlChar_FindByObjId(s32);
extern EftBlind *gEftBlind;
extern void Port_TagTestCheck(s32,s32);
void TagBlind_TestTick(s32 tick) {
    s32 i,saved[PORT_FIGHTER_CAPACITY];
    if(tick!=74)return;
    for(i=0;i<TagTeam_Count();i++) {
        BtlCapiChr *c=BtlChar_FindByObjId(i);
        saved[i]=c->unkFFC;c->unkFFC=i*9;
    }
    EftBlind_Update();
    for(i=0;i<TagTeam_Count();i++) {
        f32 a=gEftBlind->alpha[i], expected=i*25.5f;
        Port_TagTestCheck(a>expected-.01f&&a<expected+.01f,1110+i);
        BtlChar_FindByObjId(i)->unkFFC=saved[i];
    }
    EftBlind_Update();
}
void TagBlind_RenderTest(s32 tick) {
    s32 i;
    for(i=0;i<TagTeam_Count();i++) {
        BtlCapiChr *c=BtlChar_FindByObjId(i);
        c->unkFFC=(i==TagTeam_ViewActor(2))?90:0;
    }
    EftBlind_Update();
    if(tick==180)Port_TagTestCheck(1,99);
}
