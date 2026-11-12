#ifndef BATTLE_TAG_TEAM_NATIVE_H
#define BATTLE_TAG_TEAM_NATIVE_H
#include "battle/battle.h"
/* Extra actor state is separate from the original two-side PS2 data layout. */
s32 TagBattle_Count(void);
s32 TagBattle_Active(void);
s32 TagBattle_Team(s32 actor);
s32 TagBattle_FreeForAll(void),TagBattle_Allied(s32 a,s32 b);
s32 TagBattle_WinningActor(void);
void TagBattle_SetWinningActor(s32 actor);
s32 TagBattle_TeamIndex(s32 actor);
s32 TagBattle_TeamCount(s32 team);
s32 TagBattle_Target(s32 actor);
void TagBattle_CycleTarget(s32 actor);
s32 TagBattle_TargetDirection(s32 actor,s32 direction);
s32 TagBattle_SetTarget(s32 actor,s32 target);
s32 TagBattle_ConfigureLocal(void);
void TagBattle_ClearDraftItems(void);
void TagBattle_SetDraftItems(s32 actor, s32 chara, const u16 *ids);
BattleItemSet *TagBattle_GetDraftItems(s32 actor, s32 chara);
s32 TagBattle_Local(void);
s32 TagBattle_Network(void);
s32 TagBattle_LocalActor(void);
u8 *TagBattle_NetPad(s32 actor);
BattleSide *TagBattle_ExtraSide(s32 actor);
void TagBattle_Prepare(void);
void TagBattle_End(void);
s32 TagBattle_ReservePair(s32 attacker,s32 victim);
s32 TagBattle_CanInteract(s32 a,s32 b);
void TagBattle_UpdatePairs(void);
void TagBattle_ResetPairs(void);
s32 TagBattle_Pair(s32 actor);
s32 TagBattle_Presentation(void);
s32 TagBattle_DefaultCutOwner(void);
s32 TagBattle_RushAction(s32 actor);
s32 TagBattle_Preparing(s32 actor);
s32 TagBattle_CameraActor(s32 viewer);
s32 TagBattle_PairAttacker(s32 actor);
s32 TagBattle_DamageTarget(s32 actor);
void TagBattle_ClearDamageTarget(s32 actor);
s32 TagBattle_CutVisible(s32 actor,s32 viewer);
s32 TagBattle_IsActorLoading(s32 actor);
void TagBattle_SetArena(s32 stage,s32 bgm);
s32 TagBattle_TakeReturn(void);
void TagBattle_SetReturn(s32 choice);
void TagBattle_ResultBegin(void);
s32 TagBattle_ResultUpdate(void);
void TagBattle_ResultDraw(void);
s32 TagBattle_ViewActor(void);
s32 TagBattle_Spectating(void);
void TagBattle_UpdateSpectator(s32 dir);
#endif
