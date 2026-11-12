#ifndef GS_PALETTE_H
#define GS_PALETTE_H
#include <stdint.h>

typedef uint32_t (*GsPaletteRead)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);

/* PSMT4 palettes occupy 8 columns by 2 rows; PSMT8 uses 16 by 16.
   Hash the same entries the decoder reads, including the upper eight colours. */
static inline uint32_t Gs_PaletteHash(uint32_t cbp,uint32_t cpsm,uint32_t count,GsPaletteRead read) {
    uint32_t h=2166136261u,i;
    for(i=0;i<count;++i) {
        uint32_t x=count==16?i&7:i&15;
        uint32_t y=count==16?i>>3:i>>4;
        h=(h^read(cbp,1,cpsm,x,y))*16777619u;
    }
    return h;
}
#endif
