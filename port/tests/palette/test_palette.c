#include <assert.h>
#include <stdio.h>
#include "../../src/gs/gs_palette.h"
static uint32_t palette[16][16];
static uint32_t read_palette(uint32_t base,uint32_t width,uint32_t format,uint32_t x,uint32_t y) {
    assert(base==0&&width==1&&format==0&&x<16&&y<16);
    return palette[y][x];
}
int main(void) {
    uint32_t base,i,x,y;
    for(y=0;y<16;++y)for(x=0;x<16;++x)palette[y][x]=0x80000000u|(y*16+x);
    base=Gs_PaletteHash(0,0,16,read_palette);
    /* Every displayed colour must invalidate the cached image. In particular,
       blue/green life-bar palettes differ in the second row. */
    for(i=0;i<16;++i){x=i&7;y=i>>3;palette[y][x]^=0x00FF0000;
        assert(Gs_PaletteHash(0,0,16,read_palette)!=base);palette[y][x]^=0x00FF0000;}
    /* A neighbour's palette cannot change this image's identity. */
    for(x=8;x<16;++x){palette[0][x]^=0x00FF0000;assert(Gs_PaletteHash(0,0,16,read_palette)==base);}
    base=Gs_PaletteHash(0,0,256,read_palette);
    for(i=0;i<256;++i){x=i&15;y=i>>4;palette[y][x]^=0x0000FF00;
        assert(Gs_PaletteHash(0,0,256,read_palette)!=base);palette[y][x]^=0x0000FF00;}
    puts("Palette regression: 16 displayed colours, unused neighbours, 256-colour palettes PASS");return 0;
}
