#ifndef MENU_TAG_OUTFIT_H
#define MENU_TAG_OUTFIT_H
#include "menu/menu_a.h"

extern void TextBox_Init(MTextBox *,void *,u32);
extern void TextBox_SetUnk80(MTextBox *,s32);
extern void TextBox_AttachLine(MFlash *,MFlashRef *,s32,s32,s32,MTextBox *);

/* B14's plate movie stacks eight numbers at one position. Only the current
   outfit is visible; unlike BT3, these are not four separate buttons. */
static void TagMenu_DrawBT4Outfit(MFlash *flash,u32 *menu,s32 chara,s32 outfit,s32 count,s32 side) {
    static MTextBox boxes[2]; /* Flash retains this storage until the draw pass. */
    MFlashRef ref;
    MFlashUv uv={0};
    s32 j;
    char name[32];
    for(j=0;j<8;j++) {
        sprintf(name,"mc_color_plate_%d",j+1);
        Flash_FindLabel(flash,NULL,name,&ref);
        Flash_ClipSetFlags(flash,&ref,2,j==outfit);
        if(j!=outfit)continue;
        uv.x0=(j%2)*64;uv.x1=uv.x0+64;
        uv.y0=(j/2)*32;uv.y1=uv.y0+32;
        Flash_FindLabel(flash,name,"mc_color_text_off",&ref);
        Flash_ClipSetUv(flash,&ref,&uv);
        Flash_FindLabel(flash,name,"mc_color_text_on",&ref);
        Flash_ClipSetUv(flash,&ref,&uv);
    }
    for(j=0;j<2;j++) {
        Flash_FindLabel(flash,j?"mc_yajirusi_right":"mc_yajirusi_left",
                       j?"mc_yajirusi_icon_right":"mc_yajirusi_icon_left",&ref);
        Flash_ClipSetFlags(flash,&ref,2,count>1);
        uv.x0=j*32;uv.x1=uv.x0+32;uv.y0=0;uv.y1=32;
        Flash_ClipSetUv(flash,&ref,&uv);
    }
    Flash_FindLabel(flash,NULL,"mc_color_name_text",&ref);
    TextBox_AttachLine(flash,&ref,0,0,-1,&boxes[side]);
    if(menu[0]>=57 && chara>=0) {
        u32 *names=(u32 *)MPACK_AT(menu,57);
        if(chara<(s32)names[0]) {
            u32 *lines=(u32 *)MPACK_AT(names,chara+1);
            if(outfit>=0 && outfit<(s32)lines[0]) {
                TextBox_Init(&boxes[side],lines,7);
                TextBox_SetUnk80(&boxes[side],1);
                TextBox_AttachLine(flash,&ref,0,0,outfit,&boxes[side]);
            }
        }
    }
}
#endif
