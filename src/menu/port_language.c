#include "common.h"
#include "sys/pad.h"
#include "sys/dialog.h"
#ifdef PORT
#include "../../port/src/gs/game_profile.h"
#include "../../port/src/gs/menu_translation.h"
#include <stdlib.h>
extern int gPortOverlayOpen;
extern void MenuText_BootLanguageChosen(void);
extern void Ui_LanguageChosen(const char *);
extern void PortA11y_Say(const char *,int);
static u16 sLabels[2][16]={{'E','s','p','a',0xF1,'o','l',0},{'E','n','g','l','i','s','h',0}};
extern void Flash_FindLabel(DialogFlash *,char *,char *,DialogFlashRef *);
extern f32 Flash_ClipGetAlpha(DialogFlash *,DialogFlashRef *);

extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetAlign(s32);
extern void Font_SetScale(f32);
extern void Font_SetScaleXY(f32,f32);
extern void Font_SetShadowMode(s32);
extern void Font_SetShadowColor(u32);
extern void Font_SetShadowOffset(s32,s32);
extern void Font_SetColorRGBA(s32,s32,s32,s32);
extern s32 Font_GetWidth(u16 *);
extern void Font_PrintAt(s32,s32,u16 *);
extern void Font_FlushAll(void);
extern int MenuText_GetSharedFont(void);
extern void MenuText_SetSharedFont(int);
extern void Port_MenuPage(int);
static u16 sLines[6][128];
static s32 sEmphasizedLine=-1;
/* Keep strings alive through Font_FlushAll. The glyphs come directly from
 * BT4's boot archive, using the same outline and alpha as the card dialog. */
static void original_line(s32 line,const char *text,s32 x,s32 y,f32 scaleX,f32 scaleY,s32 width,s32 alpha){
    s32 i,measured;f32 fit=1.0f;
    for(i=0;text[i]&&i<127;i++)sLines[line][i]=(u8)text[i];sLines[line][i]=0;
    Font_PushStyle();Font_SetAlign(1);Font_SetScale(1.0f);Font_SetScaleXY(scaleX,scaleY);
    measured=(s32)((f32)Font_GetWidth(sLines[line])*scaleX);
    if(measured>width)fit=(f32)width/(f32)measured;
    Font_SetScaleXY(scaleX*fit,scaleY);
    Font_SetShadowMode(2);Font_SetShadowOffset(1,2);
    Font_SetShadowColor(((u32)(alpha/2)<<24)|0x202020);
    Font_SetColorRGBA(255,line==sEmphasizedLine?230:255,line==sEmphasizedLine?96:255,alpha);
    Font_PrintAt(x,y,sLines[line]);Font_PopStyle();
}
static void original_text(s32 english,s32 alpha){
    original_line(0,english?"Select language":"Selecciona el idioma",256,210,1.20f,1.85f,416,alpha);
    original_line(3,english?"Spanish":"Espa\xF1ol",156,307,0.95f,1.60f,100,alpha);
    original_line(4,english?"English":"Ingl\xE9s",357,307,0.95f,1.60f,100,alpha);
    Font_FlushAll();
}

/* The same Flash dialog, card resources, sounds and cursor input as BootCard.
 * Its allocation is shared with the following memory-card flow. */
void PortLanguage_Run(void){
    s32 selected=0,frame=0,choice=Port_GameProfileLanguage(),result;
    DialogFlashRef anchor;s32 alpha,spoken=choice;
    if(!Port_GameProfilePrompt())return;
    int three=Port_GameProfileAvailable(2);
    Dialog_SetLayout(0);Dialog_SetChoices(three?0:1);Dialog_SetPort(0);
    Dialog_SetChoiceLabels(sLabels[0],sLabels[1]);
    Dialog_SetMsg(-1);Dialog_SetTitle(-1);
    Port_GameProfileMenuSet(1,choice,0);
    Dialog_SetCursor(choice);Dialog_Start(DIALOG_CMD_OPEN);
    ColorFade_StartIn(0,0,0,20);
    PortA11y_Say(choice==2?"Selecione o idioma do jogo. Portugu\xC3\xAAs.":choice?"Select the game language. English.":"Selecciona el idioma del juego. Espa\xC3\xB1ol.",1);
    while(1){
        Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();
        if(!selected&&!gPortOverlayOpen&&ColorFade_IsInDone()){
            if(three){
                unsigned keys=gPad[0].pressed;int direction=0;result=0;
                if(keys&(PAD_LEFT|PAD_LSTICK_LEFT))direction=-1;
                if(keys&(PAD_RIGHT|PAD_LSTICK_RIGHT))direction=1;
                if(direction){int next=choice;do{next=(next+direction+3)%3;}while(!Port_GameProfileAvailable(next));choice=next;Snd_PlaySe(1,0);}
                if(keys&PAD_CROSS)result=DIALOG_RESULT_FIRST;
                if(keys&(PAD_TRIANGLE|PAD_CIRCLE))result=DIALOG_RESULT_CANCEL;
            }else {result=Dialog_Input(1);choice=gDialog->cursor;}
            if(getenv("BT4_PROFILE_PICKER_TEST")&&getenv("BT4_PROFILE_TEST_HOVER")&&frame==45){
                choice=atoi(getenv("BT4_PROFILE_TEST_HOVER"));if(choice<0||choice>(three?2:1))choice=0;Dialog_SetCursor(three?0:choice);
            }
            if(getenv("BT4_PROFILE_PICKER_TEST")&&frame==90&&getenv("BT4_PROFILE_TEST_CHOOSE")){
                choice=atoi(getenv("BT4_PROFILE_TEST_CHOOSE"));if(choice<0||choice>(three?2:1))choice=0;
                Dialog_SetCursor(three?0:choice);result=choice?DIALOG_RESULT_SECOND:DIALOG_RESULT_FIRST;
            }
            if(spoken!=choice){PortA11y_Say(choice==2?"Portugu\xC3\xAAs. Textos em portugu\xC3\xAAs, vozes em ingl\xC3\xAAs.":choice?"English. Text and voices.":"Espa\xC3\xB1ol. Textos y voces.",1);spoken=choice;}
            if(result==DIALOG_RESULT_CANCEL)exit(0);
            if(result==DIALOG_RESULT_FIRST||result==DIALOG_RESULT_SECOND){
                if(Port_GameProfileAvailable(choice)){
                    selected=1;Dialog_Start(DIALOG_CMD_CLOSE);
                }else{
                    gDialog->flags&=~DIALOG_FLAG_DECIDED;Dialog_SetCursor(choice);Snd_PlaySe(1,2);
                }
            }
        }
        Dialog_Draw(1);
        Flash_FindLabel(gDialog->flash,NULL,"mc_dummy_text_1",&anchor);
        alpha=anchor.id<0?0:(s32)(Flash_ClipGetAlpha(gDialog->flash,&anchor)*128.0f);
        /* The frame is a centred 4:3 page. Keep its labels on that same page
         * after Dialog_Draw restores the enclosing menu's anchor. */
        Port_GameProfileMenuSet(1,choice,alpha);Port_MenuPage(50);if(three){
            s32 count=0,i,slot=0;const char *names[3]={"Espa\xF1ol","English","Portugu\xEAs"};
            for(i=0;i<3;++i)count+=Port_GameProfileAvailable(i)!=0;
            sEmphasizedLine=choice+1;
            original_line(0,choice==2?"Selecione o idioma":choice?"Select language":"Selecciona el idioma",256,198,1.15f,1.75f,376,alpha);
            for(i=0;i<3;++i)if(Port_GameProfileAvailable(i)){
                s32 x=count==3?128+128*slot:176+160*slot;
                original_line(i+1,names[i],x,282,.85f,1.5f,112,alpha);++slot;
            }
            Font_FlushAll();sEmphasizedLine=-1;
        }else original_text(choice,alpha);
        Port_MenuPage(0);
        ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
        if(getenv("BT4_PROFILE_PICKER_TEST")&&frame==60){extern int gPortTagUiCaptureRequest;gPortTagUiCaptureRequest=1;}
        frame++;
        if(selected&&Dialog_IsClosed())break;
    }
    Port_GameProfileMenuSet(0,choice,0);
    Dialog_SetChoiceLabels(NULL,NULL);Dialog_SetChoices(0);Dialog_SetMsgTable(NULL);Dialog_SetMsg(-1);Dialog_SetTitle(-1);
    Dma_ResetBuffers();
    if(!Port_GameProfileChoose(choice)) {PortLanguage_Run();return;}
    MenuTranslation_SaveChoice(choice==2?"pt-BR":"en");MenuText_BootLanguageChosen();Ui_LanguageChosen(choice==2?"pt-BR":"en");
}
#endif
