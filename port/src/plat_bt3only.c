/* Dragon Rage's platform layer (gs_input.c, snd_adx.c, state.c, headless.c) asks BT3's Tag Team mod and its character
   select a few things; this BT4 build has neither: the answers of a game without them (no Tag Team, no network room). */
int TagNet_Enabled(void) { return 0; }
int TagNet_Active(void) { return 0; }
int TagNet_Role(void) { return -1; }
int TagTeam_Active(void) { return 0; }
void TagTeam_Enable(int enabled) { (void)enabled; }
int TagTeam_ViewActor(int view) { return view; }
void PortCharSel_Abandon(void) {}
/* The Android touch menu (gs/ui_android.inc) names the matchmaking service of a lobby: Dragon Net's (BT3 only). */
const char *Port_LobbyServer(void) { return ""; }
#ifdef __ANDROID__
/* BT4's local Tag battle flag (port/src/gs/tag_team_net.c on the PC, a file of network code the Android build leaves
   out): the battle sets it (battle.c, TagBattle_Active), and while it is on the game runs at 114 % (Port_SpeedMilli)
   and the renderer paces and draws a Tag fight as the PC does. A stub answering 0 made a local Tag fight run as an
   ordinary one. */
static int sTagRenderActive;
void Port_TagBattleRenderSet(int active) { __atomic_store_n(&sTagRenderActive, active != 0, __ATOMIC_RELAXED); }
int Port_TagBattleRenderActive(void) { return __atomic_load_n(&sTagRenderActive, __ATOMIC_RELAXED); }
/* (no network on this build: never a networked Tag room, nothing to catch up with) */
int Port_TagNetwork(void) { return 0; }
int Port_TagCatchup(void) { return 0; }
#endif
