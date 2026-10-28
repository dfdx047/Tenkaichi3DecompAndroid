#ifndef BATTLE_TAG_POSITION_H
#define BATTLE_TAG_POSITION_H
#include "common.h"
#include "sys/math3d.h"
void TagPosition_Reset(void);
void TagPosition_Update(void);
void TagPosition_Place(s32 actor,Vec4 *pos,Vec4 *rot,s32 area,s32 kind);
s32 TagPosition_Offset(s32 actor,Vec4 *out);
s32 TagPosition_SafeSpot(s32 actor,Vec4 *pos);
s32 TagPosition_FreeForAllStart(s32 actor,Vec4 *pos,Vec4 *rot);
s32 TagPosition_ReinforcementStart(s32 actor,u32 occupied,Vec4 *pos,Vec4 *rot);
void TagSpawn_Reset(void);
#endif
