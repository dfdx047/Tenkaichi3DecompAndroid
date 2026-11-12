#include "common.h"
#include "battle/col_c.h"

/*
 * Inline icon tags of the text printer: 0x23C310..0x23D1E8. A 16-bit string may contain
 * "<PAD=x>", "<PADS=a,b,c,d>", "<COL=RRGGBBAA>" / "<COL=DEF>" and "<UB0>"; the printer calls
 * FontTag_Handle in front of every character and the tag draws controller button icons or
 * changes the text colour.
 */

typedef struct FontIconCommonRes {
    /* 0x00 */ u32 *boot; /* CommonRes.boot: word 8 is the byte offset of the icon file */
} FontIconCommonRes;

extern FontIconCommonRes *gCommonRes;

extern void Res_RelocateOffsets(void *out, void *res, void *base);
extern void TexFile_UploadAll(FontIconRes *res, s32 tbp, s32 cbp); /* uploads every texture of a file */

extern FontIconRes *gFontIconRes;
extern s32 gFontIconUploaded;  /* 1 once the icon textures were uploaded for the current flush */
extern s32 gFontIconPadType;   /* row of gFontIconTables in use */
extern s32 gFontIconFrame;     /* animation clock */
extern s32 gFontIconTick;      /* counts calls at 60 Hz so the clock runs at 30 Hz */
extern FontIconEntry *gFontIconTables[FONT_ICON_PAD_TYPES];
extern FontTagDef gFontTags[4];
extern u16 D_002C6428[]; /* "DEF" */
extern FontIconDef D_002C6838[];

s32 FontIcon_IsOnScreen(s32 x0, s32 y0, s32 x1, s32 y1);
FontIconEntry *FontIcon_Find(u16 *str, s32 padType);
s32 FontTag_Match(u16 *str, u16 *name, s32 len);
void FontTag_Handle(u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x, s32 *y, u16 **str, s32 vramBase,
                    s32 draw);

/* Stub: returns 1. No caller. */
s32 FontIcon_Stub23C310(void) {
    return 1;
}

/* Empty. No caller. */
void FontIcon_Stub23C318(void) {
}

/* Empty. No caller. */
void FontIcon_Stub23C320(void) {
}

/* Uploads the icon textures behind the fonts' VRAM blocks. */
void FontIcon_Upload(s32 vramBase) {
    TexFile_UploadAll(gFontIconRes, vramBase + 0x40, vramBase);
}

/* 1 when a box in GS coordinates lies inside the drawing area. */
s32 FontIcon_IsOnScreen(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 result = 0;

    if (x0 > 0 && x1 < 0x1000 && y0 > 0 && y1 < 0x1000) {
        result = 1;
    }
    return result;
}

/*
 * Writes one icon sprite (TEX0_1, RGBAQ, UV, XYZ2, UV, XYZ2) showing a frame of its texture.
 * Cells per row = texture width / cell size; drawn size = (s32)(cell size * scale); corners at
 * (x + 0x700, y + 0x720) and + drawn size; column = frame % cells, row = frame / cells;
 * UV = cell * size .. (cell + 1) * size; Z = 0xFFFFFFF0; Q = 1.0f.
 *
 * Matching notes: the column and row are written out at each of their four uses (the four divisions
 * leave four zero checks in the RTL, which is what pushes the function over the scheduler's 100
 * instruction limit for a region, so that nothing of the block behind the visibility test is moved
 * in front of it); the TEX0 word is the raw word with CBP merged in, then `| tbp | TCC` into a
 * second variable.
 */
void FontIcon_PutSprite(u64 **pkt, FontCmd *cmd, FontIconDef *icon, s32 frame, s32 x, s32 y, f32 scale,
                        u32 color, s32 vramBase) {
    FontTex *tex;
    s32 size;
    u64 tex0;
    u64 reg;
    s32 tbp;
    s32 cbp;
    s32 drawn;
    s32 perRow;
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
    s32 tu0;
    s32 tv0;
    s32 tu1;
    s32 tv1;

    tex = &gFontIconRes->tex[icon->tex];
    size = icon->size;
    tex0 = tex->tex0;
    drawn = (s32)((f32)size * scale);
    perRow = (1 << ((tex0 >> 26) & 0xF)) / size;
    tbp = tex->tbp + vramBase + 0x40;
    cbp = tex->cbp + vramBase;
    tex0 |= (u64)cbp << 37;
    reg = tex0 | tbp | 0x400000000;
    x0 = x + 0x700;
    y0 = y + 0x720;
    x1 = x + drawn + 0x700;
    y1 = y + drawn + 0x720;
    if (FontIcon_IsOnScreen(x0, y0, x1, y1)) {
        tu0 = size * (frame % perRow);
        tv0 = size * (frame / perRow);
        tu1 = (frame % perRow + 1) * size;
        tv1 = (frame / perRow + 1) * size;
        (*pkt)[0] = reg;
        (*pkt)[1] = (u64)color | ((u64)0x3F800000 << 32);
        *pkt += 2;
        (*pkt)[0] = ((u64)tu0 << 4) | ((u64)tv0 << 20);
        (*pkt)[1] = ((u64)x0 << 4) | ((u64)y0 << 20) | ((u64)0xFFFFFFF0 << 32);
        *pkt += 2;
        (*pkt)[0] = ((u64)tu1 << 4) | ((u64)tv1 << 20);
        (*pkt)[1] = ((u64)x1 << 4) | ((u64)y1 << 20) | ((u64)0xFFFFFFF0 << 32);
        *pkt += 2;
    }
}

/* Ticks one icon's animation takes. */
s32 FontIcon_GetAnimLength(FontIconEntry *entry) {
    s32 total = 0;
    s32 i;

    for (i = 0; i < entry->icon->frameCount; i++) {
        total += entry->icon->frameTime[i];
    }
    return total;
}

/* Ticks the animations of all icons named in a tag argument take together. */
s32 FontIcon_GetTotalLength(u16 *str, s32 padType) {
    s32 total = 0;
    FontIconEntry *entry;

    do {
        if (*str == 0) {
            break;
        }
        entry = FontIcon_Find(str, padType);
        if (entry == NULL) {
            break;
        }
        total += FontIcon_GetAnimLength(entry);
        str += entry->name->len;
    } while (1);
    return total;
}

/* Largest cell size of the icons named in a tag argument. */
s32 FontIcon_GetMaxSize(u16 *str, s32 padType) {
    s32 max = 0;
    FontIconEntry *entry;

    while (*str != 0) {
        entry = FontIcon_Find(str, padType);
        if (entry != NULL) {
            if (max < entry->icon->size) {
                max = entry->icon->size;
            }
            str += entry->name->len;
        } else {
            break;
        }
    }
    return max;
}

/* Frame an icon shows now: the animation clock modulo length, minus start, walked through its frame times. */
s32 FontIcon_GetFrame(FontIconEntry *entry, FontIconDef *icon, s32 length, s32 start) {
    s32 frame = 0;
    s32 t;
    s32 sum;
    s32 i;
    s32 n;

    if (icon->frameCount >= 2) {
        t = gFontIconFrame % length - start;
        if (t > 0) {
            sum = 0;
            n = icon->frameCount;
            frame = n - 1;
            for (i = 0; i < n; i++) {
                if (t < sum + icon->frameTime[i]) {
                    frame = i;
                    break;
                }
                sum += icon->frameTime[i];
            }
        }
    }
    return frame;
}

/* The icon whose name starts a string, the longest name winning; NULL if none. */
FontIconEntry *FontIcon_Find(u16 *str, s32 padType) {
    FontIconEntry *entry = gFontIconTables[padType];
    FontIconEntry *best = NULL;
    FontIconName *name;

    while (entry->name != NULL) {
        name = entry->name;
        if (FontTag_Match(str, name->chars, name->len)) {
            if (best == NULL || best->name->len < name->len) {
                best = entry;
            }
        }
        entry++;
    }
    return best;
}

/* 1 when the first len characters of str equal name. */
s32 FontTag_Match(u16 *str, u16 *name, s32 len) {
    s32 result = 1;
    s32 i;

    for (i = 0; i < len; i++) {
        if (*str == 0 || *str++ != *name++) {
            result = 0;
            break;
        }
    }
    return result;
}

/* Copies characters up to a stop character (at most max), leaving *src on it; 1 when max was reached. */
s32 FontTag_CopyArg(u16 **src, u16 *dst, s32 max, u16 stop) {
    u16 *p = *src;
    s32 i;

    for (i = 0; i < max; i++) {
        if (*p == stop) {
            break;
        }
        *dst++ = *p++;
    }
    *dst = 0;
    *src = p;
    return i == max;
}

/* Copies the low bytes of up to max characters (for a stripped debug message). */
void FontTag_ToAscii(u16 *src, char *dst, s32 max) {
    s32 i;

    for (i = 0; i < max; i++) {
        if (*src == 0) {
            break;
        }
        *dst++ = *(u8 *)src;
        src++;
    }
}

/* Printer begin callback: uploads the icon textures once per flush. Uses no VRAM of its own. */
s32 FontIcon_Begin(s32 vramBase) {
    if (!gFontIconUploaded) {
        FontIcon_Upload(vramBase);
        gFontIconUploaded = 1;
    }
    return 0;
}

/* Printer tag callback: when *str is at a '<' that starts a known tag, runs it and moves *str past it. */
void FontTag_Handle(u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x, s32 *y, u16 **str, s32 vramBase,
                    s32 draw) {
    char name[16];
    u16 *p = *str;
    FontTagDef *def;
    u32 i;
    u16 ch;

    gFontIconUploaded = 0;
    if (*p == '<') {
        def = gFontTags;
        for (i = 0; i < 4; i++, def++) {
            if (!FontTag_Match(p + 1, def->name, def->len)) {
                continue;
            }
            ch = p[def->len + 1];
            if (def->hasArg == 1) {
                if (ch != '=') {
                    FontTag_ToAscii(p, name, def->len);
                    break;
                }
            } else if (ch != '>') {
                FontTag_ToAscii(p, name, def->len);
                break;
            }
            def->handler(def, pkt, cmd, saved, x, y, str, vramBase, draw);
            ch = **str;
                if (ch != '>') {
                FontTag_ToAscii(p, name, def->len);
                break;
            }
            *str += 1;
            FontTag_Handle(pkt, cmd, saved, x, y, str, vramBase, draw);
            break;
        }
    }
}

/* "<PAD=names>": draws the named icons side by side, each on its own animation. */
void FontTag_Pad(FontTagDef *def, u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x, s32 *y, u16 **str,
                 s32 vramBase, s32 draw) {
    u16 arg[32];
    u16 *p;
    s32 *padTypePtr;
    s32 padType;
    s32 max;
    u16 *s;
    FontIconEntry *entry;
    FontIconDef *icon;
    FontIconName *name;
    s32 frame;
    s32 dy;

    p = *str + def->len + 2;
    padTypePtr = cmd->tagArg;
    FontTag_CopyArg(&p, arg, 0x1F, '>');
    padType = *padTypePtr;
    FontIcon_GetTotalLength(arg, padType);
    max = FontIcon_GetMaxSize(arg, padType);
    for (s = arg; *s != 0; s += name->len) {
        entry = FontIcon_Find(s, padType);
        if (entry != NULL) {
            icon = entry->icon;
            name = entry->name;
            frame = FontIcon_GetFrame(entry, icon, FontIcon_GetAnimLength(entry), 0);
            switch (icon->size) {
            case 0x20:
                dy = (max - 0x20) / 2 - 6;
                break;
            case 0x40:
                dy = (max - 0x40) / 2;
                break;
            case 0x80:
                dy = (max - 0x80) / 2 - 6;
                break;
            default:
                dy = (max - icon->size) / 2 - 6;
                break;
            }
            if (draw) {
                FontIcon_PutSprite(pkt, cmd, icon, frame, *x, *y + dy, entry->scale, cmd->color2, vramBase);
            }
            *x += (s32)((f32)icon->advance * entry->scale) + cmd->spacingX;
        } else {
            break;
        }
    }
    *str = p;
}

/* "<PADS=a,b,c,d>": one list per controller type; the icons of the current type animate one after another. */
void FontTag_PadSequence(FontTagDef *def, u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x, s32 *y,
                         u16 **str, s32 vramBase, s32 draw) {
    u16 arg[FONT_ICON_PAD_TYPES][32];
    u16 *p;
    s32 *padTypePtr;
    s32 padType;
    s32 total;
    s32 start;
    s32 max;
    u16 *s;
    FontIconEntry *entry;
    FontIconDef *icon;
    FontIconName *name;
    s32 frame;
    s32 dy;

    p = *str + def->len + 2;
    start = 0;
    padTypePtr = cmd->tagArg;
    FontTag_CopyArg(&p, arg[0], 0x1F, ',');
    p++;
    FontTag_CopyArg(&p, arg[1], 0x1F, ',');
    p++;
    FontTag_CopyArg(&p, arg[2], 0x1F, ',');
    p++;
    FontTag_CopyArg(&p, arg[3], 0x1F, '>');
    padType = *padTypePtr;
    total = FontIcon_GetTotalLength(arg[padType], padType);
    max = FontIcon_GetMaxSize(arg[padType], padType);
    for (s = arg[padType]; *s != 0; s += name->len) {
        entry = FontIcon_Find(s, padType);
        if (entry != NULL) {
            icon = entry->icon;
            name = entry->name;
            frame = FontIcon_GetFrame(entry, icon, total, start);
            start += FontIcon_GetAnimLength(entry);
            switch (icon->size) {
            case 0x20:
                dy = (max - 0x20) / 2 - 6;
                break;
            case 0x40:
                dy = (max - 0x40) / 2;
                break;
            case 0x80:
                dy = (max - 0x80) / 2 - 6;
                break;
            default:
                dy = (max - icon->size) / 2 - 6;
                break;
            }
            if (draw) {
                FontIcon_PutSprite(pkt, cmd, icon, frame, *x, *y + dy, entry->scale, cmd->color2, vramBase);
            }
            *x += (s32)((f32)icon->advance * entry->scale) + cmd->spacingX;
        } else {
            break;
        }
    }
    *str = p;
}

/* "<COL=RRGGBBAA>" sets the text colour of the rest of the command, "<COL=DEF>" restores it. */
void FontTag_Color(FontTagDef *def, u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x, s32 *y, u16 **str,
                   s32 vramBase, s32 draw) {
    u16 *p = *str + def->len;
    u16 *s = p + 2;
    u32 color;
    u32 hex;
    s32 i;
    u16 ch;

    if (FontTag_Match(s, D_002C6428, 3)) {
        color = saved->color;
        s = p + 5;
    } else {
        hex = 0;
        for (i = 7; i >= 0; i--) {
            ch = *s++;
            if ((u16)(ch - '0') < 10) {
                hex |= (ch - '0') << (i * 4);
            } else if ((u16)(ch - 'A') < 6) {
                hex |= (ch - 'A' + 10) << (i * 4);
            } else {
                return;
            }
        }
        color = (((hex >> 16) & 0xFF) | ((hex & 0xFF) << 16)) | (((hex >> 24) << 24) | (hex & 0xFF00));
    }
    if (draw) {
        cmd->color = color;
    }
    *str = s;
}

/* "<UB0>": draws character 0xFF10 at once, in front of the text that follows. */
void FontTag_Ub0(FontTagDef *def, u64 **pkt, FontCmd *cmd, FontCmd *saved, s32 *x, s32 *y, u16 **str,
                 s32 vramBase, s32 draw) {
    u16 *p = *str + def->len + 1;

    if (draw) {
        Font_DrawCharNow(pkt, cmd, x, y, 0xFF10);
    }
    *str = p;
}

/* Boot-time init: binds the icon file and hooks the printer's callbacks. */
void FontIcon_Init(void) {
    u32 *boot = gCommonRes->boot;

    gFontIconRes = (FontIconRes *)(boot + (boot[8] >> 2));
    Res_RelocateOffsets(&gFontIconRes, gFontIconRes, gFontIconRes);
    Font_SetBeginCallback(FontIcon_Begin);
    Font_SetTagCallback(FontTag_Handle, &gFontIconPadType);
}

/* Selects the controller type whose icon table is used. */
void FontIcon_SetPadType(s32 padType) {
    gFontIconPadType = padType;
}

/* The icon file. */
FontIconRes *FontIcon_GetRes(void) {
    return gFontIconRes;
}

/* The icon definitions. */
FontIconDef *FontIcon_GetDefs(void) {
    return D_002C6838;
}

/* Restarts the icon animation clock. */
void FontIcon_ResetAnim(void) {
    gFontIconFrame = 0;
}

/* Advances the animation clock once per 30 Hz frame: every second call when vsyncs is 1. */
void FontIcon_Tick(s32 vsyncs) {
    if (vsyncs == 1) {
        if (!(gFontIconTick & 1)) {
            gFontIconFrame++;
        }
        gFontIconTick++;
    } else {
        gFontIconFrame++;
    }
}

/* Largest icon size named in a string that begins with "<PAD=". */
s32 FontIcon_GetPadTagSize(u16 *str) {
    u16 arg[32];
    u16 *p;

    p = str + 5;
    FontTag_CopyArg(&p, arg, 0x1F, '>');
    return FontIcon_GetMaxSize(arg, gFontIconPadType);
}

/* Empty; called after Font_FlushAll by the battle draw. */
void FontIcon_Stub23D1E0(void) {
}
