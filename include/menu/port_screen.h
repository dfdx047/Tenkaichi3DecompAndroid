#ifndef MENU_PORT_SCREEN_H
#define MENU_PORT_SCREEN_H
#ifdef PORT
/*
 * PC only: the screens of the port's own (menu/port_screen.c). They run in one progress mode of their own, which
 * the game does not have; which screen runs is gPortScreen, set before the mode is chosen. See docs/port/menus.md.
 */
#define MPROG_MODE_PORT_SCREEN 80 /* (the game's modes end at 70) */

enum {
    PORT_SCREEN_NONE,
    PORT_SCREEN_TBO, /* Tenkaichi Budokai Online (menu/port_tbo.c) */
};

extern s32 gPortScreen; /* the screen the mode runs next (PORT_SCREEN_*) */

/* Progress_Main's handler of the mode: runs gPortScreen's screen and chooses the next mode. Non-zero when a battle
   was set up (the menus are left), as the game's handlers return. */
s32 PortScreen_Main(void);

/* The kit a screen draws with (menu/port_screen.c): a page of the game's 512 x 448 units in the middle of the window
   (Ps_PageSet once a frame); pictures of tbo/ui by their TBO_ number (include/menu/tbo_art.h); lines of letters; the
   pad's buttons and the prompts at the foot of the screen. */
#define PS_PLAIN 0x808080 /* a picture's tint: as it is */
#define PS_CELL_UNITS 27.5f    /* a line's height at scale 1, in the page's units */
#define PS_CAP_OF_CELL 0.49f   /* its capitals' height, and */
#define PS_BASE_OF_CELL 0.744f /* its baseline, from its top */
#define PS_UNIT_X 0.857f       /* a unit across is 7:6 of a unit down: letters keep their shape */
#define PS_MSG_CAP 11.5f
#define PS_MSG_BASE 18.25f
u32 Ps_Rgba(s32 r, s32 g, s32 b, s32 a);
void Ps_ScreenQuad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3);
void Ps_Copy(char *dst, const char *src, s32 max);
void Ps_Cat(char *dst, const char *src, s32 max);
s32 Ps_Len(const char *s);
s32 Ps_Move(s32 *cur, s32 count);
s32 Ps_Side(void);
s32 Ps_Wrap(s32 v, s32 lo, s32 hi);
void Ps_PageSet(void);
void Ps_SetShift(f32 x); /* what is drawn on the page is moved this far right (a page coming in) */
f32 Ps_X(f32 x);
f32 Ps_Left(void);
f32 Ps_Y(f32 y);
u32 Ps_Uv(f32 t);
void Ps_SpriteScreen(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha);
void Ps_Sprite(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha);
f32 Ps_PicW(s32 id);
f32 Ps_PicH(s32 id);
void Ps_Stand(s32 id, f32 cx, f32 feet, f32 height, s32 alpha);
void Ps_Icon(s32 id, f32 cx, f32 cy, f32 height, s32 alpha);
void Ps_Pill(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha);
void Ps_Panel(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha);
void Ps_Quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3);
void Ps_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom);
void Ps_Align(s32 align);
f32 Ps_TextWidth(char *str, f32 scale);
f32 Ps_TextTop(f32 middle, f32 scale);
u32 Ps_Tint(u32 c);
u32 Ps_Mix(u32 a, u32 b, f32 t);
void Ps_Line(s32 id, f32 x, f32 base, f32 cap, f32 fit, u32 top, u32 bottom, u32 line, s32 bands, s32 alpha);
void Ps_TextEx(f32 x, f32 y, char *str, f32 scale, f32 width, u32 top, u32 bottom, u32 line);
void Ps_Heading(f32 x, f32 y, char *str, f32 scale, f32 width, s32 alpha);
f32 Ps_MsgWidth(char *str, f32 scale);
void Ps_Msg(f32 x, f32 y, char *str, f32 scale, s32 alpha);
s32 Ps_ButtonPic(s32 kind);
f32 Ps_ButtonW(s32 kind, f32 size);
void Ps_Button(s32 kind, f32 cx, f32 cy, f32 size);
void Ps_PromptsClear(void); /* once a frame, before the page sets its prompts */
void Ps_Prompt(s32 kind, char *label);
f32 Ps_PromptX(s32 k);
void Ps_PromptsDraw(void);
void Ps_Text(f32 x, f32 y, char *s, f32 scale, f32 width, s32 style);
void Ps_Disc(f32 cx, f32 cy, f32 r, u32 mid, u32 edge);
#endif
#endif
