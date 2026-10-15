#include "common.h"
#include "battle/view_a.h"
#include "sys/heap.h"

/*
 * MsgWin, 0x25CFC0..0x25D290: a message window that slides in from the left or the right and shows one line
 * of a text file ("mc_message_text"). Used all over the menu overlay. See battle/view_a.h.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Flash_Create(Flash *flash, void *data, void *tex);
extern void Flash_Destroy(Flash *flash);
extern void Flash_Advance(Flash *flash);
extern void Flash_Draw(Flash *flash);
extern void Flash_Play(Flash *flash, s32 arg);
extern void Flash_GotoLabel(Flash *flash, char *label, s32 arg);
extern void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out);
extern void Font_FlushAll(void);

/* text box module, after 0x2600B0 (not decompiled) */
extern void TextBox_AttachLine(Flash *flash, FlashRef *ref, s32 a, s32 b, s32 line, TextBox *box);
extern void TextBox_SetSpacing(TextBox *box, s32 a, s32 b);

/* Defined here: this object's .sdata (0x2FF0D8). */
MsgWin *gMsgWin = NULL;

/* Allocates the message window: movie and textures from the pack, text box preset 5 on `text`. */
void MsgWin_Init(u32 *pack, void *text, s32 side) {
    FlashTexRes *res = NULL;

    gMsgWin = Heap_Alloc(sizeof(MsgWin), 0x20, 0, HEAP_ANY);
    memset(gMsgWin, 0, sizeof(MsgWin));

    res = PACK_AT(pack, 1);
    Res_RelocateOffsets(&res, res, res);
    gMsgWin->tex[0] = res->tex;
    gMsgWin->tex[1] = res->tex + FLASH_TEX_SIZE;

    Flash_Create(gMsgWin->flash, PACK_AT(pack, 2), gMsgWin->tex);
    Flash_Play(gMsgWin->flash, 1);
    TextBox_Init(&gMsgWin->box, text, 5);
    gMsgWin->text = text;
    gMsgWin->side = side;
}

/* Destroys the movie and frees the window. */
void MsgWin_Term(void) {
    Flash_Destroy(gMsgWin->flash);
    if (gMsgWin != NULL) {
        Heap_Free(gMsgWin);
        gMsgWin = NULL;
    }
}

/* Steps and draws the window with one line of its text file. The first two arguments are not used. */
void MsgWin_Draw(s32 unused0, s32 unused1, s32 line) {
    FlashRef ref;
    Flash *flash;
    s32 i;

    Flash_Advance(gMsgWin->flash);
    flash = gMsgWin->flash;
    if (gMsgWin->text != NULL) {
        Flash_FindLabel(flash, NULL, "mc_message_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, line, &gMsgWin->box);
    }
    for (i = 0; i < 1; i++) {
        Flash_Draw(&gMsgWin->flash[i]);
    }
    Font_FlushAll();
}

/* Starts the slide-in animation of the window's side. */
void MsgWin_Open(void) {
    if (gMsgWin->side == 0) {
        Flash_GotoLabel(gMsgWin->flash, "fl_l_in", 1);
    } else {
        Flash_GotoLabel(gMsgWin->flash, "fl_r_in", 1);
    }
}

/* Starts the slide-out animation of the window's side. */
void MsgWin_Close(void) {
    if (gMsgWin->side == 0) {
        Flash_GotoLabel(gMsgWin->flash, "fl_l_out", 1);
    } else {
        Flash_GotoLabel(gMsgWin->flash, "fl_r_out", 1);
    }
}

/* Changes the text file of the window (NULL: no text). */
void MsgWin_SetText(void *text) {
    gMsgWin->text = text;
    gMsgWin->box.text = text;
}

/* Chooses the side the window slides in from. */
void MsgWin_SetSide(s32 side) {
    gMsgWin->side = side;
}

#ifdef PORT
/* PC build, for screens of the PC's own (menu/port_tbo.c): the window with a line of theirs in place of a line of the
   text file, written by the game's text box in the game's font, where the game writes its own (and read out by the
   screen reader as the game's lines are: it reads the box's line, port/src/a11y_game.c). */
#include "battle/view_b.h"

extern void Flash_ClipSetCallbackC(Flash *flash, FlashRef *ref, void *fn, void *arg);
extern void TextBox_AttachString(Flash *flash, FlashRef *ref, s32 x, s32 y, u16 *str, TextBox *box);

static f32 sMsgWinTextX = -1.0f; /* where the line was last drawn (the screen's 512 x 448), -1 before */

/* The text box's own drawing, and where it drew (it follows the window's slide-in). */
static void MsgWin_PortDrawClip(void *draw, f32 *prop) {
    sMsgWinTextX = prop[14]; /* the matrix's translation (FlashProp 0x38: sys/gfxm_c.h) */
    TextBox_DrawClip((TextBoxDraw *)draw, (TextBoxClipProp *)prop);
}

/* As MsgWin_Draw, with `str` (16-bit, 0xFEFF first; NULL: no line). A changed line is another buffer: the screen
   reader tells lines apart by where they are. */
void MsgWin_PortDrawString(u16 *str) {
    FlashRef ref;
    Flash *flash;

    Flash_Advance(gMsgWin->flash);
    flash = gMsgWin->flash;
    gMsgWin->text = str;
    Flash_FindLabel(flash, NULL, "mc_message_text", &ref);
    if (str != NULL) {
        TextBox_AttachString(flash, &ref, 0, 0, str, &gMsgWin->box);
        Flash_ClipSetCallbackC(flash, &ref, MsgWin_PortDrawClip, &((TextBoxFull *)&gMsgWin->box)->draw);
    } else {
        Flash_ClipSetCallbackC(flash, &ref, NULL, NULL);
    }
    Flash_Draw(flash);
    Font_FlushAll();
}

/* How wide a line may be, in the font's pixels: from where the line starts up to `right` (the screen's 512). */
s32 MsgWin_PortLineRoom(s32 right) {
    f32 x = sMsgWinTextX < 0.0f ? 40.0f : sMsgWinTextX + (f32)gMsgWin->box.unkC;

    return (s32)((f32)right - x);
}

/* One or two lines start at the same height (the two lines' place): screens whose line changes with the cursor do not
   have it jump up and down. Three start higher, as the game's. */
void MsgWin_PortSameTop(void) {
    TextBox_SetLineOffsets((struct TextBox *)&gMsgWin->box, 0x14, 0x14, 0xA, 0, 0);
}
#endif

/* Passes two values to TextBox_SetSpacing for the window's text box. */
void MsgWin_SetBoxParam(s32 a, s32 b) {
    TextBox_SetSpacing(&gMsgWin->box, a, b);
}
