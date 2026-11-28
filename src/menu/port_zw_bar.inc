/* Z Warriors' fighter picker (approved 2026-11-27, mods-zwarriors/previa_centro.py): the warrior in 3D in the middle,
   a low bar with one row of the game's old character grid (seven across, the disc's order; up and down change row),
   the forms only when asked for, each with the game's own big portrait. Included by port_zw.c; positions are the
   templates' pixels (Zt_*). The technique picker stays as it was. */
static char *Zo_L(char *pt, char *en, char *es);
static void Zo_StagesOpen(void);
static void Zo_StagesClose(void);
extern const char *Port_TagText(const char *english);

#define ZB_COLS 7
#define ZB_PICS 28
#define ZB_FACE_FILE 0x2F9 /* + fighter id: its compressed portrait, 256 x 512 (menu_e.h TS_FACE_FILE) */

typedef struct ZbCell {
    s32 base, n, form[7];
} ZbCell;
static ZbCell sZbCell[128];
static s32 sZbCells, sZbOpen, sZbTake, sZbLoads;

typedef struct ZbPic {
    s32 chara;
    u32 use, colour;
    void *data;
    MTexRes *res;
} ZbPic;
static ZbPic sZbPic[ZB_PICS];
static u32 sZbClock;

/* The grid as the game's select has it: the disc's table (Duel archive, section 2, sub 42), the two marker cells
   (random, custom) left out. */
static void Zb_Grid(void) {
    extern void *sZoSelArtPtr(void);
    const s32 *t;
    s32 i, k, n;

    if (sZbCells > 0) {
        return;
    }
    Zo_StagesOpen();
    if (sZoSelArtPtr() == NULL) {
        return;
    }
    t = (const s32 *)MPACK_AT(sZoSelArtPtr(), 42);
    n = t[0];
    for (i = 0; i < n && sZbCells < 128; i++) {
        const s32 *c = t + 4 + i * 9;
        ZbCell *z = &sZbCell[sZbCells];

        z->n = 0;
        for (k = 0; k < c[1] && k < 7; k++) {
            if (c[2 + k] >= 0 && c[2 + k] < MM_CHARA_COUNT) {
                z->form[z->n++] = c[2 + k];
            }
        }
        z->base = z->n > 0 ? z->form[0] : c[0];
        if (z->base >= 0 && z->base < MM_CHARA_COUNT) {
            sZbCells++;
        }
    }
    Zo_StagesClose();
}

/* ---- the portraits: a few kept, at most two read in a frame ---- */
static void Zb_PicFree(ZbPic *p) {
    if (p->data != NULL) {
        sceGsSyncPath(0, 0);
        Heap_Free(p->data);
    }
    p->data = NULL;
    p->res = NULL;
    p->chara = -1;
}

static void Zb_Free(void) {
    s32 i;

    for (i = 0; i < ZB_PICS; i++) {
        Zb_PicFree(&sZbPic[i]);
        sZbPic[i].use = 0;
    }
    sZbOpen = -1;
    sZbTake = 0;
}

/* Where each portrait's face is (x, y in its 256 x 512 picture), marked by eye on the 161 pictures. */
static const u8 kZbFace[161][2] = {
    {144, 167}, {128, 140}, {115, 140}, {105, 140}, {116, 173}, {120, 153}, {146, 152}, {128, 167},
    {127, 160}, {157, 160}, {121, 173}, {123, 176}, {129, 185}, {132, 173}, {124, 167}, {117, 180},
    {123, 167}, {131, 160}, {101, 167}, {128, 160}, {125, 153}, {107, 153}, {128, 153}, {112, 153},
    {127, 153}, {128, 153}, {121, 167}, {128, 160}, {129, 173}, {123, 160}, {117, 180}, {121, 160},
    {127, 157}, {131, 160}, {125, 157}, {119, 167}, {128, 153}, {113, 160}, {121, 167}, {121, 160},
    {117, 160}, {131, 167}, {108, 167}, {128, 173}, {129, 160}, {120, 153}, {131, 173}, {121, 173},
    {96, 150}, {117, 149}, {115, 167}, {123, 160}, {129, 167}, {113, 160}, {124, 167}, {121, 167},
    {127, 160}, {121, 153}, {125, 167}, {128, 153}, {109, 167}, {120, 160}, {117, 147}, {121, 140},
    {127, 167}, {131, 167}, {131, 153}, {123, 140}, {129, 147}, {127, 160}, {131, 153}, {121, 167},
    {129, 187}, {115, 167}, {128, 180}, {112, 171}, {129, 167}, {131, 167}, {131, 180}, {128, 147},
    {127, 167}, {128, 167}, {125, 160}, {123, 160}, {129, 153}, {127, 140}, {131, 147}, {117, 147},
    {120, 153}, {131, 147}, {125, 147}, {123, 147}, {103, 153}, {113, 160}, {128, 153}, {128, 162},
    {120, 147}, {125, 173}, {142, 155}, {123, 153}, {129, 167}, {120, 147}, {128, 157}, {121, 180},
    {113, 167}, {124, 160}, {125, 160}, {125, 167}, {125, 167}, {127, 171}, {124, 153}, {101, 147},
    {100, 153}, {115, 157}, {125, 153}, {112, 153}, {123, 160}, {127, 147}, {128, 147}, {128, 153},
    {100, 133}, {131, 153}, {128, 153}, {139, 147}, {123, 140}, {120, 147}, {128, 147}, {115, 133},
    {127, 147}, {128, 167}, {125, 167}, {128, 160}, {129, 160}, {123, 160}, {124, 173}, {117, 153},
    {127, 167}, {131, 153}, {121, 147}, {139, 167}, {109, 167}, {127, 167}, {120, 160}, {117, 173},
    {127, 157}, {111, 153}, {125, 160}, {144, 163}, {125, 157}, {120, 167}, {128, 157}, {128, 167},
    {113, 160}, {133, 200}, {125, 167}, {123, 140}, {124, 160}, {121, 165}, {135, 160}, {121, 144},
    {140, 155},
};

#define ZB_SW(i) (((i) & 0xE7) | (((i) & 8) << 1) | (((i) & 0x10) >> 1))

/* A colour for its card: the vivid ones of its palette. */
static void Zb_Look(ZbPic *p) {
    const u8 *e = (const u8 *)p->res->tex;
    const u8 *clut = *(u8 **)(e + 0x3C) + 0x60;
    s32 i;
    u32 sum[3] = {0, 0, 0}, n = 0;

    for (i = 0; i < 256; i++) {
        const u8 *c = clut + i * 4;
        s32 hi = c[0] > c[1] ? c[0] > c[2] ? c[0] : c[2] : c[1] > c[2] ? c[1] : c[2];
        s32 lo = c[0] < c[1] ? c[0] < c[2] ? c[0] : c[2] : c[1] < c[2] ? c[1] : c[2];

        if (c[3] > 0x40 && hi > 90 && hi - lo > hi / 2) {
            sum[0] += c[0], sum[1] += c[1], sum[2] += c[2], n++;
        }
    }
    p->colour = n ? (sum[0] / n) | (sum[1] / n) << 8 | (sum[2] / n) << 16 : 0x00A06040u;
}

static ZbPic *Zb_Pic(s32 chara) {
    s32 i, old = 0;
    ZbPic *p;
    void *file;

    for (i = 0; i < ZB_PICS; i++) {
        if (sZbPic[i].data != NULL && sZbPic[i].chara == chara) {
            sZbPic[i].use = ++sZbClock;
            return &sZbPic[i];
        }
        if (sZbPic[i].use < sZbPic[old].use) {
            old = i;
        }
    }
    if (chara < 0 || chara >= MM_CHARA_COUNT || sZbLoads >= 2) {
        return NULL;
    }
    sZbLoads++;
    p = &sZbPic[old];
    Zb_PicFree(p);
    file = File_LoadSync(ZB_FACE_FILE + chara, NULL, 0);
    if (file == NULL) {
        return NULL;
    }
    p->data = Sprite_Unpack(file, NULL, NULL);
    Heap_Free(file);
    if (p->data == NULL) {
        return NULL;
    }
    p->res = (MTexRes *)p->data;
    Res_RelocateOffsets((void *)&p->res, p->res, p->res);
    p->chara = chara;
    p->use = ++sZbClock;
    Zb_Look(p);
    return p;
}

/* ---- the bar's content: the game's fighters, or the mods' ---- */
static s32 Zb_Count(void) {
    return sZtPkMods ? sZw->pkMods : sZbCells;
}

static s32 Zb_Base(s32 at) { /* the picker's number of the cell's own */
    return sZtPkMods ? MM_CHARA_COUNT + at : sZbCell[at].base;
}

static s32 Zb_Forms(s32 at) {
    return sZtPkMods || sZbCell[at].n < 2 ? 0 : sZbCell[at].n;
}

static s32 Zb_At(void) { /* the cell the picker's cursor is in (a form: its character's) */
    s32 i, k, cur = sZw->pickCur;

    if (sZtPkMods) {
        return cur >= MM_CHARA_COUNT && cur - MM_CHARA_COUNT < sZw->pkMods ? cur - MM_CHARA_COUNT : 0;
    }
    for (i = 0; i < sZbCells; i++) {
        if (sZbCell[i].base == cur) {
            return i;
        }
    }
    for (i = 0; i < sZbCells; i++) {
        for (k = 0; k < sZbCell[i].n; k++) {
            if (sZbCell[i].form[k] == cur) {
                return i;
            }
        }
    }
    return 0;
}

static void Zb_Go(s32 at) { /* another cell: its own fighter, no forms */
    if (at < 0 || at >= Zb_Count()) {
        return;
    }
    sZbOpen = -1;
    if (sZw->pickCur != Zb_Base(at)) {
        sZw->pickCur = Zb_Base(at);
        sZw->pickRest = 0;
        Snd_PlaySe(1, 0);
    }
}

static void Zb_Move(s32 dcol, s32 drow) {
    s32 n = Zb_Count(), at = Zb_At(), rows = (n + ZB_COLS - 1) / ZB_COLS, row, col, len;

    if (n <= 0) {
        return;
    }
    row = at / ZB_COLS;
    col = at % ZB_COLS;
    row = (row + drow + rows) % rows;
    len = row == rows - 1 ? n - row * ZB_COLS : ZB_COLS;
    col = dcol ? (col + dcol + len) % len : col < len ? col : len - 1;
    Zb_Go(row * ZB_COLS + col);
}

static void Zb_Form(s32 k) {
    s32 at = Zb_At();

    if (k >= 0 && k < Zb_Forms(at) && sZw->pickCur != sZbCell[at].form[k]) {
        sZw->pickCur = sZbCell[at].form[k];
        sZw->pickRest = 0;
        Snd_PlaySe(1, 0);
    }
}

/* The pad (Pk_Update): the arrows, then 1 if Cross / Triangle were for the forms and not for the screen. */
static void Zb_Nav(Pad *pad) {
    s32 at, k;

    Zb_Grid();
    if (sZbOpen >= 0 && sZbOpen == Zb_At()) { /* (in the forms: left / right and up / down go through them) */
        at = sZbOpen;
        for (k = 0; k < Zb_Forms(at) && sZbCell[at].form[k] != sZw->pickCur; k++) {
        }
        if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
            Zb_Form((k + Zb_Forms(at) - 1) % Zb_Forms(at));
        } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
            Zb_Form((k + 1) % Zb_Forms(at));
        } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
            Zb_Form(k - 3);
        } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
            Zb_Form(k + 3);
        }
        return;
    }
    if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        Zb_Move(-1, 0);
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        Zb_Move(1, 0);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        Zb_Move(0, -1);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        Zb_Move(0, 1);
    }
}

static s32 Zb_Keys(Pad *pad) {
    if (sZbOpen >= 0 && (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE))) {
        sZbOpen = -1;
        Snd_PlaySe(1, 2);
        return 1;
    }
    if (pad->gamePressed & PADG_CROSS) {
        if (!sZbTake && sZbOpen < 0 && sZw->pickCur >= 0 && Zb_Forms(Zb_At()) > 0) { /* its forms first */
            sZbOpen = Zb_At();
            Snd_PlaySe(1, 1);
            return 1;
        }
        sZbTake = 0;
        sZbOpen = -1;
    }
    return 0;
}

/* ---- drawing ---- */
static char *Zb_Name(s32 cur) {
    static char names[8][64];
    static s32 next;
    char *out;

    if (cur < 0) {
        return Zo_L("Nenhum", "None", "Ninguno");
    }
    if (cur >= MM_CHARA_COUNT) {
        return Zw_Name(Pk_Chara(cur));
    }
    out = names[next++ & 7];
    Ps_Copy(out, Port_TagText(Zw_Name(cur)), 64);
    return out;
}

static void Zb_Card(s32 cur, f32 cx, f32 cy, f32 w, f32 h, s32 lit, s32 forms, s32 label) {
    f32 x0 = cx - w * 0.5f, y0 = cy - h * 0.5f, x1 = cx + w * 0.5f, y1 = cy + h * 0.5f;
    ZbPic *p = cur >= 0 && cur < MM_CHARA_COUNT ? Zb_Pic(cur) : NULL;
    u32 c = p != NULL ? p->colour : 0x00905038u;
    s32 r = (s32)(c & 255), g = (s32)((c >> 8) & 255), b = (s32)((c >> 16) & 255), dim = lit ? 0x80 : 0x62;
    char buf[8];

    if (lit) {
        f32 m = 7.0f + 2.0f * Zw_Beat(40);

        Mx_Rect(ZT_X(x0 - m), ZT_Y(y0 - m), ZT_X(x1 + m), ZT_Y(y1 + m), Mx_Rgba(255, 206, 70, 0x80), Mx_Rgba(230, 140, 20, 0x80));
    } else {
        Mx_Rect(ZT_X(x0 - 3.0f), ZT_Y(y0 - 3.0f), ZT_X(x1 + 3.0f), ZT_Y(y1 + 3.0f), Mx_Rgba(130, 150, 220, 0x70), Mx_Rgba(70, 90, 160, 0x70));
    }
    Mx_Rect(ZT_X(x0), ZT_Y(y0), ZT_X(x1), ZT_Y(y1), Mx_Rgba(r * dim >> 7, g * dim >> 7, b * dim >> 7, 0x80), Mx_Rgba(r / 6, g / 6, b / 6 + 30, 0x80));
    if (p != NULL) { /* around the face: a little above the card's middle, clear of the name */
        s32 sw = 196, sh = (s32)(196.0f * h / w), u0, v0;

        u0 = (s32)kZbFace[cur][0] - sw / 2;
        v0 = (s32)kZbFace[cur][1] - sh * 42 / 100;
        u0 = u0 < 0 ? 0 : u0 > 256 - sw ? 256 - sw : u0;
        v0 = v0 < 0 ? 0 : v0 > 400 - sh ? 400 - sh : v0;
        Mx_Tex((MmTex *)p->res->tex, ZT_X(x0), ZT_Y(y0), ZT_X(x1), ZT_Y(y1), u0, v0, u0 + sw, v0 + sh, lit ? 0x80 : 0x6C, 0x80);
    } else if (cur >= 0) {
        Zt_Face(Pk_Chara(cur), cx, cy - h * 0.08f, h * 0.7f, lit);
    } else {
        Zt_Text(cx, cy, "-", h * 0.5f, w, ZT_DIM, 1, 0x80);
    }
    if (label) {
        Mx_Rect(ZT_X(x0), ZT_Y(y1 - h * 0.24f), ZT_X(x1), ZT_Y(y1), Mx_Rgba(8, 10, 34, 0x20), Mx_Rgba(8, 10, 34, 0x7C));
        Zt_Text(cx, y1 - h * 0.09f, Zb_Name(cur), lit ? 22.0f : 19.0f, w - 8.0f, lit ? ZT_GOLD : ZT_WHITE, 1, 0x80);
    }
    if (forms > 1) {
        Mx_Disc(ZT_X(x1 - 22.0f), ZT_Y(y0 + 22.0f), ZT_S(40.0f), Mx_Rgba(240, 140, 24, 0x80), Mx_Rgba(200, 96, 10, 0x80));
        sprintf(buf, "%d", (int)forms);
        Zt_Text(x1 - 22.0f, y0 + 22.0f, buf, 24.0f, 38.0f, ZT_WHITE, 1, 0x80);
    }
}

static void Zb_ChipRect(s32 i, s32 n, f32 *r) {
    f32 w = (1000.0f - (f32)(n - 1) * 14.0f) / (f32)n;

    r[0] = 900.0f + (f32)i * (w + 14.0f);
    r[1] = 38.0f;
    r[2] = r[0] + w;
    r[3] = 138.0f;
}

static void Zb_Draw(void) {
    s32 kind = sZw->pickRow->kind, cur, at, n, forms, i, k, row, rows;
    char sub[128], line[48];
    char *label[4];
    s32 what[4];
    f32 r[4];

    Zb_Grid();
    sZbLoads = 0;
    cur = sZw->pickCur;
    at = Zb_At();
    n = Zb_Count();
    forms = cur >= 0 && n > 0 ? Zb_Forms(at) : 0;
    if (sZbOpen != at) {
        sZbOpen = -1;
    }
    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(10, 8, 40, 20), Mx_Rgba(10, 8, 40, 30));
    /* the warrior, in the middle and large */
    Zw_EditStage(ZT_X(1200.0f), ZT_Y(830.0f), ZT_Y(600.0f));
    sprintf(sub, "para: %s%s", sZw->pickLayer ? "camada em " : "", Zt_KindLabel(kind));
    Zt_Header(Pk_Slots(kind) ? "APRENDER T\303\211CNICA" : "ESCOLHER LUTADOR", sub);
    Zt_Pill(ZW_T_GREEN, 1950.0f, 38.0f, 2340.0f, 138.0f, Pk_Slots(kind) ? "APRENDER" : "USAR", 40.0f, 0x80);
    k = Zt_PkChips(label, what);
    for (i = 0; i < k; i++) {
        s32 lit = (what[i] == 0 && !sZtPkMods && cur != -2) || (what[i] == 1 && sZtPkMods && cur != -2) || (what[i] == 2 && cur == -2);

        Zb_ChipRect(i, k, r);
        Zt_Pill(lit ? ZW_T_ORANGE : ZW_T_NAVY, r[0], r[1], r[2], r[3], label[i], 30.0f, 0x80);
    }
    /* the card of who is chosen */
    Zt_Box(ZW_T_PANEL, 1900.0f, 170.0f, 2340.0f, forms > 0 ? 626.0f : 526.0f, 34.0f, 0x80);
    Zb_Card(cur, 2120.0f, 320.0f, 220.0f, 250.0f, 1, 0, 0);
    Zt_Text(2120.0f, 492.0f, Zb_Name(cur), 34.0f, 420.0f, ZT_GOLD, 1, 0x80);
    if (forms > 0) {
        if (sZbOpen < 0) {
            sprintf(line, "%s (%d)", Zo_L("FORMAS", "FORMS", "FORMAS"), (int)forms);
        }
        Zt_Pill(sZbOpen < 0 ? ZW_T_ORANGE : ZW_T_RED, 1925.0f, 536.0f, 2315.0f, 608.0f, sZbOpen < 0 ? line : Zo_L("FECHAR", "CLOSE", "CERRAR"), 30.0f, 0x80);
    }
    /* its forms, only when asked for */
    if (sZbOpen >= 0) {
        Zt_Box(ZW_T_ASK, 60.0f, 170.0f, 700.0f, 850.0f, 34.0f, 0x80);
        Zt_Text(380.0f, 212.0f, Zo_L("FORMAS: toque para escolher", "FORMS: tap to choose", "FORMAS: toca para elegir"), 28.0f, 590.0f, ZT_GOLD, 1, 0x80);
        for (i = 0; i < forms; i++) {
            s32 id = sZbCell[at].form[i];
            f32 cx = 170.0f + (f32)(i % 3) * 210.0f, cy = 328.0f + (f32)(i / 3) * 205.0f;

            Zb_Card(id, cx, cy, 176.0f, 142.0f, id == cur, 0, 0);
            Zt_Text(cx, cy + 92.0f, Zb_Name(id), 18.0f, 200.0f, id == cur ? ZT_GOLD : ZT_WHITE, 1, 0x80);
        }
    }
    /* the bar: one row of the grid */
    Zt_Box(ZW_T_ASK, 400.0f, 874.0f, 2000.0f, 1070.0f, 36.0f, 0x78);
    rows = n > 0 ? (n + ZB_COLS - 1) / ZB_COLS : 1;
    row = at / ZB_COLS;
    for (i = 0; i < ZB_COLS && row * ZB_COLS + i < n; i++) {
        s32 c = row * ZB_COLS + i, lit = c == at && cur >= 0;

        Zb_Card(Zb_Base(c), 540.0f + (f32)i * 220.0f, 972.0f, lit ? 206.0f : 190.0f, lit ? 172.0f : 156.0f, lit, Zb_Forms(c), 1);
    }
    Zt_Box(ZW_T_ASK, 250.0f, 874.0f, 382.0f, 1070.0f, 30.0f, 0x80);
    Mx_Fit(ZW_UP, ZT_X(316.0f), ZT_Y(924.0f), ZT_S(70.0f), 0x80);
    Mx_Fit(ZW_DOWN, ZT_X(316.0f), ZT_Y(1020.0f), ZT_S(70.0f), 0x80);
    Zt_Box(ZW_T_ASK, 2018.0f, 874.0f, 2150.0f, 1070.0f, 30.0f, 0x80);
    Zt_Text(2084.0f, 940.0f, Zo_L("LINHA", "ROW", "FILA"), 24.0f, 120.0f, ZT_DIM, 1, 0x80);
    sprintf(line, "%d/%d", (int)row + 1, (int)rows);
    Zt_Text(2084.0f, 1000.0f, line, 36.0f, 120.0f, ZT_GOLD, 1, 0x80);
    if (Pk_Slots(kind) > 0 && sZw->pickStage == 1) { /* which of its techniques */
        s32 slots = Pk_Slots(kind), who = Pk_Chara(sZw->pickCur);
        u32 shade = Mx_Rgba(4, 6, 26, 0x58);

        Mx_Rect(0.0f, 0.0f, MX_W, MX_H, shade, shade);
        Zt_Box(ZW_T_ASK, 650.0f, 200.0f, 1750.0f, 940.0f, 48.0f, 0x80);
        Zt_Text(1200.0f, 265.0f, Zb_Name(sZw->pickCur), 44.0f, 1000.0f, ZT_GOLD, 1, 0x80);
        Zt_Text(1200.0f, 325.0f, Zo_L("escolha a t\303\251cnica", "choose the technique", "elige la t\303\251cnica"), 30.0f, 1000.0f, ZT_DIM, 1, 0x80);
        for (i = 0; i < slots; i++) {
            f32 y = 370.0f + (f32)i * 140.0f;
            char name[48];
            char *note = i == 2 ? "Ultimate" : slots == 2 ? "Blast 1" : "Blast 2";

            Port_FighterMove(who, (slots == 2 ? 0 : 2) + i, name, sizeof(name));
            if (kind == ROW_LABLIKE && (Port_FighterMoveFx(who, 2 + i) == 2 || Port_FighterMoveFx(who, 2 + i) == 7)) {
                note = Zo_L("investida: n\303\243o serve", "rush: won't fit", "embestida: no sirve");
            } else if (kind >= ROW_FX0 && kind <= ROW_FX2 && Port_FighterMoveFx(who, 2 + i) != Pk_FxType(kind)) {
                note = Zo_L("outro tipo: n\303\243o serve", "another kind", "otro tipo");
            }
            Zt_Box(i == sZw->pickSlot ? ZW_T_SEL : ZW_T_PANEL, 700.0f, y, 1700.0f, y + 124.0f, 30.0f, 0x80);
            Zt_Text(740.0f, y + 44.0f, name[0] ? name : "-", 38.0f, 920.0f, ZT_WHITE, 0, 0x80);
            Zt_Text(740.0f, y + 94.0f, note, 28.0f, 920.0f, ZT_DIM, 0, 0x80);
        }
        Zt_Pill(ZW_T_RED, 700.0f, 820.0f, 1180.0f, 915.0f, Zo_L("Voltar", "Back", "Volver"), 36.0f, 0x80);
        Zt_Pill(ZW_T_GREEN, 1220.0f, 820.0f, 1700.0f, 915.0f, Zo_L("APRENDER", "LEARN", "APRENDER"), 38.0f, 0x80);
    }
    Co_PaintTick();
    Ws_ShapeTick();
    if (sZw->noteT > 0) {
        Zt_Box(ZW_T_ASK, 760.0f, 150.0f, 1840.0f, 236.0f, 40.0f, 0x80);
        Zt_Text(1300.0f, 193.0f, sZw->note, 30.0f, 1040.0f, ZT_WHITE, 1, 0x80);
    }
}

static void Zb_Tap(f32 x, f32 y) {
    s32 at, n, i, k, row, forms;
    char *label[4];
    s32 what[4];
    f32 r[4];

    Zb_Grid();
    at = Zb_At();
    n = Zb_Count();
    forms = sZw->pickCur >= 0 && n > 0 ? Zb_Forms(at) : 0;
    if (Pk_Slots(sZw->pickRow->kind) > 0 && sZw->pickStage == 1) {
        for (i = 0; i < Pk_Slots(sZw->pickRow->kind); i++) {
            if (Zt_In(x, y, 700.0f, 370.0f + i * 140.0f, 1700.0f, 494.0f + i * 140.0f)) {
                sZw->pickSlot = i;
                Snd_PlaySe(1, 0);
            }
        }
        if (Zt_In(x, y, 1220.0f, 820.0f, 1700.0f, 915.0f)) {
            sZbTake = 1;
            Zt_Press(PADG_CROSS);
        } else if (Zt_In(x, y, 700.0f, 820.0f, 1180.0f, 915.0f) || Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) {
            Zt_Press(PADG_TRIANGLE);
        }
        return;
    }
    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) {
        sZbOpen = -1;
        sZw->pickStage = 0;
        Zt_Press(PADG_TRIANGLE);
        return;
    }
    if (Zt_In(x, y, 1950.0f, 30.0f, 2350.0f, 145.0f)) { /* USE */
        if (Pk_Slots(sZw->pickRow->kind) > 0) { /* (the technique next) */
            sZbOpen = -1;
            sZw->pickStage = 1;
            sZw->pickSlot = sZw->pickSlot < Pk_Slots(sZw->pickRow->kind) ? sZw->pickSlot : 0;
            Snd_PlaySe(1, 1);
            return;
        }
        sZbTake = 1;
        sZbOpen = -1;
        Zt_Press(PADG_CROSS);
        return;
    }
    k = Zt_PkChips(label, what);
    for (i = 0; i < k; i++) {
        Zb_ChipRect(i, k, r);
        if (!Zt_In(x, y, r[0], r[1] - 6.0f, r[2], r[3] + 6.0f)) {
            continue;
        }
        sZbOpen = -1;
        if (what[i] == 2) {
            Zt_PkChoose(-2);
        } else if (what[i] == 3) {
            Zt_Press(PADG_R1);
        } else {
            sZtPkMods = what[i];
            Zt_PkChoose(what[i] ? MM_CHARA_COUNT : sZw->pickCur >= 0 && sZw->pickCur < MM_CHARA_COUNT ? sZw->pickCur : 0);
        }
        return;
    }
    if (forms > 0 && Zt_In(x, y, 1900.0f, 520.0f, 2340.0f, 626.0f)) {
        sZbOpen = sZbOpen < 0 ? at : -1;
        Snd_PlaySe(1, 1);
        return;
    }
    if (sZbOpen >= 0 && x <= 700.0f && y >= 240.0f && y <= 860.0f) {
        s32 col = (s32)((x - 65.0f) / 210.0f), line = (s32)((y - 245.0f) / 205.0f);

        if (col >= 0 && col < 3 && line >= 0) {
            Zb_Form(line * 3 + col);
        }
        return;
    }
    if (y >= 860.0f && x >= 240.0f && x <= 392.0f) {
        Zb_Move(0, y < 972.0f ? -1 : 1);
        return;
    }
    if (y >= 860.0f && x >= 430.0f && x < 430.0f + 220.0f * ZB_COLS && n > 0) {
        row = at / ZB_COLS;
        i = row * ZB_COLS + (s32)((x - 430.0f) / 220.0f);
        if (i < n) {
            if (i == at && sZw->pickCur >= 0) { /* (again: its forms) */
                if (Zb_Forms(i) > 0) {
                    sZbOpen = sZbOpen < 0 ? i : -1;
                    Snd_PlaySe(1, 1);
                }
            } else {
                Zb_Go(i);
            }
        }
    }
}

/* The pad's Square: the next of the game's fighters / the mods' / none. */
static void Zb_NextChip(void) {
    char *label[4];
    s32 what[4], n = Zt_PkChips(label, what), i, at = 0;

    for (i = 0; i < n; i++) {
        if ((what[i] == 0 && !sZtPkMods && sZw->pickCur != -2) || (what[i] == 1 && sZtPkMods) || (what[i] == 2 && sZw->pickCur == -2)) {
            at = i;
        }
    }
    for (i = 1; i <= n; i++) {
        s32 w = what[(at + i) % n];

        if (w == 3) { /* (the costume: L1 / R1) */
            continue;
        }
        sZbOpen = -1;
        if (w == 2) {
            Zt_PkChoose(-2);
        } else {
            sZtPkMods = w;
            Zt_PkChoose(w ? MM_CHARA_COUNT : 0);
        }
        Snd_PlaySe(1, 1);
        return;
    }
}
