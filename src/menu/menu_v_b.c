#include "common.h"
#include "menu/menu_v.h"

/*
 * Menu overlay DBZP.BIN, 0x396838..0x399240: the second source file of the character customising screen
 * (EvoZ): its input, the confirmation dialog, the status numbers and the edits of the saved item sets. The
 * first file (init / term / update / draw / run, 0x392F10..0x396838) is in the previous chunk except for its
 * last function, EvoZ_Load (menu_v.c). File boundary: the strings from 0x3BB6B0 on repeat strings of the first
 * file ("mc_chara_chip_%03d" sits at 0x3BB698 and again at 0x3BB6B0).
 */

/* Sends one of the screen's clips to a label of its timeline. */
void EvoZ_ClipGoto(EvoZ *ez, s32 flash, s32 kind, s32 sub, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *f = &ez->flash[flash];

    switch (kind) {
    case EVOZ_CLIP_CHIP:
        sprintf(name, "mc_chara_chip_%03d", ez->side->col);
        break;
    case EVOZ_CLIP_MENU:
        sprintf(name, "menu_plate_%d", ez->cur[1] + 1);
        break;
    case EVOZ_CLIP_SET:
        strcpy(name, "mc_select_plate");
        break;
    case EVOZ_CLIP_SLOT:
        sprintf(name, "mc_list_plate_%d", ez->cur[3] + 1);
        break;
    case EVOZ_CLIP_ZP:
        sprintf(name, "mc_num_zp_%d", ez->cur[5]);
        break;
    case EVOZ_CLIP_LIST:
        if (sub == 0) {
            sprintf(name, "mc_list_plate_%d", ez->list.cur[ez->list.tab] - ez->list.top[ez->list.tab] + 2);
        } else {
            sprintf(name, "mc_tab_btn_%d", ez->list.tab);
        }
        break;
    default:
        return;
    }
    Flash_FindLabel(f, NULL, name, &ref);
    Flash_ClipGotoLabel(f, &ref, label);
}

#define EZ_CELL (ez->side->col + ez->side->row * 7)
#define EZ_SET (SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set])
#define EZ_L (ez->list)
#define EZ_T (ez->list.tab)

/* Page up / down in the item list: with pad status 1 the direction plus the 0x800 button, else a shoulder button. */
#define EZ_PAGE(dir, shoulder) \
    (gPad[0].status != 1 ? gPad[0].gameRepeat & (shoulder) : (gPad[0].gameRepeat & (dir)) && (gPad[0].gameHeld & 0x800))

/* Pad 0 input of the screen, by state. *result is cleared when the screen is left. */
void EvoZ_Input(EvoZ *ez, s32 *result) {
    MFlashRef ref;

    if (!(ez->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(ez->flags & EVOZ_STARTED)) {
        EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_on_start");
        ez->flags |= EVOZ_STARTED;
    }
    switch (ez->state) {
    case EVOZ_ST_GRID:
        if (gPad[0].gameRepeat & 1) {
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_off_start");
            ChrGrid_MoveLeft(ez->grid, &ez->side->col, ez->side->row);
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_on_start");
            ez->side->chara = ez->side->chip[ez->side->col];
            EvoZ_RefreshStatus(ez);
            EvoZ_RequestFace(ez);
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 2) {
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_off_start");
            ChrGrid_MoveRight(ez->grid, &ez->side->col, ez->side->row);
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_on_start");
            ez->side->chara = ez->side->chip[ez->side->col];
            EvoZ_RefreshStatus(ez);
            EvoZ_RequestFace(ez);
            Snd_PlaySe(2, 0);
        } else if (gPad[0].gameRepeat & 8) {
            if (ez->rows >= 2) {
                Flash_GotoLabel(&ez->flash[1], "fl_reel_down", 1);
                ez->side->mask = 1;
                EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_off_start");
                ChrGrid_MoveUp(ez->grid, &ez->side->col, &ez->side->row, ez->rows);
                EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_on_start");
                EvoZ_SetRowChips(ez);
                ez->side->chara = ez->side->chip[ez->side->col];
                EvoZ_RefreshStatus(ez);
                EvoZ_RequestFace(ez);
                Snd_PlaySe(2, 2);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (ez->rows >= 2) {
                Flash_GotoLabel(&ez->flash[1], "fl_reel_up", 1);
                ez->side->mask = 1;
                EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_off_start");
                ChrGrid_MoveDown(ez->grid, &ez->side->col, &ez->side->row, ez->rows);
                EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_on_start");
                EvoZ_SetRowChips(ez);
                ez->side->chara = ez->side->chip[ez->side->col];
                EvoZ_RefreshStatus(ez);
                EvoZ_RequestFace(ez);
                Snd_PlaySe(2, 1);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            Flash_GotoLabel(&ez->flash[0], "fl_menu_in", 1);
            Flash_GotoLabel(&ez->flash[1], "fl_out", 1);
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_ok");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_on_start");
            ez->cur[3] = 0;
            ez->state = EVOZ_ST_MENU;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_off_start");
            Flash_GotoLabel(&ez->flash[0], "fl_chara_sele_cansel", 1);
            ColorFade_StartOut(0, 0, 0, 0x14);
            *result = 0;
            Snd_PlaySe(1, 2);
        }
        break;
    case EVOZ_ST_MENU:
        if (gPad[0].gameRepeat & 8) {
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_off_start");
            ez->cur[1]--;
            if (ez->cur[1] < 0) {
                ez->cur[1] = 2;
            }
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 4) {
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_off_start");
            ez->cur[1]++;
            if (ez->cur[1] >= 3) {
                ez->cur[1] = 0;
            }
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 1) {
            ez->cur[2]--;
            if (ez->cur[2] < 0) {
                ez->cur[2] = 2;
            }
            ez->cur[3] = 0;
            ez->set = ez->cur[2];
            EvoZ_RefreshStatus(ez);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 2) {
            ez->cur[2]++;
            if (ez->cur[2] >= 3) {
                ez->cur[2] = 0;
            }
            ez->cur[3] = 0;
            ez->set = ez->cur[2];
            EvoZ_RefreshStatus(ez);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            switch (ez->cur[1]) {
            case 0:
                EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_ok");
                EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_off_start");
                Flash_GotoLabel(&ez->flash[0], "fl_custom_list_in", 1);
                Flash_GotoLabel(&ez->flash[2], "fl_costum_list_in", 1);
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
                ez->state = EVOZ_ST_SLOTS;
                Snd_PlaySe(1, 1);
                break;
            case 1:
                EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_ok");
                EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_off_start");
                PassWin_OpenEx(EZ_SET.id, ez->side->chara, SAVE_CUSTOM_AT(VSAVE, EZ_CELL).level);
                ez->state = EVOZ_ST_PASSWORD;
                Snd_PlaySe(1, 1);
                break;
            case 2:
                if (ez->nextExp != 0) {
                    EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_ok");
                    EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_off_start");
                    Flash_GotoLabel(&ez->flash[0], "fl_zp_in", 1);
                    EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_ZP, 0, "fl_on_loop");
                    ez->zp = 0;
                    ez->cur[2] = 3;
                    ez->state = EVOZ_ST_ZP;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
                break;
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&ez->flash[0], "fl_menu_cansel", 1);
            ez->flags &= ~EVOZ_REEL_READY;
            Flash_GotoLabel(&ez->flash[1], "fl_in", 1);
            EvoZ_ClipGoto(ez, 1, EVOZ_CLIP_CHIP, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_off_start");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_off_start");
            ez->state = EVOZ_ST_GRID;
            Snd_PlaySe(1, 2);
        }
        break;
    case EVOZ_ST_SLOTS:
        if (gPad[0].gameRepeat & 8) {
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_off_start");
            if (ez->cur[3] == 7) {
                while (ez->cur[3] >= ez->rowsUsed) {
                    ez->cur[3]--;
                }
                if (EZ_SET.id[ez->cur[3]] == 0) {
                    do {
                        ez->cur[3]--;
                        if (EZ_SET.id[ez->cur[3]] != 0) {
                            ez->cur[3]++;
                            break;
                        }
                    } while (ez->cur[3] != 0);
                }
            } else {
                ez->cur[3]--;
                if (ez->cur[3] < 0) {
                    ez->cur[3] = 7;
                }
            }
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 4) {
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_off_start");
            if (ez->cur[3] == 7) {
                ez->cur[3] = 0;
            } else if (EZ_SET.id[ez->cur[3]] == 0 || ++ez->cur[3] >= ez->rowsUsed) {
                ez->cur[3] = 7;
            }
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 1) {
            ez->cur[8]--;
            if (ez->cur[8] < 0) {
                ez->cur[8] = 3;
            }
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 2) {
            ez->cur[8]++;
            if (ez->cur[8] >= 4) {
                ez->cur[8] = 0;
            }
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            switch (ez->cur[8]) {
            case 0:
                if (ez->cur[3] == 7) {
                    EZ_T = 3;
                } else {
                    EZ_T = ez->lastTab;
                }
                Flash_GotoLabel(&ez->flash[2], "fl_list_in", 1);
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_ok");
                ez->flags |= EVOZ_LIST_BUSY;
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_on_start");
                ez->curItem = EZ_SET.id[ez->cur[3]];
                ez->state = EVOZ_ST_LIST;
                Snd_PlaySe(1, 1);
                break;
            case 1:
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_ok");
                if (EZ_SET.id[ez->cur[3]] != 0) {
                    s32 id;

                    ItemHelp_Open();
                    id = EZ_SET.id[ez->cur[3]];
                    ez->flags |= EVOZ_HELP_SLOT;
                    ez->helpItem = id - 1;
                    gSaveData->item[id - 1] &= ~2;
                    ez->state = EVOZ_ST_HELP;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
                break;
            case 2:
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_ok");
                if (EZ_SET.id[ez->cur[3]] != 0) {
                    gProgress->flags |= 1;
                    EvoZ_RemoveItem(ez);
                    EvoZ_RefreshStatus(ez);
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
                break;
            case 3:
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_ok");
                if (ez->fits != 0 || EZ_SET.id[7] != 0) {
                    ez->dialog.state = 1;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
                break;
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&ez->flash[0], "fl_custom_list_cansel", 1);
            Flash_GotoLabel(&ez->flash[2], "fl_costum_list_cansel", 1);
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_off_start");
            ez->state = EVOZ_ST_MENU;
            Snd_PlaySe(1, 2);
        }
        break;
    case EVOZ_ST_PASSWORD:
        if (gPad[0].gamePressed & 0x400) {
            PassWin_Close();
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_on_start");
            ez->state = EVOZ_ST_MENU;
            Snd_PlaySe(1, 2);
        }
        break;
    case EVOZ_ST_ZP:
        if (gPad[0].gameRepeat & 8) {
            if (ez->zp < VSAVE->money && ez->zp < ez->expLeft) {
                if (ez->cur[ez->state] != 0) {
                    ez->zp += EvoZ_Pow(10, ez->cur[ez->state]);
                } else {
                    ez->zp++;
                }
                if (VSAVE->money < ez->zp) {
                    ez->zp = VSAVE->money;
                }
                if (ez->expLeft < ez->zp) {
                    ez->zp = ez->expLeft;
                }
                Snd_PlaySe(2, 0x2B);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (ez->zp > 0) {
                if (ez->cur[ez->state] != 0) {
                    ez->zp -= EvoZ_Pow(10, ez->cur[ez->state]);
                } else {
                    ez->zp--;
                }
                if (ez->zp < 0) {
                    ez->zp = 0;
                }
                Snd_PlaySe(2, 0x2B);
            }
        } else if (gPad[0].gameRepeat & 1) {
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_ZP, 0, "fl_off");
            ez->cur[ez->state]++;
            if (ez->cur[ez->state] >= 5) {
                ez->cur[ez->state] = 0;
            }
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_ZP, 0, "fl_on_loop");
            Snd_PlaySe(2, 0x2C);
        } else if (gPad[0].gameRepeat & 2) {
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_ZP, 0, "fl_off");
            ez->cur[ez->state]--;
            if (ez->cur[ez->state] < 0) {
                ez->cur[ez->state] = 4;
            }
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_ZP, 0, "fl_on_loop");
            Snd_PlaySe(2, 0x2C);
        } else if (gPad[0].gamePressed & 0x200) {
            if (ez->zp != 0) {
                ez->dialog.state = 5;
                Snd_PlaySe(2, 0x2D);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&ez->flash[0], "fl_zp_cansel", 1);
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_ZP, 0, "fl_off");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_MENU, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 0, EVOZ_CLIP_SET, 0, "fl_on_start");
            ez->cur[2] = ez->set;
            ez->state = EVOZ_ST_MENU;
            Snd_PlaySe(1, 2);
        }
        break;
    case EVOZ_ST_PAYING:
        if (gPad[0].gamePressed & 0x200) {
            VSAVE->money -= ez->zp;
            SAVE_CUSTOM_AT(VSAVE, EZ_CELL).exp += ez->zp;
            ez->exp = SAVE_CUSTOM_AT(VSAVE, EZ_CELL).exp;
            ez->flags ^= EVOZ_PAYING;
            ez->zp = 0;
            if (ez->exp >= ez->nextExp) {
                MFlash *f = &ez->flash[0];

                Flash_FindLabel(f, NULL, "mc_ability_limit_up", &ref);
                Flash_ClipGotoLabel(f, &ref, "fl_ability_limit_up_in");
                ez->flags |= EVOZ_LIMIT_UP;
                do {
                    SAVE_CUSTOM_AT(gSaveData, EZ_CELL).level++;
                    ez->nextExp = ChrTbl_GetExp(ez->side->chara, EZ_CELL);
                } while (ez->exp >= ez->nextExp && ez->nextExp != 0);
                ez->state = EVOZ_ST_LEVEL_UP;
                Snd_PlaySe(2, 0x2F);
            } else {
                ez->state = EVOZ_ST_ZP;
                Snd_PlaySe(1, 1);
            }
            EvoZ_RefreshStatus(ez);
        }
        break;
    case EVOZ_ST_HELP:
        if (gPad[0].gamePressed & 0x200) {
            if (ez->flags & EVOZ_HELP_SLOT) {
                ez->flags ^= EVOZ_HELP_SLOT;
                ItemHelp_Close();
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
                ez->state = EVOZ_ST_SLOTS;
                Snd_PlaySe(1, 2);
            } else if (EvoZ_CanEquip(ez, ez->helpItem)) {
                EZ_SET.id[ez->cur[3]] = EZ_L.ids[EZ_T][EZ_L.cur[EZ_T]] + 1;
                ez->curItem = EZ_SET.id[ez->cur[3]];
                gProgress->flags |= 1;
                EvoZ_RefreshStatus(ez);
                ItemHelp_Close();
                Flash_GotoLabel(&ez->flash[2], "fl_list_out", 1);
                ez->flags |= EVOZ_LIST_BUSY;
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_off_start");
                ez->state = EVOZ_ST_SLOTS;
                Snd_PlaySe(1, 1);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            ItemHelp_Close();
            if (ez->flags & EVOZ_HELP_SLOT) {
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
                ez->flags ^= EVOZ_HELP_SLOT;
                ez->state = EVOZ_ST_SLOTS;
            } else {
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
                ez->state = EVOZ_ST_LIST;
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case EVOZ_ST_LIST:
        if (EZ_PAGE(8, 0x4000)) {
            if (EZ_L.cur[EZ_T] > 0) {
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
                EZ_L.cur[EZ_T] -= 6;
                if (EZ_L.cur[EZ_T] < EZ_L.top[EZ_T]) {
                    if (EZ_L.cur[EZ_T] < 6) {
                        EZ_L.edge = EZ_L.top[EZ_T];
                        EZ_L.cur[EZ_T] = 0;
                        EZ_L.top[EZ_T] = 0;
                        EZ_L.bottom[EZ_T] = 5;
                    } else {
                        EZ_L.edge = EZ_L.top[EZ_T];
                        EZ_L.top[EZ_T] -= 6;
                        EZ_L.bottom[EZ_T] -= 6;
                    }
                }
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (EZ_PAGE(4, 0x10000)) {
            if (EZ_L.cur[EZ_T] < EZ_L.count[EZ_T] - 1) {
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
                EZ_L.cur[EZ_T] += 6;
                if (EZ_L.bottom[EZ_T] < EZ_L.cur[EZ_T]) {
                    if (EZ_L.count[EZ_T] - 6 < EZ_L.cur[EZ_T]) {
                        EZ_L.cur[EZ_T] = EZ_L.count[EZ_T] - 1;
                        EZ_L.edge = EZ_L.bottom[EZ_T];
                        EZ_L.top[EZ_T] = EZ_L.count[EZ_T] - 6;
                        EZ_L.bottom[EZ_T] = EZ_L.count[EZ_T] - 1;
                    } else {
                        EZ_L.edge = EZ_L.bottom[EZ_T];
                        EZ_L.top[EZ_T] += 6;
                        EZ_L.bottom[EZ_T] += 6;
                    }
                }
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & 8) {
            if (EZ_L.cur[EZ_T] > 0) {
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
                EZ_L.cur[EZ_T]--;
                if (EZ_L.cur[EZ_T] < EZ_L.top[EZ_T]) {
                    Flash_GotoLabel(&ez->flash[2], "fl_list_down", 1);
                    EZ_L.edge = EZ_L.bottom[EZ_T];
                    EZ_L.top[EZ_T]--;
                    EZ_L.bottom[EZ_T]--;
                }
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & 4) {
            if (EZ_L.cur[EZ_T] < EZ_L.count[EZ_T] - 1) {
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
                EZ_L.cur[EZ_T]++;
                if (EZ_L.cur[EZ_T] > EZ_L.bottom[EZ_T]) {
                    Flash_GotoLabel(&ez->flash[2], "fl_list_up", 1);
                    EZ_L.edge = EZ_L.top[EZ_T];
                    EZ_L.top[EZ_T]++;
                    EZ_L.bottom[EZ_T]++;
                }
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if ((gPad[0].gameRepeat & 1) && EZ_T != 3) {
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_off_start");
            EZ_T--;
            if (EZ_T < 0) {
                EZ_T = 2;
            }
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_on_start");
            ez->lastTab = EZ_T;
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & 2) && EZ_T != 3) {
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_off_start");
            EZ_T++;
            if (EZ_T >= 3) {
                EZ_T = 0;
            }
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_on_start");
            ez->lastTab = EZ_T;
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_ok");
            if ((u8)(VSAVE->item[EZ_L.ids[EZ_T][EZ_L.cur[EZ_T]]] & 1)) {
                s32 id;

                ItemHelp_Open();
                id = EZ_L.ids[EZ_T][EZ_L.cur[EZ_T]];
                ez->helpItem = id;
                gSaveData->item[id] &= ~2;
                ez->state = EVOZ_ST_HELP;
                Snd_PlaySe(1, 1);
            } else {
                Snd_PlaySe(1, 7);
            }
        } else if (gPad[0].gamePressed & 0x400) {
            Flash_GotoLabel(&ez->flash[2], "fl_list_out", 1);
            ez->flags |= EVOZ_LIST_BUSY;
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_off_start");
            EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 1, "fl_off_start");
            ez->state = EVOZ_ST_SLOTS;
            Snd_PlaySe(1, 2);
        }
        break;
    }
}

/* The two yes / no questions: "remove every item of the set?" (states 1..4) and the experience payment
   (states 5..8). A yes takes effect after 12 frames. */
void EvoZ_UpdateDialog(EvoZ *ez) {
    EvoZDialog *d = &ez->dialog;

    if (d->state == 0) {
        return;
    }
    switch (d->state) {
    case 1:
        d->state = 2;
        Dialog_SetLayout(0);
        Dialog_Start(0);
        Dialog_SetCursor(1);
        d->answer = 0;
    case 2:
        Dialog_SetChoices(1);
        Dialog_Start(0);
        d->msg = 1;
        if (d->answer > 0) {
            d->timer--;
            if (d->timer < 0) {
                gProgress->flags |= 1;
                EvoZ_RemoveAllItems(ez);
                EvoZ_RefreshStatus(ez);
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_off_start");
                ez->cur[3] = 0;
                EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
                d->timer = 0x78;
                d->state = 3;
            }
        } else if (d->answer < 0) {
            Dialog_SetCursor(1);
            d->timer = 0x78;
            d->state = 3;
        } else {
            d->answer = Dialog_Input(1);
            d->timer = 12;
        }
        break;
    case 3:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        Dialog_Input(0);
        d->state = 4;
        break;
    case 5:
        d->state = 6;
        Dialog_SetLayout(0);
        Dialog_Start(0);
        Dialog_SetCursor(1);
        d->answer = 0;
    case 6:
        Dialog_SetChoices(1);
        Dialog_Start(0);
        d->msg = 0;
        if (d->answer > 0) {
            d->timer--;
            if (d->timer < 0) {
                gProgress->flags |= 1;
                ez->state = EVOZ_ST_PAYING;
                ez->flags |= EVOZ_PAYING;
                d->timer = 0x78;
                d->state = 7;
            }
        } else if (d->answer < 0) {
            Dialog_SetCursor(1);
            d->timer = 0x78;
            d->state = 7;
        } else {
            d->answer = Dialog_Input(1);
            d->timer = 12;
        }
        break;
    case 7:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        Dialog_Input(0);
        d->state = 8;
        break;
    case 4:
    case 8:
        Dialog_Input(0);
        if (Dialog_IsClosed()) {
            d->state = 0;
        }
        break;
    }
    Dialog_SetMsg(d->msg);
    Dialog_Draw(1);
}

/* Switches between the slot list and the item list once the list movie has finished moving. */
void EvoZ_ToggleList(EvoZ *ez) {
    MFlashRef ref;
    char name[64];
    MFlash *f = &ez->flash[2];
    s32 i;

    ez->listSide ^= 1;
    ez->flags &= ~EVOZ_LIST_BUSY;
    if (ez->listSide == 0) {
        EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_on_start");
        for (i = 0; i < 8; i++) {
            sprintf(name, "mc_list_plate_%d", i + 1);
            Flash_FindLabel(f, NULL, name, &ref);
            Flash_ClipSetCallbackA(f, &ref, NULL, NULL);
            Flash_ClipSetCallbackB(f, &ref, NULL, NULL);
            Flash_ClipSetColor(f, &ref, 1.0f);
        }
    } else {
        EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_SLOT, 0, "fl_off_start");
        EvoZ_ClipGoto(ez, 2, EVOZ_CLIP_LIST, 0, "fl_on_start");
    }
}

/* base to the power of exp (exp >= 1). */
s32 EvoZ_Pow(s32 base, s32 exp) {
    s32 ret = base;
    s32 i;

    for (i = 1; i < exp; i++) {
        ret *= base;
    }
    return ret;
}

/* Experience at which a character's current level began (0 at level 0). */
s32 EvoZ_GetLevelExp(EvoZ *ez, s32 chara, s32 cell) {
    VChrEntry *tbl = (VChrEntry *)MPACK_AT(gCommonRes->data[2], 1);
    s32 ret;

    if (SAVE_CUSTOM_AT(gSaveData, cell).level != 0) {
        ret = tbl[chara].exp[SAVE_CUSTOM_AT(gSaveData, cell).level - 1];
    } else {
        ret = 0;
    }
    return ret;
}


/* Recomputes what the status panel shows for the character and item set under the cursor. */
void EvoZ_RefreshStatus(EvoZ *ez) {
    s32 base;

    ez->kind = (((VChrEntry *)MPACK_AT(gCommonRes->data[2], 1))[ez->side->chara].flags ^ 1) & 1;
    ez->capacity = ChrTbl_GetLevel(ez->side->chara, EZ_CELL, 0);
    ez->fits = ItemSet_Fit(SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id, ez->items, ez->capacity, &ez->rowsUsed);
    ez->exp = SAVE_CUSTOM_AT(VSAVE, EZ_CELL).exp;
    ez->nextExp = ChrTbl_GetExp(ez->side->chara, EZ_CELL);
    ez->expLeft = ChrTbl_GetMaxExp(ez->side->chara) - ez->exp;
    ItemSet_GetBonus(SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id, ez->items, ez->bonus);
    base = EvoZ_GetLevelExp(ez, ez->side->chara, EZ_CELL);
    ez->expBar = (f32)(ez->exp - base) / (f32)(ez->nextExp - base) * 6.66f;
}

/* Puts the characters of the cursor's grid row on the seven chips of the reel. */
void EvoZ_SetRowChips(EvoZ *ez) {
    s32 order[7] = { 8, 11, 12, 13, 14, 15, 16 };
    s32 i;

    for (i = 0; i < 7; i++) {
        s32 id = ez->grid[i + ez->side->row * 7].id;
        MTexRes *res;

        ez->side->prevChip[i] = ez->side->chip[i];
        ez->side->chip[i] = id;
        res = (MTexRes *)MPACK_AT(ez->chipPack, id + 1);
        ez->tex1[18 + i] = ez->tex1[order[i]];
        ez->tex1[order[i]] = res->tex;
    }
}

/* One step of the portrait loader (called every frame). */
void EvoZ_UpdateFace(EvoZ *ez) {
    MTexRes *res;

    switch (ez->faceState) {
    case 5:
        ez->faceState = 6;
        break;
    case 6:
        if (ez->side->flags & EVOZ_SIDE_FACE_CHANGE) {
            ez->side->flags ^= EVOZ_SIDE_FACE_CHANGE;
        }
        ez->faceState = 1;
        break;
    case 1:
        File_CancelRequests();
#ifdef PORT
        File_Request(ez->side->chara + (PortGame_IsBT4() ? 0x71C : 0x2F9), ez->faceFile, 0x16800);
#else
        File_Request(ez->side->chara + 0x2F9, ez->faceFile, 0x16800);
#endif
        ez->faceState = 2;
        break;
    case 2:
        if (File_UpdateRequests()) {
            ez->faceState = 3;
        }
        break;
    case 3:
        Sprite_Unpack(ez->faceFile, ez->faceRes, NULL);
        res = ez->faceRes;
        Res_RelocateOffsets(&res, res, res);
        ez->tex0[42] = MTEX(res, 0);
        ez->side->flags |= EVOZ_SIDE_FACE_READY;
        ez->faceState = 4;
        break;
    case 4:
        if (ez->side->flags & EVOZ_SIDE_FACE_CHANGE) {
            ez->tex0[42] = NULL;
            ez->faceState = 6;
        }
        break;
    }
}

/* Asks for the portrait of the character under the cursor. */
void EvoZ_RequestFace(EvoZ *ez) {
    if (ez->side->flags & EVOZ_SIDE_FACE_READY) {
        ez->side->flags ^= EVOZ_SIDE_FACE_READY;
    } else {
        ez->faceState = 5;
    }
    ez->side->flags |= EVOZ_SIDE_FACE_CHANGE;
}

/* Whether an item can go into the slot under the cursor: owned, allowed for the character, within the slot
   capacity and not of the same kind as an item in another slot. */
s32 EvoZ_CanEquip(EvoZ *ez, s32 item) {
    s32 ret = 1;
    u16 *ids;
    s32 used;
    s32 i;

    if (!(u8)(VSAVE->item[item] & 1)) {
        return 0;
    }
    ids = SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id;
    if (!ItemTbl_CanEquip(item, ez->side->chara, ez->items)) {
        ret = 0;
    } else {
        if (ez->curItem != 0) {
            used = ez->fits - ez->items[ez->curItem - 1].slots;
        } else {
            used = ez->fits;
        }
        if (ez->capacity >= used + ez->items[item].slots) {
            for (i = 0; i < 7; i++) {
                if (i != ez->cur[3] && ids[i] != 0) {
                    if (ez->items[item].type == ez->items[ids[i] - 1].type) {
                        if (ez->items[item].group == ez->items[ids[i] - 1].group) {
                            ret = 0;
                            break;
                        }
                    }
                }
            }
        } else {
            ret = 0;
        }
    }
    return ret;
}

/* Empties the slot under the cursor and closes the gap in the set. */
void EvoZ_RemoveItem(EvoZ *ez) {
    s32 i;

    SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id[ez->cur[3]] = 0;
    if (ez->cur[3] != 7) {
        for (i = ez->cur[3]; i < 6; i++) {
            SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id[i] = SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id[i + 1];
        }
        SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id[6] = 0;
    }
}

/* Empties the whole item set. */
void EvoZ_RemoveAllItems(EvoZ *ez) {
    s32 i;

    for (i = 0; i < 8; i++) {
        SAVE_CUSTOM_AT(VSAVE, EZ_CELL).set[ez->set].id[i] = 0;
    }
}
