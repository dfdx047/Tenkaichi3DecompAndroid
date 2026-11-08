#ifndef BATTLE_TAG_CHALLENGE_H
#define BATTLE_TAG_CHALLENGE_H
#include "types.h"
s32 TagChallenge_FirstCount(void);
s32 TagChallenge_FirstFighter(s32 member,s32 costume);
s32 TagChallenge_Active(void);
s32 TagChallenge_Hidden(s32 actor);
s32 TagChallenge_ModelHidden(s32 actor);
s32 TagChallenge_BetweenWaves(void);
s32 TagChallenge_Timer(void);
void TagChallenge_Select(s32 direction);
s32 TagChallenge_Count(void);
void TagChallenge_Choose(s32 index);
const char *TagChallenge_Title(void);
void TagChallenge_Refill(s32 direction);
const char *TagChallenge_RefillLabel(void);
void TagChallenge_SetupBattle(void);
void TagChallenge_Begin(void);
void TagChallenge_Reset(void);
void TagChallenge_Update(void);
void TagChallenge_End(void);
s32 TagChallenge_Advance(void);
s32 TagChallenge_Wave(void);
void TagChallenge_Damage(s32 actor,s32 amount);
void TagChallenge_Draw(void);
s32 TagChallenge_ResultInput(s32 pad);
s32 TagChallenge_ResultVisible(void);
#endif
