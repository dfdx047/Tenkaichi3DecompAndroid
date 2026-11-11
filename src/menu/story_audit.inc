/* Explicit native data/renderer/audio regression, isolated saves only. */
#include "menu/menu_b.h"
#include <stdio.h>
extern s32 Port_AdxStoryProbe(s32 channel,s32 part,s32 file);
extern void ModeMenu_Update(void);
extern void HistOutro_Init(s32 section);
extern void HistOutro_Update(void);
extern void HistOutro_Draw(void);
extern void HistOutro_Term(void);

static s32 Story_Audio(s32 channel,s32 id) {
    s32 part=id<0xD48?1:id<0x10CC0?2:id<0x20000?0:3;
    s32 base=part==1?1:part==2?0xD48:part==0?0x10CC0:0x20000;
    return Port_AdxStoryProbe(channel,part,id-base);
}

void Hist_StoryAudit(void) {
    s32 saga,item,tick,n,images=0,fail=0,silent=0,voices=0,scriptSeen[48]={0};
    s32 music[8]={0x834,0x834,0x835,0x81C,0x81C,0x837,0x838,0x82D};
    s32 outro[8]={0x81C,0x81C,0x81C,0x81C,0x81C,0x81E,0x828,0x81C};
    extern void Adx_StopAll(void);
    extern void Voice_PlayWithSubtitle(void *,s32,s32);
    gSaveData->flags &= ~1;
    Bgm_Play(0x86C);if(Story_Audio(0,0x86C)<=0)fail++;
    /* The host's real greeting. Guest silence is audited against the assets. */
    Voice_PlayWithSubtitle(NULL,0x855B,14);
    if(Story_Audio(4,0x855B+14)<=0)fail++;
    Adx_StopAll();
    for(saga=0;saga<8;saga++){
        gProgress->subMenu=saga;gProgress->prevMode=6;
        gSaveData->slot[saga].val[0]=0xFFFF;
        ModeMenu_Init(2);
        if(Story_Audio(0,music[saga])<=0)fail++;
        for(item=0;item<gModeMenu->itemCount;item++){
            s32 episode=gModeMenu->items[item],script,voice;
            if(episode>=gModeMenu->itemMax)continue;
            script=ModeMenu_GetLine(saga,episode);
            if(script<0 || script>=48 || scriptSeen[script]++)fail++;
            gModeMenu->cursor[0]=item;
            ModeMenu_ChangeImage();
            for(tick=0;tick<40;tick++){
                ModeMenu_UpdateImage();File_Stub264D90();
                if(gModeMenu->flags&MODEMENU_IMAGE_READY)break;
            }
            if(!(gModeMenu->flags&MODEMENU_IMAGE_READY) || !gModeMenu->tex[9])fail++;
            else images++;
            voice=gModeMenu->voiceBase+gModeMenu->lineBase+4+episode;
            Voice_PlayWithSubtitle(NULL,voice,0);n=Story_Audio(4,voice);
            if(n<0)fail++;else if(n==0)silent++;voices++;
            printf("story-chapter saga=%d episode=%d script=%d image=%d voice=%x peak=%d\n",
                   saga,episode,script,!!(gModeMenu->flags&MODEMENU_IMAGE_READY),voice,n);
        }
        ModeMenu_Term();Adx_StopAll();
        HistOutro_Init(2);
        if(Story_Audio(0,outro[saga])<=0)fail++;
        for(tick=0;tick<120;tick++)HistOutro_Update();
        HistOutro_Draw();Dma_ResetBuffers();
        HistOutro_Term();Adx_StopAll();
    }
    for(item=0;item<48;item++)if(scriptSeen[item]!=1)fail++;
    printf("story-audit failures=%d chapters=48 images=%d voice_files=%d original_silent_files=%d sagas=8 outros=8\n",
           fail,images,voices,silent);fflush(stdout);exit(fail?3:0);
}
