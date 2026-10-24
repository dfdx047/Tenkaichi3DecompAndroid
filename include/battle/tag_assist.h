#ifndef BATTLE_TAG_ASSIST_H
#define BATTLE_TAG_ASSIST_H
#include "types.h"
void TagAssist_Reset(void);
void TagAssist_Update(void);
s32 TagAssist_Ally(s32 actor);
s32 TagAssist_ReadyAlly(s32 actor);
s32 TagAssist_Active(s32 actor);
s32 TagAssist_ConsumeR3(s32 player);
s32 TagAssist_Handle(s32 actor,s32 phase);
s32 TagAssist_PendingAction(s32 actor);
s32 TagAssist_Multiplier(s32 actor);
s32 TagAssist_DamageMultiplier(s32 a,s32 b);
#endif
