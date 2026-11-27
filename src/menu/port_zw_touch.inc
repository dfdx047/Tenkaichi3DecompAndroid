/* Z Warriors by touch (Android): the editor and the questions as the approved templates (2400 x 1080 mockups,
   mods-zwarriors/todas_telas.py). Included by port_zw.c. Positions below are the templates' pixels. */
#define ZT_X(v) ((v) * (MX_W / 2400.0f))
#define ZT_Y(v) ((v) * (MX_H / 1080.0f))
#define ZT_S(v) ((v) * (MX_H / 1080.0f))  /* a size that keeps its shape */
#define ZT_CAP(v) ((v) * 0.58f)           /* a letter's capitals for the template's font size */
#define ZT_C(r, g, b) (0x80000000u | (u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16))
#define ZT_WHITE ZT_C(255, 255, 255)
#define ZT_GOLD ZT_C(255, 214, 90)
#define ZT_DIM ZT_C(190, 202, 245)
#define ZT_GREEN ZT_C(110, 230, 160)
#define ZT_INK ZT_C(22, 20, 52)

static void Zt_Text(f32 x, f32 y, char *s, f32 size, f32 width, u32 colour, s32 align, s32 alpha) {
    Mx_TextC(ZT_X(x), ZT_Y(y), s, ZT_CAP(size), width > 0.0f ? ZT_X(width) : 0.0f, colour, colour, ZT_INK, align, alpha);
}

static void Zt_Box(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 r, s32 alpha) {
    Mx_Box(id, ZT_X(x0), ZT_Y(y0), ZT_X(x1), ZT_Y(y1), ZT_S(r), alpha);
}

static void Zt_Pill(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, char *s, f32 size, s32 alpha) {
    Mx_Bar(id, ZT_X(x0), ZT_Y(y0), ZT_X(x1), ZT_Y(y1), alpha);
    Zt_Text((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, s, size, x1 - x0 - 40.0f, ZT_WHITE, 1, alpha);
}

static void Zt_Chev(f32 x, f32 y, f32 h) {
    Mx_Fit(ZW_T_CHEV, ZT_X(x), ZT_Y(y), ZT_S(h), 0x80);
}

/* ---- the editor's views: 0 the tab's chosen rows (templates 03-06), 1 / 2 all its options (07-12) ---- */
static s32 sZtPage;

typedef struct ZtItem {
    s32 kind;   /* a ROW_*; -1 a heading; -2 opens all the tab's options */
    char *label;
    s32 col;
} ZtItem;

static const ZtItem kZtMain0[] = {{ROW_NAME, "Nome", 0}, {ROW_TITLE, "T\303\255tulo", 0}, {ROW_BODY, "Corpo", 0}, {ROW_STANCE, "Postura", 0},
    {ROW_VOICE, "Voz", 0}, {-2, "Retrato / \303\255cone / rosto do HUD", 0}};
static const ZtItem kZtMain1[] = {{ROW_WORKSHOP, "Esculpir", 0}, {ROW_COLOURS, "Cores", 0}, {ROW_SHADING, "Sombreamento", 0},
    {ROW_TEXTURES, "Pintar texturas", 0}, {ROW_PART + 0, "Cabelo", 0}, {ROW_SIZE + 0, "Tamanho da cabe\303\247a", 0}};
static const ZtItem kZtMain2[] = {{ROW_MOVES, "Golpes corpo a corpo", 0}, {ROW_FINISHERS, "Finalizadores", 0}, {ROW_SKILL0, "Blast 1 (1)", 0},
    {ROW_TECH0, "Blast 2 (1)", 0}, {ROW_TECH2, "Ultimate", 0}, {ROW_LAB + 4, "Ajuste fino da Ultimate", 0}};
static const ZtItem kZtMain3[] = {{ROW_FORM + 0, "Forma 1", 0}, {ROW_FORMKEY + 0, "Comando", 0}, {ROW_FORMCOST + 0, "Custo de ki", 0},
    {ROW_FORMKIND + 0, "Tipo", 0}, {ROW_FORMAURA + 0, "Cor da aura", 0}, {ROW_FORM + 1, "Forma 2", 0}};

#define ZT_H(c, s) {-1, s, c}
static const ZtItem kZtAll0[] = {
    ZT_H(0, "GUERREIRO"), {ROW_NAME, "Nome", 0}, {ROW_TITLE, "T\303\255tulo", 0},
    ZT_H(0, "APAR\303\212NCIA"), {ROW_BODY, "Corpo", 0}, {ROW_STANCE, "Postura", 0}, {ROW_POSE, "Pose pr\303\263pria", 0}, {ROW_VOICE, "Voz", 0},
    ZT_H(1, "RETRATOS"), {ROW_RENDER, "Retrato", 1}, {ROW_CHIP, "\303\215cone de sele\303\247\303\243o", 1}, {ROW_HUD, "Rosto do HUD", 1}, {ROW_PREVIEW, "Pr\303\251via", 1},
    ZT_H(2, "TRAJES"), {ROW_COSTUMES, "Trajes", 2}, {ROW_COSEDIT, "Editar traje", 2},
    ZT_H(2, "DANOS DE BATALHA"), {ROW_DAMAGE, "Apar\303\252ncia danificada", 2}};
static const ZtItem kZtAll1a[] = {
    ZT_H(0, "FERRAMENTAS"), {ROW_WORKSHOP, "Esculpir", 0}, {ROW_COLOURS, "Cores", 0}, {ROW_SHADING, "Sombreamento", 0},
    {ROW_TEXTURES, "Pintar texturas", 0}, {ROW_SHADEFROM, "Sombreamento emprestado", 0}, {ROW_DYNSHADE, "Brilho Super Saiyajin", 0},
    {ROW_SURPRISE, "Sortear", 0}, {ROW_WHOLEHEAD, "Cabe\303\247a inteira", 0}, {ROW_OUTFIT, "Roupa inteira", 0}, {ROW_RESETPARTS, "Redefinir pe\303\247as", 0},
    ZT_H(1, "CAMADAS EXTRAS"), {ROW_XL + 0, "Camada 1", 1}, {ROW_XL + 1, "Camada 2", 1}, {ROW_XL + 2, "Camada 3", 1}, {ROW_XL + 3, "Camada 4", 1},
    ZT_H(1, "PORTE"), {ROW_SIZE + 5, "Tronco", 1}, {ROW_SIZE + 4, "Bra\303\247os", 1}, {ROW_SIZE + 6, "Pernas", 1}};
static const ZtItem kZtAll1b[] = {
    ZT_H(0, "CABE\303\207A"), {ROW_PART + 0, "Cabelo", 0}, {ROW_PART + 1, "Cabe\303\247a", 0}, {ROW_PART + 2, "Rosto", 0},
    {ROW_SIZE + 0, "Tamanho da cabe\303\247a", 0}, {ROW_SIZE + 1, "Tamanho do cabelo", 0},
    ZT_H(1, "PARTE DE CIMA"), {ROW_PART + 3, "Peito", 1}, {ROW_PART + 4, "Cintura", 1}, {ROW_PART + 5, "Bra\303\247os", 1},
    {ROW_PART + 6, "Antebra\303\247os", 1}, {ROW_PART + 7, "M\303\243os", 1}, {ROW_SIZE + 2, "Tamanho das m\303\243os", 1},
    ZT_H(2, "PARTE DE BAIXO"), {ROW_PART + 8, "Quadril e coxas", 2}, {ROW_PART + 9, "Canelas", 2}, {ROW_PART + 10, "P\303\251s", 2},
    {ROW_SIZE + 3, "Tamanho dos p\303\251s", 2}, ZT_H(2, "EXTRAS"), {ROW_PART + 11, "Acess\303\263rios", 2}};
static const ZtItem kZtAll2a[] = {
    ZT_H(0, "LUTA"), {ROW_MOVES, "Golpes corpo a corpo", 0}, {ROW_FINISHERS, "Finalizadores", 0}, {ROW_STYLE, "Atributos", 0},
    {ROW_EFFECTS, "Rajadas de ki", 0}, {ROW_AURA, "Aura", 0},
    ZT_H(1, "ANIMA\303\207\303\225ES"), {ROW_ANIM + 0, "Andar e voar", 1}, {ROW_ANIM + 1, "Carregar", 1}, {ROW_ANIM + 3, "Disparar ki", 1},
    {ROW_ANIM + 4, "Agarr\303\265es", 1}, {ROW_ANIM + 5, "Defesa e esquiva", 1}, {ROW_ANIM + 6, "Entrada", 1}, {ROW_ANIM + 7, "Vit\303\263ria", 1},
    {ROW_ANIM + 8, "Provoca\303\247\303\243o", 1},
    ZT_H(2, "BLAST 1 (1)"), {ROW_SKILL0, "T\303\251cnica", 2}, {ROW_SFX0, "Visual", 2}, {ROW_LAB + 0, "Ajuste fino", 2}, {ROW_GLOW + 0, "Brilho do corpo", 2},
    ZT_H(2, "BLAST 1 (2)"), {ROW_SKILL1, "T\303\251cnica", 2}, {ROW_SFX1, "Visual", 2}, {ROW_LAB + 1, "Ajuste fino", 2}, {ROW_GLOW + 1, "Brilho do corpo", 2}};
static const ZtItem kZtAll2b[] = {
    ZT_H(0, "BLAST 2 (1)"), {ROW_TECH0, "T\303\251cnica", 0}, {ROW_FX0, "Efeitos", 0}, {ROW_CAM + 0, "C\303\242mera", 0}, {ROW_LAB + 2, "Ajuste fino", 0},
    {ROW_GLOW + 2, "Brilho do corpo", 0},
    ZT_H(1, "BLAST 2 (2)"), {ROW_TECH1, "T\303\251cnica", 1}, {ROW_FX1, "Efeitos", 1}, {ROW_CAM + 1, "C\303\242mera", 1}, {ROW_LAB + 3, "Ajuste fino", 1},
    {ROW_GLOW + 3, "Brilho do corpo", 1},
    ZT_H(2, "ULTIMATE"), {ROW_TECH2, "T\303\251cnica", 2}, {ROW_FX2, "Efeitos", 2}, {ROW_CAM + 2, "C\303\242mera de abertura", 2},
    {ROW_CAM + 3, "C\303\242mera final", 2}, {ROW_LAB + 4, "Ajuste fino", 2}, {ROW_GLOW + 4, "Brilho do corpo", 2}};
#define ZT_FORM(c, s) ZT_H(c, s), {ROW_FORM + c, "Guerreiro", c}, {ROW_FORMKEY + c, "Comando", c}, {ROW_FORMCOST + c, "Custo de ki", c}, \
    {ROW_FORMKIND + c, "Tipo", c}, {ROW_FORMAURA + c, "Cor da aura", c}
static const ZtItem kZtAll3[] = {ZT_FORM(0, "FORMA 1"), ZT_FORM(1, "FORMA 2"), ZT_FORM(2, "FORMA 3"), ZT_FORM(3, "FORMA 4")};

#define ZT_N(a) ((s32)(sizeof(a) / sizeof((a)[0])))

static const ZtItem *Zt_Items(s32 *n) {
    if (sZtPage == 0) {
        *n = 6;
        return sZw->tab == 1 ? kZtMain1 : sZw->tab == 2 ? kZtMain2 : sZw->tab == 3 ? kZtMain3 : kZtMain0;
    }
    switch (sZw->tab) {
    case 1: *n = sZtPage == 2 ? ZT_N(kZtAll1b) : ZT_N(kZtAll1a); return sZtPage == 2 ? kZtAll1b : kZtAll1a;
    case 2: *n = sZtPage == 2 ? ZT_N(kZtAll2b) : ZT_N(kZtAll2a); return sZtPage == 2 ? kZtAll2b : kZtAll2a;
    case 3: *n = ZT_N(kZtAll3); return kZtAll3;
    }
    *n = ZT_N(kZtAll0);
    return kZtAll0;
}

static s32 Zt_Pages(void) {
    return sZw->tab == 1 || sZw->tab == 2 ? 3 : 2;
}

/* The box of item i of the view (template pixels). */
static void Zt_Rect(const ZtItem *it, s32 n, s32 i, f32 *r) {
    s32 cols = 1, k, c, lines = 0, most = 0, above = 0;
    f32 w, h;

    if (sZtPage == 0) {
        r[0] = 60.0f;
        r[1] = 285.0f + (f32)i * 106.0f;
        r[2] = 1420.0f;
        r[3] = r[1] + 96.0f;
        return;
    }
    for (k = 0; k < n; k++) {
        cols = it[k].col + 1 > cols ? it[k].col + 1 : cols;
    }
    for (c = 0; c < cols; c++) {
        for (lines = 0, k = 0; k < n; k++) {
            lines += it[k].col == c;
        }
        most = lines > most ? lines : most;
    }
    for (k = 0; k < i; k++) {
        above += it[k].col == it[i].col;
    }
    h = (f32)(s32)(780.0f / ((f32)most + 0.5f));
    h = h > 66.0f ? 66.0f : h;
    w = (f32)((2280 - (cols - 1) * 24) / cols);
    r[0] = 60.0f + (f32)it[i].col * (w + 24.0f);
    r[1] = 280.0f + (f32)above * h + 3.0f;
    r[2] = r[0] + w;
    r[3] = r[1] + h - 6.0f;
}

static s32 Zt_RowOf(s32 kind) {
    s32 n, i;
    const ZwRow *rows = Zw_Rows(sZw->tab, &n);

    for (i = 0; i < n; i++) {
        if (rows[i].kind == kind) {
            return i;
        }
    }
    return -1; /* (the input of a form has a row; every item of the views does) */
}

/* The pad's up / down over what the view shows, in its order. */
static void Zt_Step(s32 dir) {
    s32 n, i, at = -1, tries, rn;
    const ZtItem *it = Zt_Items(&n);
    const ZwRow *rows = Zw_Rows(sZw->tab, &rn);
    s32 kind = rows[sZw->row[sZw->tab]].kind;

    for (i = 0; i < n; i++) {
        if (it[i].kind == kind) {
            at = i;
        }
    }
    i = at < 0 ? (dir > 0 ? -1 : n) : at;
    for (tries = 0; tries < n; tries++) {
        i = (i + dir + n) % n;
        if (it[i].kind >= 0 && Zt_RowOf(it[i].kind) >= 0) {
            sZw->row[sZw->tab] = Zt_RowOf(it[i].kind);
            return;
        }
    }
}

static void Zt_Header(char *title, char *sub) {
    Mx_Fit(ZW_T_BACK, ZT_X(116.0f), ZT_Y(86.0f), ZT_S(112.0f), 0x80);
    Zt_Text(205.0f, 64.0f, title, 54.0f, 900.0f, ZT_GOLD, 0, 0x80);
    Zt_Text(205.0f, 120.0f, sub, 29.0f, 1100.0f, ZT_DIM, 0, 0x80);
}

static char *kZtBar0[4] = {"Pose pr\303\263pria", "Sortear tudo", "Anima\303\247\303\265es e aura", "Formas 3 e 4"};

static void Zt_EdDraw(void) {
    static char *tabs[4] = {"GUERREIRO", "PE\303\207AS", "GOLPES", "FORMAS"};
    PortWarrior *w = &sZw->w;
    s32 n, i, k, rn;
    const ZtItem *it = Zt_Items(&n);
    const ZwRow *rows = Zw_Rows(sZw->tab, &rn);
    s32 cur = rows[sZw->row[sZw->tab]].kind;
    char sub[160];
    f32 r[4];
    f32 tw = sZtPage == 0 ? 329.5f : 559.5f;

    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(10, 8, 40, 35), Mx_Rgba(10, 8, 40, 35));
    if (sZtPage == 0) {
        sprintf(sub, "%.40s%s", w->name[0] ? w->name : "Sem nome", sZw->dirty ? "  \342\200\242  n\303\243o salvo" : "");
    } else {
        sprintf(sub, "todas as op\303\247\303\265es  \342\200\242  %s%s",
                sZw->tab == 0 ? "Guerreiro" : sZw->tab == 1 ? "Pe\303\247as" : sZw->tab == 2 ? "Golpes" : "Transforma\303\247\303\265es",
                Zt_Pages() == 3 ? (sZtPage == 1 ? " 1 de 2" : " 2 de 2") : "");
    }
    Zt_Header("EDITAR GUERREIRO", sub);
    Zt_Pill(ZW_T_GREEN, 1950.0f, 38.0f, 2340.0f, 138.0f, "SALVAR", 40.0f, 0x80);
    for (k = 0; k < 4; k++) {
        f32 x = 60.0f + (f32)k * (tw + 14.0f);
        f32 y0 = sZtPage == 0 ? 165.0f : 160.0f;

        Zt_Pill(k == sZw->tab ? ZW_T_ORANGE : ZW_T_NAVY, x, y0, x + tw, y0 + (sZtPage == 0 ? 100.0f : 92.0f), tabs[k], 34.0f, 0x80);
    }
    if (sZtPage == 0) { /* the warrior on its stage */
        Zt_Box(ZW_T_STAGE, 1460.0f, 165.0f, 2340.0f, 925.0f, 34.0f, 0x80);
        Zw_EditStage(ZT_X(1900.0f), ZT_Y(790.0f), ZT_Y(500.0f));
        Zt_Text(1900.0f, 220.0f, "modelo 3D do guerreiro", 30.0f, 840.0f, ZT_DIM, 1, 0x80);
    }
    Co_PaintTick();
    Ws_ShapeTick();
    for (i = 0; i < n; i++) {
        s32 kind = it[i].kind;
        s32 sel = kind >= 0 && kind == cur;
        f32 mid;

        Zt_Rect(it, n, i, r);
        mid = (r[1] + r[3]) * 0.5f;
        if (kind == -1) {
            Zt_Text(r[0] + 12.0f, mid + 3.0f, it[i].label, 31.0f, r[2] - r[0] - 20.0f, ZT_GOLD, 0, 0x80);
            continue;
        }
        Zt_Box(sel ? ZW_T_SEL : ZW_T_PANEL, r[0], r[1], r[2], r[3], sZtPage == 0 ? 28.0f : 18.0f, 0x80);
        if (sZtPage == 0) {
            char *v = kind == -2 ? "3 itens" : kind == ROW_WORKSHOP || kind == ROW_SHADING || kind == ROW_TEXTURES ||
                      (kind >= ROW_LAB && kind <= ROW_LAB_END) ? "abrir" : Zw_Value(kind);

            Zt_Text(r[0] + 34.0f, mid, it[i].label, 36.0f, 680.0f, ZT_WHITE, 0, 0x80);
            if (IS_SIZE(kind)) { /* (a size: its ends step it) */
                char pc[24];

                sprintf(pc, "-    %d%%    +", (int)(w->size[kind - ROW_SIZE] > 0 ? w->size[kind - ROW_SIZE] : 100));
                Zt_Text(r[2] - 60.0f, mid, pc, 34.0f, 570.0f, ZT_GOLD, 2, 0x80);
            } else {
                Zt_Text(r[2] - 60.0f, mid, v, 31.0f, 570.0f, ZT_DIM, 2, 0x80);
            }
            Zt_Chev(r[2] - 32.0f, mid, 36.0f);
        } else {
            f32 size = r[3] - r[1] - 18.0f > 31.0f ? 31.0f : r[3] - r[1] - 18.0f;
            s32 inPlace = IS_SIZE(kind) || kind == ROW_COSTUMES || kind == ROW_COSEDIT || kind == ROW_DAMAGE || kind == ROW_DYNSHADE ||
                          (kind >= ROW_FORM && kind <= ROW_FORM_END) || (kind >= ROW_FORMKEY && kind <= ROW_FORMKEY_END) ||
                          (kind >= ROW_FORMCOST && kind <= ROW_FORMCOST_END) || (kind >= ROW_FORMKIND && kind <= ROW_FORMKIND_END) ||
                          (kind >= ROW_FORMAURA && kind <= ROW_FORMAURA_END); /* (changed where they are: their value shown) */

            Zt_Text(r[0] + 24.0f, mid, it[i].label, size, inPlace ? (r[2] - r[0]) * 0.55f : r[2] - r[0] - 90.0f, ZT_WHITE, 0, 0x80);
            if (inPlace) {
                Zt_Text(r[2] - 55.0f, mid, Zw_Value(kind), size * 0.85f, (r[2] - r[0]) * 0.35f, ZT_DIM, 2, 0x80);
            }
            Zt_Chev(r[2] - 34.0f, mid, 24.0f);
        }
    }
    if (sZtPage == 0) {
        static char *bar[4] = {NULL, "Sele\303\247\303\243o de personagens", "Luta de teste", "Desfazer"};
        f32 bw = (2280.0f - 3.0f * 18.0f) / 4.0f;

        for (k = 0; k < 4; k++) {
            f32 x = 60.0f + (f32)k * (bw + 18.0f);

            Zt_Pill(ZW_T_BAR, x, 940.0f, x + bw, 1044.0f, k == 0 ? kZtBar0[sZw->tab] : bar[k], 34.0f, 0x80);
        }
    }
    if (sZw->saving || sZw->noteT > 0) { /* (what has just happened) */
        Zt_Box(ZW_T_ASK, 760.0f, 40.0f, 1920.0f, 136.0f, 40.0f, 0x80);
        Zt_Text(1340.0f, 88.0f, sZw->saving ? "Montando seu guerreiro..." : sZw->note, 30.0f, 1100.0f, ZT_WHITE, 1, 0x80);
    }
}

static s32 Zt_In(f32 x, f32 y, f32 x0, f32 y0, f32 x1, f32 y1) {
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

static void Zt_Press(u32 bit) {
    gPad[0].gamePressed |= bit;
    gPad[0].gameRepeat |= bit;
}

/* A finger on (x, y), template pixels: the editor reads the presses this frame (Ed_Update comes after). */
static void Zt_EdTap(f32 x, f32 y) {
    s32 n, i, k, row;
    const ZtItem *it = Zt_Items(&n);
    f32 r[4];
    f32 tw = sZtPage == 0 ? 329.5f : 559.5f;

    if (sZw->saving) {
        return;
    }
    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) { /* back: from all the options to the tab; from the tab, leave */
        if (sZtPage != 0) {
            sZtPage = 0;
            Snd_PlaySe(1, 2);
        } else {
            Zt_Press(PADG_TRIANGLE);
        }
        return;
    }
    if (Zt_In(x, y, 1950.0f, 30.0f, 2350.0f, 145.0f)) {
        Zt_Press(PADG_START);
        return;
    }
    if (y >= 155.0f && y <= 268.0f) {
        for (k = 0; k < 4; k++) {
            f32 x0 = 60.0f + (f32)k * (tw + 14.0f);

            if (x >= x0 && x <= x0 + tw) { /* another tab; the one open again: all its options, page by page */
                sZtPage = k == sZw->tab ? (sZtPage + 1) % Zt_Pages() : 0;
                sZw->tab = k;
                sZw->tabT = 0;
                sZw->rest = 0;
                Snd_PlaySe(1, 0);
                return;
            }
        }
    }
    if (sZtPage == 0 && y >= 935.0f) {
        f32 bw = (2280.0f - 3.0f * 18.0f) / 4.0f;

        for (k = 0; k < 4; k++) {
            f32 x0 = 60.0f + (f32)k * (bw + 18.0f);

            if (x < x0 || x > x0 + bw) {
                continue;
            }
            if (k == 0 && sZw->tab >= 2) {
                sZtPage = 1;
                Snd_PlaySe(1, 1);
            } else if (k == 3) {
                Zt_Press(PADG_L3);
            } else {
                row = Zt_RowOf(k == 0 ? (sZw->tab == 0 ? ROW_POSE : ROW_SURPRISE) : ROW_PREVIEW);
                if (k > 0) { /* (the test rows are the first tab's) */
                    sZw->tab = 0;
                    row = Zt_RowOf(ROW_PREVIEW);
                }
                if (row >= 0) {
                    sZw->row[sZw->tab] = row;
                    Zt_Press(k == 2 ? PADG_CIRCLE : PADG_CROSS);
                }
            }
        }
        return;
    }
    for (i = 0; i < n; i++) {
        s32 kind = it[i].kind;

        Zt_Rect(it, n, i, r);
        if (kind == -1 || !Zt_In(x, y, r[0], r[1] - 4.0f, r[2], r[3] + 4.0f)) {
            continue;
        }
        if (kind == -2) {
            sZtPage = 1;
            Snd_PlaySe(1, 1);
            return;
        }
        row = Zt_RowOf(kind);
        if (row < 0) {
            return;
        }
        sZw->row[sZw->tab] = row;
        sZw->rest = 0;
        if (IS_SIZE(kind)) { /* the left half smaller, the right half bigger */
            Zt_Press(x < r[0] + (r[2] - r[0]) * (sZtPage == 0 ? 0.8f : 0.5f) ? PADG_LEFT : PADG_RIGHT);
        } else if (kind >= ROW_FORMKEY && kind <= ROW_FORMKEY_END) {
            Zw_Note("O comando de transforma\303\247\303\243o \303\251 o do jogo.");
        } else if ((kind >= ROW_FORMCOST && kind <= ROW_FORMCOST_END) || (kind >= ROW_FORMAURA && kind <= ROW_FORMAURA_END)) {
            Zt_Press(PADG_RIGHT);
        } else {
            Zt_Press(PADG_CROSS);
        }
        return;
    }
}

/* ---- choosing a fighter (the approved previews, mods-zwarriors/previa_escolha.py): a big card per character, its
   forms below; on the right the warrior trying it on (the look's rows), the fighter's techniques, or the fighter. ---- */
#include "menu/zw_families.h"

#define ZT_PER_PAGE 12
static s32 sZtPkPage, sZtPkMods, sZtPkSeen;
static s32 sZtPkOpen = -1; /* the character whose forms are shown: only once its card is touched */

/* The first fighter of family f. */
static s32 Zt_FamilyFirst(s32 f) {
    s32 i;

    for (i = 0; i < MM_CHARA_COUNT; i++) {
        if (kZtFamilyOf[i] == f) {
            return i;
        }
    }
    return 0;
}

static s32 Zt_Family(s32 cur) {
    return cur >= 0 && cur < MM_CHARA_COUNT ? kZtFamilyOf[cur] : -1;
}

/* The page and the tab follow the picker's cursor when it was moved by something else (opening, the pad). */
static void Zt_PkSync(void) {
    s32 seen = sZw->pickCur + 1000 * (s32)(sZw->pickRow - (const ZwRow *)0) + 3;

    if (sZtPkSeen == seen && sZw->modeT > 1) { /* (just opened: always) */
        return;
    }
    sZtPkSeen = seen;
    sZtPkOpen = -1;
    if (sZw->pickCur >= MM_CHARA_COUNT) {
        sZtPkMods = 1;
        sZtPkPage = (sZw->pickCur - MM_CHARA_COUNT) / ZT_PER_PAGE;
    } else if (sZw->pickCur >= 0) {
        sZtPkMods = 0;
        sZtPkPage = kZtFamilyOf[sZw->pickCur] / ZT_PER_PAGE;
    }
}

static s32 Zt_PkPages(void) {
    s32 n = sZtPkMods ? sZw->pkMods : ZT_FAMILIES;

    return n > 0 ? (n + ZT_PER_PAGE - 1) / ZT_PER_PAGE : 1;
}

static void Zt_PkCard(s32 i, f32 *r) {
    f32 y0 = Pk_Slots(sZw->pickRow->kind) > 0 || !Pk_Side() ? 165.0f : 262.0f;

    r[0] = 60.0f + (f32)(i % 6) * 246.0f;
    r[1] = y0 + (f32)(i / 6) * 210.0f;
    r[2] = r[0] + 232.0f;
    r[3] = r[1] + 196.0f;
}

static f32 Zt_PkFormsY(void) {
    return Pk_Slots(sZw->pickRow->kind) > 0 || !Pk_Side() ? 590.0f : 682.0f;
}

static s32 Zt_PkChips(char **label, s32 *what) { /* the side picker's chips: 0 the game's, 1 mods', 2 none, 3 costume */
    static char cos[32];
    s32 kind = sZw->pickRow->kind, n = 0;

    label[n] = "Do jogo";
    what[n++] = 0;
    if (sZw->pkMods > 0) {
        label[n] = "Dos mods";
        what[n++] = 1;
    }
    if (Pk_Side() && Pk_None()) {
        label[n] = "Nenhum";
        what[n++] = 2;
    }
    if ((kind == ROW_BODY || IS_LOOKPART(kind)) && sZw->pickCur >= 0) {
        if (sZw->pickCos > 0) {
            sprintf(cos, "Traje %d", (int)sZw->pickCos);
        } else {
            sprintf(cos, "Traje: do guerreiro");
        }
        label[n] = cos;
        what[n++] = 3;
    }
    return n;
}

static void Zt_PkChipRect(s32 i, s32 n, f32 *r) {
    f32 w = (1462.0f - (f32)(n - 1) * 14.0f) / (f32)n;

    r[0] = 60.0f + (f32)i * (w + 14.0f);
    r[1] = 160.0f;
    r[2] = r[0] + w;
    r[3] = 246.0f;
}

/* What the row being chosen for is called. */
static char *Zt_KindLabel(s32 kind) {
    static const ZtItem *const lists[6] = {kZtAll0, kZtAll1a, kZtAll1b, kZtAll2a, kZtAll2b, kZtAll3};
    static const s32 counts[6] = {ZT_N(kZtAll0), ZT_N(kZtAll1a), ZT_N(kZtAll1b), ZT_N(kZtAll2a), ZT_N(kZtAll2b), ZT_N(kZtAll3)};
    static char out[96];
    s32 l, i;

    for (l = 0; l < 6; l++) {
        char *head[3] = {"", "", ""};

        for (i = 0; i < counts[l]; i++) {
            if (lists[l][i].kind == -1) {
                head[lists[l][i].col] = lists[l][i].label;
            } else if (lists[l][i].kind == kind) {
                if (l >= 3) {
                    sprintf(out, "%s  \342\200\242  %s", head[lists[l][i].col], lists[l][i].label);
                    return out;
                }
                return lists[l][i].label;
            }
        }
    }
    return sZw->pickRow->label;
}

static void Zt_Face(s32 chara, f32 x, f32 y, f32 size, s32 lit) {
    sZwChipTag = 1;
    Zw_Chip(chara, ZT_X(x), ZT_Y(y), ZT_S(size), lit);
    sZwChipTag = 0;
}

static void Zt_PkDraw(void) {
    s32 kind = sZw->pickRow->kind;
    s32 slots = Pk_Slots(kind);
    s32 side = Pk_Side();
    s32 cur = sZw->pickCur;
    s32 fam, i, n, count;
    f32 r[4];
    char sub[128], line[96];

    Zt_PkSync();
    fam = Zt_Family(cur);
    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(10, 8, 40, 35), Mx_Rgba(10, 8, 40, 35));
    sprintf(sub, "para: %s%s", sZw->pickLayer ? "camada em " : "", Zt_KindLabel(kind));
    Zt_Header(slots ? "APRENDER T\303\211CNICA" : "ESCOLHER LUTADOR", sub);
    Zt_Pill(ZW_T_GREEN, 1950.0f, 38.0f, 2340.0f, 138.0f, slots ? "APRENDER" : "USAR", 40.0f, 0x80);
    if (!slots && side) {
        char *label[4];
        s32 what[4];

        n = Zt_PkChips(label, what);
        for (i = 0; i < n; i++) {
            s32 lit = (what[i] == 0 && !sZtPkMods && cur != -2) || (what[i] == 1 && sZtPkMods && cur != -2) || (what[i] == 2 && cur == -2);

            Zt_PkChipRect(i, n, r);
            Zt_Pill(lit ? ZW_T_ORANGE : ZW_T_NAVY, r[0], r[1], r[2], r[3], label[i], 30.0f, 0x80);
        }
    } else if (sZw->pkMods > 0) {
        Zt_Pill(sZtPkMods ? ZW_T_ORANGE : ZW_T_NAVY, 1150.0f, 48.0f, 1522.0f, 128.0f, "Dos mods", 30.0f, 0x80);
    }
    /* the cards: a character each (or a mod fighter each) */
    count = sZtPkMods ? sZw->pkMods : ZT_FAMILIES;
    for (i = 0; i < ZT_PER_PAGE; i++) {
        s32 k = sZtPkPage * ZT_PER_PAGE + i, chara, sel, members = 0, j;

        if (k >= count) {
            break;
        }
        chara = sZtPkMods ? sZw->pkRef[k] : Zt_FamilyFirst(k);
        sel = sZtPkMods ? cur == MM_CHARA_COUNT + k : fam == k;
        Zt_PkCard(i, r);
        Zt_Box(sel ? ZW_T_SEL : ZW_T_PANEL, r[0], r[1], r[2], r[3], 28.0f, 0x80);
        Zt_Face(sel && !sZtPkMods ? cur : chara, r[0] + 116.0f, r[1] + 82.0f, 132.0f, sel);
        Zt_Text(r[0] + 116.0f, r[1] + 170.0f, sZtPkMods ? Zw_Name(chara) : kZtFamilyName[k], 30.0f, 214.0f, ZT_WHITE, 1, 0x80);
        for (j = 0; !sZtPkMods && j < MM_CHARA_COUNT; j++) {
            members += kZtFamilyOf[j] == k;
        }
        if (members > 1) {
            Mx_Disc(ZT_X(r[2] - 36.0f), ZT_Y(r[1] + 34.0f), ZT_S(50.0f), Mx_Rgba(230, 128, 18, 0x80), Mx_Rgba(200, 96, 10, 0x80));
            sprintf(line, "%d", (int)members);
            Zt_Text(r[2] - 36.0f, r[1] + 34.0f, line, 28.0f, 44.0f, ZT_WHITE, 1, 0x80);
        }
    }
    /* the forms of the character under the cursor */
    if (!sZtPkMods && fam >= 0 && fam == sZtPkOpen) {
        f32 y = Zt_PkFormsY(), w;

        for (n = 0, i = 0; i < MM_CHARA_COUNT; i++) {
            n += kZtFamilyOf[i] == fam;
        }
        Zt_Box(ZW_T_STAGE, 60.0f, y, 1522.0f, y + 236.0f, 28.0f, 0x80);
        sprintf(line, "%s: %s", n > 1 ? "FORMAS" : "LUTADOR", kZtFamilyName[fam]);
        Zt_Text(90.0f, y + 36.0f, line, 30.0f, 900.0f, ZT_GOLD, 0, 0x80);
        w = 1442.0f / (f32)(n < 9 ? 9 : n);
        for (n = 0, i = 0; i < MM_CHARA_COUNT; i++) {
            f32 x;

            if (kZtFamilyOf[i] != fam) {
                continue;
            }
            x = 70.0f + w * (f32)n++ + w * 0.5f;
            Zt_Face(i, x, y + 128.0f, w - 8.0f < 128.0f ? w - 8.0f : 128.0f, i == cur);
            Zt_Text(x, y + 212.0f, kZtShort[i], 25.0f, w - 6.0f, i == cur ? ZT_GOLD : ZT_WHITE, 1, 0x80);
        }
    }
    /* the pages */
    if (Zt_PkPages() > 1) {
        Zt_Pill(ZW_T_BAR, 60.0f, 940.0f, 330.0f, 1040.0f, "<  Anterior", 32.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1252.0f, 940.0f, 1522.0f, 1040.0f, "Pr\303\263xima  >", 32.0f, 0x80);
    }
    sprintf(line, "%s %d de %d", sZtPkMods ? "Dos mods" : "Personagens", (int)sZtPkPage + 1, (int)Zt_PkPages());
    Zt_Text(800.0f, 990.0f, line, 34.0f, 800.0f, ZT_WHITE, 1, 0x80);
    /* the right side */
    if (slots) {
        Zt_Box(ZW_T_STAGE, 1560.0f, 165.0f, 2340.0f, 1040.0f, 34.0f, 0x80);
        Zt_Face(Pk_Chara(cur), 1665.0f, 270.0f, 150.0f, 1);
        Zt_Text(1770.0f, 240.0f, Zw_Name(Pk_Chara(cur)), 40.0f, 550.0f, ZT_GOLD, 0, 0x80);
        Zt_Text(1770.0f, 300.0f, "escolha a t\303\251cnica", 30.0f, 550.0f, ZT_DIM, 0, 0x80);
        for (i = 0; i < slots; i++) {
            f32 y = 380.0f + (f32)i * 180.0f;
            char name[48];
            char *note = i == 2 ? "Ultimate" : slots == 2 ? "Blast 1" : "Blast 2";

            Port_FighterMove(Pk_Chara(cur), (slots == 2 ? 0 : 2) + i, name, sizeof(name));
            if (kind == ROW_LABLIKE && (Port_FighterMoveFx(Pk_Chara(cur), 2 + i) == 2 || Port_FighterMoveFx(Pk_Chara(cur), 2 + i) == 7)) {
                note = "investida: n\303\243o serve aqui";
            } else if (kind >= ROW_FX0 && kind <= ROW_FX2 && Port_FighterMoveFx(Pk_Chara(cur), 2 + i) != Pk_FxType(kind)) {
                note = "outro tipo: n\303\243o serve aqui";
            }
            Zt_Box(i == sZw->pickSlot ? ZW_T_SEL : ZW_T_PANEL, 1590.0f, y, 2310.0f, y + 160.0f, 30.0f, 0x80);
            Zt_Text(1630.0f, y + 58.0f, name[0] ? name : "(nenhuma)", 40.0f, 640.0f, ZT_WHITE, 0, 0x80);
            Zt_Text(1630.0f, y + 115.0f, note, 30.0f, 640.0f, ZT_DIM, 0, 0x80);
        }
        Zt_Pill(ZW_T_GREY, 1590.0f, 930.0f, 2310.0f, 1020.0f, "Cancelar", 34.0f, 0x80);
    } else {
        Zt_Box(ZW_T_STAGE, 1560.0f, 165.0f, 2340.0f, 925.0f, 34.0f, 0x80);
        if (side) {
            Zw_EditStage(ZT_X(1950.0f), ZT_Y(800.0f), ZT_Y(470.0f));
            sprintf(line, "experimentando: %s", cur == -2 ? "nenhum" : Zw_Name(Pk_Chara(cur)));
        } else {
            Zt_Face(Pk_Chara(cur), 1950.0f, 500.0f, 420.0f, 1);
            sprintf(line, "%s", Zw_Name(Pk_Chara(cur)));
        }
        Zt_Text(1950.0f, 220.0f, line, 30.0f, 740.0f, ZT_DIM, 1, 0x80);
        Zt_Pill(ZW_T_GREY, 1560.0f, 940.0f, 2340.0f, 1040.0f, "Cancelar", 34.0f, 0x80);
    }
    Co_PaintTick();
    Ws_ShapeTick();
    if (sZw->noteT > 0) {
        Zt_Box(ZW_T_ASK, 760.0f, 40.0f, 1920.0f, 136.0f, 40.0f, 0x80);
        Zt_Text(1340.0f, 88.0f, sZw->note, 30.0f, 1100.0f, ZT_WHITE, 1, 0x80);
    }
}

static void Zt_PkChoose(s32 cur) {
    if (cur != sZw->pickCur) {
        sZw->pickCur = cur;
        sZw->pickRest = 0;
        Snd_PlaySe(1, 0);
    }
    sZtPkSeen = cur + 1000 * (s32)(sZw->pickRow - (const ZwRow *)0) + 3; /* (the page stays) */
}

static void Zt_PkTap(f32 x, f32 y) {
    s32 kind = sZw->pickRow->kind;
    s32 slots = Pk_Slots(kind);
    s32 side = Pk_Side();
    s32 i, n, count, fam;
    f32 r[4];

    Zt_PkSync();
    fam = Zt_Family(sZw->pickCur);
    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f) || (slots && Zt_In(x, y, 1590.0f, 925.0f, 2310.0f, 1040.0f)) ||
        (!slots && Zt_In(x, y, 1560.0f, 935.0f, 2340.0f, 1045.0f))) {
        sZw->pickStage = 0;
        Zt_Press(PADG_TRIANGLE);
        return;
    }
    if (Zt_In(x, y, 1950.0f, 30.0f, 2350.0f, 145.0f)) {
        if (slots) {
            sZw->pickStage = 1;
            sZw->pickSlot = sZw->pickSlot < slots ? sZw->pickSlot : 0;
        }
        Zt_Press(PADG_CROSS);
        return;
    }
    if (!slots && side) {
        char *label[4];
        s32 what[4];

        n = Zt_PkChips(label, what);
        for (i = 0; i < n; i++) {
            Zt_PkChipRect(i, n, r);
            if (!Zt_In(x, y, r[0], r[1] - 4.0f, r[2], r[3] + 4.0f)) {
                continue;
            }
            if (what[i] == 2) {
                Zt_PkChoose(-2);
                sZtPkOpen = -1;
            } else if (what[i] == 3) {
                Zt_Press(PADG_R1);
            } else {
                sZtPkMods = what[i];
                sZtPkPage = 0;
                sZtPkOpen = -1;
                Zt_PkChoose(what[i] ? MM_CHARA_COUNT : sZw->pickCur >= 0 && sZw->pickCur < MM_CHARA_COUNT ? sZw->pickCur : 0);
            }
            return;
        }
    } else if (sZw->pkMods > 0 && Zt_In(x, y, 1150.0f, 40.0f, 1522.0f, 136.0f)) {
        sZtPkMods ^= 1;
        sZtPkPage = 0;
        sZtPkOpen = -1;
        Zt_PkChoose(sZtPkMods ? MM_CHARA_COUNT : 0);
        return;
    }
    if (y >= 935.0f && x <= 1530.0f) {
        if (x <= 340.0f || x >= 1240.0f) {
            sZtPkOpen = -1; /* (another page: no forms in the way) */
        }
        if (x <= 340.0f) {
            sZtPkPage = (sZtPkPage + Zt_PkPages() - 1) % Zt_PkPages();
            Snd_PlaySe(1, 0);
        } else if (x >= 1240.0f) {
            sZtPkPage = (sZtPkPage + 1) % Zt_PkPages();
            Snd_PlaySe(1, 0);
        }
        return;
    }
    count = sZtPkMods ? sZw->pkMods : ZT_FAMILIES;
    for (i = 0; i < ZT_PER_PAGE && sZtPkPage * ZT_PER_PAGE + i < count; i++) {
        s32 k = sZtPkPage * ZT_PER_PAGE + i;

        Zt_PkCard(i, r);
        if (Zt_In(x, y, r[0], r[1], r[2], r[3])) {
            if (sZtPkMods) {
                Zt_PkChoose(MM_CHARA_COUNT + k);
            } else {
                s32 j, members = 0;

                if (fam != k) {
                    Zt_PkChoose(Zt_FamilyFirst(k));
                }
                for (j = 0; j < MM_CHARA_COUNT; j++) {
                    members += kZtFamilyOf[j] == k;
                }
                sZtPkOpen = members > 1 && sZtPkOpen != k ? k : -1; /* (again: closed) */
            }
            return;
        }
    }
    if (!sZtPkMods && fam >= 0 && fam == sZtPkOpen && x <= 1530.0f && y >= Zt_PkFormsY() + 50.0f && y <= Zt_PkFormsY() + 240.0f) {
        s32 at, k = 0;
        f32 w;

        for (n = 0, i = 0; i < MM_CHARA_COUNT; i++) {
            n += kZtFamilyOf[i] == fam;
        }
        w = 1442.0f / (f32)(n < 9 ? 9 : n);
        at = (s32)((x - 70.0f) / w);
        for (i = 0; x >= 70.0f && i < MM_CHARA_COUNT; i++) {
            if (kZtFamilyOf[i] == fam && k++ == at) {
                Zt_PkChoose(i);
                return;
            }
        }
        return;
    }
    for (i = 0; i < slots; i++) {
        f32 top = 380.0f + (f32)i * 180.0f;

        if (Zt_In(x, y, 1590.0f, top, 2310.0f, top + 165.0f)) {
            sZw->pickSlot = i;
            Snd_PlaySe(1, 0);
            return;
        }
    }
}

/* ---- the questions (template 02) ---- */
static void Zt_AskButton(s32 k, f32 *x0, f32 *x1) { /* (the action at the right, as the template) */
    s32 n = Ask_Count();
    s32 place = n - 1 - k;
    f32 w = n == 3 ? 320.0f : 480.0f;

    *x0 = 690.0f + (f32)place * (w + (n == 3 ? 30.0f : 60.0f));
    *x1 = *x0 + w;
}

static void Zt_AskDraw(void) {
    PortWarrior *v = sZw->cur > 0 ? &sZw->list[sZw->cur - 1] : NULL;
    s32 a = (s32)(128.0f * Zw_In(sZw->askT, 10));
    s32 n = Ask_Count(), k;
    char title[160];
    char *line1, *line2, *btn[3] = {"", "", ""};

    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(0, 0, 0, a * 75 / 128), Mx_Rgba(0, 0, 0, a * 75 / 128));
    switch (sZw->ask) {
    case ASK_REMOVE:
        sprintf(title, "Remover \"%.40s\"?", v != NULL ? (v->name[0] ? v->name : v->folder) : "");
        line1 = "A pasta vai para mods_removed.";
        line2 = "Nada \303\251 apagado.";
        btn[0] = "REMOVER";
        btn[1] = "CANCELAR";
        break;
    case ASK_COSON:
        sprintf(title, "Editar o Traje 2?");
        line1 = "O que voc\303\252 tem agora vira o Traje 1.";
        line2 = "O Traje 2 come\303\247a como uma c\303\263pia.";
        btn[0] = "COME\303\207AR";
        btn[1] = "CANCELAR";
        break;
    case ASK_COSOFF:
        sprintf(title, "Desligar os trajes?");
        line1 = "Os trajes 2 a 4 saem ao salvar.";
        line2 = "O Traje 1 fica.";
        btn[0] = "DESLIGAR";
        btn[1] = "CANCELAR";
        break;
    case ASK_WSDEL:
        sprintf(title, "Tirar da Oficina?");
        line1 = "Ele sai da Oficina.";
        line2 = "O seu guerreiro continua com voc\303\252.";
        btn[0] = "TIRAR";
        btn[1] = "CANCELAR";
        break;
    default:
        sprintf(title, "Sair sem salvar?");
        line1 = "H\303\241 mudan\303\247as que n\303\243o foram salvas.";
        line2 = "";
        btn[0] = "SALVAR";
        btn[1] = "N\303\203O SALVAR";
        btn[2] = "CANCELAR";
        break;
    }
    Zt_Box(ZW_T_ASK, 620.0f, 300.0f, 1780.0f, 790.0f, 34.0f, a);
    Zt_Text(1200.0f, 390.0f, title, 52.0f, 1100.0f, ZT_GOLD, 1, a);
    Zt_Text(1200.0f, 480.0f, line1, 36.0f, 1080.0f, ZT_WHITE, 1, a);
    Zt_Text(1200.0f, 535.0f, line2, 36.0f, 1080.0f, ZT_WHITE, 1, a);
    for (k = 0; k < n; k++) {
        s32 id = k == 0 ? (sZw->ask == ASK_REMOVE || sZw->ask == ASK_WSDEL || sZw->ask == ASK_COSOFF ? ZW_T_RED : ZW_T_GREEN)
                 : k == 1 && n == 3 ? ZW_T_RED : ZW_T_GREY;
        f32 x0, x1;

        Zt_AskButton(k, &x0, &x1);
        Zt_Pill(id, x0, 630.0f, x1, 740.0f, btn[k], 40.0f, a);
        if (k == sZw->askCur) {
            Mx_Bar(ZW_T_RING, ZT_X(x0 - 7.0f), ZT_Y(623.0f), ZT_X(x1 + 7.0f), ZT_Y(747.0f), a);
        }
    }
}

static void Zt_AskTap(f32 x, f32 y) {
    s32 k;

    for (k = 0; k < Ask_Count() && sZw->askT > 8; k++) {
        f32 x0, x1;

        Zt_AskButton(k, &x0, &x1);
        if (Zt_In(x, y, x0, 620.0f, x1, 750.0f)) {
            sZw->askCur = k;
            Zt_Press(PADG_CROSS);
            return;
        }
    }
}
