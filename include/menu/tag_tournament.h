#ifndef MENU_TAG_TOURNAMENT_H
#define MENU_TAG_TOURNAMENT_H
#include "types.h"
void TagTournament_Select(s32 direction);
void TagTournament_Choose(s32 index);
void TagTournament_BeginDraft(void);
s32 TagTournament_DraftTeam(void);
s32 TagTournament_Active(void);
s32 TagTournament_CpuMatch(void *bracket);
void TagTournament_SkipMatch(void *bracket);
void TagTournament_DrawBracket(void *bracket);
const char *TagTournament_Title(void);
s32 TagTournament_Start(void);
void TagTournament_BuildTeams(void *bracket);
void TagTournament_SetupBattle(void *bracket);
s32 TagTournament_Pad(s32 slot);
void TagTournament_End(void);
void TagTournament_TestUpdate(void);
#endif
