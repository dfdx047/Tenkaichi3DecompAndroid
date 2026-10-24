#include "common.h"
#include "menu/menu_u.h"

/*
 * EvoZ, 0x393C58..0x395E30: the screen's per-frame set-up of its three movies. A second source file (it has
 * its own copy of "mc_ability_limit_up"); its last function, EvoZ_Load, is in the next chunk (menu_v.c).
 * Read-only data 0x3BB180..0x3BB6AC.
 */

/* Clip callback: limits drawing to the list window. */
void EvoZ_SetListScissor(void) {
    Sprite_SetScissor(0, 0x200, 0xC6, 0x19C);
}

/* Clip callback: back to the whole screen. */
void EvoZ_ResetScissor(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Sets up everything the three movies show this frame: plates, numbers, icons, item names, the reel. */
void EvoZ_Refresh(UEvoZ *ez) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char sub[64];
    MFlash *flash;
    s32 i;
    s32 j;

    flash = &ez->flash[0];
    if (ez->side->flags & 2) {
        ez->faceAlpha += 0.075f;
        if (ez->faceAlpha >= 1.0f) {
            ez->faceAlpha = 1.0f;
        }
    } else {
        ez->faceAlpha = 0.0f;
    }
    Flash_FindLabel(flash, 0, "mc_single_chara_l", &ref);
    Flash_ClipSetAlpha(flash, &ref, ez->faceAlpha);
    Flash_FindLabel(flash, 0, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, ez->side->chara, &ez->box[0]);
    Flash_FindLabel(flash, 0, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, ez->side->chara, &ez->box[1]);

    /* the three-item menu; the third item is dim at the last level */
    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 32;
        uv.x1 = 0x100;
        uv.y1 = i * 32 + 32;
        sprintf(name, "menu_plate_%d", i + 1);
        if (i == 2) {
            Flash_FindLabel(flash, 0, name, &ref);
            Flash_ClipSetColor(flash, &ref, ez->nextExp == 0 ? 0.4f : 1.0f);
        }
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    /* the item-set plate */
    for (i = 0; i < 2; i++) {
        uv.x0 = i * 32;
        uv.y0 = 0;
        uv.x1 = i * 32 + 32;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, "mc_select_plate", i != 0 ? "mc_yajirusi_right" : "mc_yajirusi_left", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        uv.x0 = 0;
        uv.y0 = ez->setCur * 32;
        uv.x1 = 0x200;
        uv.y1 = ez->setCur * 32 + 32;
        Flash_FindLabel(flash, "mc_select_plate", i != 0 ? "mc_select_text_off" : "mc_select_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    /* the points page */
    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x100;
    uv.y1 = 0x20;
    Flash_FindLabel(flash, 0, "mc_text_power_2", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    sprintf(name, "mc_num_zp_%d", ez->zpDigit);
    for (i = 0; i < 2; i++) {
        uv.x0 = i * 32;
        uv.y0 = 0x20;
        uv.x1 = i * 32 + 32;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, name, i != 0 ? "mc_yajirusi_num_dwn" : "mc_yajirusi_num_up", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    Num_DrawChild(flash, "mc_num_zp_%d", "mc_num_use_zp", 0, 5, ez->pay, 0x20, 0x20, 1, 1);
    Num_DrawChild(flash, "mc_status_base_1", "mc_pay_num_%d", 0, 7, gSaveData->money, 0x20, 0x20, 0, 0);

    /* the status page: what the item set adds or takes */
    Flash_FindLabel(flash, 0, "mc_status_attribute", &ref);
    Flash_ClipSetTex(flash, &ref, ez->kind);
    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_status_ability_base1_%d", i + 1);
        uv.x0 = 0;
        uv.y0 = i * 20 + 20;
        uv.x1 = 0x80;
        uv.y1 = i * 20 + 40;
        Flash_FindLabel(flash, name, "mc_text_ability_1", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        for (j = 0; j < 3; j++) {
            sprintf(sub, "mc_status_ability_plus_%d", j + 1);
            Flash_FindLabel(flash, name, sub, &ref);
            if (j < ez->bonus[i + 1]) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            sprintf(sub, "mc_status_ability_minus_%d", j + 1);
            Flash_FindLabel(flash, name, sub, &ref);
            if (ez->bonus[i + 1] < -j) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x80;
    uv.y1 = 0x14;
    Flash_FindLabel(flash, "mc_status_ability_base2_1", "mc_text_ability_2", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 7; i++) {
        sprintf(sub, "mc_status_ability_plus_%d", i + 4);
        Flash_FindLabel(flash, "mc_status_ability_base2_1", sub, &ref);
        if (i < ez->bonus[0]) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }

    /* item slots: the character's and the free ones (digits on a sheet four wide) */
    uv.x0 = ez->capacity % 4 * 64;
    uv.y0 = ez->capacity / 4 * 64;
    uv.x1 = ez->capacity % 4 * 64 + 64;
    uv.y1 = ez->capacity / 4 * 64 + 64;
    Flash_FindLabel(flash, 0, "mc_ability_limit_big_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, "mc_ability_limit_up", "mc_ability_limit_big_num_1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = ez->fits % 4 * 32;
    uv.y0 = ez->fits / 4 * 32;
    uv.x1 = ez->fits % 4 * 32 + 32;
    uv.y1 = ez->fits / 4 * 32 + 32;
    Flash_FindLabel(flash, 0, "mc_ability_limit_s_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, "mc_ability_limit_up", "mc_ability_limit_s_num_1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    /* experience */
    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x100;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, 0, "mc_text_power_1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Num_DrawChild(flash, "mc_status_power_base", "mc_exp_num_%d", 0, 7, ez->exp, 0x20, 0x20, ez->nextExp != 0 ? 0 : 2,
                  0);
    Num_DrawChild(flash, "mc_status_power_base", "mc_next_num_%d", 0, 7, ez->nextExp, 0x20, 0x20,
                  ez->nextExp != 0 ? 0 : 2, 0);
    Flash_FindLabel(flash, 0, "mc_exp_bar_point", &ref);
    Flash_ClipSetScale(flash, &ref, ez->nextExp != 0 ? ez->expBar : 6.66f, 1.0f);

    /* the third movie: the slot list or the item list */
    flash = &ez->flash[2];
    for (i = 0; i < 2; i++) {
        uv.x0 = i * 32;
        uv.y0 = 0;
        uv.x1 = i * 32 + 32;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, "mc_custom_yajirusi", i != 0 ? "mc_yajirusi_right" : "mc_yajirusi_left", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    uv.x0 = 0;
    uv.y0 = ez->action * 64;
    uv.x1 = 0x100;
    uv.y1 = ez->action * 64 + 64;
    Flash_FindLabel(flash, "mc_custom_yajirusi", "mc_text_custm", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = ez->capacity % 4 * 32;
    uv.y0 = ez->capacity / 4 * 32;
    uv.x1 = ez->capacity % 4 * 32 + 32;
    uv.y1 = ez->capacity / 4 * 32 + 32;
    Flash_FindLabel(flash, 0, "mc_ability_limit_s_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = ez->fits % 4 * 64;
    uv.y0 = ez->fits / 4 * 64;
    uv.x1 = ez->fits % 4 * 64 + 64;
    uv.y1 = ez->fits / 4 * 64 + 64;
    Flash_FindLabel(flash, 0, "mc_ability_limit_big_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    if (ez->listOpen == 0) {
        /* the eight slots of the item set */
        for (i = 0; i < 8; i++) {
            sprintf(name, "mc_list_plate_%d", i + 1);
            Flash_FindLabel(flash, name, "mc_font_new", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            if (i < ez->used || i == 7) {
                if (SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] != 0) {
                    Flash_ClipSetTex(flash, &ref, 4);
                } else {
                    Flash_ClipSetTex(flash, &ref, 5);
                }
            } else {
                Flash_ClipSetTex(flash, &ref, 7);
            }
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            if (SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] != 0) {
                Flash_ClipSetTex(flash, &ref, 0);
            } else {
                Flash_ClipSetTex(flash, &ref, 1);
            }
            Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
            if (ez->action == 3 || ez->slot == i) {
                uv.x0 = ez->action * 64;
                uv.y0 = 0;
                uv.x1 = ez->action * 64 + 64;
                uv.y1 = 0x40;
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
            if (SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] != 0 && ez->items[SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] - 1].slots != 0) {
                uv.x0 = ez->items[SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] - 1].slots * 32 - 32;
                uv.y0 = ItemTbl_GetClass(SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] - 1, ez->items) * 42;
                uv.x1 = uv.x0 + 32;
                uv.y1 = uv.y0 + 42;
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
            if (SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] != 0) {
                uv.x0 = ez->items[SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] - 1].type * 64;
                uv.y0 = 0;
                uv.x1 = uv.x0 + 64;
                uv.y1 = 0x40;
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
            if (SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] != 0) {
                TextBox_AttachLine(flash, &ref, 0, 0, SAVE_CUSTOM_AT(gSaveData, ez->side->pick.col + ez->side->pick.row * 7).set[ez->set][i] - 1, &ez->box[10 + i]);
            } else {
                TextBox_AttachLine(flash, &ref, 0, 0, -1, &ez->box[10 + i]);
            }
        }
    } else {
        /* the slot being changed on top, then six rows of the item list and the row scrolling in */
        sprintf(name, "mc_list_plate_%d", 1);
        Flash_FindLabel(flash, name, "mc_font_new", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
        Flash_ClipSetTex(flash, &ref, 4);
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
        if (ez->curItem != 0) {
            Flash_ClipSetTex(flash, &ref, 4);
        } else {
            Flash_ClipSetTex(flash, &ref, 5);
        }
        Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
        if (ez->curItem != 0 && ez->items[ez->curItem - 1].slots != 0) {
            uv.x0 = ez->items[ez->curItem - 1].slots * 32 - 32;
            uv.y0 = ItemTbl_GetClass(ez->curItem - 1, ez->items) * 42;
            uv.x1 = uv.x0 + 32;
            uv.y1 = uv.y0 + 42;
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
        if (ez->curItem != 0) {
            uv.x0 = ez->items[ez->curItem - 1].type * 64;
            uv.y0 = 0;
            uv.x1 = uv.x0 + 64;
            uv.y1 = 0x40;
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
        if (ez->curItem != 0) {
            TextBox_AttachLine(flash, &ref, 0, 0, ez->curItem - 1, &ez->box[2]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, 0, -1, &ez->box[2]);
        }
        for (i = 0; i < 6; i++) {
            sprintf(name, "mc_list_plate_%d", i + 2);
            Flash_FindLabel(flash, 0, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 0x400, 1);
            if (i == 0 || i == ez->list.rows - 1) {
                Flash_ClipSetCallbackA(flash, &ref, EvoZ_SetListScissor, 0);
                Flash_ClipSetCallbackB(flash, &ref, EvoZ_ResetScissor, 0);
            }
            if (EvoZ_CanEquip(ez, ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]) || !(gSaveData->item[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]] & 1)) {
                Flash_ClipSetColor(flash, &ref, 1.0f);
            } else {
                Flash_ClipSetColor(flash, &ref, 0.4f);
            }
            Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            if (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]] & 1) {
                Flash_ClipSetTex(flash, &ref, 2);
            } else {
                Flash_ClipSetTex(flash, &ref, 6);
            }
            Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
            uv.x0 = ez->items[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]].type * 64;
            uv.y0 = 0;
            uv.x1 = uv.x0 + 64;
            uv.y1 = 0x40;
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
            if (ez->items[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]].slots != 0 && (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]] & 1)) {
                uv.x0 = ez->items[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]].slots * 32 - 32;
                uv.y0 = ItemTbl_GetClass(ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i], ez->items) * 42;
                uv.x1 = uv.x0 + 32;
                uv.y1 = uv.y0 + 42;
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            Flash_FindLabel(flash, name, "mc_font_new", &ref);
            if (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]] & 2) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
            if (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i]] & 1) {
                TextBox_AttachLine(flash, &ref, 0, 0, ez->list.ids[ez->list.tab][ez->list.top[ez->list.tab] + i], &ez->box[3 + i]);
            } else {
                TextBox_AttachLine(flash, &ref, 0, 0, UEVOZ_ITEM_MAX, &ez->box[3 + i]);
            }
        }
        sprintf(name, "mc_list_plate_%d", 8);
        Flash_FindLabel(flash, 0, name, &ref);
        Flash_ClipSetFlags(flash, &ref, 0x400, 1);
        Flash_ClipSetCallbackA(flash, &ref, EvoZ_SetListScissor, 0);
        Flash_ClipSetCallbackB(flash, &ref, EvoZ_ResetScissor, 0);
        if (EvoZ_CanEquip(ez, ez->list.ids[ez->list.tab][ez->list.extra]) || !(gSaveData->item[ez->list.ids[ez->list.tab][ez->list.extra]] & 1)) {
            Flash_ClipSetColor(flash, &ref, 1.0f);
        } else {
            Flash_ClipSetColor(flash, &ref, 0.4f);
        }
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
        Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
        if (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.extra]] & 1) {
            Flash_ClipSetTex(flash, &ref, 2);
        } else {
            Flash_ClipSetTex(flash, &ref, 6);
        }
        Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
        uv.x0 = ez->items[ez->list.ids[ez->list.tab][ez->list.extra]].type * 64;
        uv.y0 = 0;
        uv.x1 = uv.x0 + 64;
        uv.y1 = 0x40;
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
        if (ez->items[ez->list.ids[ez->list.tab][ez->list.extra]].slots != 0 && (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.extra]] & 1)) {
            uv.x0 = ez->items[ez->list.ids[ez->list.tab][ez->list.extra]].slots * 32 - 32;
            uv.y0 = ItemTbl_GetClass(ez->list.ids[ez->list.tab][ez->list.extra], ez->items) * 42;
            uv.x1 = uv.x0 + 32;
            uv.y1 = uv.y0 + 42;
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_font_new", &ref);
        if (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.extra]] & 2) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
        if (gSaveData->item[ez->list.ids[ez->list.tab][ez->list.extra]] & 1) {
            TextBox_AttachLine(flash, &ref, 0, 0, ez->list.ids[ez->list.tab][ez->list.extra], &ez->box[9]);
        } else {
            TextBox_AttachLine(flash, &ref, 0, 0, UEVOZ_ITEM_MAX, &ez->box[9]);
        }

        /* the four tabs; the last one is dim unless it is the open one */
        for (i = 0; i < UEVOZ_TABS; i++) {
            uv.x0 = i * 64;
            uv.y0 = 0;
            uv.x1 = i * 64 + 64;
            uv.y1 = 0x40;
            sprintf(name, "mc_tab_btn_%d", i);
            Flash_FindLabel(flash, 0, name, &ref);
            if (ez->list.tab == 3) {
                if (i == 3) {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 0.4f);
                }
            } else {
                if (i != 3) {
                    Flash_ClipSetColor(flash, &ref, 1.0f);
                } else {
                    Flash_ClipSetColor(flash, &ref, 0.4f);
                }
            }
            Flash_FindLabel(flash, name, "mc_icon_tab_off", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_icon_tab_on", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            Flash_FindLabel(flash, name, "mc_font_new_tab", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        uv.x0 = 0;
        uv.y0 = ez->list.tab * 32;
        uv.x1 = 0x100;
        uv.y1 = ez->list.tab * 32 + 32;
        Flash_FindLabel(flash, 0, "mc_tab_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        for (i = 0; i < 2; i++) {
            uv.x0 = i * 32;
            uv.y0 = 0x20;
            uv.x1 = i * 32 + 32;
            uv.y1 = 0x40;
            Flash_FindLabel(flash, "mc_menu_yajirusi", i != 0 ? "mc_menu_yajirusi_down" : "mc_menu_yajirusi_up", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (i == 0) {
                Flash_ClipSetFlags(flash, &ref, 2, ez->list.top[ez->list.tab] != 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, ez->list.bottom[ez->list.tab] != ez->list.count[ez->list.tab] - 1);
            }
        }
        {
            f32 scale = (f32)ez->list.rows / (f32)ez->list.count[ez->list.tab] * ez->list.rowsF;
            s32 y = ez->list.rowsF * 32.0f * (f32)ez->list.top[ez->list.tab] / (f32)ez->list.count[ez->list.tab];

            Flash_FindLabel(flash, 0, "mc_scroll_bar_point", &ref);
            Flash_ClipSetScale(flash, &ref, 1.0f, scale);
            Flash_ClipSetOffset(flash, &ref, 0, y);
        }
        if (ez->list.rows >= ez->list.count[ez->list.tab]) {
            Flash_ClipSetColor(flash, &ref, 0.4f);
        } else {
            Flash_ClipSetColor(flash, &ref, 1.0f);
        }
    }

    /* the character reel */
    if (!(ez->flags & UEVOZ_REEL_READY)) {
        flash = &ez->flash[1];
        uv.x0 = 0;
        uv.y0 = 0;
        uv.x1 = 0x40;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, 0, "mc_plate_text", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (ez->rows >= 2) {
            for (i = 0; i < 2; i++) {
                char *arrow;

                uv.x0 = i * 32;
                uv.y0 = 0x20;
                uv.x1 = i * 32 + 32;
                uv.y1 = 0x40;
                if (i != 0) {
                    arrow = "mc_yajirusi_down";
                } else {
                    arrow = "mc_yajirusi_up";
                }
                Flash_FindLabel(flash, arrow, i != 0 ? "mc_yajirusi_icon_down" : "mc_yajirusi_icon_up", &ref);
                Flash_ClipSetUv(flash, &ref, &uv);
                Flash_FindLabel(flash, 0, arrow, &ref);
            }
        } else {
            for (i = 0; i < 2; i++) {
                Flash_FindLabel(flash, 0, i != 0 ? "mc_yajirusi_down" : "mc_yajirusi_up", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
        Flash_FindLabel(flash, 0, "mc_chara_mask", &ref);
        if (ez->side->mask != 0) {
            Flash_ClipSetFlags(flash, &ref, 0x102, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 0x102, 0);
        }
        for (i = 0; i < 14; i++) {
            sprintf(name, "mc_chara_chip_%03d", i);
            Flash_FindLabel(flash, 0, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 0x80, (u8)ez->side->mask);
        }
    }
}

/*
 * Merged (integration step 11): EvoZ_Load (0x395E30..0x396838, formerly src/menu/menu_v.c) is appended here, so
 * this file is the whole object 0x393C58..0x396838. It uses this file's view UEvoZ of include/menu/menu_u.h
 * (the EvoZ view of include/menu/menu_v.h stays for menu_v_b.c): chipTex -> bg, VItemEntry -> UItemEntry,
 * VChrGridList -> UChrGridList. menu_u_g.c (0x392F10..0x393C58) is a separate object: both have their own copy
 * of "mc_ability_limit_up" (0x3BB150 and 0x3BB3D0).
 */

/*
 * Menu overlay DBZP.BIN, 0x395E30..0x396838: EvoZ_Load, the last function of the first source file of the
 * character customising screen (EvoZ). The rest of that file (0x392F10..0x395E30: init, term, draw, update,
 * run) is in the previous chunk; append this function to it.
 */

#define EZ_RES(n) \
    res = (MTexRes *)MPACK_AT(ez->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen's section of archive 7 and the chip file, builds the three movies, the item details page,
   the password entry and the dialog, and finds the text and the character grid. */
void EvoZ_Load(UEvoZ *ez, s32 section) {
    MTexRes *res = NULL;
    s32 i;

    ez->pack = (u32 *)MPACK_AT(gMenuArc7, section);
    ez->res = Sprite_Unpack(ez->pack, NULL, NULL);
    ez->chipFile = File_LoadSync(0x3C0, NULL, 0);
    ez->chipRes = Sprite_Unpack(ez->chipFile, NULL, NULL);
    res = ez->chipRes;
    Res_RelocateOffsets(&res, res, res);
    ez->bg = res;

    EZ_RES(3);
    ez->tex0[17] = MTEX(res, 0);
    ez->tex0[20] = MTEX(res, 1);
    ez->tex0[19] = MTEX(res, 2);
    ez->tex0[22] = MTEX(res, 3);
    ez->tex0[25] = MTEX(res, 4);
    EZ_RES(4);
    ez->tex0[18] = MTEX(res, 0);
    ez->tex0[21] = MTEX(res, 1);
    ez->tex0[23] = MTEX(res, 2);
    ez->tex0[26] = MTEX(res, 3);
    EZ_RES(5);
    ez->tex0[1] = MTEX(res, 0);
    ez->tex0[2] = MTEX(res, 1);
    ez->tex0[3] = MTEX(res, 2);
    ez->tex0[5] = MTEX(res, 3);
    EZ_RES(6);
    ez->tex0[4] = MTEX(res, 0);
    ez->tex0[6] = MTEX(res, 1);
    EZ_RES(7);
    ez->tex0[7] = MTEX(res, 0);
    ez->tex0[8] = MTEX(res, 1);
    ez->tex0[9] = MTEX(res, 2);
    ez->tex0[12] = MTEX(res, 3);
    ez->tex0[11] = MTEX(res, 4);
    ez->tex0[13] = MTEX(res, 5);
    EZ_RES(8);
    ez->tex0[27] = MTEX(res, 0);
    ez->tex0[28] = MTEX(res, 1);
    ez->tex0[29] = MTEX(res, 2);
    EZ_RES(9);
    ez->tex0[14] = MTEX(res, 0);
    ez->tex0[15] = MTEX(res, 1);
    ez->tex0[16] = MTEX(res, 2);
    EZ_RES(10);
    ez->tex0[34] = MTEX(res, 0);
    ez->tex0[36] = MTEX(res, 1);
    ez->tex0[37] = MTEX(res, 2);
    ez->tex0[38] = MTEX(res, 3);
    ez->tex0[39] = MTEX(res, 4);
    ez->tex2[3] = MTEX(res, 1);
    ez->tex2[4] = MTEX(res, 2);
    ez->tex2[5] = MTEX(res, 3);
    EZ_RES(11);
    ez->tex0[35] = MTEX(res, 0);
    ez->tex0[40] = MTEX(res, 1);
    ez->tex0[10] = MTEX(res, 2);
    ez->tex2[2] = MTEX(res, 0);
    EZ_RES(12);
    ez->tex0[0] = MTEX(res, 0);
    EZ_RES(16);
    ez->tex0[41] = MTEX(res, 0);
    EZ_RES(17);
    ez->tex0[30] = MTEX(res, 0);
    ez->tex0[31] = MTEX(res, 2);
    Flash_Create(&ez->flash[0], MPACK_AT(ez->res, 13), ez->tex0);
    Flash_Play(&ez->flash[0], 1);
    EZ_RES(18);
    ez->tex1[0] = MTEX(res, 0);
    ez->tex1[2] = MTEX(res, 1);
    EZ_RES(19);
    ez->tex1[1] = MTEX(res, 1);
    ez->tex1[4] = MTEX(res, 2);
    ez->tex1[5] = MTEX(res, 4);
    ez->tex1[6] = MTEX(res, 3);
    EZ_RES(20);
    ez->tex1[7] = MTEX(res, 0);
    EZ_RES(21);
    ez->tex1[3] = MTEX(res, 0);
    EZ_RES(22);
    ez->tex1[10] = MTEX(res, 0);
    ez->tex1[17] = MTEX(res, 1);
    ez->tex0[24] = MTEX(res, 1);
    Flash_Create(&ez->flash[1], MPACK_AT(ez->res, 23), ez->tex1);
    Flash_Play(&ez->flash[1], 1);
    EZ_RES(24);
    ez->tex2[24] = MTEX(res, 0);
    ez->tex2[23] = MTEX(res, 1);
    ez->tex2[25] = MTEX(res, 2);
    EZ_RES(25);
    ez->tex2[19] = MTEX(res, 0);
    ez->tex2[21] = MTEX(res, 1);
    ez->tex2[20] = MTEX(res, 2);
    ez->tex2[22] = MTEX(res, 3);
    ez->tex2[10] = MTEX(res, 4);
    EZ_RES(26);
    ez->tex2[11] = MTEX(res, 0);
    ez->tex2[18] = MTEX(res, 1);
    EZ_RES(27);
    ez->tex2[17] = MTEX(res, 0);
    ez->tex2[16] = MTEX(res, 1);
    EZ_RES(28);
    ez->tex2[12] = MTEX(res, 0);
    ez->tex2[13] = MTEX(res, 0);
    EZ_RES(22);
    ez->tex2[8] = MTEX(res, 1);
    EZ_RES(14);
    ez->tex2[1] = MTEX(res, 0);
    ez->tex2[0] = MTEX(res, 1);
    ez->tex2[6] = MTEX(res, 2);
    EZ_RES(15);
    ez->tex2[9] = MTEX(res, 0);
    EZ_RES(29);
    ez->tex2[15] = MTEX(res, 0);
    EZ_RES(30);
    ez->tex2[7] = MTEX(res, 0);
    Flash_Create(&ez->flash[2], MPACK_AT(ez->res, 31), ez->tex2);
    Flash_Play(&ez->flash[2], 1);

    ItemHelp_Init((u32 *)MPACK_AT(ez->res, 2));
    PassWin_Init(MPACK_AT(ez->res, 38));
    ez->dialogText = MPACK_AT(ez->res, 39);
    Dialog_Init(MPACK_AT(ez->res, 32), ez->dialogText, 0);
    Dialog_SetLayout(0);

    ez->items = (UItemEntry *)MPACK_AT(gCommonRes->data[2], 2);
    ez->text[0] = MPACK_AT(ez->res, 34);
    ez->text[1] = MPACK_AT(ez->res, 35);
    ez->text[2] = MPACK_AT(ez->res, 37);
    ez->chipPack = (u32 *)MPACK_AT(ez->res, 36);
    for (i = 0; i < 165; i++) {
        res = (MTexRes *)MPACK_AT(ez->chipPack, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    ez->grid = ((UChrGridList *)MPACK_AT(ez->res, 33))->cell;
    ez->gridCount = ez->res[ez->res[33] >> 2];
}
