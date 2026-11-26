/* PC only: what the Mod Manager's two source files share (src/menu/port_modmgr.c: the Library and the drawing;
   src/menu/port_modmgr2.c: the other screens and the mod grid of the character select). */
#ifndef PORT_MODUI_H
#define PORT_MODUI_H

#include "menu/port_mods.h"
#include "menu/port_modart.h"

#define MM_ROWS 7 /* the Library's rows */
#define MM_MOVES_Y 300.0f /* the window of the chosen mod's moves, under the list */
#define MM_MAX 256
#define MM_UI_SIZE 0x118000
#define MM_SHOW_DELAY 12
#define MM_PITCH 72.0f  /* the Library's rows: their pitch and the first one's top (mockup pixels) */
#define MM_LIST_Y 150.0f
#define MM_ROW_X 30.0f   /* the Library's list: where a bar starts, */
#define MM_ROW_W 258.0f  /* how long it is */
#define MM_TAB_X 263.0f  /* the three tabs */
#define MM_TAB_PITCH 76.0f
#define MM_TAB_W 73.0f
#define MM_INFO_X 298.0f /* the window under the model */
#define MM_INFO_Y 288.0f
#define MM_INFO_W 204.0f
#define MM_CHARA_COUNT 161

/* which screen of the manager is up */
#define MM_LIBRARY 0
#define MM_ROSTER 1
#define MM_TRANSFORM 2
#define MM_BROWSER 3
#define MM_DETAIL 4
#define MM_PICKER 5
#define MM_KEYBOARD 6
#define MM_CROP 7
#define MM_ART 8
#define MM_REMOVED 9

#define MM_LOOK_DELAY 36 /* frames the browser's cursor rests on a folder before it is looked into (a big one stalls) */
#define MM_SORTS 4       /* the Library's orders: name, fighter, on first, priority */
#define MM_SHOWS 6       /* what it shows: all, on, off, replacements, new fighters, problems */

typedef struct MmTex {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ u32 pixSize;
    /* 0x0C */ u32 clutSize;
    /* 0x10 */ s32 cbpOfs;
    /* 0x14 */ u8 unk14[0x1C];
    /* 0x30 */ u64 tex0;
    /* 0x38 */ u8 *pix;
    /* 0x3C */ u8 *clut;
} MmTex;
typedef struct MmRes {
    /* 0x00 */ s32 count;
    /* 0x04 */ u8 unk4[0xC];
    /* 0x10 */ MmTex *tex;
} MmRes;

typedef struct ModMgr {
    MmRes *ui;
    s32 count;
    s32 cur;
    s32 top;
    s32 frames;
    s32 leaving;
    s32 test;       /* it is left for a test fight with the mod under the cursor */
    s32 rest;       /* frames the cursor has rested */
    s32 costume;
    s32 shown;      /* the key of the model asked for, -1 = none */
    s32 shownChara;
    s32 wantKey;    /* what the screen that is up wants to stand on the platform: a key (-1 none), */
    s32 wantChara;  /* the character, */
    s32 wantCostume;
    s32 wantPreview; /* and whose files it is made of (a mod index, -2 the browser's folder) */
    s32 flash;      /* frames left of the flash of a switch */
    s32 note;       /* frames left of a line of news; */
    char noteLine[64]; /* the line */
    s32 confirm;    /* the question before a mod is removed is open */
    s32 confirmT;   /* frames since it opened */
    f32 curY;       /* where the cursor's plate is drawn: it glides */
    f32 yaw;
    s32 mode;       /* MM_ */
    s32 modeT;      /* frames since it came up */
    s32 back;       /* the screen a picker / keyboard / crop returns to */
    PortModInfo info[MM_MAX];
    MmRes *face[MM_MAX]; /* the gauge face of the fighter a mod replaces; NULL = not read (or none) */
    u8 faceTried[MM_MAX];
    MmRes *chip[MM_MAX]; /* a mod's chip (Port_ModChip) */
    u8 chipTried[MM_MAX];
    /* the browser */
    char path[400];
    s32 dirCount;
    s32 dirCur;
    s32 dirTop;
    s32 dirLooked;  /* the entry the info below is of, -1 = none */
    s32 dirIsMod;
    PortModInfo dirInfo;
    /* a mod's details */
    s32 detCur;
    /* the picker: a list of fighters (and, for a transformation, of new ones) */
    s32 pickCur;
    s32 pickTop;
    s32 pickCount;
    s32 pickWhat;   /* 0 the mod's target / template, 1 the fighter the transformations are of, 2 a slot's target */
    s32 pickRef[MM_CHARA_COUNT + MM_MAX + 1]; /* -1 none, a character id, or PORT_REF_MOD + mod */
    /* what is being searched for (the browser, the picker), typed on the PC keyboard */
    char search[40];
    s32 searchLen;
    s32 pickAllCount;
    s32 pickAll[MM_CHARA_COUNT + MM_MAX + 1];
    s32 typing;     /* the search line is open: the PC keyboard is letters until enter or escape */
    s32 browseWhat; /* what the browser is open for: 0 a mod to add, 1 a picture for a portrait */
    s32 art[4];     /* the portrait being made: where its picture sits (x, y), how wide and how high (percent) */
    s32 artRow;     /* which of size / width / height L2 and R2 change */
    /* the keyboard */
    char keyText[208];
    s32 keyLen;
    s32 keyCol;
    s32 keyRow;
    s32 keyLower;
    s32 keyWhich;   /* 0 white name, 1 yellow name, 2 author, 3 version, 4 description */
    /* the chip's square */
    s32 crop[3];
    u8 *artFile;    /* the portrait being cut: packed, */
    u8 *artRes;     /* unpacked, */
    MmTex *artTex;  /* its texture (NULL: none) */
    /* the roster */
    s32 rosCur;
    /* the transformations */
    s32 trWho;      /* a character id or PORT_REF_MOD + mod; -1 = none chosen */
    s32 trCur;
    PortTrans tr;
    s32 trPage;     /* 0 transformations, 1 fusions */
    s32 trFocus;    /* 1: in the fields of the slot under the cursor */
    s32 trField;    /* which field */
    s32 trAdv;      /* the advanced fields (the game's raw type, effect, scene partner) are shown */
    s32 trList;     /* the list of the field's choices is open */
    s32 trListCur;
    /* the Library's list: which mods it shows, in what order (sMm->cur is the mod under the cursor, top the first
       row shown) */
    s32 view[MM_MAX];
    s32 viewCount;
    s32 vcur;       /* the cursor's place in the list */
    s32 sort;       /* 0 name, 1 fighter, 2 on first, 3 priority */
    s32 filter;     /* 0 all, 1 on, 2 off, 3 replacements, 4 new fighters, 5 problems */
    char find[32];  /* what the list is searched for, "" nothing */
    s32 findLen;
    s32 finding;    /* the search line is open (the PC keyboard types into it) */
    /* the mod's own menu (cross on a mod): open, its line, frames since it opened */
    s32 act;
    s32 actCur;
    s32 actT;
    /* the Library's menu (square) */
    s32 menu;
    s32 menuCur;
    s32 menuT;
    /* adding mods on the library's thread */
    s32 importing;  /* 1 while it runs */
    s32 importFrom; /* who started it: 0 a drop, 1 the browser (one mod), 2 the browser (all in a folder) */
    s32 prog[3];    /* files done, of how many, what it does */
    s32 watchT;     /* frames until the mods folder is looked at for changes again */
    /* the maker's lines of the mod under the cursor (Port_ModAbout) */
    char about[248];
    s32 aboutOf;
    /* the removed mods */
    s32 goneCount;
    s32 goneCur;
    s32 goneTop;
} ModMgr;

extern ModMgr *sMm;

u32 Mm_Rgba(s32 r, s32 g, s32 b, s32 a);
void Mm_Quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3);
void Mm_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom);
void Mm_Slant(f32 x0, f32 y0, f32 x1, f32 y1, f32 lean, u32 top, u32 bottom);
void Mm_Plate(f32 x0, f32 y0, f32 x1, f32 y1, f32 lean, f32 t, u32 edge, u32 top, u32 bottom);
void Mm_Tex(MmTex *t, f32 x0, f32 y0, f32 x1, f32 y1, s32 u0, s32 v0, s32 u1, s32 v1, s32 shade, s32 alpha);
void Mm_Text(s32 x, s32 y, char *str, f32 scale, s32 r, s32 g, s32 b, s32 a);
void Mm_TextEx(f32 x, f32 y, char *str, f32 scale, f32 width, u32 top, u32 bottom, u32 line);
f32 Mm_TextWidth(char *str, f32 scale);
void Mm_Align(s32 align);  /* 0 from x on, 1 centred on x, 2 up to x */
void Mm_Pill(s32 piece, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha);
void Mm_Panel(s32 piece, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha);
void Mm_Icon(s32 piece, f32 cx, f32 cy, f32 size, s32 alpha);
MmRes *Mm_UiOpen(void);
/* The model viewer for the mod grid of the character select (port_modmgr.c). */
void Mm_ViewOpen(void);
void Mm_ViewClose(void);
void Mm_ViewWant(s32 key, s32 chara, s32 costume, s32 mod, s32 rested);
s32 Mm_ViewReady(void);
s32 Mm_ViewKey(void);
void *Mm_ViewTex(void); /* the texture file (TexFile) of the model on screen, NULL none */
void *Mm_ViewExpr(s32 i); /* its expression texture files: 0 the eyes, 1..4 the mouths; NULL none */
void *Mm_ViewModel(void); /* its model file, NULL none */
void Mm_ViewCamera(f32 yaw, f32 pitch, s32 tpose); /* turned by hand, tilted, T-pose */
void Mm_ViewPair(s32 key0, s32 chara0, s32 costume0, s32 mod0, s32 key1, s32 chara1, s32 costume1, s32 mod1, s32 rested);
void Mm_ViewPairDraw(const f32 *place0, f32 yaw0, const f32 *place1, f32 yaw1);
void Mm_ViewDraw(f32 cx, f32 feet, f32 height);
void Mm_UiClose(void);
void Mm_TexReset(void);
void Mm_TextFit(s32 x, s32 y, char *str, f32 scale, f32 width, s32 r, s32 g, s32 b, s32 a);
f32 Mm_Ease(f32 t);
f32 Mm_In(s32 delay);
f32 Mm_Beat(s32 period);
f32 Mm_Fit(char *str, f32 scale, f32 width);
void Mm_Button(s32 kind, f32 cx, f32 cy);
void Mm_Key(f32 cx, f32 cy, f32 w);
void Mm_TextBegin(void);
void Mm_TextEnd(void);
void Mm_Refresh(void);
void Mm_FreeFaces(void);
MmTex *Mm_Face(s32 i);
MmTex *Mm_Chip(s32 i);
s32 Mm_HasCostume(s32 i, s32 n);
void Mm_SetMode(s32 mode);
void Mm_Note(char *line);
s32 Mm_ShowImport(s32 n); /* after mods were added: the list again, the cursor on the first, a line of news */
void Mm_StartImport(char *path, s32 all, s32 from); /* adds on the library's thread; the screen shows how far */
void Mm_Select(s32 mod);  /* the Library's cursor onto a mod (the search and filter go when they hide it) */
void Mm2_OpenRemoved(void);
void Mm2_BrowserKeep(void); /* the browser opens in the folder it is in now, next time */
char *Mm_RefName(s32 ref);
extern s32 gMmTestFoe; /* the fighter the test fight is against */
void Mm2_OpenFoePicker(void);
void Mm_PromptReset(void);
void Mm_Prompt(s32 kind, char *label);
void Mm_PromptsDraw(f32 rise); /* the bar of the buttons at the foot (other screens) */
void Mm_PromptsText(f32 rise);

/* port_mmkit.c: the look of the approved 16:9 mockups (modmgr_design/mockups), placed in their pixels */
#define MX_W 1376.0f
#define MX_H 768.0f
#define MX_WHITE 0 /* the letters' colours */
#define MX_GOLD 1
#define MX_LAV 2
#define MX_CYAN 3
#define MX_DIM 4
#define MX_RED 5
#define MX_DARK 6
#define MX_BLUE 7
#define MX_ORANGE 8
#define MX_GREEN 9
#define MX_STYLES 10
#define MX_FOOT_Y0 694.0f /* the bar of the buttons at the foot */
#define MX_FOOT_Y1 750.0f
void Mx_Begin(void);
f32 Mx_X(f32 x);
f32 Mx_Y(f32 y);
f32 Mx_ScreenFracX(f32 x);
f32 Mx_ScreenFracY(f32 y);
f32 Mx_SizeFracY(f32 l);
f32 Mx_LenX(f32 l); /* how far a size reaches across / down, in places */
f32 Mx_LenY(f32 l);
f32 Mx_SameX(f32 dy); /* the places across as long as dy down */
u32 Mx_Rgba(s32 r, s32 g, s32 b, s32 a);
void Mx_PicEx(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha);
void Mx_Pic(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha);
void Mx_Fit(s32 id, f32 cx, f32 cy, f32 h, s32 alpha);
void Mx_Bar(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 alpha);
void Mx_BarTint(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, u32 tint, s32 alpha);
void Mx_Box(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 r, s32 alpha);
void Mx_BoxTint(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 r, u32 tint, s32 alpha);
void Mx_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom);
void Mx_Frame(f32 x0, f32 y0, f32 x1, f32 y1, f32 t, u32 c);
void Mx_Tex(MmTex *t, f32 x0, f32 y0, f32 x1, f32 y1, s32 u0, s32 v0, s32 u1, s32 v1, s32 shade, s32 alpha);
f32 Mx_TextW(char *s, f32 cap);
void Mx_Text(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 style, s32 align); /* align 0 from x, 1 centred, 2 to x */
void Mx_TextA(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 style, s32 align, s32 alpha);
void Mx_TextC(f32 x, f32 cy, char *s, f32 cap, f32 width, u32 top, u32 bottom, u32 line, s32 align, s32 alpha);
void Mx_Heading(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 align);
s32 Mx_Wrap(char *text, f32 cap, f32 width, char (*out)[64], s32 max);
char *Mx_Tail(char *s, f32 cap, f32 width);
/* Sentences, in the game's own font (its message box's): as Mx_Text, Mx_TextC, Mx_TextW, Mx_Wrap. */
void Mx_Say(f32 x, f32 cy, char *s, f32 cap, f32 width, s32 style, s32 align);
void Mx_SayC(f32 x, f32 cy, char *s, f32 cap, f32 width, u32 top, u32 bottom, u32 line, s32 align, s32 alpha);
void Mx_SayRgb(f32 x, f32 cy, char *s, f32 cap, f32 width, u32 rgb, s32 align, s32 alpha);
f32 Mx_SayW(char *s, f32 cap);
s32 Mx_SayWrap(char *text, f32 cap, f32 width, char (*out)[64], s32 max);
f32 Mx_ButtonW(s32 kind, f32 h);
void Mx_Button(s32 kind, f32 x0, f32 cy, f32 h);
void Mx_Footer(f32 x0, f32 x1, f32 y0, f32 y1);
void Mx_Background(s32 dusk);
void Mx_Chrome(char *title, s32 tab, f32 drop);
void Mx_HeaderInit(void);
void Mx_HeaderTerm(void);
void Mx_HeaderDraw(void);
void Mx_SubTabs(char *a, char *b, s32 lit, f32 y);
void Mx_Label(f32 x0, f32 y0, f32 x1, f32 y1, char *s);
void Mx_Note(char *line, s32 left, f32 y);
void Mx_Pips(f32 x0, f32 cy, s32 n, s32 max, f32 size);
void Mx_FreeFaces(void);
void Mx_ChipsOpen(void);  /* the character select's faces: read when the manager opens */
void Mx_ChipsClose(void);
void Mx_Face(s32 ref, f32 x0, f32 y0, f32 x1, f32 y1, s32 shade);
void Mx_FaceCell(s32 ref, f32 x0, f32 y0, f32 x1, f32 y1, s32 lit);
void Mx_Disc(f32 cx, f32 cy, f32 d, u32 top, u32 bottom);                /* a round shape, d high */
void Mx_FaceRound(s32 chara, f32 cx, f32 cy, f32 d, s32 shade);          /* a fighter's face cut round */
void Mx_TexRound(MmTex *t, f32 cx, f32 cy, f32 d, s32 shade);           /* a 64x64 picture cut round */
void Mx_Platform(f32 cx, f32 feet, f32 w);
void Mx_Brackets(f32 x0, f32 y0, f32 x1, f32 y1, f32 len, f32 t, u32 c);
void Mx_Window(f32 x0, f32 y0, f32 x1, f32 y1);
void Mx_Glow(f32 cx, f32 feet, f32 h, u32 c, s32 time);
void Mx_Bolts(f32 cx, f32 feet, f32 h, s32 time);
void Mx_Row(f32 x0, f32 y0, f32 x1, f32 y1, s32 sel, s32 alpha);
void Mm_SetModel(f32 cx, f32 feet, f32 height);
s32 Mm_ViewObjId(void);  /* the viewer's model's object id, -1 none */
void *Mm_ViewObjPtr(void); /* the object, NULL none */
void Mm_SetExtra(s32 k, s32 ref, f32 cx, f32 feet, f32 height); /* a model beside it (k 0..1), this frame */
void Mm_SetExtraMotion(s32 motion); /* the model motion (0..7) those play, this frame; -1 their stance */
void Mm_ExtrasReload(void);     /* they are read again (their files changed) */
void Mm_SetGlow(f32 cx, f32 feet, f32 height, u32 c); /* an aura behind a model, this frame */
void Mm_SetGlowBolts(s32 on); /* with lightning */ /* where the screen wants its model: its feet, how tall (mockup pixels) */

/* port_modmgr2.c */
void Mm2_Update(s32 live); /* input (when live) and what the screen wants on the platform */
void Mm2_Draw(void);    /* panels */
void Mm2_OpenBrowser(void);
void Mm2_OpenDetail(void);
void Mm2_Term(void);

/* touch (port_mmkit.c) */
#define MX_TAPS 160
typedef struct MxTapRect {
    f32 x0, y0, x1, y1;
    s32 kind;
} MxTapRect;
extern MxTapRect gMxTap[MX_TAPS];
extern s32 gMxTapCount;

#endif
