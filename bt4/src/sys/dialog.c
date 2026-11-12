#include "common.h"
#include "sys/heap.h"
#include "sys/pad.h"
#include "sys/dialog.h"
#ifdef PORT
extern int gPortMenuMode;                 /* headless.c: the menus are running */
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
#endif

typedef struct DialogProgress {
    /* 0x00 */ u8 unk0[0x14];
    /* 0x14 */ s32 flags; /* Progress.flags: bit 0x100 blocks dialog input */
} DialogProgress;

typedef struct DialogRect {
    s32 x0, y0, x1, y1;
} DialogRect;

typedef struct DialogResHdr {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ u8 *data;
} DialogResHdr;

extern DialogProgress *gProgress;

/* The one window; NULL while none exists. Defined here: it is this object's .sdata (0x2FF160). */
Dialog *gDialog = NULL;

extern void *memset(void *dst, s32 value, u32 size);
extern s32 sprintf(char *dst, const char *fmt, ...);
extern void Snd_PlaySe(u32 mask, s32 id);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Flash_Create(DialogFlash *obj, void *data, DialogFlashRes *res);
extern void Flash_FindLabel(DialogFlash *obj, char *parent, char *name, DialogFlashRef *out);
extern void Flash_Destroy(DialogFlash *obj);                               /* destroy */
extern void Flash_Advance(DialogFlash *obj);                               /* per frame: advance */
extern void Flash_Draw(DialogFlash *obj);                               /* per frame: draw */
extern void Flash_Play(DialogFlash *obj, s32 a);
extern void Flash_GotoLabel(DialogFlash *obj, char *label, s32 a);           /* play a root label */
extern void Flash_ClipGotoLabel(DialogFlash *obj, DialogFlashRef *clip, const char *label); /* play a clip's label */
extern void Flash_ClipSetFlags(DialogFlash *obj, DialogFlashRef *clip, s32 a, s32 visible);
extern void Flash_ClipSetUv(DialogFlash *obj, DialogFlashRef *clip, DialogRect *rect);
extern void Flash_ClipGetPos(DialogFlash *obj, DialogFlashRef *clip, s32 *x, s32 *y);
extern f32 Flash_ClipGetAlpha(DialogFlash *obj, DialogFlashRef *clip);          /* clip alpha, 0..1 */
extern void Font_FlushAll(void);
extern s32 Font_GetWidth(u16 *str);
extern s32 Font_CountLines(u16 *str);
extern void Font_PrintAt(s32 x, s32 y, u16 *str);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetScaleXY(f32 sx, f32 sy);
extern void Font_SetAlign(s32 align);
extern void Font_SetShadowMode(s32 mode);
extern void Font_SetShadowColor(u32 color);
extern void Font_SetShadowOffset(s32 x, s32 y);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
#ifdef PORT
extern const u16 *UiText_TranslateLine(const u32 *table, int line, const u16 *original);
#define DIALOG_TRANSLATE(tbl, i, str) ((u16 *)UiText_TranslateLine((const u32 *)(tbl), (i), (str)))
#else
#define DIALOG_TRANSLATE(tbl, i, str) (str)
#endif

#define DIALOG_TEXT(tbl, i) ((u16 *)((u8 *)(tbl) + ((tbl)[(i) + 1] / 4 * 4)))
#define DIALOG_PART(file, i) ((DialogResHdr *)((u8 *)(file) + ((file)[i] / 4 * 4)))

#ifdef PORT
extern int Port_GameProfileMenuActive(void);
static u16 *sChoiceLabels[2];
void Dialog_SetChoiceLabels(u16 *first,u16 *second){
    DialogFlashRef ref;char name[64];s32 i;
    sChoiceLabels[0]=first;sChoiceLabels[1]=second;
    if(!gDialog)return;
    for(i=0;i<2;i++){
        sprintf(name,"mc_menu_plate_%d",i+1);
        Flash_FindLabel(gDialog->flash,name,"mc_menu_text_off",&ref);
        if(ref.id>=0)Flash_ClipSetFlags(gDialog->flash,&ref,2,sChoiceLabels[i]?0:1);
        Flash_FindLabel(gDialog->flash,name,"mc_menu_text_on",&ref);
        if(ref.id>=0)Flash_ClipSetFlags(gDialog->flash,&ref,2,sChoiceLabels[i]?0:1);
    }
}
static void Dialog_DrawChoiceLabels(void){
    DialogFlashRef ref;s32 i,x,y;f32 alpha;char name[64];
    if(Port_GameProfileMenuActive())return; /* crisp text is drawn at the window's resolution */
    for(i=0;i<2;i++)if(sChoiceLabels[i]){
        sprintf(name,"mc_menu_plate_%d",i+1);Flash_FindLabel(gDialog->flash,NULL,name,&ref);
        if(ref.id<0)continue;
        Flash_ClipGetPos(gDialog->flash,&ref,&x,&y);alpha=Flash_ClipGetAlpha(gDialog->flash,&ref);
        Font_PushStyle();Font_SetAlign(1);Font_SetScaleXY(1.0f,1.0f);
        Font_SetShadowMode(2);Font_SetShadowOffset(1,2);Font_SetShadowColor(0x60202020);
        Font_SetColorRGBA(255,255,255,(s32)(alpha*128.0f));Font_PrintAt(x+64,y+8,sChoiceLabels[i]);Font_PopStyle();
    }
}
#endif

/* Layout 0: prints text msgIdx of the caller's table, centred on the anchor clip and shifted down by its line count. */
void Dialog_DrawText(void) {
    DialogFlashRef ref;
    s32 x;
    s32 y;
    DialogFlash *flash;
    u16 *str;
    f32 alpha;
    s32 lines;

    if (gDialog->msgTbl == NULL) {
        return;
    }
    if (gDialog->msgIdx < 0) {
        return;
    }
    flash = gDialog->flash;
    switch (gDialog->size) {
    case 0:
        Flash_FindLabel(flash, NULL, "mc_dummy_text_1", &ref);
        break;
    case 1:
        Flash_FindLabel(flash, NULL, "mc_dummy_text_4", &ref);
        break;
    }
    if (ref.id < 0) {
        return;
    }
    Flash_ClipGetPos(flash, &ref, &x, &y);
    alpha = Flash_ClipGetAlpha(flash, &ref);
    str = DIALOG_TRANSLATE(gDialog->msgTbl, gDialog->msgIdx, DIALOG_TEXT(gDialog->msgTbl, gDialog->msgIdx));
    lines = Font_CountLines(str);
    switch (gDialog->size) {
    case 0:
        switch (lines) {
        case 1:
            y += 0x41;
            break;
        case 2:
            y += 0x34;
            break;
        case 3:
            y += 0x27;
            break;
        case 4:
            y += 0x1A;
            break;
        case 5:
            y += 0xD;
            break;
        case 6:
            break;
        }
        break;
    case 1:
        switch (lines) {
        case 6:
            y += 0x1A;
            break;
        case 7:
            y += 0xD;
            break;
        case 8:
            break;
        }
        break;
    }
    Font_PushStyle();
    Font_SetAlign(1);
    Font_SetScaleXY(1.0f, 1.0f);
    Font_SetColorRGBA(0xFF, 0xFF, 0xFF, (u8)(u32)(alpha * 128.0f));
    Font_SetShadowMode(2);
    Font_SetShadowOffset(1, 2);
    Font_SetShadowColor(((u32)(alpha * 64.0f) << 24) | 0x202020);
    Font_PrintAt(x, y, str);
    Font_PopStyle();
}

/* Layout 1: prints heading titleIdx of the file's own table at anchor mc_dummy_text_2. */
void Dialog_DrawTitle(void) {
    DialogFlashRef ref;
    s32 x;
    s32 y;
    DialogFlash *flash;
    u16 *str;
    f32 alpha;

    if (gDialog->titleTbl == NULL) {
        return;
    }
    if (gDialog->titleIdx < 0) {
        return;
    }
    flash = gDialog->flash;
    Flash_FindLabel(flash, NULL, "mc_dummy_text_2", &ref);
    if (ref.id < 0) {
        return;
    }
    Flash_ClipGetPos(flash, &ref, &x, &y);
    alpha = Flash_ClipGetAlpha(flash, &ref);
    str = DIALOG_TRANSLATE(gDialog->titleTbl, gDialog->titleIdx, DIALOG_TEXT(gDialog->titleTbl, gDialog->titleIdx));
    Font_PushStyle();
    Font_SetAlign(1);
    Font_SetScaleXY(1.0f, 1.0f);
    Font_SetColorRGBA(0xFF, 0xFF, 0xFF, (u8)(u32)(alpha * 128.0f));
    Font_SetShadowMode(2);
    Font_SetShadowOffset(1, 2);
    Font_SetShadowColor(((u32)(alpha * 64.0f) << 24) | 0x202020);
    Font_PrintAt(x, y, str);
    Font_PopStyle();
}

/* Layout 1: prints body text msgIdx at anchor mc_dummy_text_3, shifted down by its line count. */
void Dialog_DrawBody(void) {
    DialogFlashRef ref;
    s32 x;
    s32 y;
    DialogFlash *flash;
    u16 *str;
    f32 alpha;
    f32 scale;

    if (gDialog->titleTbl == NULL) {
        return;
    }
    if (gDialog->msgIdx < 0) {
        return;
    }
    flash = gDialog->flash;
    Flash_FindLabel(flash, NULL, "mc_dummy_text_3", &ref);
    if (ref.id < 0) {
        return;
    }
    Flash_ClipGetPos(flash, &ref, &x, &y);
    alpha = Flash_ClipGetAlpha(flash, &ref);
    str = DIALOG_TRANSLATE(gDialog->titleTbl, gDialog->msgIdx, DIALOG_TEXT(gDialog->titleTbl, gDialog->msgIdx));
    switch (Font_CountLines(str)) {
    case 1:
        y += 0x28;
        break;
    case 2:
        y += 0x20;
        break;
    case 3:
        y += 0x14;
        break;
    case 4:
        y += 0xA;
        break;
    case 5:
        y += 4;
        break;
    case 6:
        break;
    }
    scale = 1.0f;
    Font_GetWidth(str);
    Font_PushStyle();
    Font_SetAlign(1);
    Font_SetScaleXY(scale, scale);
    Font_SetColorRGBA(0xFF, 0xFF, 0xFF, (u8)(u32)(alpha * 128.0f));
    Font_SetShadowMode(2);
    Font_SetShadowOffset(1, 2);
    Font_SetShadowColor(((u32)(alpha * 64.0f) << 24) | 0x202020);
    Font_PrintAt(x, y, str);
    Font_PopStyle();
}

/* Plays `label` on the plate of the highlighted choice (clip mc_menu_plate_<cursor + 1>). */
void Dialog_PlayCursorPlate(s32 flash, const char *label) {
    DialogFlashRef ref;
    char name[64];
    DialogFlash *obj = &gDialog->flash[flash];

    sprintf(name, "mc_menu_plate_%d", gDialog->cursor + 1);
    Flash_FindLabel(obj, NULL, name, &ref);
    Flash_ClipGotoLabel(obj, &ref, label);
}

/* Allocates the window and builds its animation from the resource file; msgTbl is the caller's text table. */
void Dialog_Init(u32 *file, u32 *msgTbl, s32 size) {
    DialogResHdr *hdr = NULL;

    gDialog = Heap_Alloc(sizeof(Dialog), 0x20, 0, 2);
    memset(gDialog, 0, sizeof(Dialog));
    hdr = DIALOG_PART(file, 1);
    Res_RelocateOffsets(&hdr, hdr, hdr);
    gDialog->res.tex0 = hdr->data;
    gDialog->tex0 = hdr->data;
    hdr = DIALOG_PART(file, 4);
    Res_RelocateOffsets(&hdr, hdr, hdr);
    gDialog->res.tex1 = hdr->data;
    gDialog->res.clut1 = hdr->data + 0x40;
    hdr = DIALOG_PART(file, 2);
    Res_RelocateOffsets(&hdr, hdr, hdr);
    gDialog->res.tex2 = hdr->data;
    gDialog->res.clut2 = hdr->data + 0x40;
    hdr = DIALOG_PART(file, 3);
    Res_RelocateOffsets(&hdr, hdr, hdr);
    gDialog->res.tex3 = hdr->data;
    gDialog->res.clut3 = hdr->data + 0x40;
    Flash_Create(gDialog->flash, DIALOG_PART(file, 5), &gDialog->res);
    Flash_Play(gDialog->flash, 1);
    gDialog->titleTbl = (u32 *)DIALOG_PART(file, 6);
    gDialog->msgTbl = msgTbl;
    gDialog->msgIdx = -1;
    gDialog->titleIdx = -1;
    gDialog->port = 0;
    gDialog->size = size;
#ifdef PORT
    sChoiceLabels[0]=sChoiceLabels[1]=NULL;
#endif
}

/* Destroys the animation and frees the window. */
void Dialog_Term(void) {
    Flash_Destroy(gDialog->flash);
    if (gDialog != NULL) {
        Heap_Free(gDialog);
        gDialog = NULL;
    }
}


/* Per frame: advances the animation, shows / hides the choice plates, prints the texts and draws the window. */
void Dialog_Draw(s32 visible) {
    DialogFlashRef ref;
    DialogRect rect;
    DialogRect unused;
    char name[64];
    DialogFlash *flash;
    s32 i;

    Flash_Advance(gDialog->flash);
    if ((gDialog->flags & DIALOG_FLAG_CLOSING) && (gDialog->flash[0].flags & 8)) {
        gDialog->flags |= DIALOG_FLAG_CLOSED;
    }
    if (!(gDialog->flags & DIALOG_FLAG_CURSOR_ON) && (gDialog->flash[0].flags & 2)) {
        Dialog_PlayCursorPlate(0, "fl_on_start");
        gDialog->flags |= DIALOG_FLAG_CURSOR_ON;
    }
    flash = gDialog->flash;
    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 1);
        if (gDialog->choices != 0) {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
rect.y0 = i * 0x20;
            rect.x0 = 0;
            rect.x1 = 0x80;
            rect.y1 = i * 0x20 + 0x20;
            Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
            Flash_ClipSetUv(flash, &ref, &rect);
            Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
            Flash_ClipSetUv(flash, &ref, &rect);
        } else {
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    switch (gDialog->layout) {
    case 0:
        Dialog_DrawText();
        break;
    case 1:
        Dialog_DrawTitle();
        Dialog_DrawBody();
        break;
    }
    gDialog->res.tex0 = visible != 0 ? gDialog->tex0 : NULL;
#ifdef PORT
    if(sChoiceLabels[0]||sChoiceLabels[1])Dialog_SetChoiceLabels(sChoiceLabels[0],sChoiceLabels[1]);
    if (gPortMenuMode) {
        Port_MenuPage(50); /* widescreen menus: the dialog a page of its own, at its true shape (port/src/gs_marker.c) */
    }
#endif
    Flash_Draw(gDialog->flash);
#ifdef PORT
    Dialog_DrawChoiceLabels();
#endif
    Font_FlushAll();
#ifdef PORT
    if (gPortMenuMode) {
        Port_MenuPage(0);
    }
#endif
}

/* Opens (0), closes (1) or shows without animation-in (2) the window; open and close play SE 4 / 5. */
void Dialog_Start(s32 cmd) {
    switch (cmd) {
    case DIALOG_CMD_OPEN:
        if (gDialog->flags & DIALOG_FLAG_OPEN) {
            return;
        }
        gDialog->flags = DIALOG_FLAG_OPEN;
        gDialog->cursor = gDialog->defCursor;
        switch (gDialog->size) {
        case 0:
            Flash_GotoLabel(gDialog->flash, "fl_window_s_in", 1);
            break;
        case 1:
            Flash_GotoLabel(gDialog->flash, "fl_window_l_in", 1);
            break;
        }
        Snd_PlaySe(1, 4);
        return;
    case DIALOG_CMD_CLOSE:
        if (gDialog->flags & DIALOG_FLAG_CLOSING) {
            return;
        }
        gDialog->flags |= DIALOG_FLAG_CLOSING;
        gDialog->flags &= ~DIALOG_FLAG_OPEN;
        switch (gDialog->size) {
        case 0:
            Flash_GotoLabel(gDialog->flash, "fl_window_s_out", 1);
            break;
        case 1:
            Flash_GotoLabel(gDialog->flash, "fl_window_l_out", 1);
            break;
        }
        Snd_PlaySe(1, 5);
        return;
    case DIALOG_CMD_SHOW:
        gDialog->flags = DIALOG_FLAG_OPEN;
        gDialog->cursor = gDialog->defCursor;
        switch (gDialog->size) {
        case 0:
            Flash_GotoLabel(gDialog->flash, "fl_window_s_open", 1);
            return;
        case 1:
            Flash_GotoLabel(gDialog->flash, "fl_window_l_open", 1);
            return;
        }
        return;
    }
}


/*
 * Reads the operating controller: game-button repeat bits 1 / 2 move the cursor (wrapping, SE 0),
 * pressed bit 0x200 confirms (SE 1; returns 1 for choice 0, -2 for choice 1), pressed bit 0x400
 * cancels when allowed (SE 2; returns -1). Returns 0 otherwise.
 */
s32 Dialog_Input(s32 allowCancel) {
    s32 result = DIALOG_RESULT_NONE;

    if (gProgress->flags & 0x100) {
        return 0;
    }
    if (!(gDialog->flash[0].flags & 2)) {
        return 0;
    }
    if (gDialog->choices == 0) {
        return 0;
    }
    if (gPad[gDialog->port].gameRepeat & 1) {
        Dialog_PlayCursorPlate(0, "fl_off_start");
        gDialog->cursor--;
        if (gDialog->cursor < 0) {
            gDialog->cursor = 1;
        }
        Dialog_PlayCursorPlate(0, "fl_on_start");
        Snd_PlaySe(1, 0);
    } else if (gPad[gDialog->port].gameRepeat & 2) {
        Dialog_PlayCursorPlate(0, "fl_off_start");
        gDialog->cursor++;
        if (gDialog->cursor >= 2) {
            gDialog->cursor = 0;
        }
        Dialog_PlayCursorPlate(0, "fl_on_start");
        Snd_PlaySe(1, 0);
    } else if (gPad[gDialog->port].gamePressed & 0x200) {
        switch (gDialog->cursor) {
        case 0:
            result = DIALOG_RESULT_FIRST;
            gDialog->flags |= DIALOG_FLAG_DECIDED;
            break;
        case 1:
            result = DIALOG_RESULT_SECOND;
            gDialog->flags |= DIALOG_FLAG_DECIDED;
            break;
        }
        Dialog_PlayCursorPlate(0, "fl_ok");
        Snd_PlaySe(1, 1);
    } else if (gPad[gDialog->port].gamePressed & 0x400) {
        if (allowCancel != 0) {
            Dialog_PlayCursorPlate(0, "fl_off_start");
            result = DIALOG_RESULT_CANCEL;
            Snd_PlaySe(1, 2);
        }
    }
    return result;
}

/* Turns the two choice plates (and input) on or off. */
void Dialog_SetChoices(s32 on) {
    gDialog->choices = on;
}

/* Selects the controller port that operates the window. */
void Dialog_SetPort(s32 port) {
    gDialog->port = port;
}

/* Selects the text layout: 0 = one text, 1 = heading + body. */
void Dialog_SetLayout(s32 layout) {
    gDialog->layout = layout;
}

/* Replaces the caller's message table. */
void Dialog_SetMsgTable(u32 *tbl) {
    gDialog->msgTbl = tbl;
}

/* Selects the text to show (< 0: none). */
void Dialog_SetMsg(s32 idx) {
    gDialog->msgIdx = idx;
}

/* Selects the heading to show in layout 1 (< 0: none). */
void Dialog_SetTitle(s32 idx) {
    gDialog->titleIdx = idx;
}

/*
 * Puts the cursor on a choice now and makes it the choice the window opens on: when the cursor is on the
 * other choice its plate plays fl_off_start first; then cursor = defCursor = choice and the plate plays
 * fl_on_start.
 *
 * The `else if` is dead code and is what makes this match: its body is deleted as dead, but the empty branch
 * survives until after register allocation, so the old cursor is live in two blocks and is allocated after
 * gDialog (gDialog in v1, the old cursor in a1). What the original had there is unknown (something compiled
 * out); only a body the early jump pass cannot remove works, such as a signed division by a power of two of
 * a variable that is used elsewhere. The comparison with defCursor is one of several that give the right
 * allocation priority, not something the code shows.
 */
void Dialog_SetCursor(s32 choice) {
    s32 old = gDialog->cursor;

    if (old == (choice ^ 1)) {
        Dialog_PlayCursorPlate(0, "fl_off_start");
    } else if (old != gDialog->defCursor) {
        old /= 2;
    }
    gDialog->cursor = gDialog->defCursor = choice;
    Dialog_PlayCursorPlate(0, "fl_on_start");
}

/* 1 once the close animation has finished. */
s32 Dialog_IsClosed(void) {
    return (gDialog->flags >> 1) & 1;
}

/* 1 while the window is fully open (the animation's flag bit 1). */
s32 Dialog_IsOpen(void) {
    return (gDialog->flash[0].flags >> 1) & 1;
}

/* The message table inside the window's resource file. */
u32 *Dialog_GetTitleTable(void) {
    return gDialog->titleTbl;
}
