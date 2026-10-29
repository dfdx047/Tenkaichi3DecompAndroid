#ifndef MENU_TAG_KIT_H
#define MENU_TAG_KIT_H
#ifdef PORT
/*
 * The Tag Team mod's pages in the look of Dragon Ball (the pictures: port/tools/db_assets.py, tbo/ui/db_*.png): a dusk
 * sky with clouds behind, the title in orange with a red capsule, slanted plates (orange under the cursor) with a dragon
 * ball each, a gold-framed picture window, a white talk box at the foot. And a touch screen's taps (TagNet_MenuPointer): what a page draws, it can
 * mark as a place to tap (Tk_Region); the next frame's input asks what was tapped (Tk_Tap). Each page's file has its
 * own copy of this (static), and draws once a frame between Tk_Begin and its prompts.
 */
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
#include "sys/pad.h"

extern void TagNet_MenuPointer(u32);
extern int Port_PageAspectMilli(void);
extern f32 sinf(f32 x);
extern const char *Port_TagText(const char *english);

#define TK_UNUSED __attribute__((unused))
#define TK_REGIONS 64
#define TK_ROW 0 /* a region's kind: a row of a list (value: its number) */
#define TK_PAD 1 /* ... a pad's buttons (value: PADG_ bits) */

typedef struct TkRegion {
    f32 x0, y0, x1, y1;
    s32 kind, value;
} TkRegion;

static TkRegion sTkRegion[TK_REGIONS];
static s32 sTkRegions;
static s32 sTkFrames;
static s32 sTkPromptKind[6], sTkPrompts;

/* Once a frame, before the page draws: its places to tap start again. */
static TK_UNUSED void Tk_Begin(void) {
    sTkRegions = 0;
    sTkFrames++;
    Ps_PageSet();
    Ps_SetShift(0);
}

static TK_UNUSED void Tk_Region(f32 x0, f32 y0, f32 x1, f32 y1, s32 kind, s32 value) {
    if (sTkRegions < TK_REGIONS) {
        TkRegion *r = &sTkRegion[sTkRegions++];

        r->x0 = x0;
        r->y0 = y0;
        r->x1 = x1;
        r->y1 = y1;
        r->kind = kind;
        r->value = value;
    }
}

/* A tap since the last frame: 1, and the place it landed on (the one drawn last, on top). 0: no tap, or on nothing. */
static TK_UNUSED s32 Tk_Tap(s32 *kind, s32 *value) {
    s32 ptr[3];
    s32 i;
    f32 x, y;

    TagNet_MenuPointer((u32)ptr);
    if (!ptr[2]) {
        return 0;
    }
    Ps_PageSet();
    Ps_SetShift(0);
    x = ((f32)ptr[0] - Ps_X(0.0f)) * 512.0f / (Ps_X(512.0f) - Ps_X(0.0f));
    y = ((f32)ptr[1] - Ps_Y(0.0f)) * 448.0f / (Ps_Y(448.0f) - Ps_Y(0.0f));
    for (i = sTkRegions - 1; i >= 0; i--) {
        TkRegion *r = &sTkRegion[i];

        if (x >= r->x0 && x < r->x1 && y >= r->y0 && y < r->y1) {
            *kind = r->kind;
            *value = r->value;
            return 1;
        }
    }
    return 0;
}

/* The pad's bit of a prompt's button (Ps_ButtonPic's kinds). */
static TK_UNUSED u32 Tk_KindBits(s32 kind) {
    switch (kind) {
    case 0:
        return PADG_CROSS;
    case 1:
        return PADG_SQUARE;
    case 2:
        return PADG_TRIANGLE;
    case 4:
        return PADG_CIRCLE;
    case 7:
        return PADG_START;
    }
    return 0;
}

/* The prompts at the foot of the screen (Ps_Prompt), each a place to tap that presses its button. */
static TK_UNUSED void Tk_PromptsClear(void) {
    Ps_PromptsClear();
    sTkPrompts = 0;
}

static TK_UNUSED void Tk_Prompt(s32 kind, char *label) {
    Ps_Prompt(kind, label);
    if (sTkPrompts < 6) {
        sTkPromptKind[sTkPrompts++] = kind;
    }
}

static TK_UNUSED void Tk_PromptsDraw(void) {
    s32 k;

    Ps_SetShift(0);
    Ps_Align(0);
    Ps_PromptsDraw();
    for (k = 0; k < sTkPrompts; k++) {
        f32 x0 = Ps_PromptX(k) - 6.0f;
        f32 x1 = k + 1 < sTkPrompts ? Ps_PromptX(k + 1) - 6.0f : 480.0f;

        if (Tk_KindBits(sTkPromptKind[k])) {
            Tk_Region(x0, 410.0f, x1, 442.0f, TK_PAD, (s32)Tk_KindBits(sTkPromptKind[k]));
        }
    }
    Ps_Align(1);
}

/* 0..1 over 16 frames since `start`, slowing at the end (a page coming in, as the online screens'). */
static TK_UNUSED f32 Tk_In(s32 start) {
    f32 t = (f32)(sTkFrames - start) / 16.0f;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    t = 1.0f - t;
    return 1.0f - t * t * t;
}

/* The dusk sky with its clouds over the whole window (the picture's middle, as wide as the window is). */
static TK_UNUSED void Tk_Backdrop(void) {
    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;
    f32 pic = Ps_PicW(TBO_DB_SKY) / Ps_PicH(TBO_DB_SKY);
    f32 u = aspect < pic ? aspect / pic : 1.0f;
    f32 v = aspect < pic ? 1.0f : pic / aspect;

    Ps_SpriteScreen(TBO_DB_SKY, 0.0f, 0.0f, 512.0f, 448.0f, 0.5f - u * 0.5f, 0.5f - v * 0.5f, 0.5f + u * 0.5f, 0.5f + v * 0.5f, PS_PLAIN, 0x80);
}

/* Where the window's left edge is, and a little in: the lists keep to it on a wide phone. */
static TK_UNUSED f32 Tk_Left(void) {
    return Ps_Left() + 14.0f;
}

/* A point of the design (made for a 2712 x 1220 phone) on the page: across the whole window, down its height. */
static TK_UNUSED f32 Tk_PX(f32 px) {
    f32 l = Ps_Left();

    return l + px * (512.0f - 2.0f * l) / 2712.0f;
}

static TK_UNUSED f32 Tk_PY(f32 py) {
    return py * 448.0f / 1220.0f;
}

/* A slanted four-cornered shape (the top edge `sk` right of the bottom one), coloured from top to bottom. */
static TK_UNUSED void Tk_Para(f32 x0, f32 y0, f32 x1, f32 y1, f32 sk, u32 top, u32 bottom) {
    Ps_Quad(x0 + sk, y0, x1, y0, x0, y1, x1 - sk, y1, top, top, bottom, bottom);
}

/* The soft glow, tinted (r, g, b), over a box. */
static TK_UNUSED void Tk_Glow(f32 x0, f32 y0, f32 x1, f32 y1, s32 r, s32 g, s32 b, s32 alpha) {
    Ps_Sprite(TBO_DB_GLOW, x0, y0, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f, Ps_Tint(Ps_Rgba(r, g, b, 0)), alpha);
}

/* A dragon ball of `stars` stars (0..7) centred on (cx, cy), `r` units high from its middle (round on the screen). */
static TK_UNUSED void Tk_Ball(f32 cx, f32 cy, f32 r, s32 stars) {
    f32 rx = r * PS_UNIT_X;

    stars = stars < 0 ? 0 : stars > 7 ? 7 : stars;
    Ps_Sprite(TBO_DB_BALLS, cx - rx, cy - r, cx + rx, cy + r, (f32)stars / 8.0f, 0.0f, (f32)(stars + 1) / 8.0f, 1.0f, PS_PLAIN, 0x80);
}

/* The page's title in the game's orange lettering at the top left; `card`, if any: a red capsule at the top right. */
static TK_UNUSED void Tk_Capsule(f32 x0, f32 y0, f32 x1, f32 y1, char *text, f32 scale) {
    f32 sk = (y1 - y0) * 0.35f;

    Tk_Para(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, sk, Ps_Rgba(255, 230, 140, 128), Ps_Rgba(255, 200, 90, 128));
    Tk_Para(x0, y0, x1, y1, sk, Ps_Rgba(225, 40, 30, 128), Ps_Rgba(170, 14, 14, 128));
    Ps_Align(1);
    Ps_Text((x0 + x1) * 0.5f, Ps_TextTop((y0 + y1) * 0.5f, scale), text, scale, x1 - x0 - sk - 8.0f, 0);
}

static TK_UNUSED void Tk_Title(char *title, char *card) {
    Ps_SetShift(0);
    Ps_Align(0);
    Ps_Heading(Tk_Left() + 4.0f, Ps_TextTop(30.0f + 1.6f * sinf((f32)sTkFrames * 0.045f), 1.35f), title, 1.35f, 300.0f, 0x80);
    if (card != NULL && card[0] != 0) {
        Tk_Capsule(318.0f, 14.0f, 500.0f, 40.0f, card, 0.62f);
    }
    Ps_Align(1);
}

/* A plate of a list: slanted, deep blue with a navy edge, or orange with a brown one under the cursor (a glow behind). */
static TK_UNUSED void Tk_Bar(f32 x0, f32 y0, f32 x1, f32 y1, s32 lit) {
    f32 sk = (y1 - y0) * 0.45f;

    if (lit) {
        Tk_Glow(x0 - 30.0f, y0 - 16.0f, x1 + 30.0f, y1 + 16.0f, 255, 170, 40, 0x60);
        Tk_Para(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, sk, Ps_Rgba(120, 30, 0, 128), Ps_Rgba(120, 30, 0, 128));
        Tk_Para(x0, y0, x1, y1, sk, Ps_Rgba(255, 240, 150, 128), Ps_Rgba(240, 120, 10, 128));
    } else {
        Tk_Para(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, sk, Ps_Rgba(20, 40, 110, 128), Ps_Rgba(20, 40, 110, 128));
        Tk_Para(x0, y0, x1, y1, sk, Ps_Rgba(110, 160, 240, 124), Ps_Rgba(35, 70, 170, 124));
    }
}

/* A light plate (the main list's): pale blue to blue, a navy edge; its letters are navy (Tk_NavyText). */
static TK_UNUSED void Tk_LightBar(f32 x0, f32 y0, f32 x1, f32 y1) {
    f32 sk = (y1 - y0) * 0.55f;

    Tk_Para(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, sk, Ps_Rgba(20, 40, 110, 128), Ps_Rgba(20, 40, 110, 128));
    Tk_Para(x0, y0, x1, y1, sk, Ps_Rgba(235, 245, 255, 124), Ps_Rgba(150, 190, 240, 124));
}

/* Navy letters with a white edge (on the light plates and in the talk box). */
static TK_UNUSED void Tk_NavyText(f32 x, f32 y, char *s, f32 scale, f32 width) {
    Ps_TextEx(x, y, s, scale, width, Ps_Rgba(30, 50, 120, 0x80), Ps_Rgba(22, 36, 96, 0x80), Ps_Rgba(255, 255, 255, 0x80));
}

/* The gold-framed window: its inside (a night blue to a sky blue), the frame over it; lit: a glow around it. */
static TK_UNUSED void Tk_Inside(f32 x0, f32 y0, f32 x1, f32 y1) {
    Ps_Rect(x0 + 4.0f, y0 + 4.0f, x1 - 4.0f, y1 - 4.0f, Ps_Rgba(40, 70, 170, 128), Ps_Rgba(150, 150, 190, 128));
}

static TK_UNUSED void Tk_Box(f32 x0, f32 y0, f32 x1, f32 y1, s32 lit) {
    if (lit) {
        Tk_Glow(x0 - 24.0f, y0 - 20.0f, x1 + 24.0f, y1 + 20.0f, 255, 190, 60, 0x70);
    }
    Tk_Inside(x0, y0, x1, y1);
    Ps_Panel(TBO_DB_FRAME, x0, y0, x1, y1, 0x80);
}

/* A small triangle pointing up (dir -1) or down (1): there is more of the list that way. */
static TK_UNUSED void Tk_Arrow(f32 cx, f32 cy, s32 dir) {
    u32 c = Ps_Rgba(0xFF, 0xE0, 0x70, 0x80);
    u32 e = Ps_Rgba(0x78, 0x28, 0x00, 0x80);
    f32 bob = 2.0f * sinf((f32)sTkFrames * 0.12f) * (f32)dir;
    f32 tip = cy + bob + 6.0f * (f32)dir;
    f32 base = cy + bob - 6.0f * (f32)dir;

    Ps_Quad(cx, tip + 2.0f * (f32)dir, cx, tip + 2.0f * (f32)dir, cx - 10.0f, base - 1.5f * (f32)dir, cx + 10.0f, base - 1.5f * (f32)dir, e, e, e, e);
    Ps_Quad(cx, tip, cx, tip, cx - 7.5f, base, cx + 7.5f, base, c, c, c, c);
}

/* The announcer, standing (kept for pages that want a host). */
static TK_UNUSED void Tk_Host(f32 cx, f32 feet, f32 height) {
    Ps_Stand(TBO_HOSTS_MIC_WAVE, cx, feet + 3.0f * sinf((f32)sTkFrames * 0.04f), height, 0x80);
}

/* `say` in a window, cut into lines that fit (up to three; the last one squeezed if it must). The line is cut again
   only when it changes (measuring is the fonts' work). */
#define TK_TALK_LINES 3
static char sTkSaid[256];
static char sTkLine[TK_TALK_LINES][128];
static s32 sTkLines;
static f32 sTkRoom;

/* `say` cut into lines `room` wide (up to three; the last one squeezed if it must): sTkLine, sTkLines. */
static TK_UNUSED void Tk_Wrap(char *say, f32 scale, f32 room) {
    char text[256];
    s32 at = 0;
    s32 k;

    if (say == NULL || say[0] == 0) {
        sTkLines = 0;
        sTkSaid[0] = 0;
        return;
    }
    Ps_Copy(text, (char *)Port_TagText(say), sizeof(text));
    if (strcmp(text, sTkSaid) == 0 && room == sTkRoom) {
        return;
    }
    Ps_Copy(sTkSaid, text, sizeof(sTkSaid));
    sTkRoom = room;
    sTkLines = 0;
    while (sTkLines < TK_TALK_LINES) { /* word by word while the line fits (one word at least; the last: the rest) */
        s32 end;

        while (text[at] == ' ') {
            at++;
        }
        if (text[at] == 0) {
            break;
        }
        end = at;
        for (;;) {
            s32 next = end;

            while (text[next] == ' ') {
                next++;
            }
            while (text[next] != 0 && text[next] != ' ') {
                next++;
            }
            if (next == end || next - at > 127) {
                break;
            }
            for (k = at; k < next; k++) {
                sTkLine[sTkLines][k - at] = text[k];
            }
            sTkLine[sTkLines][next - at] = 0;
            if (end > at && sTkLines < TK_TALK_LINES - 1 && Ps_TextWidth(sTkLine[sTkLines], scale) > room) {
                break;
            }
            end = next;
        }
        sTkLine[sTkLines][end - at] = 0;
        at = end;
        sTkLines++;
    }
}

/* The lines of Tk_Wrap, navy, centred one under another on `mid`, starting at x. */
static TK_UNUSED void Tk_WrapDraw(f32 x, f32 mid, f32 scale, f32 room) {
    s32 k;

    Ps_Align(0);
    for (k = 0; k < sTkLines; k++) {
        f32 m = mid + 22.0f * scale / 0.8f * ((f32)k - (f32)(sTkLines - 1) * 0.5f);

        Tk_NavyText(x, Ps_TextTop(m, scale), sTkLine[k], scale, room);
    }
    Ps_Align(1);
}

/* `say` in the white talk box. */
static TK_UNUSED void Tk_Talk(f32 x0, f32 y0, f32 x1, f32 y1, char *say, f32 scale) {
    f32 room = x1 - x0 - 28.0f;

    Ps_Panel(TBO_DB_DIALOG, x0, y0, x1, y1, 0x80);
    Tk_Wrap(say, scale, room);
    Tk_WrapDraw(x0 + 14.0f, (y0 + y1) * 0.5f, scale, room);
}

#endif
#endif
