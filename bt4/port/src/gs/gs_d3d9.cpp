/* Native D3D9 renderer. Uses offline SM3 shaders, not a software rasterizer.
   Kept behind the explicit test API until the full battle/UI checks pass. */
#ifdef _WIN32
#include <windows.h>
#include <d3d9.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
extern "C" {
#include "gs_internal.h"
#include "gs_draw.h"
#include "ui.h"
extern int Port_Setting(const char *,int),Port_AspectMilli(void),Port_PageAspectMilli(void);
extern unsigned gPortVBlanks;
extern volatile int gUiPresentX,gUiPresentY,gUiPresentW,gUiPresentH;
SDL_Window *GsDraw_WindowCreate(int);
}
#include "generated/shaders_d3d9.h"

namespace {
IDirect3D9 *api;
IDirect3DDevice9 *dev;
SDL_Window *window;
HMODULE library;
D3DPRESENT_PARAMETERS present;
IDirect3DSurface9 *back;
IDirect3DTexture9 *colour[MAX_TARGETS],*aux[MAX_TARGETS],*depth[MAX_TARGETS],*white,*dateCopy,*auxCopy,*movies[2];
IDirect3DSurface9 *colSurf[MAX_TARGETS],*auxSurf[MAX_TARGETS],*depSurf[MAX_TARGETS],*dateSurf,*copySurf;
IDirect3DVertexBuffer9 *gsBuffer,*vuBuffer,*fxBuffer;
IDirect3DIndexBuffer9 *indices;
IDirect3DVertexDeclaration9 *gsDecl,*vuDecl,*fxDecl;
IDirect3DVertexShader9 *vs[4],*fxVs;
IDirect3DPixelShader9 *ps[3],*outlinePs,*keyPs,*dclutPs;
uint32_t pipes[1024];int pipeCount;
bool scene;
unsigned errors;
unsigned gsCapacity,vuCapacity,indexCapacity;
template<class T> void drop(T *&p) { if(p){p->Release();p=nullptr;} }
bool checked(HRESULT hr,const char *where) {
    if(SUCCEEDED(hr))return true;
    if(errors++<20)std::fprintf(stderr,"bt3: D3D9 %s failed: %08lx\n",where,(unsigned long)hr);
    return false;
}
#define DX(call) checked((call),#call)
int width(){return GS_W*SCALE;}int height(){return GS_H*SCALE;}
void begin(){if(!scene)scene=DX(dev->BeginScene());}
void end(){if(scene){DX(dev->EndScene());scene=false;}}
void unbind(){for(unsigned i=0;i<3;i++)dev->SetTexture(i,nullptr);}
void viewport(int w,int h){D3DVIEWPORT9 v={0,0,(DWORD)w,(DWORD)h,0,1};DX(dev->SetViewport(&v));}
IDirect3DTexture9 *rt_texture(IDirect3DSurface9 **surface) {
    IDirect3DTexture9 *t=nullptr;
    if(!DX(dev->CreateTexture(width(),height(),1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&t,nullptr)))return nullptr;
    if(!DX(t->GetSurfaceLevel(0,surface))){drop(t);return nullptr;}return t;
}
void target_drop(int i){drop(colSurf[i]);drop(auxSurf[i]);drop(depSurf[i]);drop(colour[i]);drop(aux[i]);drop(depth[i]);}
void target_ensure(int i){
    if(colour[i])return;
    colour[i]=rt_texture(&colSurf[i]);aux[i]=rt_texture(&auxSurf[i]);
    DX(dev->CreateTexture(width(),height(),1,D3DUSAGE_DEPTHSTENCIL,(D3DFORMAT)MAKEFOURCC('I','N','T','Z'),D3DPOOL_DEFAULT,&depth[i],nullptr));
    if(depth[i])DX(depth[i]->GetSurfaceLevel(0,&depSurf[i]));
}
void target_bind(int i,bool z,bool clear){
    unbind();DX(dev->SetDepthStencilSurface(z?depSurf[i]:nullptr));
    DX(dev->SetRenderTarget(0,colSurf[i]));DX(dev->SetRenderTarget(1,auxSurf[i]));viewport(width(),height());
    if(clear){
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
        DX(dev->Clear(0,nullptr,D3DCLEAR_TARGET|(z?D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL:0),0,0,0));
        DX(dev->SetRenderTarget(1,nullptr));DX(dev->SetRenderTarget(0,auxSurf[i]));
        DX(dev->Clear(0,nullptr,D3DCLEAR_TARGET,0xFFFF0000u,0,0));
        DX(dev->SetRenderTarget(0,colSurf[i]));DX(dev->SetRenderTarget(1,auxSurf[i]));
    }
}
GsTex target_colour(int i){return (GsTex)(uintptr_t)colour[i];}
GsTex target_aux(int i){return (GsTex)(uintptr_t)aux[i];}
GsTex target_depth(int i){return (GsTex)(uintptr_t)depth[i];}
GsTex tex_create(uint32_t w,uint32_t h,int format,int levels){
    static const D3DFORMAT formats[]={D3DFMT_A8R8G8B8,D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5};
    IDirect3DTexture9 *t=nullptr;
    if(!DX(dev->CreateTexture(w,h,levels<1?1:levels,0,formats[format&3],D3DPOOL_MANAGED,&t,nullptr)))return 0;
    return (GsTex)(uintptr_t)t;
}
int tex_upload(GsTex handle,int level,uint32_t w,uint32_t h,int format,const void *data,uint32_t bytes){
    IDirect3DTexture9 *t=(IDirect3DTexture9 *)(uintptr_t)handle;D3DLOCKED_RECT locked={};
    if(!t||!DX(t->LockRect(level,&locked,nullptr,0))){std::free((void *)data);return 0;}
    const unsigned char *src=(const unsigned char *)data;
    if(format==GS_TEXFMT_RGBA8){
        for(unsigned y=0;y<h;y++){unsigned char *dst=(unsigned char *)locked.pBits+y*locked.Pitch;
            for(unsigned x=0;x<w;x++){dst[4*x]=src[4*x+2];dst[4*x+1]=src[4*x+1];dst[4*x+2]=src[4*x];dst[4*x+3]=src[4*x+3];}src+=w*4;}
    }else{
        unsigned stride=((w+3)/4)*(format==GS_TEXFMT_BC1?8:16),rows=(h+3)/4;
        if((uint64_t)stride*rows>bytes){t->UnlockRect(level);std::free((void *)data);return 0;}
        for(unsigned y=0;y<rows;y++)std::memcpy((unsigned char *)locked.pBits+y*locked.Pitch,src+y*stride,stride);
    }
    DX(t->UnlockRect(level));std::free((void *)data);return 1;
}
void tex_destroy(GsTex t){if(t)((IDirect3DTexture9 *)(uintptr_t)t)->Release();}
int slots(){return 1<<20;}GsTex white_tex(){return (GsTex)(uintptr_t)white;}
void sampler(int slot,IDirect3DTexture9 *t,int key){
    dev->SetTexture(slot,t?t:white);
    dev->SetSamplerState(slot,D3DSAMP_MINFILTER,key&1?D3DTEXF_LINEAR:D3DTEXF_POINT);
    dev->SetSamplerState(slot,D3DSAMP_MAGFILTER,key&1?D3DTEXF_LINEAR:D3DTEXF_POINT);
    dev->SetSamplerState(slot,D3DSAMP_MIPFILTER,key&8?D3DTEXF_LINEAR:D3DTEXF_NONE);
    dev->SetSamplerState(slot,D3DSAMP_ADDRESSU,key&2?D3DTADDRESS_CLAMP:D3DTADDRESS_WRAP);
    dev->SetSamplerState(slot,D3DSAMP_ADDRESSV,key&4?D3DTADDRESS_CLAMP:D3DTADDRESS_WRAP);
}
void textures(IDirect3DTexture9 *a,int sa,IDirect3DTexture9 *b,int sb,IDirect3DTexture9 *c=nullptr,int sc=6){
    IDirect3DTexture9 *ts[]={a,b,c};int keys[]={sa,sb,sc};float dims[12]={};
    for(int i=0;i<3;i++){sampler(i,ts[i],keys[i]);D3DSURFACE_DESC desc={};if(ts[i])ts[i]->GetLevelDesc(0,&desc);
        dims[4*i]=(float)(desc.Width?desc.Width:1);dims[4*i+1]=(float)(desc.Height?desc.Height:1);}
    dev->SetPixelShaderConstantF(5,dims,3);
}
void scissor(const int *s){RECT rect={s[0],s[1],s[0]+s[2],s[1]+s[3]};dev->SetScissorRect(&rect);dev->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);}
DWORD factor(int code,int c){
    if(code==0)return D3DBLEND_ZERO;if(code==1||code==5)return D3DBLEND_ONE;
    if(code==4)return c==0?D3DBLEND_INVSRCALPHA:c==1?D3DBLEND_INVDESTALPHA:D3DBLEND_INVBLENDFACTOR;
    return c==0?D3DBLEND_SRCALPHA:c==1?D3DBLEND_DESTALPHA:D3DBLEND_BLENDFACTOR;
}
void blend(uint32_t key,float constant){
    int a=(key>>1)&7,b=(key>>4)&7,c=(key>>7)&3;DWORD byte=(DWORD)(constant*255+.5f);if(byte>255)byte=255;
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE,key&1?TRUE:FALSE);
    dev->SetRenderState(D3DRS_SRCBLEND,key&(1<<9)?D3DBLEND_ONE:factor(a,c));dev->SetRenderState(D3DRS_DESTBLEND,factor(b,c));
    dev->SetRenderState(D3DRS_BLENDOP,a==3?D3DBLENDOP_REVSUBTRACT:b==3?D3DBLENDOP_SUBTRACT:D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_BLENDFACTOR,byte*0x01010101u);dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,TRUE);
    dev->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ONE);dev->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ZERO);dev->SetRenderState(D3DRS_BLENDOPALPHA,D3DBLENDOP_ADD);
}
void zstate(uint32_t key,bool effect){
    static const DWORD tests[]={D3DCMP_NEVER,D3DCMP_ALWAYS,D3DCMP_GREATEREQUAL,D3DCMP_GREATER};
    dev->SetRenderState(D3DRS_ZENABLE,effect?FALSE:TRUE);dev->SetRenderState(D3DRS_ZWRITEENABLE,!effect&&((key>>12)&1));dev->SetRenderState(D3DRS_ZFUNC,tests[(key>>10)&3]);
    bool stencil=!effect&&(key&(1<<24));dev->SetRenderState(D3DRS_STENCILENABLE,stencil);
    if(stencil){dev->SetRenderState(D3DRS_STENCILREF,0);dev->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL);
        dev->SetRenderState(D3DRS_STENCILMASK,255);dev->SetRenderState(D3DRS_STENCILWRITEMASK,255);
        dev->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP);dev->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);dev->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_INCRSAT);}
}
int pipe_get(uint32_t key){for(int i=0;i<pipeCount;i++)if(pipes[i]==key)return i;if(pipeCount==1024)return 0;pipes[pipeCount]=key;return pipeCount++;}
void params(const GsDraw &d,uint32_t key,bool effect){
    float data[20]={};
    if(effect){data[0]=(float)d.src;data[1]=d.misc[0];}
    else{
        for(int i=0;i<4;i++)data[i]=(float)d.mode[i];
        std::memcpy(data+4,d.misc,16);std::memcpy(data+8,d.rect,16);std::memcpy(data+12,d.orig,16);
        data[16]=d.vu?std::fmax(gsVuUni[d.uniform].misc[2],0.0f):gsZMax;data[17]=(key&(1<<9))?1.0f:0.0f;
        data[18]=d.portrait[0];data[19]=d.portrait[1];
    }
    dev->SetPixelShaderConstantF(0,data,5);
}
void issue(const GsDraw &d,uint32_t key,bool full){
    if(full){dev->SetVertexDeclaration(fxDecl);dev->SetStreamSource(0,fxBuffer,0,8);DX(dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1));}
    else if(d.vu){dev->SetVertexDeclaration(vuDecl);dev->SetStreamSource(0,vuBuffer,0,48);dev->SetIndices(indices);
        DX(dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,gsVuVertCount,d.first,d.count/3));}
    else{unsigned topo=(key>>13)&3;D3DPRIMITIVETYPE type=topo==0?D3DPT_TRIANGLELIST:topo==1?D3DPT_LINELIST:D3DPT_POINTLIST;
        dev->SetVertexDeclaration(gsDecl);dev->SetStreamSource(0,gsBuffer,0,sizeof(Vtx));DX(dev->DrawPrimitive(type,d.first,d.count/(topo==0?3:topo==1?2:1)));}
}
/* D3D9 has per-target masks but a shared blend equation. Write the aux byte
   first without blending or depth/stencil writes, then the colour normally. */
void draw(const GsDraw &d,uint32_t key,bool full){
    DWORD mask=(key>>16)&15;bool auxWrite=(mask&8)!=0;
    dev->SetRenderTarget(1,auxSurf[d.target]);
    if((key&1)&&auxWrite){
        dev->SetRenderState(D3DRS_COLORWRITEENABLE,0);dev->SetRenderState(D3DRS_COLORWRITEENABLE1,D3DCOLORWRITEENABLE_RED);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);dev->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);dev->SetRenderState(D3DRS_STENCILWRITEMASK,0);
        issue(d,key,full);blend(key,d.blendc);zstate(key,full);auxWrite=false;
    }
    if(key&1)dev->SetRenderTarget(1,nullptr);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,mask);dev->SetRenderState(D3DRS_COLORWRITEENABLE1,auxWrite?D3DCOLORWRITEENABLE_RED:0);
    issue(d,key,full);
}
void copies(){dateCopy=rt_texture(&dateSurf);auxCopy=rt_texture(&copySurf);}
void scale_changed(){end();unbind();dev->SetRenderTarget(1,nullptr);dev->SetRenderTarget(0,back);dev->SetDepthStencilSurface(nullptr);
    for(int i=0;i<gsTargetCount;i++)target_drop(i);drop(dateSurf);drop(copySurf);drop(dateCopy);drop(auxCopy);copies();}
void copy_surface(IDirect3DSurface9 *source,IDirect3DSurface9 *dest,const RECT *src=nullptr,const RECT *dst=nullptr,D3DTEXTUREFILTERTYPE filter=D3DTEXF_NONE){
    end();unbind();DX(dev->SetRenderTarget(1,nullptr));DX(dev->SetRenderTarget(0,back));DX(dev->SetDepthStencilSurface(nullptr));
    DX(dev->StretchRect(source,src,dest,dst,filter));begin();
}
int read_target(int i,uint8_t *rgba){
    if(!dev||!colSurf[i])return 0;end();IDirect3DSurface9 *read=nullptr;
    if(!DX(dev->CreateOffscreenPlainSurface(width(),height(),D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr)))return 0;
    if(!DX(dev->GetRenderTargetData(colSurf[i],read))){drop(read);return 0;}D3DLOCKED_RECT lock={};
    if(!DX(read->LockRect(&lock,nullptr,D3DLOCK_READONLY))){drop(read);return 0;}
    for(int y=0;y<448;y++)for(int x=0;x<512;x++){
        const uint8_t *p=(const uint8_t *)lock.pBits+(y*SCALE+SCALE/2)*lock.Pitch+(x*SCALE+SCALE/2)*4;uint8_t *q=rgba+(y*512+x)*4;
        q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=p[3];}
    read->UnlockRect();drop(read);return 1;
}
void target_copy(int a,int b){if(!colSurf[a]||!colSurf[b])return;copy_surface(colSurf[a],colSurf[b]);end();}
void screenshot(int best){
    static bool done;bool shot=false;
    if(getenv("BT3_SHOT_VBLANK")&&!done&&gPortVBlanks>=(unsigned)atoi(getenv("BT3_SHOT_VBLANK"))){done=true;shot=true;}
    else if(!getenv("BT3_SHOT_VBLANK")&&getenv("BT3_SHOT")&&atoi(getenv("BT3_SHOT"))>0&&gGsFrame%atoi(getenv("BT3_SHOT"))==0)shot=true;
    if(!shot)return;std::vector<uint8_t> pixels(512*448*4);if(!read_target(best,pixels.data()))return;
    char path[128];std::snprintf(path,sizeof(path),"port/build/shots/gpu_%05u.ppm",gGsFrame);FILE *f=std::fopen(path,"wb");if(!f)return;
    std::fprintf(f,"P6\n512 448\n255\n");for(int i=0;i<512*448;i++)std::fwrite(pixels.data()+4*i,1,3,f);std::fclose(f);
    std::fprintf(stderr,"bt3: D3D9 screenshot at vertical blank %u = frame %u\n",gPortVBlanks,gGsFrame);
}
void window_shot(){
    static bool done;
    const char *at=getenv("BT4_DX9_WINDOWSHOT");
    if(done||!getenv("BT4_RENDER_TEST")||!at||gPortVBlanks<(unsigned)atoi(at))return;
    done=true;IDirect3DSurface9 *read=nullptr;int w=present.BackBufferWidth,h=present.BackBufferHeight;
    if(!DX(dev->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr)))return;
    if(!DX(dev->GetRenderTargetData(back,read))){drop(read);return;}D3DLOCKED_RECT lock={};
    if(!DX(read->LockRect(&lock,nullptr,D3DLOCK_READONLY))){drop(read);return;}
    char path[128];std::snprintf(path,sizeof(path),"port/build/shots/window_%05u.ppm",gGsFrame);
    if(FILE *f=std::fopen(path,"wb")){
        std::fprintf(f,"P6\n%d %d\n255\n",w,h);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            const unsigned char *p=(const unsigned char *)lock.pBits+y*lock.Pitch+x*4;unsigned char rgb[]={p[2],p[1],p[0]};std::fwrite(rgb,1,3,f);
        }
        std::fclose(f);
    }
    read->UnlockRect();drop(read);
}
void static_state(){dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_FOGENABLE,FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);dev->SetRenderState(D3DRS_DITHERENABLE,FALSE);dev->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);}
bool buffers();
bool reset(int w,int h){
    end();unbind();Ui_D3D9Reset(0);dev->SetRenderTarget(1,nullptr);dev->SetRenderTarget(0,back);dev->SetDepthStencilSurface(nullptr);
    for(int i=0;i<MAX_TARGETS;i++)target_drop(i);
    drop(dateSurf);drop(copySurf);drop(dateCopy);drop(auxCopy);drop(gsBuffer);drop(vuBuffer);drop(indices);drop(back);
    present.BackBufferWidth=w;present.BackBufferHeight=h;
    HRESULT result=dev->Reset(&present);
    if(FAILED(result)){std::fprintf(stderr,"bt3: D3D9 reset pending: %08lx\n",(unsigned long)result);return false;}
    if(!DX(dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||!buffers())return false;
    copies();for(int i=0;i<gsTargetCount;i++){target_ensure(i);gsTargets[i].cleared=0;gsTargets[i].gen++;}
    Ui_D3D9Reset(1);std::fprintf(stderr,"bt3: D3D9 resized to %d x %d\n",w,h);return true;
}
void movie_draw(int index,int target){
    if(!movies[index])return;target_bind(target,false,!gsTargets[target].cleared);
    zstate(0,true);blend(0,0);dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,15);dev->SetRenderState(D3DRS_COLORWRITEENABLE1,0);
    dev->SetVertexShader(vs[0]);dev->SetPixelShader(ps[1]);dev->SetVertexDeclaration(gsDecl);
    float p[20]={2,1,1,0};p[10]=p[11]=1;dev->SetPixelShaderConstantF(0,p,5);textures(movies[index],7,nullptr,6);
    const Vtx vertices[]={
      {0,0,0,128,128,128,128,0,0,1},{(float)GS_W,0,0,128,128,128,128,1,0,1},{0,(float)GS_H,0,128,128,128,128,0,1,1},
      {(float)GS_W,0,0,128,128,128,128,1,0,1},{(float)GS_W,(float)GS_H,0,128,128,128,128,1,1,1},{0,(float)GS_H,0,128,128,128,128,0,1,1}};
    DX(dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,vertices,sizeof(Vtx)));gsTargets[target].cleared=1;
}
bool reserve_vertices(IDirect3DVertexBuffer9 *&buffer,unsigned &capacity,unsigned needed,unsigned limit){
    if(needed<=capacity)return true;if(needed>limit)return false;
    unsigned next=capacity?capacity:65536;while(next<needed)next=next>limit/2?limit:next*2;
    IDirect3DVertexBuffer9 *replacement=nullptr;
    if(!DX(dev->CreateVertexBuffer(next,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&replacement,nullptr)))return false;
    drop(buffer);buffer=replacement;capacity=next;return true;
}
bool reserve_indices(unsigned needed){
    if(needed<=indexCapacity)return true;if(needed>MAX_VU_IDX*4)return false;
    unsigned next=indexCapacity?indexCapacity:65536;while(next<needed)next=next>MAX_VU_IDX*2?MAX_VU_IDX*4:next*2;
    IDirect3DIndexBuffer9 *replacement=nullptr;
    if(!DX(dev->CreateIndexBuffer(next,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_DEFAULT,&replacement,nullptr)))return false;
    drop(indices);indices=replacement;indexCapacity=next;return true;
}
bool upload(IDirect3DVertexBuffer9 *buffer,const void *data,unsigned bytes){if(!bytes)return true;void *p=nullptr;if(!DX(buffer->Lock(0,bytes,&p,D3DLOCK_DISCARD)))return false;std::memcpy(p,data,bytes);return DX(buffer->Unlock());}
struct RecordedFrame {
    ~RecordedFrame(){
        for(int i=0;i<gsTargetCount;i++)gsTargets[i].draws=0;
        gsVertCount=gsVuVertCount=gsVuIdxCount=gsVuUniCount=gsDrawCount=0;
        gsAnchor=0;gsSkipped=gsNative=0;
    }
};
void frame(){
    RecordedFrame resetCounters;
    if(!dev)return;GsGpu_TitlePoll();SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_EVENT_QUIT)std::exit(0);
        if(PortInput_Hotkey(&ev)){Ui_Toggle();continue;}if(Ui_Event(&ev))continue;
        if(ev.type==SDL_EVENT_KEY_DOWN&&!ev.key.repeat&&ev.key.key==SDLK_ESCAPE&&!Ui_QuitAsk())std::exit(0);
        if(ev.type==SDL_EVENT_KEY_DOWN&&!ev.key.repeat&&ev.key.key==SDLK_F11)GsDraw_FullscreenToggle();
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_DX9_RESIZE_TEST")){
        static int resized;
        if(resized==0&&gPortVBlanks>=180){SDL_SetWindowSize(window,800,450);resized=1;}
        else if(resized==1&&gPortVBlanks>=360){SDL_SetWindowSize(window,640,360);resized=2;}
    }
    int windowW,windowH;SDL_GetWindowSizeInPixels(window,&windowW,&windowH);
    HRESULT cooperative=dev->TestCooperativeLevel();
    if(cooperative==D3DERR_DEVICELOST||windowW<1||windowH<1)return;
    if(cooperative==D3DERR_DEVICENOTRESET||windowW!=(int)present.BackBufferWidth||windowH!=(int)present.BackBufferHeight){reset(windowW,windowH);return;}
    if(FAILED(cooperative))return;
    if(!reserve_vertices(gsBuffer,gsCapacity,gsVertCount*sizeof(Vtx),MAX_VERTS*sizeof(Vtx)) ||
       !reserve_vertices(vuBuffer,vuCapacity,gsVuVertCount*48,MAX_VU_VERTS*48) || !reserve_indices(gsVuIdxCount*4))return;
    if(!upload(gsBuffer,gsVerts,gsVertCount*sizeof(Vtx))||!upload(vuBuffer,gsVuVerts,gsVuVertCount*48))return;
    if(gsVuIdxCount){void *p=nullptr;if(!DX(indices->Lock(0,gsVuIdxCount*4,&p,D3DLOCK_DISCARD)))return;std::memcpy(p,gsVuIdx,gsVuIdxCount*4);indices->Unlock();}
    for(int i=0;i<2;i++)if(gGsFbUploads&(1u<<i)){
        if(!movies[i])movies[i]=(IDirect3DTexture9 *)(uintptr_t)tex_create(GS_W,GS_H,0,1);
        uint32_t *pixels=(uint32_t *)std::malloc((size_t)GS_W*GS_H*4);
        if(pixels){for(int y=0;y<GS_H;y++)for(int x=0;x<GS_W;x++)pixels[y*GS_W+x]=Gs_VramRead((i?0x70:0)*32,8,0,x,y)|0xFF000000u;
            tex_upload((GsTex)(uintptr_t)movies[i],0,GS_W,GS_H,0,pixels,GS_W*GS_H*4);}
    }
    gGsFbUploads=0;
    static_state();float half[]={1.0f/width(),1.0f/height(),0,0};dev->SetVertexShaderConstantF(20,half,1);
    begin();int current=-1;
    for(unsigned n=0;n<gsDrawCount;n++){
        const GsDraw &d=gsDraws[n];if(!colSurf[d.target])continue;
        if(d.native==4||d.native==5){if(!gsTargets[d.target].cleared)continue;
            copy_surface(auxSurf[d.target],d.native==4?copySurf:dateSurf);current=-1;continue;}
        if(d.native>=100){if(d.native<102)movie_draw(d.native-100,d.target);current=-1;continue;}
        uint32_t key=(d.pipeline>=0&&d.pipeline<pipeCount)?pipes[d.pipeline]:0;bool effect=d.native!=0;
        if(effect&&!gsTargets[d.target].cleared)continue;
        if(current!=d.target){target_bind(d.target,!effect,!gsTargets[d.target].cleared);current=d.target;gsTargets[d.target].cleared=1;}
        else dev->SetDepthStencilSurface(effect?nullptr:depSurf[d.target]);
        if(!d.native&&(key&(1<<24))&&(n==0||gsDraws[n-1].native||gsDraws[n-1].target!=d.target||!(pipes[gsDraws[n-1].pipeline]&(1<<24)))){
            dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);DX(dev->Clear(0,nullptr,D3DCLEAR_STENCIL,0,0,0));}
        zstate(key,effect);blend(key,d.blendc);scissor(d.scissor);
        if(d.native==1||d.native==2){
            dev->SetRenderTarget(1,nullptr);
            dev->SetVertexShader(fxVs);dev->SetPixelShader(d.native==1?outlinePs:keyPs);
            float p[]={1333.333f/Port_PageAspectMilli()/GS_W,1.0f/GS_H,100.0f/255,0};dev->SetPixelShaderConstantF(0,p,1);
            textures(auxCopy,6,nullptr,6);
            dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);dev->SetRenderState(D3DRS_SRCBLEND,d.native==1?D3DBLEND_ONE:D3DBLEND_SRCALPHA);
            dev->SetRenderState(D3DRS_DESTBLEND,d.native==1?D3DBLEND_ONE:D3DBLEND_INVSRCALPHA);
            dev->SetRenderState(D3DRS_BLENDOP,d.native==1?D3DBLENDOP_REVSUBTRACT:D3DBLENDOP_ADD);
            dev->SetRenderState(D3DRS_COLORWRITEENABLE,7);dev->SetRenderState(D3DRS_COLORWRITEENABLE1,0);issue(d,key,true);continue;
        }
        if(d.native==3){dev->SetVertexShader(fxVs);dev->SetPixelShader(dclutPs);textures(depth[d.target],6,auxCopy,6,(IDirect3DTexture9 *)(uintptr_t)d.tex,6);}
        else{
            dev->SetVertexShader(vs[d.vu]);dev->SetPixelShader(ps[(key&(1<<25))?2:(key&(1<<23))?1:0]);
            if(d.vu)dev->SetVertexShaderConstantF(0,(const float *)&gsVuUni[d.uniform],sizeof(Vu0Uniform)/16);
            textures((IDirect3DTexture9 *)(uintptr_t)d.tex,d.sampler,dateCopy,6);
        }
        params(d,key,d.native==3);draw(d,key,effect);
    }
    end();int best=gGsMainFbp>=0?GsDraw_TargetGet((uint32_t)gGsMainFbp,0):-1;
    if(best<0&&gsTargetCount)best=0;if(best<0||!colSurf[best])return;screenshot(best);
    unbind();dev->SetDepthStencilSurface(nullptr);dev->SetRenderTarget(1,nullptr);dev->SetRenderTarget(0,back);
    int sw,sh;SDL_GetWindowSizeInPixels(window,&sw,&sh);float aspect=Port_AspectMilli()/1000.0f;
    int w=sw,h=(int)(sw/aspect+.5f);if(h>sh){h=sh;w=(int)(sh*aspect+.5f);}int x=(sw-w)/2,y=(sh-h)/2;
    RECT src={0,0,512*SCALE,448*SCALE},dest={x,y,x+w,y+h};dev->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    DX(dev->Clear(0,nullptr,D3DCLEAR_TARGET,0,0,0));DX(dev->StretchRect(colSurf[best],&src,back,&dest,D3DTEXF_LINEAR));
    gUiPresentX=x;gUiPresentY=y;gUiPresentW=w;gUiPresentH=h;
    viewport(sw,sh);begin();Ui_DrawD3D9();end();window_shot();DX(dev->Present(nullptr,nullptr,nullptr,nullptr));
    if(getenv("BT3_GS_VERBOSE")&&gGsFrame%60==0)std::fprintf(stderr,"bt3: D3D9 frame=%u draws=%u errors=%u\n",gGsFrame,gsDrawCount,errors);
}
bool buffers(){
    gsCapacity=vuCapacity=indexCapacity=0;
    return reserve_vertices(gsBuffer,gsCapacity,16384*sizeof(Vtx),MAX_VERTS*sizeof(Vtx)) &&
           reserve_vertices(vuBuffer,vuCapacity,65536*48,MAX_VU_VERTS*48) && reserve_indices(131072*4);
}
int init_impl(){
    if(!SDL_Init(SDL_INIT_VIDEO))return 0;window=GsDraw_WindowCreate(0);if(!window)return 0;
    HWND hwnd=(HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
    library=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto create=library?(IDirect3D9 *(WINAPI *)(UINT))GetProcAddress(library,"Direct3DCreate9"):nullptr;
    if(!create)return 0;api=create(D3D_SDK_VERSION);D3DCAPS9 caps={};if(!api||!DX(api->GetDeviceCaps(0,D3DDEVTYPE_HAL,&caps)))return 0;
    if(caps.PixelShaderVersion<D3DPS_VERSION(3,0)||caps.VertexShaderVersion<D3DVS_VERSION(3,0)||caps.NumSimultaneousRTs<2||width()>(int)caps.MaxTextureWidth||height()>(int)caps.MaxTextureHeight)return 0;
    if(FAILED(api->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_TEXTURE,(D3DFORMAT)MAKEFOURCC('I','N','T','Z')))){
        std::fprintf(stderr,"bt3: D3D9 requires sampleable INTZ depth for the game's depth effects\n");return 0;
    }
    int w,h;SDL_GetWindowSizeInPixels(window,&w,&h);present={};present.Windowed=TRUE;present.SwapEffect=D3DSWAPEFFECT_DISCARD;present.hDeviceWindow=hwnd;
    present.BackBufferWidth=w;present.BackBufferHeight=h;present.BackBufferFormat=D3DFMT_A8R8G8B8;present.PresentationInterval=Port_Setting("vsync",0)?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_IMMEDIATE;
    if(!DX(api->CreateDevice(0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_HARDWARE_VERTEXPROCESSING,&present,&dev)))return 0;
    DX(dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
    const DWORD *vertex[]={kGsVert,kVu0Vert,kVu4Vert,kVu6Vert};for(int i=0;i<4;i++)if(!DX(dev->CreateVertexShader(vertex[i],&vs[i])))return 0;
    const DWORD *fragment[]={kGsFrag,kGsnzFrag,kGswbFrag};for(int i=0;i<3;i++)if(!DX(dev->CreatePixelShader(fragment[i],&ps[i])))return 0;
    if(!DX(dev->CreateVertexShader(kFxVert,&fxVs))||!DX(dev->CreatePixelShader(kOutlineFrag,&outlinePs))||!DX(dev->CreatePixelShader(kAlphakeyFrag,&keyPs))||!DX(dev->CreatePixelShader(kDclutFrag,&dclutPs)))return 0;
    D3DVERTEXELEMENT9 g[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_TEXCOORD,0},{0,12,D3DDECLTYPE_UBYTE4N,0,D3DDECLUSAGE_TEXCOORD,1},{0,16,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_TEXCOORD,2},D3DDECL_END()};
    D3DVERTEXELEMENT9 v[]={{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_TEXCOORD,0},{0,16,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_TEXCOORD,1},{0,32,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_TEXCOORD,2},D3DDECL_END()};
    D3DVERTEXELEMENT9 f[]={{0,0,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    if(!DX(dev->CreateVertexDeclaration(g,&gsDecl))||!DX(dev->CreateVertexDeclaration(v,&vuDecl))||!DX(dev->CreateVertexDeclaration(f,&fxDecl))||!buffers())return 0;
    DX(dev->CreateVertexBuffer(24,0,0,D3DPOOL_MANAGED,&fxBuffer,nullptr));void *p=nullptr;if(!fxBuffer||!DX(fxBuffer->Lock(0,24,&p,0)))return 0;
    const float xy[]={0,0,2,0,0,2};std::memcpy(p,xy,24);fxBuffer->Unlock();
    white=(IDirect3DTexture9 *)(uintptr_t)tex_create(1,1,0,1);uint32_t *pixel=(uint32_t *)std::malloc(4);if(!pixel)return 0;*pixel=~0u;tex_upload((GsTex)(uintptr_t)white,0,1,1,0,pixel,4);copies();
    if(!white||!dateCopy||!auxCopy)return 0;
    D3DADAPTER_IDENTIFIER9 id={};api->GetAdapterIdentifier(0,0,&id);
    if(!Ui_InitD3D9(window,dev,id.Description))return 0;
    std::fprintf(stderr,"bt3: GPU renderer: direct3d9\n");return 1;
}
int init(){
    if(init_impl())return 1;
    if(dev){end();unbind();dev->SetRenderTarget(1,nullptr);dev->SetDepthStencilSurface(nullptr);}
    for(int i=0;i<MAX_TARGETS;i++)target_drop(i);
    drop(back);drop(dateSurf);drop(copySurf);drop(dateCopy);drop(auxCopy);drop(white);
    drop(gsBuffer);drop(vuBuffer);drop(fxBuffer);drop(indices);drop(gsDecl);drop(vuDecl);drop(fxDecl);
    for(int i=0;i<4;i++)drop(vs[i]);for(int i=0;i<3;i++)drop(ps[i]);
    drop(fxVs);drop(outlinePs);drop(keyPs);drop(dclutPs);drop(dev);drop(api);
    if(library){FreeLibrary(library);library=nullptr;}
    if(window){GsDraw_WindowDestroy();window=nullptr;}
    std::fprintf(stderr,"bt3: DirectX 9 could not initialize on this device\n");return 0;
}
} // namespace
extern "C" void *GsDx9_StripTexture(const void *rgba,int w,int h){
    if(!dev)return nullptr;GsTex t=tex_create(w,h,0,1);if(!t)return nullptr;size_t bytes=(size_t)w*h*4;
    void *copy=std::malloc(bytes);if(!copy){tex_destroy(t);return nullptr;}std::memcpy(copy,rgba,bytes);
    if(!tex_upload(t,0,w,h,0,copy,(uint32_t)bytes)){tex_destroy(t);return nullptr;}return (void *)(uintptr_t)t;
}
extern "C" void GsDx9_StripTextureFree(void *p){tex_destroy((GsTex)(uintptr_t)p);}
extern "C" GsBackend sDirect3D9Backend={"direct3d9",init,frame,scale_changed,white_tex,tex_create,tex_upload,tex_destroy,slots,target_ensure,target_drop,target_colour,target_aux,target_depth,pipe_get,read_target,target_copy};
#endif
