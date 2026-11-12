#include "common.h"
#include "menu/menu_v.h"

/* The details page's work pointer (0x3BB144): this object owns it. */
ItemHelp *gItemHelp = NULL;

/*
 * Menu overlay DBZP.BIN, 0x399240..0x399790: ItemHelp, the item details page (a picture, the name and the
 * description of one item). A module of its own: every character select, the tournament entrant select, the
 * customise screen, the shop and the wish screen of the main executable call it with a pack of their own.
 */

/* Allocates the page and builds its movie from the caller's pack (sections 1 / 3 textures, 2 movie, 5 / 6 text). */
void ItemHelp_Init(u32 *pack) {
    MTexRes *res = NULL;
    MTextBox *box;

    gItemHelp = Heap_Alloc(sizeof(ItemHelp), 0x20, 0, 2);
    memset(gItemHelp, 0, sizeof(ItemHelp));

    res = (MTexRes *)MPACK_AT(pack, 1);
    Res_RelocateOffsets(&res, res, res);
    gItemHelp->tex[1] = MTEX(res, 0);
    gItemHelp->tex[2] = MTEX(res, 4);
    gItemHelp->tex[3] = MTEX(res, 8);
    gItemHelp->tex[0] = MTEX(res, 9);

    res = (MTexRes *)MPACK_AT(pack, 3);
    Res_RelocateOffsets(&res, res, res);
    gItemHelp->tex[6] = MTEX(res, 0);
    gItemHelp->tex[5] = MTEX(res, 1);

    Flash_Create(&gItemHelp->flash[0], MPACK_AT(pack, 2), gItemHelp->tex);
    Flash_Play(&gItemHelp->flash[0], 1);

    gItemHelp->items = (VItemEntry *)MPACK_AT(gCommonRes->data[2], 2);
    gItemHelp->nameText = MPACK_AT(pack, 6);
    gItemHelp->descText = MPACK_AT(pack, 5);

    box = &gItemHelp->name;
    TextBox_Init(box, gItemHelp->nameText, 6);
    TextBox_SetUnk80(box, 1);

    box = &gItemHelp->desc;
    TextBox_Init(box, gItemHelp->descText, 0);
    TextBox_SetUnk50(box, 0);
    TextBox_SetUnk80(box, 1);
}

/* Frees the page. */
void ItemHelp_Term(void) {
    s32 i;

    for (i = 0; i < ITEMHELP_FLASH_NUM; i++) {
        Flash_Destroy(&gItemHelp->flash[i]);
    }
    if (gItemHelp != NULL) {
        Heap_Free(gItemHelp);
        gItemHelp = NULL;
    }
}

/* Advances and draws the page for one item (0-based index into the item table). */
void ItemHelp_Draw(s32 item) {
#ifdef PORT
    extern int Port_MenuPart(int part);
    int menuPart = Port_MenuPart(4); /* frame and all glyphs share one transform */
#endif
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash;
    s32 i;

    for (i = 0; i < ITEMHELP_FLASH_NUM; i++) {
        Flash_Advance(&gItemHelp->flash[i]);
    }
    flash = &gItemHelp->flash[0];

    Flash_FindLabel(flash, NULL, "mc_dammy_text_1", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, item, &gItemHelp->name);
    Flash_FindLabel(flash, NULL, "mc_dammy_text_2", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, item, &gItemHelp->desc);

    uv.x0 = gItemHelp->items[item].type * 0x40;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_icon_item_potara", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    Flash_FindLabel(flash, NULL, "mc_icon_item_cost", &ref);
    if (gItemHelp->items[item].slots != 0) {
        uv.x0 = gItemHelp->items[item].slots * 0x20 - 0x20;
        uv.y0 = ItemTbl_GetClass(item, gItemHelp->items) * 0x2A;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = uv.y0 + 0x2A;
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    uv.x0 = (gItemHelp->items[item].picture & 1) * 0x40;
    uv.y0 = (gItemHelp->items[item].picture >> 1) * 0x40;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = uv.y0 + 0x40;
    Flash_FindLabel(flash, NULL, "mc_item_details_icon", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    Flash_FindLabel(flash, NULL, "mc_item_details", &ref);
    Flash_ClipSetTex(flash, &ref, gItemHelp->items[item].picture);

    Flash_Draw(&gItemHelp->flash[0]);
    Font_FlushAll();
#ifdef PORT
    Port_MenuPart(menuPart);
#endif
}

/* Plays the page's opening animation. */
void ItemHelp_Open(void) {
    Flash_GotoLabel(&gItemHelp->flash[0], "fl_details_in", 1);
}

/* Plays the page's closing animation. */
void ItemHelp_Close(void) {
    Flash_GotoLabel(&gItemHelp->flash[0], "fl_details_out", 1);
}
