#include "common.h"
#include "battle/hud_0.h"
#include "sys/pad.h"
#include "sys/heap.h"

/*
 * Battle pause menu, part 2: the menu engine. Source range 0x213238-0x215010.
 *
 * A menu is a BtlMenu with an array of BtlMenuItem; an item may open a sub menu. One tree is shown at a time
 * (BtlMenuWork.top). Each frame PauseMenu_Update calls BtlMenu_Update on the top menu, which recurses into the
 * open sub menu; only the deepest open menu takes input. PauseMenu_Draw draws the deepest open menu only.
 *
 * Input (game button words of the owner's pad, gPad[pad]):
 *   up / down       pressed word: move the cursor (wraps, steps over disabled items); sound 0
 *   cross           pressed word (PADG_CROSS): confirm the item under the cursor
 *   triangle        pressed word: close the menu (state 2); the top menu also closes on start, and only
 *                   when `which` is 0 (the result menu cannot be closed)
 *   the skill list reads the repeat word itself (PauseMenu_SkillListFunc)
 *
 * Opening and closing take 6 frames each: the box grows from / shrinks to the parent's size in equal steps.
 * A menu's box is centred on the 512 x 448 screen.
 *
 * Drawing uses one sprite sheet (a texture file inside the common file) uploaded at TBP 0x2A40 / CBP 0x2A00
 * each frame the menu is shown, and the font module for the item texts.
 *
 * The engine reads the pad (game button words, Pad_GetLastStatus) and writes only menu state; it draws no
 * random numbers and reads no clock or camera. What a menu does to the battle is in the item callbacks and
 * in PauseMenu_Update (hud_0.c). It runs only while the battle is paused or over.
 *
 * Original quirks: `pad` may be -1 for "nobody" (the input tests are skipped), but gPad[pad], text[pad] and
 * Pad_GetLastStatus(pad) are still evaluated with it; BtlMenu_CallItem stores a leaf callback's result in the
 * "menu shown" word (no leaf item of the shipped menus has a callback); the controls page always asks for
 * pad type 0.
 */

extern BtlMenuWork *gBtlMenu;
extern void *gCommonRes;

extern void *memset(void *dst, s32 c, u32 n);
extern u64 *Dma_BeginDirect(void);
extern void Dma_EndDirect(u64 *end);
extern void Gfx_PutDefaultEnv(u64 **pkt);
extern void TexFile_UploadAll(BtlMenuTexFile *file, s32 tbp, s32 cbp); /* TexFile_UploadAll (gfxm_b.txt, not linked yet) */
extern void Res_RelocateOffsets(BtlMenuTexFile **out, BtlMenuTexFile *base, BtlMenuTexFile *hdr);
extern s32 Battle_GetMode(void);
extern s32 Pad_GetLastStatus(s32 pad);
extern s32 Snd_PlaySe(u32 mask, s32 id);
extern s32 Font_GetCmdCount(void);
extern void Font_Flush(s32 first);
extern s32 Font_GetGlyphHeight(void);
extern s32 Font_GetWidth(u16 *str);
extern void Font_Print(u16 *str);
extern void Font_NewLine(void);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_PushCursor(void);
extern void Font_PopCursor(void);
extern void Font_SetHome(s32 x, s32 y);
extern void Font_SetScale(f32 scale);
extern void Font_SetSpacing(s32 x, s32 y);
extern void Font_SetAlign(s32 align);
extern void Font_SetShadowMode(s32 mode);
extern void Font_SetShadowOffset(s32 x, s32 y);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetShadowColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void FontIcon_SetPadType(s32 type);
extern void FontIcon_ResetAnim(void);
extern void BtlText_DrawList(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 mode);
extern void BtlText_DrawScrollBar(u64 **pkt, s32 unused, s32 x, s32 y0, s32 y1);
extern void BtlText_CountEntries(void);

/* Starts a direct packet with the default 2D state, then two A+D registers: TEST_1 = 0 (no alpha / depth
   test) and ZBUF_1 = 0x1310000E0 (depth writes masked). */
void BtlMenu_BeginDraw(u64 **pkt) {
    *pkt = Dma_BeginDirect();
    Gfx_PutDefaultEnv(pkt);
    (*pkt)[0] = 0x1000000000008002;
    (*pkt)[1] = 0xE;
    *pkt += 2;
    (*pkt)[0] = 0;
    (*pkt)[1] = 0x47;
    *pkt += 2;
    (*pkt)[0] = 0x1310000E0;
    (*pkt)[1] = 0x4E;
    *pkt += 2;
}

/* Sends the packet. */
void BtlMenu_EndDraw(u64 **pkt) {
    Dma_EndDirect(*pkt);
}

/* Writes one textured sprite: corners in screen pixels, UV in texels, pixels of sheet entry `tex` with the
   CLUT of entry `clut`. REGLIST GIF tag with 8 registers (PRIM, NOP, TEX0_1, RGBAQ, UV, XYZ2, UV, XYZ2);
   PRIM 0x156 = sprite, textured, blended, UV coordinates; Q = 1.0f; Z = 0xFFFFFFF0; screen offset (0x700, 0x720). */
void BtlMenu_PutSprite(u64 **pkt, s32 x0, s32 y0, s32 x1, s32 y1, s32 u0, s32 v0, s32 u1, s32 v1, u32 color,
                       s32 tex, s32 clut) {
    x0 += 0x700;
    y0 += 0x720;
    x1 += 0x700;
    y1 += 0x720;
    (*pkt)[0] = 0x8400000000008001;
    (*pkt)[1] = 0x535316F0;
    *pkt += 2;
    (*pkt)[0] = 0x156;
    (*pkt)[1] = 0;
    *pkt += 2;
    (*pkt)[0] = BtlMenu_GetTex0(tex, clut);
    (*pkt)[1] = (u64)color | 0x3F80000000000000;
    *pkt += 2;
    (*pkt)[0] = ((s64)u0 << 4) | ((s64)v0 << 20);
    (*pkt)[1] = ((s64)x0 << 4) | ((s64)y0 << 20) | 0xFFFFFFF000000000;
    *pkt += 2;
    (*pkt)[0] = ((s64)u1 << 4) | ((s64)v1 << 20);
    (*pkt)[1] = ((s64)x1 << 4) | ((s64)y1 << 20) | 0xFFFFFFF000000000;
    *pkt += 2;
}

/* Fills a box with mirrored copies of one texture around the box centre: halves = 1 gives a left and a right
   copy (texture drawn at double size), anything else four quarter copies. */
void BtlMenu_PutMirrored(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, u32 color, s32 tex, s32 clut, s32 halves) {
    s32 hw = (x1 - x0) / 2;
    s32 hh = (y1 - y0) / 2;
    s32 cx = x0 + hw;
    s32 cy = y0 + hh;
    s32 w;
    s32 h;

    cx += 0x700;
    cy += 0x720;
    (*pkt)[0] = 0x4400000000008001;
    (*pkt)[1] = 0x16F0;
    *pkt += 2;
    (*pkt)[0] = 0x156;
    (*pkt)[1] = 0;
    *pkt += 2;
    (*pkt)[0] = BtlMenu_GetTex0(tex, clut);
    (*pkt)[1] = (u64)color | 0x3F80000000000000;
    *pkt += 2;
    if (halves == 1) {
        (*pkt)[0] = 0x4400000000008002;
        (*pkt)[1] = 0x5353;
        *pkt += 2;
        w = hw * 2;
        h = hh * 2;
        (*pkt)[0] = 0;
        (*pkt)[1] = ((s64)(cx - hw) << 4) | ((s64)(cy - hh) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)w << 4) | ((s64)h << 20);
        (*pkt)[1] = ((s64)cx << 4) | ((s64)(cy + hh) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = 0;
        (*pkt)[1] = ((s64)(cx + hw) << 4) | ((s64)(cy - hh) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)w << 4) | ((s64)h << 20);
        (*pkt)[1] = ((s64)cx << 4) | ((s64)(cy + hh) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
    } else {
        (*pkt)[0] = 0x4400000000008004;
        (*pkt)[1] = 0x5353;
        *pkt += 2;
        w = hw;
        h = hh;
        (*pkt)[0] = 0;
        (*pkt)[1] = ((s64)(cx - w) << 4) | ((s64)(cy - h) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)w << 4) | ((s64)h << 20);
        (*pkt)[1] = ((s64)cx << 4) | ((s64)cy << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = 0;
        (*pkt)[1] = ((s64)(cx + w) << 4) | ((s64)(cy - h) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)w << 4) | ((s64)h << 20);
        (*pkt)[1] = ((s64)cx << 4) | ((s64)cy << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = 0;
        (*pkt)[1] = ((s64)(cx - w) << 4) | ((s64)(cy + h) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)w << 4) | ((s64)h << 20);
        (*pkt)[1] = ((s64)cx << 4) | ((s64)cy << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = 0;
        (*pkt)[1] = ((s64)(cx + w) << 4) | ((s64)(cy + h) << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
        (*pkt)[0] = ((s64)w << 4) | ((s64)h << 20);
        (*pkt)[1] = ((s64)cx << 4) | ((s64)cy << 20) | 0xFFFFFFF000000000;
        *pkt += 2;
    }
}

/* TEX0 for the pixels of sheet entry `tex` and the CLUT of entry `clut` (TCC on). */
u64 BtlMenu_GetTex0(s32 tex, s32 clut) {
    BtlMenuTex *c = clut + gBtlMenu->tex->ent;
    BtlMenuTex *t = tex + gBtlMenu->tex->ent;
    s32 cbp = c->cbpOfs;
    s32 tbp = t->tbpOfs;
    u64 tex0 = t->tex0;

    tex0 |= (u64)(cbp + 0x2A00) << 37;
    tex0 |= tbp + 0x2A40;
    tex0 |= 0x400000000;
    return tex0;
}

/* Uploads the sprite sheet. */
void BtlMenu_UploadTex(void) {
    TexFile_UploadAll(gBtlMenu->tex, 0x2A40, 0x2A00);
}

/* The pad type used in battle mode 5. */
s32 BtlMenu_GetDefaultPadType(void) {
    return 0;
}

/* 1 when the menu is the skill-list page. */
s32 BtlMenu_IsSkillList(BtlMenu *menu) {
    return menu->items->type == BTL_MENU_ITEM_SKILLS;
}

/* 1 when the menu is the controls page. */
s32 BtlMenu_IsControls(BtlMenu *menu) {
    return menu->items->type == BTL_MENU_ITEM_CONTROLS;
}

/* String n of the common file's offset table. */
u16 *BtlMenu_GetText(s32 n) {
    u32 *file = *(u32 **)((u8 *)gCommonRes + 0x28);

    return (u16 *)(file + file[n] / 4);
}

/* Text of a menu item. */
u16 *BtlMenu_GetItemText(s32 n) {
#ifdef PORT
    /* Tenkaichi Budokai Online's sparring fight goes back to its search, and its pause menu says so */
    static const char from[] = "Return to Character Select";
    static const char to[] = "Back to Budokai Online";
    static u16 line[32];
    extern s32 gPortTboSpar; /* menu/port_tbo.c */
    u16 *s = BtlMenu_GetText(n + 8);

    if (gPortTboSpar) {
        u16 *t = *s == 0xFEFF ? s + 1 : s;
        s32 i;

        for (i = 0; from[i] != 0 && t[i] == (u8)from[i]; i++) {
        }
        if (from[i] == 0 && t[i] == 0) {
            u16 *d = line;

            if (*s == 0xFEFF) {
                *d++ = 0xFEFF;
            }
            for (i = 0; to[i] != 0; i++) {
                d[i] = (u8)to[i];
            }
            d[i] = 0;
            return line;
        }
    }
    return s;
#else
    return BtlMenu_GetText(n + 8);
#endif
}

/* Text of the controls page for a pad type and a control configuration. */
u16 *BtlMenu_GetControlsText(s32 padType, s32 config) {
    s32 base;

    if (padType == 0) {
        base = 0x26;
    } else if (padType == 1) {
        base = 0x4E;
    } else if (padType == 2) {
        base = 0x76;
    } else {
        base = 0x9E;
    }
    return BtlMenu_GetText(base + config);
}

/* Finds the item with an id in a menu or its sub menus (depth first). */
BtlMenuItem *BtlMenu_FindItem(BtlMenu *menu, s32 id) {
    BtlMenuItem *found = NULL;
    s32 i;

    for (i = 0; i < menu->count; i++) {
        if (id == menu->items[i].id) {
            found = &menu->items[i];
            break;
        }
        if (menu->items[i].sub != NULL) {
            found = BtlMenu_FindItem(menu->items[i].sub, id);
            if (found != NULL) {
                break;
            }
        }
    }
    return found;
}

/* Finds the menu that holds the item with an id. */
BtlMenu *BtlMenu_FindOwner(BtlMenu *menu, s32 id) {
    BtlMenu *found = NULL;
    s32 i;

    for (i = 0; i < menu->count; i++) {
        if (id == menu->items[i].id) {
            found = menu;
            break;
        }
        if (menu->items[i].sub != NULL) {
            found = BtlMenu_FindOwner(menu->items[i].sub, id);
            if (found != NULL) {
                break;
            }
        }
    }
    return found;
}

/* Box width: 20 / 22 glyph heights for the two special pages, else the widest item text (at least 128),
   times the scale, plus the margins. */
s32 BtlMenu_GetWidth(BtlMenu *menu) {
    s32 pad = gBtlMenu->pad;
    s32 w = 0;
    s32 i;

    if (menu == NULL) {
        return 0;
    }
    if (BtlMenu_IsSkillList(menu)) {
        w = Font_GetGlyphHeight() * 20;
    } else if (BtlMenu_IsControls(menu)) {
        w = Font_GetGlyphHeight() * 22;
    } else {
        for (i = 0; i < menu->count; i++) {
            s32 n = Font_GetWidth(BtlMenu_GetItemText(menu->items[i].text[pad]));

            if (w < n) {
                w = n;
            }
        }
        if (w < 0x80) {
            w = 0x80;
        }
    }
    return (s32)(w * menu->scale) + menu->padX * 2;
}

/* Box height: 18 / 15 glyph heights for the two special pages, else 28 per item, times the scale, plus margins. */
s32 BtlMenu_GetHeight(BtlMenu *menu) {
    s32 h;

    if (menu == NULL) {
        return 0;
    }
    if (BtlMenu_IsSkillList(menu)) {
        h = Font_GetGlyphHeight() * 18;
    } else if (BtlMenu_IsControls(menu)) {
        h = Font_GetGlyphHeight() * 15;
    } else {
        h = menu->count * 28;
    }
    return (s32)(h * menu->scale) + menu->padY * 2;
}

/* 1 when the open index points at an item that has a sub menu. */
s32 BtlMenu_HasOpenSub(BtlMenu *menu) {
    s32 found = 0;
    s32 i;

    for (i = 0; i < menu->count; i++) {
        if (menu->items[i].sub != NULL && menu->open == i) {
            found = 1;
            break;
        }
    }
    return found;
}

/* Draws row `part` (h texels high) of sheet entry `tex`, w x h pixels, with (x, y) its top-left corner or,
   when `centered`, its centre. */
void BtlMenu_DrawPart(u64 **pkt, s32 x, s32 y, s32 w, s32 h, s32 tex, s32 part, s32 centered) {
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;

    if (!centered) {
        x0 = x;
        y0 = y;
        x1 = x0 + w;
        y1 = y0 + h;
    } else {
        x0 = x - (w >> 1);
        y0 = y - (h >> 1);
        x1 = x + (w >> 1);
        y1 = y + (h >> 1);
    }
    BtlMenu_PutSprite(pkt, x0, y0, x1, y1, 0, h * part, w, (part + 1) * h, 0x80808080, tex, tex);
}

/* Draws a box with sheet entry kind + 1. */
void BtlMenu_DrawFrame(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 kind) {
    BtlMenu_PutMirrored(pkt, x0, x1, y0, y1, 0x80808080, kind + 1, kind + 1, 0);
}

/* Draws the skill list's page bar: sheet entry 4 with the CLUT of entry 0x26 + page. */
void BtlMenu_DrawPageBar(u64 **pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 page) {
    BtlMenu_PutMirrored(pkt, x0 + 4, x1 - 3, y0, y1, 0x80808080, 4, page + 0x26, 0);
}

/* Draws the tab of a skill-list page: sheet entry 3 with the CLUT of entry 0x26 + page, 50 pixels per tab. */
void BtlMenu_DrawPageTab(u64 **pkt, s32 x, s32 y, s32 page) {
    s32 w = 50;
    s32 ofs = page * w + 20;

    x += ofs;
    BtlMenu_PutMirrored(pkt, x, x + w, y + 6, y + 0x19, 0x80808080, 3, page + 0x26, 1);
}

/* Draws a menu: the growing box while it opens or closes; when active, the deepest open menu's box and
   contents (skill list, controls page, or the item texts). */
void BtlMenu_Draw(u64 **pkt, BtlMenu *menu) {
    s32 pad = gBtlMenu->pad;
    BtlMenuList *list = &gBtlMenu->list[pad];
    s32 padX = menu->padX;
    s32 padY = menu->padY;
    s32 skills = BtlMenu_IsSkillList(menu);
    s32 i;
    s32 w;
    s32 h;
    s32 x;
    s32 y;

    switch (menu->state) {
    case BTL_MENU_STATE_OPEN:
    case BTL_MENU_STATE_OPENING:
    case BTL_MENU_STATE_CLOSE:
    case BTL_MENU_STATE_CLOSING:
        w = menu->w;
        h = menu->h;
        if (w < 4 && h >= 4) {
            w = 4;
        }
        if (h < 4 && w >= 4) {
            h = 4;
        }
        x = (0x200 - w) >> 1;
        y = (0x1C0 - h) >> 1;
        BtlMenu_DrawFrame(pkt, x, x + w, y, y + h, skills != 0);
        return;
    case BTL_MENU_STATE_ACTIVE:
        if (!BtlMenu_HasOpenSub(menu)) {
            w = BtlMenu_GetWidth(menu);
            h = BtlMenu_GetHeight(menu);
            x = (0x200 - BtlMenu_GetWidth(menu)) >> 1;
            y = (0x1C0 - BtlMenu_GetHeight(menu)) >> 1;
            if (skills) {
                s32 x1 = x + w;

                BtlMenu_DrawFrame(pkt, x, x1, y + 0x10C, y + h, 1);
                BtlMenu_DrawFrame(pkt, x, x1, y, y + 0x111, 1);
                for (i = list->pages - 1; i >= 0; i--) {
                    BtlMenu_DrawPageTab(pkt, x, y, i);
                }
                BtlMenu_DrawPageBar(pkt, x, x1, y + 0x14, y + 0x39, list->page);
                BtlMenu_DrawPageTab(pkt, x, y, list->page);
            } else {
                BtlMenu_DrawFrame(pkt, x, x + w, y, y + h, 0);
            }
            if (skills) {
                BtlText_DrawList(pkt, x + padX, x - padX + w, y + 0x3E, y + 0x102, 0);
                BtlText_DrawList(pkt, x + padX, x - padX + w, y + 0x11B, y - padY + h, 1);
                BtlText_DrawScrollBar(pkt, x + padX, x - padX + w, y + 0x3E, y + 0x100);
            } else if (BtlMenu_IsControls(menu)) {
                BtlText_DrawControls(pkt, x + padX, x - padX + w, y + padY, y - padY + h);
            } else {
                s32 ty = y + padY;

                i = 0;
                Font_PushStyle();
                Font_PushCursor();
                Font_SetHome(0x100, ty + 0x2A);
                Font_SetScale(menu->scale);
                Font_SetSpacing(0, 0x1C - Font_GetGlyphHeight());
                Font_SetAlign(1);
                Font_SetShadowMode(2);
                Font_SetShadowColorRGBA(0, 0, 0, 0x40);
                Font_SetShadowOffset(1, 2);
                for (; i < menu->count; i++) {
                    s32 text;

                    if (menu->cursor == i) {
                        Font_SetColorRGBA(0xF4, 0xFF, 0x3F, 0x80);
                    } else {
                        Font_SetColorRGBA(0x89, 0xC3, 0xE5, 0x80);
                    }
                    text = menu->items[i].text[pad];
                    if (text >= 0) {
                        Font_Print(BtlMenu_GetItemText(text));
                        Font_NewLine();
                    } else {
                        BtlMenu_DrawPart(pkt, x + w / 2, ty + 0xE, 0x100, 0x20, 0, ~text, 1);
                    }
                }
                Font_PopCursor();
                Font_PopStyle();
            }
            if (!skills && !BtlMenu_IsControls(menu)) {
                FontIcon_ResetAnim();
            }
        }
        for (i = 0; i < menu->count; i++) {
            if (menu->items[i].sub != NULL && menu->open == i) {
                BtlMenu_Draw(pkt, menu->items[i].sub);
            }
        }
        break;
    }
}

/* Calls the callback of an active item. A non-zero result ends the item's activity; for a leaf item the
   result also becomes the "menu shown" word. */
void BtlMenu_CallItem(BtlMenu *menu, s32 idx, s32 leaf) {
    BtlMenuItem *item = &menu->items[idx];
    BtlMenuEvent ev;
    s32 result;

    if (item->active && item->func != NULL) {
        ev.menu = menu;
        ev.id = item->id;
        ev.arg = item->arg;
        ev.first = item->called == 0;
        ev.selected = item->selected;
        result = item->func(&ev);
        if (result != 0) {
            item->active = 0;
        }
        item->selected = 0;
        item->called = 1;
        if (leaf) {
            gBtlMenu->active = result;
        }
    }
}

/* Wraps the cursor into range and moves it forward to an item that is not disabled (not on the two special
   pages). */
void BtlMenu_FixCursor(BtlMenu *menu) {
    s32 start = menu->cursor;

    if (!BtlMenu_IsSkillList(menu) && !BtlMenu_IsControls(menu)) {
        s32 c;
        s32 n;

        do {
            c = menu->cursor;
            if (c < 0) {
                n = menu->count - 1;
                menu->cursor = n;
                c = n;
            }
            if (c >= menu->count) {
                n = 0;
                menu->cursor = n;
                c = n;
            }
            if (!menu->items[c].disabled) {
                break;
            }
            n = c + 1;
            menu->cursor = n;
        } while (n != start);
    }
}

/* Marks the menu as shown. */
void BtlMenu_SetActive(void) {
    gBtlMenu->active = 1;
}

/* Marks the menu as hidden. */
void BtlMenu_ClearActive(void) {
    gBtlMenu->active = 0;
}

/* Allocates the work (0x120 bytes, heap 2) and binds the sprite sheet: entry 7 of the common file's table. */
void BtlMenu_Init(void) {
    u32 *file;

    gBtlMenu = Heap_Alloc(0x120, 0x20, 0, 2);
    memset(gBtlMenu, 0, 0x120);
    BtlMenu_SetActive();
    file = *(u32 **)((u8 *)gCommonRes + 0x28);
    gBtlMenu->tex = (BtlMenuTexFile *)(file + file[7] / 4);
    Res_RelocateOffsets(&gBtlMenu->tex, gBtlMenu->tex, gBtlMenu->tex);
}

/* Frees the work. */
void BtlMenu_Term(void) {
    Heap_Free(gBtlMenu);
    gBtlMenu = NULL;
}

/* Draws the shown tree: uploads the sheet, builds one direct packet, then flushes the font commands added. */
void BtlMenu_DrawAll(void) {
    u64 *pkt;
    s32 first;

    if (gBtlMenu->active && gBtlMenu->top != NULL) {
        BtlMenu_UploadTex();
        first = Font_GetCmdCount();
        BtlMenu_BeginDraw(&pkt);
        BtlMenu_Draw(&pkt, gBtlMenu->top);
        BtlMenu_EndDraw(&pkt);
        Font_Flush(first);
    }
}

/* Returns the work. */
BtlMenuWork *BtlMenu_GetWork(void) {
    return gBtlMenu;
}

/* Sets the tree to draw. */
void BtlMenu_SetTop(BtlMenu *menu) {
    gBtlMenu->top = menu;
}

/* One frame of a menu for controller port `pad` (parent = the menu it was opened from, NULL for the top).
   Returns 1 when the menu has finished closing. */
s32 BtlMenu_Update(s32 which, BtlMenu *parent, BtlMenu *menu, s32 pad) {
    u32 press;
    s32 sound;
    s32 pw;
    s32 ph;
    s32 w;
    s32 h;

    gBtlMenu->active = 1;
    gBtlMenu->pad = pad;
    if (Battle_GetMode() == 5) {
        gBtlMenu->padType = BtlMenu_GetDefaultPadType();
    } else {
        gBtlMenu->padType = Pad_GetLastStatus(pad);
    }
    FontIcon_SetPadType(gBtlMenu->padType);
    BtlText_CountEntries();
    gBtlMenu->skillList = 0;
    BtlMenu_FixCursor(menu);
    if (menu->open >= 0) {
        if (menu->items[menu->open].sub != NULL) {
            BtlMenu_CallItem(menu, menu->open, 0);
            if (BtlMenu_Update(which, menu, menu->items[menu->open].sub, pad)) {
                menu->items[menu->open].active = 0;
                menu->open = -1;
            }
            return 0;
        }
        menu->open = -1;
    }
    if (menu->items[menu->cursor].active && menu->items[menu->cursor].func != NULL) {
        goto end;
    }
    press = gPad[pad].gamePressed;
    if (BtlMenu_IsSkillList(menu)) {
        gBtlMenu->skillList = 1;
    }
    if (BtlMenu_IsSkillList(menu) || BtlMenu_IsControls(menu)) {
        sound = 0;
    } else {
        sound = 1;
    }
    switch (menu->state) {
    case BTL_MENU_STATE_OPEN:
        Snd_PlaySe(1, 4);
        menu->state = BTL_MENU_STATE_OPENING;
        pw = BtlMenu_GetWidth(parent);
        ph = BtlMenu_GetHeight(parent);
        w = BtlMenu_GetWidth(menu);
        h = BtlMenu_GetHeight(menu);
        menu->frame = 0;
        menu->w = pw;
        menu->h = ph;
        menu->dw = (w - pw) / 6.0f;
        menu->dh = (h - ph) / 6.0f;
        /* fall through */
    case BTL_MENU_STATE_OPENING:
        menu->w += menu->dw;
        menu->h += menu->dh;
        menu->frame++;
        if (menu->frame >= 6) {
            menu->w = BtlMenu_GetWidth(menu);
            menu->h = BtlMenu_GetHeight(menu);
            menu->state = BTL_MENU_STATE_ACTIVE;
        }
        break;
    case BTL_MENU_STATE_CLOSE:
        Snd_PlaySe(1, 5);
        menu->state = BTL_MENU_STATE_CLOSING;
        pw = BtlMenu_GetWidth(parent);
        ph = BtlMenu_GetHeight(parent);
        w = BtlMenu_GetWidth(menu);
        h = BtlMenu_GetHeight(menu);
        menu->frame = 0;
        menu->w = w;
        menu->h = h;
        menu->dw = (pw - w) / 6.0f;
        menu->dh = (ph - h) / 6.0f;
        /* fall through */
    case BTL_MENU_STATE_CLOSING:
        menu->w += menu->dw;
        menu->h += menu->dh;
        menu->frame++;
        if (menu->frame >= 6) {
            menu->w = BtlMenu_GetWidth(parent);
            menu->h = BtlMenu_GetHeight(parent);
            menu->state = BTL_MENU_STATE_OPEN;
            return 1;
        }
        break;
    case BTL_MENU_STATE_ACTIVE: {
        s32 move;
        s32 old;

        if (pad >= 0) {
            if (gBtlMenu->top != menu) {
                if (press & PADG_TRIANGLE) {
                    menu->state = BTL_MENU_STATE_CLOSE;
                    break;
                }
            } else if ((press & (PADG_START | PADG_TRIANGLE)) && which == 0) {
                menu->state = BTL_MENU_STATE_CLOSE;
                break;
            }
        }
        move = 0;
        old = menu->cursor;
        if (pad >= 0) {
            if (press & PADG_UP) {
                if (sound) {
                    Snd_PlaySe(1, 0);
                }
                move = -1;
            }
            if (press & PADG_DOWN) {
                if (sound) {
                    Snd_PlaySe(1, 0);
                }
                move = 1;
            }
        }
        if (move != 0) {
            menu->cursor += move;
            for (;;) {
                if (menu->cursor < 0) {
                    menu->cursor = menu->count - 1;
                }
                if (menu->cursor >= menu->count) {
                    menu->cursor = 0;
                }
                if (menu->cursor == old) {
                    break;
                }
                if (!menu->items[menu->cursor].disabled) {
                    break;
                }
                menu->cursor += move;
            }
        } else if (pad >= 0) {
            BtlMenuItem *item = &menu->items[menu->cursor];

            if (press & PADG_CROSS) {
                item->picked = 1;
                if (item->sub != NULL) {
                    menu->open = menu->cursor;
                    BtlMenu_Update(which, menu, menu->items[menu->open].sub, pad);
                    if (item->func != NULL) {
                        item->active = 1;
                        item->selected = 1;
                    }
                } else if (item->func != NULL) {
                    item->active = 1;
                    item->selected = 1;
                } else {
                    s32 i;

                    switch (item->type) {
                    case BTL_MENU_ITEM_BACK:
                        menu->state = BTL_MENU_STATE_CLOSE;
                        break;
                    case BTL_MENU_ITEM_VALUE:
                        if (menu->mode == BTL_MENU_MODE_RADIO) {
                            for (i = 0; i < menu->count; i++) {
                                menu->items[i].value = 0;
                            }
                            item->value = 1;
                        } else if (menu->mode == BTL_MENU_MODE_SINGLE) {
                            item->value ^= 1;
                            if (item->value) {
                                for (i = 0; i < menu->count; i++) {
                                    if (i != menu->cursor) {
                                        menu->items[i].value = 0;
                                    }
                                }
                            }
                        } else if (menu->mode == BTL_MENU_MODE_TOGGLE) {
                            item->value ^= 1;
                        }
                        break;
                    }
                }
            }
        }
        break;
    }
    }
end:
    if (menu->items[menu->cursor].sub == NULL) {
        BtlMenu_CallItem(menu, menu->cursor, 1);
    }
    return 0;
}

/* Value of the item with an id (0 when there is none). */
s32 BtlMenu_GetValue(BtlMenu *menu, s32 id) {
    BtlMenuItem *item = BtlMenu_FindItem(menu, id);

    if (item != NULL) {
        return item->value;
    }
    return (s32)item;
}

/* Sets the value of the item with an id. */
void BtlMenu_SetValue(BtlMenu *menu, s32 id, s32 value) {
    BtlMenuItem *item = BtlMenu_FindItem(menu, id);

    if (item != NULL) {
        item->value = value;
    }
}

/* The "confirmed once" word of the item with an id. */
s32 BtlMenu_GetPicked(BtlMenu *menu, s32 id) {
    BtlMenuItem *item = BtlMenu_FindItem(menu, id);

    if (item != NULL) {
        return item->picked;
    }
    return (s32)item;
}

/* Sets it. */
void BtlMenu_SetPicked(BtlMenu *menu, s32 id, s32 picked) {
    BtlMenuItem *item = BtlMenu_FindItem(menu, id);

    if (item != NULL) {
        item->picked = picked;
    }
}

/* Enables / disables the item with an id. */
void BtlMenu_SetDisabled(BtlMenu *menu, s32 id, s32 disabled) {
    BtlMenuItem *item = BtlMenu_FindItem(menu, id);

    if (item != NULL) {
        item->disabled = disabled;
    }
}

/* Closes a tree: no open sub menu, cursor 0, state "open", every item's active / value / picked cleared. */
void BtlMenu_Reset(BtlMenu *menu) {
    s32 i;

    if (menu != NULL) {
        menu->open = -1;
        menu->cursor = 0;
        menu->state = BTL_MENU_STATE_OPEN;
        for (i = 0; i < menu->count; i++) {
            menu->items[i].active = 0;
            menu->items[i].value = 0;
            menu->items[i].picked = 0;
            if (menu->items[i].sub != NULL) {
                BtlMenu_Reset(menu->items[i].sub);
            }
        }
        gBtlMenu->active = 0;
    }
}

/* Looks for a picked item of type "result" in a tree. Found in the top menu, the item itself is returned;
   found deeper, the top menu's item under the cursor is (the entry the box was opened from). */
BtlMenuItem *BtlMenu_FindResult(BtlMenu *top, BtlMenu *menu) {
    BtlMenuItem *found = NULL;
    s32 i;

    for (i = 0; i < menu->count; i++) {
        if (menu->items[i].type == BTL_MENU_ITEM_RESULT && menu->items[i].picked) {
            if (top == NULL) {
                found = &menu->items[i];
            } else {
                found = &top->items[top->cursor];
            }
            break;
        }
        if (menu->items[i].sub != NULL) {
            if (top == NULL) {
                top = menu;
            }
            found = BtlMenu_FindResult(top, menu->items[i].sub);
            if (found != NULL) {
                break;
            }
        }
    }
    return found;
}

/* 1 when the item with an id has a sub menu and that sub menu is the open one of its menu. */
s32 BtlMenu_IsSubOpen(BtlMenu *menu, s32 id) {
    BtlMenuItem *item = BtlMenu_FindItem(menu, id);
    BtlMenu *owner;

    if (item == NULL) {
        return 0;
    }
    if (item->sub == NULL) {
        return 0;
    }
    owner = BtlMenu_FindOwner(menu, id);
    if (owner == NULL) {
        return 0;
    }
    return owner->items[owner->open].id == id;
}

/* Sets the skill-list script. */
void BtlMenu_SetScript(u16 *text) {
    gBtlMenu->text = text;
}

/* Sets the second script. */
void BtlMenu_SetScript2(u16 *text) {
    gBtlMenu->text2 = text;
}

/* Returns the skill-list script. */
u16 *BtlMenu_GetScript(void) {
    return gBtlMenu->text;
}

/* Returns the second script. */
u16 *BtlMenu_GetScript2(void) {
    return gBtlMenu->text2;
}

/* 1 while the menu is shown. */
s32 BtlMenu_IsActive(void) {
    return gBtlMenu->active;
}
