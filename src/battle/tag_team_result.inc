#include "battle/hud_0.h"
static s32 sTagResultActive,sTagResultCursor,sTagResultFrames,sTagResultWinner,sTagResultSent;
static s32 sTagResultPending,sTagResultStickTimer;
static u32 sTagResultStickLast,sTagResultStickHeld;
extern int Port_TagResultChoice(void),Port_TagHost(void),Port_TagAllEnded(void),Port_TagPhase(void);
extern void Port_TagClose(void),Port_TagCommand(int,int,int);
static void TagOnline_Ending(void);
extern BtlMenu gPauseMenuResultVersus;
extern void BtlMenu_PortDrawMenu(BtlMenu *menu);
static BtlMenu sTagNativeResult;
static BtlMenuItem sTagNativeItems[4];

u16 *TagBattle_ResultText(s32 id) {
    extern int Port_GameProfileEnglish(void);
    static u16 lines[4][128];
    u16 *line=lines[id-0x7110];
    const char *text; s32 i,en=Port_GameProfileEnglish();
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_UI_LANGUAGE_TEST"))en=!strcmp(getenv("BT4_UI_LANGUAGE_TEST"),"en");
    switch(id) {
    case 0x7110:
        if(TagBattle_FreeForAll()){
            extern const char *Port_TagPlayerName(s32);
            static char title[96];s32 winner=TagBattle_WinningActor();
            if(winner<0)text=en?"DRAW":"EMPATE";
            else {
                const char *name=Port_TagPlayerName(winner);char cpu[20];
                if(!name[0]){snprintf(cpu,sizeof(cpu),"CPU %02d",winner+1);name=cpu;}
                snprintf(title,sizeof(title),en?"%s WINS":"GANA %s",name);text=title;
            }
            break;
        }
        text=sTagResultWinner==1?(en?"TEAM 1 WINS":"GANA EL EQUIPO 1"):
        sTagResultWinner==2?(en?"TEAM 2 WINS":"GANA EL EQUIPO 2"):(en?"DRAW":"EMPATE");break;
    case 0x7111: text=sTagResultPending>=0?(en?"Waiting for players...":"Esperando jugadores..."):(en?"Rematch":"Revancha");break;
    case 0x7112: text=en?"Return to Character Select":"Volver a elegir personaje";break;
    default: text="Tenkaichi Budokai Online";break;
    }
    for(i=0;text[i]&&i<127;++i)line[i]=(u8)text[i];
    line[i]=0;return line;
}
static void TagBattle_ResultNativeBegin(void) {
    s32 i,guest=TagBattle_Network()&&!Port_TagHost();
    sTagNativeResult=gPauseMenuResultVersus;
    memset(sTagNativeItems,0,sizeof(sTagNativeItems));
    sTagNativeResult.items=sTagNativeItems;sTagNativeResult.count=4;
    for(i=0;i<4;++i) {
        sTagNativeItems[i].text[0]=sTagNativeItems[i].text[1]=0x7110+i;
        sTagNativeItems[i].type=BTL_MENU_ITEM_VALUE;
        sTagNativeItems[i].disabled=i==0||(guest&&i<3);
    }
    BtlMenu_Reset(&sTagNativeResult);
    sTagNativeResult.cursor=sTagResultCursor+1;
}
static void TagBattle_ResultNativeUpdate(void) {
    u32 pressed=gPad[0].gamePressed;
    /* The original engine owns artwork, font and animation. Actions remain
     * behind the shared result barrier, outside the simulation's input stream. */
    gPad[0].gamePressed=0;
    BtlMenu_SetScript(((u16 **)((u8 *)BtlObj_Get(BattleSide_GetObjId(0))+0xBC))[0]);
    BtlMenu_Update(1,NULL,&sTagNativeResult,0);BtlMenu_ClearActive();
    gPad[0].gamePressed=pressed;sTagNativeResult.cursor=sTagResultCursor+1;
}
void TagBattle_ResultBegin(void) {
    s32 flags=Battle_GetResult()->winner;
    sTagResultWinner=(flags&BTL_RESULT_WIN_P1)?1:(flags&BTL_RESULT_WIN_P2)?2:0;
    sTagResultCursor=TagBattle_Network()&&!Port_TagHost()?2:0;
    sTagResultFrames=0;sTagResultSent=0;sTagResultActive=1;
    sTagResultPending=-1;sTagResultStickTimer=0;sTagResultStickLast=sTagResultStickHeld=0;
    if(TagBattle_Network()) {
        if(!Port_TagHost()&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RESULT_READY_DELAY")) {
            s32 i;extern void Port_TagYield(void);for(i=0;i<250;++i)Port_TagYield();
        }
        Port_TagCommand(21,0,0);
    }
    {extern void DemoCam_Reset(void);DemoCam_Reset();}
    TagBattle_ResultNativeBegin();
    if(getenv("BT4_TAG_RESULT_TEST"))printf("tag result: winner=%d opened native=1 options=3\n",sTagResultWinner);
}
s32 TagBattle_ResultUpdate(void) {
    const char *test=getenv("BT4_TAG_RESULT_TEST");
    s32 network=TagBattle_Network(),host=!network||Port_TagHost(),confirm;
    ++sTagResultFrames;TagBattle_ResultNativeUpdate();
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_LONG_VIDEO")&&sTagResultFrames<300)return 0;
    if(sTagResultFrames<30)return 0;
    if(test&&test[0]!='h'&&sTagResultFrames%15==0) {
        s32 wanted=atoi(test);
        if(sTagResultCursor!=wanted)gPad[0].gameRepeat=PADG_DOWN;
        else gPad[0].gamePressed=PADG_CROSS;
    }
    confirm=(gPad[0].gamePressed&(PADG_CROSS|PADG_START))!=0;
    if(host&&sTagResultPending<0) {
        u32 stick=gPad[0].gameLeft[1]>0.5f?PADG_DOWN:gPad[0].gameLeft[1]<-0.5f?PADG_UP:0;
        u32 repeat=Pad_CalcRepeat(stick,stick&~sTagResultStickHeld,&sTagResultStickTimer,&sTagResultStickLast,20,4);
        s32 was=sTagResultCursor;sTagResultStickHeld=stick;Ps_Move(&sTagResultCursor,3);
        if(was==sTagResultCursor) {
            if(repeat&PADG_DOWN)sTagResultCursor=(sTagResultCursor+1)%3;
            else if(repeat&PADG_UP)sTagResultCursor=(sTagResultCursor+2)%3;
        }
    }
    if(network) {
        s32 choice=Port_TagResultChoice();
        if(choice<0) {
            if((confirm&&sTagResultCursor==2)||Port_TagPhase()==0||Port_TagPhase()==6) {
                sTagResultCursor=2;TagOnline_Ending();Port_TagClose();
            } else {
                if(host&&confirm&&sTagResultPending<0)sTagResultPending=sTagResultCursor;
                if(!sTagResultSent&&(gPad[0].gamePressed&PADG_TRIANGLE))sTagResultPending=-1;
                if(host&&sTagResultPending>=0&&Port_TagAllEnded()&&!sTagResultSent) {
                    Port_TagCommand(18,sTagResultPending,0);sTagResultSent=1;
                }
                return 0;
            }
        } else {sTagResultCursor=choice;TagOnline_Ending();}
    } else if(!confirm)return 0;
    if(sTagResultCursor==0)BattleResult_Set(BTL_RESULT_ABORT,0x8000);
    else {
        if(sTagResultCursor==1&&!network)Port_TagCommand(18,1,0);
        TagBattle_SetReturn(sTagResultCursor==1?1:2);BattleResult_Set(BTL_RESULT_ABORT,0);
    }
    if(test)printf("tag result: choice=%d confirmed\n",sTagResultCursor);
    sTagResultActive=0;return 1;
}
void TagBattle_ResultDraw(void) {
    if(sTagResultActive)BtlMenu_PortDrawMenu(&sTagNativeResult);
}
