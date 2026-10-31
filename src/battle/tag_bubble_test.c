#include "common.h"
#include "battle/eft_b.h"
extern void Port_TagTestCheck(s32,s32);
extern s32 EftBubble_StartBurst(s32);
extern void EftBubble_UpdateChrs(void);
extern void EftBubble_Init(EftBTask *), EftBubble_Term(void);
void TagBubble_TestTick(s32 tick) {
    s32 i,owned;
    EftBTask task={0};
    if(tick!=73)return;
    /* Some stages have no bubble texture set, so the scene removes this
       manager. Still exercise its real allocation and actor-state lifecycle. */
    owned=gEftBubble==NULL;
    if(owned)EftBubble_Init(&task);
    Port_TagTestCheck(gEftBubble!=NULL,940);
    for(i=0;i<TagTeam_Count();i++) {
        gEftBubble->chr[i].flags=0;
        Port_TagTestCheck(EftBubble_StartBurst(i),941+i);
        Port_TagTestCheck(gEftBubble->chr[i].objId==i,951+i);
    }
    Port_TagTestCheck(!EftBubble_StartBurst(-1)&&!EftBubble_StartBurst(PORT_FIGHTER_CAPACITY),962);
    for(i=0;i<10;i++)EftBubble_UpdateChrs();
    for(i=0;i<TagTeam_Count();i++)Port_TagTestCheck(!EFT_BUBBLE_CHR_FLAGS(&gEftBubble->chr[i])->bursting,963+i);
    if(owned)EftBubble_Term();
}
