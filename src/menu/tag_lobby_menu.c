/* Native-font practice overlay. Input/cursor state lives outside rollback;
   drawing it does not stop the simulation or allocate battle resources. */
#include "common.h"
#include "menu/port_screen.h"
#include "sys/pad.h"
#include "battle/tag_options.h"
#include "menu/tag_kit.h"
#include "../../port/src/gs/tag_net.h"

static int selectedGuest=-1;
int TagLobbyRoster_Input(void) {
    TagLobbyRoster r;int i,row=-1;
    TagNet_Roster((u32)&r);
    for(i=1;i<r.count;i++)if(r.member[i]==selectedGuest)row=i;
    if(gPad[0].gamePressed&PADG_R3){selectedGuest=row>=0?-1:r.count>1?r.member[1]:-1;return 1;}
    if(row<0){selectedGuest=-1;return 0;}
    if(gPad[0].gamePressed&PADG_TRIANGLE){selectedGuest=-1;return 1;}
    if(gPad[0].gameRepeat&PADG_UP)row=row<=1?r.count-1:row-1;
    if(gPad[0].gameRepeat&PADG_DOWN)row=row+1>=r.count?1:row+1;
    selectedGuest=r.member[row];
    if(gPad[0].gamePressed&PADG_SQUARE){TagNet_Kick(selectedGuest);selectedGuest=-1;}
    return 1;
}
void TagLobbyRoster_Draw(int hostMenu) {
    TagLobbyRoster r;int i;char line[64];float y=hostMenu?48:142;
    if(!hostMenu && (Port_TagOption(TAG_OPT_HIDE_ALL_HUD) || !Port_TagOption(TAG_OPT_PRACTICE_ROSTER)))return;
    TagNet_Roster((u32)&r);
    Ps_PageSet();Ps_SetShift(0);Ps_Align(1);
    Ps_Text(422,y,hostMenu?"CONNECTED":"PLAYERS / PING TO HOST",hostMenu?.60f:.49f,156,1);
    for(i=0;i<r.count;i++) {
        if(!r.member[i])sprintf(line,Port_TagText("P1  -  Host"));
        else if(r.ping[i]<0)sprintf(line,"P%d  -  ...",r.member[i]+1);
        else sprintf(line,"P%d  -  %d ms",r.member[i]+1,r.ping[i]);
        Ps_Text(422,y+16+i*14,line,.59f,156,r.member[i]==r.role || (hostMenu && r.member[i]==selectedGuest)?1:3);
    }
    if(hostMenu)Ps_Text(422,y+20+r.count*14,selectedGuest>=0?"SQUARE - REMOVE / TRIANGLE - BACK":"R3 - MANAGE PLAYERS",.45f,156,3);
}

void TagLobbyMenu_Draw(void) {
    TagPracticeUi ui;int i,row=0;char label[128];
    static char guest[][24]={"Return to Practice","Change Fighter","Leave Lobby"};
    if(!TagNet_IsPractice())return;
    TagNet_PracticeUi((u32)&ui);
    if(!ui.visible) {
        if(Port_TagOption(TAG_OPT_HIDE_ALL_HUD))return;
        TagLobbyRoster_Draw(0);
        extern const char *TagLobby_Prompt(void);const char *s=TagLobby_Prompt();
        if(s && *s){Ps_PageSet();Ps_SetShift(0);Ps_Align(1);Ps_Text(256,396,(char *)s,.65f,470,3);}
        return;
    }
    Tk_Begin();Ps_Align(1);
    /* the online screens' look over the running map: a dusk, the kit's bars and window (menu/tag_kit.h) */
    Ps_ScreenQuad(0,0,512,0,0,448,512,448,Ps_Rgba(6,10,40,96),Ps_Rgba(6,10,40,96),Ps_Rgba(4,6,24,118),Ps_Rgba(4,6,24,118));
    Tk_Title("PRACTICE LOBBY",NULL);
    Ps_Text(256,58,"The practice map keeps running while this menu is open.",.64f,478,3);
    for(i=0;i<3;i++) {
        float y=90+i*42;
        Tk_Bar(ui.choice==i?22:30,y,ui.choice==i?296:272,y+34,ui.choice==i);
        Ps_Text(150,Ps_TextTop(y+17,ui.choice==i?.86f:.77f),guest[i],ui.choice==i?.86f:.77f,231,ui.choice==i?1:0);
    }
    Tk_Box(300,80,492,392,0);
    Ps_Text(395,94,"PVP LEADERBOARD",.72f,180,2);
    Ps_Text(395,117,"Player          W / L / D",.58f,180,3);
    for(i=1;i<TL_MEMBERS;i++)if(ui.lobby.members[i].present) {
        TlMember *m=&ui.lobby.members[i];sprintf(label,"P%d           %d / %d / %d",i+1,m->wins,m->losses,m->draws);
        Ps_Text(395,142+row++*24,label,.65f,202,ui.choice==100+i?1:3);
    }
    Ps_Align(0);Ps_PromptsClear();Ps_Prompt(0,"Select");Ps_Prompt(2,"Resume");Ps_PromptsDraw();Ps_Align(1);
}
