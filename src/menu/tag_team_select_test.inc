/* Hidden native input fixtures. No operating-system input is generated. */
static const char *sTagSelectTest;
static s32 sTagTestSeat;
static s32 sTagTestTick,sTagTestPhase,sTagTestBase,sTagTestMapTick;
static s32 sTagLongTick;
static s32 TagLongVideo_Chara(s32 seat){
    static s32 roster[2][5]={{122,35,70,21,101},{151,81,229,154,78}};
    const char *custom=getenv("BT4_RENDER_TEST")?getenv("BT4_TAG_TEST_ROSTER"):NULL;
    if(custom){s32 n=0;while(n<seat&&*custom){if(*custom++==',')++n;}if(n==seat&&*custom)return atoi(custom);}
    return roster[Port_TagTeam(seat)][Port_TagTeamIndex(seat)];
}
static void TagLongVideo_Position(s32 seat){
    s32 cell,form,team=Port_TagTeam(seat),wanted;
    TsMember *member;
    if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_TAG_LONG_VIDEO"))return;
    wanted=TagLongVideo_Chara(seat);member=&gTeamSel->side[team]->member[Port_TagTeamIndex(seat)%5];sTagLongTick=0;
    for(cell=0;cell<gTeamSel->masterCount[team];++cell){
        TsCell *entry=&gTeamSel->cells[team][cell];s32 found=entry->id==wanted;
        for(form=0;form<entry->formCount;++form)if(entry->form[form]==wanted)found=1;
        if(found){member->row=cell/TS_COLS;member->col=cell%TS_COLS;member->color=0;break;}
    }
}
static void TagLongVideo_Input(s32 seat){
    s32 team=Port_TagTeam(seat);TeamSelSide *side=gTeamSel->side[team];
    if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_TAG_LONG_VIDEO"))return;
    gPad[team].gamePressed=gPad[team].gameHeld=gPad[team].gameRepeat=0;
    if(getenv("BT4_TAG_WIDE_AUDIT")&&side->state==TEAMSEL_ST_COLOR){
        static s32 hold;if(hold++<80)return;
    }
    if(++sTagLongTick%28)return;
    if(side->state==TEAMSEL_ST_FORM && side->chara!=TagLongVideo_Chara(seat))gPad[team].gameRepeat=PADG_RIGHT;
    else gPad[team].gamePressed=PADG_CROSS;
}
static void TagSelectTest_Reset(void){
    sTagSelectTest=getenv("BT4_RENDER_TEST")?getenv("BT4_TAG_SELECT_TEST"):NULL;
    sTagTestTick=sTagTestPhase=sTagTestMapTick=0;sTagTestBase=-1;sTagTestSeat=getenv("BT4_TAG_TIMEOUT_SEAT")?atoi(getenv("BT4_TAG_TIMEOUT_SEAT")):0;
}
static s32 TagSelectTest_HoldSeat(s32 seat){return sTagSelectTest && strcmp(sTagSelectTest,"map-back") && strcmp(sTagSelectTest,"music-preview") && seat==sTagTestSeat && sTagTestPhase<20;}
static void TagSelectTest_Position(s32 seat){
    s32 i;if(!sTagSelectTest || seat!=sTagTestSeat || sTagTestPhase>=20)return;
    for(i=0;i<gTeamSel->masterCount[Port_TagTeam(seat)];++i){
        TsCell *c=&gTeamSel->cells[Port_TagTeam(seat)][i];s32 pick=0;
        if(sTagSelectTest[0]=='t' || !strcmp(sTagSelectTest,"form-back"))pick=c->formCount>1 && TeamSel_IsCharaFree(Port_TagTeam(seat),c->form[0]);
        if(!strcmp(sTagSelectTest,"invalid"))pick=c->id==TS_RANDOM;
        if(pick){TsMember *m=&gTeamSel->side[Port_TagTeam(seat)]->member[Port_TagTeamIndex(seat)%5];m->row=i/TS_COLS;m->col=i%TS_COLS;sTagTestBase=c->formCount?c->form[0]:c->id;
            fprintf(stderr,"tag select fixture: cell=%d base=%d forms=%d\n",i,sTagTestBase,c->formCount);break;}
    }
}
static void TagSelectTest_Input(s32 seat,s32 ownTurn){
    s32 state=gTeamSel->side[Port_TagTeam(seat)]->state;
    if(!sTagSelectTest)return;
    if(gTeamSel->flags&TEAMSEL_STAGE){
        if(strcmp(sTagSelectTest,"map-back") && strcmp(sTagSelectTest,"music-preview"))return;
        ++sTagTestMapTick;
        if(gTeamSel->stage->state==TEAMSEL_STAGE_GRID && sTagTestPhase<2 && sTagTestMapTick%12==0){
            if(gTeamSel->stage->col!=TS_STAGE_COLS)gPad[0].gameRepeat=PADG_RIGHT;
            else{gPad[0].gamePressed=PADG_CROSS;sTagTestPhase=1;}
        }else if(gTeamSel->stage->state==TEAMSEL_STAGE_BGM && sTagTestPhase==1 && sTagTestMapTick>150){
            if(sTagTestMapTick==151)fprintf(stderr,"tag select fixture: music list open without character layers\n");
            if(!strcmp(sTagSelectTest,"map-back") && sTagTestMapTick==170){gPad[0].gamePressed=PADG_TRIANGLE;sTagTestPhase=2;}
            if(!strcmp(sTagSelectTest,"music-preview") && sTagTestMapTick==170)gPad[0].gameRepeat=PADG_DOWN;
        }else if(gTeamSel->stage->state==TEAMSEL_STAGE_GRID && sTagTestPhase==2){
            gPad[0].gamePressed=PADG_TRIANGLE;sTagTestPhase=20;
        }
        return;
    }
    if(!ownTurn || seat!=sTagTestSeat)return;
    ++sTagTestTick;
    if(sTagTestPhase>=20 && !strcmp(sTagSelectTest,"map-back")){
        gPad[0].gamePressed=PADG_TRIANGLE;
        fprintf(stderr,"tag select fixture: returned draft editable\n");return;
    }
    if(!strcmp(sTagSelectTest,"cancel")){
        if(sTagTestTick==40)gPad[0].gamePressed=PADG_TRIANGLE;
    }else if(!strcmp(sTagSelectTest,"form-back")){
        if(sTagTestTick==30)gPad[0].gamePressed=PADG_CROSS;
        if(sTagTestTick==65 && state==TEAMSEL_ST_FORM)gPad[0].gamePressed=PADG_TRIANGLE;
        if(sTagTestTick==95 && state==TEAMSEL_ST_GRID){fprintf(stderr,"tag select fixture: form cancel returned grid\n");gPad[0].gamePressed=PADG_TRIANGLE;}
    }else if(!strcmp(sTagSelectTest,"timeout")){
        if(sTagTestTick==30)gPad[0].gamePressed=PADG_CROSS;
        if(sTagTestTick==60 && state==TEAMSEL_ST_FORM)gPad[0].gameRepeat=PADG_RIGHT;
        if(sTagTestTick==90){Port_TagMenuTestAdvance(59999);fprintf(stderr,"tag select fixture: selected transformation=%d expected-base=%d\n",gTeamSel->side[Port_TagTeam(seat)]->chara,sTagTestBase);}
        if(sTagTestTick==100){fprintf(stderr,"tag select fixture: boundary 59999 confirmed=%d\n",Port_TagConfirmed(seat));Port_TagMenuTestAdvance(1);sTagTestPhase=20;}
    }else if(!strcmp(sTagSelectTest,"invalid") && sTagTestTick==30){Port_TagMenuTestAdvance(60000);sTagTestPhase=20;}
}
