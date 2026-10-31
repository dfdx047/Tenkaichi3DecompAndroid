#ifndef BATTLE_TAG_FUSION_H
#define BATTLE_TAG_FUSION_H
#include "types.h"
void TagFusion_Reset(void);
void TagFusion_Remove(s32 actor);
void TagFusion_Update(void);
s32 TagFusion_Find(s32 actor,s32 index);
s32 TagFusion_Prepare(s32 actor,s32 index,s32 partner);
void TagFusion_ModelLoaded(s32 actor);
s32 TagFusion_Hidden(s32 actor);
s32 TagFusion_Owner(s32 actor);
s32 TagFusion_SharedBody(s32 actor);
s32 TagFusion_Seconds(s32 actor);
s32 TagFusion_Duration(s32 actor);
s32 TagFusion_Controller(s32 actor);
s32 TagFusion_NextController(s32 actor);
s32 TagFusion_SwitchSeconds(s32 actor);
s32 TagFusion_Requester(s32 actor);
#endif
