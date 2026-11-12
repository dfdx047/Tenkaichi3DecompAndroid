#include "common.h"
#include "menu/menu_z.h"
#include "sys/misc_a.h"
#ifdef PORT
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/*
 * Menu overlay DBZP.BIN, 0x3AAF30..0x3AE648: the DcPass object, the password entry screen of the Data Center
 * (progress mode 54): an on-screen keyboard of two pages, 34 cells of text, the status page of the character a
 * password decodes to and the list of slots to store it in. (Written as two halves, menu_z_d.c
 * 0x3AAF30..0x3AC440 = the keyboard, the password text, the decoding and the storing of the new character, and
 * menu_za.c 0x3AC440..0x3AE648 = the slot list, init / draw / input / run; merged here.) The work structure is a
 * local variable of DcPass_Run, so there is no work pointer; the object's `.data` is the key table at 0x3BC928
 * (0x8C bytes). Its read-only data is 0x3BD700 ("mc_input_code_%d_%02d") .. 0x3BE16C (the two jump tables of
 * DcPass_Input); with the `#if 0` attempts compiled in, the strings come out exactly as in the original.
 *
 * Two functions are INCLUDE_ASM. What they refer to in the object's data:
 *   DcPass_WrapPos     nothing
 *   DcPass_DrawStatus  its own seven strings, 0x3BD850..0x3BD914 ("mc_status_ability_plus_%d" 0x3BD850,
 *                      "mc_status_ability_base2_1" 0x3BD870, "mc_text_ability_&d" 0x3BD890,
 *                      "mc_status_ability_base1_%d" 0x3BD8A8, "mc_status_ability_minus_%d" 0x3BD8C8,
 *                      "mc_text_ability_1" 0x3BD8E8, "mc_status_attribute" 0x3BD900); no C function shares them
 */

/*
 * The on-screen keyboard: [page][row][col]; codes 0..6 are DCKEY_. This object's only `.data` (0x3BC928, 0x8C
 * bytes; its work structure is a local of DcPass_Run, so there is no work pointer).
 */
char gDcPassKeys[DCPASS_PAGES][DCPASS_ROWS][DCPASS_COLS] = {
    {
        { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N' },
        { 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', DCKEY_WIDE_D1, DCKEY_WIDE_D2 },
        { '#', '$', '%', '&', '@', '!', '?', '-', '+', '*', '(', ')', DCKEY_WIDE_D1, DCKEY_WIDE_D2 },
        { DCKEY_ZERO, DCKEY_ZERO, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', DCKEY_WIDE_E, DCKEY_WIDE_E },
        { DCKEY_BUTTON_L, DCKEY_BUTTON_L, DCKEY_BUTTON_L, DCKEY_BUTTON_L, DCKEY_BUTTON_M, DCKEY_BUTTON_M, DCKEY_BUTTON_M,
          DCKEY_BUTTON_M, DCKEY_BUTTON_M, DCKEY_BUTTON_M, DCKEY_BUTTON_R, DCKEY_BUTTON_R, DCKEY_BUTTON_R, DCKEY_BUTTON_R },
    },
    {
        { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n' },
        { 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', DCKEY_WIDE_D1, DCKEY_WIDE_D2 },
        { '#', '$', '%', '&', '@', '!', '?', '-', '+', '*', '(', ')', DCKEY_WIDE_D1, DCKEY_WIDE_D2 },
        { DCKEY_ZERO, DCKEY_ZERO, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', DCKEY_WIDE_E, DCKEY_WIDE_E },
        { DCKEY_BUTTON_L, DCKEY_BUTTON_L, DCKEY_BUTTON_L, DCKEY_BUTTON_L, DCKEY_BUTTON_M, DCKEY_BUTTON_M, DCKEY_BUTTON_M,
          DCKEY_BUTTON_M, DCKEY_BUTTON_M, DCKEY_BUTTON_M, DCKEY_BUTTON_R, DCKEY_BUTTON_R, DCKEY_BUTTON_R, DCKEY_BUTTON_R },
    },
};

/* Keeps a value in lo..hi as a ring. */
static inline s32 DcPass_Wrap(s32 value, s32 lo, s32 hi) {
    if (value < lo) {
        return hi;
    }
    if (value > hi) {
        return lo;
    }
    return value;
}

/* Sets the texture rectangle of a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcPass_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Texture rectangle of one cell of a sheet. */
static inline void DcPass_SetCell(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->y0 = row * h;
    uv->x1 = uv->x0 + w;
    uv->y1 = uv->y0 + h;
}

/* Keeps a three-way cursor in 0..2. */
s32 DcPass_Wrap3(s32 value) {
    return DcPass_Wrap(value, 0, 2);
}

/* The character (or DCKEY_ code) of a keyboard cell; the wide "0" key gives '0'. */
char DcPass_GetKey(DcKeyPos *pos) {
    char key = gDcPassKeys[pos->page][pos->row][pos->col];

    if (key == DCKEY_ZERO) {
        key = '0';
    }
    return key;
}

/* Keeps a keyboard position inside the keyboard (every axis is a ring). */
/*
 * NOT MATCHING: the three wraps compile to the same conditional-move sequence here; the original has that
 * sequence for the row only and another (the one DcPass_Wrap3 has as a function of its own) for page and
 * column. Same results for every input.
 *
 * What the difference is (cleanup, build/scratch_cleanup2_I/t/): one source form gives BOTH original sequences;
 * which one comes out depends on whether the first scheduling pass moved `slt` above the `bltz` (speculative
 * motion between blocks). Here it does (`li v0,hi / li a1,hi / bltz / slt` for all three); in the original it did
 * not, so the constant's register is free for the result, the result shares v0 with the compare, and reload
 * then picks the other conditional-move alternative (`movn v1,zero,v0 / move v0,v1`) wherever the result is in
 * v0 (page, column) and keeps `move a1,zero / movz a1,v1,v0` where v0 is still busy with the previous result
 * (row). The scheduler only moves instructions between blocks while the function is one "region" of at most 10
 * basic blocks: five wraps in one function (11 blocks) reproduce the original's page sequence exactly, three
 * (7 blocks) do not. So the original function had more than 10 basic blocks when it was scheduled, i.e. source
 * that is not visible in the code any more; what it was is not found (nested inlines, scopes, `do { } while
 * (0)`, a third and fourth dead wrap were tried). The order `lw / li / sw` of the row (the store of the page
 * after the row's constant) is not reproduced either.
 */
#if 0
void DcPass_WrapPos(DcKeyPos *pos) {
    pos->page = DcPass_Wrap(pos->page, 0, DCPASS_PAGES - 1);
    pos->row = DcPass_Wrap(pos->row, 0, DCPASS_ROWS - 1);
    pos->col = DcPass_Wrap(pos->col, 0, DCPASS_COLS - 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z_d", DcPass_WrapPos);
#endif

/* Puts a keyboard position on the middle (1) or right (2) button of the bottom row. */
void DcPass_GotoButton(s32 button, DcKeyPos *pos) {
    switch (button) {
    case 2:
        pos->row = DCPASS_ROWS - 1;
        pos->col = 12;
        break;
    case 1:
        pos->row = DCPASS_ROWS - 1;
        pos->col = 6;
        break;
    }
}

/* Whether the cell in another column of the same row belongs to the same (wide) key. */
s32 DcPass_IsSameKeyCol(DcKeyPos *pos, s32 col) {
    DcKeyPos other = *pos;

    other.col = col;
    return DcPass_GetKey(pos) == DcPass_GetKey(&other);
}

/* Whether the cell in another row of the same column belongs to the same key. */
s32 DcPass_IsSameKeyRow(DcKeyPos *pos, s32 row) {
    DcKeyPos other = *pos;

    other.row = row;
    return DcPass_GetKey(pos) == DcPass_GetKey(&other);
}

/* Turns a keyboard position into the row / column numbers of its clip ("mc_input_code_<row>_<col>"). */
void DcPass_GetClipPos(DcKeyPos *pos, DcKeyPos *out) {
    *out = *pos;
    if (pos->row == 1) {
        if (pos->col >= 12) {
            out->row++;
        }
    } else if (pos->row == 3) {
        out->col -= 2;
        if (out->col < 0) {
            out->col = 0;
        }
        if (pos->col >= 12) {
            out->col = 10;
        }
    } else if (pos->row == 4) {
        out->col = DcPass_GetKey(pos);
    }
}

/* The keyboard cell that types a character (digits are on row 3 of the first page). */
DcKeyPos DcPass_FindKey(char c) {
    DcKeyPos pos;
    char digit[2];

    if (c >= '0' && c <= '9') {
        digit[0] = c;
        digit[1] = 0;
        pos.page = 0;
        pos.row = 3;
        pos.col = atoi(digit);
        return pos;
    }
    for (pos.page = 0; pos.page < DCPASS_PAGES; pos.page++) {
        for (pos.row = 0; pos.row < DCPASS_ROWS; pos.row++) {
            for (pos.col = 0; pos.col < DCPASS_COLS; pos.col++) {
                if (c == DcPass_GetKey(&pos)) {
                    return pos;
                }
            }
        }
    }
    return pos;
}

/*
 * Cuts the caps of the four rows of keys from the sheet. The kind of key (small letter, wide key) is taken from
 * the key at *cur, not from the cell being drawn.
 */
void DcPass_DrawKeys(MFlash *flash, DcKeyPos *cur) {
    DcKeyPos out;
    DcKeyPos pos;
    MFlashUv uv;
    char name[0x100];
    char key;
    s32 shift;

    pos.page = cur->page;
    for (pos.row = 0; pos.row < DCPASS_ROWS - 1; pos.row++) {
        for (pos.col = 0; pos.col < DCPASS_COLS; pos.col++) {
            key = DcPass_GetKey(cur);
            DcPass_GetClipPos(&pos, &out);
            shift = (key >= 'a' && key <= 'z') ? 4 : 0;
            DcPass_SetCell(&uv, out.col, out.row + shift, 0x20, 0x28);
            sprintf(name, "mc_input_code_%d_%02d", out.row + 1, out.col + 1);
            /* no braces: each use keeps its own MFlashRef */
            if (key == DCKEY_WIDE_D1 || key == DCKEY_WIDE_D2)
                DcPass_SetUv(flash, name, "mc_text_input_code_d_on", &uv);
            else if (key == DCKEY_WIDE_E)
                DcPass_SetUv(flash, name, "mc_text_input_code_e_on", &uv);
            else
                DcPass_SetUv(flash, name, "mc_text_input_code_a_on", &uv);
        }
    }
}

/* Cuts the captions of the three bottom buttons and the keyboard's lettering for the page shown. */
void DcPass_DrawButtons(MFlash *flash, s32 page) {
    char name[0x100];
    MFlashUv uv;

    DcPass_SetCell(&uv, 0, 1, 0x100, 0x40);
    sprintf(name, "mc_input_code_5_%02d", 1);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_on", &uv);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_off", &uv);
    if (page == 0) {
        DcPass_SetCell(&uv, 0, 3, 0x100, 0x40);
    } else {
        uv.x0 = 0;
        uv.x1 = 0x100;
        uv.y0 = 0x80;
        uv.y1 = 0xC0;
    }
    sprintf(name, "mc_input_code_5_%02d", 2);
    DcPass_SetUv(flash, name, "mc_text_input_code_c_on", &uv);
    DcPass_SetUv(flash, name, "mc_text_input_code_c_off", &uv);
    DcPass_SetCell(&uv, 0, 0, 0x100, 0x40);
    sprintf(name, "mc_input_code_5_%02d", 3);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_on", &uv);
    DcPass_SetUv(flash, name, "mc_text_input_code_b_off", &uv);
    uv.x0 = 0;
    uv.x1 = 0x200;
    uv.y0 = page << 7;
    uv.y1 = (page << 7) + 0x80;
    DcPass_SetUv(flash, NULL, "mc_text_keyboard", &uv);
}

/* Wraps a keyboard position and lights or dims its key. */
void DcPass_LightKey(MFlash *flash, DcKeyPos *pos, s32 on) {
    MFlashRef ref;
    DcKeyPos out;
    char name[0x100];

    DcPass_WrapPos(pos);
    DcPass_GetClipPos(pos, &out);
    sprintf(name, "mc_input_code_%d_%02d", out.row + 1, out.col + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Plays the "pressed" animation of a key. */
void DcPass_FlashKey(MFlash *flash, DcKeyPos *pos) {
    MFlashRef ref;
    DcKeyPos out;
    char name[0x100];

    DcPass_GetClipPos(pos, &out);
    sprintf(name, "mc_input_code_%d_%02d", out.row + 1, out.col + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Draws the keyboard of the page shown. */
void DcPass_DrawKeyboard(DcPass *pass) {
    MFlash *flash = &pass->view.flash[0];
    DcKeyPos *pos = &pass->keyPos;

    DcPass_DrawKeys(flash, pos);
    DcPass_DrawButtons(flash, pos->page);
}

/* Empties the text: 34 spaces. */
void DcPassText_Clear(DcPassText *text) {
    s32 i;

    for (i = 0; i < DCPASS_TEXT_LEN; i++) {
        text->text[i] = ' ';
    }
    text->text[DCPASS_TEXT_LEN] = 0;
}

/* Types the character of a key at the text cursor and moves the cursor on (it stops on the last cell). */
void DcPassText_Put(DcKeyPos *pos, DcPassText *text) {
    text->text[text->cursor] = DcPass_GetKey(pos);
    if (text->cursor < DCPASS_TEXT_LEN - 1) {
        text->cursor++;
    }
}

/* Backspace: clears the cell under the cursor, or steps back when it is empty. Returns 1 if nothing was left. */
s32 DcPassText_Delete(DcPassText *text) {
    if (text->cursor == 0) {
        if (text->text[0] == ' ') {
            return 1;
        }
    } else if (text->text[text->cursor] == ' ') {
        text->cursor--;
        text->text[text->cursor] = ' ';
        goto done;
    }
    text->text[text->cursor--] = ' ';
done:
    if (text->cursor < 0) {
        text->cursor = 0;
    }
    return 0;
}

/* Whether all 34 cells are filled. */
s32 DcPassText_IsFull(DcPassText *text) {
    s32 i;

    for (i = 0; i < DCPASS_TEXT_LEN; i++) {
        if (text->text[i] == ' ') {
            return 0;
        }
    }
    return 1;
}

/* Shows the text: every cell gets the cap of the key that types its character. */
void DcPass_DrawText(MFlash *flash, char *text) {
    MFlashUv uv;
    char name[0x100];
    DcKeyPos pos;
    s32 i;

    for (i = 0; i < DCPASS_TEXT_LEN + 1; i++) {
        char c = text[i];

        pos = DcPass_FindKey(c);
        if (c >= 'a' && c <= 'z') {
            DcPass_SetCell(&uv, pos.col, pos.row + 4, 0x20, 0x28);
        } else {
            DcPass_SetCell(&uv, pos.col, pos.row, 0x20, 0x28);
        }
        if (i < 17) {
            sprintf(name, "mc_input_pass_%d_%02d", 1, i + 1);
        } else {
            sprintf(name, "mc_input_pass_%d_%02d", 2, i - 16);
        }
        DcPass_SetUv(flash, NULL, name, &uv);
    }
}

/* Puts the cursor clip under the cell the next character goes to. */
void DcPass_DrawCursor(MFlash *flash, DcPassText *text) {
    char name[0x100];
    MFlashRef ref;
    s32 x;
    s32 y;
    s32 cx;
    s32 cy;

    if (text->cursor < 17) {
        sprintf(name, "mc_input_pass_%d_%02d", 1, text->cursor + 1);
    } else {
        sprintf(name, "mc_input_pass_%d_%02d", 2, text->cursor - 16);
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGetPos(flash, &ref, &x, &y);
    Flash_FindLabel(flash, NULL, "mc_input_cursor", &ref);
    Flash_ClipSetOffset(flash, &ref, 0, 0);
    Flash_ClipGetPos(flash, &ref, &cx, &cy);
    x -= cx;
    y -= cy;
    Flash_FindLabel(flash, NULL, "mc_input_cursor", &ref);
    Flash_ClipSetOffset(flash, &ref, x, y + 10);
}

/*
 * Copies the text up to its first empty cell into pass and measures it. Returns 1 (not a password) when an
 * empty cell comes before the 33rd, or when only one of the last two cells is filled.
 */
s32 DcPassText_Pack(DcPassText *text) {
    s32 i;

    for (i = 0; text->text[i] != 0; i++) {
        text->pass[i] = text->text[i];
        if (text->text[i] == ' ' && i < 0x20) {
            return 1;
        }
    }
    text->pass[i] = 0;
    if (text->pass[0x20] == ' ') {
        if (text->pass[0x21] == ' ') {
            text->pass[0x20] = 0;
        } else {
            return 1;
        }
    }
    text->len = strlen(text->pass);
    return 0;
}

/* Reads the character of every saved custom character from the save. */
void DcPassList_Refresh(DcPassList *list) {
    s32 i;

    for (i = 0; i < SAVE_REC_COUNT; i++) {
        list->chara[i] = ZSAVE->rec[i].chara;
    }
}

/* Stores the decoded character in the save slot under the list's cursor and marks the save as changed. */
void DcPass_StoreRec(ZStatus *status, DcPassList *list) {
    s32 idx = list->top + list->cursor;

    list->chara[idx] = status->chara;
    ZSAVE->rec[idx] = status->rec;
    ZPROG->flags |= ZPROG_DIRTY;
}

/* Whether a character is in the grid of unlocked characters. */
static inline s32 DcPass_IsListed(DcPass *pass, s32 chara) {
    s32 i;

    for (i = 0; i < pass->gridCount; i++) {
        if (pass->grid[i].id == chara) {
            return 1;
        }
    }
    return 0;
}

/*
 * Decodes the typed text: 32 characters are the previous game's format (converted), 34 this game's. On success
 * (the character must be unlocked) fills the status page's record and returns 1.
 */
/*
 * FAKE MATCH (permuter): the `do { } while (0)` around the decoding and the grid test. It stands for something
 * that made that part a block of its own in the original (a statement macro or a loop that lost its condition);
 * no other form was found: the wrapper has to hold exactly those two parts (starting at the Pack test or the
 * memset, ending behind the stores, or around the decoding alone, all fail), `for (;;) { ...; break; }` is
 * equivalent, and an inline helper for the decoding, a switch on the length, and `break` with a result flag do
 * not match.
 * What it changes: the grid search (an inline loop) is then nested in a loop, so the loop pass leaves the block
 * of a failed DcPassText_Pack (`v0 = 0; goto end`) at the test, where the original has it
 * (`beql v0,zero,+ / lw / b end / move v0,zero`), and the other failures branch to it; without the wrapper the
 * second jump pass merges that block into the `else return 0` of the 34-character branch.
 * Also needed: the 34-character branch is `if (decode == 1) { if (!valid) return 0; } else return 0;`, and the
 * record's character is stored before the status page's.
 */
s32 DcPass_Decode(DcPass *pass) {
    ChrPassData data;
    OldPassChar old;
    ZChrEntry *chrTbl;
    u16 attr;
    s32 i;

    memset(&data, 0, sizeof(ChrPassData));
    if (DcPassText_Pack(&pass->text) != 0) {
        return 0;
    }
    do {
        if (pass->text.len == 32) {
            memset(&old, 0, sizeof(OldPassChar));
            if (OldPass_DecodeChar(&old, pass->text.pass) != 1) {
                return 0;
            }
            if (!PassChk_IsOldValid(&old)) {
                return 0;
            }
            PassChk_ConvertOld(&data, &old);
        } else if (pass->text.len == 34) {
            if (ChrPass_Decode(&data, pass->text.pass) == 1) {
                if (!PassChk_IsValid(&data)) {
                    return 0;
                }
            } else {
                return 0;
            }
        } else {
            return 0;
        }
        if (!DcPass_IsListed(pass, data.charId)) {
            return 0;
        }
    } while (0);
    pass->status.rec.chara = data.charId;
    pass->status.chara = data.charId;
    for (i = 0; i < 8; i++) {
        pass->status.rec.item[i] = data.item[i];
    }
    pass->status.rec.level = data.extraSlots;
    ItemSet_GetBonus(pass->status.rec.item, pass->itemTbl, &pass->status.val[0]);
    chrTbl = (ZChrEntry *)MPACK_AT(gCommonRes->data[2], 1);
    attr = chrTbl[pass->status.chara].flags;
    pass->flags |= DCPASS_FACE_CHANGE;
    pass->status.attr = (attr ^ 1) & 1;
    DcPassList_Refresh(&pass->list);
    return 1;
}

#define DCPASS_FLAG_OFF(f, b) \
    if ((f) & (b)) { \
        (f) ^= (b); \
    }

/* Loads the decoded character's large picture in the background and fades it in. */
void DcPass_UpdateFace(DcPass *pass) {
    MTexRes *res;

    if (pass->flags & DCPASS_FACE_CHANGE) {
        File_CancelRequests();
        File_Request(pass->status.chara + DCPASS_FACE_FILE, pass->faceFile, DCPASS_FACE_SIZE);
        DCPASS_FLAG_OFF(pass->flags, DCPASS_FACE_CHANGE);
        pass->flags |= DCPASS_FACE_LOADING;
        pass->faceAlpha = 0.0f;
        DCPASS_FLAG_OFF(pass->flags, DCPASS_FACE_READY);
    }
    if (pass->flags & DCPASS_FACE_LOADING) {
        if (File_UpdateRequests()) {
            res = NULL;
            Sprite_Unpack(pass->faceFile, pass->faceRes, NULL);
            res = pass->faceRes;
            Res_RelocateOffsets(&res, res, res);
            pass->view.tex1[7] = res->tex;
            DCPASS_FLAG_OFF(pass->flags, DCPASS_FACE_LOADING);
            pass->flags |= DCPASS_FACE_READY;
        }
    }
    if (pass->flags & DCPASS_FACE_READY) {
        if (pass->faceAlpha < 1.0f) {
            pass->faceAlpha += 0.05f;
        }
    }
}

/* Draws the typed text and its cursor. */
void DcPass_DrawTextLine(DcPass *pass) {
    MFlash *flash = &pass->view.flash[0];
    DcPassText *text = &pass->text;

    DcPass_DrawText(flash, text->text);
    DcPass_DrawCursor(flash, text);
}

/* Shows or hides a clip found by name (each use has its own MFlashRef on the stack). */
static inline void DcPass_SetVisible(MFlash *flash, char *parent, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetFlags(flash, &ref, 2, on);
}

/* Sets the picture of a clip found by name. */
static inline void DcPass_SetTex(MFlash *flash, char *parent, char *name, s32 tex) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetTex(flash, &ref, tex);
}

/*
 * Fills the status page of a character: hides the item-slot marks beyond the slots it has, lights the marks of
 * the four bars (minus marks for a negative value, plus marks for a positive one) and sets the attribute icon.
 */
/* Texture rectangle of row i of the 0x80 x 0x14 caption sheet. */
#define DCPASS_ROW(uv, i) DcPass_SetCell(uv, 0, i, 0x80, 0x14)

/*
 * NOT MATCHING: 27 of 233 instructions: the attempt keeps the constant 0x80 of the row rectangle in a saved
 * register from the end of the loop to the rectangle after it, the original loads it again each time (and so
 * has one saved register free: registers shift in that part). Calls, arguments and stores are the same.
 *
 * Cleanup notes: the sharing is done by the second CSE pass, which (unlike the first) follows the fall-through
 * out of the loop, so the rectangle after the loop reuses the loop's register for 0x80; the original's two
 * loads mean that pass did not see the two as one path. The same shows in the movie pointer: the original
 * uses the function-level `view + 0x2C` (s6) in the loop's last two calls, the attempt the loop's hoisted copy
 * (s2). An inline helper for the caption (own `uv`) is worse (50..87 differences).
 */
#if 0
void DcPass_DrawStatus(DcPassView *view, ZStatus *status) {
    char name[0x100];
    char sub[0x100];
    MFlashUv uv;
    s32 i;
    s32 j;

    for (i = status->val[0]; i < 7; i++) {
        sprintf(name, "mc_status_ability_plus_%d", i + 4);
        DcPass_SetVisible(&view->flash[1], "mc_status_ability_base2_1", name, 0);
    }
    DCPASS_ROW(&uv, i);
    DcPass_SetUv(&view->flash[1], "mc_status_ability_base2_1", "mc_text_ability_&d", &uv);
    for (i = 0; i < 4; i++) {
        s32 value = status->val[i + 1];

        sprintf(name, "mc_status_ability_base1_%d", i + 1);
        for (j = 0; j < 4; j++) {
            sprintf(sub, "mc_status_ability_minus_%d", j + 1);
            DcPass_SetVisible(&view->flash[1], name, sub, 0);
            sprintf(sub, "mc_status_ability_plus_%d", j + 1);
            DcPass_SetVisible(&view->flash[1], name, sub, 0);
            if (value < 0 && value < -j && -j <= 0) {
                sprintf(sub, "mc_status_ability_minus_%d", j + 1);
                DcPass_SetVisible(&view->flash[1], name, sub, 1);
            } else if (value > 0 && j >= 0 && j < value) {
                sprintf(sub, "mc_status_ability_plus_%d", j + 1);
                DcPass_SetVisible(&view->flash[1], name, sub, 1);
            }
        }
        DCPASS_ROW(&uv, i);
        sprintf(name, "mc_status_ability_base1_%d", i);
        DcPass_SetUv(&view->flash[1], name, "mc_text_ability_1", &uv);
    }
    {
        MFlashRef ref;

        Flash_FindLabel(&view->flash[1], NULL, "mc_status_attribute", &ref);
        Flash_ClipSetTex(&view->flash[1], &ref, status->attr);
    }
    DCPASS_ROW(&uv, i);
    sprintf(name, "mc_status_ability_base1_%d", i);
    DcPass_SetUv(&view->flash[1], name, "mc_text_ability_1", &uv);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z_d", DcPass_DrawStatus);
#endif

/*
 * Sets the transparency of the character's large picture. Inline in the original and defined here, behind
 * DcPass_DrawStatus: its string "mc_single_chara_r" (0x3BD918) lies between that function's last string and the
 * first string of DcPass_SetListClip, although only DcPass_Draw (far below) uses it.
 */
static inline void DcPass_SetFaceAlpha(DcPass *pass) {
    MFlashRef ref;
    MFlash *flash = &pass->view.flash[1];

    Flash_FindLabel(flash, NULL, "mc_single_chara_r", &ref);
    Flash_ClipSetAlpha(flash, &ref, pass->faceAlpha);
}

/* ---- second half of the object (was src/menu/menu_za.c, 0x3AC440..0x3AE648) ---- */

void DcPass_ClipBegin(void);
void DcPass_ClipEnd(void);

/* Starts a line of the guide and shows its subtitle. */
static inline void DcPass_Say(DcPass *pass, s32 line) {
    Voice_PlayWithSubtitle(pass->view.subtitles, DC_VOICE_BASE, line);
    pass->voiceLine = line;
}

/*
 * Texture rectangle of one cell of a sheet, as DcPass_SetCell but with the stores in another order: the two
 * halves of this file were written separately and DcPass_DrawArrows only matches with this order (x0, x1, y0,
 * y1: with DcPass_SetCell two stores of it swap), the head's functions only with the other.
 */
static inline void DcPass_SetCellB(MFlashUv *uv, s32 col, s32 row, s32 w, s32 h) {
    uv->x0 = col * w;
    uv->x1 = uv->x0 + w;
    uv->y0 = row * h;
    uv->y1 = uv->y0 + h;
}

/* Makes the four plates of the slot list draw inside the list's window. */
void DcPass_SetListClip(DcPassView *view) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &view->flash[1];
    s32 i;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetCallbackA(flash, &ref, DcPass_ClipBegin, NULL);
        Flash_ClipSetCallbackB(flash, &ref, DcPass_ClipEnd, NULL);
    }
}

/* Hides the up or the down arrow of the slot list. */
void DcPass_HideArrow(DcPassView *view, s32 up) {
    MFlashRef ref;
    char parent[0x100];
    char name[0x100];
    MFlash *flash = &view->flash[1];

    if (up) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);
}

/* Sets the pictures of the two arrows and hides the one that leads nowhere. */
void DcPass_DrawArrows(DcPassView *view, DcPassList *list) {
    MFlashUv uv;
    MFlash *flash = &view->flash[1];
    s32 top;

    DcPass_SetCellB(&uv, 0, 1, 0x20, 0x20);
    DcPass_SetUv(flash, "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    DcPass_SetCellB(&uv, 1, 1, 0x20, 0x20);
    DcPass_SetUv(flash, "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    top = list->top;
    if (top == 0) {
        DcPass_HideArrow(view, 1);
    } else if (top == SAVE_REC_COUNT - DCPASS_LIST_ROWS) {
        DcPass_HideArrow(view, 0);
    }
}

/* Places the scroll bar's knob. */
void DcPass_DrawScrollBar(DcPassView *view, DcPassList *list) {
    MFlashRef ref;
    MFlash *flash = &view->flash[1];
    s32 range = 0xB5;
    s32 num = SAVE_REC_COUNT;
    s32 y = list->top * range / num;

    Flash_FindLabel(flash, NULL, "mc_scroll_bar_point", &ref);
    Flash_ClipSetScale(flash, &ref, 1.0f, 1.21285701f);
    Flash_ClipSetOffset(flash, &ref, 0, y);
}

/* Lights or dims the plate the list's cursor is on. */
void DcPass_LightRow(DcPassView *view, DcPassList *list, s32 on) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &view->flash[1];

    sprintf(name, "mc_menu_plate2_%d", list->cursor + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Keeps the list's first slot in 0..11; returns whether it was in range. */
s32 DcPass_ClampTop(DcPassList *list) {
    if (list->top < 0) {
        list->top = 0;
        return 0;
    }
    if (list->top > SAVE_REC_COUNT - DCPASS_LIST_ROWS) {
        list->top = SAVE_REC_COUNT - DCPASS_LIST_ROWS;
        return 0;
    }
    return 1;
}

/*
 * Fills the four plates of the slot list: the name and form of the character saved there, or a dimmed plate.
 * FAKE MATCH (permuter): the pointer `buf`. It is set to the name buffer at its declaration and again in the
 * "cursor on an empty slot" arm, and only the second lookup of a filled plate reads it. Without it the code is
 * the same with three saved registers rotated (20 of 161 instructions: name buffer / list / view in s6 / s4 / s5
 * instead of s4 / s5 / s6). The three live through the whole function and are ordered by weighted references
 * over live length; the hoisted `sp + 16` is equivalent to a constant, so its live length counts double (254
 * against 128), and with 14 weighted references it ranks behind list and view (8 each): it needs 16. The
 * second assignment is that reference: it becomes a copy of the hoisted register into a pseudo that gets no
 * register and is rematerialised at its use (`addiu v0,sp,16`, which is what the original has at all three
 * lookups of that arm anyway), and the copy itself is then deleted. So the original had one more use of the
 * hoisted address inside the loop that left no instruction; what it was is not found (the permuter's own form
 * left `buf` unset on the other paths; initialising it keeps the match and makes the C well defined; `buf`
 * for more of the lookups, or set only once, does not match). Also needed, found in the cleanup: the movie is
 * `&view->flash[1]` written at every use, not a local.
 */
void DcPass_DrawRows(DcPassView *view, DcPassList *list) {
    MFlashRef ref;
    char name[0x100];
    s32 i;
    s32 chara;
    char *buf = name;

    for (i = 0; i < 4; i++) {
        sprintf(name, "mc_menu_plate2_%d", i + 1);
        if (i != 3) {
            chara = list->chara[i + list->top];
            if (i == list->cursor) {
                if (chara == -1) {
                    buf = name;
                    Flash_FindLabel(&view->flash[1], NULL, name, &ref);
                    Flash_ClipSetAlpha(&view->flash[1], &ref, 1.0f);
                    continue;
                }
            } else if (chara == -1) {
                goto empty;
            }
            Flash_FindLabel(&view->flash[1], name, "mc_menu_text1_on", &ref);
            TextBox_AttachLine(&view->flash[1], &ref, 0, 0, chara, &view->nameBox[i]);
            Flash_FindLabel(&view->flash[1], buf, "mc_menu_text2_on", &ref);
            TextBox_AttachLine(&view->flash[1], &ref, 0, 0, chara, &view->formBox[i]);
            Flash_FindLabel(&view->flash[1], NULL, name, &ref);
            Flash_ClipSetAlpha(&view->flash[1], &ref, 1.0f);
        } else {
            chara = list->chara[list->extra];
            if (chara == -1) {
            empty:
                Flash_FindLabel(&view->flash[1], NULL, name, &ref);
                Flash_ClipSetAlpha(&view->flash[1], &ref, 0.5f);
                continue;
            }
            Flash_FindLabel(&view->flash[1], name, "mc_menu_text1_on", &ref);
            TextBox_AttachLine(&view->flash[1], &ref, 0, 0, chara, &view->nameBoxB);
            Flash_FindLabel(&view->flash[1], name, "mc_menu_text2_on", &ref);
            TextBox_AttachLine(&view->flash[1], &ref, 0, 0, chara, &view->formBoxB);
            Flash_FindLabel(&view->flash[1], NULL, name, &ref);
            Flash_ClipSetAlpha(&view->flash[1], &ref, 1.0f);
        }
    }
}

/* Draws the slot list. */
void DcPass_DrawList(DcPass *pass) {
    DcPass_SetListClip(&pass->view);
    DcPass_DrawArrows(&pass->view, &pass->list);
    DcPass_DrawScrollBar(&pass->view, &pass->list);
    DcPass_DrawRows(&pass->view, &pass->list);
}

/* Blinks the guide's eyes and moves her mouth while a voice plays. */
void DcPass_AnimGuide(DcPass *pass) {
    MFlashRef ref;
    MFlash *flash = &pass->view.flash[pass->movie];

    Flash_FindLabel(flash, NULL, "mc_guide_blma_eye", &ref);
    FlashAnim_Blink(flash, &ref, &pass->view.blink, 0);
    Flash_FindLabel(flash, NULL, "mc_guide_blma_mouth", &ref);
    if (Voice_GetStat(0) != MVOICE_IDLE) {
        FlashAnim_Talk(flash, &ref, &pass->view.talk, 0);
    } else {
        FlashAnim_ShowNext2(flash, &ref, 0);
    }
}

/* Scrolls the backdrop pattern of both movies. */
void DcPass_AnimBg(DcPass *pass) {
    MFlashRef ref;
    MFlashUv uv;
    s32 i;

    uv.y0 = 0;
    uv.y1 = 0x40;
    uv.x0 = 0;
    uv.x1 = 0x40;
    for (i = 0; i < DCPASS_FLASH_NUM; i++) {
        Flash_FindLabel(&pass->view.flash[i], NULL, "mc_compane_3", &ref);
        FlashAnim_Scroll(&pass->view.flash[i], &ref, &uv, &pass->view.scroll, NULL, -0.152380944f, 0.0f);
    }
}

/* Puts the new character's name and form name on the two pairs of plates. */
void DcPass_DrawNames(DcPass *pass) {
    MFlashRef ref;
    MFlash *flash = &pass->view.flash[1];
    s32 chara = pass->status.chara;

    Flash_FindLabel(flash, NULL, "mc_name_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.nameBoxL);
    Flash_FindLabel(flash, NULL, "mc_form_text_l", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.formBoxL);
    Flash_FindLabel(flash, NULL, "mc_name_text_r", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.nameBoxR);
    Flash_FindLabel(flash, NULL, "mc_form_text_r", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &pass->view.formBoxR);
}

/* Whether L1 or L2 asks to move the text cursor left. */
s32 DcPass_IsPrevPressed(void) {
    if (gPad[0].status == 0 && (*(u64 *)&gPad[0].gameRepeat & (PADG_L1 | PADG_L2))) {
        return 1;
    }
    return 0;
}

/* Whether R1 or R2 asks to move the text cursor right. */
s32 DcPass_IsNextPressed(void) {
    if (gPad[0].status == 0 && (*(u64 *)&gPad[0].gameRepeat & (PADG_R1 | PADG_R2))) {
        return 1;
    }
    return 0;
}

/* Draws the first blink timer. */
void DcPass_ResetBlink(DcPassView *view) {
    view->blink = Rand_Range(0x20);
}

/* Empties the password text. */
void DcPass_ResetText(DcPassText *text) {
    text->unk50 = -100;
    text->unk58 = 1;
    text->unk54 = -100;
    DcPassText_Clear(text);
}

/* Sets the work's starting values. */
void DcPass_Start(DcPass *pass) {
    DcPass_ResetBlink(&pass->view);
    DcPass_ResetText(&pass->text);
    pass->state = DCPASS_ST_TYPE;
    pass->timer = 30;
}

/* Advances both movies. */
void DcPass_Update(DcPass *pass) {
    s32 i;

    for (i = 0; i < DCPASS_FLASH_NUM; i++) {
        Flash_Advance(&pass->view.flash[i]);
    }
}

/*
 * A section of the screen's pack. `host` is the file the section was built from: the development build could
 * read it from the host PC. Nothing uses it here, but the strings are still in the object, in this order.
 */
static inline u8 *DcPass_Section(DcPass *pass, s32 n, const char *host) {
    return MPACK_AT(pass->res, n);
}

#define DP_HOST "host:data/ps2/test/datacenter/password_input/"

#define DP_RES(n, host) \
    res = (MTexRes *)DcPass_Section(pass, n, host); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen (section `section` of archive 8) and builds its two movies, text boxes and windows. */
void DcPass_Init(DcPass *pass, s32 section) {
    MTexRes *res = NULL;
    s32 i;

    pass->pack = MPACK_AT(gMenuArc8, section);
    pass->res = Sprite_Unpack(pass->pack, NULL, NULL);
    DP_RES(10, DP_HOST "dc_select_bg_PS2_.dbt");
    pass->view.bg = res;
    DP_RES(15, DP_HOST "dc_compane_PS2_.dbt");
    pass->view.tex0[0] = MTEX(res, 0);
    pass->view.tex0[1] = MTEX(res, 1);
    pass->view.tex0[2] = MTEX(res, 2);
    pass->view.tex0[3] = MTEX(res, 3);
    pass->view.tex0[4] = MTEX(res, 4);
    pass->view.tex0[5] = MTEX(res, 5);
    pass->view.tex0[6] = MTEX(res, 6);
    DP_RES(15, DP_HOST "dc_compane_PS2_.dbt");
    pass->view.tex1[0] = MTEX(res, 0);
    pass->view.tex1[1] = MTEX(res, 1);
    pass->view.tex1[2] = MTEX(res, 2);
    pass->view.tex1[3] = MTEX(res, 3);
    pass->view.tex1[4] = MTEX(res, 4);
    pass->view.tex1[5] = MTEX(res, 5);
    pass->view.tex1[6] = MTEX(res, 6);
    DP_RES(16, DP_HOST "pwd_imput_EFFECT_PS2_.dbt");
    pass->view.tex0[24] = MTEX(res, 0);
    pass->view.tex0[25] = MTEX(res, 1);
    pass->view.tex0[26] = MTEX(res, 2);
    DP_RES(16, DP_HOST "pwd_imput_EFFECT_PS2_.dbt");
    pass->view.tex1[30] = MTEX(res, 0);
    pass->view.tex1[31] = MTEX(res, 1);
    pass->view.tex1[32] = MTEX(res, 2);
    DP_RES(11, DP_HOST "dc_select_guide_PS2_.dbt");
    pass->view.tex0[27] = MTEX(res, 0);
    pass->view.tex0[28] = MTEX(res, 1);
    pass->view.tex0[29] = MTEX(res, 3);
    DP_RES(11, DP_HOST "dc_select_guide_PS2_.dbt");
    pass->view.tex1[34] = MTEX(res, 0);
    pass->view.tex1[35] = MTEX(res, 1);
    pass->view.tex1[36] = MTEX(res, 3);
    DP_RES(1, DP_HOST "pwd_imput_1_TEX_PS2_.dbt");
    pass->view.tex0[7] = MTEX(res, 0);
    pass->view.tex0[10] = MTEX(res, 1);
    pass->view.tex0[12] = MTEX(res, 2);
    pass->view.tex0[14] = MTEX(res, 3);
    pass->view.tex0[15] = MTEX(res, 4);
    pass->view.tex0[16] = MTEX(res, 5);
    pass->view.tex0[18] = MTEX(res, 6);
    pass->view.tex0[19] = MTEX(res, 7);
    pass->view.tex0[20] = MTEX(res, 8);
    pass->view.tex0[22] = MTEX(res, 9);
    DP_RES(2, DP_HOST "pwd_imput_1_TEXT_JP_PS2_.dbt");
    pass->view.tex0[8] = MTEX(res, 0);
    pass->view.tex0[9] = MTEX(res, 1);
    pass->view.tex0[11] = MTEX(res, 2);
    pass->view.tex0[13] = MTEX(res, 3);
    pass->view.tex0[17] = MTEX(res, 4);
    pass->view.tex0[21] = MTEX(res, 5);
    pass->view.tex0[23] = MTEX(res, 6);
    Flash_Create(&pass->view.flash[0], DcPass_Section(pass, 17, DP_HOST "password_input_1_PS2_.fod"), pass->view.tex0);
    Flash_Play(&pass->view.flash[0], 1);
    DP_RES(3, DP_HOST "pwd_imput_2_TEX_PS2_.dbt");
    pass->view.tex1[8] = MTEX(res, 0);
    pass->view.tex1[9] = MTEX(res, 1);
    pass->view.tex1[10] = MTEX(res, 2);
    pass->view.tex1[11] = MTEX(res, 3);
    pass->view.tex1[15] = MTEX(res, 4);
    pass->view.tex1[16] = MTEX(res, 5);
    pass->view.tex1[19] = MTEX(res, 6);
    pass->view.tex1[20] = MTEX(res, 7);
    pass->view.tex1[22] = MTEX(res, 8);
    pass->view.tex1[23] = MTEX(res, 9);
    pass->view.tex1[24] = MTEX(res, 10);
    pass->view.tex1[25] = MTEX(res, 11);
    pass->view.tex1[26] = MTEX(res, 12);
    pass->view.tex1[29] = MTEX(res, 13);
    DP_RES(5, DP_HOST "pwd_imput_ball_PS2_.dbt");
    pass->view.tex1[13] = MTEX(res, 0);
    DP_RES(4, DP_HOST "pwd_imput_2_TEXT_JP_PS2_.dbt");
    pass->view.tex1[12] = MTEX(res, 0);
    pass->view.tex1[14] = MTEX(res, 1);
    pass->view.tex1[21] = MTEX(res, 2);
    pass->view.tex1[33] = MTEX(res, 3);
    pass->view.tex1[7] = NULL;
    pass->view.tex1[17] = NULL;
    pass->view.tex1[18] = NULL;
    pass->view.tex1[27] = NULL;
    pass->view.tex1[28] = NULL;
    Flash_Create(&pass->view.flash[1], DcPass_Section(pass, 18, DP_HOST "password_input_2_PS2_.fod"), pass->view.tex1);
    Flash_Play(&pass->view.flash[1], 1);
    pass->view.nameText = DcPass_Section(pass, 13, DP_HOST "chara_name_JP_PS2_.pak");
    pass->view.formText = DcPass_Section(pass, 14, DP_HOST "chara_form_JP_PS2_.pak");
    for (i = 0; i < DCPASS_LIST_ROWS; i++) {
        TextBox_Init(&pass->view.nameBox[i], pass->view.nameText, 2);
        TextBox_SetRect(&pass->view.nameBox[i], 0xAF, 0x1FF, 0x6A, 0x132);
        TextBox_SetUnk80(&pass->view.nameBox[i], 1);
        TextBox_Init(&pass->view.formBox[i], pass->view.formText, 4);
        TextBox_SetRect(&pass->view.formBox[i], 0xAF, 0x1FF, 0x6A, 0x132);
        TextBox_SetUnk80(&pass->view.formBox[i], 1);
    }
    TextBox_Init(&pass->view.nameBoxB, pass->view.nameText, 2);
    TextBox_SetRect(&pass->view.nameBoxB, 0xAF, 0x1FF, 0x6A, 0x132);
    TextBox_SetUnk80(&pass->view.nameBoxB, 1);
    TextBox_Init(&pass->view.formBoxB, pass->view.formText, 4);
    TextBox_SetRect(&pass->view.formBoxB, 0xAF, 0x1FF, 0x6A, 0x132);
    TextBox_SetUnk80(&pass->view.formBoxB, 1);
    TextBox_Init(&pass->view.nameBoxL, pass->view.nameText, 1);
    TextBox_Init(&pass->view.formBoxL, pass->view.formText, 1);
    TextBox_Init(&pass->view.nameBoxR, pass->view.nameText, 2);
    TextBox_Init(&pass->view.formBoxR, pass->view.formText, 2);
    pass->faceFile = Heap_Alloc(DCPASS_FACE_SIZE, 0x40, 0, 2);
    pass->faceRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    File_LoadSync(pass->status.chara + DCPASS_FACE_FILE, pass->faceFile, DCPASS_FACE_SIZE);
    pass->view.subtitles = DcPass_Section(pass, 9, DP_HOST "dcenter_lips_PS2_.pak");
    pass->view.msgText = DcPass_Section(pass, 8, DP_HOST "dc_msg_JP_PS2_.pak");
    MsgWin_Init(DcPass_Section(pass, 6, DP_HOST "if_msg_window_PS2_.pak"), pass->view.msgText, 0, (s32)pass->view.unk17C);
    pass->view.dialogMsg = DcPass_Section(pass, 21, DP_HOST "font_datacenter_JP_PS2_.pak");
    Dialog_Init(DcPass_Section(pass, 7, DP_HOST "if_system_window_JP_PS2_.pak"), pass->view.dialogMsg, 0);
    pass->itemTbl = (ZItemEntry *)DcPass_Section(pass, 12, DP_HOST "zitem_parameter_PS2_.dat");
    PassChk_Init((u32 *)DcPass_Section(pass, 19, DP_HOST "zitem_parameter_PS2_.pak"));
    pass->grid = (DcGridCell *)(DcPass_Section(pass, 20, DP_HOST "character_select_order_PS2_.dat") + 0x10);
    pass->gridCount = *(s32 *)DcPass_Section(pass, 20, DP_HOST "character_select_order_PS2_.dat");
}

/* Frees everything DcPass_Init made. */
void DcPass_Term(DcPass *pass) {
    s32 i;

    MsgWin_Term();
    Dialog_Term();
    PassChk_Term();
    for (i = 0; i < DCPASS_FLASH_NUM; i++) {
        Flash_Destroy(&pass->view.flash[i]);
    }
    if (pass->res != NULL) {
        Heap_Free(pass->res);
        pass->res = NULL;
    }
    if (pass->faceFile != NULL) {
        Heap_Free(pass->faceFile);
        pass->faceFile = NULL;
    }
    if (pass->faceRes != NULL) {
        Heap_Free(pass->faceRes);
        pass->faceRes = NULL;
    }
}

/* Draws the frame. */
void DcPass_Draw(DcPass *pass) {
    MFlash *flash0 = &pass->view.flash[0];

    if (!(pass->flags & DCPASS_STARTED)) {
        DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
        DcPass_LightRow(&pass->view, &pass->list, 1);
        Flash_GotoLabel(flash0, "fl_input_in", 1);
        pass->flags |= DCPASS_STARTED;
    }
    DcPass_DrawKeyboard(pass);
    DcPass_DrawTextLine(pass);
    DcPass_SetFaceAlpha(pass);
    DcPass_DrawStatus(&pass->view, &pass->status);
    DcPass_DrawList(pass);
    DcPass_DrawNames(pass);
    DcPass_AnimGuide(pass);
    DcPass_AnimBg(pass);
#ifdef PORT
    Port_MenuPage(44); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(pass->view.bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(pass->view.bg, 0, 0, 0x80);
#endif
    Flash_Draw(&pass->view.flash[pass->movie]);
    MsgWin_Draw(0, 0, pass->voiceLine);
    Dialog_Draw(0);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Plays the "chosen" animation of the plate the list's cursor is on. */
void DcPass_ConfirmRow(DcPass *pass) {
    MFlashRef ref;
    char name[0x100];
    MFlash *flash = &pass->view.flash[1];

    sprintf(name, "mc_menu_plate2_%d", pass->list.cursor + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_ok");
}

/* Moves the list's cursor by `step`; returns whether it ran off the three rows. */
static inline s32 DcPass_ClampCursor(DcPassList *list) {
    if (list->cursor < 0) {
        list->cursor = 0;
        return 1;
    }
    if (list->cursor > DCPASS_LIST_ROWS - 1) {
        list->cursor = DCPASS_LIST_ROWS - 1;
        return 1;
    }
    return 0;
}

/* The slot the list's cursor is on. */
static inline s32 DcPass_GetSlot(DcPassList *list) {
    return list->top + list->cursor;
}

/* Changes to the other movie when the one shown asks for it (trigger 0 of its timeline). */
static inline void DcPass_SwitchMovie(DcPass *pass) {
    if (pass->view.flash[0].trig & 1) {
        pass->movie = 1;
        Flash_GotoLabel(&pass->view.flash[1], "fl_new_character", 1);
    } else if (pass->view.flash[1].trig & 1) {
        pass->movie = 0;
        Flash_GotoLabel(&pass->view.flash[0], "fl_input_in", 1);
    }
}

/* Leaves the screen from the keyboard. */
static inline void DcPass_Leave(DcPass *pass) {
    pass->result = 0;
    pass->flags |= DCPASS_LEAVE;
    Flash_GotoLabel(&pass->view.flash[0], "fl_input_out", 1);
}

/* Reads pad 0 for the current state. */
void DcPass_Input(DcPass *pass) {
    s32 ret;

    if (!(pass->view.flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (pass->flags & DCPASS_LEAVE) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    DcPass_SwitchMovie(pass);
    switch (pass->state) {
    case DCPASS_ST_TYPE:
        if (pass->movie == 1) {
            break;
        }
        if (DcPass_IsPrevPressed()) {
            if (pass->text.cursor > 0) {
                pass->text.cursor--;
                Snd_PlaySe(1, 0);
            }
        } else if (DcPass_IsNextPressed()) {
            if (pass->text.cursor < DCPASS_TEXT_LAST) {
                pass->text.cursor++;
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].status == 1 && (gPad[0].gameHeld & PADG_SQUARE)) {
            break;
        } else if (gPad[0].gameRepeat & PADG_LEFT) {
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 0);
            do {
                pass->keyPos.col--;
            } while (DcPass_IsSameKeyCol(&pass->keyPos, pass->keyPos.col + 1));
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_RIGHT) {
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 0);
            do {
                pass->keyPos.col++;
            } while (DcPass_IsSameKeyCol(&pass->keyPos, pass->keyPos.col - 1));
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_UP) {
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 0);
            do {
                pass->keyPos.row--;
            } while (DcPass_IsSameKeyRow(&pass->keyPos, pass->keyPos.row + 1));
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 0);
            do {
                pass->keyPos.row++;
            } while (DcPass_IsSameKeyRow(&pass->keyPos, pass->keyPos.row - 1));
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_CIRCLE) {
            pass->keyPos.page++;
            DcPass_WrapPos(&pass->keyPos);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_START) {
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 0);
            DcPass_GotoButton(DCPASS_KEY_OK, &pass->keyPos);
            DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            DcPass_FlashKey(pass->view.flash, &pass->keyPos);
            switch (DcPass_GetKey(&pass->keyPos)) {
            case DCPASS_KEY_PAGE:
                pass->keyPos.page++;
                DcPass_WrapPos(&pass->keyPos);
                break;
            case DCPASS_KEY_OK:
                if (DcPass_Decode(pass)) {
                    pass->state = DCPASS_ST_SHOWN;
                    Flash_GotoLabel(&pass->view.flash[0], "fl_new_character", 1);
                } else {
                    DcPass_Say(pass, 0x13);
                    pass->state = DCPASS_ST_FAILED;
                    Flash_GotoLabel(&pass->view.flash[0], "fl_failure_in", 1);
                    MsgWin_Open();
                }
                break;
            case DCPASS_KEY_BACK:
                if (DcPassText_Delete(&pass->text)) {
                case DCPASS_KEY_QUIT:
                    DcPass_Leave(pass);
                }
                break;
            case DCPASS_KEY_LEFT:
                if (pass->text.cursor > 0) {
                    pass->text.cursor--;
                }
                break;
            case DCPASS_KEY_RIGHT:
                if (pass->text.cursor < DCPASS_TEXT_LAST) {
                    pass->text.cursor++;
                }
                break;
            default:
                if (pass->text.cursor == DCPASS_TEXT_LAST) {
                    DcPassText_Put(&pass->keyPos, &pass->text);
                    if (DcPassText_IsFull(&pass->text)) {
                        DcPass_LightKey(pass->view.flash, &pass->keyPos, 0);
                        DcPass_GotoButton(DCPASS_KEY_OK, &pass->keyPos);
                        DcPass_LightKey(pass->view.flash, &pass->keyPos, 1);
                    }
                } else {
                    DcPassText_Put(&pass->keyPos, &pass->text);
                }
                break;
            }
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            if (DcPassText_Delete(&pass->text)) {
                DcPass_Leave(pass);
            }
            Snd_PlaySe(1, 2);
        }
        break;
    case DCPASS_ST_FAILED:
        if ((gPad[0].gamePressed & PADG_CROSS) || Voice_GetStat(0) == MVOICE_IDLE) {
            pass->state = DCPASS_ST_TYPE;
            Flash_GotoLabel(&pass->view.flash[0], "fl_failure_out", 1);
            MsgWin_Close();
            if (gPad[0].gamePressed & PADG_CROSS) {
                Snd_PlaySe(1, 1);
                Voice_StopWithLip();
            }
        }
        break;
    case DCPASS_ST_SHOWN:
        if (gPad[0].gamePressed & PADG_CROSS) {
            if (pass->movie == 1) {
                pass->state = DCPASS_ST_LIST;
                Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_in", 1);
                DcPass_Say(pass, 0x14);
                MsgWin_Open();
                if (gPad[0].gamePressed & PADG_CROSS) {
                    Snd_PlaySe(1, 1);
                }
            }
        }
        break;
    case DCPASS_ST_LIST:
        if ((gPad[0].gameRepeat & PADG_UP) && DcPass_GetSlot(&pass->list) != 0) {
            DcPass_LightRow(&pass->view, &pass->list, 0);
            pass->list.cursor--;
            if (DcPass_ClampCursor(&pass->list)) {
                pass->list.top--;
                if (DcPass_ClampTop(&pass->list)) {
                    Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_down", 1);
                    pass->list.extra = pass->list.top + 3;
                }
            }
            DcPass_LightRow(&pass->view, &pass->list, 1);
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gameRepeat & PADG_DOWN) && DcPass_GetSlot(&pass->list) != SAVE_REC_COUNT - 1) {
            DcPass_LightRow(&pass->view, &pass->list, 0);
            pass->list.cursor++;
            if (DcPass_ClampCursor(&pass->list)) {
                pass->list.top++;
                if (DcPass_ClampTop(&pass->list)) {
                    Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_up", 1);
                    pass->list.extra = pass->list.top - 1;
                }
            }
            DcPass_LightRow(&pass->view, &pass->list, 1);
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            pass->state = DCPASS_ST_ASK_QUIT;
            Dialog_SetMsg(1);
            Dialog_Start(0);
            Dialog_SetCursor(1);
            Dialog_SetChoices(1);
            Voice_StopWithLip();
        } else if ((gPad[0].gamePressed & PADG_LEFT) && pass->list.top + 2 >= 3) {
            pass->list.top -= 3;
            DcPass_ClampTop(&pass->list);
            Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_down", 1);
            pass->list.extra = pass->list.top + 3;
            Snd_PlaySe(1, 0);
        } else if ((gPad[0].gamePressed & PADG_RIGHT) && pass->list.top != SAVE_REC_COUNT - DCPASS_LIST_ROWS) {
            pass->list.top += 3;
            DcPass_ClampTop(&pass->list);
            Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_up", 1);
            pass->list.extra = pass->list.top - 1;
            Snd_PlaySe(1, 0);
        }
        if (gPad[0].gamePressed & PADG_CROSS) {
            DcPass_ConfirmRow(pass);
            if ((&pass->list)->chara[DcPass_GetSlot(&pass->list)] == -1) {
                pass->state = DCPASS_ST_STORED;
                DcPass_StoreRec(&pass->status, &pass->list);
                DcPass_Say(pass, 0x15);
                Snd_PlaySe(1, 1);
            } else {
                pass->state = DCPASS_ST_ASK_OVER;
                Dialog_SetMsg(0);
                Dialog_Start(0);
                Dialog_SetCursor(1);
                Dialog_SetChoices(1);
            }
        }
        break;
    case DCPASS_ST_ASK_QUIT:
        if (Dialog_IsClosed()) {
            pass->state = pass->nextState;
            if (pass->state == DCPASS_ST_TYPE) {
                Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_out", 1);
                Flash_GotoLabel(&pass->view.flash[1], "fl_input_in", 1);
                MsgWin_Close();
            }
        }
        ret = Dialog_Input(0);
        if (ret == 1) {
            pass->nextState = DCPASS_ST_TYPE;
            Dialog_Start(1);
            Dialog_SetChoices(0);
        } else if (ret == -2) {
            pass->nextState = DCPASS_ST_LIST;
            Dialog_Start(1);
            Dialog_SetChoices(0);
        }
        break;
    case DCPASS_ST_ASK_OVER:
        if (Dialog_IsClosed()) {
            pass->state = pass->nextState;
            if (pass->state == DCPASS_ST_STORED) {
                DcPass_Say(pass, 0x15);
            }
        }
        ret = Dialog_Input(0);
        if (ret == 1) {
            pass->nextState = DCPASS_ST_STORED;
            Dialog_Start(1);
            Dialog_SetChoices(0);
            DcPass_StoreRec(&pass->status, &pass->list);
        } else if (ret == -2) {
            pass->nextState = DCPASS_ST_LIST;
            Dialog_Start(1);
            Dialog_SetChoices(0);
        }
        break;
    case DCPASS_ST_STORED:
        if ((gPad[0].gamePressed & PADG_CROSS) || Voice_GetStat(0) == MVOICE_IDLE) {
            if (!(pass->flags & DCPASS_LEAVE)) {
                MsgWin_Close();
                Flash_GotoLabel(&pass->view.flash[1], "fl_chara_list_out", 1);
                if (gPad[0].gamePressed & PADG_CROSS) {
                    Snd_PlaySe(1, 1);
                }
            }
            pass->result = 0;
            pass->flags |= DCPASS_LEAVE;
        }
        break;
    }
}

/* Fades out once the screen is left; returns 1 when the fade has finished. */
s32 DcPass_CheckLeave(DcPass *pass) {
    if (pass->flags & DCPASS_LEAVE) {
        if (!(pass->flags & DCPASS_LEAVING)) {
            pass->flags |= DCPASS_LEAVING;
            ColorFade_StartOut(0, 0, 0, 20);
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

/* The password entry screen (progress mode 54). Returns 0. */
s32 DcPass_Run(s32 section) {
    DcPass pass;

    memset(&pass, 0, sizeof(pass));
    DcPass_Start(&pass);
    DcPass_Init(&pass, section);
    ColorFade_StartIn(0, 0, 0, 20);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        DcPass_Update(&pass);
        DcPass_Draw(&pass);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (DcPass_CheckLeave(&pass)) {
            break;
        }
        DcPass_Input(&pass);
        DcPass_UpdateFace(&pass);
    }
    DcPass_Term(&pass);
    Dma_ResetBuffers();
    return pass.result;
}

/* Clip callback: limits drawing to the slot list's window. */
void DcPass_ClipBegin(void) {
    Sprite_SetScissor(0xAF, 0x1FF, 0x6A, 0x132);
}

/* Clip callback: back to the whole screen. */
void DcPass_ClipEnd(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}
