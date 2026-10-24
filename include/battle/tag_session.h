#ifndef BATTLE_TAG_SESSION_H
#define BATTLE_TAG_SESSION_H
#include "types.h"
enum { TAG_MODE_TEAM, TAG_MODE_FREE_FOR_ALL, TAG_MODE_TRAINING, TAG_MODE_GAUNTLET, TAG_MODE_TOURNAMENT };
s32 TagSession_Selected(void);
s32 TagSession_Mode(void);
s32 TagSession_Players(void);
s32 TagSession_Size(s32 side);
s32 TagSession_Owner(s32 side, s32 member);
void TagSession_Leave(void);
void TagSession_Open(void);
void TagSession_Return(s32 flags, s32 reason);
s32 TagSession_Run(void);
void TagSession_Apply(void);
void TagSession_Setup(s32 mode,s32 players);
s32 TagSession_DraftMode(void);
s32 TagSession_DraftSize(s32 side);
s32 TagSession_DraftOwner(s32 side,s32 member);
s32 TagSession_MinSize(s32 side);
s32 TagSession_PlayerTeam(s32 player);
void TagSession_SetPlayerTeam(s32 player,s32 team);
s32 TagSession_Time(void);
s32 TagSession_Level(void);
s32 TagSession_Music(void);
s32 TagSession_BattleMusic(void);
s32 TagSession_CpuSetting(s32 side,s32 member);
void TagSession_SetCpuSetting(s32 side,s32 member,s32 level);
s32 TagSession_CpuLevel(s32 side,s32 member);
void TagSession_ChangeRule(s32 rule,s32 direction);
const char *TagSession_RuleLabel(s32 rule);
s32 TagSession_Commit(void);
void TagMenu_Request(s32 page);
#endif
