/* Real asynchronous decoder, with no renderer consuming images for a while. */
#include "../../src/gs/gs_texpack.c"
#include <assert.h>
const char *Port_GetEnv(const char *name){return SDL_getenv(name);}
const uint8_t *Gs_BlockPtr(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return NULL;}
uint32_t Gs_VramRead(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
uint32_t Gs_PageHash(uint32_t page){(void)page;return 0;}
int main(void){
    uint32_t header[32]={0};uint8_t pixels[65536];TexPackImage image;
    FILE *file=fopen("async-texture.dds","wb");assert(file);
    memcpy(header,"DDS ",4);header[1]=124;header[3]=header[4]=128;header[7]=1;
    header[19]=32;header[22]=32;header[23]=255;
    memset(pixels,0x7c,sizeof(pixels));
    assert(fwrite(header,1,sizeof(header),file)==sizeof(header));
    assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));fclose(file);
    sCount=64;sEntries=calloc(sCount,sizeof(*sEntries));sHeld=calloc(sCount,sizeof(*sHeld));
    assert(sEntries&&sHeld);
    sSync=0;sBudget=2*sizeof(pixels);sLock=SDL_CreateMutex();sWake=SDL_CreateCondition();
    assert(sLock&&sWake);
    for(uint32_t i=0;i<sCount;++i){sEntries[i].path="async-texture.dds";sEntries[i].archive=-1;}
    sReader=SDL_CreateThread(reader,"pack-test-reader",NULL);assert(sReader);
    for(uint32_t i=0;i<sCount;++i)assert(TexPack_Get(i,&image)>=0);
    SDL_Delay(300); /* Simulates a renderer that has stopped consuming frames. */
    SDL_LockMutex(sLock);
    assert(sHeldBytes<=sBudget+sizeof(pixels));
    assert(sHead!=sTail); /* The reader parked instead of decoding the backlog. */
    SDL_UnlockMutex(sLock);
    for(uint32_t i=0;i<sCount;++i){
        uint64_t deadline=SDL_GetTicks()+5000;int got;
        while((got=TexPack_Get(i,&image))==0 && SDL_GetTicks()<deadline)SDL_Delay(1);
        assert(got==1&&image.bytes[0]==sizeof(pixels));
        /* The worker never evicts this borrowed pointer while the caller copies. */
        SDL_Delay(1);
        assert(image.data[0][0]==0x7c&&image.data[0][sizeof(pixels)-1]==0x7c);
    }
    SDL_LockMutex(sLock);assert(sHeldBytes<=sBudget+sizeof(pixels));SDL_UnlockMutex(sLock);
    remove("async-texture.dds");
    puts("pack_async: paused consumer stays bounded, pending requests wake the reader, borrowed data stays valid PASS");
    /* Process exit retires the blocked test worker; it owns no external state. */
    return 0;
}
