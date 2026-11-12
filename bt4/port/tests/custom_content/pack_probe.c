#include "../../src/gs/gs_texpack.c"
#include <assert.h>
const char *Port_GetEnv(const char *name){return SDL_getenv(name);}
/* The probe selects actual entries by ID; live-GS matching is tested in game. */
const uint8_t *Gs_BlockPtr(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return NULL;}
uint32_t Gs_VramRead(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
uint32_t Gs_PageHash(uint32_t page){(void)page;return 0;}
int main(int argc,char **argv){
    /* The Time Skip backdrop is ordinary content, including replacement packs.
       Test ingestion directly so a restored effect cannot be silently excluded. */
    add("", "c3b168f19eccae01-1234-00006654.png", -1, 0);
    assert(sCount == 1 && sEntries[0].tex == UINT64_C(0xc3b168f19eccae01));
    free(sEntries[0].path);
    sCount = 0;
    assert(argc==2);SDL_setenv_unsafe("BT3_TEXTURES",argv[1],1);TexPack_Init();assert(sCount>0);
    /* Lookup keeps every content key except draw-time TCC. Exact variants
       still win when a pack intentionally supplies both alpha modes. */
    for(uint32_t i=0;i<sCount;i++) {
        Entry *e=&sEntries[i];
        int exact=find(e->tex,e->clut,e->bits,e->pal);
        assert(exact>=0 && find_image(e->tex,e->clut,e->bits,e->pal)==exact);
        int alternate=find(e->tex,e->clut,e->bits^0x4000u,e->pal);
        assert(find_image(e->tex,e->clut,e->bits^0x4000u,e->pal)==(alternate<0?exact:alternate));
        if(find(e->tex,e->clut^1,e->bits,e->pal)<0 && find(e->tex,e->clut^1,e->bits^0x4000u,e->pal)<0)
            assert(find_image(e->tex,e->clut^1,e->bits,e->pal)<0);
        if(find(e->tex,e->clut,e->bits^0x40u,e->pal)<0 && find(e->tex,e->clut,e->bits^0x4040u,e->pal)<0)
            assert(find_image(e->tex,e->clut,e->bits^0x40u,e->pal)<0);
    }
    unsigned checked=0;int beyond2gb=0;
    for(uint32_t i=0;i<sCount;i+=(sCount>32?sCount/31:1)){
        TexPackImage image;assert(load(&sEntries[i],&image));assert(image.w&&image.h&&image.levels);
        if(sEntries[i].archive>=0){mz_zip_archive_file_stat st;assert(mz_zip_reader_file_stat(&sZip[sEntries[i].archive].zip,sEntries[i].member,&st));beyond2gb|=st.m_local_header_ofs>UINT64_C(2147483648);}
        image_free(&image);checked++;
    }
    printf("pack_entries=%u sampled_images=%u read_beyond_2gb=%d\n",sCount,checked,beyond2gb);
    return 0;
}
