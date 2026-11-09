/* Texture packs (gs_texpack.c): replacement textures found by PCSX2's file names. */
#ifndef PORT_GS_TEXPACK_H
#define PORT_GS_TEXPACK_H
#include <stdint.h>

#define TEXPACK_MAX_LEVELS 14

typedef struct TexPackImage {
    uint8_t *file;                            /* the whole file (TexPack_Free) */
    uint32_t w, h;
    int format;                               /* 0: 8-bit R, G, B, A; 1, 2, 3: DXT1, DXT3, DXT5 */
    int levels;
    const uint8_t *data[TEXPACK_MAX_LEVELS];  /* each level's data, inside `file` */
    uint32_t bytes[TEXPACK_MAX_LEVELS];
} TexPackImage;

void TexPack_Init(void);  /* looks through the folder once */
int TexPack_Count(void);
/* The file for the texture the GS registers describe (as it lies in GS memory now), or NULL. *maxAlpha: the largest
   alpha byte the original texture has. The game uses bit 7 of the alpha it writes into the picture as a mask for
   later draws (a texture whose alpha stays at 0x7F never sets it); a replacement's alpha is redrawn and compressed
   and strays over 0x80 in places, so the renderer keeps it at or below the original's largest value. */
const char *TexPack_Lookup(uint32_t tbp, uint32_t tbw, uint32_t psm, uint32_t tw, uint32_t th, uint32_t tcc, uint32_t cbp, uint32_t cpsm,
                           uint32_t *maxAlpha);
int TexPack_Load(const char *path, TexPackImage *img);
void TexPack_Free(TexPackImage *img);
/* A DXT (BC1-BC3) image made 8-bit RGBA in place, levels above maxSide left out (for GPUs without DXT); 0 = failed. */
int TexPack_Decompress(TexPackImage *img, uint32_t maxSide);

#endif
