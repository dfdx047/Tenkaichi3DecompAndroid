#ifndef BATTLE_TAG_BALANCE_H
#define BATTLE_TAG_BALANCE_H
#include "types.h"
void TagBalance_Reset(void);
void TagBalance_Update(void);
s32 TagBalance_Modify(s32 victim,s32 source,s32 damage,s32 flags);
s32 TagBalance_Damage(void *victim,s32 damage,s32 flags,s32 source);
f32 TagBalance_Recovery(s32 actor);
s32 TagBalance_AgeStep(s32 actor);
#endif
