#ifdef PORT
#include "common.h"
#include "battle/tag_team.h"
#include "battle/tag_session.h"
#include "battle/tag_challenge.h"
#include "menu/tag_tournament.h"
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "menu/port_screen.h"
#include "sys/save.h"
#include "plat_songs.h"

static s32 selected, mode, humans=1;
static s32 duelTime=3,comLevel=2;
static s32 musicIds[1+24+PORT_SONG_MAX],musicCount,musicChoice;
static void music_reset(void) {
    s32 i;
    musicCount=1;musicChoice=0;musicIds[0]=TS_BGM_RANDOM;
    PortSongs_Refresh();
    for(i=0;i<(PortGame_IsBT4()?24:20);i++)
        if(PortGame_IsBT4() || (gSaveData->bgmBits&(1u<<i)))musicIds[musicCount++]=i;
    if(!PortGame_IsBT4())for(i=0;i<gPortSongCount && i<PORT_SONG_MAX;i++)
        musicIds[musicCount++]=gPortSongOffsets[i];
}
s32 TagSession_Music(void) {return musicCount?musicIds[musicChoice]:TS_BGM_RANDOM;}
s32 TagSession_BattleMusic(void) {
    s32 id=TagSession_Music();
    /* Resolve Random from the same playable choices shown in setup, rather
       than the stage screen's raw disc list (which may include placeholders). */
    return id!=TS_BGM_RANDOM?id:musicCount>1?musicIds[1+Rand_Range(musicCount-1)]:8;
}
static s32 cpuSetting[4][5];
static s32 selection_side(s32 side) {return selected && mode==TAG_MODE_TOURNAMENT && !TagTournament_Active() && side==0?TagTournament_DraftTeam():side;}
s32 TagSession_CpuSetting(s32 side,s32 member) {side=selection_side(side);return cpuSetting[side][member]<0?comLevel:cpuSetting[side][member];}
void TagSession_SetCpuSetting(s32 side,s32 member,s32 level) {side=selection_side(side);if((u32)side<4 && (u32)member<5 && (u32)level<5)cpuSetting[side][member]=level;}
s32 TagSession_CpuLevel(s32 side,s32 member) {extern s32 CpuLevel_FromSetting(u32);return CpuLevel_FromSetting(TagSession_CpuSetting(side,member));}
/* This page assigns people to teams only. The native roster's End button
 * determines the actual sizes later, including unequal teams. */
static s32 playerTeam[4];
static s32 owner_at(s32 side,s32 member) {
    s32 p,n=0;
    for(p=0;p<humans;p++) if(playerTeam[p]==side) {
        if(n++==member) return p;
    }
    return -1;
}
void TagSession_Setup(s32 nextMode,s32 players) {
    s32 p;
    TagSession_Leave(); mode=nextMode; humans=players<0?0:players>4?4:players;
    duelTime=mode==TAG_MODE_TRAINING?4:gSaveData->rule[0];
    comLevel=gSaveData->rule[1];
    if(duelTime<0 || duelTime>4) duelTime=3;
    if(comLevel<0 || comLevel>4) comLevel=2;
    if(mode==TAG_MODE_GAUNTLET)music_reset();
    for(p=0;p<humans;p++) playerTeam[p]=mode==TAG_MODE_GAUNTLET || mode==TAG_MODE_TOURNAMENT?0:p%2;
}
s32 TagSession_Time(void) { static const s32 times[]={1,2,3,4,0};return mode==TAG_MODE_GAUNTLET || mode==TAG_MODE_TOURNAMENT?0:times[duelTime]; }
s32 TagSession_Level(void) { extern s32 CpuLevel_FromSetting(u32);return CpuLevel_FromSetting(comLevel); }
void TagSession_ChangeRule(s32 rule,s32 direction) {
    extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
    s32 *value=rule?&comLevel:&duelTime;
    if(rule==2 && mode==TAG_MODE_GAUNTLET) {
        musicChoice=Ps_Wrap(musicChoice+direction,0,musicCount-1);
        Port_TagMenuTrace(94,musicChoice,TagSession_Music(),musicCount,0,0);return;
    }
    if(rule>1)return;
    if(mode==TAG_MODE_TOURNAMENT && !rule)return;
    if(mode==TAG_MODE_GAUNTLET && !rule) {TagChallenge_Refill(direction);return;}
    *value=Ps_Wrap(*value+direction,0,4);
    Port_TagMenuTrace(47,rule,direction,duelTime,comLevel,humans);
}
const char *TagSession_RuleLabel(s32 rule) {
    static const char *times[]={"60 seconds","90 seconds","180 seconds","240 seconds","No limit"};
    extern const char *CpuLevel_SettingName(s32);
    if(rule==2 && mode==TAG_MODE_GAUNTLET) {
        static char name[80];
        s32 id=TagSession_Music();
        extern void TagNet_MenuName(s32,s32,u32,s32);
        if(id>=PORT_SONG_FIRST_OFFSET && !PortGame_IsBT4())
            Ps_Copy(name,gPortSongNames[id-PORT_SONG_FIRST_OFFSET],sizeof(name));
        else TagNet_MenuName(2,id,(u32)name,sizeof(name));
        return name;
    }
    if(mode==TAG_MODE_GAUNTLET && !rule)return TagChallenge_RefillLabel();
    if(rule)return CpuLevel_SettingName(comLevel);
    return times[duelTime];
}
s32 TagSession_Selected(void) { return selected; }
s32 TagSession_Mode(void) { return selected ? mode : TAG_MODE_TEAM; }
s32 TagSession_Players(void) { return humans; }
s32 TagSession_Size(s32 side) { return 5; }
s32 TagSession_MinSize(s32 side) {
    s32 n=TagSession_DraftSize(selection_side(side));
    return n>0?n:1;
}
s32 TagSession_Owner(s32 side,s32 member) {
    return selected && side>=0 && side<2 && member>=0 && member<5 ? owner_at(selection_side(side),member) : -1;
}
void TagSession_Leave(void) {
    extern void TeamSel_ResetTagRoster(void);
    memset(cpuSetting,0xFF,sizeof(cpuSetting));
    selected=0; TagTeam_Enable(0);
    TagTournament_End();
    /* Keep selections only while returning inside this battle session. A new
       mode or a visit back to the main/setup menu starts a fresh roster. */
    TeamSel_ResetTagRoster();
}
void TagSession_Open(void) {
    TagSession_Leave(); TagMenu_Request(3); gProgress->mode=4;
}
void TagSession_Return(s32 flags,s32 reason) {
    if(!selected) return;
    if((flags&8) && (reason&(8|0x80))) { TagSession_Leave(); TagMenu_Request(1); gProgress->mode=4; }
    else if(mode==TAG_MODE_TOURNAMENT) gProgress->mode=35;
    else gProgress->mode=40;
}
s32 TagSession_DraftMode(void) { return mode; }
s32 TagSession_DraftSize(s32 side) {
    s32 p,n=0;
    for(p=0;p<humans;p++) if(playerTeam[p]==side) n++;
    return n;
}
s32 TagSession_DraftOwner(s32 side,s32 member) { return owner_at(side,member); }
s32 TagSession_PlayerTeam(s32 player) { return playerTeam[player]; }
void TagSession_SetPlayerTeam(s32 player,s32 team) {
    extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
    if((u32)player>=(u32)humans || (u32)team>(mode==TAG_MODE_TOURNAMENT?3:1) || mode==TAG_MODE_GAUNTLET) return;
    playerTeam[player]=team;
    Port_TagMenuTrace(45,playerTeam[0],playerTeam[1],playerTeam[2],playerTeam[3],humans);
}
s32 TagSession_Commit(void) {
    selected=1; TagTeam_Enable(1);
    gTsProgress->players=humans?TEAMSEL_PLAYERS_VS_CPU:TEAMSEL_PLAYERS_CPU_CPU;
    gTsProgress->battleType=1;
    if(mode==TAG_MODE_GAUNTLET)gTsProgress->bgm=TagSession_Music();
    if(mode==TAG_MODE_TOURNAMENT)TagTournament_BeginDraft();
    return 1;
}
s32 TagSession_Run(void) { TagSession_Open(); return 0; }
void TagSession_Apply(void) {
    s32 t,m,actor=0;
    if(!selected) return;
    TagTeam_Configure(humans,humans,0);
    for(m=0;m<5;m++) for(t=0;t<2;t++) if(m<Battle_GetSetup()->sides[t].memberCount) TagTeam_SetAssignment(actor++,owner_at(t,m));
    {
        extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
        BattleSetup *b=Battle_GetSetup();
        Port_TagMenuTrace(46,b->rule.timeLimit,b->sides[0].members[0].cpuLevel,
            humans,mode,b->sides[1].members[b->sides[1].memberCount-1].cpuLevel);
    }
}
#endif
