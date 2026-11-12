#include "common.h"
#include "battle/view_a.h"
#include "battle/view_b.h"
#include "menu/port_screen.h"
#include <stdio.h>

/*
 * TextBox, second half: 0x2600B0..0x260D20. The module starts in view_a_e.c (TextBox_Init at 0x25FE00) and this
 * is the same object; see battle/view_b.h for the complete layout of a text box.
 *
 * A text box is a text file plus the style its lines are drawn in. TextBox_AttachLine hangs one line on a movie
 * clip: it fills box->draw and registers TextBox_DrawClip as the clip's "draw over" callback, so the movie
 * player draws the text at the clip's position, tinted with the clip's colour transform, each time it draws
 * the clip.
 */

extern void Flash_ClipSetCallbackC(Flash *flash, FlashRef *ref, void *fn, void *arg);
extern f32 Flash_ClipGetAlpha(Flash *flash, FlashRef *ref);
extern void Gfx_AddDefaultEnv(void);

extern void Font_FlushAll(void);
extern s32 Font_GetWidth(u16 *str);
extern s32 Font_GetHeight(u16 *str);
extern s32 Font_CountLines(u16 *str);
extern void Font_PrintAt(s32 x, s32 y, u16 *str);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_PrintNumAt(s32 x, s32 y, s32 value, s32 digits);
extern void Flash_ClipSetFlags(Flash *flash, FlashRef *ref, s32 props, u8 on);

extern void Font_SetScaleXY(f32 sx, f32 sy);
extern void Font_SetAlign(s32 align);
extern void Font_SetShadowMode(s32 mode);
extern void Font_SetShadowColor(u32 color);
extern void Font_SetShadowOffset(s32 x, s32 y);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetClip(s32 x0, s32 y0, s32 x1, s32 y1);
extern void Font_SetSpacing(s32 x, s32 y);
#ifdef PORT
extern const u16 *UiText_TranslateLine(const u32 *table, int line, const u16 *original);
#endif

#define TB(box) ((TextBoxFull *)(box))

/* A line wider than `w` is shrunk horizontally to fit. */
void TextBox_SetMaxWidth(TextBox *box, s32 w) {
    TB(box)->maxW = w;
    TB(box)->flags |= TEXTBOX_FLAG_MAX_W;
}

/* A text taller than `h` is shrunk vertically to fit. */
void TextBox_SetMaxHeight(TextBox *box, s32 h) {
    TB(box)->maxH = h;
    TB(box)->flags |= TEXTBOX_FLAG_MAX_H;
}

/* Both limits. `unused` is not looked at. */
void TextBox_SetMaxSize(TextBox *box, s32 w, s32 h) {
    TextBox_SetMaxWidth(box, w);
    TextBox_SetMaxHeight(box, h);
}

/* Extra y offset for a text of one, two, three, four and five rows (to centre it in its window). */
void TextBox_SetLineOffsets(TextBox *box, s32 y1, s32 y2, s32 y3, s32 y4, s32 y5) {
    TB(box)->flags |= TEXTBOX_FLAG_LINE_Y;
    TB(box)->lineY[0] = y1;
    TB(box)->lineY[1] = y2;
    TB(box)->lineY[2] = y3;
    TB(box)->lineY[3] = y4;
    TB(box)->lineY[4] = y5;
}

/* Character and line spacing. */
void TextBox_SetSpacing(TextBox *box, s32 x, s32 y) {
    TB(box)->flags |= TEXTBOX_FLAG_SPACING;
    TB(box)->spacingX = x;
    TB(box)->spacingY = y;
}

/* The clip callback: moves the text by the clip's position, tints text and shadow with the clip's colour
   transform, and prints the string unless it has become fully transparent.

   NOT MATCHING: kept as INCLUDE_ASM. The attempt below is behaviourally exact. It differs in how the clamped
   component reaches its store: the original copies it with `move v1,v0` (seven times; the eighth needs no
   copy), this C narrows it with `andi v1,v0,0xff` (eight times, so the function is one instruction longer).
   An `s32` helper gives the plain copy but then the compiler turns the outer test into a branch around, or
   stores in both arms; some 40 shapes of the clamp were tried. Everything else is identical. */
/* Second cleanup pass: on this compiler `move` is also what an int-to-long sign extension compiles to
   (`(s64)u0 << 4` in BtlText_PutSprite), and a helper returning s64 / long does give a move in the arm
   (`move v0,v0`), but the narrowing then moves to the join (`andi v0,v0,0xff` in front of the `sb`): 48
   differences. A `u8` result local in a macro is promoted to a full register and has its zero hoisted in front
   of the branch (204 instructions). So the original copy is most likely a plain copy between two int
   registers that the allocator could not merge (the next statement's `lbu` already sits in v0 at the join),
   in a shape where the compiler does not hoist the `= 0` arm; not found (15 more shapes tried here).
   Cleanup 4: twelve more shapes (nested ternaries on an `s32` temporary, `r = 0xFF; if (t < 0x100) r = t;` in
   a block or in an `s32` inline helper, min / max macros, a helper taking `u8 *`): 62 to 130 differences, all
   because the `< 0` test then becomes straight-line code or the zero is hoisted. The `u8` helper below stays
   the closest form. */
#if 0
/* Clamps a colour component to a byte. */
static inline u8 TextBox_ClampByte(s32 c) {
    return c < 0 ? 0 : (c > 0xFF ? 0xFF : c);
}

/* A colour component after a clip's colour transform. */
#define TB_TINT(c, mul, add) (c) = TextBox_ClampByte((s32)((f32)(c) * (mul)) + (add))

s32 TextBox_DrawClip(TextBoxDraw *draw, TextBoxClipProp *prop) {
    u32 shadow;

    if (prop != NULL) {
        draw->x = (f32)draw->x + prop->x;
        draw->y = (f32)draw->y + prop->y;
        TB_TINT(draw->color[0], prop->mul[0], prop->add[0]);
        TB_TINT(draw->color[1], prop->mul[1], prop->add[1]);
        TB_TINT(draw->color[2], prop->mul[2], prop->add[2]);
        TB_TINT(draw->color[3], prop->mul[3], prop->add[3]);
        TB_TINT(draw->shadow[0], prop->mul[0], prop->add[0]);
        TB_TINT(draw->shadow[1], prop->mul[1], prop->add[1]);
        TB_TINT(draw->shadow[2], prop->mul[2], prop->add[2]);
        TB_TINT(draw->shadow[3], prop->mul[3], prop->add[3]);
    }
    if (draw->color[3] != 0) {
        shadow = (draw->shadow[0] | (draw->shadow[2] << 16)) | ((draw->shadow[3] << 24) | (draw->shadow[1] << 8));
        Font_PushStyle();
        Font_SetAlign(draw->align);
        Font_SetScaleXY(draw->scaleX, draw->scaleY);
        Font_SetColorRGBA(draw->color[0], draw->color[1], draw->color[2], draw->color[3]);
        Font_SetShadowMode(2);
        Font_SetShadowOffset(1, 2);
        Font_SetShadowColor(shadow);
        Font_SetClip(draw->clip[0], draw->clip[1], draw->clip[2], draw->clip[3]);
        Font_SetSpacing(draw->spacingX, draw->spacingY);
        Font_PrintAt(draw->x, draw->y, draw->str);
        Font_PopStyle();
        if (draw->noFlush != 1) {
            Font_FlushAll();
            Gfx_AddDefaultEnv();
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/view_b", TextBox_DrawClip);
#endif

/* Fills box->draw for one string and hangs it on a clip. */
#define TB_ATTACH(flash, ref, x, y, str, box) \
    ofs[1] = 0; \
    ofs[0] = 0; \
    alpha = Flash_ClipGetAlpha(flash, ref); \
    TB_ATTACH_STR(str, box) \
    if (TB(box)->flags & TEXTBOX_FLAG_MAX_W) { \
        size = Font_GetWidth(str); \
        if (TB(box)->maxW < size) { \
            scale[0] = (f32)TB(box)->maxW / (f32)size; \
        } else { \
            scale[0] = 1.0f; \
        } \
    } else { \
        scale[0] = 1.0f; \
    } \
    if (TB(box)->flags & TEXTBOX_FLAG_MAX_H) { \
        size = Font_GetHeight(str); \
        if (TB(box)->maxH < size) { \
            scale[1] = (f32)TB(box)->maxH / (f32)size; \
        } else { \
            scale[1] = 1.0f; \
        } \
    } else { \
        scale[1] = 1.0f; \
    } \
    if (TB(box)->flags & TEXTBOX_FLAG_LINE_Y) { \
        switch (Font_CountLines(str)) { \
        case 1: \
            ofs[1] += TB(box)->lineY[0]; \
            break; \
        case 2: \
            ofs[1] += TB(box)->lineY[1]; \
            break; \
        case 3: \
            ofs[1] += TB(box)->lineY[2]; \
            break; \
        case 4: \
            ofs[1] += TB(box)->lineY[3]; \
            break; \
        case 5: \
            ofs[1] += TB(box)->lineY[4]; \
            break; \
        } \
    } \
    TB(box)->draw.x = ofs[0] + TB(box)->x + x; \
    TB(box)->draw.y = ofs[1] + TB(box)->y + y; \
    TB(box)->draw.scaleX = scale[0]; \
    TB(box)->draw.scaleY = scale[1]; \
    if (TB(box)->flags & TEXTBOX_FLAG_COLOR) { \
        TB(box)->draw.color[0] = TB(box)->color[0]; \
        TB(box)->draw.color[1] = TB(box)->color[1]; \
        TB(box)->draw.color[2] = TB(box)->color[2]; \
        TB(box)->draw.color[3] = (u32)(TB(box)->color[3] * alpha); \
    } else { \
        TB(box)->draw.color[0] = 0xFF; \
        TB(box)->draw.color[1] = 0xFF; \
        TB(box)->draw.color[2] = 0xFF; \
        TB(box)->draw.color[3] = (u32)(alpha * 128.0f); \
    } \
    if (TB(box)->flags & TEXTBOX_FLAG_COLOR2) { \
        TB(box)->draw.shadow[0] = TB(box)->shadow[0]; \
        TB(box)->draw.shadow[1] = TB(box)->shadow[1]; \
        TB(box)->draw.shadow[2] = TB(box)->shadow[2]; \
        TB(box)->draw.shadow[3] = (u32)(TB(box)->shadow[3] * alpha); \
    } else { \
        TB(box)->draw.shadow[0] = 0x20; \
        TB(box)->draw.shadow[1] = 0x20; \
        TB(box)->draw.shadow[2] = 0x20; \
        TB(box)->draw.shadow[3] = (u32)(alpha * 64.0f); \
    } \
    if (TB(box)->flags & TEXTBOX_FLAG_RECT) { \
        TB(box)->draw.clip[0] = TB(box)->clip[0]; \
        TB(box)->draw.clip[1] = TB(box)->clip[1]; \
        TB(box)->draw.clip[2] = TB(box)->clip[2]; \
        TB(box)->draw.clip[3] = TB(box)->clip[3]; \
    } else { \
        TB(box)->draw.clip[2] = 0x1FF; \
        TB(box)->draw.clip[3] = 0x1BF; \
        TB(box)->draw.clip[0] = 0; \
        TB(box)->draw.clip[1] = 0; \
    } \
    if (TB(box)->flags & TEXTBOX_FLAG_SPACING) { \
        TB(box)->draw.spacingX = TB(box)->spacingX; \
        TB(box)->draw.spacingY = TB(box)->spacingY; \
    } else { \
        TB(box)->draw.spacingX = 0; \
        TB(box)->draw.spacingY = 0; \
    } \
    TB(box)->draw.str = str; \
    Flash_ClipSetCallbackC(flash, ref, TextBox_DrawClip, &TB(box)->draw)

/* Shows line `line` of the box's text file on a clip, at (x, y) from the clip's position. A negative line or
   a missing clip takes the text off the clip. */
void TextBox_AttachLine(Flash *flash, FlashRef *ref, s32 x, s32 y, s32 line, TextBox *box) {
    s32 size;
    s32 ofs[2];
    f32 scale[2];
    f32 alpha;
    u16 *str;

    if (line < 0 || ref->id < 0) {
        Flash_ClipSetCallbackC(flash, ref, NULL, NULL);
        return;
    }
#ifdef PORT
#define TB_ATTACH_STR(str, box) str = (u16 *)((u8 *)TB(box)->text + ((((u32 *)TB(box)->text)[line + 1] >> 2) << 2)); \
    str = (u16 *)UiText_TranslateLine((const u32 *)TB(box)->text, line, str);
#else
#define TB_ATTACH_STR(str, box) str = (u16 *)((u8 *)TB(box)->text + ((((u32 *)TB(box)->text)[line + 1] >> 2) << 2));
#endif
    TB_ATTACH(flash, ref, x, y, str, box);
#undef TB_ATTACH_STR
}

/* The same for a string that is not in the box's text file. */
void TextBox_AttachString(Flash *flash, FlashRef *ref, s32 x, s32 y, u16 *str, TextBox *box) {
    s32 size;
    s32 ofs[2];
    f32 scale[2];
    f32 alpha;

    if (ref->id < 0) {
        Flash_ClipSetCallbackC(flash, ref, NULL, NULL);
        return;
    }
#define TB_ATTACH_STR(str, box)
    TB_ATTACH(flash, ref, x, y, str, box);
#undef TB_ATTACH_STR
}


/* A single replacement caption for BT4's costume window. It uses the port's
   cached, antialiased TrueType line renderer, keeping the movie's fade/position. */
extern int PortTbo_TextId(const char *text, int kind);
extern int PortTbo_TextInfo(int id, int what);
typedef struct Bt4CostumeCaption {
    const void *key;
    unsigned stamp;
    s32 number, total;
    char name[128];
} Bt4CostumeCaption;
/* The caller's storage is an active TextBox, not spare callback memory.
 * Never overwrite its resource pointer or retain a label from its file. */
static Bt4CostumeCaption sCostumeCaptions[16];
static unsigned sCostumeCaptionStamp;

static void Bt4Costume_Utf8(char *out, const u16 *in) {
    s32 n = 0, i;
    for (i = 0; in != NULL && i < 96 && in[i] && n < 120; i++) {
        u32 c = in[i];
        if (c < 32 || (c >= 0xD800 && c <= 0xDFFF) || c >= 0xFFF0) continue;
        if (c < 128) out[n++] = c;
        else if (c < 2048) { out[n++] = 0xC0 | (c >> 6); out[n++] = 0x80 | (c & 63); }
        else { out[n++] = 0xE0 | (c >> 12); out[n++] = 0x80 | ((c >> 6) & 63); out[n++] = 0x80 | (c & 63); }
    }
    out[n] = 0;
}

static void Bt4Costume_Line(char *text, f32 cx, f32 base, f32 cap, f32 maxWidth,
                            u32 top, u32 bottom, s32 alpha) {
    s32 id = PortTbo_TextId(text, 0), band, bands = cap <= 9 ? 1 : 3;
    f32 ky, kx, width, x0, x1, y0, height;
    if (id < 0) return;
    ky = cap / 1000.0f; kx = ky * 0.857f;
    width = PortTbo_TextInfo(id, 0) * kx;
    if (width > maxWidth) { kx *= maxWidth / width; width = maxWidth; }
    x0 = cx - width * 0.5f - PortTbo_TextInfo(id, 4) * kx;
    x1 = x0 + PortTbo_TextInfo(id, 1) * kx;
    y0 = base - PortTbo_TextInfo(id, 3) * ky;
    height = PortTbo_TextInfo(id, 2) * ky;
    { extern int Port_MenusWide(void);
      if (Port_MenusWide()) {
          /* This caption is a TBO texture on a native Flash costume plate.
           * It bypasses the renderer's native-art pass, so use the SAME
           * parent anchor as the plate, once, rather than a centred page. */
          extern int Port_MenuPartNow(void), Port_PageAspectMilli(void);
          int part=Port_MenuPartNow(),aspect=Port_PageAspectMilli();
          f32 pivot=part==2||part==5?0.0f:part==3||part==6?512.0f:256.0f;
          f32 scale=aspect>1333?1333.333333f/(f32)aspect:1.0f;
          x0=pivot+(x0-pivot)*scale;x1=pivot+(x1-pivot)*scale;
      }
    }
    Ps_SpriteScreen(id, x0, y0, x1, y0 + height, 0, 0.5f, 1, 1,
                    Ps_Tint(Ps_Rgba(10, 35, 49, 255)), alpha);
    for (band = 0; band < bands; band++) {
        f32 t0 = band / (f32)bands, t1 = (band + 1) / (f32)bands;
        Ps_SpriteScreen(id, x0, y0 + height*t0, x1, y0 + height*t1,
                       0, t0*0.5f, 1, t1*0.5f,
                       Ps_Tint(Ps_Mix(top, bottom, (band + 0.5f)/(f32)bands)), alpha);
    }
}

static void Bt4Costume_DrawCaption(void *arg, TextBoxClipProp *prop) {
    Bt4CostumeCaption *caption = arg;
    s32 alpha = (s32)(128.0f * prop->mul[3]) + prop->add[3];
    f32 cx = prop->x + 24.0f;
    char number[16], name[128], counter[32];
    if (alpha <= 0) return;
    if (alpha > 128) alpha = 128;
    sprintf(number, "%02d", caption->number);
    {
        extern int Port_GameProfileEnglish(void);
        s32 english=Port_GameProfileEnglish();
        sprintf(counter,english?"COSTUME %d/%d":"TRAJE %d/%d",caption->number,caption->total);
        strcpy(name,caption->name);
        if(!name[0])sprintf(name,english?"Costume %d":"Traje %d",caption->number);
    }
    Bt4Costume_Line(number, cx, prop->y + 18, 18, 42,
                    Ps_Rgba(255,255,235,255), Ps_Rgba(105,231,240,255), alpha);
    Bt4Costume_Line(name, cx, prop->y + 43, 10, 184,
                    Ps_Rgba(255,255,243,255), Ps_Rgba(225,249,248,255), alpha);
    Bt4Costume_Line(counter, cx, prop->y + 61, 9.0f, 150,
                    Ps_Rgba(248,255,255,255), Ps_Rgba(248,255,255,255), alpha);
    Gfx_AddDefaultEnv();
}

/* BT4 uses the left-pointing atlas arrow for both costume controls. Keep the
   right control's UV mirrored, in every selector which uses that movie. */
void Bt4Costume_FixArrows(Flash *flash) {
    FlashRef ref;
    Flash_FindLabel(flash, "mc_yajirusi_right", "mc_yajirusi_icon_right", &ref);
    Flash_ClipSetFlags(flash, &ref, 4, 1);
}

void Bt4Costume_AttachCaption(Flash *flash, FlashRef *ref, s32 number, s32 total,
                              const u16 *name, void *storage) {
    Bt4CostumeCaption *caption = NULL;
    s32 i,pick=0;
    for(i=0;i<16;++i){
        if(sCostumeCaptions[i].key==storage){caption=&sCostumeCaptions[i];break;}
        if(sCostumeCaptions[i].stamp<sCostumeCaptions[pick].stamp)pick=i;
    }
    if(!caption)caption=&sCostumeCaptions[pick];
    caption->key=storage;caption->stamp=++sCostumeCaptionStamp;
    caption->number=number;caption->total=total;
    {
        const u32 *row=*(const u32 **)((const u8 *)storage+4);
        u32 offset=(u32)name-(u32)row;
        /* A costume name is a short line in the current character's table.
         * Reject missing/header entries instead of treating table metadata as
         * a UTF-16 pointer. The visible fallback still identifies the costume. */
        if(!row || !name || offset<(u32)(number+1)*4 || offset>65536u){
            if(getenv("BT4_RENDER_TEST")){
                static s32 shown;
                if(shown++<16)fprintf(stderr,"costume caption fallback: number=%d total=%d offset=%08X header=%08X\n",number,total,offset,row?row[0]:0);
            }
            name=NULL;
        }
        Bt4Costume_Utf8(caption->name,name);
    }
    Flash_ClipSetFlags(flash, ref, 0x800, 1);
    Flash_ClipSetCallbackC(flash, ref, Bt4Costume_DrawCaption, caption);
}
