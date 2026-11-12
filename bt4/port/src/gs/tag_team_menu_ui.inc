#include "online_language.h"
// One presentation layer for the local Tag Team menu, including held frames.
// It uses the same artwork as the in-game online pages, without drawing into
// the GS world buffers or taking ownership of the player's controls.
static int sTagMenuOpen,sTagMenuCursor;
static SDL_GPUTexture *sTagMenuArt[3];
static bool sTagMenuArtTried;
extern "C" void Port_TagMenuSet(int open,int cursor) {
    open=open!=0;cursor=cursor==1;
    if ((sTagMenuOpen!=open || sTagMenuCursor!=cursor) && getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_PAUSE_TEST"))
        fprintf(stderr,"tag pause: open=%d cursor=%d\n",open,cursor);
    sTagMenuOpen=open;sTagMenuCursor=cursor;
}
static void tag_menu_art_load(void) {
    if(sTagMenuArtTried)return;
    sTagMenuArtTried=true;
    const char *files[3]={"kit_window","kit_sel","kit_bar"};
    for(int i=0;i<3;++i){
        char file[1024];SDL_snprintf(file,sizeof(file),"tbo/ui/%s.png",files[i]);
        SDL_Surface *src=SDL_LoadPNG(file);
        if(!src)continue;
        SDL_Surface *rgba=SDL_ConvertSurface(src,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(src);
        if(!rgba)continue;
        size_t bytes=(size_t)rgba->w*rgba->h*4;uint8_t *pixels=(uint8_t *)malloc(bytes);
        if(pixels)for(int y=0;y<rgba->h;++y)memcpy(pixels+(size_t)y*rgba->w*4,(uint8_t*)rgba->pixels+(size_t)y*rgba->pitch,(size_t)rgba->w*4);
        if(pixels)sTagMenuArt[i]=make_strip(pixels,bytes,rgba->w,rgba->h,1);
        SDL_DestroySurface(rgba);
    }
}
static void tag_menu_draw(void) {
    tag_menu_art_load();
    ImDrawList *dl=ImGui::GetForegroundDrawList();
    float scale=fminf(gUiPresentW/512.0f,gUiPresentH/288.0f);
    float cx=gUiPresentX+gUiPresentW*.5f,cy=gUiPresentY+gUiPresentH*.5f;
    float left=cx-125*scale,right=cx+125*scale,top=cy-69*scale,bottom=cy+76*scale;
    dl->AddRectFilled(ImVec2(gUiPresentX,gUiPresentY),ImVec2(gUiPresentX+gUiPresentW,gUiPresentY+gUiPresentH),IM_COL32(0,6,14,115));
    if(sTagMenuArt[0])dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)sTagMenuArt[0]),ImVec2(left,top),ImVec2(right,bottom));
    else {dl->AddRectFilled(ImVec2(left,top),ImVec2(right,bottom),IM_COL32(17,67,73,255),8*scale);dl->AddRect(ImVec2(left,top),ImVec2(right,bottom),IM_COL32(142,176,180,255),8*scale);}
    ImFont *font=sFontBold?sFontBold:ImGui::GetFont();
    auto text=[&](const char *label,float y,float size,ImU32 color,float maxWidth){
        label=PortOnline_Text(label);size*=scale;ImVec2 ext=font->CalcTextSizeA(size,FLT_MAX,0,label);
        if(ext.x>maxWidth*scale){size*=maxWidth*scale/ext.x;ext=font->CalcTextSizeA(size,FLT_MAX,0,label);}
        ImVec2 pos(cx-ext.x*.5f,y-ext.y*.5f);
        dl->AddText(font,size,ImVec2(pos.x+scale,pos.y+scale),IM_COL32(3,14,25,240),label);
        dl->AddText(font,size,pos,color,label);
    };
    text("TAG TEAM ONLINE",cy-48*scale,14,IM_COL32(224,248,255,255),220);
    text("The match continues while this menu is open.",cy-30*scale,8.8f,IM_COL32(182,218,223,255),218);
    const char *labels[2]={"Continue","Leave Room"};
    for(int i=0;i<2;++i){
        float y=cy-14*scale+i*29*scale;SDL_GPUTexture *art=sTagMenuArt[i==sTagMenuCursor?1:2];
        if(art)dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)art),ImVec2(left+15*scale,y),ImVec2(right-15*scale,y+25*scale));
        else dl->AddRectFilled(ImVec2(left+15*scale,y),ImVec2(right-15*scale,y+25*scale),i==sTagMenuCursor?IM_COL32(251,201,42,255):IM_COL32(83,130,159,255),12*scale);
        text(labels[i],y+12.5f*scale,13,i==sTagMenuCursor?IM_COL32(255,242,187,255):IM_COL32(242,241,255,255),195);
    }
    text("X: Confirm | TRIANGLE / START: Back",cy+59*scale,8.8f,IM_COL32(215,235,239,255),220);
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_PAUSE_TEST")){
        static int old=-1;if(old!=sTagMenuCursor){old=sTagMenuCursor;fprintf(stderr,"tag pause audit: overlay=1 legacy=0 cursor=%d\n",old);}
    }
}
