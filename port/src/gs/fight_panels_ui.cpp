// The fight panels: with "Side bars: Fighter panels", the side bars of a 4:3 fight in a wide window become each
// side's own panel, as in the JoJo fighting games of the time: the fighter's large portrait (the one of the
// character select), faded softly into the panel at its edges, over the fighter's colours, with its name down the
// inner edge and its form under it, in two of the game's fonts (fonts.tsv, chosen in the settings). Nothing else
// (asked for: no gauges). Outside a fight the bars are the border.
//
// What a fight is comes from port/src/fight_panels.c (read on the game's thread, once per blank); this file only
// draws, on the render thread, into ImGui's background list (ui.cpp: bars_draw). The portraits are read from their
// files here (portrait_rgba) and kept as textures: colour, grey and a white silhouette stacked in one.
#include <ctype.h>
#include <algorithm>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "fight_panels.h"
#include "../../include/plat_game.h"
#include "../../include/tag_text.h"

extern "C" int Port_HudSwap(void);                         // gs/net.c: the HUD's sides are swapped on this copy
SDL_GPUTexture *Ui_Texture(uint8_t *pix, uint32_t w, uint32_t h); // ui.cpp (takes pix)
void Ui_TextureFree(SDL_GPUTexture *tex);
ImFont *Ui_FontName(void);  // ui.cpp: the name's face, chosen in the settings from the game's fonts (fonts.tsv)
ImFont *Ui_FontForm(void);  // the form's
ImFont *Ui_FontBold(void);  // the first of the game's fonts, where one of those two cannot be had

#define ART_W 256
#define ART_H 512
static const float kPi = 3.14159265f;

// The characters' names by id, in English (the order of the character table).
static const char *const kCharaNames[] = {
    "Goku (Early)", "Goku (Med)", "Goku (Med) Super Saiyan", "Goku (End)", "Goku (End) Super Saiyan", "Goku (End) Super Saiyan 2",
    "Goku (End) Super Saiyan 3", "Goku (GT)", "Goku (GT) Super Saiyan", "Goku (GT) Super Saiyan 3", "Goku (GT) Super Saiyan 4", "Kid Goku",
    "Great Ape", "Kid Gohan", "Teen Gohan", "Teen Gohan Super Saiyan", "Teen Gohan Super Saiyan 2", "Gohan",
    "Gohan Super Saiyan", "Gohan Super Saiyan 2", "Great Saiyaman", "Ultimate Gohan", "Piccolo (Early)", "Piccolo (End)",
    "Nail", "Krillin", "Yamcha", "Tien", "Chiaotzu", "Vegeta (Scouter)",
    "Vegeta (Scouter) Great Ape", "Vegeta", "Vegeta Super Saiyan", "Super Vegeta", "Vegeta (2nd Form)", "Vegeta (2nd Form) Super Saiyan",
    "Vegeta (2nd Form) Super Saiyan 2", "Majin Vegeta", "Vegeta (2nd Form) Super Saiyan 4", "Trunks (Sword)", "Trunks (Sword) Super Saiyan", "Trunks",
    "Trunks Super Saiyan", "Super Trunks", "Kid Trunks", "Kid Trunks Super Saiyan", "Goten", "Goten Super Saiyan",
    "Gotenks", "Gotenks Super Saiyan", "Gotenks Super Saiyan 3", "Vegito", "Super Vegito", "Super Gogeta",
    "Gogeta Super Saiyan 4", "Mr Satan", "Videl", "Great Saiyaman 2", "Supreme Kai", "Kibito Kai",
    "Yajirobe", "Paikuhan", "Tapion", "Pan", "Uub", "Majuub",
    "Master Roshi", "Master Roshi Full Power", "Grandpa Gohan", "Namu", "Hatchan", "King Vegeta",
    "King Vegeta Great Ape", "Bardock", "Bardock Great Ape", "Celipa", "Celipa Great Ape", "Raditz",
    "Raditz Great Ape", "Saibamen", "Nappa", "Nappa Great Ape", "Zarbon", "Zarbon Transformed",
    "Dodoria", "Cui", "Ginyu", "Recoome", "Burter", "Jeice",
    "Guldo", "Frieza 1st Form", "Frieza 2nd Form", "Frieza 3rd Form", "Frieza Final Form", "Frieza 100% Power",
    "Mecha Frieza", "King Cold", "Appule", "Frieza Soldier", "Android 16", "Android 17",
    "Android 18", "Android 19", "Android 2", "Cell 1st Form", "Cell 2nd Form", "Perfect Cell",
    "Super Perfect Cell", "Cell Jr", "Babidi", "Dabura", "Majin Buu", "Evil Buu",
    "Super Buu", "Super Buu Gotenks Absorbed", "Super Buu Gohan Absorbed", "Kid Buu", "Garlic Jr", "Super Garlic Jr",
    "Wheelo", "Turles", "Turles Great Ape", "Slug", "Slug Giant", "Sauza",
    "Cooler", "Cooler Final Form", "Metal Cooler", "Android 13", "Android 13 Fusion", "Broly",
    "Broly Super Saiyan", "Broly Legendary Super Saiyan", "Zangya", "Bojack", "Bojack 100% Power", "Janemba",
    "Super Janemba", "Hildegarn", "Baby Vegeta", "Super Baby 1", "Super Baby 2", "Super Baby Great Ape",
    "Super 17", "Nuova Shenron", "Shin Shenron", "Omega Shenron", "Taopaipai", "Cyborg Tao",
    "General Blue", "Akuman", "Pilaf Machine", "Pilaf Machine Fusion", "Tambourine", "Demon King Piccolo",
    "Arale", "Chichi", "Spopovich", "Future Gohan", "Future Gohan Super Saiyan",
};

static const char *chara_name(int chara) {
    if(PortGame_IsBT4()) return PortGame_CharacterName(chara);
    return chara >= 0 && chara < (int)(sizeof(kCharaNames) / sizeof(kCharaNames[0])) ? Port_TagText(kCharaNames[chara]) : NULL;
}

// ---------------------------------------------------------------------------------------------------- portrait files
// A large portrait file: byte-pair packed (the game's sys/bpe.c), a texture file with one 256 x 512 picture of 256
// colours, its pixels in the GS's 8-bit layout as the game uploads them (a 128 x 256 32-bit rectangle) and its CLUT
// in the GS's storage order.
// One byte of the packed data, or false at its end (a damaged or cut file, a mod's: never read past it).
static bool take(const uint8_t *&p, const uint8_t *end, int &v) {
    if (p >= end) {
        return false;
    }
    v = *p++;
    return true;
}

static uint8_t *bpe_decode(const uint8_t *d, long len, uint32_t *size) {
    uint32_t total, pack, n = 0;
    if (len < 8) {
        return NULL;
    }
    memcpy(&total, d, 4);
    memcpy(&pack, d + 4, 4);
    if (total == 0 || total > (8u << 20) || (uint64_t)pack + 8 > (uint64_t)len) {
        return NULL;
    }
    uint8_t *out = (uint8_t *)malloc(total + 16);
    if (out == NULL) {
        return NULL;
    }
    const uint8_t *p = d + 8, *end = p + pack;
    bool ok = true;
    uint64_t steps = 0, budget = (uint64_t)total * 512 + (1u << 20); // (far more than a real file needs)
    while (ok && p < end && n < total) {
        uint8_t left[256], right[256], stack[512];
        int c = 0, count, sp = 0, i, size16, v, hi, lo;
        for (i = 0; i < 256; i++) {
            left[i] = (uint8_t)i;
            right[i] = 0;
        }
        ok = take(p, end, count);
        while (ok) {
            if (count > 127) {
                c += count - 127;
                count = 0;
            }
            if (c >= 256) {
                break;
            }
            for (i = 0; ok && i <= count && c < 256; i++, c++) {
                ok = take(p, end, v);
                left[c] = (uint8_t)v;
                if (ok && c != left[c]) {
                    ok = take(p, end, v);
                    right[c] = (uint8_t)v;
                }
            }
            if (!ok || c >= 256) {
                break;
            }
            ok = take(p, end, count);
        }
        if (!ok || !take(p, end, hi) || !take(p, end, lo)) {
            ok = false;
            break;
        }
        size16 = hi * 256 + lo;
        for (;;) {
            int ch;
            if (++steps > budget) { // (a damaged table whose pairs lead back to themselves never ends otherwise)
                ok = false;
                break;
            }
            if (sp > 0) {
                ch = stack[--sp];
            } else {
                if (size16-- == 0) {
                    break;
                }
                if (!take(p, end, ch)) {
                    ok = false;
                    break;
                }
            }
            if (ch == left[ch]) {
                if (n < total) {
                    out[n++] = (uint8_t)ch;
                }
            } else if (sp < 510) {
                stack[sp++] = right[ch];
                stack[sp++] = left[ch];
            }
        }
    }
    if (!ok) {
        free(out);
        return NULL;
    }
    *size = total;
    return out;
}

static const uint8_t kBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const uint8_t kCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                     {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const uint8_t kCol8First[4][16] = {{0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
                                          {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};

static uint32_t addr32(uint32_t bw, uint32_t x, uint32_t y) {
    return (((y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + kBlock32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + kCol32[y & 7][x & 7];
}

static uint32_t addr8(uint32_t bw, uint32_t x, uint32_t y) {
    uint32_t col = (y & 15) >> 2, sx = (col & 1) ? ((x & 15) ^ 4) : (x & 15);
    return ((((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + kCol8First[y & 3][sx] + col * 64;
}

// The CLUT's storage order: entries 8..15 and 16..23 of every 32 change places.
static int csm1(int i) {
    return (i & 0x18) == 8 ? i + 8 : (i & 0x18) == 16 ? i - 8 : i;
}

// The portrait file at path as RGBA (256 x 512 x 4 bytes, the PS2's alpha doubled). 1 = read.
static int portrait_rgba(const char *path, uint8_t *rgba) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *pk = len > 0 ? (uint8_t *)malloc((size_t)len) : NULL;
    if (pk == NULL || fread(pk, 1, (size_t)len, fp) != (size_t)len) {
        free(pk);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    uint32_t size = 0, f[2];
    uint8_t *raw = bpe_decode(pk, len, &size);
    free(pk);
    if (raw == NULL) {
        return 0;
    }
    int ok = 0;
    uint8_t *mem = (uint8_t *)calloc(1, 1 << 20);
    if (mem != NULL && size >= 0x60) {
        memcpy(f, raw + 0x20, sizeof(f));
        if ((uint64_t)f[0] * 4 + 0x60 + 131072 <= size && (uint64_t)f[1] * 4 + 0x60 + 1024 <= size) {
            const uint8_t *data = raw + f[0] * 4 + 0x60, *clut = raw + f[1] * 4 + 0x60;
            uint8_t pal[1024];
            for (int i = 0; i < 256; i++) {
                memcpy(&pal[i * 4], clut + csm1(i) * 4, 4);
                pal[i * 4 + 3] = pal[i * 4 + 3] >= 0x80 ? 255 : (uint8_t)(pal[i * 4 + 3] * 2);
            }
            for (uint32_t y = 0; y < 256; y++) { // the upload, 128 x 256 at 32 bits, into GS memory
                for (uint32_t x = 0; x < 128; x++) {
                    memcpy(&mem[(addr32(2, x, y) * 4) & 0xFFFFC], &data[(y * 128 + x) * 4], 4);
                }
            }
            for (uint32_t y = 0; y < ART_H; y++) { // read back as the 8-bit picture
                for (uint32_t x = 0; x < ART_W; x++) {
                    memcpy(rgba + (y * ART_W + x) * 4, &pal[mem[addr8(4, x, y) & 0xFFFFF] * 4], 4);
                }
            }
            ok = 1;
        }
    }
    free(mem);
    free(raw);
    return ok;
}

// ---------------------------------------------------------------------------------------------------- portraits
struct Art {
    char path[FP_PATH];
    SDL_GPUTexture *tex;
    bool ok;
    float hx, hy, hs;   // the head's square in the portrait (pixels)
    float fx, fy;       // the face (the middle of the skin under the hair), what the panel places
    ImVec4 theme;       // the fighter's main colour, 0..1
    double used;
};
static Art sArt[24];

static float frac1(float v) { return v - floorf(v); }
static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float ease_out(float t) { t = clamp01(t); return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }
static float hash1(float n) { return frac1(sinf(n * 12.9898f) * 43758.5453f); }

// The fighter's colour: the hue most of its strongly coloured pixels have (clothes, hair, aura), skin weighs little.
static ImVec4 theme_of(const uint8_t *px) {
    float w[24] = {0}, r[24] = {0}, g[24] = {0}, b[24] = {0};
    for (int i = 0; i < ART_W * ART_H; i += 3) {
        const uint8_t *p = px + i * 4;
        if (p[3] < 200) {
            continue;
        }
        float R = p[0] / 255.0f, G = p[1] / 255.0f, B = p[2] / 255.0f;
        float mx = fmaxf(R, fmaxf(G, B)), mn = fminf(R, fminf(G, B)), d = mx - mn;
        if (mx < 0.2f || d < 0.25f) {
            continue;
        }
        float s = d / mx, h;
        if (mx == R) {
            h = fmodf((G - B) / d + 6.0f, 6.0f);
        } else if (mx == G) {
            h = (B - R) / d + 2.0f;
        } else {
            h = (R - G) / d + 4.0f;
        }
        int k = (int)(h * 4.0f) % 24;
        float wt = s * s * s * mx;
        w[k] += wt;
        r[k] += R * wt;
        g[k] += G * wt;
        b[k] += B * wt;
    }
    int best = -1;
    float bw = 0.0f;
    for (int k = 0; k < 24; k++) {
        float v = w[k] + 0.5f * (w[(k + 23) % 24] + w[(k + 1) % 24]);
        if (v > bw) {
            bw = v;
            best = k;
        }
    }
    if (best < 0 || w[best] <= 0.0f) {
        return ImVec4(0.85f, 0.55f, 0.15f, 1.0f);
    }
    return ImVec4(r[best] / w[best], g[best] / w[best], b[best] / w[best], 1.0f);
}

// Where the head is: from the first lines that have anything in them, as wide as the head there (plat_mods.c,
// default_square, does the same for the chips of the select screens).
static void head_of(const uint8_t *px, float *x0, float *y0, float *s) {
    int top = -1, lo = ART_W, hi = -1;
    for (int y = 0; y < 400 && top < 0; y++) {
        int n = 0;
        for (int x = 0; x < ART_W; x++) {
            n += px[(y * ART_W + x) * 4 + 3] >= 0x80;
        }
        if (n >= 6) {
            top = y;
        }
    }
    if (top < 0) {
        *x0 = 58;
        *y0 = 18;
        *s = 140;
        return;
    }
    for (int y = top + 30; y < top + 130 && y < ART_H; y++) {
        for (int x = 0; x < ART_W; x++) {
            if (px[(y * ART_W + x) * 4 + 3] >= 0x80) {
                lo = x < lo ? x : lo;
                hi = x > hi ? x : hi;
            }
        }
    }
    if (hi < lo) {
        lo = 48;
        hi = 208;
    }
    int size = hi - lo + 1;
    size = size > 168 ? 168 : size < 110 ? 110 : size;
    int x = (lo + hi + 1) / 2 - size / 2;
    x = x < 0 ? 0 : x + size > ART_W ? ART_W - size : x;
    *x0 = (float)x;
    *y0 = (float)(top + 4);
    *s = (float)size;
}

// The crop of each disc portrait (asked for: a template instead of a guess): the point of the face the panel centres,
// in the portrait's pixels, by character id. Set by hand from the portraits (port/tools has nothing for it: they were
// looked at). bars/faces.txt next to the game overrides: lines "<character id> <x> <y>".
static const short kFace[][2] = {
    {115,170}, {130,142}, {138,162}, {123,152}, {139,155}, {126,156}, {135,142}, {124,160}, {134,154}, {145,145},
    {121,145}, {127,158}, {125,170}, {130,165}, {123,148}, {125,150}, {138,141}, {125,136}, {145,160}, {134,150},
    {131,162}, {110,165}, {128,160}, {128,160}, {128,160}, {127,93}, {115,155}, {125,106}, {100,150}, {122,136},
    {115,160}, {124,138}, {126,101}, {115,160}, {121,139}, {106,139}, {120,170}, {129,130}, {113,136}, {122,138},
    {133,128}, {116,136}, {114,143}, {119,113}, {127,146}, {128,147}, {127,158}, {130,147}, {126,145}, {110,165},
    {132,147}, {124,144}, {131,150}, {95,155}, {124,152}, {125,151}, {130,152}, {115,140}, {130,165}, {130,155},
    {100,160}, {100,140}, {90,160}, {105,150}, {133,138}, {135,115}, {128,102}, {138,121}, {115,140}, {100,150},
    {135,118}, {122,130}, {115,165}, {125,176}, {140,175}, {113,151}, {125,180}, {119,122}, {125,185}, {128,160},
    {129,104}, {135,160}, {115,170}, {120,175}, {130,170}, {130,165}, {100,165}, {115,160}, {120,160}, {120,165},
    {130,165}, {110,160}, {90,170}, {110,165}, {115,160}, {130,155}, {75,165}, {110,165}, {140,160}, {120,170},
    {122,132}, {129,147}, {130,89}, {150,160}, {110,155}, {120,165}, {115,160}, {110,160}, {115,175}, {100,170},
    {110,160}, {125,160}, {120,170}, {110,160}, {120,190}, {115,165}, {120,195}, {115,190}, {115,165}, {110,160},
    {90,170}, {100,154}, {128,190}, {140,165}, {155,170}, {90,145}, {120,165}, {110,160}, {130,165}, {130,165},
    {110,170}, {144,157}, {125,175}, {128,139}, {140,165}, {115,180}, {125,175}, {110,170}, {130,175}, {130,165},
    {140,135}, {126,139}, {107,148}, {110,175}, {130,134}, {115,170}, {110,170}, {115,170}, {118,142}, {110,170},
    {115,165}, {130,175}, {130,160}, {110,170}, {110,160}, {130,165}, {120,160}, {110,170}, {133,118}, {128,140},
    {128,151},
};
static short sFaceOwn[sizeof(kFace) / sizeof(kFace[0])][2];
static bool sFaceRead;

static void faces_read(void) {
    sFaceRead = true;
    FILE *fp = fopen("bars/faces.txt", "r");
    if (fp == NULL) {
        return;
    }
    char line[128];
    int c, x, y, n = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%d %d %d", &c, &x, &y) == 3 && c >= 0 && c < (int)(sizeof(kFace) / sizeof(kFace[0]))) {
            sFaceOwn[c][0] = (short)x;
            sFaceOwn[c][1] = (short)y;
            n++;
        }
    }
    fclose(fp);
    fprintf(stderr, "bt3: fight panels: %d faces from bars/faces.txt\n", n);
}

// Where a fighter's face is in its portrait: the table for a disc portrait, else (one of a mod's) found from the skin.
static void face_point(const Art *art, int chara, int costume, float *x, float *y) {
    if (!sFaceRead) {
        faces_read();
    }
    if (costume < 8 && chara >= 0 && chara < (int)(sizeof(kFace) / sizeof(kFace[0]))) {
        if (sFaceOwn[chara][0] != 0 || sFaceOwn[chara][1] != 0) {
            *x = sFaceOwn[chara][0];
            *y = sFaceOwn[chara][1];
        } else {
            *x = kFace[chara][0];
            *y = kFace[chara][1];
        }
        return;
    }
    *x = art->fx;
    *y = art->fy;
}

static bool skin(const uint8_t *p) {
    return p[3] >= 200 && p[2] > 90 && p[0] > 150 && p[0] - p[1] >= 15 && p[0] - p[1] <= 75 && p[1] - p[2] >= 20 && p[1] - p[2] <= 85;
}

// The face: the first band of skin under the top of the head, in the head's columns. A spiky head of hair makes the
// head's square a poor guide to where the face is. Without skin (Frieza, Cell, Buu...) the head's square is used.
static void face_of(const uint8_t *px, Art *a) {
    int xa = (int)a->hx - 20, xb = (int)(a->hx + a->hs) + 20, top = (int)a->hy - 4, first = -1;
    xa = xa < 0 ? 0 : xa;
    xb = xb > ART_W ? ART_W : xb;
    a->fx = a->hx + a->hs * 0.5f;
    a->fy = a->hy + a->hs * 0.45f;
    for (int y = top < 0 ? 0 : top; y < top + 260 && y < ART_H && first < 0; y++) {
        int n = 0;
        for (int x = xa; x < xb; x++) {
            n += skin(px + (y * ART_W + x) * 4);
        }
        if (n >= 8) {
            first = y;
        }
    }
    if (first < 0) {
        return;
    }
    float sx = 0.0f;
    int n = 0;
    for (int y = first; y < first + 55 && y < ART_H; y++) {
        for (int x = xa; x < xb; x++) {
            if (skin(px + (y * ART_W + x) * 4)) {
                sx += x;
                n++;
            }
        }
    }
    if (n >= 150) {
        a->fx = sx / n;
        a->fy = (float)first + 28.0f; // (the band starts at the forehead)
    }
}

static Art *art_get(const char *path, double now) {
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    Art *slot = NULL;
    for (Art &a : sArt) {
        if (a.path[0] != '\0' && strcmp(a.path, path) == 0) {
            a.used = now;
            return a.ok ? &a : NULL;
        }
    }
    for (Art &a : sArt) { // a free place, else the one unused the longest (not one drawn just now)
        if (a.path[0] == '\0') {
            slot = &a;
            break;
        }
        if (now - a.used > 2.0 && (slot == NULL || a.used < slot->used)) {
            slot = &a;
        }
    }
    if (slot == NULL) {
        return NULL;
    }
    if (slot->tex != NULL) {
        Ui_TextureFree(slot->tex);
    }
    memset(slot, 0, sizeof(*slot));
    snprintf(slot->path, sizeof(slot->path), "%s", path);
    slot->used = now;
    uint8_t *pix = (uint8_t *)malloc((size_t)ART_W * ART_H * 4 * 3);
    if (pix == NULL || !portrait_rgba(path, pix)) {
        free(pix);
        fprintf(stderr, "bt3: fight panels: no portrait in %s\n", path);
        return NULL;
    }
    head_of(pix, &slot->hx, &slot->hy, &slot->hs);
    face_of(pix, slot);
    slot->theme = theme_of(pix);
    uint8_t *grey = pix + ART_W * ART_H * 4, *sil = grey + ART_W * ART_H * 4;
    for (int i = 0; i < ART_W * ART_H; i++) {
        const uint8_t *p = pix + i * 4;
        uint8_t l = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
        grey[i * 4 + 0] = grey[i * 4 + 1] = grey[i * 4 + 2] = l;
        grey[i * 4 + 3] = p[3];
        sil[i * 4 + 0] = sil[i * 4 + 1] = sil[i * 4 + 2] = 255;
        sil[i * 4 + 3] = p[3];
    }
    slot->tex = Ui_Texture(pix, ART_W, ART_H * 3); // (takes pix)
    slot->ok = slot->tex != NULL;
    return slot->ok ? slot : NULL;
}

enum { BAND_COLOUR, BAND_GREY, BAND_SILHOUETTE };

// ---------------------------------------------------------------------------------------------------- drawing
static float sAlpha = 1.0f; // the whole panel's (it fades in and out with the fight)

static ImU32 C(int r, int g, int b, int a = 255) {
    int A = (int)(a * sAlpha);
    return IM_COL32(r < 0 ? 0 : r > 255 ? 255 : r, g < 0 ? 0 : g > 255 ? 255 : g, b < 0 ? 0 : b > 255 ? 255 : b, A < 0 ? 0 : A > 255 ? 255 : A);
}
static ImU32 Cv(ImVec4 c, float k, int a = 255) { // a colour times k
    return C((int)(c.x * k * 255), (int)(c.y * k * 255), (int)(c.z * k * 255), a);
}
static ImU32 Cmix(ImVec4 c, ImVec4 d, float t, int a = 255) {
    return C((int)((c.x + (d.x - c.x) * t) * 255), (int)((c.y + (d.y - c.y) * t) * 255), (int)((c.z + (d.z - c.z) * t) * 255), a);
}

static ImFont *face(ImFont *f) {
    f = f != NULL ? f : Ui_FontBold();
    return f != NULL ? f : ImGui::GetFont();
}

// Text with an outline all round and its fill shaded from top to bottom.
static void text_fx(ImDrawList *dl, ImFont *f, float size, ImVec2 p, const char *t, ImU32 top, ImU32 bot, ImU32 line, float th,
                    float alpha = 1.0f) {
    if (size < 1.0f) {
        return;
    }
    if (th > 0.0f) {
        int n = th > 2.5f ? 16 : 8;
        for (int i = 0; i < n; i++) {
            float a = i * 2.0f * kPi / n;
            dl->AddText(f, size, ImVec2(p.x + cosf(a) * th, p.y + sinf(a) * th), line, t);
        }
    }
    int v0 = dl->VtxBuffer.Size;
    dl->AddText(f, size, p, C(255, 255, 255, (int)(255 * alpha)), t);
    ImVec2 sz = f->CalcTextSizeA(size, FLT_MAX, 0.0f, t);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size, ImVec2(p.x, p.y + sz.y * 0.2f),
                                                  ImVec2(p.x, p.y + sz.y * 0.85f), top, bot);
}

// The visible box of a string at `size` (from the glyphs, not the line height): offset from the pen and extent.
static void ink_box(ImFont *f, float size, const char *t, float *ox, float *oy, float *w, float *h) {
    ImFontBaked *bk = f->GetFontBaked(size);
    float x = 0.0f, minx = FLT_MAX, maxx = -FLT_MAX, miny = FLT_MAX, maxy = -FLT_MAX;
    for (const char *s = t; *s != '\0';) {
        unsigned int c = 0;
        s += ImTextCharFromUtf8(&c, s, NULL);
        ImFontGlyph *g = bk->FindGlyph((ImWchar)c);
        if (g == NULL) {
            continue;
        }
        if (g->Visible) {
            minx = fminf(minx, x + g->X0);
            maxx = fmaxf(maxx, x + g->X1);
            miny = fminf(miny, g->Y0);
            maxy = fmaxf(maxy, g->Y1);
        }
        x += g->AdvanceX;
    }
    if (maxx < minx) {
        *ox = *oy = *w = *h = 0.0f;
        return;
    }
    *ox = minx;
    *oy = miny;
    *w = maxx - minx;
    *h = maxy - miny;
}

// A string whose ink is centred on c.
static void text_centre(ImDrawList *dl, ImFont *f, float size, ImVec2 c, const char *t, ImU32 top, ImU32 bot, ImU32 line, float th,
                        float alpha = 1.0f) {
    float ox, oy, w, h;
    ink_box(f, size, t, &ox, &oy, &w, &h);
    text_fx(dl, f, size, ImVec2(c.x - ox - w * 0.5f, c.y - oy - h * 0.5f), t, top, bot, line, th, alpha);
}

// The size at which the ink of t is at most w wide and h high.
static float fit_size(ImFont *f, const char *t, float w, float h) {
    float ox, oy, iw, ih;
    ink_box(f, 100.0f, t, &ox, &oy, &iw, &ih);
    if (iw <= 0.0f || ih <= 0.0f) {
        return h;
    }
    return fminf(100.0f * w / iw, 100.0f * h / ih);
}

// "Goku (End) Super Saiyan" -> "GOKU" and "SUPER SAIYAN"; "Frieza Final Form" -> "FRIEZA", "FINAL FORM".
static void split_name(const char *full, char *main, size_t mcap, char *sub, size_t scap) {
    static const char *const kForms[] = {" Legendary Super Saiyan", " Super Saiyan", " Great Ape", " Transformed", " Fusion",
                                         " Final Form", " 1st Form", " 2nd Form", " 3rd Form", " 100% Power", " Giant",
                                         " Gotenks Absorbed", " Gohan Absorbed"};
    const char *cut = NULL, *rest = NULL;
    char paren[64] = "";
    const char *p = strstr(full, " (");
    if (p != NULL) {
        const char *q = strchr(p, ')');
        cut = p;
        if (q != NULL) {
            snprintf(paren, sizeof(paren), "%.*s", (int)(q - p - 2), p + 2);
            rest = q + 1;
            while (*rest == ' ') {
                rest++;
            }
        }
    } else {
        for (const char *k : kForms) {
            const char *f = strstr(full, k);
            if (f != NULL && f > full && (cut == NULL || f < cut)) {
                cut = f;
            }
        }
        if (cut != NULL) {
            rest = cut + 1;
        }
    }
    size_t n = cut != NULL ? (size_t)(cut - full) : strlen(full);
    if (n >= mcap) {
        n = mcap - 1;
    }
    for (size_t i = 0; i < n; i++) {
        main[i] = full[i]; // (as written, as the game shows names)
    }
    main[n] = '\0';
    const char *s = rest != NULL && rest[0] != '\0' ? rest : paren;
    size_t i = 0;
    for (; s[i] != '\0' && i + 1 < scap; i++) {
        sub[i] = s[i]; // (as written: the brush face reads better in mixed case)
    }
    sub[i] = '\0';
}


// ---------------------------------------------------------------------------------------------------- one panel
struct Anim {
    int key;        // who is shown: member << 16 | character + 1
    double enterAt; // the fight's start (the panel comes in)
    double swapAt;  // the fighter changed (a switch, a transformation)
};
static Anim sAnim[2];
static FpState sLast; // the last fight read (drawn while it fades out)
static bool sWasBattle;
static double sEndAt = -10.0;

struct Panel {
    float x0, x1, y0, y1; // the panel's rectangle, without the trim
    bool right;           // the right-hand panel: the inner edge is its left
    float W, H;
    float X(float u) const { return right ? x0 + u : x1 - u; } // u: from the inner edge outward
};

static float smooth01(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

// The portrait into a..b (screen pixels) as a grid whose points carry their own alpha: it thins out toward the
// panel's sides and its bottom, and toward the bottom of the picture itself, so no edge of it is ever seen cut.
static void art_faded(ImDrawList *dl, const Panel &p, const Art *art, int band, ImVec2 a, ImVec2 b, ImU32 tint) {
    const int nx = 12, ny = 24;
    float fadeL = p.W * (p.right ? 0.16f : 0.20f), fadeR = p.W * (p.right ? 0.20f : 0.16f); // (the screen's edge fades longer)
    float v0 = (band * ART_H + 0.5f) / (ART_H * 3.0f), v1 = ((band + 1) * ART_H - 0.5f) / (ART_H * 3.0f);
    ImVec4 tc = ImGui::ColorConvertU32ToFloat4(tint);
    dl->PushTexture(ImTextureRef((ImTextureID)(intptr_t)art->tex));
    dl->PrimReserve(nx * ny * 6, (nx + 1) * (ny + 1));
    ImDrawIdx base = (ImDrawIdx)dl->_VtxCurrentIdx;
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            ImDrawIdx q = (ImDrawIdx)(base + j * (nx + 1) + i);
            dl->PrimWriteIdx(q);
            dl->PrimWriteIdx((ImDrawIdx)(q + 1));
            dl->PrimWriteIdx((ImDrawIdx)(q + nx + 2));
            dl->PrimWriteIdx(q);
            dl->PrimWriteIdx((ImDrawIdx)(q + nx + 2));
            dl->PrimWriteIdx((ImDrawIdx)(q + nx + 1));
        }
    }
    for (int j = 0; j <= ny; j++) {
        float ty = (float)j / ny, y = a.y + (b.y - a.y) * ty;
        for (int i = 0; i <= nx; i++) {
            float tx = (float)i / nx, x = a.x + (b.x - a.x) * tx;
            float k = smooth01((x - p.x0) / fadeL) * smooth01((p.x1 - x) / fadeR);
            k *= smooth01(ty / 0.16f) * (1.0f - smooth01((ty - 0.62f) / 0.36f));              // the picture's own top and bottom
            k *= smooth01((p.y1 - y) / (p.H * 0.12f)) * smooth01((y - p.y0) / (p.H * 0.10f)); // the panel's
            ImVec2 uv(tx, v0 + (v1 - v0) * ty); // (as drawn on both sides: asked for, not mirrored)
            dl->PrimWriteVtx(ImVec2(x, y), uv, ImGui::ColorConvertFloat4ToU32(ImVec4(tc.x, tc.y, tc.z, tc.w * k)));
        }
    }
    dl->PopTexture();
}

static void background(ImDrawList *dl, const Panel &p, ImVec4 th, ImVec2 focus, double now) {
    // the fighter's colour, deep at the top, to near black below
    dl->AddRectFilledMultiColor(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), Cv(th, 0.58f), Cv(th, 0.58f), Cv(th, 0.08f), Cv(th, 0.08f));
    // soft focus lines out of the head, turning slowly
    dl->PushClipRect(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), true);
    const int n = 26;
    float R = p.H * 1.6f, r0 = p.H * 0.05f, spin = (float)fmod(now * 0.04, 2.0 * kPi);
    for (int i = 0; i < n; i++) {
        float a = spin + i * 2.0f * kPi / n + hash1((float)i) * 0.12f;
        float w = 0.025f + 0.03f * hash1(i + 7.0f);
        float lit = 0.35f + 0.65f * hash1(i + 3.0f);
        ImVec2 q0(focus.x + cosf(a) * r0, focus.y + sinf(a) * r0);
        ImVec2 q1(focus.x + cosf(a - w) * R, focus.y + sinf(a - w) * R);
        ImVec2 q2(focus.x + cosf(a + w) * R, focus.y + sinf(a + w) * R);
        dl->AddTriangleFilled(q0, q1, q2, Cmix(th, ImVec4(1, 1, 1, 1), 0.55f, (int)(26 * lit)));
    }
    // darker toward the screen's edge
    float edge = p.W * 0.45f;
    if (p.right) {
        dl->AddRectFilledMultiColor(ImVec2(p.x1 - edge, p.y0), ImVec2(p.x1, p.y1), C(0, 0, 0, 0), C(0, 0, 0, 130), C(0, 0, 0, 130), C(0, 0, 0, 0));
    } else {
        dl->AddRectFilledMultiColor(ImVec2(p.x0, p.y0), ImVec2(p.x0 + edge, p.y1), C(0, 0, 0, 130), C(0, 0, 0, 0), C(0, 0, 0, 0), C(0, 0, 0, 130));
    }
    dl->PopClipRect();
}

// Text as the game draws its stylized names (gs/menu_text.c, the stage names' style from the disc): pale yellow at
// the top of the capitals to orange red at the baseline, a dark outline a seventh of the capitals' height, and a
// soft drop shadow down and to the right. c: where the ink's middle goes.
static void game_text(ImDrawList *dl, ImFont *f, float size, ImVec2 c, const char *t, float alpha) {
    float ox, oy, w, h;
    ink_box(f, size, t, &ox, &oy, &w, &h);
    if (h <= 0.0f) {
        return;
    }
    ImVec2 pen(c.x - ox - w * 0.5f, c.y - oy - h * 0.5f);
    float border = fmaxf(1.0f, h / 7.0f);
    ImVec2 sh(h * 0.19f, h * 0.14f);
    int A = (int)(255 * alpha);
    for (int ring = 0; ring < 2; ring++) { // the shadow: the outlined letters, offset and softened
        float r = border + ring * border * 0.6f;
        ImU32 col = C(0, 0, 0, A * (ring == 0 ? 70 : 40) / 255);
        for (int i = 0; i < 12; i++) {
            float a = i * 2.0f * kPi / 12;
            dl->AddText(f, size, ImVec2(pen.x + sh.x + cosf(a) * r, pen.y + sh.y + sinf(a) * r), col, t);
        }
    }
    ImU32 line = C(0x2C, 0x15, 0x1B, A);
    int n = border > 2.5f ? 16 : 10;
    for (int i = 0; i < n; i++) {
        float a = i * 2.0f * kPi / n;
        dl->AddText(f, size, ImVec2(pen.x + cosf(a) * border, pen.y + sinf(a) * border), line, t);
    }
    int v0 = dl->VtxBuffer.Size;
    dl->AddText(f, size, pen, C(255, 255, 255, A), t);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size, ImVec2(pen.x, pen.y + oy), ImVec2(pen.x, pen.y + oy + h),
                                                  C(0xF4, 0xEF, 0x89), C(0xD2, 0x68, 0x4B));
}

// The name, one letter under the other down the inner edge, stamped in one by one, in the name's face: white into
// the fighter's colour, a dark ink outline and a shadow (asked for: down the side, in a brush face).
static void name_column(ImDrawList *dl, const Panel &p, const char *name, ImVec4 th, double start, double now) {
    ImFont *f = face(Ui_FontName());
    int n = (int)strlen(name);
    if (n == 0) {
        return;
    }
    float top = p.y0 + p.H * 0.035f, avail = p.H * 0.70f, colW = p.W * 0.34f, cx = p.X(p.W * 0.19f);
    float tot = 0.0f, maxw = 0.0f, gap = 10.0f, space = 34.0f; // (at size 100)
    for (int i = 0; i < n; i++) {
        char c[2] = {name[i], 0};
        float ox, oy, w, h;
        if (name[i] == ' ') {
            tot += space;
            continue;
        }
        ink_box(f, 100.0f, c, &ox, &oy, &w, &h);
        tot += h;
        maxw = fmaxf(maxw, w);
    }
    tot += gap * (n - 1);
    float size = fminf(100.0f * colW / fmaxf(maxw, 1.0f), 100.0f * avail / fmaxf(tot, 1.0f));
    size = fminf(size, p.H * 0.11f);
    float k = size / 100.0f, y = top;
    ImU32 lite = Cmix(th, ImVec4(1, 1, 1, 1), 0.70f);
    for (int i = 0; i < n; i++) {
        char c[2] = {name[i], 0};
        float ox, oy, w, h;
        if (name[i] == ' ') {
            y += (space + gap) * k;
            continue;
        }
        ink_box(f, 100.0f, c, &ox, &oy, &w, &h);
        float t = (float)(now - (start + 0.25 + i * 0.05)) / 0.16f;
        if (t > 0.0f) {
            float s = 1.0f + (1.0f - ease_out(t)) * 1.0f, al = clamp01(t * 1.5f);
            ImVec2 c0(cx, y + h * k * 0.5f);
            ImU32 ink = C(10, 4, 2, (int)(200 * al));
            text_centre(dl, f, size * s, ImVec2(c0.x + size * 0.04f, c0.y + size * 0.05f), c, ink, ink, ink, 0.0f, al); // (shadow)
            text_centre(dl, f, size * s, c0, c, C(255, 255, 255), lite, C(12, 6, 4, (int)(255 * al)), fmaxf(1.5f, size * 0.055f), al);
        }
        y += (h + gap) * k;
    }
}

// A line of text across the panel at cy, at most h high, in the game's lettering; on two lines when one would be
// too small for the panel's width.
static void text_block(ImDrawList *dl, const Panel &p, const char *text, float cy, float h, float al) {
    if (text[0] == '\0' || al <= 0.0f) {
        return;
    }
    ImFont *f = face(Ui_FontForm());
    char l1[64], l2[64];
    float w = p.W * 0.88f, cx = p.X(p.W * 0.5f);
    float size = fit_size(f, text, w, h);
    const char *sp = strchr(text, ' ');
    if (size < h * 0.62f && sp != NULL) { // split at the space nearest the middle
        const char *best = sp;
        long half = (long)strlen(text) / 2;
        for (const char *q = sp; q != NULL; q = strchr(q + 1, ' ')) {
            if (labs((long)(q - text) - half) < labs((long)(best - text) - half)) {
                best = q;
            }
        }
        snprintf(l1, sizeof(l1), "%.*s", (int)(best - text), text);
        snprintf(l2, sizeof(l2), "%s", best + 1);
        size = fminf(fit_size(f, l1, w, h * 0.78f), fit_size(f, l2, w, h * 0.78f));
        game_text(dl, f, size, ImVec2(cx, cy - h * 0.48f), l1, al);
        game_text(dl, f, size, ImVec2(cx, cy + h * 0.48f), l2, al);
        return;
    }
    game_text(dl, f, size, ImVec2(cx, cy), text, al);
}

static void draw_panel(ImDrawList *dl, const Panel &p, const FpState &st, int side, Anim &an, double now) {
    const FpSide &s = st.side[side];
    const FpMember &m = s.m[s.active];
    Art *art = art_get(m.path, now);
    ImVec4 th = art != NULL ? art->theme : ImVec4(0.85f, 0.55f, 0.15f, 1.0f);

    int key = (s.active << 16) | (m.chara + 1);
    if (key != an.key) {
        an.swapAt = an.key == 0 ? an.enterAt : now;
        an.key = key;
    }
    bool ko = m.hpMax > 0 && m.hp <= 0;

    // where the portrait goes: the head a little toward the screen's edge, clear of the name
    float enter = ease_out((float)(now - an.enterAt - 0.05) / 0.55f);
    float swap = ease_out((float)(now - an.swapAt) / 0.35f);
    float sc = p.H * 0.78f / ART_H; // (large: the panel's height, the sides fading out)
    float out = (1.0f - enter) * (p.W + ART_W * sc * 0.5f) + (1.0f - swap) * p.W * 0.7f; // (comes in from the screen's edge)
    ImVec2 head(p.X(p.W * 0.58f + out), p.y0 + p.H * 0.26f); // the face, beside the name (still: asked for)

    background(dl, p, th, head, now);
    dl->PushClipRect(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), true);
    if (art != NULL) {
        float hx, hy;
        face_point(art, m.chara, m.costume, &hx, &hy);
        float left = head.x - hx * sc;
        ImVec2 a(left, head.y - hy * sc), b(left + ART_W * sc, head.y + (ART_H - hy) * sc);
        int A = (int)(255 * clamp01(enter * 1.5f));
        float sh = p.H * 0.006f, sx = p.right ? sh : -sh; // a dark offset copy behind, as a printed shadow
        art_faded(dl, p, art, BAND_SILHOUETTE, ImVec2(a.x + sx, a.y + sh), ImVec2(b.x + sx, b.y + sh), C(0, 0, 0, A * 100 / 255));
        art_faded(dl, p, art, ko ? BAND_GREY : BAND_COLOUR, a, b, ko ? C(150, 150, 160, A) : C(255, 255, 255, A));
    }
    if (swap < 1.0f) { // a new fighter: a white flash
        dl->AddRectFilled(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), C(255, 255, 255, (int)(170 * (1.0f - swap))));
    }
    char full[96], name[64], form[64];
    const char *nm = chara_name(m.chara);
    snprintf(full, sizeof(full), "%s", nm != NULL ? nm : "???");
    split_name(full, name, sizeof(name), form, sizeof(form));
    double nameAt = an.swapAt > an.enterAt ? an.swapAt : an.enterAt;
    // the name down the inner edge in its face, the form across the bottom in the game's lettering
    name_column(dl, p, name, th, nameAt, now);
    text_block(dl, p, form, p.y1 - p.H * 0.075f, p.H * 0.048f, clamp01((float)(now - nameAt - 0.45) / 0.3f));
    dl->PopClipRect();
    // a gold line along the screen's edge
    float ex = p.right ? p.x1 - 1.5f : p.x0 + 1.5f;
    dl->AddLine(ImVec2(ex, p.y0), ImVec2(ex, p.y1), C(214, 164, 62), 3.0f);
}

// Called by ui.cpp (bars_draw) with the picture's two edges and its height, each bar `bar` wide of which `trim` is the
// border along the picture. Returns false when there is no fight to show (the bars are drawn as usual then).
bool FightPanels_Draw(ImDrawList *dl, float lx, float rx, float y0, float y1, float bar, float trim) {
    FpState st;
    double now = ImGui::GetTime();
    bool battle = PortPanels_Read(&st) && st.battle;
    if (battle && !sWasBattle) {
        memset(sAnim, 0, sizeof(sAnim));
        sAnim[0].enterAt = sAnim[1].enterAt = now;
    }
    if (!battle && sWasBattle) {
        sEndAt = now;
    }
    sWasBattle = battle;
    if (battle) {
        sLast = st;
        sAlpha = clamp01((float)(now - sAnim[0].enterAt) / 0.35f);
    } else if (now - sEndAt < 0.45) {
        st = sLast; // (fading out with the last fight read)
        sAlpha = 1.0f - (float)(now - sEndAt) / 0.45f;
    } else {
        return false;
    }
    if (bar - trim < 8.0f) {
        sAlpha = 1.0f;
        return false;
    }
    int swap = Port_HudSwap() ? 1 : 0;
    for (int place = 0; place < 2; place++) {
        Panel p;
        p.right = place == 1;
        p.x0 = p.right ? rx + trim : lx - bar;
        p.x1 = p.right ? rx + bar : lx - trim;
        p.y0 = y0;
        p.y1 = y1;
        p.W = p.x1 - p.x0;
        p.H = y1 - y0;
        int side = place ^ swap;
        if (st.side[side].found && st.side[side].active >= 0 && st.side[side].active < FP_MEMBERS) {
            draw_panel(dl, p, st, side, sAnim[side], now);
        }
    }
    sAlpha = 1.0f;
    return true;
}

const char *TagHud_Name(int chara) { return chara_name(chara); }
