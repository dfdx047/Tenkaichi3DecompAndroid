#include "common.h"
#include "battle/eft_e.h"
extern void Port_TagTestCheck(s32,s32);
void TagWater_TestTick(s32 tick) {
    s32 i;
    if(tick!=72)return;
    for(i=0;i<TagTeam_Count();i++) {
        Port_TagTestCheck(EftWater_SetWake(i,0),900+i);
        Port_TagTestCheck(gEftDust->wake[i].on && gEftDust->wake[i].objId==i,910+i);
    }
    Port_TagTestCheck(!EftWater_SetWake(-1,0)&&!EftWater_SetWake(EFT_FIGHTER_CAPACITY,0),920);
    EftWater_UpdateWakes();
    EftWater_Reset();
    for(i=0;i<TagTeam_Count();i++)Port_TagTestCheck(!gEftDust->wake[i].on,921+i);
}
