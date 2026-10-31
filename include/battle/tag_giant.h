#ifndef BATTLE_TAG_GIANT_H
#define BATTLE_TAG_GIANT_H
#include "types.h"
/* The data's body class follows transformations and Body Change automatically. */
f32 TagGiant_ObjectScale(void *object);
void TagGiant_UpdateBody(void *object);
f32 TagGiant_AnimationStep(void *object);
s32 TagGiant_Damage(s32 actor,s32 damage);
s32 TagGiant_Armor(s32 actor);
f32 TagGiant_Camera(s32 object);
#endif
