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
