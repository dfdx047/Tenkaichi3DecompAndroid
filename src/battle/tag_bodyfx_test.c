#include "common.h"
#include "battle/tag_team.h"
#include "battle/eft_o.h"
extern void Port_TagTestCheck(s32,s32);
void TagBodyFx_TestTick(s32 tick) {
    s32 i,j;
    if(tick==75)for(i=0;i<TagTeam_Count();i++) {
        EftBodyFxArg a={BattleSide_GetObjId(i),0,1};
        Port_TagTestCheck(EftBodyFx_Start(a),750+i);
        Port_TagTestCheck(gEftBodyFx->task[a.objId]!=NULL,760+i);
        for(j=0;j<i;j++)
            Port_TagTestCheck(gEftBodyFx->task[a.objId]!=gEftBodyFx->task[BattleSide_GetObjId(j)],770+i);
    }
    if(tick==76)for(i=0;i<TagTeam_Count();i++)
        Port_TagTestCheck(EftBodyFx_Stop(BattleSide_GetObjId(i)),780+i);
    if(tick==180)for(i=0;i<TagTeam_Count();i++)
        Port_TagTestCheck(gEftBodyFx->task[BattleSide_GetObjId(i)]==NULL,790+i);
}
