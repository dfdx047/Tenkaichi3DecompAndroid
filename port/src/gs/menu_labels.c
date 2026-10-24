/* The list of menu labels of the "Menu font" setting, made from the player's own disc data.
 *
 * The menus' labels are lettering painted into textures. To draw one again from a font (menu_text.c) the renderer
 * has to know which texture holds which label, in which row, and how large its letters are. That list
 * (labels.tsv, and name-styles.tsv for the names of added stages and songs) is made here, from the HUD and menu archives
 * in the player's gamedata: nothing of it is in the repository or in a release. This file identifies the archive,
 * section and texture of each label, plus its style; game-text/translations/labels/en.json holds its English wording.
 * Everything else is measured: the textures are unpacked and decoded, hashed the way the renderer hashes a decoded
 * texture, and each row's letter height, baseline, margin and width are taken from its pixels; for the long name
 * strips (stages, songs, the main menu) the port's own letter cutter (namefont.c) tells capitals from lower case.
 *
 * The texture numbers are those of the USA release (SLUS-21678), so the twelve archives are checked by size and
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


/* The selected locale is loaded from game-text/translations/labels/<locale>.json by menu_translation.c. */

/* The letter cutter (namefont.c's, this file's copy) is used by MenuLabels_Make and MenuLabels_MainItem, which
   may run in two threads: one at a time. */
static pthread_mutex_t sMlCutter = PTHREAD_MUTEX_INITIALIZER;
#define ML_FILES 12
#define ML_UNPACKED_MAX (64u * 1024 * 1024)
#define ML_GS_BYTES (512 * 512 * 4) /* the part of the GS's memory a menu texture is uploaded to and read from */
#define ML_GROUPS 160
#define ML_LABELS 40
#define ML_TEXTURES 16
#define ML_ROWS 640 /* what menu_text.c holds */

/* The HUD resource and menu archives the labels are in (gamedata/pzs3us1/<number>.bin), with size and CRC-32 on the USA disc. The
   section and texture numbers in catalog() are only right for these files: anything else is left alone. */
static const struct { int number; uint32_t size, crc; } kMlFiles[ML_FILES] = {
    {5, 701184, 0x7822C98C},
    {448, 3693312, 0xFF3A84B3}, {449, 1544768, 0xB0DA7301}, {450, 2884544, 0x5E5A6AD3}, {451, 8374016, 0xBAC607A9},
    {452, 3466880, 0x0EC0646D}, {453, 1776896, 0x717BA147}, {459, 3329600, 0xE34CA2A0}, {460, 2398528, 0x13642238},
    {461, 3604416, 0xD60327A4}, {463, 875072, 0xB576B4B2}, {472, 740992, 0x26936C01}};

/* The colours of a label. They are chosen by label ID, never read from the texture. */
typedef struct MlColors {
    const char *name;
    uint8_t top[3], bottom[3], outline[3];
    float outlineWidth; /* pixels of the original texture; -1: the renderer's own (usually 3) */
    int outer;        /* a second outline around the first: */
    uint8_t outerColor[3];
    int outerWidth;
    int shadow; /* 0 none, 1 soft, 2 hard opaque, 3 hard translucent */
    int bevelWidth;
    float depth;
    int midGradient;
    int hasMiddle;
    uint8_t middle[3];
} MlColors;
static const MlColors kMlColors[] = {
    {"title-prompt", {239, 239, 223}, {177, 179, 96}, {63, 21, 17}, 2, 1, {217, 194, 193}, 2, 0, 0, 0, 0},
    {"main-menu", {203, 203, 203}, {203, 203, 203}, {94, 35, 109}, -1, 0, {0, 0, 0}, 0, 1, 0, 0, 0},
    {"main-heading", {255, 190, 47}, {255, 34, 34}, {124, 48, 37}, 3, 1, {255, 238, 216}, 2, 0, 0, 0, 0},
    {"purple", {203, 203, 203}, {203, 203, 203}, {70, 0, 89}, -1, 0, {0, 0, 0}, 0, 1, 0, 0, 0},
    {"orange", {244, 239, 137}, {210, 104, 75}, {44, 21, 27}, -1, 0, {0, 0, 0}, 0, 1, 0, 0, 0},
    {"notice-blue", {225, 240, 242}, {99, 168, 193}, {21, 64, 88}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"notice-orange", {255, 210, 170}, {228, 101, 47}, {103, 45, 25}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"notice-red", {226, 145, 145}, {173, 31, 31}, {106, 26, 36}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"notice-silver", {239, 242, 251}, {164, 173, 202}, {54, 62, 101}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"notice-green", {192, 226, 188}, {96, 151, 95}, {37, 83, 34}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"notice-gold", {244, 245, 229}, {177, 165, 100}, {89, 61, 30}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"notice-purple", {210, 210, 226}, {127, 125, 151}, {56, 48, 95}, 3, 1, {0, 0, 0}, 1, 1, 2, 3, 1},
    {"combo-red-digits", {234, 28, 37}, {222, 22, 28}, {0, 0, 0}, 1, 0, {0, 0, 0}, 0, 2, 0, 0.3f, 0},
    {"combo-hits", {255, 245, 223}, {216, 215, 1}, {102, 61, 18}, 1.5f, 0, {0, 0, 0}, 0, 3, 0, 0, 1},
    {"combo-orange-digits", {255, 190, 47}, {255, 34, 34}, {0, 0, 0}, 1, 0, {0, 0, 0}, 0, 2, 0, 0.3f, 0},
    {"combo-message", {254, 93, 13}, {254, 60, 15}, {102, 61, 18}, 1.5f, 0, {18, 10, 3}, 0, 3, 0, 0, 0, 1, {255, 254, 1}},
    {"combo-power", {119, 186, 253}, {92, 172, 253}, {20, 41, 86}, 1.5f, 0, {6, 16, 42}, 0, 3, 0, 0, 0, 1, {255, 255, 255}},
    {"timer-white", {238, 238, 238}, {196, 196, 196}, {36, 36, 36}, 3, 0, {0, 0, 0}, 0, 0, 0, 0.3f, 0},
    {"timer-yellow", {238, 238, 108}, {196, 196, 36}, {38, 38, 24}, 3, 0, {0, 0, 0}, 0, 0, 0, 0.3f, 0},
    {"timer-red", {238, 100, 100}, {196, 36, 36}, {41, 27, 27}, 3, 0, {0, 0, 0}, 0, 0, 0, 0.3f, 0},
    {"win-counter", {242, 242, 242}, {150, 150, 150}, {68, 68, 68}, 1.5f, 0, {20, 20, 20}, 0, 3, 0, 0, 0, 1, {216, 216, 216}},
    {"blast-stock-counter", {255, 139, 0}, {254, 122, 2}, {0, 0, 0}, 3, 0, {0, 0, 0}, 0, 0, 0, 0, 0, 1, {255, 254, 0}},
};

enum { ML_TITLE_PROMPT, ML_MAIN_MENU, ML_MAIN_HEADING, ML_PURPLE, ML_ORANGE,
       ML_NOTICE_BLUE, ML_NOTICE_ORANGE, ML_NOTICE_RED, ML_NOTICE_SILVER, ML_NOTICE_GREEN, ML_NOTICE_GOLD, ML_NOTICE_PURPLE,
       ML_COMBO_RED_DIGITS, ML_COMBO_HITS, ML_COMBO_ORANGE_DIGITS, ML_COMBO_MESSAGE, ML_COMBO_POWER,
       ML_TIMER_WHITE, ML_TIMER_YELLOW, ML_TIMER_RED, ML_WIN_COUNTER, ML_BLAST_STOCK_COUNTER };
/* Stable label group IDs own their visual styles; locale JSON only supplies wording. */
static const struct { const char *id; int style; } kMlGroupStyles[] = {
    {"battle-class-heading", ML_MAIN_HEADING},
    {"battle-training-normal", ML_PURPLE},
    {"battle-training-selected", ML_ORANGE},
    {"bgm-heading", ML_MAIN_HEADING},
    {"bgm-select-heading", ML_MAIN_HEADING},
    {"blast-stock-counter", ML_BLAST_STOCK_COUNTER},
    {"character-color-normal", ML_PURPLE},
    {"character-color-selected", ML_ORANGE},
    {"character-preview-color", ML_ORANGE},
    {"character-reference-normal", ML_PURPLE},
    {"character-reference-selected", ML_ORANGE},
    {"combo-boost", ML_COMBO_MESSAGE},
    {"combo-counter", ML_COMBO_MESSAGE},
    {"combo-first-attack", ML_COMBO_MESSAGE},
    {"combo-hits", ML_COMBO_HITS},
    {"combo-lock-on", ML_COMBO_MESSAGE},
    {"combo-max-power", ML_COMBO_POWER},
    {"combo-orange-digits", ML_COMBO_ORANGE_DIGITS},
    {"combo-red-digits", ML_COMBO_RED_DIGITS},
    {"controller-heading", ML_MAIN_HEADING},
    {"customize-menu-normal", ML_PURPLE},
    {"customize-menu-selected", ML_ORANGE},
    {"customize-slots-normal", ML_PURPLE},
    {"customize-slots-selected", ML_ORANGE},
    {"data-center-normal", ML_PURPLE},
    {"data-center-selected", ML_ORANGE},
    {"data-heading", ML_MAIN_HEADING},
    {"evolution-heading", ML_MAIN_HEADING},
    {"evolution-normal", ML_PURPLE},
    {"evolution-selected", ML_ORANGE},
    {"fighter-heading", ML_MAIN_HEADING},
    {"fusion-heading", ML_MAIN_HEADING},
    {"heading-battle-training", ML_MAIN_HEADING},
    {"history-action-normal", ML_PURPLE},
    {"history-action-selected", ML_ORANGE},
    {"history-difficulty-normal", ML_PURPLE},
    {"history-difficulty-selected", ML_ORANGE},
    {"history-heading", ML_MAIN_HEADING},
    {"item-shop-confirm-normal", ML_PURPLE},
    {"item-shop-confirm-selected", ML_ORANGE},
    {"item-shop-normal", ML_PURPLE},
    {"item-shop-selected", ML_ORANGE},
    {"main-5", ML_MAIN_MENU},
    {"main-6", ML_ORANGE},
    {"main-heading", ML_MAIN_HEADING},
    {"map-heading", ML_MAIN_HEADING},
    {"mission-heading", ML_MAIN_HEADING},
    {"notice-blue", ML_NOTICE_BLUE},
    {"notice-gold", ML_NOTICE_GOLD},
    {"notice-green", ML_NOTICE_GREEN},
    {"notice-orange", ML_NOTICE_ORANGE},
    {"notice-purple", ML_NOTICE_PURPLE},
    {"notice-red", ML_NOTICE_RED},
    {"notice-silver", ML_NOTICE_SILVER},
    {"options-actions-normal", ML_PURPLE},
    {"options-actions-selected", ML_ORANGE},
    {"options-controls-normal", ML_PURPLE},
    {"options-controls-selected", ML_ORANGE},
    {"options-heading", ML_MAIN_HEADING},
    {"options-normal", ML_PURPLE},
    {"options-selected", ML_ORANGE},
    {"options-sound-normal", ML_PURPLE},
    {"options-sound-selected", ML_ORANGE},
    {"options-values-normal", ML_PURPLE},
    {"options-values-selected", ML_ORANGE},
    {"password-heading", ML_MAIN_HEADING},
    {"practice-normal", ML_PURPLE},
    {"practice-selected", ML_ORANGE},
    {"reference-heading", ML_MAIN_HEADING},
    {"replay-heading", ML_MAIN_HEADING},
    {"save-confirm-normal", ML_PURPLE},
    {"save-confirm-selected", ML_ORANGE},
    {"screen-heading", ML_MAIN_HEADING},
    {"shenron-heading", ML_MAIN_HEADING},
    {"shop-heading", ML_MAIN_HEADING},
    {"sim-dragon-actions", ML_ORANGE},
    {"sim-dragon-actions-normal", ML_PURPLE},
    {"sim-heading", ML_MAIN_HEADING},
    {"song", ML_PURPLE},
    {"song-highlighted", ML_ORANGE},
    {"song-random", ML_PURPLE},
    {"song-random-highlighted", ML_ORANGE},
    {"sound-digits-normal", ML_PURPLE},
    {"sound-digits-selected", ML_ORANGE},
    {"sound-heading", ML_MAIN_HEADING},
    {"stage", ML_ORANGE},
    {"survival-heading", ML_MAIN_HEADING},
    {"team-end-normal", ML_PURPLE},
    {"team-end-selected", ML_ORANGE},
    {"timer-red", ML_TIMER_RED},
    {"timer-white", ML_TIMER_WHITE},
    {"timer-yellow", ML_TIMER_YELLOW},
    {"title-normal", ML_PURPLE},
    {"title-press-start", ML_TITLE_PROMPT},
    {"title-selected", ML_ORANGE},
    {"tour-heading", ML_MAIN_HEADING},
    {"training-controller-normal", ML_PURPLE},
    {"training-controller-selected", ML_ORANGE},
    {"training-heading", ML_MAIN_HEADING},
    {"training-normal", ML_PURPLE},
    {"training-selected", ML_ORANGE},
    {"ultimate-battle", ML_ORANGE},
    {"ultimate-battle-normal", ML_PURPLE},
    {"ultimate-heading", ML_MAIN_HEADING},
    {"versus-com-level-normal", ML_PURPLE},
    {"versus-com-level-selected", ML_ORANGE},
    {"versus-dp-normal", ML_PURPLE},
    {"versus-dp-selected", ML_ORANGE},
    {"versus-heading", ML_MAIN_HEADING},
    {"versus-normal", ML_PURPLE},
    {"versus-onoff-normal", ML_PURPLE},
    {"versus-onoff-selected", ML_ORANGE},
    {"versus-referee-normal", ML_PURPLE},
    {"versus-referee-selected", ML_ORANGE},
    {"versus-selected", ML_ORANGE},
    {"versus-settings-normal", ML_PURPLE},
    {"versus-settings-selected", ML_ORANGE},
    {"versus-time-normal", ML_PURPLE},
    {"versus-time-selected", ML_ORANGE},
    {"versus-transform-1p-normal", ML_PURPLE},
    {"versus-transform-1p-selected", ML_ORANGE},
    {"versus-transform-2p-normal", ML_PURPLE},
    {"versus-transform-2p-selected", ML_ORANGE},
    {"versus-type-normal", ML_PURPLE},
    {"versus-type-selected", ML_ORANGE},
    {"win-counter", ML_WIN_COUNTER},
    {"world-tour-custom-normal", ML_PURPLE},
    {"world-tour-custom-selected", ML_ORANGE},
    {"world-tour-list-normal", ML_PURPLE},
    {"world-tour-list-selected", ML_ORANGE},
};

static int ml_style_for_id(const char *id) {
    const char *slash = strchr(id, '/');
    size_t n;
    int i;
    if (slash == NULL) return -1;
    n = (size_t)(slash - id);
    for (i = 0; i < (int)(sizeof(kMlGroupStyles) / sizeof(kMlGroupStyles[0])); i++) {
        if (strlen(kMlGroupStyles[i].id) == n && strncmp(kMlGroupStyles[i].id, id, n) == 0)
            return kMlGroupStyles[i].style;
    }
    return -1;
}
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
    char text[32];
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
    MlRect rect[12];
    int nRects;
} MlGroup;

typedef struct MlRow { /* one label: a row of labels.tsv, an entry of labels.json */
    char id[64], textFrom[64];
    uint64_t hash;
    int isRect, texW, texH, x, y, w;
    const char *text;
    int height, cap, baseline, center, margin, art, widthMilli;
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

/* A member of archive `file` (kept: several menus are in one). File 5's HUD members are already unpacked. */
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
    if (!ml_section(c, c->file[f], (uint32_t)outer, &packed)) {
        return 0;
    }
    if (file == 5) {
        *out = packed;
    } else if (!ml_unpack(c, packed, out)) {
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
static int ml_texture_palette(MlCtx *c, MlBuf d, uint32_t index, uint32_t paletteIndex, MlTex *out) {
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
    if (paletteIndex != index && !ml_get32(d, (uint64_t)table * 4 + (uint64_t)paletteIndex * 64 + 4, &palAt)) {
        return ml_fail(c, "a palette in a menu archive lies outside its texture table") - 1;
    }
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

static int ml_texture(MlCtx *c, MlBuf d, uint32_t index, MlTex *out) {
    return ml_texture_palette(c, d, index, index, out);
}

/* ---- the catalogue --------------------------------------------------------------------------------------------- */

/* The PC's item of the main menu (at the end of this file). */
static int ml_item_pics(MlCtx *c, unsigned char *out[2], const char *text);
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
   The original rows come from the selected locale's `original` fields. `textFrom` names the other selection state's group. */
static MlGroup *ml_add(MlCtx *c, const char *id, int file, int outer, int part, int index, int centered,
                       const char *textFrom, int width, int nested) {
    MlGroup *g = ml_group(c, id, 32, width);
    MlBuf pack, d;
    int actual = 0;
    const char *labels = MenuTranslation_Group(id);

    if (g == NULL || !ml_screen(c, file, outer, &pack) || (nested >= 0 && !ml_section(c, pack, (uint32_t)nested, &pack)) ||
        (part >= 0 && !ml_section(c, pack, (uint32_t)part, &d)) || labels == NULL || !ml_labels(c, g, labels)) {
        if (labels == NULL) { ml_fail(c, "translation labels are missing for %s", id); }
        return NULL;
    }
    if (part < 0) { d = pack; }
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

/* The textures that hold labels. The numbers are the USA disc's (kMlFiles); the English text comes from en.json.
   A texture often shares its file with panels and backgrounds, so each is named by its number.
   Normal and chosen sheets have separate labels and translation entries. */
static int ml_catalog(MlCtx *c) {
    /* Shared face heights for headings with the same measured letter size. Baselines remain per texture. */
    enum { ML_HEADING_CAP_REPLAY = 20, ML_HEADING_CAP_COMPACT = 21,
           ML_HEADING_CAP_STANDARD = 22, ML_HEADING_CAP_LARGE = 24 };
    typedef struct MlHeading {
        const char *id;
        int file, outer, part, index, width, cap, baseline, margin, art, centered;
    } MlHeading;
    /* The measured face inside each cream border: archive, member, texture file, texture, width, cap, baseline,
       left margin, face width. Password and Replay are centered on their 512-pixel sheets. */
    static const MlHeading kHeadings[] = {
        {"history-heading", 460, 1, 11, 1, 512, ML_HEADING_CAP_STANDARD, 28, 9, 319, 0},
        {"ultimate-heading", 451, 1, 4, 1, 512, ML_HEADING_CAP_STANDARD, 29, 9, 318, 0},
        {"sim-heading", 451, 3, 8, 1, 512, ML_HEADING_CAP_STANDARD, 28, 8, 244, 0},
        {"mission-heading", 451, 2, 8, 1, 512, ML_HEADING_CAP_STANDARD, 29, 9, 249, 0},
        {"survival-heading", 451, 2, 9, 1, 512, ML_HEADING_CAP_STANDARD, 29, 8, 169, 0},
        {"fusion-heading", 451, 6, 4, 1, 512, ML_HEADING_CAP_STANDARD, 29, 9, 237, 0},
        {"tour-heading", 450, 1, 22, 1, 512, ML_HEADING_CAP_STANDARD, 28, 9, 322, 0},
        {"evolution-heading", 452, 1, 3, 1, 512, ML_HEADING_CAP_COMPACT, 29, 9, 203, 0},
        {"shop-heading", 452, 2, 20, 1, 512, ML_HEADING_CAP_LARGE, 30, 8, 190, 0},
        {"map-heading", 448, 2, 55, 1, 512, ML_HEADING_CAP_COMPACT, 29, 9, 230, 0},
        {"bgm-select-heading", 448, 2, 55, 2, 512, ML_HEADING_CAP_STANDARD, 29, 9, 236, 0},
        {"training-heading", 461, 1, 5, 1, 512, ML_HEADING_CAP_STANDARD, 28, 9, 366, 0},
        {"heading-battle-training", 461, 1, 5, 3, 512, ML_HEADING_CAP_STANDARD, 28, 9, 310, 0},
        {"data-heading", 459, 1, 9, 1, 512, ML_HEADING_CAP_COMPACT, 29, 9, 250, 0},
        {"password-heading", 459, 3, 2, 6, 512, ML_HEADING_CAP_COMPACT, 32, 8, 332, 1},
        {"fighter-heading", 459, 2, 4, 4, 256, ML_HEADING_CAP_COMPACT, 29, 11, 192, 0},
        {"replay-heading", 459, 4, 4, 6, 512, ML_HEADING_CAP_REPLAY, 42, 8, 200, 1},
        {"reference-heading", 463, 1, 4, 1, 512, ML_HEADING_CAP_LARGE, 29, 8, 345, 0},
        {"options-heading", 453, 1, 18, 1, 512, ML_HEADING_CAP_COMPACT, 29, 8, 161, 0},
        {"screen-heading", 453, 1, 18, 2, 512, ML_HEADING_CAP_COMPACT, 29, 8, 141, 0},
        {"sound-heading", 453, 1, 18, 3, 512, ML_HEADING_CAP_STANDARD, 29, 8, 128, 0},
        {"bgm-heading", 453, 1, 18, 4, 512, ML_HEADING_CAP_COMPACT, 29, 9, 195, 0},
        {"controller-heading", 453, 1, 18, 5, 512, ML_HEADING_CAP_STANDARD, 29, 8, 210, 0},
        {"shenron-heading", 472, 2, 5, 1, 512, ML_HEADING_CAP_STANDARD, 29, 8, 173, 0},
    };
    static const char *const kState[2] = {"normal", "selected"};
    const char *kMain = MenuTranslation_MainOriginal(), *kMlItem = MenuTranslation_ItemOriginal();
    static const struct { const char *family; int part; } kVersusLists[3] = {
        {"versus-time", 15}, {"versus-com-level", 16}, {"versus-referee", 18}};
    MlBuf select, main, timer;
    MlGroup *g;
    char id[48], from[48];
    int i, k, n;

#define ML_ID(...) (snprintf(id, sizeof(id), __VA_ARGS__), id)
#define ML_FROM(on, ...) ((on) ? (snprintf(from, sizeof(from), __VA_ARGS__), from) : NULL)
#define ML_ADD(...) do { g = ml_add(c, __VA_ARGS__); if (g == NULL) { return 0; } } while (0)
#define ML_NOTICE(id, index) do { \
    ML_ADD(id, 5, 4, -1, index, 1, NULL, 512, -1); \
    g->height = 64; g->hasFont = 1; g->fontCap = 43; g->fontBaseline = 50; g->fontMargin = 8; \
} while (0)

    /* The first HUD sheet: timer digits use one picture with three palettes. A fourth picture says "Wins";
       blast stock has its own eight-digit atlas. The infinity mark occupies two digit cells. */
    if (!ml_screen(c, 5, 1, &timer)) { return 0; }
    for (i = 0; i < 3; i++) {
        ML_ADD(i == 0 ? "timer-white" : i == 1 ? "timer-yellow" : "timer-red",
               5, 1, -1, 0, 1, NULL, 128, -1);
        if (i != 0 && ml_texture_palette(c, timer, 0, (uint32_t)i, &g->tex[0]) <= 0) { return 0; }
        ml_cells(g, 10, 4, 32, 0);
        g->nRects = 11;
        snprintf(g->rect[10].text, sizeof(g->rect[10].text), "%s", MenuTranslation_Text("timer-white/0/10"));
        g->rect[10].x = 64; g->rect[10].y = 64; g->rect[10].w = 64; g->rect[10].h = 32;
        g->hasFont = 1; g->fontCap = 22; g->fontBaseline = 28;
    }
    ML_ADD("win-counter", 5, 1, -1, 4, 1, NULL, 64, -1);
    g->nRects = 1;
    snprintf(g->rect[0].text, sizeof(g->rect[0].text), "%s", MenuTranslation_Text("win-counter/0/0"));
    g->rect[0].x = 0; g->rect[0].y = 0; g->rect[0].w = 64; g->rect[0].h = 32;
    g->hasFont = 1; g->fontCap = 10; g->fontBaseline = 15;
    ML_ADD("blast-stock-counter", 5, 1, -1, 14, 1, NULL, 128, -1);
    ml_cells(g, 8, 4, 32, 0);
    g->hasFont = 1; g->fontCap = 19; g->fontBaseline = 25;

    /* File ID 6 is pzs3us1/00005.bin. Member 4 is the battle announcement texture file: the first seven
       textures hold words, with two 64-pixel rows in textures 3 and 4. Textures 7 and 8 are effects/icons. */
    ML_NOTICE("notice-blue", 0);
    ML_NOTICE("notice-orange", 1);
    ML_NOTICE("notice-red", 2);
    ML_NOTICE("notice-silver", 3);
    /* The second row has more pale fill than border; name the border explicitly for both rows' measurements. */
    g->hasOutline = 1;
    g->outline[0] = 106; g->outline[1] = 116; g->outline[2] = 158;
    ML_NOTICE("notice-green", 4);
    ML_NOTICE("notice-gold", 5);
    ML_NOTICE("notice-purple", 6);

    /* Member 5 is the combo HUD sheet. Textures 0 and 1 are 32x32 digit atlases; texture 2
       has Hits! in its upper right. The event messages each occupy the top half of 3..7. */
    ML_ADD("combo-red-digits", 5, 5, -1, 0, 1, NULL, 128, -1);
    ml_cells(g, 10, 4, 32, 0);
    g->hasFont = 1; g->fontCap = 19; g->fontBaseline = 26;
    ML_ADD("combo-orange-digits", 5, 5, -1, 1, 1, NULL, 128, -1);
    ml_cells(g, 10, 4, 32, 0);
    g->hasFont = 1; g->fontCap = 9; g->fontBaseline = 22;
    ML_ADD("combo-hits", 5, 5, -1, 2, 1, NULL, 128, -1);
    g->nRects = 1;
    snprintf(g->rect[0].text, sizeof(g->rect[0].text), "%s", MenuTranslation_Text("combo-hits/0/0"));
    g->rect[0].x = 64; g->rect[0].y = 0; g->rect[0].w = 64; g->rect[0].h = 32;
    g->hasFont = 1; g->fontCap = 10; g->fontBaseline = 29;
    ML_ADD("combo-counter", 5, 5, -1, 3, 1, NULL, 128, -1);
    g->height = 32; g->hasFont = 1; g->fontCap = 13; g->fontBaseline = 23; g->fontMargin = 4;
    ML_ADD("combo-max-power", 5, 5, -1, 4, 1, NULL, 128, -1);
    g->height = 32; g->hasFont = 1; g->fontCap = 13; g->fontBaseline = 23; g->hasArt = 1; g->fontArt = 124;
    ML_ADD("combo-lock-on", 5, 5, -1, 5, 1, NULL, 128, -1);
    g->height = 32; g->hasFont = 1; g->fontCap = 13; g->fontBaseline = 23; g->fontMargin = 4;
    ML_ADD("combo-first-attack", 5, 5, -1, 6, 1, NULL, 128, -1);
    g->height = 32; g->hasFont = 1; g->fontCap = 13; g->fontBaseline = 23; g->hasArt = 1; g->fontArt = 124;
    ML_ADD("combo-boost", 5, 5, -1, 7, 1, NULL, 128, -1);
    g->height = 32; g->hasFont = 1; g->fontCap = 13; g->fontBaseline = 23; g->fontMargin = 4;

    if (!ml_screen(c, 448, 2, &select) || !ml_screen(c, 449, 2, &main)) {
        return 0;
    }
    /* the name strips. Their texts are namefont.c's tables: the names of the stages and of the songs */
    g = ml_strip(c, "stage", 64, select, 6, ML_TEXTURES);
    for (n = 0; g != NULL && n < STAGE_NAMES; n++) {
        g->labels[g->nLabels++] = kStageNames[n];
    }
    for (i = 0; g != NULL && i < 2; i++) {
        g = ml_strip(c, i ? "song-highlighted" : "song", 32, select, i ? 3 : 4, 3);
        for (n = 0; g != NULL && n < SONG_NAMES; n++) {
            g->labels[g->nLabels++] = kSongNames[n];
        }
    }
    /* the main menu, as it is and while chosen: its Flash clips use these same rows. The second sheet of each holds
       the PC's item too (in the game's memory, which the textures are found by): it is written into it here as the
       game writes it, and is the twelfth label */
    {
        unsigned char *item[2];
        char withItem[1024];
        int haveItem = ml_item_pics(c, item, NULL);

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

    ML_ADD("main-heading", 449, 2, 23, 1, 0, NULL, 512, -1);
    g->height = 64; g->hasFont = g->hasArt = g->hasOutline = 1;
    g->fontCap = 25; g->fontBaseline = 30; g->fontArt = 194; g->fontMargin = 9;
    g->outline[0] = 87; g->outline[1] = 22; g->outline[2] = 12;
    ML_ADD("versus-heading", 448, 1, 24, 1, 0, NULL, 512, -1);
    g->height = 64; g->hasFont = g->hasArt = g->hasOutline = 1;
    g->fontCap = 21; g->fontBaseline = 29; g->fontArt = 270; g->fontMargin = 7;
    g->outline[0] = 30; g->outline[1] = 8; g->outline[2] = 4;
    for (i = 0; i < (int)(sizeof(kHeadings) / sizeof(kHeadings[0])); i++) {
        const uint8_t outline[3] = {87, 22, 12};
        const MlHeading *heading = &kHeadings[i];
        ML_ADD(heading->id, heading->file, heading->outer, heading->part, heading->index, heading->centered, NULL, heading->width, -1);
        g->height = 64;
        g->hasFont = g->hasArt = 1;
        g->fontCap = heading->cap; g->fontBaseline = heading->baseline;
        g->fontArt = heading->art; g->fontMargin = heading->margin;
        g->hasOutline = 1;
        memcpy(g->outline, outline, sizeof(outline));
    }
    ML_ADD("battle-class-heading", 461, 1, 8, 0, 0, NULL, 256, -1);
    g->height = 64;
    g->hasOutline = 1;
    g->outline[0] = 87; g->outline[1] = 22; g->outline[2] = 12;

    ML_ADD("title-normal", 449, 1, 8, 0, 1, NULL, 512, -1);
    ML_ADD("title-selected", 449, 1, 8, 1, 1, "title-normal", 512, -1);
    ML_ADD("title-press-start", 449, 1, 6, 0, 1, NULL, 512, -1);
    /* measured on the inner letters, not on this prompt's bright outer border */
    g->hasFont = g->hasArt = 1;
    g->fontCap = 17;
    g->fontBaseline = 25;
    g->fontArt = 264;
    g->hasOutline = 1;
    g->outline[0] = 63, g->outline[1] = 21, g->outline[2] = 17;
    for (i = 0; i < 4; i++) {
        const char *family = i < 2 ? "versus" : "versus-type";
        ML_ADD(ML_ID("%s-%s", family, kState[i % 2]), 448, 1, 8, i,
               0, ML_FROM(i % 2, "%s-normal", family), 512, -1);
    }
    for (i = 0; i < 4; i++) {
        const char *family = i % 2 == 0 ? "options" : "options-sound";
        ML_ADD(ML_ID("%s-%s", family, kState[i / 2]), 453, 1, 16, i,
               0, ML_FROM(i >= 2, "%s-normal", family), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("options-actions-%s", kState[i]), 453, 1, 27, i, 0, ML_FROM(i, "options-actions-normal"), 256, -1);
        ML_ADD(ML_ID("save-confirm-%s", kState[i]), 453, 1, 3, i, 1, ML_FROM(i, "save-confirm-normal"), 128, 35);
    }
    for (i = 0; i < 4; i++) {
        const char *family = i % 2 == 0 ? "options-values" : "options-controls";
        ML_ADD(ML_ID("%s-%s", family, kState[i / 2]), 453, 1, 14, i,
               1, ML_FROM(i >= 2, "%s-normal", family), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("history-difficulty-%s", kState[i]), 460, 1, 9, 2 + i, 1, ML_FROM(i, "history-difficulty-normal"), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("history-action-%s", kState[i]), 460, 1, 9, 4 + i, 0, ML_FROM(i, "history-action-normal"), 512, -1);
    }
    ML_ADD("ultimate-battle", 451, 1, 3, 7, 1, NULL, 512, -1);
    ML_ADD("sim-dragon-actions", 451, 5, 2, 19, 0, NULL, 512, -1);
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("evolution-%s", kState[i]), 452, 1, 5, i, 0, ML_FROM(i, "evolution-normal"), 512, -1);
        ML_ADD(ML_ID("customize-menu-%s", kState[i]), 452, 3, 4, i, 0, ML_FROM(i, "customize-menu-normal"), 256, -1);
        ML_ADD(ML_ID("customize-slots-%s", kState[i]), 452, 3, 4, i + 2, 1, ML_FROM(i, "customize-slots-normal"), 512, -1);
        ML_ADD(ML_ID("item-shop-%s", kState[i]), 452, 2, 3, i, 0, ML_FROM(i, "item-shop-normal"), 512, -1);
        ML_ADD(ML_ID("data-center-%s", kState[i]), 459, 1, 8, i, 0, ML_FROM(i, "data-center-normal"), 512, -1);
        ML_ADD(ML_ID("character-reference-%s", kState[i]), 463, 1, 8, i, 0,
               ML_FROM(i, "character-reference-normal"), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("training-%s", kState[i]), 461, 1, 9, i * 4, 0, ML_FROM(i, "training-normal"), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("practice-%s", kState[i]), 461, 1, 9, 1 + i * 4, 0, ML_FROM(i, "practice-normal"), 512, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("training-controller-%s", kState[i]), 461, 1, 9, 3 + i * 4, 0,
               ML_FROM(i, "training-controller-normal"), 512, -1);
    }
    ML_ADD("ultimate-battle-normal", 451, 1, 3, 6, 1, "ultimate-battle", 512, -1);
    ML_ADD("sim-dragon-actions-normal", 451, 5, 2, 18, 0, "sim-dragon-actions", 512, -1);
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("world-tour-list-%s", kState[i]), 450, 1, 5, i, 0,
               ML_FROM(i, "world-tour-list-normal"), 512, -1);
        ML_ADD(ML_ID("world-tour-custom-%s", kState[i]), 450, 2, 13, i, 1, ML_FROM(i, "world-tour-custom-normal"), 256, -1);
        ML_ADD(ML_ID("versus-settings-%s", kState[i]), 448, 1, 13, i, 0,
               ML_FROM(i, "versus-settings-normal"), 256, -1);
        ML_ADD(ML_ID("versus-dp-%s", kState[i]), 448, 1, 12, i + 1, 1, ML_FROM(i, "versus-dp-normal"), 256, -1);
        ml_outline(g, i); /* the narrow "10" has more fill than outline: the outline is this sheet's colour, not the commonest */
        ML_ADD(ML_ID("versus-onoff-%s", kState[i]), 448, 1, 17, i, 1, ML_FROM(i, "versus-onoff-normal"), 256, -1);
        for (k = 0; k < 3; k++) {
            ML_ADD(ML_ID("%s-%s", kVersusLists[k].family, kState[i]), 448, 1, kVersusLists[k].part, i, 1,
                   ML_FROM(i, "%s-normal", kVersusLists[k].family), 256, -1);
            ml_outline(g, i); /* (the infinity sign's fill outnumbers its outline too) */
        }
        for (k = 1; k <= 2; k++) {
            ML_ADD(ML_ID("versus-transform-%dp-%s", k, kState[i]), 448, 1, 17, k * 2 + i, 1, ML_FROM(i, "versus-transform-%dp-normal", k), 128, -1);
        }
        ML_ADD(ML_ID("item-shop-confirm-%s", kState[i]), 452, 2, 28, i, 1, ML_FROM(i, "item-shop-confirm-normal"), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("battle-training-%s", kState[i]), 461, 1, 9, 2 + i * 4, 0, ML_FROM(i, "battle-training-normal"), 256, -1);
    }
    for (i = 0; i < 2; i++) {
        ML_ADD(ML_ID("team-end-%s", kState[i]), 448, 2, 36, i, 1, ML_FROM(i, "team-end-normal"), 256, -1);
        ML_ADD(ML_ID("sound-digits-%s", kState[i]), 453, 1, 11, i, 0, NULL, 128, -1);
        ml_cells(g, 10, 4, 32, 0);
        ML_ADD(ML_ID("character-color-%s", kState[i]), 448, 2, 29, i, 0, NULL, 128, -1);
        ml_cells(g, 4, 2, 64, 1);
        ml_outline(g, i);
    }
    ML_ADD("character-preview-color", 463, 1, 7, 0, 0, NULL, 128, -1);
    ml_cells(g, 10, 4, 32, 0);
    ML_ADD("song-random", 448, 2, 4, 3, 0, NULL, 512, -1);
    ML_ADD("song-random-highlighted", 448, 2, 3, 3, 0, "song-random", 512, -1);
    return 1;
#undef ML_ID
#undef ML_FROM
#undef ML_ADD
#undef ML_NOTICE
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
            size_t cellBytes = 0;
            uint8_t *cell;
            for (n = 0; n < g->nRects; n++) {
                size_t bytes = (size_t)g->rect[n].w * g->rect[n].h * 4;
                if (bytes > cellBytes) { cellBytes = bytes; }
            }
            cell = ml_alloc(c, cellBytes);
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
                if (g->hasFont) {
                    r->cap = g->fontCap;
                    r->baseline = g->fontBaseline;
                    if (ml_starts(g->id, "timer-")) {
                        static const uint8_t widths[3][11] = {
                            {12, 9, 11, 12, 11, 11, 12, 9, 12, 12, 29},
                            {12, 9, 11, 12, 11, 11, 12, 9, 12, 12, 29},
                            {12, 9, 11, 12, 11, 11, 12, 9, 12, 12, 29},
                        };
                        int variant = strcmp(g->id, "timer-white") == 0 ? 0 : strcmp(g->id, "timer-yellow") == 0 ? 1 : 2;
                        r->art = widths[variant][n];
                        if (n == 10) { r->cap = 53; r->baseline = 38; }
                        else { r->cap = (n == 3 || n == 5 || n == 8) ? 21 : 22; r->baseline = 28; }
                    } else if (strcmp(g->id, "win-counter") == 0) {
                        r->x = 8; r->w = 56; r->art = 32;
                        r->cap = 10; r->baseline = 15;
                    } else if (strcmp(g->id, "blast-stock-counter") == 0) {
                        static const uint8_t widths[8] = {14, 11, 14, 14, 16, 15, 14, 11};
                        r->art = widths[n]; r->cap = 19; r->baseline = 25;
                    }
                    if (strcmp(g->id, "combo-red-digits") == 0) {
                        static const uint8_t widths[10] = {24, 18, 25, 24, 24, 24, 25, 18, 25, 24};
                        static const uint8_t caps[10] = {22, 22, 26, 26, 24, 26, 26, 24, 24, 26};
                        r->art = widths[n]; r->cap = caps[n]; r->baseline = 27; r->widthMilli = 1000;
                    }
                }
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
                    if (strcmp(g->id, "combo-max-power") == 0 || strcmp(g->id, "combo-first-attack") == 0) {
                        r->margin = 0;
                    }
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
        {
            const char *translated = MenuTranslation_Text(r->id);
            int style = ml_style_for_id(r->id);
            if (translated == NULL) { return ml_fail(c, "translation label is missing for %s", r->id); }
            if (style < 0) {
                return ml_fail(c, "%s: label has no catalog style", r->id);
            }
            r->colors = style;
            r->text = translated;
        }
        r->italic = ml_starts(r->id, "title-press-start/") || ml_starts(r->id, "notice-") || ml_starts(r->id, "combo-") ||
                    ml_starts(r->id, "timer-");
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
           r->cap <= 64 && r->baseline >= 0 && (r->baseline <= r->height || (r->isRect && strcmp(MenuTranslation_Original(r->id), "\xE2\x88\x9E") == 0 && r->baseline <= 64)) &&
           r->margin >= 0 && r->margin * 2 < r->w && r->art >= 0 && r->art <= r->w;
}

/* labels.tsv and name-styles.tsv: what menu_text.c reads. */
static int ml_tsv(MlCtx *c, MlText *labels, MlText *styles, int *count) {
    static const char *const kLike[3] = {"song/0/0", "song-highlighted/0/0", "stage/0/0"};
    char top[8], bottom[8], middle[8], outline[8], outer[8], outlineBottom[8], bevelLight[8], bevelDark[8];
    int i;

    ml_put(labels, "# " MENU_LABELS_VERSION " hash width height y row_height cap_height baseline center margin width_milli x row_width art_width top bottom "
                   "middle_rgb has_middle outline italic outline_milli outer_milli outer_rgb shadow outline_bottom bevel_milli bevel_light bevel_dark depth_milli mid_gradient font_face label_id\n");
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
        const MlRow *r = &c->rows[i];
        const MlColors *col = &kMlColors[r->colors];
        const char *original = MenuTranslation_Original(r->id);
        if (!ml_row_ok(r)) {
            return ml_fail(c, "%s: the label was measured outside its texture", r->id);
        }
        if (original == NULL || original[0] == '\0') return ml_fail(c, "%s: original label is missing", r->id);
        ml_put(labels, "%016llx\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\t%d\t%s\t%d\t%d\t%d\t%s\t%d\t%s\t%d\t%s\t%s\t%d\t%d\t%d\t%s\n", (unsigned long long)r->hash,
               r->texW, r->texH, r->y, r->height, r->cap, r->baseline, r->center, r->margin,
               r->widthMilli ? r->widthMilli : 1000, r->x, r->w, r->art,
               ml_hex(col->top, top), ml_hex(col->bottom, bottom), ml_hex(col->middle, middle), col->hasMiddle,
               ml_hex(col->outline, outline), r->italic,
               col->outlineWidth < 0 ? -1 : (int)(col->outlineWidth * 1000.0f + 0.5f), col->outerWidth * 1000, ml_hex(col->outerColor, outer), col->shadow,
               ml_hex(r->colors == ML_MAIN_HEADING ? kHeadingOutlineBottom : col->depth ? col->outerColor : col->outline, outlineBottom),
               r->colors == ML_MAIN_HEADING ? 1000 : col->bevelWidth * 1000,
               ml_hex(r->colors == ML_MAIN_HEADING ? kHeadingBevelLight : col->top, bevelLight),
               ml_hex(r->colors == ML_MAIN_HEADING ? kHeadingBevelDark : col->depth ? col->outerColor : col->outline, bevelDark),
               (int)(col->depth * 1000.0f + 0.5f), col->midGradient, ml_starts(r->id, "notice-") || ml_starts(r->id, "combo-") ||
               ml_starts(r->id, "timer-") || ml_starts(r->id, "win-counter/") || ml_starts(r->id, "blast-stock-counter/"), r->id);
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

    ml_put(t, "{\n");
    ml_put(t, "  \"color_styles\": {\n");
    for (i = 0; i < (int)(sizeof(kMlColors) / sizeof(kMlColors[0])); i++) {
        const MlColors *col = &kMlColors[i];
        int more = col->outlineWidth >= 0 || col->outer || !col->shadow || col->bevelWidth || col->depth || col->midGradient || col->hasMiddle;
        ml_put(t, "    \"%s\": {\n", col->name);
        ml_json_rgb(t, "top", col->top, ",");
        ml_json_rgb(t, "bottom", col->bottom, ",");
        if (col->hasMiddle) { ml_json_rgb(t, "middle", col->middle, ","); }
        ml_json_rgb(t, "outline", col->outline, more ? "," : "");
        if (more) {
            ml_put(t, "      \"outline_width\": %g,\n", (double)col->outlineWidth);
            ml_json_rgb(t, "outer_outline", col->outerColor, ",");
            ml_put(t, "      \"outer_outline_width\": %d,\n", col->outerWidth);
            if (i == ML_MAIN_HEADING || col->bevelWidth) {
                ml_json_rgb(t, "outline_bottom", i == ML_MAIN_HEADING ? kHeadingOutlineBottom : col->outerColor, ",");
                ml_put(t, "      \"bevel_width\": %d,\n", i == ML_MAIN_HEADING ? 1 : col->bevelWidth);
                ml_json_rgb(t, "bevel_light", i == ML_MAIN_HEADING ? kHeadingBevelLight : col->top, ",");
                ml_json_rgb(t, "bevel_dark", i == ML_MAIN_HEADING ? kHeadingBevelDark : col->outerColor, ",");
            }
            if (col->depth || col->midGradient) {
                ml_put(t, "      \"depth\": %g,\n      \"mid_gradient\": %s,\n", (double)col->depth, col->midGradient ? "true" : "false");
            }
            ml_put(t, "      \"shadow\": %s\n", col->shadow == 3 ? "\"translucent-hard\"" :
                   col->shadow == 2 ? "\"hard\"" : col->shadow ? "true" : "false");
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
        ml_json_text(t, MenuTranslation_Original(r->id));
        ml_put(t, ",\n      \"enabled\": true,\n      \"height\": %d,\n      \"cap_height\": %d,\n      \"baseline\": %d,\n      \"align\": \"%s\",\n"
                  "      \"margin\": %d,\n      \"art_width\": %d,\n",
               r->height, r->cap, r->baseline, r->center ? "center" : "left", r->margin, r->art);
        if (r->widthMilli) {
            ml_put(t, "      \"width_scale\": %d.%03d,\n", r->widthMilli / 1000, r->widthMilli % 1000);
        }
        if (!r->isRect && r->x == 0 && r->w == r->texW && r->texW != 512) {
            ml_put(t, "      \"texture_width\": %d,\n", r->texW);
        }
        if (ml_starts(r->id, "notice-") || ml_starts(r->id, "combo-") || ml_starts(r->id, "timer-") ||
            ml_starts(r->id, "win-counter/") || ml_starts(r->id, "blast-stock-counter/")) {
            ml_put(t, "      \"font_face\": \"battle\",\n");
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
    extern int PortGame_IsBT4(void);
    int bt4 = PortGame_IsBT4();

    /* Native BT4 uses its own artwork in the same menu archive slots. The
       bounded package/texture parser still checks the layout when cutting
       letters; the USA archive CRC is not the identity of this disc. */
    { extern int PortGame_Region(void); if (bt4 || PortGame_Region()) anyDisc = 1; }
    snprintf(path, sizeof(path), "%s/%s/%05d.bin", dataDir, bt4?"pzs4us1":"pzs3us1", kMlFiles[i].number);
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
    if (!MenuTranslation_Load(outDir, why, whyLen)) {
        pthread_mutex_unlock(&sMlCutter);
        free(c);
        return MENU_LABELS_FAILED;
    }
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
static void ml_cutter_style_clear(int s) {
    int i;
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
static void ml_cutter_clear(void) {
    int s;
    for (s = 0; s < NF_STYLES; s++) ml_cutter_style_clear(s);
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
static int ml_item_style(MlCtx *c) {
    /* B14's painted labels differ from BT3's. The cutter must describe the
       actual sheet, including unused rows, before lending any glyphs. */
    static const char *bt4Stages[] = {
        "Wasteland - Evening", "Rocky Area - Noon", "Planet Namek", "Tournament of Power - Final",
        "World Tournament - Noon", "Kami's Lookout - Noon", "Cell Game Arena - Ruins", "Supreme Kai's World",
        "Hyperbolic Time Chamber", "City Ruins - Noon", "Mountain Road - Noon", "Archipelago - Evening",
        "Kame House - Noon", "Glacier", "Ice Continent", "Dying Planet",
        "Outer Space - Earth", "Tournament of Power - Climax", "Hell", "Desert - Noon",
        "King's Castle", "Penguin Village - Noon", "Mount Paozu", "Plains - Noon",
        "South City", "Baba's Palace - Night", "Rocky Area - Night", "World Tournament - Evening",
        "Cell Games Arena - Noon", "City Ruins - Evening", "City Ruins - Future", "Tournament of Power",
        "Champa's Ring", "Islands", "Planet - Evening", "Random"
    };
    static const char *bt4Songs[] = {
        "Bu Yuu Retsuden", "Dragon Crisis", "Victory Road", "Little Light",
        "Super Sonic", "Explosion (RB1)", "Final Confrontation", "Dark Half",
        "Denouement", "Stand After Confusion", "The Warrior", "Tense Atmosphere",
        "Challenger", "Gallant", "Sky Battle for Justice", "Move Forward Fearlessly",
        "Dynamite Battle", "Burnin' Up", "Wild Rush", "Evolution", "Force Of Justice",
        "Challengers", "Joker", "Evil Desires"
    };
    extern int PortGame_IsBT4(void);
    int bt4 = PortGame_IsBT4();
    const char *names[ML_LABELS];
    char split[1024], *p;
    const char *kMlMain;
    MlBuf select, main;
    NfStyle music;
    int count = 0, i, ok = 0;

    if (!MenuTranslation_Ensure() || (kMlMain = bt4 ?
        "Dragon History|Time Nest|Dragon World Tour|Duel Mode|UNUSED|Evolution Z|Ultimate Training|Baba's Palace|Zuno's Temple|Options|Shenron Mode" : MenuTranslation_MainOriginal()) == NULL ||
        snprintf(split, sizeof(split), "%s", kMlMain) >= (int)sizeof(split)) return 0;
    for (p = split; count < ML_LABELS; p++) { /* the main menu's texts, one by one */
        names[count++] = p;
        if ((p = strchr(p, '|')) == NULL) {
            break;
        }
        *p = '\0';
    }
    ml_cutter_clear();
    /* Stage and song names together supply the full alphabet: the main menu
       and stages alone have no F, which would disable Free For All. Read and
       cut these once for the entire set of mod pages. */
    if (!ml_screen(c, 448, 2, &select) || !ml_screen(c, 449, 2, &main) ||
        ml_cutter_sheets(c, select, 6, NF_STAGE, 9) != 9 ||
        ml_cutter_sheets(c, select, 4, NF_SONG, 3) != 3 ||
        !style_build(NF_STAGE, bt4 ? bt4Stages : kStageNames, STAGE_NAMES, 64) ||
        !style_build(NF_SONG, bt4 ? bt4Songs : kSongNames, bt4 ? 8 : SONG_NAMES, 32)) return 0;
    /* Keep the original song glyphs: style_lend deliberately will not lend a
       previously rescaled glyph again. */
    music=sStyle[NF_SONG];
    memset(&sStyle[NF_SONG],0,sizeof(sStyle[NF_SONG]));
    ml_cutter_style_clear(NF_SONG);
    if (
        ml_cutter_sheets(c, main, 5, NF_SONG, 2) == 2 && ml_cutter_sheets(c, main, 6, NF_SONG_LIT, 2) == 2 &&
        style_build(NF_SONG, names, count, 32) && style_build(NF_SONG_LIT, names, count, 32)) {
        style_lend(&sStyle[NF_SONG], &sStyle[NF_STAGE]);
        style_lend(&sStyle[NF_SONG_LIT], &sStyle[NF_STAGE]);
        style_lend(&sStyle[NF_SONG], &music);
        style_lend(&sStyle[NF_SONG_LIT], &music);
        sState = 3;
        ok=1;
    }
    for(i=0;i<128;i++) free(music.g[i].px);
    return ok;
}

static int ml_item_compose(const char *text, unsigned char *out[2]) {
    int k, w, h, ok = text != NULL;
    out[0] = out[1] = NULL;
    for (k = 0; k < 2 && ok; k++) {
            out[k] = MenuLabels_NameCompose_(NF_SONG + k, text, &w, &h);
            ok = out[k] != NULL && w == SHEET_W && h == 32;
    }
    if (!ok) {
        free(out[0]);
        free(out[1]);
        out[0] = out[1] = NULL;
    }
    return ok;
}

static int ml_item_pics(MlCtx *c, unsigned char *out[2], const char *text) {
    int ok;
    out[0] = out[1] = NULL;
    ok = ml_item_style(c) && ml_item_compose(text ? text : MenuTranslation_ItemOriginal(), out);
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
        ok = ml_item_pics(c, out, NULL);
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

/* Arbitrary mode labels in the exact lettering of the main menu. */
int MenuLabels_MainTexts(const char *dataDir, const char *const *text, int count, unsigned char *out[][2]) {
    MlCtx *c=calloc(1,sizeof(*c)); int ok=0,i;
    for(i=0;i<count;i++) out[i][0]=out[i][1]=NULL;
    if(!c) return 0;
    pthread_mutex_lock(&sMlCutter);
    if(ml_read_one(c,dataDir,ml_file(448),0)==MENU_LABELS_MADE &&
       ml_read_one(c,dataDir,ml_file(449),0)==MENU_LABELS_MADE && ml_item_style(c)) {
        ok=1;
        for(i=0;i<count;i++) if(!ml_item_compose(text[i],out[i])) {
            ok=0;
        }
    }
    ml_cutter_clear();
    pthread_mutex_unlock(&sMlCutter);
    ml_free(c); free(c); return ok;
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

/* B14 stores the unselected menu lettering in four bits per pixel. Mod pages
   need writable eight-bit sheets in both states. Own their packets and color
   tables instead of writing eight-bit indices into the disc's smaller bank. */
uint32_t Port_MainMenuSheets(uint32_t original, uint32_t model, int lit) {
    enum { PIX=0x20080, PAL=0x480, BYTES=128+2*(PIX+PAL) };
    extern void *Port_LowAlloc(size_t);
    static uint8_t *owned[2], *clean[2];
    MlCtx *c;
    MlBuf screen, file;
    uint8_t *template=(uint8_t *)Port_Ptr(model);
    uint8_t *templatePix=(uint8_t *)Port_Ptr(rd32(template+56));
    uint8_t *templatePal=(uint8_t *)Port_Ptr(rd32(template+60));
    const char *root=getenv("BT3_DATA");
    int i,x,y,j;
    if(lit<0 || lit>1) return original;
    if(owned[lit]) { memcpy(owned[lit],clean[lit],BYTES); return (uint32_t)(uintptr_t)owned[lit]; }
    c=calloc(1,sizeof(*c));
    if(!c || rd32(template+8)!=PIX || rd32(template+12)!=PAL ||
       ml_read_one(c,root?root:"gamedata",ml_file(449),0)!=MENU_LABELS_MADE ||
       !ml_screen(c,449,2,&screen) || !ml_section(c,screen,5+lit,&file)) goto fail;
    owned[lit]=Port_LowAlloc(BYTES);clean[lit]=calloc(BYTES,1);
    if(!owned[lit] || !clean[lit]) goto fail;
    memset(owned[lit],0,BYTES);
    for(i=0;i<2;i++) {
        MlTex picture;
        uint8_t *e=owned[lit]+64*i, *pix=owned[lit]+128+i*(PIX+PAL),*pal=pix+PIX;
        uint32_t src, palette;
        uint64_t tex0; int colors;
        if(ml_texture(c,file,i,&picture)<=0 || picture.w!=512 || picture.h>256) goto fail;
        src=rd32(file.p+4)*4+i*64;palette=rd32(file.p+src+4)*4+96;
        memcpy(&tex0,file.p+src+48,8);colors=((tex0>>20)&63)==20?16:256;
        if(palette>file.n || file.n-palette<(unsigned)colors*4) goto fail;
        memcpy(e,template,64);memcpy(pix,templatePix,96);memcpy(pix+PIX-32,templatePix+PIX-32,32);
        memcpy(pal,templatePal,96);memcpy(pal+PAL-32,templatePal+PAL-32,32);
        { uint32_t a=(uint32_t)(uintptr_t)pix,b=(uint32_t)(uintptr_t)pal;
          memcpy(e+56,&a,4);memcpy(e+60,&b,4); }
        for(j=0;j<256;j++) {
            int slot=(j&0xe7)|((j&8)<<1)|((j&16)>>1);
            int source=colors==16?j%16:slot;
            memcpy(pal+96+slot*4,file.p+palette+source*4,4);
        }
        for(y=0;y<picture.h;y++) for(x=0;x<512;x++) {
            const uint8_t *rgba=picture.rgba+4*(y*512+x);int index=0;
            for(j=0;j<colors;j++) {
                int slot=(j&0xe7)|((j&8)<<1)|((j&16)>>1);
                const uint8_t *q=pal+96+slot*4;
                if(memcmp(q,rgba,4)==0) {index=j;break;}
            }
            { int block=(y&~15)*512+(x&~15)*2,swap=(((y+2)>>2)&1)*4;
              int line=(((y&~3)>>1)+(y&1))&7;
              pix[96+block+line*1024+((x+swap)&7)*4+((y>>1)&1)+((x>>2)&2)]=index; }
        }
    }
    memcpy(clean[lit],owned[lit],BYTES);
    ml_free(c);free(c);
    return (uint32_t)(uintptr_t)owned[lit];
fail:
    fprintf(stderr,"native disc: could not prepare BT4 main-menu textures\n");
    if(c) {ml_free(c);free(c);}
    exit(2);
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

/* ---- the European disc's menu art (MenuLabels_Art) -------------------------------------------------------------- */

/* A texture of the USA disc's menus that is painted otherwise in the chosen language: the FNV-1a hash of its
   decoded pixels (what the renderer has when it decodes it, as the labels' hashes) and the European texture. */
typedef struct MlArt {
    uint64_t hash;
    int w, h;
    uint32_t file;           /* the European texture file: sArtFile[file] */
    uint16_t index, palette; /* its texture and the texture whose colour table it is drawn with */
    int top;                 /* the hash is of the first ML_ART_TOP rows only (a sheet of the main menu: below, the
                                PC's item is written in, ml_item_into, and is kept) */
} MlArt;

#define ML_ART_TOP 96

static struct { const uint8_t *p; size_t n; } *sArtFile;
static MlArt *sArt;
static int sArtCount, sArtReady;
static uint8_t sArtSize[10][10];                 /* the sizes (as powers of two) any of them has */
static pthread_mutex_t sArtLock = PTHREAD_MUTEX_INITIALIZER;

static int ml_art_order(const void *a, const void *b) {
    const MlArt *x = a, *y = b;
    return x->hash < y->hash ? -1 : x->hash > y->hash ? 1 : 0;
}

static int ml_log2(int v) {
    int k = 0;
    while (k < 10 && (1 << k) != v) {
        k++;
    }
    return k;
}

/* One texture with one colour table of a texture file, decoded into `out` (its own context, freed by the caller). */
static int ml_art_decode(MlCtx *t, MlBuf d, uint32_t index, uint32_t palette, MlTex *out) {
    return ml_texture_palette(t, d, index, palette, out);
}

/* Texture `index` drawn with the table of texture `palette`, in the USA and the European file: 1 (and `out` filled)
   when the pictures differ, 0 when they are the same, -1 when it is no such texture in both. */
static int ml_art_one(MlBuf usa, MlBuf eu, uint32_t file, uint32_t index, uint32_t palette, MlArt *out) {
    MlCtx *t = calloc(1, sizeof(*t));
    MlTex a, b;
    int r = -1;

    if (t == NULL) {
        return -1;
    }
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    if (ml_art_decode(t, usa, index, palette, &a) > 0 && ml_art_decode(t, eu, index, palette, &b) > 0 && a.w == b.w && a.h == b.h) {
        r = memcmp(a.rgba, b.rgba, (size_t)a.w * a.h * 4) != 0;
        if (r) {
            out->hash = a.hash;
            out->w = a.w;
            out->h = a.h;
            out->file = file;
            out->index = (uint16_t)index;
            out->palette = (uint16_t)palette;
            out->top = 0;
            if (a.w == SHEET_W && a.h == 128) { /* (a main menu sheet as the game has it, the PC's item written in, in
                                                   the pair after this one: ml_art_pair) */
                size_t i;
                out[1] = out[0];
                out[1].top = 1;
                out[1].hash = UINT64_C(14695981039346656037);
                for (i = 0; i < (size_t)SHEET_W * ML_ART_TOP * 4; i++) {
                    out[1].hash = (out[1].hash ^ a.rgba[i]) * UINT64_C(1099511628211);
                }
            }
        }
    }
    ml_free(t);
    free(t);
    return r;
}

/* The pairs of one recorded texture file: every texture whose European picture differs from the USA one, with its
   own colour table and, then, with each other table of the file (labels drawn normal and chosen can be one picture
   in two tables). */
static int ml_art_pair(MlBuf usa, MlBuf eu, uint32_t file, int itemSheets, MlArt *out, int room) {
    uint32_t count, i, j;
    int n = 0;

    if (!ml_get32(usa, 0, &count) || count == 0 || count > 64) {
        return 0;
    }
    for (i = 0; i < count && n + 1 < room; i++) {
        if (ml_art_one(usa, eu, file, i, i, out + n) <= 0) {
            continue;
        }
        n += itemSheets && out[n].w == SHEET_W && out[n].h == 128 ? 2 : 1;
        for (j = 0; j < count && n + 1 < room; j++) {
            if (j != i && ml_art_one(usa, eu, file, i, j, out + n) > 0) {
                n += itemSheets && out[n].w == SHEET_W && out[n].h == 128 ? 2 : 1;
            }
        }
    }
    return n;
}

/* What the pairing builds: the pairs, and the European texture files they are decoded from (kept: sArtFile). */
typedef struct MlArtBuild {
    MlArt *art;
    int n, cap;
    uint32_t files, capFiles;
} MlArtBuild;

/* A table of sections as the archives have them (a count, then count + 1 offsets in order): its count, or 0. */
static uint32_t ml_table_count(MlBuf d) {
    uint32_t count, prev, off, i;
    if (d.n < 12 || !ml_get32(d, 0, &count) || count == 0 || count > 16384 || (uint64_t)(count + 2) * 4 > d.n || !ml_get32(d, 4, &prev)) {
        return 0;
    }
    prev &= ~3u;
    if (prev < (count + 2) * 4) {
        return 0;
    }
    for (i = 2; i <= count + 1; i++) {
        if (!ml_get32(d, (uint64_t)i * 4, &off) || (off &= ~3u) < prev || off > d.n) {
            return 0;
        }
        prev = off;
    }
    return count;
}

/* Section `index` (1 ..) of such a table, from its offset to the next one's. */
static MlBuf ml_part(MlBuf d, uint32_t index) {
    uint32_t first = 0, last = 0;
    MlBuf out = {d.p, 0};
    ml_get32(d, (uint64_t)index * 4, &first);
    ml_get32(d, (uint64_t)index * 4 + 4, &last);
    first &= ~3u;
    last &= ~3u;
    if (first <= last && last <= d.n) {
        out.p = d.p + first;
        out.n = last - first;
    }
    return out;
}

/* A texture file of a menu archive: its texture table's first entry an 8- or 4-bit texture of at most 512. */
static int ml_is_texture_file(MlBuf d) {
    uint32_t table;
    uint64_t tex0;
    if (d.n < 16 || !ml_get32(d, 4, &table) || !ml_get64(d, (uint64_t)table * 4 + 48, &tex0)) {
        return 0;
    }
    return (((tex0 >> 20) & 63) == 19 || ((tex0 >> 20) & 63) == 20) && ((tex0 >> 26) & 15) <= 9 && ((tex0 >> 30) & 15) <= 9;
}

/* A member, unpacked (the HUD's are not packed: copied). NULL: not one. */
static uint8_t *ml_member(MlBuf packed, int raw, size_t *n) {
    uint32_t full = 0, pk = 0;
    uint8_t *out;
    if (raw) {
        if ((out = malloc(packed.n ? packed.n : 1)) != NULL) {
            memcpy(out, packed.p, packed.n);
            *n = packed.n;
        }
        return out;
    }
    if (!ml_get32(packed, 0, &full) || !ml_get32(packed, 4, &pk) || full == 0 || full > ML_UNPACKED_MAX || pk > packed.n - 8 ||
        (out = malloc(full)) == NULL) {
        return NULL;
    }
    if (ml_unpack_bytes(packed.p, packed.n, out, full) != (long)full) {
        free(out);
        return NULL;
    }
    *n = full;
    return out;
}

/* The two discs' sections side by side (path: the member, then the section numbers below it): every texture file
   that differs is paired (ml_art_pair), its European copy kept. */
static int ml_art_walk(MlBuf usa, MlBuf eu, int file, uint32_t *path, int depth, MlArtBuild *b) {
    uint32_t count, i;
    if (usa.n == eu.n && memcmp(usa.p, eu.p, usa.n) == 0) {
        return 1;
    }
    if (ml_is_texture_file(usa) && ml_is_texture_file(eu)) {
        uint8_t *copy;
        int had = b->n, m;
        /* the main menu's sheets (449, member 2, parts 5 and 6: ml_item_into) */
        int items = file == 449 && depth == 1 && path[0] == 2 && (path[1] == 5 || path[1] == 6);
        if (b->cap - b->n < 64 * 64 * 2 + 2) { /* (the most one texture file can give: every texture with every table) */
            int cap = b->cap * 2 + 64 * 64 * 2 + 2;
            MlArt *more = realloc(b->art, (size_t)cap * sizeof(*more));
            if (more == NULL) {
                return 0;
            }
            b->art = more;
            b->cap = cap;
        }
        if (b->files == b->capFiles) {
            uint32_t cap = b->capFiles ? b->capFiles * 2 : 256;
            void *more = realloc(sArtFile, cap * sizeof(*sArtFile));
            if (more == NULL) {
                return 0;
            }
            sArtFile = more;
            b->capFiles = cap;
        }
        if ((copy = malloc(eu.n)) == NULL) {
            return 0;
        }
        memcpy(copy, eu.p, eu.n);
        sArtFile[b->files].p = copy;
        sArtFile[b->files].n = eu.n;
        b->n += ml_art_pair(usa, (MlBuf){copy, eu.n}, b->files, items, b->art + b->n, b->cap - b->n);
        for (m = had; m < b->n && getenv("BT3_ART_LOG") != NULL; m++) { /* BT3_ART_LOG=1: every pair, and every swap */
            fprintf(stderr, "bt3: menu art: %05d/%u", file, path[0]);
            for (i = 1; i <= (uint32_t)depth; i++) {
                fprintf(stderr, "/%u", path[i]);
            }
            fprintf(stderr, " texture %u colours %u: %dx%d %016llx\n", b->art[m].index, b->art[m].palette, b->art[m].w, b->art[m].h,
                    (unsigned long long)b->art[m].hash);
        }
        b->files++;
        return 1;
    }
    if (depth >= 4 || (count = ml_table_count(usa)) == 0 || ml_table_count(eu) != count) {
        return 1;
    }
    for (i = 1; i <= count; i++) {
        path[depth + 1] = i;
        if (!ml_art_walk(ml_part(usa, i), ml_part(eu, i), file, path, depth + 1, b)) {
            return 0;
        }
    }
    return 1;
}

int MenuLabels_Art(const char *dataDir, int bank, char *why, size_t whyLen) {
    MlCtx *c = calloc(1, sizeof(*c));
    MlArtBuild b;
    uint32_t k;
    int f, result = -1;

    memset(&b, 0, sizeof(b));
    snprintf(why, whyLen, "out of memory");
    if (c == NULL || bank < 1 || bank > 4) {
        goto done;
    }
    if (ml_read_disc(c, dataDir, 0) != MENU_LABELS_MADE) {
        snprintf(why, whyLen, "%s", c->why);
        goto done;
    }
    for (f = 0; f < ML_FILES; f++) { /* each USA archive beside the same archive of the language's bank */
        int number = kMlFiles[f].number, entry = number == 5 ? 5 + bank : 451 + (number - 448) + bank * 31;
        char path[1300];
        FILE *fp;
        long size;
        uint8_t *eu = NULL;
        MlBuf usa = c->file[f], euBuf;
        uint32_t count, m, at[6];
        snprintf(path, sizeof(path), "%s/pzs3eu1/%05d.bin", dataDir, entry);
        if ((fp = fopen(path, "rb")) == NULL || fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 12 || size > 64L * 1024 * 1024 ||
            fseek(fp, 0, SEEK_SET) != 0 || (eu = malloc((size_t)size)) == NULL || fread(eu, 1, (size_t)size, fp) != (size_t)size) {
            if (fp != NULL) {
                fclose(fp);
            }
            free(eu);
            snprintf(why, whyLen, "%s is missing or cannot be read", path);
            goto done;
        }
        fclose(fp);
        euBuf.p = eu;
        euBuf.n = (size_t)size;
        count = ml_table_count(usa);
        if (count != 0 && ml_table_count(euBuf) == count) {
            for (m = 1; m <= count; m++) {
                size_t nu = 0, ne = 0;
                uint8_t *mu = ml_member(ml_part(usa, m), number == 5, &nu), *me = ml_member(ml_part(euBuf, m), number == 5, &ne);
                int ok = 1;
                if (mu != NULL && me != NULL) {
                    at[0] = m;
                    ok = ml_art_walk((MlBuf){mu, nu}, (MlBuf){me, ne}, number, at, 0, &b);
                }
                free(mu);
                free(me);
                if (!ok) {
                    free(eu);
                    goto done;
                }
            }
        }
        free(eu);
    }
    qsort(b.art, (size_t)b.n, sizeof(*b.art), ml_art_order);
    for (k = 0; k < (uint32_t)b.n; k++) {
        sArtSize[ml_log2(b.art[k].w)][ml_log2(b.art[k].h)] = 1;
    }
    sArt = b.art;
    b.art = NULL;
    sArtCount = b.n;
    __atomic_store_n(&sArtReady, 1, __ATOMIC_RELEASE);
    snprintf(why, whyLen, "%d textures in %u texture files", sArtCount, b.files);
    result = sArtCount;
done:
    if (result < 0) {
        for (k = 0; k < b.files; k++) {
            free((void *)sArtFile[k].p);
        }
        free(sArtFile);
        sArtFile = NULL;
    }
    free(b.art);
    if (c != NULL) {
        ml_free(c);
        free(c);
    }
    return result;
}

unsigned MenuLabels_ArtKey(void) {
    return __atomic_load_n(&sArtReady, __ATOMIC_ACQUIRE) ? 0x41525431u : 0;
}

int MenuLabels_ArtSwap(unsigned char *rgba, int w, int h) {
    uint64_t hash = UINT64_C(14695981039346656037);
    MlArt key, *hit;
    MlCtx *t;
    MlTex tex;
    size_t i, bytes = (size_t)w * h * 4;
    int done = 0;

    if (!__atomic_load_n(&sArtReady, __ATOMIC_ACQUIRE) || rgba == NULL || w < 1 || h < 1 || w > 512 || h > 512 ||
        !sArtSize[ml_log2(w)][ml_log2(h)]) {
        return 0;
    }
    for (i = 0; i < bytes; i++) {
        hash = (hash ^ rgba[i]) * UINT64_C(1099511628211);
    }
    key.hash = hash;
    hit = bsearch(&key, sArt, (size_t)sArtCount, sizeof(*sArt), ml_art_order);
    if ((hit == NULL || hit->top) && w == SHEET_W && h == 128) { /* a main menu sheet with the PC's item written in */
        key.hash = UINT64_C(14695981039346656037);
        for (i = 0; i < (size_t)SHEET_W * ML_ART_TOP * 4; i++) {
            key.hash = (key.hash ^ rgba[i]) * UINT64_C(1099511628211);
        }
        hit = bsearch(&key, sArt, (size_t)sArtCount, sizeof(*sArt), ml_art_order);
        hit = hit != NULL && hit->top ? hit : NULL;
    } else if (hit != NULL && hit->top) {
        hit = NULL;
    }
    if (getenv("BT3_ART_LOG") != NULL) {
        fprintf(stderr, "bt3: menu art: %dx%d %016llx %s\n", w, h, (unsigned long long)hash, hit != NULL ? "swapped" : "not one");
    }
    if (hit == NULL || hit->w != w || hit->h != h) {
        return 0;
    }
    pthread_mutex_lock(&sArtLock);
    t = calloc(1, sizeof(*t));
    memset(&tex, 0, sizeof(tex));
    if (t != NULL && ml_art_decode(t, (MlBuf){sArtFile[hit->file].p, sArtFile[hit->file].n}, hit->index, hit->palette, &tex) > 0 &&
        tex.w == w && tex.h == h) {
        memcpy(rgba, tex.rgba, hit->top ? (size_t)w * ML_ART_TOP * 4 : bytes); /* (below: the PC's item, kept) */
        done = 1;
    }
    if (t != NULL) {
        ml_free(t);
        free(t);
    }
    pthread_mutex_unlock(&sArtLock);
    return done;
}
