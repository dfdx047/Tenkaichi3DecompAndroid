#include "common.h"
#include "battle/eft_z.h"
extern void Port_TagTestCheck(s32,s32);
void TagDust_TestTick(s32 tick) {
    s32 i,j;
    if(tick!=71)return;
    for(i=0;i<TagTeam_Count();i++) {
        EftGndDust_SetSlide(i,0,1);EftGndDust_SetDash(i,0,1);
        Port_TagTestCheck(gEftGndDust->slide[i] && gEftGndDust->dash[i],880+i);
    }
    for(i=0;i<TagTeam_Count();i++) {
        for(j=0;j<TagTeam_Count();j++) {
            Port_TagTestCheck(gEftGndDust->slide[i]!=gEftGndDust->dash[j],890);
            if(i!=j)Port_TagTestCheck(gEftGndDust->slide[i]!=gEftGndDust->slide[j] &&
                                     gEftGndDust->dash[i]!=gEftGndDust->dash[j],891);
        }
        EftGndDust_SetSlide(i,1,1);EftGndDust_SetDash(i,1,1);
    }
    EftGndDust_SetSlide(-1,0,1);EftGndDust_SetDash(EFT_FIGHTER_CAPACITY,0,1);
}
