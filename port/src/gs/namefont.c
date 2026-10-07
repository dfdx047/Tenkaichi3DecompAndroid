/* The names of added stages and songs in the game's own lettering.

   The game has no font for the stage select's names: every name of the disc is a picture (sheets of 512x256 with four
   stage names of 512x64, or eight song names of 512x32). A stage or a song added from outside the disc has no such
   picture, so its name is put together here from the letters of the disc's names: when the stage select has loaded
   its resources the menu hands over its name sheets (Port_NameFontSheets), they are decoded, and each name picture is
   cut into its letters. Nothing of this is stored anywhere: the letters exist in memory only, made from the player's
   own game data while the program runs. What this file carries of the disc is what the pictures SAY (kStageNames,
   kSongNames), which is what tells the letters apart.

   The pictures were drawn the way a paint program styles a text layer, and that is how a name is put together again:
     - the letters' fill (a top-to-bottom colour ramp) is what is cut out and kept, pixel for pixel;
     - the outline is one colour, 3 pixels around the fill; neighbouring letters' outlines run into each other, so it
       is not cut out but drawn again around the composed fills (colour measured from the sheets);
     - the shadow is the fill moved 4 right and 3 down and blurred, under everything.
   The spacing comes from the disc's names too: the gaps between each pair of neighbouring letters give every letter a
   left and a right bearing (least squares), the gaps at the blanks give the width of a blank.

   Only reads the game's memory; the game's state is not touched. Runs on the game's thread for the copy of the sheets
   (once), and on the render thread for the rest (ui.cpp asks for a name's picture when it first shows it). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "namefont.h"

#define SHEET_W 512
#define SHEET_H 256
#define MAX_SHEETS 9
#define OUTLINE_R 3.0f   /* the outline's width, pixels */
#define SHADOW_DX 4      /* the shadow: the fill moved by this ... */
#define SHADOW_DY 3
#define SHADOW_SIGMA 1.25f /* ... and blurred by this (Gaussian) */
#define MAX_INST 1024

/* What the disc's name pictures say, in the order of the sheets (stage id; music-list value). Read off the decoded
   sheets one by one; a wrong entry shows as a letter count that does not match its picture, and then the whole
   style is left to the plain text (see style_build). */
static const char *const kStageNames[] = {
    "Wasteland - Noon", "Rocky Area - Noon", "Planet Namek", "Dying Namek",
    "World Tournament Stage - Noon", "Kami's Lookout", "Cell Games Arena - Evening", "Supreme Kai's World",
    "Hyperbolic Time Chamber", "City Ruins - Noon", "Mountain Road - Noon", "Islands",
    "Kame House", "Planet - Night", "Glacier", "Ruined Earth",
    "Outer Space", "Penguin Village", "Hell", "Desert - Noon",
    "King's Castle", "Muscle Tower", "Mount Paozu", "Wasteland - Evening",
    "Wasteland - Night", "Rocky Area - Evening", "Rocky Area - Night", "World Tournament Stage - Evening",
    "Cell Game - Noon", "City Ruins - Evening", "City Ruins - Night", "Desert - Evening",
    "Desert - Night", "Mountain Road - Evening", "Planet - Evening", "Random",
};
static const char *const kSongNames[] = {
    "The Meteor", "Vital Burner", "Innocent World", "After The Fire", "Sweet Vibration", "Survive", "Heat Capacity", "Overture",
    "Shine", "Power Scale", "Edge of Spirit", "Caution !", "Menace", "Hot Soul", "High and Scream", "Shootout In Meteor",
    "Dynamite Battle", "Burnin' Up", "Wild Rush", "Evolution", "********", "********", "********", "********",
};

typedef struct NfGlyph {
    uint8_t *px;    /* w * h * 4: red, green, blue, and how much of the pixel is fill (0 = not this letter's) */
    short w, h;
    short ox, oy;   /* the box's corner: from the fill's left edge, and from the top of the name's row */
    short sw;       /* the fill's width */
    short sy0, sy1; /* the fill's first and last row in the name's row */
    short lent;     /* 1: not cut from this style's names but made from the other style's letter (style_lend) */
    float lb, rb;   /* bearings: the gap to a neighbour is the left letter's rb + the right letter's lb */
} NfGlyph;

typedef struct NfStyle {
    int ok;
    int rowH;
    uint8_t outline[3];
    uint8_t ramp[64][3]; /* the fill's colour at each row of the name's row */
    NfGlyph g[128];
    float space;   /* width of a blank, between the two bearings */
    float margin;  /* songs: where the first letter's fill starts */
    float centre;  /* stages: the middle of the names */
    int names, used;      /* (for the test report) */
    int widthSpread;      /* the largest difference between two cuts of the same letter, pixels */
} NfStyle;

static uint8_t *sSheet[NF_STYLES][MAX_SHEETS]; /* straight copies of the sheets: RGBA, alpha as the PS2 has it (0x80 = opaque) */
static int sSheetCount[NF_STYLES];
static NfStyle sStyle[NF_STYLES];
static volatile int sState; /* 0 no sheets yet, 1 sheets copied, 2 being cut, 3 done (sStyle[].ok says what came of it) */

/* ---- the sheets ---------------------------------------------------------------------------------------------- */

/* GS memory order (the hardware's tables, as in gs_core.c): the sheets are 8-bit textures that the game uploads as
   32-bit images of half the size, which lands every byte where the 8-bit format expects it. */
static const uint8_t kBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const uint8_t kCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                     {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const uint8_t kCol8First[4][16] = {{0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
                                          {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};

static uint32_t rd32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

/* One texture of a texture file (TexEntry, include/sys/gfxm_b_b.h; 0x40 bytes, pointers already relocated) to RGBA.
   Only the one layout the name sheets have is taken: 512x256, 8 bits with a 256-colour table of 32-bit colours,
   pixels sent as a 256x128 image of 32 bits. NULL for anything else. */
static uint8_t *sheet_decode(const uint8_t *e) {
    static uint8_t mem[SHEET_W * SHEET_H];
    uint64_t tex0;
    uint32_t pixSize = rd32(e + 0x08), clutSize = rd32(e + 0x0C), pixBlt = rd32(e + 0x18), clutBlt = rd32(e + 0x1C);
    const uint8_t *pix = (const uint8_t *)(uintptr_t)rd32(e + 0x38);
    const uint8_t *clut = (const uint8_t *)(uintptr_t)rd32(e + 0x3C);
    uint8_t *out;
    int x, y;

    memcpy(&tex0, e + 0x30, 8);
    if (pix == NULL || clut == NULL || pixSize != 0x60 + SHEET_W * SHEET_H + 0x20 || clutSize != 0x60 + 0x400 + 0x20) {
        return NULL;
    }
    if (((tex0 >> 20) & 0x3F) != 0x13 || ((tex0 >> 14) & 0x3F) != 8 || ((tex0 >> 26) & 0xF) != 9 || ((tex0 >> 30) & 0xF) != 8 ||
        ((tex0 >> 51) & 0xF) != 0 || ((tex0 >> 55) & 1) != 0) { /* 8 bits, 512 wide in memory, 512x256, table of 32-bit colours in the swapped order */
        return NULL;
    }
    if ((pixBlt & 0x3F) != 4 || ((pixBlt >> 8) & 0x3F) != 0 || ((clutBlt >> 8) & 0x3F) != 0) { /* both sent as 32-bit images, the pixels 256 wide */
        return NULL;
    }
    /* each packet: a DMA tag, a tag with three registers (BITBLTBUF, TRXPOS, TRXREG... as the game patches them),
       the image's tag, the image. The size registers must say what the sizes above say. */
    if (rd32(pix + 0x30) != SHEET_W / 2 || rd32(pix + 0x34) != SHEET_H / 2 || pix[0x38] != 0x52 || (rd32(pix + 0x50) & 0x7FFF) != SHEET_W * SHEET_H / 16 ||
        rd32(clut + 0x30) != 16 || rd32(clut + 0x34) != 16 || clut[0x38] != 0x52 || (rd32(clut + 0x50) & 0x7FFF) != 0x40) {
        return NULL;
    }
    pix += 0x60;
    clut += 0x60;
    for (y = 0; y < SHEET_H / 2; y++) { /* the upload: 32-bit pixels, buffer 256 wide */
        for (x = 0; x < SHEET_W / 2; x++) {
            uint32_t a = (((uint32_t)(y & ~0x1F) * 4 + ((x >> 1) & ~0x1F) + kBlock32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + kCol32[y & 7][x & 7];
            memcpy(mem + a * 4, pix + (y * (SHEET_W / 2) + x) * 4, 4);
        }
    }
    out = (uint8_t *)malloc(SHEET_W * SHEET_H * 4);
    if (out == NULL) {
        return NULL;
    }
    for (y = 0; y < SHEET_H; y++) { /* read back as 8 bits, buffer 512 wide, through the colour table */
        for (x = 0; x < SHEET_W; x++) {
            int col = (y & 15) >> 2, sx = (col & 1) ? ((x & 15) ^ 4) : (x & 15);
            uint32_t a = (((uint32_t)((y >> 1) & ~0x1F) * 4 + ((x >> 2) & ~0x1F) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + kCol8First[y & 3][sx] + col * 64;
            uint32_t i = mem[a];
            i = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1); /* the table's own order: entries 8..15 and 16..23 of every 32 change places */
            memcpy(out + (y * SHEET_W + x) * 4, clut + i * 4, 4);
        }
    }
    return out;
}

static void test_run(void);

void Port_NameFontSheets(int stage, int song, int songLit) {
    static const int kNeed[NF_STYLES] = {9, 3, 3}; /* 36 names by four; 24 by eight */
    uint32_t addr[NF_STYLES];
    uint8_t *got[NF_STYLES][MAX_SHEETS];
    int s, i, bad = 0;

    if (sState != 0) { /* once: the sheets are the same every time the stage select is set up */
        return;
    }
    addr[NF_STAGE] = (uint32_t)stage;
    addr[NF_SONG] = (uint32_t)song;
    addr[NF_SONG_LIT] = (uint32_t)songLit;
    memset(got, 0, sizeof(got));
    for (s = 0; s < NF_STYLES && !bad; s++) {
        for (i = 0; i < kNeed[s] && addr[s] != 0; i++) {
            got[s][i] = sheet_decode((const uint8_t *)(uintptr_t)addr[s] + i * 0x40);
            if (got[s][i] == NULL) {
                break;
            }
        }
        if (i != kNeed[s] && s != NF_SONG_LIT) { /* (the second song style is an extra: without it the first is used) */
            bad = 1;
        }
    }
    if (bad) { /* not the sheets this was written for (another edition of the game?): the names stay plain text */
        for (s = 0; s < NF_STYLES; s++) {
            for (i = 0; i < MAX_SHEETS; i++) {
                free(got[s][i]);
            }
        }
        return;
    }
    for (s = 0; s < NF_STYLES; s++) {
        for (i = 0; i < MAX_SHEETS; i++) {
            sSheet[s][i] = got[s][i];
            if (got[s][i] != NULL) {
                sSheetCount[s] = i + 1;
            }
        }
    }
    __sync_synchronize();
    sState = 1;
    if (getenv("BT3_NAMEFONT_TEST") != NULL) { /* testing: cut the letters now and write the test pictures */
        test_run();
    }
}

/* ---- cutting the letters ------------------------------------------------------------------------------------- */

typedef struct Inst { /* one letter of one name picture */
    short name, ch;
    short sx0, sx1, sy0, sy1; /* the fill's box */
    short bx0, bx1, by0, by1; /* the box of every pixel that is this letter's (the fill and its soft edge) */
    short label;
} Inst;

static uint8_t *sInstPx[MAX_INST]; /* while a style is cut: each cut's pixels */

static int cmp_int(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

static float median_f(float *v, int n) {
    int i, j;
    if (n <= 0) {
        return 0.0f;
    }
    for (i = 1; i < n; i++) { /* (small lists) */
        float t = v[i];
        for (j = i; j > 0 && v[j - 1] > t; j--) {
            v[j] = v[j - 1];
        }
        v[j] = t;
    }
    return (n & 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) * 0.5f;
}

static const uint8_t *row_px(int s, int name, int rowH) {
    int per = SHEET_H / rowH;
    return sSheet[s][name / per] + (size_t)(name % per) * rowH * SHEET_W * 4;
}

/* Cuts the names of one style into letters. 0 = the pictures are not what the table says (nothing is kept). */
static int style_build(int s, const char *const *names, int count, int rowH) {
    NfStyle *st = &sStyle[s];
    int per = SHEET_H / rowH, cell = rowH * SHEET_W;
    uint8_t *t = NULL;     /* per name, per pixel: how much of it is fill, 0..255 */
    short *lab = NULL;     /* per pixel of the name at hand: which letter owns it (1..), 0 none */
    int *stack = NULL;
    Inst *inst = NULL;
    int nInst = 0, n, x, y, i, j, k, ok = 0;
    int full[64], maxFull = 0, y0v, y1v;
    float *gaps = NULL;
    int nPairs = 0, nSpaces = 0;
    struct Pair { short a, b, space; float gap; } *pairs = NULL;
    static uint32_t colour[4096];
    static int colourN[4096];
    int nColour = 0, best = 0, last = 0;

    memset(st, 0, sizeof(*st));
    st->rowH = rowH;
    st->names = count;
    if (rowH > 64 || sSheetCount[s] * per < count) {
        return 0;
    }
    /* the outline's colour: the one colour most opaque pixels have (the fill is a ramp of many) */
    for (n = 0; n < count; n++) {
        const uint8_t *p = row_px(s, n, rowH);
        for (i = 0; i < cell; i++, p += 4) {
            uint32_t c;
            if (p[3] < 120) {
                continue;
            }
            c = p[0] | p[1] << 8 | p[2] << 16;
            if (nColour > 0 && colour[last] == c) {
                colourN[last]++;
                continue;
            }
            for (j = 0; j < nColour && colour[j] != c; j++) {
            }
            if (j == nColour) {
                if (nColour == 4096) {
                    continue;
                }
                colour[nColour] = c;
                colourN[nColour++] = 0;
            }
            colourN[j]++;
            last = j;
        }
    }
    for (j = 0; j < nColour; j++) {
        if (colourN[j] > colourN[best]) {
            best = j;
        }
    }
    if (nColour == 0) {
        return 0;
    }
    st->outline[0] = (uint8_t)colour[best];
    st->outline[1] = (uint8_t)(colour[best] >> 8);
    st->outline[2] = (uint8_t)(colour[best] >> 16);

    t = (uint8_t *)calloc((size_t)count, (size_t)cell);
    lab = (short *)malloc(sizeof(short) * (size_t)cell);
    stack = (int *)malloc(sizeof(int) * (size_t)cell);
    inst = (Inst *)calloc(MAX_INST, sizeof(Inst));
    pairs = (struct Pair *)calloc(MAX_INST, sizeof(*pairs));
    gaps = (float *)calloc(MAX_INST, sizeof(float));
    if (t == NULL || lab == NULL || stack == NULL || inst == NULL || pairs == NULL || gaps == NULL) {
        goto done;
    }
    /* How far every opaque pixel is from the outline's colour towards the fill's (brighter; the shadow under the
       outline's soft edge is darker and counts as none). The fill's own colour changes down the row, so the measure
       is taken against the brightest pixel of each row of the cell over all names; the rows above and below the
       ones that reach the full fill (letters' soft top and bottom edges only) take the nearest full row's. */
    memset(full, 0, sizeof(full));
    for (n = 0; n < count; n++) {
        const uint8_t *p = row_px(s, n, rowH);
        uint8_t *d = t + (size_t)n * cell;
        for (i = 0; i < cell; i++, p += 4) {
            int v = 0;
            if (p[3] >= 100) {
                for (k = 0; k < 3; k++) {
                    if (p[k] - st->outline[k] > v) {
                        v = p[k] - st->outline[k];
                    }
                }
            }
            d[i] = (uint8_t)v;
            if (v > full[i / SHEET_W]) {
                full[i / SHEET_W] = v;
                memcpy(st->ramp[i / SHEET_W], p, 3);
            }
        }
    }
    for (y = 0; y < rowH; y++) {
        if (full[y] > maxFull) {
            maxFull = full[y];
        }
    }
    if (maxFull < 40) {
        goto done;
    }
    for (y0v = 0; full[y0v] * 10 < maxFull * 7; y0v++) {
    }
    for (y1v = rowH - 1; full[y1v] * 10 < maxFull * 7; y1v--) {
    }
    for (y = 0; y < rowH; y++) {
        if (y < y0v) {
            full[y] = full[y0v];
            memcpy(st->ramp[y], st->ramp[y0v], 3);
        } else if (y > y1v) {
            full[y] = full[y1v];
            memcpy(st->ramp[y], st->ramp[y1v], 3);
        }
    }
    for (n = 0; n < count; n++) {
        uint8_t *d = t + (size_t)n * cell;
        for (i = 0; i < cell; i++) {
            int v = d[i] * 255 / full[i / SHEET_W];
            d[i] = (uint8_t)(v > 255 ? 255 : v < 30 ? 0 : v);
        }
    }

    for (n = 0; n < count; n++) {
        uint8_t *d = t + (size_t)n * cell;
        struct Comp { short x0, x1, y0, y1, root; int area; } comp[160];
        /* Where two letters are set so close that their fills run into each other (the "ky" of Rocky), the join
           is only a pixel or two of half fill: the pieces are looked for again among the fuller pixels only, step
           by step, until the picture has as many letters as the table says. */
        static const int kSolid[] = {128, 160, 192, 216, 236};
        int nComp = 0, order[160], nGlyph = 0, want = 0, first = nInst, changed, step, solid = 128;
        const char *c;

        for (c = names[n]; *c != '\0'; c++) {
            want += *c != ' ';
        }
        for (step = 0; step < (int)(sizeof(kSolid) / sizeof(kSolid[0])); step++) {
        solid = kSolid[step];
        nComp = 0;
        nGlyph = 0;
        /* the solid fill's connected pieces (4 neighbours: letters set close stay apart) */
        memset(lab, 0, sizeof(short) * (size_t)cell);
        for (i = 0; i < cell; i++) {
            int sp = 0;
            struct Comp *cp;
            if (d[i] < solid || lab[i] != 0) {
                continue;
            }
            if (nComp == 160) {
                goto done;
            }
            cp = &comp[nComp++];
            cp->x0 = cp->x1 = (short)(i % SHEET_W);
            cp->y0 = cp->y1 = (short)(i / SHEET_W);
            cp->area = 0;
            cp->root = (short)(nComp - 1);
            lab[i] = (short)nComp;
            stack[sp++] = i;
            while (sp > 0) {
                int q = stack[--sp], qx = q % SHEET_W, qy = q / SHEET_W;
                static const int kDx[4] = {1, -1, 0, 0}, kDy[4] = {0, 0, 1, -1};
                cp->area++;
                if (qx < cp->x0) { cp->x0 = (short)qx; }
                if (qx > cp->x1) { cp->x1 = (short)qx; }
                if (qy < cp->y0) { cp->y0 = (short)qy; }
                if (qy > cp->y1) { cp->y1 = (short)qy; }
                for (k = 0; k < 4; k++) {
                    int nx = qx + kDx[k], ny = qy + kDy[k], ni = ny * SHEET_W + nx;
                    if (nx >= 0 && nx < SHEET_W && ny >= 0 && ny < rowH && d[ni] >= solid && lab[ni] == 0) {
                        lab[ni] = (short)nComp;
                        stack[sp++] = ni;
                    }
                }
            }
        }
        /* pieces above one another are one letter (the dot of an i, the two parts of a !) */
        do {
            changed = 0;
            for (i = 0; i < nComp; i++) {
                for (j = i + 1; j < nComp; j++) {
                    struct Comp *a = &comp[i], *b = &comp[j];
                    int ov, wa, wb;
                    if (a->root != i || b->root != j) {
                        continue;
                    }
                    ov = (a->x1 < b->x1 ? a->x1 : b->x1) - (a->x0 > b->x0 ? a->x0 : b->x0) + 1;
                    wa = a->x1 - a->x0 + 1;
                    wb = b->x1 - b->x0 + 1;
                    if (ov * 2 >= (wa < wb ? wa : wb)) {
                        if (b->x0 < a->x0) { a->x0 = b->x0; }
                        if (b->x1 > a->x1) { a->x1 = b->x1; }
                        if (b->y0 < a->y0) { a->y0 = b->y0; }
                        if (b->y1 > a->y1) { a->y1 = b->y1; }
                        a->area += b->area;
                        b->root = (short)i;
                        changed = 1;
                    }
                }
            }
        } while (changed);
        for (i = 0; i < nComp; i++) {
            j = i;
            while (comp[j].root != j) {
                j = comp[j].root;
            }
            comp[i].root = (short)j;
            if (j == i && comp[i].area >= 3) {
                order[nGlyph++] = i;
            }
        }
        for (i = 1; i < nGlyph; i++) { /* left to right */
            int o = order[i];
            for (j = i; j > 0 && comp[order[j - 1]].x0 > comp[o].x0; j--) {
                order[j] = order[j - 1];
            }
            order[j] = o;
        }
        if (nGlyph == want) {
            break;
        }
        }
        if (nGlyph != want || nInst + nGlyph > MAX_INST) {
            /* The picture does not have the letters the table says. One wrong name means the table and the
               sheets do not belong together, and then no label can be trusted: the style is not used. */
            if (getenv("BT3_NAMEFONT_TEST") != NULL) {
                fprintf(stderr, "bt3: name font: style %d name %d \"%s\": %d letters in the picture, %d in the table\n", s, n, names[n], nGlyph, want);
                for (i = 0; i < nComp; i++) {
                    fprintf(stderr, "   piece %d: x %d..%d y %d..%d area %d root %d\n", i, comp[i].x0, comp[i].x1, comp[i].y0, comp[i].y1, comp[i].area, comp[i].root);
                }
            }
            goto done;
        }
        for (i = 0; i < cell; i++) { /* every piece's pixels to its letter */
            if (lab[i] != 0) {
                lab[i] = (short)(comp[lab[i] - 1].root + 1);
            }
        }
        /* The rest of the fill (its soft edge, between fill and outline colour) goes to the letter it touches,
           round by round outwards. Marked negative during a round so that a round only sees the one before. */
        for (k = 0, changed = 1; k < 8 && changed; k++) {
            changed = 0;
            for (y = 0; y < rowH; y++) {
                for (x = 0; x < SHEET_W; x++) {
                    int votes[8], vn = 0, dy, dx, bestL = 0, bestN = 0, a, b;
                    i = y * SHEET_W + x;
                    if (d[i] == 0 || lab[i] != 0) {
                        continue;
                    }
                    for (dy = -1; dy <= 1; dy++) {
                        for (dx = -1; dx <= 1; dx++) {
                            int nx = x + dx, ny = y + dy;
                            if ((dx != 0 || dy != 0) && nx >= 0 && nx < SHEET_W && ny >= 0 && ny < rowH && lab[ny * SHEET_W + nx] > 0) {
                                votes[vn++] = lab[ny * SHEET_W + nx];
                            }
                        }
                    }
                    for (a = 0; a < vn; a++) {
                        int cnt = 0;
                        for (b = 0; b < vn; b++) {
                            cnt += votes[b] == votes[a];
                        }
                        if (cnt > bestN) {
                            bestN = cnt;
                            bestL = votes[a];
                        }
                    }
                    lab[i] = (short)-bestL;
                    changed |= bestL != 0;
                }
            }
            for (i = 0; i < cell; i++) {
                if (lab[i] < 0) {
                    lab[i] = (short)-lab[i];
                }
            }
        }
        j = 0;
        for (c = names[n]; *c != '\0'; c++) {
            Inst *in;
            struct Comp *cp;
            if (*c == ' ') {
                continue;
            }
            cp = &comp[order[j++]];
            in = &inst[nInst++];
            in->name = (short)n;
            in->ch = (short)(unsigned char)*c;
            in->label = (short)(cp->root + 1);
            in->bx0 = in->by0 = in->sx0 = in->sy0 = 32767;
            in->bx1 = in->by1 = in->sx1 = in->sy1 = -1;
        }
        for (i = 0; i < cell; i++) {
            if (lab[i] > 0) {
                for (j = first; j < nInst; j++) {
                    if (inst[j].label == lab[i]) {
                        Inst *in = &inst[j];
                        x = i % SHEET_W;
                        y = i / SHEET_W;
                        if (x < in->bx0) { in->bx0 = (short)x; }
                        if (x > in->bx1) { in->bx1 = (short)x; }
                        if (y < in->by0) { in->by0 = (short)y; }
                        if (y > in->by1) { in->by1 = (short)y; }
                        if (d[i] >= 128) { /* the fill's box: by half-filled pixels, whatever step found the pieces */
                            if (x < in->sx0) { in->sx0 = (short)x; }
                            if (x > in->sx1) { in->sx1 = (short)x; }
                            if (y < in->sy0) { in->sy0 = (short)y; }
                            if (y > in->sy1) { in->sy1 = (short)y; }
                        }
                        break;
                    }
                }
            }
        }
        /* this name's gaps: between neighbours, and across a blank */
        j = first;
        for (c = names[n]; *c != '\0'; c++) {
            if (*c == ' ') {
                continue;
            }
            if (j > first) {
                pairs[nPairs].a = inst[j - 1].ch;
                pairs[nPairs].b = inst[j].ch;
                pairs[nPairs].space = c[-1] == ' ';
                pairs[nPairs].gap = (float)(inst[j].sx0 - inst[j - 1].sx1 - 1);
                nPairs++;
            }
            j++;
        }
        /* Every cut's pixels are copied now, while this name's ownership map exists; which cut of each letter is
           kept is decided after the last name (below), and the others are freed there. */
        for (j = first; j < nInst; j++) {
            Inst *in = &inst[j];
            int w = in->bx1 - in->bx0 + 1, h = in->by1 - in->by0 + 1;
            const uint8_t *src = row_px(s, n, rowH);
            uint8_t *px = (uint8_t *)calloc((size_t)w * h, 4);
            if (px == NULL) {
                goto done;
            }
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    i = (in->by0 + y) * SHEET_W + in->bx0 + x;
                    if (lab[i] == in->label) {
                        memcpy(px + (y * w + x) * 4, src + i * 4, 3);
                        px[(y * w + x) * 4 + 3] = d[i];
                    }
                }
            }
            sInstPx[j] = px;
        }
        st->used++;
    }

    /* which cut of each letter: one whose fill is as wide as that letter's cuts are in the middle */
    {
        int ch;
        for (ch = 33; ch < 127; ch++) {
            int widths[MAX_INST], wn = 0, mid, pick = -1;
            for (j = 0; j < nInst; j++) {
                if (inst[j].ch == ch) {
                    widths[wn++] = inst[j].sx1 - inst[j].sx0 + 1;
                }
            }
            if (wn == 0) {
                continue;
            }
            qsort(widths, (size_t)wn, sizeof(int), cmp_int);
            mid = widths[wn / 2];
            if (widths[wn - 1] - widths[0] > st->widthSpread) {
                st->widthSpread = widths[wn - 1] - widths[0];
            }
            for (j = 0; j < nInst; j++) {
                if (inst[j].ch == ch && inst[j].sx1 - inst[j].sx0 + 1 == mid) {
                    pick = j;
                    break;
                }
            }
            if (pick >= 0) {
                NfGlyph *g = &st->g[ch];
                Inst *in = &inst[pick];
                g->px = sInstPx[pick];
                sInstPx[pick] = NULL;
                g->w = (short)(in->bx1 - in->bx0 + 1);
                g->h = (short)(in->by1 - in->by0 + 1);
                g->ox = (short)(in->bx0 - in->sx0);
                g->oy = in->by0;
                g->sw = (short)mid;
                g->sy0 = in->sy0;
                g->sy1 = in->sy1;
            }
        }
        for (j = 0; j < nInst; j++) {
            free(sInstPx[j]);
            sInstPx[j] = NULL;
        }
    }

    /* Bearings. Every gap between two neighbours is the left one's right bearing plus the right one's left bearing;
       with more gaps than letters this is solved by going round (each bearing the mean of what its gaps leave it,
       pulled a little towards half the usual gap, which is what a letter seen in few pairs gets). */
    {
        float lb[128], rb[128], usual, sum[128];
        int cnt[128], it;
        n = 0;
        for (i = 0; i < nPairs; i++) {
            if (!pairs[i].space) {
                gaps[n++] = pairs[i].gap;
            }
        }
        usual = median_f(gaps, n);
        for (i = 0; i < 128; i++) {
            lb[i] = rb[i] = usual * 0.5f;
        }
        for (it = 0; it < 40; it++) {
            memset(sum, 0, sizeof(sum));
            memset(cnt, 0, sizeof(cnt));
            for (i = 0; i < nPairs; i++) {
                if (!pairs[i].space) {
                    sum[pairs[i].a] += pairs[i].gap - lb[pairs[i].b];
                    cnt[pairs[i].a]++;
                }
            }
            for (i = 0; i < 128; i++) {
                rb[i] = (sum[i] + usual * 0.5f) / (float)(cnt[i] + 1);
            }
            memset(sum, 0, sizeof(sum));
            memset(cnt, 0, sizeof(cnt));
            for (i = 0; i < nPairs; i++) {
                if (!pairs[i].space) {
                    sum[pairs[i].b] += pairs[i].gap - rb[pairs[i].a];
                    cnt[pairs[i].b]++;
                }
            }
            for (i = 0; i < 128; i++) {
                lb[i] = (sum[i] + usual * 0.5f) / (float)(cnt[i] + 1);
            }
        }
        for (i = 0; i < 128; i++) {
            st->g[i].lb = lb[i];
            st->g[i].rb = rb[i];
        }
        n = 0;
        for (i = 0; i < nPairs; i++) {
            if (pairs[i].space) {
                gaps[n++] = pairs[i].gap - rb[pairs[i].a] - lb[pairs[i].b];
                nSpaces++;
            }
        }
        st->space = n > 0 ? median_f(gaps, n) : usual * 3.0f;
    }
    /* where the names sit in their rows: the first letter's left edge (songs), the middle of the name (stages) */
    {
        float *lefts = (float *)calloc((size_t)count * 2, sizeof(float)), *mids = lefts + count;
        int prev = -1, m = 0;
        if (lefts == NULL) {
            goto done;
        }
        for (j = 0; j < nInst; j++) {
            if (inst[j].name != prev) {
                prev = inst[j].name;
                for (k = j; k + 1 < nInst && inst[k + 1].name == prev; k++) {
                }
                lefts[m] = (float)inst[j].sx0;
                mids[m] = (float)(inst[j].sx0 + inst[k].sx1 + 1) * 0.5f;
                m++;
            }
        }
        st->margin = median_f(lefts, m);
        st->centre = median_f(mids, m);
        free(lefts);
    }
    ok = 1;
done:
    for (i = 0; i < MAX_INST; i++) { /* (cuts left over when a name stopped the work) */
        free(sInstPx[i]);
        sInstPx[i] = NULL;
    }
    if (!ok) {
        for (i = 0; i < 128; i++) {
            free(st->g[i].px);
        }
        memset(st->g, 0, sizeof(st->g));
    }
    free(t);
    free(lab);
    free(stack);
    free(inst);
    free(pairs);
    free(gaps);
    st->ok = ok;
    return ok;
}

/* Letters one style's names do not have but the other's do (the stage names have no B, F, U or f, the song names no
   G, K, L, N, z or hyphen): the two are one typeface in two sizes, so such a letter is made from the other style's
   cut, brought to this style's size (measured on the capitals both have) and given this style's colours. A little
   softer than a letter cut at its own size; better than giving up the whole name for one letter. */
static void style_lend(NfStyle *to, const NfStyle *from) {
    static const char kCaps[] = "HIEMTDPR"; /* flat at the top and at the bottom */
    float hTo = 0.0f, hFrom = 0.0f, baseTo = 0.0f, baseFrom = 0.0f, k;
    int n = 0, ch, x, y, i;

    if (!to->ok || !from->ok) {
        return;
    }
    for (i = 0; kCaps[i] != '\0'; i++) {
        const NfGlyph *a = &to->g[(int)kCaps[i]], *b = &from->g[(int)kCaps[i]];
        if (a->px != NULL && b->px != NULL && !a->lent && !b->lent) {
            hTo += (float)(a->sy1 - a->sy0 + 1);
            hFrom += (float)(b->sy1 - b->sy0 + 1);
            baseTo += (float)(a->sy1 + 1);
            baseFrom += (float)(b->sy1 + 1);
            n++;
        }
    }
    if (n == 0 || hFrom <= 0.0f) {
        return;
    }
    k = hTo / hFrom;
    baseTo /= (float)n;
    baseFrom /= (float)n;
    for (ch = 33; ch < 127; ch++) {
        const NfGlyph *src = &from->g[ch];
        NfGlyph *g = &to->g[ch];
        int x0, x1, y0, y1, w, h, sx0 = 32767, sx1 = -1, sy0 = 32767, sy1 = -1;
        uint8_t *px;

        if (g->px != NULL || src->px == NULL || src->lent) {
            continue;
        }
        /* the new box, in this style's row: x from the fill's left edge, y from the top of the row */
        x0 = (int)floorf((float)src->ox * k) - 1;
        x1 = (int)ceilf((float)(src->ox + src->w) * k) + 1;
        y0 = (int)floorf(baseTo + ((float)src->oy - baseFrom) * k) - 1;
        y1 = (int)ceilf(baseTo + ((float)(src->oy + src->h) - baseFrom) * k) + 1;
        if (y0 < 0) { y0 = 0; }
        if (y1 > to->rowH) { y1 = to->rowH; }
        w = x1 - x0;
        h = y1 - y0;
        if (w <= 0 || h <= 0 || (px = (uint8_t *)calloc((size_t)w * h, 4)) == NULL) {
            continue;
        }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                float sum = 0.0f;
                int sub, v, c;
                for (sub = 0; sub < 16; sub++) { /* 4 x 4 samples of the pixel, each between the source's pixel centres */
                    float fx = ((float)(x0 + x) + ((float)(sub & 3) + 0.5f) * 0.25f) / k - (float)src->ox - 0.5f;
                    float fy = ((float)(y0 + y) + ((float)(sub >> 2) + 0.5f) * 0.25f - baseTo) / k + baseFrom - (float)src->oy - 0.5f;
                    int ix = (int)floorf(fx), iy = (int)floorf(fy), dx, dy;
                    float ax = fx - (float)ix, ay = fy - (float)iy;
                    for (dy = 0; dy < 2; dy++) {
                        for (dx = 0; dx < 2; dx++) {
                            int px_ = ix + dx, py_ = iy + dy;
                            if (px_ >= 0 && px_ < src->w && py_ >= 0 && py_ < src->h) {
                                sum += (float)src->px[(py_ * src->w + px_) * 4 + 3] * (dx ? ax : 1.0f - ax) * (dy ? ay : 1.0f - ay);
                            }
                        }
                    }
                }
                v = (int)(sum / 16.0f + 0.5f);
                if (v < 30) {
                    continue;
                }
                if (v > 255) { v = 255; }
                for (c = 0; c < 3; c++) {
                    px[(y * w + x) * 4 + c] = (uint8_t)(to->outline[c] + ((int)to->ramp[y0 + y][c] - (int)to->outline[c]) * v / 255);
                }
                px[(y * w + x) * 4 + 3] = (uint8_t)v;
                if (v >= 128) {
                    if (x < sx0) { sx0 = x; }
                    if (x > sx1) { sx1 = x; }
                    if (y < sy0) { sy0 = y; }
                    if (y > sy1) { sy1 = y; }
                }
            }
        }
        if (sx1 < 0) {
            free(px);
            continue;
        }
        g->px = px;
        g->w = (short)w;
        g->h = (short)h;
        g->ox = (short)-sx0;
        g->oy = (short)y0;
        g->sw = (short)(sx1 - sx0 + 1);
        g->sy0 = (short)(y0 + sy0);
        g->sy1 = (short)(y0 + sy1);
        g->lb = src->lb * k;
        g->rb = src->rb * k;
        g->lent = 1;
    }
}

static void build_all(void) {
    int s, i;
    style_build(NF_STAGE, kStageNames, (int)(sizeof(kStageNames) / sizeof(kStageNames[0])), 64);
    style_build(NF_SONG, kSongNames, (int)(sizeof(kSongNames) / sizeof(kSongNames[0])), 32);
    style_build(NF_SONG_LIT, kSongNames, (int)(sizeof(kSongNames) / sizeof(kSongNames[0])), 32);
    style_lend(&sStyle[NF_STAGE], &sStyle[NF_SONG]); /* (the plain song style: its fill is one colour, so the amount of fill reads cleanest) */
    style_lend(&sStyle[NF_SONG], &sStyle[NF_STAGE]);
    style_lend(&sStyle[NF_SONG_LIT], &sStyle[NF_STAGE]);
    if (getenv("BT3_NAMEFONT_TEST") == NULL) { /* (the test compares against the sheets afterwards) */
        for (s = 0; s < NF_STYLES; s++) {
            for (i = 0; i < MAX_SHEETS; i++) {
                free(sSheet[s][i]);
                sSheet[s][i] = NULL;
            }
        }
    }
}

int NameFont_Ready(void) {
    if (sState == 1 && __sync_bool_compare_and_swap(&sState, 1, 2)) {
        build_all();
        __sync_synchronize();
        sState = 3;
    }
    return sState == 3;
}

/* ---- putting a name together --------------------------------------------------------------------------------- */

static NfStyle *style_for(int style) {
    if (style == NF_SONG_LIT && !sStyle[NF_SONG_LIT].ok) {
        style = NF_SONG;
    }
    return style >= 0 && style < NF_STYLES && sStyle[style].ok ? &sStyle[style] : NULL;
}

int NameFont_Has(int style, int ch) {
    NfStyle *st = style_for(style);
    return st != NULL && ch > 32 && ch < 127 && st->g[ch].px != NULL;
}

/* The name's picture: straight RGBA, 8 bits, `*w` x `*h` (h is the row's height on the disc, 64 or 32; w is 512, or
   more when the name is longer than a row). Stage names sit in the middle, song names start where the disc's do.
   NULL when the style has no letters or the name has a character none of the disc's names has. The caller frees. */
unsigned char *NameFont_Compose(int style, const char *text, int *w, int *h) {
    NfStyle *st;
    const unsigned char *c;
    float pen = 0.0f, width, start, *fill, *bodyA, *shadow, *tmp, kern[2 * 8 + 1];
    int W, H, n = 0, i, x, y, dx, dy, last = -1, pendingSpace = 0;
    uint8_t *rgb, *out;
    struct { int ch; float x; } item[128];

    if (!NameFont_Ready() || (st = style_for(style)) == NULL || text == NULL) {
        return NULL;
    }
    for (c = (const unsigned char *)text; *c != '\0'; c++) {
        if (*c == ' ') {
            pendingSpace += last >= 0; /* (blanks in front of the name do not count) */
            continue;
        }
        if (*c >= 127 || st->g[*c].px == NULL || n == 128) {
            return NULL;
        }
        if (last >= 0) {
            pen += st->g[last].rb + st->g[*c].lb + (float)pendingSpace * st->space;
        }
        pendingSpace = 0;
        item[n].ch = *c;
        item[n].x = pen;
        n++;
        pen += (float)st->g[*c].sw;
        last = *c;
    }
    if (n == 0) {
        return NULL;
    }
    width = pen;
    H = st->rowH;
    W = SHEET_W;
    if (style == NF_STAGE) {
        if (width + 24.0f > (float)W) {
            W = (int)width + 24;
        }
        start = st->centre + (float)(W - SHEET_W) * 0.5f - width * 0.5f;
    } else {
        start = st->margin;
        if (start + width + 16.0f > (float)W) {
            W = (int)(start + width) + 16;
        }
    }
    fill = (float *)calloc((size_t)W * H * 4, sizeof(float));
    rgb = (uint8_t *)calloc((size_t)W * H, 3);
    out = (uint8_t *)calloc((size_t)W * H, 4);
    if (fill == NULL || rgb == NULL || out == NULL) {
        free(fill);
        free(rgb);
        free(out);
        return NULL;
    }
    bodyA = fill + (size_t)W * H;
    shadow = bodyA + (size_t)W * H;
    tmp = shadow + (size_t)W * H;
    /* the fills, each letter at the height it has on the disc (the colour ramp runs down the row) */
    for (i = 0; i < n; i++) {
        NfGlyph *g = &st->g[item[i].ch];
        int gx = (int)floorf(start + item[i].x + 0.5f) + g->ox;
        for (y = 0; y < g->h; y++) {
            for (x = 0; x < g->w; x++) {
                const uint8_t *p = g->px + (y * g->w + x) * 4;
                int cx = gx + x, cy = g->oy + y;
                if (p[3] != 0 && cx >= 0 && cx < W && cy >= 0 && cy < H && (float)p[3] > fill[cy * W + cx] * 255.0f) {
                    fill[cy * W + cx] = (float)p[3] / 255.0f;
                    memcpy(rgb + (cy * W + cx) * 3, p, 3);
                }
            }
        }
    }
    /* the outline: everything within OUTLINE_R of the fill, with a soft edge that follows the fill's soft edge */
    for (i = 0; i < W * H; i++) {
        bodyA[i] = -100.0f;
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            float f = fill[y * W + x];
            if (f <= 0.0f) {
                continue;
            }
            for (dy = -4; dy <= 4; dy++) {
                for (dx = -4; dx <= 4; dx++) {
                    int cx = x + dx, cy = y + dy;
                    float v = f - sqrtf((float)(dx * dx + dy * dy));
                    if (cx >= 0 && cx < W && cy >= 0 && cy < H && v > bodyA[cy * W + cx]) {
                        bodyA[cy * W + cx] = v;
                    }
                }
            }
        }
    }
    for (i = 0; i < W * H; i++) {
        float v = bodyA[i] + OUTLINE_R;
        bodyA[i] = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    }
    /* the shadow: the fill, moved and blurred */
    {
        float sum = 0.0f;
        for (i = -8; i <= 8; i++) {
            kern[i + 8] = expf(-(float)(i * i) / (2.0f * SHADOW_SIGMA * SHADOW_SIGMA));
            sum += kern[i + 8];
        }
        for (i = 0; i < 17; i++) {
            kern[i] /= sum;
        }
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                float v = 0.0f;
                for (i = -5; i <= 5; i++) {
                    int sx = x - SHADOW_DX + i;
                    if (sx >= 0 && sx < W) {
                        v += kern[i + 8] * fill[y * W + sx];
                    }
                }
                tmp[y * W + x] = v;
            }
        }
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                float v = 0.0f;
                for (i = -5; i <= 5; i++) {
                    int sy = y - SHADOW_DY + i;
                    if (sy >= 0 && sy < H) {
                        v += kern[i + 8] * tmp[sy * W + x];
                    }
                }
                shadow[y * W + x] = v > 1.0f ? 1.0f : v;
            }
        }
    }
    /* fill over outline over shadow. The disc's pictures are opaque at alpha 0x7F of 0x80. */
    for (i = 0; i < W * H; i++) {
        float b = bodyA[i], a = b + shadow[i] * (1.0f - b);
        const uint8_t *col = fill[i] > 0.0f ? rgb + i * 3 : st->outline;
        if (a <= 0.0f) {
            memcpy(out + i * 4, st->outline, 3); /* (no dark fringe when the picture is drawn larger) */
            continue;
        }
        out[i * 4 + 0] = (uint8_t)((float)col[0] * b / a + 0.5f);
        out[i * 4 + 1] = (uint8_t)((float)col[1] * b / a + 0.5f);
        out[i * 4 + 2] = (uint8_t)((float)col[2] * b / a + 0.5f);
        out[i * 4 + 3] = (uint8_t)(a * (255.0f * 127.0f / 128.0f) + 0.5f);
    }
    free(fill);
    free(rgb);
    *w = W;
    *h = H;
    return out;
}

/* ---- testing ------------------------------------------------------------------------------------------------- */

static void ppm_write(const char *path, const uint8_t *rgba, int w, int h, int ps2Alpha) {
    FILE *fp = fopen(path, "wb");
    int i, k;
    if (fp == NULL) {
        fprintf(stderr, "bt3: name font test: cannot write %s\n", path);
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; i++) { /* over a flat grey-blue, so that outline and shadow are both seen */
        static const uint8_t kBack[3] = {96, 120, 150};
        int a = ps2Alpha ? rgba[i * 4 + 3] * 255 / 128 : rgba[i * 4 + 3];
        for (k = 0; k < 3; k++) {
            fputc((rgba[i * 4 + k] * a + kBack[k] * (255 - a)) / 255, fp);
        }
    }
    fclose(fp);
}

/* BT3_NAMEFONT_TEST="Stage Name|Song Name" and BT3_NAMEFONT_OUT=<folder>: once the letters are cut, says which
   characters each style has, how far a disc name put together again is from its picture, and writes the two test
   names as pictures (stage.ppm, song.ppm, song_lit.ppm), made by the same code the overlay's pictures come from.
   BT3_NAMEFONT_DISC=<n> also writes disc stage name n both ways (disc_cut.ppm, disc_orig.ppm). */
static void test_run(void) {
    static const char *const kStyleName[NF_STYLES] = {"stage", "song", "song_lit"};
    const char *spec = getenv("BT3_NAMEFONT_TEST"), *dir = getenv("BT3_NAMEFONT_OUT"), *bar;
    char text[2][128], path[700];
    int s, c, w, h, i;

    if (!NameFont_Ready()) {
        return;
    }
    memset(text, 0, sizeof(text));
    bar = strchr(spec, '|');
    snprintf(text[0], sizeof(text[0]), "%.*s", bar != NULL ? (int)(bar - spec) : (int)strlen(spec), spec);
    snprintf(text[1], sizeof(text[1]), "%s", bar != NULL ? bar + 1 : spec);
    if (dir == NULL) {
        dir = ".";
    }
    for (s = 0; s < NF_STYLES; s++) {
        NfStyle *st = &sStyle[s];
        const char *const *names = s == NF_STAGE ? kStageNames : kSongNames;
        const char *t = text[s == NF_STAGE ? 0 : 1];
        char have[128], miss[128];
        int hn = 0, mn = 0;
        uint8_t *pic;

        char lent[128];
        int ln = 0;
        for (c = 33; c < 127; c++) {
            if (st->g[c].px != NULL && st->g[c].lent) {
                lent[ln++] = (char)c;
            } else if (st->g[c].px != NULL) {
                have[hn++] = (char)c;
            } else {
                miss[mn++] = (char)c;
            }
        }
        have[hn] = miss[mn] = lent[ln] = '\0';
        fprintf(stderr, "bt3: name font: %s: %s, %d of %d names cut, %d characters\n", kStyleName[s], st->ok ? "ok" : "NOT USABLE", st->used, st->names, hn);
        if (!st->ok) {
            continue;
        }
        fprintf(stderr, "bt3: name font: %s has:     %s\n", kStyleName[s], have);
        fprintf(stderr, "bt3: name font: %s from the other size: %s\n", kStyleName[s], lent);
        fprintf(stderr, "bt3: name font: %s missing: %s\n", kStyleName[s], miss);
        fprintf(stderr, "bt3: name font: %s outline %d,%d,%d; blank %.2f; cuts of one letter differ by up to %d px in width\n", kStyleName[s],
                st->outline[0], st->outline[1], st->outline[2], st->space, st->widthSpread);
        /* every disc name put together again, against its picture: mean and largest difference in alpha (of 128) */
        {
            double total = 0.0, worst = 0.0;
            int worstN = -1, n;
            for (n = 0; n < st->names; n++) {
                const uint8_t *src = row_px(s, n, st->rowH);
                double bestErr = 1e30;
                int shift;
                pic = NameFont_Compose(s, names[n], &w, &h);
                if (pic == NULL || w != SHEET_W) {
                    free(pic);
                    continue;
                }
                for (shift = -1; shift <= 1; shift++) { /* (the name's place may be a pixel off) */
                    double e = 0.0;
                    for (i = 0; i < w * h; i++) {
                        int x = i % w + shift;
                        int a = x >= 0 && x < w ? pic[(i + shift) * 4 + 3] * 128 / 255 : 0;
                        e += abs(a - src[i * 4 + 3]);
                    }
                    if (e < bestErr) {
                        bestErr = e;
                    }
                }
                bestErr /= (double)(w * h);
                total += bestErr;
                if (bestErr > worst) {
                    worst = bestErr;
                    worstN = n;
                }
                free(pic);
            }
            fprintf(stderr, "bt3: name font: %s disc names put together again: mean alpha difference %.3f of 128 per pixel of the row, worst %.3f (\"%s\")\n",
                    kStyleName[s], total / st->names, worst, worstN >= 0 ? names[worstN] : "");
        }
        mn = 0;
        for (i = 0; t[i] != '\0'; i++) {
            if (t[i] != ' ' && !NameFont_Has(s, (unsigned char)t[i])) {
                miss[mn++] = t[i];
            }
        }
        miss[mn] = '\0';
        pic = NameFont_Compose(s, t, &w, &h);
        if (pic == NULL) {
            fprintf(stderr, "bt3: name font: %s \"%s\": not drawn, no letter for: %s (the overlay falls back to plain text)\n", kStyleName[s], t, miss);
        } else {
            snprintf(path, sizeof(path), "%s/%s.ppm", dir, kStyleName[s]);
            ppm_write(path, pic, w, h, 0);
            fprintf(stderr, "bt3: name font: %s \"%s\" -> %s (%dx%d)\n", kStyleName[s], t, path, w, h);
            free(pic);
        }
        if (s == NF_STAGE && getenv("BT3_NAMEFONT_DISC") != NULL) {
            int n = atoi(getenv("BT3_NAMEFONT_DISC"));
            if (n >= 0 && n < st->names && (pic = NameFont_Compose(s, names[n], &w, &h)) != NULL) {
                snprintf(path, sizeof(path), "%s/disc_cut.ppm", dir);
                ppm_write(path, pic, w, h, 0);
                snprintf(path, sizeof(path), "%s/disc_orig.ppm", dir);
                ppm_write(path, row_px(s, n, st->rowH), SHEET_W, st->rowH, 1);
                free(pic);
            }
        }
    }
}
