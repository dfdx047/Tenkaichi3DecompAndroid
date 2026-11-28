/* BT4's large portraits (256 x 512, files 0x71C + fighter of the player's own disc) for the character select's cards,
   read and decoded here, outside the game's memory: nothing of the game's state changes (online play replays its
   frames). Cut round the face to 136 x 176; at most FACE_PER_FRAME made in a frame, the FACE_KEPT last used kept.
   Until one is made (or when there is none) the picture is clear: the card shows its colour. */
#define FACE_COUNT 250
#define FACE_W 136
#define FACE_H 176
#define FACE_RAW 132448
#define FACE_KEPT 96
#define FACE_PER_FRAME 3

static const uint16_t kFaceAt[FACE_COUNT][2] = { /* the point between the eyes in each portrait, marked by eye */
    {122,152}, {122,149}, {134,157}, {125,146}, {129,175}, {95,161}, {126,152}, {127,153},
    {138,159}, {125,158}, {134,161}, {129,146}, {126,163}, {121,177}, {133,157}, {121,167},
    {130,165}, {125,143}, {145,159}, {131,169}, {118,131}, {119,163}, {114,168}, {123,159},
    {121,156}, {121,173}, {135,157}, {126,167}, {122,185}, {153,153}, {123,169}, {113,168},
    {125,168}, {142,157}, {123,162}, {127,165}, {121,165}, {118,163}, {119,153}, {129,169},
    {90,173}, {127,162}, {123,176}, {150,173}, {123,137}, {127,165}, {131,159}, {126,168},
    {146,165}, {133,153}, {91,155}, {127,153}, {123,155}, {121,153}, {126,139}, {121,155},
    {125,153}, {87,143}, {135,165}, {123,153}, {130,160}, {123,147}, {123,148}, {135,144},
    {123,148}, {134,153}, {127,159}, {127,143}, {138,154}, {131,173}, {127,165}, {127,157},
    {121,153}, {141,148}, {115,153}, {126,147}, {118,161}, {129,163}, {123,157}, {131,159},
    {123,145}, {123,123}, {127,159}, {129,155}, {127,151}, {129,159}, {126,169}, {129,169},
    {123,174}, {142,157}, {117,164}, {137,169}, {117,152}, {157,177}, {122,169}, {123,159},
    {130,167}, {127,169}, {137,151}, {139,166}, {131,121}, {121,159}, {134,158}, {123,164},
    {127,177}, {123,157}, {127,163}, {129,150}, {123,167}, {130,181}, {126,133}, {142,151},
    {125,147}, {126,153}, {115,159}, {115,163}, {118,163}, {119,168}, {137,168}, {125,165},
    {102,113}, {138,167}, {131,153}, {127,161}, {123,157}, {137,165}, {127,155}, {123,163},
    {123,164}, {134,163}, {119,169}, {139,165}, {129,180}, {129,158}, {130,158}, {134,148},
    {126,165}, {131,139}, {142,151}, {142,170}, {131,175}, {123,179}, {131,157}, {113,172},
    {131,156}, {123,168}, {123,168}, {117,155}, {110,177}, {127,163}, {121,143}, {123,151},
    {126,139}, {123,139}, {127,169}, {123,168}, {123,165}, {127,159}, {142,142}, {141,163},
    {131,133}, {127,144}, {127,140}, {123,140}, {127,100}, {131,159}, {123,151}, {131,159},
    {131,159}, {123,152}, {131,153}, {123,159}, {126,155}, {118,155}, {127,160}, {123,179},
    {133,176}, {134,169}, {127,157}, {127,163}, {113,169}, {123,175}, {121,177}, {134,167},
    {129,170}, {122,157}, {145,163}, {141,156}, {123,161}, {123,165}, {123,171}, {129,167},
    {137,157}, {134,165}, {137,167}, {185,196}, {123,173}, {129,160}, {123,169}, {155,164},
    {130,177}, {142,157}, {131,159}, {123,155}, {123,148}, {134,165}, {131,165}, {119,173},
    {134,163}, {135,169}, {129,151}, {134,163}, {123,156}, {125,207}, {126,151}, {123,168},
    {131,149}, {129,157}, {145,163}, {134,163}, {111,157}, {139,160}, {131,163}, {133,165},
    {142,158}, {131,159}, {142,163}, {133,179}, {134,167}, {163,179}, {127,167}, {127,120},
    {127,120}, {127,120}, {127,120}, {127,120}, {127,120}, {127,120}, {127,120}, {127,120},
    {123,145}, {138,150}, {123,150}, {123,150}, {123,150}, {123,150}, {127,150}, {123,150},
    {123,150}, {123,150},
};

static struct { GsTex tex; uint32_t used; } sFace[FACE_COUNT];
static unsigned char sFaceMissing[FACE_COUNT];
static GsTex sFaceClear;

/* Philip Gage's byte pair expansion (src/sys/bpe.c), bounded. Returns what was written, 0 for a bad stream. */
static size_t face_expand(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    uint8_t left[256], right[256], stack[512];
    size_t p = 8, end, w = 0;

    if (n < 8) {
        return 0;
    }
    end = 8 + (size_t)((uint32_t)in[4] | (uint32_t)in[5] << 8 | (uint32_t)in[6] << 16 | (uint32_t)in[7] << 24);
    if (end > n || end < 8) {
        return 0;
    }
    while (p < end) {
        int count = in[p++], c = 0, i, size, top = 0;

        for (i = 0; i < 256; i++) {
            left[i] = (uint8_t)i;
        }
        for (;;) {
            if (count > 127) {
                c += count - 127;
                count = 0;
            }
            if (c >= 256) {
                break;
            }
            for (i = 0; i <= count && c < 256; i++, c++) {
                if (p + 2 > end) {
                    return 0;
                }
                left[c] = in[p++];
                if (c != left[c]) {
                    right[c] = in[p++];
                }
            }
            if (c >= 256) {
                break;
            }
            if (p >= end) {
                return 0;
            }
            count = in[p++];
        }
        if (p + 2 > end) {
            return 0;
        }
        size = in[p] << 8 | in[p + 1];
        p += 2;
        for (;;) {
            if (top != 0) {
                c = stack[--top];
            } else {
                if (size-- == 0) {
                    break;
                }
                if (p >= end) {
                    return 0;
                }
                c = in[p++];
            }
            if (c == left[c]) {
                if (w >= cap) {
                    return 0;
                }
                out[w++] = (uint8_t)c;
            } else {
                if (top > 510) {
                    return 0;
                }
                stack[top++] = right[c];
                stack[top++] = left[c];
            }
        }
    }
    return w;
}

/* The portrait's face as RGBA (the GS's alpha), or NULL. Its 8-bit picture is swizzled at 0xC0, its colours at
   0x20140 (CSM1 order). */
static uint32_t *face_pixels(int chara) {
    extern int Port_FilePath(int ptid, int flid, const char *fname, char *out, int size);
    extern FILE *Port_OpenSpan(const char *path, long *size);
    char path[512];
    uint8_t *in = NULL, *raw = NULL;
    uint32_t *px = NULL, pal[256];
    long size = 0;
    FILE *fp;
    int i, x, y, x0, y0;

    if (!Port_FilePath(1, 0x71C + chara - 1, NULL, path, sizeof(path)) || (fp = Port_OpenSpan(path, &size)) == NULL) {
        return NULL;
    }
    if (size > 0 && size < 0x40000 && (in = malloc((size_t)size)) != NULL && fread(in, 1, (size_t)size, fp) == (size_t)size &&
        (raw = malloc(FACE_RAW)) != NULL && face_expand(in, (size_t)size, raw, FACE_RAW) == FACE_RAW &&
        (px = malloc(FACE_W * FACE_H * 4)) != NULL) {
        for (i = 0; i < 256; i++) {
            int j = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
            const uint8_t *c = raw + 0x20140 + j * 4;

            pal[i] = (uint32_t)c[0] | (uint32_t)c[1] << 8 | (uint32_t)c[2] << 16 | (uint32_t)c[3] << 24;
        }
        x0 = kFaceAt[chara][0] - FACE_W / 2; /* the eyes in the middle, a little under half way down */
        y0 = kFaceAt[chara][1] - FACE_H * 55 / 100;
        for (y = 0; y < FACE_H; y++) {
            for (x = 0; x < FACE_W; x++) {
                int u = x0 + x, v = y0 + y;
                int block = (v & ~0xF) * 256 + (u & ~0xF) * 2;
                int swap = (((v + 2) >> 2) & 1) * 4;
                int row = (((v & ~3) >> 1) + (v & 1)) & 7;
                int at = u < 0 || u >= 256 || v < 0 || v >= 512 ? 0 : block + row * 256 * 2 + ((u + swap) & 7) * 4 + ((v >> 1) & 1) + ((u >> 2) & 2);

                px[y * FACE_W + x] = u < 0 || u >= 256 || v < 0 || v >= 512 ? 0 : pal[raw[0xC0 + at]]; /* past the portrait: clear */
            }
        }
    }
    fclose(fp);
    free(in);
    free(raw);
    return px;
}

static GsTex face_clear(void) {
    if (sFaceClear == 0) {
        uint32_t *blank = calloc(1, 4);

        if (blank != NULL) {
            sFaceClear = tbo_upload(blank, 1, 1);
        }
    }
    return sFaceClear != 0 ? sFaceClear : gsWhite;
}

static GsTex face_texture(int chara) {
    static uint32_t frame;
    static int made;
    uint32_t *px;
    int i, kept = 0, old = -1;

    if (sFace[chara].tex != 0) {
        sFace[chara].used = gGsFrame;
        return sFace[chara].tex;
    }
    if (frame != gGsFrame) {
        frame = gGsFrame;
        made = 0;
    }
    if (sFaceMissing[chara] || made >= FACE_PER_FRAME || sBackend->texSlotsLeft() <= 12) {
        return face_clear();
    }
    for (i = 0; i < FACE_COUNT; i++) {
        if (sFace[i].tex != 0) {
            kept++;
            if (old < 0 || sFace[i].used < sFace[old].used) {
                old = i;
            }
        }
    }
    if (kept >= FACE_KEPT && old >= 0 && gGsFrame - sFace[old].used > 3) {
        sBackend->texDestroy(sFace[old].tex);
        sFace[old].tex = 0;
    }
    made++;
    px = face_pixels(chara);
    if (px == NULL) {
        sFaceMissing[chara] = 1;
        return face_clear();
    }
    sFace[chara].tex = tbo_upload(px, FACE_W, FACE_H);
    sFace[chara].used = gGsFrame;
    return sFace[chara].tex != 0 ? sFace[chara].tex : face_clear();
}

