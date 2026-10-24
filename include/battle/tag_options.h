#ifndef BATTLE_TAG_OPTIONS_H
#define BATTLE_TAG_OPTIONS_H
/* One catalogue for the native menu, F1 window, persistence and range checks.
   Keep existing IDs stable when adding options. */
#define TAG_OPTION_GROUPS(X) \
 X("Revival") X("Combat") X("Cinematics") X("Targeting and Views") X("HUD") X("Controllers") X("Training") X("Fusion") X("Outnumbered") X("Giants") X("Stages")
#define TAG_OPTION_LIST(X) \
 X(REVIVE,"tag_revive","Revive Teammates",0,0,1,1,1,"Stay near a fallen teammate. Movement and blocking are allowed.") \
 X(REVIVE_SECONDS,"tag_revive_seconds","Revival Time",0,1,15,1,3,"Seconds spent inside the revival ring.") \
 X(REVIVE_STOCKS,"tag_revive_stocks","Revival Cost",0,0,5,1,2,"Blast stocks paid by the rescuer.") \
 X(REVIVE_RADIUS,"tag_revive_radius","Revival Range",0,15,120,5,45,"Distance from the fallen teammate, in game units.") \
 X(TAKEOVER,"tag_takeover","Teammate Takeover",1,0,1,1,1,"After a KO, press Square to control a living CPU teammate.") \
 X(RING_OUT,"tag_ring_out","Ring Outs",1,0,1,1,1,"Leaving the tournament ring permanently eliminates that fighter.") \
 X(CPU_TRANSFORM,"tag_cpu_transform","CPU Transformations",1,0,200,10,100,"100 percent is normal. Zero disables CPU transformations.") \
 X(SHARED_CLASH,"tag_shared_clash","Beam Clash Camera",2,0,1,1,0,"Clash participants use the clash camera. Optionally share it with everyone.") \
 X(UNLOCKED_HUD,"tag_unlocked_hud","Unlocked Enemy HUD",3,0,2,1,2,"Always show, show with one enemy, or hide while locked off.") \
 X(FUSIONS,"tag_fusions","Allow Fusions",7,0,1,1,1,"Allow fusion moves.") \
 X(TRAIN_HEALTH,"tag_training_health","Recover Health",6,0,1,1,1,"Recover health after two seconds without damage in training.") \
 X(TRAIN_KI,"tag_training_ki","Full Ki",6,0,1,1,1,"Keep ki full in modded training.") \
 X(TRAIN_STOCKS,"tag_training_stocks","Full Blast Stocks",6,0,1,1,1,"Keep blast stocks full in modded training.") \
 X(ALLY_PANEL,"tag_ally_panel","Side Ally Health",4,0,1,1,0,"Show the teammate health list beside your view.") \
 X(ALLY_OVERHEAD,"tag_ally_overhead","Ally Overhead Health",4,0,1,1,1,"Show health above other living teammates.") \
 X(ENEMY_OVERHEAD,"tag_enemy_overhead","Enemy Overhead Health",4,0,1,1,1,"Show health above opponents.") \
 X(TARGET_MARKER,"tag_target_marker","Target Indicator",3,0,1,1,1,"Mark the fighter you are targeting.") \
 X(SHARED_ULTIMATE,"tag_shared_ultimate","Ultimate Camera",2,0,1,1,1,"Show the move's camera to everyone when it takes priority.") \
 X(SHARED_RUSH,"tag_shared_rush","Rush Attack Camera",2,0,1,1,0,"Participants follow the rush. Switch other players' views too.") \
 X(SHARED_GRAB,"tag_shared_grab","Grab Camera",2,0,1,1,0,"Participants follow the grab. Switch other players' views too.") \
 X(SHARED_MELEE,"tag_shared_melee","Melee Clash Camera",2,0,1,1,0,"Share the close combat clash camera with everyone.") \
 X(SHARED_VANISH,"tag_shared_vanish","Vanish Clash Camera",2,0,1,1,0,"Share the vanish battle camera with everyone.") \
 X(SHARED_TRANSFORM,"tag_shared_transform","Transformation Camera",2,0,1,1,1,"Show transformations to everyone, or keep player views.") \
 X(CAMERA_DISTANCE,"tag_camera_distance","Battle Camera Distance",3,70,180,10,100,"Normal combat camera distance. Authored cinematic shots stay intact.") \
 X(REVIVE_HEALTH,"tag_revive_health","Revival Health",0,5,100,5,25,"Percent of maximum health restored by a revival.") \
 X(REVIVE_RING,"tag_revive_ring","Show Revival Ring",0,0,1,1,1,"Show the range ring around fallen teammates.") \
 X(REVIVE_OPACITY,"tag_revive_opacity","Revival Ring Opacity",0,0,100,5,45,"How visible the revival range ring is, in percent.") \
 X(CONTROLLER_CHECK,"tag_controller_check","Controller Check In",5,0,1,1,1,"Require players to join with their own Start button before selecting.") \
 X(KEEP_CHECKINS,"tag_keep_checkins","Remember Controllers",5,0,1,1,1,"Keep player assignments until the game closes.") \
 X(ALL_SELECT,"tag_all_select","Shared Character Selection",5,0,1,1,0,"Let every controller use character selection. P1 can always go back.") \
 X(SLOT_LABELS,"tag_slot_labels","Player Slot Labels",5,0,1,1,1,"Show which player owns each character selection slot.") \
 X(EXTRA_INTROS,"tag_extra_intros","Fighter Intros",1,0,1,1,1,"Play an entrance for every fighter instead of only the team leaders.") \
 X(CINEMATIC_PAUSE,"tag_cinematic_pause","Special Move Pause",2,0,2,1,2,"Pause everyone else, only the target, or nobody when a move shares its camera.") \
 X(THREE_WIDE,"tag_three_wide","Three Player Layout",3,0,1,1,1,"Full width bottom view, or a centered bottom view the same size as the top two.") \
 X(TIMED_FUSIONS,"tag_timed_fusions","Timed Dance Fusions",7,0,1,1,1,"Fusion Dance partners separate when their timer expires. Potara is permanent.") \
 X(FUSION_SECONDS,"tag_fusion_seconds","Fusion Time Limit",7,10,300,10,80,"Fusion Dance time limit in seconds.") \
 X(FUSION_METER,"tag_fusion_meter","Fusion Time Meter",7,0,1,1,1,"Show time remaining for temporary Fusion Dance fusions.") \
 X(FUSION_SWAP,"tag_fusion_swap","Switch Fusion Control",7,0,1,1,1,"Two human fusion partners take turns controlling their fused fighter.") \
 X(FUSION_SWAP_SECONDS,"tag_fusion_swap_seconds","Control Switch Time",7,5,60,5,20,"Seconds of play before the other fusion partner takes control.") \
 X(FUSION_OWNER,"tag_fusion_owner","Show Fusion Controller",7,0,1,1,1,"Show which player is controlling the fused fighter.") \
 X(FUSION_COUNTDOWN,"tag_fusion_countdown","Control Switch Countdown",7,0,1,1,1,"Show who takes control next during the final five seconds before a switch.") \
 X(BEAM_ASSIST,"tag_beam_assist","Allow Beam Assists",1,0,1,1,1,"Press R3 near a struggling teammate to assist. Costs one blast stock.") \
 X(BEAM_ASSIST_POWER,"tag_beam_assist_power","Assist Power",1,110,300,10,150,"Push and damage multiplier with one helper, in percent. Stacks up to 300 percent.") \
 X(BEAM_ASSIST_CPU,"tag_beam_assist_cpu","CPU Beam Assists",1,0,1,1,1,"Nearby CPU teammates can join beam struggles.") \
 X(BEAM_ASSIST_RANGE,"tag_beam_assist_range","Assist Range",1,30,150,10,60,"Maximum distance from the struggling teammate.") \
 X(GIANTS,"tag_giants","Larger Giants",9,0,1,1,0,"Increase giant bodies and their collision volumes together.") \
 X(GIANT_SIZE,"tag_giant_size","Giant Size",9,100,400,25,200,"Percent of native size. Normal-sized fighters are unchanged.") \
 X(GIANT_SPEED,"tag_giant_speed","Giant Attack Speed",9,25,100,5,75,"Ordinary attacks only. Cinematic animations keep their normal timing.") \
 X(GIANT_DAMAGE,"tag_giant_damage","Giant Melee Damage",9,100,500,25,150,"Percent of ordinary melee damage while larger giants are enabled.") \
 X(GIANT_ARMOR,"tag_giant_armor","Giant Armor",9,0,5,1,1,"Extra stagger resistance. Damage and armor-breaking moves still work.") \
 X(GIANT_CAMERA,"tag_giant_camera","Giant Camera Distance",9,100,250,10,150,"Extra distance for normal follow cameras; cinematic cameras stay unchanged.") \
 X(SPECTATE_FALLEN,"tag_spectate_fallen","Watch Fallen Fighters",3,0,1,1,1,"Include defeated fighters when L3 switches the spectator camera.") \
 X(SPECTATOR_HINTS,"tag_spectator_hints","Spectator Hints",3,0,1,1,1,"Show camera switching and teammate takeover controls while spectating.") \
 X(TARGET_STYLE,"tag_target_style","Target Marker Look",3,0,3,1,0,"Choose an arrow, a ring, a corner reticle, or arrow and ring.") \
 X(TARGET_STICK,"tag_target_stick","Right Stick Targeting",3,0,2,1,2,"Off, hold L3 and flick, or flick alone. Locked off, the stick still moves the camera.") \
 X(ATTACK_WARNING,"tag_attack_warning","Enemy Attack Indicators",4,0,1,1,0,"Red pointers mark enemies targeting you and flash when they attack.") \
 X(STYLED_HUD,"tag_styled_hud","Styled Battle Indicators",4,0,1,1,1,"Use the mod's beveled health, revival and fusion meters and gold target arrow.") \
 X(OUTNUMBERED,"tag_outnumbered","Outnumbered Assistance",8,0,3,1,0,"Off, Balanced, Strong, or Custom. Help fighters on the smaller living team.") \
 X(OUT_SCOPE,"tag_out_scope","When to Help",8,0,1,1,1,"Smaller teams only, or also recovery help against two or more attackers.") \
 X(OUT_HUMANS,"tag_out_humans","Players Only",8,0,1,1,0,"Apply assistance only to human-controlled fighters.") \
 X(OUT_DAMAGE,"tag_out_damage","Custom Damage Bonus",8,0,100,5,15,"Bonus per team-size disadvantage. Total damage is capped at 300 percent.") \
 X(OUT_DEFENSE,"tag_out_defense","Custom Damage Reduction",8,0,60,5,10,"Reduction per team-size disadvantage. At least 40 percent damage remains.") \
 X(OUT_RECOVERY,"tag_out_recovery","Custom Recovery Speed",8,100,300,10,150,"Speed of ordinary hit reactions and recovery. Cinematics are unchanged.") \
 X(OUT_PROTECTION,"tag_out_protection","Custom Get Up Protection",8,0,30,1,5,"Tenths of a second protected from ordinary hits. Specials and grabs still work.") \
 X(OUT_BREAKER,"tag_out_breaker","Custom Combo Breaker",8,0,30,1,12,"Hits before an escape, with a five-second cooldown. Zero disables it.") \
 X(LOCAL_CINEMATICS,"tag_local_cinematics","Keep Moves Nearby",2,0,1,1,0,"Stage attacks and transformations near their starting point, with terrain and arena safety checks.") \
 X(PLAYER_LABELS,"tag_player_labels","Player Corner Labels",4,0,1,1,0,"Show P1, P2 and CPU labels in the corner of each view.") \
 X(SHARED_SPECIAL,"tag_shared_special","Special Attack Camera",2,0,1,1,0,"Switch everyone's camera for ordinary beam and projectile specials.") \
 X(MAP_SIZE,"tag_map_size","Map Size",10,100,300,25,100,"Scale the arena, terrain and water together. Applies on the next stage load; fighters keep their normal size.") \
 X(REVIVE_WAVE_HEIGHT,"tag_revive_wave_height","Ring Wave Height",0,0,80,1,8,"Height of the traveling edge around the revival ring. Zero makes it flat.") \
 X(REVIVE_WAVE_SPEED,"tag_revive_wave_speed","Ring Wave Speed",0,0,500,25,100,"Speed of the traveling edge. 100 percent is one cycle per second; zero holds it still.") \
 X(NATIVE_HUD,"tag_native_hud","Base Game HUD",4,0,1,1,1,"Show the original health, ki, timer and battle prompts. Mod indicators have their own switches.") \
 X(COMBO_METER,"tag_combo_meter","Combo Hit Counter",4,0,1,1,1,"Show the number of hits in a combo.") \
 X(DAMAGE_METER,"tag_damage_meter","Damage Counter",4,0,1,1,1,"Show the damage dealt by a combo or special move.") \
 X(CLASH_QTE,"tag_clash_qte","Clash Button Prompts",1,0,1,1,0,"Replace clash mashing and stick spinning with successive button prompts. A wrong press briefly locks your input.")
#define TAG_OPTION_ENUM(id,key,label,group,min,max,step,def,help) TAG_OPT_##id,
enum {
    TAG_OPTION_LIST(TAG_OPTION_ENUM)
    TAG_OPT_COUNT
};
#undef TAG_OPTION_ENUM
typedef struct TagOptionInfo {
    const char *key, *label, *help;
    int group, min, max, step, initial;
} TagOptionInfo;
#define TAG_OPTION_INFO(id,key,label,group,min,max,step,def,help) {key,label,help,group,min,max,step,def},
static const TagOptionInfo tagOptionInfo[TAG_OPT_COUNT]={TAG_OPTION_LIST(TAG_OPTION_INFO)};
#undef TAG_OPTION_INFO
#define TAG_GROUP_NAME(name) name,
static const char *tagOptionGroups[]={TAG_OPTION_GROUPS(TAG_GROUP_NAME)};
#undef TAG_GROUP_NAME
#define TAG_OPTION_GROUP_COUNT 11
/* These switches only draw information. In a room they stay local; gameplay
   and cinematic rules still come from the host. Keep this explicit so adding
   a gameplay setting to the HUD category cannot accidentally desynchronize it. */
static inline int TagOption_IsPresentation(int id) {
    switch(id) {
    case TAG_OPT_ALLY_PANEL: case TAG_OPT_ALLY_OVERHEAD: case TAG_OPT_ENEMY_OVERHEAD:
    case TAG_OPT_TARGET_MARKER: case TAG_OPT_TARGET_STYLE: case TAG_OPT_UNLOCKED_HUD:
    case TAG_OPT_REVIVE_RING: case TAG_OPT_REVIVE_OPACITY:
    case TAG_OPT_REVIVE_WAVE_HEIGHT: case TAG_OPT_REVIVE_WAVE_SPEED:
    case TAG_OPT_FUSION_METER: case TAG_OPT_FUSION_OWNER: case TAG_OPT_FUSION_COUNTDOWN:
    case TAG_OPT_SPECTATOR_HINTS: case TAG_OPT_ATTACK_WARNING: case TAG_OPT_STYLED_HUD:
    case TAG_OPT_PLAYER_LABELS: case TAG_OPT_NATIVE_HUD:
    case TAG_OPT_COMBO_METER: case TAG_OPT_DAMAGE_METER:
        return 1;
    default:return 0;
    }
}
#ifdef __cplusplus
extern "C" {
#endif
int Port_TagOption(int option);
void Port_TagOptionSet(int option,int value);
#ifdef __cplusplus
}
#endif
#endif
