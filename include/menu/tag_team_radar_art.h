#ifndef TAG_TEAM_RADAR_ART_H
#define TAG_TEAM_RADAR_ART_H
/* Shared by the live HUD and its editor. Original code-drawn Dragon Radar
 * casing, phosphor screen and cardinal marks, in aspect-correct UI units. */
static void TrRadarArrow(f32 x,f32 y,f32 dx,f32 dy,f32 size,u32 color){
    f32 n=sqrtf(dx*dx+dy*dy),px,py;
    if(n<.001f){dx=0;dy=-1;n=1;}dx/=n;dy/=n;px=-dy;py=dx;
    Ps_Quad(x+dx*size*PS_UNIT_X,y+dy*size,x+(px*.7f-dx*.7f)*size*PS_UNIT_X,y+(py*.7f-dy*.7f)*size,
        x+(-px*.7f-dx*.7f)*size*PS_UNIT_X,y+(-py*.7f-dy*.7f)*size,x+(-px*.7f-dx*.7f)*size*PS_UNIT_X,y+(-py*.7f-dy*.7f)*size,color,color,color,color);
}
static void TrRadarFace(f32 cx,f32 cy){
    s32 k;u32 silver=Ps_Rgba(224,230,226,0x80),red=Ps_Rgba(211,31,24,0x80);
    /* An opaque face is repainted before the contacts on every rendered frame. */
    Ps_Rect(cx-5*PS_UNIT_X,cy-47,cx+5*PS_UNIT_X,cy-36,Ps_Rgba(236,239,234,0x80),Ps_Rgba(105,118,116,0x80));
    Ps_Disc(cx,cy-47,6.5f,Ps_Rgba(255,255,249,0x80),Ps_Rgba(144,158,153,0x80));
    Ps_Disc(cx+.8f,cy+1.5f,41,Ps_Rgba(31,42,37,0x80),Ps_Rgba(31,42,37,0x80));
    Ps_Disc(cx,cy,40.5f,silver,Ps_Rgba(96,110,103,0x80));
    Ps_Disc(cx,cy-.6f,38.8f,Ps_Rgba(255,255,247,0x80),Ps_Rgba(255,255,247,0x80));
    Ps_Disc(cx,cy,37.5f,Ps_Rgba(21,47,25,0x80),Ps_Rgba(21,47,25,0x80));
    Ps_Disc(cx,cy,36.2f,Ps_Rgba(1,103,6,0x80),Ps_Rgba(0,17,2,0x80));
    for(k=-5;k<=5;++k){
        f32 v=k*6.0f,span=sqrtf(35*35-v*v),half=k==0?.27f:.17f;
        u32 grid=Ps_Rgba(70,165,47,k==0?0x32:0x24);
        Ps_Rect(cx-span*PS_UNIT_X,cy+v-half,cx+span*PS_UNIT_X,cy+v+half,grid,grid);
        Ps_Rect(cx+(v-half)*PS_UNIT_X,cy-span,cx+(v+half)*PS_UNIT_X,cy+span,grid,grid);
    }
    TrRadarArrow(cx,cy-32.5f,0,1,2.2f,red);TrRadarArrow(cx,cy+32.5f,0,-1,2.2f,red);
    TrRadarArrow(cx-32.5f*PS_UNIT_X,cy,1,0,2.2f,red);TrRadarArrow(cx+32.5f*PS_UNIT_X,cy,-1,0,2.2f,red);
}
#endif
