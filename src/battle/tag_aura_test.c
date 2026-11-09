#include "common.h"
#include "battle/eft_n.h"
struct EftNTask { u8 prefix[0x38]; void *work; };
extern void Port_TagTestCheck(s32,s32);
extern void EftAura_Hide(s32),EftAura_Show(s32),EftAura_MarkModelNew(s32);
extern s32 EftAura_ChangeType(s32,s32,s32);
void TagAura_TestTick(s32 tick) {
    s32 i;
    if(tick!=110)return;
    for(i=0;i<TagTeam_Count();i++) {
        EftAuraWork *a=((EftAuraMgr *)gEftAuraPool)->tasks[i]->work;
        EftAura_Hide(i);Port_TagTestCheck(a->flags&4,980+i);
        EftAura_Show(i);Port_TagTestCheck(!(a->flags&4),990+i);
        EftAura_MarkModelNew(i);Port_TagTestCheck(a->flags&0x40,1000+i);
    }
    EftAura_Hide(-1);EftAura_Show(PORT_FIGHTER_CAPACITY);
    Port_TagTestCheck(!EftAura_ChangeType(-1,0,0),1010);
}
