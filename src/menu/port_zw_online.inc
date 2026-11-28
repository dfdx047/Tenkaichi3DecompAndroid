/* Z Warriors online (the approved templates, 2026-11-27; mods-zwarriors/online_zw2.py): the screens, their animations
   and the warriors saved on this phone. NOTHING CONNECTS YET: there is no queue, no room and no warrior sent; the
   screens say so (the "preview" strip) and walk from one to the other so the design can be judged on a phone. Included
   by port_zw.c after port_zw_touch.inc; positions are the templates' pixels (Zt_*). */
#include "../../include/menu/port_zw_net.h"
static ZwNetInfo sZoNet;
static PortWarrior sZoPeer;
static int sZoHasPeer, sZoGen;

enum { ZO_MENU, ZO_QUICK, ZO_SEARCH, ZO_FOUND, ZO_CREATE, ZO_CODE, ZO_ROOM, ZO_STAGES, ZO_MOVES, ZO_PREPARE, ZO_RESULT, ZO_LOST, ZO_VERSION };

static struct {
    s32 screen, from, t, sel, ready, host, friendReady;
    s32 focus;           /* the button the arrows are on (its hit id) */
    s32 swap, swapFrom;  /* 1: choosing another warrior (left / right); the one before */
    s32 pref[5], rule[6], stage, stagePick, stagePage, movesMine, codeLen, rounds;
    char code[8];
} sZo;

#define L(pt, en, es) Zo_L(pt, en, es)
static char *Zo_L(char *pt, char *en, char *es) {
    extern int Port_TagLang(void); /* 0 English, 1 Portuguese, 2 Spanish (the language of the port's own screens) */
    s32 l = Port_TagLang();

    return l == 1 ? pt : l == 2 ? es : en;
}

/* ---- what a finger can press: each draw lists its boxes, the next tap is looked up in them ---- */
typedef struct ZoHit {
    f32 x0, y0, x1, y1;
    s32 id;
} ZoHit;
static ZoHit sZoHit[64];
static s32 sZoHits;
enum { H_BACK = 1, H_LANG0, H_LANG1, H_LANG2, H_QUICK, H_CREATE, H_ENTER, H_HOW, H_TEST, H_REPORT, H_PREV, H_NEXT, H_GO, H_MOVES,
       H_CANCEL, H_PRACTICE, H_REFUSE, H_ACCEPT, H_JOIN, H_ERASE, H_PASTE, H_COPY, H_READY, H_START, H_SWAP, H_LEAVE, H_ROOM,
       H_REMATCH, H_CONFIRM, H_PGPREV, H_PGNEXT, H_DEMO, H_HEADBACK, H_PICKED, H_PREF = 40, H_RULE = 48, H_KEY = 64, H_STAGE = 128 };

static void Zo_Hit(s32 id, f32 x0, f32 y0, f32 x1, f32 y1) {
    if (sZoHits < 64) {
        ZoHit *h = &sZoHit[sZoHits++];

        h->x0 = x0, h->y0 = y0, h->x1 = x1, h->y1 = y1, h->id = id;
    }
}

static void Zo_Go(s32 screen) {
    {
        extern void Mm_ViewPairOff(void); /* menu/port_modmgr.c: the room's second model does not stay */

        Mm_ViewPairOff();
    }
    sZo.from = sZo.screen;
    sZo.screen = screen;
    sZo.t = 0;
    sZo.focus = 0;
    sZo.swap = 0;
    sZw->rest = 0;
}

static s32 Zo_Alpha(void) {
    return (s32)(128.0f * Zw_In(sZo.t, 12));
}

static f32 Zo_Slide(s32 order) { /* what is still to come in, in the template's pixels (each a little after the one before) */
    return 70.0f * (1.0f - Zw_In(sZo.t - order * 2, 14));
}

static void Zo_Button(s32 hit, s32 art, f32 x0, f32 y0, f32 x1, f32 y1, char *s, f32 size) {
    Zt_Pill(art, x0, y0, x1, y1, s, size, Zo_Alpha());
    if (hit) {
        Zo_Hit(hit, x0, y0, x1, y1);
    }
}

/* The "play online" button: its glow breathes and a shine crosses it now and then. */
static void Zo_BigButton(f32 x0, f32 y0, f32 x1, f32 y1, char *title, char *sub, s32 alpha, s32 lit) {
    f32 h = y1 - y0, w = x1 - x0, pad = h * 30.0f / 116.0f, beat = Zw_Beat(80), grow = 4.0f * Zw_Beat(80);
    s32 cross = sZw->frames % 150;

    if (!lit) { /* not the chosen one: dark and still, as the main menu's icons */
        Mx_PicEx(ZW_T_ONLINE, ZT_X(x0 - w * 30.0f / 700.0f), ZT_Y(y0 - pad), ZT_X(x1 + w * 30.0f / 700.0f), ZT_Y(y1 + pad), 0.0f, 0.0f, 1.0f, 1.0f,
                 Mx_Rgba(52, 52, 70, 0x80), alpha);
        Zt_Text(x0 + h + (w - h - h * 0.5f) * 0.5f, y0 + h * 0.38f, title, h * 0.40f, w - h * 1.7f, ZT_C(140, 144, 170), 1, alpha);
        Zt_Text(x0 + h + (w - h - h * 0.5f) * 0.5f, y0 + h * 0.75f, sub, h * 0.21f, w - h * 1.7f, ZT_C(130, 120, 100), 1, alpha);
        return;
    }
    Mx_Pic(ZW_T_GLOW, ZT_X(x0 - pad - grow), ZT_Y(y0 - pad - grow), ZT_X(x1 + pad + grow), ZT_Y(y1 + pad + grow),
           (s32)((f32)alpha * (0.55f + 0.35f * beat)));
    Mx_Pic(ZW_T_ONLINE, ZT_X(x0 - w * 30.0f / 700.0f), ZT_Y(y0 - pad), ZT_X(x1 + w * 30.0f / 700.0f), ZT_Y(y1 + pad), alpha);
    if (cross < 45) {
        f32 x = x0 + h * 0.9f + (w - h * 1.9f) * (f32)cross / 45.0f;

        Mx_Pic(ZW_T_SHINE, ZT_X(x - h * 0.45f), ZT_Y(y0 + h * 0.1f), ZT_X(x + h * 0.45f), ZT_Y(y1 - h * 0.1f), (s32)((f32)alpha * 0.8f));
    }
    Zt_Text(x0 + h + (w - h - h * 0.5f) * 0.5f, y0 + h * 0.38f, title, h * 0.40f, w - h * 1.7f, ZT_WHITE, 1, alpha);
    Zt_Text(x0 + h + (w - h - h * 0.5f) * 0.5f, y0 + h * 0.75f, sub, h * 0.21f, w - h * 1.7f, ZT_GOLD, 1, alpha);
}

static void Zo_Header(char *title, char *sub, s32 lang) {
    s32 a = Zo_Alpha(), i;
    static char *kLang[3] = {"PT", "EN", "ES"};
    static const s32 kId[3] = {1, 0, 2};
    extern int Port_TagLang(void);

    Mx_Fit(ZW_T_BACK, ZT_X(116.0f), ZT_Y(86.0f), ZT_S(112.0f), a);
    Zo_Hit(H_HEADBACK, 40.0f, 20.0f, 195.0f, 160.0f);
    Zt_Text(205.0f, 64.0f, title, 54.0f, 880.0f, ZT_GOLD, 0, a);
    Zt_Text(205.0f, 120.0f, sub, 29.0f, 880.0f, ZT_DIM, 0, a);
    if (lang) {
        Zt_Box(ZW_T_PANEL, 1888.0f, 36.0f, 2358.0f, 134.0f, 49.0f, a);
        for (i = 0; i < 3; i++) {
            Zo_Button(H_LANG0 + i, Port_TagLang() == kId[i] ? ZW_T_ORANGE : ZW_T_NAVY, 1900.0f + i * 154.0f, 48.0f, 2042.0f + i * 154.0f,
                      122.0f, kLang[i], 34.0f);
        }
    }
}

static void Zo_Strip(void) { /* (said on every screen) */
    Zt_Box(ZW_T_RED, 700.0f, 2.0f, 1700.0f, 38.0f, 16.0f, 0x70);
    Zt_Text(1200.0f, 20.0f, L("BETA: online em teste, nem toda rede conecta", "BETA: online in testing, not every network connects",
                            "BETA: online en pruebas, no toda red conecta"), 24.0f, 970.0f, ZT_WHITE, 1, 0x80);
}

static void Zo_Bar(s32 n, const s32 *hit, const s32 *art, char **label) {
    s32 i;
    f32 w = (2280.0f - 18.0f * (f32)(n - 1)) / (f32)n;

    for (i = 0; i < n; i++) {
        f32 x = 60.0f + (w + 18.0f) * (f32)i, y = 940.0f + Zo_Slide(4 + i);

        Zo_Button(hit[i], art[i], x, y, x + w, y + 104.0f, label[i], 34.0f);
    }
}

/* ---- the warriors saved here ---- */
static PortWarrior *Zo_Mine(void) {
    return sZw->count > 0 ? &sZw->list[sZo.sel % sZw->count] : NULL;
}

static PortWarrior *Zo_Friend(void) { /* the other player's, once it has come and been made here */
    return sZoHasPeer ? &sZoPeer : NULL;
}

static char *Zo_Name(PortWarrior *v) {
    return v == NULL ? L("(nenhum guerreiro)", "(no warrior)", "(sin guerrero)") : v->name[0] ? v->name : v->folder;
}

static char *Zo_Info(PortWarrior *v) {
    static char out[2][160];
    static s32 n;
    char *s = out[n++ & 1];

    if (v == NULL) {
        return L("Crie um no Z Warriors", "Make one in Z Warriors", "Crea uno en Z Warriors");
    }
    snprintf(s, 160, "%s: %s  -  %s: %s", L("Corpo", "Body", "Cuerpo"), Zw_Name(v->body), L("golpes", "moves", "golpes"), Zw_Name(v->moves));
    return s;
}

/* A side's panel: its warrior (the one on this phone's side in 3D: the game has one viewer), its name and badge. */
static void Zo_Side(f32 x0, f32 x1, char *title, PortWarrior *v, s32 model, s32 arrows, char *badge, s32 badgeArt, s32 badgeHit) {
    s32 a = Zo_Alpha();
    f32 cx = (x0 + x1) * 0.5f, dy = Zo_Slide(1);

    Zt_Box(badge != NULL && badgeArt == ZW_T_GREEN ? ZW_T_SEL : ZW_T_PANEL, x0, 170.0f + dy, x1, 925.0f + dy, 34.0f, a);
    Zt_Text(cx, 215.0f + dy, title, 36.0f, x1 - x0 - 40.0f, ZT_GOLD, 1, a);
    if (model && v != NULL && v->mod >= 0) {
        PortModInfo info;

        Port_ModGetInfo(v->mod, &info);
        Zw_Stage(0x20000 + v->mod * 64 + (sZw->viewGen & 63), info.chara, v->mod, ZT_X(cx), ZT_Y(730.0f + dy), ZT_Y(450.0f), 0);
    } else if (model && v != NULL) {
        Zw_Stage(0x30000 + v->body, v->body, -1, ZT_X(cx), ZT_Y(730.0f + dy), ZT_Y(450.0f), 0);
    } else {
        Mx_Fit(ZW_PLATFORM, ZT_X(cx), ZT_Y(700.0f + dy), ZT_Y(180.0f), a);
        if (v != NULL && model != 2) { /* (2: one of the room's pair, drawn by Zo_Pair) */
            Zw_Chip(Lib_Face(v), ZT_X(cx), ZT_Y(470.0f + dy + 8.0f * Zw_Beat(120)), ZT_S(300.0f), 0);
        }
    }
    Zt_Text(cx, 792.0f + dy, Zo_Name(v), 42.0f, x1 - x0 - 40.0f, ZT_WHITE, 1, a);
    Zt_Text(cx, 836.0f + dy, Zo_Info(v), 25.0f, x1 - x0 - 40.0f, ZT_DIM, 1, a);
    if (sZo.swap && v != NULL && x0 < 100.0f) { /* choosing: the arrows and the confirm */
        Mx_Frame(ZT_X(x0), ZT_Y(170.0f + dy), ZT_X(x1), ZT_Y(925.0f + dy), ZT_S(7.0f + 3.0f * Zw_Beat(40)), Mx_Rgba(255, 206, 70, 0x80));
        Zo_Button(H_PREV, ZW_T_ORANGE, x0 + 30.0f, 850.0f + dy, x0 + 170.0f, 918.0f + dy, "<", 48.0f);
        Zo_Button(H_NEXT, ZW_T_ORANGE, x1 - 170.0f, 850.0f + dy, x1 - 30.0f, 918.0f + dy, ">", 48.0f);
        Zo_Button(H_PICKED, ZW_T_GREEN, cx - 210.0f, 856.0f + dy, cx + 210.0f, 914.0f + dy, L("X  CONFIRMAR", "X  CONFIRM", "X  CONFIRMAR"), 30.0f);
    } else if (arrows) {
        Zo_Button(H_SWAP, ZW_T_BAR, cx - 220.0f, 856.0f + dy, cx + 220.0f, 916.0f + dy, L("Trocar guerreiro", "Change warrior", "Cambiar guerrero"), 30.0f);
    } else if (badge != NULL) {
        f32 g = badgeArt == ZW_T_GREEN ? 3.0f * Zw_Beat(50) : 0.0f;

        Zo_Button(badgeHit, badgeArt, cx - 180.0f - g, 862.0f + dy - g, cx + 180.0f + g, 914.0f + dy + g, badge, 28.0f);
    }
}

static void Zo_Row(s32 hit, f32 x0, f32 y, f32 x1, char *name, char *value, s32 sel, f32 h, s32 order) {
    s32 a = Zo_Alpha();
    f32 dx = Zo_Slide(order);

    Zt_Box(sel ? ZW_T_SEL : ZW_T_PANEL, x0 + dx, y, x1 + dx, y + h, 28.0f, a);
    Zt_Text(x0 + 36.0f + dx, y + h * 0.5f, name, 33.0f, (x1 - x0) * 0.46f, ZT_WHITE, 0, a);
    Zt_Text(x1 - 80.0f + dx, y + h * 0.5f, value, 31.0f, (x1 - x0) * 0.44f, ZT_GOLD, 2, a);
    if (hit) {
        Zt_Chev(x1 - 48.0f + dx, y + h * 0.5f, 34.0f);
        Zo_Hit(hit, x0, y, x1, y + h);
    }
}

static void Zo_Card(s32 hit, f32 y0, f32 y1, char *title, char *l1, char *l2, char *btn, s32 art, s32 hot, s32 order) {
    s32 a = Zo_Alpha();
    f32 h = y1 - y0, dx = Zo_Slide(order);

    Zt_Box(hot ? ZW_T_SEL : ZW_T_PANEL, 900.0f + dx, y0, 2340.0f + dx, y1, 34.0f, a);
    Zt_Text(950.0f + dx, y0 + h * 0.27f, title, 48.0f, 860.0f, ZT_GOLD, 0, a);
    Zt_Text(950.0f + dx, y0 + h * 0.55f, l1, 30.0f, 880.0f, ZT_WHITE, 0, a);
    Zt_Text(950.0f + dx, y0 + h * 0.78f, l2, 27.0f, 880.0f, ZT_DIM, 0, a);
    Zo_Button(0, art, 1860.0f + dx, y0 + h * 0.5f - 58.0f, 2290.0f + dx, y0 + h * 0.5f + 58.0f, btn, 40.0f);
    Zo_Hit(hit, 900.0f, y0, 2340.0f, y1);
}

/* ---- the choices (kept while the screens are open) ---- */
static char *Zo_Pref(s32 k, s32 v) {
    switch (k) {
    case 0: return v ? L("S\303\263 lutadores do jogo", "Game fighters only", "Solo luchadores del juego") : L("Guerreiros e lutadores", "Warriors and fighters", "Guerreros y luchadores");
    case 1: return v == 0 ? L("Boa ou \303\263tima", "Good or great", "Buena u \303\263ptima") : v == 1 ? L("S\303\263 \303\263tima", "Great only", "Solo \303\263ptima") : L("Qualquer", "Any", "Cualquiera");
    case 2: return v == 0 ? L("Autom\303\241tica", "Automatic", "Autom\303\241tica") : v == 1 ? L("Am\303\251rica do Sul", "South America", "Sudam\303\251rica") : L("Qualquer", "Any", "Cualquiera");
    case 3: return v ? L("O de cada um", "Each one's pick", "El de cada uno") : L("Sorteado", "Random", "Al azar");
    default: return v == 0 ? "180 s" : v == 1 ? "240 s" : L("Sem limite", "No limit", "Sin l\303\255mite");
    }
}
static const s32 kZoPrefN[5] = {2, 3, 3, 2, 3};

/* The stages are the game's: the small pictures of the Duel's stage select (archive 5, section 2, sub 46), the big one
   of the chosen stage (TS_STAGE_FILE + id, eight 128 x 256 pieces) and the names (TagNet_MenuName), as the Tag Team
   online screen has them (menu/tag_online_menu.c). Held only while these screens are open. */
#define ZO_PER_PAGE 9
#define ZO_STAGE_FILE 0x39D /* + stage id: its compressed picture (menu_e.h TS_STAGE_FILE) */
extern void TagNet_MenuName(s32 kind, s32 id, u32 out, s32 size);
extern s32 PortGame_StageCount(void);
extern void *gMenuArc5;
static void *sZoSelArt, *sZoBigData;
static u8 *sZoChips;
static MTexRes *sZoBig;
static s32 sZoBigId = -1, sZoOwnArc, sZoStages[64], sZoStageN;

void *sZoSelArtPtr(void) { return sZoSelArt; } /* (the fighter picker reads the grid's table from it) */
static void Zo_StagesOpen(void) {
    s32 i;

    if (sZoSelArt != NULL) {
        return;
    }
    sZoOwnArc = gMenuArc5 == NULL;
    if (sZoOwnArc) {
        gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
    }
    if (gMenuArc5 == NULL) {
        return;
    }
    sZoSelArt = Sprite_Unpack(MPACK_AT(gMenuArc5, 2), NULL, NULL);
    sZoChips = (u8 *)MPACK_AT(sZoSelArt, 46);
    sZoStageN = 0;
    for (i = 0; i < PortGame_StageCount() && sZoStageN < 63; i++) {
        if (i != 35) { /* (not a stage) */
            MTexRes *pic = (MTexRes *)MPACK_AT(sZoChips, i + 1);

            Res_RelocateOffsets((void *)&pic, pic, pic);
            sZoStages[sZoStageN++] = i;
        }
    }
    sZoStages[sZoStageN++] = -1; /* a random one */
}

static void Zo_StagesClose(void) {
    sceGsSyncPath(0, 0);
    if (sZoBigData != NULL) {
        Heap_Free(sZoBigData);
    }
    if (sZoSelArt != NULL) {
        Heap_Free(sZoSelArt);
    }
    if (sZoOwnArc && gMenuArc5 != NULL) {
        Heap_Free(gMenuArc5);
        gMenuArc5 = NULL;
    }
    sZoBigData = sZoSelArt = NULL;
    sZoChips = NULL;
    sZoBig = NULL;
    sZoBigId = -1;
    sZoOwnArc = 0;
}

static char *Zo_StageName(s32 id) {
    static char out[2][96];
    static s32 n;
    char *s = out[n++ & 1];

    if (id < 0) {
        return L("Sorteado", "Random", "Al azar");
    }
    s[0] = 0;
    TagNet_MenuName(1, id, (u32)s, 96);
    return s;
}

static void Zo_StageBig(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 a) {
    s32 col, row;
    f32 w = x1 - x0, h = y1 - y0;

    if (id != sZoBigId) {
        void *file;

        sceGsSyncPath(0, 0);
        if (sZoBigData != NULL) {
            Heap_Free(sZoBigData);
        }
        sZoBigData = NULL;
        sZoBig = NULL;
        sZoBigId = id;
        if (id >= 0 && (file = File_LoadSync(ZO_STAGE_FILE + id, NULL, 0)) != NULL) {
            sZoBigData = Sprite_Unpack(file, NULL, NULL);
            Heap_Free(file);
            sZoBig = (MTexRes *)sZoBigData;
            Res_RelocateOffsets((void *)&sZoBig, sZoBig, sZoBig);
        }
    }
    if (sZoBig == NULL) {
        Zt_Text((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, "?", 220.0f + 12.0f * Zw_Beat(60), w, ZT_GOLD, 1, a);
        return;
    }
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 4; col++) {
            MmTex *t = (MmTex *)(sZoBig->tex + (col + row * 4) * 0x40);

            Mx_Tex(t, ZT_X(x0 + w * col / 4), ZT_Y(y0 + h * row / 2), ZT_X(x0 + w * (col + 1) / 4), ZT_Y(y0 + h * (row + 1) / 2), 0, 0, 128, 256, 128, a);
        }
    }
}

static void Zo_StageChip(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, s32 a) {
    MTexRes *pic;
    u64 tex;

    if (id < 0 || sZoChips == NULL) {
        Zt_Text((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, "?", 90.0f, x1 - x0, ZT_GOLD, 1, a);
        return;
    }
    pic = (MTexRes *)MPACK_AT(sZoChips, id + 1);
    tex = *(u64 *)(pic->tex + 0x30);
    Mx_Tex((MmTex *)pic->tex, ZT_X(x0), ZT_Y(y0), ZT_X(x1), ZT_Y(y1), 0, 0, 1 << ((tex >> 26) & 15), 1 << ((tex >> 30) & 15), 128, a);
}

static char *Zo_Rule(s32 k, s32 v) {
    switch (k) {
    case 0: return Zo_StageName(sZo.stage);
    case 1: return v == 0 ? "180 s" : v == 1 ? "240 s" : L("Sem limite", "No limit", "Sin l\303\255mite");
    case 2: return v ? L("Ligados", "On", "Activados") : L("Desligados", "Off", "Desactivados");
    case 3: return v == 0 ? L("Normal", "Normal", "Normal") : v == 1 ? "x1,5" : "x2";
    case 4: return v ? L("S\303\263 convidados", "Invited only", "Solo invitados") : L("Quem tiver o c\303\263digo", "Anyone with the code", "Quien tenga el c\303\263digo");
    default: return v ? L("S\303\263 lutadores", "Fighters only", "Solo luchadores") : L("Criados e lutadores", "Warriors and fighters", "Creados y luchadores");
    }
}
static const s32 kZoRuleN[6] = {1, 3, 2, 3, 2, 2};

static void Zo_NewCode(void) {
    static const char abc[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    s32 i, k = sZw->frames * 2654435761u >> 3;

    for (i = 0; i < 6; i++) {
        k = k * 1103515245 + 12345;
        sZo.code[i] = abc[((u32)k >> 16) % 32];
    }
    sZo.code[6] = 0;
    sZo.codeLen = 6;
}

static void Zo_Progress(f32 y, char *name, f32 v, s32 a) {
    static char pc[4][8];
    static s32 n;
    char *s = pc[n++ & 3];

    v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    Zt_Text(590.0f, y, name, 32.0f, 350.0f, ZT_WHITE, 0, a);
    Zt_Box(ZW_T_NAVY, 960.0f, y - 18.0f, 1600.0f, y + 18.0f, 18.0f, a);
    if (v > 0.03f) {
        Zt_Box(v >= 1.0f ? ZW_T_GREEN : ZW_T_ORANGE, 960.0f, y - 18.0f, 960.0f + 640.0f * v, y + 18.0f, 18.0f, a);
    }
    if (v >= 1.0f) {
        Zt_Text(1640.0f, y, L("pronto", "ready", "listo"), 28.0f, 190.0f, ZT_GREEN, 0, a);
    } else {
        snprintf(s, 8, "%d%%", (s32)(v * 100.0f));
        Zt_Text(1640.0f, y, s, 28.0f, 190.0f, ZT_DIM, 0, a);
    }
}

static void Zo_Overlay(void) {
    u32 c = Mx_Rgba(4, 6, 26, (s32)(96.0f * Zw_In(sZo.t, 10)));

    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, c, c);
    sZoHits = 0; /* (only the box answers) */
}

/* ---- the arrows: from the button they are on to the nearest one that way ---- */
static s32 Zo_Focusable(s32 id) {
    return id != H_DEMO && id != H_HEADBACK && id != H_PREV && id != H_NEXT && id != H_PICKED;
}

static const ZoHit *Zo_Find(s32 id) {
    s32 i;

    for (i = 0; id != 0 && i < sZoHits; i++) {
        if (sZoHit[i].id == id) {
            return &sZoHit[i];
        }
    }
    return NULL;
}

static void Zo_FocusDraw(void) {
    static const s32 kFirst[13] = {H_QUICK, H_GO, H_CANCEL, H_ACCEPT, H_GO, H_KEY + 1, H_READY, H_CONFIRM, H_BACK, 0, H_REMATCH, H_ROOM, H_ROOM};
    const ZoHit *h = Zo_Find(sZo.focus);
    s32 i;

    if (h == NULL) { /* (a new screen, or what it was on is gone) */
        sZo.focus = Zo_Find(kFirst[sZo.screen]) != NULL ? kFirst[sZo.screen] : 0;
        for (i = 0; sZo.focus == 0 && i < sZoHits; i++) {
            if (Zo_Focusable(sZoHit[i].id)) {
                sZo.focus = sZoHit[i].id;
            }
        }
        h = Zo_Find(sZo.focus);
    }
    if (h != NULL && !sZo.swap) {
        f32 g = 5.0f + 4.0f * Zw_Beat(40);

        Mx_Frame(ZT_X(h->x0 - g), ZT_Y(h->y0 - g), ZT_X(h->x1 + g), ZT_Y(h->y1 + g), ZT_S(7.0f), Mx_Rgba(255, 214, 80, 0x80));
    }
}

static void Zo_Move(s32 dx, s32 dy) {
    const ZoHit *from = Zo_Find(sZo.focus);
    f32 best = 1.0e9f;
    s32 i, to = 0;

    if (from == NULL) {
        return;
    }
    for (i = 0; i < sZoHits; i++) {
        const ZoHit *h = &sZoHit[i];
        f32 ax = (h->x0 + h->x1 - from->x0 - from->x1) * 0.5f, ay = (h->y0 + h->y1 - from->y0 - from->y1) * 0.5f;
        f32 along = (f32)dx * ax + (f32)dy * ay, off, gap, score;

        if (!Zo_Focusable(h->id) || h->id == from->id || along < 20.0f) {
            continue;
        }
        off = dx ? ay : ax;
        off = off < 0.0f ? -off : off;
        gap = dx ? (h->y0 > from->y1 ? h->y0 - from->y1 : from->y0 > h->y1 ? from->y0 - h->y1 : 0.0f)
                 : (h->x0 > from->x1 ? h->x0 - from->x1 : from->x0 > h->x1 ? from->x0 - h->x1 : 0.0f);
        score = along + gap * 3.0f + off * 0.35f;
        if (score < best) {
            best = score;
            to = h->id;
        }
    }
    if (to != 0) {
        sZo.focus = to;
        Snd_PlaySe(1, 0);
    }
}

/* The room's two warriors in 3D, facing each other: the viewer's model and one beside it (port_modmgr.c). */
static void Zo_PairOne(PortWarrior *v, s32 *key, s32 *chara, s32 *mod) {
    if (v->mod >= 0) {
        PortModInfo info;

        Port_ModGetInfo(v->mod, &info);
        *key = 0x20000 + v->mod * 64 + (sZw->viewGen & 63);
        *chara = info.chara;
        *mod = v->mod;
    } else {
        *key = 0x30000 + v->body;
        *chara = v->body;
        *mod = -1;
        if (v->body >= ZW_MODREF) {
            extern int Port_ModRefMod(int who);

            *mod = Port_ModRefMod(v->body);
            *chara = Port_ModRefChara(v->body);
        }
    }
}

/* The turn that faces the screen for a model drawn at template x (port_modmgr.c Mm_PlaceModel: its place across over
   its distance is (x - 0.492) / 0.76; the camera looks along the middle). */
static f32 Zo_Front(f32 x) {
    f32 r = (x / 2400.0f - 0.492f) / 0.76f;

    return 3.1415927f + r * (1.0f - 0.2f * r * r); /* (atan, near enough for these few degrees) */
}

static void Zo_Pair(PortWarrior *mine, PortWarrior *theirs) {
    s32 k0, c0, m0, k1, c1, m1;
    f32 left[3] = {ZT_X(450.0f), ZT_Y(730.0f), ZT_Y(450.0f)}, right[3] = {ZT_X(1950.0f), ZT_Y(730.0f), ZT_Y(450.0f)};

    Zo_PairOne(mine, &k0, &c0, &m0);
    Zo_PairOne(theirs, &k1, &c1, &m1);
    sZw->rest++;
    Mm_ViewPair(k0, c0, 0, m0, k1, c1, 0, m1, sZw->rest > 4);
    Mm_ViewPairDraw(left, Zo_Front(450.0f), right, Zo_Front(1950.0f));
}

/* ---- the screens ---- */
static void Zo_RoomDraw(void) {
    s32 a = Zo_Alpha(), k;
    static const s32 hit[4] = {H_LEAVE, H_SWAP, H_MOVES, H_START};
    s32 art[4] = {ZW_T_RED, ZW_T_BAR, ZW_T_BAR, ZW_T_GREY};
    char *label[4];
    s32 both = sZo.ready && sZo.friendReady;

    Zo_Header(L("SALA PRIVADA", "PRIVATE ROOM", "SALA PRIVADA"), L("Z Warriors Online  -  1 contra 1", "Z Warriors Online  -  1 vs 1", "Z Warriors Online  -  1 contra 1"), 0);
    Zt_Box(ZW_T_PANEL, 1480.0f, 40.0f, 2340.0f, 140.0f, 50.0f, a);
    Zt_Text(1520.0f, 90.0f, L("C\303\223DIGO", "CODE", "C\303\223DIGO"), 30.0f, 160.0f, ZT_DIM, 0, a);
    Zt_Text(1700.0f, 90.0f, sZo.code, 52.0f, 340.0f, ZT_GOLD, 0, a);
    Zo_Button(H_COPY, ZW_T_BAR, 2060.0f, 50.0f, 2325.0f, 130.0f, L("Copiar", "Copy", "Copiar"), 32.0f);
    Zo_Side(60.0f, 840.0f, sZo.host ? L("VOC\303\212 (anfitri\303\243o)", "YOU (host)", "T\303\232 (anfitri\303\263n)") : L("VOC\303\212", "YOU", "T\303\232"), Zo_Mine(), Zo_Friend() != NULL ? 2 : 1, 0,
            sZo.ready ? L("PRONTO", "READY", "LISTO") : L("TOQUE: PRONTO", "TAP WHEN READY", "TOCA: LISTO"), sZo.ready ? ZW_T_GREEN : ZW_T_ORANGE, H_READY);
    Zo_Side(1560.0f, 2340.0f, L("AMIGO", "FRIEND", "AMIGO"), Zo_Friend(), 2, 0,
            !sZoNet.peerHere ? NULL : sZo.friendReady ? L("PRONTO", "READY", "LISTO") : L("ESCOLHENDO...", "CHOOSING...", "ELIGIENDO..."), sZo.friendReady ? ZW_T_GREEN : ZW_T_GREY, 0);
    if (sZo.screen == ZO_ROOM && Zo_Mine() != NULL && Zo_Friend() != NULL) {
        Zo_Pair(Zo_Mine(), Zo_Friend());
    }
    {
        f32 s = 120.0f + 8.0f * Zw_Beat(60);

        Zt_Text(1200.0f, 235.0f, "VS", s, 500.0f, ZT_GOLD, 1, a);
    }
    Zt_Box(ZW_T_STAGE, 870.0f, 320.0f, 1530.0f, 575.0f, 34.0f, a);
    if (sZoNet.state == ZN_CONNECTING) {
        Zt_Text(1200.0f, 447.0f, L("Conectando ao servidor...", "Connecting...", "Conectando..."), 34.0f, 610.0f, ZT_GOLD, 1, a);
    } else if (!sZoNet.peerHere) {
        Zt_Text(1200.0f, 410.0f, L("Esperando o amigo entrar", "Waiting for your friend", "Esperando a tu amigo"), 34.0f, 610.0f, ZT_GOLD, 1, a);
        Zt_Text(1200.0f, 480.0f, L("Envie o c\303\263digo acima.", "Send them the code above.", "Env\303\255a el c\303\263digo."), 29.0f, 610.0f, ZT_WHITE, 1, a);
    } else {
        Zt_Text(910.0f, 390.0f, sZoHasPeer ? L("Guerreiro do amigo recebido", "Friend's warrior received", "Guerrero del amigo recibido")
                                : L("Recebendo o guerreiro...", "Receiving the warrior...", "Recibiendo el guerrero..."), 29.0f, 590.0f, ZT_GREEN, 0, a);
        Zt_Text(910.0f, 447.0f, L("Mesma vers\303\243o do online", "Same online version", "Misma versi\303\263n"), 29.0f, 590.0f, ZT_GREEN, 0, a);
        Zt_Text(910.0f, 504.0f, L("Mesmo disco do jogo", "Same game disc", "Mismo disco del juego"), 29.0f, 590.0f, ZT_GREEN, 0, a);
    }
    for (k = 0; k < 3; k++) {
        char *name = k == 0 ? L("Cen\303\241rio", "Stage", "Escenario") : k == 1 ? L("Tempo", "Time", "Tiempo") : L("Itens", "Items", "Objetos");

        Zo_Row(sZo.host ? H_RULE + k : 0, 870.0f, 598.0f + (f32)k * 112.0f, 1530.0f, name, Zo_Rule(k, sZo.rule[k]), sZo.host && sZo.focus == H_RULE + k, 96.0f, k);
    }
    art[3] = both ? ZW_T_GREEN : ZW_T_GREY;
    label[0] = L("Sair da sala", "Leave", "Salir");
    label[1] = L("Trocar guerreiro", "Change warrior", "Cambiar guerrero");
    label[2] = L("Ver golpes do amigo", "Friend's moves", "Golpes del amigo");
    label[3] = L("COME\303\207AR LUTA", "START FIGHT", "EMPEZAR");
    Zo_Bar(4, hit, art, label);
}

static void Zo_Draw(void) {
    s32 a = Zo_Alpha(), i, k;
    s32 hit[4], art[4];
    char *label[4];

    sZoHits = 0;
    Mx_Rect(0.0f, 0.0f, MX_W, MX_H, Mx_Rgba(10, 8, 40, 0x30), Mx_Rgba(10, 8, 40, 0x48)); /* (the world behind, dimmed) */
    switch (sZo.screen) {
    case ZO_MENU:
        Zo_Header("Z WARRIORS ONLINE", L("Escolha como quer lutar", "Choose how to fight", "Elige c\303\263mo luchar"), 1);
        Zo_Side(60.0f, 860.0f, L("SEU GUERREIRO", "YOUR WARRIOR", "TU GUERRERO"), Zo_Mine(), 1, 1, NULL, 0, 0);
        Zo_Card(H_QUICK, 170.0f, 410.0f, L("PARTIDA R\303\201PIDA", "QUICK MATCH", "PARTIDA R\303\201PIDA"),
                L("Procura um rival agora, sem c\303\263digo.", "Finds a rival now, no code.", "Busca un rival ahora, sin c\303\263digo."),
                L("S\303\263 encontra quem tamb\303\251m aceita guerreiros criados.", "Only meets who also accepts made warriors.", "Solo con quien acepta guerreros creados."),
                L("JOGAR", "PLAY", "JUGAR"), ZW_T_ORANGE, sZo.focus == H_QUICK, 0);
        Zo_Card(H_CREATE, 430.0f, 670.0f, L("CRIAR SALA", "CREATE ROOM", "CREAR SALA"),
                L("Voc\303\252 recebe um c\303\263digo para enviar ao amigo.", "You get a code to send your friend.", "Recibes un c\303\263digo para tu amigo."),
                L("Voc\303\252 escolhe o cen\303\241rio e as regras.", "You choose the stage and rules.", "T\303\272 eliges el escenario y las reglas."),
                L("CRIAR", "CREATE", "CREAR"), ZW_T_GREEN, sZo.focus == H_CREATE, 1);
        Zo_Card(H_ENTER, 690.0f, 925.0f, L("ENTRAR COM C\303\223DIGO", "ENTER A CODE", "ENTRAR CON C\303\223DIGO"),
                L("Digite o c\303\263digo que recebeu.", "Type the code you got.", "Escribe el c\303\263digo recibido."),
                L("Voc\303\252 v\303\252 o guerreiro do anfitri\303\243o antes da luta.", "You see the host's warrior before the fight.", "Ves el guerrero del anfitri\303\263n antes."),
                L("ENTRAR", "ENTER", "ENTRAR"), ZW_T_BAR, sZo.focus == H_ENTER, 2);
        hit[0] = H_BACK, hit[1] = H_HOW, hit[2] = H_TEST, hit[3] = H_REPORT;
        art[0] = ZW_T_RED, art[1] = art[2] = art[3] = ZW_T_BAR;
        label[0] = L("Voltar", "Back", "Volver"), label[1] = L("Como funciona", "How it works", "C\303\263mo funciona");
        label[2] = L("Testar conex\303\243o", "Test connection", "Probar conexi\303\263n"), label[3] = L("Relatar erro", "Report a bug", "Informar error");
        Zo_Bar(4, hit, art, label);
        break;
    case ZO_QUICK:
        Zo_Header(L("PARTIDA R\303\201PIDA", "QUICK MATCH", "PARTIDA R\303\201PIDA"), L("1 contra 1  -  sem pontos de ranque", "1 vs 1  -  no rank points", "1 contra 1  -  sin puntos de rango"), 1);
        Zo_Side(60.0f, 860.0f, L("SEU GUERREIRO", "YOUR WARRIOR", "TU GUERRERO"), Zo_Mine(), 1, 0, NULL, 0, 0);
        for (k = 0; k < 5; k++) {
            char *name = k == 0 ? L("Aceitar rivais com", "Accept rivals with", "Aceptar rivales con") : k == 1 ? L("Conex\303\243o do rival", "Rival's connection", "Conexi\303\263n del rival")
                       : k == 2 ? L("Regi\303\243o", "Region", "Regi\303\263n") : k == 3 ? L("Cen\303\241rio", "Stage", "Escenario") : L("Tempo da luta", "Fight time", "Tiempo");

            Zo_Row(H_PREF + k, 900.0f, 170.0f + (f32)k * 115.0f, 2340.0f, name, Zo_Pref(k, sZo.pref[k]), sZo.focus == H_PREF + k, 100.0f, k);
        }
        Zo_BigButton(1270.0f, 780.0f, 1970.0f, 896.0f, L("BUSCAR RIVAL", "FIND A RIVAL", "BUSCAR RIVAL"), L("Entrar na fila", "Join the queue", "Entrar en la cola"), a, sZo.focus == H_GO);
        Zo_Hit(H_GO, 1270.0f, 770.0f, 1970.0f, 906.0f);
        hit[0] = H_BACK, hit[1] = H_SWAP, hit[2] = H_MOVES, art[0] = ZW_T_RED, art[1] = art[2] = ZW_T_BAR;
        label[0] = L("Voltar", "Back", "Volver"), label[1] = L("Trocar guerreiro", "Change warrior", "Cambiar guerrero"), label[2] = L("Meus golpes", "My moves", "Mis golpes");
        Zo_Bar(3, hit, art, label);
        break;
    case ZO_SEARCH: {
        static char tm[48];
        f32 cx = 1620.0f, cy = 490.0f;

        Zo_Header(L("PARTIDA R\303\201PIDA", "QUICK MATCH", "PARTIDA R\303\201PIDA"), L("Procurando um rival...", "Looking for a rival...", "Buscando un rival..."), 0);
        Zo_Side(60.0f, 860.0f, L("SEU GUERREIRO", "YOUR WARRIOR", "TU GUERRERO"), Zo_Mine(), 1, 0, NULL, 0, 0);
        Zt_Box(ZW_T_PANEL, 900.0f, 170.0f, 2340.0f, 925.0f, 34.0f, a);
        for (i = 0; i < 4; i++) { /* rings going out */
            f32 p = (f32)((sZo.t + i * 30) % 120) / 120.0f;

            Mx_Fit(ZW_T_RIPPLE, ZT_X(cx), ZT_Y(cy), ZT_S(80.0f + 520.0f * p), (s32)((f32)a * (1.0f - p)));
        }
        Mx_Fit(ZW_CLOUD, ZT_X(cx), ZT_Y(cy), ZT_S(660.0f), a); /* the flying cloud going round, with its trail */
        if (Zo_Mine() != NULL) {
            Zw_Chip(Lib_Face(Zo_Mine()), ZT_X(cx), ZT_Y(cy), ZT_S(130.0f), 1);
        }
        snprintf(tm, sizeof(tm), "%s  00:%02d", L("Tempo na fila", "Time in queue", "Tiempo en cola"), (sZo.t / 60) % 60);
        Zt_Text(cx, 820.0f, tm, 44.0f, 1300.0f, ZT_GOLD, 1, a);
        Zt_Text(cx, 880.0f, L("A busca vai se abrindo quanto mais tempo passa.", "The search widens the longer it takes.", "La b\303\272squeda se ampl\303\255a con el tiempo."), 28.0f, 1300.0f, ZT_DIM, 1, a);
        hit[0] = H_CANCEL, hit[1] = H_PRACTICE, art[0] = ZW_T_RED, art[1] = ZW_T_BAR;
        label[0] = L("Cancelar busca", "Cancel", "Cancelar"), label[1] = L("Treinar enquanto espera", "Practise while you wait", "Entrenar mientras esperas");
        Zo_Bar(2, hit, art, label);
        break;
    }
    case ZO_FOUND: {
        static char left[16];
        s32 s = 15 - sZo.t / 60;

        Zo_Header(L("RIVAL ENCONTRADO", "RIVAL FOUND", "RIVAL ENCONTRADO"), L("Os dois precisam aceitar", "Both must accept", "Los dos deben aceptar"), 0);
        Zo_Side(60.0f, 860.0f, L("VOC\303\212", "YOU", "T\303\232"), Zo_Mine(), 1, 0, NULL, 0, 0);
        Zo_Side(1540.0f, 2340.0f, "RIVAL", Zo_Friend(), 0, 0, NULL, 0, 0);
        Zt_Text(1200.0f, 330.0f, "VS", 150.0f + (sZo.t < 20 ? 90.0f * (1.0f - Zw_In(sZo.t, 20)) : 8.0f * Zw_Beat(60)), 560.0f, ZT_GOLD, 1, a);
        Zt_Box(ZW_T_STAGE, 900.0f, 470.0f, 1500.0f, 700.0f, 34.0f, a);
        Zt_Text(940.0f, 515.0f, L("Conex\303\243o: exemplo (52 ms)", "Connection: example (52 ms)", "Conexi\303\263n: ejemplo (52 ms)"), 30.0f, 530.0f, ZT_GREEN, 0, a);
        Zt_Text(940.0f, 585.0f, L("Mesma vers\303\243o do app", "Same app version", "Misma versi\303\263n de la app"), 30.0f, 530.0f, ZT_GREEN, 0, a);
        Zt_Text(940.0f, 655.0f, L("Mesmo disco do jogo", "Same game disc", "Mismo disco del juego"), 30.0f, 530.0f, ZT_GREEN, 0, a);
        snprintf(left, sizeof(left), "00:%02d", s < 0 ? 0 : s);
        Zt_Text(1200.0f, 775.0f, L("Aceite em", "Accept within", "Acepta en"), 32.0f, 500.0f, ZT_WHITE, 1, a);
        Zt_Text(1200.0f, 850.0f, left, s <= 5 ? 70.0f + 8.0f * Zw_Beat(30) : 70.0f, 500.0f, ZT_GOLD, 1, a);
        hit[0] = H_REFUSE, hit[1] = H_MOVES, hit[2] = H_ACCEPT, art[0] = ZW_T_RED, art[1] = ZW_T_BAR, art[2] = ZW_T_GREEN;
        label[0] = L("Recusar", "Decline", "Rechazar"), label[1] = L("Ver golpes do rival", "Rival's moves", "Golpes del rival"), label[2] = L("ACEITAR LUTA", "ACCEPT", "ACEPTAR");
        Zo_Bar(3, hit, art, label);
        break;
    }
    case ZO_CREATE:
        Zo_Header(L("CRIAR SALA", "CREATE ROOM", "CREAR SALA"), L("Defina as regras da sua sala", "Set your room's rules", "Define las reglas de tu sala"), 1);
        Zo_Side(60.0f, 860.0f, L("SEU GUERREIRO", "YOUR WARRIOR", "TU GUERRERO"), Zo_Mine(), 1, 0, NULL, 0, 0);
        for (k = 0; k < 6; k++) {
            char *name = k == 0 ? L("Cen\303\241rio", "Stage", "Escenario") : k == 1 ? L("Tempo da luta", "Fight time", "Tiempo") : k == 2 ? L("Itens", "Items", "Objetos")
                       : k == 3 ? L("Vida", "Health", "Vida") : k == 4 ? L("Quem pode entrar", "Who can join", "Qui\303\251n puede entrar") : L("Guerreiros permitidos", "Allowed", "Permitidos");

            Zo_Row(H_RULE + k, 900.0f, 170.0f + (f32)k * 104.0f, 2340.0f, name, Zo_Rule(k, sZo.rule[k]), sZo.focus == H_RULE + k, 92.0f, k);
        }
        Zo_BigButton(1270.0f, 812.0f, 1970.0f, 912.0f, L("CRIAR SALA", "CREATE ROOM", "CREAR SALA"), L("Gera o c\303\263digo", "Makes the code", "Genera el c\303\263digo"), a, sZo.focus == H_GO);
        Zo_Hit(H_GO, 1270.0f, 800.0f, 1970.0f, 925.0f);
        hit[0] = H_BACK, hit[1] = H_SWAP, hit[2] = H_MOVES, art[0] = ZW_T_RED, art[1] = art[2] = ZW_T_BAR;
        label[0] = L("Voltar", "Back", "Volver"), label[1] = L("Trocar guerreiro", "Change warrior", "Cambiar guerrero"), label[2] = L("Meus golpes", "My moves", "Mis golpes");
        Zo_Bar(3, hit, art, label);
        break;
    case ZO_CODE: {
        static const char *kRows[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
        static char key[128][2], at[6][2];
        s32 r, c, n = 0;

        Zo_Header(L("ENTRAR COM C\303\223DIGO", "ENTER A CODE", "ENTRAR CON C\303\223DIGO"), L("Digite o c\303\263digo de 6 letras e n\303\272meros", "Type the 6 letters and numbers", "Escribe las 6 letras y n\303\272meros"), 1);
        Zo_Side(60.0f, 860.0f, L("SEU GUERREIRO", "YOUR WARRIOR", "TU GUERRERO"), Zo_Mine(), 1, 0, NULL, 0, 0);
        Zt_Box(ZW_T_PANEL, 900.0f, 170.0f, 2340.0f, 925.0f, 34.0f, a);
        for (i = 0; i < 6; i++) {
            f32 x = 1085.0f + (f32)i * 180.0f;

            Zt_Box(i == sZo.codeLen ? ZW_T_SEL : ZW_T_STAGE, x, 200.0f, x + 160.0f, 340.0f, 28.0f, a);
            at[i][0] = i < sZo.codeLen ? sZo.code[i] : 0;
            at[i][1] = 0;
            Zt_Text(x + 80.0f, 270.0f, at[i], 76.0f, 130.0f, ZT_GOLD, 1, a);
        }
        for (r = 0; r < 4; r++) {
            s32 len = (s32)strlen(kRows[r]);
            f32 x0 = 1620.0f - ((f32)len * 136.0f - 12.0f) * 0.5f;

            for (c = 0; c < len; c++, n++) {
                f32 x = x0 + (f32)c * 136.0f, y = 375.0f + (f32)r * 115.0f;

                key[(s32)kRows[r][c]][0] = kRows[r][c];
                Zo_Button(H_KEY + (kRows[r][c] - '0'), ZW_T_BAR, x, y, x + 124.0f, y + 100.0f, key[(s32)kRows[r][c]], 40.0f);
            }
        }
        Zo_Button(H_ERASE, ZW_T_RED, 950.0f, 838.0f, 1300.0f, 912.0f, L("Apagar", "Erase", "Borrar"), 32.0f);
        Zo_Button(H_JOIN, sZo.codeLen == 6 ? ZW_T_GREEN : ZW_T_GREY, 1330.0f, 838.0f, 2290.0f, 912.0f, L("ENTRAR NA SALA", "JOIN ROOM", "ENTRAR EN LA SALA"), 36.0f);
        hit[0] = H_BACK, hit[1] = H_SWAP, art[0] = ZW_T_RED, art[1] = ZW_T_BAR;
        label[0] = L("Voltar", "Back", "Volver"), label[1] = L("Trocar guerreiro", "Change warrior", "Cambiar guerrero");
        Zo_Bar(2, hit, art, label);
        break;
    }
    case ZO_ROOM:
        Zo_RoomDraw();
        break;
    case ZO_STAGES: {
        static char page[24];
        s32 first = sZo.stagePage * ZO_PER_PAGE, pages = (sZoStageN + ZO_PER_PAGE - 1) / ZO_PER_PAGE;
        f32 dx = Zo_Slide(1);

        Zo_Header(L("ESCOLHER CEN\303\201RIO", "CHOOSE THE STAGE", "ELIGE EL ESCENARIO"), L("S\303\263 o anfitri\303\243o escolhe", "The host chooses", "Elige el anfitri\303\263n"), 0);
        Zo_Button(H_CONFIRM, ZW_T_GREEN, 1950.0f, 38.0f, 2340.0f, 138.0f, L("CONFIRMAR", "CONFIRM", "CONFIRMAR"), 38.0f);
        Zt_Box(ZW_T_PANEL, 60.0f - dx, 170.0f, 1060.0f - dx, 925.0f, 34.0f, a);
        Zo_StageBig(sZo.stagePick, 90.0f - dx, 200.0f, 1030.0f - dx, 815.0f, a);
        Zt_Text(560.0f - dx, 870.0f, Zo_StageName(sZo.stagePick), 48.0f, 950.0f, ZT_GOLD, 1, a);
        for (i = 0; i < ZO_PER_PAGE && first + i < sZoStageN; i++) {
            s32 id = sZoStages[first + i];
            f32 x = 1100.0f + (f32)(i % 3) * 418.0f + dx, y = 170.0f + (f32)(i / 3) * 222.0f;

            Zt_Box(id == sZo.stagePick ? ZW_T_SEL : ZW_T_PANEL, x, y, x + 404.0f, y + 208.0f, 24.0f, a);
            Zo_StageChip(id, x + 16.0f, y + 16.0f, x + 388.0f, y + 150.0f, a);
            Zt_Text(x + 202.0f, y + 180.0f, Zo_StageName(id), 26.0f, 380.0f, ZT_WHITE, 1, a);
            Zo_Hit(H_STAGE + first + i, x - dx, y, x - dx + 404.0f, y + 208.0f);
        }
        Zo_Button(H_PGPREV, ZW_T_BAR, 1100.0f, 840.0f, 1440.0f, 925.0f, L("<  Anterior", "<  Previous", "<  Anterior"), 32.0f);
        snprintf(page, sizeof(page), "%d / %d", sZo.stagePage + 1, pages);
        Zt_Text(1720.0f, 882.0f, page, 38.0f, 400.0f, ZT_WHITE, 1, a);
        Zo_Button(H_PGNEXT, ZW_T_BAR, 2000.0f, 840.0f, 2340.0f, 925.0f, L("Pr\303\263xima  >", "Next  >", "Siguiente  >"), 32.0f);
        hit[0] = H_BACK, art[0] = ZW_T_RED, label[0] = L("Voltar sem mudar", "Back without changing", "Volver sin cambiar");
        Zo_Bar(1, hit, art, label);
        break;
    }
    case ZO_MOVES: {
        PortWarrior *v = sZo.movesMine ? Zo_Mine() : Zo_Friend();
        static const s32 kKind[4] = {ROW_SKILL0, ROW_SKILL1, ROW_TECH2, ROW_TECH0};
        static char val[7][96], forms[96];

        Zo_Header(sZo.movesMine ? L("MEUS GOLPES", "MY MOVES", "MIS GOLPES") : L("GOLPES DO AMIGO", "FRIEND'S MOVES", "GOLPES DEL AMIGO"), Zo_Name(v), 0);
        Zo_Side(60.0f, 860.0f, sZo.movesMine ? L("VOC\303\212", "YOU", "T\303\232") : L("AMIGO", "FRIEND", "AMIGO"), v, 1, 0, NULL, 0, 0); /* (this screen has the one 3D viewer for whoever it shows) */
        if (v != NULL) {
            s32 n = 0;

            for (k = 0; k < 4; k++) {
                char *name = k == 0 ? "Blast 1 (1)" : k == 1 ? "Blast 1 (2)" : k == 2 ? "Ultimate" : "Blast 2";

                snprintf(val[k], 96, "%s", Zw_MoveName(v, kKind[k]));
                Zo_Row(0, 900.0f, 170.0f + (f32)k * 108.0f, 2340.0f, name, val[k], 0, 96.0f, k);
            }
            Zo_Row(0, 900.0f, 602.0f, 2340.0f, L("Golpes corpo a corpo", "Melee", "Cuerpo a cuerpo"), Zw_Name(v->moves), 0, 96.0f, 4);
            for (k = 0; k < 4; k++) {
                n += v->formTo[k][0] != 0;
            }
            snprintf(forms, sizeof(forms), "%d", n);
            Zo_Row(0, 900.0f, 710.0f, 2340.0f, L("Transforma\303\247\303\265es", "Transformations", "Transformaciones"), forms, 0, 96.0f, 5);
            Zo_Row(0, 900.0f, 818.0f, 2340.0f, L("Corpo", "Body", "Cuerpo"), Zw_Name(v->body), 0, 96.0f, 6);
        }
        hit[0] = H_BACK, art[0] = ZW_T_RED, label[0] = L("Voltar", "Back", "Volver");
        Zo_Bar(1, hit, art, label);
        break;
    }
    case ZO_PREPARE:
        Zo_RoomDraw();
        Zo_Overlay();
        Zt_Box(ZW_T_ASK, 520.0f, 300.0f, 1880.0f, 780.0f, 48.0f, a);
        Zt_Text(1200.0f, 375.0f, L("PREPARANDO A LUTA", "GETTING READY", "PREPARANDO"), 58.0f, 1280.0f, ZT_GOLD, 1, a);
        Zo_Progress(460.0f, L("Seu guerreiro", "Your warrior", "Tu guerrero"), (f32)sZo.t / 60.0f, a);
        Zo_Progress(550.0f, L("Guerreiro do amigo", "Friend's warrior", "Del amigo"), (f32)(sZo.t - 20) / 150.0f, a);
        Zo_Progress(640.0f, L("Cen\303\241rio", "Stage", "Escenario"), (f32)(sZo.t - 60) / 150.0f, a);
        Zt_Text(1200.0f, 730.0f, L("A luta come\303\247a quando os dois celulares terminarem.", "The fight starts when both phones are done.", "La lucha empieza cuando ambos terminen."), 28.0f, 1280.0f, ZT_DIM, 1, a);
        break;
    case ZO_RESULT: {
        static char score[2][64];

        Zo_Header(L("FIM DA LUTA", "FIGHT OVER", "FIN DE LA LUCHA"), L("Exemplo de resultado", "Example result", "Resultado de ejemplo"), 0);
        Zt_Text(1200.0f, 235.0f, L("VIT\303\223RIA!", "VICTORY!", "\302\241VICTORIA!"), 110.0f + (sZo.t < 24 ? 120.0f * (1.0f - Zw_In(sZo.t, 24)) : 5.0f * Zw_Beat(60)), 880.0f, ZT_GOLD, 1, a);
        Zo_Side(60.0f, 760.0f, L("VOC\303\212", "YOU", "T\303\232"), Zo_Mine(), 1, 0, NULL, 0, 0);
        Zo_Side(1640.0f, 2340.0f, L("AMIGO", "FRIEND", "AMIGO"), Zo_Friend(), 0, 0, NULL, 0, 0);
        Zt_Box(ZW_T_STAGE, 800.0f, 340.0f, 1600.0f, 700.0f, 34.0f, a);
        snprintf(score[0], 64, "%s   %d  x  0", L("Placar da sala", "Room score", "Marcador"), sZo.rounds);
        Zt_Text(840.0f, 395.0f, score[0], 36.0f, 730.0f, ZT_GOLD, 0, a);
        Zt_Text(840.0f, 460.0f, L("Tempo da luta   01:48", "Fight time   01:48", "Tiempo   01:48"), 33.0f, 730.0f, ZT_WHITE, 0, a);
        Zt_Text(840.0f, 525.0f, L("Maior combo   24 golpes", "Best combo   24 hits", "Mejor combo   24 golpes"), 33.0f, 730.0f, ZT_WHITE, 0, a);
        Zt_Text(840.0f, 590.0f, L("Dano causado   38.200", "Damage   38,200", "Da\303\261o   38.200"), 33.0f, 730.0f, ZT_WHITE, 0, a);
        Zt_Text(840.0f, 655.0f, L("Conex\303\243o   est\303\241vel", "Connection   stable", "Conexi\303\263n   estable"), 33.0f, 730.0f, ZT_GREEN, 0, a);
        hit[0] = H_LEAVE, hit[1] = H_SWAP, hit[2] = H_ROOM, hit[3] = H_REMATCH;
        art[0] = ZW_T_RED, art[1] = art[2] = ZW_T_BAR, art[3] = ZW_T_GREEN;
        label[0] = L("Sair da sala", "Leave", "Salir"), label[1] = L("Trocar guerreiro", "Change warrior", "Cambiar guerrero");
        label[2] = L("Voltar \303\240 sala", "Back to room", "Volver a la sala"), label[3] = L("REVANCHE", "REMATCH", "REVANCHA");
        Zo_Bar(4, hit, art, label);
        break;
    }
    default: { /* ZO_LOST, ZO_VERSION */
        s32 lost = sZo.screen == ZO_LOST;

        Zo_RoomDraw();
        Zo_Overlay();
        Zt_Box(ZW_T_ASK, 520.0f, 300.0f, 1880.0f, 780.0f, 48.0f, a);
        Zt_Text(1200.0f, 385.0f, lost ? L("A CONEX\303\203O CAIU", "CONNECTION LOST", "SE PERDI\303\223 LA CONEXI\303\223N") : L("VERS\303\225ES DIFERENTES", "DIFFERENT VERSIONS", "VERSIONES DISTINTAS"), 56.0f, 1280.0f, ZT_GOLD, 1, a);
        Zt_Text(1200.0f, 485.0f, lost ? L("Perdemos o contato com o outro jogador.", "We lost contact with the other player.", "Perdimos el contacto con el otro jugador.")
                                      : L("O outro jogador usa outra vers\303\243o do app.", "The other player has another app version.", "El otro jugador usa otra versi\303\263n."), 32.0f, 1260.0f, ZT_WHITE, 1, a);
        Zt_Text(1200.0f, 545.0f, lost ? L("A luta foi encerrada sem vencedor.", "The fight ended with no winner.", "La lucha termin\303\263 sin ganador.")
                                      : L("Os dois precisam da mesma vers\303\243o para lutar.", "Both need the same version to fight.", "Los dos necesitan la misma versi\303\263n."), 32.0f, 1260.0f, ZT_DIM, 1, a);
        Zo_Button(H_LEAVE, ZW_T_RED, 640.0f, 650.0f, 1170.0f, 745.0f, L("Sair", "Leave", "Salir"), 34.0f);
        Zo_Button(H_ROOM, ZW_T_GREEN, 1230.0f, 650.0f, 1760.0f, 745.0f, L("Voltar \303\240 sala", "Back to room", "Volver a la sala"), 34.0f);
        break;
    }
    }
    Zo_Strip();
    Zo_FocusDraw();
    if (sZw->noteT > 0) {
        Zt_Box(ZW_T_ASK, 640.0f, 128.0f, 1760.0f, 214.0f, 40.0f, 0x80);
        Zt_Text(1200.0f, 171.0f, sZw->note, 30.0f, 1060.0f, ZT_WHITE, 1, 0x80);
    }
}

/* ---- what a press does ---- */
static void Zo_Rules(void) { /* the host's choices: stage, time, items, health, who may join, fighters allowed */
    int r[6] = {sZo.stage, sZo.rule[1], sZo.rule[2], sZo.rule[3], sZo.rule[4], sZo.rule[5]};

    Port_ZwNetRules(r);
}

static void Zo_DrawStage(void) { /* a "random" stage is drawn by the host, so both fight on the same one */
    if (sZo.stage < 0 && sZoStageN > 1) {
        sZo.stage = sZoStages[(u32)(sZw->frames * 2654435761u >> 8) % (u32)(sZoStageN - 1)];
        Zo_Rules();
    }
}

static void Zo_Leave(void) {
    Port_ZwNetLeave();
    Port_ZwNetTick();
    Zw_Reload(); /* (the rival's warrior is gone from the library) */
    Zo_StagesClose();
    Zw_SetMode(ZW_LIBRARY);
    sZw->noteT = 0;
}

static void Zo_Back(void) {
    switch (sZo.screen) {
    case ZO_MENU: Zo_Leave(); break;
    case ZO_QUICK: case ZO_CREATE: case ZO_CODE: Zo_Go(ZO_MENU); break;
    case ZO_SEARCH: case ZO_FOUND: Port_ZwNetLeave(); Zo_Go(ZO_QUICK); break;
    case ZO_ROOM: Port_ZwNetLeave(); Zo_Go(ZO_MENU); break;
    case ZO_STAGES: case ZO_MOVES: Zo_Go(sZo.from); break;
    case ZO_RESULT: case ZO_LOST: case ZO_VERSION: case ZO_PREPARE: Zo_Go(ZO_ROOM); break;
    default: Zo_Go(ZO_MENU); break;
    }
}

static void Zo_Open(void) {
    s32 sel = sZw->cur > 0 ? sZw->cur - 1 : 0;

    memset(&sZo, 0, sizeof(sZo));
    sZo.sel = sel;
    sZoLibSel = 1; /* (back on the home screen, the arrows are still on the button) */
    Zw_SetMode(ZW_ONLINE);
    sZw->noteT = 0;
    Zo_StagesOpen();
}

static void Zo_Press(s32 id) {
    extern void Port_TagLangSet(int);

    if (sZo.swap) { /* (choosing a warrior: only the arrows, the confirm, and back to give it up) */
        if (id == H_PICKED) {
            sZo.swap = 0;
            sZo.focus = H_SWAP;
            Port_ZwNetWarrior(sZo.sel % sZw->count);
            Snd_PlaySe(1, 1);
        } else if (id == H_PREV || id == H_NEXT) {
            extern int Port_WarriorIsGuest(int i);
            s32 tries;

            for (tries = 0; tries < sZw->count; tries++) { /* (never the rival's) */
                sZo.sel = (sZo.sel + (id == H_PREV ? sZw->count - 1 : 1)) % sZw->count;
                if (!Port_WarriorIsGuest(sZo.sel)) {
                    break;
                }
            }
            sZw->rest = 0;
            Snd_PlaySe(1, 0);
        } else if (id == H_BACK || id == H_HEADBACK) {
            sZo.sel = sZo.swapFrom;
            sZo.swap = 0;
            sZw->rest = 0;
            Snd_PlaySe(1, 2);
        }
        return;
    }
    if (id == H_HEADBACK) {
        id = H_BACK;
    }
    Snd_PlaySe(1, id == H_BACK || id == H_CANCEL || id == H_LEAVE || id == H_REFUSE ? 2 : 1);
    if (id >= H_STAGE) {
        if (id - H_STAGE < sZoStageN) {
            sZo.stagePick = sZoStages[id - H_STAGE];
        }
        return;
    }
    if (id >= H_KEY) {
        if (sZo.codeLen < 6) {
            sZo.code[sZo.codeLen++] = (char)('0' + (id - H_KEY));
            sZo.code[sZo.codeLen] = 0;
        }
        return;
    }
    if (id >= H_RULE) {
        s32 k = id - H_RULE;

        if (k == 0) {
            s32 i;

            sZo.stagePick = sZo.stage;
            for (i = 0; i < sZoStageN; i++) {
                if (sZoStages[i] == sZo.stage) {
                    sZo.stagePage = i / ZO_PER_PAGE;
                }
            }
            Zo_Go(ZO_STAGES);
        } else {
            sZo.rule[k] = (sZo.rule[k] + 1) % kZoRuleN[k];
            Zo_Rules();
        }
        return;
    }
    if (id >= H_PREF) {
        sZo.pref[id - H_PREF] = (sZo.pref[id - H_PREF] + 1) % kZoPrefN[id - H_PREF];
        return;
    }
    switch (id) {
    case H_BACK: case H_CANCEL: Zo_Back(); break;
    case H_LANG0: Port_TagLangSet(1); break;
    case H_LANG1: Port_TagLangSet(0); break;
    case H_LANG2: Port_TagLangSet(2); break;
    case H_QUICK: case H_CREATE: case H_ENTER:
        if (sZw->count == 0) {
            Zw_Note(L("Crie um guerreiro primeiro.", "Make a warrior first.", "Crea un guerrero primero."));
            break;
        }
        if (id == H_ENTER) {
            sZo.codeLen = 0;
            sZo.code[0] = 0;
        }
        Zo_Go(id == H_QUICK ? ZO_QUICK : id == H_CREATE ? ZO_CREATE : ZO_CODE);
        break;
    case H_HOW: Zw_Note(L("Seu guerreiro salvo luta contra o de um amigo. Ainda em constru\303\247\303\243o.", "Your saved warrior fights a friend's. Still being built.", "Tu guerrero contra el de un amigo. En construcci\303\263n.")); break;
    case H_TEST: case H_PRACTICE: Zw_Note(L("Dispon\303\255vel quando o online ligar.", "Available once online is on.", "Disponible cuando el online est\303\251 activo.")); break;
    case H_REPORT: Zw_Note(Port_MakeReport("") ? L("Relat\303\263rio salvo na pasta reports.", "Report saved in the reports folder.", "Informe guardado en reports.") : L("N\303\243o deu para criar o relat\303\263rio.", "Could not make the report.", "No se pudo crear el informe.")); break;
    case H_SWAP:
        if (sZw->count > 1) {
            sZo.swap = 1;
            sZo.swapFrom = sZo.sel % sZw->count;
            sZo.sel = sZo.swapFrom;
            sZo.ready = 0;
        } else {
            Zw_Note(L("S\303\263 h\303\241 um guerreiro salvo.", "There is only one saved warrior.", "Solo hay un guerrero guardado."));
        }
        break;
    case H_GO:
        if (sZw->count == 0) {
            break;
        }
        if (sZo.screen == ZO_QUICK) {
            Port_ZwNetQuick(sZo.sel % sZw->count, sZo.pref[0] == 0);
            Zo_Go(ZO_SEARCH);
        } else {
            int r[6] = {sZo.stage, sZo.rule[1], sZo.rule[2], sZo.rule[3], sZo.rule[4], sZo.rule[5]};

            Port_ZwNetHost(sZo.sel % sZw->count, r);
            Zo_Go(ZO_ROOM);
        }
        break;
    case H_MOVES: sZo.movesMine = sZo.screen == ZO_QUICK || sZo.screen == ZO_CREATE; Zo_Go(ZO_MOVES); break;
    case H_REFUSE: Zo_Go(ZO_QUICK); break;
    case H_ACCEPT:
        Zo_NewCode();
        sZo.host = 0;
        sZo.ready = sZo.friendReady = 1;
        sZo.rounds = 0;
        Zo_Go(ZO_PREPARE);
        break;
    case H_ERASE:
        if (sZo.codeLen > 0) {
            sZo.code[--sZo.codeLen] = 0;
        }
        break;
    case H_JOIN:
        if (sZo.codeLen == 6 && sZw->count > 0) {
            Port_ZwNetJoin(sZo.sel % sZw->count, sZo.code);
            Zo_Go(ZO_ROOM);
        }
        break;
    case H_COPY:
        if (sZo.code[0] != 0) {
            Port_ZwNetCopyCode();
            Zw_Note(L("C\303\263digo copiado.", "Code copied.", "C\303\263digo copiado."));
        }
        break;
    case H_READY: Port_ZwNetReady(!sZo.ready); break;
    case H_START:
        if (sZo.host && sZo.ready && sZo.friendReady) {
            Zo_DrawStage();
            Port_ZwNetStart(0);
            Zw_Note(L("Abrindo a luta...", "Starting the fight...", "Abriendo la lucha..."));
        } else {
            Zw_Note(L("O anfitri\303\243o come\303\247a quando os dois estiverem prontos.", "The host starts when both are ready.", "El anfitri\303\263n empieza cuando ambos est\303\251n listos."));
        }
        break;
    case H_LEAVE: Port_ZwNetLeave(); Zo_Go(ZO_MENU); break;
    case H_ROOM: Zo_Go(ZO_ROOM); break;
    case H_REMATCH: Zo_Go(ZO_ROOM); break;
    case H_CONFIRM:
        sZo.stage = sZo.stagePick;
        Zo_Rules();
        if (sZoNet.state == ZN_ROOM && sZoNet.quick) { /* a quick match: the stage chosen, the fight */
            Zo_DrawStage();
            if (sZoNet.peer >= 0) {
                Port_ZwNetStart(1);
                Zw_Note(L("Abrindo a luta...", "Starting the fight...", "Abriendo la lucha..."));
            } else {
                Zw_Note(L("Ainda recebendo o guerreiro do rival. Confirme de novo.", "Still receiving the rival's warrior. Confirm again.", "Recibiendo el guerrero rival. Confirma otra vez."));
            }
        } else {
            Zo_Go(sZo.from);
        }
        break;
    case H_PGPREV: case H_PGNEXT: {
        s32 pages = (sZoStageN + ZO_PER_PAGE - 1) / ZO_PER_PAGE;

        if (pages > 0) {
            sZo.stagePage = (sZo.stagePage + (id == H_PGPREV ? pages - 1 : 1)) % pages;
        }
        break;
    }
    }
}

static void Zo_Tap(f32 x, f32 y) {
    s32 i;

    for (i = sZoHits - 1; i >= 0; i--) {
        const ZoHit *h = &sZoHit[i];

        if (x >= h->x0 && x <= h->x1 && y >= h->y0 && y <= h->y1) {
            if (!sZo.swap && Zo_Focusable(h->id)) {
                sZo.focus = h->id; /* (the arrows go on from where the finger was) */
            }
            Zo_Press(h->id);
            return;
        }
    }
}

/* The library read again, the chosen warrior still the chosen one (the order is by name). */
static void Zo_Reload(void) {
    char folder[64];
    s32 i;

    folder[0] = 0;
    if (Zo_Mine() != NULL) {
        snprintf(folder, sizeof(folder), "%s", Zo_Mine()->folder);
    }
    Zw_Reload();
    for (i = 0; i < sZw->count; i++) {
        if (strcmp(sZw->list[i].folder, folder) == 0) {
            sZo.sel = i;
        }
    }
    sZw->rest = 0;
}

/* The room as the server has it, into the screens. */
static void Zo_NetFrame(void) {
    static int had;

    Port_ZwNetTick();
    Port_ZwNetGet(&sZoNet);
    if (sZoNet.libraryGen != sZoGen) { /* (the friend's warrior was made: the library is read again) */
        sZoGen = sZoNet.libraryGen;
        Zo_Reload();
    }
    { /* the rival's warrior read only when it changes: Port_WarriorGet reads its file, every frame was costly */
        static int at = -1, gen = -1;

        sZoHasPeer = sZoNet.peer >= 0;
        if (sZoHasPeer && (sZoNet.peer != at || sZoNet.libraryGen != gen)) {
            Port_WarriorGet(sZoNet.peer, &sZoPeer);
        }
        at = sZoHasPeer ? sZoNet.peer : -1;
        gen = sZoNet.libraryGen;
    }
    if (sZoNet.state == ZN_ROOM) {
        if (!had) {
            Snd_PlaySe(1, 1);
        }
        snprintf(sZo.code, sizeof(sZo.code), "%s", sZoNet.code);
        sZo.codeLen = (s32)strlen(sZo.code);
        sZo.host = sZoNet.host;
        sZo.ready = sZoNet.ready;
        sZo.friendReady = sZoNet.peerReady;
        if (!sZo.host) {
            sZo.stage = sZoNet.rules[0];
            sZo.rule[1] = sZoNet.rules[1], sZo.rule[2] = sZoNet.rules[2], sZo.rule[3] = sZoNet.rules[3];
        }
        if (sZo.screen == ZO_SEARCH) { /* a quick match: the host chooses the stage and the fight starts */
            Zo_Go(ZO_ROOM);
            if (sZo.host) {
                sZo.stagePick = sZo.stage = sZoNet.rules[0];
                Zo_Go(ZO_STAGES);
            } else {
                Zw_Note(L("Rival encontrado! Ele escolhe o cen\303\241rio.", "Rival found! They choose the stage.", "\302\241Rival! Elige el escenario."));
            }
        }
    } else if (sZoNet.state == ZN_CONNECTING) {
        sZo.code[0] = 0;
    } else if (sZoNet.state == ZN_FAILED) {
        static char *msg;
        int e = sZoNet.error;

        msg = e == ZNE_NET ? L("Sem resposta do servidor. Confira a internet.", "No answer from the server.", "El servidor no responde.")
            : e == ZNE_NOROOM ? L("Sala n\303\243o encontrada ou encerrada.", "Room not found or closed.", "Sala no encontrada.")
            : e == ZNE_FULL ? L("A sala est\303\241 cheia.", "The room is full.", "La sala est\303\241 llena.")
            : e == ZNE_VERSION ? L("Vers\303\265es diferentes: atualize o app.", "Different versions: update.", "Versiones distintas.")
            : e == ZNE_BUSY ? L("Muitas tentativas. Espere um pouco.", "Too many tries. Wait a bit.", "Demasiados intentos.")
            : e == ZNE_OFFICIAL ? L("Este app n\303\243o \303\251 aceito pelo servidor.", "This app is not accepted.", "App no aceptada.")
            : e == ZNE_TIME ? L("Acerte o rel\303\263gio do celular.", "Check the phone's clock.", "Revisa el reloj.")
            : e == ZNE_WARRIOR ? L("N\303\243o deu para ler o guerreiro.", "Could not read the warrior.", "No se pudo leer el guerrero.")
            : L("O online falhou. Tente de novo.", "Online failed. Try again.", "El online fall\303\263.");
        Port_ZwNetLeave();
        Zo_Go(ZO_MENU);
        Zw_Note(msg);
    }
    had = sZoNet.state == ZN_ROOM;
}

static void Zo_Update(s32 live) {
    Pad *pad = &gPad[0];

    sZo.t++;
    Zo_NetFrame();
    if (!live) {
        return;
    }
    if (sZo.swap) {
        if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
            Zo_Press(H_PREV);
        } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
            Zo_Press(H_NEXT);
        } else if (pad->gamePressed & PADG_CROSS) {
            Zo_Press(H_PICKED);
        } else if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
            Zo_Press(H_BACK);
        }
        return;
    }
    if (pad->gamePressed & (PADG_TRIANGLE | PADG_CIRCLE)) {
        Snd_PlaySe(1, 2);
        Zo_Back();
    } else if (pad->gamePressed & PADG_CROSS) {
        if (sZo.focus != 0) {
            Zo_Press(sZo.focus);
        }
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        Zo_Move(-1, 0);
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        Zo_Move(1, 0);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        Zo_Move(0, -1);
    } else if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        Zo_Move(0, 1);
    }
}
#undef L

