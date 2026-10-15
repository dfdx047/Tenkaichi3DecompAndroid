/* Menu labels and the game's shared text drawn from a TrueType font (the two settings in F1 > UI).
 *
 * "Menu font": a label keeps the row of the menu texture it has on the disc, so the menus' animation, tint,
 * clipping and layout go on working; the row is drawn again from the font, several times the size. Which texture
 * has which label is in labels.tsv in gamedata/game-text, made from the user's own disc (nothing of it is in the
 * repository or in a release). When it is not there, or is of an older format, it is made at start
 * (menu_labels.c), in a thread of its own so that the program does not wait: until it is there the setting
 * offers only "Original", and a font the settings ask for is taken into use when it is (MenuText_Poll, which the
 * renderer calls between two frames). In a run whose buttons are recorded or played back, or that is re-run
 * blank by blank (BT3_PAD_REC, BT3_PAD_PLAY, BT3_PAD_TABLE, BT3_SYNCTEST), it is made at start before anything is
 * drawn, as song_mp3.c converts its songs then: the pictures of such a run do not depend on how fast it was.
 * "Dialogue / UI font": the glyph cells of the game's own font sheets (Font_Print) are drawn again from the font.
 *
 * The fonts are files next to the program: fonts/ in a release, port/third_party/fonts in a source checkout, with
 * the list fonts.tsv. A font file is checked before anything of it is used (face_open): stb_truetype reads what
 * the file says without looking at its size, so a damaged file must not reach it. A font that fails is not used,
 * the setting falls back to "Original", and one line in the log says why.
 *
 * Nothing here is game state: the game is only read (the glyph table Font_UploadTexture hands over), and what
 * this file keeps is on the renderer's side (not saved by a rollback, not in the desync checksum). Texture-pack
 * replacements take precedence. */
#include "menu_text.h"
#include "menu_labels.h"
#include "../port_addr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#define STBTT_assert(x) ((void)0) /* never stops the game over a font; face_open has checked what it relies on */
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/imgui/imstb_truetype.h"

#define MENU_TEXT_MAX 512
#define MENU_TEXT_WIDTH 512
#define MENU_TEXT_HEIGHT 512
#define FONT_FILE_MAX (16 * 1024 * 1024)
#define ROW_CACHE_BYTES (24u * 1024 * 1024) /* drawn labels kept for the next time their texture is decoded */
#define ROW_CACHE_MAX 256

/* One lock for everything below: the renderer decodes textures, the game's thread registers glyph tables, and
   the settings window changes the fonts. The public functions take it; the static ones are called with it held. */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------------------------------ a checked font */

typedef struct Face {
    unsigned char *data;
    size_t size;
    stbtt_fontinfo info;
    uint32_t cmap;   /* the Unicode character map used: its offset in the file, */
    uint32_t cmapLen; /* how many bytes of the file belong to it, */
    int cmapFormat;  /* and its format (4 or 12) */
    int glyphs, unitsPerEm, capUnits;
} Face;

static uint32_t be16(const unsigned char *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t be32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* A table of the font by its tag: offset and length, both inside the file (face_check made sure). */
static int face_table(const Face *f, const char *tag, uint32_t *off, uint32_t *len) {
    uint32_t n = be16(f->data + 4), i;
    for (i = 0; i < n; i++) {
        const unsigned char *r = f->data + 12 + 16 * i;
        if (memcmp(r, tag, 4) == 0) {
            *off = be32(r + 8);
            *len = be32(r + 12);
            return 1;
        }
    }
    return 0;
}

/* The outline of glyph g lies inside its part of the glyf table, the way stb_truetype walks it. Returns the
   number of points it comes to (parts of a composite counted), or -1. `cost` remembers the answers. */
static int face_glyph_cost(const Face *f, const unsigned char *glyf, const unsigned char *loca, int longLoca,
                           int g, int32_t *cost, int depth) {
    uint32_t a, b;
    const unsigned char *p, *end;
    int contours, total = 0;

    if (cost[g] != -2) { return cost[g]; }
    if (depth > 4) { return -1; } /* (also ends a composite that contains itself) */
    a = longLoca ? be32(loca + 4 * (size_t)g) : be16(loca + 2 * (size_t)g) * 2;
    b = longLoca ? be32(loca + 4 * (size_t)g + 4) : be16(loca + 2 * (size_t)g + 2) * 2;
    if (a == b) { return cost[g] = 0; }
    if (b - a < 10) { return cost[g] = -1; }
    p = glyf + a;
    end = glyf + b;
    contours = (int16_t)be16(p);
    p += 10;
    if (contours > 0) {
        int points, i = 0, last = -1, c, lastFlag = 1, lastAlone = 0;
        size_t coords = 0;
        if ((size_t)(end - p) < (size_t)contours * 2 + 2) { return cost[g] = -1; }
        for (c = 0; c < contours; c++) {
            int e = (int)be16(p + 2 * c);
            if (e <= last) { return cost[g] = -1; }
            lastAlone = e == last + 1; /* (of the last contour: it has one point) */
            last = e;
        }
        points = last + 1;
        p += (size_t)contours * 2;
        if ((size_t)(end - p) < 2 + (size_t)be16(p)) { return cost[g] = -1; }
        p += 2 + be16(p); /* the instructions */
        while (i < points) {
            int flag, repeat = 1;
            if (p >= end) { return cost[g] = -1; }
            flag = *p++;
            if (flag & 8) {
                if (p >= end) { return cost[g] = -1; }
                repeat += *p++;
            }
            if (repeat > points - i) { repeat = points - i; }
            coords += (size_t)repeat * ((flag & 2 ? 1 : flag & 16 ? 0 : 2) + (flag & 4 ? 1 : flag & 32 ? 0 : 2));
            i += repeat;
            lastFlag = flag;
        }
        /* stb_truetype looks at the point after a contour's first one when that is off the curve: such a
           contour of one point at the very end of the glyph would make it read past its array */
        if (lastAlone && !(lastFlag & 1)) { return cost[g] = -1; }
        if ((size_t)(end - p) < coords) { return cost[g] = -1; }
        total = points + 2 * contours;
    } else if (contours < 0) {
        int more = 1, parts = 0;
        cost[g] = -1; /* while its parts are looked at: a part that leads back here fails */
        while (more) {
            uint32_t flags, part;
            int sub;
            if (end - p < 4 || ++parts > 64) { return -1; }
            flags = be16(p);
            part = be16(p + 2);
            p += 4;
            p += flags & 2 ? (flags & 1 ? 4 : 2) : 0;
            p += flags & 8 ? 2 : flags & 0x40 ? 4 : flags & 0x80 ? 8 : 0;
            if (p > end || part >= (uint32_t)f->glyphs) { return -1; }
            sub = face_glyph_cost(f, glyf, loca, longLoca, (int)part, cost, depth + 1);
            if (sub < 0) { return -1; }
            total += sub;
            more = (flags & 0x20) != 0;
        }
    }
    if (total > 16384) { return cost[g] = -1; }
    return cost[g] = total;
}

/* NULL if the file in f->data is a font everything below can use without reading outside it; else what is wrong. */
static const char *face_check(Face *f) {
    const unsigned char *d = f->data;
    uint32_t n, i, head, headLen, hhea, hheaLen, maxp, maxpLen, hmtx, hmtxLen, loca, locaLen, glyf, glyfLen, cmap, cmapLen, t, tl;
    uint32_t metrics, prev = 0, best = 0;
    int longLoca, bestRank = 0;
    int32_t *cost;

    if (f->size < 12) { return "the file is too short"; }
    if (be32(d) != 0x00010000u && memcmp(d, "true", 4) != 0) {
        return "not a TrueType font (a .ttf with glyf outlines is needed; .otf, collections and web fonts are not read)";
    }
    n = be16(d + 4);
    if (n < 1 || n > 512 || 12 + 16 * (size_t)n > f->size) { return "the table directory is damaged"; }
    for (i = 0; i < n; i++) {
        uint32_t off = be32(d + 12 + 16 * i + 8), len = be32(d + 12 + 16 * i + 12);
        if (off > f->size || len > f->size - off) { return "a table lies outside the file (the file is cut short or damaged)"; }
    }
    if (face_table(f, "fvar", &t, &tl)) { return "a variable font (a static one is needed)"; }
    if (!face_table(f, "head", &head, &headLen) || headLen < 54 || !face_table(f, "hhea", &hhea, &hheaLen) || hheaLen < 36 ||
        !face_table(f, "maxp", &maxp, &maxpLen) || maxpLen < 6 || !face_table(f, "hmtx", &hmtx, &hmtxLen) ||
        !face_table(f, "loca", &loca, &locaLen) || !face_table(f, "glyf", &glyf, &glyfLen) ||
        !face_table(f, "cmap", &cmap, &cmapLen) || cmapLen < 4) {
        return "a table a TrueType font needs is missing or too short";
    }
    f->unitsPerEm = (int)be16(d + head + 18);
    if (f->unitsPerEm < 16 || f->unitsPerEm > 16384 || be16(d + head + 50) > 1) { return "the font header is damaged"; }
    longLoca = be16(d + head + 50) == 1;
    f->glyphs = (int)be16(d + maxp + 4);
    metrics = be16(d + hhea + 34);
    if (f->glyphs < 1 || metrics < 1 || metrics > (uint32_t)f->glyphs ||
        hmtxLen < 4 * (size_t)metrics + 2 * (size_t)((uint32_t)f->glyphs - metrics)) { return "the glyph widths are damaged"; }
    if (locaLen < ((size_t)f->glyphs + 1) * (longLoca ? 4 : 2)) { return "the glyph index is too short"; }
    for (i = 0; i <= (uint32_t)f->glyphs; i++) {
        uint32_t at = longLoca ? be32(d + loca + 4 * (size_t)i) : be16(d + loca + 2 * (size_t)i) * 2;
        if (at < prev || at > glyfLen) { return "the glyph index points outside the glyphs"; }
        prev = at;
    }
    cost = malloc((size_t)f->glyphs * sizeof(*cost));
    if (cost == NULL) { return "out of memory"; }
    for (i = 0; i < (uint32_t)f->glyphs; i++) { cost[i] = -2; }
    for (i = 0; i < (uint32_t)f->glyphs; i++) {
        if (face_glyph_cost(f, d + glyf, d + loca, longLoca, (int)i, cost, 0) < 0) {
            free(cost);
            return "a glyph's outline is damaged";
        }
    }
    free(cost);
    /* the character map: a Unicode one, in the two formats fonts have (4: the first 65536 characters, 12: all) */
    n = be16(d + cmap + 2);
    if (4 + 8 * (size_t)n > cmapLen) { return "the character map is damaged"; }
    for (i = 0; i < n; i++) {
        const unsigned char *r = d + cmap + 4 + 8 * i;
        uint32_t platform = be16(r), encoding = be16(r + 2), off = be32(r + 4), format, rank;
        if (off > cmapLen || cmapLen - off < 16) { continue; }
        format = be16(d + cmap + off);
        if (!(platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10))) || (format != 4 && format != 12)) { continue; }
        rank = format == 12 ? 2 : 1;
        if ((int)rank > bestRank) {
            bestRank = (int)rank;
            best = off;
        }
    }
    if (bestRank == 0) { return "no Unicode character map"; }
    f->cmap = cmap + best;
    f->cmapLen = cmapLen - best;
    f->cmapFormat = bestRank == 2 ? 12 : 4;
    if (f->cmapFormat == 4) {
        uint32_t segs = be16(d + f->cmap + 6) / 2;
        if (segs < 1 || 16 + 8 * (size_t)segs > f->cmapLen) { return "the character map is damaged"; }
    } else {
        uint32_t groups = be32(d + f->cmap + 12);
        if (groups > (f->cmapLen - 16) / 12) { return "the character map is damaged"; }
    }
    return NULL;
}

/* The glyph of a character, 0 if the font has none. (Not stbtt_FindGlyphIndex: that one trusts the map's own
   search fields and offsets.) */
static int face_glyph(const Face *f, unsigned code) {
    const unsigned char *m = f->data + f->cmap;
    uint32_t glyph = 0;

    if (f->cmapFormat == 4) {
        uint32_t segs = be16(m + 6) / 2, lo = 0, hi = segs;
        const unsigned char *ends = m + 14, *starts = ends + 2 * segs + 2, *deltas = starts + 2 * segs, *offsets = deltas + 2 * segs;
        if (code > 0xFFFF) { return 0; }
        while (lo < hi) { /* the first segment that ends at or after the character */
            uint32_t mid = (lo + hi) / 2;
            if (be16(ends + 2 * mid) < code) { lo = mid + 1; } else { hi = mid; }
        }
        if (lo == segs || be16(starts + 2 * lo) > code) { return 0; }
        if (be16(offsets + 2 * lo) == 0) {
            glyph = (code + be16(deltas + 2 * lo)) & 0xFFFF;
        } else {
            size_t at = (size_t)(offsets + 2 * lo - m) + be16(offsets + 2 * lo) + 2 * (size_t)(code - be16(starts + 2 * lo));
            if (at + 2 > f->cmapLen) { return 0; }
            glyph = be16(m + at);
            if (glyph != 0) { glyph = (glyph + be16(deltas + 2 * lo)) & 0xFFFF; }
        }
    } else {
        uint32_t lo = 0, hi = be32(m + 12);
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            const unsigned char *g = m + 16 + 12 * (size_t)mid;
            if (code < be32(g)) {
                hi = mid;
            } else if (code > be32(g + 4)) {
                lo = mid + 1;
            } else {
                glyph = be32(g + 8) + (code - be32(g));
                break;
            }
        }
    }
    return glyph < (uint32_t)f->glyphs ? (int)glyph : 0;
}

static void face_close(Face *f) {
    free(f->data);
    memset(f, 0, sizeof(*f));
}

/* Reads and checks a font file. NULL if it can be used, else the reason (and *f is empty). */
static const char *face_open(Face *f, const char *path) {
    FILE *fp = fopen(path, "rb");
    long size;
    const char *why;
    int x0, y0, x1, y1, h;
    uint32_t kern, kernLen;

    memset(f, 0, sizeof(*f));
    if (fp == NULL) { return "the file cannot be opened"; }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return "the file cannot be read";
    }
    if (size > FONT_FILE_MAX) {
        fclose(fp);
        return "the file is larger than 16 MB";
    }
    f->size = (size_t)size;
    f->data = malloc(f->size + 16); /* (a few spare bytes: nothing is meant to read them) */
    if (f->data == NULL || fread(f->data, 1, f->size, fp) != f->size) {
        fclose(fp);
        face_close(f);
        return "the file cannot be read";
    }
    fclose(fp);
    memset(f->data + f->size, 0, 16);
    why = face_check(f);
    if (why == NULL && !stbtt_InitFont(&f->info, f->data, 0)) { why = "stb_truetype does not take it"; }
    if (why == NULL) {
        /* Kerning from the plain kern table only, and only if its pairs are all inside it. The GPOS table (what
           stb_truetype would prefer) is a tree of offsets that it follows unchecked: not used. */
        f->info.gpos = 0;
        if (face_table(f, "kern", &kern, &kernLen) &&
            (kernLen < 18 || 18 + 6 * (size_t)be16(f->data + kern + 10) > kernLen)) { f->info.kern = 0; }
        h = face_glyph(f, 'H');
        if (h == 0 || !stbtt_GetGlyphBox(&f->info, h, &x0, &y0, &x1, &y1) || y1 - y0 < f->unitsPerEm / 8 || y1 - y0 < 4) {
            why = "it has no capital H to take the letter height from";
        } else {
            f->capUnits = y1 - y0;
        }
    }
    if (why != NULL) { face_close(f); }
    return why;
}

/* A glyph as an 8-bit coverage picture, like stbtt_GetGlyphBitmapSubpixel, but only if it has a sane size: the
   box in the glyph's header and the outline itself are looked at first. NULL: nothing to draw. free() it. */
static unsigned char *face_bitmap(const Face *f, int glyph, float sx, float sy, float shiftX, float shiftY,
                                  int *w, int *h, int *ox, int *oy) {
    stbtt_vertex *v = NULL;
    stbtt__bitmap bm;
    int n, i, x0, y0, x1, y1, lo = 32767, hi = -32768;

    if (!(sx > 0.0f) || !(sy > 0.0f) || sx > 64.0f || sy > 64.0f) { return NULL; }
    n = stbtt_GetGlyphShape(&f->info, glyph, &v);
    if (n <= 0 || v == NULL) {
        free(v);
        return NULL;
    }
    for (i = 0; i < n; i++) {
        int k, c[4] = {v[i].x, v[i].y, v[i].x, v[i].y};
        if (v[i].type == STBTT_vcurve) { c[2] = v[i].cx; c[3] = v[i].cy; }
        for (k = 0; k < 4; k++) {
            if (c[k] < lo) { lo = c[k]; }
            if (c[k] > hi) { hi = c[k]; }
        }
    }
    stbtt_GetGlyphBitmapBoxSubpixel(&f->info, glyph, sx, sy, shiftX, shiftY, &x0, &y0, &x1, &y1);
    bm.w = x1 - x0;
    bm.h = y1 - y0;
    bm.stride = bm.w;
    bm.pixels = NULL;
    /* (a real label is at most 2048 x 256 pixels; an outline far outside that is a damaged font, and would have
       the rasteriser cut curves the size of a house into pixel-long pieces) */
    if (bm.w > 0 && bm.h > 0 && bm.w <= 4096 && bm.h <= 2048 && (float)(hi - lo) * (sx > sy ? sx : sy) <= 16384.0f) {
        bm.pixels = malloc((size_t)bm.w * bm.h);
        if (bm.pixels != NULL) {
            stbtt_Rasterize(&bm, 0.35f, v, n, sx, sy, shiftX, shiftY, x0, y0, 1, NULL);
        }
    }
    free(v);
    *w = bm.w; *h = bm.h; *ox = x0; *oy = y0;
    return bm.pixels;
}

static int face_kern(const Face *f, int a, int b) { return stbtt_GetGlyphKernAdvance(&f->info, a, b); }

/* ------------------------------------------------------------------------------------------ the font list */

typedef struct MenuFont {
    char file[256], name[256];
    int id, usage; /* usage: 1 menu labels, 2 the shared text, 3 both */
} MenuFont;
static MenuFont *sFonts;
static int sFontCount, sFontsLoaded, sLabelsThere;
static char sFontDir[1024], sLabelDir[1024], sDataDir[1024];

/* The list of labels being made from the disc (see the top): */
static volatile int sMake; /* 1: its thread is at it; 2: the thread is done and what it left waits to be taken (MenuText_Poll) */
static int sMakeResult;    /* what MenuLabels_Make returned */
static int sLabelsOld;     /* a list of an older format is there: used should a new one not come about */
static int sWantedId;      /* the Menu font the settings ask for, kept while the list is awaited */
static void labels_take(void);

static int file_there(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp != NULL) { fclose(fp); }
    return fp != NULL;
}

/* What labels.tsv the folder has: 0 none, 1 the current layout, 2 an older layout. */
static int labels_state(void) {
    char path[1200], line[256];
    FILE *fp;
    int state = 2;
    snprintf(path, sizeof(path), "%s/labels.tsv", sLabelDir);
    fp = fopen(path, "rb");
    if (fp == NULL) { return 0; }
    if (fgets(line, sizeof(line), fp) != NULL && strstr(line, MENU_LABELS_VERSION) != NULL) { state = 1; }
    fclose(fp);
    return state;
}

/* Where the fonts are ($BT3_FONTS, fonts/ next to the program, the source checkout's folder) and where the
   labels made from the disc are ($BT3_MENU_FONT, gamedata/game-text). An empty name switches the thing off. */
static void menu_text_dirs(void) {
    const char *fonts = getenv("BT3_FONTS"), *labels = getenv("BT3_MENU_FONT"), *data = getenv("BT3_DATA");
    int state;
    if (fonts != NULL) {
        snprintf(sFontDir, sizeof(sFontDir), "%s", fonts);
    } else {
        snprintf(sFontDir, sizeof(sFontDir), "%s", file_there("fonts/fonts.tsv") ? "fonts" : "port/third_party/fonts");
    }
    if (labels != NULL) {
        snprintf(sLabelDir, sizeof(sLabelDir), "%s", labels);
    } else {
        snprintf(sLabelDir, sizeof(sLabelDir), "%s/game-text", data != NULL ? data : "gamedata");
    }
    snprintf(sDataDir, sizeof(sDataDir), "%s", data != NULL ? data : "gamedata");
    state = sLabelDir[0] != '\0' ? labels_state() : 0;
    sLabelsThere = state == 1;
    sLabelsOld = state == 2;
}

/* Makes the list of labels from the disc and says in the log, in one line, what came of it. In the thread, or at
   start in a run that has to be the same every time. Reads and writes files only. */
static void labels_make(void) {
    /* BT3_TEST_LABELS_SLOW=<ms>: waits that long first, and in a thread whatever the run (testing: a slow disk) */
    int slow = getenv("BT3_TEST_LABELS_SLOW") != NULL ? atoi(getenv("BT3_TEST_LABELS_SLOW")) : 0;
    char why[240];
    struct timespec a, b;

    if (slow > 0) { usleep((useconds_t)slow * 1000); }
    clock_gettime(CLOCK_MONOTONIC, &a);
    /* BT3_MENU_FONT_ANY_DISC=1: the archives are not checked to be the USA disc's (a changed archive: where its
       textures are numbered differently, labels are missing or wrong) */
    sMakeResult = MenuLabels_Make(sDataDir, sLabelDir, getenv("BT3_MENU_FONT_ANY_DISC") != NULL, why, sizeof(why));
    clock_gettime(CLOCK_MONOTONIC, &b);
    if (sMakeResult == MENU_LABELS_MADE) {
        fprintf(stderr, "bt3: menu text: the list of labels was made from the disc (%s, %ld ms) in %s\n", why,
                (long)((b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000), sLabelDir);
    } else {
        fprintf(stderr, "bt3: menu text: no list of labels was made: %s. %s\n", why,
                sLabelsOld ? "The older list is used." : "The Menu font setting offers only \"Original\".");
    }
}

static void *labels_thread(void *arg) {
    (void)arg;
    labels_make();
    __sync_synchronize();
    sMake = 2;
    return NULL;
}

/* Starts making the list, once. Returns at once, the work goes on in a thread of its own; in a recorded or re-run
   session (see the top) it returns when the list is made and taken. */
static void labels_start(void) {
    pthread_t th;
    pthread_attr_t attr;
    int started;

    if (getenv("BT3_TEST_LABELS_SLOW") == NULL &&
        (getenv("BT3_PAD_PLAY") != NULL || getenv("BT3_PAD_REC") != NULL || getenv("BT3_PAD_TABLE") != NULL || getenv("BT3_SYNCTEST") != NULL)) {
        labels_make();
        labels_take();
        return;
    }
    sMake = 1;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    started = pthread_create(&th, &attr, labels_thread, NULL) == 0;
    pthread_attr_destroy(&attr);
    if (!started) { /* (no thread to be had) */
        labels_make();
        labels_take();
    }
}

static void fonts_read(void);

static void menu_text_fonts(void) {
    int i, menu = 0;
    if (sFontsLoaded) { return; }
    sFontsLoaded = 1;
    menu_text_dirs();
    fonts_read();
    for (i = 0; i < sFontCount; i++) { menu |= sFonts[i].usage & 1; }
    if (!sLabelsThere && sLabelDir[0] != '\0') {
        if (menu) {
            labels_start(); /* (without a font for the menus there is nothing to make it for) */
        } else {
            sLabelsThere = sLabelsOld;
        }
    }
}

/* fonts.tsv: every font with what it is for. A font for the menus is offered once the list of labels is there. */
static void fonts_read(void) {
    char path[1200], line[1200];
    FILE *fp;
    if (!sFontDir[0]) { return; }
    snprintf(path, sizeof(path), "%s/fonts.tsv", sFontDir);
    fp = fopen(path, "rb");
    if (fp == NULL) { return; }
    while (fgets(line, sizeof(line), fp) != NULL) {
        MenuFont font, *next;
        char *name, *usage;
        size_t n, i;
        unsigned id = 2166136261u;
        if (line[0] == '#' || (name = strchr(line, '\t')) == NULL) { continue; }
        *name++ = '\0';
        font.usage = 3;
        usage = strchr(name, '\t');
        if (usage != NULL) { *usage++ = '\0'; font.usage = atoi(usage) & 3; }
        n = strlen(line);
        if (!n || n >= sizeof(font.file) || line[0] == '/' || strchr(line, '\\') || strchr(line, ':') ||
            strcmp(line, "..") == 0 || strncmp(line, "../", 3) == 0 || strstr(line, "/../")) { continue; }
        name[strcspn(name, "\r\n")] = '\0';
        if (!name[0] || strlen(name) >= sizeof(font.name) || font.usage == 0) { continue; }
        for (i = 0; i < n; i++) { id = (id ^ (unsigned char)line[i]) * 16777619u; }
        font.id = (int)(id & 0x7FFFFFFFu);
        if (!font.id) { font.id = 1; }
        strcpy(font.file, line); strcpy(font.name, name);
        next = realloc(sFonts, (size_t)(sFontCount + 1) * sizeof(*sFonts));
        if (next == NULL) { break; }
        sFonts = next; sFonts[sFontCount++] = font;
    }
    fclose(fp);
}

static int font_available(int choice, int shared) {
    menu_text_fonts();
    /* (no labels made from the disc, or not yet: a font has no menu text to draw) */
    return choice == 0 || (choice > 0 && choice <= sFontCount && (sFonts[choice - 1].usage & (shared ? 2 : sLabelsThere ? 1 : 0)) != 0);
}

/* Opens the font of a choice. Says in the log, once per file, why one cannot be used. */
static int font_open(Face *f, int choice) {
    char path[1400];
    const char *why;
    snprintf(path, sizeof(path), "%s/%s", sFontDir, sFonts[choice - 1].file);
    why = face_open(f, path);
    if (why != NULL) {
        fprintf(stderr, "bt3: font \"%s\" (%s) is not used: %s. The setting is back on \"Original\".\n",
                sFonts[choice - 1].name, path, why);
        sFonts[choice - 1].usage = 0; /* not offered again in this run */
        return 0;
    }
    return 1;
}

int MenuText_FontCount(void) {
    int n;
    pthread_mutex_lock(&sLock);
    menu_text_fonts();
    n = sFontCount + 1;
    pthread_mutex_unlock(&sLock);
    return n;
}

int MenuText_FontAvailable(int choice, int shared) {
    int r;
    pthread_mutex_lock(&sLock);
    r = font_available(choice, shared);
    pthread_mutex_unlock(&sLock);
    return r;
}

/* (the list is read once and only grows before that: the names stay where they are) */
const char *MenuText_FontName(int choice) {
    const char *r;
    pthread_mutex_lock(&sLock);
    menu_text_fonts();
    r = choice > 0 && choice <= sFontCount ? sFonts[choice - 1].name : "Original";
    pthread_mutex_unlock(&sLock);
    return r;
}

static int font_id(int choice) {
    menu_text_fonts();
    return choice > 0 && choice <= sFontCount ? sFonts[choice - 1].id : 0;
}

int MenuText_FontId(int choice) {
    int r;
    pthread_mutex_lock(&sLock);
    r = font_id(choice);
    pthread_mutex_unlock(&sLock);
    return r;
}

static int font_by_id(int id) {
    int i;
    menu_text_fonts();
    for (i = 1; i <= sFontCount; i++) {
        if (sFonts[i - 1].id == id) { return i; }
    }
    return 0;
}

int MenuText_RasterScale(int internalScale) { return internalScale < 2 ? 2 : internalScale > 4 ? 4 : internalScale; }

/* ------------------------------------------------------------------------------------------- menu labels */

typedef struct MenuLabel {
    uint64_t hash;
    int width, height, y, rowHeight, cap, baseline, center, margin, widthMilli, x, rowWidth, artWidth;
    unsigned top, bottom, outline, outlineBottom, bevelLight, bevelDark;
    int italic;
    int outlineMilli, outerMilli, shadow, bevelMilli;
    unsigned outerColor;
    char text[256];
} MenuLabel;

/* A drawn label: the box inside its row that has anything in it (scaled pixels, GS alpha), the rest of the row
   being transparent in the colour `empty`. */
typedef struct Row {
    int x, y, w, h;
    uint8_t *px;
    uint32_t empty;
} Row;

static MenuLabel sLabels[MENU_TEXT_MAX], sNameStyles[3];
static int sCount, sLabelsLoaded;
static volatile int sFontChoice;
static Face sFont;
static struct { int width, height; } sSizes[32]; /* the sizes of the textures that have labels */
static int sSizeCount;
static struct RowCache { int label, scale; unsigned used; Row row; } sRows[ROW_CACHE_MAX];
static unsigned sRowBytes, sRowClock;

static void rows_clear(void) {
    int i;
    for (i = 0; i < ROW_CACHE_MAX; i++) { free(sRows[i].row.px); }
    memset(sRows, 0, sizeof(sRows));
    sRowBytes = 0;
}

static void menu_text_name_styles(const char *dir) {
    char path[1200], line[256];
    FILE *fp;
    int selected;
    for (selected = 0; selected < 3; selected++) {
        MenuLabel *label = &sNameStyles[selected];
        memset(label, 0, sizeof(*label));
        label->outlineMilli = -1; label->shadow = 1;
        label->width = label->rowWidth = 512; label->height = label->rowHeight = selected == 2 ? 64 : 32;
        label->cap = selected == 2 ? 21 : 16; label->baseline = selected == 2 ? 44 : 22;
        label->center = selected == 2; label->margin = selected == 2 ? 8 : 4; label->widthMilli = 1000;
        label->top = selected ? 0xF4EF89 : 0xCBCBCB;
        label->bottom = selected ? 0xD2684B : 0xCBCBCB;
        label->outline = selected ? 0x2C151B : 0x460059;
        label->outlineBottom = label->outline;
    }
    snprintf(path, sizeof(path), "%s/name-styles.tsv", dir);
    fp = fopen(path, "rb");
    if (fp == NULL) { /* catalogs exported before dynamic stage names */
        snprintf(path, sizeof(path), "%s/song-styles.tsv", dir);
        fp = fopen(path, "rb");
    }
    if (fp == NULL) { return; }
    while (fgets(line, sizeof(line), fp) != NULL) {
        MenuLabel label;
        memset(&label, 0, sizeof(label));
        if (sscanf(line, "%d %d %d %d %d %x %x %x", &selected, &label.cap, &label.baseline, &label.margin,
                   &label.widthMilli, &label.top, &label.bottom, &label.outline) != 8 || selected < 0 || selected > 2 ||
            label.cap < 1 || label.cap > (selected == 2 ? 64 : 32) || label.baseline < 0 || label.baseline > (selected == 2 ? 64 : 32) || label.margin < 0 || label.margin >= 256 ||
            label.widthMilli < 500 || label.widthMilli > 2000 || label.top > 0xFFFFFF || label.bottom > 0xFFFFFF || label.outline > 0xFFFFFF) { continue; }
        label.width = label.rowWidth = 512; label.height = label.rowHeight = selected == 2 ? 64 : 32;
        label.center = selected == 2;
        label.outlineMilli = -1; label.shadow = 1; /* (as the disc names of labels.tsv: the outline by the size, the drop shadow on) */
        label.outlineBottom = label.outline;
        sNameStyles[selected] = label;
    }
    fclose(fp);
}

/* labels.tsv, once. It does not depend on the font. */
static void menu_text_labels(void) {
    char path[1200], line[1024];
    FILE *fp;
    int i, k;

    if (sLabelsLoaded) { return; }
    sLabelsLoaded = 1;
    menu_text_fonts();
    if (!sLabelsThere) { return; }
    snprintf(path, sizeof(path), "%s/labels.tsv", sLabelDir);
    fp = fopen(path, "rb");
    if (fp == NULL) { return; }
    while (sCount < MENU_TEXT_MAX && fgets(line, sizeof(line), fp) != NULL) {
        MenuLabel label;
        unsigned long long hash;
        int pos = 0, i;
        size_t n;
        if (line[0] == '#') { continue; }
        memset(&label, 0, sizeof(label));
        if (sscanf(line, "%llx\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%x\t%x\t%x\t%d\t%d\t%d\t%x\t%d\t%x\t%d\t%x\t%x\t%n",
                   &hash, &label.width, &label.height, &label.y, &label.rowHeight, &label.cap,
                   &label.baseline, &label.center, &label.margin, &label.widthMilli, &label.x, &label.rowWidth,
                   &label.artWidth, &label.top, &label.bottom, &label.outline, &label.italic,
                   &label.outlineMilli, &label.outerMilli, &label.outerColor, &label.shadow,
                   &label.outlineBottom, &label.bevelMilli, &label.bevelLight, &label.bevelDark, &pos) != 25 || pos == 0) { continue; }
        if (label.outlineMilli < -1 || label.outlineMilli > 8000 || label.outerMilli < 0 || label.outerMilli > 8000 ||
            label.outerColor > 0xFFFFFF || label.shadow < 0 || label.shadow > 1 ||
            label.outlineBottom > 0xFFFFFF || label.bevelMilli < 0 || label.bevelMilli > 4000 ||
            label.bevelLight > 0xFFFFFF || label.bevelDark > 0xFFFFFF) { continue; }
        n = strcspn(line + pos, "\r\n");
        if (n == 0 || n >= sizeof(label.text) || (label.width < 8 || label.width > MENU_TEXT_WIDTH || (label.width & (label.width - 1))) || label.height < 1 || label.height > MENU_TEXT_HEIGHT ||
            label.y < 0 || label.rowHeight < 1 || label.rowHeight > 64 || label.y > label.height - label.rowHeight ||
            label.cap < 1 || label.cap > 64 || label.baseline < 0 || label.baseline > label.rowHeight ||
            label.widthMilli < 500 || label.widthMilli > 2000 || label.x < 0 || label.rowWidth < 1 || label.x > label.width - label.rowWidth ||
            label.artWidth < 0 || label.artWidth > label.rowWidth || label.margin < 0 || label.margin * 2 >= label.rowWidth || label.top > 0xFFFFFF || label.bottom > 0xFFFFFF || label.outline > 0xFFFFFF || label.italic < 0 || label.italic > 1) {
            continue;
        }
        for (i = 0; i < (int)n; i++) {
            if ((unsigned char)line[pos + i] < 32) { break; }
        }
        if (i != (int)n) { continue; } /* no control characters silently dropped */
        memcpy(label.text, line + pos, n);
        label.hash = (uint64_t)hash;
        sLabels[sCount++] = label;
    }
    fclose(fp);
    for (i = 0; i < sCount; i++) {
        for (k = 0; k < sSizeCount && (sSizes[k].width != sLabels[i].width || sSizes[k].height != sLabels[i].height); k++) {
        }
        if (k == sSizeCount && sSizeCount < (int)(sizeof(sSizes) / sizeof(sSizes[0]))) {
            sSizes[sSizeCount].width = sLabels[i].width;
            sSizes[sSizeCount++].height = sLabels[i].height;
        } else if (k == sSizeCount) {
            sSizes[0].width = 0; /* more sizes than the list holds: every size is looked at */
        }
    }
    menu_text_name_styles(sLabelDir);
    fprintf(stderr, "bt3: menu text: %d labels from %s\n", sCount, sLabelDir);
}

static int label_size(int width, int height) {
    int k;
    if (sSizeCount > 0 && sSizes[0].width == 0) { return 1; }
    for (k = 0; k < sSizeCount; k++) {
        if (sSizes[k].width == width && sSizes[k].height == height) { return 1; }
    }
    return 0;
}

static void set_font(int choice) {
    if (!font_available(choice, 0)) { choice = 0; }
    if (choice == sFontChoice) { return; }
    face_close(&sFont);
    rows_clear();
    sFontChoice = 0;
    if (choice != 0) {
        menu_text_labels();
        if (sCount != 0 && font_open(&sFont, choice)) {
            sFontChoice = choice;
            fprintf(stderr, "bt3: menu font: %s\n", sFonts[choice - 1].name);
        }
    }
}

int MenuText_GetFont(void) { return sFontChoice; }

void MenuText_SetFont(int choice) {
    pthread_mutex_lock(&sLock);
    set_font(choice);
    pthread_mutex_unlock(&sLock);
}

/* (The settings window sets "Original" while the list is awaited, having nothing else to offer: that does not
   take back what the settings file asked for.) */
void MenuText_SetFontId(int id) {
    pthread_mutex_lock(&sLock);
    set_font(font_by_id(id));
    sWantedId = sMake != 0 && sFontChoice == 0 ? id : 0;
    pthread_mutex_unlock(&sLock);
}

int MenuText_FontIdToSave(void) {
    int r;
    pthread_mutex_lock(&sLock);
    r = sWantedId != 0 ? sWantedId : font_id(sFontChoice);
    pthread_mutex_unlock(&sLock);
    return r;
}

/* With the lock held: what the maker of the list left is taken. From here on the fonts for the menus are offered,
   and the one the settings asked for is in use. */
static void labels_take(void) {
    int id = sWantedId;
    sMake = 0;
    sWantedId = 0;
    sLabelsThere = sMakeResult == MENU_LABELS_MADE || sLabelsOld;
    if (!sLabelsThere) { return; }
    sLabelsLoaded = 0; /* (asked for before the list was there, it was found empty) */
    if (id != 0 && sFontChoice == 0) { set_font(font_by_id(id)); }
}

/* ---- the label of the PC's item of the main menu (src/menu/menu_a.c) ----
   Made from the player's disc (MenuLabels_MainItem) in a thread of its own, started with the first frame, so that it is
   there when the main menu is first set up; the menu waits for it if not. */
static unsigned char *sItemPic[2];
static volatile int sItem; /* 0 not started, 1 being made, 2 done (sItemPic NULL: not made) */

static void *item_thread(void *arg) {
    const char *data = getenv("BT3_DATA");
    (void)arg;
    MenuLabels_MainItem(data != NULL ? data : "gamedata", sItemPic);
    __sync_synchronize();
    sItem = 2;
    return NULL;
}

static void item_start(void) {
    pthread_t th;
    pthread_attr_t attr;
    int started;

    if (sItem != 0 || !__sync_bool_compare_and_swap(&sItem, 0, 1)) {
        return;
    }
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    started = pthread_create(&th, &attr, item_thread, NULL) == 0;
    pthread_attr_destroy(&attr);
    if (!started) {
        item_thread(NULL);
    }
}

/* The game (menu_a.c): the label for the free row of the label sheet `entry` (a texture entry of the game's, its
   address), as the item is (lit 0) or while chosen (1), into out (512 x 32 bytes, the game's): the values the sheet's
   pixels hold, each the colour of the sheet's table nearest the picture's. 0: no label from the disc. */
int Port_MainItemLabel(int entry, int lit, int out) {
    const uint8_t *e = (const uint8_t *)Port_Ptr((uint32_t)entry);
    const uint8_t *pic;
    uint8_t *dst = (uint8_t *)Port_Ptr((uint32_t)out);
    uint32_t clutAt;
    int waited = 0;

    item_start();
    while (sItem != 2 && waited < 5000) { /* (a few tenths of a second, once: it began with the first frame) */
        usleep(1000);
        waited++;
    }
    __sync_synchronize();
    if (sItem != 2 || lit < 0 || lit > 1 || (pic = sItemPic[lit]) == NULL) {
        return 0;
    }
    memcpy(&clutAt, e + 0x3C, 4);
    MenuLabels_ItemPixels(pic, (const uint8_t *)Port_Ptr(clutAt) + 0x60, dst);
    return 1;
}

void MenuText_Poll(void) {
    item_start();
    if (sMake != 2) { return; }
    pthread_mutex_lock(&sLock);
    if (sMake == 2) { labels_take(); }
    pthread_mutex_unlock(&sLock);
}

/* Decode strictly, skipping characters absent from the selected font. Malformed UTF-8 keeps the original row. */
static int menu_text_codes(Face *font, const char *text, int *glyphs) {
    const unsigned char *p = (const unsigned char *)text;
    int count = 0;
    while (*p) {
        unsigned c = *p++, minimum = 0;
        int n = 0, i, g;
        if (c >= 0xC2 && c <= 0xDF) { n = 1; c &= 31; minimum = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; c &= 15; minimum = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; c &= 7; minimum = 0x10000; }
        else if (c >= 128) { return 0; }
        for (i = 0; i < n; i++) {
            if ((*p & 0xC0) != 0x80) { return 0; }
            c = (c << 6) | (*p++ & 63);
        }
        if (c < minimum || c < 32 || (c >= 0x7F && c <= 0x9F) || c > 0x10FFFF ||
            (c >= 0xD800 && c <= 0xDFFF)) { return 0; }
        g = face_glyph(font, c);
        if (g == 0) { continue; }
        glyphs[count++] = g;
    }
    return count;
}

/* Draws a label into out (malloc'ed box). Only the box that can have anything in it is worked on: the letters'
   own, widened by the outline, the shadow's blur and the shadow's offset. 0: nothing drawn (the row keeps the
   original). */
static void menu_text_distance_line(const float *src, float *dst, int length, int stride,
                                    int *sites, float *bounds) {
    int q, k = -1;
    for (q = 0; q < length; q++) {
        float value = src[q*stride], crossing;
        if (value >= 1e11f) { continue; }
        while (k >= 0) {
            int p = sites[k];
            crossing = ((value + (float)q*q) - (src[p*stride] + (float)p*p)) / (2*(q-p));
            if (crossing > bounds[k]) { break; }
            k--;
        }
        if (k < 0) { k = 0; bounds[0] = -1e12f; }
        else { bounds[++k] = crossing; }
        sites[k] = q;
        bounds[k+1] = 1e12f;
    }
    if (k < 0) {
        for (q = 0; q < length; q++) { dst[q*stride] = 1e12f; }
        return;
    }
    k = 0;
    for (q = 0; q < length; q++) {
        int p;
        while (bounds[k+1] < q) { k++; }
        p = sites[k];
        dst[q*stride] = (float)(q-p)*(q-p) + src[p*stride];
    }
}

/* (in `font`: the Menu font's own, or the online screens' copy of it) */
static int menu_text_row(Face *font, const MenuLabel *label, int resolution, Row *out) {
    typedef struct Ink { unsigned char *bitmap; int w, h, x, y; } Ink;
    unsigned char *fill = NULL, *body = NULL, *blur = NULL, *outer = NULL;
    float *distance = NULL, *rowDistance = NULL, *bounds = NULL, *bevelDistance = NULL;
    int *sites = NULL;
    Ink *ink;
    int glyphs[256], len = menu_text_codes(font, label->text, glyphs);
    int i, x, y, c, advance, bearing, previous = 0, width = 0, ok = 0;
    int w = label->rowWidth * resolution, h = label->rowHeight * resolution, count;
    int radius = resolution, window = radius * 2 + 1;
    int padding = label->rowWidth < 32 ? label->rowWidth / 8 : 4;
    float border = label->rowWidth < 48 ? label->rowWidth / 16 : 3;
    float scaleY, scaleX;
    int inkLeft = 0, inkRight = 0, hasInk = 0;
    int bx0 = w, by0 = h, bx1 = 0, by1 = 0, grow, fx0, fy0, fw, fh;
    float pen, start, baseline = (float)label->baseline * resolution;
    float cap = label->cap * resolution;
    float available = w - (2 * label->margin + 2 * padding) * resolution;

    if (border < 1) { border = 1; }
    if (label->outlineMilli >= 0) { border = label->outlineMilli / 1000.0f; }
    scaleY = label->cap * resolution / (float)font->capUnits;
    scaleX = scaleY * label->widthMilli / 1000.0f;
    if (label->italic) { available -= cap * 0.22f; }
    out->x = out->y = out->w = out->h = 0;
    out->px = NULL;
    out->empty = (label->outline >> 16 & 255) | (label->outline & 0xFF00) | (label->outline & 255) << 16; /* r, g, b, alpha 0 as bytes */

    if (!len) { return 0; }
    for (i = 0; i < len; i++) {
        stbtt_GetGlyphHMetrics(&font->info, glyphs[i], &advance, &bearing);
        if (label->artWidth) {
            int x0, y0, x1, y1;
            int at = width + (previous ? face_kern(font, previous, glyphs[i]) : 0);
            if (stbtt_GetGlyphBox(&font->info, glyphs[i], &x0, &y0, &x1, &y1)) {
                if (!hasInk || at + x0 < inkLeft) { inkLeft = at + x0; }
                if (!hasInk || at + x1 > inkRight) { inkRight = at + x1; }
                hasInk = 1;
            }
        }
        width += advance + (previous ? face_kern(font, previous, glyphs[i]) : 0);
        previous = glyphs[i];
    }
    if (label->artWidth && hasInk && inkRight > inkLeft) {
        width = inkRight - inkLeft;
        scaleX = label->artWidth * resolution / (float)width * label->widthMilli / 1000.0f;
    }
    if (width <= 0 || !(available > 0.0f)) { return 0; }
    if (width * scaleX > available) {
        float ratio = available / (width * scaleX);
        scaleX *= ratio; scaleY *= ratio; cap *= ratio;
        baseline = h * 0.5f + (baseline - h * 0.5f) * ratio;
    }
    start = label->center ? (w - width * scaleX) * 0.5f : (float)label->margin * resolution;
    if (label->artWidth && hasInk) {
        start -= inkLeft * scaleX;
    } else if (!label->center) {
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&font->info, glyphs[0], scaleX, scaleY, &x0, &y0, &x1, &y1);
        start -= x0; /* align the glyph's visible edge, rather than its font-specific bearing */
    }
    /* the letters, and the box they cover in the row */
    ink = calloc((size_t)len, sizeof(*ink));
    if (ink == NULL) { return 0; }
    pen = start; previous = 0;
    for (i = 0; i < len; i++) {
        int ox = 0, oy = 0, px0, py0;
        if (previous) { pen += face_kern(font, previous, glyphs[i]) * scaleX; }
        if (!(pen > -65536.0f && pen < 65536.0f)) { break; }
        px0 = (int)floorf(pen); py0 = (int)floorf(baseline);
        ink[i].bitmap = face_bitmap(font, glyphs[i], scaleX, scaleY, pen - px0, baseline - py0, &ink[i].w, &ink[i].h, &ox, &oy);
        if (ink[i].bitmap != NULL) {
            int x0 = px0 + ox, y0 = py0 + oy, x1 = x0 + ink[i].w, y1 = y0 + ink[i].h;
            ink[i].x = x0; ink[i].y = y0;
            if (label->italic) { /* the shear below moves a pixel by up to this much, and spreads it over two */
                float a = (baseline - cap * 0.5f - y0) * 0.22f, b = (baseline - cap * 0.5f - (y1 - 1)) * 0.22f;
                x0 += (int)floorf(a < b ? a : b);
                x1 += (int)floorf(a > b ? a : b) + 1;
            }
            if (x0 < bx0) { bx0 = x0; }
            if (y0 < by0) { by0 = y0; }
            if (x1 > bx1) { bx1 = x1; }
            if (y1 > by1) { by1 = y1; }
        }
        stbtt_GetGlyphHMetrics(&font->info, glyphs[i], &advance, &bearing);
        pen += advance * scaleX; previous = glyphs[i];
    }
    if (bx0 < 0) { bx0 = 0; }
    if (by0 < 0) { by0 = 0; }
    if (bx1 > w) { bx1 = w; }
    if (by1 > h) { by1 = h; }
    if (bx1 <= bx0 || by1 <= by0) { /* letters without a visible pixel in the row: an empty row, as it was */
        ok = 1;
        goto done;
    }
    /* the outline reaches `grow` pixels from a letter, the shadow's blur `radius` more, and the shadow lies 4 to
       the right and 3 down of all that */
    grow = (int)ceilf((border + label->outerMilli / 1000.0f) * resolution + 0.5f) + 1 + radius;
    fx0 = bx0 - grow < 0 ? 0 : bx0 - grow;
    fy0 = by0 - grow < 0 ? 0 : by0 - grow;
    fw = (bx1 + grow + 4 * resolution > w ? w : bx1 + grow + 4 * resolution) - fx0;
    fh = (by1 + grow + 3 * resolution > h ? h : by1 + grow + 3 * resolution) - fy0;
    count = fw * fh;
    fill = calloc((size_t)count, 1);
    body = malloc((size_t)count);
    if (label->outerMilli) { outer = malloc((size_t)count); }
    blur = malloc((size_t)count);
    distance = malloc((size_t)count * sizeof(float));
    rowDistance = malloc((size_t)count * sizeof(float));
    sites = malloc((size_t)(fw > fh ? fw : fh) * sizeof(int));
    bounds = malloc((size_t)((fw > fh ? fw : fh) + 1) * sizeof(float));
    if (label->bevelMilli) { bevelDistance = malloc((size_t)count * sizeof(float)); }
    out->px = malloc((size_t)count * 4);
    if (fill == NULL || body == NULL || blur == NULL || distance == NULL || rowDistance == NULL ||
        sites == NULL || bounds == NULL || out->px == NULL || (label->outerMilli && outer == NULL) ||
        (label->bevelMilli && bevelDistance == NULL)) {
        free(out->px);
        out->px = NULL;
        goto done;
    }
    for (i = 0; i < len; i++) {
        const unsigned char *bitmap = ink[i].bitmap;
        int bw = ink[i].w, gh = ink[i].h;
        if (bitmap == NULL) { continue; }
        for (y = 0; y < gh; y++) {
            int py = ink[i].y + y - fy0;
            if (py < 0 || py >= fh) { continue; }
            for (x = 0; x < bw; x++) {
                int px = ink[i].x + x - fx0;
                if (label->italic) {
                    /* Shear around the cap midpoint, retaining baseline and antialiasing. */
                    float shifted = (px + fx0) + (baseline - cap * 0.5f - (py + fy0)) * 0.22f;
                    int left = (int)floorf(shifted), k;
                    float fraction = shifted - left;
                    for (k = 0; k < 2; k++) {
                        int tx = left + k - fx0;
                        unsigned char coverage = (unsigned char)(bitmap[y*bw+x] * (k ? fraction : 1-fraction) + 0.5f);
                        if (tx >= 0 && tx < fw) {
                            int at = py*fw+tx;
                            fill[at] = (unsigned char)fminf(255, fill[at] + coverage);
                        }
                    }
                    continue;
                }
                if (px >= 0 && px < fw && bitmap[y*bw+x] > fill[py*fw+px]) {
                    fill[py*fw+px] = bitmap[y*bw+x];
                }
            }
        }
    }
    /* Exact circular distances give every style rounded outlines, including diagonal corners. */
    for (i = 0; i < count; i++) {
        float uncovered = 1.0f - fill[i]/255.0f;
        distance[i] = fill[i] ? uncovered*uncovered : 1e12f;
    }
    for (y = 0; y < fh; y++) {
        menu_text_distance_line(distance+y*fw, rowDistance+y*fw, fw, 1, sites, bounds);
    }
    for (x = 0; x < fw; x++) {
        menu_text_distance_line(rowDistance+x, distance+x, fh, fw, sites, bounds);
    }
    for (i = 0; i < count; i++) { distance[i] = sqrtf(distance[i]); }
    for (y = 0; y < fh; y++) {
        for (x = 0; x < fw; x++) {
            int at = y*fw+x;
            float d = distance[at], coverage;
            coverage = fminf(1, fmaxf(0, border + 0.5f - d/resolution));
            coverage = coverage*coverage*(3-2*coverage);
            body[at] = (unsigned char)fmaxf(fill[at], coverage*255);
            if (outer != NULL) {
                float extraTop = 0;
                if (label->bevelMilli) {
                    int up = y >= resolution ? y-resolution : 0;
                    int down = y+resolution < fh ? y+resolution : fh-1;
                    float facing = (distance[up*fw+x] - distance[down*fw+x])/(2.0f*resolution);
                    extraTop = 0.6f*fminf(1, fmaxf(0, facing));
                }
                coverage = fminf(1, fmaxf(0, border + label->outerMilli/1000.0f +
                                            extraTop + 0.5f - d/resolution));
                coverage = coverage*coverage*(3-2*coverage);
                outer[at] = (unsigned char)fmaxf(body[at], coverage*255);
            }
        }
    }
    if (bevelDistance != NULL) { memcpy(bevelDistance, distance, (size_t)count * sizeof(float)); }
    /* Separable box blur for the offset shadow. */
    for (y = 0; y < fh; y++) {
        int sum = 0;
        for (x = 0; x <= radius && x < fw; x++) { sum += body[y*fw+x]; }
        for (x = 0; x < fw; x++) {
            blur[y*fw+x] = (unsigned char)(sum/window);
            if (x-radius >= 0) { sum -= body[y*fw+x-radius]; }
            if (x+radius+1 < fw) { sum += body[y*fw+x+radius+1]; }
        }
    }
    for (x = 0; x < fw; x++) {
        int sum = 0;
        for (y = 0; y <= radius && y < fh; y++) { sum += blur[y*fw+x]; }
        for (y = 0; y < fh; y++) {
            distance[y*fw+x] = sum/(window*255.0f)*0.65f;
            if (y-radius >= 0) { sum -= blur[(y-radius)*fw+x]; }
            if (y+radius+1 < fh) { sum += blur[(y+radius+1)*fw+x]; }
        }
    }
    for (y = 0; y < fh; y++) {
        float ramp = ((y + fy0) - (baseline - cap)) / fmaxf(1, cap - 1);
        float outlineRamp = ((y + fy0) - (baseline - cap - border*resolution)) /
                            fmaxf(1, cap + border*resolution*2);
        ramp = fminf(1, fmaxf(0, ramp));
        outlineRamp = fminf(1, fmaxf(0, outlineRamp));
        for (x = 0; x < fw; x++) {
            int at = y*fw+x, sx = x-4*resolution, sy = y-3*resolution;
            float f = fill[at]/255.0f, b = body[at]/255.0f;
            float o = outer != NULL ? outer[at]/255.0f : b;
            float shadow = label->shadow && sx >= 0 && sy >= 0 ? distance[sy*fw+sx] : 0;
            float alpha = o + shadow*(1-o);
            float outerBand = 0, innerBand = 0, bottomShade = 0, topShade = 0, rightShade = 0;
            if (bevelDistance != NULL && b > f) {
                float unit = fmaxf(1, label->bevelMilli*resolution/1000.0f);
                float radial = bevelDistance[at]/unit;
                int left = x >= resolution ? x-resolution : 0;
                int right = x+resolution < fw ? x+resolution : fw-1;
                outerBand = fminf(1, fmaxf(0, (radial-2.0f)/0.4f));
                innerBand = fminf(1, fmaxf(0, (1.5f-radial)/0.5f));
                bottomShade = fminf(1, fmaxf(0, ((y+fy0) - (baseline-2.5f*resolution))/(2.5f*resolution)));
                topShade = fminf(1, fmaxf(0, ((baseline-cap+1.5f*resolution)-(y+fy0))/(1.5f*resolution)));
                rightShade = fminf(1, fmaxf(0, (bevelDistance[y*fw+right] -
                                               bevelDistance[y*fw+left])/(2*resolution)));
            }
            for (c = 0; c < 3; c++) {
                int shift = 16-c*8;
                float color = ((label->top >> shift)&255)*(1-ramp) + ((label->bottom >> shift)&255)*ramp;
                float outline = ((label->outline >> shift)&255)*(1-bottomShade) +
                                ((label->outlineBottom >> shift)&255)*bottomShade;
                if (bevelDistance != NULL) {
                    float lip = ((label->bevelLight >> shift)&255)*(1-topShade*0.3f) +
                                ((label->outerColor >> shift)&255)*topShade*0.3f;
                    outline = outline*(1-outerBand) + lip*outerBand;
                    outline = outline*(1-innerBand*0.33f) + lip*innerBand*0.33f;
                    outline = outline*(1-bottomShade*innerBand*innerBand*0.7f) +
                              ((label->bevelDark >> shift)&255)*bottomShade*innerBand*innerBand*0.7f;
                    {
                        float core = (((label->outlineBottom >> shift)&255) +
                                      ((label->bevelDark >> shift)&255))*0.5f;
                        float lipRight = ((label->bevelLight >> shift)&255)*0.65f +
                                         ((label->outerColor >> shift)&255)*0.35f;
                        float rightColor = ((label->outlineBottom >> shift)&255)*(1-innerBand) + core*innerBand;
                        rightColor = rightColor*(1-outerBand) + lipRight*outerBand;
                        outline = outline*(1-rightShade) + rightColor*rightShade;
                    }
                } else {
                    outline = ((label->outline >> shift)&255)*(1-outlineRamp) +
                              ((label->outlineBottom >> shift)&255)*outlineRamp;
                }
                float premult = color*f + outline*(b-f);
                if (outer != NULL) { premult += ((label->outerColor >> shift)&255)*(o-b); }
                out->px[at*4+c] = alpha > 0 ? (unsigned char)(premult/alpha + 0.5f) : (unsigned char)outline;
            }
            out->px[at*4+3] = (unsigned char)(alpha*127.0f + 0.5f);
        }
    }
    out->x = fx0; out->y = fy0; out->w = fw; out->h = fh;
    ok = 1;
done:
    for (i = 0; i < len; i++) { free(ink[i].bitmap); }
    free(ink); free(fill); free(body); free(outer); free(blur); free(distance);
    free(rowDistance); free(sites); free(bounds); free(bevelDistance);
    return ok;
}

/* A label's drawn row, from the ones kept or drawn now. *owned: the caller frees row->px (it was not kept). */
static int row_get(int n, int scale, Row *row, int *owned) {
    int i, slot = -1;
    unsigned bytes;
    *owned = 0;
    for (i = 0; i < ROW_CACHE_MAX; i++) {
        if (sRows[i].scale == scale && sRows[i].label == n) {
            sRows[i].used = ++sRowClock;
            *row = sRows[i].row;
            return 1;
        }
    }
    if (!menu_text_row(&sFont, &sLabels[n], scale, row)) { return 0; }
    bytes = (unsigned)row->w * (unsigned)row->h * 4;
    *owned = 1;
    if (bytes > ROW_CACHE_BYTES / 4) { return 1; }
    for (;;) { /* room: a free place and the bytes, the longest unused go first */
        int oldest = -1;
        slot = -1;
        for (i = 0; i < ROW_CACHE_MAX; i++) {
            if (sRows[i].scale == 0) {
                slot = i;
            } else if (oldest < 0 || sRows[i].used < sRows[oldest].used) {
                oldest = i;
            }
        }
        if ((slot >= 0 && sRowBytes + bytes <= ROW_CACHE_BYTES) || oldest < 0) { break; }
        sRowBytes -= (unsigned)sRows[oldest].row.w * (unsigned)sRows[oldest].row.h * 4;
        free(sRows[oldest].row.px);
        memset(&sRows[oldest], 0, sizeof(sRows[oldest]));
    }
    if (slot >= 0) {
        sRows[slot].label = n;
        sRows[slot].scale = scale;
        sRows[slot].used = ++sRowClock;
        sRows[slot].row = *row;
        sRowBytes += bytes;
        *owned = 0;
    }
    return 1;
}

/* Puts a drawn row into a picture `stride` pixels wide, its row rectangle's corner at (x, y). */
static void row_paste(uint8_t *dst, int stride, int x, int y, int rowWidth, int rowHeight, const Row *row) {
    int i, k;
    for (i = 0; i < rowHeight; i++) {
        uint32_t *line = (uint32_t *)(void *)(dst + ((size_t)(y + i) * stride + x) * 4);
        uint32_t empty;
        memcpy(&empty, &row->empty, 4); /* (the bytes r, g, b, 0 in memory order) */
        for (k = 0; k < rowWidth; k++) { memcpy(&line[k], &empty, 4); }
        if (i >= row->y && i < row->y + row->h) {
            memcpy(dst + ((size_t)(y + i) * stride + x + row->x) * 4, row->px + (size_t)(i - row->y) * row->w * 4, (size_t)row->w * 4);
        }
    }
}

uint8_t *MenuText_Render(const uint8_t *rgba, int width, int height, int indexed, int scale, int *outWidth, int *outHeight) {
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i, bytes;
    int n, x, y, changed = 0, rw, rh;
    uint8_t *result = NULL;
    if (sFontChoice == 0 || !indexed) { return NULL; } /* Original: use the untouched game sprites. */
    pthread_mutex_lock(&sLock);
    if (sFontChoice == 0 || sCount == 0 || rgba == NULL || !label_size(width, height) || scale < 1 || scale > 4 ||
        outWidth == NULL || outHeight == NULL) {
        pthread_mutex_unlock(&sLock);
        return NULL;
    }
    bytes = (size_t)width * height * 4;
    rw = width * scale; rh = height * scale;
    for (i = 0; i < bytes; i++) { hash = (hash ^ rgba[i]) * UINT64_C(1099511628211); }
    for (n = 0; n < sCount; n++) {
        const MenuLabel *label = &sLabels[n];
        Row row;
        int owned;
        if (label->hash != hash || label->width != width || label->height != height) { continue; }
        if (result == NULL) {
            result = malloc((size_t)rw * rh * 4);
            if (result == NULL) { break; }
            for (y = 0; y < height; y++) { /* the original, each pixel a block */
                const uint8_t *src = rgba + (size_t)y * width * 4;
                uint8_t *dst = result + (size_t)y * scale * rw * 4;
                int k;
                for (x = 0; x < width; x++) {
                    for (k = 0; k < scale; k++) { memcpy(dst + ((size_t)x * scale + k) * 4, src + (size_t)x * 4, 4); }
                }
                for (k = 1; k < scale; k++) { memcpy(dst + (size_t)k * rw * 4, dst, (size_t)rw * 4); }
            }
        }
        if (row_get(n, scale, &row, &owned)) {
            row_paste(result, rw, label->x * scale, label->y * scale, label->rowWidth * scale, label->rowHeight * scale, &row);
            if (owned) { free(row.px); }
            changed++;
        }
    }
    pthread_mutex_unlock(&sLock);
    if (!changed) { free(result); return NULL; }
    *outWidth = rw; *outHeight = rh;
    return result;
}

static uint8_t *menu_text_render_name(const char *text, int kind, int scale, int *outWidth, int *outHeight) {
    MenuLabel label;
    Row row;
    uint8_t *result;
    size_t bytes, i, length;
    if (sFontChoice == 0 || sCount == 0 || text == NULL || kind < 0 || kind > 2 || scale < 1 || scale > 4 ||
        outWidth == NULL || outHeight == NULL) { return NULL; }
    length = strlen(text);
    if (length == 0 || length >= sizeof(label.text)) { return NULL; }
    label = sNameStyles[kind];
    memcpy(label.text, text, length + 1);
    bytes = (size_t)512 * label.rowHeight * scale * scale * 4;
    result = malloc(bytes);
    if (result == NULL) { return NULL; }
    if (!menu_text_row(&sFont, &label, scale, &row)) { free(result); return NULL; }
    row_paste(result, 512 * scale, 0, 0, 512 * scale, label.rowHeight * scale, &row);
    free(row.px);
    /* Menu texture pixels use PS2 alpha; the ImGui overlay uses conventional RGBA. */
    for (i = 3; i < bytes; i += 4) { result[i] = (uint8_t)((unsigned)result[i] * 255 / 128); }
    *outWidth = 512 * scale; *outHeight = label.rowHeight * scale;
    return result;
}

uint8_t *MenuText_RenderSong(const char *text, int selected, int scale, int *outWidth, int *outHeight) {
    uint8_t *r;
    if (selected < 0 || selected > 1) { return NULL; }
    pthread_mutex_lock(&sLock);
    r = menu_text_render_name(text, selected, scale, outWidth, outHeight);
    pthread_mutex_unlock(&sLock);
    return r;
}

uint8_t *MenuText_RenderStage(const char *text, int scale, int *outWidth, int *outHeight) {
    uint8_t *r;
    pthread_mutex_lock(&sLock);
    r = menu_text_render_name(text, 2, scale, outWidth, outHeight);
    pthread_mutex_unlock(&sLock);
    return r;
}

/* ------------------------------------------------------------------------------- the game's shared text */

/* The game's font sheets pack four glyph planes into one indexed texture. Each CLUT selects one plane.
   Only glyph cells are replaced; sprites, tags and the game's shadow/color/clip passes stay unchanged. */
typedef struct SharedGlyph { uint16_t code, u, v; } SharedGlyph;
typedef struct SharedSheet {
    int tbp, cbp, count, cap, baseline, calibrated;
    SharedGlyph *glyphs;
} SharedSheet;
typedef struct SharedRect { int x, y, w, h, top, bottom; } SharedRect;
typedef struct SharedLayout {
    int choice, cap, baseline, width, sourceHeight, height;
    SharedRect *rects;
} SharedLayout;
static SharedSheet sSharedSheets[7];
static SharedLayout sSharedLayouts[7][4];
static volatile int sSharedChoice;
static Face sSharedFont;
static uint32_t sSharedGeneration;

int MenuText_GetSharedFont(void) { return sSharedChoice; }

static void set_shared_font(int choice) {
    if (!font_available(choice, 1)) { choice = 0; }
    if (choice == sSharedChoice) { return; }
    face_close(&sSharedFont);
    sSharedChoice = 0;
    if (choice != 0 && font_open(&sSharedFont, choice)) {
        sSharedChoice = choice;
        fprintf(stderr, "bt3: dialogue / UI font: %s\n", sFonts[choice - 1].name);
    }
}

void MenuText_SetSharedFont(int choice) {
    pthread_mutex_lock(&sLock);
    set_shared_font(choice);
    pthread_mutex_unlock(&sLock);
}

void MenuText_SetSharedFontId(int id) {
    pthread_mutex_lock(&sLock);
    set_shared_font(font_by_id(id));
    pthread_mutex_unlock(&sLock);
}

static SharedSheet *shared_sheet(int tbp, int cbp) {
    int i;
    for (i = 0; i < 7; i++) {
        if (sSharedSheets[i].count && sSharedSheets[i].tbp == tbp && cbp >= sSharedSheets[i].cbp &&
            cbp < sSharedSheets[i].cbp + 4) { return &sSharedSheets[i]; }
    }
    return NULL;
}

/* What of the settings here a decoded texture depends on, for the renderer's texture cache: 0 with both
   settings on "Original", and for every texture neither setting would touch. */
uint32_t MenuText_TexKey(int width, int height, int indexed, int tbp, int cbp, int scale) {
    uint32_t key = 0;
    if (sFontChoice == 0 && sSharedChoice == 0) { return 0; }
    pthread_mutex_lock(&sLock);
    if (sFontChoice != 0 && indexed && label_size(width, height)) {
        key ^= (uint32_t)scale * 0x9E3779B9u ^ (uint32_t)sFontChoice * 0x85EBCA6Bu;
    }
    if (sSharedChoice != 0 && shared_sheet(tbp, cbp) != NULL) {
        /* (the generation: the atlas layout follows registration and H calibration) */
        key ^= (uint32_t)sSharedChoice * 0xC2B2AE35u ^ (uint32_t)scale * 0x27D4EB2Du ^ sSharedGeneration * 0x165667B1u;
    }
    pthread_mutex_unlock(&sLock);
    return key;
}

void MenuText_RegisterGlyphs(int tbp, int cbp, const void *glyphs, int count, int lineHeight) {
    SharedSheet *sheet;
    int slot = cbp >= 0x2A00 ? (cbp - 0x2A00) / 0x208 : -1, i;
    if (slot < 0 || slot >= 7 || cbp != 0x2A00 + slot * 0x208 || tbp != cbp + 8 ||
        glyphs == NULL || count < 1 || count > 65536 || lineHeight < 1 || lineHeight > 64) { return; }
    pthread_mutex_lock(&sLock);
    sheet = &sSharedSheets[slot];
    if (sheet->count == count && sheet->tbp == tbp && sheet->cbp == cbp &&
        memcmp(sheet->glyphs, glyphs, (size_t)count * sizeof(*sheet->glyphs)) == 0) {
        pthread_mutex_unlock(&sLock); return;
    }
    if (sheet->count != count) {
        SharedGlyph *copy = realloc(sheet->glyphs, (size_t)count * sizeof(*copy));
        if (copy == NULL) { pthread_mutex_unlock(&sLock); return; }
        sheet->glyphs = copy;
    }
    memcpy(sheet->glyphs, glyphs, (size_t)count * sizeof(*sheet->glyphs));
    sheet->tbp = tbp; sheet->cbp = cbp; sheet->count = count;
    sheet->cap = lineHeight;
    for (i = 0; i < count; i++) {
        if (sheet->glyphs[i].code == 'H') {
            int h = (sheet->glyphs[i].v >> 10) & 31;
            if (h) { sheet->cap = h; }
            break;
        }
    }
    sheet->baseline = sheet->cap; sheet->calibrated = 0;
    sSharedGeneration++;
    for (i = 0; i < 4; i++) {
        SharedLayout *layout = &sSharedLayouts[slot][i];
        free(layout->rects); memset(layout, 0, sizeof(*layout));
    }
    pthread_mutex_unlock(&sLock);
}

/* Each plane gets its own packed atlas. Padding follows the selected font's actual bounds, so
   descenders and accents fit without changing the cap height or borrowing pixels from the next glyph. */
static SharedLayout *menu_text_shared_layout(SharedSheet *sheet, int plane, int width, int height) {
    SharedLayout *layout = &sSharedLayouts[sheet - sSharedSheets][plane];
    int i, x = 1, y = 1, rowHeight = 0, bottom = 1;
    float sy = (float)sheet->cap / sSharedFont.capUnits;
    if (layout->rects && layout->choice == sSharedChoice && layout->cap == sheet->cap &&
        layout->baseline == sheet->baseline && layout->width == width && layout->sourceHeight == height) { return layout; }
    free(layout->rects); memset(layout, 0, sizeof(*layout));
    layout->rects = calloc((size_t)sheet->count, sizeof(*layout->rects));
    if (!layout->rects) { return NULL; }
    for (i = 0; i < sheet->count; i++) {
        SharedGlyph *g = &sheet->glyphs[i];
        SharedRect *r = &layout->rects[i];
        int gx = g->u & 1023, gy = g->v & 1023, w = (g->u >> 10) & 31, h = (g->v >> 10) & 31;
        int x0, y0, x1, y1, glyph;
        if (((g->u >> 15) | ((g->v >> 15) << 1)) != plane || !w || !h || gx+w > width || gy+h > height || w + 2 > width) { continue; }
        glyph = face_glyph(&sSharedFont, g->code);
        if (glyph && stbtt_GetGlyphBox(&sSharedFont.info, glyph, &x0, &y0, &x1, &y1) && x1 > x0 && y1 > y0) {
            int top = (int)floorf(sheet->baseline - y1 * sy);
            int end = (int)ceilf(sheet->baseline - y0 * sy);
            r->top = top < 0 ? -top : 0; r->bottom = end > h ? end-h : 0;
            if (r->top > 64) { r->top = 64; } /* (a real font needs a few pixels for an accent or a descender) */
            if (r->bottom > 64) { r->bottom = 64; }
        }
        r->w = w; r->h = h + r->top + r->bottom;
        if (x + w + 1 > width) { x = 1; y += rowHeight + 2; rowHeight = 0; }
        r->x = x; r->y = y; x += w + 2;
        if (r->h > rowHeight) { rowHeight = r->h; }
        if (y + r->h + 1 > bottom) { bottom = y + r->h + 1; }
    }
    layout->height = 1;
    while (layout->height < bottom) { layout->height *= 2; }
    layout->choice = sSharedChoice; layout->cap = sheet->cap; layout->baseline = sheet->baseline;
    layout->width = width; layout->sourceHeight = height;
    if (layout->height > 4096) { /* does not fit a texture: this sheet keeps the game's glyphs */
        free(layout->rects); memset(layout, 0, sizeof(*layout));
        return NULL;
    }
    return layout;
}

static uint8_t *menu_text_glyphs(const uint8_t *rgba, int width, int height, int tbp, int cbp,
                               int scale, int *outWidth, int *outHeight) {
    SharedSheet *sheet;
    uint8_t *result = NULL;
    SharedLayout *layout;
    int i, plane, rw, rh, changed = 0;
    if (!sSharedChoice || !rgba || width < 1 || width > 1024 || height < 1 || height > 1024 ||
        scale < 1 || scale > 4 || !outWidth || !outHeight) { return NULL; }
    sheet = shared_sheet(tbp, cbp);
    if (!sheet) { return NULL; }
    plane = cbp - sheet->cbp; rw = width * scale; rh = height * scale;
    if (!sheet->calibrated) {
        for (i = 0; i < sheet->count; i++) {
            SharedGlyph *g = &sheet->glyphs[i];
            int x, y, top = 64, bottom = -1;
            int gx = g->u & 1023, gy = g->v & 1023, w = (g->u >> 10) & 31, h = (g->v >> 10) & 31;
            if (g->code != 'H' || ((g->u >> 15) | ((g->v >> 15) << 1)) != plane ||
                gx + w > width || gy + h > height) { continue; }
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    if (rgba[((size_t)(gy+y) * width + gx+x)*4+3] >= 64) {
                        if (y < top) { top = y; }
                        if (y > bottom) { bottom = y; }
                    }
                }
            }
            if (bottom >= top) {
                sheet->cap = bottom - top + 1; sheet->baseline = bottom + 1; sheet->calibrated = 1;
                sSharedGeneration++;
            }
            break;
        }
    }
    layout = menu_text_shared_layout(sheet, plane, width, height);
    if (!layout) { return NULL; }
    rw = width * scale; rh = layout->height * scale;
    result = calloc((size_t)rw * rh, 4);
    if (!result) { return NULL; }
    for (i = 0; i < sheet->count; i++) {
        SharedGlyph *g = &sheet->glyphs[i];
        SharedRect *r = &layout->rects[i];
        int gx = g->u & 1023, gy = g->v & 1023, w = (g->u >> 10) & 31, h = (g->v >> 10) & 31;
        int bx0, by0, bx1, by1, bw, bh, ox, oy, x, y, glyph;
        int left = w, maxAlpha = 0;
        uint8_t ink[3] = {128,128,128};
        float sy, sx;
        unsigned char *bitmap;
        if (!r->w) { continue; }
        /* Keep the original pixels for missing glyphs, including spaces and symbols. */
        for (y = 0; y < h * scale; y++) {
            for (x = 0; x < w * scale; x++) {
                memcpy(result + ((size_t)((r->y+r->top)*scale+y) * rw + r->x*scale+x)*4,
                       rgba + ((size_t)(gy+y/scale) * width + gx+x/scale)*4, 4);
            }
        }
        glyph = face_glyph(&sSharedFont, g->code);
        if (!glyph || !stbtt_GetGlyphBox(&sSharedFont.info, glyph, &bx0, &by0, &bx1, &by1) || bx1 <= bx0 || by1 <= by0) { continue; }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                const uint8_t *src = rgba + ((size_t)(gy+y) * width + gx+x)*4;
                if (src[3] >= 32 && x < left) { left = x; }
                if (src[3] > maxAlpha) { maxAlpha = src[3]; memcpy(ink, src, 3); }
            }
        }
        if (!maxAlpha || left == w) { continue; }
        sy = (float)sheet->cap * scale / sSharedFont.capUnits;
        /* Keep the game's advance width. Only compress a glyph when its ink exceeds its original cell. */
        sx = fminf(sy, (float)((w-left) * scale) / (bx1 - bx0));
        bitmap = face_bitmap(&sSharedFont, glyph, sx, sy, 0.0f, 0.0f, &bw, &bh, &ox, &oy);
        if (!bitmap) { continue; }
        for (y = 0; y < r->h * scale; y++) {
            memset(result + ((size_t)(r->y * scale + y) * rw + r->x * scale) * 4, 0, (size_t)w * scale * 4);
        }
        for (y = 0; y < bh; y++) {
            int py = (sheet->baseline + r->top) * scale + oy + y;
            for (x = 0; x < bw; x++) {
                int px = left * scale + x;
                if (px < w * scale && py >= 0 && py < r->h * scale) {
                    uint8_t *dst = result + ((size_t)(r->y * scale + py) * rw + r->x * scale + px) * 4;
                    memcpy(dst, ink, 3); /* 0x80 is neutral under GS color modulation */
                    dst[3] = (uint8_t)((bitmap[y*bw+x] * (unsigned)maxAlpha + 127) / 255);
                }
            }
        }
        free(bitmap); changed = 1;
    }
    if (!changed) { free(result); return NULL; }
    *outWidth = rw; *outHeight = rh;
    return result;
}

uint8_t *MenuText_RenderGlyphs(const uint8_t *rgba, int width, int height, int tbp, int cbp,
                             int scale, int *outWidth, int *outHeight) {
    uint8_t *result;
    if (!sSharedChoice) { return NULL; }
    pthread_mutex_lock(&sLock);
    result = menu_text_glyphs(rgba, width, height, tbp, cbp, scale, outWidth, outHeight);
    pthread_mutex_unlock(&sLock);
    return result;
}

int MenuText_MapGlyph(int tbp, int cbp, int width, int height,
                     float u0, float v0, float u1, float v1, float *rect) {
    SharedSheet *sheet;
    int i, found = 0;
    if (!sSharedChoice || !rect || width < 1 || width > 1024 || height < 1 || height > 1024) { return 0; }
    pthread_mutex_lock(&sLock);
    sheet = sSharedChoice ? shared_sheet(tbp, cbp) : NULL;
    if (sheet != NULL) {
        SharedLayout *layout = menu_text_shared_layout(sheet, cbp-sheet->cbp, width, height);
        for (i = 0; layout != NULL && i < sheet->count; i++) {
            SharedGlyph *g = &sheet->glyphs[i]; SharedRect *r = &layout->rects[i];
            int gx = g->u & 1023, gy = g->v & 1023, w = (g->u >> 10) & 31, h = (g->v >> 10) & 31;
            if (!r->w || fabsf(u0-(gx+0.5f)) > 0.01f || fabsf(v0-(gy+0.5f)) > 0.01f ||
                fabsf(u1-u0-w) > 0.01f || fabsf(v1-v0-h) > 0.01f) { continue; }
            rect[0]=r->x; rect[1]=r->y; rect[2]=r->w; rect[3]=r->h;
            rect[4]=r->top; rect[5]=r->bottom; rect[6]=layout->width; rect[7]=layout->height;
            found = 1; break;
        }
    }
    pthread_mutex_unlock(&sLock);
    return found;
}

/* --------------------------------------------------------------------- lines for the PC's own screens */

/* Tenkaichi Budokai Online (src/menu/port_tbo.c, gs/tbo_text.c) writes its lines with the fonts of this list: its
   headings and buttons with the Menu font, its talk box with the Dialogue / UI font, and with a setting on
   "Original" with the first font of fonts.tsv for that use (any font of the list if none is for it). Each of the
   two kinds keeps its own copy of its font, opened when first needed and again when the setting changes. */
static Face sLineFace[2];
static int sLineChoice[2];       /* the font in sLineFace (0: none) */
static uint32_t sLineGeneration; /* changes when one of the two changes */

static int line_choice(int kind) {
    int choice = kind == 0 ? sFontChoice : sSharedChoice, i;
    menu_text_fonts();
    if (choice > 0 && choice <= sFontCount && sFonts[choice - 1].usage != 0) { return choice; }
    for (i = 0; i < sFontCount; i++) {
        if (sFonts[i].usage & (kind == 0 ? 1 : 2)) { return i + 1; }
    }
    for (i = 0; i < sFontCount; i++) {
        if (sFonts[i].usage != 0) { return i + 1; }
    }
    return 0;
}

/* (with the lock held) */
static Face *line_face(int kind) {
    int tries;
    for (tries = 0; tries < 4; tries++) { /* (a font that cannot be used is taken off the list: the next one) */
        int choice = line_choice(kind);
        if (choice == sLineChoice[kind]) { break; }
        face_close(&sLineFace[kind]);
        sLineChoice[kind] = 0;
        sLineGeneration++;
        if (choice == 0) { break; }
        if (font_open(&sLineFace[kind], choice)) { sLineChoice[kind] = choice; }
    }
    return sLineChoice[kind] != 0 ? &sLineFace[kind] : NULL;
}

/* The next character of UTF-8 text, 0 at its end; a malformed or control character is a question mark. */
static unsigned line_next(const unsigned char **text) {
    const unsigned char *p = *text;
    unsigned c = *p++, minimum = 0;
    int n = 0, i;
    if (c == 0) { return 0; }
    if (c >= 0xC2 && c <= 0xDF) { n = 1; c &= 31; minimum = 0x80; }
    else if (c >= 0xE0 && c <= 0xEF) { n = 2; c &= 15; minimum = 0x800; }
    else if (c >= 0xF0 && c <= 0xF4) { n = 3; c &= 7; minimum = 0x10000; }
    else if (c >= 0x80) { *text = p; return '?'; }
    for (i = 0; i < n; i++) {
        if ((*p & 0xC0) != 0x80) { *text = p; return '?'; }
        c = (c << 6) | (*p++ & 63);
    }
    *text = p;
    if (c < minimum || c < 32 || (c >= 0x7F && c <= 0x9F) || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) { return '?'; }
    return c;
}

/* Lays out (and with `out`, draws) a line: capitals `cap` pixels high, `pad` pixels of room on every side. The
   picture is `*w` x `*h` coverage bytes (0..255), the baseline `*base` rows down, the pen starting `pad` pixels in;
   returns how far the pen goes, in pixels (0: no font, or nothing to write). A character the font does not have is
   a question mark. */
#define LINE_CHARS 160
static int line_layout(int kind, const char *text, int cap, int pad, uint8_t **out, int *w, int *h, int *base) {
    const unsigned char *p = (const unsigned char *)text;
    Face *f = line_face(kind);
    int glyphs[LINE_CHARS], count = 0, i, ascent, descent, gap, adv, lsb, prev = 0, width, height, top;
    float scale, x = 0.0f, pen[LINE_CHARS];
    unsigned c;

    if (f == NULL || text == NULL || cap < 4 || cap > 256 || pad < 0 || pad > 64) { return 0; }
    scale = (float)cap / (float)f->capUnits;
    while ((c = line_next(&p)) != 0 && count < LINE_CHARS) {
        int g = face_glyph(f, c);
        if (g == 0) { g = face_glyph(f, '?'); }
        if (g == 0) { continue; }
        if (prev != 0 && f->info.kern) { x += (float)face_kern(f, prev, g) * scale; }
        pen[count] = x;
        glyphs[count++] = g;
        stbtt_GetGlyphHMetrics(&f->info, g, &adv, &lsb);
        x += (float)adv * scale;
        prev = g;
    }
    if (count == 0 || x > 4096.0f) { return 0; }
    stbtt_GetFontVMetrics(&f->info, &ascent, &descent, &gap);
    top = (int)ceilf((float)ascent * scale);
    width = (int)ceilf(x) + 2 * pad;
    height = top + (int)ceilf((float)-descent * scale) + 2 * pad;
    if (height < cap + 2 * pad) { height = cap + 2 * pad; }
    *w = width; *h = height; *base = pad + top;
    if (out != NULL) {
        uint8_t *px = calloc((size_t)width * height, 1);
        *out = px;
        for (i = 0; px != NULL && i < count; i++) {
            int gw, gh, ox, oy, gx, gy;
            float shift = pen[i] - floorf(pen[i]);
            unsigned char *g = face_bitmap(f, glyphs[i], scale, scale, shift, 0.0f, &gw, &gh, &ox, &oy);
            if (g == NULL) { continue; }
            for (gy = 0; gy < gh; gy++) {
                int y = *base + oy + gy;
                if (y < 0 || y >= height) { continue; }
                for (gx = 0; gx < gw; gx++) {
                    int xx = pad + (int)floorf(pen[i]) + ox + gx;
                    uint8_t v = g[(size_t)gy * gw + gx];
                    if (xx >= 0 && xx < width && v > px[(size_t)y * width + xx]) { px[(size_t)y * width + xx] = v; }
                }
            }
            free(g);
        }
    }
    return (int)ceilf(x);
}

uint32_t MenuText_LineGeneration(void) {
    uint32_t r;
    pthread_mutex_lock(&sLock);
    line_face(0);
    line_face(1);
    r = sLineGeneration;
    pthread_mutex_unlock(&sLock);
    return r;
}

int MenuText_LineMeasure(int kind, const char *text, int cap, int pad, int *w, int *h, int *base) {
    int r;
    if (kind < 0 || kind > 1) { return 0; }
    pthread_mutex_lock(&sLock);
    r = line_layout(kind, text, cap, pad, NULL, w, h, base);
    pthread_mutex_unlock(&sLock);
    return r;
}

uint8_t *MenuText_LineRender(int kind, const char *text, int cap, int pad, int *w, int *h, int *base) {
    uint8_t *px = NULL;
    if (kind < 0 || kind > 1) { return NULL; }
    pthread_mutex_lock(&sLock);
    if (line_layout(kind, text, cap, pad, &px, w, h, base) == 0) {
        free(px);
        px = NULL;
    }
    pthread_mutex_unlock(&sLock);
    return px;
}

/* A heading of the online screens, drawn as the main menu's replaced headings are (menu_text_row with the colours
   of menu_labels.c's "main-heading": gold to red, a brown outline with its bevel, a light rim round it), in their
   row's measures: capitals HEADING_CAP pixels of the disc's texture, drawn HEADING_RES times larger. */
#define HEADING_CAP 22
#define HEADING_BASE 30   /* the baseline, from the row's top */
#define HEADING_HEIGHT 42 /* the row */
#define HEADING_MARGIN 8  /* left of the letters' edge, and right of their end */
#define HEADING_RES 4

static int heading_label(const char *text, MenuLabel *label) {
    int adv, w, h, base;
    size_t length = text != NULL ? strlen(text) : 0;
    if (length == 0 || length >= sizeof(label->text)) { return 0; }
    adv = line_layout(0, text, HEADING_CAP, 0, NULL, &w, &h, &base);
    if (adv <= 0 || adv > 1000) { return 0; }
    memset(label, 0, sizeof(*label));
    label->rowWidth = adv + 2 * HEADING_MARGIN + 2 * 4 + 2; /* (4: menu_text_row's own room inside the margins) */
    label->rowHeight = HEADING_HEIGHT;
    label->cap = HEADING_CAP;
    label->baseline = HEADING_BASE;
    label->margin = HEADING_MARGIN;
    label->widthMilli = 1000;
    label->top = 0xFFBE2F; label->bottom = 0xFF2222; label->outline = 0x7C3025; label->outlineBottom = 0x64180C;
    label->outlineMilli = 3000; label->outerMilli = 2000; label->outerColor = 0xFFEED8;
    label->bevelMilli = 1000; label->bevelLight = 0xA86C5B; label->bevelDark = 0x180603;
    memcpy(label->text, text, length + 1);
    return adv;
}

int MenuText_HeadingMeasure(const char *text, int *cap, int *w, int *h, int *base, int *pad) {
    MenuLabel label;
    int adv;
    pthread_mutex_lock(&sLock);
    adv = line_face(0) != NULL ? heading_label(text, &label) : 0;
    pthread_mutex_unlock(&sLock);
    if (adv <= 0) { return 0; }
    *cap = HEADING_CAP * HEADING_RES;
    *w = label.rowWidth * HEADING_RES; *h = HEADING_HEIGHT * HEADING_RES;
    *base = HEADING_BASE * HEADING_RES; *pad = HEADING_MARGIN * HEADING_RES;
    return adv * HEADING_RES;
}

uint8_t *MenuText_HeadingRender(const char *text, int *w, int *h) {
    MenuLabel label;
    Row row;
    uint8_t *px = NULL;
    Face *f;
    pthread_mutex_lock(&sLock);
    f = line_face(0);
    if (f != NULL && heading_label(text, &label) > 0 && menu_text_row(f, &label, HEADING_RES, &row)) {
        *w = label.rowWidth * HEADING_RES; *h = HEADING_HEIGHT * HEADING_RES;
        px = malloc((size_t)*w * *h * 4);
        if (px != NULL) { row_paste(px, *w, 0, 0, *w, *h, &row); }
        free(row.px);
    }
    pthread_mutex_unlock(&sLock);
    return px;
}
