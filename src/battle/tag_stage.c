#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/tag_stage.h"
#include "battle/tag_position.h"
#include "battle/tag_lobby.h"
extern s32 Port_TagStageScale(void *file,s32 capacity,s32 percent);
extern s32 Port_TagStageRepair(void *file,s32 capacity);
extern s32 Port_TagStagePan(s32 stage,s32 fallback);
extern s32 Battle_GetStage(void),BtlStage_HasFeature(s32);
static s32 stagePercent=100;
static s32 stagePan=-1;
void TagStage_Prepare(void *file,s32 capacity) {
    TagSpawn_Reset();
    stagePan=-1;
    if(PortGame_IsBT4()) Port_TagStageRepair(file,capacity);
    s32 requested=TagLobby_Practice()?200:TagTeam_Active()?Port_TagOption(TAG_OPT_MAP_SIZE):100;
    stagePercent=Port_TagStageScale(file,capacity,requested);
}
f32 TagStage_Scale(void) { return stagePercent/100.0f; }
s32 TagStage_PanBlur(void) {
    if(stagePan<0) stagePan=Port_TagStagePan(Battle_GetStage(),BtlStage_HasFeature(13));
    return stagePan;
}
