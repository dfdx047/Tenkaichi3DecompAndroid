#include "common.h"
#include "battle/btl_seq.h"
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "sys/heap.h"
#include "sys/adx.h"

/* ---- Skill-list text of the pause menu, 0x215420-0x216AC0 ----
 * The list is a UTF-16 script. Every line starts with two characters whose second one is a hex digit (a mask
 * tested against work->unk10), optionally followed by "&ddd" (unlock condition), then a tag:
 *   '$n'   page n title          '*abc' entry (three digits a, b, c)      '%0' / '%1' detail lines
 *   '#'    note                  '@'    end of the script
 * Lines are separated by BtlText_NextLine(). BtlTextList / BtlTextWork are in battle/btl_seq.h.
 *
 * What the matching code says about the original source file (see also the note above BtlSeq_FirstState):
 * - BtlText_DrawPart (0x215420) was defined in the same file as, and above, BtlText_DrawList: two branches of
 *   DrawList are annulled / not annulled the right way only when the compiler has already compiled DrawPart.
 *   That is why this file starts at 0x215420 and not at 0x215540. The file may well have started earlier still.
 * - BtlText_IsLineDimmed was `static`: DrawList keeps the text pointer in a register across the call, which the
 *   compiler only does for a function it has marked `const` itself, and it only does that for static ones. */

extern f32 gBtlTextAlpha;
extern u64 *gSaveData; /* local view: only charaBits (+0xC10) is used here */

extern BtlTextWork *BtlMenu_GetWork(void);
extern u16 *BtlMenu_GetScript(void);
extern u16 *BtlMenu_GetScript2(void);
extern u16 *BtlText_NextLine(u16 *line);        /* start of the next line */
extern void BtlText_PutScissor(void *pkt, s32 x0, s32 y0, s32 x1, s32 y1); /* GS scissor */
extern void BtlText_PutSprite(void *pkt, s32 x0, s32 y0, s32 x1, s32 y1, s32 u, s32 v, s32 w, s32 h, u32 color, s32 part);
extern s32 BtlCtrl_TestMemberUnk70(s32 side);
extern s32 Font_GetCmdCount(void);
extern void Font_Flush(s32 font);
extern s32 Font_GetGlyphHeight(void);
extern f32 Font_FitScale(u16 *str, s32 width);
extern void Font_PrintAt(s32 x, s32 y, u16 *str);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetScale(f32 a);
extern void Font_SetScaleXY(f32 sx, f32 sy);
extern void Font_GetScaleXY(f32 *sx, f32 *sy);
extern void Font_SetClip(s32 x0, s32 y0, s32 x1, s32 y1);
extern void Font_SetAlign(s32 align);
extern void Font_SetFlags(s32 a);
extern void Font_SetShadowMode(s32 a);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetColor2RGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetShadowColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern s32 FontIcon_GetPadTagSize(u16 *str);
extern s32 Battle_IsSplitScreen(void);


/* Draws one part of the list sprite sheet: a w x h rectangle at (x, y); a negative h flips it vertically. */
void BtlText_DrawPart(void *pkt, s32 x, s32 y, s32 w, s32 h, s32 part) {
    BtlText_PutSprite(pkt, x, y, x + w, y + h, 0, 0, w, h < 0 ? -h : h, 0x80808080, part);
}

/* Draws the icon of a page title: digit '0'..'8' picks part 7..14 ('7' and '8' share the last one). */
void BtlText_DrawPageIcon(void *pkt, s32 x, s32 y, u16 digit) {
    s32 w = Font_GetGlyphHeight();
    s32 idx = digit - '0';
    s32 part = 0;

    w *= 20;
    switch (idx) {
    case 0:
        part = 0;
        break;
    case 1:
        part = 1;
        break;
    case 2:
        part = 2;
        break;
    case 3:
        part = 3;
        break;
    case 4:
        part = 4;
        break;
    case 5:
        part = 5;
        break;
    case 6:
        part = 6;
        break;
    case 7:
    case 8:
        part = 7;
        break;
    }
    BtlText_DrawPart(pkt, x, y, w, 0x20, part + 7);
}
static s32 BtlText_IsLineDimmed(s32 page, s32 entry, s32 side);

/* Reads the mask digit of a line, steps over the two leading characters and tells if the line is shown. */
s32 BtlText_CheckLineMask(u16 **cursor, s32 side) {
    BtlTextWork *work = BtlMenu_GetWork();
    u16 *p = *cursor;
    u32 mask = 0;
    s32 shift;

    switch (p[1]) {
    case 'F':
        mask = 0xF;
        break;
    case '1':
        mask = 1;
        break;
    case '2':
        mask = 2;
        break;
    case '4':
        mask = 4;
        break;
    case '8':
        mask = 8;
        break;
    case 'E':
        mask = 0xE;
        break;
    case 'D':
        mask = 0xD;
        break;
    case 'B':
        mask = 0xB;
        break;
    case '7':
        mask = 7;
        break;
    }
    p += 2;
    shift = work->unk10;
    *cursor = p;
    return (mask >> shift) & 1;
}

/* Steps over an "&ddd" unlock prefix; 0 when the line is locked (character ddd not unlocked in the save). */
s32 BtlText_CheckUnlock(u16 **cursor) {
    BtlTextWork *work = BtlMenu_GetWork();
    s32 ret = 1;
    u16 *p = *cursor;

    if (p[0] == '&') {
        s32 id = (p[1] - '0') * 100;

        id += (p[2] - '0') * 10;
        id += p[3] - '0';

        p += 4;
        if (id == 0x56) {
            if (BtlCtrl_TestMemberUnk70(work->side) != 0) {
                ret = 0;
            }
        } else {
            ret = (gSaveData[0xC10 / 8 + id / 64] >> (id % 64)) & 1;
        }
    }
    *cursor = p;
    return ret;
}

/* Stub: never dims a line. It has to be static: only then does the compiler see that it is a `const` function,
 * which BtlText_DrawList needs (p stays in a register across the call). */
static s32 BtlText_IsLineDimmed(s32 page, s32 entry, s32 side) {
    return 0;
}

/* A colour component scaled by the line alpha. */
#define BTLTEXT_COL(v) ((u8)(u32)(gBtlTextAlpha * (v)))

/* Draws the list inside the clip rectangle (x0, y0)-(x1, y1). mode 0: the page title and the entries of the
 * current page, scrolled, with the selected one highlighted; other modes: the detail lines ('%') and the note
 * ('#') of the selected entry.
 * Matching notes: the locals are declared in the order of their stack slots (most of them are spilled); the
 * three entry digits are an array; `line` is a register copy of p taken once per line. */
void BtlText_DrawList(void *pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 mode) {
    s32 icon[3];
    u16 *p;
    f32 sx;
    f32 sy;
    BtlTextWork *work;
    s32 side;
    s32 lineH;
    s32 go;
    s32 page;
    s32 entry;
    s32 y;
    BtlTextList *list;
    f32 fit;
    s32 tag;
    u16 *line;
    s32 h;

    work = BtlMenu_GetWork();
    go = 1;
    page = -1;
    side = work->side;
    y = y0;
    entry = 0;
    list = &work->list[side];
    p = BtlMenu_GetScript();
    lineH = Font_GetGlyphHeight() * 20;
    if (p == NULL) {
        return;
    }
    if (mode == 0) {
        y -= list->scroll[list->page] * 28;
    }
    Font_PushStyle();
    Font_SetFlags(1);
    Font_SetScaleXY(1.0f, 1.0f);
    Battle_IsSplitScreen();
    Font_SetShadowMode(2);
    Font_SetColorRGBA(0xFF, 0xFF, 0xFF, 0x80);
    Font_SetShadowColorRGBA(0, 0x10, 0x10, 0x40);
    if (mode == 0) {
        Font_SetClip(x0, y0, x1, y1);
        BtlText_PutScissor(pkt, x0, y0, x1, y1);
    } else {
        Font_SetClip(x0, y0 - 7, x1, y1 + 7);
        BtlText_PutScissor(pkt, x0, y0 - 7, x1, y1 + 7);
    }
    p += 1;
    while (go && *p != 0) {
        if (BtlText_CheckLineMask(&p, side) == 0) {
            p = BtlText_NextLine(p);
            continue;
        }
        if (BtlText_CheckUnlock(&p) == 0) {
            gBtlTextAlpha = 0.5f;
        }
        line = p;
        tag = *line;
        switch (tag) {
        case '$':
            page = line[1] - '0';
            entry = -1;
            break;
        case '*':
            entry++;
            break;
        }
        if (tag != '@' && page != list->page) {
            p = BtlText_NextLine(line);
            continue;
        }
        if (tag != '$' && BtlText_IsLineDimmed(page, entry, side) != 0) {
            gBtlTextAlpha = 0.5f;
        }
        if (mode == 0) {
            switch (tag) {
            case '$':
                Font_PushStyle();
                BtlText_PutScissor(pkt, 0, 0, 0x1FF, 0x1BF);
                Font_SetClip(0, 0, 0x1FF, 0x1BF);
                Font_SetColorRGBA(0xFF, 0xFF, 0xFF, 0x80);
                Font_SetShadowColorRGBA(0x56, 0x3D, 0x39, 0x80);
                Font_PrintAt(x0 + 0x1E, y0 - 0x22, p + 2);
                BtlText_DrawPageIcon(pkt, x0, y0 - 0x28, p[1]);
                BtlText_PutScissor(pkt, x0, y0, x1, y1);
                Font_PopStyle();
                break;
            case '*':
                if (y0 < y + 0x20 && y < y1) {
                    icon[0] = p[1] - '0';
                    icon[1] = p[2] - '0';
                    icon[2] = p[3] - '0';
                    if (entry == list->cursor[list->page]) {
                        Font_SetColorRGBA(BTLTEXT_COL(255.0f), BTLTEXT_COL(247.0f), BTLTEXT_COL(15.0f), 0x80);
                        Font_SetShadowColorRGBA(BTLTEXT_COL(28.0f), BTLTEXT_COL(32.0f), BTLTEXT_COL(21.0f), 0x40);
                    } else {
                        Font_SetColorRGBA(BTLTEXT_COL(255.0f), BTLTEXT_COL(255.0f), BTLTEXT_COL(255.0f), 0x80);
                        Font_SetShadowColorRGBA(BTLTEXT_COL(32.0f), BTLTEXT_COL(43.0f), BTLTEXT_COL(94.0f), 0x40);
                    }
                    fit = Font_FitScale(p + 2, (x1 - x0) - 0x44);
                    if (fit < 1.0f) {
                        Font_PushStyle();
                        Font_GetScaleXY(&sx, &sy);
                        Font_SetScaleXY(sx * fit, sy);
                        Font_PrintAt(x0 + 0xF, y + 3, p + 4);
                        Font_PopStyle();
                    } else {
                        Font_PrintAt(x0 + 0xF, y + 3, p + 4);
                    }
                    h = lineH - 2;
                    if (entry == list->cursor[list->page]) {
                        BtlText_DrawPart(pkt, x0 - 1, y, h, 0x1F, 0x11);
                    } else {
                        BtlText_DrawPart(pkt, x0 - 1, y, h, 0x1F, 0x10);
                    }
                    if (icon[0] > 0) {
                        BtlText_DrawPart(pkt, x1 - 0x35, y - 5, h, 0x1F, 0x13);
                    }
                    if (icon[1] > 0) {
                        BtlText_DrawPart(pkt, x1 - 0x20, y - 5, h, 0x1F, icon[1] + 0x1C);
                    }
                    if (icon[2] > 0) {
                        BtlText_DrawPart(pkt, x1 - 0x20, y - 5, h, 0x1F, icon[2] + 0x13);
                    }
                }
                y += 0x1C;
                break;
            case '@':
                go = 0;
                break;
            }
        } else {
            switch (tag) {
            case '%':
                switch (line[1]) {
                case '0':
                    if (entry != list->cursor[list->page]) {
                        break;
                    }
                    Font_SetColorRGBA(BTLTEXT_COL(170.0f), BTLTEXT_COL(215.0f), BTLTEXT_COL(255.0f), 0x80);
                    Font_SetShadowColorRGBA(BTLTEXT_COL(43.0f), BTLTEXT_COL(54.0f), BTLTEXT_COL(64.0f), 0x40);
                    fit = Font_FitScale(p + 2, (x1 - x0) - 0x1E);
                    if (fit < 1.0f) {
                        Font_PushStyle();
                        Font_GetScaleXY(&sx, &sy);
                        Font_SetScaleXY(sx * fit, sy);
                        Font_PrintAt(x0 + 0xF, y + 3, p + 2);
                        Font_PopStyle();
                    } else {
                        Font_PrintAt(x0 + 0xF, y + 3, p + 2);
                    }
                    y += 0x1C;
                    break;
                case '1':
                    if (entry != list->cursor[list->page]) {
                        break;
                    }
                    Font_SetColorRGBA(BTLTEXT_COL(239.0f), BTLTEXT_COL(89.0f), BTLTEXT_COL(89.0f), 0x80);
                    Font_SetShadowColorRGBA(BTLTEXT_COL(64.0f), BTLTEXT_COL(17.0f), BTLTEXT_COL(0.0f), 0x40);
                    fit = Font_FitScale(p + 2, (x1 - x0) - 0x1E);
                    if (fit < 1.0f) {
                        Font_PushStyle();
                        Font_GetScaleXY(&sx, &sy);
                        Font_SetScaleXY(sx * fit, sy);
                        Font_PrintAt(x0 + 0xF, y + 3, p + 2);
                        Font_PopStyle();
                    } else {
                        Font_PrintAt(x0 + 0xF, y + 3, p + 2);
                    }
                    y += 0x1C;
                    break;
                }
                break;
            case '#':
                if (entry == list->cursor[list->page]) {
                    Font_SetColor2RGBA(BTLTEXT_COL(128.0f), BTLTEXT_COL(128.0f), BTLTEXT_COL(128.0f), 0x80);
                    Font_PrintAt(x0 + 0xF, y, p + 1);
                    y += 0x1C;
                    Font_SetColor2RGBA(0x80, 0x80, 0x80, 0x80);
                    if (work->unk10 == 1) {
                        if (FontIcon_GetPadTagSize(p + 1) > 0x20) {
                            y += 0x14;
                        }
                    }
                }
                break;
            case '@':
                go = 0;
                break;
            }
        }
        gBtlTextAlpha = 1.0f;
        p = BtlText_NextLine(p);
    }
    Font_PopStyle();
    BtlText_PutScissor(pkt, 0, 0, 0x1FF, 0x1BF);
}

/* Draws the scroll bar of the current page: the frame, then the thumb (always drawn; with more than 7 entries it
 * is shortened and moved, otherwise it fills the frame).
 * Matched in cleanup 4. Two things were needed. (1) A BEHAVIOURAL FIX: the earlier attempt drew the thumb's upper
 * part only inside `if (over)`; the original's `beqz` skips only the two coordinate updates (the wrong branch
 * target was hidden among 29 "register swap" lines). (2) `len` is computed in front of the `if`, and the height
 * behind it goes through `h = y0; h = y1 - h;`: written `h = y1 - y0;` gcse sees the same expression as `len`,
 * calls it partially redundant and adds a copy (`subu v0 / move t0,v0`). The two-step form came from the
 * permuter and may be a stand-in for what the source really had (`half = y0; half = (y1 - half) / 2;` works
 * too): what matters is that the subtraction's operand is a variable set twice. */
void BtlText_DrawScrollBar(void *pkt, s32 unused, s32 x, s32 y0, s32 y1) {
    s32 visible = 7;
    BtlTextWork *work = BtlMenu_GetWork();
    BtlTextList *list = &work->list[work->side];
    s32 h = y1 - y0;
    s32 half = h / 2;
    s32 count = list->count[list->page];
    s32 scroll = list->cursor[list->page];
    s32 over;
    s32 len;

    BtlText_DrawPart(pkt, x, y0, 9, h - half, 5);
    BtlText_DrawPart(pkt, x, y1, 9, -half, 5);
    y0 += 2;
    x += 2;
    y1 -= 2;
    over = visible < count;
    count--;
    if (count < 0) {
        count = 0;
    }
    len = y1 - y0;
    if (over) {
        f32 unit = (f32)len / (f32)(count + 7);

        y0 += (s32)((f32)scroll * unit);
        y1 = y0 + (s32)(unit * (f32)visible);
    }
    h = y0;
    h = y1 - h;
    half = h / 2;
    BtlText_DrawPart(pkt, x, y0 - 0x1B, 5, h - half + 0x1B, 6);
    BtlText_DrawPart(pkt, x, y1 + 0x1B, 5, -(half + 0x1B), 6);
}

/* Counts the pages and the entries of each page of the current side's list. */
void BtlText_CountEntries(void) {
    u16 *p;
    s32 go = 1;
    s32 page = -1;
    BtlTextWork *work = BtlMenu_GetWork();
    s32 side = work->side;
    BtlTextList *list = &work->list[side];
    s32 i;

    p = BtlMenu_GetScript();
    if (p == NULL) {
        return;
    }
    list->pages = 0;
    for (i = 9; i >= 0; i--) {
        list->count[i] = 0;
    }
    p += 1;
    while (go && *p != 0) {
        s32 tag;

        if (BtlText_CheckLineMask(&p, side) == 0) {
            p = BtlText_NextLine(p);
            continue;
        }
        BtlText_CheckUnlock(&p);
        tag = *p;
        if (tag == '$') {
            page = p[1] - '0';
        }
        switch (tag) {
        case '$':
            list->pages++;
            break;
        case '*':
            list->count[page]++;
            break;
        case '@':
            go = 0;
            break;
        }
        p = BtlText_NextLine(p);
    }
}

/* Returns the name of the n-th entry of the second script (8 characters into its line), NULL when it has none.
 * The bare `return;` is what the original does when there is no script: v0 is left as the NULL just returned. */
u16 *BtlText_FindEntry(s32 n) {
    u16 *p;
    s32 go = 1;
    u16 *ret = NULL;


    p = BtlMenu_GetScript2();
    if (p == NULL) {
        return;
    }
    p += 1;
    while (go && *p != 0) {
        p += 2;
        BtlText_CheckUnlock(&p);
        switch (*p) {
        case '*':
            if (n <= 0) {
                ret = p + 4;
                go = 0;
            }
            n--;
            break;
        case '@':
            go = 0;
            break;
        }
        p = BtlText_NextLine(p);
    }
    return ret;
}

/* Draws the name of entry n at (x, y); align 0 / 1 / other picks the font alignment 0 / 2 / 1. */
void BtlText_DrawEntryName(s32 x, s32 y, s32 n, s32 align, f32 alpha) {
    u16 *str = BtlText_FindEntry(n);

    if (str != NULL) {
        s32 font = Font_GetCmdCount();

        Font_PushStyle();
        Font_SetFlags(1);
        Font_SetScale(0.9f);
        Font_SetScaleXY(0.95f, 1.0f);
        Font_SetColorRGBA(0xFF, 0xFF, 0xFF, (u8)(u32)(alpha * 128.0f));
        Font_SetShadowMode(2);
        Font_SetShadowColorRGBA(0x20, 0x20, 0xFF, (u8)(u32)(alpha * 64.0f));
        switch (align) {
        case 0:
            Font_SetAlign(0);
            break;
        case 1:
            Font_SetAlign(2);
            break;
        default:
            Font_SetAlign(1);
            break;
        }
        Font_PrintAt(x, y, str);
        Font_PopStyle();
        Font_Flush(font);
    }
}

/* Battle sequence state machine, 0x216AC0-0x2187E0. See battle/btl_seq.h for the state list.
 *
 * Object boundaries, from .rodata: the tick table of BtlClock_Tick (0x2F1AD8) follows jtbl_002F1A80 of
 * BtlText_CheckLineMask with no padding, and the jump tables after it sit at +0x18 from it, so the text
 * functions and at least BtlClock_Tick were one object. With the file starting at BtlText_DrawPart (0x215420,
 * .rodata from jtbl_002F1A50) every table lands on its original address.
 *
 * Object boundaries, from code generation: a call to a function the compiler has already compiled in the same
 * file is treated differently by the delay-slot pass than a call to an unknown one (it looks through the call
 * when deciding whether a branch must be annulled). Two places depend on it:
 * - BtlText_DrawList needs BtlText_DrawPart known (same file, above it): matches as written.
 * - BtlSeq_CheckBattleEnd needs BtlSeq_TickClocks NOT known. So the state handlers and the win check
 *   (BtlSeqIntroTalk_Setup .. the end; their first jump table, 0x2F1AF0, is 16-byte aligned, which is where a
 *   new object's .rodata would start) were most likely a second source file, and this file should be split
 *   somewhere after BtlSeq_TickClocks (0x217090) and before BtlSeq_CheckBattleEnd (0x217EF0). Until then
 *   BtlSeq_CheckBattleEnd calls it through a second declaration of the same symbol. */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

/* Local views of things owned by other modules (their headers are still moving). */
typedef struct BtlSeqObj {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ s32 chara;  /* character id */
    /* 0x010 */ u8 unk10[0x93C - 0x10];
    /* 0x93C */ u8 *talkTbl; /* 4 bytes per opponent character id: {intro line or 0xFF, speaks second, win line or 0xFF, ?} */
} BtlSeqObj;

extern void BtlFacade_SetCtrl10D(s32 side);
extern BtlSeqObj *BtlObj_Get(s32 idx);
extern void BtlObj_SetSubState(BtlSeqObj *obj, s32 a, s32 line); /* mouth / talk animation */
extern void BtlCtrl_StartEntrance(s32 side);        /* character flag 0xEF: entrance pose */
extern void BtlCtrl_EndEntrance(s32 side);        /* character flag 0xF0: end of entrance */
extern void BtlCtrl_StartWinPose(s32 side);        /* character flag 0xF1: win pose */
extern void BtlCtrl_StartLosePose(s32 side);        /* character flag 0xF2: lose pose */
extern s32 BtlCtrl_IsPoseReached(s32 side);         /* pose reached */
extern void DemoCam_PlayObjAnim(s32 side, s32 cut); /* fighter camera cut */
extern void DemoCam_SetScaleHeight(s32 a);
extern s32 DemoCam_PlayStageAnim(s32 cut);          /* stage camera cut */
extern s32 DemoCam_IsActive(void);             /* camera cut still playing */
extern s32 DemoCam_IsInUse(void);
extern void DemoCam_Stop(void);            /* stop the camera cut */
extern void ScrXfade_RequestCapture(void);
extern void ScrXfade_Start(s32 a, f32 seconds);
extern s32 BtlScript_IsEventRunning(void);
extern void BtlScript_AbortEvents(void);
extern u8 *BtlScript_GetCurrentEvent(void);
extern void HudNotice_Show(s32 id);          /* HUD announcement */
extern s32 BtlCharApi_AnyHasFlag128(void);             /* any character has flag 0x128 */
extern s32 BtlCharApi_HasMemberUnk70(s32 side);
extern s32 BtlCtrl_TestFlag7(s32 side);         /* character flag 7 */
extern s32 BtlCtrl_IsTeamDead(s32 side);         /* every character of the side has no health */
extern s32 BtlPause_GetPadCount(void);
extern void BtlPause_SetPad(s32 pad);
extern s32 BtlPause_GetMenuPad(void);
extern s32 BtlPause_GetPad(void);
extern s32 PauseMenu_Update(s32 a, s32 b);     /* pause / result menu update */
#ifdef PORT
extern int Port_NetActive(void); /* port/src/gs/net.c: 1 in an online match */
extern int Port_NetMe(void);     /* which player this copy plays (0 or 1) */
extern s32 gPortSeSilent;        /* src/sys/snd.c */
extern void PauseMenu_Reset(void);
#endif
extern void PauseMenu_Draw(void);
extern void Snd_SetPause(s32 a, s32 b);
extern void Hud_ShowAll(s32 on);          /* HUD visibility bits */
extern void Hud_ShowGauges(s32 on);
extern void Hud_ShowTimer(s32 on);
extern void Hud_ShowNotice(s32 on);
extern void Hud_ShowCombo(s32 on);
extern void Hud_ShowPrompt(s32 on);
extern void Fade_Start(s32 idx, s32 dir, f32 seconds);


s32 BtlSeqIntroTalk_Setup(BtlSeqTalkCtx *ctx);
s32 BtlSeqWinTalk_Setup(BtlSeqTalkCtx *ctx);
s32 BtlSeq_CanDraw(void);
void BtlSeq_JudgeByHealth(BattleResult *result);
void BtlSeq_SetResultPad(void);
s32 BtlSeq_IsModeZero(void);

/* Returns the first state of a table that has an enter handler (0 when none has). */
s32 BtlSeq_FirstState(BtlSeqState *table) {
    s32 i;

    for (i = 0; i < BTL_SEQ_STATE_COUNT; i++) {
        if (table[i].enter != NULL) {
            return i;
        }
    }
    return 0;
}

/* Calls one handler of a state with the state's poll callback loaded into the context; -1 for an empty slot. */
s32 BtlSeq_Call(BtlSeqFunc func, s32 state) {
    if (func == NULL) {
        return -1;
    }
    gBtlSeq->ctx.poll = gBtlSeq->table[state].poll;
    return func(&gBtlSeq->ctx);
}

/* Clears the sequence, picks the state table for the battle mode and enters its first state. */
s32 BtlSeq_Reset(void) {
    memset(gBtlSeq, 0, sizeof(BtlSeq));
    BtlSeq_ResetClocks();
    switch (Battle_GetMode()) {
    case 1:
        gBtlSeq->table = gBtlSeqTblMode1;
        break;
    case 5:
    case 6:
    case 7:
        gBtlSeq->table = gBtlSeqTblMode5to7;
        break;
    default:
        gBtlSeq->table = gBtlSeqTblDefault;
        break;
    }
    gBtlSeq->state = BtlSeq_FirstState(gBtlSeq->table);
    Voice_Stop(0);
    Voice_Stop(1);
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].enter, gBtlSeq->state);
}

/* Allocates the sequence and starts it. */
s32 BtlSeq_Init(void) {
    gBtlSeq = Heap_Alloc(sizeof(BtlSeq), 0x20, 0, HEAP_ANY);
    memset(gBtlSeq, 0, sizeof(BtlSeq));
    return BtlSeq_Reset();
}

/* Frees the sequence. */
void BtlSeq_Term(void) {
    Heap_Free(gBtlSeq);
    gBtlSeq = NULL;
}

/* Runs the current state's preUpdate handler (before the battle simulation). */
s32 BtlSeq_PreUpdate(void) {
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].preUpdate, gBtlSeq->state);
}

/* Runs the current state's update handler and performs the state change it asks for; 1 = leave the battle. */
s32 BtlSeq_Update(void) {
    s32 next = BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].update, gBtlSeq->state);

    if (next != gBtlSeq->state) {
        BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].exit, gBtlSeq->state);
        gBtlSeq->state = next;
        if (gBtlSeq->state == BTL_SEQ_EXIT) {
            if (Battle_IsRematchRequested() || (Battle_GetMode() == 6 && BattleResult_IsReasonBit2())) {
                Battle_GetWork()->flags |= BATTLE_FLAG_RESTART;
                return 0;
            }
            return 1;
        }
        if (gBtlSeq->table[gBtlSeq->state].enter == NULL) {
            gBtlSeq->state = BTL_SEQ_END;
        }
        BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].enter, gBtlSeq->state);
    }
    return 0;
}

/* Returns the battle clock. */
BtlClock *BtlSeq_GetClock(void) {
    return &gBtlSeq->clock;
}

/* Returns the second clock. */
BtlClock *BtlSeq_GetSubClock(void) {
    return &gBtlSeq->subClock;
}

/* Returns the current state. */
s32 BtlSeq_GetState(void) {
    return gBtlSeq->state;
}

/* True while the fight itself is running (state 3). */
s32 BtlSeq_IsFighting(void) {
    return BtlSeq_GetState() == BTL_SEQ_FIGHT;
}

/* True during the finish announcement (state 4). */
s32 BtlSeq_IsFinish(void) {
    return BtlSeq_GetState() == BTL_SEQ_FINISH;
}

/* Runs the current state's extra handler (mode 1: skip the talk). */
s32 BtlSeq_CallExtra(void) {
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].extra, gBtlSeq->state);
}

/* Turns the battle-end check (and the clocks) off or on. */
void BtlSeq_SetEndCheckOff(s32 off) {
    gBtlSeq->endCheckOff = off;
}

/* Returns whether the battle-end check is off. */
s32 BtlSeq_GetEndCheckOff(void) {
    return gBtlSeq->endCheckOff;
}

/* Zeroes a clock. */
void BtlClock_Clear(BtlClock *clock) {
    memset(clock, 0, sizeof(BtlClock));
}

/* Advances a clock by one tick (34, 32, 34 ms in turn), stopping at 9:59:59.999. */
s32 BtlClock_Tick(BtlClock *clock) {
    s32 tickMs[3] = { 34, 32, 34 };

    s32 add = tickMs[clock->ticks++ % 3];

    clock->ms += add;
    if (clock->hours >= 9 && clock->minutes >= 59 && clock->seconds >= 59 && clock->ms >= 999) {
        clock->hours = 9;
        clock->minutes = 59;
        clock->seconds = 59;
        clock->ms = 999;
    } else {
        if (clock->ms >= 1000) {
            clock->ms -= 1000;
            clock->seconds++;
        }
        if (clock->seconds >= 60) {
            clock->seconds -= 60;
            clock->minutes++;
        }
        if (clock->minutes >= 60) {
            clock->minutes -= 60;
            clock->hours++;
        }
    }
    return 1;
}

/* Clears both clocks and loads the time limit into the battle clock. */
void BtlSeq_ResetClocks(void) {
    BtlClock *clock = BtlSeq_GetClock();

    BtlClock_Clear(clock);
    clock->timeLeft = Battle_GetTimeLimit();
    BtlClock_Clear(BtlSeq_GetSubClock());
}

/* Ticks both clocks and updates the time left; 1 when the time limit is reached. */
s32 BtlSeq_TickClocks(void) {
    BtlClock *clock;

    BtlSeq_GetClock();
    BtlClock_Tick(BtlSeq_GetSubClock());
    clock = BtlSeq_GetClock();
    BtlClock_Tick(clock);
    if (Battle_IsTimeLimitOff()) {
        return 0;
    }
    if (clock->minutes * 60 + clock->seconds >= Battle_GetTimeLimit()) {
        clock->timeLeft = 0;
        return 1;
    }
    clock->timeLeft = Battle_GetTimeLimit() - (clock->minutes * 60 + clock->seconds);
    return 0;
}

/* Restarts the second clock. */
void BtlSeq_ResetSubClock(void) {
    BtlClock_Clear(BtlSeq_GetSubClock());
}

/* Seconds left of the time limit, -1 when there is none. */
s32 BtlSeq_GetTimeLeft(void) {
    if (Battle_IsTimeLimitOff()) {
        return -1;
    }
    return BtlSeq_GetClock()->timeLeft;
}

/* Stops both fighters' talk animation and both voice players. */
void BtlSeq_StopTalk(void) {
    BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(0)), 0, -1);
    BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(1)), 0, -1);
    Voice_Stop(0);
    Voice_Stop(1);
}

/* Chooses the two entrance lines: the pair's special dialogue when both have a table entry, else random. */
s32 BtlSeqIntroTalk_Setup(BtlSeqTalkCtx *ctx) {
    BtlSeqObj *obj0 = BtlObj_Get(BattleSide_GetObjId(0));
    u8 *tbl0 = obj0->talkTbl;
    BtlSeqObj *obj1 = BtlObj_Get(BattleSide_GetObjId(1));
    u8 *tbl1 = obj1->talkTbl;

    if (tbl0 != NULL && tbl1 != NULL) {
        if (tbl0[obj1->chara * 4] != 0xFF) {
            if (tbl0[obj1->chara * 4 + 1] == 0) {
                ctx->side[0] = 0;
                ctx->side[1] = 1;
                ctx->chara[0] = obj0->chara;
                ctx->chara[1] = obj1->chara;
                ctx->line[0] = tbl0[obj1->chara * 4] + 6;
                ctx->line[1] = tbl1[obj0->chara * 4] + 6;
            } else {
                ctx->side[1] = 0;
                ctx->side[0] = 1;
                ctx->chara[0] = obj1->chara;
                ctx->chara[1] = obj0->chara;
                ctx->line[0] = tbl1[obj0->chara * 4] + 6;
                ctx->line[1] = tbl0[obj1->chara * 4] + 6;
            }
        } else {
            ctx->side[0] = 0;
            ctx->side[1] = 1;
            ctx->chara[0] = obj0->chara;
            ctx->chara[1] = obj1->chara;
            ctx->line[0] = rand() % 2;
            ctx->line[1] = rand() % 2;
        }
    } else {
        ctx->side[0] = 0;
        ctx->side[1] = 1;
        ctx->chara[0] = obj0->chara;
        ctx->chara[1] = obj1->chara;
        ctx->line[0] = rand() % 2;
        ctx->line[1] = rand() % 2;
    }
    ctx->step = 0;
    Ramp_Start(&ctx->timer, 10.0f, 0.0f, 1.0f);
    return 1;
}

/* State 1 enter: prepares the entrance lines; mode 1 has none and waits for the skip request. */
s32 BtlSeqIntroTalk_Enter(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() == 1) {
        BtlFacade_SetCtrl10D(0);
        BtlFacade_SetCtrl10D(1);
        ctx->skip = 0;
        ctx->step = 99;
    } else {
        BtlSeqIntroTalk_Setup(ctx);
    }
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    return 1;
}

/* State 1 preUpdate: at steps 0 and 2 starts a speaker's pose, camera cut, voice line and talk animation. */
s32 BtlSeqIntroTalk_PreUpdate(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
        BtlCtrl_StartEntrance(ctx->side[0]);
        Ramp_Start(&ctx->timer, 10.0f, 0.0f, 1.0f);
        DemoCam_PlayObjAnim(ctx->side[0], 0);
        Voice_PlayChara(ctx->side[0], ctx->chara[0], ctx->line[0]);
        BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(ctx->side[0])), 2, ctx->line[0]);
        ctx->step++;
        break;
    case 2:
        BtlCtrl_StartEntrance(ctx->side[1]);
        Ramp_Start(&ctx->timer, 10.0f, 0.0f, 1.0f);
        DemoCam_PlayObjAnim(ctx->side[1], 0);
        Voice_PlayChara(ctx->side[1], ctx->chara[1], ctx->line[1]);
        BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(ctx->side[1])), 2, ctx->line[1]);
        ctx->step++;
        break;
    case 3:
        break;
    }
    return 1;
}

/* State 1 update: waits for each line to end (voice stopped or 10 s), or for the skip; then goes to state 2. */
s32 BtlSeqIntroTalk_Update(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
        if (BtlCtrl_IsPoseReached(ctx->side[0])) {
            if (Voice_IsStopped(ctx->side[0])) {
                ScrXfade_RequestCapture();
                ScrXfade_Start(1, 1.0f);
                ctx->step++;
            } else if (Ramp_Step(&ctx->timer)) {
                ScrXfade_RequestCapture();
                ScrXfade_Start(1, 1.0f);
                ctx->step++;
            }
        }
        break;
    case 2:
    case 3:
        if (BtlCtrl_IsPoseReached(ctx->side[1])) {
            if (Voice_IsStopped(ctx->side[1]) || Ramp_Step(&ctx->timer)) {
                ScrXfade_RequestCapture();
                ScrXfade_Start(1, 1.0f);
                ctx->step++;
            }
        }
        break;
    case 4:
        return 2;
    case 99:
        if (ctx->skip) {
            ScrXfade_RequestCapture();
            ScrXfade_Start(1, 1.0f);
            goto done;
        }
        break;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
            if (ctx->step != 4) {
                ScrXfade_RequestCapture();
                ScrXfade_Start(1, 1.0f);
                if (BtlScript_IsEventRunning()) {
                    BtlScript_AbortEvents();
                }
            }
done:
            BtlSeq_StopTalk();
            return 2;
        }
    }
    return 1;
}

/* State 1 exit: stops the camera cut and ends both entrance poses (not in mode 1). */
s32 BtlSeqIntroTalk_Exit(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() != 1) {
        DemoCam_Stop();
        BtlCtrl_EndEntrance(ctx->side[0]);
        BtlCtrl_EndEntrance(ctx->side[1]);
    }
    return 1;
}

/* State 1 extra (mode 1): asks the state to end. */
s32 BtlSeqIntroTalk_Skip(BtlSeqTalkCtx *ctx) {
    s32 mode = Battle_GetMode();

    if (mode == 1) {
        ctx->skip = mode;
    }
    return 1;
}

/* Chooses the speaker, line and announcement of the winner scene. */
s32 BtlSeqWinTalk_Setup(BtlSeqTalkCtx *ctx) {
    s32 winner = BattleResult_GetWinnerSide();
    s32 loser = winner == 0;
    BtlSeqObj *winObj = BtlObj_Get(BattleSide_GetObjId(winner));
    u8 *winTbl = winObj->talkTbl;
    BtlSeqObj *loseObj = BtlObj_Get(BattleSide_GetObjId(loser));
    u8 *loseTbl = loseObj->talkTbl;

    if (BattleResult_IsPlayerWin()) {
        if (winTbl == NULL || loseTbl == NULL || BtlCharApi_HasMemberUnk70(winner) || BtlCharApi_HasMemberUnk70(loser)) {
            ctx->side[0] = winner;
            if (BtlCharApi_HasMemberUnk70(winner)) {
                ctx->chara[0] = 0x56;
            } else {
                ctx->chara[0] = winObj->chara;
            }
            ctx->line[0] = rand() % 2 + 3;
            ctx->step = 0;
        } else {
            if (winTbl[loseObj->chara * 4 + 2] != 0xFF) {
                ctx->side[0] = winner;
                ctx->chara[0] = winObj->chara;
                ctx->line[0] = winTbl[loseObj->chara * 4 + 2] + 0x1E;
                ctx->step = 0;
            } else {
                ctx->side[0] = winner;
                ctx->chara[0] = winObj->chara;
                ctx->line[0] = rand() % 2 + 3;
                ctx->step = 0;
            }
        }
        HudNotice_Show(6);
    } else {
        ctx->side[0] = loser;
        if (BtlCharApi_HasMemberUnk70(loser)) {
            ctx->chara[0] = 0x56;
        } else {
            ctx->chara[0] = loseObj->chara;
        }
        ctx->step = 0;
        ctx->line[0] = 0x2A;
        HudNotice_Show(7);
    }
    Ramp_Start(&ctx->timer, 10.0f, 0.0f, 1.0f);
    return 1;
}

/* State 5 enter: prepares the winner scene and hides HUD parts; mode 1 can be told to wait for the skip instead. */
s32 BtlSeqWinTalk_Enter(BtlSeqTalkCtx *ctx) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    ScrXfade_RequestCapture();
    ScrXfade_Start(1, 1.0f);
    if (Battle_GetMode() == 1) {
        if (*BtlScript_GetCurrentEvent() & 0x10) {
            ctx->skip = 0;
            ctx->step = 99;
        } else {
            BtlSeqWinTalk_Setup(ctx);
        }
        Hud_ShowGauges(0);
        Hud_ShowTimer(0);
        Hud_ShowCombo(0);
        Hud_ShowPrompt(0);
        Hud_ShowNotice(0);
    } else {
        BtlSeqWinTalk_Setup(ctx);
        Hud_ShowGauges(0);
        Hud_ShowTimer(0);
        Hud_ShowCombo(0);
        Hud_ShowPrompt(0);
    }
    return 1;
}

/* State 5 preUpdate: step 0 starts the pose, camera cut and voice line; step 2 starts a 1.5 s hold. */
s32 BtlSeqWinTalk_PreUpdate(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
        if (BattleResult_IsPlayerWin()) {
            BtlCtrl_StartWinPose(ctx->side[0]);
            DemoCam_PlayObjAnim(ctx->side[0], 1);
            DemoCam_SetScaleHeight(1);
        } else {
            BtlCtrl_StartLosePose(ctx->side[0]);
            DemoCam_PlayObjAnim(ctx->side[0], 2);
        }
        Ramp_Start(&ctx->timer, 10.0f, 0.0f, 1.0f);
        Voice_PlayChara(ctx->side[0], ctx->chara[0], ctx->line[0]);
        BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(ctx->side[0])), 2, ctx->line[0]);
        ctx->step++;
        break;
    case 2:
        Ramp_Start(&ctx->timer, 1.5f, 0.0f, 1.0f);
        ctx->step++;
        break;
    case 3:
        break;
    }
    return 5;
}

/* State 5 update: waits for the line (or 10 s), holds 1.5 s, then goes to state 6; the poll callback skips. */
s32 BtlSeqWinTalk_Update(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
        if (BtlCtrl_IsPoseReached(ctx->side[0])) {
            if (Voice_IsStopped(ctx->side[0]) || Ramp_Step(&ctx->timer)) {
                ctx->step = 2;
            }
        }
        break;
    case 2:
    case 3:
        if (Ramp_Step(&ctx->timer)) {
            ctx->step = 4;
        }
        break;
    case 4:
        return 6;
    case 99:
        if (ctx->skip) {
            goto done;
        }
        break;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
done:
            BtlSeq_StopTalk();
            return 6;
        }
    }
    return 5;
}

/* State 5 exit: stops the talk (not in mode 1). */
s32 BtlSeqWinTalk_Exit(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() != 1) {
        BtlSeq_StopTalk();
    }
    return 1;
}

/* State 5 extra (mode 1): asks the state to end. */
s32 BtlSeqWinTalk_Skip(BtlSeqTalkCtx *ctx) {
    s32 mode = Battle_GetMode();

    if (mode == 1) {
        ctx->skip = mode;
    }
    return 1;
}

/* State 0 enter: restarts the step counter and holds the fighters. */
s32 BtlSeqStageIntro_Enter(BtlSeqWaitCtx *ctx) {
    ctx->step = 0;
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    return 1;
}

/* State 0 preUpdate: at steps 0, 2 and 4 starts stage camera cut 0, 1 and 2. */
s32 BtlSeqStageIntro_PreUpdate(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        DemoCam_PlayStageAnim(0);
        ctx->step++;
        break;
    case 2:
        DemoCam_PlayStageAnim(1);
        ctx->step++;
        break;
    case 4:
        DemoCam_PlayStageAnim(2);
        ctx->step++;
        break;
    case 1:
    case 3:
    case 5:
    case 6:
        break;
    }
    return 0;
}

/* State 0 update: steps on when a camera cut ends; goes to state 1 after the third or on the poll callback. */
s32 BtlSeqStageIntro_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (!DemoCam_IsActive()) {
            ScrXfade_RequestCapture();
            ScrXfade_Start(1, 1.0f);
            ctx->step++;
        }
        break;
    case 4:
    case 5:
        if (!DemoCam_IsActive()) {
            ctx->step++;
            return 1;
        }
        break;
    case 6:
        return 1;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
            return 1;
        }
    }
    return 0;
}

/* State 0 exit: stops a camera cut that is still playing. */
s32 BtlSeqStageIntro_Exit(BtlSeqWaitCtx *ctx) {
    if (DemoCam_IsActive()) {
        DemoCam_Stop();
    }
    ScrXfade_RequestCapture();
    ScrXfade_Start(1, 1.0f);
    return 1;
}

/* True when the battle ended by time up in mode 0: the only case where equal health is a draw. */
s32 BtlSeq_CanDraw(void) {
    if (BattleResult_IsTimeUp() && Battle_GetMode() == 0) {
        return 1;
    }
    return 0;
}

/* Picks the winner from the two sides' health; equal health falls back to a rule flag, a draw, or rand(). */
void BtlSeq_JudgeByHealth(BattleResult *result) {
    if (result->health[0] == result->health[1]) {
        if (BattleResult_IsEvent3CSet(0)) {
            result->winner = BTL_RESULT_WIN_P1;
        } else if (BattleResult_IsEvent3CSet(1)) {
            result->winner = BTL_RESULT_WIN_P2;
        } else if (BtlSeq_CanDraw()) {
            result->winner = BTL_RESULT_DRAW;
        } else if (Battle_GetMode() == 8) {
            result->winner = BTL_RESULT_WIN_P2;
        } else if (rand() & 1) {
            result->winner = BTL_RESULT_WIN_P1;
        } else {
            result->winner = BTL_RESULT_WIN_P2;
        }
    } else if (result->health[1] < result->health[0]) {
        result->winner = BTL_RESULT_WIN_P1;
    } else {
        result->winner = BTL_RESULT_WIN_P2;
    }
}

/* Ticks the clocks and decides whether the battle is over, filling the result block; 1 when it is.
 *
 * Two things were needed to match:
 * - The function has to END with the last success path (`return 1` falling off the end) and every `return 0`
 *   has to be an early return. Then the compiler shares only the final `return 1` between the blocks and keeps
 *   the three "set reason + BtlSeq_JudgeByHealth" blocks separate; with `return 0` as the last statement it
 *   merges those three blocks into one.
 * - The call to BtlSeq_TickClocks() must look like a call to a function the compiler has not seen the body of
 *   (the branch before it is `bnezl` only then). In the original this function was therefore compiled without
 *   BtlSeq_TickClocks() defined above it, i.e. the clock code was most likely a separate source file. Until the
 *   file is split, the call goes through a second declaration of the same symbol. */
extern s32 BtlSeq_TickClocksExt(void) __asm__("BtlSeq_TickClocks");

s32 BtlSeq_CheckBattleEnd(void) {
    BattleResult *result = Battle_GetResult();
    s32 timeUp;

    if (result->winner & 0x1F) {
        return 1;
    }
    if (BtlSeq_GetEndCheckOff()) {
        return 0;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return 0;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return 0;
    }
    if (BtlCharApi_AnyHasFlag128()) {
        return 0;
    }
    timeUp = BtlSeq_TickClocksExt();
    if (Battle_GetMode() == 1) {
        return 0;
    }
    if (timeUp) {
        result->reason = BTL_REASON_TIME_UP;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (BtlCtrl_TestFlag7(0) && BtlCtrl_TestFlag7(1)) {
        result->reason = BTL_REASON_FLAG7;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (BtlCtrl_TestFlag7(1)) {
        result->winner = BTL_RESULT_WIN_P1;
        result->reason = BTL_REASON_FLAG7;
        return 1;
    }
    if (BtlCtrl_TestFlag7(0)) {
        result->winner = BTL_RESULT_WIN_P2;
        result->reason = BTL_REASON_FLAG7;
        return 1;
    }
    if (BtlCtrl_IsTeamDead(0) && BtlCtrl_IsTeamDead(1)) {
        result->reason = BTL_REASON_KO;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (BtlCtrl_IsTeamDead(1)) {
        result->winner = BTL_RESULT_WIN_P1;
        result->reason = BTL_REASON_KO;
        return 1;
    }
    if (BtlCtrl_IsTeamDead(0)) {
        result->winner = BTL_RESULT_WIN_P2;
        result->reason = BTL_REASON_KO;
        return 1;
    }
    if (!BattleReplay_IsActive()) {
        return 0;
    }
    if (!BattleReplay_TestDataFlag()) {
        return 0;
    }
    result->winner = BTL_RESULT_OTHER;
    result->reason = BTL_REASON_BIT18;
    return 1;
}

/* State 3 enter: releases the fighters (clears the demo and ready flags); mode 8 shows the HUD. */
s32 BtlSeqFight_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags &= ~BATTLE_FLAG_DEMO;
    Battle_GetWork()->flags &= ~BATTLE_FLAG_READY;
    if (Battle_GetMode() == 8) {
        Hud_ShowAll(1);
    }
    return 1;
}

/* State 3 preUpdate: nothing. */
s32 BtlSeqFight_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 3;
}

/* State 3 update: opens / runs the pause menu, then checks for the end of the battle (-> 4, or 6 without a finish scene). */
s32 BtlSeqFight_Update(BtlSeqWaitCtx *ctx) {
    if (Battle_GetMode() != 8) {
        if (ctx->poll != NULL && !(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) && ctx->poll()) {
            if (Battle_GetMode() == 7) {
                BattleResult_Set(BTL_RESULT_ABORT, 0);
            } else if (BattleReplay_IsActive() && BattleReplay_IsLoaded() && BtlPause_GetPad() == 1) {
            } else if (Battle_GetMode() == 1 && BtlScript_IsEventRunning()) {
                BtlScript_AbortEvents();
            } else {
#ifdef PORT
                /* Online the pause menu does not stop the fight (a player could hold the other in a pause for as
                   long as they liked): the menu opens for the one who pressed START, the fight, its clock and its
                   sound go on. That player's fighter takes no buttons meanwhile (BtlInput_Sample) and the menu is
                   drawn on that player's screen only (PauseMenu_Draw). */
                if (Port_NetActive()) {
                    Battle_GetWork()->flags |= BATTLE_FLAG_PAUSE_MENU;
                } else
#endif
                {
                    Battle_GetWork()->flags |= BATTLE_FLAG_PAUSE_MENU | BATTLE_FLAG_PAUSE;
                    Adx_PauseSeVoice();
                    Snd_SetPause(4, 1);
                }
            }
        }
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) {
#ifdef PORT
            if (Port_NetActive()) {
                gPortSeSilent = BtlPause_GetMenuPad() != Port_NetMe(); /* (its sounds are for its owner) */
                if (PauseMenu_Update(BtlPause_GetMenuPad(), 0) == 0) {
                    Battle_GetWork()->flags &= ~BATTLE_FLAG_PAUSE_MENU;
                }
                gPortSeSilent = 0;
            } else
#endif
            if (PauseMenu_Update(BtlPause_GetMenuPad(), 0) == 0) {
                Battle_GetWork()->flags &= ~(BATTLE_FLAG_PAUSE_MENU | BATTLE_FLAG_PAUSE);
                Snd_SetPause(4, 0);
            }
        }
    }
    if (BtlSeq_CheckBattleEnd()) {
#ifdef PORT
        /* Online the fight can end under an open pause menu (it does not stop for it): the menu is closed, unless
           it is what ended the battle (as in the original, where it stays up through the fade). */
        if (Port_NetActive() && (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) && !BattleResult_IsAborted()) {
            PauseMenu_Reset();
            Battle_GetWork()->flags &= ~BATTLE_FLAG_PAUSE_MENU;
        }
#endif
        if (Battle_GetMode() == 7) {
            return 6;
        }
        return BattleResult_IsAborted() ? 6 : 4;
    }
    return 3;
}

/* State 3 exit: nothing. */
s32 BtlSeqFight_Exit(BtlSeqWaitCtx *ctx) {
    return 1;
}

/* State 2 enter: holds the fighters, starts the 0.8 s wait and shows the HUD. */
s32 BtlSeqReady_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    ctx->step = 0;
    Ramp_Start(&ctx->timer, 0.8f, 0.0f, 1.0f);
    Hud_ShowAll(1);
    return 1;
}

/* State 2 preUpdate: nothing. */
s32 BtlSeqReady_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 2;
}

/* State 2 update: after 0.8 s shows announcement 0 and releases the fighters; 2 s later goes to state 3. */
s32 BtlSeqReady_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        if (Ramp_Step(&ctx->timer)) {
            HudNotice_Show(0);
            Battle_GetWork()->flags &= ~BATTLE_FLAG_DEMO;
            Battle_GetWork()->flags |= BATTLE_FLAG_READY;
            Ramp_Start(&ctx->timer, 2.0f, 0.0f, 1.0f);
            ctx->step++;
        }
        break;
    case 1:
        if (Ramp_Step(&ctx->timer)) {
            return 3;
        }
        break;
    }
    return 2;
}

/* State 2 exit: announcement 1. */
s32 BtlSeqReady_Exit(BtlSeqWaitCtx *ctx) {
    HudNotice_Show(1);
    return 1;
}

/* Gives the result menu to pad 0 when one pad plays, else to the winning side. */
void BtlSeq_SetResultPad(void) {
    if (BtlPause_GetPadCount() == 1) {
        BtlPause_SetPad(0);
    } else {
        BtlPause_SetPad(BattleResult_GetWinnerSide());
    }
}

/* True in battle mode 0. */
s32 BtlSeq_IsModeZero(void) {
    return Battle_GetMode() == 0;
}

/* State 6 enter: mode 0 with a finished battle opens the result menu (step 0); otherwise starts the 1.2 s fade out (step 1). */
s32 BtlSeqEnd_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    if (BattleResult_IsAborted()) {
        Fade_Start(0, 0, 1.0f);
        Ramp_Start(&ctx->timer, 1.2f, 0.0f, 1.0f);
        ctx->step = 1;
    } else if (BtlSeq_IsModeZero()) {
        ctx->step = 0;
        BtlSeq_SetResultPad();
    } else {
        Fade_Start(0, 0, 1.0f);
        Ramp_Start(&ctx->timer, 1.2f, 0.0f, 1.0f);
        ctx->step = 1;
    }
    return 1;
}

/* State 6 preUpdate: nothing. */
s32 BtlSeqEnd_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 6;
}

/* State 6 update: step 0 runs the result menu until it aborts the battle, step 1 waits for the fade and returns 99. */
s32 BtlSeqEnd_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        PauseMenu_Update(BtlPause_GetMenuPad(), 1);
        PauseMenu_Draw();
        if (BattleResult_IsAborted()) {
            Fade_Start(0, 0, 1.0f);
            Ramp_Start(&ctx->timer, 1.2f, 0.0f, 1.0f);
            ctx->step = 1;
        }
        break;
    case 1:
        if (Ramp_Step(&ctx->timer)) {
            return BTL_SEQ_EXIT;
        }
        break;
    }
    return 6;
}

/* State 6 exit: stops the camera cut. */
s32 BtlSeqEnd_Exit(BtlSeqWaitCtx *ctx) {
    if (DemoCam_IsInUse()) {
        DemoCam_Stop();
    }
    DemoCam_SetScaleHeight(0);
    return 1;
}

/* State 4 enter: holds the fighters, starts the 3.5 s wait and shows the announcement for the finish reason. */
s32 BtlSeqFinish_Enter(BtlSeqWaitCtx *ctx) {
    BtlSeqTimer *timer;

    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    timer = &ctx->timer;
    Ramp_Start(timer, 3.5f, 0.0f, 1.0f);
    if (BattleResult_IsKo()) {
        if (BattleResult_IsWinnerEvent59Clear()) {
            HudNotice_Show(3);
        } else {
            HudNotice_Show(2);
        }
    } else if (BattleResult_IsTimeUp()) {
        HudNotice_Show(5);
    } else if (BattleResult_IsReasonBit2()) {
        HudNotice_Show(4);
    } else if (BattleResult_IsReasonBit18()) {
        Ramp_Start(timer, 1.1f, 0.0f, 1.0f);
    }
    return 1;
}

/* State 4 preUpdate: nothing. */
s32 BtlSeqFinish_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 4;
}

/* State 4 update: when the wait ends goes to the winner scene (5) if a side won and the mode is not 8, else to 6. */
s32 BtlSeqFinish_Update(BtlSeqWaitCtx *ctx) {
    if (Ramp_Step(&ctx->timer)) {
        if (Battle_GetMode() != 8 && BattleResult_HasWinner()) {
            return 5;
        }
        return 6;
    }
    return 4;
}

/* State 4 exit: nothing. */
s32 BtlSeqFinish_Exit(BtlSeqWaitCtx *ctx) {
    return 1;
}
