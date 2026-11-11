#include "../../port/src/gs/online_language.h"
/* Native TAG TEAM draft. A connection controls one roster seat, never both
 * teams. Confirmed choices come from the room authority. The normal two-pad
 * selector and its battle handoff above remain unchanged.
 * Local CPU combat uses the normal stage/music picker after the draft.
 */
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
extern void TagBattle_ClearDraftItems(void),TagBattle_SetDraftItems(s32,s32,const u16 *);
extern void Port_TagPoll(void), Port_TagClose(void), Port_TagCommand(int command, int a, int b);
extern int Port_TagPhase(void), Port_TagSeat(void), Port_TagTurn(void), Port_TagCapacity(void);
extern int Port_TagDraftSeat(void), Port_TagCPU(int seat);
extern int Port_TagDraftPending(void);
extern int Port_TagHumans(void), TagBattle_ConfigureLocal(void),Port_TagNetwork(void),Port_TagHost(void);
extern void Port_TagArena(int,int,int);
extern int Port_TagStage(void),Port_TagBgm(void);
extern int Port_TagPlayerChara(int seat), Port_TagPlayerCostume(int seat), Port_TagConfirmed(int seat);
extern const char *Port_TagPlayerName(int seat), *Port_TagError(void);

extern int Port_TagFreeForAll(void);
extern int Port_TagTeam(int seat),Port_TagTeamIndex(int seat),Port_TagTeamCount(int team);
static s32 sTagPage[2]={-1,-1};
static void TeamSel_TagSetRoster(void){
    s32 team,i,n,page,first;
    for(team=0;team<2;++team){
        TeamSelSide *side=gTeamSel->side[team];s32 count=0,changed;
        first=Port_TagTeamCount(team);
        for(i=0;i<Port_TagCapacity();++i)if(Port_TagTeam(i)==team&&!Port_TagConfirmed(i)){first=Port_TagTeamIndex(i);break;}
        page=(first<Port_TagTeamCount(team)?first:first-1)/5;if(page<0)page=0;
        changed=sTagPage[team]!=page;sTagPage[team]=page;
        if(changed){memset(side->member,0,sizeof(side->member));for(n=0;n<5;++n)side->member[n].chara=-1;}
        for(i=0;i<Port_TagCapacity();++i)if(Port_TagTeam(i)==team&&Port_TagConfirmed(i)&&Port_TagTeamIndex(i)/5==page){
            s32 chara=Port_TagPlayerChara(i),costume=Port_TagPlayerCostume(i),checked=costume,valid=0,cell,form;
            n=Port_TagTeamIndex(i)%5;
            for(cell=0;cell<gTeamSel->masterCount[team];++cell){TsCell *entry=&gTeamSel->cells[team][cell];
                if(entry->id==chara&&chara!=TS_RANDOM&&chara!=TS_LOCKED&&chara!=TS_CUSTOM&&chara!=TS_EMPTY)valid=1;
                for(form=0;form<entry->formCount&&form<7;++form)if(entry->form[form]==chara)valid=1;}
            if(!valid||ChrTbl_WrapCostume(chara,&checked)<=0||checked!=costume){Port_TagClose();return;}
            side->member[n].chara=chara;side->member[n].color=costume;memset(&side->member[n].items,0,sizeof(TsItemSet));++count;
        }
        if(side->memberCount!=count||changed){
            side->memberCount=count;side->cur=count<5?count:TS_CUR_MENU;TeamSel_SetTeamTex(team);
            if(count>0){side->chara=side->member[count-1].chara;TeamSel_RequestFace(team);}
            Flash_GotoLabel(&gTeamSel->flash[team+1],"fl_in",1);Flash_GotoLabel(&gTeamSel->flash[team+3],"fl_out",1);
        }
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_AVAILABILITY_TEST")){
        static s32 previous=-1;s32 mask=0;for(i=0;i<Port_TagCapacity();++i)if(Port_TagConfirmed(i))mask|=1<<i;
        if(previous!=mask){previous=mask;for(team=0;team<2;++team)
            fprintf(stderr,"tag availability: confirmed=%x team=%d chara=184 real=%d free=%d\n",mask,team,TeamSel_IsRealChara(184),TeamSel_IsCharaFree(team,184));}
    }
}

extern unsigned Port_TagMenuMillis(void);
extern void Port_TagMenuTestAdvance(unsigned ms);
extern s32 Port_TagCancelSelect(void),Port_TagReselect(void);
static s32 sTagTurnSeconds,sTagTransition;
static unsigned sTagTurnStarted;
static void TeamSel_TagDrawFilter(void){
    static char *clips[]={"mc_single_chara_l","mc_single_chara_r","mc_name_text_l","mc_name_text_r","mc_form_text_l","mc_form_text_r"};
    MFlashRef ref;s32 i,hide=(gTeamSel->flags&TEAMSEL_STAGE)!=0;
    if(hide || sTagFacesHidden){
        for(i=0;i<6;++i){Flash_FindLabel(&gTeamSel->flash[0],NULL,clips[i],&ref);Flash_ClipSetFlags(&gTeamSel->flash[0],&ref,2,!hide);}
    }
    sTagFacesHidden=hide;
}
static s32 TeamSel_TagComplete(void){
    s32 i;for(i=0;i<Port_TagCapacity();++i)if(!Port_TagConfirmed(i))return 0;
    return Port_TagCapacity()>0;
}
static s32 sTagPublishedStage=-1,sTagPublishedBgm=-1;
static s32 sTagAppliedStage=-1,sTagAppliedBgm=-1;
static void TeamSel_TagPublishArena(void){
    s32 stage,bgmIndex,bgm;
    if(!Port_TagNetwork()||!Port_TagHost()||Port_TagPhase()!=2||!gTeamSel||!gTeamSel->stage)return;
    stage=gTeamSel->stage->stage;
    bgmIndex=gTeamSel->stage->state==TEAMSEL_STAGE_BGM?gTeamSel->stage->bgmCursor:gTeamSel->stage->bgm;
    if(stage<0||stage>=0x63||bgmIndex<0||bgmIndex>=gTeamSel->bgmCount)return;
    bgm=gTeamSel->bgmIds[bgmIndex];if(bgm<0||bgm>65535)return;
    if(stage==sTagPublishedStage&&bgm==sTagPublishedBgm)return;
    Port_TagArena(stage,bgm,1);sTagPublishedStage=stage;sTagPublishedBgm=bgm;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag map preview: published stage=%d bgm=%d cell=%d col=%d row=%d\n",stage,bgm,gTeamSel->stage->row*TS_STAGE_COLS+gTeamSel->stage->col,gTeamSel->stage->col,gTeamSel->stage->row);
}
static void TeamSel_TagApplyArena(void){
    s32 stage,bgm,i,cell=-1,bgmIndex=-1,cursorChanged,rowChanged,stageChanged;
    if(!Port_TagNetwork()||Port_TagHost()||Port_TagPhase()!=2||!(gTeamSel->flags&TEAMSEL_STAGE))return;
    stage=Port_TagStage();bgm=Port_TagBgm();
    if(stage>=0&&stage<0x63&&stage!=sTagAppliedStage){
        for(i=0;i<gTeamSel->stageCount;++i)if(gTeamSel->stageIds[i]==stage){cell=i;break;}
        if(cell>=0){
            s32 col=cell%TS_STAGE_COLS,row=cell/TS_STAGE_COLS;
            cursorChanged=gTeamSel->stage->col!=col||gTeamSel->stage->row!=row;
            rowChanged=gTeamSel->stage->row!=row;
            stageChanged=gTeamSel->stage->stage!=stage;
            if(cursorChanged){
                TeamSel_ClipGoto(0,0,TEAMSEL_CLIP_STAGE_CHIP,"fl_off_start");
                gTeamSel->stage->col=col;gTeamSel->stage->row=row;
                if(rowChanged)TeamSel_SetStageChips();
            }
            if(stageChanged){
                gTeamSel->stage->stage=stage;TeamSel_RequestStage();
            }
            if(cursorChanged)TeamSel_ClipGoto(0,0,TEAMSEL_CLIP_STAGE_CHIP,"fl_on_start");
            sTagAppliedStage=stage;
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag map preview: received stage=%d cell=%d col=%d row=%d cursor-updated=%d\n",stage,cell,col,row,cursorChanged);
        }
    }
    if(bgm>=0&&bgm<=65535&&bgm!=sTagAppliedBgm){
        for(i=0;i<gTeamSel->bgmCount;++i)if(gTeamSel->bgmIds[i]==bgm){bgmIndex=i;break;}
        if(bgmIndex>=0){gTeamSel->stage->bgm=bgmIndex;gTeamSel->stage->bgmCursor=bgmIndex;sTagAppliedBgm=bgm;
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag map preview: received bgm=%d\n",bgm);}
    }
}
static void TeamSel_TagMapTransition(void){
    s32 i;
    /* Close the six character movies before the map timeline enters. */
    for(i=0;i<2;++i){
        ItemPanel_Hide(i);gTeamSel->side[i]->state=TEAMSEL_ST_DONE;
        gTeamSel->side[i]->flags&=~TEAMSEL_SIDE_PANEL;
        Flash_GotoLabel(&gTeamSel->flash[i+1],"fl_out",1);
        Flash_GotoLabel(&gTeamSel->flash[i+3],"fl_out",1);
        Flash_GotoLabel(&gTeamSel->flash[i+5],"fl_color_ok",1);
    }
    ItemHelp_Close();sTagTransition=20;sTagTurnSeconds=0;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag select: closing character movies before map\n");
}
static void TeamSel_TagOpenMap(void){
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_LONG_VIDEO")){
        s32 wanted=getenv("BT4_TEST_STAGE")?atoi(getenv("BT4_TEST_STAGE")):31;
        s32 i;for(i=0;i<gTeamSel->stageCount;++i)if(gTeamSel->stageIds[i]==wanted){
            gTeamSel->stage->col=i%TS_STAGE_COLS;gTeamSel->stage->row=i/TS_STAGE_COLS;gTeamSel->stage->stage=wanted;
            TeamSel_SetStageChips();TeamSel_RequestStage();break;
        }
    }
    Flash_GotoLabel(&gTeamSel->flash[0],"fl_map",1);IconWin_Open();
    TeamSel_ClipGoto(0,0,TEAMSEL_CLIP_STAGE_CHIP,"fl_on_start");
    gTeamSel->flags|=TEAMSEL_STAGE;gTeamSel->faceMask=1;
    gTeamSel->stage->state=TEAMSEL_STAGE_GRID;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag select: native map entered; character layers hidden\n");
}
static s32 TeamSel_TagRestartDraft(void){
    s32 i;
    if(Port_TagNetwork()){Port_TagCommand(12,0,0);gPad[0].gamePressed=0;return 0;}
    if(!Port_TagReselect())return 0;
    TagBattle_ClearDraftItems();
    gTeamSel->flags&=~(TEAMSEL_STAGE|TEAMSEL_DECIDED|TEAMSEL_LEAVING);
    gTeamSel->stage->mask=0;gTeamSel->faceMask=0;sTagTransition=0;
    Flash_GotoLabel(&gTeamSel->flash[0],"fl_map_cansel",1);IconWin_Close();
    TeamSel_ClipGoto(0,0,TEAMSEL_CLIP_STAGE_CHIP,"fl_off_start");
    for(i=0;i<2;++i){gTeamSel->side[i]->state=TEAMSEL_ST_GRID;gTeamSel->side[i]->mask=0;}
    TeamSel_TagSetRoster();Snd_PlaySe(1,2);
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag select: map back to character draft; room retained\n");
    return 1;
}
/* The master cell's first form is the same base shown by the native grid.
   On a non-fighter cell, walk the unlocked grid to a valid, available base. */
static s32 TeamSel_TagAutoConfirm(s32 seat){
    s32 team=Port_TagTeam(seat),slot=Port_TagTeamIndex(seat)%5,i,index,id=-1,color;
    TeamSelSide *side=gTeamSel->side[team];TsMember *m=&side->member[slot];
    s32 hovered=side->chara,origin=m->row*TS_COLS+m->col;
    if(side->flags&TEAMSEL_SIDE_CUSTOM){
        for(i=0;i<gTeamSel->masterCount[team];++i){
            TsCell *c=&gTeamSel->cells[team][i];s32 j,found=c->id==hovered;
            for(j=0;j<c->formCount && j<7;++j)if(c->form[j]==hovered)found=1;
            if(found){origin=i;break;}
        }
    }
    if(origin<0 || origin>=gTeamSel->masterCount[team])origin=0;
    for(i=0;i<gTeamSel->masterCount[team];++i){
        TsCell *c;index=(origin+i)%gTeamSel->masterCount[team];c=&gTeamSel->cells[team][index];
        if(c->id<0 || c->id==TS_RANDOM || c->id==TS_LOCKED || c->id==TS_CUSTOM || c->id==TS_EMPTY)continue;
        id=c->formCount>0?c->form[0]:c->id;
        if(id>=0 && ChrGrid_IsSelectable(gTeamSel->cells[team],index) && TeamSel_IsCharaFree(team,id))break;
        id=-1;
    }
    if(id<0)return 0;
    color=m->color;if(ChrTbl_WrapCostume(id,&color)<=0)return 0;
    m->chara=id;m->color=color;m->plate=0;m->form=0;
    m->row=index/TS_COLS;m->col=index%TS_COLS;memset(&m->items,0,sizeof(TsItemSet));
    TagBattle_SetDraftItems(seat,id,m->items.id);
    Port_TagCommand(4,id,color);if(Port_TagError()[0])return 0;
    side->flags&=~(TEAMSEL_SIDE_FORM|TEAMSEL_SIDE_CUSTOM|TEAMSEL_SIDE_PANEL);
    ItemHelp_Close();ItemPanel_Hide(team);side->chara=id;
    side->memberCount=slot+1;side->cur=side->memberCount;side->state=TEAMSEL_ST_TEAM;
    TeamSel_RequestFace(team);TeamSel_SetTeamTex(team);
    Flash_GotoLabel(&gTeamSel->flash[team+5],"fl_color_ok",1);
    Flash_GotoLabel(&gTeamSel->flash[team+3],"fl_out",1);
    Flash_GotoLabel(&gTeamSel->flash[team+1],"fl_in",1);Snd_PlaySe(1,1);
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag select: timeout seat=%d hovered=%d base=%d elapsed=%u\n",seat,hovered,id,Port_TagMenuMillis()-sTagTurnStarted);
    return 1;
}
static void TeamSel_TagCaption(s32 seat,s32 pending){
    char line[128];s32 team,n,turn;
    extern int Port_MenusWide(void);
    f32 left;
    if(gTeamSel->flags&TEAMSEL_STAGE)return; /* native map/music UI owns this page */
    Ps_PageSet();left=Port_MenusWide()?Ps_Left():0;
    Ps_Align(0);Ps_Panel(TBO_KIT_WINDOW,10+left,1,502-left,29,0x78);
    Ps_Text(24+left,7,sTagTransition?"TAG TEAM | PREPARING STAGE SELECT":"TAG TEAM | CHARACTER SELECT",.56f,365-2*left,3);
    if(sTagTurnSeconds>0){
        sprintf(line,"%02d",sTagTurnSeconds);Ps_Align(2);
        Ps_Text(486-left,3,line,.95f,65,sTagTurnSeconds<=10?1:0);
        Ps_Rect(411-left,25,487-left,26,Ps_Rgba(23,50,60,0x80),Ps_Rgba(23,50,60,0x80));
        Ps_Rect(411-left,25,411-left+76.0f*sTagTurnSeconds/60,26,Ps_Rgba(255,213,91,0x80),Ps_Rgba(255,213,91,0x80));
        Ps_Align(0);
    }
    for(team=0;team<2;++team){
        turn=-1;for(n=0;n<Port_TagCapacity();++n)if(Port_TagTeam(n)==team&&!Port_TagConfirmed(n)){turn=n;break;}
        Ps_Panel(TBO_KIT_WINDOW,team?263:10+left,431,team?502-left:249,447,0x78);
        Ps_Copy(line,(char*)PortOnline_Text(Port_TagFreeForAll()?"Fighters: ":team?"RED TEAM: ":"BLUE TEAM: "),128);
        if(turn<0)Ps_Cat(line,(char *)PortOnline_Text("READY"),128);else Ps_Cat(line,(char*)Port_TagPlayerName(turn),128);
        {char pageText[40];sprintf(pageText," | %d-%d/%d",sTagPage[team]*5+1,(sTagPage[team]+1)*5<Port_TagTeamCount(team)?(sTagPage[team]+1)*5:Port_TagTeamCount(team),Port_TagTeamCount(team));Ps_Cat(line,pageText,128);}
        if(turn==seat)Ps_Cat(line,pending?(char *)PortOnline_Text(" | CONFIRMING"):Port_TagCPU(seat)?(char *)PortOnline_Text(" | SELECT CPU"):(char *)PortOnline_Text(" | YOUR TURN"),128);
        Ps_Text(team?273:20+left,433,line,.43f,217-left,team?1:3);
    }
    Ps_Align(1);Ps_Text(256,412,"TRIANGLE: BACK",.42f,180,0);Ps_Align(0);
}

#include "tag_team_select_test.inc"

static void TagCatalog_Exit(void){extern void exit(s32);exit(0);}
s32 TeamSel_TagRun(s32 section){
    TsTeam saved[2];s32 oldPlayers=gTsProgress->players,oldType=gTsProgress->battleType;
    s32 seat=Port_TagSeat(),team=Port_TagTeam(seat),slot=Port_TagTeamIndex(seat)%5,lastSeat=-1;
    s32 i,localTurn=0,pending=0,result=1,exit=0,testFrames=0,startLocal=0,stageFrames=0,chosenStage=0,chosenBgm=2,keepRoom=0,mapPending=0;
    Pad savedPads[2];SaveData *savedSave;
    if(seat<0 || seat>=10 || Port_TagPhase()!=2)return 0;
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];
    TagBattle_ClearDraftItems();
    savedSave=Heap_Alloc(SAVE_SIZE,0x20,0,2);memcpy(savedSave,gSaveData,SAVE_SIZE);Save_UnlockAll(gSaveData);
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));
    for(i=0;i<5;++i){gTsProgress->team[0].member[i].chara=-1;gTsProgress->team[1].member[i].chara=-1;}
    gTsProgress->players=TEAMSEL_PLAYERS_TWO;gTsProgress->battleType=1;TeamSel_Init(section);
    sTagPage[0]=sTagPage[1]=-1;sTagSelectActive=1;sTagFacesHidden=sTagTransition=sTagTurnSeconds=0;TagSelectTest_Reset();
    sTagPublishedStage=sTagPublishedBgm=sTagAppliedStage=sTagAppliedBgm=-1;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_CATALOG")){
        s32 c,j,k;fprintf(stderr,"catalog base-file=%d stages=%d\n",gProgress->baseFile,gTeamSel->stageCount);
        for(c=0;c<gTeamSel->masterCount[0];++c){TsCell *v=&gTeamSel->cells[0][c];
            fprintf(stderr,"catalog grid cell=%d id=%d forms=",c,v->id);for(j=0;j<v->formCount&&j<7;++j)fprintf(stderr,"%d,",v->form[j]);fprintf(stderr,"\n");}
        for(c=0;c<250;++c){
            fprintf(stderr,"catalog fighter=%d costumes=%d name=",c,ChrTbl_WrapCostume(c,NULL));
            for(k=0;k<2;++k){u8 *text=k?gTeamSel->formText:gTeamSel->nameText;u16 *line=(u16 *)(text+(((u32 *)text)[c+1]&~3));
                for(j=0;line[j]&&j<100;++j){u32 ch=line[j];if(ch==0xFEFF)continue;if(ch<128)fputc(ch,stderr);else if(ch<2048){fputc(0xC0|(ch>>6),stderr);fputc(0x80|(ch&63),stderr);}else{fputc(0xE0|(ch>>12),stderr);fputc(0x80|((ch>>6)&63),stderr);fputc(0x80|(ch&63),stderr);}}
                fprintf(stderr," | ");}
            fprintf(stderr,"\n");
            if(c==104||c==154){u8 *group=(u8 *)gTeamSel->costumeText+((u32 *)gTeamSel->costumeText)[c+1];
                for(k=0;k<ChrTbl_WrapCostume(c,NULL);++k){u16 *line=(u16 *)(group+(((u32 *)group)[k+1]&~3));fprintf(stderr,"catalog costume chara=%d costume=%d text=",c,k);
                    for(j=0;line[j]&&j<100;++j)if(line[j]<128)fputc(line[j],stderr);fprintf(stderr,"\n");}}
        }
        fflush(stderr);TagCatalog_Exit();
    }
    ColorFade_StartIn(0,0,0,0x14);
    while(!exit || !ColorFade_IsOutDone() || (keepRoom && Port_TagNetwork() && Port_TagDraftPending() && !Port_TagError()[0])){
        s32 ownTurn=0,before=0,draft=-1,canInput;
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        Pad_Update();Snd_Update();ColorFade_Update();Port_TagPoll();
        if(!exit && Port_TagPhase()!=2){if(Port_TagNetwork()&&Port_TagPhase()==3)startLocal=1;keepRoom=Port_TagPhase()==1;exit=1;ColorFade_StartOut(0,0,0,0x14);}
        if(!exit){
            TeamSel_TagSetRoster();
            if(pending && (Port_TagError()[0] || Port_TagConfirmed(seat)))pending=0;
            draft=Port_TagDraftSeat();
            if(!sTagTransition && !(gTeamSel->flags&TEAMSEL_STAGE) && !mapPending && TeamSel_TagComplete())TeamSel_TagMapTransition();
            ownTurn=!pending && !sTagTransition && !(gTeamSel->flags&TEAMSEL_STAGE) && draft>=0;
            if(ownTurn && draft!=seat){seat=draft;team=Port_TagTeam(seat);slot=Port_TagTeamIndex(seat)%5;localTurn=0;}
            if(ownTurn && !localTurn){
                TeamSelSide *side=gTeamSel->side[team];side->cur=slot;side->state=TEAMSEL_ST_GRID;
                if(Port_TagCPU(seat)){
                    s32 cell;for(cell=0;cell<gTeamSel->masterCount[team];++cell){
                        s32 id=gTeamSel->cells[team][cell].id;
                        if(id<0 || id==TS_RANDOM || id==TS_LOCKED || id==TS_CUSTOM || id==TS_EMPTY)continue;
                        if(!ChrGrid_IsSelectable(gTeamSel->cells[team],cell)||!TeamSel_IsCharaFree(team,id))continue;
                        side->member[slot].col=cell%TS_COLS;side->member[slot].row=cell/TS_COLS;side->member[slot].color=0;break;
                    }
                }
                TagSelectTest_Position(seat);
                TagLongVideo_Position(seat);
                TeamSel_SetChips(team);side->chara=side->chip[0][side->member[slot].col];TeamSel_RequestFace(team);
                Flash_GotoLabel(&gTeamSel->flash[team+1],"fl_out",1);Flash_GotoLabel(&gTeamSel->flash[team+3],"fl_in",1);
                TeamSel_ClipGoto(team+3,team,TEAMSEL_CLIP_CHIP,"fl_on_start");
            }
        }
        localTurn=ownTurn;canInput=ColorFade_IsInDone() && (gTeamSel->flash[0].flags&MFLASH_PAD);
        if(ownTurn && canInput && lastSeat!=seat){lastSeat=seat;sTagTurnStarted=Port_TagMenuMillis();sTagTurnSeconds=60;
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag select: turn seat=%d timeout=60000ms\n",seat);
        }
        if(!exit && canInput)TagSelectTest_Input(seat,ownTurn);
        if(!exit && canInput && (gPad[0].gamePressed&PADG_SELECT)){
            keepRoom=Port_TagCancelSelect();exit=1;ColorFade_StartOut(0,0,0,0x14);
        }
        if(!exit && canInput && (gPad[0].gamePressed&PADG_TRIANGLE)){
            if((gTeamSel->flags&TEAMSEL_STAGE) && gTeamSel->stage->state==TEAMSEL_STAGE_GRID){
                if((!Port_TagNetwork()||Port_TagHost())&&TeamSel_TagRestartDraft()){lastSeat=-1;localTurn=0;ownTurn=0;gPad[0].gamePressed=0;}
            }else if(!(gTeamSel->flags&TEAMSEL_STAGE) && (sTagTransition || !ownTurn || gTeamSel->side[team]->state==TEAMSEL_ST_GRID)){
                keepRoom=Port_TagCancelSelect();exit=1;ColorFade_StartOut(0,0,0,0x14);Snd_PlaySe(1,2);
            }
        }
        savedPads[0]=gPad[0];savedPads[1]=gPad[1];
        memset(&gPad[0].held,0,sizeof(Pad)-0x148);memset(&gPad[1].held,0,sizeof(Pad)-0x148);
        if(!exit && ownTurn){
            gPad[team]=savedPads[0];
            if(getenv("BT4_TAG_DRAFT_TEST") && !TagSelectTest_HoldSeat(seat) && ++testFrames%40==0){gPad[team].gamePressed=PADG_CROSS;gPad[team].gameHeld=PADG_CROSS;gPad[team].gameRepeat=PADG_CROSS;}
            TagLongVideo_Input(seat);
            /* Local play uses the native three saved sets. Network rooms
               keep a uniform draft and receive no local combat items. */
            if(Port_TagNetwork()){
                gTeamSel->side[team]->member[slot].plate=0;gTeamSel->plateCount[team]=1;
            }
        }
        before=gTeamSel->side[team]->memberCount;TeamSel_Update();
        if(!exit && ownTurn && canInput)TeamSel_Input(&result);
        if(!exit && ownTurn && gTeamSel->side[team]->memberCount>before){
            TsMember *m=&gTeamSel->side[team]->member[slot];
            if(Port_TagNetwork())memset(&m->items,0,sizeof(TsItemSet));
            TagBattle_SetDraftItems(seat,m->chara,m->items.id);
            Port_TagCommand(4,m->chara,m->color);pending=1;
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag draft: native confirmation seat=%d chara=%d costume=%d\n",seat,m->chara,m->color);
        }else if(!exit && ownTurn && canInput && lastSeat==seat){
            unsigned elapsed=Port_TagMenuMillis()-sTagTurnStarted;
            sTagTurnSeconds=elapsed>=60000?0:(60000-elapsed+999)/1000;
            if(elapsed>=60000){
                if(TeamSel_TagAutoConfirm(seat))pending=1;
                else{keepRoom=Port_TagCancelSelect();exit=1;ColorFade_StartOut(0,0,0,0x14);}
            }
        }
        if(!exit && sTagTransition && --sTagTransition==0){TeamSel_TagOpenMap();stageFrames=0;}
        if(!exit && (gTeamSel->flags&TEAMSEL_STAGE)){
            gPad[0]=savedPads[0];
            if(getenv("BT4_TAG_LOCAL_TEST") && !getenv("BT4_TAG_STAGE_PREVIEW") && (!sTagSelectTest || !strcmp(sTagSelectTest,"timeout") || !strcmp(sTagSelectTest,"invalid")) && ++stageFrames>(getenv("BT4_TAG_WIDE_AUDIT")?100:45)){gPad[0].gamePressed=PADG_CROSS;gPad[0].gameRepeat=0;}
            TeamSel_TagApplyArena();
            if(ColorFade_IsInDone()&&(!Port_TagNetwork()||Port_TagHost()))TeamSel_Input(&result);
            TeamSel_TagPublishArena();
            if((gTeamSel->flags&TEAMSEL_DECIDED)&&!mapPending){
                chosenStage=gTeamSel->stage->stage;
                if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_TEST_STAGE"))chosenStage=atoi(getenv("BT4_TAG_TEST_STAGE"));
                if(chosenStage==TS_STAGE_RANDOM){do{chosenStage=gTeamSel->stageIds[Rand_Range(gTeamSel->stageCount)];}while(chosenStage==TS_STAGE_RANDOM||chosenStage<0||chosenStage>=0x63);}
                chosenBgm=gTeamSel->bgmIds[gTeamSel->stage->bgm];if(chosenBgm==TS_BGM_RANDOM)chosenBgm=TS_RANDOM_BGM();
                if(Port_TagNetwork()){Port_TagArena(chosenStage,chosenBgm,1);Port_TagCommand(5,0,0);mapPending=1;}
                else{startLocal=1;exit=1;ColorFade_StartOut(0,0,0,0x14);}
                fprintf(stderr,"tag stage: chosen=%d bgm=%d native-preview=1\n",chosenStage,chosenBgm);
            }
        }
        gPad[0]=savedPads[0];gPad[1]=savedPads[1];
        TeamSel_Draw();Font_FlushAll();TeamSel_TagCaption(seat,pending);
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_WIDE_AUDIT")&&ColorFade_IsInDone()){
            static s32 frames[3];
            s32 kind=(gTeamSel->flags&TEAMSEL_STAGE)?2:gTeamSel->side[team]->state==TEAMSEL_ST_COLOR?1:0;
            if(++frames[kind]==(kind==2?80:kind==1?60:20)){extern void Port_TagUiCapture(s32);Port_TagUiCapture(70+kind);}
        }
        ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE || gTeamSel->stageState!=TEAMSEL_LOAD_IDLE){TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();}
    TeamSel_Term();Dma_ResetBuffers();sTagSelectActive=sTagFacesHidden=0;
    if(startLocal){extern void TagBattle_SetArena(s32,s32);TagBattle_SetArena(chosenStage,chosenBgm);startLocal=TagBattle_ConfigureLocal();}
    if(!startLocal && !keepRoom)Port_TagClose();
    if(getenv("BT4_RENDER_TEST") && !startLocal)fprintf(stderr,"tag select: returned from draft room-phase=%d retained=%d\n",Port_TagPhase(),keepRoom);
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=oldPlayers;gTsProgress->battleType=oldType;
    memcpy(gSaveData,savedSave,SAVE_SIZE);Heap_Free(savedSave);return startLocal;
}

