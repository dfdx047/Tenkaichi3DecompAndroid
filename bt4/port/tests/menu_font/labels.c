/* The maker of the menu labels (port/src/gs/menu_labels.c) on its own: for comparing what it writes, and against
 * damaged menu archives, none of which may crash it or make it read or write outside a buffer.
 *
 * The game makes the list from the player's files. They are checked to be the USA disc's first (size and CRC-32),
 * but the code must not lean on that for its memory: here the check is left out and the archives are damaged, in
 * the file (pass a folder of damaged copies with --any) or after unpacking (--fuzz: in every round an unpacked
 * member gets bytes flipped, numbers replaced or its end cut off, half the time aimed at the tables and texture
 * headers the catalogue reads, which a damaged compressed stream never gets to). Built with the address and
 * undefined-behaviour sanitizers, which stop the program at the first such read or write.
 *
 *   gcc -g -O1 -DUI_TEXT_HOST_ALLOC -fsanitize=address,undefined -fno-sanitize-recover=all port/tests/menu_font/labels.c port/src/gs/menu_translation.c port/src/gs/ui_text_extract.c port/src/gs/ui_text_translate.c port/src/plat_settings.c -lm -lpthread -o labels_test
 *   labels_test <gamedata> <out folder>                        as the game does it (the disc is checked)
 *   labels_test --any <gamedata> <out folder>                  without the check
 *   labels_test --fuzz <rounds> <seed> <gamedata> <out folder> damaged after unpacking; prints how often the list
 *                                                              was given up and why, and ends with "fuzz OK"
 *
 * <gamedata> is only read. The first two forms print the result and the time taken, and write the list. */
#include "../../src/gs/menu_labels.c"
#include <time.h>

int MenuText_FirstFontId(int setting) { (void)setting; return 0; }

static uint64_t sSeed = 88172645463325252ull;
static uint32_t rnd(void) {
    sSeed ^= sSeed << 13;
    sSeed ^= sSeed >> 7;
    sSeed ^= sSeed << 17;
    return (uint32_t)(sSeed >> 16);
}

static int sCall, sTarget;
static struct { char why[240]; int count; } sWhy[64];

static uint32_t value(void) {
    static const uint32_t kValues[] = {0, 1, 2, 3, 4, 8, 0x10, 0x18, 0x20, 0x40, 0x7F, 0x80, 0xFF, 0x100, 0x200, 0x3FF, 0x400, 0xFFF, 0x1000, 0xFFFF,
                                       0x10000, 0xFFFFF, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFF0u, 0x3FFFFFFF, 0x40000000};
    return (rnd() & 1) != 0 ? kValues[rnd() % (sizeof(kValues) / sizeof(kValues[0]))] : (rnd() & 1) != 0 ? rnd() % 4096 : rnd() << 16 ^ rnd();
}

static uint32_t get(const uint8_t *data, size_t n, size_t at) {
    uint32_t v = 0;
    if (at <= n && n - at >= 4) {
        memcpy(&v, data + at, 4);
    }
    return v;
}

static void put(uint8_t *data, size_t n, size_t at, uint32_t v, int flip) {
    if (at <= n && n - at >= 4) {
        if (flip) {
            data[at + rnd() % 4] ^= (uint8_t)(1u << (rnd() & 7));
        } else {
            memcpy(data + at, &v, 4);
        }
    }
}

/* One unpacked member of a round is damaged (in one round of four: every one); the others stay whole, so the work
   gets as far as it can. Half the time the damage is aimed: at the member's table, or at a texture file the
   catalogue reads in it (its count and table, a texture's entry: the pointers, the upload's format and widths,
   TEX0 with the size and the pixel format; or the size registers before the pixels). */
static void damage(uint8_t *data, size_t *n) {
    static const int kParts[] = {2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 27, 28, 29, 35, 36};
    static const int kFields[] = {0, 4, 8, 12, 24, 24, 28, 48, 48, 48, 52, 52, 52, 56};
    uint32_t kind, times, i;

    if ((sCall++ != sTarget && sTarget >= 0) || *n < 16) {
        return;
    }
    kind = rnd() % 16;
    times = 1 + rnd() % 4;
    for (i = 0; i < times; i++) {
        /* mostly near the start, where the tables and the texture headers are */
        size_t span = (rnd() & 3) != 0 ? (*n < 8192 ? *n : 8192) : *n, at = rnd() % span;
        if (kind < 8) {
            int part = kParts[rnd() % (sizeof(kParts) / sizeof(kParts[0]))], flip = (rnd() & 3) == 0;
            size_t file = get(data, *n, (size_t)part * 4) & ~3u, entry;
            uint32_t count = get(data, *n, file), where = rnd() % 8;
            if (where == 0) {
                put(data, *n, (size_t)part * 4, value(), flip);
            } else if (where == 1) {
                put(data, *n, file + (rnd() & 4), value(), flip);
            } else {
                entry = file + (size_t)get(data, *n, file + 4) * 4 + (size_t)(rnd() % (count > 0 && count < 64 ? count : 8)) * 64;
                if (where < 6) {
                    put(data, *n, entry + kFields[rnd() % (sizeof(kFields) / sizeof(kFields[0]))], value(), flip || (rnd() & 1));
                } else if (where == 6) { /* the size registers of the upload */
                    put(data, *n, file + (size_t)get(data, *n, entry) * 4 + 96 - 48 + (rnd() & 4), value(), flip);
                } else { /* TEX0's size and format bits, one at a time */
                    size_t tex0 = entry + 48 + rnd() % 6;
                    if (tex0 < *n) {
                        data[tex0] ^= (uint8_t)(1u << (rnd() & 7));
                    }
                }
            }
        } else if (kind < 11) {
            data[at] ^= (uint8_t)(1u << (rnd() & 7));
        } else if (kind < 14) {
            put(data, *n, at & ~(size_t)3, value(), 0);
        } else if (kind == 14) {
            data[at] = (uint8_t)rnd();
        } else {
            *n = (rnd() & 1) != 0 ? rnd() % 4096 : rnd() % *n;
            return;
        }
    }
}

static void count_why(const char *why) {
    char text[240];
    const char *colon = strstr(why, ": the texture in");
    int i;
    snprintf(text, sizeof(text), "%s", colon != NULL ? colon + 2 : why); /* (without the name of the sheet) */
    for (i = 0; i < 63 && sWhy[i].count > 0 && strcmp(sWhy[i].why, text) != 0; i++) {
    }
    snprintf(sWhy[i].why, sizeof(sWhy[i].why), "%s", text);
    sWhy[i].count++;
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    char why[240];
    int r;

    if (argc == 6 && strcmp(argv[1], "--fuzz") == 0) {
        int rounds = atoi(argv[2]), made = 0, failed = 0, i;
        sSeed ^= (uint64_t)strtoull(argv[3], NULL, 0) * 0x9E3779B97F4A7C15ull;
        sMlDamage = damage;
        for (i = 0; i < rounds; i++) {
            sCall = 0;
            sTarget = (rnd() & 3) == 0 ? -1 : (int)(rnd() % 20);
            r = MenuLabels_Make(argv[4], argv[5], 1, why, sizeof(why));
            if (r == MENU_LABELS_MADE) {
                made++;
            } else {
                failed++;
                count_why(why);
            }
        }
        for (i = 0; i < 64; i++) {
            if (sWhy[i].count > 0) {
                printf("%6d  %s\n", sWhy[i].count, sWhy[i].why);
            }
        }
        printf("%d rounds: the list was made %d times, given up %d times\nfuzz OK\n", rounds, made, failed);
        return 0;
    }
    if (argc == 3 || (argc == 4 && strcmp(argv[1], "--any") == 0)) {
        double t0 = now();
        r = MenuLabels_Make(argv[argc - 2], argv[argc - 1], argc == 4, why, sizeof(why));
        printf("%s: %s (%.0f ms)\n", r == MENU_LABELS_MADE ? "made" : r == MENU_LABELS_OTHER_DISC ? "not this disc" : "failed", why, (now() - t0) * 1000.0);
        return r == MENU_LABELS_MADE ? 0 : 1;
    }
    fprintf(stderr, "usage: %s [--any] <gamedata> <out folder> | --fuzz <rounds> <seed> <gamedata> <out folder>\n", argv[0]);
    return 2;
}
