#include "../../port/src/gs/online_language.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "battle/tag_team_hud_layout.h"
#include "menu/tag_team_radar_art.h"
extern f32 sqrtf(f32);
extern void Port_SettingSave(const char *,int),Port_SettingsWrite(void);
#define OPT_ITEM_TAG_HUD 23 /* virtual Screen row; leaves the original save/state IDs intact */
extern void Flash_ClipGetPos(MFlash *,MFlashRef *,s32 *,s32 *);
extern f32 Flash_ClipGetAlpha(MFlash *,MFlashRef *);
extern void MsgWin_PortDrawString(u16 *);
extern void Flash_ClipSetCallbackC(MFlash *,MFlashRef *,void *,void *);
extern s32 PortTbo_TextId(const char *,s32),PortTbo_TextInfo(s32,s32);
extern void Gfx_AddDefaultEnv(void);
static void TagHudScreen_Caption(void *arg,f32 *prop){
    s32 id=PortTbo_TextId("Tag Team HUD",0),on=gOption->cursor==OPT_ITEM_TAG_HUD;
    s32 alpha=(s32)(128*prop[24]),i;f32 ky=18.0f/1000,kx=ky*.857f;
    f32 x0,x1,y0,h;u32 top,bottom;
    static const f32 edge[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-.7f,-.7f},{.7f,-.7f},{-.7f,.7f},{.7f,.7f}};
    {extern f32 Port_MenuReadableScale(void);f32 scale=Port_MenuReadableScale();ky*=scale;kx*=scale;}
    if(id<0 || alpha<=0)return;if(alpha>128)alpha=128;
    x0=prop[14]+82-PortTbo_TextInfo(id,4)*kx;
    x1=x0+PortTbo_TextInfo(id,1)*kx;
    /* The native clip gives 4:3 page coordinates; TBO textures bypass the
     * renderer's native-menu transform, so map the caption exactly once. */
    {extern int Port_MenusWide(void),Port_PageAspectMilli(void),Port_MenuPartNow(void);
     if(Port_MenusWide()){
        int part=Port_MenuPartNow();f32 scale=1333.333333f/Port_PageAspectMilli();
        f32 pivot=part==2||part==5?0:part==3||part==6?512:256;
        if(scale<1){x0=pivot+(x0-pivot)*scale;x1=pivot+(x1-pivot)*scale;}
     }}
    y0=prop[17]+24-PortTbo_TextInfo(id,3)*ky;h=PortTbo_TextInfo(id,2)*ky;
    for(i=0;i<8;++i)Ps_SpriteScreen(id,x0+edge[i][0]*.65f,y0+edge[i][1]*.65f,x1+edge[i][0]*.65f,y0+h+edge[i][1]*.65f,0,.5f,1,1,0,alpha);
    top=on?Ps_Rgba(255,247,179,255):Ps_Rgba(255,255,255,255);
    bottom=on?Ps_Rgba(255,188,46,255):Ps_Rgba(226,229,226,255);
    for(i=0;i<3;++i){f32 a=i/3.0f,b=(i+1)/3.0f;
        Ps_SpriteScreen(id,x0,y0+h*a,x1,y0+h*b,0,a*.5f,1,b*.5f,Ps_Tint(Ps_Mix(top,bottom,(a+b)*.5f)),alpha);
    }
    Gfx_AddDefaultEnv();
}
static void TagHudScreen_Restore(MFlash *flash){
    MFlashRef ref;
    Flash_FindLabel(flash,"mc_menu_plate_4","mc_menu_text_off",&ref);
    Flash_ClipSetFlags(flash,&ref,0x800,0);Flash_ClipSetCallbackC(flash,&ref,NULL,NULL);
    Flash_FindLabel(flash,"mc_menu_plate_4","mc_menu_text_on",&ref);
    Flash_ClipSetFlags(flash,&ref,0x800,0);Flash_ClipSetCallbackC(flash,&ref,NULL,NULL);
}
static void TagHudScreen_Prepare(MFlash *flash){
    MFlashRef ref;
    /* The original fourth animated plate supplies its own highlight and slide. */
    Flash_FindLabel(flash,NULL,"mc_menu_plate_4",&ref);Flash_ClipSetFlags(flash,&ref,2,1);
    Flash_FindLabel(flash,"mc_menu_plate_4","mc_menu_text_off",&ref);
    Flash_ClipSetFlags(flash,&ref,0x800,1);Flash_ClipSetCallbackC(flash,&ref,TagHudScreen_Caption,NULL);
    Flash_FindLabel(flash,"mc_menu_plate_4","mc_menu_text_on",&ref);
    Flash_ClipSetFlags(flash,&ref,0x800,1);Flash_ClipSetCallbackC(flash,&ref,TagHudScreen_Caption,NULL);
}
static void TagHudScreen_Description(void){
    static u16 line[180];
    extern int Port_GameProfileEnglish(void);
    const char *text=Port_GameProfileEnglish()?"Arrange the Tag Team HUD\nover a battle preview.":"Organiza el HUD de Tag Team\nsobre una vista de combate.";
    s32 i;line[0]=0xFEFF;for(i=0;text[i];++i)line[i+1]=(u8)text[i];line[i+1]=0;
    MsgWin_PortDrawString(line);
}
static s32 sHudEditorOn,sHudEditorBlock,sHudEditorFrames,sHudVideoDone,sHudMoveFrames;
s32 TagHudEditor_RecordDone(void){return sHudVideoDone;}
static TagHudRect sHudEditorRects[4];
static char *sHudEditorNames[4]={"Allies","Targets","Radar","Team Score"};
static void TagHudScreen_TestInput(void){
    static s32 tick,phase,reported;const char *test=getenv("BT4_HUD_MENU_TEST");
    if(!test || !getenv("BT4_RENDER_TEST"))return;
    if(sHudEditorOn)return;
    if(!(gOption->flash[0].flags&MFLASH_PAD))return;
    ++tick;if(tick%15)return;
    if(getenv("BT4_TAG_LONG_VIDEO") && sHudVideoDone){
        gPad[0].gamePressed=PADG_TRIANGLE;return;
    }
    if(gOption->state==OPT_ST_TOP){
        if(gOption->cursor==OPT_ITEM_SCREEN)gPad[0].gamePressed=PADG_CROSS;
        else gPad[0].gameRepeat=PADG_DOWN;
    }else if(gOption->state==OPT_ITEM_SCREEN){
        if(test[0]=='v' && phase){
            if(phase==1 && gOption->cursor==OPT_ITEM_TAG_HUD){gPad[0].gameRepeat=PADG_UP;phase=2;}
            else if(phase==2 && gOption->cursor==OPT_ITEM_ADJUST){gPad[0].gameRepeat=PADG_DOWN;phase=3;}
            else if(phase==3 && gOption->cursor==OPT_ITEM_TAG_HUD){gPad[0].gameRepeat=PADG_DOWN;phase=4;}
            else if(phase==4 && gOption->cursor==OPT_ITEM_SCR_RESET){gPad[0].gameRepeat=PADG_UP;phase=5;}
            else if(phase==5 && gOption->cursor==OPT_ITEM_TAG_HUD){fprintf(stderr,"tag hud menu: open cancel up down reset navigation PASS\n");phase=6;}
            return;
        }
        if(gOption->cursor==OPT_ITEM_TAG_HUD){
            if(!reported){fprintf(stderr,"tag hud menu: Screen fourth row selected\n");reported=1;
                {extern void Port_TagUiCapture(s32);Port_TagUiCapture(40);}}
            if((test[0]=='o'||test[0]=='v') && !phase){gPad[0].gamePressed=PADG_CROSS;phase=1;}
        }else gPad[0].gameRepeat=PADG_DOWN;
    }
}
static void TagHudEditor_Open(void){
    sHudEditorOn=1;sHudEditorBlock=0;sHudEditorFrames=0;sHudMoveFrames=0;
    Ps_PageSet();Ps_SetShift(0);
    TagHud_ReadRects(sHudEditorRects,Ps_Left());
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag hud editor: opened page=%d cursor=%d\n",gOption->page,gOption->cursor);
}
static void TagHudEditor_Input(void){
    TagHudRect defaults[4],*b=&sHudEditorRects[sHudEditorBlock];
    s32 step=2,i;u32 move;
    const char *test=getenv("BT4_HUD_EDITOR_TEST");
    ++sHudEditorFrames;
    if(getenv("BT4_RENDER_TEST") && sHudEditorFrames==30){extern void Port_TagUiCapture(s32);Port_TagUiCapture(41);}
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_LONG_VIDEO")){
        s32 f=sHudEditorFrames;
        if(f==150||f==330||f==510)gPad[0].gamePressed=PADG_R1;
        if(f>=90&&f<110)gPad[0].gameRepeat=PADG_UP;
        if(f>=240&&f<255)gPad[0].gameRepeat=PADG_DOWN;
        if(f>=420&&f<435)gPad[0].gameRepeat=PADG_LEFT;
        if(f>=600&&f<610)gPad[0].gameRepeat=PADG_DOWN;
        if(f==780){gPad[0].gamePressed=PADG_CROSS;sHudVideoDone=1;}
    }
    if(getenv("BT4_HUD_MENU_TEST") && getenv("BT4_HUD_MENU_TEST")[0]=='v' && sHudEditorFrames==30)gPad[0].gamePressed=PADG_TRIANGLE;
    if(test && test[0]=='m'){
        if(sHudEditorFrames>=35 && sHudEditorFrames<45)gPad[0].gameRepeat=PADG_RIGHT;
        if(sHudEditorFrames==50)gPad[0].gamePressed=PADG_R1;
        if(sHudEditorFrames>=55 && sHudEditorFrames<65)gPad[0].gameRepeat=PADG_DOWN;
        if(sHudEditorFrames==90)gPad[0].gamePressed=PADG_CROSS;
    }
    if(test && test[0]=='s'){
        if(sHudEditorFrames>=35 && sHudEditorFrames<70)gPad[0].gameHeld=PADG_RIGHT2;
        if(sHudEditorFrames==80)gPad[0].gamePressed=PADG_R1;
        if(sHudEditorFrames>=90 && sHudEditorFrames<100)gPad[0].gameHeld=PADG_DOWN|PADG_L2;
        if(sHudEditorFrames==120)gPad[0].gamePressed=PADG_CROSS;
    }
    if(gPad[0].gamePressed&PADG_R1)sHudEditorBlock=(sHudEditorBlock+1)%4;
    if(gPad[0].gamePressed&PADG_L1)sHudEditorBlock=(sHudEditorBlock+3)%4;
    b=&sHudEditorRects[sHudEditorBlock];
    move=gPad[0].gameHeld|gPad[0].gameRepeat;
    if(move&PADG_LEFT2)move|=PADG_LEFT;
    if(move&PADG_RIGHT2)move|=PADG_RIGHT;
    if(move&PADG_UP2)move|=PADG_UP;
    if(move&PADG_DOWN2)move|=PADG_DOWN;
    move&=PADG_LEFT|PADG_RIGHT|PADG_UP|PADG_DOWN;
    if(move)++sHudMoveFrames;else sHudMoveFrames=0;
    if(sHudMoveFrames>=18)step=4;
    if(gPad[0].gameHeld&PADG_L2)step=8;
    if(gPad[0].gamePressed&(PADG_L1|PADG_R1))sHudMoveFrames=0;
    if(move&PADG_LEFT)b->x-=step;
    if(move&PADG_RIGHT)b->x+=step;
    if(move&PADG_UP)b->y-=step;
    if(move&PADG_DOWN)b->y+=step;
    TagHud_ClampRect(b,Ps_Left());TagHud_DefaultRects(defaults,Ps_Left());
    if(gPad[0].gamePressed&PADG_SQUARE)*b=defaults[sHudEditorBlock];
    if(gPad[0].gamePressed&PADG_SELECT)for(i=0;i<4;++i)sHudEditorRects[i]=defaults[i];
    if(gPad[0].gamePressed&(PADG_CROSS|PADG_START)){
        for(i=0;i<4;++i){
            f32 dx=sHudEditorRects[i].x-defaults[i].x,dy=sHudEditorRects[i].y-defaults[i].y;
            Port_SettingSave(sTagHudKeys[i][0],(s32)(dx+(dx>=0?.5f:-.5f)));
            Port_SettingSave(sTagHudKeys[i][1],(s32)(dy+(dy>=0?.5f:-.5f)));
        }
        Port_SettingsWrite();sHudEditorOn=0;
        if(test)fprintf(stderr,"tag hud editor: saved native settings\n");
    }else if(gPad[0].gamePressed&PADG_TRIANGLE)sHudEditorOn=0;
}
static void TagHudEditor_Frame(TagHudRect *b,s32 selected){
    u32 c=selected?Ps_Rgba(255,211,90,0x80):Ps_Rgba(102,157,179,0x55);
    Ps_Rect(b->x-2,b->y-2,b->x+b->w+2,b->y-1,c,c);
    Ps_Rect(b->x-2,b->y+b->h+1,b->x+b->w+2,b->y+b->h+2,c,c);
    Ps_Rect(b->x-2,b->y-1,b->x-1,b->y+b->h+1,c,c);
    Ps_Rect(b->x+b->w+1,b->y-1,b->x+b->w+2,b->y+b->h+1,c,c);
}
static void TagHudEditor_Draw(void){
    s32 i,j;f32 x,y;TagHudRect *b;char line[80];
    Ps_PageSet();Ps_SetShift(0);Ps_Align(0);
    /* Ps_* already maps this editor to the window. Its untextured bars and
     * frames must bypass the additional native Flash menu transform too. */
    Port_MenuFill(1);
    Ps_Sprite(TBO_TAG_HUD_PREVIEW,Ps_Left(),0,512-Ps_Left(),448,0,0,1,1,PS_PLAIN,0x80);
    for(i=0;i<4;++i){
        b=&sHudEditorRects[i];x=b->x;y=b->y;
        if(i==TAG_HUD_ALLIES){
            Ps_Text(x+3,y,"ALLIES",0.43f,90,3);
            for(j=0;j<4;++j){f32 sy=y+18+j*32;
                Ps_Rect(x,sy,x+104,sy+27,Ps_Rgba(27,48,61,0x68),Ps_Rgba(8,19,30,0x50));
                Ps_Sprite(TBO_TAG_HUD_FACES,x+1,sy-1,x+27,sy+28,(j+1)/10.0f,0,(j+2)/10.0f,1,PS_PLAIN,0x80);
                sprintf(line,"CPU %02d",j+2);Ps_Text(x+30,sy+1,line,.48f,66,0);
                Ps_Rect(x+30,sy+15,x+97,sy+19,Ps_Rgba(122,246,95,0x80),Ps_Rgba(29,128,36,0x80));
                Ps_Rect(x+30,sy+22,x+87,sy+23,Ps_Rgba(110,237,255,0x80),Ps_Rgba(22,115,162,0x80));
            }
        }else if(i==TAG_HUD_TARGETS){
            Ps_Align(2);Ps_Text(x+140,y,"TARGET 01",.46f,140,1);Ps_Align(0);
            for(j=0;j<5;++j){f32 sx=x+j*28;
                Ps_Sprite(TBO_TAG_HUD_FACES,sx,y+16,sx+26,y+45,(j+5)/10.0f,0,(j+6)/10.0f,1,PS_PLAIN,0x80);
                Ps_Rect(sx+2,y+47,sx+24,y+50,Ps_Rgba(17,37,43,0x80),Ps_Rgba(17,37,43,0x80));
                Ps_Rect(sx+2,y+47,sx+24-j*3,y+50,Ps_Rgba(122,246,95,0x80),Ps_Rgba(29,128,36,0x80));
                if(j==0)Ps_Rect(sx+3,y+53,sx+23,y+54.5f,Ps_Rgba(255,213,91,0x80),Ps_Rgba(255,213,91,0x80));
            }
        }else if(i==TAG_HUD_RADAR){
            TrRadarFace(x+41,y+61);
            for(j=0;j<9;++j){
                f32 dx=(j%3-1)*17.0f,dy=(j/3-1)*18.0f;
                u32 color=j<4?Ps_Rgba(104,230,255,0x80):Ps_Rgba(255,118,94,0x80);
                Ps_Disc(x+41+dx*PS_UNIT_X,y+61+dy,1.7f,color,color);
            }
            TrRadarArrow(x+41,y+61,0,-1,3.4f,Ps_Rgba(239,43,29,0x80));
            Ps_Align(1);Ps_Text(x+41,y+109,"L3 CHANGE TARGET",.39f,116,0);Ps_Align(0);
        }else{
            Ps_Align(1);Ps_Text(x+40,y,"TAG TEAM",.44f,80,1);Ps_Text(x+40,y+13,"5 : 5",.54f,64,0);Ps_Align(0);
        }
        TagHudEditor_Frame(b,i==sHudEditorBlock);
    }
    Ps_Panel(TBO_KIT_WINDOW,156,352,356,395,0x78);
    Ps_Align(1);Ps_Text(256,356,"Tag Team HUD",.65f,190,2);
    Ps_Copy(line,"L1 / R1: ",80);Ps_Cat(line,(char *)PortOnline_Text(sHudEditorNames[sHudEditorBlock]),80);Ps_Text(256,378,line,.42f,190,3);
    Ps_Text(256,399,"D-PAD: MOVE | L2: FASTER | SELECT: RESET ALL",.40f,490,0);Ps_Align(0);
    Ps_PromptsClear();Ps_Prompt(0,"Save");Ps_Prompt(1,"Reset Panel");Ps_Prompt(2,"Cancel");Ps_PromptsDraw();
    Port_MenuFill(0);
}
