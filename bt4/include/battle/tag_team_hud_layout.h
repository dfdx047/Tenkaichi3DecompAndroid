#ifndef TAG_TEAM_HUD_LAYOUT_H
#define TAG_TEAM_HUD_LAYOUT_H
/* UI page coordinates. Saved values are offsets from aspect-aware defaults. */
enum { TAG_HUD_ALLIES,TAG_HUD_TARGETS,TAG_HUD_RADAR,TAG_HUD_SCORE,TAG_HUD_WIDGETS };
typedef struct TagHudRect {f32 x,y,w,h;} TagHudRect;
static const char *sTagHudKeys[4][2]={{"tag_hud_allies_x","tag_hud_allies_y"},{"tag_hud_targets_x","tag_hud_targets_y"},{"tag_hud_radar_x","tag_hud_radar_y"},{"tag_hud_score_x","tag_hud_score_y"}};
static void TagHud_DefaultRects(TagHudRect *r,f32 left){
    f32 right=512-left;
    r[0]=(TagHudRect){left+12,257,105,142};
    r[1]=(TagHudRect){right-152,210,140,58};
    r[2]=(TagHudRect){right-94,286,82,132};
    r[3]=(TagHudRect){216,75,80,32};
}
static void TagHud_ClampRect(TagHudRect *r,f32 left){
    if(r->x<left+6)r->x=left+6;
    if(r->x+r->w>506-left)r->x=506-left-r->w;
    if(r->y<70)r->y=70;
    if(r->y+r->h>422)r->y=422-r->h;
}
static void TagHud_ReadRects(TagHudRect *r,f32 left){
    extern int Port_Setting(const char *,int);
    s32 i;TagHud_DefaultRects(r,left);
    for(i=0;i<4;++i){r[i].x+=Port_Setting(sTagHudKeys[i][0],0);r[i].y+=Port_Setting(sTagHudKeys[i][1],0);TagHud_ClampRect(&r[i],left);}
}
#endif
