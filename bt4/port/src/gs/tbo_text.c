/*
 * Tenkaichi Budokai Online's letters (src/menu/port_tbo.c), written with the TrueType fonts of the font list
 * (menu_text.c, MenuText_Line*): its headings and buttons in the Menu font, its talk box in the Dialogue / UI font,
 * each on "Original" the first font of fonts.tsv for that use. A font added to fonts.tsv is a choice in F1 > UI and
 * so a look for these screens too; any character the font has can be written.
 *
 * The game's side asks for a line each frame it draws it (PortTbo_TextId, on the game's thread) and gets a number
 * for it and its measures; it then draws it as one of the screens' pictures (TBO_PSM, the number in CBP). The
 * renderer asks here for the picture of that number (TboText_Pixels) the first time and whenever the line or the
 * font changed (its version). The picture is the line twice: filled above, outlined below, white, so the game
 * tints and blends it as it did its letter sheet; a heading (kind 2) is one picture in its colours, the main
 * menu's heading style (MenuText_Heading*).
 *
 * Nothing here is game state: the screens are a menu of the PC's, before and outside an online match.
 */
#include "menu_text.h"
#include "online_language.h"
#include "menu_translation.h" /* the online menus' lines in the chosen language */
#include "../../../include/menu/tbo_art.h"
#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern unsigned gGsFrame;
#define TEXT_MAX 128
#define IDLE_MS 1000 /* a line not asked for that long gives its place to another */

typedef struct {
    char text[TEXT_MAX];
    int kind;
    uint32_t generation; /* of the fonts it was measured with */
    uint32_t version;    /* changes whenever what the place holds changes (0: never used) */
    uint64_t askedMs;
    unsigned askedFrame;
    int advance, w, h, base, pad, cap, outline;
} TboLine;

static TboLine sLines[TBO_TEXT_SLOTS];
static uint32_t sVersion;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
unsigned PortTbo_TextGeneration(void){return MenuText_LineGeneration();}

/* The size the letters are drawn at, by kind (capitals, in pixels: the screens show them smaller, from smaller
   copies), and the outline's thickness: as the letter sheet had it, about a 16th of the capitals. */
static int line_cap(int kind) { return kind == 1 ? 48 : kind == 5 ? 160 : 80; }
/* (the redesigned screens' letters: an outline as thick as their mock-ups' strokes) */
static int line_outline(int kind) { return kind == 5 ? 12 : kind == 3 ? 6 : kind == 4 ? 4 : kind == 0 ? 3 : 2; }
/* the kind of menu_text.c's lines: 0 Menu font, 1 Dialogue / UI font, 2 the display letters (here kind 3; kind 5 the
   same twice as large, for the largest lines), 3 the screens' texts (here kind 4) */
static int line_font(int kind) { return kind == 3 || kind == 5 ? 2 : kind == 4 ? 3 : kind; }
/* Keep the fill and outline on aligned, power-of-two rows. Otherwise their
 * shared edge drifts through odd mip heights and clips short menu labels. */
static int line_extent(int value){int size=1;while(size<value&&size<16384)size*=2;return size;}

static int line_measure(TboLine *l, uint32_t generation) {
    l->generation = generation;
    l->version = ++sVersion;
    if (sVersion == 0) {
        l->version = sVersion = 1;
    }
    if (l->kind == 2) {
        l->outline = 0;
        l->advance = MenuText_HeadingMeasure(l->text, &l->cap, &l->w, &l->h, &l->base, &l->pad);
        return l->advance > 0;
    }
    l->cap = line_cap(l->kind);
    l->outline = line_outline(l->kind);
    l->pad = l->outline + 2;
    l->advance = MenuText_LineMeasure(line_font(l->kind), l->text, l->cap, l->pad, &l->w, &l->h, &l->base);
    return l->advance > 0;
}

/* The number of a line (TBO_TEXT_BASE and on), -1 if it cannot be written (no font, no text, no room). */
static int text_id(const char *text, int kind, int translate) {
    if(translate)text=PortOnline_Text(text);
    uint32_t generation = MenuText_LineGeneration();
    uint64_t now = SDL_GetTicks();
    int i, spare = -1, id = -1;

    if (text == NULL || text[0] == '\0' || kind < 0 || kind > 5) {
        return -1;
    }
    {   /* a line of the menu in the chosen language (translations/port/<language>.json; as it is without one), here
           where each is drawn and measured, so the two always agree */
        const char *t = translate ? MenuTranslation_TboLine(text) : NULL;
        if (t != NULL) {
            text = t;
        }
    }
    pthread_mutex_lock(&sLock);
    for (i = 0; i < TBO_TEXT_SLOTS; i++) {
        TboLine *l = &sLines[i];
        if (l->version != 0 && l->kind == kind && strncmp(l->text, text, TEXT_MAX - 1) == 0) {
            l->askedMs = now; l->askedFrame = gGsFrame;
            if (l->generation != generation && !line_measure(l, generation)) {
                break;
            }
            id = l->advance > 0 ? TBO_TEXT_BASE + i : -1;
            break;
        }
        if (spare < 0 && (l->version == 0 || (now - l->askedMs >= IDLE_MS && gGsFrame - l->askedFrame > 2))) {
            spare = i;
        }
    }
    if (i == TBO_TEXT_SLOTS && spare >= 0) {
        TboLine *l = &sLines[spare];
        SDL_strlcpy(l->text, text, sizeof(l->text));
        l->kind = kind;
        l->askedMs = now; l->askedFrame = gGsFrame;
        if (line_measure(l, generation)) {
            id = TBO_TEXT_BASE + spare;
        }
    }
    pthread_mutex_unlock(&sLock);
    return id;
}
int PortTbo_TextId(const char *text,int kind){return text_id(text,kind,1);}
int PortTbo_TextIdLiteral(const char *text,int kind){return text_id(text,kind,0);}

/* How far the pen goes for a line, in thousandths of its capitals' height, without keeping it (for laying out:
   where to break a line). 0 without a font. */
static int text_measure(const char *text, int kind, int translate) {
    if(translate)text=PortOnline_Text(text);
    int w, h, base, cap, adv;
    if (text == NULL || text[0] == '\0' || kind < 0 || kind > 5 || kind == 2) {
        return 0;
    }
    {
        const char *t = translate ? MenuTranslation_TboLine(text) : NULL;
        if (t != NULL) {
            text = t;
        }
    }
    cap = line_cap(kind);
    adv = MenuText_LineMeasure(line_font(kind), text, cap, 0, &w, &h, &base);
    return (int)((int64_t)adv * 1000 / cap);
}
int PortTbo_TextMeasure(const char *text,int kind){return text_measure(text,kind,1);}
int PortTbo_TextMeasureLiteral(const char *text,int kind){return text_measure(text,kind,0);}

/* A measure of a line, in thousandths of its capitals' height: 0 how far the pen goes, 1 the picture's width,
   2 its height (of one of its two halves; a heading's: all of it), 3 the baseline from its top, 4 the room left of
   the pen's start. */
int PortTbo_TextInfo(int id, int what) {
    int r = 0, v;
    pthread_mutex_lock(&sLock);
    if (id >= TBO_TEXT_BASE && id < TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        const TboLine *l = &sLines[id - TBO_TEXT_BASE];
        v = what == 0 ? l->advance : what == 1 ? (l->kind==2?l->w:line_extent(l->w)) : what == 2 ? (l->kind==2?l->h:line_extent(l->h)) : what == 3 ? l->base : what == 4 ? l->pad : 0;
        r = l->cap > 0 ? (int)((int64_t)v * 1000 / l->cap) : 0;
    }
    pthread_mutex_unlock(&sLock);
    return r;
}

/* The version of a line's place (0: nothing there). */
uint32_t TboText_Version(uint32_t id) {
    uint32_t r = 0;
    pthread_mutex_lock(&sLock);
    if (id >= TBO_TEXT_BASE && id < TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        r = sLines[id - TBO_TEXT_BASE].version;
    }
    pthread_mutex_unlock(&sLock);
    return r;
}

/* The outline of a line as a distance: each pixel as bright as it is near the letters (within r and a pixel's
   softening of their edge, as the plain loop's disc of radius r), from an exact distance transform (Felzenszwalb and
   Huttenlocher: two passes, a few operations a pixel). The plain loop (TboText_Pixels, kept for no memory) looked at
   (2r + 1)^2 pixels for each: with the redesigned screens' thicker outlines and larger letters a line took 15 to
   190 ms, the largest over half a second, and the menus stopped while their lines were made; this takes 3 to 15 ms.
   The same outline but for its last pixel of softening (a mean of one level in 255). */
#define TT_INF 1e30
static void line_dt1(const double *f, int n, double *d, int *v, double *z) {
    int k = -1, q, j;
    for (q = 0; q < n; q++) {
        double s;
        if (f[q] >= TT_INF) continue;
        if (k < 0) { k = 0; v[0] = q; z[0] = -TT_INF; z[1] = TT_INF; continue; }
        s = ((f[q] + (double)q * q) - (f[v[k]] + (double)v[k] * v[k])) / (2.0 * q - 2.0 * v[k]);
        while (s <= z[k]) { k--; s = ((f[q] + (double)q * q) - (f[v[k]] + (double)v[k] * v[k])) / (2.0 * q - 2.0 * v[k]); }
        k++; v[k] = q; z[k] = s; z[k + 1] = TT_INF;
    }
    if (k < 0) { for (q = 0; q < n; q++) d[q] = TT_INF; return; }
    for (j = 0, q = 0; q < n; q++) { while (z[j + 1] < q) j++; d[q] = (double)(q - v[j]) * (q - v[j]) + f[v[j]]; }
}
static int line_outline_fast(const uint8_t *cov, int lw, int lh, int r, uint32_t *px, int tw, int th) {
    int n = lw > lh ? lw : lh, x, y;
    double *g = malloc(sizeof(double) * (size_t)lw * lh), *f = malloc(sizeof(double) * n), *d = malloc(sizeof(double) * n);
    double *z = malloc(sizeof(double) * (n + 1));
    int *v = malloc(sizeof(int) * n);
    if (g == NULL || f == NULL || d == NULL || z == NULL || v == NULL) {
        free(g); free(f); free(d); free(z); free(v);
        return 0;
    }
    for (x = 0; x < lw; x++) {
        for (y = 0; y < lh; y++) f[y] = cov[(size_t)y * lw + x] >= 128 ? 0.0 : TT_INF;
        line_dt1(f, lh, d, v, z);
        for (y = 0; y < lh; y++) g[(size_t)y * lw + x] = d[y];
    }
    for (y = 0; y < lh; y++) {
        for (x = 0; x < lw; x++) f[x] = g[(size_t)y * lw + x];
        line_dt1(f, lw, d, v, z);
        for (x = 0; x < lw; x++) {
            double t = (double)r + 1.0 - sqrt(d[x]);
            int fill = cov[(size_t)y * lw + x], line = t >= 1.0 ? 255 : t <= 0.0 ? 0 : (int)(t * 255.0 + 0.5);
            if (fill > line) line = fill;
            px[(size_t)y * tw + x] = 0xFFFFFFu | (uint32_t)((fill * 128 + 127) / 255) << 24;
            px[(size_t)(y + th) * tw + x] = 0xFFFFFFu | (uint32_t)((line * 128 + 127) / 255) << 24;
        }
    }
    free(g); free(f); free(d); free(z); free(v);
    return 1;
}

/* The picture of a line for the renderer: *w x *h RGBA (the GS's alpha, 0x80 opaque), the filled line in the upper
   half and its outline in the lower (a heading: itself); NULL if there is none. free() it. *version: what it
   shows. */
uint32_t *TboText_Pixels(uint32_t id, uint32_t *version, uint32_t *w, uint32_t *h) {
    TboLine l;
    uint8_t *cov;
    uint32_t *px;
    int lw, lh, lb, x, y, dx, dy, r,tw,th;

    if (id < TBO_TEXT_BASE || id >= TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        return NULL;
    }
    pthread_mutex_lock(&sLock);
    l = sLines[id - TBO_TEXT_BASE];
    pthread_mutex_unlock(&sLock);
    *version = l.version;
    if (l.version == 0 || l.advance <= 0) {
        return NULL;
    }
    if (l.kind == 2) {
        px = (uint32_t *)(void *)MenuText_HeadingRender(l.text, &lw, &lh);
        if (px != NULL && (lw != l.w || lh != l.h)) {
            free(px);
            px = NULL;
        }
        *w = (uint32_t)lw;
        *h = (uint32_t)lh;
        return px;
    }
    cov = MenuText_LineRender(line_font(l.kind), l.text, l.cap, l.pad, &lw, &lh, &lb);
    if (cov == NULL) {
        return NULL;
    }
    if (lw != l.w || lh != l.h) { /* (the font changed since it was measured: it is measured again next frame) */
        free(cov);
        return NULL;
    }
    tw=line_extent(lw);th=line_extent(lh);
    px = malloc((size_t)tw * th * 2 * 4);
    if (px == NULL) {
        free(cov);
        return NULL;
    }
    r = l.outline;
    for(y=0;y<th*2;++y)for(x=0;x<tw;++x)px[(size_t)y*tw+x]=0x00FFFFFFu;
    if (r > 0 && line_outline_fast(cov, lw, lh, r, px, tw, th)) {
        r = -1; /* (done) */
    }
    for (y = 0; r >= 0 && y < lh; y++) {
        for (x = 0; x < lw; x++) {
            int fill = cov[(size_t)y * lw + x], line = 0;
            for (dy = -r; dy <= r && line < 255; dy++) {
                int yy = y + dy;
                if (yy < 0 || yy >= lh) {
                    continue;
                }
                for (dx = -r; dx <= r; dx++) {
                    int xx = x + dx, c;
                    if (xx < 0 || xx >= lw || dx * dx + dy * dy > r * r + r) {
                        continue;
                    }
                    c = cov[(size_t)yy * lw + xx];
                    if (c > line) {
                        line = c;
                    }
                }
            }
            px[(size_t)y * tw + x] = 0xFFFFFFu | (uint32_t)((fill * 128 + 127) / 255) << 24;
            px[(size_t)(y + th) * tw + x] = 0xFFFFFFu | (uint32_t)((line * 128 + 127) / 255) << 24;
        }
    }
    free(cov);
    *w = (uint32_t)tw;
    *h = (uint32_t)th * 2;
    return px;
}
