#include "common.h"
#include "menu/menu_b.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

ModeBg *gModeBg = NULL; /* 0x3B12F4 */

/*
 * ModeBg, 0x33A360..0x33ABA0: the background of a game mode's screens (a still picture and, except for sub menu
 * 3, a movie of scrolling clouds / smoke). Used by ModeMenu and by ModeInfo, taken from the sub menu's own pack.
 */

#define BG_TEX(n, k) gModeBg->tex[n] = MTEX(res, k)

/* Builds the background from the sub menu's pack: section 1 picture, 10 textures, 9 movie. */
void ModeBg_Init(u32 *pack) {
    MTexRes *res = NULL;

    gModeBg = Heap_Alloc(0x78, 0x20, 0, 2);
    memset(gModeBg, 0, 0x78);
    res = (MTexRes *)MPACK_AT(pack, 1);
    Res_RelocateOffsets(&res, res, res);
    gModeBg->bg = res;
    if (gProgress->subMenu == 3) {
        return;
    }
    res = (MTexRes *)MPACK_AT(pack, 10);
    Res_RelocateOffsets(&res, res, res);
    switch (gProgress->subMenu) {
    case 0:
        BG_TEX(0, 3);
        BG_TEX(1, 2);
        BG_TEX(2, 1);
        BG_TEX(3, 0);
        BG_TEX(4, 4);
        BG_TEX(5, 5);
        break;
    case 1:
        BG_TEX(0, 2);
        BG_TEX(1, 1);
        BG_TEX(2, 0);
        BG_TEX(3, 3);
        BG_TEX(4, 5);
        BG_TEX(5, 4);
        break;
    case 4:
        BG_TEX(0, 0);
        BG_TEX(1, 1);
        BG_TEX(2, 2);
        BG_TEX(3, 3);
        BG_TEX(4, 4);
        BG_TEX(5, 5);
        break;
    case 2:
    case 5:
        BG_TEX(0, 2);
        BG_TEX(1, 1);
        BG_TEX(2, 0);
        break;
    case 6:
        BG_TEX(0, 12);
        BG_TEX(1, 11);
        BG_TEX(2, 10);
        BG_TEX(10, 0);
        BG_TEX(11, 1);
        BG_TEX(12, 2);
        BG_TEX(6, 3);
        BG_TEX(7, 4);
        BG_TEX(8, 5);
        BG_TEX(9, 6);
        BG_TEX(3, 7);
        BG_TEX(4, 8);
        BG_TEX(5, 9);
        break;
    case 7:
        BG_TEX(0, 3);
        BG_TEX(1, 0);
        BG_TEX(2, 1);
        BG_TEX(3, 2);
        break;
    }
    Flash_Create(&gModeBg->flash[0], MPACK_AT(pack, 9), gModeBg->tex);
    Flash_Play(&gModeBg->flash[0], 1);
}

/* Frees the background. */
void ModeBg_Term(void) {
    if (gProgress->subMenu != 3) {
        Flash_Destroy(&gModeBg->flash[0]);
    }
    if (gModeBg != NULL) {
        Heap_Free(gModeBg);
        gModeBg = NULL;
    }
}

/* Advances and draws the background: the picture, then the movie with its layers scrolled. */
void ModeBg_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

    if (gProgress->subMenu != 3) {
        for (i = 0; i < 1; i++) {
            Flash_Advance(&gModeBg->flash[i]);
        }
    }
#ifdef PORT
    Port_MenuPage(11); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gModeBg->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gModeBg->bg, 0, 0, 0x80);
#endif
    if (gProgress->subMenu != 3) {
        flash = &gModeBg->flash[0];
        switch (gProgress->subMenu) {
        case 0: {
            f32 speed[4] = { 0.60952381f, 0.47407408f, 1.0666667f, 0.85333334f };

            for (i = 0; i < 2; i++) {
                uv.x0 = 0;
                uv.y0 = 0;
                uv.x1 = 0x200;
                uv.y1 = 0x80;
                sprintf(name, "mc_kumo_%d", i + 1);
                Flash_FindLabel(flash, NULL, name, &ref);
                FlashAnim_Scroll(flash, &ref, &uv, &gModeBg->scrollX[i], NULL, speed[i], 0.0f);
                uv.x0 = 0;
                uv.y0 = 0;
                uv.x1 = 0x80;
                uv.y1 = 0x200;
                sprintf(name, "mc_kemuri_%d", i + 1);
                Flash_FindLabel(flash, NULL, name, &ref);
                FlashAnim_Scroll(flash, &ref, &uv, NULL, &gModeBg->scrollY[i], 0.0f, speed[i]);
            }
            break;
        }
        case 1:
            uv.x0 = 0;
            uv.y0 = 0;
            uv.x1 = 0x200;
            uv.y1 = 0x100;
            Flash_FindLabel(flash, NULL, "mc_kumo", &ref);
            FlashAnim_Scroll(flash, &ref, &uv, &gModeBg->scrollX[0], NULL, -0.56888889f, 0.0f);
            break;
        case 2:
            uv.x0 = 0;
            uv.y0 = 0;
            uv.x1 = 0x200;
            uv.y1 = 0x100;
            Flash_FindLabel(flash, NULL, "mc_kumo", &ref);
            FlashAnim_Scroll(flash, &ref, &uv, &gModeBg->scrollX[0], NULL, -0.42666667f, 0.0f);
            break;
        case 5:
            uv.x0 = 0;
            uv.y0 = 0;
            uv.x1 = 0x200;
            uv.y1 = 0x80;
            Flash_FindLabel(flash, NULL, "mc_kumo", &ref);
            FlashAnim_Scroll(flash, &ref, &uv, &gModeBg->scrollX[0], NULL, -0.34133333f, 0.0f);
            break;
        case 6:
            uv.x0 = 0;
            uv.y0 = 0;
            uv.x1 = 0x200;
            uv.y1 = 0x100;
            Flash_FindLabel(flash, NULL, "mc_kumo", &ref);
            FlashAnim_Scroll(flash, &ref, &uv, &gModeBg->scrollX[0], NULL, -0.42666667f, 0.0f);
            break;
        case 7:
            break;
        }
        Flash_Draw(&gModeBg->flash[0]);
    }
#ifdef PORT
    Port_MenuPage(0);
#endif
}
