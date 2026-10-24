#include "common.h"
#include "battle/eft_ab.h"
extern void Port_TagTestCheck(s32,s32);
void TagTransformFx_TestTick(s32 tick) {
    s32 i;
    if(tick==120)for(i=0;i<TagTeam_Count();i++) {
        EftTransformArg a={i,0};
        Port_TagTestCheck(EftTransform_Start(&a),1020+i);
        Port_TagTestCheck(gEftTransform->actorTask[i]!=NULL,1030+i);
        Port_TagTestCheck(!EftTransform_Start(&a),1040+i);
    }
    if(tick==122)for(i=0;i<TagTeam_Count();i++) {
        EftTransform *w=gEftTransform->actorTask[i]->work;
        Port_TagTestCheck(!(w->flags&EFT_TF_FLASH),1050+i);
        Port_TagTestCheck(EftTransform_FlashFor(i),1060+i);
    }
    if(tick==124)for(i=0;i<TagTeam_Count();i++) {
        Port_TagTestCheck(EftTransform_EndFor(i),1070+i);
        Port_TagTestCheck(gEftTransform->actorTask[i]==NULL,1080+i);
        if(i+1<TagTeam_Count())Port_TagTestCheck(gEftTransform->actorTask[i+1]!=NULL,1090+i);
    }
    /* A new transformation must survive termination of its fading predecessor. */
    if(tick==125)for(i=0;i<TagTeam_Count();i++) {
        EftTransformArg a={i,0};
        Port_TagTestCheck(EftTransform_Start(&a),1130+i);
    }
    if(tick==140)for(i=0;i<TagTeam_Count();i++) {
        Port_TagTestCheck(gEftTransform->actorTask[i]!=NULL,1140+i);
        Port_TagTestCheck(EftTransform_FlashFor(i),1150+i);
    }
    if(tick==144)for(i=0;i<TagTeam_Count();i++)
        Port_TagTestCheck(EftTransform_EndFor(i),1160+i);
    if(tick==179)Port_TagTestCheck(gEftTransform->sceneFlashes==0,1100);
}
