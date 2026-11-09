#ifndef BATTLE_TAG_CPU_H
#define BATTLE_TAG_CPU_H
#include "types.h"
void TagCpu_Reset(void);
void TagCpu_Update(void);
void TagCpu_Damage(s32 victim,s32 attacker,s32 damage);
s32 TagCpu_Attacker(s32 actor);
#endif
