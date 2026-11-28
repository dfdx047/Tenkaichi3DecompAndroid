/* Colours and Paint textures in the Android style (templates approved 2026-11-27, mods-zwarriors/previa_pintura.py).
   Touch presses what is under the finger; one tap paints one dab (the engine gets taps, not drags). The pad still
   works (Co_Update, Tx_Update). Template pixels (Zt_*). Included by port_zw.c after port_zw_shade.inc. */
#define ZL(pt, en, es) Zo_L(pt, en, es)
static s32 sZpPop;  /* paint: 1 the brush's size, 2 wipe? */
static s32 sZpTop;  /* paint: the first texture shown */

/* The picture of sZw->pic in the box (x, y, size), template pixels; out: where it went and a pixel's size. */
static void Zp_Canvas(f32 x, f32 y, f32 size, s32 cur, f32 *px, f32 *py, f32 *sc) {
    s32 w = sZw->picW, h = sZw->picH;

    *px = x, *py = y, *sc = 0.0f;
    if (w <= 0 || h <= 0) {
        return;
    }
    *sc = size / (f32)(w > h ? w : h);
    *px = x + (size - (f32)w * *sc) * 0.5f;
    *py = y + (size - (f32)h * *sc) * 0.5f;
    Zt_Box(ZW_T_ASK, x - 16.0f, y - 16.0f, x + size + 16.0f, y + size + 16.0f, 24.0f, 0x80);
    if (!(sZw->mode == ZW_TEXTURE && sTxFocus == 0)) { /* (drawn or pointed on: the pad takes hold of it) */
        Zf_Add(x, y, x + size, y + size, 1);
    }
    Co_PicTexture(cur, 0);
    Mx_PicEx(TBO_PICTURE, ZT_X(*px), ZT_Y(*py), ZT_X(*px + w * *sc), ZT_Y(*py + h * *sc), 0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
}

static s32 Zp_CanvasHit(f32 x, f32 y, f32 bx, f32 by, f32 size) {
    s32 w = sZw->picW, h = sZw->picH;
    f32 sc, px, py;

    if (w <= 0 || h <= 0) {
        return 0;
    }
    sc = size / (f32)(w > h ? w : h);
    px = bx + (size - w * sc) * 0.5f, py = by + (size - h * sc) * 0.5f;
    if (x < px || y < py || x >= px + w * sc || y >= py + h * sc) {
        return 0;
    }
    sZw->picX = (s32)((x - px) / sc);
    sZw->picY = (s32)((y - py) / sc);
    return 1;
}

static void Zp_Frame(f32 px, f32 py, f32 sc) { /* the pixel last touched */
    if (sc > 0.0f) {
        Mx_Frame(ZT_X(px + sZw->picX * sc - 4.0f), ZT_Y(py + sZw->picY * sc - 4.0f), ZT_X(px + (sZw->picX + 1) * sc + 4.0f),
                 ZT_Y(py + (sZw->picY + 1) * sc + 4.0f), ZT_S(6.0f), Mx_Rgba(255, 255, 255, 0x80));
    }
}

static void Zp_Save(char *title, char *sub) {
    Zt_Header(title, sub);
    Zt_Pill(ZW_T_GREEN, 1950.0f, 38.0f, 2340.0f, 138.0f, sZw->dirty ? ZL("SALVAR", "SAVE", "GUARDAR") : ZL("SALVO", "SAVED", "GUARDADO"), 40.0f, 0x80);
}

/* ---- colours ---- */
static char *Zp_Colours(void) {
    return ZL("CORES", "COLOURS", "COLORES");
}

static void Zp_ColourDraw(void) {
    s32 i, at, k = Co_Region(), stage = sZw->colourStage;
    f32 px, py, sc;
    char line[64];

    Co_PaintTick();
    Ws_ShapeTick();
    if (stage == 3) { /* the picture: the colour under the finger */
        s32 cur = sZw->picW > 0 ? sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15 : 0;

        Zp_Save(Zp_Colours(), ZL("Toque na imagem, na cor que quer trocar", "Tap the colour you want to change", "Toca el color que quieres cambiar"));
        Zp_Canvas(300.0f, 200.0f, 760.0f, cur, &px, &py, &sc);
        Zp_Frame(px, py, sc);
        Zt_Box(ZW_T_ASK, 1340.0f, 165.0f, 2360.0f, 930.0f, 34.0f, 0x80);
        Zt_Text(1850.0f, 215.0f, ZL("CORES DESTA TEXTURA", "THIS TEXTURE'S COLOURS", "COLORES DE LA TEXTURA"), 38.0f, 960.0f, ZT_GOLD, 1, 0x80);
        for (i = 0; i < 16; i++) {
            Za_Dot(1450.0f + (f32)(i % 4) * 265.0f, 330.0f + (f32)(i / 4) * 130.0f, 90.0f, sZw->picPal[i] & 0xFFFFFF, i == cur);
        }
        Zt_Pill(ZW_T_RED, 1340.0f, 945.0f, 1675.0f, 1050.0f, ZL("Voltar", "Back", "Volver"), 38.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1690.0f, 945.0f, 2025.0f, 1050.0f, ZL("Original", "Original", "Original"), 36.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 2040.0f, 945.0f, 2360.0f, 1050.0f, ZL("Inteira", "All of it", "Entera"), 36.0f, 0x80);
    } else if (stage != 0 && k >= 0) { /* the colour */
        s32 own = (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF;

        Zw_EditStage(ZT_X(700.0f), ZT_Y(900.0f), ZT_Y(620.0f));
        Zp_Save(Zp_Colours(), Co_RegionName(k));
        Zt_Box(ZW_T_ASK, 1340.0f, 165.0f, 2360.0f, 930.0f, 34.0f, 0x80);
        Zt_Text(1850.0f, 215.0f, ZL("SUA COR", "YOUR COLOUR", "TU COLOR"), 40.0f, 500.0f, ZT_GOLD, 1, 0x80);
        Za_Dot(1420.0f, 212.0f, 54.0f, Co_FromHsv(sZw->pkH, sZw->pkS, sZw->pkV), 0);
        Za_Dot(2290.0f, 212.0f, 50.0f, (u32)sZw->reg[k * 4 + 1] & 0xFFFFFF, 0);
        Za_Slider(0, ZL("Tom", "Hue", "Tono"), sZw->pkH);
        Za_Slider(1, ZL("For\303\247a", "Strength", "Fuerza"), sZw->pkS);
        Za_Slider(2, ZL("Luz", "Light", "Luz"), sZw->pkV);
        for (i = 0; i < 30; i++) {
            Za_Dot(1420.0f + (f32)(i % 10) * 96.0f, 680.0f + (f32)(i / 10) * 84.0f, 62.0f, kSwatch[i], 0);
        }
        Zt_Pill(ZW_T_NAVY, 1030.0f, 340.0f, 1310.0f, 440.0f,
                own ? ZL("Cores pr\303\263prias", "Own colours", "Colores propios") : sZw->pkOnly == 0 ? ZL("Todo lugar", "Everywhere", "En todo")
                                                                                       : ZL("S\303\263 uma parte", "One part", "Solo una parte"), 30.0f, 0x80);
        Zt_Text(1170.0f, 300.0f, ZL("S\303\223 EM", "ONLY ON", "SOLO EN"), 30.0f, 280.0f, ZT_GOLD, 1, 0x80);
        Zt_Pill(ZW_T_RED, 1340.0f, 945.0f, 1675.0f, 1050.0f, ZL("Cancelar", "Cancel", "Cancelar"), 36.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1690.0f, 945.0f, 2025.0f, 1050.0f, ZL("Original", "Original", "Original"), 36.0f, 0x80);
        Zt_Pill(ZW_T_GREEN, 2040.0f, 945.0f, 2360.0f, 1050.0f, ZL("Manter", "Keep", "Mantener"), 40.0f, 0x80);
        Zc_Frame(1);
    } else { /* the textures */
        Zw_EditStage(ZT_X(1440.0f), ZT_Y(900.0f), ZT_Y(620.0f));
        Zp_Save(Zp_Colours(), ZL("Toque na textura que quer mudar", "Tap the texture to change", "Toca la textura a cambiar"));
        if (sZw->coCur - 1 < sZw->coTop) {
            sZw->coTop = sZw->coCur - 1 < 0 ? 0 : sZw->coCur - 1;
        }
        if (sZw->coCur >= sZw->coTop + 7) {
            sZw->coTop = sZw->coCur - 6;
        }
        Zt_Box(ZW_T_ASK, 40.0f, 165.0f, 900.0f, 1050.0f, 34.0f, 0x80);
        for (i = 0; i < 7 && sZw->coTop + i < sZw->coRows; i++) {
            s32 row = sZw->coTop + i, z = sZw->coList[row];
            f32 y = 225.0f + (f32)i * 112.0f;

            if (z < 0) {
                Zt_Text(80.0f, y + 55.0f, z == -1 ? ZL("CABE\303\207A", "HEAD", "CABEZA") : z == -2 ? ZL("CORPO", "BODY", "CUERPO") : ZL("DETALHES", "DETAILS", "DETALLES"),
                        30.0f, 700.0f, ZT_GOLD, 0, 0x80);
                continue;
            }
            Zt_Box(row == sZw->coCur ? ZW_T_SEL : ZW_T_PANEL, 65.0f, y, 875.0f, y + 102.0f, 28.0f, 0x80);
            Za_Dot(125.0f, y + 51.0f, 62.0f, Co_Now(z, &at) & 0xFFFFFF, 0);
            Zt_Text(185.0f, y + 51.0f, Co_RegionName(z), 34.0f, at >= 0 ? 460.0f : 640.0f, ZT_WHITE, 0, 0x80);
            if (at >= 0) {
                Za_Dot(700.0f, y + 51.0f, 40.0f, (z < sZw->regs ? sZw->regWas[z] : (u32)sZw->reg[z * 4 + 1]) & 0xFFFFFF, 0);
                Zt_Text(750.0f, y + 51.0f, ">", 36.0f, 40.0f, ZT_GOLD, 1, 0x80);
                Za_Dot(805.0f, y + 51.0f, 54.0f, Co_Now(z, &at) & 0xFFFFFF, 0);
            }
        }
        if (sZw->coTop > 0) {
            Mx_Fit(ZW_UP, ZT_X(470.0f), ZT_Y(195.0f), ZT_S(60.0f), 0x80);
            Zf_Add(470.0f - 50.0f, 195.0f - 30.0f, 470.0f + 50.0f, 195.0f + 30.0f, 0);
        }
        if (sZw->coTop + 7 < sZw->coRows) {
            Mx_Fit(ZW_DOWN, ZT_X(470.0f), ZT_Y(1025.0f), ZT_S(60.0f), 0x80);
            Zf_Add(470.0f - 50.0f, 1025.0f - 30.0f, 470.0f + 50.0f, 1025.0f + 30.0f, 0);
        }
        Zt_Pill(ZW_T_BAR, 1960.0f, 260.0f, 2340.0f, 360.0f, ZL("Abrir textura", "Open texture", "Abrir textura"), 34.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1960.0f, 380.0f, 2340.0f, 480.0f, ZL("Textura inteira", "Whole texture", "Textura entera"), 34.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1960.0f, 500.0f, 2340.0f, 600.0f, ZL("Original", "Original", "Original"), 34.0f, 0x80);
        Zt_Pill(ZW_T_RED, 1960.0f, 620.0f, 2340.0f, 720.0f, ZL("Tudo original", "All original", "Todo original"), 34.0f, 0x80);
        sprintf(line, "%d / %d %s", (int)sZw->w.colours, (int)ZW_COLOURS, ZL("cores", "colours", "colores"));
        Zt_Text(2150.0f, 790.0f, line, 30.0f, 380.0f, ZT_DIM, 1, 0x80);
        Zt_Pill(ZW_T_GREEN, 1960.0f, 945.0f, 2340.0f, 1050.0f, ZL("Pronto", "Done", "Listo"), 40.0f, 0x80);
    }
    if (k >= 0 && !sZw->saving) {
        Glow_Update(stage == 0 ? sZw->reg[k * 4] : 0, (sZw->reg[k * 4 + 3] >> 16) & 0x1FFF, 1);
    } else {
        Glow_Off();
    }
    Za_Note();
}

static void Zp_ColourTap(f32 x, f32 y) {
    s32 stage = sZw->colourStage, i;

    if (stage == 1) {
        sZcTouch = 1;
        sZcEdit = 0;
    }

    if (sZw->saving) {
        return;
    }
    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) {
        Zt_Press(PADG_TRIANGLE);
        return;
    }
    if (stage == 0 && Zt_In(x, y, 1950.0f, 30.0f, 2350.0f, 145.0f)) {
        if (sZw->dirty) {
            Zt_Press(PADG_START);
        }
        return;
    }
    if (stage == 3) {
        if (Zp_CanvasHit(x, y, 300.0f, 200.0f, 760.0f)) {
            Zt_Press(PADG_CROSS);
        } else if (x >= 1318.0f && y >= 265.0f && y < 785.0f) {
            s32 k = (s32)((y - 265.0f) / 130.0f) * 4 + (s32)((x - 1318.0f) / 265.0f);

            for (i = 0; k >= 0 && k < 16 && i < sZw->picW * sZw->picH; i++) {
                if ((sZw->pic[i] & 15) == k) {
                    sZw->picX = i % sZw->picW;
                    sZw->picY = i / sZw->picW;
                    Zt_Press(PADG_CROSS);
                    break;
                }
            }
        } else if (y >= 945.0f && x >= 1340.0f) {
            Zt_Press(x < 1680.0f ? PADG_TRIANGLE : x < 2030.0f ? PADG_SQUARE : PADG_CIRCLE);
        }
        return;
    }
    if (stage != 0) {
        if (Zt_In(x, y, 1030.0f, 340.0f, 1310.0f, 440.0f)) {
            sZw->pkRow = CP_ONLY;
            Zt_Press(((sZw->reg[Co_Region() * 4 + 3] >> 16) & 0x1FFF) ? PADG_CROSS : PADG_RIGHT);
        } else if (y >= 945.0f && x >= 1340.0f) {
            sZw->pkRow = CP_HUE;
            Zt_Press(x < 1680.0f ? PADG_TRIANGLE : x < 2030.0f ? PADG_SQUARE : PADG_CROSS);
        } else if (x >= 1360.0f && y >= 290.0f && y < 650.0f) {
            s32 k = (s32)((y - 290.0f) / 125.0f);
            f32 v = (x - 1390.0f) / 920.0f;

            v = v < 0.0f ? 0.0f : v > 0.999f ? 0.999f : v;
            if (k >= 0 && k < 3) {
                *(k == 0 ? &sZw->pkH : k == 1 ? &sZw->pkS : &sZw->pkV) = v;
                Co_Apply();
                Snd_PlaySe(1, 0);
            }
        } else if (x >= 1372.0f && y >= 640.0f && y < 890.0f) {
            i = (s32)((y - 638.0f) / 84.0f) * 10 + (s32)((x - 1372.0f) / 96.0f);
            if (i >= 0 && i < 30) {
                Co_ToHsv(kSwatch[i], &sZw->pkH, &sZw->pkS, &sZw->pkV);
                Co_Apply();
                Snd_PlaySe(1, 0);
            }
        }
        return;
    }
    if (x <= 900.0f) {
        if (y < 222.0f) {
            Zt_Press(PADG_UP);
        } else if (y >= 1005.0f) {
            Zt_Press(PADG_DOWN);
        } else {
            i = sZw->coTop + (s32)((y - 225.0f) / 112.0f);
            if (i >= 0 && i < sZw->coRows && sZw->coList[i] >= 0) {
                if (sZw->coCur == i) {
                    Zt_Press(PADG_CROSS);
                } else {
                    sZw->coCur = i;
                    Snd_PlaySe(1, 0);
                }
            }
        }
    } else if (x >= 1960.0f) {
        if (y >= 260.0f && y < 740.0f) {
            static const u32 press[4] = {PADG_CROSS, PADG_R1, PADG_SQUARE, PADG_CIRCLE};

            Zt_Press(press[(s32)((y - 260.0f) / 120.0f)]);
        } else if (y >= 945.0f) {
            Zt_Press(PADG_TRIANGLE);
        }
    }
}

/* ---- paint textures ---- */
static char *Zp_Paint(void) {
    return ZL("PINTAR TEXTURAS", "PAINT TEXTURES", "PINTAR TEXTURAS");
}

static void Zp_PaintDraw(void) {
    s32 i, cur = 0;
    f32 px, py, sc;
    char name[40], line[64];

    if (sTxN > 0 && sZw->picW > 0) {
        cur = sZw->pic[sZw->picY * sZw->picW + sZw->picX] & 15;
    }
    if (sTxPlace) { /* a picture being put on */
        Zp_Save(Zp_Paint(), ZL("Toque na textura para posicionar a imagem", "Tap the texture to place the picture", "Toca la textura para colocar la imagen"));
        Zp_Canvas(700.0f, 190.0f, 700.0f, cur, &px, &py, &sc);
        Zt_Box(ZW_T_ASK, 1560.0f, 165.0f, 2360.0f, 930.0f, 34.0f, 0x80);
        Zt_Text(1960.0f, 215.0f, ZL("IMAGEM", "PICTURE", "IMAGEN"), 38.0f, 700.0f, ZT_GOLD, 1, 0x80);
        Zt_Text(1610.0f, 285.0f, ZL("Tamanho", "Size", "Tama\303\261o"), 34.0f, 300.0f, ZT_WHITE, 0, 0x80);
        Zt_Pill(ZW_T_BAR, 1610.0f, 320.0f, 1945.0f, 420.0f, "-", 48.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1975.0f, 320.0f, 2310.0f, 420.0f, "+", 48.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1610.0f, 440.0f, 2310.0f, 540.0f, ZL("Girar", "Turn", "Girar"), 36.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1610.0f, 570.0f, 2310.0f, 670.0f, ZL("Escolher outra imagem", "Choose another picture", "Elegir otra imagen"), 34.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1610.0f, 690.0f, 2310.0f, 790.0f, ZL("Outra desta textura", "Another of this texture", "Otra de esta textura"), 34.0f, 0x80);
        Zt_Pill(sTxLayer >= 0 ? ZW_T_BAR : ZW_T_GREY, 1610.0f, 810.0f, 2310.0f, 910.0f, ZL("Tirar a imagem", "Take it off", "Quitar la imagen"), 34.0f, 0x80);
        Zt_Pill(ZW_T_RED, 1560.0f, 945.0f, 1955.0f, 1050.0f, ZL("Cancelar", "Cancel", "Cancelar"), 38.0f, 0x80);
        Zt_Pill(ZW_T_GREEN, 1965.0f, 945.0f, 2360.0f, 1050.0f, ZL("Aplicar", "Apply", "Aplicar"), 40.0f, 0x80);
    } else if (sTxFocus == 0) { /* choosing the texture */
        Zw_EditStage(ZT_X(2020.0f), ZT_Y(900.0f), ZT_Y(560.0f));
        Zp_Save(Zp_Paint(), ZL("Escolha uma textura para desenhar", "Choose a texture to draw on", "Elige una textura para dibujar"));
        sZpTop = sTxCur < sZpTop ? sTxCur : sTxCur >= sZpTop + 7 ? sTxCur - 6 : sZpTop;
        Zt_Box(ZW_T_ASK, 40.0f, 165.0f, 640.0f, 1050.0f, 34.0f, 0x80);
        Zt_Text(70.0f, 208.0f, ZL("TEXTURAS", "TEXTURES", "TEXTURAS"), 32.0f, 400.0f, ZT_GOLD, 0, 0x80);
        for (i = 0; i < 7 && sZpTop + i < sTxN; i++) {
            s32 k = sZpTop + i;
            f32 y = 245.0f + (f32)i * 110.0f;

            Zt_Box(k == sTxCur ? ZW_T_SEL : ZW_T_PANEL, 60.0f, y, 620.0f, y + 98.0f, 26.0f, 0x80);
            Tx_Name(k, name);
            Zt_Text(95.0f, y + 49.0f, name, 34.0f, 340.0f, ZT_WHITE, 0, 0x80);
            snprintf(line, sizeof(line), "%dx%d", (int)(sTxInfo[k] & 0xFFF), (int)((sTxInfo[k] >> 12) & 0xFFF));
            Zt_Text(595.0f, y + 49.0f, line, 26.0f, 150.0f, ZT_DIM, 2, 0x80);
        }
        if (sZpTop > 0) {
            Mx_Fit(ZW_UP, ZT_X(560.0f), ZT_Y(208.0f), ZT_S(54.0f), 0x80);
            Zf_Add(560.0f - 50.0f, 208.0f - 30.0f, 560.0f + 50.0f, 208.0f + 30.0f, 0);
        }
        if (sZpTop + 7 < sTxN) {
            Mx_Fit(ZW_DOWN, ZT_X(340.0f), ZT_Y(1030.0f), ZT_S(54.0f), 0x80);
            Zf_Add(340.0f - 50.0f, 1030.0f - 30.0f, 340.0f + 50.0f, 1030.0f + 30.0f, 0);
        }
        if (sTxN <= 0) {
            Zt_Text(1140.0f, 540.0f, ZL("Nenhuma textura para pintar.", "No textures to draw on.", "No hay texturas."), 36.0f, 640.0f, ZT_DIM, 1, 0x80);
        } else {
            Zp_Canvas(820.0f, 230.0f, 640.0f, cur, &px, &py, &sc);
        }
        Zt_Pill(ZW_T_GREEN, 760.0f, 945.0f, 1060.0f, 1050.0f, ZL("Desenhar", "Draw", "Dibujar"), 38.0f, 0x80);
        Zt_Pill(ZW_T_BAR, 1080.0f, 945.0f, 1520.0f, 1050.0f, ZL("P\303\264r uma imagem", "Put a picture", "Poner una imagen"), 34.0f, 0x80);
    } else { /* drawing */
        char *tool[9];

        tool[0] = ZL("Pincel", "Brush", "Pincel");
        tool[1] = ZL("Balde", "Fill", "Cubo");
        tool[2] = ZL("Conta-gotas", "Pick", "Gotero");
        tool[3] = ZL("Varinha", "Wand", "Varita");
        snprintf(line, sizeof(line), "%s %d", ZL("Tamanho", "Size", "Tama\303\261o"), (int)sZw->picSize + 1);
        tool[4] = line;
        tool[5] = ZL("Desfazer", "Undo", "Deshacer");
        tool[6] = ZL("Refazer", "Redo", "Rehacer");
        tool[7] = ZL("Imagem", "Picture", "Imagen");
        tool[8] = ZL("Limpar", "Wipe", "Limpiar");
        Zw_EditStage(ZT_X(2020.0f), ZT_Y(900.0f), ZT_Y(560.0f));
        Tx_Name(sTxCur, name);
        Zp_Save(Zp_Paint(), name);
        Zt_Box(ZW_T_ASK, 40.0f, 165.0f, 440.0f, 1050.0f, 34.0f, 0x80);
        Zt_Text(240.0f, 208.0f, ZL("FERRAMENTAS", "TOOLS", "HERRAMIENTAS"), 32.0f, 360.0f, ZT_GOLD, 1, 0x80);
        for (i = 0; i < 9; i++) {
            f32 x = 60.0f + (f32)(i % 2) * 185.0f, y = 245.0f + (f32)(i / 2) * 158.0f;
            s32 on = (i == 0 && sTxTool == TXT_BRUSH) || (i == 1 && sTxTool == TXT_FILL) || (i == 2 && sTxTool == TXT_PICK) || (i == 3 && sTxTool == TXT_TOOLS);

            Zt_Pill(on ? ZW_T_ORANGE : ZW_T_NAVY, x, y, i == 8 ? x + 360.0f : x + 175.0f, y + 140.0f, tool[i], 27.0f, 0x80);
        }
        Zp_Canvas(700.0f, 190.0f, 700.0f, cur, &px, &py, &sc);
        Zp_Frame(px, py, sc);
        Zt_Box(ZW_T_ASK, 480.0f, 930.0f, 1680.0f, 1060.0f, 34.0f, 0x80);
        for (i = 0; i < 16; i++) {
            Za_Dot(525.0f + (f32)i * 66.0f, 995.0f, 52.0f, sZw->picPal[i] & 0xFFFFFF, i == sZw->picBrush);
        }
        Zt_Pill(ZW_T_GREEN, 1585.0f, 955.0f, 1665.0f, 1035.0f, "+", 44.0f, 0x80);
        if (sZpPop != 0) {
            sZfN = 0;
        }
        if (sZpPop == 1) {
            Zt_Box(ZW_T_ASK, 470.0f, 300.0f, 1250.0f, 470.0f, 34.0f, 0x80);
            for (i = 0; i < 5; i++) {
                f32 x = 555.0f + (f32)i * 150.0f;
                u32 white = Mx_Rgba(255, 255, 255, 0x80);

                Zt_Pill(i == sZw->picSize ? ZW_T_ORANGE : ZW_T_NAVY, x - 65.0f, 320.0f, x + 65.0f, 450.0f, "", 30.0f, 0x80);
                Mx_Disc(ZT_X(x), ZT_Y(385.0f), ZT_S(20.0f + i * 17.0f), white, white);
            }
        } else if (sZpPop == 2) {
            Zt_Box(ZW_T_ASK, 650.0f, 330.0f, 1750.0f, 750.0f, 48.0f, 0x80);
            Zt_Text(1200.0f, 420.0f, ZL("LIMPAR O DESENHO?", "WIPE THE DRAWING?", "\302\277BORRAR EL DIBUJO?"), 52.0f, 1000.0f, ZT_GOLD, 1, 0x80);
            Zt_Text(1200.0f, 520.0f, ZL("A textura volta a ser como era.", "The texture goes back to how it was.", "La textura vuelve a como era."), 34.0f, 1000.0f, ZT_WHITE, 1, 0x80);
            Zt_Pill(ZW_T_BAR, 710.0f, 620.0f, 1180.0f, 720.0f, ZL("Cancelar", "Cancel", "Cancelar"), 38.0f, 0x80);
            Zt_Pill(ZW_T_RED, 1220.0f, 620.0f, 1690.0f, 720.0f, ZL("Limpar", "Wipe", "Borrar"), 38.0f, 0x80);
        }
    }
    Za_Note();
}

static void Zp_PaintTap(f32 x, f32 y) {
    s32 i;

    if (sZw->saving || sZw->picImport) {
        sZw->picImport = 0; /* (a tap: the wait for a file given up) */
        return;
    }
    if (sTxPlace) {
        if (Zp_CanvasHit(x, y, 700.0f, 190.0f, 700.0f)) {
            sTxPX = (f32)sZw->picX;
            sTxPY = (f32)sZw->picY;
        } else if (Zt_In(x, y, 1610.0f, 320.0f, 1945.0f, 420.0f)) {
            sTxPW *= 0.85f;
        } else if (Zt_In(x, y, 1975.0f, 320.0f, 2310.0f, 420.0f)) {
            sTxPW *= 1.18f;
        } else if (Zt_In(x, y, 1610.0f, 440.0f, 2310.0f, 540.0f)) {
            sTxTurn = (sTxTurn + 1) & 3;
        } else if (Zt_In(x, y, 1610.0f, 570.0f, 2310.0f, 670.0f)) {
            Zt_Press(PADG_R3);
        } else if (Zt_In(x, y, 1610.0f, 690.0f, 2310.0f, 790.0f)) {
            Zt_Press(PADG_R2);
        } else if (Zt_In(x, y, 1610.0f, 810.0f, 2310.0f, 910.0f) && sTxLayer >= 0) {
            Zt_Press(PADG_CIRCLE);
        } else if (Zt_In(x, y, 1560.0f, 945.0f, 1955.0f, 1060.0f) || Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) {
            Zt_Press(PADG_TRIANGLE);
        } else if (Zt_In(x, y, 1965.0f, 945.0f, 2360.0f, 1060.0f)) {
            Zt_Press(PADG_CROSS);
        }
        return;
    }
    if (sZpPop == 1) {
        if (y >= 300.0f && y <= 470.0f && x >= 490.0f && x < 1240.0f) {
            sZw->picSize = (s32)((x - 490.0f) / 150.0f);
        }
        sZpPop = 0;
        Snd_PlaySe(1, 1);
        return;
    }
    if (sZpPop == 2) {
        if (Zt_In(x, y, 1220.0f, 620.0f, 1690.0f, 720.0f)) {
            Tx_Wipe();
        }
        sZpPop = 0;
        return;
    }
    if (Zt_In(x, y, 40.0f, 20.0f, 195.0f, 150.0f)) {
        Tx_Store();
        if (sTxFocus == 1) {
            sTxFocus = 0;
            Snd_PlaySe(1, 2);
        } else {
            Tx_Leave();
        }
        return;
    }
    if (Zt_In(x, y, 1950.0f, 30.0f, 2350.0f, 145.0f)) {
        Tx_Store();
        if (sZw->dirty) {
            sZw->saving = 2;
            Snd_PlaySe(1, 1);
        }
        return;
    }
    if (sTxFocus == 0) {
        if (x <= 640.0f) {
            if (y >= 1005.0f) {
                sTxCur = sTxCur + 4 < sTxN ? sTxCur + 4 : sTxN - 1;
                sZpTop = sTxCur > 6 ? sTxCur - 6 : 0;
            } else if (y < 240.0f) {
                sTxCur = sTxCur > 4 ? sTxCur - 4 : 0;
            } else {
                i = sZpTop + (s32)((y - 245.0f) / 110.0f);
                if (i < 0 || i >= sTxN) {
                    return;
                }
                sTxCur = i;
            }
            Tx_Load(sTxCur);
            Snd_PlaySe(1, 0);
        } else if (Zt_In(x, y, 760.0f, 945.0f, 1060.0f, 1060.0f) && sTxN > 0) {
            sTxFocus = 1;
            sTxTool = TXT_BRUSH;
            Snd_PlaySe(1, 1);
        } else if (Zt_In(x, y, 1080.0f, 945.0f, 1520.0f, 1060.0f) && sTxN > 0) {
            Tx_Pictures();
        }
        return;
    }
    if (Zp_CanvasHit(x, y, 700.0f, 190.0f, 700.0f)) {
        if (sTxTool == TXT_BRUSH) {
            Tx_Stroke(1);
            Tx_Store();
        } else if (sTxTool == TXT_FILL) {
            Tx_Keep();
            Co_PicFill(sZw->picX, sZw->picY);
            sZw->picDirty = 1;
            Tx_Store();
        } else if (sTxTool == TXT_PICK) {
            Tx_Pick();
            sTxTool = TXT_BRUSH;
        } else { /* the wand */
            Tx_Wand();
            Tx_Store();
            sTxTool = TXT_BRUSH;
        }
        return;
    }
    if (x <= 440.0f && y >= 245.0f) {
        i = (s32)((y - 245.0f) / 158.0f) * 2 + (x >= 245.0f);
        i = i >= 8 ? 8 : i;
        if (i < 4) {
            sTxTool = i == 0 ? TXT_BRUSH : i == 1 ? TXT_FILL : i == 2 ? TXT_PICK : TXT_TOOLS; /* (TXT_TOOLS: the wand) */
            Snd_PlaySe(1, 0);
        } else if (i == 4) {
            sZpPop = 1;
        } else if (i == 5 || i == 6) {
            Tx_UseTool(i == 5 ? TXT_UNDO : TXT_REDO);
        } else if (i == 7) {
            Tx_Pictures();
        } else {
            sZpPop = 2;
        }
        return;
    }
    if (y >= 930.0f && x >= 495.0f) {
        if (x >= 1575.0f && x <= 1690.0f) {
            Tx_NewColour();
        } else {
            i = (s32)((x - 492.0f) / 66.0f);
            if (i >= 0 && i < 16) {
                sZw->picBrush = i;
                Snd_PlaySe(1, 0);
            }
        }
    }
}
#undef ZL
