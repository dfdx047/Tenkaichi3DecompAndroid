#include "common.h"
#include "menu/menu_z.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * Menu overlay DBZP.BIN, 0x3A65E8..0x3A9850: the DcList object, the list of saved custom characters of the Data
 * Center (progress mode 55, handler Dc_Main 0x3A9850). (Written as two halves, menu_y_b.c 0x3A65E8..0x3A7D98 =
 * the list itself, its menu and most of the drawing, and menu_z.c 0x3A7D98..0x3A9850 = the item page, the
 * dialogs, init / draw / term / run; merged here.) The work structure is a local variable of DcList_Run, not a
 * heap block, so the object has no work pointer and no `.data`.
 *
 * Object boundary: the first function follows Option_Term, the last function of the option screen object;
 * the object's read-only data is 0x3BCA00 ("mc_guide_blma_eye") .. 0x3BD34C (the jump table of DcList_Input),
 * behind a block of initialised data (0x3BC928..0x3BCA00: the `.data` of the following objects of this link
 * group: gDcPassKeys, gPassWin, gPassKeys, gReplayMenu, gDcSave).
 *
 * The code is written against small sub-structures of the work (DcView, DcChars, DcStatus), each passed by
 * pointer; DcChars_GetRec returns a record by value.
 */

/*
 * Small helpers that the original inlined: each use has its own clip reference on the stack (16 bytes apart)
 * whose address is worked out again at every use, which is what an inlined function's locals look like.
 */

/* Gives a child clip its texture rectangle. */
static inline void DcView_SetChildUv(DcView *v, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, parent, name, &ref);
    Flash_ClipSetUv(v->flash, &ref, uv);
}

/* Shows or hides a child clip. */
static inline void DcView_ShowChild(DcView *v, char *parent, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, parent, name, &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, on);
}

/* Texture rectangle of a row of a sheet of w x h pictures. */
static inline void DcUv_SetRow(MFlashUv *uv, s32 row, s32 w, s32 h) {
    uv->x0 = 0;
    uv->y0 = row * h;
    uv->x1 = w;
    uv->y1 = row * h + h;
}

/* Starts the guide's blink timer at a random phase. */
void DcView_InitBlink(DcView *v) {
    v->blink = Rand_Range(0x20);
}

/* Copies the 14 saved custom characters out of the save; no row is scrolling. */
void DcChars_Load(DcChars *c) {
    s32 i;

    c->extra = -1;
    for (i = 0; i < SAVE_REC_COUNT; i++) {
        /* gSaveData->rec[i]; the byte offset is worked out first in the original */
        s32 off = i * sizeof(ZSaveRec) + 0x2D40;

        c->rec[i] = *(ZSaveRec *)((u8 *)gSaveData + off);
    }
}

/* Works out what the record's items add up to. */
void DcStatus_Calc(DcStatus *s, ZItemEntry *table) {
    ItemSet_GetBonus(s->rec.item, table, s->bonus);
}

/* Resets the list's state for a new visit. */
void DcList_Reset(DcList *d) {
    DcView_InitBlink(&d->view);
    DcChars_Load(&d->chars);
    d->unkE7C = 0x1E;
}

/* Animates the guide's (Bulma's) eyes and mouth. */
void DcList_UpdateGuide(DcList *d) {
    MFlashRef ref;
    MFlash *flash = d->view.flash;

    Flash_FindLabel(flash, NULL, "mc_guide_blma_eye", &ref);
    FlashAnim_Blink(flash, &ref, &d->view.blink, 0);
    Flash_FindLabel(flash, NULL, "mc_guide_blma_mouth", &ref);
    if (Voice_GetStat(0) != MVOICE_IDLE) {
        FlashAnim_Talk(flash, &ref, &d->view.talk, 0);
    } else {
        FlashAnim_ShowNext2(flash, &ref, 0);
    }
}

/* Scrolls the backdrop strip "mc_compane_3". */
void DcList_ScrollCloud(DcList *d) {
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash = d->view.flash;

    uv.y0 = 0;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_compane_3", &ref);
    FlashAnim_Scroll(flash, &ref, &uv, &d->cloudX, NULL, -0.15238095f, 0.0f);
}

/* Clip callback: limits drawing to the list area. */
void DcList_SetListScissor(void) {
    Sprite_SetScissor(0xAF, 0x1FF, 0x6A, 0x139);
}

/* Clip callback: back to the whole screen. */
void DcList_ResetScissor(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Hangs the scissor callbacks on the four list plates. */
void DcView_SetRowCallbacks(DcView *v) {
    MFlashRef ref;
    char name[0x100];
    s32 i;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        Flash_FindLabel(v->flash, NULL, name, &ref);
        Flash_ClipSetCallbackA(v->flash, &ref, DcList_SetListScissor, NULL);
        Flash_ClipSetCallbackB(v->flash, &ref, DcList_ResetScissor, NULL);
    }
}

/* Hides the up or the down arrow of the list. */
void DcView_HideArrow(DcView *v, s32 up) {
    MFlashRef ref;
    char parent[0x100];
    char name[0x100];

    if (up) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(v->flash, parent, name, &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, 0);
}

/* Gives the two arrows their pictures and hides the one that cannot be used. */
void DcView_SetArrows(DcView *v, DcChars *c) {
    MFlashUv uv;
    s32 top;

    uv.x0 = 0;
    uv.x1 = 0x20;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    DcView_SetChildUv(v, "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    DcView_SetChildUv(v, "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    top = c->top;
    if (top == 0) {
        DcView_HideArrow(v, 1);
    } else if (top == DC_TOP_MAX) {
        DcView_HideArrow(v, 0);
    }
}

/* Places the scroll bar's knob. */
void DcView_SetScrollBar(DcView *v, DcChars *c) {
    MFlashRef ref;
    s32 n = SAVE_REC_COUNT;
    s32 y = c->top * 0xB5 / n;

    Flash_FindLabel(v->flash, NULL, "mc_scroll_bar_point", &ref);
    Flash_ClipSetScale(v->flash, &ref, 1.0f, 1.2128571f);
    Flash_ClipSetOffset(v->flash, &ref, 0, y);
}

/* Lights or dims the plate the cursor is on. */
void DcView_LightRow(DcView *v, DcChars *c, s32 on) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "mc_menu_plate2_%d", c->row + 1);
    Flash_FindLabel(v->flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_off_start");
    }
}

/* Keeps the list's top inside 0..11; 0 when it had to be corrected. */
s32 DcChars_ClampTop(DcChars *c) {
    if (c->top < 0) {
        c->top = 0;
        return 0;
    }
    if (c->top > DC_TOP_MAX) {
        c->top = DC_TOP_MAX;
        return 0;
    }
    return 1;
}

/* Keeps the cursor inside the three plates; 1 when it had to be corrected (the list must scroll). */
s32 DcChars_ClampRow(DcChars *c) {
    if (c->row < 0) {
        c->row = 0;
        return 1;
    }
    if (c->row > DC_ROWS - 1) {
        c->row = DC_ROWS - 1;
        return 1;
    }
    return 0;
}

/* Fills the four list plates: name and form of each record, empty records dimmed. */
void DcView_SetRows(DcView *v, DcChars *c) {
    MFlashRef ref;
    char name[0x100];
    s32 i;
    s32 chara;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        if (i != 3) {
            chara = c->rec[i + c->top].chara;
        } else {
            chara = c->rec[c->extra].chara;
        }
        if (i == c->row) {
            if (chara == -1) {
                Flash_FindLabel(v->flash, NULL, name, &ref);
                Flash_ClipSetAlpha(v->flash, &ref, 1.0f);
                continue;
            }
        } else if (chara == -1) {
            Flash_FindLabel(v->flash, NULL, name, &ref);
            Flash_ClipSetAlpha(v->flash, &ref, 0.5f);
            continue;
        }
        Flash_FindLabel(v->flash, name, "mc_menu_text1_on", &ref);
        TextBox_AttachLine(v->flash, &ref, 0, 0, chara, &v->nameBox[i]);
        Flash_FindLabel(v->flash, name, "mc_menu_text2_on", &ref);
        TextBox_AttachLine(v->flash, &ref, 0, 0, chara, &v->formBox[i]);
        Flash_FindLabel(v->flash, NULL, name, &ref);
        Flash_ClipSetAlpha(v->flash, &ref, 1.0f);
    }
}

/* The record the cursor is on. */
s32 DcChars_GetCursor(DcChars *c) {
    return c->top + c->row;
}

/* A copy of the record the cursor is on. */
ZSaveRec DcChars_GetRec(DcChars *c) {
    return c->rec[DcChars_GetCursor(c)];
}

/* Takes the record under the cursor as the one the details panel shows. */
void DcList_PickRec(DcList *d) {
    DcStatus *s = &d->status;
    ZChrEntry *tbl;

    d->status.rec = DcChars_GetRec(&d->chars);
    DcStatus_Calc(s, d->itemTbl);
    tbl = (ZChrEntry *)MPACK_AT(gCommonRes->data[2], 1);
    s->attr = (tbl[s->rec.chara].flags ^ 1) & 1;
}

/* Plays the "chosen" animation of the cursor's plate. */
void DcList_RowOk(DcList *d) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_menu_plate2_%d", d->chars.row + 1);
    flash = d->view.flash;
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Per frame: sets up everything the list part of the movie shows. */
void DcList_DrawList(DcList *d) {
    DcView *v = &d->view;
    DcChars *c = &d->chars;

    DcView_SetRowCallbacks(v);
    DcView_SetArrows(v, c);
    DcView_SetScrollBar(v, c);
    DcView_SetRows(v, c);
}

/* Input of the list: up / down one record, left / right three, confirm opens the details, cancel leaves. */
void DcList_InputList(DcList *d) {
    ZSaveRec rec;

    if (gPad[0].gameRepeat & 8) {
        DcChars *c = &d->chars;

        if (DcChars_GetCursor(c) != 0) {
            DcView *v = &d->view;

            DcView_LightRow(v, c, 0);
            d->chars.row--;
            if (DcChars_ClampRow(c)) {
                c->top--;
                if (DcChars_ClampTop(c)) {
                    Flash_GotoLabel(v->flash, "fl_chara_list_down", 1);
                    d->chars.extra = c->top + 3;
                }
            }
            DcView_LightRow(v, c, 1);
            Snd_PlaySe(1, 0);
            return;
        }
    }
    if (gPad[0].gameRepeat & 4) {
        DcChars *c = &d->chars;

        if (DcChars_GetCursor(c) != SAVE_REC_COUNT - 1) {
            DcView *v = &d->view;

            DcView_LightRow(v, c, 0);
            d->chars.row++;
            if (DcChars_ClampRow(c)) {
                c->top++;
                if (DcChars_ClampTop(c)) {
                    Flash_GotoLabel(v->flash, "fl_chara_list_up", 1);
                    d->chars.extra = c->top - 1;
                }
            }
            DcView_LightRow(v, c, 1);
            Snd_PlaySe(1, 0);
            return;
        }
    }
    if (gPad[0].gamePressed & 0x200) {
        rec = DcChars_GetRec(&d->chars);
        if (rec.chara >= 0) {
            DcView *v;

            DcList_RowOk(d);
            v = &d->view;
            Flash_GotoLabel(v->flash, "fl_chara_details_in", 1);
            MsgWin_Close();
            d->state = DCLIST_ST_MENU;
            DcView_LightMenu(v, d->menuCursor, 0);
            d->menuCursor = 0;
            DcView_LightMenu(v, 0, 1);
            DcList_PickRec(d);
            d->flags |= DCLIST_FACE_CHANGE;
            Snd_PlaySe(1, 1);
        } else {
            Snd_PlaySe(1, 7);
        }
        DcList_RowOk(d);
        return;
    }
    if (gPad[0].gamePressed & 0x400) {
        d->result = 0;
        d->flags |= DCLIST_LEAVE;
        Flash_GotoLabel(d->view.flash, "fl_chara_list_cancel", 1);
        MsgWin_Close();
        Snd_PlaySe(1, 2);
        return;
    }
    if (gPad[0].gamePressed & 1) {
        s32 top = d->chars.top;

        if (top + 2 >= 3) {
            DcChars *c;

            d->chars.top = top - 3;
            c = &d->chars;
            DcChars_ClampTop(c);
            Flash_GotoLabel(d->view.flash, "fl_chara_list_down", 1);
            d->chars.extra = c->top + 3;
            Snd_PlaySe(1, 0);
            return;
        }
    }
    if (gPad[0].gamePressed & 2) {
        s32 top = d->chars.top;

        if (top != DC_TOP_MAX) {
            DcChars *c;

            d->chars.top = top + 3;
            c = &d->chars;
            DcChars_ClampTop(c);
            Flash_GotoLabel(d->view.flash, "fl_chara_list_up", 1);
            d->chars.extra = c->top - 1;
            Snd_PlaySe(1, 0);
        }
    }
}

/* Gives the three plates of the details menu their text pictures. */
void DcView_SetMenuText(DcView *v) {
    MFlashUv uv;
    char name[0x100];
    s32 i;

    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x100;
        uv.y1 = i * 0x20 + 0x20;
        sprintf(name, "menu_plate_%d", i + 1);
        DcView_SetChildUv(v, name, "mc_menu_text_on", &uv);
        DcView_SetChildUv(v, name, "mc_menu_text_off", &uv);
    }
}

/* Draws the status panel: used item slots, the four stat changes as rows of marks, the attribute. */
void DcView_SetStatus(DcView *v, DcStatus *s) {
    MFlashUv uv;
    char slots[0x100];
    char base[0x100];
    char name[0x100];
    s32 i;
    s32 k;
    s32 val;

    sprintf(slots, "mc_status_ability_base%d_%d", 2, 1);
    for (i = s->bonus[0]; i < 7; i++) {
        sprintf(name, "mc_status_ability_plus_%d", i + 4);
        DcView_ShowChild(v, slots, name, 0);
    }
    DcUv_SetRow(&uv, i, 0x80, 0x14);
    sprintf(name, "mc_text_ability_%d", 1);
    DcView_SetChildUv(v, slots, name, &uv);
    for (i = 0; i < 4; i++) {
        val = s->bonus[1 + i];
        sprintf(base, "mc_status_ability_base%d_%d", 1, i + 1);
        for (k = 0; k < 4; k++) {
            sprintf(name, "mc_status_ability_minus_%d", k + 1);
            DcView_ShowChild(v, base, name, 0);
            sprintf(name, "mc_status_ability_plus_%d", k + 1);
            DcView_ShowChild(v, base, name, 0);
            if (val < 0 && val < -k && -k <= 0) {
                sprintf(name, "mc_status_ability_minus_%d", k + 1);
                DcView_ShowChild(v, base, name, 1);
            } else if (val > 0 && k >= 0 && k < val) {
                sprintf(name, "mc_status_ability_plus_%d", k + 1);
                DcView_ShowChild(v, base, name, 1);
            }
        }
        DcUv_SetRow(&uv, i + 1, 0x80, 0x14);
        sprintf(name, "mc_text_ability_%d", 1);
        DcView_SetChildUv(v, base, name, &uv);
    }
    {
        MFlashRef ref;

        Flash_FindLabel(v->flash, NULL, "mc_status_attribute", &ref);
        Flash_ClipSetTex(v->flash, &ref, s->attr);
    }
}

/* Lights or dims a plate of the details menu. */
void DcView_LightMenu(DcView *v, s32 item, s32 on) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "menu_plate_%d", item + 1);
    Flash_FindLabel(v->flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(v->flash, &ref, "fl_off_start");
    }
}

/* Wraps the details menu's cursor into 0..2. */
void DcList_WrapMenu(s32 *cursor) {
    s32 max = 2;

    if (*cursor < 0) {
        *cursor = max;
    } else if (*cursor > max) {
        *cursor = 0;
    }
}

/* Hides the backing plates of the name and form text. */
void DcView_HideNamePlates(DcView *v) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, "mc_name_plate_l", "ch_name_plate", &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, 0);
    Flash_FindLabel(v->flash, "mc_form_plate_l", "ch_form_plate", &ref);
    Flash_ClipSetFlags(v->flash, &ref, 2, 0);
}

/* Shows a character's name and form in the details panel. */
void DcView_SetNameText(DcView *v, s32 line) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(v->flash, &ref, 0, 0, line, &v->nameBoxB);
    Flash_FindLabel(v->flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(v->flash, &ref, 0, 0, line, &v->formBoxB);
}

/*
 * Sets how opaque the character picture is. Inlined into the draw function of the next chunk (0x3A9478); its
 * string lies here in the object's read-only data, between those of DcView_SetNameText and DcView_LightItem,
 * so this is where it was defined.
 */
static inline void DcView_SetPictureAlpha(DcView *v, f32 alpha) {
    MFlashRef ref;

    Flash_FindLabel(v->flash, NULL, "mc_single_chara_r", &ref);
    Flash_ClipSetAlpha(v->flash, &ref, alpha);
}

/* Plays the "chosen" animation of a plate of the details menu. */
void DcView_MenuOk(DcView *v, s32 item) {
    MFlashRef ref;
    char name[0x100];

    sprintf(name, "menu_plate_%d", item + 1);
    Flash_FindLabel(v->flash, NULL, name, &ref);
    Flash_ClipGotoLabel(v->flash, &ref, "fl_ok");
}

/* Whether an item slot holds no valid item id; never true for the last slot. */
s32 DcRec_IsSlotEmpty(u16 *items, s32 slot) {
    s32 empty = 0;

    items += slot;
    if ((u16)(*items - 1) >= SAVE_ITEM_COUNT) {
        empty = slot != 7;
    }
    return empty;
}

/* Lights or dims a row of the item list (the second movie). */
void DcView_LightItem(DcView *v, s32 row, s32 on) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_list_plate_%d", row + 1);
    flash = &v->flash[1];
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Input of the details menu: 0 opens the item list, 1 builds the password, 2 asks whether to delete. */
void DcList_InputMenu(DcList *d) {
    ZSaveRec rec;

    if (gPad[0].gamePressed & 0x200) {
        DcView *v = &d->view;

        DcView_MenuOk(v, d->menuCursor);
        switch (d->menuCursor) {
            case 0:
                d->state = DCLIST_ST_ITEMS;
                Flash_GotoLabel(&d->view.flash[1], "fl_evo_in", 1);
                DcView_LightItem(v, d->itemCursor, 0);
                d->itemCursor = 0;
                while (DcRec_IsSlotEmpty(d->status.rec.item, d->itemCursor)) {
                    d->itemCursor++;
                }
                DcView_LightItem(v, d->itemCursor, 1);
                Snd_PlaySe(1, 1);
                break;
            case 1:
                rec = DcChars_GetRec(&d->chars);
                PassWin_OpenEx(&rec, rec.chara, rec.level);
                d->state = DCLIST_ST_PASSWORD;
                Snd_PlaySe(1, 1);
                break;
            case 2:
                d->state = DCLIST_ST_DIALOG;
                Dialog_SetMsg(2);
                Dialog_Start(0);
                Dialog_SetCursor(1);
                Dialog_SetChoices(1);
                Snd_PlaySe(1, 1);
                break;
        }
    } else if (gPad[0].gamePressed & 0x400) {
        Flash_GotoLabel(d->view.flash, "fl_chara_details_out", 1);
        MsgWin_Open();
        d->state = DCLIST_ST_LIST;
        Snd_PlaySe(1, 2);
    } else if (gPad[0].gameRepeat & 8) {
        DcView *v = &d->view;
        s32 *cursor;

        DcView_LightMenu(v, d->menuCursor, 0);
        d->menuCursor--;
        cursor = &d->menuCursor;
        DcList_WrapMenu(cursor);
        DcView_LightMenu(v, *cursor, 1);
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & 4) {
        DcView *v = &d->view;
        s32 *cursor;

        DcView_LightMenu(v, d->menuCursor, 0);
        d->menuCursor++;
        cursor = &d->menuCursor;
        DcList_WrapMenu(cursor);
        DcView_LightMenu(v, *cursor, 1);
        Snd_PlaySe(1, 0);
    }
}

/* ---- second half of the object (was src/menu/menu_z.c, 0x3A7D98..0x3A9850) ---- */

/* Starts a line of the guide and shows its subtitle. */
static inline void DcList_Say(DcList *list, s32 line) {
    Voice_PlayWithSubtitle(list->subtitles, DC_VOICE_BASE, line);
    list->voiceLine = line;
}

/* Sets the texture rectangle of a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcList_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Texture rectangle of one cell of a sheet. */
static inline void DcList_SetCell(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->y0 = row * h;
    uv->x1 = uv->x0 + w;
    uv->y1 = uv->y0 + h;
}

/* Keeps the item page's cursor in 0..7 (a ring). */
void DcList_WrapItemCursor(s32 *cursor) {
    s32 max = DCLIST_ITEM_ROWS - 1;

    if (*cursor < 0) {
        *cursor = max;
    } else if (*cursor > max) {
        *cursor = 0;
    }
}

/* Whether the cursor skips a row of the item page: it holds no item, and it is not the last row. */
s32 DcList_IsRowSkipped(u16 *items, s32 row) {
    s32 ret = 0;

    items += row;
    if ((u16)(*items - 1) >= SAVE_ITEM_COUNT) {
        ret = row != DCLIST_ITEM_ROWS - 1;
    }
    return ret;
}

/* Puts the character's name and form name on the large plates of the item page. */
void DcList_DrawNames(DcView *view, s32 chara) {
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash = &view->flash[1];

    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->nameBoxL);
    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x100;
    uv.y1 = 0x20;
    DcList_SetUv(flash, NULL, "mc_name_plate_l", &uv);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &view->formBoxL);
}

/* Lights or dims a row of the item page. */
void DcList_LightItemRow(DcView *view, s32 row, s32 on) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_list_plate_%d", row + 1);
    flash = &view->flash[1];
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Fills the eight rows of the item page from the current record: name, cost icon, kind icon. */
void DcList_DrawItems(DcList *list, s32 cursor) {
    MFlash *flash = &list->view.flash[1];
    MFlashRef ref;
    MFlashUv uv;
    char name[0x100];
    s32 i;

    for (i = 0; i < DCLIST_ITEM_ROWS; i++) {
        u16 item = list->status.rec.item[i];
        s32 slots = list->itemTbl[item - 1].slots;

        sprintf(name, "mc_list_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
        if (item != 0) {
            TextBox_AttachLine(flash, &ref, 0, 0, item - 1, &list->view.itemBox[i]);
        }
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        if (i == cursor) {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
        uv.x0 = 0x40;
        uv.y0 = 0;
        uv.x1 = 0x80;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, name, "mc_icon_custom", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (item != 0) {
            if (slots != 0) {
                DcList_SetCell(&uv, list->itemTbl[item - 1].slots - 1, ItemTbl_GetClass(item - 1, list->itemTbl), 0x20, 0x2A);
                DcList_SetUv(flash, name, "mc_icon_item_cost", &uv);
                Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
            uv.x0 = list->itemTbl[item - 1].type << 6;
            uv.y0 = 0;
            uv.x1 = uv.x0 + 0x40;
            uv.y1 = 0x40;
            DcList_SetUv(flash, name, "mc_icon_item_potara", &uv);
            Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 1);
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            Flash_ClipSetTex(flash, &ref, 0);
        } else {
            Flash_FindLabel(flash, name, "mc_icon_item_cost", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, name, "mc_icon_item_potara", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 5);
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            Flash_ClipSetTex(flash, &ref, 3);
        }
        if (i == DCLIST_ITEM_ROWS - 1) {
            Flash_FindLabel(flash, name, "mc_list_plate_off", &ref);
            Flash_ClipSetTex(flash, &ref, 4);
            Flash_FindLabel(flash, name, "mc_list_plate_on", &ref);
            Flash_ClipSetTex(flash, &ref, 3);
        }
    }
}

/* Cuts the two "limit" numbers of the item page from their sheets. */
void DcList_DrawLimitNums(DcList *list) {
    MFlashUv uv;
    MFlash *flash = &list->view.flash[1];

    DcList_SetCell(&uv, 2, 2, 0x40, 0x40);
    DcList_SetUv(flash, NULL, "mc_ability_limit_big_num", &uv);
    DcList_SetCell(&uv, 2, 2, 0x20, 0x20);
    DcList_SetUv(flash, NULL, "mc_ability_limit_s_num", &uv);
}

/* The item id (1-based, 0 = none) on the row under the item page's cursor. */
u16 DcList_GetCursorItem(DcList *list) {
    return list->status.rec.item[list->itemCursor];
}

/* Plays the "chosen" animation of the row under the cursor. */
void DcList_FlashItemRow(DcList *list) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash;

    sprintf(name, "mc_list_plate_%d", list->itemCursor + 1);
    flash = &list->view.flash[1];
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Fills the item page: names, the eight rows, the limit numbers. */
void DcList_DrawItemPage(DcList *list) {
    DcList_DrawNames(&list->view, list->status.rec.chara);
    DcList_DrawItems(list, list->itemCursor);
    DcList_DrawLimitNums(list);
}

/* State 2, the item page: up / down move over the rows that hold an item, confirm opens the details. */
void DcList_InputItems(DcList *list) {
    if (gPad[0].gamePressed & ZPAD_OK) {
        DcList_FlashItemRow(list);
        if (DcList_GetCursorItem(list) != 0) {
            ItemHelp_Open();
            list->state = DCLIST_ST_ITEM_HELP;
            Snd_PlaySe(1, 1);
        } else {
            Snd_PlaySe(1, 7);
        }
    } else if (gPad[0].gamePressed & ZPAD_CANCEL) {
        list->state = DCLIST_ST_MENU;
        Flash_GotoLabel(&list->view.flash[1], "fl_evo_cansel", 1);
        DcList_LightItemRow(&list->view, list->itemCursor, 0);
        list->itemCursor = 0;
        DcList_LightItemRow(&list->view, list->itemCursor, 1);
        Snd_PlaySe(1, 2);
    } else if (gPad[0].gameRepeat & ZPAD_UP) {
        DcList_LightItemRow(&list->view, list->itemCursor, 0);
        do {
            list->itemCursor--;
            DcList_WrapItemCursor(&list->itemCursor);
        } while (DcList_IsRowSkipped(list->status.rec.item, list->itemCursor));
        DcList_LightItemRow(&list->view, list->itemCursor, 1);
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & ZPAD_DOWN) {
        DcList_LightItemRow(&list->view, list->itemCursor, 0);
        do {
            list->itemCursor++;
            DcList_WrapItemCursor(&list->itemCursor);
        } while (DcList_IsRowSkipped(list->status.rec.item, list->itemCursor));
        DcList_LightItemRow(&list->view, list->itemCursor, 1);
        Snd_PlaySe(1, 0);
    }
}

/* Draws the item details page for the item under the cursor. */
void DcList_DrawItemHelp(DcList *list) {
    s32 item = DcList_GetCursorItem(list) - 1;

    if (item < 0) {
        item = 0;
    }
    ItemHelp_Draw(item);
}

/* State 3, the item details page: confirm or cancel closes it. */
void DcList_InputItemHelp(DcList *list) {
    if (DcList_GetCursorItem(list) != 0) {
        if (gPad[0].gamePressed & ZPAD_OK) {
            list->state = DCLIST_ST_ITEMS;
            ItemHelp_Close();
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & ZPAD_CANCEL) {
            list->state = DCLIST_ST_ITEMS;
            ItemHelp_Close();
            Snd_PlaySe(1, 2);
        }
    }
}

/* Draws the window that shows the character's password. */
void DcList_DrawPassword(DcList *list) {
    PassWin_Draw();
}

/* State 4, the password window: confirm or cancel closes it. */
void DcList_InputPassword(DcList *list) {
    if ((gPad[0].gamePressed & ZPAD_OK) || (gPad[0].gamePressed & ZPAD_CANCEL)) {
        PassWin_Close();
        list->state = DCLIST_ST_MENU;
        Snd_PlaySe(1, 2);
    }
}

/* Empties the record under the cursor, in the list's copy and in the save, and marks the save as changed. */
void DcList_DeleteRec(DcList *list) {
    s32 idx = DcChars_GetCursor(&list->chars);

    ZSAVE->rec[idx].chara = list->chars.rec[DcChars_GetCursor(&list->chars)].chara = -1;
    ZPROG->flags |= ZPROG_DIRTY;
}

/* Draws the confirmation dialog. */
void DcList_DrawDialog(DcList *list) {
    Dialog_Draw(0);
}

/* State 5, "delete this character?": yes deletes the record, then the dialog closes into the next state. */
void DcList_InputDialog(DcList *list) {
    s32 result;

    if (Dialog_IsClosed()) {
        list->state = list->nextState;
        if (list->state == DCLIST_ST_DELETED) {
            Flash_GotoLabel(&list->view.flash[0], "fl_chara_details_out", 1);
            DcList_Say(list, 0x1B);
            MsgWin_Open();
        }
    }
    result = Dialog_Input(0);
    if (result == 1) {
        DcList_DeleteRec(list);
        list->nextState = DCLIST_ST_DELETED;
        Dialog_Start(1);
        Dialog_SetChoices(0);
    } else if (result == -2) {
        list->nextState = DCLIST_ST_MENU;
        Dialog_Start(1);
        Dialog_SetChoices(0);
    }
}

/* State 6: the guide's line after a deletion; confirm (or its end) returns to the list. */
void DcList_InputDeleted(DcList *list) {
    if ((gPad[0].gamePressed & ZPAD_OK) || Voice_GetStat(0) == MVOICE_IDLE) {
        list->state = DCLIST_ST_LIST;
        if (gPad[0].gamePressed & ZPAD_OK) {
            Snd_PlaySe(1, 1);
        }
        DcList_Say(list, 0x1A);
    }
}

/* Asks for the large picture of a character (the file is read in the background). */
static inline void DcList_RequestFace(DcList *list, DcStatus *status) {
    File_CancelRequests();
    File_Request(status->rec.chara + DCLIST_FACE_FILE, list->faceFile, DCLIST_FACE_SIZE);
}

/* Loads the large picture of the character under the cursor in the background and fades it in. */
void DcList_UpdateFace(DcList *list) {
    MTexRes *res;
    s32 flags;

    if (list->flags & DCLIST_FACE_CHANGE) {
        list->status.rec = DcChars_GetRec(&list->chars);
        DcList_RequestFace(list, &list->status);
        flags = list->flags;
        list->faceAlpha = 0.0f;
        list->flags = (flags ^ DCLIST_FACE_CHANGE) | DCLIST_FACE_LOADING;
        if (list->flags & DCLIST_FACE_READY) {
            list->flags ^= DCLIST_FACE_READY;
        }
    } else if (list->flags & DCLIST_FACE_LOADING) {
        if (File_UpdateRequests()) {
            res = NULL;
            Sprite_Unpack(list->faceFile, list->faceRes, NULL);
            res = list->faceRes;
            Res_RelocateOffsets(&res, res, res);
            list->view.tex0[7] = res->tex;
            list->flags = (list->flags ^ DCLIST_FACE_LOADING) | DCLIST_FACE_READY;
        }
    }
    if (list->flags & DCLIST_FACE_READY) {
        if (list->faceAlpha < 1.0f) {
            list->faceAlpha += 0.05f;
        }
    }
}

/* Advances the two movies. */
void DcList_Update(DcList *list) {
    s32 i;

    for (i = 0; i < DCLIST_FLASH_NUM; i++) {
        Flash_Advance(&list->view.flash[i]);
    }
}

/*
 * A section of the screen's pack. `host` is the file the section was built from: the development build could
 * read it from the host PC. Nothing uses it here, but the strings are still in the object, in this order
 * (0x3BCDC8..0x3BD30E).
 */
static inline u8 *DcList_Section(DcList *list, s32 n, const char *host) {
    return MPACK_AT(list->res, n);
}

#define DCLIST_HOST "host:data/ps2/test/main/DC/LIST/"

#define DCLIST_RES(n, host) \
    res = (MTexRes *)DcList_Section(list, n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen's section of archive 8, builds the two movies, the text boxes and the shared windows. */
void DcList_Init(DcList *list) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;

    list->pack = MPACK_AT(gMenuArc8, list->section);
    list->res = Sprite_Unpack(list->pack, NULL, NULL);
    list->faceFile = Heap_Alloc(DCLIST_FACE_SIZE, 0x40, 0, 2);
    list->faceRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    DCLIST_RES(1, DCLIST_HOST "chara_list_bg_PS2_.dbt");
    list->view.bg = res;
    File_LoadSync(DcChars_GetCursor(&list->chars) + DCLIST_FACE_FILE, list->faceFile, DCLIST_FACE_SIZE);
    Sprite_Unpack(list->faceFile, list->faceRes, NULL);
    DCLIST_RES(19, DCLIST_HOST "dc_compane_PS2_.dbt");
    list->view.tex0[0] = MTEX(res, 0);
    list->view.tex0[1] = MTEX(res, 1);
    list->view.tex0[2] = MTEX(res, 2);
    list->view.tex0[3] = MTEX(res, 3);
    list->view.tex0[4] = MTEX(res, 4);
    list->view.tex0[5] = MTEX(res, 5);
    list->view.tex0[6] = MTEX(res, 6);
    DCLIST_RES(20, DCLIST_HOST "pwd_imput_ball_PS2_.dbt");
    list->view.tex0[13] = MTEX(res, 0);
    DCLIST_RES(3, DCLIST_HOST "chara_list_top_tex_PS2_.dbt");
    list->view.tex0[8] = MTEX(res, 0);
    list->view.tex0[9] = MTEX(res, 1);
    list->view.tex0[10] = MTEX(res, 2);
    list->view.tex0[11] = MTEX(res, 3);
    list->view.tex0[15] = MTEX(res, 4);
    list->view.tex0[17] = MTEX(res, 5);
    list->view.tex0[18] = MTEX(res, 6);
    list->view.tex0[20] = MTEX(res, 7);
    list->view.tex0[21] = MTEX(res, 8);
    list->view.tex0[24] = MTEX(res, 9);
    list->view.tex0[25] = MTEX(res, 10);
    list->view.tex0[27] = MTEX(res, 11);
    list->view.tex0[28] = MTEX(res, 12);
    list->view.tex0[29] = MTEX(res, 13);
    list->view.tex0[32] = MTEX(res, 14);
    list->view.tex0[33] = MTEX(res, 15);
    list->view.tex0[34] = MTEX(res, 16);
    DCLIST_RES(4, DCLIST_HOST "chara_list_top_text_JP_PS2_.dbt");
    list->view.tex0[12] = MTEX(res, 0);
    list->view.tex0[14] = MTEX(res, 1);
    list->view.tex0[16] = MTEX(res, 2);
    list->view.tex0[19] = MTEX(res, 3);
    list->view.tex0[26] = MTEX(res, 4);
    DCLIST_RES(2, DCLIST_HOST "dc_select_guide_PS2_.dbt");
    list->view.tex0[35] = MTEX(res, 0);
    list->view.tex0[36] = MTEX(res, 1);
    list->view.tex0[37] = MTEX(res, 3);
    list->view.tex0[7] = NULL;
    list->view.tex0[22] = NULL;
    list->view.tex0[23] = NULL;
    list->view.tex0[30] = NULL;
    list->view.tex0[31] = NULL;
    Flash_Create(&list->view.flash[0], DcList_Section(list, 5, DCLIST_HOST "chara_list_top_PS2_.fod"), list->view.tex0);
    Flash_Play(&list->view.flash[0], 1);
    DCLIST_RES(7, DCLIST_HOST "chara_list_z_item_tex_PS2_.dbt");
    list->view.tex1[0] = MTEX(res, 0);
    list->view.tex1[1] = MTEX(res, 1);
    list->view.tex1[2] = MTEX(res, 2);
    list->view.tex1[4] = MTEX(res, 3);
    list->view.tex1[5] = MTEX(res, 4);
    list->view.tex1[6] = MTEX(res, 5);
    list->view.tex1[10] = MTEX(res, 6);
    list->view.tex1[11] = MTEX(res, 7);
    list->view.tex1[12] = MTEX(res, 8);
    list->view.tex1[13] = MTEX(res, 9);
    list->view.tex1[14] = MTEX(res, 10);
    DCLIST_RES(8, DCLIST_HOST "chara_list_z_item_text_JP_PS2_.dbt");
    list->view.tex1[3] = MTEX(res, 0);
    DCLIST_RES(6, DCLIST_HOST "chara_list_z_item_tex_plate_PS2_.dbt");
    list->view.tex1[7] = MTEX(res, 0);
    list->view.tex1[8] = MTEX(res, 0);
    list->view.tex1[9] = NULL;
    list->view.tex1[15] = NULL;
    list->view.tex1[16] = NULL;
    Flash_Create(&list->view.flash[1], DcList_Section(list, 9, DCLIST_HOST "chara_list_z_item_PS2_.fod"), list->view.tex1);
    Flash_Play(&list->view.flash[1], 1);
    list->nameText = DcList_Section(list, 17, DCLIST_HOST "chara_name_JP_PS2_.pak");
    list->formText = DcList_Section(list, 18, DCLIST_HOST "chara_form_JP_PS2_.pak");
    for (i = 0; i < 4; i++) {
        TextBox_Init(&list->view.nameBox[i], list->nameText, 2);
        TextBox_SetRect(&list->view.nameBox[i], 0xAF, 0x1FF, 0x6A, 0x139);
        TextBox_SetUnk80(&list->view.nameBox[i], 1);
        TextBox_Init(&list->view.formBox[i], list->formText, 4);
        TextBox_SetRect(&list->view.formBox[i], 0xAF, 0x1FF, 0x6A, 0x139);
        TextBox_SetUnk80(&list->view.formBox[i], 1);
    }
    TextBox_Init(&list->view.nameBoxB, list->nameText, 1);
    TextBox_SetUnk80(&list->view.nameBoxB, 1);
    TextBox_Init(&list->view.formBoxB, list->formText, 3);
    TextBox_SetUnk80(&list->view.formBoxB, 1);
    TextBox_Init(&list->view.nameBoxL, list->nameText, 1);
    TextBox_SetUnk80(&list->view.nameBoxL, 1);
    TextBox_Init(&list->view.formBoxL, list->formText, 3);
    TextBox_SetUnk80(&list->view.formBoxL, 1);
    list->itemText = DcList_Section(list, 21, DCLIST_HOST "font_zitem_name_JP_PS2_.pak");
    for (j = 0; j < DCLIST_ITEM_ROWS; j++) {
        TextBox_Init(&list->view.itemBox[j], list->itemText, 2);
        TextBox_SetUnk80(&list->view.itemBox[j], 1);
    }
    list->msgText = DcList_Section(list, 13, DCLIST_HOST "dc_msg_JP_PS2_.pak");
    MsgWin_Init(DcList_Section(list, 11, DCLIST_HOST "if_msg_window_PS2_.pak"), list->msgText, 0, (s32)list->unkC44);
    MsgWin_Open();
    list->voiceLine = -1;
    list->dialogMsg = DcList_Section(list, 22, DCLIST_HOST "font_datacenter_JP_PS2_.pak");
    Dialog_Init(DcList_Section(list, 12, DCLIST_HOST "if_system_window_JP_PS2_.pak"), list->dialogMsg, 0);
    PassWin_Init(DcList_Section(list, 10, DCLIST_HOST "dc_password_window_JP_PS2_.pak"));
    ItemHelp_Init(DcList_Section(list, 16, DCLIST_HOST "ez_item_details_JP_PS2_.pak"));
    list->itemTbl = (ZItemEntry *)DcList_Section(list, 15, DCLIST_HOST "zitem_parameter_PS2_.dat");
    list->subtitles = DcList_Section(list, 14, DCLIST_HOST "dcenter_lips_PS2_.pak");
}

/*
 * Draws the screen: the list movie, the message window, the item page, then the windows on top.
 *
 * (As a file of its own the tail of this object left this function INCLUDE_ASM, 2 of 99 instructions apart: in
 * the call DcView_LightRow(view, &list->chars, 1) the original loads a2 = 1 before a1 = list + 0xCB0. It matches
 * since the two halves are one file: the compiler has to have seen the definition of DcView_LightRow.)
 */
void DcList_Draw(DcList *list) {
    MFlashRef ref;
    DcView *view;

    if (!(list->flags & DCLIST_STARTED)) {
        Flash_GotoLabel(&list->view.flash[0], "fl_chara_list_in", 1);
        list->flags |= DCLIST_STARTED;
    }
    view = &list->view;
    if (!(list->flags & DCLIST_GREETED)) {
        if (list->view.flash[0].flags & MFLASH_PAD) {
            DcView_LightRow(view, &list->chars, 1);
            DcView_LightMenu(view, list->menuCursor, 1);
            DcList_LightItemRow(view, list->itemCursor, 1);
            DcList_Say(list, 0x1A);
            list->flags |= DCLIST_GREETED;
        }
    }
    DcList_DrawList(list);
    Flash_FindLabel(&view->flash[0], NULL, "mc_single_chara_r", &ref);
    Flash_ClipSetAlpha(&view->flash[0], &ref, list->faceAlpha);
    DcView_HideNamePlates(view);
    DcView_SetMenuText(view);
    DcView_SetStatus(view, &list->status);
    DcView_SetNameText(view, list->status.rec.chara);
    DcList_DrawItemPage(list);
    DcList_UpdateGuide(list);
    DcList_ScrollCloud(list);
#ifdef PORT
    Port_MenuPage(45); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(list->view.bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(list->view.bg, 0, 0, 0x80);
#endif
    Flash_Draw(&view->flash[0]);
    MsgWin_Draw(0, 0, list->voiceLine);
    Flash_Draw(&list->view.flash[1]);
    DcList_DrawDialog(list);
    DcList_DrawPassword(list);
    DcList_DrawItemHelp(list);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Destroys the movies, frees the buffers and closes the shared windows. */
void DcList_Term(DcList *list) {
    s32 i;

    for (i = 0; i < DCLIST_FLASH_NUM; i++) {
        Flash_Destroy(&list->view.flash[i]);
    }
    if (list->res != NULL) {
        Heap_Free(list->res);
        list->res = NULL;
    }
    if (list->faceRes != NULL) {
        Heap_Free(list->faceRes);
        list->faceRes = NULL;
    }
    if (list->faceFile != NULL) {
        Heap_Free(list->faceFile);
        list->faceFile = NULL;
    }
    PassWin_Term();
    Dialog_Term();
    MsgWin_Term();
    ItemHelp_Term();
}

/* Reads the pad for the current state once the list movie accepts input. */
void DcList_Input(DcList *list) {
    if (!(list->view.flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (list->flags & DCLIST_LEAVE) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    switch (list->state) {
    case DCLIST_ST_LIST:
        DcList_InputList(list);
        break;
    case DCLIST_ST_MENU:
        DcList_InputMenu(list);
        break;
    case DCLIST_ST_ITEMS:
        DcList_InputItems(list);
        break;
    case DCLIST_ST_ITEM_HELP:
        DcList_InputItemHelp(list);
        break;
    case DCLIST_ST_PASSWORD:
        DcList_InputPassword(list);
        break;
    case DCLIST_ST_DIALOG:
        DcList_InputDialog(list);
        break;
    case DCLIST_ST_DELETED:
        DcList_InputDeleted(list);
        break;
    }
}

/* Once the screen is to be left: starts the fade out and returns 1 when it is over. */
s32 DcList_CheckLeave(DcList *list) {
    if (list->flags & DCLIST_LEAVE) {
        if (!(list->flags & DCLIST_LEAVING)) {
            ColorFade_StartOut(0, 0, 0, 0x14);
            list->flags |= DCLIST_LEAVING;
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
        } else {
            return 1;
        }
    }
    return 0;
}

/* Runs the saved-character list (progress mode 55) with its own frame loop; the work is on the stack. */
s32 DcList_Run(s32 section) {
    DcList list;

    memset(&list, 0, sizeof(DcList));
    DcList_Reset(&list);
    list.section = section;
    DcList_Init(&list);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        DcList_Update(&list);
        DcList_Draw(&list);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (DcList_CheckLeave(&list)) {
            break;
        }
        DcList_Input(&list);
        DcList_UpdateFace(&list);
    }
    DcList_Term(&list);
    Dma_ResetBuffers();
    return list.result;
}
