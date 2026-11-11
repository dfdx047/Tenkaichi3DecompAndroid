/* GPL-3.0-or-later. Native TAG overlay, deliberately independent of menu art.
 * Main BT4 gauges continue to show the player and the selected opponent.
 * All dimensions use the UI page; Ps_Left anchors both edges on wide displays.
 * No synthetic health, union meter, distance or player data is drawn. */
#include "battle/hud_a.h"
#include "battle/tag_team_hud_math.h"
#include "battle/tag_team_hud_layout.h"
#include "menu/tag_team_radar_art.h"
static TagHudRect sTagHudRects[4];

typedef struct TagHudFace { u8 pad[0x50]; HudRes *res; } TagHudFace;
extern TagHudFace *BtlCtrl_GetObj(s32 actor);
extern s32 BtlCharApi_GetHp(s32 actor), BtlCharApi_GetHpMax(s32 actor);
extern s32 BtlSide_GetKi(s32 actor);
extern void BtlCharApi_GetPos(s32 actor, f32 *out);
extern f32 BtlCharApi_GetYaw(s32 actor);
extern f32 sqrtf(f32 x), sinf(f32 x), cosf(f32 x);
extern void Vu0Cur_ResetStack(void), Vu0Cur_LoadIdentity(void), HudGfx_SetEnv(void);
extern void HudSprite_InitTex(HudSprite *spr,HudRes *res,s32 tex,s32 sub);
extern void HudSprite_SetRect(HudSprite *spr,s32 x0,s32 x1,s32 y0,s32 y1);
extern void HudSprite_DrawAt(HudSprite *spr,HudRes *res,s32 ctx,s32 tbp,s32 cbp);
extern void Port_GsMarker(s32 effect);
extern s32 TagBattle_HudCamera(f32 *basis),TagBattle_HudHead(s32 actor,f32 *out),TagBattle_HudProject(f32 *head,f32 *out);
extern void TagBattle_HudRear(f32 *head,f32 *out);
extern s32 BtlSeq_IsFighting(void);
static f32 sTagHudBasis[4];
static s32 sTagHudGeometryValid;
static f32 sTagHudRadarRange=600;
static f32 sTagHudPositions[10][4];
extern void BtlCharApi_GetBasePos(s32 actor,f32 *out);
extern void Port_TagHudTrace(s32 actor,s32 front,s32 kind,f32 *basis,f32 *own,f32 *pos,f32 *head,f32 *scr,f32 *marker,f32 *radar);

static u32 TagHud_Color(s32 r,s32 g,s32 b) { return Ps_Rgba(r,g,b,0x80); }
static void TagHud_Hex(f32 x,f32 y,f32 w,f32 h,u32 top,u32 bottom) {
    f32 c=w*0.20f;
    Ps_Quad(x+c,y,x+w-c,y,x,y+h*0.5f,x+w,y+h*0.5f,top,top,bottom,bottom);
    Ps_Quad(x,y+h*0.5f,x+w,y+h*0.5f,x+c,y+h,x+w-c,y+h,bottom,bottom,bottom,bottom);
}
static void TagHud_Stroke(f32 x0,f32 y0,f32 x1,f32 y1,f32 width,u32 c) {
    f32 dx=x1-x0,dy=y1-y0,n=sqrtf(dx*dx+dy*dy),px,py;
    if(n<0.001f)return;
    px=-dy*width*0.5f/n;py=dx*width*0.5f/n;
    Ps_Quad(x0+px,y0+py,x1+px,y1+py,x0-px,y0-py,x1-px,y1-py,c,c,c,c);
}
static void TagHud_Label(f32 x,f32 y,char *s,f32 scale,f32 width,u32 color) {
    Ps_TextExOuter(x,y,s,scale,width,color,color,TagHud_Color(3,10,17),Ps_Rgba(0,0,0,0x50),0.35f);
}
static void TagHud_Seat(char *label,char *prefix,s32 actor) {
    s32 n;
    Ps_Copy(label,prefix,32);n=Ps_Len(label);
    label[n]='0';label[n+1]='1'+(TagBattle_FreeForAll()?actor:TagBattle_TeamIndex(actor));label[n+2]=0;
}
static void TagHud_Number(char *label,char *prefix,s32 value) {
    char reverse[12];s32 n=0,len;
    Ps_Copy(label,prefix,32);len=Ps_Len(label);
    do{reverse[n++]='0'+value%10;value/=10;}while(value>0 && n<10);
    while(n>0 && len<31)label[len++]=reverse[--n];label[len]=0;
}
static void TagHud_Arrow(f32 x,f32 y,f32 dx,f32 dy,f32 size,u32 color) {
    f32 n,px,py;
    dx/=PS_UNIT_X;n=sqrtf(dx*dx+dy*dy);
    if(n<0.001f){dx=0;dy=1;n=1;}
    dx/=n;dy/=n;px=-dy;py=dx;
    Ps_Quad(x+dx*size*PS_UNIT_X,y+dy*size,x+(px*0.65f-dx*0.65f)*size*PS_UNIT_X,y+(py*0.65f-dy*0.65f)*size,
        x+(-px*0.65f-dx*0.65f)*size*PS_UNIT_X,y+(-py*0.65f-dy*0.65f)*size,x+(-px*0.65f-dx*0.65f)*size*PS_UNIT_X,y+(-py*0.65f-dy*0.65f)*size,color,color,color,color);
}
static void TagHud_Portrait(s32 actor,f32 x,f32 y,f32 w,f32 h) {
    TagHudFace *obj=BtlCtrl_GetObj(actor);
    HudSprite spr;
    HudRes *res;
    if(!obj || !obj->res)return;
    res=obj->res;
    if(res->texCount<1 || !res->tex)return;
    spr.flags=0;
    /* Portraits share the original upload addresses. Force each queued upload,
     * then leave the resource unmarked for the next main-gauge draw. */
    res->tex[0].mark=0;
    HudGfx_SetEnv();Port_GsMarker(0x19); /* page coordinates already account for aspect */
    HudSprite_InitTex(&spr,res,0,0);
    HudSprite_SetRect(&spr,(s32)Ps_X(x),(s32)Ps_X(x+w),(s32)Ps_Y(y),(s32)Ps_Y(y+h));
    if(BtlCharApi_GetHp(actor)<=0){spr.r=0x35;spr.g=0x35;spr.b=0x35;spr.a=0x65;}
    HudSprite_DrawAt(&spr,res,0,0x2C00,0x2CD0);
    res->tex[0].mark=0;
    Port_GsMarker(0x13);
}
static void TagHud_Bar(s32 actor,f32 x,f32 y,f32 w,f32 h) {
    s32 hp=BtlCharApi_GetHp(actor),max=BtlCharApi_GetHpMax(actor),k;
    f32 f=max>0?(f32)hp/max:0;
    u32 green=TagHud_Color(114,242,91),dark=TagHud_Color(19,111,31);
    if(f<0)f=0;if(f>1)f=1;
    if(f<0.25f){green=TagHud_Color(255,118,74);dark=TagHud_Color(137,33,23);}
    Ps_Rect(x-1,y-1,x+w+1,y+h+1,TagHud_Color(2,8,13),TagHud_Color(2,8,13));
    Ps_Rect(x,y,x+w,y+h,TagHud_Color(24,44,45),TagHud_Color(11,23,29));
    if(f>0)Ps_Rect(x,y,x+w*f,y+h,green,dark);
    for(k=1;k<4;++k)Ps_Rect(x+w*k/4,y,x+w*k/4+0.5f,y+h,Ps_Rgba(0,0,0,0x40),Ps_Rgba(0,0,0,0x40));
}
static void TagHud_RadarPoint(f32 cx,f32 cy,s32 actor,s32 target,f32 *own) {
    f32 pos[4],r[4],px,py; s32 far;
    u32 color=!TagBattle_Allied(actor,TagBattle_ViewActor())?TagHud_Color(255,118,94):TagHud_Color(104,230,255);
    memcpy(pos,sTagHudPositions[actor],sizeof(pos));
    far=ThRadar(own,pos,sTagHudBasis,sTagHudRadarRange,30,r);
    px=cx+r[0]*PS_UNIT_X;py=cy+r[1];
    if(actor==target)color=TagHud_Color(255,213,91);
    if(far){
        TagHud_Arrow(px,py,r[0]*PS_UNIT_X,r[1],4.6f,TagHud_Color(1,12,18));
        TagHud_Arrow(px,py,r[0]*PS_UNIT_X,r[1],3.2f,color);
    }else if(actor==target){
        TagHud_Hex(px-4,py-4.5f,8,9,color,color);
        TagHud_Hex(px-3,py-3.5f,6,7,TagHud_Color(3,29,28),TagHud_Color(3,29,28));
        Ps_Disc(px,py,1.4f,color,color);
    }else if(!TagBattle_Allied(actor,TagBattle_ViewActor())){
        TagHud_Arrow(px,py,0,-1,2.6f,color);
    }else Ps_Rect(px-1.6f,py-1.8f,px+1.6f,py+1.8f,color,color);
    if(r[3]>8){TagHud_Stroke(px-2,py-6,px,py-8,1,color);TagHud_Stroke(px,py-8,px+2,py-6,1,color);}
    if(r[3]<-8){TagHud_Stroke(px-2,py+6,px,py+8,1,color);TagHud_Stroke(px,py+8,px+2,py+6,1,color);}
}
static void TagHud_Radar(f32 cx,f32 cy,s32 target) {
    f32 *own=sTagHudPositions[TagBattle_ViewActor()],yaw,dx,dy;s32 i;
    if(!sTagHudGeometryValid)return;
    TrRadarFace(cx,cy);
    for(i=0;i<TagBattle_Count();++i)if(i!=target && i!=TagBattle_ViewActor() && BtlCharApi_GetHp(i)>0)TagHud_RadarPoint(cx,cy,i,target,own);
    if(target>=0 && target<TagBattle_Count() && BtlCharApi_GetHp(target)>0)TagHud_RadarPoint(cx,cy,target,target,own);
    yaw=BtlCharApi_GetYaw(BattleSide_GetObjId(TagBattle_ViewActor()));
    dx=sinf(yaw)*sTagHudBasis[0]+cosf(yaw)*sTagHudBasis[1];
    dy=-(sinf(yaw)*sTagHudBasis[2]+cosf(yaw)*sTagHudBasis[3]);
    TrRadarArrow(cx,cy,dx,dy,4.4f,TagHud_Color(21,39,19));
    TrRadarArrow(cx,cy,dx,dy,3.4f,TagHud_Color(239,43,29));
    Ps_Align(1);TagHud_Label(cx,cy+48,"L3 CHANGE TARGET",0.39f,116,TagHud_Color(225,242,245));Ps_Align(0);
}
static s32 TagHud_Blocked(f32 x,f32 y) {
    f32 l=Ps_Left(),r=512-l;s32 i;
    if(y<92 || y>417 || ((x<l+145 || x>r-145) && y>110 && y<200))return 1;
    for(i=0;i<4;++i){TagHudRect *b=&sTagHudRects[i];if(x>b->x-5 && x<b->x+b->w+5 && y>b->y-5 && y<b->y+b->h+5)return 1;}
    return 0;
}
static void TagHud_WorldMarkers(s32 target) {
    s32 i;
    f32 left=Ps_Left(),right=512-left,own[4];
    if(!sTagHudGeometryValid)return;
    memcpy(own,sTagHudPositions[TagBattle_ViewActor()],sizeof(own));
    for(i=0;i<TagBattle_Count();++i){
        f32 head[4],scr[3],pos[4],rad[5],x,y,dx,dy,p[2];s32 front,on;
        u32 c;char number[32];
        if(i==TagBattle_ViewActor() || BtlCharApi_GetHp(i)<=0 || !TagBattle_HudHead(i,head))continue;
        front=TagBattle_HudProject(head,scr);
        memcpy(pos,sTagHudPositions[i],sizeof(pos));ThRadar(own,pos,sTagHudBasis,sTagHudRadarRange,30,rad);rad[4]=sTagHudRadarRange;
        x=left+scr[0]*(512-2*left)/512;y=scr[1];
        on=front && x>left+10 && x<right-10 && y>92 && y<417 && !TagHud_Blocked(x,y);
        c=i==target?TagHud_Color(255,213,91):!TagBattle_Allied(i,TagBattle_ViewActor())?TagHud_Color(255,118,94):TagHud_Color(104,230,255);
        if(!on){
            if(i!=target){if(getenv("BT4_TAG_HUD_LOG"))Port_TagHudTrace(i,front,0,sTagHudBasis,own,pos,head,scr,scr,rad);continue;}
            if(front){dx=x-256;dy=y-224;}
            else{
                TagBattle_HudRear(head,p);dx=p[0]*PS_UNIT_X;dy=p[1];
            }
            ThRay(256,224,dx,dy,left+18,162,right-18,408,p);
            {
                s32 widget;
                for(widget=0;widget<TAG_HUD_WIDGETS;++widget){
                    TagHudRect *b=&sTagHudRects[widget];
                    ThAvoid(256,224,b->x-10,b->y-10,b->x+b->w+10,b->y+b->h+20,p);
                }
                ThAvoid(256,224,left,110,left+145,200,p);
                ThAvoid(256,224,right-145,110,right,200,p);
            }
            TagHud_Arrow(p[0],p[1],dx,dy,9,TagHud_Color(2,12,20));
            TagHud_Arrow(p[0],p[1],dx,dy,6.2f,c);
            if(getenv("BT4_TAG_HUD_LOG"))Port_TagHudTrace(i,front,2,sTagHudBasis,own,pos,head,scr,p,rad);
            Ps_Align(1);
            if(front)TagHud_Seat(number,"TARGET ",i);
            else if(scr[2]>=0)TagHud_Seat(number,"NEAR ",i);
            else TagHud_Seat(number,"BEHIND ",i);
            TagHud_Label(p[0],p[1]+13,number,0.42f,90,c);Ps_Align(0);
            continue;
        }
        p[0]=x;p[1]=y;
        if(getenv("BT4_TAG_HUD_LOG"))Port_TagHudTrace(i,front,1,sTagHudBasis,own,pos,head,scr,p,rad);
        TagHud_Stroke(x-4,y-5,x,y-1,2.7f,TagHud_Color(3,12,18));
        TagHud_Stroke(x,y-1,x+4,y-5,2.7f,TagHud_Color(3,12,18));
        TagHud_Stroke(x-4,y-5,x,y-1,1.2f,c);TagHud_Stroke(x,y-1,x+4,y-5,1.2f,c);
        if(i==target){
            TagHud_Number(number,"",1+(TagBattle_FreeForAll()?i:TagBattle_TeamIndex(i)));
            Ps_Align(1);TagHud_Label(x,y-20,number,0.50f,22,c);Ps_Align(0);
        }
    }
}

void TagBattle_DrawRoster(void) {
    s32 i,slot,target=TagBattle_Target(TagBattle_ViewActor()),alive[2]={0,0},count=TagBattle_Count();
    f32 left,right,x,y,pitch,scale;
    s32 view=TagBattle_ViewActor(),allies=TagBattle_TeamCount(TagBattle_Team(view))-1,enemies=TagBattle_TeamCount(1-TagBattle_Team(view));
    if(TagBattle_FreeForAll()){allies=0;enemies=count-1;}
    char label[32],score[8];
    u32 cyan=TagHud_Color(110,237,255),gold=TagHud_Color(255,211,90),white=TagHud_Color(240,249,252);
    if(!TagBattle_Active() || !gHud || !BtlSeq_IsFighting() || !(gHud->flags&HUD_SHOW_GAUGES) || (Battle_GetWork()->flags&BATTLE_FLAG_LOADING))return;
    Ps_PageSet();Ps_SetShift(0);Ps_Align(0);Port_GsMarker(0x13);
    Vu0Cur_ResetStack();Vu0Cur_LoadIdentity();
    TagHud_ReadRects(sTagHudRects,Ps_Left());
    left=sTagHudRects[0].x;right=sTagHudRects[1].x+sTagHudRects[1].w;
    if(enemies>5){sTagHudRects[1].w=enemies*28;sTagHudRects[1].x=right-sTagHudRects[1].w;}
    pitch=allies>4?124.0f/allies:32.0f;scale=pitch/32.0f;
    sTagHudGeometryValid=TagBattle_HudCamera(sTagHudBasis);
    /* One current simulation snapshot for every contact. No selected-target
     * zoom, display shake, interpolation or contact history enters the radar. */
    for(i=0;i<count;++i)BtlCharApi_GetBasePos(BattleSide_GetObjId(i),sTagHudPositions[i]);
    sTagHudRadarRange=600;
    TagHud_WorldMarkers(target);
    for(i=0;i<count;++i)if(BtlCharApi_GetHp(i)>0)++alive[TagBattle_Team(i)];
    /* Compact steel tabs repeat the angular frames of the original BT4 HUD. */
    Ps_Align(1);TagHud_Label(sTagHudRects[3].x+40,sTagHudRects[3].y,TagBattle_FreeForAll()?"FREE FOR ALL":"TAG TEAM",0.44f,80,gold);
    if(TagBattle_FreeForAll()){score[0]='0'+alive[0]+alive[1];score[1]=0;}
    else {score[0]='0'+alive[0];score[1]=' ';score[2]=':';score[3]=' ';score[4]='0'+alive[1];score[5]=0;}
    TagHud_Label(sTagHudRects[3].x+40,sTagHudRects[3].y+13,score,0.54f,64,white);Ps_Align(0);
    if(allies>0)TagHud_Label(left+3,sTagHudRects[0].y,"ALLIES",0.43f,80,cyan);
    for(i=0,slot=0;i<count;++i){
        if(i==view||!TagBattle_Allied(i,view))continue;
        s32 hp=BtlCharApi_GetHp(i),ki=BtlSide_GetKi(i);
        x=left;y=sTagHudRects[0].y+18+slot*pitch;
        Ps_Quad(x+14,y+2*scale,x+104,y+2*scale,x+14,y+25*scale,x+96,y+25*scale,Ps_Rgba(27,48,61,0x68),Ps_Rgba(22,36,46,0x58),Ps_Rgba(8,19,30,0x65),Ps_Rgba(8,19,30,0x45));
        TagHud_Hex(x,y-scale,27*scale,29*scale,TagHud_Color(126,156,170),TagHud_Color(39,60,75));
        TagHud_Hex(x+1,y,25*scale,27*scale,TagHud_Color(4,17,24),TagHud_Color(4,17,24));
        TagHud_Portrait(i,x+2,y+1*scale,23*scale,25*scale);
        if(TagBattle_Network()){extern const char *Port_TagPlayerName(s32);extern s32 Port_TagCPU(s32);
            if(Port_TagCPU(i))TagHud_Number(label,"CPU ",TagBattle_TeamIndex(i)+1);
            else Ps_Copy(label,(char*)Port_TagPlayerName(i),32);}
        else TagHud_Seat(label,"CPU ",i);
        TagHud_Label(x+30,y+1*scale,label,(allies>4?0.37f:0.48f),66,hp>0?white:TagHud_Color(145,153,159));
        TagHud_Bar(i,x+30,y+(allies>4?10.0f:15.0f),67,3.5f*scale);
        if(ki<0)ki=0;if(ki>100000)ki=100000;
        Ps_Rect(x+30,y+(allies>4?13.5f:21.0f),x+97,y+(allies>4?14.5f:22.5f),TagHud_Color(7,19,29),TagHud_Color(7,19,29));
        if(hp>0)Ps_Rect(x+30,y+(allies>4?13.5f:21.0f),x+30+67*(f32)ki/100000,y+(allies>4?14.5f:22.5f),cyan,TagHud_Color(22,115,162));
        else TagHud_Label(x+6,y+7*scale,"KO",0.4f,23,TagHud_Color(241,157,124));
        ++slot;
    }
    x=right-enemies*28;
    TagHud_Seat(label,"TARGET ",target);
    Ps_Align(2);TagHud_Label(right,sTagHudRects[1].y,label,0.46f,140,gold);Ps_Align(0);
    y=sTagHudRects[1].y;
    for(i=0,slot=0;i<count;++i){
        if(TagBattle_Allied(i,view))continue;
        f32 sx=x+slot*28;
        u32 edge=i==target?gold:TagHud_Color(125,138,153);
        TagHud_Hex(sx,y+16,26,29,edge,edge);
        TagHud_Hex(sx+1,y+17,24,27,TagHud_Color(24,21,28),TagHud_Color(24,21,28));
        TagHud_Portrait(i,sx+2,y+18,22,25);
        TagHud_Bar(i,sx+2,y+47,22,3);
        if(i==target)Ps_Rect(sx+3,y+53,sx+23,y+54.5f,gold,gold);
        if(BtlCharApi_GetHp(i)<=0)TagHud_Label(sx+5,y+26,"KO",0.5f,23,TagHud_Color(241,157,124));
        ++slot;
    }
    TagHud_Radar(sTagHudRects[2].x+41,sTagHudRects[2].y+61,target);
    Ps_Align(0);
    if(TagBattle_Spectating()){
        TagHud_Number(label,TagBattle_Network()?"SPECTATING PLAYER ":"SPECTATING CPU ",TagBattle_TeamIndex(TagBattle_ViewActor())+1);
        TagHud_Label(Ps_Left()+12,410,label,.46f,230,gold);
        TagHud_Label(Ps_Left()+12,427,"L1 / R1  SWITCH TEAMMATE",.40f,230,white);
    }else TagHud_Label(Ps_Left()+12,427,TagBattle_Network()?"TAG TEAM ONLINE":"CPU PRACTICE",0.40f,140,TagHud_Color(186,215,222));
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_WIDE_AUDIT")){
        static s32 frames;if(++frames==90){extern void Port_TagUiCapture(s32);Port_TagUiCapture(73);}
    }
    Port_GsMarker(0x13);
}
