/*
 * PC only, included by plat_mods.c after plat_warrior_parts.inc (and by the offline check port/build/model/wp_check.c):
 * the colours of the Custom Character System.
 *
 * Every fighter texture is 4-bit with a palette of 16 colours (model part 11, the texture file: a record of 0x40 bytes
 * per texture from 0x20, its palette at word 1 * 4 + 0x60, PSMT4 in TEX0). The palettes' colours are grouped into
 * regions, each one material: a palette is mostly one material's shading ramp (or several, each in order), so
 * neighbours of one hue (a step of 6 degrees at most, strength 0.12) are one run and stay together; runs of the whole
 * model join in a region within 12 degrees and 0.2 of strength (greys by lightness, blacks one). The face texture's
 * palette and the hair's textures' palettes are classes of their own: their colours join the body's only when they
 * are that colour (4 degrees, 0.08), so eyebrows are not skin and black hair is not the outlines. A region knows what
 * it is: the skin (the body colour most of the face is drawn with), the hair, the face's own, and which parts of the
 * model draw it (each triangle's texture read at its corners and middle), so the screen can call it "Top & Sleeves"
 * or "Boots", and leave out what is drawn nowhere.
 *
 * A warrior's colour=<key>,<r>,<g>,<b> lines paint regions: the key is WARRIOR_KEY | the region's average colour
 * 0xBBGGRR (keys below 48 are the first version's hue bins). Each colour of the region takes the new hue (its own
 * difference from the region's average halved: warm shadows stay warm), the new strength (its difference kept), and
 * the new lightness with its shading kept: the shades below the average spread below the new one as far as they did,
 * or as far as there is room, and the lights above likewise. The shading ramps (16x16 PSMCT32 palettes drawn
 * SUBTRACTED from the texture, so they look like the inverse of their colour) of a region's textures are painted the
 * same way: shadow = colour - ramp, the shadow painted, the ramp made again.
 */
#ifndef WARRIOR_COLOURS
#define WARRIOR_COLOURS 16
#endif
#define WARRIOR_KEY 0x1000000
#define WARRIOR_KEY_BROWS 0x2000000   /* (with WARRIOR_KEY_KIND: the eyebrows' region */
#define WARRIOR_KEY_HAIR 0x4000000    /*  ... the hair's */
#define WARRIOR_KEY_FACEK 0x8000000   /*  ... the face texture's own) */
#define WARRIOR_KEY_KIND 0x10000000   /* the key says which kind of region (two regions may have the very same colour: Goku
                                         (End) Super Saiyan's brows and another); keys without it match any kind */

#define WARRIOR_ZONES 48
enum { WC_FACE = 1, WC_SKIN = 2, WC_HAIR = 4, WC_UNSEEN = 8, WC_BROWS = 16, WC_EYES = 32 }; /* (BROWS / EYES: the face's own, by where on it) */

typedef struct WarriorZone {
    float hx, hy, s, v; /* (sums: the hue as a vector, strength, lightness) */
    float vmin, vmax;
    int r, g, b, n;
    int grey;           /* 0 a colour, 1 a grey / white, 2 black */
    int face;           /* 1: only on the face texture */
    int cls;            /* its colours' palettes: 0 the body's, 1 the face's, 2 the hair's (mixed: 0), 3 + g a part's own
                           copies of colour group g (plat_warrior_parts.inc wp_split) */
    int flags, slots;   /* WC_*, the parts that use it (1 << slot, 1 << 12 any other) */
    int apart;          /* 1: the eyebrows' colours (wc_brow_entries), which join nothing else */
} WarriorZone;

/* The face palette's entries that are eyebrows (set by wc_analyse for warrior_zones; [palette][entry]). */
static uint8_t wc_apart[64][16];
static int wc_facePal = -1;    /* (wc_analyse for warrior_zones: the face texture's palette, how much of it each entry is) */
static int wc_faceCnt[16];
static int warrior_zone_kind(const WarriorZone *z) {
    return z->apart ? WARRIOR_KEY_BROWS : z->cls == 2 ? WARRIOR_KEY_HAIR : z->cls == 1 ? WARRIOR_KEY_FACEK : 0;
}


static void warrior_hsv(int r, int g, int b, float *h, float *s, float *v) {
    float mx = (float)(r > g ? (r > b ? r : b) : (g > b ? g : b)) / 255.0f;
    float mn = (float)(r < g ? (r < b ? r : b) : (g < b ? g : b)) / 255.0f;
    float d = mx - mn, rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;

    *v = mx;
    *s = mx > 0.0f ? d / mx : 0.0f;
    if (d <= 0.0f) {
        *h = 0.0f;
    } else if (mx == rf) {
        *h = (gf - bf) / d / 6.0f;
    } else if (mx == gf) {
        *h = ((bf - rf) / d + 2.0f) / 6.0f;
    } else {
        *h = ((rf - gf) / d + 4.0f) / 6.0f;
    }
    if (*h < 0.0f) {
        *h += 1.0f;
    }
}

static void warrior_rgb(float h, float s, float v, uint8_t *out) {
    float f, p, q, t, rgb[3];
    int i;

    h -= (float)(int)h;
    if (h < 0.0f) {
        h += 1.0f;
    }
    s = s < 0.0f ? 0.0f : s > 1.0f ? 1.0f : s;
    v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    h *= 6.0f;
    i = (int)h;
    f = h - (float)i;
    p = v * (1.0f - s);
    q = v * (1.0f - s * f);
    t = v * (1.0f - s * (1.0f - f));
    switch (i % 6) {
    case 0: rgb[0] = v; rgb[1] = t; rgb[2] = p; break;
    case 1: rgb[0] = q; rgb[1] = v; rgb[2] = p; break;
    case 2: rgb[0] = p; rgb[1] = v; rgb[2] = t; break;
    case 3: rgb[0] = p; rgb[1] = q; rgb[2] = v; break;
    case 4: rgb[0] = t; rgb[1] = p; rgb[2] = v; break;
    default: rgb[0] = v; rgb[1] = p; rgb[2] = q; break;
    }
    for (i = 0; i < 3; i++) {
        float c = rgb[i] * 255.0f + 0.5f;
        out[i] = (uint8_t)(c < 0.0f ? 0 : c > 255.0f ? 255 : c);
    }
}

/* The first version's hue bins (old colour= keys below 48). */
static int warrior_zone(int r, int g, int b) {
    float h, s, v;

    warrior_hsv(r, g, b, &h, &s, &v);
    if (v < 0.22f) {
        return 0;
    }
    if (s < 0.16f) {
        return v > 0.7f ? 1 : 2;
    }
    return (s >= 0.45f ? 16 : 32) + (int)((h * 360.0f + 15.0f) / 30.0f) % 12;
}

/* The palettes of a texture file: the place of each PSMT4 texture's 16 colours. */
static int warrior_palettes(const uint8_t *d, uint32_t size, uint32_t *at, int max) {
    uint32_t n, i;
    int count = 0;

    if (size < 0x20) {
        return 0;
    }
    memcpy(&n, d, 4);
    for (i = 0; i < n && 0x20 + i * 0x40 + 0x40 <= size; i++) {
        uint32_t f[4];
        uint64_t tex0;
        uint32_t o;

        memcpy(f, d + 0x20 + i * 0x40, 16);
        memcpy(&tex0, d + 0x20 + i * 0x40 + 0x30, 8);
        o = f[1] * 4 + 0x60;
        if (f[3] == 0 || ((tex0 >> 20) & 0x3F) != 0x14 || o + 64 > size || count >= max) {
            continue;
        }
        at[count++] = o;
    }
    return count;
}

static int warrior_zone_of(const uint8_t *e, float *h, float *s, float *v) {
    warrior_hsv(e[0], e[1], e[2], h, s, v);
    if (*v < 0.16f || (*s < 0.25f && *v < 0.24f)) {
        return 2;
    }
    return *s < 0.12f ? 1 : 0;
}

/* Which palette (of warrior_palettes' `at`) is the model's face texture's, -1 none. */
static int warrior_face_pal(const uint8_t *d, uint32_t size, const uint8_t *mdl, uint32_t mdlSize, const uint32_t *at, int pals) {
    uint32_t n, f[2];
    int p;

    if (mdl == NULL || mdlSize < 0x90 || size < 0x20) {
        return -1;
    }
    memcpy(&n, d, 4);
    if (mdl[0x4C] >= n || 0x20 + (uint32_t)mdl[0x4C] * 0x40 + 8 > size) {
        return -1;
    }
    memcpy(f, d + 0x20 + mdl[0x4C] * 0x40, 8);
    for (p = 0; p < pals; p++) {
        if (at[p] == f[1] * 4 + 0x60) {
            return p;
        }
    }
    return -1;
}

static float warrior_hue_gap(float a, float b) { /* in degrees */
    float d = fabsf(a - b);

    d -= (float)(int)d;
    return (d > 0.5f ? 1.0f - d : d) * 360.0f;
}

/* The regions, made of exact colours (no "close enough" between colours: that mixed eyebrows with hair, skin and
   halos). Each palette falls into shade families: runs of its entries that go on as one colour (same kind of colour,
   hue within 14 degrees and strength within 0.25 of the entry before; greys within 0.3 of lightness). Families of
   different palettes are one region only when they share a colour (an entry within 4 of an entry of the other in
   each of red, green and blue: the same gi orange on the chest's and the trousers' textures). The hair's never join
   another's, nor a part's own copies, nor the eyebrows' (wc_apart); the body's and the face texture's may (the
   skin). Black joins only black of its own palette class. Each palette colour gets its region in label[]. */
#define WZ_FAMS 1024

static int wz_root(int *up, int a) {
    while (up[a] != a) {
        up[a] = up[up[a]];
        a = up[a];
    }
    return a;
}

static int warrior_zones(const uint8_t *d, const uint32_t *at, int pals, WarriorZone *zn, uint8_t label[64][16], const uint8_t *cls) {
    static int famPal[WZ_FAMS], famA[WZ_FAMS], famB[WZ_FAMS], famGrey[WZ_FAMS], up[WZ_FAMS], zoneOf[WZ_FAMS];
    int fams = 0, count = 0, p, pass, k, f, g, z, start;

    for (pass = 0; pass < 4; pass++) { /* the families: the body's palettes first, then the hair's, the face's, the copies */
        for (p = 0; p < pals; p++) {
            int pc = cls[p];

            if (pass < 3 ? pc != (pass == 0 ? 0 : pass == 1 ? 2 : 1) : pc < 3) {
                continue;
            }
            for (start = 0; start < 16 && fams < WZ_FAMS;) {
                float h0, s0, v0, h, s, v;
                int grey = warrior_zone_of(d + at[p] + start * 4, &h0, &s0, &v0), end = start + 1;

                while (end < 16) {
                    int gg = warrior_zone_of(d + at[p] + end * 4, &h, &s, &v);

                    if (gg != grey || wc_apart[p][end] != wc_apart[p][start] || (gg == 0 && (warrior_hue_gap(h, h0) > 14.0f || fabsf(s - s0) > 0.25f)) ||
                        (gg == 1 && fabsf(v - v0) > 0.3f)) {
                        break;
                    }
                    h0 = h;
                    s0 = s;
                    v0 = v;
                    end++;
                }
                famPal[fams] = p;
                famA[fams] = start;
                famB[fams] = end;
                famGrey[fams] = grey;
                up[fams] = fams;
                fams++;
                start = end;
            }
        }
    }
    for (f = 0; f < fams; f++) { /* families sharing a colour are one region */
        int cf = cls[famPal[f]];

        for (g = f + 1; g < fams; g++) {
            int cg = cls[famPal[g]], a, b, share = 0;

            if (famGrey[f] != famGrey[g] || wc_apart[famPal[f]][famA[f]] != wc_apart[famPal[g]][famA[g]] || wz_root(up, f) == wz_root(up, g)) {
                continue;
            }
            if (cf != cg && (cf >= 2 || cg >= 2)) {
                continue; /* (the hair's and a part's own copies keep to themselves; the body's and the face's may meet) */
            }
            if (famGrey[f] == 2 && cf != cg) {
                continue; /* (black hair is not the outlines) */
            }
            for (a = famA[f]; a < famB[f] && !share; a++) {
                const uint8_t *x = d + at[famPal[f]] + a * 4;

                for (b = famA[g]; b < famB[g] && !share; b++) {
                    const uint8_t *y = d + at[famPal[g]] + b * 4;

                    share = abs(x[0] - y[0]) <= 4 && abs(x[1] - y[1]) <= 4 && abs(x[2] - y[2]) <= 4;
                }
            }
            if (share) {
                up[wz_root(up, g)] = wz_root(up, f);
            }
        }
    }
    for (f = 0; f < fams; f++) { /* the hair: one colour across its textures (12 degrees, the hair's own families only) */
        float hf, sf2, vf;

        if (cls[famPal[f]] != 2 || famGrey[f] != 0) {
            continue;
        }
        warrior_zone_of(d + at[famPal[f]] + famA[f] * 4, &hf, &sf2, &vf);
        for (g = f + 1; g < fams; g++) {
            float hg, sg, vg;

            if (cls[famPal[g]] != 2 || famGrey[g] != 0) {
                continue;
            }
            warrior_zone_of(d + at[famPal[g]] + famA[g] * 4, &hg, &sg, &vg);
            if (warrior_hue_gap(hf, hg) < 12.0f && fabsf(sf2 - sg) < 0.2f) {
                up[wz_root(up, g)] = wz_root(up, f);
            }
        }
    }
    if (wc_facePal >= 0 && wc_facePal < pals) { /* the face texture's own shades of its skin: with the skin (15 degrees) */
        int sk = -1, sf = -1;
        float hs, ss, vs;

        for (k = 0; k < 16; k++) { /* its skin: the coloured entry most of it is */
            float h, s2, v;

            if (warrior_zone_of(d + at[wc_facePal] + k * 4, &h, &s2, &v) == 0 && !wc_apart[wc_facePal][k] && (sk < 0 || wc_faceCnt[k] > wc_faceCnt[sk])) {
                sk = k;
            }
        }
        for (f = 0; sk >= 0 && f < fams; f++) {
            if (famPal[f] == wc_facePal && sk >= famA[f] && sk < famB[f]) {
                sf = f;
            }
        }
        if (sf >= 0) {
            warrior_zone_of(d + at[wc_facePal] + sk * 4, &hs, &ss, &vs);
        }
        for (f = 0; sf >= 0 && f < fams; f++) {
            float h, s2, v;

            if (famPal[f] != wc_facePal || famGrey[f] != 0 || wc_apart[famPal[f]][famA[f]]) {
                continue;
            }
            warrior_zone_of(d + at[famPal[f]] + famA[f] * 4, &h, &s2, &v);
            if (warrior_hue_gap(h, hs) < 15.0f && fabsf(s2 - ss) < 0.25f) {
                up[wz_root(up, f)] = wz_root(up, sf);
            }
        }
    }
    for (f = 0; f < fams; f++) {
        zoneOf[f] = -1;
    }
    for (f = 0; f < fams; f++) { /* the regions, in the families' order */
        int r = wz_root(up, f), pc = cls[famPal[f]];
        float h, s, v;

        if (zoneOf[r] < 0) {
            if (count >= WARRIOR_ZONES) {
                zoneOf[r] = count - 1;
            } else {
                zoneOf[r] = count++;
                memset(&zn[zoneOf[r]], 0, sizeof(zn[0]));
                zn[zoneOf[r]].grey = famGrey[f];
                zn[zoneOf[r]].cls = pc;
                zn[zoneOf[r]].face = pc == 1;
                zn[zoneOf[r]].apart = wc_apart[famPal[f]][famA[f]];
                zn[zoneOf[r]].vmin = 1.0f;
                zn[zoneOf[r]].vmax = 0.0f;
            }
        }
        z = zoneOf[r];
        if (zn[z].cls != pc && pc < 3) { /* (a body colour also on the face: the body's) */
            zn[z].cls = 0;
            zn[z].face = 0;
        }
        for (k = famA[f]; k < famB[f]; k++) {
            const uint8_t *e = d + at[famPal[f]] + k * 4;

            warrior_zone_of(e, &h, &s, &v);
            zn[z].hx += cosf(h * 6.2831853f);
            zn[z].hy += sinf(h * 6.2831853f);
            zn[z].s += s;
            zn[z].v += v;
            zn[z].vmin = v < zn[z].vmin ? v : zn[z].vmin;
            zn[z].vmax = v > zn[z].vmax ? v : zn[z].vmax;
            zn[z].r += e[0];
            zn[z].g += e[1];
            zn[z].b += e[2];
            zn[z].n++;
            label[famPal[f]][k] = (uint8_t)z;
        }
    }
    return count;
}

static uint32_t warrior_zone_rgb(const WarriorZone *z) {
    return (uint32_t)(z->r / z->n) | ((uint32_t)(z->g / z->n) << 8) | ((uint32_t)(z->b / z->n) << 16);
}

/* The region a key names: the one whose average colour is nearest the key's (-1 none near enough). */
static int warrior_zone_find(const WarriorZone *zn, int count, int key) {
    int best = -1, bestD = 24 * 24 * 3, z;

    for (z = 0; z < count; z++) {
        uint32_t c = warrior_zone_rgb(&zn[z]);
        int dr = (int)(c & 0xFF) - (key & 0xFF), dg = (int)((c >> 8) & 0xFF) - ((key >> 8) & 0xFF), db = (int)((c >> 16) & 0xFF) - ((key >> 16) & 0xFF);
        int dd = dr * dr + dg * dg + db * db;

        if (dd < bestD) {
            bestD = dd;
            best = z;
        }
    }
    return best;
}

/* The region a key names among a part's own (cls 3 + g), or among the rest (cls -1). */
static int warrior_zone_find_in(const WarriorZone *zn, int count, int key, int cls) {
    int best = -1, bestD = 24 * 24 * 3, z;

    for (z = 0; z < count; z++) {
        if ((key & WARRIOR_KEY_KIND) && cls < 0 && warrior_zone_kind(&zn[z]) != (key & (WARRIOR_KEY_BROWS | WARRIOR_KEY_HAIR | WARRIOR_KEY_FACEK))) {
            continue; /* (another kind of region of the same colour) */
        }
        uint32_t c = warrior_zone_rgb(&zn[z]);
        int dr = (int)(c & 0xFF) - (key & 0xFF), dg = (int)((c >> 8) & 0xFF) - ((key >> 8) & 0xFF), db = (int)((c >> 16) & 0xFF) - ((key >> 16) & 0xFF);
        int dd = dr * dr + dg * dg + db * db;

        if ((cls < 0 ? zn[z].cls >= 3 : zn[z].cls != cls) || dd >= bestD) {
            continue;
        }
        bestD = dd;
        best = z;
    }
    return best;
}

/* The colour groups of colour lines: their part masks, each once, in order (WP_SPLIT_GROUPS at most). */
static int wp_colour_groups(const int *part, int count, int *groups) {
    int n = 0, c, g;

    for (c = 0; part != NULL && c < count; c++) {
        if (part[c] == 0) {
            continue;
        }
        for (g = 0; g < n && groups[g] != part[c]; g++) {
        }
        if (g == n && n < WP_SPLIT_GROUPS) {
            groups[n++] = part[c];
        }
    }
    return n;
}

/* A colour of region z given the region's new colour `rgb` (see the top). */
static void warrior_paint(uint8_t *e, const WarriorZone *z, uint32_t rgb) {
    float h, s, v, th, ts, tv, mh, mv = z->v / (float)z->n, ms = z->s / (float)z->n, below, above, k;

    warrior_hsv((int)(rgb & 0xFF), (int)((rgb >> 8) & 0xFF), (int)((rgb >> 16) & 0xFF), &th, &ts, &tv);
    warrior_hsv(e[0], e[1], e[2], &h, &s, &v);
    mh = atan2f(z->hy, z->hx) / 6.2831853f;
    if (z->grey == 0 && ts > 0.05f) { /* (the colour's own hue difference, halved) */
        float dh = h - mh;

        while (dh > 0.5f) {
            dh -= 1.0f;
        }
        while (dh < -0.5f) {
            dh += 1.0f;
        }
        th += dh * 0.5f;
    }
    s = ts + (s - ms) * (ts > 0.05f ? 1.0f : 0.0f);
    below = mv - z->vmin;
    above = z->vmax - mv;
    if (v < mv) {
        k = below > 0.001f ? (tv - 0.03f) / below : 1.0f;
    } else {
        k = above > 0.001f ? (1.0f - tv) / above : 1.0f;
    }
    k = k > 1.0f ? 1.0f : k < 0.0f ? 0.0f : k;
    warrior_rgb(th, s, tv + (v - mv) * k, e);
}

/* ---- reading a texture's colour numbers (4-bit textures are uploaded as 32-bit rectangles: port/src/gs/gs_core.c's
   GS addressing, port/build/model/gsmem.py) ---- */
static const uint8_t kWcBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const uint8_t kWcBlock16[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static const uint8_t kWcCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                       {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const uint8_t kWcCol4First[4][32] = {
    {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
    {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
    {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
    {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63}};
#define WC_MEM_WORDS (1 << 20)

static uint32_t wc_addr32(uint32_t bw, uint32_t x, uint32_t y) {
    return ((((y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + kWcBlock32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + kWcCol32[y & 7][x & 7]);
}

static uint32_t wc_addr4(uint32_t bw, uint32_t x, uint32_t y) {
    uint32_t c = (y & 15) >> 2, xx = x & 31;

    xx = (c & 1) ? xx ^ 4 : xx;
    return ((((y >> 2) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kWcBlock16[(y >> 4) & 7][(x >> 5) & 3]) << 9) + kWcCol4First[y & 3][xx] + c * 128;
}

/* Texture i's colour numbers (w x h, malloc'ed), NULL when it is not a 4-bit one. */
static uint8_t *wc_indices(const uint8_t *d, uint32_t size, const WpTex *e, int *w, int *h) {
    static uint32_t *mem;
    const uint8_t *pix;
    uint32_t upw, uph, dbw = e->pixBlt & 0x3F, tbw = (uint32_t)(e->tex0 >> 14) & 0x3F, x, y;
    uint8_t *out;

    if (((e->tex0 >> 20) & 0x3F) != 0x14 || e->pixOfs == 0 || e->pixOfs * 4 + 0x60 > size) {
        return NULL;
    }
    pix = d + e->pixOfs * 4;
    memcpy(&upw, pix + 0x30, 4);
    memcpy(&uph, pix + 0x34, 4);
    *w = 1 << ((e->tex0 >> 26) & 0xF);
    *h = 1 << ((e->tex0 >> 30) & 0xF);
    if (upw == 0 || uph == 0 || e->pixOfs * 4 + 0x60 + upw * uph * 4 > size || *w > 1024 || *h > 1024) {
        return NULL;
    }
    if (mem == NULL && (mem = calloc(WC_MEM_WORDS, 4)) == NULL) {
        return NULL;
    }
    for (y = 0; y < uph; y++) {
        for (x = 0; x < upw; x++) {
            uint32_t a = wc_addr32(dbw > 0 ? dbw : 1, x, y), v;

            if (a < WC_MEM_WORDS) {
                memcpy(&v, pix + 0x60 + (y * upw + x) * 4, 4);
                mem[a] = v;
            }
        }
    }
    if ((out = malloc((size_t)(*w * *h))) == NULL) {
        return NULL;
    }
    for (y = 0; y < (uint32_t)*h; y++) {
        for (x = 0; x < (uint32_t)*w; x++) {
            uint32_t a = wc_addr4(tbw, x, y);

            out[y * (uint32_t)*w + x] = (a >> 3) < WC_MEM_WORDS ? (uint8_t)((mem[a >> 3] >> ((a & 7) * 4)) & 15) : 0;
        }
    }
    return out;
}

/* ---- the generated shading ramps made again from the pictures' colours as they are now (after the colours were
   painted): each marked ramp (wp_compose) from the main colour of the picture it shades, its most drawn colour that
   is solid and not near black. Returns how many. ---- */
static int wp_shade_regen(uint8_t *tex, uint32_t size) {
    static WpTex ent[WP_MAX_TEX];
    int count = wp_textures(tex, size, ent, WP_MAX_TEX), t, n = 0;
    uint32_t at = size >= 8 ? wp_u32(tex + 4) * 4 : 0;

    for (t = 0; t < count; t++) {
        uint32_t m1 = wp_u32(tex + at + t * 0x40 + 0x38), m2 = wp_u32(tex + at + t * 0x40 + 0x3C), base = 0x909090;
        int pic = (int)((m1 >> 16) & 0xFF), w, h, i, cnt[16], best = -1;
        uint8_t *ix;
        const uint8_t *pal;

        if ((m1 >> 30) != 2 || (m2 >> 24) != 0xA5 || pic >= count || ent[t].pixOfs == 0 || ent[t].pixOfs * 4 + 0x60 + 1024 > size) {
            continue;
        }
        if (ent[pic].clutOfs != 0 && ent[pic].clutOfs * 4 + 0x60 + 64 <= size) {
            pal = tex + ent[pic].clutOfs * 4 + 0x60;
            base = wp_pal_main(pal, 16);
            if ((ix = wc_indices(tex, size, &ent[pic], &w, &h)) != NULL) {
                memset(cnt, 0, sizeof(cnt));
                for (i = 0; i < w * h; i++) {
                    cnt[ix[i] & 15]++;
                }
                free(ix);
                for (i = 0; i < 16; i++) {
                    if (pal[i * 4 + 3] >= 0x40 && pal[i * 4] * 30 + pal[i * 4 + 1] * 59 + pal[i * 4 + 2] * 11 >= 4000 && (best < 0 || cnt[i] > cnt[best])) {
                        best = i;
                    }
                }
                if (best >= 0 && cnt[best] > 0) {
                    base = (uint32_t)pal[best * 4] | (uint32_t)pal[best * 4 + 1] << 8 | (uint32_t)pal[best * 4 + 2] << 16;
                }
            }
        }
        wp_gen_ramp(tex + ent[t].pixOfs * 4 + 0x60, m1 & 0xFFFF, m2 & 0xFFFFFF, base);
        n++;
    }
    return n;
}

/* ---- what the regions are ---- */
typedef struct WcInfo {
    uint32_t at[64];
    int pals, facePal, zones, ramps;
    uint8_t cls[64]; /* each palette: 0 the body's, 1 the face texture's, 2 the hair's */
    uint8_t label[64][16];
    WarriorZone zn[WARRIOR_ZONES];
    int rampZone[16];
    uint32_t rampAt[16];
    int tally[WARRIOR_ZONES][13]; /* how much of each region each part draws (1 << slot; 12 any other) */
} WcInfo;

typedef struct WcUseCtx {
    WcInfo *in;
    const uint8_t *d;
    uint32_t size;
    const WpTex *ent;
    int texCount, rampCount, slot;
    int palOf[WP_MAX_TEX];  /* each texture's palette, -1 none */
    uint8_t *idx[WP_MAX_TEX];
    int iw[WP_MAX_TEX], ih[WP_MAX_TEX], tried[WP_MAX_TEX];
    uint8_t link[16][WP_MAX_TEX]; /* ramp r is drawn over texture t */
    int curRamp;                  /* the ramp of the batch being read (-1 none) */
    int rampUse[16][WARRIOR_ZONES]; /* how much of each region each ramp shades */
} WcUseCtx;

static void wc_sample(WcUseCtx *c, int t, float s, float u) {
    int x, y, k, p = c->palOf[t];

    if (p < 0 || c->idx[t] == NULL) {
        return;
    }
    s -= floorf(s);
    u -= floorf(u);
    x = (int)(s * (float)c->iw[t]) % c->iw[t];
    y = (int)(u * (float)c->ih[t]) % c->ih[t];
    k = c->idx[t][y * c->iw[t] + x];
    c->in->tally[c->in->label[p][k]][c->slot >= 0 ? c->slot : 12]++;
    if (c->curRamp >= 0) {
        c->rampUse[c->curRamp][c->in->label[p][k]]++;
    }
}

static void wc_use_batch(void *ctx, uint32_t *h, float *v, int count) {
    WcUseCtx *c = (WcUseCtx *)ctx;
    uint64_t a = (uint64_t)h[4] | ((uint64_t)h[5] << 32), b = (uint64_t)h[8] | ((uint64_t)h[9] << 32);
    uint32_t tbp = (uint32_t)(a & 0x3FFF), cbp = (uint32_t)((a >> 37) & 0x3FFF), ramp = (uint32_t)((b >> 37) & 0x3FFF);
    int t, r, i;

    for (t = 0; t < c->texCount && (c->ent[t].tbpOfs != tbp || c->ent[t].cbpOfs != cbp); t++) {
    }
    for (r = 0; r < c->rampCount && c->ent[c->texCount + r].tbpOfs != ramp; r++) {
    }
    if (t < c->texCount && r < c->rampCount && r < 16) {
        c->link[r][t] = 1;
    }
    if (t >= c->texCount) {
        return;
    }
    c->curRamp = r < c->rampCount && r < 16 ? r : -1;
    if (!c->tried[t]) {
        c->tried[t] = 1;
        c->idx[t] = wc_indices(c->d, c->size, &c->ent[t], &c->iw[t], &c->ih[t]);
    }
    for (i = 0; i < count; i++) { /* each vertex, and each triangle's middle */
        const float *q = v + i * 12;

        wc_sample(c, t, q[8], q[9]);
        if (i >= 2) {
            const float *q1 = v + (i - 1) * 12, *q2 = v + (i - 2) * 12;

            wc_sample(c, t, (q[8] + q1[8] + q2[8]) / 3.0f, (q[9] + q1[9] + q2[9]) / 3.0f);
        }
    }
}

/* Everything about a look's colours: its palettes and their classes, its regions, what each region is and where it
   is drawn, each ramp's region. */
static void wc_analyse(const uint8_t *d, uint32_t size, const uint8_t *mdl, uint32_t mdlSize, WcInfo *in) {
    static WcUseCtx c;
    static WpRec recs[WP_MAX_RECS * 2];
    static WpTex ent[WP_MAX_TEX];
    static uint32_t hairKey[32];
    int nrec = 0, nent = 0, r, t, p, k, z, hairs = 0, skin = -1;

    memset(in, 0, sizeof(*in));
    in->pals = warrior_palettes(d, size, in->at, 64);
    in->facePal = warrior_face_pal(d, size, mdl, mdlSize, in->at, in->pals);
    memset(&c, 0, sizeof(c));
    if (mdl != NULL) {
        nrec = wp_records(mdl, mdlSize, recs, WP_MAX_RECS * 2);
        nent = wp_textures(d, size, ent, WP_MAX_TEX);
        hairs = nrec > 0 ? wp_hair_textures(mdl, recs, nrec, hairKey, 32) : 0;
    }
    c.in = in;
    c.d = d;
    c.size = size;
    c.ent = ent;
    c.texCount = mdl != NULL && mdl[0x4F] <= nent ? mdl[0x4F] : 0;
    c.rampCount = mdl != NULL && c.texCount + mdl[0x51] <= nent ? (mdl[0x51] < 16 ? mdl[0x51] : 16) : 0;
    for (t = 0; t < c.texCount; t++) { /* each texture's palette, and the palettes' classes */
        c.palOf[t] = -1;
        for (p = 0; p < in->pals; p++) {
            if (in->at[p] == ent[t].clutOfs * 4 + 0x60) {
                c.palOf[t] = p;
                if (p != in->facePal && wp_is_hair(hairKey, hairs, ent[t].tbpOfs | (ent[t].cbpOfs << 16))) {
                    in->cls[p] = 2;
                }
            }
        }
    }
    if (in->facePal >= 0) {
        in->cls[in->facePal] = 1;
    }
    for (t = 0; t < c.texCount; t++) { /* a part's own copies */
        if (c.palOf[t] >= 0 && (ent[t].unk28 & 0xFF000000u) == WP_SPLIT_MARK) {
            in->cls[c.palOf[t]] = (uint8_t)(2 + (ent[t].unk28 & 0xFF));
        }
    }
    memset(wc_apart, 0, sizeof(wc_apart));
    wc_facePal = -1;
    if (mdl != NULL && in->facePal >= 0 && mdl[0x4C] < nent) { /* the eyebrows' entries of the face palette (kept apart) */
        int w, h, x, y, dd, cnt[16], below[16], above[16], side[16];
        uint8_t wh[16], *ix = wc_indices(d, size, &ent[mdl[0x4C]], &w, &h);
        const uint8_t *pal = d + in->at[in->facePal];

        if (ix != NULL) {
            memset(cnt, 0, sizeof(cnt));
            memset(below, 0, sizeof(below));
            memset(above, 0, sizeof(above));
            memset(side, 0, sizeof(side));
            for (k = 0; k < w * h; k++) {
                cnt[ix[k]]++;
            }
            wc_facePal = in->facePal;
            memcpy(wc_faceCnt, cnt, sizeof(wc_faceCnt));
            for (k = 0; k < 16; k++) { /* the whites: light greys, not the skin (the texture's most) */
                int lo = pal[k * 4] < pal[k * 4 + 1] ? pal[k * 4] : pal[k * 4 + 1], hi = pal[k * 4] > pal[k * 4 + 1] ? pal[k * 4] : pal[k * 4 + 1], most = 1;

                lo = lo < pal[k * 4 + 2] ? lo : pal[k * 4 + 2];
                hi = hi > pal[k * 4 + 2] ? hi : pal[k * 4 + 2];
                for (t = 0; t < 16; t++) {
                    most &= cnt[t] <= cnt[k];
                }
                wh[k] = cnt[k] > 0 && !most && hi - lo < 40 && lo > 150;
            }
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    int kk = ix[y * w + x], b = 0, a = 0, sd = 0;

                    if (wh[kk]) {
                        continue;
                    }
                    for (dd = 1; dd <= 6; dd++) {
                        b |= y + dd < h && wh[ix[(y + dd) * w + x]];
                        a |= y - dd >= 0 && wh[ix[(y - dd) * w + x]];
                        sd |= (x + dd < w && wh[ix[y * w + x + dd]]) && (x - dd >= 0 && wh[ix[y * w + x - dd]]);
                    }
                    below[kk] += b && !a;
                    above[kk] += a;
                    side[kk] += sd;
                }
            }
            for (k = 0; k < 16; k++) { /* lying over the whites (just above them, none above it), not a small dot among them; */
                float hh, ss, vv;                 /* counted with every entry of the same colour (a palette may hold one */
                int g = warrior_zone_of(pal + k * 4, &hh, &ss, &vv), j, sc = 0, sb = 0, sa = 0, sd = 0; /* colour twice: */
                                                  /* Goku (End) Super Saiyan's brows are entries 12 and 13, both drawn) */
                for (j = 0; j < 16; j++) {
                    if (abs(pal[j * 4] - pal[k * 4]) <= 4 && abs(pal[j * 4 + 1] - pal[k * 4 + 1]) <= 4 && abs(pal[j * 4 + 2] - pal[k * 4 + 2]) <= 4) {
                        sc += cnt[j];
                        sb += below[j];
                        sa += above[j];
                        sd += side[j];
                    }
                }
                wc_apart[in->facePal][k] = g == 0 && sc >= 40 && sb >= 6 && sb > sa * 3 && sd * 2 < sb;
            }
            free(ix);
        }
    }
    in->zones = warrior_zones(d, in->at, in->pals, in->zn, in->label, in->cls);
    memset(wc_apart, 0, sizeof(wc_apart));
    for (r = 0; r < nrec; r++) {
        c.slot = recs[r].node < 0x47 ? wp_slot_of(recs[r].node) : wp_chain_slot_of(recs, r);
        wp_batches((uint8_t *)mdl, &recs[r], wc_use_batch, &c);
    }
    for (t = 0; t < c.texCount; t++) {
        free(c.idx[t]);
    }
    for (z = 0; z < in->zones; z++) { /* the parts each region is on (an eighth of its use at least), seen at all */
        int total = 0, s;

        for (s = 0; s < 13; s++) {
            total += in->tally[z][s];
        }
        in->zn[z].flags = in->zn[z].face ? WC_FACE : 0;
        in->zn[z].slots = 0;
        for (s = 0; s < 13; s++) {
            if (in->tally[z][s] > 0 && in->tally[z][s] * 8 >= total) {
                in->zn[z].slots |= 1 << s;
            }
        }
        if (total == 0 && nrec > 0) {
            in->zn[z].flags |= WC_UNSEEN;
        }
        if (!in->zn[z].face && in->zn[z].grey != 2 && in->zn[z].n >= 3 && in->tally[z][WP_FACE] > 0 &&
            (skin < 0 || in->tally[z][WP_FACE] > in->tally[skin][WP_FACE])) { /* (a white skin too: Frieza) */
            skin = z; /* the skin: the body colour most of the face is */
        }
    }
    if (skin >= 0) {
        in->zn[skin].flags |= WC_SKIN;
    }
    for (z = 0; z < in->zones; z++) { /* the eyebrows: the colours kept apart for them */
        if (in->zn[z].apart && in->zn[z].grey != 1) {
            in->zn[z].flags |= WC_BROWS | WC_FACE;
        }
    }
    if (mdl != NULL && in->facePal >= 0 && mdl[0x4C] < nent) { /* the face's own: the eyebrows above the eyes' whites, the eyes on them */
        int w, h, x, y, cnt[16], white = -1, best = -1;
        double ys[16], zy[WARRIOR_ZONES], zc[WARRIOR_ZONES];
        uint8_t *ix = wc_indices(d, size, &ent[mdl[0x4C]], &w, &h);
        const uint8_t *pal = d + in->at[in->facePal];

        if (ix != NULL) {
            memset(cnt, 0, sizeof(cnt));
            memset(ys, 0, sizeof(ys));
            memset(zy, 0, sizeof(zy));
            memset(zc, 0, sizeof(zc));
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    cnt[ix[y * w + x]]++;
                    ys[ix[y * w + x]] += y;
                }
            }
            for (k = 0; k < 16; k++) { /* the whites: the lightest grey drawn */
                int lo = pal[k * 4] < pal[k * 4 + 1] ? pal[k * 4] : pal[k * 4 + 1], hi = pal[k * 4] > pal[k * 4 + 1] ? pal[k * 4] : pal[k * 4 + 1];

                lo = lo < pal[k * 4 + 2] ? lo : pal[k * 4 + 2];
                hi = hi > pal[k * 4 + 2] ? hi : pal[k * 4 + 2];
                if (cnt[k] > 0 && in->label[in->facePal][k] != skin && hi - lo < 40 && lo > 150 && (white < 0 || lo > best)) {
                    white = k;
                    best = lo;
                }
                zy[in->label[in->facePal][k]] += ys[k];
                zc[in->label[in->facePal][k]] += cnt[k];
            }
            { /* where the whites are next to each colour: eyebrows have them just below and not above (they lie over the
                 eyes), the eyes' colours have them on both sides; where on the texture does not tell (a second, small eye
                 drawn elsewhere on it moved the averages) */
                static int below[WARRIOR_ZONES], above[WARRIOR_ZONES], side[WARRIOR_ZONES];
                uint8_t wh[16];
                int dd;

                memset(below, 0, sizeof(below));
                memset(above, 0, sizeof(above));
                memset(side, 0, sizeof(side));
                for (k = 0; k < 16; k++) {
                    int lo = pal[k * 4] < pal[k * 4 + 1] ? pal[k * 4] : pal[k * 4 + 1], hi = pal[k * 4] > pal[k * 4 + 1] ? pal[k * 4] : pal[k * 4 + 1];

                    lo = lo < pal[k * 4 + 2] ? lo : pal[k * 4 + 2];
                    hi = hi > pal[k * 4 + 2] ? hi : pal[k * 4 + 2];
                    wh[k] = cnt[k] > 0 && in->label[in->facePal][k] != skin && hi - lo < 40 && lo > 150;
                }
                for (y = 0; white >= 0 && y < h; y++) {
                    for (x = 0; x < w; x++) {
                        int kk = ix[y * w + x], zz = in->label[in->facePal][kk], b = 0, t = 0, sd = 0;

                        if (wh[kk]) {
                            continue;
                        }
                        for (dd = 1; dd <= 6; dd++) {
                            b |= y + dd < h && wh[ix[(y + dd) * w + x]];
                            t |= y - dd >= 0 && wh[ix[(y - dd) * w + x]];
                            sd |= (x + dd < w && wh[ix[y * w + x + dd]]) && (x - dd >= 0 && wh[ix[y * w + x - dd]]);
                        }
                        below[zz] += b && !t;
                        above[zz] += t;
                        side[zz] += sd;
                    }
                }
                for (z = 0; white >= 0 && z < in->zones; z++) {
                    if (!in->zn[z].face || z == skin || zc[z] <= 0 || z == in->label[in->facePal][white] || (in->zn[z].slots & ~(1 << WP_FACE)) != 0 ||
                        in->zn[z].grey != 0) {
                        continue;
                    }
                    if (in->zn[z].apart) {
                        in->zn[z].flags |= WC_BROWS;
                    } else if ((side[z] >= 4 || (above[z] >= 4 && below[z] >= 4)) &&
                               (skin < 0 || warrior_hue_gap(atan2f(in->zn[z].hy, in->zn[z].hx) / 6.2831853f, atan2f(in->zn[skin].hy, in->zn[skin].hx) / 6.2831853f) > 20.0f)) {
                        /* (a shade of the skin by the eyes is not the eyes) */
                        in->zn[z].flags |= WC_EYES;
                    }
                }
            }
            free(ix);
        }
    }
    for (z = 0; z < in->zones; z++) { /* the hair: on the hair's textures, or drawn mostly on the hair */
        int total = 0, s;

        for (s = 0; s < 13; s++) {
            total += in->tally[z][s];
        }
        if (z != skin && !in->zn[z].face && (in->zn[z].cls == 2 || (total > 0 && in->tally[z][WP_HAIR] * 5 >= total * 3))) {
            in->zn[z].flags |= WC_HAIR;
        }
    }
    for (r = 0; r < c.rampCount; r++) { /* each ramp's region: half of what it shades (else three in five of its textures' colours) */
        int count[WARRIOR_ZONES], total = 0, best = -1, drawn = 0;

        memset(count, 0, sizeof(count));
        in->rampZone[r] = -1;
        in->rampAt[r] = ent[c.texCount + r].pixOfs * 4 + 0x60;
        if (ent[c.texCount + r].pixOfs == 0 || in->rampAt[r] + 256 * 4 > size) {
            continue;
        }
        for (z = 0; z < in->zones; z++) {
            if (best < 0 || c.rampUse[r][z] > c.rampUse[r][best]) {
                best = z;
            }
            drawn += c.rampUse[r][z];
        }
        if (best >= 0 && drawn > 0 && c.rampUse[r][best] * 2 >= drawn) {
            in->rampZone[r] = best;
            continue;
        }
        best = -1;
        for (t = 0; t < c.texCount; t++) {
            if (!c.link[r][t] || c.palOf[t] < 0) {
                continue;
            }
            for (k = 0; k < 16; k++) {
                count[in->label[c.palOf[t]][k]]++;
                total++;
            }
        }
        for (z = 0; z < in->zones; z++) {
            if (best < 0 || count[z] > count[best]) {
                best = z;
            }
        }
        if (best >= 0 && total > 0 && count[best] * 5 >= total * 3) {
            in->rampZone[r] = best;
        }
    }
    in->ramps = c.rampCount;
}

/* ---- colours picked on a texture (Z Warriors' eyedropper) ----
 * A colour line whose key has WARRIOR_TINT names a texture by its picture (wc_tex_hash: the same picture whatever
 * the model it is put in, so a part swapped elsewhere does not move it) and one of its palette's 16 colours, or 16:
 * all of them. That colour becomes the line's exactly (its alpha, the game's number, kept); "all" turns every colour
 * of the palette as its most drawn one turns (hue, strength, lightness). The shading ramps the texture is drawn with
 * follow its most drawn colour. */
#define WARRIOR_TINT 0x40000000
#define WT_KEY(hash, entry) (WARRIOR_TINT | (int)(((hash) & 0xFFFFFFu) << 5) | (entry))

/* The picture of texture e (its pixels, not where they go): a 24-bit number. 0 when it has none. */
static uint32_t wc_tex_hash(const uint8_t *d, uint32_t size, const WpTex *e) {
    uint32_t h = 2166136261u, i, from = e->pixOfs * 4 + 0x60, len;

    if (e->pixOfs == 0 || e->pixSize <= 0x60 || from + (e->pixSize - 0x60) > size) {
        return 0;
    }
    len = e->pixSize - 0x60;
    for (i = 0; i < len; i++) {
        h = (h ^ d[from + i]) * 16777619u;
    }
    return (h ^ (h >> 24)) & 0xFFFFFF;
}

typedef struct WcTexInfo { /* a model's textures as the eyedropper sees them */
    int count, texCount, rampCount;
    WpTex ent[WP_MAX_TEX];
    uint32_t hash[WP_MAX_TEX];
    int slots[WP_MAX_TEX];     /* the parts drawing each (1 << slot, 1 << 12 any other) */
    int most[WP_MAX_TEX];      /* its most drawn palette colour */
    uint8_t link[16][WP_MAX_TEX]; /* ramp r is drawn over texture t */
    int node;
    const WpRec *recs;
    int nrec, rec;
} WcTexInfo;

static void wc_texinfo_batch(void *ctx, uint32_t *h, float *v, int count) {
    WcTexInfo *ti = (WcTexInfo *)ctx;
    uint64_t a = (uint64_t)h[4] | ((uint64_t)h[5] << 32), b = (uint64_t)h[8] | ((uint64_t)h[9] << 32);
    uint32_t tbp = (uint32_t)(a & 0x3FFF), cbp = (uint32_t)((a >> 37) & 0x3FFF), ramp = (uint32_t)((b >> 37) & 0x3FFF);
    int t, r, slot = ti->node < 0x47 ? wp_slot_of(ti->node) : wp_chain_slot_of(ti->recs, ti->rec);

    (void)v;
    (void)count;
    for (t = 0; t < ti->texCount && (ti->ent[t].tbpOfs != tbp || ti->ent[t].cbpOfs != cbp); t++) {
    }
    for (r = 0; r < ti->rampCount && ti->ent[ti->texCount + r].tbpOfs != ramp; r++) {
    }
    if (t >= ti->texCount) {
        return;
    }
    ti->slots[t] |= 1 << (slot >= 0 ? slot : 12);
    if (r < ti->rampCount && r < 16) {
        ti->link[r][t] = 1;
    }
}

/* What the eyedropper needs of a model's textures (mdl NULL: no parts, no ramps). */
static void wc_texinfo(const uint8_t *d, uint32_t size, const uint8_t *mdl, uint32_t mdlSize, WcTexInfo *ti) {
    static WpRec recs[WP_MAX_RECS * 2];
    int t, k;

    memset(ti, 0, sizeof(*ti));
    ti->count = wp_textures(d, size, ti->ent, WP_MAX_TEX);
    ti->texCount = mdl != NULL && mdl[0x4F] <= ti->count ? mdl[0x4F] : ti->count;
    ti->rampCount = mdl != NULL && ti->texCount + mdl[0x51] <= ti->count ? (mdl[0x51] < 16 ? mdl[0x51] : 16) : 0;
    for (t = 0; t < ti->texCount; t++) {
        int w, h, cnt[16];
        uint8_t *ix = wc_indices(d, size, &ti->ent[t], &w, &h);

        ti->hash[t] = wc_tex_hash(d, size, &ti->ent[t]);
        ti->most[t] = 0;
        if (ix != NULL) {
            memset(cnt, 0, sizeof(cnt));
            for (k = 0; k < w * h; k++) {
                cnt[ix[k]]++;
            }
            for (k = 1; k < 16; k++) {
                ti->most[t] = cnt[k] > cnt[ti->most[t]] ? k : ti->most[t];
            }
            free(ix);
        }
    }
    if (mdl != NULL) {
        ti->nrec = wp_records(mdl, mdlSize, recs, WP_MAX_RECS * 2);
        ti->recs = recs;
        for (ti->rec = 0; ti->rec < ti->nrec; ti->rec++) {
            ti->node = recs[ti->rec].node;
            wp_batches((uint8_t *)mdl, &recs[ti->rec], wc_texinfo_batch, ti);
        }
    }
}

/* Colour c turned as `from` turns into `to`: from a grey, every colour takes the new one's hue and strength (a white
   gi made red); else the coloured ones turn by its hue and scale by its strength, the greys stay grey; lightness
   scales by its lightness in both. */
static void wc_turn(const uint8_t *from, const uint8_t *to, uint8_t *c) {
    float hf, sf, vf, ht, st, vt, h, sc, v;

    warrior_hsv(from[0], from[1], from[2], &hf, &sf, &vf);
    warrior_hsv(to[0], to[1], to[2], &ht, &st, &vt);
    warrior_hsv(c[0], c[1], c[2], &h, &sc, &v);
    if (sf < 0.04f) {
        h = ht;
        sc = st;
    } else if (sc >= 0.04f) {
        h += ht - hf;
        sc *= st / sf;
    }
    if (v <= vf) { /* (darker than it: scaled; lighter: the room up to white shared out the same way) */
        v = vf > 0.02f ? v * vt / vf : vt;
    } else {
        v = vf < 0.98f ? vt + (v - vf) * (1.0f - vt) / (1.0f - vf) : vt;
    }
    warrior_rgb(h - floorf(h), sc > 1.0f ? 1.0f : sc, v > 1.0f ? 1.0f : v, c);
}

/* The eyedropper's colour lines of the warrior painted into texture file d (model mdl), in place. */
static void warrior_tint(uint8_t *d, uint32_t size, const uint8_t *mdl, uint32_t mdlSize, const int *key, const uint32_t *rgb, int count) {
    static WcTexInfo ti;
    int c, t, k, r, any = 0;

    for (c = 0; c < count; c++) {
        any |= (key[c] & WARRIOR_TINT) != 0;
    }
    if (!any) {
        return;
    }
    wc_texinfo(d, size, mdl, mdlSize, &ti);
    for (c = 0; c < count; c++) {
        uint32_t hash = ((uint32_t)key[c] >> 5) & 0xFFFFFF;
        int entry = key[c] & 31;
        uint8_t to[3] = {(uint8_t)(rgb[c] & 0xFF), (uint8_t)((rgb[c] >> 8) & 0xFF), (uint8_t)((rgb[c] >> 16) & 0xFF)};

        if (!(key[c] & WARRIOR_TINT) || entry > 16) {
            continue;
        }
        for (t = 0; t < ti.texCount; t++) {
            uint8_t *pal, from[3];
            int m;

            if (ti.hash[t] != hash || ti.ent[t].clutOfs == 0 || ti.ent[t].clutOfs * 4 + 0x60 + 64 > size) {
                continue;
            }
            pal = d + ti.ent[t].clutOfs * 4 + 0x60;
            m = entry == 16 ? ti.most[t] : entry;
            memcpy(from, pal + m * 4, 3);
            if (entry == 16) {
                int con = (int)(rgb[c] >> 24); /* (a contrast in percent, 0: as it is) */

                for (k = 0; k < 16; k++) {
                    wc_turn(from, to, pal + k * 4);
                }
                if (con > 0 && con != 100) { /* the whole texture's contrast: each colour from the colours' middle */
                    float mid[3] = {0.0f, 0.0f, 0.0f}, f = (float)con / 100.0f;
                    int j;

                    for (k = 0; k < 16; k++) {
                        for (j = 0; j < 3; j++) {
                            mid[j] += (float)pal[k * 4 + j] / 16.0f;
                        }
                    }
                    for (k = 0; k < 16; k++) {
                        for (j = 0; j < 3; j++) {
                            float x = mid[j] + ((float)pal[k * 4 + j] - mid[j]) * f;

                            pal[k * 4 + j] = (uint8_t)(x < 0.0f ? 0 : x > 255.0f ? 255 : (int)(x + 0.5f));
                        }
                    }
                }
            } else { /* the colour picked, with the shades of it next to it in the palette (a gradient's run: it turns
                        as one; a colour of its own, as the face's eyebrows, alone) */
                int lo = entry, hi = entry, gk;
                float h0, s0, v0, h1, s1, v1;

                gk = warrior_zone_of(pal + entry * 4, &h0, &s0, &v0);
                while (lo > 0 && warrior_zone_of(pal + (lo - 1) * 4, &h1, &s1, &v1) == gk && (gk != 0 || (warrior_hue_gap(h1, h0) <= 8.0f && fabsf(s1 - s0) <= 0.15f))) {
                    lo--;
                }
                while (hi < 15 && warrior_zone_of(pal + (hi + 1) * 4, &h1, &s1, &v1) == gk && (gk != 0 || (warrior_hue_gap(h1, h0) <= 8.0f && fabsf(s1 - s0) <= 0.15f))) {
                    hi++;
                }
                for (k = lo; k <= hi; k++) {
                    if (k != entry) {
                        wc_turn(from, to, pal + k * 4);
                    }
                }
                memcpy(pal + entry * 4, to, 3);
                if (ti.most[t] >= lo && ti.most[t] <= hi) {
                    m = ti.most[t]; /* (its most drawn colour turned with it: the shading follows) */
                }
            }
            if (m != ti.most[t]) {
                continue; /* (only its most drawn colour carries the shading) */
            }
            for (r = 0; r < ti.rampCount; r++) { /* its shading ramps: shadow = colour - ramp, turned, a ramp again */
                uint32_t at = ti.ent[ti.texCount + r].pixOfs * 4 + 0x60;

                if (!ti.link[r][t] || ti.ent[ti.texCount + r].pixOfs == 0 || at + 256 * 4 > size) {
                    continue;
                }
                for (k = 0; k < 256; k++) {
                    uint8_t *e = d + at + k * 4, base[3], shade[3];
                    int j;

                    if ((e[0] | e[1] | e[2]) == 0) {
                        continue;
                    }
                    memcpy(base, from, 3);
                    for (j = 0; j < 3; j++) {
                        int sh = (int)base[j] - (int)e[j];

                        shade[j] = (uint8_t)(sh < 0 ? 0 : sh);
                    }
                    memcpy(base, to, 3);
                    wc_turn(from, to, shade);
                    for (j = 0; j < 3; j++) {
                        int v = (int)base[j] - (int)shade[j];

                        e[j] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
                    }
                }
            }
        }
    }
}

/* The texture file (part 11) of model `mdl` (part 2, NULL: no ramps) painted as the warrior's colours say, in place. */
static void warrior_recolour_with(uint8_t *d, const WcInfo *info, const int *key, const uint32_t *rgb, const int *part, int count);

static void warrior_recolour(uint8_t *d, uint32_t size, const uint8_t *mdl, uint32_t mdlSize, const int *key, const uint32_t *rgb, const int *part,
                             int count) {
    static WcInfo in;

    wc_analyse(d, size, mdl, mdlSize, &in); /* (all of it before anything is painted) */
    warrior_recolour_with(d, &in, key, rgb, part, count);
    warrior_tint(d, size, mdl, mdlSize, key, rgb, count); /* (the eyedropper's) */
}

/* The painting itself, with the file's analysis made already (from the same palettes, unpainted). */
static void warrior_recolour_with(uint8_t *d, const WcInfo *info, const int *key, const uint32_t *rgb, const int *part, int count) {
    const WcInfo *ip = info;
#define in (*ip)
    int c, p, k, groups[WP_SPLIT_GROUPS], ngroups = wp_colour_groups(part, count, groups);

    for (c = 0; c < count; c++) {
        if ((key[c] & WARRIOR_KEY) && !(key[c] & WARRIOR_TINT)) {
            int g, z, r;

            for (g = 0; g < ngroups && (part == NULL || groups[g] != part[c]); g++) {
            }
            z = warrior_zone_find_in(in.zn, in.zones, key[c] & ~WARRIOR_KEY, part != NULL && part[c] != 0 ? 3 + g : -1);
            const WarriorZone *zn;

            if (z < 0) {
                continue;
            }
            zn = &in.zn[z];
            for (p = 0; p < in.pals; p++) {
                for (k = 0; k < 16; k++) {
                    if (in.label[p][k] == z) {
                        warrior_paint(d + in.at[p] + k * 4, zn, rgb[c]);
                    }
                }
            }
            for (r = 0; r < in.ramps; r++) { /* its ramps: shadow = colour - ramp, painted the same way, made a ramp again */
                if (in.rampZone[r] != z) {
                    continue;
                }
                for (k = 0; k < 256; k++) {
                    uint8_t *e = d + in.rampAt[r] + k * 4, base[3], shade[3];
                    int j;

                    if ((e[0] | e[1] | e[2]) == 0) {
                        continue;
                    }
                    base[0] = (uint8_t)(zn->r / zn->n);
                    base[1] = (uint8_t)(zn->g / zn->n);
                    base[2] = (uint8_t)(zn->b / zn->n);
                    for (j = 0; j < 3; j++) {
                        int sh = (int)base[j] - (int)e[j];

                        shade[j] = (uint8_t)(sh < 0 ? 0 : sh);
                    }
                    warrior_paint(base, zn, rgb[c]);
                    warrior_paint(shade, zn, rgb[c]);
                    for (j = 0; j < 3; j++) {
                        int v = (int)base[j] - (int)shade[j];

                        e[j] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
                    }
                }
            }
            continue;
        }
        { /* (an old key: the first version's hue bin, painted as one region) */
            WarriorZone old;
            float h, s, v;

            memset(&old, 0, sizeof(old));
            old.vmin = 1.0f;
            for (p = 0; p < in.pals; p++) {
                for (k = 0; k < 16; k++) {
                    const uint8_t *e = d + in.at[p] + k * 4;

                    if (warrior_zone(e[0], e[1], e[2]) == key[c]) {
                        warrior_hsv(e[0], e[1], e[2], &h, &s, &v);
                        old.hx += cosf(h * 6.2831853f);
                        old.hy += sinf(h * 6.2831853f);
                        old.s += s;
                        old.v += v;
                        old.vmin = v < old.vmin ? v : old.vmin;
                        old.vmax = v > old.vmax ? v : old.vmax;
                        old.n++;
                    }
                }
            }
            for (p = 0; old.n > 0 && p < in.pals; p++) {
                for (k = 0; k < 16; k++) {
                    uint8_t *e = d + in.at[p] + k * 4;

                    if (warrior_zone(e[0], e[1], e[2]) == key[c]) {
                        warrior_paint(e, &old, rgb[c]);
                    }
                }
            }
        }
    }
}

#undef in

/* Another texture file painted as `painted` is (the face's expressions, pak parts 12..16: their palettes are the face
   texture's colours, some a little different): each of its palette colours takes the new colour of the nearest
   colour of the original file `orig`, with its own small difference kept. */
static void warrior_recolour_follow(uint8_t *d, uint32_t size, const uint8_t *orig, const uint8_t *painted, uint32_t mainSize) {
    uint32_t at[64], mat[64];
    int pals = warrior_palettes(d, size, at, 64), mpals = warrior_palettes(orig, mainSize, mat, 64), p, k, q, j;

    for (p = 0; p < pals; p++) {
        for (k = 0; k < 16; k++) {
            uint8_t *e = d + at[p] + k * 4;
            int best = 10 * 10 * 3 + 1, bq = -1, bj = 0; /* (only a colour within 10 of one of the texture file's) */

            for (q = 0; q < mpals; q++) {
                for (j = 0; j < 16; j++) {
                    const uint8_t *o = orig + mat[q] + j * 4;
                    int dr = e[0] - o[0], dg = e[1] - o[1], db = e[2] - o[2], dd = dr * dr + dg * dg + db * db;

                    if (dd < best) {
                        best = dd;
                        bq = q;
                        bj = j;
                    }
                }
            }
            if (bq >= 0) {
                const uint8_t *o = orig + mat[bq] + bj * 4, *n = painted + mat[bq] + bj * 4;
                int c;

                for (c = 0; c < 3; c++) {
                    int v = n[c] + e[c] - o[c];

                    e[c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
                }
            }
        }
    }
}

/* The regions of a look for the colours screen: up to `max` records of {key, average colour 0xBBGGRR, palette
   colours | flags << 16, the parts drawing it}, the skin first, then the hair, then by how much is drawn, the face's
   own and those drawn nowhere last. Also writes each palette colour's region (index into the records, -1 none) to
   `which` (64 x 16) when not NULL. */
static int warrior_regions(const uint8_t *t, uint32_t ts, const uint8_t *m, uint32_t ms, int32_t *out, int max, int8_t *which, const int *groups,
                           int ngroups) {
    static WcInfo in;
    int z, count = 0, p, k, s;
    int score[WARRIOR_ZONES], index[WARRIOR_ZONES];

    wc_analyse(t, ts, m, ms, &in);
    for (z = 0; z < in.zones; z++) {
        int drawn = 0;

        for (s = 0; s < 13; s++) {
            drawn += in.tally[z][s];
        }
        score[z] = drawn + (in.zn[z].flags & WC_SKIN ? 10000000 : 0) + (in.zn[z].flags & WC_HAIR ? 5000000 : 0) + (in.zn[z].flags & WC_BROWS ? 3000000 : 0) - (in.zn[z].face ? 1000000 : 0) -
                   (in.zn[z].flags & WC_UNSEEN ? 2000000 : 0);
        index[z] = -1;
    }
    while (count < max) {
        int best = -1;

        for (z = 0; z < in.zones; z++) {
            if (index[z] < 0 && (best < 0 || score[z] > score[best])) {
                best = z;
            }
        }
        if (best < 0) {
            break;
        }
        out[count * 4] = WARRIOR_KEY | WARRIOR_KEY_KIND | warrior_zone_kind(&in.zn[best]) | (int)warrior_zone_rgb(&in.zn[best]);
        out[count * 4 + 1] = (int)warrior_zone_rgb(&in.zn[best]);
        out[count * 4 + 2] = in.zn[best].n | in.zn[best].flags << 16;
        out[count * 4 + 3] = in.zn[best].slots | /* (a part's own copy: the parts it is for, << 16) */
                             (in.zn[best].cls >= 3 && in.zn[best].cls - 3 < ngroups ? groups[in.zn[best].cls - 3] << 16 : 0);
        index[best] = count++;
    }
    for (p = 0; which != NULL && p < 64; p++) {
        for (k = 0; k < 16; k++) {
            which[p * 16 + k] = (int8_t)(p < in.pals ? index[in.label[p][k]] : -1);
        }
    }
    return count;
}
