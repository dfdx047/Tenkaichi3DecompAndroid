/* The shading screens in the Android style (templates approved 2026-11-27, mods-zwarriors/previa_sombra.py): by part,
   by texture, the choices as boxes, the shadow colour. Touch first; the pad still works (Sh_Update, St_Update,
   Fx_Update). Positions are the templates' pixels (Zt_*). Included by port_zw.c. */
#define ZL(pt, en, es) Zo_L(pt, en, es)
static s32 sZaPop; /* 0 none, 1 same shading as, 2 glow, 3 outline */

/* The colour screens with a pad: up / down go from control to control (hue, strength, light, the colours, [only on,]
   the three buttons); Cross takes hold of one (left / right, and up / down on the colours, change it), Cross again lets
   go. Zc_Filter turns the pad into what the screen's own update understands; a finger's presses pass (sZcTouch). */
static s32 sZcItem, sZcEdit, sZcTouch, sZcSwatch;
static s32 sZcHold; /* frames an arrow has been held: one step a press, then slowly */
static void Za_Dot(f32 x, f32 y, f32 d, u32 bgr, s32 lit);

static void Zc_Reset(void) {
    sZcItem = sZcEdit = sZcSwatch = 0;
}

/* only: 1 the screen has "only on". `apply`: 1 the texture colour (Co_Apply), 0 the others (Fx_Set). */
static void Zc_Filter(Pad *pad, s32 only, s32 apply) {
    u32 held = pad->gameHeld, hit = pad->gamePressed;
    u32 arrows = PADG_UP | PADG_UP2 | PADG_DOWN | PADG_DOWN2 | PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2;
    u32 step;
    s32 up, down, left, right;
    s32 buttons = 4 + only, last = buttons + 2, changed = 0;
    u32 all = PADG_UP | PADG_UP2 | PADG_DOWN | PADG_DOWN2 | PADG_LEFT | PADG_LEFT2 | PADG_RIGHT | PADG_RIGHT2 | PADG_CROSS | PADG_L1 | PADG_R1;

    if (sZcTouch) {
        sZcTouch = 0;
        return;
    }
    /* one step a press; held, one more every quarter second after half a second (the pad's own repeat was too fast) */
    sZcHold = (held & arrows) && !(hit & arrows) ? sZcHold + 1 : 0;
    step = hit & arrows;
    if (sZcHold > 30 && (sZcHold - 30) % 15 == 0) {
        step |= held & arrows;
    }
    up = (step & (PADG_UP | PADG_UP2)) != 0, down = (step & (PADG_DOWN | PADG_DOWN2)) != 0;
    left = (step & (PADG_LEFT | PADG_LEFT2)) != 0, right = (step & (PADG_RIGHT | PADG_RIGHT2)) != 0;
    pad->gamePressed &= ~all;
    pad->gameRepeat &= ~all;
    pad->gameHeld &= ~all;
    if (sZcEdit) {
        if (sZcItem < 3) {
            f32 *v = sZcItem == 0 ? &sZw->pkH : sZcItem == 1 ? &sZw->pkS : &sZw->pkV;
            f32 d = (held & (PADG_LEFT | PADG_LEFT2)) ? -0.004f : (held & (PADG_RIGHT | PADG_RIGHT2)) ? 0.004f : 0.0f;

            if (d != 0.0f) {
                *v = *v + d < 0.0f ? 0.0f : *v + d > 0.999f ? 0.999f : *v + d;
                changed = 1;
            }
        } else if (sZcItem == 3 && (up || down || left || right)) {
            sZcSwatch = (sZcSwatch + (left ? 29 : right ? 1 : up ? 20 : 10)) % 30;
            Co_ToHsv(kSwatch[sZcSwatch], &sZw->pkH, &sZw->pkS, &sZw->pkV);
            changed = 1;
            Snd_PlaySe(1, 0);
        }
        if (changed) {
            if (apply) {
                Co_Apply();
            } else {
                Fx_Set((s32)(0x1000000 | Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV)));
            }
        }
        if (hit & (PADG_CROSS | PADG_TRIANGLE | PADG_CIRCLE)) { /* let go (Triangle too: back steps out of the control first) */
            sZcEdit = 0;
            pad->gamePressed &= ~(PADG_TRIANGLE | PADG_CIRCLE);
            Snd_PlaySe(1, 1);
        }
        return;
    }
    if (up || down) {
        sZcItem = sZcItem >= buttons ? (up ? buttons - 1 : 0) : (sZcItem + (up ? buttons : 1)) % (buttons + 1);
        Snd_PlaySe(1, 0);
    } else if ((left || right) && sZcItem >= buttons) {
        sZcItem = buttons + (sZcItem - buttons + (left ? 2 : 1)) % 3;
        Snd_PlaySe(1, 0);
    }
    sZcItem = sZcItem > last ? 0 : sZcItem;
    if (hit & PADG_CROSS) {
        if (sZcItem < 4) {
            sZcEdit = 1;
            Snd_PlaySe(1, 1);
        } else if (only && sZcItem == 4) {
            sZw->pkRow = CP_ONLY;
            pad->gamePressed |= PADG_RIGHT;
            pad->gameRepeat |= PADG_RIGHT;
        } else {
            sZw->pkRow = 0;
            pad->gamePressed |= sZcItem == buttons ? PADG_TRIANGLE : sZcItem == buttons + 1 ? PADG_SQUARE : PADG_CROSS;
        }
    }
}

/* The frame on the control the pad is on (thick and beating while it is held). */
static void Zc_Frame(s32 only) {
    s32 buttons = 4 + only;
    f32 x0, y0, x1, y1;

    if (sZcItem < 3) {
        x0 = 1368.0f, y0 = 245.0f + sZcItem * 125.0f, x1 = 2332.0f, y1 = y0 + 118.0f;
    } else if (sZcItem == 3) {
        x0 = 1372.0f, y0 = 640.0f, x1 = 2330.0f, y1 = 890.0f;
    } else if (only && sZcItem == 4) {
        x0 = 1022.0f, y0 = 332.0f, x1 = 1318.0f, y1 = 448.0f;
    } else {
        s32 b = sZcItem - buttons;

        x0 = b == 0 ? 1336.0f : b == 1 ? 1686.0f : 2036.0f, x1 = b == 0 ? 1680.0f : b == 1 ? 2030.0f : 2364.0f, y0 = 940.0f, y1 = 1055.0f;
    }
    Mx_Frame(ZT_X(x0), ZT_Y(y0), ZT_X(x1), ZT_Y(y1), ZT_S(sZcEdit ? 10.0f + 4.0f * Zw_Beat(30) : 6.0f),
             sZcEdit ? Mx_Rgba(90, 255, 140, 0x80) : Mx_Rgba(255, 214, 80, 0x80));
    if (sZcEdit) {
        Za_Dot(1420.0f + (sZcSwatch % 10) * 96.0f, 680.0f + (sZcSwatch / 10) * 84.0f, sZcItem == 3 ? 76.0f : 0.0f, kSwatch[sZcSwatch], sZcItem == 3);
        Zt_Text(950.0f, 1030.0f, Zo_L("\342\206\220 \342\206\222 muda  \342\200\242  X confirma", "left / right change  \342\200\242  X confirms",
                                     "izq / der cambia  \342\200\242  X confirma"), 30.0f, 720.0f, ZT_GOLD, 1, 0x80);
    }
}

static char *Za_Part(s32 e) {
    static char out[4][48];
    static s32 n;
    char *s;

    if (e == SH_ALL) {
        return ZL("Corpo todo", "Whole body", "Todo el cuerpo");
    }
    switch (e / 2) {
    case 0: s = ZL("Cabelo", "Hair", "Pelo"); break;
    case 1: s = ZL("Cabe\303\247a", "Head", "Cabeza"); break;
    case 2: s = ZL("Rosto", "Face", "Rostro"); break;
    case 3: s = ZL("Peito", "Chest", "Pecho"); break;
    case 4: s = ZL("Cintura", "Waist", "Cintura"); break;
    case 5: s = ZL("Bra\303\247os", "Upper arms", "Brazos"); break;
    case 6: s = ZL("Antebra\303\247os", "Forearms", "Antebrazos"); break;
    case 7: s = ZL("M\303\243os", "Hands", "Manos"); break;
    case 8: s = ZL("Quadril", "Hips", "Cadera"); break;
    case 9: s = ZL("Canelas", "Shins", "Espinillas"); break;
    case 10: s = ZL("P\303\251s", "Feet", "Pies"); break;
    default: s = ZL("Acess\303\263rios", "Accessories", "Accesorios"); break;
    }
    if (!(e & 1)) {
        return s;
    }
    snprintf(out[n & 3], 48, "%s +", s); /* (the layer over it) */
    return out[n++ & 3];
}

static char *Za_Glow(s32 g) {
    return g == 0 ? ZL("O pr\303\263prio", "Its own", "El propio") : g == 1 ? ZL("Nenhum", "None", "Ninguno") : g == 2 ? ZL("Suave", "Soft", "Suave")
         : g == 3 ? ZL("Forte", "Bright", "Fuerte") : ZL("Intenso", "Blazing", "Intenso");
}

static char *Za_Line(s32 v) {
    static char out[32];

    if (v == 0) {
        return ZL("Junto do corpo", "With the body", "Con el cuerpo");
    }
    if (v == 1) {
        return ZL("Pr\303\263prio", "Its own", "Propio");
    }
    snprintf(out, sizeof(out), "%s %d", ZL("Grupo", "Group", "Grupo"), (int)(v - 1));
    return out;
}

static s32 Za_Entry(void) {
    return sShRows > 0 ? sShList[sZw->shRow][0] : SH_ALL;
}

static void Za_PartRect(s32 i, f32 *r) {
    s32 rows = (sShRows + 1) / 2;
    f32 pitch = rows > 7 ? 800.0f / (f32)rows : 114.0f;

    r[0] = 60.0f + (f32)(i / rows) * 245.0f;
    r[1] = 240.0f + (f32)(i % rows) * pitch;
    r[2] = r[0] + 235.0f;
    r[3] = r[1] + pitch - 14.0f;
}

static void Za_Dot(f32 x, f32 y, f32 d, u32 bgr, s32 lit) {
    u32 c, edge;

    if (d >= 50.0f && y > 340.0f && (x > 1300.0f || y > 930.0f)) { /* (a colour to press, not a row's mark) */
        Zf_Add(x - d * 0.5f, y - d * 0.5f, x + d * 0.5f, y + d * 0.5f, 0);
    }
    c = Co_Rgba(bgr), edge = lit ? Mx_Rgba(255, 190, 40, 0x80) : Mx_Rgba(255, 255, 255, 0x80);

    Mx_Disc(ZT_X(x), ZT_Y(y), ZT_S(d + (lit ? 16.0f : 6.0f)), edge, edge);
    Mx_Disc(ZT_X(x), ZT_Y(y), ZT_S(d), c, c);
}

static void Za_Head(char *sub, s32 tabs) {
    Zt_Header(ZL("SOMBREAMENTO", "SHADING", "SOMBREADO"), sub);
    if (tabs) {
        Zt_Pill(sZw->shByTex ? ZW_T_NAVY : ZW_T_ORANGE, 1060.0f, 38.0f, 1420.0f, 138.0f, ZL("Por parte", "By part", "Por parte"), 38.0f, 0x80);
        Zt_Pill(sZw->shByTex ? ZW_T_ORANGE : ZW_T_NAVY, 1440.0f, 38.0f, 1800.0f, 138.0f, ZL("Por textura", "By texture", "Por textura"), 38.0f, 0x80);
        Zt_Pill(ZW_T_GREEN, 1950.0f, 38.0f, 2340.0f, 138.0f, sZw->dirty ? ZL("SALVAR", "SAVE", "GUARDAR") : ZL("SALVO", "SAVED", "GUARDADO"), 40.0f, 0x80);
    }
}

static void Za_Row(s32 k, char *name, char *value) {
    f32 y = 270.0f + (f32)k * 135.0f;

    Zt_Box(ZW_T_PANEL, 1370.0f, y, 2330.0f, y + 115.0f, 28.0f, 0x80);
    Zt_Text(1406.0f, y + 57.0f, name, 33.0f, 420.0f, ZT_WHITE, 0, 0x80);
    Zt_Text(2240.0f, y + 57.0f, value, 31.0f, 420.0f, ZT_GOLD, 2, 0x80);
    Zt_Chev(2290.0f, y + 57.0f, 34.0f);
}

static void Za_Note(void) {
    if (sZw->saving || sZw->noteT > 0) {
        Zt_Box(ZW_T_ASK, 640.0f, 150.0f, 1760.0f, 240.0f, 40.0f, 0x80);
        Zt_Text(1200.0f, 195.0f, sZw->saving ? ZL("Salvando seu guerreiro...", "Saving...", "Guardando...") : sZw->note, 30.0f, 1060.0f, ZT_WHITE, 1, 0x80);
    }
}

static void Za_PartsDraw(void) {
    s32 e, i, k;
    f32 r[4];
    char sub[64];

    Sh_Load();
    e = Za_Entry();
    Zw_EditStage(ZT_X(950.0f), ZT_Y(900.0f), ZT_Y(620.0f));
    Ws_ShapeTick(); /* (the part chosen blinks on the model) */
    Za_Head(ZL("Escolha a parte e como a sombra aparece", "Choose a part and how its shadow looks", "Elige la parte y su sombra"), 1);
    Zt_Box(ZW_T_ASK, 40.0f, 165.0f, 560.0f, 1050.0f, 34.0f, 0x80);
    Zt_Text(70.0f, 205.0f, ZL("PARTES", "PARTS", "PARTES"), 32.0f, 400.0f, ZT_GOLD, 0, 0x80);
    for (i = 0; i < sShRows; i++) {
        s32 v = sShList[i][0];

        Za_PartRect(i, r);
        Zt_Pill(i == sZw->shRow ? ZW_T_ORANGE : ZW_T_NAVY, r[0], r[1], r[2], r[3], Za_Part(v), 28.0f, 0x80);
        if (v != SH_ALL && Sh_Edit(v, 0) >= 0) { /* (changed) */
            Za_Dot(r[2] - 22.0f, r[1] + 20.0f, 22.0f, 0x003C3CF0, 0);
        }
    }
    Zt_Box(ZW_T_ASK, 1340.0f, 165.0f, 2360.0f, 920.0f, 34.0f, 0x80);
    Zt_Text(1850.0f, 215.0f, Za_Part(e), 40.0f, 960.0f, ZT_GOLD, 1, 0x80);
    Za_Row(0, ZL("Cor da sombra", "Shadow colour", "Color de sombra"),
           Sh_Style(e) == 7 ? ZL("Sua cor", "Your colour", "Tu color") : Sh_Style(e) == 8 ? ZL("A da parte copiada", "As the copied part", "La de la parte")
                            : ZL("A do jogo", "The game's", "La del juego"));
    if (Sh_Style(e) == 7) {
        Za_Dot(1990.0f, 327.0f, 52.0f, (u32)Sh_Pick(e, 3) & 0xFFFFFF, 0);
    }
    k = 1;
    if (e != SH_ALL) {
        s32 src = Sh_Same(e);

        Za_Row(k++, ZL("Sombra igual \303\240 de", "Same shading as", "Sombra igual a"),
               src >= 0 && src != SH_ALL ? Za_Part(src) : ZL("N\303\243o: a pr\303\263pria", "No: its own", "No: la propia"));
    }
    Za_Row(k++, ZL("Brilho", "Glow", "Brillo"), e == SH_ALL && Sh_Glow(e) == 0 ? ZL("Cada parte o seu", "Each its own", "Cada una el suyo") : Za_Glow(Sh_Glow(e)));
    if (e != SH_ALL) {
        Za_Row(k++, ZL("Contorno", "Outline", "Contorno"), Za_Line(Sh_Line(e)));
    } else {
        Zt_Text(1850.0f, 640.0f, ZL("Toque numa parte para ajustar s\303\263 ela.", "Tap a part to change only it.", "Toca una parte para ajustarla."), 30.0f, 940.0f, ZT_DIM, 1, 0x80);
    }
    Zt_Pill(ZW_T_BAR, 1340.0f, 945.0f, 1845.0f, 1050.0f, ZL("Como era", "As it was", "Como era"), 40.0f, 0x80);
    Zt_Pill(ZW_T_GREEN, 1855.0f, 945.0f, 2360.0f, 1050.0f, ZL("Pronto", "Done", "Listo"), 40.0f, 0x80);
    if (sZaPop) {
        u32 shade = Mx_Rgba(4, 6, 26, 0x58);

        Mx_Rect(0.0f, 0.0f, MX_W, MX_H, shade, shade);
        sZfN = 0;
        Zt_Box(ZW_T_ASK, 500.0f, 170.0f, 1900.0f, 1010.0f, 48.0f, 0x80);
        if (sZaPop == 1) {
            s32 src = Sh_Same(e), n = 0;

            Zt_Text(1200.0f, 230.0f, ZL("SOMBRA IGUAL \303\200 DE...", "SAME SHADING AS...", "SOMBRA IGUAL A..."), 48.0f, 1300.0f, ZT_GOLD, 1, 0x80);
            for (i = 0; i < sShRows && n < 15; i++) {
                s32 v = sShList[i][0];
                f32 x, y;

                if (v == e) {
                    continue;
                }
                x = 540.0f + (f32)(n % 3) * 445.0f, y = 290.0f + (f32)(n / 3) * 122.0f;
                n++;
                Zt_Pill((v == SH_ALL ? src < 0 || src == SH_ALL : src == v) ? ZW_T_ORANGE : ZW_T_NAVY, x, y, x + 430.0f, y + 108.0f,
                        v == SH_ALL ? ZL("N\303\243o: a pr\303\263pria", "No: its own", "No: la propia") : Za_Part(v), 34.0f, 0x80);
            }
        } else if (sZaPop == 2) {
            Zt_Text(1200.0f, 230.0f, ZL("BRILHO", "GLOW", "BRILLO"), 48.0f, 1300.0f, ZT_GOLD, 1, 0x80);
            for (i = 0; i < 5; i++) {
                f32 y = 290.0f + (f32)i * 118.0f;

                Zt_Pill(Sh_Glow(e) == i ? ZW_T_ORANGE : ZW_T_NAVY, 650.0f, y, 1750.0f, y + 104.0f, Za_Glow(i), 36.0f, 0x80);
            }
        } else {
            Zt_Text(1200.0f, 225.0f, ZL("CONTORNO", "OUTLINE", "CONTORNO"), 48.0f, 1300.0f, ZT_GOLD, 1, 0x80);
            Zt_Text(1200.0f, 288.0f, ZL("Partes no mesmo grupo viram uma s\303\263 superf\303\255cie.", "Parts in one group are one surface.", "Las partes de un grupo se unen."),
                    30.0f, 1340.0f, ZT_DIM, 1, 0x80);
            for (i = 0; i < 2; i++) {
                Zt_Pill(Sh_Line(e) == i ? ZW_T_ORANGE : ZW_T_NAVY, 540.0f + (f32)i * 670.0f, 330.0f, 1190.0f + (f32)i * 670.0f, 440.0f, Za_Line(i), 36.0f, 0x80);
            }
            for (i = 0; i < 16; i++) {
                f32 x = 540.0f + (f32)(i % 4) * 335.0f, y = 470.0f + (f32)(i / 4) * 105.0f;

                snprintf(sub, sizeof(sub), "%s %d", ZL("Grupo", "Group", "Grupo"), (int)(i + 1));
                Zt_Pill(Sh_Line(e) == i + 2 ? ZW_T_ORANGE : ZW_T_NAVY, x, y, x + 315.0f, y + 90.0f, sub, 32.0f, 0x80);
            }
        }
        Zt_Pill(ZW_T_RED, 900.0f, 915.0f, 1500.0f, 990.0f, ZL("Fechar", "Close", "Cerrar"), 34.0f, 0x80);
    }
    Za_Note();
}

static void Za_TexDraw(void) {
    s32 i, r, z = -1, own = 0, ramps[1 + 4 * 4];

    Zw_EditStage(ZT_X(1050.0f), ZT_Y(900.0f), ZT_Y(620.0f));
    Za_Head(ZL("Escolha a textura e a cor da sombra dela", "Choose a texture and its shadow colour", "Elige la textura y su sombra"), 1);
    Zt_Box(ZW_T_ASK, 40.0f, 165.0f, 760.0f, 1050.0f, 34.0f, 0x80);
    for (i = 0; i < 8 && sZw->shTexTop + i < sZw->coRows; i++) {
        s32 row = sZw->shTexTop + i, zz = sZw->coList[row];
        f32 y = 225.0f + (f32)i * 96.0f;

        if (zz < 0) {
            Zt_Text(80.0f, y + 55.0f, zz == -1 ? ZL("CABE\303\207A", "HEAD", "CABEZA") : zz == -2 ? ZL("CORPO", "BODY", "CUERPO") : ZL("DETALHES", "DETAILS", "DETALLES"),
                    30.0f, 600.0f, ZT_GOLD, 0, 0x80);
            continue;
        }
        if (row == sZw->shTexRow) {
            z = zz;
        }
        Zt_Box(row == sZw->shTexRow ? ZW_T_SEL : ZW_T_PANEL, 65.0f, y, 735.0f, y + 88.0f, 26.0f, 0x80);
        Za_Dot(120.0f, y + 44.0f, 52.0f, (u32)sZw->reg[zz * 4 + 1] & 0xFFFFFF, 0);
        Zt_Text(175.0f, y + 44.0f, Co_RegionName(zz), 32.0f, 470.0f, ZT_WHITE, 0, 0x80);
        if (St_Colour(zz) != 0) {
            Za_Dot(690.0f, y + 44.0f, 34.0f, (u32)St_Colour(zz) & 0xFFFFFF, 0);
        }
    }
    if (sZw->shTexTop > 0) {
        Mx_Fit(ZW_UP, ZT_X(400.0f), ZT_Y(195.0f), ZT_S(60.0f), 0x80);
        Zf_Add(400.0f - 50.0f, 195.0f - 30.0f, 400.0f + 50.0f, 195.0f + 30.0f, 0);
    }
    if (sZw->shTexTop + 8 < sZw->coRows) {
        Mx_Fit(ZW_DOWN, ZT_X(400.0f), ZT_Y(1020.0f), ZT_S(60.0f), 0x80);
        Zf_Add(400.0f - 50.0f, 1020.0f - 30.0f, 400.0f + 50.0f, 1020.0f + 30.0f, 0);
    }
    Zt_Box(ZW_T_ASK, 1340.0f, 165.0f, 2360.0f, 930.0f, 34.0f, 0x80);
    if (z >= 0) {
        own = St_Colour(z);
        Zt_Text(1850.0f, 215.0f, Co_RegionName(z), 36.0f, 960.0f, ZT_GOLD, 1, 0x80);
        Port_WarriorTexShade((s32)St_Hash(z), ramps);
        for (r = 0; r < ramps[0] && r < 2; r++) {
            s32 c;

            for (c = 0; c < 4; c++) {
                u32 col = Co_Rgba((u32)ramps[1 + r * 4 + c]);

                Mx_Rect(ZT_X(1390.0f + c * 230.0f), ZT_Y(265.0f + r * 60.0f), ZT_X(1615.0f + c * 230.0f), ZT_Y(315.0f + r * 60.0f), col, col);
            }
        }
        Zt_Text(1390.0f, 420.0f, ZL("Cor da sombra", "Shadow colour", "Color de sombra"), 34.0f, 500.0f, ZT_WHITE, 0, 0x80);
        Zt_Pill(own == 0 ? ZW_T_ORANGE : ZW_T_NAVY, 1390.0f, 455.0f, 1750.0f, 545.0f, ZL("A do jogo", "The game's", "La del juego"), 34.0f, 0x80);
        Zt_Pill(own != 0 ? ZW_T_ORANGE : ZW_T_NAVY, 1770.0f, 455.0f, 2310.0f, 545.0f, ZL("Qualquer cor...", "Any colour...", "Cualquier color..."), 34.0f, 0x80);
        for (i = 0; i < 30; i++) {
            Za_Dot(1420.0f + (f32)(i % 10) * 96.0f, 605.0f + (f32)(i / 10) * 84.0f, 62.0f, kSwatch[i], own != 0 && (u32)(own & 0xFFFFFF) == kSwatch[i]);
        }
        Zt_Text(1390.0f, 865.0f, ZL("Degrad\303\252", "Gradient", "Degradado"), 30.0f, 170.0f, ZT_WHITE, 0, 0x80);
        for (i = 0; i < 9; i++) {
            char n[4];
            s32 g = (own >> 25) & 15;

            snprintf(n, sizeof(n), "%d", (int)(i + 1));
            Zt_Pill(own != 0 && (g == 0 ? 5 : g) == i + 1 ? ZW_T_ORANGE : own != 0 ? ZW_T_NAVY : ZW_T_GREY, 1575.0f + i * 83.0f, 828.0f, 1650.0f + i * 83.0f, 903.0f, n, 32.0f, 0x80);
        }
    }
    Zt_Pill(ZW_T_BAR, 1340.0f, 945.0f, 1845.0f, 1050.0f, ZL("A do jogo", "The game's", "La del juego"), 40.0f, 0x80);
    Zt_Pill(ZW_T_GREEN, 1855.0f, 945.0f, 2360.0f, 1050.0f, ZL("Pronto", "Done", "Listo"), 40.0f, 0x80);
    Za_Note();
}

static void Za_Slider(s32 k, char *name, f32 v) {
    f32 y = 270.0f + (f32)k * 125.0f, c = y + 58.0f;
    s32 i;

    Zt_Text(1390.0f, y, name, 34.0f, 500.0f, ZT_WHITE, 0, 0x80);
    for (i = 0; i < 36; i++) {
        f32 q = ((f32)i + 0.5f) / 36.0f;
        u32 col = Co_Rgba(k == 0 ? Co_FromHsv(q, 0.85f, 0.95f) : k == 1 ? Co_FromHsv(sZw->pkH, q, sZw->pkV) : Co_FromHsv(sZw->pkH, sZw->pkS, q));

        Mx_Rect(ZT_X(1390.0f + 920.0f * i / 36.0f), ZT_Y(c - 14.0f), ZT_X(1390.0f + 920.0f * (i + 1) / 36.0f) + 1.0f, ZT_Y(c + 14.0f), col, col);
    }
    Za_Dot(1390.0f + 920.0f * v, c, 48.0f, 0x00FFFFFF, 1);
}

/* The shadow's colour (Fx_Draw for the shading's two targets). */
static void Za_ColourDraw(void) {
    s32 i;

    Zw_EditStage(ZT_X(950.0f), ZT_Y(900.0f), ZT_Y(620.0f));
    Za_Head(sZw->fxTarget != 4 && sZw->fxTarget != 5 && sZw->fxTarget != 8 ? ZL("Cor do golpe, da aura ou do brilho", "Colour of the technique, aura or glow", "Color de la t\303\251cnica, aura o brillo") : sZw->fxTarget == 8 ? ZL("Cor nova para esta textura", "A new colour for this texture", "Color nuevo para la textura") : sZw->fxTarget == 5 ? ZL("Cor da sombra da textura", "The texture's shadow colour", "Sombra de la textura")
                               : ZL("Cor da sombra da parte", "The part's shadow colour", "Sombra de la parte"), 0);
    Zt_Box(ZW_T_ASK, 1340.0f, 165.0f, 2360.0f, 930.0f, 34.0f, 0x80);
    Zt_Text(1850.0f, 215.0f, sZw->fxTarget != 4 && sZw->fxTarget != 5 && sZw->fxTarget != 8 ? ZL("COR", "COLOUR", "COLOR") : sZw->fxTarget == 8 ? ZL("COR NOVA", "NEW COLOUR", "COLOR NUEVO") : ZL("COR DA SOMBRA", "SHADOW COLOUR", "COLOR DE SOMBRA"), 40.0f, 960.0f, ZT_GOLD, 1, 0x80);
    Za_Slider(0, ZL("Tom", "Hue", "Tono"), sZw->pkH);
    Za_Slider(1, ZL("For\303\247a", "Strength", "Fuerza"), sZw->pkS);
    Za_Slider(2, ZL("Luz", "Light", "Luz"), sZw->pkV);
    for (i = 0; i < 30; i++) {
        Za_Dot(1420.0f + (f32)(i % 10) * 96.0f, 680.0f + (f32)(i / 10) * 84.0f, 62.0f, kSwatch[i], 0);
    }
    Za_Dot(950.0f, 220.0f, 90.0f, Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV), 0);
    Zt_Pill(ZW_T_RED, 1340.0f, 945.0f, 1675.0f, 1050.0f, ZL("Cancelar", "Cancel", "Cancelar"), 36.0f, 0x80);
    Zt_Pill(ZW_T_BAR, 1690.0f, 945.0f, 2025.0f, 1050.0f, ZL("A do jogo", "The game's", "La del juego"), 36.0f, 0x80);
    Zt_Pill(ZW_T_GREEN, 2040.0f, 945.0f, 2360.0f, 1050.0f, "OK", 40.0f, 0x80);
    Zc_Frame(0);
}

static void Za_Colour(s32 part) { /* the picker opened (as the pad's Cross does) */
    s32 v;

    if (part) {
        s32 e = Za_Entry();

        v = Sh_Style(e) == 7 ? Sh_Pick(e, 3) : 0;
        sZw->shE = e;
        sZw->shT = 3;
        sZw->fxTarget = 4;
    } else {
        s32 z = sZw->coList[sZw->shTexRow];

        v = St_Colour(z);
        sZw->shTexZ = z;
        sZw->fxTarget = 5;
    }
    sZw->fxBack = v;
    Co_ToHsv(v != 0 ? (u32)v & 0xFFFFFF : 0x6A3A50, &sZw->pkH, &sZw->pkS, &sZw->pkV);
    sZw->pkRow = 0;
    sZw->pkSwatch = 0;
    Zw_SetMode(ZW_FXCOL);
}

static void Za_ColourTap(f32 x, f32 y) {
    s32 i;

    sZcTouch = 1;
    sZcEdit = 0;

    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f) || Zt_In(x, y, 1340.0f, 945.0f, 1675.0f, 1060.0f)) {
        Zt_Press(PADG_TRIANGLE);
    } else if (Zt_In(x, y, 1690.0f, 945.0f, 2025.0f, 1060.0f)) {
        Zt_Press(PADG_SQUARE);
    } else if (Zt_In(x, y, 2040.0f, 945.0f, 2360.0f, 1060.0f)) {
        sZw->pkRow = 0; /* (not on "its own") */
        Zt_Press(PADG_CROSS);
    } else if (x >= 1360.0f && y >= 290.0f && y < 650.0f) {
        s32 k = (s32)((y - 290.0f) / 125.0f);
        f32 v = (x - 1390.0f) / 920.0f;

        v = v < 0.0f ? 0.0f : v > 0.999f ? 0.999f : v;
        if (k >= 0 && k < 3) {
            *(k == 0 ? &sZw->pkH : k == 1 ? &sZw->pkS : &sZw->pkV) = v;
            Fx_Set((s32)(0x1000000 | Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV)));
            Snd_PlaySe(1, 0);
        }
    } else if (x >= 1372.0f && y >= 640.0f && y < 890.0f) {
        i = (s32)((y - 638.0f) / 84.0f) * 10 + (s32)((x - 1372.0f) / 96.0f);
        if (i >= 0 && i < 30) {
            Co_ToHsv(kSwatch[i], &sZw->pkH, &sZw->pkS, &sZw->pkV);
            Fx_Set((s32)(0x1000000 | kSwatch[i]));
            Snd_PlaySe(1, 0);
        }
    }
}

static void Za_Tap(f32 x, f32 y) {
    s32 i, e, k;
    f32 r[4];

    if (sZw->saving) {
        return;
    }
    if (!sZw->shByTex) {
        Sh_Load();
    }
    e = Za_Entry();
    if (sZaPop) {
        if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) { /* (back: the box closes) */
            sZaPop = 0;
            return;
        }
        if (sZaPop == 1) {
            s32 n = 0;

            for (i = 0; i < sShRows && n < 15; i++) {
                s32 v = sShList[i][0];
                f32 bx, by;

                if (v == e) {
                    continue;
                }
                bx = 540.0f + (f32)(n % 3) * 445.0f, by = 290.0f + (f32)(n / 3) * 122.0f;
                n++;
                if (Zt_In(x, y, bx, by, bx + 430.0f, by + 108.0f)) {
                    if (v != SH_ALL && Sh_Style(v) == 8) {
                        Zw_Note(ZL("Essa parte j\303\241 copia outra.", "That part already copies another.", "Esa parte ya copia otra."));
                        return;
                    }
                    Sh_SetMode(e, v == SH_ALL ? 0 : 8, v == SH_ALL ? 0 : v);
                    Sh_Changed();
                    sZaPop = 0;
                }
            }
        } else if (sZaPop == 2) {
            for (i = 0; i < 5; i++) {
                if (Zt_In(x, y, 650.0f, 290.0f + i * 118.0f, 1750.0f, 394.0f + i * 118.0f) && (k = Sh_Edit(e, 1)) >= 0) {
                    sZw->w.edit[k][2] = (sZw->w.edit[k][2] & ~0xFF) | i;
                    Sh_Changed();
                    sZaPop = 0;
                }
            }
        } else {
            s32 pick = -1;

            for (i = 0; i < 2; i++) {
                pick = Zt_In(x, y, 540.0f + i * 670.0f, 330.0f, 1190.0f + i * 670.0f, 440.0f) ? i : pick;
            }
            for (i = 0; i < 16; i++) {
                f32 bx = 540.0f + (f32)(i % 4) * 335.0f, by = 470.0f + (f32)(i / 4) * 105.0f;

                pick = Zt_In(x, y, bx, by, bx + 315.0f, by + 90.0f) ? i + 2 : pick;
            }
            if (pick >= 0 && (k = Sh_Edit(e, 1)) >= 0) {
                sZw->w.edit[k][2] = (sZw->w.edit[k][2] & ~0xFF00) | pick << 8;
                Sh_Changed();
                sZaPop = 0;
            }
        }
        if (Zt_In(x, y, 900.0f, 905.0f, 1500.0f, 1000.0f)) {
            sZaPop = 0;
        }
        Snd_PlaySe(1, 1);
        return;
    }
    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f) || Zt_In(x, y, 1855.0f, 945.0f, 2360.0f, 1060.0f)) { /* back to the editor (the changes stay in it) */
        sZw->shByTex = 0;
        sZw->shFocus = 0;
        Zw_SetMode(ZW_EDIT);
        Snd_PlaySe(1, 2);
        return;
    }
    if (y >= 30.0f && y <= 150.0f) {
        if (x >= 1060.0f && x <= 1420.0f) {
            sZw->shByTex = 0;
        } else if (x >= 1440.0f && x <= 1800.0f && !sZw->shByTex) {
            sZw->shByTex = 1;
            Co_Load();
            sZw->shTexRow = sZw->coRows > 1 ? 1 : 0;
            sZw->shTexTop = 0;
        } else if (x >= 1950.0f && sZw->dirty) {
            sZw->saving = 2;
        } else {
            return;
        }
        Snd_PlaySe(1, 1);
        return;
    }
    sZw->shFocus = 0;
    if (!sZw->shByTex) {
        for (i = 0; i < sShRows; i++) {
            Za_PartRect(i, r);
            if (Zt_In(x, y, r[0], r[1] - 6.0f, r[2], r[3] + 6.0f)) {
                sZw->shRow = i;
                Snd_PlaySe(1, 0);
                return;
            }
        }
        if (x >= 1370.0f && y >= 270.0f && y < 810.0f) {
            k = (s32)((y - 270.0f) / 135.0f);
            if (e == SH_ALL) {
                k = k == 0 ? 0 : k == 1 ? 2 : -1; /* (the whole body: colour and glow) */
            }
            if (k == 0) {
                Za_Colour(1);
            } else if (k >= 1 && k <= 3) {
                sZaPop = k;
            } else {
                return;
            }
            Snd_PlaySe(1, 1);
        } else if (Zt_In(x, y, 1340.0f, 945.0f, 1845.0f, 1060.0f)) {
            k = Sh_Edit(e, 0);
            if (k >= 0) {
                memmove(sZw->w.edit[k], sZw->w.edit[k + 1], sizeof(sZw->w.edit[0]) * (u32)(sZw->w.edits - k - 1));
                sZw->w.edits--;
                Sh_Changed();
            }
            Zw_Note(ZL("Sombra como no jogo.", "Shaded as the game has it.", "Sombra como en el juego."));
            Snd_PlaySe(1, 2);
        }
        return;
    }
    if (x <= 760.0f) { /* the textures */
        if (y < 222.0f) {
            sZw->shTexTop = sZw->shTexTop > 4 ? sZw->shTexTop - 4 : 0;
        } else if (y >= 995.0f) {
            sZw->shTexTop = sZw->shTexTop + 8 < sZw->coRows ? sZw->shTexTop + 4 : sZw->shTexTop;
        } else {
            i = sZw->shTexTop + (s32)((y - 225.0f) / 96.0f);
            if (i >= 0 && i < sZw->coRows && sZw->coList[i] >= 0) {
                sZw->shTexRow = i;
            }
        }
        Snd_PlaySe(1, 0);
        return;
    }
    if (sZw->shTexRow < 0 || sZw->shTexRow >= sZw->coRows || sZw->coList[sZw->shTexRow] < 0) {
        return;
    }
    {
        s32 z = sZw->coList[sZw->shTexRow], own = St_Colour(z);

        if (Zt_In(x, y, 1390.0f, 455.0f, 1750.0f, 545.0f) || Zt_In(x, y, 1340.0f, 945.0f, 1845.0f, 1060.0f)) {
            St_Set(z, 0);
        } else if (Zt_In(x, y, 1770.0f, 455.0f, 2310.0f, 545.0f)) {
            Za_Colour(0);
            Snd_PlaySe(1, 1);
            return;
        } else if (x >= 1372.0f && y >= 563.0f && y < 815.0f) {
            i = (s32)((y - 563.0f) / 84.0f) * 10 + (s32)((x - 1372.0f) / 96.0f);
            if (i < 0 || i >= 30) {
                return;
            }
            St_Set(z, (s32)(0x1000000u | kSwatch[i]));
        } else if (x >= 1575.0f && y >= 820.0f && y <= 912.0f && own != 0) {
            i = (s32)((x - 1575.0f) / 83.0f);
            if (i < 0 || i > 8) {
                return;
            }
            St_Set(z, (own & 0x1FFFFFF) | ((i + 1) << 25));
        } else {
            return;
        }
        Sh_Changed();
        Snd_PlaySe(1, 0);
    }
}
#undef ZL
