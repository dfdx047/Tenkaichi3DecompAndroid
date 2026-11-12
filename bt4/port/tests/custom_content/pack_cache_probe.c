/* Exercise the production CPU cache through TexPack_Get, without reading a
   disc image or allocating a real multi-megabyte texture working set. */
#include "../../src/gs/gs_texpack.c"
#include <assert.h>

const char *Port_GetEnv(const char *name) { return SDL_getenv(name); }
const uint8_t *Gs_BlockPtr(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e) {
    (void)a;(void)b;(void)c;(void)d;(void)e;return NULL;
}
uint32_t Gs_VramRead(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e) {
    (void)a;(void)b;(void)c;(void)d;(void)e;return 0;
}
uint32_t Gs_PageHash(uint32_t page) { (void)page;return 0; }

static void reset_cache(size_t budget) {
    for (uint32_t i=0;i<sCount;++i) if (sEntries[i].state==2) image_free(&sEntries[i].img);
    free(sEntries);free(sHeld);
    sCount=8;sEntries=calloc(sCount,sizeof(*sEntries));sHeld=calloc(sCount,sizeof(*sHeld));
    assert(sEntries&&sHeld);
    sHeldBytes=sHeldCount=sTick=sHead=sTail=0;sBudget=budget;sSync=0;
}

static void ready(uint32_t id,unsigned age,size_t bytes) {
    TexPackImage image={0};
    image.file=SDL_malloc(1);assert(image.file);
    image.w=image.h=image.levels=1;image.bytes[0]=(uint32_t)bytes;
    sEntries[id].used=age;
    store(id,1,&image);
}

int main(void) {
    TexPackImage image;
    sLock=SDL_CreateMutex();assert(sLock);

    /* 80% residency must stay warm, even after repeated cache hits. */
    reset_cache(100);
    for (uint32_t i=0;i<4;++i) ready(i,i+1,20);
    sTick=4;
    for (unsigned i=0;i<1000;++i) assert(TexPack_Get((int)(i%4),&image)==1);
    assert(sHeldBytes==80&&sHeldCount==4);
    for (unsigned i=0;i<4;++i) assert(sEntries[i].state==2);

    /* At exactly 100%, do not sweep. Loading/missing entries are not residents. */
    ready(4,1,20);sEntries[6].state=1;sEntries[7].state=3;
    assert(TexPack_Get(4,&image)==1);
    assert(sHeldBytes==100&&sHeldCount==5);
    assert(TexPack_Get(6,&image)==0&&TexPack_Get(7,&image)==-1);
    assert(sHeldBytes==100&&sHeldCount==5);

    /* Crossing 100% retires oldest ready images to 75%; keep is protected. */
    ready(5,0,20);sTick=2000;
    sEntries[0].used=10;sEntries[1].used=20;sEntries[2].used=30;
    sEntries[3].used=40;sEntries[4].used=50;
    assert(TexPack_Get(5,&image)==1);
    assert(sHeldBytes==60&&sHeldCount==3);
    assert(sEntries[0].state==0&&sEntries[1].state==0&&sEntries[2].state==0);
    assert(sEntries[3].state==2&&sEntries[4].state==2&&sEntries[5].state==2);
    assert(sEntries[6].state==1&&sEntries[7].state==3);
    assert(image.file==sEntries[5].img.file);

    /* One large current image is retained even if it alone exceeds the cap. */
    reset_cache(100);ready(0,0,120);
    assert(TexPack_Get(0,&image)==1&&sHeldBytes==120&&sHeldCount==1);
    assert(sEntries[0].state==2&&image.file==sEntries[0].img.file);
    assert(TexPack_Get(-1,&image)==-1&&TexPack_Get(8,&image)==-1);
    /* A pending request must release the previous oversized image; otherwise
       the background reader remains blocked with no way to serve it. */
    sEntries[1].state=1;
    assert(TexPack_Get(1,&image)==0 && sHeldBytes==0 && sHeldCount==0);

    reset_cache(100);free(sEntries);free(sHeld);SDL_DestroyMutex(sLock);
    puts("pack_cache: below-budget warm hits, high/low watermarks, LRU order, active/loading safety passed");
    return 0;
}
