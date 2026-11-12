/* Damaged font files against port/src/gs/menu_text.c: none may crash it or make it read outside the file.
 *
 * The game reads a font the user chose at every start, so a file that is cut short or has flipped bits must be
 * turned down (the setting falls back to "Original"), and one that passes the checks must be safe to draw with.
 * This takes a good font, damages a copy in many ways, and each time does everything the game does with a font:
 * chooses it for all three settings, draws menu and battle labels, an added stage's and song's name, and the game's glyph sheets,
 * and, for the fonts that were accepted, every glyph the font has. Built with the address and undefined-behaviour
 * sanitizers, which stop the program at the first read or write outside a buffer.
 *
 *   gcc -g -O1 -DUI_TEXT_HOST_ALLOC -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=all -Iport/src/gs \
 *       port/tests/menu_font/fuzz.c port/src/gs/menu_labels.c port/src/gs/menu_translation.c \
 *       port/src/gs/ui_text_extract.c port/src/gs/ui_text_translate.c \
 *       port/src/plat_settings.c -lm -lpthread -o /tmp/menu_font_fuzz
 *   (menu_labels.c: menu_text.c names it for the list of labels; this test brings its own list, so it never runs
 *   here. It has its own test, labels.c in this folder.)
 *   (-fno-sanitize=shift: stb_truetype builds a 32-bit number as p[0] << 24 in a signed int, in every font.)
 *   /tmp/menu_font_fuzz <work folder> <rounds> <seed> port/third_party/fonts/dejavu/DejaVuSansCondensed.ttf
 *   /tmp/menu_font_fuzz <work folder> 0 0 <any files...>          (each file as it is: not fonts, empty, huge)
 *
 * It prints how many copies were accepted and why the others were turned down, and ends with "fuzz OK". */
#include "../../src/gs/menu_text.c"
#include <sys/stat.h>

static uint64_t sSeed = 88172645463325252ull;
static uint32_t rnd(void) {
    sSeed ^= sSeed << 13;
    sSeed ^= sSeed >> 7;
    sSeed ^= sSeed << 17;
    return (uint32_t)(sSeed >> 16);
}

static struct { const char *why; int count; } sWhy[64];
static int sAccepted, sDrawn;

static void count_why(const char *why) {
    int i;
    for (i = 0; i < 64 && sWhy[i].why != NULL && strcmp(sWhy[i].why, why) != 0; i++) {
    }
    if (i < 64) {
        sWhy[i].why = why;
        sWhy[i].count++;
    }
}

static char sDir[900], sFontPath[1024];
static uint8_t sTex[64 * 64 * 4];      /* a made-up menu texture: labels.tsv below names its hash */
static uint8_t sSheet[256 * 256 * 4];  /* a made-up glyph sheet of the game's */
static SharedGlyph sTable[400];
static int sTableCount;

static const char *const kTexts[] = {
    "Dragon History", "Press START button", "1P VS COM", "\xC3\x81rea \xC3\xA7\xC3\xB1\xC3\xBC \xC2\xBF?", "\xE2\x88\x9E 180 ???", "AVAWTo.,;'\"gjpqy",
    "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xCE\xA9\xCE\xBC\xCE\xAD\xCE\xB3\xCE\xB1", "HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH",
};
static const char *const kLabelIds[] = {
    "main-6/0/0", "main-6/0/1", "main-6/0/2", "main-6/0/3",
    "main-6/0/4", "main-6/0/5", "main-6/0/6", "main-6/0/7"
};

static void setup(void) {
    char path[1100];
    FILE *fp;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i;
    int n, plane;

    for (i = 0; i < sizeof(sTex); i++) { sTex[i] = (uint8_t)(i * 7 + (i >> 8)); }
    for (i = 0; i < sizeof(sTex); i++) { hash = (hash ^ sTex[i]) * UINT64_C(1099511628211); }
    snprintf(path, sizeof(path), "%s/fonts.tsv", sDir);
    fp = fopen(path, "wb");
    fprintf(fp, "f.ttf\tFuzzed\t7\n");
    fclose(fp);
    snprintf(path, sizeof(path), "%s/labels.tsv", sDir);
    fp = fopen(path, "wb");
    fprintf(fp, "# " MENU_LABELS_VERSION " made by port/tests/menu_font/fuzz.c\n");
    for (n = 0; n < 8; n++) { /* eight rows of 8 pixels: plain, centred, italic, measured width, outer outline, no shadow */
        fprintf(fp, "%016llx\t64\t64\t%d\t8\t%d\t%d\t%d\t%d\t%d\t0\t64\t%d\tf4ef89\td2684b\t000000\t0\t2c151b\t%d\t%d\t%d\td9c2c1\t%d\t2c151b\t0\tf4ef89\t2c151b\t0\t0\t%d\t%s\n",
                (unsigned long long)hash, n * 8, 4 + n % 4, 6 + n % 3, n & 1, n % 5, 500 + n * 200, n == 3 ? 40 : 0,
                n == 2 || n == 6, n == 4 ? 2000 : -1, n == 4 ? 2000 : 0, n != 5, n & 1, kLabelIds[n]);
    }
    fclose(fp);
    snprintf(path, sizeof(path), "%s/name-styles.tsv", sDir);
    fp = fopen(path, "wb");
    fprintf(fp, "0\t16\t22\t4\t1000\tffffff\tffffff\t460059\n1\t16\t22\t4\t1000\tf4ef89\td2684b\t2c151b\n2\t21\t44\t8\t1000\tf4ef89\td2684b\t2c151b\n");
    fclose(fp);
    setenv("BT3_FONTS", sDir, 1);
    setenv("BT3_MENU_FONT", sDir, 1);
    setenv("BT3_MENU_LANG", "en", 1);
    snprintf(sFontPath, sizeof(sFontPath), "%s/f.ttf", sDir);
    /* the glyph table: cells of 12 x 16 in a 256 x 256 sheet, the characters 0x20..0x17F over the four planes */
    for (n = 0; n < 352; n++) {
        int cell = n % 300, gx = (cell % 20) * 12 + 2, gy = (cell / 20) * 16 + 2;
        plane = n / 100;
        sTable[n].code = (uint16_t)(0x20 + n);
        sTable[n].u = (uint16_t)(gx | 10 << 10 | (plane & 1) << 15);
        sTable[n].v = (uint16_t)(gy | 14 << 10 | (plane >> 1) << 15);
    }
    sTableCount = 352;
    for (i = 0; i < sizeof(sSheet); i += 4) {
        sSheet[i] = sSheet[i + 1] = sSheet[i + 2] = 128;
        sSheet[i + 3] = (uint8_t)(((i / 4) % 256 % 12 > 2 && (i / 4 / 256) % 16 > 3) ? 128 : 0);
    }
}

/* Everything the game does with the font file at sFontPath. */
static void exercise(void) {
    Face face;
    const char *why;
    int w, h, scale, plane, g, n;
    uint8_t *px;
    float rect[8];

    why = face_open(&face, sFontPath);
    if (why != NULL) {
        count_why(why);
    } else {
        /* accepted: every glyph it has, drawn, measured and kerned */
        sAccepted++;
        for (g = 0; g < face.glyphs; g++) {
            int adv, lsb, x0, y0, x1, y1, ox, oy;
            unsigned char *bm;
            stbtt_GetGlyphHMetrics(&face.info, g, &adv, &lsb);
            stbtt_GetGlyphBox(&face.info, g, &x0, &y0, &x1, &y1);
            face_kern(&face, g, (int)(rnd() % (uint32_t)face.glyphs));
            bm = face_bitmap(&face, g, 32.0f / face.capUnits, 32.0f / face.capUnits, 0.25f, 0.5f, &w, &h, &ox, &oy);
            if (bm != NULL) { sDrawn++; }
            free(bm);
        }
        for (n = 0; n < 0x3000; n++) { face_glyph(&face, (unsigned)n); }
        face_glyph(&face, 0x1F600);
        face_glyph(&face, 0x10FFFF);
        face_close(&face);
    }
    /* the game's own way: the settings, then the textures */
    pthread_mutex_lock(&sLock);
    menu_text_fonts();
    if (sFontCount > 0) { sFonts[0].uses[FONT_MENU] = sFonts[0].uses[FONT_DIALOGUE] = sFonts[0].uses[FONT_BATTLE] = 1; }
    pthread_mutex_unlock(&sLock);
    MenuText_SetFont(0);
    MenuText_SetBattleFont(0);
    MenuText_SetSharedFont(0);
    MenuText_SetFont(1);
    pthread_mutex_lock(&sLock);
    if (sFontCount > 0) { sFonts[0].uses[FONT_MENU] = sFonts[0].uses[FONT_DIALOGUE] = sFonts[0].uses[FONT_BATTLE] = 1; }
    pthread_mutex_unlock(&sLock);
    MenuText_SetBattleFont(1);
    MenuText_SetSharedFont(1);
    if ((MenuText_GetFont() != 0) != (why == NULL) || (MenuText_GetBattleFont() != 0) != (why == NULL) ||
        (MenuText_GetSharedFont() != 0) != (why == NULL)) {
        fprintf(stderr, "fuzz: the settings and the check disagree about %s\n", sFontPath);
        abort();
    }
    for (scale = 2; scale <= 4; scale += 2) {
        MenuText_TexKey(64, 64, 1, 0x2A08, 0x2A00, scale);
        px = MenuText_Render(sTex, 64, 64, 1, scale, &w, &h);
        free(px);
        px = MenuText_Render(sTex, 64, 64, 1, scale, &w, &h); /* (again: from the kept rows) */
        free(px);
        for (n = 0; n < (int)(sizeof(kTexts) / sizeof(kTexts[0])); n++) {
            free(MenuText_RenderSong(kTexts[n], n & 1, scale, &w, &h));
            free(MenuText_RenderStage(kTexts[n], scale, &w, &h));
        }
        MenuText_RegisterGlyphs(0x2A08, 0x2A00, sTable, sTableCount, 16);
        for (plane = 0; plane < 4; plane++) {
            px = MenuText_RenderGlyphs(sSheet, 256, 256, 0x2A08, 0x2A00 + plane, scale, &w, &h);
            free(px);
            MenuText_MapGlyph(0x2A08, 0x2A00 + plane, 256, 256, 2.5f, 2.5f, 12.5f, 16.5f, rect);
        }
    }
}

static void put(const unsigned char *d, size_t n) {
    FILE *fp = fopen(sFontPath, "wb");
    if (fp == NULL || fwrite(d, 1, n, fp) != n) {
        fprintf(stderr, "fuzz: cannot write %s\n", sFontPath);
        exit(2);
    }
    fclose(fp);
}

static unsigned char *slurp(const char *path, size_t *size) {
    FILE *fp = fopen(path, "rb");
    unsigned char *d;
    long n;
    if (fp == NULL) { return NULL; }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    d = malloc((size_t)n + 1);
    *size = fread(d, 1, (size_t)n, fp);
    fclose(fp);
    return d;
}

/* A table of the good font to aim at: offset and length; the directory itself is "table" 0. */
static void pick_table(const unsigned char *d, size_t size, size_t *off, size_t *len) {
    static const char *const kTags[] = {"cmap", "head", "hhea", "maxp", "hmtx", "loca", "glyf", "kern", "glyf", "glyf", "cmap", "loca"};
    uint32_t n = be16(d + 4), i, pick = rnd() % 13;
    *off = 0;
    *len = 12 + 16 * (size_t)n;
    if (pick == 12) { return; }
    for (i = 0; i < n; i++) {
        if (memcmp(d + 12 + 16 * i, kTags[pick], 4) == 0) {
            *off = be32(d + 12 + 16 * i + 8);
            *len = be32(d + 12 + 16 * i + 12);
        }
    }
    if (*off + *len > size || *len == 0) { *off = 0; *len = size; }
}

int main(int argc, char **argv) {
    static const unsigned kEdge[] = {0, 1, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, 2, 3, 4};
    unsigned char *good, *copy;
    size_t size, n, off, len;
    int rounds, r, i, f;

    if (argc < 5) {
        fprintf(stderr, "usage: %s <work folder> <rounds> <seed> <font or file>...\n", argv[0]);
        return 2;
    }
    snprintf(sDir, sizeof(sDir), "%s", argv[1]);
    mkdir(sDir, 0700);
    rounds = atoi(argv[2]);
    sSeed ^= (uint64_t)strtoull(argv[3], NULL, 0) * 0x9E3779B97F4A7C15ull;
    setup();
    for (f = 4; f < argc; f++) {
        good = slurp(argv[f], &size);
        if (good == NULL) {
            fprintf(stderr, "fuzz: cannot read %s\n", argv[f]);
            return 2;
        }
        put(good, size); /* the file as it is */
        exercise();
        if (rounds > 0 && (size < 12 || MenuText_GetFont() == 0)) {
            fprintf(stderr, "fuzz: %s itself is not accepted: nothing to damage\n", argv[f]);
            return 2;
        }
        copy = malloc(size + 20 * 1024 * 1024);
        for (r = 0; r < rounds; r++) {
            int kind = (int)(rnd() % 16);
            memcpy(copy, good, size);
            n = size;
            pick_table(good, size, &off, &len);
            switch (kind) {
            case 0: /* cut short, anywhere */
                n = rnd() % size;
                break;
            case 1: /* cut short inside a table */
                n = off + rnd() % len;
                break;
            case 2: /* a few flipped bits, anywhere */
                for (i = 1 + (int)(rnd() % 8); i > 0; i--) { copy[rnd() % size] ^= (unsigned char)(1u << (rnd() % 8)); }
                break;
            case 3: case 4: case 5: /* a few flipped bits in one table */
                for (i = 1 + (int)(rnd() % 6); i > 0; i--) { copy[off + rnd() % len] ^= (unsigned char)(1u << (rnd() % 8)); }
                break;
            case 6: case 7: case 8: /* 16-bit fields of a table set to the values that break arithmetic */
                for (i = 1 + (int)(rnd() % 3); i > 0 && len >= 2; i--) {
                    size_t at = off + (rnd() % (len / 2)) * 2;
                    unsigned v = kEdge[rnd() % (sizeof(kEdge) / sizeof(kEdge[0]))];
                    copy[at] = (unsigned char)(v >> 8);
                    copy[at + 1] = (unsigned char)v;
                }
                break;
            case 9: /* a stretch of a table random */
                for (i = (int)(rnd() % 64), n = off + rnd() % len; i > 0 && n < size; i--) { copy[n++] = (unsigned char)rnd(); }
                n = size;
                break;
            case 10: /* a stretch zeroed */
                i = (int)(rnd() % 4096);
                n = off + rnd() % len;
                memset(copy + n, 0, n + (size_t)i > size ? size - n : (size_t)i);
                n = size;
                break;
            case 11: /* the head of a table: where the counts and formats are */
                for (i = 1 + (int)(rnd() % 4); i > 0; i--) { copy[off + rnd() % (len < 64 ? len : 64)] = (unsigned char)rnd(); }
                break;
            case 12: /* two directory entries exchange their places in the file */
                {
                    uint32_t a = rnd() % be16(good + 4), b = rnd() % be16(good + 4);
                    memcpy(copy + 12 + 16 * a + 8, good + 12 + 16 * b + 8, 8);
                    memcpy(copy + 12 + 16 * b + 8, good + 12 + 16 * a + 8, 8);
                }
                break;
            case 13: /* a table's offset or length changed */
                {
                    uint32_t a = rnd() % be16(good + 4);
                    copy[12 + 16 * a + 8 + rnd() % 8] = (unsigned char)rnd();
                }
                break;
            case 14: /* nothing but noise, of any size up to the font's */
                n = rnd() % (size < 65536 ? size : 65536);
                for (i = 0; i < (int)n; i++) { copy[i] = (unsigned char)rnd(); }
                if (rnd() & 1) { memcpy(copy, good, n < 12 ? n : 12); }
                break;
            default: /* one glyph's outline damaged: its counts, flags and coordinates */
                {
                    uint32_t t = be16(good + 4), k, glyf = 0, glyfLen = 0;
                    for (k = 0; k < t; k++) {
                        if (memcmp(good + 12 + 16 * k, "glyf", 4) == 0) { glyf = be32(good + 12 + 16 * k + 8); glyfLen = be32(good + 12 + 16 * k + 12); }
                    }
                    if (glyfLen > 64) {
                        size_t at = glyf + rnd() % (glyfLen - 32);
                        for (i = 1 + (int)(rnd() % 12); i > 0; i--) { copy[at + rnd() % 32] = (unsigned char)(rnd() & 1 ? rnd() : kEdge[rnd() % 8]); }
                    }
                }
                break;
            }
            put(copy, n);
            exercise();
        }
        if (rounds > 0) { /* and the sizes at the edges: empty, tiny, larger than the game reads */
            put(good, 0);
            exercise();
            put(good, 11);
            exercise();
            put(good, 12);
            exercise();
            memset(copy + size, 0, 20 * 1024 * 1024);
            put(copy, size + 20 * 1024 * 1024);
            exercise();
        }
        free(copy);
        free(good);
    }
    printf("%d files tried: %d accepted (%d glyphs drawn), the others turned down:\n", (rounds > 0 ? rounds + 5 : 1) * (argc - 4), sAccepted, sDrawn);
    for (i = 0; i < 64 && sWhy[i].why != NULL; i++) { printf("  %6d  %s\n", sWhy[i].count, sWhy[i].why); }
    remove(sFontPath);
    printf("fuzz OK\n");
    return 0;
}
