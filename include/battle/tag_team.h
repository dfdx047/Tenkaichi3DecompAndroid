#ifndef BATTLE_TAG_TEAM_H
#define BATTLE_TAG_TEAM_H

#include "types.h"
#include "battle/battle.h"

/* Five native roster slots per team. Contact masks are stored separately from
 * the original packed object flags. Offline has four local views; online has ten logical player cameras. */
#define TAG_TEAM_CAPACITY PORT_FIGHTER_CAPACITY

void TagTeam_Enable(s32 enabled);
s32 TagTeam_Enabled(void);
s32 TagTeam_Active(void);
s32 TagTeam_Count(void);
void TagTeam_Begin(void);
void TagTeam_End(void);
void TagTeam_Reset(void);
void TagTeam_Update(void);
BattleSide *TagTeam_Side(s32 fighter);
s32 TagTeam_Team(s32 fighter);
s32 TagTeam_NativeSide(s32 fighter);
s32 TagTeam_Target(s32 fighter);
s32 TagTeam_IsEnemy(s32 a, s32 b);
s32 TagTeam_CheckEnd(s32 timeUp);
void TagTeam_CycleTarget(s32 fighter);
s32 TagTeam_SetCpuTarget(s32 fighter,s32 target);
void TagTeam_SetupDemo(s32 human, s32 team0, s32 team1);
void TagTeam_Configure(s32 players, s32 views, s32 coop);
void TagTeam_SetAssignment(s32 actor, s32 player);
s32 TagTeam_ViewCount(void);
s32 TagTeam_CameraCount(void);
s32 TagTeam_ViewVisible(s32 view);
s32 TagTeam_VisibleViews(void);
s32 TagTeam_HudMarker(s32 view,s32 shared,s32 bottom);
s32 TagTeam_ViewActor(s32 view);
s32 TagTeam_Player(s32 actor);
s32 TagTeam_InputPlayer(s32 actor);
void TagTeam_RefreshControl(s32 actor);
void TagTeam_ViewRect(s32 view, s32 *rect);
void TagTeam_AfterUpdate(void);
void TagTeam_UpdateMechanics(void);
s32 TagTeam_ControlActor(s32 player);
s32 TagTeam_TakeoverCandidate(s32 player);
s32 TagTeam_Takeover(s32 player);
s32 TagTeam_Spectating(s32 view);
void TagTeam_CycleSpectator(s32 view);
s32 TagTeam_LockedOff(s32 actor);
s32 TagTeam_HudTarget(s32 actor);
s32 TagTeam_WinningActor(s32 team);
void TagTeam_TargetInput(s32 actor, s32 held);
void TagTeam_TargetStick(s32 actor, u32 buttons);
s32 TagTeam_TargetStickConsumed(s32 actor);
s32 TagTeam_BodiesTouch(s32 a, s32 b);
s32 TagTeam_IsAlive(s32 actor);
void TagTeam_GrabContact(s32 attacker,s32 victim);
s32 TagTeam_CameraActor(s32 actor);
s32 TagTeam_ShareClashCamera(s32 actor);
s32 TagTeam_CinematicPaused(s32 actor);
/* A held pair owns its reactions until the native grab/rush releases it. */
s32 TagTeam_CanContact(s32 attacker,s32 victim);

#endif
