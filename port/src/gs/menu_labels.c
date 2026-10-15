/* The list of menu labels of the "Menu font" setting, made from the player's own disc data.
 *
 * The menus' labels are lettering painted into textures. To draw one again from a font (menu_text.c) the renderer
 * has to know which texture holds which label, in which row, and how large its letters are. That list
 * (labels.tsv, and name-styles.tsv for the names of added stages and songs) is made here, from the menu archives
 * in the player's gamedata: nothing of it is in the repository or in a release. What this file carries is what a
 * contributor wrote down: which archive, section and texture a menu's labels are in, and what they SAY (catalog()).
 * Everything else is measured: the textures are unpacked and decoded, hashed the way the renderer hashes a decoded
 * texture, and each row's letter height, baseline, margin and width are taken from its pixels; for the long name
 * strips (stages, songs, the main menu) the port's own letter cutter (namefont.c) tells capitals from lower case.
 *
 * The texture numbers are those of the USA release (SLUS-21678), so the eleven archives are checked by size and
 * CRC-32 first; with any other disc nothing is made. The archives are the player's files: every read of them, of
 * what is unpacked from them and of the textures in them is checked against the buffer's size, whatever the
 * check of the disc said (port/tests/menu_font/labels.c runs this on damaged archives with the sanitizers).
 *
 * This was port/tools/menu_font.py (Python, with a C helper built by gcc at installation); the files written are
 * the same bytes. It moved here so that a release, which has neither Python nor a compiler, has the setting too.
 * labels.json, the form of the list a person edits (docs/port/menu_font.md), is written as well unless one is
 * there; port/tools/menu_font.py turns an edited one into labels.tsv.
 *
 * Called by menu_text.c, at start, in a thread of its own. Only files are read and written: nothing of the game's
 * memory, nothing of the renderer's. The letter cutter is compiled into this file a second time, with its own
 * variables, so the one the stage select uses (the game's and the render thread's) is not touched either. */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif
#include "menu_labels.h"

/* namefont.c again, as this file's own: the functions the game calls get other names, the variables are static. */
#define Port_NameFontSheets MenuLabels_NameSheets_
#define NameFont_Ready MenuLabels_NameReady_
#define NameFont_Has MenuLabels_NameHas_
#define NameFont_Compose MenuLabels_NameCompose_
#include "namefont.c"

#define ML_FILES 11

/* The texts of the main menu's labels, in the order of their rows (the list's, and the PC's item: menu_a.c). */
static const char kMlMain[] = "Dragon History|Ultimate Battle|Dragon World Tour|Duel|Dragon Net Battle|Evolution Z|"
                              "Ultimate Training|Data Center|Character Reference|Options|Shenron";
/* The PC's item: its label is written into the free row after them (src/menu/menu_a.c). */
static const char kMlItem[] = "Tenkaichi Budokai Online";

/* The letter cutter (namefont.c's, this file's copy) is used by MenuLabels_Make and MenuLabels_MainItem, which
   may run in two threads: one at a time. */
static pthread_mutex_t sMlCutter = PTHREAD_MUTEX_INITIALIZER;
#define ML_UNPACKED_MAX (64u * 1024 * 1024)
#define ML_GS_BYTES (512 * 512 * 4) /* the part of the GS's memory a menu texture is uploaded to and read from */
#define ML_GROUPS 128
#define ML_LABELS 40
#define ML_TEXTURES 16
#define ML_ROWS 512 /* what menu_text.c holds */

/* The menu archives the labels are in (gamedata/pzs3us1/<number>.bin), with size and CRC-32 on the USA disc. The
   section and texture numbers in catalog() are only right for these files: anything else is left alone. */
static const struct { int number; uint32_t size, crc; } kMlFiles[ML_FILES] = {
    {448, 3693312, 0xFF3A84B3}, {449, 1544768, 0xB0DA7301}, {450, 2884544, 0x5E5A6AD3}, {451, 8374016, 0xBAC607A9},
    {452, 3466880, 0x0EC0646D}, {453, 1776896, 0x717BA147}, {459, 3329600, 0xE34CA2A0}, {460, 2398528, 0x13642238},
    {461, 3604416, 0xD60327A4}, {463, 875072, 0xB576B4B2}, {472, 740992, 0x26936C01}};

/* The colours of a label. They are chosen by what a label is (ml_color_style), never read from the texture. */
typedef struct MlColors {
    const char *name;
    uint8_t top[3], bottom[3], outline[3];
    int outlineWidth; /* pixels of the original texture; -1: the renderer's own (usually 3) */
    int outer;        /* a second outline around the first: */
    uint8_t outerColor[3];
    int outerWidth;
    int shadow;
} MlColors;
static const MlColors kMlColors[5] = {
    {"title-prompt", {239, 239, 223}, {177, 179, 96}, {63, 21, 17}, 2, 1, {217, 194, 193}, 2, 0},
    {"main-menu", {203, 203, 203}, {203, 203, 203}, {94, 35, 109}, -1, 0, {0, 0, 0}, 0, 1},
    {"main-heading", {255, 190, 47}, {255, 34, 34}, {124, 48, 37}, 3, 1, {255, 238, 216}, 2, 0},
    {"purple", {255, 255, 255}, {255, 255, 255}, {70, 0, 89}, -1, 0, {0, 0, 0}, 0, 1},
    {"orange", {244, 239, 137}, {210, 104, 75}, {44, 21, 27}, -1, 0, {0, 0, 0}, 0, 1},
};
enum { ML_TITLE_PROMPT, ML_MAIN_MENU, ML_MAIN_HEADING, ML_PURPLE, ML_ORANGE };
static const uint8_t kHeadingOutlineBottom[3] = {100, 24, 12};
static const uint8_t kHeadingBevelLight[3] = {168, 108, 91};
static const uint8_t kHeadingBevelDark[3] = {24, 6, 3};

typedef struct MlBuf {
    const uint8_t *p;
    size_t n;
} MlBuf;

typedef struct MlTex { /* a decoded texture: RGBA as the GS has it (alpha 0x80 = opaque) */
    uint8_t *rgba;
    int w, h;
    uint64_t hash;
} MlTex;

typedef struct MlRect {
    char text[4];
    int x, y, w, h;
} MlRect;

typedef struct MlGroup { /* the labels of one texture (or, for the name strips, of a run of textures) */
    char id[48], textFrom[48];
    int height, width;
    int strip;    /* one of the long name strips: measured with the letter cutter, never a `centered` of its own */
    int centered;
    const char *labels[ML_LABELS];
    int nLabels;
    MlTex tex[ML_TEXTURES];
    int nTex;
    int hasOutline, hasFont, hasArt;
    uint8_t outline[3];
    int fontCap, fontBaseline, fontArt, fontMargin;
    MlRect rect[10];
    int nRects;
} MlGroup;

typedef struct MlRow { /* one label: a row of labels.tsv, an entry of labels.json */
    char id[64], textFrom[64];
    uint64_t hash;
    int isRect, texW, texH, x, y, w;
    const char *text;
    int height, cap, baseline, center, margin, art;
    int colors, italic;
} MlRow;

typedef struct MlCtx {
    void **allocs;
    int nAllocs, capAllocs;
    char why[240];
    MlBuf file[ML_FILES];
    struct { int file, outer; MlBuf data; } screen[32];
    int nScreens;
    int32_t *gs;       /* for each byte of GS memory: the byte of the upload that lands there, or -1 */
    uint32_t gsW, gsH, gsBw; /* the upload `gs` was made for (0: none) */
    uint32_t *map;     /* for each pixel of a texture: the upload's byte it is read from, and the shift in it */
    MlGroup *groups;
    int nGroups;
    MlRow *rows;
    int nRows;
} MlCtx;

/* Testing (port/tests/menu_font/labels.c): called with every unpacked section, to damage it. */
static void (*sMlDamage)(uint8_t *data, size_t *n);

static int ml_fail(MlCtx *c, const char *fmt, ...) {
    if (c->why[0] == '\0') { /* (the first reason is the one) */
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(c->why, sizeof(c->why), fmt, ap);
        va_end(ap);
    }
    return 0;
}

/* Memory that lives until the list is made or given up: freed in one go (ml_free). */
static void *ml_alloc(MlCtx *c, size_t n) {
    void *p;
    if (c->nAllocs == c->capAllocs) {
        int cap = c->capAllocs ? c->capAllocs * 2 : 256;
        void **more = realloc(c->allocs, (size_t)cap * sizeof(*more));
        if (more == NULL) {
            ml_fail(c, "out of memory");
            return NULL;
        }
        c->allocs = more;
        c->capAllocs = cap;
    }
    p = calloc(n ? n : 1, 1);
    if (p == NULL) {
        ml_fail(c, "out of memory");
        return NULL;
    }
    c->allocs[c->nAllocs++] = p;
    return p;
}

static void ml_free(MlCtx *c) {
    int i;
    for (i = 0; i < c->nAllocs; i++) {
        free(c->allocs[i]);
    }
    free(c->allocs);
    c->allocs = NULL;
    c->nAllocs = c->capAllocs = 0;
}

/* ---- the archives ---------------------------------------------------------------------------------------------- */

static int ml_get32(MlBuf d, uint64_t at, uint32_t *v) {
    if (at > d.n || d.n - at < 4) {
        return 0;
    }
    *v = (uint32_t)d.p[at] | (uint32_t)d.p[at + 1] << 8 | (uint32_t)d.p[at + 2] << 16 | (uint32_t)d.p[at + 3] << 24;
    return 1;
}

static int ml_get64(MlBuf d, uint64_t at, uint64_t *v) {
    uint32_t lo, hi;
    if (at > d.n || d.n - at < 8 || !ml_get32(d, at, &lo) || !ml_get32(d, at + 4, &hi)) {
        return 0;
    }
    *v = (uint64_t)hi << 32 | lo;
    return 1;
}

/* A member of an archive: from the offset its table names to the archive's end. */
static int ml_section(MlCtx *c, MlBuf d, uint32_t index, MlBuf *out) {
    uint32_t off;
    if (!ml_get32(d, (uint64_t)index * 4, &off)) {
        return ml_fail(c, "a menu archive is shorter than its table");
    }
    off &= ~3u;
    out->p = d.p + (off < d.n ? off : d.n);
    out->n = off < d.n ? d.n - off : 0;
    return 1;
}

/* The game's byte-pair compression (src/sys/bpe.c), written again with every read and write checked: `data` is
   the whole member (size and packed size first), `size` the unpacked size it names. Returns the bytes written, or
   -1. No game code is executed. */
static long ml_unpack_bytes(const uint8_t *data, size_t len, uint8_t *out, size_t size) {
    size_t p = 8, n = 0, end;
    if (len < 8) { return -1; }
    end = 8 + (size_t)((uint32_t)data[4] | (uint32_t)data[5] << 8 | (uint32_t)data[6] << 16 | (uint32_t)data[7] << 24);
    if (end > len || end < 8) { return -1; }
    while (p < end) {
        uint8_t left[256], right[256], stack[512];
        int ch = 0, i, sp;
        size_t block, k;
        for (i = 0; i < 256; i++) { left[i] = (uint8_t)i; right[i] = 0; }
        while (ch < 256) {
            int count;
            if (p >= end) { return -1; }
            count = data[p++];
            if (count > 127) { ch += count - 127; count = 0; }
            if (ch == 256) { break; }
            for (i = 0; i <= count; i++) {
                if (ch >= 256 || p >= end) { return -1; }
                left[ch] = data[p++];
                if (ch != left[ch]) {
                    if (p >= end) { return -1; }
                    right[ch] = data[p++];
                }
                ch++;
            }
        }
        if (end - p < 2) { return -1; }
        block = (size_t)data[p] * 256 + data[p + 1];
        p += 2;
        if (end - p < block) { return -1; }
        for (k = 0; k < block; k++) {
            sp = 0;
            stack[sp++] = data[p + k];
            while (sp > 0) {
                uint8_t v = stack[--sp];
                if (left[v] == v) {
                    if (n >= size) { return -1; }
                    out[n++] = v;
                } else {
                    if (sp + 2 > 256) { return -1; }
                    stack[sp++] = right[v];
                    stack[sp++] = left[v];
                }
            }
        }
        p += block;
    }
    return p == end ? (long)n : -1;
}

static int ml_unpack(MlCtx *c, MlBuf d, MlBuf *out) {
    uint32_t size, packed;
    uint8_t *buf;
    size_t n;

    if (!ml_get32(d, 0, &size) || !ml_get32(d, 4, &packed) || size > ML_UNPACKED_MAX || packed > d.n - 8) {
        return ml_fail(c, "a menu archive's compressed part has a wrong header");
    }
    buf = ml_alloc(c, size);
    if (buf == NULL) {
        return 0;
    }
    if (ml_unpack_bytes(d.p, d.n, buf, size) != (long)size) {
        return ml_fail(c, "a menu archive's compressed part is damaged");
    }
    n = size;
    if (sMlDamage != NULL) {
        sMlDamage(buf, &n);
    }
    out->p = buf;
    out->n = n;
    return 1;
}

static int ml_file(int number) {
    int i;
    for (i = 0; i < ML_FILES && kMlFiles[i].number != number; i++) {
    }
    return i;
}

/* The unpacked member `outer` of the archive `file` (kept: several menus are in one). */
static int ml_screen(MlCtx *c, int file, int outer, MlBuf *out) {
    MlBuf packed;
    int i, f = ml_file(file);

    for (i = 0; i < c->nScreens; i++) {
        if (c->screen[i].file == file && c->screen[i].outer == outer) {
            *out = c->screen[i].data;
            return 1;
        }
    }
    if (f == ML_FILES || c->nScreens == (int)(sizeof(c->screen) / sizeof(c->screen[0]))) {
        return ml_fail(c, "the catalogue names more archives than there is room for");
    }
    if (!ml_section(c, c->file[f], (uint32_t)outer, &packed) || !ml_unpack(c, packed, out)) {
        return 0;
    }
    c->screen[c->nScreens].file = file;
    c->screen[c->nScreens].outer = outer;
    c->screen[c->nScreens++].data = *out;
    return 1;
}

/* ---- the textures ---------------------------------------------------------------------------------------------- */

/* GS memory order for 4-bit textures (the hardware's tables; the 32-bit and 8-bit ones are namefont.c's). */
static const uint8_t kMlBlock4[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static const uint8_t kMlCol4[4][32] = {
    {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
    {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
    {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
    {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63}};

/* The width a texture entry (TexEntry, 0x40 bytes) of a texture file names. 0: the file ends before the entry. */
static int ml_entry_width(MlBuf d, uint32_t index, int *width) {
    uint32_t table;
    uint64_t tex0;
    if (!ml_get32(d, 4, &table) || !ml_get64(d, (uint64_t)table * 4 + (uint64_t)index * 64 + 48, &tex0)) {
        return 0;
    }
    *width = 1 << ((tex0 >> 26) & 15);
    return 1;
}

/* A texture of a texture file to RGBA, by the format and the buffer widths it is uploaded and read with: 8 or 4
   bits with a colour table, at most 512 x 512, the pixels sent as a 32-bit image. Returns 1; 0 if it is no such
   texture (it is then no sheet of labels); -1 if the file is not what it should be. */
static int ml_texture(MlCtx *c, MlBuf d, uint32_t index, MlTex *out) {
    uint32_t table, blt, pixAt, palAt, uw, uh, dbw, tbw, x, y, i, count;
    uint64_t e, tex0, reg, pixels, palette;
    uint8_t colors[256][4];
    int w, h, psm;

    if (!ml_get32(d, 4, &table)) {
        return ml_fail(c, "a texture file in a menu archive is cut short") - 1;
    }
    e = (uint64_t)table * 4 + (uint64_t)index * 64;
    if (e > d.n || d.n - e < 64) {
        return 0;
    }
    ml_get64(d, e + 48, &tex0);
    ml_get32(d, e + 24, &blt);
    ml_get32(d, e, &pixAt);
    ml_get32(d, e + 4, &palAt);
    w = 1 << ((tex0 >> 26) & 15);
    h = 1 << ((tex0 >> 30) & 15);
    psm = (int)((tex0 >> 20) & 63);
    if (w > 512 || h > 512 || (psm != 19 && psm != 20) || blt >> 8 != 0) {
        return 0;
    }
    pixels = (uint64_t)pixAt * 4 + 96;
    palette = (uint64_t)palAt * 4 + 96;
    if (!ml_get64(d, pixels - 48, &reg)) {
        return ml_fail(c, "a texture in a menu archive lies outside its file") - 1;
    }
    uw = (uint32_t)(reg & 4095);
    uh = (uint32_t)((reg >> 32) & 4095);
    dbw = blt & 63;
    tbw = (uint32_t)((tex0 >> 14) & 63);
    if (c->gs == NULL) {
        c->gs = ml_alloc(c, ML_GS_BYTES * sizeof(*c->gs));
        c->map = ml_alloc(c, 512 * 512 * sizeof(*c->map));
        if (c->gs == NULL || c->map == NULL) {
            return -1;
        }
    }
    if (c->gsW != uw || c->gsH != uh || c->gsBw != dbw || c->gsW == 0) { /* the upload: 32-bit pixels, buffer `dbw` wide */
        c->gsW = 0;
        for (i = 0; i < ML_GS_BYTES; i++) {
            c->gs[i] = -1;
        }
        for (y = 0; y < uh; y++) {
            for (x = 0; x < uw; x++) {
                uint32_t a = (((y & ~31u) * dbw + ((x >> 1) & ~31u) + kBlock32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + kCol32[y & 7][x & 7];
                int32_t at = (int32_t)((y * uw + x) * 4);
                if (a >= ML_GS_BYTES / 4) {
                    return ml_fail(c, "a texture in a menu archive is uploaded outside the memory menus use") - 1;
                }
                c->gs[a * 4] = at;
                c->gs[a * 4 + 1] = at + 1;
                c->gs[a * 4 + 2] = at + 2;
                c->gs[a * 4 + 3] = at + 3;
            }
        }
        c->gsW = uw;
        c->gsH = uh;
        c->gsBw = dbw;
    }
    for (y = 0; y < (uint32_t)h; y++) { /* read back as 8 or 4 bits, buffer `tbw` wide */
        for (x = 0; x < (uint32_t)w; x++) {
            uint32_t col = (y & 15) >> 2, sx = x ^ ((col & 1) ? 4u : 0u), a, address, shift;
            if (psm == 19) {
                a = ((((y >> 1) & ~31u) * (tbw >> 1) + ((x >> 2) & ~31u) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + kCol8First[y & 3][sx & 15] + col * 64;
                address = a;
                shift = 0;
            } else {
                a = ((((y >> 2) & ~31u) * (tbw >> 1) + ((x >> 2) & ~31u) + kMlBlock4[(y >> 4) & 7][(x >> 5) & 3]) << 9) + kMlCol4[y & 3][sx & 31] + col * 128;
                address = a >> 1;
                shift = (a & 1) * 4;
            }
            if (address >= ML_GS_BYTES) {
                return ml_fail(c, "a texture in a menu archive is read from outside the memory menus use") - 1;
            }
            if (c->gs[address] < 0) {
                return 0; /* its size is outside what was uploaded */
            }
            c->map[y * (uint32_t)w + x] = (uint32_t)c->gs[address] << 3 | shift;
        }
    }
    count = psm == 19 ? 256 : 16;
    for (i = 0; i < count; i++) {
        /* the table's own order: entries 8..15 and 16..23 of every 32 change places */
        uint64_t at = palette + (uint64_t)(psm == 19 ? ((i & 0xE7) | ((i & 8) << 1) | ((i & 16) >> 1)) : i) * 4;
        if (at > d.n || d.n - at < 4) {
            return ml_fail(c, "a texture's colours in a menu archive lie outside its file") - 1;
        }
        memcpy(colors[i], d.p + at, 4);
    }
    out->rgba = ml_alloc(c, (size_t)w * h * 4);
    if (out->rgba == NULL) {
        return -1;
    }
    for (i = 0; i < (uint32_t)(w * h); i++) {
        uint64_t at = pixels + (c->map[i] >> 3);
        if (at >= d.n) {
            return ml_fail(c, "a texture's pixels in a menu archive lie outside its file") - 1;
        }
        memcpy(out->rgba + (size_t)i * 4, colors[(d.p[at] >> (c->map[i] & 7)) & (count - 1)], 4);
    }
    out->w = w;
    out->h = h;
    /* FNV-1a, 64 bits, over the decoded pixels: what menu_text.c looks a texture up by */
    out->hash = UINT64_C(14695981039346656037);
    for (i = 0; i < (uint32_t)(w * h * 4); i++) {
        out->hash = (out->hash ^ out->rgba[i]) * UINT64_C(1099511628211);
    }
    return 1;
}

/* ---- the catalogue --------------------------------------------------------------------------------------------- */

/* The PC's item of the main menu (at the end of this file). */
static int ml_item_pics(MlCtx *c, unsigned char *out[2]);
static int ml_item_into(MlCtx *c, MlBuf screen, int part, MlTex *sheet, const unsigned char *pic);

static MlGroup *ml_group(MlCtx *c, const char *id, int height, int width) {
    MlGroup *g;
    if (c->nGroups == ML_GROUPS) {
        ml_fail(c, "the catalogue has more entries than there is room for");
        return NULL;
    }
    g = &c->groups[c->nGroups++];
    snprintf(g->id, sizeof(g->id), "%s", id);
    g->height = height;
    g->width = width;
    return g;
}

/* The labels of a group, top row first: `text` with a bar between two (an empty one: a row that is no label). */
static int ml_labels(MlCtx *c, MlGroup *g, const char *text) {
    while (text != NULL) {
        const char *bar = strchr(text, '|');
        size_t n = bar != NULL ? (size_t)(bar - text) : strlen(text);
        char *copy = ml_alloc(c, n + 1);
        if (copy == NULL || g->nLabels == ML_LABELS) {
            return ml_fail(c, "the catalogue has more labels in a texture than there is room for");
        }
        memcpy(copy, text, n);
        g->labels[g->nLabels++] = copy;
        text = bar != NULL ? bar + 1 : NULL;
    }
    return 1;
}

/* The name strips: every texture 512 wide of the texture file `part` of `pack`, the first `keep` of them. */
static MlGroup *ml_strip(MlCtx *c, const char *id, int height, MlBuf pack, int part, int keep) {
    MlGroup *g = ml_group(c, id, height, 512);
    MlBuf d;
    uint32_t count, i;

    if (g == NULL || !ml_section(c, pack, (uint32_t)part, &d)) {
        return NULL;
    }
    g->strip = 1;
    if (!ml_get32(d, 0, &count)) {
        ml_fail(c, "a texture file in a menu archive is cut short");
        return NULL;
    }
    for (i = 0; i < count; i++) {
        MlTex t;
        int width, r;
        if (!ml_entry_width(d, i, &width)) {
            ml_fail(c, "a texture file in a menu archive has fewer textures than it says");
            return NULL;
        }
        if (width != 512) {
            continue;
        }
        r = ml_texture(c, d, i, &t);
        if (r < 0) {
            return NULL;
        }
        if (r > 0 && g->nTex < keep) {
            if (g->nTex == ML_TEXTURES) {
                ml_fail(c, "a menu archive has more name sheets than there is room for");
                return NULL;
            }
            g->tex[g->nTex++] = t;
        }
    }
    return g;
}

/* One texture of labels, by its place: archive `file`, its member `outer` (unpacked), in it the texture file `part`
   (inside the member `nested` if that is not -1), in it the texture `index`, which has to be `width` wide.
   `labels`: see ml_labels; NULL for none. `textFrom`: the group whose labels these are the other state of. */
static MlGroup *ml_add(MlCtx *c, const char *id, int file, int outer, int part, int index, const char *labels, int centered,
                       const char *textFrom, int width, int nested) {
    MlGroup *g = ml_group(c, id, 32, width);
    MlBuf pack, d;
    int actual = 0;

    if (g == NULL || !ml_screen(c, file, outer, &pack) || (nested >= 0 && !ml_section(c, pack, (uint32_t)nested, &pack)) ||
        !ml_section(c, pack, (uint32_t)part, &d) || !ml_labels(c, g, labels)) {
        return NULL;
    }
    g->centered = centered;
    snprintf(g->textFrom, sizeof(g->textFrom), "%s", textFrom != NULL ? textFrom : "");
    if (ml_texture(c, d, (uint32_t)index, &g->tex[0]) <= 0 || !ml_entry_width(d, (uint32_t)index, &actual) || actual != width) {
        ml_fail(c, "%s: the texture in the menu archive is not the sheet of labels it should be", id);
        return NULL;
    }
    g->nTex = 1;
    return g;
}

static void ml_outline(MlGroup *g, int selected) {
    static const uint8_t kOutline[2][3] = {{70, 0, 89}, {44, 21, 27}};
    g->hasOutline = 1;
    memcpy(g->outline, kOutline[selected != 0], 3);
}

/* The cells of a sheet of numbers: `count` of them, `perRow` in a row, each w x 32, saying `first`, `first` + 1, ... */
static void ml_cells(MlGroup *g, int count, int perRow, int w, int first) {
    int n;
    for (n = 0; n < count; n++) {
        snprintf(g->rect[n].text, sizeof(g->rect[n].text), "%d", first + n);
        g->rect[n].x = (n % perRow) * w;
        g->rect[n].y = (n / perRow) * 32;
        g->rect[n].w = w;
        g->rect[n].h = 32;
    }
    g->nRects = count;
}

/* The textures that hold labels, and what the labels say. The numbers are the USA disc's (kMlFiles); the texts are
   what the pictures show, read off them by the contributor. A texture often shares its file with panels and
   backgrounds, so each is named by its number. Most menus have two sheets, the labels as they are and as they are
   while chosen: the second takes its texts from the first (`textFrom`), so that an edited label changes in both. */
static int ml_catalog(MlCtx *c) {
    /* Shared face heights for headings with the same measured letter size. Baselines remain per texture. */
    enum { ML_HEADING_CAP_REPLAY = 20, ML_HEADING_CAP_COMPACT = 21,
           ML_HEADING_CAP_STANDARD = 22, ML_HEADING_CAP_LARGE = 24 };
    typedef struct MlHeading {
        const char *id, *text;
        int file, outer, part, index, width, cap, baseline, margin, art, centered;
    } MlHeading;
    /* The measured face inside each cream border: archive, member, texture file, texture, width, cap, baseline,
       left margin, face width. Password and Replay are centered on their 512-pixel sheets. */
    static const MlHeading kHeadings[] = {
        {"history-heading", "Dragon History", 460, 1, 11, 1, 512, ML_HEADING_CAP_STANDARD, 28, 9, 319, 0},
        {"ultimate-heading", "Ultimate Battle", 451, 1, 4, 1, 512, ML_HEADING_CAP_STANDARD, 29, 9, 318, 0},
        {"sim-heading", "Sim Dragon", 451, 3, 8, 1, 512, ML_HEADING_CAP_STANDARD, 28, 8, 244, 0},
        {"mission-heading", "Mission 100", 451, 2, 8, 1, 512, ML_HEADING_CAP_STANDARD, 29, 9, 249, 0},
        {"survival-heading", "Survival", 451, 2, 9, 1, 512, ML_HEADING_CAP_STANDARD, 29, 8, 169, 0},
        {"fusion-heading", "Disc Fusion", 451, 6, 4, 1, 512, ML_HEADING_CAP_STANDARD, 29, 9, 237, 0},
        {"tour-heading", "Dragon World Tour", 450, 1, 22, 1, 512, ML_HEADING_CAP_STANDARD, 28, 9, 322, 0},
        {"evolution-heading", "Evolution Z", 452, 1, 3, 1, 512, ML_HEADING_CAP_COMPACT, 29, 9, 203, 0},
        {"shop-heading", "Item Shop", 452, 2, 20, 1, 512, ML_HEADING_CAP_LARGE, 30, 8, 190, 0},
        {"map-heading", "Map Select", 448, 2, 55, 1, 512, ML_HEADING_CAP_COMPACT, 29, 9, 230, 0},
        {"bgm-select-heading", "BGM Select", 448, 2, 55, 2, 512, ML_HEADING_CAP_STANDARD, 29, 9, 236, 0},
        {"training-heading", "Ultimate Training", 461, 1, 5, 1, 512, ML_HEADING_CAP_STANDARD, 28, 9, 366, 0},
        {"heading-battle-training", "Battle Training", 461, 1, 5, 3, 512, ML_HEADING_CAP_STANDARD, 28, 9, 310, 0},
        {"data-heading", "Data Center", 459, 1, 9, 1, 512, ML_HEADING_CAP_COMPACT, 29, 9, 250, 0},
        {"password-heading", "Password Input Screen", 459, 3, 2, 6, 512, ML_HEADING_CAP_COMPACT, 32, 8, 332, 1},
        {"fighter-heading", "Fighter Catalog", 459, 2, 4, 4, 256, ML_HEADING_CAP_COMPACT, 29, 11, 192, 0},
        {"replay-heading", "Replay Data", 459, 4, 4, 6, 512, ML_HEADING_CAP_REPLAY, 42, 8, 200, 1},
        {"reference-heading", "Character Reference", 463, 1, 4, 1, 512, ML_HEADING_CAP_LARGE, 29, 8, 345, 0},
        {"options-heading", "Options", 453, 1, 18, 1, 512, ML_HEADING_CAP_COMPACT, 29, 8, 161, 0},
        {"screen-heading", "Screen", 453, 1, 18, 2, 512, ML_HEADING_CAP_COMPACT, 29, 8, 141, 0},
        {"sound-heading", "Sound", 453, 1, 18, 3, 512, ML_HEADING_CAP_STANDARD, 29, 8, 128, 0},
        {"bgm-heading", "BGM Test", 453, 1, 18, 4, 512, ML_HEADING_CAP_COMPACT, 29, 9, 195, 0},
        {"controller-heading", "Controller", 453, 1, 18, 5, 512, ML_HEADING_CAP_STANDARD, 29, 8, 210, 0},
        {"shenron-heading", "Shenron", 472, 2, 5, 1, 512, ML_HEADING_CAP_STANDARD, 29, 8, 173, 0},
    };
    static const char *const kState[2] = {"normal", "selected"};
    const char *kMain = kMlMain;
    static const char kUltimate[] = "Sim Dragon|Mission 100|Survival|Disc Fusion|???";
    static const char kSimDragon[] = "Start|Battle Record|Explanation";
    static const struct { const char *family; int part; const char *labels; } kVersusLists[3] = {
        {"versus-time", 15, "60|90|180|240|\xE2\x88\x9E"}, /* (the last one: the infinity sign) */
        {"versus-com-level", 16, "Very Weak|Weak|Average|Strong|Very Strong"},
        {"versus-referee", 18, "Ox King|Videl|Supreme Kai\xE2\x84\xA2|Shenron|Announcer 1|Announcer 2|Announcer 3"}};
    MlBuf select, main;
    MlGroup *g;
    char id[48], from[48], text[48];
    int i, k, n;

#define ML_ID(...) (snprintf(id, sizeof(id), __VA_ARGS__), id)
#define ML_FROM(on, ...) ((on) ? (snprintf(from, sizeof(from), __VA_ARGS__), from) : NULL)
#define ML_ADD(...) do { g = ml_add(c, __VA_ARGS__); if (g == NULL) { return 0; } } while (0)

    if (!ml_screen(c, 448, 2, &select) || !ml_screen(c, 449, 2, &main)) {
        return 0;
    }
    /* the name strips. Their texts are namefont.c's tables: the names of the stages and of the songs */
    g = ml_strip(c, "stage", 64, select, 6, ML_TEXTURES);
    for (n = 0; g != NULL && n < (int)(sizeof(kStageNames) / sizeof(kStageNames[0])); n++) {
        g->labels[g->nLabels++] = kStageNames[n];
    }
    for (i = 0; g != NULL && i < 2; i++) {
        g = ml_strip(c, i ? "song-highlighted" : "song", 32, select, i ? 3 : 4, 3);
        for (n = 0; g != NULL && n < (int)(sizeof(kSongNames) / sizeof(kSongNames[0])); n++) {
            g->labels[g->nLabels++] = kSongNames[n];
        }
    }
    /* the main menu, as it is and while chosen: its Flash clips use these same rows. The second sheet of each holds
       the PC's item too (in the game's memory, which the textures are found by): it is written into it here as the
       game writes it, and is the twelfth label */
    {
        unsigned char *item[2];
        char withItem[sizeof(kMlMain) + sizeof(kMlItem) + 1];
        int haveItem = ml_item_pics(c, item);

        snprintf(withItem, sizeof(withItem), "%s|%s", kMain, kMlItem);
        for (i = 5; g != NULL && i <= 6; i++) {
            g = ml_strip(c, ML_ID("main-%d", i), 32, main, i, ML_TEXTURES);
            if (g != NULL && !ml_labels(c, g, haveItem && g->nTex == 2 && ml_item_into(c, main, i, &g->tex[1], item[i - 5]) ? withItem : kMain)) {
                g = NULL;
            }
        }
        if (haveItem) {
            free(item[0]);
            free(item[1]);
        }
    }
    if (g == NULL) {
        return 0;
    }

    ML_ADD("main-heading", 449, 2, 23, 1, "Main Menu", 0, NULL, 512, -1);
    g->height = 64; g->hasFont = g->hasArt = g->hasOutline = 1;
    g->fontCap = 25; g->fontBaseline = 30; g->fontArt = 194; g->fontMargin = 9;
    g->outline[0] = 87; g->outline[1] = 22; g->outline[2] = 12;
    ML_ADD("versus-heading", 448, 1, 24, 1, "Versus Menu", 0, NULL, 512, -1);
    g->height = 64; g->hasFont = g->hasArt = g->hasOutline = 1;
    g->fontCap = 21; g->fontBaseline = 29; g->fontArt = 270; g->fontMargin = 7;
    g->outline[0] = 30; g->outline[1] = 8; g->outline[2] = 4;
    for (i = 0; i < (int)(sizeof(kHeadings) / sizeof(kHeadings[0])); i++) {
        const uint8_t outline[3] = {87, 22, 12};
        const MlHeading *heading = &kHeadings[i];
        ML_ADD(heading->id, heading->file, heading->outer, heading->part, heading->index,
               heading->text, heading->centered, NULL, heading->width, -1);
        g->height = 64;
        g->hasFont = g->hasArt = 1;
        g->fontCap = heading->cap; g->fontBaseline = heading->baseline;
        g->fontArt = heading->art; g->fontMargin = heading->margin;
        g->hasOutline = 1;
        memcpy(g->outline, outline, sizeof(outline));
    }
    ML_ADD("battle-class-heading", 461, 1, 8, 0, "Basic|Intermediate|Advanced", 0, NULL, 256, -1);
    g->height = 64;
    g->hasOutline = 1;
    g->outline[0] = 87; g->outline[1] = 22; g->outline[2] = 12;

    ML_ADD("title-normal", 449, 1, 8, 0, "New Game|Continue", 1, NULL, 512, -1);
    ML_ADD("title-selected", 449, 1, 8, 1, "New Game|Continue", 1, "title-normal", 512, -1);
    ML_ADD("title-press-start", 449, 1, 6, 0, "Press START button", 1, NULL, 512, -1);
    /* measured on the inner letters, not on this prompt's bright outer border */
    g->hasFont = g->hasArt = 1;
    g->fontCap = 17;
    g->fontBaseline = 25;
    g->fontArt = 264;
    g->hasOutline = 1;
    g->outline[0] = 63, g->outline[1] = 21, g->outline[2] = 17;
    for (i = 0; i < 4; i++) {
        const char *family = i < 2 ? "versus" : "versus-type";
        ML_ADD(ML_ID("%s-%s", family, kState[i % 2]), 448, 1, 8, i, i < 2 ? "1P VS COM|1P VS 2P|COM VS COM|Battle Settings" : "Single Battle|Team Battle|DP Battle",
               0, ML_FROM(i % 2, "%s-normal", family), 512, -1);
    }
    for (i = 0; i < 4; i++) {
        const char *family = i % 2 == 0 ? "options" : "options-sound";
        ML_ADD(ML_ID("%s-%s", family, kState[i / 2]), 453, 1, 16, i,
               i % 2 == 0 ? "Save/Load|Screen|Sound|Controller|Camera Type|Screen Shake|Screen Adjust|Speaker" : "Volume|BGM Test|Change Voice|Control Type|Vibration",
               0, ML_FROM(i >= 2, "%s-normal", family), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("options-actions-%s", kState[i]), 453, 1, 27, i, "EXIT|Default", 0, ML_FROM(i, "options-actions-normal"), 256, -1);
        ML_ADD(ML_ID("save-confirm-%s", kState[i]), 453, 1, 3, i, "Yes|No", 1, ML_FROM(i, "save-confirm-normal"), 128, 35);
    }
    for (i = 0; i < 4; i++) {
        const char *family = i % 2 == 0 ? "options-values" : "options-controls";
        ML_ADD(ML_ID("%s-%s", family, kState[i / 2]), 453, 1, 14, i, i % 2 == 0 ? "ON|OFF|Save|Load|Stereo|Mono|English|Japanese" : "1P|2P|Normal|Custom|Simple",
               1, ML_FROM(i >= 2, "%s-normal", family), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("history-difficulty-%s", kState[i]), 460, 1, 9, 2 + i, "Easy|Normal|Hard", 1, ML_FROM(i, "history-difficulty-normal"), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("history-action-%s", kState[i]), 460, 1, 9, 4 + i, "Select Scenario|Select Difficulty", 0, ML_FROM(i, "history-action-normal"), 512, -1);
    }
    ML_ADD("ultimate-battle", 451, 1, 3, 7, kUltimate, 1, NULL, 512, -1);
    ML_ADD("sim-dragon-actions", 451, 5, 2, 19, kSimDragon, 0, NULL, 512, -1);
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("evolution-%s", kState[i]), 452, 1, 5, i, "Character Customize|Item Shop|Evolution Z Explanation", 0, ML_FROM(i, "evolution-normal"), 512, -1);
        ML_ADD(ML_ID("customize-menu-%s", kState[i]), 452, 3, 4, i, "Equip Item|Password|Distribute Z Points", 0, ML_FROM(i, "customize-menu-normal"), 256, -1);
        ML_ADD(ML_ID("customize-slots-%s", kState[i]), 452, 3, 4, i + 2, "|||Distribute Z Points", 1, ML_FROM(i, "customize-slots-normal"), 512, -1);
        ML_ADD(ML_ID("item-shop-%s", kState[i]), 452, 2, 3, i, "Item Exchange|Item List", 0, ML_FROM(i, "item-shop-normal"), 512, -1);
        ML_ADD(ML_ID("data-center-%s", kState[i]), 459, 1, 8, i, "Password Input Screen|Replay|Fighter Catalog", 0, ML_FROM(i, "data-center-normal"), 512, -1);
        ML_ADD(ML_ID("character-reference-%s", kState[i]), 463, 1, 8, i, "Character Introduction|Voice Playback|Chi-Chi's Commentary|View Model", 0,
               ML_FROM(i, "character-reference-normal"), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("training-%s", kState[i]), 461, 1, 9, i * 4, "Training|Battle Training", 0, ML_FROM(i, "training-normal"), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("practice-%s", kState[i]), 461, 1, 9, 1 + i * 4, "Single Practice|Team Practice", 0, ML_FROM(i, "practice-normal"), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("training-controller-%s", kState[i]), 461, 1, 9, 3 + i * 4, "Nunchuk Style|Classic Style|GC Controller", 0,
               ML_FROM(i, "training-controller-normal"), 512, -1);
    }
    ML_ADD("ultimate-battle-normal", 451, 1, 3, 6, kUltimate, 1, "ultimate-battle", 512, -1);
    ML_ADD("sim-dragon-actions-normal", 451, 5, 2, 18, kSimDragon, 0, "sim-dragon-actions", 512, -1);
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("world-tour-list-%s", kState[i]), 450, 1, 5, i,
               "World Tour|Free|World Tournament|World Martial Arts Big Tournament|Cell Games|Otherworld Tournament|Yamcha Game", 0,
               ML_FROM(i, "world-tour-list-normal"), 512, -1);
        ML_ADD(ML_ID("world-tour-custom-%s", kState[i]), 450, 2, 13, i, "Normal|Custom 1|Custom 2|Custom 3", 1, ML_FROM(i, "world-tour-custom-normal"), 256, -1);
        ML_ADD(ML_ID("versus-settings-%s", kState[i]), 448, 1, 13, i, "Duel Time|COM Level|Referee|Transformations|Map Setting|Default", 0,
               ML_FROM(i, "versus-settings-normal"), 256, -1);
        ML_ADD(ML_ID("versus-dp-%s", kState[i]), 448, 1, 12, i + 1, "10|15|20", 1, ML_FROM(i, "versus-dp-normal"), 256, -1);
        ml_outline(g, i); /* the narrow "10" has more fill than outline: the outline is this sheet's colour, not the commonest */
        ML_ADD(ML_ID("versus-onoff-%s", kState[i]), 448, 1, 17, i, "ON|OFF", 1, ML_FROM(i, "versus-onoff-normal"), 256, -1);
        for (k = 0; k < 3; k++) {
            ML_ADD(ML_ID("%s-%s", kVersusLists[k].family, kState[i]), 448, 1, kVersusLists[k].part, i, kVersusLists[k].labels, 1,
                   ML_FROM(i, "%s-normal", kVersusLists[k].family), 256, -1);
            ml_outline(g, i); /* (the infinity sign's fill outnumbers its outline too) */
        }
        for (k = 1; k <= 2; k++) {
            snprintf(text, sizeof(text), "%dP:ON|%dP:OFF", k, k);
            ML_ADD(ML_ID("versus-transform-%dp-%s", k, kState[i]), 448, 1, 17, k * 2 + i, text, 1, ML_FROM(i, "versus-transform-%dp-normal", k), 128, -1);
        }
        ML_ADD(ML_ID("item-shop-confirm-%s", kState[i]), 452, 2, 28, i, "Yes|No|Explanation", 1, ML_FROM(i, "item-shop-confirm-normal"), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("battle-training-%s", kState[i]), 461, 1, 9, 2 + i * 4, "Basic|Intermediate|Advanced", 0, ML_FROM(i, "battle-training-normal"), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("team-end-%s", kState[i]), 448, 2, 36, i, "END", 1, ML_FROM(i, "team-end-normal"), 256, -1);
        ML_ADD(ML_ID("sound-digits-%s", kState[i]), 453, 1, 11, i, NULL, 0, NULL, 128, -1);
        ml_cells(g, 10, 4, 32, 0);
        ML_ADD(ML_ID("character-color-%s", kState[i]), 448, 2, 29, i, NULL, 0, NULL, 128, -1);
        ml_cells(g, 4, 2, 64, 1);
        ml_outline(g, i);
    }
    ML_ADD("character-preview-color", 463, 1, 7, 0, NULL, 0, NULL, 128, -1);
    ml_cells(g, 10, 4, 32, 0);
    ML_ADD("song-random", 448, 2, 4, 3, "Random", 0, NULL, 512, -1);
    ML_ADD("song-random-highlighted", 448, 2, 3, 3, "Random", 0, "song-random", 512, -1);
    return 1;
#undef ML_ID
#undef ML_FROM
#undef ML_ADD
}

/* What a label is drawn in: by the state its group is the sheet of. */
static int ml_color_style(const char *group) {
    static const char *const kOrange[] = {"stage", "song-highlighted", "main-6", "ultimate-battle", "sim-dragon-actions",
                                          "character-preview-color", "song-random-highlighted"};
    size_t n = strlen(group), i;
    if (strcmp(group, "main-5") == 0) {
        return ML_MAIN_MENU;
    }
    if (strcmp(group, "main-heading") == 0 || strcmp(group, "versus-heading") == 0 ||
        strcmp(group, "heading-battle-training") == 0 ||
        (n >= 8 && strcmp(group + n - 8, "-heading") == 0)) {
        return ML_MAIN_HEADING;
    }
    if (strcmp(group, "title-press-start") == 0) {
        return ML_TITLE_PROMPT;
    }
    if (n >= 9 && strcmp(group + n - 9, "-selected") == 0) {
        return ML_ORANGE;
    }
    for (i = 0; i < sizeof(kOrange) / sizeof(kOrange[0]); i++) {
        if (strcmp(group, kOrange[i]) == 0) {
            return ML_ORANGE;
        }
    }
    return ML_PURPLE;
}

/* ---- measuring ------------------------------------------------------------------------------------------------- */

/* Letter height and baseline of the long name strips, from the port's own letter cutter: it tells capitals from
   lower case, which a row's pixels alone do not. A strip it cannot cut keeps the measurements of its rows. */
static int ml_measure_strips(MlCtx *c) {
    static const size_t kSheet = (size_t)SHEET_W * SHEET_H * 4;
    unsigned char seen[128];
    int gi, i, letters = 0, ok = 1;

    memset(seen, 0, sizeof(seen));
    for (gi = 0; gi < c->nGroups && ok; gi++) {
        MlGroup *g = &c->groups[gi];
        const char *names[ML_LABELS];
        uint8_t *rows;
        size_t bytes = 0, row = (size_t)g->height * SHEET_W * 4;
        int count = 0, slot = 0, t, y, sheets, cap[7], base[7], caps = 0, ch;

        if (!g->strip || g->nLabels == 0 || g->nTex == 0) {
            continue;
        }
        /* the rows that have a name, one under the other, in sheets of 256 rows as the cutter takes them (the last
           one filled up with nothing) */
        rows = ml_alloc(c, ((size_t)g->nLabels * row + kSheet - 1) / kSheet * kSheet);
        if (rows == NULL) {
            return 0;
        }
        for (t = 0; t < g->nTex; t++) {
            for (y = 0; y < g->tex[t].h; y += g->height, slot++) {
                size_t at = (size_t)y * SHEET_W * 4, have = (size_t)g->tex[t].h * SHEET_W * 4 - at;
                if (slot < g->nLabels && g->labels[slot][0] != '\0') {
                    memcpy(rows + bytes, g->tex[t].rgba + at, have < row ? have : row);
                    bytes += have < row ? have : row;
                    names[count++] = g->labels[slot];
                }
            }
        }
        sheets = (int)((bytes + kSheet - 1) / kSheet);
        if (sheets < 1 || sheets > MAX_SHEETS) {
            continue;
        }
        for (i = 0; i < MAX_SHEETS; i++) {
            free(sSheet[0][i]);
            sSheet[0][i] = NULL;
        }
        for (i = 0; i < 128; i++) {
            free(sStyle[0].g[i].px);
        }
        memset(&sStyle[0], 0, sizeof(sStyle[0]));
        sSheetCount[0] = sheets;
        for (i = 0; i < sheets && ok; i++) {
            sSheet[0][i] = malloc(kSheet);
            if (sSheet[0][i] == NULL) {
                ok = ml_fail(c, "out of memory");
            } else {
                memcpy(sSheet[0][i], rows + (size_t)i * kSheet, kSheet);
            }
        }
        if (!ok || !style_build(0, names, count, g->height)) {
            continue;
        }
        for (ch = 33; ch < 127; ch++) {
            const NfGlyph *l = &sStyle[0].g[ch];
            if (l->px == NULL) {
                continue;
            }
            letters += !seen[ch];
            seen[ch] = 1;
            if (strchr("HETILMN", ch) != NULL) { /* capitals with a flat top and bottom */
                base[caps] = l->sy1 + 1;
                cap[caps++] = l->sy1 - l->sy0 + 1;
            }
        }
        if (caps > 0) {
            qsort(base, (size_t)caps, sizeof(int), cmp_int);
            qsort(cap, (size_t)caps, sizeof(int), cmp_int);
            g->hasFont = 1;
            g->fontBaseline = base[caps / 2];
            g->fontCap = cap[caps / 2];
        }
    }
    for (i = 0; i < MAX_SHEETS; i++) {
        free(sSheet[0][i]);
        sSheet[0][i] = NULL;
    }
    for (i = 0; i < 128; i++) {
        free(sStyle[0].g[i].px);
    }
    memset(&sStyle[0], 0, sizeof(sStyle[0]));
    sSheetCount[0] = 0;
    if (ok && letters < 20) {
        return ml_fail(c, "the lettering of the menus was not recognised");
    }
    return ok;
}

/* A row's letters from its pixels (`count` of them, `width` in a line): the outline is the commonest opaque colour
   unless one is given, the fill what is clearly brighter. The top of the fill is the top of the capitals (thin
   strokes count), the baseline is under the last line that is well filled. Returns 0 if the row has no letters. */
static int ml_row_style(const uint8_t *px, int count, int width, int centered, const uint8_t *outline, MlRow *row) {
    struct { uint32_t rgb; int n; } seen[256];
    int lines[512], nSeen = 0, last = 0, i, best = -1, sum, x0 = width, x1 = -1, y0 = 512, y1 = -1, most = 0, any = 0;
    uint8_t common[3];

    for (i = 0; i < count; i++) {
        const uint8_t *p = px + (size_t)i * 4;
        uint32_t rgb = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16;
        int k;
        if (p[3] < 120) {
            continue;
        }
        any = 1;
        if (nSeen > 0 && seen[last].rgb == rgb) {
            seen[last].n++;
            continue;
        }
        for (k = 0; k < nSeen && seen[k].rgb != rgb; k++) {
        }
        if (k == nSeen) {
            if (nSeen == 256) { /* (a texture with a colour table has no more) */
                return 0;
            }
            seen[nSeen].rgb = rgb;
            seen[nSeen++].n = 0;
        }
        seen[k].n++;
        last = k;
    }
    if (!any || count > 512 * width) {
        return 0;
    }
    for (i = 0; i < nSeen; i++) { /* (of two equally common colours, the one met first) */
        if (best < 0 || seen[i].n > seen[best].n) {
            best = i;
        }
    }
    if (outline == NULL) {
        common[0] = (uint8_t)seen[best].rgb;
        common[1] = (uint8_t)(seen[best].rgb >> 8);
        common[2] = (uint8_t)(seen[best].rgb >> 16);
        outline = common;
    }
    sum = outline[0] + outline[1] + outline[2];
    memset(lines, 0, sizeof(lines));
    for (i = 0; i < count; i++) {
        const uint8_t *p = px + (size_t)i * 4;
        int x = i % width, y = i / width;
        if (p[3] < 100 || p[0] + p[1] + p[2] - sum <= 100) {
            continue;
        }
        lines[y]++;
        if (x < x0) { x0 = x; }
        if (x > x1) { x1 = x; }
        if (y < y0) { y0 = y; }
    }
    if (x1 < 0) {
        return 0;
    }
    for (i = 0; i < 512; i++) {
        if (lines[i] > most) { most = lines[i]; }
    }
    for (i = 0; i < 512; i++) {
        if (lines[i] > 0 && (double)lines[i] >= most * 0.35) { y1 = i; }
    }
    row->cap = y1 - y0 + 1;
    row->baseline = y1 + 1;
    row->center = centered;
    row->margin = centered ? 8 : x0;
    row->art = x1 - x0 + 1;
    return 1;
}

static const char *ml_family(const MlRow *r, char *out, size_t n) {
    snprintf(out, n, "%.*s", (int)strcspn(r->id, "/"), r->id);
    return out;
}

static int ml_starts(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* The row of that name: the last one, should two have it. NULL: none. */
static MlRow *ml_row(MlCtx *c, const char *id) {
    int i;
    for (i = c->nRows - 1; i >= 0 && strcmp(c->rows[i].id, id) != 0; i--) {
    }
    return i >= 0 ? &c->rows[i] : NULL;
}

/* The rows of one list (the group `family` if given, and every row whose name starts with `prefix`) get the
   largest letter height and the lowest baseline measured in it. */
static void ml_even(MlCtx *c, const char *family, const char *prefix) {
    char of[64];
    int i, pass, cap = 0, base = 0;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < c->nRows; i++) {
            MlRow *r = &c->rows[i];
            if (!(family != NULL && strcmp(ml_family(r, of, sizeof(of)), family) == 0) && !ml_starts(r->id, prefix)) {
                continue;
            }
            if (pass == 0) {
                cap = r->cap > cap ? r->cap : cap;
                base = r->baseline > base ? r->baseline : base;
            } else {
                r->cap = cap;
                r->baseline = base;
            }
        }
    }
}

/* The rows of the list: every label of every group, measured, then evened out where a menu's labels are one size. */
static int ml_rows(MlCtx *c) {
    static const char *const kAudio[] = {"song", "song-highlighted", "song-random", "song-random-highlighted", "options-sound-normal", "options-sound-selected"};
    /* These lists are set in one size; measured word by word, a word without a capital gives the height of its
       lower case. Each list gets the largest of its measurements, in both states. */
    static const char *const kUniform[] = {"history-action", "ultimate-battle", "sim-dragon-actions", "world-tour-list", "world-tour-custom", "versus-settings",
                                           "versus-dp", "versus-onoff", "versus-time", "versus-com-level", "versus-referee", "versus-transform", "save-confirm",
                                           "item-shop-confirm", "battle-training", "sound-digits", "character-color", "character-reference",
                                           "character-preview-color", "customize-menu", "customize-slots"};
    char family[64], prefix[64];
    int gi, i, k, n, t, y, cap = 0, margin = 1 << 30;

    for (gi = 0; gi < c->nGroups; gi++) {
        const MlGroup *g = &c->groups[gi];
        int label = 0;
        if (g->nRects > 0) {
            const MlTex *tex = &g->tex[0];
            uint8_t *cell = ml_alloc(c, (size_t)g->rect[0].w * g->rect[0].h * 4);
            if (cell == NULL) {
                return 0;
            }
            for (n = 0; n < g->nRects; n++) {
                const MlRect *rc = &g->rect[n];
                MlRow *r = &c->rows[c->nRows];
                int got = 0;
                if (rc->x + rc->w > tex->w || c->nRows == ML_ROWS * 2) {
                    return ml_fail(c, "%s: the sheet is narrower than its cells, or there are too many labels", g->id);
                }
                for (y = rc->y; y < rc->y + rc->h && y < tex->h; y++, got++) {
                    memcpy(cell + (size_t)got * rc->w * 4, tex->rgba + ((size_t)y * tex->w + rc->x) * 4, (size_t)rc->w * 4);
                }
                memset(r, 0, sizeof(*r));
                if (!ml_row_style(cell, got * rc->w, rc->w, 1, g->hasOutline ? g->outline : NULL, r)) {
                    continue;
                }
                snprintf(r->id, sizeof(r->id), "%s/0/%d", g->id, n);
                r->isRect = 1;
                r->hash = tex->hash;
                r->texW = g->width;
                r->texH = tex->h;
                r->x = rc->x;
                r->y = rc->y;
                r->w = rc->w;
                r->text = rc->text;
                r->height = rc->h;
                r->margin = 0;
                c->nRows++;
            }
            continue;
        }
        for (t = 0; t < g->nTex; t++) {
            const MlTex *tex = &g->tex[t];
            for (y = 0; y < tex->h; y += g->height, label++) {
                const char *text = label < g->nLabels ? g->labels[label] : "";
                MlRow *r = &c->rows[c->nRows];
                int lines = tex->h - y < g->height ? tex->h - y : g->height;
                if (text[0] == '\0') {
                    continue;
                }
                if (c->nRows == ML_ROWS * 2) {
                    return ml_fail(c, "more labels than there is room for");
                }
                memset(r, 0, sizeof(*r));
                if (!ml_row_style(tex->rgba + (size_t)y * tex->w * 4, lines * tex->w, tex->w, g->strip ? strcmp(g->id, "stage") == 0 : g->centered,
                                  g->hasOutline ? g->outline : NULL, r)) {
                    continue;
                }
                if (g->hasFont) {
                    r->cap = g->fontCap;
                    r->baseline = g->fontBaseline;
                    if (g->fontMargin) { r->margin = g->fontMargin; }
                }
                if (g->hasArt) {
                    r->art = g->fontArt;
                }
                snprintf(r->id, sizeof(r->id), "%s/%d/%d", g->id, t, y / g->height);
                r->hash = tex->hash;
                r->texW = g->width;
                r->texH = tex->h;
                r->y = y;
                r->w = g->width;
                r->text = text;
                r->height = g->height;
                if (strcmp(g->id, "main-5") == 0) {
                    snprintf(r->textFrom, sizeof(r->textFrom), "main-6/%d/%d", t, y / g->height);
                } else if (g->textFrom[0] != '\0') {
                    snprintf(r->textFrom, sizeof(r->textFrom), "%s/%d/%d", g->textFrom, t, y / g->height);
                }
                c->nRows++;
            }
        }
    }
    /* "Random" in the song list is set like the songs' names */
    for (i = 0; i < c->nRows; i++) {
        MlRow *r = &c->rows[i];
        if (ml_starts(r->id, "song-random")) {
            const char *like = strstr(r->id, "highlighted") != NULL ? "song-highlighted/0/0" : "song/0/0";
            for (k = 0; k < c->nRows && strcmp(c->rows[k].id, like) != 0; k++) {
            }
            if (k < c->nRows) {
                r->cap = c->rows[k].cap;
                r->baseline = c->rows[k].baseline;
                r->margin = c->rows[k].margin;
            }
        }
    }
    for (i = 0; i < c->nRows; i++) {
        ml_family(&c->rows[i], family, sizeof(family));
        for (k = 0; k < (int)(sizeof(kAudio) / sizeof(kAudio[0])); k++) {
            if (strcmp(family, kAudio[k]) == 0) {
                c->rows[i].center = 0;
                c->rows[i].margin = 4;
            }
        }
    }
    for (k = 0; k < (int)(sizeof(kUniform) / sizeof(kUniform[0])); k++) {
        snprintf(prefix, sizeof(prefix), "%s-", kUniform[k]);
        ml_even(c, kUniform[k], prefix);
    }
    for (i = 0; i < c->nRows; i++) {
        if (ml_starts(c->rows[i].id, "versus-settings-") && c->rows[i].y == 160) {
            c->rows[i].center = 1; /* "Default" is in the middle; the five settings above it start at the left */
            c->rows[i].margin = 8;
        }
    }
    for (k = 0; k < 2; k++) { /* the two actions of Dragon History: one height, one left edge */
        for (i = 0; i < c->nRows; i++) {
            MlRow *r = &c->rows[i];
            if (!ml_starts(r->id, "history-action-")) {
                continue;
            }
            if (k == 0) {
                cap = r->cap > cap ? r->cap : cap;
                margin = r->margin < margin ? r->margin : margin;
            } else {
                r->cap = cap;
                r->center = 0;
                r->margin = margin;
            }
        }
    }
    ml_even(c, NULL, "sound-digits-normal/");
    ml_even(c, NULL, "sound-digits-selected/");
    for (i = 0; i < c->nRows; i++) {
        MlRow *r = &c->rows[i];
        if (ml_starts(r->id, "battle-class-heading/0/")) {
            /* Match each finished cream rim to the source title; the long middle row uses its own crop. */
            static const struct { int cap, baseline, margin, art, x, width; } kClassTitle[3] = {
                {20, 42, 23, 87, 0, 256}, {20, 42, 6, 232, 16, 240}, {20, 42, 21, 164, 0, 256}};
            int index = r->y / 64;
            if (index < 3) {
                r->cap = kClassTitle[index].cap;
                r->baseline = kClassTitle[index].baseline;
                r->margin = kClassTitle[index].margin;
                r->art = kClassTitle[index].art;
                r->x = kClassTitle[index].x;
                r->w = kClassTitle[index].width;
            }
        }
        if (strcmp(r->id, "password-heading/0/0") == 0) {
            r->x = 3; r->w = 509;
        }
        if (strcmp(r->id, "replay-heading/0/0") == 0) {
            r->x = 9; r->w = 503;
        }
        r->colors = ml_color_style(ml_family(r, family, sizeof(family)));
        r->italic = ml_starts(r->id, "title-press-start/");
    }
    return 1;
}

/* ---- writing --------------------------------------------------------------------------------------------------- */

typedef struct MlText {
    char *p;
    size_t n, cap;
    int bad;
} MlText;

static void ml_put(MlText *t, const char *fmt, ...) {
    va_list ap;
    int n;

    if (t->p == NULL && !t->bad) {
        t->cap = 65536;
        t->p = malloc(t->cap);
        t->bad = t->p == NULL;
    }
    for (;;) {
        if (t->bad) {
            return;
        }
        va_start(ap, fmt);
        n = vsnprintf(t->p + t->n, t->cap - t->n, fmt, ap);
        va_end(ap);
        if (n >= 0 && (size_t)n < t->cap - t->n) {
            t->n += (size_t)n;
            return;
        }
        if (n < 0) {
            t->bad = 1;
            return;
        } else {
            size_t cap = (t->cap + (size_t)n + 1) * 2;
            char *more = realloc(t->p, cap);
            if (more == NULL) {
                t->bad = 1;
                return;
            }
            t->p = more;
            t->cap = cap;
        }
    }
}

static const char *ml_hex(const uint8_t *rgb, char *out) {
    snprintf(out, 8, "%02x%02x%02x", rgb[0], rgb[1], rgb[2]);
    return out;
}

/* The checks port/tools/menu_font.py makes of a row before it goes into labels.tsv (and menu_text.c makes of a
   line it reads). A row the measuring got wrong stops the list: nothing is written. */
static int ml_row_ok(const MlRow *r) {
    return r->texW >= 8 && r->texW <= 512 && (r->texW & (r->texW - 1)) == 0 && r->x >= 0 && r->x < r->texW && r->w >= 1 && r->w <= r->texW - r->x &&
           r->texH >= 1 && r->texH <= 512 && r->height >= 1 && r->height <= 64 && r->y >= 0 && r->y <= r->texH - r->height && r->cap >= 1 &&
           r->cap <= 64 && r->baseline >= 0 && r->baseline <= r->height && r->margin >= 0 && r->margin * 2 < r->w && r->art >= 0 && r->art <= r->w;
}

/* labels.tsv and name-styles.tsv: what menu_text.c reads. */
static int ml_tsv(MlCtx *c, MlText *labels, MlText *styles, int *count) {
    static const char *const kLike[3] = {"song/0/0", "song-highlighted/0/0", "stage/0/0"};
    char top[8], bottom[8], outline[8], outer[8], outlineBottom[8], bevelLight[8], bevelDark[8];
    int i;

    ml_put(labels, "# " MENU_LABELS_VERSION " hash width height y row_height cap_height baseline center margin width_milli x row_width art_width top bottom "
                   "outline italic outline_milli outer_milli outer_rgb shadow outline_bottom bevel_milli bevel_light bevel_dark text\n");
    /* the names of added songs and stages are drawn like the disc's (ui.cpp) */
    ml_put(styles, "# kind (0 song, 1 selected song, 2 stage) cap_height baseline margin width_milli top bottom outline\n");
    for (i = 0; i < 3; i++) {
        int k;
        for (k = 0; k < c->nRows && strcmp(c->rows[k].id, kLike[i]) != 0; k++) {
        }
        if (k < c->nRows) {
            const MlRow *r = &c->rows[k];
            const MlColors *col = &kMlColors[r->colors];
            ml_put(styles, "%d\t%d\t%d\t%d\t1000\t%s\t%s\t%s\n", i, r->cap, r->baseline, r->margin, ml_hex(col->top, top), ml_hex(col->bottom, bottom),
                   ml_hex(col->outline, outline));
        }
    }
    *count = 0;
    for (i = 0; i < c->nRows; i++) {
        const MlRow *r = &c->rows[i], *from = r->textFrom[0] != '\0' ? ml_row(c, r->textFrom) : NULL;
        const MlColors *col = &kMlColors[r->colors];
        const char *text = from != NULL ? from->text : r->text;
        if (!ml_row_ok(r)) {
            return ml_fail(c, "%s: the label was measured outside its texture", r->id);
        }
        if (text[0] == '\0') {
            continue;
        }
        if (strpbrk(text, "\t\r\n") != NULL || strlen(text) > 255 || text[strspn(text, " ")] == '\0') {
            return ml_fail(c, "%s: the catalogue's text cannot be a label", r->id);
        }
        /* (the width of the artwork is aimed at only by the label that artwork shows) */
        ml_put(labels, "%016llx\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t1000\t%d\t%d\t%d\t%s\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%s\t%d\t%s\t%s\t%s\n", (unsigned long long)r->hash,
               r->texW, r->texH, r->y, r->height, r->cap, r->baseline, r->center, r->margin, r->x, r->w, strcmp(text, r->text) == 0 ? r->art : 0,
               ml_hex(col->top, top), ml_hex(col->bottom, bottom), ml_hex(col->outline, outline), r->italic,
               col->outlineWidth < 0 ? -1 : col->outlineWidth * 1000, col->outerWidth * 1000, ml_hex(col->outerColor, outer), col->shadow,
               ml_hex(r->colors == ML_MAIN_HEADING ? kHeadingOutlineBottom : col->outline, outlineBottom),
               r->colors == ML_MAIN_HEADING ? 1000 : 0,
               ml_hex(r->colors == ML_MAIN_HEADING ? kHeadingBevelLight : col->top, bevelLight),
               ml_hex(r->colors == ML_MAIN_HEADING ? kHeadingBevelDark : col->outline, bevelDark), text);
        ++*count;
    }
    if (*count > ML_ROWS) {
        return ml_fail(c, "more labels than the renderer holds");
    }
    return !labels->bad && !styles->bad ? 1 : ml_fail(c, "out of memory");
}

/* A text as JSON writes it with nothing but ASCII. */
static void ml_json_text(MlText *t, const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    ml_put(t, "\"");
    while (*p != '\0') {
        unsigned ch = *p++;
        int more = ch >= 0xF0 ? 3 : ch >= 0xE0 ? 2 : ch >= 0xC0 ? 1 : 0;
        if (more > 0) {
            ch &= 0x3Fu >> more;
            for (; more > 0 && (*p & 0xC0) == 0x80; more--) {
                ch = ch << 6 | (*p++ & 0x3F);
            }
        }
        if (ch == '"' || ch == '\\') {
            ml_put(t, "\\%c", (int)ch);
        } else if (ch >= 0x10000) {
            ml_put(t, "\\u%04x\\u%04x", 0xD800 + ((ch - 0x10000) >> 10), 0xDC00 + ((ch - 0x10000) & 0x3FF));
        } else if (ch < 0x20 || ch >= 0x7F) {
            ml_put(t, "\\u%04x", ch);
        } else {
            ml_put(t, "%c", (int)ch);
        }
    }
    ml_put(t, "\"");
}

static void ml_json_rgb(MlText *t, const char *key, const uint8_t *rgb, const char *end) {
    ml_put(t, "      \"%s\": [\n        %d,\n        %d,\n        %d\n      ]%s\n", key, rgb[0], rgb[1], rgb[2], end);
}

/* labels.json: the same list in the form a person edits (docs/port/menu_font.md), laid out as Python's json module
   writes it with an indent of 2, which is what wrote this file before and what reads it (port/tools/menu_font.py). */
static int ml_json(MlCtx *c, MlText *t) {
    int i;

    ml_put(t, "{\n  \"color_styles\": {\n");
    for (i = 0; i < (int)(sizeof(kMlColors) / sizeof(kMlColors[0])); i++) {
        const MlColors *col = &kMlColors[i];
        int more = col->outlineWidth >= 0 || col->outer || !col->shadow;
        ml_put(t, "    \"%s\": {\n", col->name);
        ml_json_rgb(t, "top", col->top, ",");
        ml_json_rgb(t, "bottom", col->bottom, ",");
        ml_json_rgb(t, "outline", col->outline, more ? "," : "");
        if (more) {
            ml_put(t, "      \"outline_width\": %d,\n", col->outlineWidth);
            ml_json_rgb(t, "outer_outline", col->outerColor, ",");
            ml_put(t, "      \"outer_outline_width\": %d,\n", col->outerWidth);
            if (i == ML_MAIN_HEADING) {
                ml_json_rgb(t, "outline_bottom", kHeadingOutlineBottom, ",");
                ml_put(t, "      \"bevel_width\": 1,\n");
                ml_json_rgb(t, "bevel_light", kHeadingBevelLight, ",");
                ml_json_rgb(t, "bevel_dark", kHeadingBevelDark, ",");
            }
            ml_put(t, "      \"shadow\": %s\n", col->shadow ? "true" : "false");
        }
        ml_put(t, "    }%s\n", i + 1 < (int)(sizeof(kMlColors) / sizeof(kMlColors[0])) ? "," : "");
    }
    ml_put(t, "  },\n  \"labels\": [\n");
    for (i = 0; i < c->nRows; i++) {
        const MlRow *r = &c->rows[i];
        ml_put(t, "    {\n      \"id\": \"%s\",\n      \"hash\": \"%016llx\",\n", r->id, (unsigned long long)r->hash);
        if (r->isRect || r->x != 0 || r->w != r->texW) {
            ml_put(t, "      \"texture_width\": %d,\n      \"texture_height\": %d,\n      \"x\": %d,\n      \"y\": %d,\n      \"width\": %d,\n", r->texW, r->texH,
                   r->x, r->y, r->w);
        } else {
            ml_put(t, "      \"texture_height\": %d,\n      \"y\": %d,\n", r->texH, r->y);
        }
        ml_put(t, "      \"original\": ");
        ml_json_text(t, r->text);
        ml_put(t, ",\n      \"text\": ");
        ml_json_text(t, r->text);
        ml_put(t, ",\n      \"enabled\": true,\n      \"height\": %d,\n      \"cap_height\": %d,\n      \"baseline\": %d,\n      \"align\": \"%s\",\n"
                  "      \"margin\": %d,\n      \"art_width\": %d,\n",
               r->height, r->cap, r->baseline, r->center ? "center" : "left", r->margin, r->art);
        if (!r->isRect && r->x == 0 && r->w == r->texW && r->texW != 512) {
            ml_put(t, "      \"texture_width\": %d,\n", r->texW);
        }
        if (r->textFrom[0] != '\0') {
            ml_put(t, "      \"text_from\": \"%s\",\n", r->textFrom);
        }
        ml_put(t, "      \"color_style\": \"%s\"%s\n    }%s\n", kMlColors[r->colors].name, r->italic ? ",\n      \"italic\": true" : "", i + 1 < c->nRows ? "," : "");
    }
    ml_put(t, "  ]\n}\n");
    return !t->bad ? 1 : ml_fail(c, "out of memory");
}

/* A file of `dir`, whole or not at all: written under another name and given its own when it is complete. (With
   the process number: two copies of the game started together each write their own.) */
static int ml_write(MlCtx *c, const char *dir, const char *name, const MlText *t) {
    char path[1200], tmp[1300];
    FILE *fp;
    int ok;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    snprintf(tmp, sizeof(tmp), "%s.%ld.part", path, (long)getpid());
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        return ml_fail(c, "%s cannot be written to", dir);
    }
    ok = fwrite(t->p, 1, t->n, fp) == t->n;
    ok = fclose(fp) == 0 && ok;
#ifdef _WIN32
    ok = ok && MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING); /* (rename does not replace a file on Windows) */
#else
    ok = ok && rename(tmp, path) == 0;
#endif
    if (!ok) {
        remove(tmp);
        return ml_fail(c, "%s cannot be written to", dir);
    }
    return 1;
}

/* ---- the disc -------------------------------------------------------------------------------------------------- */

static uint32_t ml_crc32(const uint8_t *p, size_t n) {
    static uint32_t table[256];
    static int made;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    if (!made) {
        uint32_t k, v;
        int b;
        for (k = 0; k < 256; k++) {
            for (v = k, b = 0; b < 8; b++) {
                v = (v >> 1) ^ ((v & 1) ? 0xEDB88320u : 0);
            }
            table[k] = v;
        }
        made = 1;
    }
    for (i = 0; i < n; i++) {
        crc = table[(crc ^ p[i]) & 255] ^ (crc >> 8);
    }
    return ~crc;
}

/* Reads the eleven archives. Returns MENU_LABELS_MADE when they are all there and (unless `anyDisc`) the USA disc's. */
/* Reads archive i of kMlFiles and checks it is the USA release's. */
static int ml_read_one(MlCtx *c, const char *dataDir, int i, int anyDisc) {
    char path[1200];
    struct stat st;
    FILE *fp;
    uint8_t *data;
    long size;

    snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", dataDir, kMlFiles[i].number);
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || (fp = fopen(path, "rb")) == NULL) {
        ml_fail(c, "%s is missing (the game data of the USA release, SLUS-21678, is needed)", path);
        return MENU_LABELS_OTHER_DISC;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0 || size > 64 * 1024 * 1024 ||
        (!anyDisc && (uint32_t)size != kMlFiles[i].size)) {
        fclose(fp);
        ml_fail(c, "%s is not the file of the USA release (SLUS-21678): its menu textures are numbered differently", path);
        return MENU_LABELS_OTHER_DISC;
    }
    data = ml_alloc(c, (size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, fp) != (size_t)size) {
        fclose(fp);
        ml_fail(c, "%s cannot be read", path);
        return MENU_LABELS_FAILED;
    }
    fclose(fp);
    if (!anyDisc && ml_crc32(data, (size_t)size) != kMlFiles[i].crc) {
        ml_fail(c, "%s is not the file of the USA release (SLUS-21678): its menu textures are numbered differently", path);
        return MENU_LABELS_OTHER_DISC;
    }
    c->file[i].p = data;
    c->file[i].n = (size_t)size;
    return MENU_LABELS_MADE;
}

static int ml_read_disc(MlCtx *c, const char *dataDir, int anyDisc) {
    int i, r;

    for (i = 0; i < ML_FILES; i++) {
        if ((r = ml_read_one(c, dataDir, i, anyDisc)) != MENU_LABELS_MADE) {
            return r;
        }
    }
    return MENU_LABELS_MADE;
}

int MenuLabels_Make(const char *dataDir, const char *outDir, int anyDisc, char *why, size_t whyLen) {
    MlCtx *c = calloc(1, sizeof(*c));
    MlText labels = {NULL, 0, 0, 0}, styles = {NULL, 0, 0, 0}, json = {NULL, 0, 0, 0};
    char path[1200];
    struct stat st;
    int result, count = 0;

    if (c == NULL) {
        snprintf(why, whyLen, "out of memory");
        return MENU_LABELS_FAILED;
    }
    pthread_mutex_lock(&sMlCutter);
    result = ml_read_disc(c, dataDir, anyDisc);
    if (result == MENU_LABELS_MADE) {
        result = MENU_LABELS_FAILED;
        c->groups = ml_alloc(c, ML_GROUPS * sizeof(*c->groups));
        c->rows = ml_alloc(c, ML_ROWS * 2 * sizeof(*c->rows));
        if (c->groups != NULL && c->rows != NULL && ml_catalog(c) && ml_measure_strips(c) && ml_rows(c) && ml_tsv(c, &labels, &styles, &count) &&
            ml_json(c, &json)) {
#ifdef _WIN32
            _mkdir(outDir);
#else
            mkdir(outDir, 0755);
#endif
            /* labels.json is the player's to edit: one that is there stays. labels.tsv goes last: it is the file
               whose being there says the list is made. */
            snprintf(path, sizeof(path), "%s/labels.json", outDir);
            if ((stat(path, &st) == 0 || ml_write(c, outDir, "labels.json", &json)) && ml_write(c, outDir, "name-styles.tsv", &styles) &&
                ml_write(c, outDir, "labels.tsv", &labels)) {
                result = MENU_LABELS_MADE;
            }
        }
    }
    if (result == MENU_LABELS_MADE) {
        snprintf(why, whyLen, "%d labels", count);
    } else {
        snprintf(why, whyLen, "%s", c->why[0] != '\0' ? c->why : "the menu archives could not be read");
    }
    free(labels.p);
    free(styles.p);
    free(json.p);
    ml_free(c);
    free(c);
    pthread_mutex_unlock(&sMlCutter);
    return result;
}

/* ---- the PC's item of the main menu (src/menu/menu_a.c) ---------------------------------------------------------- */

/* The cutter's sheets and letters, all three styles, given back. (with sMlCutter held) */
static void ml_cutter_clear(void) {
    int s, i;
    for (s = 0; s < NF_STYLES; s++) {
        for (i = 0; i < MAX_SHEETS; i++) {
            free(sSheet[s][i]);
            sSheet[s][i] = NULL;
        }
        for (i = 0; i < 128; i++) {
            free(sStyle[s].g[i].px);
        }
        memset(&sStyle[s], 0, sizeof(sStyle[s]));
        sSheetCount[s] = 0;
    }
    sState = 0;
}

/* The 512-wide textures of texture file `part` of a screen into the cutter's sheets of style `s` (a shorter one
   filled up with nothing below). The number of sheets. */
static int ml_cutter_sheets(MlCtx *c, MlBuf screen, int part, int s, int want) {
    MlBuf d;
    uint32_t count, i;
    int n = 0;

    if (!ml_section(c, screen, (uint32_t)part, &d) || !ml_get32(d, 0, &count)) {
        return 0;
    }
    for (i = 0; i < count && n < want; i++) {
        MlTex t;
        int width;
        if (!ml_entry_width(d, i, &width) || width != SHEET_W || ml_texture(c, d, i, &t) <= 0 || t.h > SHEET_H) {
            continue;
        }
        if ((sSheet[s][n] = (uint8_t *)calloc((size_t)SHEET_W * SHEET_H, 4)) == NULL) {
            return 0;
        }
        memcpy(sSheet[s][n++], t.rgba, (size_t)SHEET_W * t.h * 4);
    }
    sSheetCount[s] = n;
    return n;
}

/* The label's two pictures (MenuLabels_MainItem), with sMlCutter held and archives 448 and 449 read into `c`. */
static int ml_item_pics(MlCtx *c, unsigned char *out[2]) {
    const char *names[ML_LABELS];
    char split[sizeof(kMlMain)], *p;
    MlBuf select, main;
    int count = 0, k, w, h, ok = 0;

    out[0] = out[1] = NULL;
    memcpy(split, kMlMain, sizeof(split));
    for (p = split; count < ML_LABELS; p++) { /* the main menu's texts, one by one */
        names[count++] = p;
        if ((p = strchr(p, '|')) == NULL) {
            break;
        }
        *p = '\0';
    }
    ml_cutter_clear();
    /* the stage select's names lend the letters the main menu's labels do not have (the k), as namefont.c does */
    if (ml_screen(c, 448, 2, &select) && ml_screen(c, 449, 2, &main) && ml_cutter_sheets(c, select, 6, NF_STAGE, 9) == 9 &&
        ml_cutter_sheets(c, main, 5, NF_SONG, 2) == 2 && ml_cutter_sheets(c, main, 6, NF_SONG_LIT, 2) == 2 &&
        style_build(NF_STAGE, kStageNames, (int)(sizeof(kStageNames) / sizeof(kStageNames[0])), 64) &&
        style_build(NF_SONG, names, count, 32) && style_build(NF_SONG_LIT, names, count, 32)) {
        style_lend(&sStyle[NF_SONG], &sStyle[NF_STAGE]);
        style_lend(&sStyle[NF_SONG_LIT], &sStyle[NF_STAGE]);
        sState = 3;
        for (k = 0, ok = 1; k < 2 && ok; k++) {
            out[k] = MenuLabels_NameCompose_(NF_SONG + k, kMlItem, &w, &h);
            ok = out[k] != NULL && w == SHEET_W && h == 32;
        }
    }
    if (!ok) {
        free(out[0]);
        free(out[1]);
        out[0] = out[1] = NULL;
    }
    ml_cutter_clear();
    return ok;
}

int MenuLabels_MainItem(const char *dataDir, unsigned char *out[2]) {
    MlCtx *c = calloc(1, sizeof(*c));
    int ok = 0;

    out[0] = out[1] = NULL;
    if (c == NULL) {
        return 0;
    }
    pthread_mutex_lock(&sMlCutter);
    if (ml_read_one(c, dataDir, ml_file(448), 0) == MENU_LABELS_MADE && ml_read_one(c, dataDir, ml_file(449), 0) == MENU_LABELS_MADE) {
        ok = ml_item_pics(c, out);
    }
    if (!ok) {
        fprintf(stderr, "bt3: main menu: the lettering of the PC's item was not made from the disc (%s)\n",
                c->why[0] != '\0' ? c->why : "its letters were not recognised");
    }
    pthread_mutex_unlock(&sMlCutter);
    ml_free(c);
    free(c);
    return ok;
}

void MenuLabels_ItemPixels(const unsigned char *pic, const unsigned char *table, unsigned char *out) {
    int i, j;

    for (i = 0; i < SHEET_W * 32; i++) {
        const unsigned char *p = pic + i * 4;
        long pa = ((long)p[3] * 128 + 127) / 255; /* (the picture's alpha is 0..255, the table's the PS2's, 0x80 = opaque) */
        int best = 0;
        long bestD = -1;
        for (j = 0; j < 256; j++) { /* the nearest of the table's colours, alpha first (the transparent ones match by it alone) */
            const unsigned char *q = table + j * 4;
            long da = pa - q[3], d = da * da * 16;
            if (pa > 0 && q[3] > 0) {
                long dr = (long)p[0] - q[0], dg = (long)p[1] - q[1], db = (long)p[2] - q[2];
                d += (dr * dr + dg * dg + db * db) * (pa < q[3] ? pa : q[3]) / 128;
            }
            if (bestD < 0 || d < bestD) {
                bestD = d;
                best = j;
            }
        }
        out[i] = (unsigned char)((best & 0xE7) | ((best & 8) << 1) | ((best & 0x10) >> 1)); /* (the table's order: 8..15 and 16..23 of every 32 change places) */
    }
}

/* The colour table of texture `index` of a texture file, in the order it is in memory (256 colours of 4 bytes). */
static int ml_table(MlBuf d, uint32_t index, uint8_t table[256 * 4]) {
    uint32_t at, palAt;
    uint64_t e, palette;

    if (!ml_get32(d, 4, &at)) {
        return 0;
    }
    e = (uint64_t)at * 4 + (uint64_t)index * 64;
    if (e > d.n || d.n - e < 64 || !ml_get32(d, e + 4, &palAt)) {
        return 0;
    }
    palette = (uint64_t)palAt * 4 + 96;
    if (palette > d.n || d.n - palette < 256 * 4) {
        return 0;
    }
    memcpy(table, d.p + palette, 256 * 4);
    return 1;
}

/* The second sheet of the main menu's labels (state `lit`) as the game holds it: with the PC's item written into its
   free fourth row (src/menu/menu_a.c, Port_MainItemLabel), each pixel the colour of the value written; and the hash
   menu_text.c finds the sheet by, again. 0: not the sheet expected (nothing changed). */
static int ml_item_into(MlCtx *c, MlBuf screen, int part, MlTex *sheet, const unsigned char *pic) {
    static uint8_t table[256 * 4], index[SHEET_W * 32];
    MlBuf d;
    uint32_t count, i, seen = 0, at = 0;
    int width, x, y;

    if (sheet->w != SHEET_W || sheet->h != 128 || !ml_section(c, screen, (uint32_t)part, &d) || !ml_get32(d, 0, &count)) {
        return 0;
    }
    for (i = 0; i < count; i++) { /* the second 512-wide texture of the file, as ml_strip counts them */
        if (ml_entry_width(d, i, &width) && width == SHEET_W && ++seen == 2) {
            at = i;
            break;
        }
    }
    if (seen != 2 || !ml_table(d, at, table)) {
        return 0;
    }
    MenuLabels_ItemPixels(pic, table, index);
    for (y = 0; y < 32; y++) {
        for (x = 0; x < SHEET_W; x++) {
            int v = index[y * SHEET_W + x], slot = (v & 0xE7) | ((v & 8) << 1) | ((v & 0x10) >> 1);
            memcpy(sheet->rgba + ((size_t)(96 + y) * SHEET_W + x) * 4, table + slot * 4, 4);
        }
    }
    sheet->hash = UINT64_C(14695981039346656037);
    for (i = 0; i < (uint32_t)(sheet->w * sheet->h * 4); i++) {
        sheet->hash = (sheet->hash ^ sheet->rgba[i]) * UINT64_C(1099511628211);
    }
    return 1;
}
