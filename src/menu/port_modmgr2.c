#include "common.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "sys/save.h"

#ifdef PORT
/*
 * PC only: the Mod Manager's other screens (the Library, the drawing and the frame loop are in port_modmgr.c).
 *
 *   ROSTER      the mod grid: the new fighters in the order the character select shows them (square there);
 *               L2 / R2 move the one under the cursor
 *   TRANSFORM   a fighter's four transformations: target and blast stock cost per slot, the slot plain R3 takes
 *   the browser folders and drives, a mod's model and contents shown while the cursor rests on its folder; square
 *               copies the folder into the library
 *   details     what a mod is: replacing a fighter or a new one, which fighter / template, its two names (typed
 *               on a keyboard drawn here), the square of its portrait that is its face (moved and sized here)
 *
 * and, outside the manager:
 *   PortMods_PatchSelect   called by the select screens when their pack is unpacked: the names and faces of the
 *                          mods that replace a fighter go into the pack's own tables
 *   PortModGrid_Pick       the mod grid itself, opened with square from the character select
 */
#include "menu/port_modui.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"

#define ROWS 9
#define BR_ROWS 7 /* the browser's rows */
#define RM_ROWS 6 /* the removed mods' */
#define ROW_H 36.0f
#define ROW_Y 76.0f
#define ART_FILE_SIZE 0x16800
#define ART_RES_SIZE 0x20800

extern s32 ChrView_Hide(void);
extern void Job_Run(void);
extern void Font_SetScale(f32 scale);
extern void Font_SetScaleXY(f32 sx, f32 sy);
extern void Mm_Align(s32 align);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_PrintAsciiAt(s32 x, s32 y, char *str);
extern void *Sprite_Unpack(void *src, void *dst, s32 *size);

static PortDirEntry sEnt[ROWS];
static char sFull[500];

/* ------------------------------------------------------------------------------------------------- small things */
static s32 Str_Len(char *s) {
    s32 n = 0;

    while (s[n] != 0) {
        n++;
    }
    return n;
}

static void Str_Copy(char *dst, char *src, s32 max) {
    s32 i;

    for (i = 0; i < max - 1 && src[i] != 0; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

/* One of two strings (a conditional expression of a literal and one of the game's own pointers does not compile
   in this build: the two are different kinds of pointer until they are passed). */
static char *Either(s32 cond, char *a, char *b) {
    if (cond) {
        return a;
    }
    return b;
}

/* Whether a file name is an archive the library unpacks (zip, 7z, rar). */
static s32 Is_Archive(char *name) {
    s32 n = Str_Len(name);
    s32 k;

    for (k = n - 1; k > 0 && name[k] != '.'; k--) {
    }
    if (k <= 0) {
        return 0;
    }
    name += k + 1;
    return ((name[0] | 32) == 'z' && (name[1] | 32) == 'i' && (name[2] | 32) == 'p' && name[3] == 0) || (name[0] == '7' && (name[1] | 32) == 'z' && name[2] == 0) ||
           ((name[0] | 32) == 'r' && (name[1] | 32) == 'a' && (name[2] | 32) == 'r' && name[3] == 0);
}

/* The search line of a screen. It is opened with a button (R1): from then on the PC keyboard types into it, until
   enter (the search stays) or escape (it is cleared). Returns 1 when the line changed this frame. */
static s32 Search_Type(void) {
    s32 changed = 0;
    s32 c;

    while ((c = Port_TextRead()) != 0) {
        if (c == 13 || c == 27) {
            if (c == 27 && sMm->searchLen > 0) {
                sMm->searchLen = 0;
                sMm->search[0] = 0;
                changed = 1;
            }
            sMm->typing = 0;
            Port_TextMode(0);
            break;
        } else if (c == 8) {
            if (sMm->searchLen > 0) {
                sMm->search[--sMm->searchLen] = 0;
                changed = 1;
            }
        } else if (c >= 32 && c < 127 && sMm->searchLen < 30) {
            sMm->search[sMm->searchLen++] = (char)c;
            sMm->search[sMm->searchLen] = 0;
            changed = 1;
        }
    }
    return changed;
}

static void Search_Open(void) {
    sMm->typing = 1;
    Port_TextMode(1);
    Snd_PlaySe(1, 1);
}

static void Search_Clear(void) {
    sMm->searchLen = 0;
    sMm->search[0] = 0;
}

/* The search line as it is shown: what was typed and a blinking mark, or a hint. */
static char *Search_Shown(void) {
    static char line[64];

    if (sMm->typing) {
        sprintf(line, "SEARCH: %s%s", sMm->search, Either((sMm->frames / 16) & 1, "_", " "));
    } else if (sMm->searchLen == 0) {
        return "R1: SEARCH";
    } else {
        sprintf(line, "SEARCH: %s", sMm->search);
    }
    return line;
}

/* Whether `name` has the search text in it (any case). */
static s32 Search_Has(char *name) {
    s32 i;
    s32 k;

    if (sMm->searchLen == 0) {
        return 1;
    }
    for (i = 0; name[i] != 0; i++) {
        for (k = 0; k < sMm->searchLen; k++) {
            s32 a = name[i + k];
            s32 b = sMm->search[k];

            if (a >= 'A' && a <= 'Z') {
                a += 32;
            }
            if (b >= 'A' && b <= 'Z') {
                b += 32;
            }
            if (a != b) {
                break;
            }
        }
        if (k == sMm->searchLen) {
            return 1;
        }
    }
    return 0;
}

/* The cursor of a grid of `cols` columns: the directions move it, L2 / R2 a page. */
static void Grid_Move(s32 *cur, s32 count, s32 cols, s32 page) {
    Pad *pad = &gPad[0];
    s32 was = *cur;

    if (count <= 0) {
        return;
    }
    if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        *cur = (*cur + 1) % count;
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        *cur = (*cur + count - 1) % count;
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        *cur = *cur + cols < count ? *cur + cols : *cur % cols;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        if (*cur >= cols) {
            *cur -= cols;
        } else {
            *cur = (count - 1) / cols * cols + *cur % cols;
            if (*cur >= count) {
                *cur -= cols;
            }
        }
    } else if (pad->gameRepeat & PADG_R2) {
        *cur = *cur + page < count ? *cur + page : count - 1;
    } else if (pad->gameRepeat & PADG_L2) {
        *cur = *cur >= page ? *cur - page : 0;
    }
    if (*cur != was) {
        Snd_PlaySe(1, 0);
        sMm->rest = 0;
    }
}

static void List_Move(s32 *cur, s32 *top, s32 count, s32 rows) {
    Pad *pad = &gPad[0];
    s32 was = *cur;

    if (count <= 0) {
        return;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        *cur = (*cur + 1) % count;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        *cur = (*cur + count - 1) % count;
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        *cur = *cur + rows < count ? *cur + rows : count - 1;
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        *cur = *cur >= rows ? *cur - rows : 0;
    }
    if (*cur != was) {
        Snd_PlaySe(1, 0);
        sMm->rest = 0;
    }
    if (*cur < *top) {
        *top = *cur;
    }
    if (*cur >= *top + rows) {
        *top = *cur - rows + 1;
    }
}

/* The portrait of a mod (the picture the select screens show large), unpacked into this screen's buffers. */
static void Art_Load(s32 mod) {
    MmRes *res;

    sMm->artTex = NULL;
    if (sMm->artFile == NULL) {
        sMm->artFile = Heap_Alloc(ART_FILE_SIZE, 0x40, 0, 2);
        sMm->artRes = Heap_Alloc(ART_RES_SIZE, 0x40, 0, 2);
    }
    if (Port_ModPortrait(mod, sMm->artFile, ART_FILE_SIZE) < 0x100 || *(u32 *)sMm->artFile > ART_RES_SIZE) {
        return;
    }
    Sprite_Unpack(sMm->artFile, sMm->artRes, NULL);
    res = (MmRes *)sMm->artRes;
    Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
    sMm->artTex = res->tex;
}

void Mm2_Term(void) {
    if (sMm->artFile != NULL) {
        Heap_Free(sMm->artFile);
        Heap_Free(sMm->artRes);
        sMm->artFile = NULL;
        sMm->artRes = NULL;
    }
}

/* ------------------------------------------------------------------------------------------------- the browser */
static void Browser_List(void) {
    Search_Clear();
    Port_DirKind(sMm->browseWhat);
    sMm->dirCount = Port_DirOpen(sMm->path);
    sMm->dirCur = 0;
    sMm->dirTop = 0;
    sMm->dirLooked = -1;
    sMm->dirIsMod = 0;
    sMm->rest = 0;
}

/* Where the browser opens: the folder it was in last (this session), else the Downloads folder, where mods land. */
static char sLastPath[400];

static void Browser_Start(void) {
    extern char *getenv(const char *name);
    char *home;
    s32 i;

    if (sLastPath[0] != 0) {
        Str_Copy(sMm->path, sLastPath, 400);
        return;
    }
    sMm->path[0] = 0;
    home = getenv("USERPROFILE");
    if (home != NULL && Str_Len(home) < 360) {
        sprintf(sMm->path, "%s/Downloads", home);
        for (i = 0; sMm->path[i] != 0; i++) {
            if (sMm->path[i] == '\\') {
                sMm->path[i] = '/';
            }
        }
    }
}

void Mm2_OpenBrowser(void) {
    sMm->browseWhat = 0;
    Browser_Start();
    Mm_SetMode(MM_BROWSER);
    Browser_List();
    if (sMm->dirCount == 0 && sMm->path[0] != 0) { /* (a folder that is gone, or empty: the drives) */
        sMm->path[0] = 0;
        Browser_List();
    }
}

static void Art_Base(void);

/* The browser for a picture file (a mod's portrait is made from it). */
static void Browser_OpenPicture(void) {
    sMm->browseWhat = 1;
    Browser_Start();
    Mm_SetMode(MM_BROWSER);
    Browser_List();
    if (sMm->dirCount == 0 && sMm->path[0] != 0) {
        sMm->path[0] = 0;
        Browser_List();
    }
}

/* Where the browser goes when it is closed (the folder it was in is kept for next time). */
static void Browser_Close(void) {
    Str_Copy(sLastPath, sMm->path, 400);
    Mm_SetMode(sMm->browseWhat == 1 ? MM_ART : MM_LIBRARY);
}

static void Browser_Full(s32 i) {
    PortDirEntry e;

    Port_DirGet(i, &e);
    if (sMm->path[0] == 0) {
        Str_Copy(sFull, e.name, 500);
    } else {
        sprintf(sFull, "%s/%s", sMm->path, e.name);
    }
}

static void Browser_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 was = sMm->dirCur;
    u32 typed = 0; /* a button the PC keyboard stands for this frame */

    sMm->wantKey = -1;
    if (sMm->dirIsMod == 1 && sMm->dirLooked == sMm->dirCur) { /* (a batch is not shown: it is several) */
        sMm->wantKey = 0x10000 + sMm->dirCur;
        sMm->wantChara = sMm->dirInfo.chara >= 0 ? sMm->dirInfo.chara : 0;
        sMm->wantCostume = 0;
        sMm->wantPreview = -2;
    }
    Mm_SetModel(1052.0f, 448.0f, 300.0f);
    if (sMm->typing) {
        Mm_Prompt(11, "DONE");
        Mm_Prompt(12, "CLEAR");
    } else if (sMm->browseWhat == 1) {
        PortDirEntry e;

        Port_DirGet(sMm->dirCur, &e);
        Mm_Prompt(0, sMm->dirCount > 0 && !e.isDir ? "USE PICTURE" : "OPEN");
        Mm_Prompt(8, "SEARCH");
        Mm_Prompt(2, "UP");
        Mm_Prompt(4, "CLOSE");
    } else {
        PortDirEntry e;

        Port_DirGet(sMm->dirCur, &e);
        if (sMm->dirCount > 0 && e.isDir) {
            Mm_Prompt(0, "OPEN");
        }
        if (sMm->dirCount > 0) {
            if (!e.isDir) {
                Mm_Prompt(1, Is_Archive(e.name) ? "ADD THIS ARCHIVE" : "ADD ITS FOLDER");
            } else {
                Mm_Prompt(1, sMm->dirLooked != sMm->dirCur ? "ADD" : sMm->dirIsMod == 1 ? "ADD THIS MOD" : sMm->dirIsMod == 2 ? "ADD EVERY FIGHTER" : "ADD ALL INSIDE");
            }
        }
        Mm_Prompt(8, "SEARCH");
        Mm_Prompt(2, "UP");
        Mm_Prompt(4, "CLOSE");
    }
    if (!live) {
        return;
    }
    /* a folder the cursor has rested on for a moment is looked into: is it a mod? (not at once: reading a folder of
       many files stalls a frame, and the cursor only passes over most of them) */
    if (sMm->rest >= MM_LOOK_DELAY && sMm->dirLooked != sMm->dirCur && sMm->dirCount > 0) {
        PortDirEntry e;

        Port_DirGet(sMm->dirCur, &e);
        sMm->dirLooked = sMm->dirCur;
        sMm->dirIsMod = 0;
        if (e.isDir && sMm->path[0] != 0 && sMm->browseWhat == 0) {
            Browser_Full(sMm->dirCur);
            if (sMm->shown >= 0x10000) { /* the folder on the platform goes first: its files are about to be another's */
                ChrView_Hide();
                sMm->shown = -1;
            }
            sMm->dirIsMod = Port_DirLook(sFull, &sMm->dirInfo);
        }
    }
    if (sMm->typing) { /* the search line is open: letters go into it, the directions still move the cursor */
        if (Search_Type()) {
            sMm->dirCount = Port_DirFilter(sMm->search);
            sMm->dirCur = 0;
            sMm->dirTop = 0;
            sMm->dirLooked = -1;
            sMm->dirIsMod = 0;
            sMm->rest = 0;
        }
        List_Move(&sMm->dirCur, &sMm->dirTop, sMm->dirCount, BR_ROWS);
        return;
    }
    if (pad->gamePressed & PADG_R1) {
        Search_Open();
        return;
    }
    List_Move(&sMm->dirCur, &sMm->dirTop, sMm->dirCount, BR_ROWS);
    if (sMm->dirCur != was) {
        return;
    }
    if (pad->gamePressed & PADG_CIRCLE) {
        Snd_PlaySe(1, 2);
        Browser_Close();
    } else if ((pad->gamePressed | typed) & PADG_TRIANGLE) {
        s32 n = Str_Len(sMm->path);

        Snd_PlaySe(1, 2);
        if (n == 0) {
            Browser_Close();
            return;
        }
        while (n > 0 && sMm->path[n - 1] != '/') {
            n--;
        }
        sMm->path[n > 0 ? n - 1 : 0] = 0;
        Browser_List();
    } else if (sMm->dirCount > 0 && ((pad->gamePressed | typed) & PADG_CROSS)) {
        PortDirEntry e;

        Port_DirGet(sMm->dirCur, &e);
        if (e.isDir) {
            Browser_Full(sMm->dirCur);
            if (Str_Len(sFull) < 380) {
                Str_Copy(sMm->path, sFull, 400);
                Snd_PlaySe(1, 1);
                Browser_List();
            }
        } else if (sMm->browseWhat == 1) { /* a picture: the mod's portrait is made from it */
            Browser_Full(sMm->dirCur);
            if (Port_ModArtSource(sMm->cur, sFull)) {
                Str_Copy(sLastPath, sMm->path, 400);
                sMm->art[0] = 0;
                sMm->art[1] = 0;
                sMm->art[2] = 100;
                sMm->art[3] = 100;
                Mm_SetMode(MM_ART);
                Art_Base();
                Mm_Note("MOVE AND SIZE IT, THEN PRESS CROSS TO SAVE");
                Snd_PlaySe(1, 1);
            } else {
                Mm_Note("CAN'T READ THAT PICTURE (USE PNG OR BMP)");
                Snd_PlaySe(1, 2);
            }
        }
    } else if (sMm->dirCount > 0 && sMm->browseWhat == 0 && ((pad->gamePressed | typed) & PADG_SQUARE)) {
        Browser_Full(sMm->dirCur);
        if (Str_Len(sFull) <= 3 && sFull[1] == ':') { /* (every folder of a whole drive would be searched and copied) */
            Mm_Note("OPEN A FOLDER FIRST, NOT A WHOLE DRIVE");
            Snd_PlaySe(1, 2);
            return;
        }
        /* one mod: that folder as it is; anything else (a batch, a folder of mods, an archive, a file): every mod in it */
        if (sMm->dirIsMod == 1 && sMm->dirLooked == sMm->dirCur) {
            Mm_StartImport(sFull, 0, 1);
        } else {
            Mm_StartImport(sFull, 1, 2);
        }
    }
}

/* The folder the browser is in is where it opens next time. */
void Mm2_BrowserKeep(void) {
    Str_Copy(sLastPath, sMm->path, 400);
}

/* The search line (the browser's, the picker's): "R1: SEARCH" and what is typed. */
static void Search_Draw(f32 x0, f32 y0, f32 x1, f32 y1) {
    static char line[64];
    f32 cy = (y0 + y1) * 0.5f;
    f32 cap = (y1 - y0) * 0.6f;
    f32 w;

    Mx_Box(sMm->typing ? MM_FIELD : MM_NAVBAR, x0, y0, x1, y1, (y1 - y0) * 0.5f, 0x80);
    if (sMm->typing) {
        Mx_Frame(x0 + 8.0f, y0 + 3.0f, x1 - 8.0f, y1 - 3.0f, 1.5f, Mx_Rgba(0xFF, 0xC8, 0x40, 0x60));
    }
    Mx_TextC(x0 + 22.0f, cy, "R1: SEARCH", cap, 0.0f, Mx_Rgba(0xE0, 0xD0, 0xFF, 0x80), Mx_Rgba(0xB8, 0xA0, 0xF0, 0x80), Mx_Rgba(0x20, 0x10, 0x48, 0x80), 0, 0x80);
    w = Mx_TextW("R1: SEARCH", cap) + 34.0f;
    sprintf(line, "%s%s", sMm->search, sMm->typing && ((sMm->frames / 16) & 1) ? "_" : "");
    Mx_Text(x0 + w, cy, line, cap, x1 - x0 - w - 20.0f, MX_WHITE, 0);
}

#define BR_X0 105.0f
#define BR_X1 705.0f
#define BR_Y 168.0f
#define BR_PITCH 69.0f
#define BR_H 58.0f

static void Browser_Draw(void) {
    static char line[128];
    s32 looked = sMm->dirLooked == sMm->dirCur && sMm->dirIsMod != 0;
    s32 row;
    s32 k;

    Mx_Label(145.0f, 97.0f, 570.0f, 129.0f, sMm->browseWhat == 1 ? "PICK A PICTURE" : "ADD MODS");
    Mx_Box(MM_FIELD, 600.0f, 94.0f, 962.0f, 132.0f, 19.0f, 0x80);
    Mx_Fit(MM_FOLDER, 630.0f, 113.0f, 28.0f, 0x80);
    Mx_Text(656.0f, 113.0f, Mx_Tail(Either(sMm->path[0] != 0, sMm->path, "THIS PC"), 22.0f, 290.0f), 22.0f, 0.0f, MX_WHITE, 0);
    Search_Draw(980.0f, 94.0f, 1345.0f, 132.0f);
    for (row = 0; row < BR_ROWS; row++) {
        s32 i = sMm->dirTop + row;
        f32 y0 = BR_Y + BR_PITCH * (f32)row;
        s32 sel = i == sMm->dirCur;
        s32 icon = -1;

        if (i >= sMm->dirCount) {
            break;
        }
        Port_DirGet(i, &sEnt[row]);
        Mx_Row(BR_X0, y0, BR_X1, y0 + BR_H, sel, sEnt[row].isDir || sel ? 0x80 : 0x70);
        if (sEnt[row].isDir) {
            icon = sel && looked ? (sMm->dirIsMod == 2 ? MM_FOLDER_BATCH : MM_FOLDER_MOD) : MM_FOLDER;
        } else if (Is_Archive(sEnt[row].name)) {
            icon = MM_ZIP;
        }
        if (icon >= 0) {
            Mx_Fit(icon, BR_X0 + 50.0f, y0 + BR_H * 0.5f, 46.0f, 0x80);
        } else { /* a file: a picture (portraits) or a mod's own file */
            Mx_Box(MM_CELL, BR_X0 + 34.0f, y0 + 12.0f, BR_X0 + 66.0f, y0 + 46.0f, 6.0f, 0x80);
        }
        Mx_Text(BR_X0 + 88.0f, y0 + BR_H * 0.5f, sEnt[row].name, 28.0f, BR_X1 - BR_X0 - 120.0f, sEnt[row].isDir || sel ? MX_WHITE : MX_LAV, 0);
    }
    if (sMm->dirTop > 0) {
        Mx_Fit(MM_UP, (BR_X0 + BR_X1) * 0.5f, BR_Y - 15.0f - 3.0f * Mm_Beat(36), 26.0f, 0x80);
    }
    if (sMm->dirTop + BR_ROWS < sMm->dirCount) {
        Mx_Fit(MM_DOWN, (BR_X0 + BR_X1) * 0.5f, BR_Y + BR_PITCH * (f32)BR_ROWS + 2.0f + 3.0f * Mm_Beat(36), 26.0f, 0x80);
    }
    if (sMm->dirCount == 0) {
        Mx_Window(BR_X0, 230.0f, BR_X1, 360.0f);
        Mx_Text((BR_X0 + BR_X1) * 0.5f, 278.0f, Either(sMm->searchLen > 0, "NOTHING MATCHES THAT", "NOTHING HERE"), 30.0f, 0.0f, MX_GOLD, 1);
        Mx_Say((BR_X0 + BR_X1) * 0.5f, 322.0f, Either(sMm->browseWhat == 1, "No folders or pictures in this folder", "No folders or mod files in this folder"), 23.0f, 540.0f,
                MX_WHITE, 1);
    }
    /* at the right: what the folder under the cursor holds */
    if (sMm->browseWhat == 1) {
        Mx_Window(805.0f, 210.0f, 1330.0f, 520.0f);
        Mx_Text(1067.0f, 250.0f, "PICK A PICTURE", 31.0f, 0.0f, MX_GOLD, 1);
        Mx_Text(1067.0f, 305.0f, "A PNG or BMP becomes the", 25.0f, 480.0f, MX_WHITE, 1);
        Mx_Text(1067.0f, 340.0f, "big portrait on the", 25.0f, 480.0f, MX_WHITE, 1);
        Mx_Text(1067.0f, 375.0f, "character select.", 25.0f, 480.0f, MX_WHITE, 1);
        Mx_Text(1067.0f, 440.0f, "Tip: a see-through background", 23.0f, 480.0f, MX_CYAN, 1);
        Mx_Text(1067.0f, 472.0f, "looks best.", 23.0f, 480.0f, MX_CYAN, 1);
    } else if (looked && sMm->dirIsMod == 2) { /* a batch: the faces of its fighters over the light, their names */
        s32 n = sMm->dirInfo.charas > 8 ? 8 : sMm->dirInfo.charas;

        Mx_Platform(1053.0f, 330.0f, 380.0f);
        for (k = 0; k < n; k++) {
            f32 off = (f32)k - (f32)(n - 1) * 0.5f;
            f32 cx = 1053.0f + off * 66.0f;
            f32 cy = 240.0f + (off < 0.0f ? -off : off) * 9.0f - (k & 1) * 26.0f;

            Mx_FaceCell(sMm->dirInfo.charaList[k], cx - 38.0f, cy - 44.0f, cx + 38.0f, cy + 44.0f, 0);
        }
        Mx_Box(MM_WINDOW, 790.0f, 385.0f, 1336.0f, 692.0f, 22.0f, 0x80);
        Mx_Text(1063.0f, 412.0f, "A BATCH OF MODS", 30.0f, 0.0f, MX_GOLD, 1);
        sprintf(line, "%d FIGHTERS INSIDE", (int)sMm->dirInfo.charas);
        Mx_Text(1063.0f, 447.0f, line, 30.0f, 0.0f, MX_GREEN, 1);
        Mx_SayC(1063.0f, 479.0f, "Square adds each one as its own mod", 22.0f, 500.0f, Mx_Rgba(0xE8, 0xD8, 0xFF, 0x80), Mx_Rgba(0xC0, 0xA8, 0xF0, 0x80), Mx_Rgba(0x20, 0x10, 0x48, 0x80), 1,
                 0x80);
        Mx_Box(MM_WINDOW_DARK, 820.0f, 500.0f, 1306.0f, 682.0f, 14.0f, 0x80);
        for (k = 0; k < n && k < 6; k++) {
            sprintf(line, "%.40s - %.40s", sEnt[sMm->dirCur - sMm->dirTop >= 0 && sMm->dirCur - sMm->dirTop < BR_ROWS ? sMm->dirCur - sMm->dirTop : 0].name,
                    Mm_RefName(sMm->dirInfo.charaList[k]));
            Mx_Text(845.0f, 521.0f + 29.0f * (f32)k, line, 25.0f, 440.0f, MX_WHITE, 0);
        }
    } else if (looked) { /* one mod: it stands on the light (Browser_Update), what it is under it */
        static const s32 bit[3] = {0x100, 0x200, 0x400};
        static char *kindName[3] = {"ANIMS", "EFFECTS", "PORTRAIT"};
        f32 x = 840.0f;

        Mx_Window(805.0f, 485.0f, 1300.0f, 690.0f);
        Mx_Text(1052.0f, 511.0f, "CHARACTER MOD FOUND", 31.0f, 450.0f, MX_GOLD, 1);
        Mx_Text(1052.0f, 575.0f, sMm->dirInfo.chara >= 0 ? sMm->dirInfo.target : "A fighter it doesn't name", 30.0f, 450.0f, MX_WHITE, 1);
        sprintf(line, sMm->dirInfo.costumes == 1 ? "%d COSTUME" : "%d COSTUMES", (int)sMm->dirInfo.costumes);
        for (k = -1; k < 3; k++) {
            char *s = k < 0 ? line : kindName[k];
            f32 tw = Mx_TextW(s, 19.0f) + 28.0f;

            if (k >= 0 && !(sMm->dirInfo.kinds & bit[k])) {
                continue;
            }
            Mx_BarTint(MM_TAB, x, 645.0f, x + tw, 675.0f, Mx_Rgba(0x80, 0x70, 0xA0, 0x80), 0x80);
            Mx_TextC(x + tw * 0.5f, 660.0f, s, 19.0f, 0.0f, Mx_Rgba(0xF4, 0xEC, 0xFF, 0x80), Mx_Rgba(0xD8, 0xC8, 0xFF, 0x80), Mx_Rgba(0x24, 0x12, 0x4C, 0x80), 1, 0x80);
            x += tw + 14.0f;
        }
    } else {
        Mx_Window(805.0f, 485.0f, 1300.0f, 690.0f);
        if (sMm->dirLooked != sMm->dirCur && sMm->dirCount > 0) {
            Mx_Text(1052.0f, 515.0f, Either((sMm->frames / 12) & 1, "LOOKING...", "LOOKING.."), 28.0f, 0.0f, MX_GREEN, 1);
        } else {
            Mx_Text(1052.0f, 515.0f, "FIND A MOD", 28.0f, 0.0f, MX_GOLD, 1);
        }
        Mx_Say(1052.0f, 565.0f, "Rest on a mod folder or a zip", 24.0f, 450.0f, MX_WHITE, 1);
        Mx_Say(1052.0f, 598.0f, "to see it here.", 24.0f, 450.0f, MX_WHITE, 1);
        Mx_Say(1052.0f, 645.0f, "Square on a folder of mods adds them all.", 21.0f, 450.0f, MX_CYAN, 1);
    }
}

/* ------------------------------------------------------------------------------------------------- a mod's details */
#define DET_ROWS 9

/* The maker's lines of a mod (Port_ModAbout: author 32 bytes, version 16, description 200), read again when they
   change. */
static char sDetAbout[248];
static s32 sDetAboutOf = -1;

static char *Det_About(s32 part) {
    if (sDetAboutOf != sMm->cur) {
        Port_ModAbout(sMm->cur, sDetAbout);
        sDetAbout[31] = 0;
        sDetAbout[47] = 0;
        sDetAbout[247] = 0;
        sDetAboutOf = sMm->cur;
    }
    return sDetAbout + (part == 0 ? 0 : part == 1 ? 32 : 48);
}

/* The longest a line of the keyboard may be: the name and the title as the select screens have room, the maker's
   lines as the library keeps them. */
static void Kb_Save(void) {
    Port_ModSetText(sMm->cur, sMm->keyWhich, sMm->keyText);
    sDetAboutOf = -1; /* (the maker's lines read again, here and in the Library) */
    sMm->aboutOf = -1;
}

static s32 Kb_Max(void) {
    return sMm->keyWhich == 2 ? 31 : sMm->keyWhich == 3 ? 15 : sMm->keyWhich == 4 ? 199 : 30;
}

void Mm2_OpenDetail(void) {
    if (sMm->count == 0) {
        return;
    }
    sMm->detCur = 0;
    Mm_SetMode(MM_DETAIL);
}

static void Picker_Open(s32 what, s32 back, s32 now) {
    s32 n = 0;
    s32 i;

    if (what == 2 || what == 4) {
        sMm->pickRef[n++] = -1;
    }
    if (what == 1 || what == 2) { /* the new fighters first */
        for (i = 0; i < sMm->count; i++) {
            if (sMm->info[i].isNew && sMm->info[i].on) { /* (new fighters can be targets and results of anyone) */
                sMm->pickRef[n++] = PORT_REF_MOD + i;
            }
        }
    }
    for (i = 0; i < MM_CHARA_COUNT; i++) {
        sMm->pickRef[n++] = i;
    }
    sMm->pickCount = n;
    sMm->pickAllCount = n;
    for (i = 0; i < n; i++) {
        sMm->pickAll[i] = sMm->pickRef[i];
    }
    Search_Clear();
    sMm->pickCur = 0;
    sMm->pickTop = 0;
    for (i = 0; i < n; i++) { /* the cursor starts on what is chosen now */
        if (sMm->pickRef[i] == now) {
            sMm->pickCur = i;
            sMm->pickTop = i >= ROWS ? i - ROWS / 2 : 0;
            if (sMm->pickTop > n - ROWS) {
                sMm->pickTop = n - ROWS > 0 ? n - ROWS : 0;
            }
            break;
        }
    }
    sMm->pickWhat = what;
    sMm->back = back;
    Mm_SetMode(MM_PICKER);
}

static void Keyboard_Open(s32 which) {
    PortModInfo *m = &sMm->info[sMm->cur];

    Str_Copy(sMm->keyText, which == 0 ? m->white : which == 1 ? m->yellow : Det_About(which - 2), 208);
    sMm->keyLen = Str_Len(sMm->keyText);
    sMm->keyCol = 0;
    sMm->keyRow = 0;
    sMm->keyLower = 0;
    sMm->keyWhich = which;
    sMm->back = MM_DETAIL;
    Mm_SetMode(MM_KEYBOARD);
}

static void Crop_Open(void) {
    PortModInfo *m = &sMm->info[sMm->cur];

    Art_Load(sMm->cur);
    if (sMm->artTex == NULL) {
        Mm_Note("THIS MOD HAS NO PORTRAIT TO CUT AN ICON FROM");
        return;
    }
    sMm->crop[0] = m->chip[2] > 0 ? m->chip[0] : 58;
    sMm->crop[1] = m->chip[2] > 0 ? m->chip[1] : 18;
    sMm->crop[2] = m->chip[2] > 0 ? m->chip[2] : 140;
    sMm->back = MM_DETAIL;
    Mm_SetMode(MM_CROP);
}

static void Art_Open(void);

static void Detail_Update(s32 live) {
    Pad *pad = &gPad[0];
    PortModInfo *m = &sMm->info[sMm->cur];
    s32 was = sMm->detCur;

    sMm->wantKey = -1;
    if (Mm_HasCostume(sMm->cur, 0)) {
        sMm->wantKey = sMm->cur * 4;
        sMm->wantChara = m->chara >= 0 ? m->chara : 0;
        sMm->wantCostume = 0;
        sMm->wantPreview = sMm->cur;
    }
    Mm_SetModel(1050.0f, 540.0f, 395.0f);
    Mm_Prompt(0, sMm->detCur == 0 ? "SWITCH" : "CHANGE");
    Mm_Prompt(2, "BACK");
    if (!live) {
        return;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sMm->detCur = (sMm->detCur + 1) % DET_ROWS;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sMm->detCur = (sMm->detCur + DET_ROWS - 1) % DET_ROWS;
    }
    if (sMm->detCur != was) {
        Snd_PlaySe(1, 0);
        return;
    }
    if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(MM_LIBRARY);
        return;
    }
    if (sMm->detCur == 0 && (pad->gamePressed & (PADG_CROSS | PADG_LEFT | PADG_RIGHT | PADG_LEFT2 | PADG_RIGHT2))) {
        Port_ModSet(sMm->cur, 0, !m->isNew);
        Mm_Refresh();
        Mm_FreeFaces();
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_CROSS) {
        Snd_PlaySe(1, 1);
        switch (sMm->detCur) {
        case 1:
            Picker_Open(0, MM_DETAIL, sMm->info[sMm->cur].chara);
            break;
        case 2:
            Keyboard_Open(0);
            break;
        case 3:
            Keyboard_Open(1);
            break;
        case 4:
            Crop_Open();
            break;
        case 5:
            Art_Open();
            break;
        case 6:
        case 7:
        case 8:
            Keyboard_Open(sMm->detCur - 4);
            break;
        }
    }
}

/* (mockup 08) the mod's name at the top, a bar per setting, what the one under the cursor does, the model with its
   name plate as the select screens will show it */
static void Detail_Draw(void) {
    static char *label[DET_ROWS] = {"TYPE", "", "NAME", "TITLE", "ICON", "PORTRAIT", "AUTHOR", "VERSION", "ABOUT"};
    static char *help[DET_ROWS][2] = {
        {"Replace: takes over a fighter everywhere", "New fighter: gets a place on the mod grid"},
        {"Who it takes over - or, if it is new,", "whose voice, stats and moves it starts from"},
        {"The big name on the select screens", "Empty: the fighter's own name"},
        {"The small yellow line under the name", "For example: Super Saiyan 4"},
        {"Its square face on the grids,", "cut from its portrait"},
        {"The big picture on the character select:", "the mod's own, or a PNG of yours"},
        {"Who made the mod: shown in the Library", "(saved in its mod.txt)"},
        {"The mod's version, e.g. 1.2", "(saved in its mod.txt)"},
        {"A line or two about the mod, shown in the", "Library under what it replaces"},
    };
    static char line[96];
    PortModInfo *m = &sMm->info[sMm->cur];
    s32 row;

    Mx_Text(640.0f, 52.0f, "EDIT MOD", 28.0f, 0.0f, MX_GOLD, 1);
    Mx_Box(MM_FIELD, 535.0f, 84.0f, 1315.0f, 128.0f, 22.0f, 0x80);
    Mx_Text(560.0f, 106.0f, m->name, 30.0f, 730.0f, MX_GOLD, 0);
    for (row = 0; row < DET_ROWS; row++) {
        f32 y0 = 158.0f + 50.0f * (f32)row;
        f32 cy = y0 + 22.0f;
        s32 sel = row == sMm->detCur;
        char *value = "";
        s32 dim = 0;

        Mx_Row(65.0f, y0, 695.0f, y0 + 44.0f, sel, 0x80);
        Mx_Text(95.0f, cy, Either(row == 1, Either(m->isNew, "BASED ON", "REPLACES"), label[row]), 25.0f, 185.0f, MX_BLUE, 0);
        switch (row) {
        case 0:
            value = m->isNew ? "< NEW FIGHTER >" : "< REPLACE >";
            break;
        case 1:
            value = m->chara < 0 ? "- (CROSS: PICK ONE)" : m->target;
            break;
        case 2:
            value = Either(m->white[0] != 0, m->white, "DEFAULT");
            dim = m->white[0] == 0;
            break;
        case 3:
            value = Either(m->yellow[0] != 0, m->yellow, "DEFAULT");
            dim = m->yellow[0] == 0;
            break;
        case 4:
            value = m->chip[2] > 0 ? "CUSTOM" : "AUTOMATIC";
            break;
        case 5:
            value = m->art[0] ? "CUSTOM" : "THE MOD'S OWN";
            break;
        case 6:
        case 7:
        case 8:
            value = Det_About(row - 6);
            dim = value[0] == 0;
            value = Either(!dim, value, "NONE");
            break;
        }
        if (row == 0) {
            Mx_Text(470.0f, cy, value, 26.0f, 380.0f, sel ? MX_ORANGE : MX_WHITE, 1);
        } else {
            Mx_TextA(295.0f, cy, value, 25.0f, row == 4 ? 300.0f : 385.0f, sel ? MX_ORANGE : MX_WHITE, 0, dim ? 0x60 : 0x80);
        }
        if (row == 4) {
            Mx_FaceCell(PORT_REF_MOD + sMm->cur, 622.0f, y0 + 4.0f, 658.0f, y0 + 40.0f, 0);
        }
    }
    Mx_Window(62.0f, 612.0f, 775.0f, 692.0f);
    Mx_SayC(92.0f, 636.0f, help[sMm->detCur][0], 26.0f, 660.0f, Mx_Rgba(0xF0, 0xE0, 0xFF, 0x80), Mx_Rgba(0xD8, 0xC0, 0xF8, 0x80), Mx_Rgba(0x24, 0x10, 0x48, 0x80), 0, 0x80);
    Mx_SayC(92.0f, 669.0f, help[sMm->detCur][1], 26.0f, 660.0f, Mx_Rgba(0xF0, 0xE0, 0xFF, 0x80), Mx_Rgba(0xD8, 0xC0, 0xF8, 0x80), Mx_Rgba(0x24, 0x10, 0x48, 0x80), 0, 0x80);
    /* the name plate under the model, as the select screens show it */
    Mx_Box(MM_BAR_SEL, 862.0f, 556.0f, 1335.0f, 632.0f, 32.0f, 0x80);
    Mx_TextC(1098.0f, 593.0f, Either(m->white[0] != 0, m->white, m->chara >= 0 ? m->target : m->name), 40.0f, 430.0f, Mx_Rgba(0xFF, 0xFF, 0xFF, 0x80), Mx_Rgba(0xFF, 0xF4, 0xE0, 0x80),
             Mx_Rgba(0x5C, 0x24, 0x08, 0x80), 1, 0x80);
    sprintf(line, "%.38s", m->yellow[0] != 0 ? m->yellow : m->isNew ? "New fighter" : "Replaces a fighter");
    Mx_Box(MM_NAVBAR, 948.0f, 626.0f, 1250.0f, 662.0f, 18.0f, 0x80);
    Mx_Text(1099.0f, 644.0f, line, 22.0f, 280.0f, MX_GOLD, 1);
}

/* ------------------------------------------------------------------------------------------------- the picker */
static void Trans_Picked(s32 ref);
static char *Trans_PickTitle(void);

#define PK_COLS 2
#define PK_PAGE 8

static void Picker_Choose(s32 ref) {
    switch (sMm->pickWhat) {
    case 0:
        if (ref >= 0 && ref < MM_CHARA_COUNT) {
            Port_ModSet(sMm->cur, sMm->info[sMm->cur].isNew ? 1 : 2, ref);
            Mm_Refresh();
            Mm_FreeFaces();
        }
        break;
    case 1:
        sMm->trWho = ref;
        sMm->trCur = 1;
        sMm->trFocus = 0;
        Port_TransGet(ref, &sMm->tr);
        break;
    case 2: /* a field of the forms screen */
    case 4:
        Trans_Picked(ref);
        break;
    case 3: /* whom the test fight is against */
        if (ref >= 0 && ref < MM_CHARA_COUNT) {
            static char note[80];

            gMmTestFoe = ref;
            sprintf(note, "TEST FIGHTS: VS %.40s", Mm_RefName(ref));
            Mm_Note(note);
        }
        break;
    }
}

void Mm2_OpenFoePicker(void) {
    Picker_Open(3, MM_LIBRARY, gMmTestFoe);
}

static void Picker_Update(s32 live) {
    Pad *pad = &gPad[0];
    u32 typed = 0;

    sMm->wantKey = -1;
    { /* the one under the cursor on the light */
        s32 ref = sMm->pickCount > 0 ? sMm->pickRef[sMm->pickCur] : -1;

        if (ref >= PORT_REF_MOD) {
            PortModInfo *m = &sMm->info[ref - PORT_REF_MOD];

            if (m->chara >= 0) {
                sMm->wantKey = 0x80000 + ref;
                sMm->wantChara = m->chara;
                sMm->wantCostume = 0;
                sMm->wantPreview = ref - PORT_REF_MOD;
            }
        } else if (ref >= 0 && ref < MM_CHARA_COUNT) {
            sMm->wantKey = 0x80000 + ref;
            sMm->wantChara = ref;
            sMm->wantCostume = 0;
            sMm->wantPreview = -1;
        }
    }
    Mm_SetModel(1158.0f, 552.0f, 385.0f);
    if (sMm->typing) {
        Mm_Prompt(11, "DONE");
        Mm_Prompt(12, "CLEAR");
    } else {
        Mm_Prompt(0, "PICK");
        Mm_Prompt(8, "SEARCH");
        if (sMm->pickCount > PK_PAGE) {
            Mm_Prompt(10, "PAGE");
        }
        Mm_Prompt(2, "BACK");
    }
    if (!live) {
        return;
    }
    if (sMm->typing) {
        s32 i;

        if (Search_Type()) {
            sMm->pickCount = 0;
            for (i = 0; i < sMm->pickAllCount; i++) {
                if (sMm->pickAll[i] < 0 || Search_Has(Mm_RefName(sMm->pickAll[i]))) {
                    sMm->pickRef[sMm->pickCount++] = sMm->pickAll[i];
                }
            }
            sMm->pickCur = 0;
            sMm->pickTop = 0;
        }
        Grid_Move(&sMm->pickCur, sMm->pickCount, PK_COLS, PK_PAGE);
        return;
    }
    if (pad->gamePressed & PADG_R1) {
        Search_Open();
        return;
    }
    Grid_Move(&sMm->pickCur, sMm->pickCount, PK_COLS, PK_PAGE);
    if (sMm->pickCount > 0 && ((pad->gamePressed | typed) & PADG_CROSS)) {
        Snd_PlaySe(1, 1);
        Picker_Choose(sMm->pickRef[sMm->pickCur]);
        Mm_SetMode(sMm->back);
    } else if ((pad->gamePressed | typed) & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(sMm->back);
    }
}


/* A fighter's name as two lines: "Goku (End)" / "Super Saiyan 3" (after its ")"), else as it wraps. */
static s32 Picker_Lines(char *name, char (*out)[64]) {
    s32 k = 0;

    while (name[k] != 0 && name[k] != ')') {
        k++;
    }
    if (name[k] == ')' && name[k + 1] == ' ' && name[k + 2] != 0 && k < 60) {
        Str_Copy(out[0], name, k + 2);
        Str_Copy(out[1], name + k + 2, 64);
        return 2;
    }
    return Mx_Wrap(name, 30.0f, 280.0f, out, 2);
}

static void Picker_Draw(void) {
    static char *title[4] = {"WHICH FIGHTER DOES IT REPLACE?", "WHOSE FORMS?", "TRANSFORMS INTO?", "TEST FIGHT OPPONENT?"};
    static char lines[2][64];
    char *head = Either(sMm->pickWhat == 2 || sMm->pickWhat == 4, Trans_PickTitle(),
                        Either(sMm->pickWhat == 0 && sMm->info[sMm->cur].isNew, "BASED ON WHICH FIGHTER?", title[sMm->pickWhat < 4 ? sMm->pickWhat : 2]));
    s32 first = sMm->pickCur / PK_PAGE * PK_PAGE;
    s32 k;

    Mx_Box(MM_NAVBAR, 130.0f, 106.0f, 1345.0f, 140.0f, 17.0f, 0x80);
    Mx_Text(150.0f, 123.0f, head, 25.0f, 360.0f, MX_GOLD, 0);
    Search_Draw(520.0f, 106.0f, 1000.0f, 140.0f);
    if (sMm->pickCount > PK_PAGE) {
        static char page[32];

        sprintf(page, "PAGE %d / %d", (int)(sMm->pickCur / PK_PAGE + 1), (int)((sMm->pickCount + PK_PAGE - 1) / PK_PAGE));
        Mx_Text(1325.0f, 123.0f, page, 23.0f, 0.0f, MX_WHITE, 2);
    }
    if (sMm->pickCount == 0) {
        Mx_Window(100.0f, 250.0f, 915.0f, 380.0f);
        Mx_Text(507.0f, 300.0f, "NOTHING MATCHES THAT", 30.0f, 0.0f, MX_GOLD, 1);
        Mx_Text(507.0f, 345.0f, "R1, then type another name", 24.0f, 0.0f, MX_WHITE, 1);
    }
    for (k = 0; k < PK_PAGE; k++) {
        s32 at = first + k;
        f32 x0 = 100.0f + 415.0f * (f32)(k % PK_COLS);
        f32 y0 = 195.0f + 113.0f * (f32)(k / PK_COLS);
        f32 x1 = x0 + 400.0f;
        s32 sel = at == sMm->pickCur;
        s32 ref;
        s32 n;

        if (at >= sMm->pickCount) { /* (the page's empty places) */
            if (sMm->pickCount > 0) {
                Mx_Box(MM_BAR, x0, y0, x1, y0 + 100.0f, 24.0f, 0x50);
                Mx_Fit(MM_LOCK, (x0 + x1) * 0.5f, y0 + 50.0f, 56.0f, 0x50);
            }
            continue;
        }
        ref = sMm->pickRef[at];
        if (sel) {
            Mx_Box(MM_BAR_SEL, x0 - 4.0f, y0 - 4.0f, x1 + 4.0f, y0 + 104.0f, 26.0f, 0x80);
        } else {
            Mx_Box(MM_BAR, x0, y0, x1, y0 + 100.0f, 24.0f, 0x80);
        }
        if (ref < 0) {
            Mx_Fit(MM_LOCK, x0 + 52.0f, y0 + 50.0f, 56.0f, 0x80);
            Mx_Text(x0 + 100.0f, y0 + 50.0f, "NONE (EMPTY)", 30.0f, 280.0f, MX_WHITE, 0);
            continue;
        }
        Mx_FaceCell(ref, x0 + 16.0f, y0 + 14.0f, x0 + 88.0f, y0 + 86.0f, 0);
        n = Picker_Lines(Mm_RefName(ref), lines);
        if (n == 1) {
            Mx_Text(x0 + 102.0f, y0 + 50.0f, lines[0], 30.0f, 285.0f, MX_WHITE, 0);
        } else if (n == 2) {
            Mx_Text(x0 + 102.0f, y0 + 32.0f, lines[0], 30.0f, 285.0f, MX_WHITE, 0);
            Mx_Text(x0 + 102.0f, y0 + 69.0f, lines[1], 30.0f, 285.0f, MX_WHITE, 0);
        }
        if (ref >= PORT_REF_MOD) {
            Mx_Bar(MM_TAB, x1 - 96.0f, y0 - 12.0f, x1 - 6.0f, y0 + 20.0f, 0x80);
            Mx_TextC(x1 - 51.0f, y0 + 4.0f, "NEW", 21.0f, 0.0f, Mx_Rgba(0xFF, 0xFF, 0xFF, 0x80), Mx_Rgba(0xE0, 0xFF, 0xFF, 0x80), Mx_Rgba(0x06, 0x40, 0x48, 0x80), 1, 0x80);
        }
    }
    /* under the model: who it is */
    if (sMm->pickCount > 0 && sMm->pickRef[sMm->pickCur] >= 0) {
        s32 n = Picker_Lines(Mm_RefName(sMm->pickRef[sMm->pickCur]), lines);

        if (n == 1) {
            Mx_Text(1157.0f, 625.0f, lines[0], 34.0f, 390.0f, MX_LAV, 1);
        } else if (n == 2) {
            Mx_Text(1157.0f, 608.0f, lines[0], 34.0f, 390.0f, MX_LAV, 1);
            Mx_Text(1157.0f, 650.0f, lines[1], 34.0f, 390.0f, MX_LAV, 1);
        }
    }
}

/* ------------------------------------------------------------------------------------------------- the keyboard */
static char *sKeys[2][4] = {
    {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ .-'", "0123456789"},
    {"abcdefghij", "klmnopqrst", "uvwxyz ()!", "0123456789"},
};

static void Keyboard_Update(s32 live) {
    Pad *pad = &gPad[0];

    sMm->wantKey = -1;
    Mm_Prompt(0, "TYPE");
    Mm_Prompt(1, "DELETE");
    Mm_Prompt(8, sMm->keyLower ? "CAPS" : "lower case");
    Mm_Prompt(7, "SAVE");
    Mm_Prompt(2, "CANCEL");
    if (!live) {
        return;
    }
    {
        /* the PC keyboard: its letters go into the line, enter keeps it, escape leaves */
        s32 c;

        while ((c = Port_TextRead()) != 0) {
            if (c == 13) {
                Kb_Save();
                Mm_Refresh();
                Snd_PlaySe(1, 1);
                Mm_SetMode(sMm->back);
                return;
            } else if (c == 27) {
                Snd_PlaySe(1, 2);
                Mm_SetMode(sMm->back);
                return;
            } else if (c == 8) {
                if (sMm->keyLen > 0) {
                    sMm->keyText[--sMm->keyLen] = 0;
                }
            } else if (c >= 32 && c < 127 && sMm->keyLen < Kb_Max()) {
                sMm->keyText[sMm->keyLen++] = (char)c;
                sMm->keyText[sMm->keyLen] = 0;
            }
        }
    }
    if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        sMm->keyCol = (sMm->keyCol + 1) % 10;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        sMm->keyCol = (sMm->keyCol + 9) % 10;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sMm->keyRow = (sMm->keyRow + 1) % 4;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sMm->keyRow = (sMm->keyRow + 3) % 4;
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & PADG_CROSS) {
        if (sMm->keyLen < Kb_Max()) {
            sMm->keyText[sMm->keyLen++] = sKeys[sMm->keyLower][sMm->keyRow][sMm->keyCol];
            sMm->keyText[sMm->keyLen] = 0;
            Snd_PlaySe(1, 1);
        }
    } else if (pad->gameRepeat & PADG_SQUARE) {
        if (sMm->keyLen > 0) {
            sMm->keyText[--sMm->keyLen] = 0;
            Snd_PlaySe(1, 2);
        }
    } else if (pad->gamePressed & PADG_R1) {
        sMm->keyLower ^= 1;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_START) {
        Kb_Save();
        Mm_Refresh();
        Snd_PlaySe(1, 1);
        Mm_SetMode(sMm->back);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(sMm->back);
    }
}

/* (mockup 10) the line being typed in its frame, the keys, how the name will look */
static void Keyboard_Draw(void) {
    static char one[2];
    static char shown[216];
    static char lines[4][64];
    static char *tab[5] = {"NAME", "TITLE", "AUTHOR", "VERSION", "ABOUT"};
    static char count[16];
    PortModInfo *m = &sMm->info[sMm->cur];
    s32 r;
    s32 c;

    Mx_Box(MM_TAB_SEL, 72.0f, 143.0f, 272.0f, 192.0f, 16.0f, 0x80);
    Mx_TextC(172.0f, 164.0f, tab[sMm->keyWhich], 26.0f, 180.0f, Mx_Rgba(0xFF, 0xF4, 0xD8, 0x80), Mx_Rgba(0xFF, 0xE0, 0x98, 0x80), Mx_Rgba(0x5C, 0x30, 0x08, 0x80), 1, 0x80);
    Mx_Box(MM_CARD, 72.0f, 178.0f, 1305.0f, 262.0f, 24.0f, 0x80);
    sprintf(shown, "%s%s", sMm->keyText, Either((sMm->frames / 16) & 1, "_", " "));
    Mx_Text(120.0f, 219.0f, Mx_Tail(shown, 46.0f, 1000.0f), 46.0f, 1000.0f, MX_WHITE, 0);
    sprintf(count, "%d / %d", (int)sMm->keyLen, (int)Kb_Max());
    Mx_Text(1264.0f, 236.0f, count, 25.0f, 0.0f, MX_WHITE, 2);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 10; c++) {
            f32 x0 = 72.0f + 76.0f * (f32)c;
            f32 y0 = 302.0f + 74.0f * (f32)r;
            s32 sel = r == sMm->keyRow && c == sMm->keyCol;

            one[0] = sKeys[sMm->keyLower][r][c];
            one[1] = 0;
            if (sel) {
                Mx_Box(MM_KEYCAP_SEL, x0 - 4.0f, y0 - 4.0f, x0 + 68.0f, y0 + 68.0f, 20.0f, 0x80);
            } else {
                Mx_Box(MM_KEYCAP, x0, y0, x0 + 64.0f, y0 + 64.0f, 18.0f, 0x80);
            }
            if (one[0] == ' ') {
                Mx_Text(x0 + 32.0f, y0 + 32.0f, "SPACE", 15.0f, 54.0f, sel ? MX_ORANGE : MX_WHITE, 1);
            } else {
                Mx_Text(x0 + 32.0f, y0 + 32.0f, one, 32.0f, 0.0f, sel ? MX_ORANGE : MX_WHITE, 1);
            }
        }
    }
    /* how it will look: the name plate of the select screens */
    Mx_Box(MM_WINDOW, 885.0f, 305.0f, 1080.0f, 360.0f, 16.0f, 0x80);
    Mx_Window(885.0f, 337.0f, 1310.0f, 582.0f);
    Mx_Text(912.0f, 325.0f, "PREVIEW", 24.0f, 0.0f, MX_CYAN, 0);
    Mx_Box(MM_NAVBAR, 908.0f, 412.0f, 1288.0f, 512.0f, 46.0f, 0x80);
    Mx_Frame(930.0f, 418.0f, 1266.0f, 506.0f, 2.0f, Mx_Rgba(0xFF, 0xC8, 0x40, 0x70));
    if (sMm->keyWhich == 4) { /* the description as the Library wraps it */
        s32 n = Mx_Wrap(sMm->keyLen > 0 ? sMm->keyText : "(none)", 22.0f, 360.0f, lines, 4);

        for (r = 0; r < n; r++) {
            Mx_Text(1098.0f, 462.0f + 25.0f * ((f32)r - (f32)(n - 1) * 0.5f), lines[r], 22.0f, 360.0f, MX_WHITE, 1);
        }
    } else if (sMm->keyWhich >= 2) {
        Mx_Text(1098.0f, 446.0f, sMm->keyWhich == 2 ? "BY" : "VERSION", 22.0f, 330.0f, MX_CYAN, 1);
        Mx_Text(1098.0f, 482.0f, sMm->keyLen > 0 ? sMm->keyText : "(none)", 30.0f, 330.0f, MX_GOLD, 1);
    } else if (sMm->keyWhich == 0) {
        Mx_Heading(1098.0f, 462.0f, sMm->keyLen > 0 ? sMm->keyText : m->chara >= 0 ? m->target : m->name, 46.0f, 330.0f, 1);
    } else {
        Mx_Heading(1098.0f, 446.0f, m->white[0] != 0 ? m->white : m->chara >= 0 ? m->target : m->name, 34.0f, 330.0f, 1);
        Mx_Text(1098.0f, 489.0f, sMm->keyLen > 0 ? sMm->keyText : "(none)", 24.0f, 330.0f, MX_GOLD, 1);
    }
    Mx_Say(688.0f, 657.0f, sMm->keyWhich >= 2 ? "Or use your keyboard: Enter saves, Esc cancels. Saved in the mod's mod.txt, shown in the Library."
                                                : "Or use your keyboard: Enter saves, Esc cancels. Leave it empty to use the fighter's own name.",
            24.0f, 1260.0f, MX_DARK, 1);
}

/* ------------------------------------------------------------------------------------------------- the chip's square */
static void Crop_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 *c = sMm->crop;

    sMm->wantKey = -1;
    Mm_Prompt(0, "SAVE");
    Mm_Prompt(15, "MOVE");
    Mm_Prompt(10, "SIZE");
    Mm_Prompt(1, "RESET");
    Mm_Prompt(2, "CANCEL");
    if (!live) {
        return;
    }
    if (pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2)) {
        c[0] += 2;
    }
    if (pad->gameHeld & (PADG_LEFT | PADG_LEFT2)) {
        c[0] -= 2;
    }
    if (pad->gameHeld & (PADG_DOWN | PADG_DOWN2)) {
        c[1] += 2;
    }
    if (pad->gameHeld & (PADG_UP | PADG_UP2)) {
        c[1] -= 2;
    }
    if (pad->gameHeld & PADG_R2) {
        c[2] += 2;
    }
    if (pad->gameHeld & PADG_L2) {
        c[2] -= 2;
    }
    if (c[2] < 32) {
        c[2] = 32;
    }
    if (c[2] > 256) {
        c[2] = 256;
    }
    if (c[0] < 0) {
        c[0] = 0;
    }
    if (c[1] < 0) {
        c[1] = 0;
    }
    if (c[0] + c[2] > 256) {
        c[0] = 256 - c[2];
    }
    if (c[1] + c[2] > 512) {
        c[1] = 512 - c[2];
    }
    if (pad->gamePressed & PADG_SQUARE) { /* back to the square the library finds by itself (size 0) */
        Port_ModSet(sMm->cur, 3, 0);
        Port_ModSet(sMm->cur, 4, 0);
        Port_ModSet(sMm->cur, 5, 0);
        Mm_Refresh();
        Mm_FreeFaces();
        Mm_Note("ICON IS AUTOMATIC AGAIN");
        Snd_PlaySe(1, 2);
        Mm_SetMode(sMm->back);
        return;
    }
    if (pad->gamePressed & PADG_CROSS) {
        Port_ModSet(sMm->cur, 3, c[0]);
        Port_ModSet(sMm->cur, 4, c[1]);
        Port_ModSet(sMm->cur, 5, c[2]);
        Mm_Refresh();
        Mm_FreeFaces();
        Snd_PlaySe(1, 1);
        Mm_SetMode(sMm->back);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(sMm->back);
    }
}

#define CR_X0 246.0f /* (mockup 11) where the portrait stands in its frame, and how small */
#define CR_Y0 170.0f
#define CR_K 0.988f

static void Crop_Draw(void) {
    s32 *c = sMm->crop;
    f32 kx = Mx_SameX(CR_K);
    f32 px = 373.0f - 128.0f * kx;
    f32 x0 = px + (f32)c[0] * kx;
    f32 y0 = CR_Y0 + (f32)c[1] * CR_K;
    f32 s = (f32)c[2] * CR_K;
    f32 sx = (f32)c[2] * kx;
    f32 lw = Mx_SameX(163.0f) * 0.5f;
    f32 gw = Mx_SameX(88.0f) * 0.5f;
    u32 gold = Mx_Rgba(0xFF, 0xD0, 0x50, 0x80);
    s32 k;

    Mx_Label(150.0f, 106.0f, 650.0f, 141.0f, "CHOOSE ITS ICON");
    Mx_Box(MM_WINDOW_DARK, 180.0f, 158.0f, 566.0f, 686.0f, 22.0f, 0x80);
    if (sMm->artTex != NULL) {
        Mx_Tex(sMm->artTex, px, CR_Y0, px + 256.0f * kx, CR_Y0 + 512.0f * CR_K, 0, 0, 256, 512, 0x50, 0x80);
        Mx_Tex(sMm->artTex, x0, y0, x0 + sx, y0 + s, c[0], c[1], c[0] + c[2], c[1] + c[2], 0x80, 0x80); /* the square at full light */
        Mx_Frame(x0 - 1.5f, y0 - 1.5f, x0 + sx + 1.5f, y0 + s + 1.5f, 2.0f, Mx_Rgba(0xFF, 0xD8, 0x70, 0x70));
        Mx_Brackets(x0 - 6.0f, y0 - 6.0f, x0 + sx + 6.0f, y0 + s + 6.0f, 14.0f, 4.0f, gold);
    } else {
        Mx_Text(373.0f, 420.0f, "NO PORTRAIT", 30.0f, 0.0f, MX_WHITE, 1);
    }
    /* what it gives: as large as the lists show it, and among the grid's faces */
    Mx_Window(725.0f, 108.0f, 1255.0f, 686.0f);
    Mx_Text(990.0f, 134.0f, "ICON ON THE GRIDS", 30.0f, 0.0f, MX_GOLD, 1);
    Mx_Text(990.0f, 187.0f, "LARGE PREVIEW", 28.0f, 0.0f, MX_GOLD, 1);
    Mx_Rect(990.0f - lw - 5.0f, 210.0f, 990.0f + lw + 5.0f, 383.0f, Mx_Rgba(0xFF, 0xE0, 0x80, 0x80), Mx_Rgba(0xC8, 0x88, 0x18, 0x80));
    Mx_Rect(990.0f - lw, 215.0f, 990.0f + lw, 378.0f, Mx_Rgba(0x10, 0x18, 0x40, 0x80), Mx_Rgba(0x10, 0x18, 0x40, 0x80));
    if (sMm->artTex != NULL) {
        Mx_Tex(sMm->artTex, 990.0f - lw, 215.0f, 990.0f + lw, 378.0f, c[0], c[1], c[0] + c[2], c[1] + c[2], 0x80, 0x80);
    }
    Mx_Text(990.0f, 418.0f, "GRID PREVIEW", 28.0f, 0.0f, MX_GOLD, 1);
    for (k = -1; k <= 1; k++) {
        f32 cx = 990.0f + 102.0f * (f32)k;

        if (k == 0) {
            Mx_Rect(cx - gw - 4.0f, 441.0f, cx + gw + 4.0f, 537.0f, Mx_Rgba(0xFF, 0xE0, 0x80, 0x80), Mx_Rgba(0xC8, 0x88, 0x18, 0x80));
            Mx_Rect(cx - gw, 445.0f, cx + gw, 533.0f, Mx_Rgba(0x10, 0x18, 0x40, 0x80), Mx_Rgba(0x10, 0x18, 0x40, 0x80));
            if (sMm->artTex != NULL) {
                Mx_Tex(sMm->artTex, cx - gw, 445.0f, cx + gw, 533.0f, c[0], c[1], c[0] + c[2], c[1] + c[2], 0x80, 0x80);
            }
        } else if (sMm->cur + k >= 0 && sMm->cur + k < sMm->count) { /* (its neighbours in the library) */
            Mx_FaceCell(PORT_REF_MOD + sMm->cur + k, cx - 40.0f, 448.0f, cx + 40.0f, 530.0f, 0);
        }
    }
    Mx_Text(768.0f, 577.0f, "D-pad: move the square", 27.0f, 460.0f, MX_LAV, 0);
    Mx_Button(6, 762.0f, 615.0f, 30.0f);
    Mx_Text(768.0f + Mx_ButtonW(6, 30.0f) + 4.0f, 615.0f, "/", 27.0f, 0.0f, MX_LAV, 0);
    Mx_Button(5, 800.0f + Mx_ButtonW(6, 30.0f), 615.0f, 30.0f);
    Mx_Text(808.0f + Mx_ButtonW(6, 30.0f) + Mx_ButtonW(5, 30.0f), 615.0f, ": smaller / bigger", 27.0f, 330.0f, MX_LAV, 0);
    Mx_Text(768.0f, 652.0f, "Square: back to automatic", 27.0f, 460.0f, MX_LAV, 0);
}

/* ------------------------------------------------------------------------------------------------- its portrait
 * The large picture the select screens show of a fighter, made here: the mod's own portrait or a picture file (square),
 * moved with the directions, sized with L2 / R2 - all of it, or only its width or its height (L1 / R1 choose) -
 * inside the frame, shown the way the character select shows it: behind the bar of faces, cut off by the foot of
 * the screen. Cross keeps it (the library writes the portrait file), circle goes back to the portrait the mod came
 * with.
 */
#define ART_FX 250.0f    /* (mockup 12) where the select screen's portrait frame stands in the little screen, */
#define ART_FY 178.0f
#define ART_K 0.964f     /* and how small it is */
#define ART_VIS 448.0f   /* the lines of the frame the screen shows (the portrait is 512 high) */
/* The picture the portrait is made from, whole in the frame, into this screen's texture. */
static void Art_Base(void) {
    MmRes *res;

    sMm->artTex = NULL;
    if (sMm->artFile == NULL) {
        sMm->artFile = Heap_Alloc(ART_FILE_SIZE, 0x40, 0, 2);
        sMm->artRes = Heap_Alloc(ART_RES_SIZE, 0x40, 0, 2);
    }
    if (Port_ModArtBase(sMm->cur, sMm->artRes, ART_RES_SIZE) < 0x100) {
        return;
    }
    res = (MmRes *)sMm->artRes;
    Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
    sMm->artTex = res->tex;
}

static void Art_Open(void) {
    PortModInfo *m = &sMm->info[sMm->cur];

    sMm->art[0] = m->art[0] ? m->art[1] : 0;
    sMm->art[1] = m->art[0] ? m->art[2] : 0;
    sMm->art[2] = m->art[0] && m->art[3] > 0 ? m->art[3] : 100;
    sMm->art[3] = m->art[0] && m->artH > 0 ? m->artH : sMm->art[2];
    sMm->artRow = 0;
    Mm_SetMode(MM_ART);
    Art_Base();
    if (sMm->artTex == NULL) {
        Mm_Note("NO PICTURE YET: PRESS SQUARE TO PICK A PNG");
    }
}

static void Art_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 *a = sMm->art;

    sMm->wantKey = -1;
    Mm_Prompt(0, "SAVE");
    Mm_Prompt(1, "PICK PNG");
    Mm_Prompt(14, "CHOOSE");
    Mm_Prompt(10, "CHANGE");
    Mm_Prompt(4, "RESET");
    Mm_Prompt(2, "CANCEL");
    if (!live) {
        return;
    }
    if (pad->gameHeld & (PADG_RIGHT | PADG_RIGHT2)) {
        a[0] += 3;
    }
    if (pad->gameHeld & (PADG_LEFT | PADG_LEFT2)) {
        a[0] -= 3;
    }
    if (pad->gameHeld & (PADG_DOWN | PADG_DOWN2)) {
        a[1] += 3;
    }
    if (pad->gameHeld & (PADG_UP | PADG_UP2)) {
        a[1] -= 3;
    }
    if (pad->gamePressed & PADG_R1) { /* what L2 / R2 change: size, width, height */
        sMm->artRow = (sMm->artRow + 1) % 3;
        Snd_PlaySe(1, 0);
    } else if (pad->gamePressed & PADG_L1) {
        sMm->artRow = (sMm->artRow + 2) % 3;
        Snd_PlaySe(1, 0);
    }
    if (pad->gameHeld & (PADG_R2 | PADG_L2)) {
        s32 d = (pad->gameHeld & PADG_R2) ? 2 : -2;

        if (sMm->artRow != 2) {
            a[2] += d;
        }
        if (sMm->artRow != 1) {
            a[3] += d;
        }
    }
    a[2] = a[2] < 30 ? 30 : a[2] > 400 ? 400 : a[2];
    a[3] = a[3] < 30 ? 30 : a[3] > 400 ? 400 : a[3];
    if (a[0] < -500) {
        a[0] = -500;
    }
    if (a[0] > 500) {
        a[0] = 500;
    }
    if (a[1] < -800) {
        a[1] = -800;
    }
    if (a[1] > 800) {
        a[1] = 800;
    }
    if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(MM_DETAIL);
    } else if (pad->gamePressed & PADG_SQUARE) {
        Snd_PlaySe(1, 1);
        Browser_OpenPicture();
    } else if (pad->gamePressed & PADG_CIRCLE) {
        Port_ModArtReset(sMm->cur);
        Mm_Refresh();
        Mm_FreeFaces();
        a[0] = 0;
        a[1] = 0;
        a[2] = 100;
        a[3] = 100;
        Art_Base();
        Mm_Note("PORTRAIT RESET TO THE MOD'S OWN");
        Snd_PlaySe(1, 2);
    } else if ((pad->gamePressed & PADG_CROSS) && sMm->artTex != NULL) {
        if (Port_ModArtSet2(sMm->cur, a[0], a[1], a[2], a[3])) {
            Mm_Refresh();
            Mm_FreeFaces();
            Mm_Note("PORTRAIT SAVED");
            Snd_PlaySe(1, 1);
            Mm_SetMode(MM_DETAIL);
        } else {
            Mm_Note("COULDN'T SAVE THE PORTRAIT");
            Snd_PlaySe(1, 2);
        }
    }
}

/* Across, the little screen's frame is as wide as it is for its height (the screen's shape may be any). */
static f32 Art_Kx(void) {
    return Mx_SameX(ART_K);
}

static f32 Art_Fx(void) {
    return 373.5f - 128.0f * Art_Kx();
}

/* The picture as it sits in the frame, on the little screen: what falls outside the frame or under the foot of the
   screen is cut off. */
static void Art_Picture(s32 shade, s32 alpha) {
    f32 zx = (f32)sMm->art[2] / 100.0f;
    f32 zy = (f32)sMm->art[3] / 100.0f;
    f32 x0 = 128.0f + (f32)sMm->art[0] - 128.0f * zx;
    f32 y0 = 256.0f + (f32)sMm->art[1] - 256.0f * zy;
    f32 x1 = x0 + 256.0f * zx;
    f32 y1 = y0 + 512.0f * zy;
    f32 cx0 = x0 < 0.0f ? 0.0f : x0;
    f32 cy0 = y0 < 0.0f ? 0.0f : y0;
    f32 cx1 = x1 > 256.0f ? 256.0f : x1;
    f32 cy1 = y1 > ART_VIS ? ART_VIS : y1;

    if (sMm->artTex == NULL || cx1 <= cx0 || cy1 <= cy0) {
        return;
    }
    Mx_Tex(sMm->artTex, Art_Fx() + cx0 * Art_Kx(), ART_FY + cy0 * ART_K, Art_Fx() + cx1 * Art_Kx(), ART_FY + cy1 * ART_K, (s32)((cx0 - x0) / zx), (s32)((cy0 - y0) / zy),
           (s32)((cx1 - x0) / zx), (s32)((cy1 - y0) / zy), shade, alpha);
}

/* (mockup 12) the little select screen at the left, how large and where at the right */
static void Art_Draw(void) {
    static char *rowName[4] = {"SIZE", "WIDTH", "HEIGHT", "OFFSET"};
    static char value[32];
    PortModInfo *m = &sMm->info[sMm->cur];
    f32 w = 256.0f * Art_Kx();
    f32 h = ART_VIS * ART_K;
    f32 bar = ART_FY + h * 0.68f;
    s32 k;

    Mx_Box(MM_WINDOW_DARK, 82.0f, 170.0f, 667.0f, 618.0f, 14.0f, 0x80);
    Mx_Rect(90.0f, 178.0f, 659.0f, 178.0f + h * 0.62f, Mx_Rgba(0x48, 0x98, 0xE8, 0x80), Mx_Rgba(0xB8, 0xE0, 0xF8, 0x80));
    Mx_Rect(90.0f, 178.0f + h * 0.62f, 659.0f, 610.0f, Mx_Rgba(0x70, 0xB0, 0x50, 0x80), Mx_Rgba(0x30, 0x68, 0x28, 0x80));
    Art_Picture(0x80, 0x80);
    /* where the frame is: what the select screen can show */
    Mx_Frame(Art_Fx(), ART_FY + 40.0f, Art_Fx() + w, ART_FY + h, 2.0f, Mx_Rgba(0xFF, 0xFF, 0xFF, 0x50));
    /* the bar of faces over it, this fighter's under the cursor */
    Mx_Rect(90.0f, bar - 34.0f, 659.0f, bar + 34.0f, Mx_Rgba(0x40, 0x70, 0xE8, 0x80), Mx_Rgba(0x10, 0x28, 0x98, 0x80));
    Mx_Rect(90.0f, bar - 34.0f, 659.0f, bar - 31.0f, Mx_Rgba(0xB0, 0xD8, 0xFF, 0x80), Mx_Rgba(0xB0, 0xD8, 0xFF, 0x80));
    Mx_Rect(90.0f, bar + 31.0f, 659.0f, bar + 34.0f, Mx_Rgba(0x06, 0x0C, 0x40, 0x80), Mx_Rgba(0x06, 0x0C, 0x40, 0x80));
    for (k = -2; k <= 2; k++) {
        f32 cx = Art_Fx() + w * 0.5f + 75.0f * (f32)k;

        if (k == 0) {
            Mx_FaceCell(PORT_REF_MOD + sMm->cur, cx - 30.0f, bar - 28.0f, cx + 30.0f, bar + 28.0f, 1);
            Mx_Brackets(cx - 38.0f, bar - 36.0f, cx + 38.0f, bar + 36.0f, 14.0f, 4.0f, Mx_Rgba(0xFF, 0xD0, 0x30, 0x80));
            Mx_Text(cx, bar - 50.0f, "1P", 22.0f, 0.0f, MX_GOLD, 1);
        } else {
            Mx_Rect(cx - 28.0f, bar - 26.0f, cx + 28.0f, bar + 26.0f, Mx_Rgba(0x20, 0x30, 0x80, 0x80), Mx_Rgba(0x0C, 0x14, 0x50, 0x80));
        }
    }
    if (sMm->artTex == NULL) {
        Mx_Text(374.0f, 300.0f, "NO PICTURE YET", 30.0f, 0.0f, MX_WHITE, 1);
        Mx_Text(374.0f, 340.0f, "Square: pick a PNG", 24.0f, 0.0f, MX_WHITE, 1);
    }
    Mx_TextC(374.0f, 647.0f, "LIVE PREVIEW", 30.0f, 0.0f, Mx_Rgba(0xF4, 0xE8, 0xFF, 0x80), Mx_Rgba(0xD8, 0xC4, 0xFF, 0x80), Mx_Rgba(0x2C, 0x14, 0x54, 0x80), 1, 0x80);
    /* at the right: its numbers, the row L2 / R2 change lit */
    Mx_Window(722.0f, 150.0f, 1328.0f, 668.0f);
    Mx_Box(MM_BAR_SEL, 773.0f, 128.0f, 1278.0f, 180.0f, 26.0f, 0x80);
    Mx_TextC(1025.0f, 154.0f, "SELECT SCREEN PORTRAIT", 29.0f, 470.0f, Mx_Rgba(0x5A, 0x24, 0x70, 0x80), Mx_Rgba(0x3C, 0x14, 0x52, 0x80), Mx_Rgba(0xFF, 0xF0, 0xD0, 0x80), 1, 0x80);
    Mx_Text(1025.0f, 220.0f, m->name, 30.0f, 560.0f, MX_GOLD, 1);
    for (k = 0; k < 4; k++) {
        f32 cy = 272.0f + 50.0f * (f32)k;

        if (k == sMm->artRow) {
            Mx_Box(MM_BAR_SEL, 772.0f, cy - 23.0f, 1278.0f, cy + 23.0f, 22.0f, 0x80);
        }
        switch (k) {
        case 0:
            if (sMm->art[2] == sMm->art[3]) {
                sprintf(value, "%d%%", (int)sMm->art[2]);
            } else {
                sprintf(value, "%d%% x %d%%", (int)sMm->art[2], (int)sMm->art[3]);
            }
            break;
        case 1:
            sprintf(value, "%d%%", (int)sMm->art[2]);
            break;
        case 2:
            sprintf(value, "%d%%", (int)sMm->art[3]);
            break;
        default:
            sprintf(value, "%d, %d", (int)sMm->art[0], (int)sMm->art[1]);
            break;
        }
        Mx_Text(835.0f, cy, rowName[k], 27.0f, 0.0f, k == sMm->artRow ? MX_ORANGE : MX_BLUE, 0);
        Mx_Text(1040.0f, cy, value, 28.0f, 220.0f, k == sMm->artRow ? MX_ORANGE : MX_WHITE, 0);
    }
    Mx_Rect(740.0f, 474.0f, 1310.0f, 476.0f, Mx_Rgba(0xA0, 0xE0, 0xE0, 0x50), Mx_Rgba(0xA0, 0xE0, 0xE0, 0x50));
    Mx_Button(15, 762.0f, 500.0f, 28.0f);
    Mx_Text(800.0f, 500.0f, ": move", 25.0f, 0.0f, MX_LAV, 0);
    Mx_Button(14, 762.0f, 536.0f, 28.0f);
    Mx_Text(770.0f + Mx_ButtonW(14, 28.0f), 536.0f, ": size, width or height", 25.0f, 420.0f, MX_LAV, 0);
    Mx_Button(10, 762.0f, 572.0f, 28.0f);
    Mx_Text(770.0f + Mx_ButtonW(10, 28.0f), 572.0f, ": smaller / bigger", 25.0f, 420.0f, MX_LAV, 0);
    Mx_Button(1, 762.0f, 608.0f, 28.0f);
    Mx_Text(800.0f, 608.0f, ": use another picture (PNG or BMP)", 25.0f, 480.0f, MX_LAV, 0);
    Mx_Button(4, 762.0f, 644.0f, 28.0f);
    Mx_Text(800.0f, 644.0f, ": back to the mod's own", 25.0f, 480.0f, MX_LAV, 0);
}

/* ------------------------------------------------------------------------------------------------- the roster */
#define ROS_COLS 5
#define ROS_X 34.0f
#define ROS_Y 96.0f
#define ROS_PITCH 58.0f
#define ROS_ROWS 3
static s32 sRosTop; /* the first row of the mod grid shown (it scrolls past four rows) */

static void Roster_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 count = Port_SkinCount();
    s32 was = sMm->rosCur;

    if (sMm->rosCur >= count) {
        sMm->rosCur = count > 0 ? count - 1 : 0;
    }
    sMm->wantKey = -1;
    if (count > 0) {
        s32 mod = Port_SkinMod(sMm->rosCur);

        if (mod >= 0 && Mm_HasCostume(mod, 0)) {
            sMm->wantKey = 0x20000 + mod;
            sMm->wantChara = sMm->info[mod].chara >= 0 ? sMm->info[mod].chara : 0;
            sMm->wantCostume = 0;
            sMm->wantPreview = mod;
        }
    }
    Mm_SetModel(1068.0f, 548.0f, 420.0f);
    if (count > 1) {
        Mm_Prompt(10, "MOVE LEFT / RIGHT");
    }
    if (count > 0) {
        Mm_Prompt(15, "SELECT");
    }
    Mm_Prompt(2, "BACK");
    if (!live) {
        return;
    }
    if (pad->gamePressed & PADG_L1) {
        Snd_PlaySe(1, 0);
        Mm_SetMode(MM_LIBRARY);
        return;
    }
    if (pad->gamePressed & PADG_R1) {
        Snd_PlaySe(1, 0);
        Mm_SetMode(MM_TRANSFORM);
        return;
    }
    if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(MM_LIBRARY);
        return;
    }
    if (count == 0) {
        return;
    }
    if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        sMm->rosCur = (sMm->rosCur + 1) % count;
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        sMm->rosCur = (sMm->rosCur + count - 1) % count;
    } else if ((pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) && sMm->rosCur + ROS_COLS < count) {
        sMm->rosCur += ROS_COLS;
    } else if ((pad->gameRepeat & (PADG_UP | PADG_UP2)) && sMm->rosCur >= ROS_COLS) {
        sMm->rosCur -= ROS_COLS;
    } else if ((pad->gamePressed & PADG_R2) && sMm->rosCur + 1 < count) {
        Port_RosterMove(sMm->rosCur, 1);
        sMm->rosCur++;
        Mm_Refresh();
    } else if ((pad->gamePressed & PADG_L2) && sMm->rosCur > 0) {
        Port_RosterMove(sMm->rosCur, -1);
        sMm->rosCur--;
        Mm_Refresh();
    }
    if (sMm->rosCur != was) {
        Snd_PlaySe(1, 0);
        sMm->rest = 0;
    }
}

/* The rows shown keep the cursor's in them. */
static void Roster_Scroll(s32 count) {
    s32 row = sMm->rosCur / ROS_COLS;
    s32 rows = (count + ROS_COLS - 1) / ROS_COLS;

    if (row < sRosTop) {
        sRosTop = row;
    }
    if (row >= sRosTop + ROS_ROWS) {
        sRosTop = row - ROS_ROWS + 1;
    }
    if (sRosTop > rows - ROS_ROWS) {
        sRosTop = rows - ROS_ROWS;
    }
    if (sRosTop < 0) {
        sRosTop = 0;
    }
}

/* (mockup 13) the faces of the new fighters in their window, the one under the cursor large at the right */
static void Roster_Draw(void) {
    static char line[96];
    s32 count = Port_SkinCount();
    s32 i;

    Mx_Window(48.0f, 150.0f, 690.0f, 688.0f);
    Mx_BoxTint(MM_TAB, 50.0f, 152.0f, 425.0f, 194.0f, 20.0f, Mx_Rgba(0x70, 0x60, 0x90, 0x80), 0x80);
    Mx_TextC(80.0f, 173.0f, "MOD GRID ORDER", 27.0f, 0.0f, Mx_Rgba(0xF0, 0xE4, 0xFF, 0x80), Mx_Rgba(0xD4, 0xC0, 0xFF, 0x80), Mx_Rgba(0x2C, 0x14, 0x5C, 0x80), 0, 0x80);
    if (count > 0) {
        sprintf(line, "%d / %d", (int)(sMm->rosCur + 1), (int)count);
        Mx_Text(670.0f, 175.0f, line, 30.0f, 0.0f, MX_WHITE, 2);
    }
    Roster_Scroll(count);
    for (i = sRosTop * ROS_COLS; i < count && i < (sRosTop + ROS_ROWS) * ROS_COLS; i++) {
        f32 x0 = 85.0f + 117.0f * (f32)(i % ROS_COLS);
        f32 y0 = 212.0f + 107.0f * (f32)(i / ROS_COLS - sRosTop);
        s32 sel = i == sMm->rosCur;

        Mx_FaceCell(PORT_REF_MOD + Port_SkinMod(i), x0, y0, x0 + 95.0f, y0 + 95.0f, sel);
        if (sel) {
            f32 b = 2.0f * Mm_Beat(40);

            Mx_Brackets(x0 - 12.0f - b, y0 - 12.0f - b, x0 + 107.0f + b, y0 + 107.0f + b, 22.0f, 5.0f, Mx_Rgba(0xFF, 0xD8, 0x40, 0x80));
            Mx_Fit(MM_UP, x0 + 47.5f, y0 - 18.0f - b, 18.0f, 0x80);
            Mx_Fit(MM_DOWN, x0 + 47.5f, y0 + 113.0f + b, 18.0f, 0x80);
            Mx_Fit(MM_LEFT, x0 - 18.0f - b, y0 + 47.5f, 18.0f, 0x80);
            Mx_Fit(MM_RIGHT, x0 + 113.0f + b, y0 + 47.5f, 18.0f, 0x80);
        }
    }
    if (sRosTop > 0) {
        Mx_Fit(MM_UP, 370.0f, 200.0f, 22.0f, 0x80);
    }
    if ((sRosTop + ROS_ROWS) * ROS_COLS < count) {
        Mx_Fit(MM_DOWN, 370.0f, 538.0f, 22.0f, 0x80);
    }
    if (count == 0) {
        Mx_Text(369.0f, 300.0f, "NO NEW FIGHTERS YET", 32.0f, 0.0f, MX_GOLD, 1);
        Mx_Say(369.0f, 350.0f, "To make one: Library, R2 Edit,", 25.0f, 560.0f, MX_WHITE, 1);
        Mx_Say(369.0f, 385.0f, "then set Type to New Fighter.", 25.0f, 560.0f, MX_WHITE, 1);
    }
    Mx_Say(82.0f, 623.0f, "The order new fighters show in when", 27.0f, 0.0f, MX_WHITE, 0);
    Mx_Say(82.0f, 659.0f, "you press", 27.0f, 0.0f, MX_WHITE, 0);
    Mx_Button(1, 90.0f + Mx_SayW("you press", 27.0f), 659.0f, 30.0f);
    Mx_Say(128.0f + Mx_SayW("you press", 27.0f), 659.0f, "on the character select.", 27.0f, 0.0f, MX_WHITE, 0);
    if (count > 0) {
        s32 mod = Port_SkinMod(sMm->rosCur);

        if (mod >= 0) {
            PortModInfo *m = &sMm->info[mod];

            Mx_Window(812.0f, 585.0f, 1330.0f, 688.0f);
            Mx_Text(1071.0f, 621.0f, Either(m->white[0] != 0, m->white, m->name), 40.0f, 480.0f, MX_GOLD, 1);
            sprintf(line, "Based on %.60s", m->target);
            Mx_Text(1071.0f, 662.0f, line, 23.0f, 480.0f, MX_GOLD, 1);
        }
    }
}

/* ------------------------------------------------------------------------------------------------- transformations
 * FORMS: a fighter's transformations and fusions (PortTrans), on two pages - L2 / R2 switch. Left: the fighter, the
 * slots (what each turns into, the input that does it, its stock cost), what plain R3 does, back to the original.
 * Right: what the slot under the cursor turns into, as a model on the platform, a line saying what it does, and its
 * settings: cross goes in, up / down choose one, left / right change it, cross on a fighter opens the list, circle
 * empties it, SELECT shows the advanced ones (the game's raw type and effect), triangle comes out. What a fighter has
 * before anything is set here is its file's own; new fighters can be targets and fusion results too.
 */
#define TR_LIST_X0 22.0f
#define TR_LIST_X1 246.0f
#define TR_FIELD_X0 22.0f   /* the settings of a slot: in the list's place while they are edited */
#define TR_FIELD_X1 268.0f
#define TR_FIELD_Y 104.0f
#define TR_FIELD_PITCH 26.0f
#define TR_INFO_X0 262.0f   /* under the model: what the slot does */
#define TR_INFO_X1 494.0f

/* the kinds of fusion */
static char *sTrFuseType[3] = {"-", "FUSION DANCE", "POTARA"};

/* The kind of change (+0xA0), as the FORMS screen names it. */
static char *sTrKind[5] = {"POWER UP", "BACK TO BASE", "GREAT APE", "ABSORB (CELL)", "MACHINE MERGE (PILAF)"};
static const s8 sTrKindAura[5] = {2, 1, 4, 7, 0}; /* (the aura a kind starts with when it is chosen) */

static char *sTrDir[4] = {"LEFT + R3", "UP + R3", "RIGHT + R3", "DOWN + R3"};
static char *sTrHold[3] = {"LEFT + HOLD R3", "UP + HOLD R3", "RIGHT + HOLD R3"}; /* (fusions: the skill list marks them held) */

/* the fields of a slot: transformations (plain, then advanced) / fusions */
#define TF_INTO 0
#define TF_KIND 1
#define TF_COST 2
#define TF_FRESH 3
#define TF_AURA 4
#define TF_ASSIST 5
#define FF_RESULT 0
#define FF_TYPE 1
#define FF_COST 2
#define FF_ANIM 3
#define FF_PARTNER 4 /* ..7 */
#define FF_LEAD 8

/* The aura a transformation bursts into: the change's effect (+0xA4), as the game's forms use them, which kinds of
   change it goes with (a bit per kind), and its colour for the preview behind the model (alpha 0: none). */
#define TR_BURSTS 10
static char *sTrBurst[TR_BURSTS] = {"NONE",                 "WHITE FLASH",        "SUPER SAIYAN (GOLD)", "SUPER SAIYAN 2 / 3 (GOLD, LIGHTNING)",
                                    "LEGENDARY (GREEN)",    "FORM CHANGE (PURPLE)", "MUSCLE UP (WHITE)", "NEEDS A MOON",
                                    "MAKES ITS OWN MOON",   "ABSORPTION"};
static const s8 sTrBurstVal[TR_BURSTS] = {0, 1, 2, 11, 9, 8, 10, 3, 4, 7};
static const u8 sTrBurstKinds[TR_BURSTS] = {0x13, 0x03, 0x01, 0x01, 0x01, 0x03, 0x01, 0x04, 0x04, 0x08};
static const u8 sTrBurstRgb[TR_BURSTS][4] = {{0, 0, 0, 0},         {235, 245, 255, 0x60}, {255, 210, 60, 0x70},  {255, 225, 80, 0x70},
                                             {90, 240, 120, 0x70}, {200, 110, 255, 0x68}, {255, 240, 215, 0x60}, {255, 150, 60, 0x50},
                                             {255, 150, 60, 0x50}, {160, 230, 90, 0x58}};
#define TR_BURST_LIGHTNING 3

/* The auras the kind of slot k can have: their places in the table above; how many. */
static s32 Trans_Bursts(s32 k, s32 *out) {
    s32 kind = sMm->tr.type[k] >= 0 && sMm->tr.type[k] < 5 ? sMm->tr.type[k] : 0;
    s32 n = 0;
    s32 i;

    for (i = 0; i < TR_BURSTS; i++) {
        if (sTrBurstKinds[i] & (1 << kind)) {
            out[n++] = i;
        }
    }
    return n;
}

/* Which aura of the table a slot's effect is, -1 none of them. */
static s32 Trans_Burst(s32 k) {
    s32 i;

    for (i = 0; i < TR_BURSTS; i++) {
        if (sTrBurstVal[i] == sMm->tr.effect[k]) {
            return i;
        }
    }
    return -1;
}

/* The kind of slot k changed: its aura, if it does not go with the new kind, becomes the kind's own. */
static void Trans_KindAura(s32 k) {
    s32 list[TR_BURSTS];
    s32 n = Trans_Bursts(k, list);
    s32 b = Trans_Burst(k);
    s32 i;

    for (i = 0; i < n && list[i] != b; i++) {
    }
    if (i == n) {
        sMm->tr.effect[k] = sTrKindAura[sMm->tr.type[k] >= 0 && sMm->tr.type[k] < 5 ? sMm->tr.type[k] : 0];
    }
}
/* The motions of a fusion's scene (PortTrans.fuseAnim). */
static char *sTrAnim[6] = {"BY THE KIND", "GOKU & VEGETA DANCE", "GOTEN & TRUNKS DANCE", "SSJ4 PAIR DANCE", "POTARA", "THE FIGHTERS' OWN"};

static s32 Trans_Slots(void) {
    return sMm->trPage == 0 ? 4 : 3;
}

/* The rows of the list: 0 the fighter, then the slots, then (transformations) plain R3, then back to the original. */
static s32 Trans_Rows(void) {
    if (sMm->trWho < 0) {
        return 1;
    }
    return sMm->trPage == 0 ? 7 : 5;
}

static s32 Trans_ResetRow(void) {
    return Trans_Rows() - 1;
}

/* How many fields the panel shows (the advanced ones only when asked for). */
static s32 Trans_Field(void) {
    if (sMm->trPage == 0) {
        return sMm->trAdv ? 6 : 5;
    }
    return sMm->trAdv ? 9 : 8;
}

/* What is set here is kept at once (and the part changed counts as set here). */
static void Trans_Keep(void) {
    if (sMm->trPage == 0) {
        sMm->tr.set = 1;
    } else {
        sMm->tr.fuseSet = 1;
    }
    Port_TransSet(sMm->trWho, &sMm->tr);
    sMm->rest = 0;
    if (sMm->trPage == 1) { /* (the two who fuse are read again: their files may carry the scene's motions now) */
        Mm_ExtrasReload();
    }
}

/* The field a fighter picked goes into (Picker_Choose, pickWhat 2 / 4). */
static void Trans_Picked(s32 ref) {
    s32 k = sMm->trCur - 1;

    if (k < 0) {
        return;
    }
    if (sMm->trPage == 0) {
        if (sMm->trField == TF_INTO) {
            sMm->tr.target[k] = ref;
            if (ref >= 0 && sMm->tr.cost[k] == 0 && k != 3) {
                sMm->tr.cost[k] = 1;
            }
        } else {
            sMm->tr.assist[k] = ref >= PORT_REF_MOD ? -1 : ref;
        }
    } else {
        if (sMm->trField == FF_RESULT) {
            sMm->tr.fuse[k] = ref;
        } else if (sMm->trField == FF_LEAD) {
            sMm->tr.fuseLead[k] = ref >= PORT_REF_MOD ? -1 : ref;
        } else {
            sMm->tr.partner[k][sMm->trField - FF_PARTNER] = ref >= PORT_REF_MOD ? -1 : ref;
        }
    }
    Trans_Keep();
}

static char *Trans_PickTitle(void) {
    if (sMm->trPage == 0) {
        return sMm->trField == TF_INTO ? "TRANSFORMS INTO" : "WHO ELSE IS IN ITS SCENE?";
    }
    return sMm->trField == FF_RESULT ? "FUSES INTO" : sMm->trField == FF_LEAD ? "PARTNER SHOWN IN ITS SCENE" : "FUSES WITH";
}

/* Opens the list of fighters for the field under the cursor: targets and results may be new fighters (2), the
   others are fighters of the roster (4: a teammate who is a new fighter counts as its template). */
static void Trans_Pick(void) {
    s32 k = sMm->trCur - 1;
    s32 now;

    if (sMm->trPage == 0) {
        now = sMm->trField == TF_INTO ? sMm->tr.target[k] : sMm->tr.assist[k];
        Picker_Open(sMm->trField == TF_INTO ? 2 : 4, MM_TRANSFORM, now);
    } else {
        now = sMm->trField == FF_RESULT ? sMm->tr.fuse[k] : sMm->trField == FF_LEAD ? sMm->tr.fuseLead[k] : sMm->tr.partner[k][sMm->trField - FF_PARTNER];
        Picker_Open(sMm->trField == FF_RESULT ? 2 : 4, MM_TRANSFORM, now);
    }
}

/* Left / right on a field: its value one step on. Returns 1 when it changed. */
static s32 Trans_Step(s32 dir) {
    s32 k = sMm->trCur - 1;
    s32 i;

    if (sMm->trPage == 0) {
        switch (sMm->trField) {
        case TF_KIND:
            sMm->tr.type[k] = (sMm->tr.type[k] + 5 + dir) % 5;
            Trans_KindAura(k);
            return 1;
        case TF_COST:
            if (sMm->tr.cost[k] + dir < 0 || sMm->tr.cost[k] + dir > 7) {
                return 0;
            }
            sMm->tr.cost[k] += dir;
            return 1;
        case TF_FRESH:
            sMm->tr.fresh[k] ^= 1;
            return 1;
        case TF_AURA: {
            s32 list[TR_BURSTS];
            s32 n = Trans_Bursts(k, list);
            s32 b = Trans_Burst(k);

            for (i = 0; i < n && list[i] != b; i++) {
            }
            i = i == n ? 0 : (i + n + dir) % n;
            sMm->tr.effect[k] = sTrBurstVal[list[i]];
            return 1;
        }
        }
    } else {
        switch (sMm->trField) {
        case FF_ANIM:
            sMm->tr.fuseAnim[k] = (sMm->tr.fuseAnim[k] + 6 + dir) % 6;
            return 1;
        case FF_TYPE:
            sMm->tr.fuseType[k] = sMm->tr.fuseType[k] == 2 ? 1 : 2;
            return 1;
        case FF_COST:
            if (sMm->tr.fuseCost[k] + dir < 0 || sMm->tr.fuseCost[k] + dir > 7) {
                return 0;
            }
            sMm->tr.fuseCost[k] += dir;
            return 1;
        }
    }
    return 0;
}

/* ---- a field's choices as a list (cross on a field that is not a fighter): no stepping through them one by one ---- */
static char *sTrStocks[8] = {"0 STOCKS", "1 STOCK", "2 STOCKS", "3 STOCKS", "4 STOCKS", "5 STOCKS", "6 STOCKS", "7 STOCKS"};
static char *sTrFresh[2] = {"KEPT AS THEY ARE", "REPAIRED"};

/* How many choices the field under the cursor has (0: none to list - a fighter, or the scene's lead). */
static s32 Trans_Choices(void) {
    if (sMm->trPage == 0) {
        switch (sMm->trField) {
        case TF_KIND:
            return 5;
        case TF_COST:
            return 8;
        case TF_FRESH:
            return 2;
        case TF_AURA: {
            s32 list[TR_BURSTS];

            return Trans_Bursts(sMm->trCur - 1, list);
        }
        }
        return 0;
    }
    return sMm->trField == FF_TYPE ? 2 : sMm->trField == FF_COST ? 8 : sMm->trField == FF_ANIM ? 6 : 0;
}

static char *Trans_ChoiceName(s32 i) {
    if (sMm->trPage == 0) {
        switch (sMm->trField) {
        case TF_KIND:
            return sTrKind[i];
        case TF_COST:
            return sTrStocks[i];
        case TF_AURA: {
            s32 list[TR_BURSTS];

            if (i < Trans_Bursts(sMm->trCur - 1, list)) { /* (not as a "? :" with a literal: the table's 4-byte
                                                              pointer was then read as an 8-byte one) */
                return sTrBurst[list[i]];
            }
            return "";
        }
        }
        return sTrFresh[i];
    }
    return sMm->trField == FF_TYPE ? sTrFuseType[i + 1] : sMm->trField == FF_ANIM ? sTrAnim[i] : sTrStocks[i];
}

/* The choice that is set now (-1: none of them, a look of its own). */
static s32 Trans_ChoiceNow(void) {
    s32 k = sMm->trCur - 1;
    s32 i;

    if (sMm->trPage == 0) {
        switch (sMm->trField) {
        case TF_KIND:
            return sMm->tr.type[k];
        case TF_COST:
            return sMm->tr.cost[k];
        case TF_AURA: {
            s32 list[TR_BURSTS];
            s32 n = Trans_Bursts(k, list);
            s32 b = Trans_Burst(k);

            for (i = 0; i < n && list[i] != b; i++) {
            }
            return i < n ? i : -1;
        }
        }
        return sMm->tr.fresh[k] ? 1 : 0;
    }
    if (sMm->trField == FF_ANIM) {
        return sMm->tr.fuseAnim[k];
    }
    return sMm->trField == FF_TYPE ? (sMm->tr.fuseType[k] == 2 ? 1 : 0) : sMm->tr.fuseCost[k];
}

static void Trans_ChoiceSet(s32 i) {
    s32 k = sMm->trCur - 1;

    if (sMm->trPage == 0) {
        switch (sMm->trField) {
        case TF_KIND:
            sMm->tr.type[k] = i;
            Trans_KindAura(k);
            break;
        case TF_COST:
            sMm->tr.cost[k] = i;
            break;
        case TF_FRESH:
            sMm->tr.fresh[k] = i;
            break;
        case TF_AURA: {
            s32 list[TR_BURSTS];

            if (i < Trans_Bursts(k, list)) {
                sMm->tr.effect[k] = sTrBurstVal[list[i]];
            }
            break;
        }
        }
    } else if (sMm->trField == FF_ANIM) {
        sMm->tr.fuseAnim[k] = i;
    } else if (sMm->trField == FF_TYPE) {
        sMm->tr.fuseType[k] = i + 1;
    } else {
        sMm->tr.fuseCost[k] = i;
    }
    Trans_Keep();
}

/* The list of choices beside the settings window, the one set now marked, the cursor's gold. */
static void Trans_DrawChoices(f32 x1) {
    s32 n = Trans_Choices();
    s32 now = Trans_ChoiceNow();
    f32 pitch = n > 8 ? 42.0f : 46.0f;
    f32 h = 74.0f + pitch * (f32)n;
    f32 y0 = 400.0f - h * 0.5f;
    f32 x0 = x1 - 330.0f;
    s32 i;

    if (!sMm->trList || n == 0) {
        return;
    }
    if (y0 < 140.0f) {
        y0 = 140.0f;
    }
    Mx_Box(MM_WINDOW, x0, y0 + 24.0f, x1, y0 + h, 22.0f, 0x80);
    Mx_Box(MM_BAR_SEL, x0 + 12.0f, y0, x1 - 12.0f, y0 + 48.0f, 22.0f, 0x80);
    Mx_TextC((x0 + x1) * 0.5f, y0 + 24.0f, sMm->trPage == 0 ? (sMm->trField == TF_KIND ? "KIND" : sMm->trField == TF_COST ? "STOCKS" : sMm->trField == TF_FRESH ? "CLOTHES" : "AURA")
                                                     : (sMm->trField == FF_TYPE ? "KIND" : sMm->trField == FF_ANIM ? "ANIMATIONS" : "STOCKS"),
             24.0f, 0.0f, Mx_Rgba(0xFF, 0xF0, 0xB0, 0x80), Mx_Rgba(0xF0, 0xA0, 0x30, 0x80), Mx_Rgba(0x50, 0x20, 0x06, 0x80), 1, 0x80);
    for (i = 0; i < n; i++) {
        f32 iy = y0 + 62.0f + pitch * (f32)i;

        if (i == sMm->trListCur) {
            Mx_Box(MM_BAR_SEL, x0 + 16.0f, iy - 2.0f, x1 - 16.0f, iy + pitch - 6.0f, 17.0f, 0x80);
            Mx_TextC((x0 + x1) * 0.5f, iy + (pitch - 8.0f) * 0.5f, Trans_ChoiceName(i), 21.0f, x1 - x0 - 80.0f, Mx_Rgba(0xFF, 0xFF, 0xF4, 0x80), Mx_Rgba(0xFF, 0xF0, 0xD0, 0x80),
                     Mx_Rgba(0x6C, 0x34, 0x08, 0x80), 1, 0x80);
        } else {
            Mx_Box(MM_BAR, x0 + 20.0f, iy, x1 - 20.0f, iy + pitch - 8.0f, 15.0f, 0x80);
            Mx_Text((x0 + x1) * 0.5f, iy + (pitch - 8.0f) * 0.5f, Trans_ChoiceName(i), 20.0f, x1 - x0 - 80.0f, MX_LAV, 1);
        }
        if (i == now) { /* (what is set now) */
            Mx_Fit(MM_PIP, x0 + 36.0f, iy + (pitch - 8.0f) * 0.5f, 18.0f, 0x80);
        }
        if (sMm->trPage == 0 && sMm->trField == TF_AURA) { /* (its colour) */
            s32 list[TR_BURSTS];
            s32 b = i < Trans_Bursts(sMm->trCur - 1, list) ? list[i] : 0;
            u32 c = Mx_Rgba(sTrBurstRgb[b][0], sTrBurstRgb[b][1], sTrBurstRgb[b][2], sTrBurstRgb[b][3] != 0 ? 0x80 : 0);

            Mx_Rect(x1 - 52.0f, iy + 6.0f, x1 - 30.0f, iy + pitch - 14.0f, c, c);
        }
    }
}

/* Whether the field under the cursor is a fighter (cross opens the list). */
static s32 Trans_IsFighterField(void) {
    if (sMm->trPage == 0) {
        return sMm->trField == TF_INTO || sMm->trField == TF_ASSIST;
    }
    return sMm->trField == FF_RESULT || sMm->trField >= FF_PARTNER;
}

/* Circle on a field: empty. */
static void Trans_Empty(void) {
    s32 k = sMm->trCur - 1;

    if (sMm->trPage == 0) {
        if (sMm->trField == TF_INTO) {
            sMm->tr.target[k] = -1;
            if (sMm->tr.next == k) {
                sMm->tr.next = -1;
            }
        } else if (sMm->trField == TF_ASSIST) {
            sMm->tr.assist[k] = -1;
        }
    } else {
        if (sMm->trField == FF_RESULT) {
            sMm->tr.fuse[k] = -1;
        } else if (sMm->trField == FF_LEAD) {
            sMm->tr.fuseLead[k] = -1;
        } else if (sMm->trField >= FF_PARTNER) {
            sMm->tr.partner[k][sMm->trField - FF_PARTNER] = -1;
        }
    }
}

/* What the slot under the cursor turns into (a character id or PORT_REF_MOD + mod), -1 none. */
static s32 Trans_SlotRef(void) {
    s32 k = sMm->trCur - 1;

    if (sMm->trWho < 0 || k < 0 || k >= Trans_Slots()) {
        return -1;
    }
    return sMm->trPage == 0 ? sMm->tr.target[k] : sMm->tr.fuse[k];
}

/* What would go wrong with the slot under the cursor, in words; "" nothing. */
static char *Trans_Warning(void) {
    s32 k = sMm->trCur - 1;
    s32 ref = Trans_SlotRef();
    s32 i;

    if (ref < 0) {
        return "";
    }
    if (ref >= PORT_REF_MOD) {
        PortModInfo *m = &sMm->info[ref - PORT_REF_MOD];

        if (!m->isNew) {
            return "THAT MOD REPLACES A FIGHTER: PICK THE FIGHTER";
        }
        if (!m->on || m->skin < 0) {
            return "THAT NEW FIGHTER IS OFF: TURN IT ON IN THE LIBRARY";
        }
    }
    if (ref == sMm->trWho) {
        return "IT TURNS INTO ITSELF";
    }
    if (sMm->trPage == 0) {
        if (sMm->tr.effect[k] == 3) {
            return "GREAT APE (NEEDS A MOON): ONLY ON MOON STAGES";
        }
        return "";
    }
    for (i = 0; i < 4 && sMm->tr.partner[k][i] < 0; i++) {
    }
    if (i == 4) {
        return "NO PARTNER SET: IT CAN NEVER FUSE";
    }
    return "";
}

/* The line over the model: how the slot is done and what it turns into. */
static char *Trans_Summary(void) {
    static char line[96];
    s32 k = sMm->trCur - 1;
    s32 ref = Trans_SlotRef();
    s32 i;

    if (ref < 0) {
        return sMm->trWho >= 0 && k >= 0 && k < Trans_Slots() ? "EMPTY: CROSS, THEN PICK WHAT IT TURNS INTO" : "";
    }
    if (sMm->trPage == 0) {
        sprintf(line, "%s, %d STOCK%s: %.40s", Either(k == sMm->tr.next, "R3", sTrDir[k]), (int)sMm->tr.cost[k], sMm->tr.cost[k] == 1 ? "" : "S",
                Mm_RefName(ref));
        return line;
    }
    for (i = 0; i < 4 && sMm->tr.partner[k][i] < 0; i++) {
    }
    sprintf(line, "%s WITH %.24s: %.30s", sTrHold[k], Either(i < 4, Mm_RefName(i < 4 ? sMm->tr.partner[k][i] : 0), "NOBODY"), Mm_RefName(ref));
    return line;
}

static void Trans_Update(s32 live) {
    Pad *pad = &gPad[0];
    s32 was = sMm->trCur;
    s32 rows = Trans_Rows();
    s32 slotRow = sMm->trCur >= 1 && sMm->trCur <= Trans_Slots();
    s32 ref = Trans_SlotRef();

    /* what the slot turns into, on the platform */
    sMm->wantKey = -1;
    if (ref >= 0) {
        if (ref >= PORT_REF_MOD) {
            PortModInfo *m = &sMm->info[ref - PORT_REF_MOD];

            if (m->chara >= 0) {
                sMm->wantKey = 0x40000 + ref;
                sMm->wantChara = m->chara;
                sMm->wantCostume = 0;
                sMm->wantPreview = ref - PORT_REF_MOD;
            }
        } else if (ref < MM_CHARA_COUNT) {
            sMm->wantKey = 0x40000 + ref;
            sMm->wantChara = ref;
            sMm->wantCostume = 0;
            sMm->wantPreview = -1;
        }
    } else if (sMm->trWho >= 0 && !slotRow && sMm->trPage == 0) { /* (not on a slot: the fighter itself) */
        s32 who = sMm->trWho;

        if (who >= PORT_REF_MOD && sMm->info[who - PORT_REF_MOD].chara >= 0) {
            sMm->wantKey = 0x40000 + who;
            sMm->wantChara = sMm->info[who - PORT_REF_MOD].chara;
            sMm->wantCostume = 0;
            sMm->wantPreview = who - PORT_REF_MOD;
        } else if (who >= 0 && who < MM_CHARA_COUNT) {
            sMm->wantKey = 0x40000 + who;
            sMm->wantChara = who;
            sMm->wantCostume = 0;
            sMm->wantPreview = -1;
        }
    }
    if (sMm->trPage == 1) { /* the result in the middle, the two who fuse above it */
        Mm_SetModel(690.0f, 610.0f, 320.0f);
        if (ref >= 0) {
            s32 i;

            for (i = 0; i < 4 && sMm->tr.partner[sMm->trCur - 1][i] < 0; i++) {
            }
            Mm_SetExtra(0, sMm->trWho, 545.0f, 300.0f, 170.0f);
            if (i < 4) {
                Mm_SetExtra(1, sMm->tr.partner[sMm->trCur - 1][i], 835.0f, 300.0f, 170.0f);
            }
        }
        if (ref >= 0) {
            s32 k = sMm->trCur - 1;
            s32 set = sMm->tr.fuseAnim[k] > 0 && sMm->tr.fuseAnim[k] <= 4 ? sMm->tr.fuseAnim[k] : sMm->tr.fuseType[k] == 2 ? 4 : 1;
            if (sMm->tr.fuseAnim[k] != 5) {
                Mm_SetExtraMotion(set == 2 ? 3 : 4); /* (the dance: Goten and Trunks have theirs at another place) */
            }
        }
    } else if (slotRow) { /* before (the fighter) and after (the form) */
        Mm_SetModel(930.0f, 548.0f, 300.0f);
        if (ref >= 0) { /* the aura it bursts into, behind the form (the list's cursor while the list is open) */
            s32 b = Trans_Burst(sMm->trCur - 1);

            if (sMm->trFocus && sMm->trList && sMm->trField == TF_AURA) {
                s32 list[TR_BURSTS];

                b = sMm->trListCur < Trans_Bursts(sMm->trCur - 1, list) ? list[sMm->trListCur] : -1;
            }
            if (b >= 0 && b < TR_BURSTS && sTrBurstRgb[b][3] != 0) {
                Mm_SetGlow(930.0f, 548.0f, 300.0f, Mx_Rgba(sTrBurstRgb[b][0], sTrBurstRgb[b][1], sTrBurstRgb[b][2], sTrBurstRgb[b][3]));
                Mm_SetGlowBolts(b == TR_BURST_LIGHTNING); /* (Super Saiyan 2 and 3: its lightning) */
            }
        }
        if (ref >= 0) {
            Mm_SetExtra(0, sMm->trWho, 640.0f, 548.0f, 300.0f);
        }
    } else {
        Mm_SetModel(785.0f, 548.0f, 370.0f);
    }
    if (sMm->trFocus && sMm->trList) {
        Mm_Prompt(0, "CHOOSE");
        Mm_Prompt(2, "CLOSE");
    } else if (sMm->trFocus) {
        if (!Trans_IsFighterField() && Trans_Choices() > 0) {
            Mm_Prompt(0, "CHOICES");
        }
        Mm_Prompt(3, "CHANGE");
        if (Trans_IsFighterField()) {
            Mm_Prompt(0, "PICK FIGHTER");
            Mm_Prompt(4, "EMPTY");
        }
        Mm_Prompt(13, sMm->trAdv ? "LESS" : "ADVANCED");
        Mm_Prompt(2, "DONE");
    } else {
        if (sMm->trCur == 0 || sMm->trWho < 0) {
            Mm_Prompt(0, "PICK FIGHTER");
        } else if (slotRow) {
            Mm_Prompt(0, "EDIT SLOT");
            Mm_Prompt(3, "STOCK COST");
            Mm_Prompt(4, "EMPTY");
        } else if (sMm->trCur == Trans_ResetRow()) {
            Mm_Prompt(0, "RESET");
        } else {
            Mm_Prompt(3, "CHANGE");
        }
        if (sMm->trWho >= 0) {
            Mm_Prompt(10, sMm->trPage == 0 ? "FUSIONS" : "TRANSFORMATIONS");
        }
        Mm_Prompt(2, "BACK");
    }
    if (!live) {
        return;
    }
    if (sMm->trFocus && sMm->trList) { /* the list of a field's choices */
        s32 n = Trans_Choices();
        s32 was2 = sMm->trListCur;

        if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            sMm->trList = 0;
            Snd_PlaySe(1, 2);
            return;
        }
        if (n > 0 && (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2))) {
            sMm->trListCur = (sMm->trListCur + 1) % n;
        } else if (n > 0 && (pad->gameRepeat & (PADG_UP | PADG_UP2))) {
            sMm->trListCur = (sMm->trListCur + n - 1) % n;
        }
        if (sMm->trListCur != was2) {
            Snd_PlaySe(1, 0);
            return;
        }
        if ((pad->gamePressed & PADG_CROSS) && n > 0) {
            Trans_ChoiceSet(sMm->trListCur);
            sMm->trList = 0;
            Snd_PlaySe(1, 1);
        }
        return;
    }
    if (sMm->trFocus) { /* the fields of the slot */
        s32 f = sMm->trField;
        s32 n = Trans_Field();

        if (pad->gamePressed & PADG_TRIANGLE) {
            sMm->trFocus = 0;
            sMm->trList = 0;
            Snd_PlaySe(1, 2);
            return;
        }
        if (pad->gamePressed & PADG_SELECT) {
            sMm->trAdv ^= 1;
            if (sMm->trField >= Trans_Field()) {
                sMm->trField = 0;
            }
            Snd_PlaySe(1, 0);
            return;
        }
        if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            sMm->trField = (f + 1) % n;
        } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            sMm->trField = (f + n - 1) % n;
        }
        if (sMm->trField != f) {
            Snd_PlaySe(1, 0);
            return;
        }
        if ((pad->gamePressed & PADG_CROSS) && Trans_IsFighterField()) {
            Snd_PlaySe(1, 1);
            Trans_Pick();
        } else if ((pad->gamePressed & PADG_CROSS) && Trans_Choices() > 0) { /* its choices, as a list */
            s32 now = Trans_ChoiceNow();

            sMm->trList = 1;
            sMm->trListCur = now >= 0 ? now : 0;
            Snd_PlaySe(1, 1);
        } else if ((pad->gamePressed & PADG_CIRCLE) && Trans_IsFighterField()) {
            Trans_Empty();
            Trans_Keep();
            Snd_PlaySe(1, 2);
        } else if ((pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2 | PADG_LEFT | PADG_LEFT2)) || (pad->gamePressed & PADG_CROSS)) {
            s32 dir = (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -1 : 1;

            if (Trans_Step(dir)) {
                Trans_Keep();
                Snd_PlaySe(1, 0);
            }
        }
        return;
    }
    if (pad->gamePressed & PADG_L1) {
        Snd_PlaySe(1, 0);
        Mm_SetMode(MM_ROSTER);
        return;
    }
    if (pad->gamePressed & PADG_R1) {
        Snd_PlaySe(1, 0);
        Mm_SetMode(MM_LIBRARY);
        return;
    }
    if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(MM_LIBRARY);
        return;
    }
    if ((pad->gamePressed & (PADG_L2 | PADG_R2)) && sMm->trWho >= 0) { /* the other page */
        sMm->trPage ^= 1;
        sMm->trCur = sMm->trCur > 0 ? 1 : 0;
        sMm->rest = 0;
        Snd_PlaySe(1, 0);
        return;
    }
    if (sMm->trCur >= rows) {
        sMm->trCur = 0;
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sMm->trCur = (sMm->trCur + 1) % rows;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sMm->trCur = (sMm->trCur + rows - 1) % rows;
    }
    if (sMm->trCur != was) {
        sMm->rest = 0;
        Snd_PlaySe(1, 0);
        return;
    }
    if (sMm->trCur == 0) {
        if (pad->gamePressed & PADG_CROSS) {
            Snd_PlaySe(1, 1);
            Picker_Open(1, MM_TRANSFORM, sMm->trWho);
        }
        return;
    }
    if (sMm->trCur == Trans_ResetRow()) {
        if (pad->gamePressed & PADG_CROSS) {
            if (sMm->trPage == 0) {
                sMm->tr.set = 0;
            } else {
                sMm->tr.fuseSet = 0;
            }
            Port_TransSet(sMm->trWho, &sMm->tr);
            Port_TransGet(sMm->trWho, &sMm->tr); /* (what the fighter's file has, again) */
            Mm_Note(sMm->trPage == 0 ? "TRANSFORMATIONS BACK TO THE ORIGINAL" : "FUSIONS BACK TO THE ORIGINAL");
            Snd_PlaySe(1, 1);
        }
        return;
    }
    if (sMm->trPage == 0 && sMm->trCur == 5) { /* what plain R3 does */
        if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2 | PADG_LEFT | PADG_LEFT2)) {
            s32 dir = (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? 3 : 1;
            s32 n = sMm->tr.next + 1; /* -1..2 as 0..3 */
            s32 k;

            for (k = 0; k < 4; k++) {
                n = (n + dir) % 4;
                if (n == 0 || sMm->tr.target[n - 1] >= 0) {
                    break;
                }
            }
            sMm->tr.next = n - 1;
            Trans_Keep();
            Snd_PlaySe(1, 0);
        }
        return;
    }
    { /* a slot */
        s32 k = sMm->trCur - 1;

        if (pad->gamePressed & PADG_CROSS) {
            sMm->trFocus = 1;
            sMm->trField = 0;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & PADG_CIRCLE) {
            if (sMm->trPage == 0) {
                sMm->tr.target[k] = -1;
                if (sMm->tr.next == k) {
                    sMm->tr.next = -1;
                }
            } else {
                sMm->tr.fuse[k] = -1;
            }
            Trans_Keep();
            Snd_PlaySe(1, 2);
        } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2 | PADG_LEFT | PADG_LEFT2)) {
            s32 dir = (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) ? -1 : 1;
            s32 *cost = sMm->trPage == 0 ? &sMm->tr.cost[k] : &sMm->tr.fuseCost[k];

            if (*cost + dir >= 0 && *cost + dir <= 7) {
                *cost += dir;
                Trans_Keep();
                Snd_PlaySe(1, 0);
            }
        }
    }
}

/* The fighter of a slot, as a name ("-" none). */
static char *Trans_Name(s32 ref) {
    return Either(ref < 0, "-", Mm_RefName(ref));
}

/* What a field holds, in words. */
static char *Trans_Value(s32 f) {
    static char line[48];
    s32 k = sMm->trCur - 1;
    s32 i;

    if (sMm->trPage == 0) {
        switch (f) {
        case TF_INTO:
            return Trans_Name(sMm->tr.target[k]);
        case TF_KIND:
            return sTrKind[sMm->tr.type[k] >= 0 && sMm->tr.type[k] < 5 ? sMm->tr.type[k] : 0];
        case TF_COST:
            sprintf(line, "< %d >", (int)sMm->tr.cost[k]);
            return line;
        case TF_ASSIST:
            return Trans_Name(sMm->tr.assist[k]);
        case TF_FRESH:
            return sMm->tr.fresh[k] ? "REPAIRED" : "KEPT AS THEY ARE";
        case TF_AURA:
            i = Trans_Burst(k);
            return Either(i >= 0, sTrBurst[i >= 0 ? i : 0], "THE FILE'S OWN");
        }
    } else {
        switch (f) {
        case FF_ANIM:
            return sTrAnim[sMm->tr.fuseAnim[k] >= 0 && sMm->tr.fuseAnim[k] <= 5 ? sMm->tr.fuseAnim[k] : 0];
        case FF_RESULT:
            return Trans_Name(sMm->tr.fuse[k]);
        case FF_TYPE:
            return sTrFuseType[sMm->tr.fuseType[k] == 2 ? 2 : 1];
        case FF_COST:
            sprintf(line, "< %d >", (int)sMm->tr.fuseCost[k]);
            return line;
        case FF_LEAD:
            return Either(sMm->tr.fuseLead[k] < 0, "THE FIRST PARTNER", Mm_RefName(sMm->tr.fuseLead[k]));
        default:
            return Trans_Name(sMm->tr.partner[k][f - FF_PARTNER]);
        }
    }
    return "";
}

/* The stock pips of a slot, three to a row, from (x0, y top). */
static void Trans_Pips(f32 x0, f32 y0, s32 n) {
    s32 k;

    for (k = 0; k < n; k++) {
        Mx_Fit(MM_PIP, x0 + 7.0f + 13.0f * (f32)(k % 3) + ((k / 3) & 1) * 6.0f, y0 + 7.0f + 12.0f * (f32)(k / 3), 16.0f, 0x80);
    }
}

/* The settings of the slot under the cursor, in the window at the right (cross goes in: the lit one changes). */
static void Trans_Fields(f32 x0, f32 y0, f32 x1, f32 y1, s32 centred) {
    static char *trLabel[6] = {"INTO", "KIND", "STOCKS", "CLOTHES", "AURA", "IN SCENE"};
    static char *fuLabel[9] = {"INTO", "KIND", "STOCKS", "ANIMATIONS", "PARTNER 1", "PARTNER 2", "PARTNER 3", "PARTNER 4", "SHOWN"};
    static char lines[2][64];
    s32 n = Trans_Field();
    f32 pitch = (y1 - y0 - 30.0f) / (f32)n;
    f32 cap = pitch >= 90.0f ? 27.0f : pitch >= 60.0f ? 24.0f : 21.0f;
    f32 x = centred ? (x0 + x1) * 0.5f : x0 + 24.0f;
    s32 f;

    for (f = 0; f < n; f++) {
        f32 top = y0 + 15.0f + pitch * (f32)f;
        s32 lit = sMm->trFocus && f == sMm->trField;
        s32 lc;
        s32 k;

        if (lit) {
            Mx_Rect(x0 + 8.0f, top, x1 - 8.0f, top + pitch - 4.0f, Mx_Rgba(0x04, 0x18, 0x22, 0x68), Mx_Rgba(0x04, 0x18, 0x22, 0x68));
            Mx_Frame(x0 + 8.0f, top, x1 - 8.0f, top + pitch - 4.0f, 2.0f, Mx_Rgba(0xFF, 0xD0, 0x50, 0x80));
        }
        Mx_Text(x, top + cap * 0.75f, sMm->trPage == 0 ? trLabel[f] : fuLabel[f], cap, x1 - x0 - 40.0f, sMm->trPage == 0 ? MX_CYAN : MX_GOLD, centred);
        lc = pitch >= cap * 3.3f ? Mx_Wrap(Trans_Value(f), cap, x1 - x0 - 40.0f, lines, 2) : 1;
        if (lc <= 1) {
            Str_Copy(lines[0], Trans_Value(f), 64);
            lc = 1;
        }
        for (k = 0; k < lc; k++) {
            Mx_Text(x, top + cap * 2.0f + cap * 1.25f * (f32)k, lines[k], cap, x1 - x0 - 40.0f, lit ? MX_GOLD : sMm->trPage == 0 ? MX_ORANGE : MX_WHITE, centred);
        }
    }
}

/* (mockups 14 and 15) FORMS: the slots at the left, before and after in the middle, the settings at the right */
static void Trans_Draw(void) {
    static char *slotT[4] = {"FORM 1", "FORM 2", "FORM 3", "REVERT"};
    static char *slotF[3] = {"FUSION 1", "FUSION 2", "FUSION 3"};
    static char line[96];
    static char two[2][64];
    s32 slot = sMm->trCur >= 1 && sMm->trCur <= Trans_Slots();
    s32 ref = Trans_SlotRef();
    char *warn = Trans_Warning();
    s32 k;

    Mx_SubTabs("TRANSFORMATIONS", "FUSIONS", sMm->trPage, 108.0f);
    if (sMm->trPage == 0) {
        /* the fighter, the four slots, plain R3, back to the original */
        Mx_Row(55.0f, 172.0f, 553.0f, 232.0f, sMm->trCur == 0, 0x80);
        Mx_Text(78.0f, 202.0f, "FIGHTER", 27.0f, 0.0f, MX_LAV, 0);
        Mx_Text(96.0f + Mx_TextW("FIGHTER", 27.0f), 202.0f, Either(sMm->trWho < 0, "CROSS: PICK ONE", Mm_RefName(sMm->trWho)), 27.0f, 375.0f - Mx_TextW("FIGHTER", 27.0f),
                sMm->trWho < 0 ? MX_ORANGE : MX_LAV, 0);
        if (sMm->trWho >= 0) {
            Mx_FaceCell(sMm->trWho, 495.0f, 181.0f, 537.0f, 223.0f, 0);
        }
        if (sMm->trWho < 0) {
            Mx_Window(55.0f, 260.0f, 553.0f, 520.0f);
            Mx_Text(304.0f, 300.0f, "PICK A FIGHTER", 30.0f, 0.0f, MX_GOLD, 1);
            Mx_Say(80.0f, 350.0f, "Then set what it transforms and", 24.0f, 450.0f, MX_WHITE, 0);
            Mx_Say(80.0f, 382.0f, "fuses into, how, and what it costs.", 24.0f, 450.0f, MX_WHITE, 0);
            Mx_Say(80.0f, 430.0f, "New fighters from mods work too:", 24.0f, 450.0f, MX_CYAN, 0);
            Mx_Say(80.0f, 462.0f, "as the fighter, and as a form.", 24.0f, 450.0f, MX_CYAN, 0);
            return;
        }
        for (k = 0; k < 4; k++) {
            f32 y0 = 252.0f + 80.0f * (f32)k;
            s32 r = sMm->tr.target[k];

            Mx_Row(55.0f, y0, 553.0f, y0 + 60.0f, sMm->trCur == k + 1, 0x80);
            Mx_Text(78.0f, y0 + 30.0f, slotT[k], 27.0f, 0.0f, MX_LAV, 0);
            Mx_TextA(208.0f, y0 + 30.0f, Trans_Name(r), 27.0f, 190.0f, MX_LAV, 0, r < 0 ? 0x60 : 0x80);
            if (r >= 0) {
                if (sMm->tr.next == k) {
                    Mx_Text(425.0f, y0 + 30.0f, "R3", 20.0f, 0.0f, MX_WHITE, 1);
                } else { /* "UP" over "+ R3" */
                    Str_Copy(two[0], sTrDir[k], 64);
                    two[0][Str_Len(two[0]) - 5] = 0;
                    Mx_Text(425.0f, y0 + 20.0f, two[0], 16.0f, 46.0f, MX_WHITE, 1);
                    Mx_Text(425.0f, y0 + 40.0f, "+ R3", 16.0f, 0.0f, MX_WHITE, 1);
                }
                Mx_FaceCell(r, 452.0f, y0 + 9.0f, 494.0f, y0 + 51.0f, 0);
                Trans_Pips(500.0f, y0 + 9.0f, sMm->tr.cost[k]);
            }
        }
        Mx_Row(55.0f, 572.0f, 523.0f, 624.0f, sMm->trCur == 5, 0x80);
        if (sMm->tr.next >= 0 && sMm->tr.target[sMm->tr.next] >= 0) {
            sprintf(line, "PLAIN R3: < %s >", slotT[sMm->tr.next]);
        } else {
            sprintf(line, "PLAIN R3: < NOTHING >");
        }
        Mx_Text(289.0f, 598.0f, line, 27.0f, 440.0f, MX_LAV, 1);
        Mx_Row(55.0f, 635.0f, 523.0f, 687.0f, sMm->trCur == 6, 0x80);
        Mx_Text(289.0f, 661.0f, "BACK TO THE ORIGINAL FORMS", 27.0f, 440.0f, MX_LAV, 1);
        /* the middle: from the fighter (its face) to the form (the model), what it does */
        if (slot && ref >= 0) {
            Mx_Fit(MM_RIGHT, 785.0f + 4.0f * Mm_Beat(30), 420.0f, 70.0f, 0x80);
        }
        if (slot) {
            Mx_Box(MM_NAVBAR, 553.0f, 595.0f, 1003.0f, 688.0f, 16.0f, 0x80);
            Mx_Frame(560.0f, 601.0f, 996.0f, 682.0f, 1.5f, Mx_Rgba(0x90, 0xB8, 0xF0, 0x60));
            Str_Copy(line, Trans_Summary(), 96);
            for (k = 0; line[k] != 0 && !(line[k] == ':' && line[k + 1] == ' '); k++) {
            }
            if (line[k] == ':') {
                line[k + 1] = 0;
                Mx_Text(778.0f, 623.0f, line, 26.0f, 430.0f, MX_GOLD, 1);
                Mx_Text(778.0f, 660.0f, line + k + 2, 26.0f, 430.0f, MX_GOLD, 1);
            } else {
                Mx_Text(778.0f, 641.0f, line, 23.0f, 430.0f, MX_GOLD, 1);
            }
            if (warn[0] != 0) {
                Mx_Text(778.0f, 572.0f, warn, 21.0f, 450.0f, MX_RED, 1);
            }
        }
        Mx_Window(1028.0f, 160.0f, 1345.0f, 612.0f);
        if (slot) {
            Trans_Fields(1028.0f, 160.0f, 1345.0f, 612.0f, 1);
            Trans_DrawChoices(1018.0f);
        } else {
            Mx_Text(1186.0f, 200.0f, "FIGHTER", 27.0f, 0.0f, MX_CYAN, 1);
            Mx_Wrap(Mm_RefName(sMm->trWho), 27.0f, 280.0f, two, 2);
            Mx_Text(1186.0f, 240.0f, two[0], 27.0f, 280.0f, MX_ORANGE, 1);
            Mx_Text(1186.0f, 274.0f, two[1], 27.0f, 280.0f, MX_ORANGE, 1);
            Mx_Text(1186.0f, 340.0f, "ITS FORMS", 27.0f, 0.0f, MX_CYAN, 1);
            Mx_Text(1186.0f, 380.0f, sMm->tr.set ? "SET HERE" : "AS ITS FILE HAS THEM", 24.0f, 280.0f, MX_ORANGE, 1);
            Mx_Say(1186.0f, 460.0f, "The skill list follows", 22.0f, 280.0f, MX_WHITE, 1);
            Mx_Say(1186.0f, 490.0f, "what is set here.", 22.0f, 280.0f, MX_WHITE, 1);
            Mx_Say(1186.0f, 560.0f, "Cross on a form: edit it", 22.0f, 280.0f, MX_GOLD, 1);
        }
        return;
    }
    /* fusions: the fighter, three cards, back to the original */
    Mx_Row(55.0f, 148.0f, 485.0f, 192.0f, sMm->trCur == 0, 0x80);
    Mx_Text(78.0f, 170.0f, "FIGHTER", 24.0f, 0.0f, MX_GOLD, 0);
    Mx_Text(84.0f + Mx_TextW("FIGHTER", 24.0f), 170.0f, Mm_RefName(sMm->trWho), 24.0f, 280.0f, MX_WHITE, 0);
    for (k = 0; k < 3; k++) {
        f32 y0 = 205.0f + 115.0f * (f32)k;
        s32 r = sMm->tr.fuse[k];
        s32 sel = sMm->trCur == k + 1;

        if (sel) {
            Mx_Box(MM_BAR_SEL, 51.0f, y0 - 4.0f, 489.0f, y0 + 99.0f, 34.0f, 0x80);
        } else {
            Mx_Box(MM_BAR, 55.0f, y0, 485.0f, y0 + 95.0f, 32.0f, 0x80);
        }
        if (r >= 0) {
            Mx_FaceCell(r, 70.0f, y0 + 12.0f, 140.0f, y0 + 82.0f, 0);
        } else {
            Mx_Fit(MM_LOCK, 105.0f, y0 + 47.0f, 50.0f, 0x60);
        }
        Mx_Text(152.0f, y0 + 26.0f, slotF[k], 24.0f, 0.0f, MX_GOLD, 0);
        Mx_TextA(162.0f + Mx_TextW(slotF[k], 24.0f), y0 + 26.0f, Trans_Name(r), 24.0f, 310.0f - Mx_TextW(slotF[k], 24.0f), MX_WHITE, 0, r < 0 ? 0x60 : 0x80);
        if (r >= 0) {
            Mx_Text(152.0f, y0 + 70.0f, sTrHold[k], 22.0f, 210.0f, MX_GOLD, 0);
            Mx_Pips(370.0f, y0 + 70.0f, sMm->tr.fuseCost[k], 0, 18.0f);
        }
    }
    Mx_Box(sMm->trCur == 4 ? MM_BAR_SEL : MM_WINDOW_DARK, 55.0f, 552.0f, 485.0f, 628.0f, 30.0f, 0x80);
    Mx_Text(270.0f, 590.0f, "BACK TO THE ORIGINAL FUSIONS", 23.0f, 390.0f, sMm->trCur == 4 ? MX_ORANGE : MX_LAV, 1);
    /* the middle: the two who fuse, what they become */
    if (slot && ref >= 0) {
        s32 i;

        for (i = 0; i < 4 && sMm->tr.partner[sMm->trCur - 1][i] < 0; i++) {
        }
        Mx_Fit(MM_BALL, 690.0f, 222.0f, 40.0f + 6.0f * Mm_Beat(24), 0x80); /* (between the two who fuse) */
        (void)i;
    }
    if (warn[0] != 0) {
        Mx_Text(60.0f, 640.0f, warn, 21.0f, 840.0f, MX_RED, 0);
    }
    if (slot) {
        Mx_Text(60.0f, 670.0f, Trans_Summary(), 24.0f, 840.0f, MX_GOLD, 0);
    }
    Mx_Window(912.0f, 145.0f, 1345.0f, 680.0f);
    if (slot) {
        Trans_Fields(912.0f, 145.0f, 1345.0f, 680.0f, 0);
        Trans_DrawChoices(902.0f);
    } else {
        Mx_Text(1128.0f, 220.0f, "FUSIONS", 28.0f, 0.0f, MX_GOLD, 1);
        Mx_Text(1128.0f, 262.0f, sMm->tr.fuseSet ? "SET HERE" : "AS ITS FILE HAS THEM", 24.0f, 380.0f, MX_WHITE, 1);
        Mx_Say(1128.0f, 340.0f, "Each fuses with a partner who", 22.0f, 380.0f, MX_WHITE, 1);
        Mx_Say(1128.0f, 370.0f, "is on its team, holding R3.", 22.0f, 380.0f, MX_WHITE, 1);
        Mx_Say(1128.0f, 440.0f, "Cross on a fusion: edit it", 22.0f, 380.0f, MX_GOLD, 1);
    }
}

/* ------------------------------------------------------------------------------------------------- removed mods
 * The folders of <data>/mods_removed (where circle on the Library puts a mod): cross puts one back into the library,
 * switched off. Nothing is ever deleted here.
 */
void Mm2_OpenRemoved(void) {
    sMm->goneCount = Port_RemovedList();
    sMm->goneCur = 0;
    sMm->goneTop = 0;
    Mm_SetMode(MM_REMOVED);
}

static void Removed_Update(s32 live) {
    Pad *pad = &gPad[0];

    sMm->wantKey = -1;
    if (sMm->goneCount > 0) {
        Mm_Prompt(0, "PUT BACK");
    }
    Mm_Prompt(2, "BACK");
    if (!live) {
        return;
    }
    if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        Snd_PlaySe(1, 2);
        Mm_SetMode(MM_LIBRARY);
        return;
    }
    List_Move(&sMm->goneCur, &sMm->goneTop, sMm->goneCount, RM_ROWS);
    if (sMm->goneCount > 0 && (pad->gamePressed & PADG_CROSS)) {
        static char name[64];
        static char note[80];
        s32 at;

        Port_RemovedName(sMm->goneCur, name);
        at = Port_RemovedRestore(sMm->goneCur);
        sMm->goneCount = Port_RemovedList();
        if (sMm->goneCur >= sMm->goneCount) {
            sMm->goneCur = sMm->goneCount > 0 ? sMm->goneCount - 1 : 0;
        }
        Mm_FreeFaces();
        Mm_Refresh();
        if (at >= 0) {
            sprintf(note, "%.40s IS BACK (OFF)", name);
            Mm_Note(note);
            Mm_Select(at);
            Snd_PlaySe(1, 1);
        } else {
            Mm_Note("COULD NOT MOVE IT BACK");
            Snd_PlaySe(1, 2);
        }
    }
}

#define RM_ROWS 6

/* (mockup 07) the removed mods as bars, the one under the cursor at the right */
static void Removed_Draw(void) {
    static char name[64];
    static char line[32];
    s32 row;

    Mx_Box(MM_FIELD, 150.0f, 132.0f, 740.0f, 178.0f, 23.0f, 0x80);
    Mx_Text(175.0f, 155.0f, "REMOVED MODS", 28.0f, 0.0f, MX_GOLD, 0);
    sprintf(line, "%d", (int)sMm->goneCount);
    Mx_Text(715.0f, 155.0f, line, 28.0f, 0.0f, MX_GOLD, 2);
    for (row = 0; row < RM_ROWS && sMm->goneTop + row < sMm->goneCount; row++) {
        f32 y0 = 200.0f + 80.0f * (f32)row;
        s32 sel = sMm->goneTop + row == sMm->goneCur;

        Port_RemovedName(sMm->goneTop + row, name);
        Mx_Row(125.0f, y0, 678.0f, y0 + 66.0f, sel, 0x80);
        Mx_Fit(MM_FOLDER, 182.0f, y0 + 33.0f, 42.0f, sel ? 0x80 : 0x60);
        Mx_Text(240.0f, y0 + 33.0f, name, 32.0f, 410.0f, MX_LAV, 0);
    }
    if (sMm->goneTop > 0) {
        Mx_Fit(MM_UP, 400.0f, 190.0f, 22.0f, 0x80);
    }
    if (sMm->goneTop + RM_ROWS < sMm->goneCount) {
        Mx_Fit(MM_DOWN, 400.0f, 682.0f, 22.0f, 0x80);
    }
    Mx_Window(765.0f, 130.0f, 1340.0f, 686.0f);
    if (sMm->goneCount == 0) {
        Mx_Text(1052.0f, 300.0f, "NOTHING REMOVED", 32.0f, 0.0f, MX_GOLD, 1);
        Mx_Say(1052.0f, 350.0f, "Circle on the Library moves a mod here.", 24.0f, 520.0f, MX_WHITE, 1);
    } else {
        Port_RemovedName(sMm->goneCur, name);
        Mx_Box(MM_WINDOW_DARK, 788.0f, 146.0f, 1320.0f, 316.0f, 22.0f, 0x80);
        Mx_Text(1054.0f, 231.0f, name, 36.0f, 480.0f, MX_WHITE, 1);
        Mx_SayC(812.0f, 358.0f, "Cross puts it back in the library,", 27.0f, 500.0f, Mx_Rgba(0xF0, 0xE0, 0xFF, 0x80), Mx_Rgba(0xD8, 0xC0, 0xF8, 0x80), Mx_Rgba(0x24, 0x10, 0x48, 0x80), 0,
                 0x80);
        Mx_SayC(812.0f, 400.0f, "switched off.", 27.0f, 500.0f, Mx_Rgba(0xF0, 0xE0, 0xFF, 0x80), Mx_Rgba(0xD8, 0xC0, 0xF8, 0x80), Mx_Rgba(0x24, 0x10, 0x48, 0x80), 0, 0x80);
        Mx_Box(MM_BAR_SEL, 870.0f, 524.0f, 1242.0f, 582.0f, 26.0f, 0x80);
        Mx_TextC(1056.0f, 553.0f, "PUT BACK (OFF)", 28.0f, 0.0f, Mx_Rgba(0xFF, 0xF6, 0xD8, 0x80), Mx_Rgba(0xFF, 0xE0, 0x90, 0x80), Mx_Rgba(0x5C, 0x30, 0x08, 0x80), 1, 0x80);
    }
    Mx_Rect(773.0f, 604.0f, 1332.0f, 678.0f, Mx_Rgba(0x06, 0x1A, 0x24, 0x70), Mx_Rgba(0x06, 0x14, 0x1C, 0x78));
    Mx_Say(1052.0f, 628.0f, "They are kept in the mods_removed", 23.0f, 520.0f, MX_WHITE, 1);
    Mx_Say(1052.0f, 657.0f, "folder. Nothing is deleted.", 23.0f, 520.0f, MX_WHITE, 1);
}

/* ------------------------------------------------------------------------------------------------- the three hooks */
void Mm2_Update(s32 live) {
    live = live && sMm->modeT >= 4;
    switch (sMm->mode) {
    case MM_ROSTER:
        Roster_Update(live);
        break;
    case MM_TRANSFORM:
        Trans_Update(live);
        break;
    case MM_BROWSER:
        Browser_Update(live);
        break;
    case MM_DETAIL:
        Detail_Update(live);
        break;
    case MM_PICKER:
        Picker_Update(live);
        break;
    case MM_KEYBOARD:
        Keyboard_Update(live);
        break;
    case MM_CROP:
        Crop_Update(live);
        break;
    case MM_REMOVED:
        Removed_Update(live);
        break;
    case MM_ART:
        Art_Update(live);
        break;
    }
}

void Mm2_Draw(void) {
    switch (sMm->mode) {
    case MM_ROSTER:
        Roster_Draw();
        break;
    case MM_TRANSFORM:
        Trans_Draw();
        break;
    case MM_BROWSER:
        Browser_Draw();
        break;
    case MM_DETAIL:
        Detail_Draw();
        break;
    case MM_PICKER:
        Picker_Draw();
        break;
    case MM_KEYBOARD:
        Keyboard_Draw();
        break;
    case MM_CROP:
        Crop_Draw();
        break;
    case MM_REMOVED:
        Removed_Draw();
        break;
    case MM_ART:
        Art_Draw();
        break;
    }
}

/* ============================================================================================ outside the manager */

/* The select screens call this when their pack is unpacked and its chips are ready: the mods that replace a fighter
   put their names (when they have any) and their face (when they bring a portrait or have a face set) into the
   pack's own tables, so every grid and name plate of that screen shows them. names / forms: the two name tables
   (a line per character id, 16-bit letters), chips: the pack of face pictures (64 x 64, 256 colours each). */
void PortMods_PatchSelect(void *names, void *forms, u32 *chips) {
    static u8 buf[0x1600];
    static PortModInfo m;
    s32 count = Port_ModCount();
    s32 i;
    s32 k;
    extern int Port_NetSession(void); /* port/src/gs/net.c */

    if (Port_NetSession()) {
        return; /* online: the game as it is on the disc, names and faces too */
    }
    for (i = 0; i < count; i++) {
        Port_ModGetInfo(i, &m);
        if (!m.on || m.isNew || m.chara < 0 || m.chara >= MM_CHARA_COUNT) {
            continue;
        }
        for (k = 0; k < 2; k++) {
            char *src = k == 0 ? m.white : m.yellow;
            void *table = k == 0 ? names : forms;

            if (src[0] != 0 && table != NULL) {
                u16 *dst = (u16 *)MPACK_AT(table, m.chara + 1);
                s32 n;

                if (*dst == 0xFEFF) {
                    dst++;
                }
                for (n = 0; n < 30 && src[n] != 0; n++) {
                    dst[n] = (u8)src[n];
                }
                dst[n] = 0;
            }
        }
        if (chips != NULL && ((m.kinds & 0x400) || m.chip[2] > 0) && Port_ModChip(i, buf, 0x1600) > 0) {
            MmRes *res = (MmRes *)MPACK_AT(chips, m.chara + 1);
            MmTex *t = res->tex;

            if (t != NULL && t->pixSize == 0x1080 && t->clutSize == 0x480 && t->pix != NULL && t->clut != NULL) {
                memcpy(t->pix + 0x60, buf + 0x60 + 0x60, 0x1000);
                memcpy(t->clut + 0x60, buf + 0x60 + 0x1080 + 0x60, 0x400);
            }
        }
    }
}

/* The name of a fighter picked from the mod grid, for a select screen's name plate: `code` is the costume number that
   carries the skin, which 0 the white name / 1 the yellow one, side the plate's. `table` is the screen's own name
   table (its lines say whether a string starts with a byte-order mark). NULL: the mod has no such name, the
   template's stands. */
u16 *PortMods_SkinName(s32 code, s32 which, s32 side, void *table) {
    static u16 line[2][2][34];
    static PortModInfo m;
    u16 *dst = line[side & 1][which & 1];
    char *src;
    s32 n;

    if (code < 8 || (code - 8) / 4 >= Port_SkinCount()) {
        return NULL;
    }
    Port_ModGetInfo(Port_SkinMod((code - 8) / 4), &m);
    src = which == 0 ? m.white : m.yellow;
    if (src[0] == 0) {
        return NULL;
    }
    if (table != NULL && *(u16 *)MPACK_AT(table, 1) == 0xFEFF) {
        *dst++ = 0xFEFF;
    }
    for (n = 0; n < 30 && src[n] != 0; n++) {
        dst[n] = (u8)src[n];
    }
    dst[n] = 0;
    return line[side & 1][which & 1];
}

/* ---- the mod grid of the character select ----
 * Opened with square: the new fighters as a grid of faces in another colour, the one under the cursor large at the
 * right. Cross picks, L1 / R1 change the costume, triangle or square go back. Returns 1 with the template's
 * character id and the costume code that carries the skin, 0 when nothing was picked.
 */
#define MG_COLS 5
#define MG_ROWS 4
#define MG_MAX 100
typedef struct ModGrid {
    s32 count;
    s32 cur;
    s32 top;        /* the first row shown (the tray shows five; more scroll) */
    s32 costume;
    s32 frames;
    s32 rest;
    s32 shown;      /* the skin * 4 + costume whose portrait is loaded, -1 none */
    s32 artT;       /* frames since that portrait came */
    s32 pickT;      /* frames since the pick (the screen flashes and leaves), 0 = not yet */
    s32 outT;       /* frames since the fighter at the right started to go (it slides off), -1 not going */
    s32 inT;        /* frames since the one shown came (it slides in, an aura flares behind it), -1 none yet */
    s32 lastKey;    /* the model drawn last frame (Mm_ViewKey) */
    f32 curX;       /* where the cursor's frame is drawn: it glides */
    f32 curY;
    PortModInfo info[MG_MAX];
    MmRes *chip[MG_MAX];
    u8 *artFile;
    u8 *artRes;
    MmTex *artTex;
    MmRes *ui;      /* the manager's pictures (its letters, its pieces, this page) */
} ModGrid;

static s32 Mg_HasCostume(PortModInfo *m, s32 n) {
    return (m->kinds & (1 << n)) != 0 || (n == 0);
}

/* 0..1..0 over `period` frames. */
static f32 Mg_Beat(ModGrid *g, s32 period) {
    s32 k = g->frames % period;

    return (f32)(k * 2 < period ? k * 2 : (period - k) * 2) / (f32)period;
}

/* (mockup 16) the sky backdrop, the new fighters' faces in the tray, the one under the cursor large at the right with
   its name and its costumes, the buttons at the foot */
#define MG_X 103.0f
#define MG_Y 168.0f
#define MG_PX 137.0f
#define MG_PY 115.0f
#define MG_W 118.0f
#define MG_H 103.0f
#define MG_MODEL_X 1100.0f   /* the model, close: from its waist up above the name plate, its legs below the screen */
#define MG_MODEL_FEET 975.0f
#define MG_MODEL_H 1050.0f


#define MG_OUT_T 6  /* frames the one shown takes to slide off */
#define MG_IN_T 12  /* and the next one to slide in */

/* The change of fighter, as the select screens bring their portraits in: the one shown slides off to the right, the
   next slides in from there and an aura flares up behind it as it lands. Where across the model is (mockup pixels
   from its place), and whether it is drawn. */
static s32 Mg_Slide(ModGrid *g, f32 *dx) {
    *dx = 0.0f;
    if (g->outT >= 0) {
        f32 f = (f32)g->outT / (f32)MG_OUT_T;

        if (f >= 1.0f) {
            return 0;
        }
        *dx = f * f * 520.0f;
    } else if (g->inT >= 0 && g->inT < MG_IN_T) {
        *dx = (1.0f - Mm_Ease((f32)g->inT / (f32)MG_IN_T)) * 420.0f;
    }
    return 1;
}

static void Mg_Draw(ModGrid *g, f32 in, f32 art) {
    PortModInfo *m = &g->info[g->cur];
    f32 drop = (1.0f - in) * -160.0f;
    s32 i;
    s32 k;

    Mx_Background(0); /* (the Mod Manager's sky) */
    for (i = 0; i < 14; i++) { /* sparks rise, each at its own pace */
        s32 life = 150 + i * 23;
        s32 t = (g->frames + i * 61) % life;
        f32 f = (f32)t / (f32)life;
        f32 x = 860.0f + (f32)((i * 97 + 31) % 300) + 20.0f * f * (f32)((i & 1) ? 1 : -1);
        f32 y = 640.0f - f * 600.0f;
        s32 a = (s32)((f < 0.2f ? f * 5.0f : f > 0.8f ? (1.0f - f) * 5.0f : 1.0f) * 90.0f * in);
        f32 r = 3.0f + (f32)(i % 3);

        Mx_Rect(x - r, y - r, x + r, y + r, Mx_Rgba(0xFF, 0xE8, 0xFF, a), Mx_Rgba(0xFF, 0xC0, 0xF0, a));
    }
    /* the fighter in 3D, close, from the waist up, still (the character select shows its portrait once it is picked);
       the aura of its coming behind it */
    if (g->count > 0) {
        f32 dx;

        if (g->outT < 0 && g->inT >= MG_IN_T / 3 && g->inT < MG_IN_T / 3 + 36) {
            f32 k = 1.0f - (f32)(g->inT - MG_IN_T / 3) / 36.0f;

            Mx_Glow(MG_MODEL_X, 600.0f, 560.0f * (1.15f - 0.15f * k), Mx_Rgba(0xB8, 0xE4, 0xFF, (s32)(k * k * 0x78)), g->frames);
        }
        if (Mg_Slide(g, &dx)) {
            Mm_ViewDraw(MG_MODEL_X + dx, MG_MODEL_FEET, MG_MODEL_H);
        }
    }
    Mx_Fit(MM_BALL, 100.0f, 72.0f + drop, 64.0f, 0x80);
    Mx_Heading(146.0f, 72.0f + drop, "MOD FIGHTERS", 50.0f, 500.0f, 0);
    Mx_Rect(110.0f, 110.0f + drop, 640.0f, 113.0f + drop, Mx_Rgba(0xF0, 0xC8, 0xFF, 0x70), Mx_Rgba(0xB0, 0x88, 0xE0, 0x70));
    Mx_BoxTint(MM_WINDOW, 55.0f, 133.0f, 835.0f, 655.0f, 34.0f, Mx_Rgba(0x50, 0x58, 0x90, 0x80), 0x78);
    for (i = 0; i < MG_COLS * MG_ROWS; i++) {
        s32 at = g->top * MG_COLS + i;
        f32 x0 = MG_X + MG_PX * (f32)(i % MG_COLS);
        f32 y0 = MG_Y + MG_PY * (f32)(i / MG_COLS);
        f32 pop = Mm_Ease((f32)(g->frames - 2 - i) / 8.0f);

        Mx_Box(MM_CELL, x0, y0, x0 + MG_W, y0 + MG_H, 14.0f, 0x80);
        if (at < g->count && g->chip[at] != NULL && pop > 0.0f) {
            Mx_Tex(g->chip[at]->tex, x0 + 7.0f, y0 + 7.0f, x0 + MG_W - 7.0f, y0 + MG_H - 7.0f, 0, 0, 64, 64, at == g->cur ? 0x80 : 0x6C, (s32)(pop * 128.0f));
        }
        if (at < g->count && Port_ModIsWarrior(Port_SkinMod(at)) && pop > 0.0f) { /* a warrior of Z Warriors: a halo over it */
            Mx_Fit(ZW_HALO, x0 + MG_W * 0.5f, y0 + 4.0f + 2.0f * Mg_Beat(g, 48), 26.0f, (s32)(pop * 128.0f));
        }
    }
    if (g->count > MG_COLS * MG_ROWS && (g->top + MG_ROWS) * MG_COLS < g->count) {
        Mx_Fit(MM_DOWN, 445.0f, 636.0f + 3.0f * Mg_Beat(g, 36), 22.0f, 0x80);
    }
    if (g->top > 0) {
        Mx_Fit(MM_UP, 445.0f, 150.0f - 3.0f * Mg_Beat(g, 36), 22.0f, 0x80);
    }
    if (g->count > 0 && in > 0.6f) { /* the cursor: gold corners that breathe, 1P, arrows to the rows */
        f32 b = 3.0f * Mg_Beat(g, 44) + (g->pickT > 0 ? 6.0f : 0.0f);
        f32 x0 = g->curX - MG_W * 0.5f;
        f32 y0 = g->curY - MG_H * 0.5f;

        Mx_Frame(x0 - 2.0f, y0 - 2.0f, x0 + MG_W + 2.0f, y0 + MG_H + 2.0f, 3.0f, Mx_Rgba(0xFF, 0xE0, 0x50, 0x80));
        Mx_Brackets(x0 - 12.0f - b, y0 - 12.0f - b, x0 + MG_W + 12.0f + b, y0 + MG_H + 12.0f + b, 26.0f, 6.0f, Mx_Rgba(0xFF, 0xD8, 0x30, 0x80));
        Mx_TextC(x0 + 8.0f, y0 + 14.0f, "1P", 28.0f, 0.0f, Mx_Rgba(0xFF, 0x70, 0x40, 0x80), Mx_Rgba(0xD0, 0x20, 0x10, 0x80), Mx_Rgba(0xFF, 0xE8, 0x80, 0x80), 0, 0x80);
        if (g->cur >= MG_COLS) {
            Mx_Fit(MM_UP, g->curX, y0 - 22.0f - b, 20.0f, 0x80);
        }
        if (g->cur / MG_COLS < (g->count - 1) / MG_COLS) {
            Mx_Fit(MM_DOWN, g->curX, y0 + MG_H + 22.0f + b, 20.0f, 0x80);
        }
    }
    if (g->count > 0) { /* the name plate over the portrait's foot, the costumes under it */
        static char like[80];
        f32 ny = (1.0f - in) * 160.0f;

        Mx_Box(MM_WINDOW, 856.0f, 535.0f + ny, 1340.0f, 640.0f + ny, 40.0f, 0x80);
        Mx_Text(1098.0f, 568.0f + ny, Either(m->white[0] != 0, m->white, m->name), 40.0f, 440.0f, MX_WHITE, 1);
        if (m->yellow[0] != 0) {
            Mx_Text(1098.0f, 611.0f + ny, m->yellow, 28.0f, 440.0f, MX_GOLD, 1);
        } else {
            sprintf(like, "Like %.60s", m->target);
            Mx_Text(1098.0f, 611.0f + ny, like, 26.0f, 440.0f, MX_GOLD, 1);
        }
        Mx_Text(1108.0f, 660.0f + ny, "COSTUMES", 20.0f, 0.0f, MX_WHITE, 1);
        for (k = 0; k < 4; k++) {
            static char *num[4] = {"1", "2", "3", "4"};
            f32 cx = 975.0f + 89.0f * (f32)k;
            f32 cy = 689.0f + ny;

            if (k == g->costume) {
                Mx_Bar(MM_TAB_SEL, cx - 38.0f, cy - 15.0f, cx + 38.0f, cy + 15.0f, 0x80);
                Mx_TextC(cx, cy, num[k], 22.0f, 0.0f, Mx_Rgba(0xFF, 0xFF, 0xF0, 0x80), Mx_Rgba(0xFF, 0xF0, 0xC8, 0x80), Mx_Rgba(0x6C, 0x3C, 0x08, 0x80), 1, 0x80);
            } else {
                Mx_Bar(MM_NAVBAR, cx - 38.0f, cy - 14.0f, cx + 38.0f, cy + 14.0f, Mg_HasCostume(m, k) ? 0x80 : 0x40);
                Mx_TextA(cx, cy, num[k], 22.0f, 0.0f, MX_WHITE, 1, Mg_HasCostume(m, k) ? 0x80 : 0x40);
            }
        }
    } else {
        Mx_Text(445.0f, 360.0f, "NO NEW FIGHTERS YET", 34.0f, 0.0f, MX_GOLD, 1);
        Mx_Say(445.0f, 410.0f, "Make one in the Mod Manager", 26.0f, 0.0f, MX_WHITE, 1);
    }
    Mm_PromptReset();
    if (g->count > 0) {
        Mm_Prompt(0, "PICK");
        Mm_Prompt(14, "COSTUME");
    }
    Mm_Prompt(2, "BACK");
    Mx_Footer(52.0f, 602.0f, 690.0f, 750.0f);
    if (g->pickT > 0) { /* the pick: a flash of light */
        s32 a = g->pickT < 5 ? g->pickT * 22 : (14 - g->pickT) * 12;

        Ps_ScreenQuad(0.0f, 0.0f, 512.0f, 0.0f, 0.0f, 448.0f, 512.0f, 448.0f, Mx_Rgba(0xFF, 0xF0, 0xFF, a < 0 ? 0 : a), Mx_Rgba(0xFF, 0xF0, 0xFF, a < 0 ? 0 : a),
                      Mx_Rgba(0xFF, 0xC0, 0xFF, a < 0 ? 0 : a), Mx_Rgba(0xFF, 0xC0, 0xFF, a < 0 ? 0 : a));
    }
}

s32 gPortModGridStart = -1; /* the skin the grid opens on (Z Warriors' preview), -1 the first */

s32 PortModGrid_Pick(s32 padNo, s32 *chara, s32 *code) {
    ModGrid *g;
    Pad *pad = &gPad[padNo];
    s32 result = 0;
    s32 done = 0;
    s32 i;

    g = Heap_Alloc(sizeof(ModGrid), 0x20, 0, 2);
    memset(g, 0, sizeof(ModGrid));
    g->count = Port_SkinCount();
    if (g->count > MG_MAX) {
        g->count = MG_MAX;
    }
    g->shown = -1;
    g->outT = -1;
    g->inT = -1;
    g->lastKey = -1;
    if (gPortModGridStart >= 0 && gPortModGridStart < g->count) {
        g->cur = gPortModGridStart;
        g->top = g->cur / MG_COLS - MG_ROWS + 1 > 0 ? g->cur / MG_COLS - MG_ROWS + 1 : 0;
    }
    gPortModGridStart = -1;
    g->curX = MG_X + MG_PX * (f32)(g->cur % MG_COLS) + MG_W * 0.5f;
    g->curY = MG_Y + MG_PY * (f32)(g->cur / MG_COLS - g->top) + MG_H * 0.5f;
    g->artFile = Heap_Alloc(ART_FILE_SIZE, 0x40, 0, 2);
    g->artRes = Heap_Alloc(ART_RES_SIZE, 0x40, 0, 2);
    for (i = 0; i < g->count; i++) {
        MmRes *res = Heap_Alloc(0x1600, 0x40, 0, 2);

        Port_ModGetInfo(Port_SkinMod(i), &g->info[i]);
        if (res != NULL && Port_ModChip(Port_SkinMod(i), res, 0x1600) > 0) {
            Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
            g->chip[i] = res;
        } else if (res != NULL) {
            Heap_Free(res);
        }
    }
    Snd_PlaySe(1, 1);
    Port_EscQuit(0); /* Esc goes back to the roster, it does not close the game */
    g->ui = Mm_UiOpen(); /* the letters and the pieces */
    Mm_ViewOpen();
    while (!done) {
        PortModInfo *m = &g->info[g->cur];
        s32 was = g->cur;
        f32 in;
        f32 art;
        f32 tx;
        f32 ty;
        s32 key;
        s32 k;

        Gfx_BeginFrame();
        Mm_TexReset();
        Pad_Update();
        if (Port_EscTake()) {
            pad->gamePressed |= PADG_TRIANGLE;
        }
        Snd_Update();
        in = Mm_Ease((f32)g->frames / 14.0f);
        /* the model of the one under the cursor, once it has rested */
        key = g->cur * 4 + g->costume;
        if (g->count > 0) {
            Mm_ViewWant(key, g->info[g->cur].chara >= 0 ? g->info[g->cur].chara : 0, g->costume, Port_SkinMod(g->cur), g->rest >= 6);
        }
        if (0 && g->rest >= 6 && g->shown != key && g->count > 0) { /* (its portrait: not read here any more) */
            MmRes *res;

            g->shown = key;
            g->artTex = NULL;
            g->artT = 0;
            if (Port_ModPortrait(Port_SkinMod(g->cur), g->artFile, ART_FILE_SIZE) >= 0x100 && *(u32 *)g->artFile <= ART_RES_SIZE) {
                Sprite_Unpack(g->artFile, g->artRes, NULL);
                res = (MmRes *)g->artRes;
                Res_RelocateOffsets((void *)&res, (u8 *)res, (void *)res);
                g->artTex = res->tex;
            }
        }
        art = Mm_Ease((f32)g->artT / 10.0f);
        if (g->cur / MG_COLS < g->top) { /* the rows shown keep the cursor's */
            g->top = g->cur / MG_COLS;
        } else if (g->cur / MG_COLS >= g->top + MG_ROWS) {
            g->top = g->cur / MG_COLS - MG_ROWS + 1;
        }
        tx = MG_X + MG_PX * (f32)(g->cur % MG_COLS) + MG_W * 0.5f;
        ty = MG_Y + MG_PY * (f32)(g->cur / MG_COLS - g->top) + MG_H * 0.5f;
        g->curX += (tx - g->curX) * 0.45f;
        g->curY += (ty - g->curY) * 0.45f;

        if (Mm_ViewKey() != g->lastKey) { /* (the next one is in: it slides in) */
            if (Mm_ViewKey() >= 0) {
                g->inT = 0;
                g->outT = -1;
            }
            g->lastKey = Mm_ViewKey();
        }
        if (g->outT >= 0 && g->rest >= 6 && Mm_ViewKey() >= 0 && Mm_ViewKey() == key) { /* (back on the one shown before
                                                                                            another came: it slides back) */
            g->inT = 0;
            g->outT = -1;
        }
        Mx_Begin();
        Mg_Draw(g, in, art);
        if (g->outT >= 0 && g->outT < 100) {
            g->outT++;
        }
        if (g->inT >= 0 && g->inT < 100) {
            g->inT++;
        }
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        g->frames++;
        if (g->rest < 1000) {
            g->rest++;
        }
        if (g->artT < 100) {
            g->artT++;
        }
        if (g->pickT > 0) { /* picked: the flash, then out */
            if (++g->pickT > 14) {
                done = 1;
            }
            continue;
        }
        if (g->frames < 10) {
            continue;
        }
        if (g->count == 0) {
            if (g->frames > 70 || (pad->gamePressed & (PADG_TRIANGLE | PADG_SQUARE | PADG_CROSS))) {
                done = 1;
            }
            continue;
        }
        if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
            g->cur = (g->cur + 1) % g->count;
        } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
            g->cur = (g->cur + g->count - 1) % g->count;
        } else if ((pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) && g->cur / MG_COLS < (g->count - 1) / MG_COLS) {
            g->cur = g->cur + MG_COLS < g->count ? g->cur + MG_COLS : g->count - 1; /* (to the last of a shorter row) */
        } else if ((pad->gameRepeat & (PADG_UP | PADG_UP2)) && g->cur >= MG_COLS) {
            g->cur -= MG_COLS;
        }
        if (g->cur != was) {
            g->costume = 0;
            g->rest = 0;
            g->outT = g->lastKey >= 0 ? 0 : -1; /* (the one shown slides off) */
            Snd_PlaySe(2, 0);
        }
        if (pad->gamePressed & (PADG_R1 | PADG_L1)) {
            s32 step = (pad->gamePressed & PADG_R1) ? 1 : 3;
            s32 n = g->costume;

            for (k = 0; k < 4; k++) {
                n = (n + step) % 4;
                if (Mg_HasCostume(&g->info[g->cur], n)) {
                    break;
                }
            }
            if (n != g->costume) {
                g->costume = n;
                g->outT = g->lastKey >= 0 ? 0 : -1;
                Snd_PlaySe(1, 0);
            }
        }
        if (pad->gamePressed & PADG_CROSS) {
            *chara = g->info[g->cur].chara;
            *code = PORT_SKIN_CODE(g->cur, g->costume);
            result = 1;
            g->pickT = 1;
            Snd_PlaySe(1, 1);
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_SQUARE)) {
            done = 1;
            Snd_PlaySe(1, 2);
        }
    }
    for (i = 0; i < g->count; i++) {
        if (g->chip[i] != NULL) {
            Heap_Free(g->chip[i]);
        }
    }
    sceGsSyncPath(0, 0);
    Mm_ViewClose();
    Port_EscQuit(1);
    Mm_UiClose();
    Heap_Free(g->artFile);
    Heap_Free(g->artRes);
    Heap_Free(g);
    return result;
}
#endif
