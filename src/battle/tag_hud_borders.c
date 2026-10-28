/* Split dividers belong to the scene/HUD layer, below Ready/Fight, pause text
 * and the timer. Drawing them in the window overlay covered those elements. */
#ifdef PORT
#include "battle/hud_c.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
extern void Port_GsMarker(s32);

static void rect(s32 x0,s32 y0,s32 x1,s32 y1) {
    HudCSprite s;
    HudSprite_InitPlain(&s,4,7,13,0x80);
    HudSprite_SetRect(&s,x0,x1,y0,y1);
    HudSprite_DrawAt(&s,0,0,0,0);
}
void TagHud_DrawBorders(s32 shared) {
    s32 views=TagTeam_ViewCount(),i,r[4];
    if(shared || views<2) return;
    Port_GsMarker(0x5F);
    Port_GsMarker(0x13);
    if(views==3 && TagTeam_ViewVisible(2)) {
        TagTeam_ViewRect(2,r);
        if(r[0]==128) {rect(0,224,128,448);rect(384,224,512,448);}
    }
    for(i=0;i<views;i++) if(TagTeam_ViewVisible(i)) {
        TagTeam_ViewRect(i,r);
        if(r[0]>0) rect(r[0]-1,r[1],r[0]+1,r[1]+r[3]);
        if(r[0]+r[2]<512) rect(r[0]+r[2]-1,r[1],r[0]+r[2]+1,r[1]+r[3]);
        if(r[1]>0) rect(r[0],r[1]-1,r[0]+r[2],r[1]+1);
        if(r[1]+r[3]<448) rect(r[0],r[1]+r[3]-1,r[0]+r[2],r[1]+r[3]+1);
    }
}
#endif
