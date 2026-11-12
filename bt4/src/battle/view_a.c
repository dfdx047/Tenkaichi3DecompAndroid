#include "common.h"
#include "battle/view_a.h"
#include "sys/heap.h"
#include "sys/snd.h"

/*
 * GetWin, 0x25C2A8..0x25CFC0: the reward window of the menus ("title_get" movie: a title strip, an icon, up
 * to two lines of text or an amount of money). Called by the result code of the main executable
 * (0x2BDCE8..0x2BE5A0) and by the menu overlay. See battle/view_a.h.
 *
 * The object starts here: its strings (0x2F30E0) and two jump tables (0x2F3190, 0x2F31C0) follow the padded
 * end of btl_script_cmd.c's jump tables.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 sprintf(char *dst, const char *fmt, ...);

extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Flash_Create(Flash *flash, void *data, void *tex);
extern void Flash_Destroy(Flash *flash);                                  /* destroys the movie */
extern void Flash_Advance(Flash *flash);                                  /* steps the movie */
extern void Flash_Draw(Flash *flash);                                  /* draws the movie */
extern void Flash_Play(Flash *flash, s32 arg);
extern void Flash_GotoLabel(Flash *flash, char *label, s32 arg);             /* starts the animation at a frame label */
extern void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out);
extern void Flash_ClipSetFlags(Flash *flash, FlashRef *ref, s32 prop, s32 value);
extern void Flash_ClipSetTex(Flash *flash, FlashRef *ref, s32 frame);        /* shows one frame of a clip */
extern void Flash_ClipSetUv(Flash *flash, FlashRef *ref, FlashUv *uv);      /* sets the texture rectangle of a clip */
extern void Font_FlushAll(void);
extern s32 Font_CountLines(u16 *str);

extern void Num_Draw_(Flash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode)
    __asm__("Num_Draw");
/* text box module, after 0x2600B0 (not decompiled) */
extern void TextBox_SetLineOffsets(TextBox *box, s32 a, s32 b, s32 c, s32 d, s32 e);
extern void TextBox_AttachLine(Flash *flash, FlashRef *ref, s32 a, s32 b, s32 line, TextBox *box); /* draws a line in a clip */

/* Defined here: this object's .sdata (0x2FF0D0). */
GetWin *gGetWin = NULL;

/* Allocates the reward window and builds its movie and five text boxes from a pack file. */
void GetWin_Init(u32 *pack, s32 lang) {
    FlashTexRes *res = NULL;
    s32 i;

    gGetWin = Heap_Alloc(sizeof(GetWin), 0x20, 0, HEAP_ANY);
    memset(gGetWin, 0, sizeof(GetWin));

    res = PACK_AT(pack, 1);
    Res_RelocateOffsets(&res, res, res);
    gGetWin->tex[0] = res->tex;
    gGetWin->tex[12] = res->tex + FLASH_TEX_SIZE;
    gGetWin->tex[2] = res->tex + FLASH_TEX_SIZE * 2;
    gGetWin->tex[1] = res->tex + FLASH_TEX_SIZE * 3;

    res = PACK_AT(pack, 2);
    Res_RelocateOffsets(&res, res, res);
    gGetWin->tex[5] = res->tex + FLASH_TEX_SIZE;
    gGetWin->tex[6] = res->tex;
    gGetWin->tex[8] = res->tex + FLASH_TEX_SIZE * 2;

    res = PACK_AT(pack, 3);
    Res_RelocateOffsets(&res, res, res);
    gGetWin->tex[9] = res->tex + FLASH_TEX_SIZE;
    gGetWin->tex[10] = res->tex + FLASH_TEX_SIZE * 2;
    gGetWin->tex[11] = res->tex;

    res = PACK_AT(pack, 4);
    Res_RelocateOffsets(&res, res, res);
    gGetWin->tex[7] = res->tex;

    gGetWin->text[0] = PACK_AT(pack, 8);
    gGetWin->text[1] = PACK_AT(pack, 7);
    gGetWin->text[2] = PACK_AT(pack, 9);
    gGetWin->text[3] = PACK_AT(pack, 11);
    gGetWin->text[4] = PACK_AT(pack, 11);

    TextBox_Init(&gGetWin->box[0], gGetWin->text[0], 0);
    TextBox_Init(&gGetWin->box[1], gGetWin->text[1], 0);
    TextBox_Init(&gGetWin->box[2], gGetWin->text[2], 0);
    TextBox_Init(&gGetWin->box[3], gGetWin->text[3], 0);
    TextBox_Init(&gGetWin->box[4], gGetWin->text[4], 0);
    TextBox_SetColor(&gGetWin->box[1], 0xFFFF0080);

    for (i = 0; i < GETWIN_TEXT_COUNT; i++) {
        TextBox_SetUnk80(&gGetWin->box[i], 1);
        TextBox_SetUnk50(&gGetWin->box[i], 1);
        TextBox_SetUnkC(&gGetWin->box[i], 0x100, 0);
    }

    gGetWin->items = PACK_AT(pack, 10);
    Flash_Create(gGetWin->flash, PACK_AT(pack, lang == 1 ? 5 : 6), gGetWin->tex);
    Flash_Play(gGetWin->flash, 1);
}

/* Destroys the movie and frees the window. */
void GetWin_Term(void) {
    Flash_Destroy(gGetWin->flash);
    if (gGetWin != NULL) {
        Heap_Free(gGetWin);
        gGetWin = NULL;
    }
}

/* Steps and draws the window: picks the title strip and the icon from the kind, then the texts or the amount. */
void GetWin_Draw(void) {
    FlashRef ref;
    FlashUv uv;
    char name[0x40];
    Flash *flash = gGetWin->flash;
    s32 kind = gGetWin->kind;
    s32 i;

    for (i = 0; i < 1; i++) {
        Flash_Advance(&gGetWin->flash[i]);
    }

    uv.y0 = 0;
    switch (kind) {
    case 3:
    case 4:
    case 5:
    case 9:
        uv.y0 = 0x80;
        break;
    case 1:
    case 8:
        uv.y0 = 0x40;
        break;
    case 6:
        uv.y0 = 0xC0;
        break;
    }
    uv.y1 = uv.y0 + 0x40;
    uv.x1 = 0x200;
    uv.x0 = 0;
    Flash_FindLabel(flash, NULL, "title_get", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (kind >= 7 && kind <= 9) {
        Flash_ClipSetTex(flash, &ref, 1);
    } else {
        Flash_ClipSetTex(flash, &ref, 0);
    }

    uv.x0 = 0;
    uv.y0 = 0;
    uv.x1 = 0x40;
    uv.y1 = 0x40;
    switch (kind) {
    case 0:
        uv.x0 = 0x40;
        uv.x1 = 0x80;
        break;
    case 1:
        uv.x0 = 0x80;
        uv.x1 = 0xC0;
        break;
    case 3:
        uv.x0 = 0xC0;
        uv.x1 = 0x100;
        break;
    case 4:
        uv.y0 = 0x40;
        uv.y1 = 0x80;
        break;
    case 5:
        uv.y0 = 0x40;
        uv.x0 = 0x40;
        uv.y1 = 0x80;
        uv.x1 = 0x80;
        break;
    case 6:
        uv.y0 = 0x40;
        uv.y1 = 0x80;
        uv.x0 = 0x80;
        uv.x1 = 0xC0;
        break;
    case 8:
        uv.x1 = 0x80;
        uv.x0 = 0;
        uv.y1 = uv.y0 + 0x80;
        break;
    case 7:
        uv.x1 = 0x40;
        uv.x0 = 0;
        uv.y1 = uv.y0 + 0x40;
        break;
    case 9:
        uv.y0 = 0x40;
        uv.y1 = 0x80;
        uv.x0 = 0xC0;
        uv.x1 = 0x100;
        break;
    }
    Flash_FindLabel(flash, NULL, kind != 8 ? "mc_icon_get_1_loop" : "mc_icon_get_2_loop", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    Flash_FindLabel(flash, NULL, "mc_icon_get_2_loop", &ref);
    Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, kind == 8);
    Flash_FindLabel(flash, NULL, "mc_icon_get_1_loop", &ref);
    Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, kind != 8);

    for (i = 0; i < 7; i++) {
        sprintf(name, "mc_pay_num_%d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, kind == 6);
    }
    if (kind == 6) {
        Num_Draw_(flash, "mc_pay_num_%d", 0, 7, gGetWin->line2, 0x20, 0x20, 0);
    }

    Flash_FindLabel(flash, NULL, "mc_battle_text_base", &ref);
    Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, kind == 8);

    if (gGetWin->line1 >= 0) {
        Flash_FindLabel(flash, NULL, "mc_dammy_text_1", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gGetWin->line1, &gGetWin->box[gGetWin->box1]);
    } else {
        Flash_FindLabel(flash, NULL, "mc_text_base_1", &ref);
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
        Flash_FindLabel(flash, NULL, "mc_dammy_text_1", &ref);
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
    }
    Flash_Draw(gGetWin->flash);

    if (gGetWin->line2 >= 0) {
        if (kind == 8) {
            if (gGetWin->tall != 0) {
                TextBox_SetLineOffsets(&gGetWin->box[gGetWin->box2], 0xC, 0, 0, 0, 0);
            }
            Flash_FindLabel(flash, NULL, "mc_text_base_2", &ref);
            Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
        }
        Flash_FindLabel(flash, NULL, "mc_dammy_text_2", &ref);
        if (kind != 6) {
            TextBox_AttachLine(flash, &ref, 0, 0, gGetWin->line2, &gGetWin->box[gGetWin->box2]);
        }
        TextBox_SetLineOffsets(&gGetWin->box[gGetWin->box2], 0, 0, 0, 0, 0);
    } else {
        Flash_FindLabel(flash, NULL, "mc_text_base_2", &ref);
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, kind == 6);
        Flash_FindLabel(flash, NULL, "mc_dammy_text_2", &ref);
        Flash_ClipSetFlags(flash, &ref, FLASH_PROP_VISIBLE, 0);
    }
    Font_FlushAll();
}

/* Plays the opening animation and its sound. */
void GetWin_Open(void) {
    Flash_GotoLabel(gGetWin->flash, "fl_get_in", 1);
    Snd_PlaySe(2, gGetWin->se);
}

/* Plays the closing animation. */
void GetWin_Close(void) {
    Flash_GotoLabel(gGetWin->flash, "fl_get_out", 1);
}

/* Plays the "next reward" animation and its sound. */
void GetWin_Next(void) {
    Flash_GotoLabel(gGetWin->flash, "fl_next_get", 1);
    Snd_PlaySe(2, gGetWin->se);
}

/* Non-zero while one of the three animations runs. */
s32 GetWin_IsAnimating(void) {
    if (gGetWin->flash[0].flags & 1) {
        return 1;
    }
    return 0;
}

/* Chooses what the window announces: the kind and a line number, item index or amount. */
void GetWin_Setup(s32 kind, s32 value) {
    gGetWin->line1 = -1;
    gGetWin->line2 = __builtin_abs(value);
    gGetWin->tall = 0;
    gGetWin->box1 = gGetWin->box2 = 0;

    if (kind == 2) {
        u8 type = gGetWin->items[value].type;

        kind = 5;
        if (type == 1) {
            kind = 4;
        }
        if (type == 0) {
            /* Two statements that end in 3: the original block was folded only late (its conditional move
               keeps an unsimplified `xori 0`), which a plain `kind = 3` does not reproduce. What the
               statements really were is not known. */
            kind = 4;
            kind--;
        }
        gGetWin->box2 = 2;
        gGetWin->se = 6;
    } else if (kind == 7) {
        gGetWin->box2 = 3;
        gGetWin->se = 7;
    } else if (kind == 8) {
        gGetWin->se = 8;
        gGetWin->line1 = gGetWin->line2;
        if (gGetWin->line1 < 3) {
            gGetWin->line1 = 0;
        } else if (gGetWin->line1 >= 3 && gGetWin->line1 < 7) {
            gGetWin->line1 = 1;
        } else if (gGetWin->line1 >= 7 && gGetWin->line1 < 14) {
            gGetWin->line1 = 2;
        } else if (gGetWin->line1 >= 14 && gGetWin->line1 < 19) {
            gGetWin->line1 = 3;
        } else if (gGetWin->line1 >= 19 && gGetWin->line1 < 35) {
            gGetWin->line1 = 4;
        } else if (gGetWin->line1 >= 35 && gGetWin->line1 < 40) {
            gGetWin->line1 = 5;
        } else if (gGetWin->line1 >= 40 && gGetWin->line1 < 44) {
            gGetWin->line1 = 6;
        } else if (gGetWin->line1 >= 44 && gGetWin->line1 < 48) {
            gGetWin->line1 = 7;
        }
        gGetWin->box1 = 3;
        gGetWin->box2 = 4;
        gGetWin->line2 += 8;
        if (Font_CountLines(PACK_AT(gGetWin->text[gGetWin->box1], gGetWin->line2 + 1)) > 0) {
            gGetWin->tall = 1;
        }
    } else if (kind == 0) {
        gGetWin->line1 = gGetWin->line2;
        gGetWin->box1 = 0;
        gGetWin->box2 = 1;
        gGetWin->se = 7;
    } else if (kind == 6) {
        gGetWin->se = 6;
    } else if (kind == 1) {
        gGetWin->line2 += 0x38;
        gGetWin->box2 = 3;
        gGetWin->se = 7;
    } else if (kind == 9) {
        gGetWin->line2 += 0x5B;
        gGetWin->box2 = 4;
        gGetWin->se = 8;
    }
    gGetWin->kind = kind;
}
