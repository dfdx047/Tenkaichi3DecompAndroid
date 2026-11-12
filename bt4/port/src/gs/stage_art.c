/* PNG map art expressed as the game's native texture packets. The original
 * selector still draws its frame, cursor, transitions and text in their order. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "../../include/plat_stages.h"
#include "../port_addr.h"
static struct {int tried;unsigned chip,preview;} art[PORT_STAGE_MAX];
/* UI assets must survive rollback/boot-state restoration. Keep their bounded
 * lifetime cache outside both the PS2 heap and the saved low-memory arena. */
static unsigned art_alloc(unsigned bytes){
#ifdef _WIN32
    static uintptr_t next=0x60000000u;
    unsigned aligned=(bytes+0xFFFFu)&~0xFFFFu;
    while(next+aligned<0xE0000000u){
        void *p=VirtualAlloc((void*)next,aligned,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        next+=aligned;if(p)return Port_Addr(p);
    }
#endif
    return 0;
}
static void w32(unsigned char *p,uint32_t n){memcpy(p,&n,4);}
static void w64(unsigned char *p,uint64_t n){memcpy(p,&n,8);}
static unsigned build(SDL_Surface *source,int preview){
    int w=preview?128:64,h=preview?256:64,count=preview?8:1;
    int pixels=w*h*4,packet=0x80+pixels,bytes=0x40+count*0x40+count*packet,i,x,y;
    SDL_Surface *image=SDL_ScaleSurface(source,preview?512:64,preview?512:64,SDL_SCALEMODE_LINEAR);
    if(!image)return 0;
    unsigned addr=art_alloc(bytes);unsigned char *base=Port_Ptr(addr);
    if(!addr){SDL_DestroySurface(image);return 0;}
    memset(base,0,bytes);w32(base,count);w32(base+4,0x10);w32(base+0x10,addr+0x40);
    for(i=0;i<count;++i){
        unsigned char *entry=base+0x40+i*0x40,*p=base+0x40+count*0x40+i*packet;
        int qwc=pixels/16,sx=preview?(i%4)*128:0,sy=preview?(i/4)*256:0;
        w32(entry,1);w32(entry+8,packet);w32(entry+0x10,pixels/256);w32(entry+0x18,w/64);
        w64(entry+0x30,((uint64_t)(w/64)<<14)|((uint64_t)(preview?7:6)<<26)|((uint64_t)(preview?8:6)<<30));
        w32(entry+0x38,Port_Addr(p));
        w32(p+12,0x50000000u|(qwc+7));
        w64(p+0x10,0x1000000000008003ull);w64(p+0x18,0xE);
        w64(p+0x28,0x51);w64(p+0x30,(uint64_t)w|((uint64_t)h<<32));w64(p+0x38,0x52);
        w64(p+0x48,0x53);w64(p+0x50,(uint64_t)qwc|0x8000ull|(2ull<<58));
        for(y=0;y<h;++y)for(x=0;x<w;++x){
            unsigned char *d=p+0x60+(y*w+x)*4;
            const unsigned char *s=(const unsigned char*)image->pixels+(sy+y)*image->pitch+(sx+x)*4;
            memcpy(d,s,3);d[3]=128;
            if(!preview){
                float dx=x-31.5f,dy=y-31.5f,r=sqrtf(dx*dx+dy*dy);
                d[3]=r>31.25f?0:r>30.5f?(unsigned char)((31.25f-r)*128/0.75f):128;
            }
        }
        w64(p+0x60+pixels,0x1000000000008001ull);w64(p+0x68+pixels,0xE);w64(p+0x78+pixels,0x3F);
    }
    SDL_DestroySurface(image);return preview?addr:addr+0x40;
}
unsigned Port_StageArt(int stage,int kind){
    int index=stage-PORT_STAGE_FIRST;char path[600];SDL_Surface *png,*rgba;SDL_PathInfo info;
    if(index<0||index>=PortStages_Count()||kind<0||kind>1)return 0;
    if(!art[index].tried){
        art[index].tried=1;
        if(!PortStages_ImagePath(index,path,sizeof(path))||!SDL_GetPathInfo(path,&info))return 0;
        /* Bound both compressed and decoded size before asking the PNG loader. */
        if(info.size>8*1024*1024)return 0;
        {unsigned char head[24];SDL_IOStream *f=SDL_IOFromFile(path,"rb");
         if(!f)return 0;size_t n=SDL_ReadIO(f,head,sizeof(head));SDL_CloseIO(f);
         if(n!=24||memcmp(head,"\211PNG\r\n\032\n",8))return 0;
         unsigned w=(head[16]<<24)|(head[17]<<16)|(head[18]<<8)|head[19];
         unsigned h=(head[20]<<24)|(head[21]<<16)|(head[22]<<8)|head[23];
         if(!w||!h||w>2048||h>2048)return 0;}
        png=SDL_LoadPNG(path);if(!png){fprintf(stderr,"map art: unreadable PNG, native fallback: %s\n",path);return 0;}
        rgba=SDL_ConvertSurface(png,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(png);if(!rgba)return 0;
        art[index].chip=build(rgba,0);art[index].preview=build(rgba,1);SDL_DestroySurface(rgba);
        fprintf(stderr,"map art: stage=%d native-chip=%d native-preview=%d path=%s\n",stage,art[index].chip!=0,art[index].preview!=0,path);
    }
    return kind?art[index].preview:art[index].chip;
}
