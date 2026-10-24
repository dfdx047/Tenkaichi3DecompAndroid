#include "common.h"
#include "battle/eft_aa.h"
extern EftCharSlots *gEftCharSlot;
extern EftBladeMgr *gEftBlade;
extern EftBlind *gEftBlind;
extern void Port_TagTestCheck(s32,s32);
extern s32 BtlCharApi_GetChara(s32);
static void *get_slot(s32 kind,s32 actor) {
    switch(kind) {
    case 0:return EftCharSlot_Get0(actor);
    case 1:return EftCharSlot_Get1(actor);
    case 2:return EftCharSlot_Get2(actor);
    case 3:return EftCharSlot_Get3(actor);
    default:return EftCharSlot_GetAbsorb(actor);
    }
}
void TagEffectSlots_TestTick(s32 tick) {
    static s32 marker[5][EFT_FIGHTER_CAPACITY];
    void (*set[5])(s32,void *)={EftCharSlot_Set0,EftCharSlot_Set1,EftCharSlot_Set2,
                               EftCharSlot_Set3,EftCharSlot_SetAbsorb};
    void (*clear[5])(s32)={EftCharSlot_Clear0,EftCharSlot_Clear1,EftCharSlot_Clear2,
                          EftCharSlot_Clear3,EftCharSlot_ClearAbsorb};
    s32 i,k;
    if(tick!=70)return;
    {
        /* Restore real effects before the next update: exercise indexing
           without substituting fake tasks into a running effect. */
        EftCharSlots saved=*gEftCharSlot;
        for(k=0;k<5;k++)for(i=0;i<TagTeam_Count();i++)set[k](i,&marker[k][i]);
        for(k=0;k<5;k++)for(i=0;i<TagTeam_Count();i++)
            Port_TagTestCheck(get_slot(k,i)==&marker[k][i],800+k*10+i);
        for(k=0;k<5;k++) {
            set[k](-1,&marker[0][0]);set[k](EFT_FIGHTER_CAPACITY,&marker[0][0]);
            Port_TagTestCheck(!get_slot(k,-1)&&!get_slot(k,EFT_FIGHTER_CAPACITY),850+k);
            for(i=0;i<TagTeam_Count();i++) {
                clear[k](i);Port_TagTestCheck(!get_slot(k,i),855+k);
            }
        }
        *gEftCharSlot=saved;
    }
    for(i=0;i<TagTeam_Count();i++)
        Port_TagTestCheck(gEftBlade->chara[i]==BtlCharApi_GetChara(i),860+i);
}
