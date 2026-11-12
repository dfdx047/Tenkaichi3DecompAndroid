#include "common.h"
#include "menu/menu_k.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * TourBg, 0x366F58..0x3673F8: the backdrop of the tournament screens (a picture and a movie of drifting clouds),
 * used by the entrant select and by the bracket. Work area gTourBg, 0x68 bytes.
 */

TourBg *gTourBg = NULL; /* 0x3B591C */

#define TB_RES(n) \
    res = (MTexRes *)MPACK_AT(gTourBg->res, n); \
    Res_RelocateOffsets(&res, res, res)

/*
 * Unpacks the backdrop file of tournament `kind` and creates its movie. `tex` (optional) gives three more
 * textures of the caller for the movie's last image records.
 */
void TourBg_Init(void *file, u32 kind, u8 **tex) {
    MTexRes *res = NULL;

    gTourBg = Heap_Alloc(sizeof(TourBg), 0x20, 0, 2);
    memset(gTourBg, 0, sizeof(TourBg));
    gTourBg->res = Sprite_Unpack(file, NULL, NULL);
    TB_RES(1);
    gTourBg->bg = res;
    TB_RES(2);
    switch (kind) {
    case TOUR_WORLD:
    case TOUR_CELL:
    case TOUR_YAMCHA:
        gTourBg->tex[0] = MTEX(res, 0);
        gTourBg->tex[2] = MTEX(res, 1);
        gTourBg->tex[1] = MTEX(res, 2);
        break;
    case TOUR_BIG:
        gTourBg->tex[0] = MTEX(res, 0);
        gTourBg->tex[4] = MTEX(res, 1);
        gTourBg->tex[3] = MTEX(res, 2);
        gTourBg->tex[2] = MTEX(res, 3);
        gTourBg->tex[1] = MTEX(res, 4);
        gTourBg->tex[6] = MTEX(res, 5);
        gTourBg->tex[5] = MTEX(res, 6);
        break;
    case TOUR_OTHERWORLD:
        gTourBg->tex[0] = MTEX(res, 0);
        gTourBg->tex[1] = MTEX(res, 1);
        gTourBg->tex[2] = MTEX(res, 2);
        break;
    }
    if (tex != NULL) {
        switch (kind) {
        case TOUR_BIG:
            gTourBg->tex[7] = tex[0];
            gTourBg->tex[8] = tex[1];
            gTourBg->tex[9] = tex[2];
            break;
        case TOUR_WORLD:
            gTourBg->tex[3] = tex[0];
            gTourBg->tex[4] = tex[1];
            gTourBg->tex[5] = tex[2];
            break;
        case TOUR_CELL:
            gTourBg->tex[3] = tex[0];
            gTourBg->tex[4] = tex[1];
            gTourBg->tex[5] = tex[2];
            break;
        case TOUR_OTHERWORLD:
            gTourBg->tex[3] = tex[0];
            gTourBg->tex[4] = tex[1];
            gTourBg->tex[5] = tex[2];
            break;
        case TOUR_YAMCHA:
            gTourBg->tex[3] = tex[0];
            gTourBg->tex[4] = tex[1];
            gTourBg->tex[5] = tex[2];
            break;
        }
    }
    Flash_Create(&gTourBg->flash[0], MPACK_AT(gTourBg->res, 3), gTourBg->tex);
    Flash_Play(&gTourBg->flash[0], 1);
    gTourBg->kind = kind;
}

/* Frees the backdrop. */
void TourBg_Term(void) {
    s32 i;

    for (i = 0; i < 1; i++) {
        Flash_Destroy(&gTourBg->flash[i]);
    }
    if (gTourBg->res != NULL) {
        Heap_Free(gTourBg->res);
        gTourBg->res = NULL;
    }
    if (gTourBg != NULL) {
        Heap_Free(gTourBg);
        gTourBg = NULL;
    }
}

/* Advances and draws the backdrop: the picture, then the movie with its cloud layer scrolled sideways. */
void TourBg_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash;
    s32 i;

    for (i = 0; i < 1; i++) {
        Flash_Advance(&gTourBg->flash[i]);
    }
#ifdef PORT
    Port_MenuPage(22); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gTourBg->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gTourBg->bg, 0, 0, 0x80);
#endif
    flash = &gTourBg->flash[0];
    uv.y1 = 0x100;
    uv.x1 = 0x200;
    uv.y0 = 0;
    uv.x0 = 0;
    Flash_FindLabel(flash, NULL, "mc_bg_move_obj_kumo", &ref);
    switch (gTourBg->kind) {
    case TOUR_WORLD:
        FlashAnim_Scroll(flash, &ref, &uv, &gTourBg->scroll, NULL, -0.14222222f, 0.0f);
        break;
    case TOUR_CELL:
        FlashAnim_Scroll(flash, &ref, &uv, &gTourBg->scroll, NULL, -0.17066667f, 0.0f);
        break;
    case TOUR_YAMCHA:
        FlashAnim_Scroll(flash, &ref, &uv, &gTourBg->scroll, NULL, 0.15515151f, 0.0f);
        break;
    }
    Flash_Draw(&gTourBg->flash[0]);
#ifdef PORT
    Port_MenuPage(0);
#endif
}
