#include "common.h"
#include "menu/menu_g.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/common.h"

ItemPanel *gItemPanel[2] = {NULL, NULL}; /* 0x3B38E0, one per side */

/*
 * ItemPanel, 0x351C38..0x352CB8: the panel that lists the items equipped by the character under the cursor
 * of a character select, one instance per side (movie labels "fl_evo_in" / "fl_evo_cansel"). The callers are
 * the character-select screens in the neighbouring chunks; nothing here is called from this chunk.
 */

/* Sends the set plate (kind 0) or the list plate under the cursor (kind 1) to a label. */
void ItemPanel_ClipGoto(s32 side, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    ItemPanel *p = gItemPanel[side];
    MFlash *flash = &p->flash;

    switch (kind) {
    case 0:
        strcpy(name, "mc_select_plate");
        break;
    case 1:
        sprintf(name, "mc_list_plate_%d", p->cursor + 1);
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

#define IP_RES(n) \
    res = (MTexRes *)MPACK_AT(pack, n); \
    Res_RelocateOffsets(&res, res, res)

/* Allocates the panel of one side and builds its movie from the caller's unpacked pack. */
void ItemPanel_Init(u32 *pack, s32 side) {
    MTexRes *res = NULL;
    ItemPanel *p;
    s32 i;

    gItemPanel[side] = Heap_Alloc(0x6C8, 0x20, 0, 2);
    memset(gItemPanel[side], 0, 0x6C8);
    p = gItemPanel[side];
    IP_RES(16);
    p->tex[12] = MTEX(res, 0);
    IP_RES(1);
    p->tex[19] = MTEX(res, side ? 1 : 0);
    p->tex[20] = MTEX(res, side ? 3 : 2);
    IP_RES(2);
    p->tex[8] = MTEX(res, 1);
    IP_RES(3);
    p->tex[7] = MTEX(res, 0);
    p->tex[11] = MTEX(res, 1);
    IP_RES(6);
    p->tex[13] = MTEX(res, 1);
    p->tex[14] = side ? MTEX(res, 3) : MTEX(res, 0);
    IP_RES(7);
    p->tex[15] = MTEX(res, 0);
    IP_RES(8);
    p->tex[16] = MTEX(res, 1);
    p->tex[17] = MTEX(res, 2);
    p->tex[18] = MTEX(res, 3);
    IP_RES(11);
    p->tex[0] = MTEX(res, 0);
    p->tex[1] = MTEX(res, 0);
    IP_RES(9);
    p->tex[3] = MTEX(res, 0);
    IP_RES(12);
    p->tex[5] = MTEX(res, 0);
    p->tex[4] = MTEX(res, 1);
    IP_RES(10);
    p->tex[6] = MTEX(res, 3);
    p->tex[10] = MTEX(res, 4);
    Flash_Create(&p->flash, MPACK_AT(pack, 4 + side), p->tex);
    Flash_Play(&p->flash, 1);
    p->text[0] = MPACK_AT(pack, 13);
    p->text[1] = MPACK_AT(pack, 14);
    p->text[2] = MPACK_AT(pack, 15);
    p->items = (MItemEntry *)MPACK_AT(gCommonRes->data[2], 2);
    TextBox_Init(&p->name, p->text[0], side ? 2 : 1);
    TextBox_SetUnk80(&p->name, 1);
    TextBox_Init(&p->form, p->text[1], side ? 4 : 3);
    TextBox_SetUnk80(&p->form, 1);
    for (i = 0; i < ITEMPANEL_ROWS; i++) {
        TextBox_Init(&p->line[i], p->text[2], 6);
        TextBox_SetUnk80(&p->line[i], 1);
    }
}

/* Frees the panel of one side. */
void ItemPanel_Term(s32 side) {
    Flash_Destroy(&gItemPanel[side]->flash);
    if (gItemPanel[side] != NULL) {
        Heap_Free(gItemPanel[side]);
        gItemPanel[side] = NULL;
    }
}

/* Draws the panel: set name, character and form names, slots used / available, and the eight item rows. */
void ItemPanel_Draw(s32 side) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    ItemPanel *p = gItemPanel[side];
    MFlash *flash;
    s32 i;

    if (!(p->flags & ITEMPANEL_SHOWN)) {
        return;
    }
    flash = &p->flash;
    uv.x0 = side * 0x20;
    uv.y0 = 0;
    uv.x1 = uv.x0 + 0x20;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, "mc_select_plate", side ? "mc_yajirusi_right" : "mc_yajirusi_left", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = 0;
    uv.y0 = p->set * 0x20;
    uv.x1 = 0x100;
    uv.y1 = uv.y0 + 0x20;
    Flash_FindLabel(flash, "mc_select_plate", "mc_select_text_off", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = 0;
    uv.y0 = p->set * 0x20;
    uv.x1 = 0x100;
    uv.y1 = uv.y0 + 0x20;
    Flash_FindLabel(flash, "mc_select_plate", "mc_select_text_on", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, NULL, "mc_name_text", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, p->chara, &p->name);
    Flash_FindLabel(flash, NULL, "mc_form_text", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, p->chara, &p->form);
    uv.x0 = (p->used % 4) * 0x40;
    uv.y0 = (p->used / 4) * 0x40;
    uv.x1 = uv.x0 + 0x40;
    uv.y1 = uv.y0 + 0x40;
    Flash_FindLabel(flash, NULL, "mc_ability_limit_big_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = (p->limit % 4) * 0x20;
    uv.y0 = (p->limit / 4) * 0x20;
    uv.x1 = uv.x0 + 0x20;
    uv.y1 = uv.y0 + 0x20;
    Flash_FindLabel(flash, NULL, "mc_ability_limit_s_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < ITEMPANEL_ROWS; i++) {
        sprintf(name, "mc_list_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
        if (i < p->count || i == ITEMPANEL_BACK) {
            if (p->item.id[i] != 0) {
                Flash_ClipSetTex(flash, &ref, 4);
            } else {
                Flash_ClipSetTex(flash, &ref, 5);
            }
        } else {
            Flash_ClipSetTex(flash, &ref, 7);
        }
        Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
        if (p->item.id[i] != 0) {
            Flash_ClipSetTex(flash, &ref, 0);
        } else {
            Flash_ClipSetTex(flash, &ref, 1);
        }
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        if (p->cursor == i) {
            uv.x1 = 0x80;
            uv.x0 = 0x40;
            uv.y0 = 0;
            uv.y1 = 0x40;
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
        if (p->item.id[i] != 0) {
            if (p->items[p->item.id[i] - 1].slots != 0) {
                uv.x0 = p->items[p->item.id[i] - 1].slots * 0x20 - 0x20;
                uv.y0 = ItemTbl_GetClass(p->item.id[i] - 1, p->items) * 42;
                uv.x1 = uv.x0 + 0x20;
                uv.y1 = uv.y0 + 42;
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
        if (p->item.id[i] != 0) {
            uv.x0 = p->items[p->item.id[i] - 1].type * 0x40;
            uv.y0 = 0;
            uv.y1 = 0x40;
            uv.x1 = uv.x0 + 0x40;
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        if (p->item.id[i] != 0) {
            Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, p->item.id[i] - 1, &p->line[i]);
        }
    }
    Flash_Draw(flash);
    Font_FlushAll();
}

/* Steps the movie; lights the cursor plates on the first frame; ends the closing animation. */
void ItemPanel_Update(s32 side) {
    ItemPanel *p = gItemPanel[side];

    if (p->flags & ITEMPANEL_SHOWN) {
        Flash_Advance(&p->flash);
        if (!(p->flags & ITEMPANEL_STARTED)) {
            ItemPanel_ClipGoto(side, 0, "fl_on_start");
            ItemPanel_ClipGoto(side, 1, "fl_on_start");
            p->flags |= ITEMPANEL_STARTED;
        }
        if (p->flags & ITEMPANEL_CLOSING) {
            if (p->flash.trig & 2) {
                p->flags &= ~ITEMPANEL_SHOWN;
                p->flags &= ~ITEMPANEL_CLOSING;
            }
        }
    }
}

/* One pad's input: up / down move the cursor, confirm returns the item id under it (0 on an empty row),
   cancel (or left on side 0, right on side 1) returns -1. */
s32 ItemPanel_Input(s32 side, s32 pad) {
    ItemPanel *p = gItemPanel[side];
    s32 result = 0;

    if (gPad[pad].gameRepeat & PADG_UP) {
        ItemPanel_ClipGoto(side, 1, "fl_off_start");
        if (p->cursor == ITEMPANEL_BACK) {
            p->cursor = p->count - 1;
        } else {
            p->cursor--;
            if (p->cursor < 0) {
                p->cursor = ITEMPANEL_BACK;
            }
        }
        ItemPanel_ClipGoto(side, 1, "fl_on_start");
        Snd_PlaySe(1, 0);
    } else if (gPad[pad].gameRepeat & PADG_DOWN) {
        ItemPanel_ClipGoto(side, 1, "fl_off_start");
        if (p->cursor == ITEMPANEL_BACK) {
            p->cursor = 0;
        } else {
            p->cursor++;
            if (p->cursor >= p->count) {
                p->cursor = ITEMPANEL_BACK;
            }
        }
        ItemPanel_ClipGoto(side, 1, "fl_on_start");
        Snd_PlaySe(1, 0);
    } else if (gPad[pad].gamePressed & PADG_CROSS) {
        ItemPanel_ClipGoto(side, 1, "fl_ok");
        if (p->item.id[p->cursor] != 0) {
            result = p->item.id[p->cursor];
            Snd_PlaySe(1, 1);
        } else {
            Snd_PlaySe(1, 7);
        }
    } else if ((side == 0 && (gPad[pad].gamePressed & (PADG_TRIANGLE | PADG_LEFT))) ||
               (side != 0 && (gPad[pad].gamePressed & (PADG_TRIANGLE | PADG_RIGHT)))) {
        ItemPanel_ClipGoto(side, 1, "fl_off_start");
        ItemPanel_ClipGoto(side, 0, "fl_off_start");
        result = -1;
    }
    return result;
}

/* The save block's two kinds of item sets, as structures (sys/save.h has them as arrays). */
typedef struct ItemPanelSave {
    /* 0x0000 */ u8 unk0[0x1808];
    /* 0x1808 */ struct {
        MItemSet set[3];
        s32 unk30;
        u16 level;
        u16 unk36;
    } custom[97];
    /* 0x2D40 */ struct {
        MItemSet set;
        u8 unk10[0xC];
    } rec[14];
} ItemPanelSave;

#define IP_SAVE ((ItemPanelSave *)gSaveData)

/* Loads the set to show: item set `set` of custom slot `slot`, or the set of saved record `slot`. */
void ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec) {
    ItemPanel *p = gItemPanel[side];

    if (fromRec) {
        p->item = IP_SAVE->rec[slot].set;
    } else {
        p->item = IP_SAVE->custom[slot].set[set];
    }
    p->chara = chara;
    p->set = set + 1;
    p->limit = ChrTbl_GetLevel(chara, slot, fromRec);
    p->used = ItemSet_Fit(p->item.id, p->items, p->limit, &p->count);
    if (fromRec) {
        p->used = 10;
        p->limit = 10;
    }
    p->cursor = 0;
}

/* Opens the panel. */
void ItemPanel_Show(s32 side) {
    ItemPanel *p = gItemPanel[side];

    Flash_GotoLabel(&p->flash, "fl_evo_in", 1);
    p->flags |= ITEMPANEL_SHOWN;
    p->flags &= ~ITEMPANEL_CLOSING;
    if (p->flags & ITEMPANEL_STARTED) {
        ItemPanel_ClipGoto(side, 0, "fl_on_start");
        ItemPanel_ClipGoto(side, 1, "fl_on_start");
    }
    Snd_PlaySe(2, 4);
}

/* Starts the closing animation; ItemPanel_Update hides the panel when it ends. */
void ItemPanel_Hide(s32 side) {
    ItemPanel *p = gItemPanel[side];

    Flash_GotoLabel(&p->flash, "fl_evo_cansel", 1);
    p->flags |= ITEMPANEL_CLOSING;
    Snd_PlaySe(2, 4);
}
