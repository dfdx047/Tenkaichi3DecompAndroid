// The in-game menu of the Android build (Dragon Rage), included by ui.cpp in place of the PC's settings window.
// Made for fingers and for a controller: a side bar of pages, large switches, choices as rows of buttons and wide
// sliders, in the launcher's colours (night blue, orange, gold). Only what an Android player can use is here: no
// full screen, display, V-sync or keyboard bindings. Its text is in English, Portuguese or Spanish: the launcher's
// language (setting "dr_lang", written by the app), else the system's.
#include <sys/system_properties.h>

extern "C" int gGsFragDepth; // gs_draw.c: draws may write depth from the fragment shader (precise depth)

static int sLang = -1; // 0 English, 1 Portuguese, 2 Spanish

static int lang(void) {
    if (sLang < 0) {
        int s = Port_Setting("dr_lang", -1);
        sLang = 0;
        if (s >= 0 && s <= 2) {
            sLang = s;
        } else {
            int n = 0;
            SDL_Locale **l = SDL_GetPreferredLocales(&n);
            if (l != NULL && n > 0 && l[0] != NULL && l[0]->language != NULL) {
                if (strncmp(l[0]->language, "pt", 2) == 0) { sLang = 1; }
                else if (strncmp(l[0]->language, "es", 2) == 0) { sLang = 2; }
            }
            SDL_free(l);
        }
    }
    return sLang;
}
#define T(en, pt, es) (lang() == 1 ? (pt) : lang() == 2 ? (es) : (en))

static const ImVec4 kOrange(0.98f, 0.55f, 0.13f, 1.0f), kGold(1.0f, 0.82f, 0.36f, 1.0f), kDim(0.66f, 0.69f, 0.80f, 1.0f);
static const ImVec4 kCard(0.105f, 0.115f, 0.185f, 0.96f), kNight(0.05f, 0.06f, 0.11f, 0.97f);

static float fs(void) { return ImGui::GetFontSize(); }

static void dim_text(const char *text) {
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static void card_begin(const char *id, const char *title) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCard);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, fs() * 0.7f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(fs() * 1.0f, fs() * 0.8f));
    ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened);
    ImGui::PushStyleColor(ImGuiCol_Text, kGold);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + fs() * 2.2f, p.y + fs() * 0.12f), ImGui::GetColorU32(kOrange), fs() * 0.06f);
        ImGui::Dummy(ImVec2(0.0f, fs() * 0.35f));
    }
}

static void card_end(void) {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, fs() * 0.4f));
}

// A line of text with an on / off switch at its right; the whole line answers a touch.
static bool aswitch(const char *label, const char *desc, bool *v) {
    ImGui::PushID(label);
    const float w = ImGui::GetContentRegionAvail().x, sw = fs() * 2.6f, sh = fs() * 1.4f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w - sw - fs() * 0.8f);
    ImGui::TextUnformatted(label);
    if (desc != NULL && desc[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, kDim);
        ImGui::TextWrapped("%s", desc);
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const float h = ImGui::GetItemRectSize().y > sh ? ImGui::GetItemRectSize().y : sh;
    ImGui::SetCursorScreenPos(p);
    bool clicked = ImGui::InvisibleButton("row", ImVec2(w, h));
    if (clicked) { *v = !*v; }
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 a(p.x + w - sw, p.y + (h - sh) * 0.5f), b(p.x + w, p.y + (h + sh) * 0.5f);
    dl->AddRectFilled(a, b, ImGui::GetColorU32(*v ? kOrange : ImVec4(0.24f, 0.26f, 0.36f, 1.0f)), sh * 0.5f);
    const float r = sh * 0.38f, cx = *v ? b.x - sh * 0.5f : a.x + sh * 0.5f;
    dl->AddCircleFilled(ImVec2(cx, (a.y + b.y) * 0.5f), r, ImGui::GetColorU32(*v ? ImVec4(1.0f, 0.97f, 0.9f, 1.0f) : kDim), 24);
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible) {
        dl->AddRect(ImVec2(p.x - 4.0f, p.y - 4.0f), ImVec2(p.x + w + 4.0f, p.y + h + 4.0f), ImGui::GetColorU32(kGold), fs() * 0.3f, 0, 2.0f);
    }
    ImGui::Dummy(ImVec2(0.0f, fs() * 0.25f));
    ImGui::PopID();
    return clicked;
}

// A title and a row of buttons, one for each value; the one nearest to *v is lit. The row wraps.
static bool achoice(const char *label, const char *desc, int *v, const char *const *names, const int *vals, int n) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    int best = 0, d = 1 << 30;
    for (int i = 0; i < n; i++) {
        int e = abs(vals[i] - *v);
        if (e < d) { d = e; best = i; }
    }
    bool changed = false;
    const float avail = ImGui::GetContentRegionAvail().x, gap = ImGui::GetStyle().ItemSpacing.x;
    float x = 0.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, fs() * 0.6f);
    for (int i = 0; i < n; i++) {
        float bw = ImGui::CalcTextSize(names[i]).x + fs() * 1.6f;
        if (bw > avail) { bw = avail; }
        if (i > 0 && x + bw <= avail) {
            ImGui::SameLine();
        } else {
            x = 0.0f;
        }
        x += bw + gap;
        bool on = i == best;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, kOrange);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kOrange);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.07f, 0.02f, 1.0f));
        }
        ImGui::PushID(i);
        if (ImGui::Button(names[i], ImVec2(bw, fs() * 2.1f)) && !on) {
            *v = vals[i];
            changed = true;
        }
        ImGui::PopID();
        if (on) { ImGui::PopStyleColor(3); }
    }
    ImGui::PopStyleVar();
    if (desc != NULL && desc[0] != '\0') { dim_text(desc); }
    ImGui::Dummy(ImVec2(0.0f, fs() * 0.25f));
    ImGui::PopID();
    return changed;
}

// A title with the value at its right and a slider the whole width under it.
static bool aslider(const char *label, const char *desc, int *v, int lo, int hi, const char *unit) {
    ImGui::PushID(label);
    char val[32];
    snprintf(val, sizeof(val), "%d%s", *v, unit);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::CalcTextSize(val).x - ImGui::GetStyle().ItemSpacing.x);
    ImGui::TextColored(kOrange, "%s", val);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, fs() * 1.4f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(fs() * 0.6f, fs() * 0.45f));
    bool changed = ImGui::SliderInt("##s", v, lo, hi, "", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleVar(2);
    if (desc != NULL && desc[0] != '\0') { dim_text(desc); }
    ImGui::Dummy(ImVec2(0.0f, fs() * 0.25f));
    ImGui::PopID();
    return changed;
}

static void save_setting(const char *name, int value) {
    Port_SettingSave(name, value);
    Port_SettingsWrite();
}

// ---- the pages

static void page_video(PortVideo &v) {
    card_begin("res", T("Picture", "Imagem", "Imagen"));
    {
        static const int q[] = {4, 5, 6, 7, 8, 10, 12, 16, 20, 24};
        static const char *const names[] = {"1x", "1.25x", "1.5x", "1.75x", "2x", "2.5x", "3x", "4x", "5x", "6x"};
        char desc[200];
        snprintf(desc, sizeof(desc), T("%d x %d. 1x is the PlayStation 2's own. Higher is sharper and heavier for the GPU.",
                                       "%d x %d. 1x é a do PlayStation 2. Mais alta fica mais nítida e pesa mais na GPU.",
                                       "%d x %d. 1x es la del PlayStation 2. Más alta se ve más nítida y pesa más en la GPU."),
                 128 * v.scale4, 112 * v.scale4);
        achoice(T("Internal resolution", "Resolução interna", "Resolución interna"), desc, &v.scale4, names, q, 10);
    }
    {
        static const int vals[] = {0, 1333, 1600, 1778, 2222, 2389};
        const char *names[] = {T("Screen", "Tela", "Pantalla"), "4:3", "16:10", "16:9", "20:9", "21:9"};
        int a = v.aspectMilli;
        if (achoice(T("Aspect ratio", "Proporção", "Relación de aspecto"),
                    T("Screen: the shape of this device's screen, nothing squeezed. Wider than 4:3 shows more of the fight.",
                      "Tela: o formato da tela do aparelho, sem esticar. Mais largo que 4:3 mostra mais da luta.",
                      "Pantalla: la forma de la pantalla del equipo, sin estirar. Más ancho que 4:3 muestra más de la pelea."),
                    &a, names, vals, 6)) {
            v.aspectMilli = a;
        }
    }
    if (!sGL) {
        static const int vals[] = {0, 1, 2, 3, 4};
        const char *names[] = {T("Bilinear", "Bilinear", "Bilineal"), T("Sharp", "Nítido", "Nítido"), "FXAA", "FSR 1", "Snapdragon GSR"};
        achoice(T("Screen filter", "Filtro de tela", "Filtro de pantalla"),
                T("How the picture is scaled to the screen. FSR and GSR sharpen; best below 2x.",
                  "Como a imagem é ampliada para a tela. FSR e GSR deixam mais nítido; melhor abaixo de 2x.",
                  "Cómo se amplía la imagen a la pantalla. FSR y GSR la hacen más nítida; mejor debajo de 2x."),
                &v.filter, names, vals, 5);
    }
    {
        static const int vals[] = {0, 1};
        const char *names[] = {T("Sharp (console)", "Nítido (console)", "Nítido (consola)"), T("Smooth", "Suave", "Suave")};
        achoice(T("2D filtering", "Filtro 2D", "Filtro 2D"),
                T("The HUD, menus and text above 1x.", "HUD, menus e textos acima de 1x.", "HUD, menús y textos encima de 1x."),
                &v.smooth2d, names, vals, 2);
    }
    {
        bool on = v.texPack != 0;
        char label[96];
        snprintf(label, sizeof(label), T("Texture pack (%d textures)", "Pacote de texturas (%d texturas)", "Pack de texturas (%d texturas)"), v.texPackCount);
        if (v.texPackCount > 0) {
            if (aswitch(label, NULL, &on)) { v.texPack = on; }
        } else {
            dim_text(T("Texture pack: none installed (the launcher's Textures page).", "Pacote de texturas: nenhum instalado (página Texturas do app).",
                       "Pack de texturas: ninguno instalado (página Texturas de la app)."));
        }
    }
    card_end();

    card_begin("renderer", T("Renderer", "Renderizador", "Renderizador"));
    {
        static int api = -1;
        if (api < 0) { api = Port_Setting("gpu_api", 0) == 1 ? 1 : 0; }
        static const int vals[] = {0, 1};
        const char *names[] = {"Vulkan", "OpenGL ES"};
        if (achoice(T("Graphics API", "API gráfica", "API gráfica"),
                    T("From the next start of the game. If it cannot start, the other one is tried.",
                      "Vale a partir da próxima vez que o jogo abrir. Se não iniciar, o outro é tentado.",
                      "Vale desde el próximo inicio del juego. Si no arranca, se prueba el otro."),
                    &api, names, vals, 2)) {
            Port_SettingSave("gpu_api", api);
            Port_SettingSave("dr_renderer", api + 1);
            Port_SettingsWrite();
        }
        ImGui::TextColored(kDim, "%s: %s  ·  GPU: %s", T("Running now", "Rodando agora", "Ahora"), sGL ? "OpenGL ES" : "Vulkan", sGpuName);
    }
    card_end();
}

static void page_performance(PortVideo &v) {
    card_begin("speed", T("Speed", "Velocidade", "Velocidad"));
    {
        bool fsk = v.frameSkip != 0;
        if (aswitch(T("Skip pictures instead of slowing down", "Pular quadros em vez de ficar lento", "Saltar cuadros en vez de ir lento"),
                    T("When the GPU is behind, the game keeps its speed and leaves a picture out.",
                      "Quando a GPU não dá conta, o jogo mantém a velocidade e pula um quadro.",
                      "Cuando la GPU no alcanza, el juego mantiene la velocidad y salta un cuadro."), &fsk)) {
            v.frameSkip = fsk;
        }
    }
    {
        bool fd = gGsFragDepth != 0;
        if (aswitch(T("Precise depth", "Profundidade precisa", "Profundidad precisa"),
                    T("Exact overlap of surfaces in one plane (waterfalls, decals). Off is faster on weak GPUs; rare flicker.",
                      "Sobreposição exata de superfícies no mesmo plano (cachoeiras, decalques). Desligado é mais rápido em GPUs fracas; pode piscar raramente.",
                      "Superposición exacta de superficies en el mismo plano (cascadas, calcos). Apagado es más rápido en GPUs débiles; puede parpadear."),
                    &fd)) {
            gGsFragDepth = fd;
            save_setting("frag_depth", fd);
        }
    }
    {
        static const int vals[] = {0, 1, 2};
        const char *names[] = {T("Off (30)", "Desligado (30)", "Apagado (30)"), "60", "120"};
        int smooth = gsInterp < 0 ? 0 : gsInterp > 2 ? 2 : gsInterp;
        if (achoice(T("Smooth motion", "Movimento suave", "Movimiento suave"),
                    T("Extra pictures between the game's 30. Costs GPU; the fight itself does not change.",
                      "Quadros extras entre os 30 do jogo. Pesa na GPU; a luta em si não muda.",
                      "Cuadros extra entre los 30 del juego. Pesa en la GPU; la pelea no cambia."),
                    &smooth, names, vals, 3)) {
            gsInterp = smooth;
            save_setting("interp", gsInterp);
        }
    }
    if (!sGL) {
        static const int vals[] = {0, 1, 2, 3};
        const char *names[] = {T("Automatic", "Automático", "Automático"), "FIFO (VSync)", "Mailbox", "Immediate"};
        int pm = GsGpu_PresentGet();
        if (achoice(T("Present mode", "Modo de apresentação", "Modo de presentación"),
                    T("FIFO: steady, a little more latency. Mailbox: no tearing. Immediate: least latency.",
                      "FIFO: estável, um pouco mais de atraso. Mailbox: sem rasgos. Immediate: menor atraso.",
                      "FIFO: estable, un poco más de retraso. Mailbox: sin cortes. Immediate: menor retraso."),
                    &pm, names, vals, 4)) {
            GsGpu_PresentSet(pm);
            Port_SettingsWrite();
        }
    }
    {
        bool on = meter_on();
        if (aswitch(T("Show frame rate", "Mostrar FPS", "Mostrar FPS"),
                    T("And the connection in an online match.", "E a conexão numa partida online.", "Y la conexión en una partida online."), &on)) {
            sMeter = on;
            save_setting("meter", sMeter);
        }
    }
    card_end();

    card_begin("diag", T("Diagnostics", "Diagnóstico", "Diagnóstico"));
    {
        char model[PROP_VALUE_MAX] = "", soc[PROP_VALUE_MAX] = "";
        __system_property_get("ro.product.model", model);
        __system_property_get("ro.soc.model", soc);
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("%s: %.0f fps", T("Pictures", "Quadros", "Cuadros"), io.Framerate);
        ImGui::Text("GPU: %s", sGpuName);
        ImGui::Text("%s: %s", T("Renderer", "Renderizador", "Renderizador"), sGL ? "OpenGL ES 3.2" : "Vulkan");
        ImGui::Text("%s: %gx (%d x %d)", T("Resolution", "Resolução", "Resolución"), v.scale4 / 4.0, 128 * v.scale4, 112 * v.scale4);
        ImGui::Text("%s: %s%s%s", T("Device", "Aparelho", "Equipo"), model, soc[0] ? "  ·  " : "", soc);
        dim_text(T("The full log is in the launcher's Debug page (Export log).", "O log completo fica na página Depuração do app (Exportar log).",
                   "El log completo está en la página Depuración de la app (Exportar log)."));
    }
    card_end();
}

static void page_effects(PortVideo &v) {
    card_begin("fx", T("PlayStation 2 effects", "Efeitos do PlayStation 2", "Efectos del PlayStation 2"));
    const char *names[6] = {T("Outline", "Contorno", "Contorno"), T("See-through tint", "Tom através do cenário", "Tono a través del escenario"),
                            T("Depth tint", "Névoa de distância", "Niebla de distancia"), T("Glare and glow", "Brilho e reflexo", "Brillo y resplandor"),
                            T("Distance blur", "Desfoque de distância", "Desenfoque de distancia"), T("Movement blur", "Desfoque de movimento", "Desenfoque de movimiento")};
    for (int i = 0; i < 6; i++) {
        bool on = !((v.fxOff >> i) & 1);
        if (aswitch(names[i], NULL, &on)) { v.fxOff = (v.fxOff & ~(1 << i)) | (on ? 0 : 1 << i); }
    }
    aslider(T("Glow strength", "Intensidade do brilho", "Intensidad del brillo"),
            T("100% is the PlayStation 2's; 60% by default.", "100% é o do PlayStation 2; 60% por padrão.", "100% es el del PlayStation 2; 60% por defecto."),
            &v.glow, 0, 200, "%");
    card_end();
}

static void page_audio(PortVideo &v) {
    card_begin("vol", T("Volume", "Volume", "Volumen"));
    aslider(T("Music", "Música", "Música"), NULL, &v.music, 0, 200, "%");
    aslider(T("Sound effects", "Efeitos sonoros", "Efectos de sonido"), NULL, &v.effects, 0, 200, "%");
    aslider(T("Voices", "Vozes", "Voces"), NULL, &v.voices, 0, 200, "%");
    aslider(T("Stage sounds", "Sons do cenário", "Sonidos del escenario"), NULL, &v.stage, 0, 200, "%");
    {
        bool follows = gPortMusicFollows > 0;
        if (aswitch(T("Music follows game speed", "Música acompanha a velocidade", "La música sigue la velocidad"),
                    T("Outside 100% speed (online, Cheats page).", "Fora de 100% de velocidade (online, página Trapaças).", "Fuera de 100% de velocidad (online, página Trucos)."),
                    &follows)) {
            gPortMusicFollows = follows;
            save_setting("music_follows_speed", follows);
        }
    }
    card_end();
}

static void page_interface(PortVideo &v) {
    card_begin("hud", T("Screen layout", "Layout da tela", "Diseño de pantalla"));
    {
        static const int vals[] = {0, 1, 2};
        const char *names[] = {T("Edges", "Bordas", "Bordes"), T("Edges, long bars", "Bordas, barras longas", "Bordes, barras largas"), T("Middle (4:3)", "Centro (4:3)", "Centro (4:3)")};
        achoice(T("HUD in a wide picture", "HUD na tela larga", "HUD en pantalla ancha"), NULL, &v.hudLayout, names, vals, 3);
    }
    {
        bool m43 = Port_Menus43() != 0;
        if (aswitch(T("Menus in 4:3", "Menus em 4:3", "Menús en 4:3"),
                    T("Menus at their own shape instead of stretched; fights stay wide.", "Menus no formato original em vez de esticados; lutas continuam largas.",
                      "Menús en su forma original en vez de estirados; las peleas siguen anchas."), &m43)) {
            Port_Menus43Set(m43);
            save_setting("menus_43", m43);
        }
    }
    {
        static const int vals[] = {0, 1, 2};
        const char *names[] = {T("Black", "Preto", "Negro"), T("Border", "Moldura", "Marco"), T("Fighter panels", "Painéis dos lutadores", "Paneles de luchadores")};
        int b = sBars < 0 ? 0 : sBars;
        if (achoice(T("Side bars", "Barras laterais", "Barras laterales"),
                    T("What fills the sides of a 4:3 picture.", "O que preenche as laterais de uma imagem 4:3.", "Lo que llena los lados de una imagen 4:3."),
                    &b, names, vals, 3)) {
            sBars = b;
            save_setting("side_bars", sBars);
        }
    }
    card_end();

    card_begin("fonts", T("Fonts", "Fontes", "Fuentes"));
    struct { const char *label; int *v; int use; } f[3] = {
        {T("Menu font", "Fonte dos menus", "Fuente de menús"), &v.menuFont, MENU_TEXT_MENU},
        {T("Battle font", "Fonte da luta", "Fuente de pelea"), &v.battleFont, MENU_TEXT_BATTLE},
        {T("Dialogue font", "Fonte dos diálogos", "Fuente de diálogos"), &v.textFont, MENU_TEXT_DIALOGUE},
    };
    for (int k = 0; k < 3; k++) {
        const char *names[16];
        int vals[16], n = 0;
        for (int i = 0; i < MenuText_FontCount() && n < 16; i++) {
            if (i == 0 || MenuText_FontAvailable(i, f[k].use)) {
                names[n] = i == 0 ? T("Original", "Original", "Original") : MenuText_FontName(i);
                vals[n++] = i;
            }
        }
        achoice(f[k].label, NULL, f[k].v, names, vals, n);
    }
    card_end();
}

static void page_controls(void) {
    card_begin("clash", T("Clashes", "Choques", "Choques"));
    {
        bool cs = Port_ClashSquare() != 0;
        if (aswitch(T("Punch button counts in clashes", "Botão de soco conta nos choques", "Botón de golpe cuenta en choques"),
                    T("In a clash of beams or rushes each press of Square (X on Xbox) counts as a d-pad tap. Works online.",
                      "Num choque de rajadas ou de golpes, cada toque em Quadrado (X no Xbox) conta como um toque no direcional. Funciona online.",
                      "En un choque de rayos o golpes, cada toque de Cuadrado (X en Xbox) cuenta como un toque de la cruceta. Funciona online."),
                    &cs)) {
            Port_ClashSquareSet(cs);
        }
        if (cs) {
            static const int q[] = {4, 6, 7, 8};
            static const char *const names[] = {"1", "1.5", "1.75", "2"};
            int at = Port_ClashSquareX();
            if (achoice(T("Worth of a press", "Valor de cada toque", "Valor de cada toque"), NULL, &at, names, q, 4)) {
                Port_ClashSquareXSet(at);
            }
        }
    }
    card_end();

    card_begin("pad", T("Physical controller", "Controle físico", "Control físico"));
    {
        int count = 0;
        int *slot = PortInput_PadSlot(0), *pads = PortInput_PadButtons(0);
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        if (count == 0) {
            dim_text(T("No controller connected. The on-screen controller is set up in the launcher (Controls).",
                       "Nenhum controle conectado. O controle na tela é configurado no app (Controles).",
                       "Ningún control conectado. El control en pantalla se configura en la app (Controles)."));
        } else {
            ImGui::Text("%s: %s", T("Player 1", "Jogador 1", "Jugador 1"), *slot >= 0 && *slot < count ? SDL_GetGamepadNameForID(ids[*slot]) : "-");
        }
        SDL_free(ids);
        if (count > 0) {
            sticks_panel(0);
            ImGui::Spacing();
            dim_text(T("Tap a binding, then press the new button (5 s without a press keeps it).",
                       "Toque numa função e aperte o botão novo (5 s sem apertar mantém).",
                       "Toca una función y pulsa el botón nuevo (5 s sin pulsar la mantiene)."));
            char label[96];
            for (int a = 0; a < PORT_PAD_BUTTONS; a++) {
                bool wait = sCapKind == 2 && sCapPlayer == 0 && sCapAction == a;
                ImGui::PushID(a);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(PortInput_ActionLabel(a));
                ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
                snprintf(label, sizeof(label), "%s", wait ? T("press a button...", "aperte um botão...", "pulsa un botón...") : pad_source_name(pads[a]));
                if (ImGui::Button(label, ImVec2(-FLT_MIN, fs() * 1.9f))) {
                    capture(2, 0, a);
                }
                ImGui::PopID();
            }
            if (ImGui::Button(T("Reset to defaults", "Restaurar padrão", "Restaurar valores"), ImVec2(-FLT_MIN, fs() * 2.0f))) {
                PortInput_ResetDefaults(0);
                PortInput_Save();
            }
        }
    }
    card_end();
}

static void page_online(void) {
    card_begin("how", T("How online works", "Como o online funciona", "Cómo funciona el online"));
    ImGui::TextWrapped("%s", T(
        "Online play is the PC version's: Dragon Rage on Android and the PC game meet in the same rooms.\n\n"
        "1. In the game's main menu choose Tenkaichi Budokai Online (or Dragon Net Battle).\n"
        "2. Quick match finds an opponent by itself. Create room gives a code; your friend types it in Join.\n"
        "3. Only button presses travel; both games compute the fight, with rollback for late presses.\n"
        "4. Both players need the same release and the same game (the USA disc, SLUS-21678).",
        "O online é o mesmo da versão de PC: o Dragon Rage no Android e o jogo no PC se encontram nas mesmas salas.\n\n"
        "1. No menu principal do jogo escolha Tenkaichi Budokai Online (ou Dragon Net Battle).\n"
        "2. Partida rápida acha um adversário sozinha. Criar sala gera um código; seu amigo digita em Entrar.\n"
        "3. Só os botões apertados viajam; os dois jogos calculam a luta, com rollback para toques atrasados.\n"
        "4. Os dois precisam da mesma versão e do mesmo jogo (o disco americano, SLUS-21678).",
        "El online es el mismo de la versión de PC: Dragon Rage en Android y el juego de PC se encuentran en las mismas salas.\n\n"
        "1. En el menú principal del juego elige Tenkaichi Budokai Online (o Dragon Net Battle).\n"
        "2. Partida rápida busca un rival sola. Crear sala da un código; tu amigo lo escribe en Unirse.\n"
        "3. Solo viajan los botones pulsados; los dos juegos calculan la pelea, con rollback para toques atrasados.\n"
        "4. Los dos necesitan la misma versión y el mismo juego (el disco americano, SLUS-21678)."));
    card_end();

    card_begin("fail", T("If it does not connect", "Se não conectar", "Si no conecta"));
    ImGui::TextWrapped("%s", T(
        "- \"Too many rooms\" or \"busy\": the free matchmaking service has hourly limits. Wait a few minutes.\n"
        "- \"Update available\" or \"different version\": update Dragon Rage (the launcher says when there is one).\n"
        "- \"Room codes need an official release\": the service only takes codes from signed releases; use Quick match or play by address.\n"
        "- Stuck on connecting: try Wi-Fi instead of mobile data (or the other way round), turn off VPN and data saver.\n"
        "- Lag: both on Wi-Fi, near the router; the frame rate meter shows the connection.\n"
        "- Still failing: export the log in the launcher (Debug page) and send it on our Discord.",
        "- \"Too many rooms\" ou \"busy\": o serviço gratuito de partidas tem limite por hora. Espere alguns minutos.\n"
        "- \"Update available\" ou \"different version\": atualize o Dragon Rage (o app avisa quando tem versão nova).\n"
        "- \"Room codes need an official release\": o serviço só aceita códigos de versões assinadas; use Partida rápida ou jogue por endereço (IP).\n"
        "- Travado em conectando: troque Wi-Fi por dados móveis (ou o contrário), desligue VPN e economia de dados.\n"
        "- Lag: os dois no Wi-Fi, perto do roteador; o medidor de FPS mostra a conexão.\n"
        "- Continua falhando: exporte o log no app (página Depuração) e mande no nosso Discord.",
        "- \"Too many rooms\" o \"busy\": el servicio gratuito de partidas tiene límite por hora. Espera unos minutos.\n"
        "- \"Update available\" o \"different version\": actualiza Dragon Rage (la app avisa cuando hay versión nueva).\n"
        "- \"Room codes need an official release\": el servicio solo acepta códigos de versiones firmadas; usa Partida rápida o juega por dirección (IP).\n"
        "- Se queda conectando: cambia Wi-Fi por datos móviles (o al revés), apaga VPN y ahorro de datos.\n"
        "- Lag: los dos en Wi-Fi, cerca del router; el medidor de FPS muestra la conexión.\n"
        "- Sigue fallando: exporta el log en la app (página Depuración) y mándalo en nuestro Discord."));
    card_end();

    card_begin("chat", T("Online chat", "Chat online", "Chat online"));
    {
        bool chat = PortChat_Enabled() != 0;
        if (aswitch(T("Show the other player's messages", "Mostrar mensagens do outro jogador", "Mostrar mensajes del otro jugador"),
                    T("Typing needs a keyboard (Bluetooth or USB): Tab opens a line.", "Para escrever precisa de teclado (Bluetooth ou USB): Tab abre a linha.",
                      "Para escribir hace falta teclado (Bluetooth o USB): Tab abre la línea."), &chat)) {
            PortChat_EnabledSet(chat);
        }
    }
    card_end();

    card_begin("watch", T("Spectators and lobbies", "Espectadores e lobbies", "Espectadores y lobbies"));
    {
        bool watch = Port_Setting("net_spectators", 1) != 0;
        if (aswitch(T("Let others watch my matches", "Deixar outros assistirem minhas partidas", "Dejar que otros vean mis partidas"),
                    T("Up to six people can watch your match, by your room code or your address; they never change the fight.", "Até seis pessoas podem assistir à sua partida, pelo código da sala ou pelo seu endereço; elas nunca mudam a luta.", "Hasta seis personas pueden ver tu partida, por el código de la sala o tu dirección; nunca cambian la pelea."), &watch)) {
            save_setting("net_spectators", watch ? 1 : 0);
        }
        ImGui::TextWrapped("%s", T("- Watch a match: Dragon Net Battle > Join room, type the code and choose Watch the match. View: player 1, player 2 or split screen (R1).\n- Lobby: Dragon Net Battle > Create room > Open a lobby (or Tenkaichi Budokai Online > Lobby). Up to eight people: two fight, the rest watch, with rules, Ready and chat.\n- Quick match, Region row: Auto finds someone nearby first and everyone after 25 s; a region first, everyone after 40 s.\n- The pause menu (Back button) has Back to the lobby, Leave / Close the lobby, Change view and Stop watching when they apply.",
            "- Assistir a uma partida: Dragon Net Battle > Entrar em sala, digite o código e escolha Assistir à partida. Câmera: jogador 1, jogador 2 ou tela dividida (R1).\n- Lobby: Dragon Net Battle > Criar sala > Abrir um lobby (ou Tenkaichi Budokai Online > Lobby). Até oito pessoas: duas lutam e as outras assistem, com regras, Pronto e chat.\n- Partida rápida, linha Região: Auto procura alguém perto primeiro e qualquer um depois de 25 s; uma região primeiro, qualquer um depois de 40 s.\n- O menu de pausa (botão Voltar) tem Voltar ao lobby, Sair / Fechar o lobby, Trocar câmera e Parar de assistir quando valem.",
            "- Ver una partida: Dragon Net Battle > Entrar a sala, escribe el código y elige Ver la partida. Cámara: jugador 1, jugador 2 o pantalla dividida (R1).\n- Lobby: Dragon Net Battle > Crear sala > Abrir un lobby (o Tenkaichi Budokai Online > Lobby). Hasta ocho personas: dos pelean y el resto mira, con reglas, Listo y chat.\n- Partida rápida, fila Región: Auto busca a alguien cerca primero y a cualquiera tras 25 s; una región primero, cualquiera tras 40 s.\n- El menú de pausa (botón Atrás) tiene Volver al lobby, Salir / Cerrar el lobby, Cambiar cámara y Dejar de ver cuando aplican."));
    }
    card_end();
}

static void page_cheats(void) {
    card_begin("spd", T("Game speed", "Velocidade do jogo", "Velocidad del juego"));
    {
        int speed = Port_GameSpeed();
        if (aslider(T("Speed", "Velocidade", "Velocidad"),
                    T("Outside online play only. 100% is the game's own; 114% is what competitive play uses.",
                      "Só fora do online. 100% é a do jogo; 114% é a usada no competitivo.",
                      "Solo fuera del online. 100% es la del juego; 114% es la del competitivo."),
                    &speed, 50, 200, "%")) {
            Port_GameSpeedSet(speed);
            Port_AudioRefresh();
        }
        if (ImGui::Button(T("Back to 100%", "Voltar para 100%", "Volver a 100%"), ImVec2(0.0f, fs() * 2.0f))) {
            Port_GameSpeedSet(100);
            Port_AudioRefresh();
        }
    }
    card_end();

    card_begin("unlock", T("Unlock everything", "Desbloquear tudo", "Desbloquear todo"));
    dim_text(T("All characters, stages, music, items and the most Zenni: a debug function the developers left in. It empties your "
               "records list and becomes permanent when the game saves. Use it from the main menu on.",
               "Todos os personagens, cenários, músicas, itens e o máximo de Zenni: uma função de debug que os desenvolvedores deixaram. "
               "Ela apaga sua lista de recordes e fica permanente quando o jogo salvar. Use a partir do menu principal.",
               "Todos los personajes, escenarios, música, objetos y el máximo de Zenni: una función de depuración que dejaron los desarrolladores. "
               "Vacía tu lista de récords y queda permanente cuando el juego guarda. Úsala desde el menú principal."));
    static bool sure;
    if (!sure) {
        if (ImGui::Button(T("Unlock everything...", "Desbloquear tudo...", "Desbloquear todo..."), ImVec2(-FLT_MIN, fs() * 2.2f))) { sure = true; }
    } else {
        ImGui::TextColored(kGold, "%s", T("Sure? This cannot be undone once the game saves.", "Certeza? Não dá para desfazer depois que o jogo salvar.",
                                          "¿Seguro? No se puede deshacer cuando el juego guarde."));
        if (ImGui::Button(T("Unlock", "Desbloquear", "Desbloquear"), ImVec2(fs() * 8.0f, fs() * 2.2f))) {
            gPortUnlockAll = 1;
            sure = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel", "Cancelar", "Cancelar"), ImVec2(fs() * 8.0f, fs() * 2.2f))) { sure = false; }
    }
    if (gPortUnlockDone) {
        ImGui::TextColored(kOrange, "%s", T("Done.", "Feito.", "Listo."));
    }
    card_end();

    card_begin("start", T("Game start", "Início do jogo", "Inicio del juego"));
    {
        static int logos = -1, intro = -1, copy = -1;
        if (logos < 0) {
            logos = Port_Setting("skip_logos", 0) != 0;
            intro = Port_Setting("skip_intro", 0) != 0;
            copy = Port_Setting("skip_copyright", 0) != 0;
        }
        bool b = logos != 0;
        if (aswitch(T("Skip logos", "Pular logos", "Saltar logos"), NULL, &b)) { logos = b; save_setting("skip_logos", logos); }
        b = intro != 0;
        if (aswitch(T("Skip intro video", "Pular vídeo de abertura", "Saltar video de apertura"), NULL, &b)) { intro = b; save_setting("skip_intro", intro); }
        b = copy != 0;
        if (aswitch(T("Skip copyright page", "Pular tela de copyright", "Saltar pantalla de copyright"), NULL, &b)) { copy = b; save_setting("skip_copyright", copy); }
        dim_text(T("From the next start of the game.", "Vale na próxima vez que o jogo abrir.", "Vale desde el próximo inicio del juego."));
    }
    card_end();
}

// The full-screen window of the Android menus (pause menu, online): night blue with the launcher's glow, text about a
// thirtieth of the screen's height, the launcher's colours. dr_end() closes it.
static void dr_begin(const char *id) {
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kNight);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const float pad = fs() * 1.1f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad * 1.4f, pad * 0.8f));
    ImGui::Begin(id, NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    // text about a thirtieth of the screen's height (the system's density made it much larger on phones), and the
    // launcher's colours for buttons, fields and sliders
    float base = ImGui::GetStyle().FontSizeBase > 0.0f ? ImGui::GetStyle().FontSizeBase : ImGui::GetFontSize();
    float want = io.DisplaySize.y / 29.0f;
    if (want > ImGui::GetFontSize()) { want = ImGui::GetFontSize(); }
    if (want < 22.0f) { want = 22.0f; }
    ImGui::PushFont(NULL, base * want / ImGui::GetFontSize());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.18f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.27f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.98f, 0.55f, 0.13f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.18f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.24f, 0.36f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.25f, 0.27f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, kOrange);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, kGold);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.98f, 0.55f, 0.13f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_NavCursor, kGold);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, fs() * 0.45f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(fs() * 0.45f, fs() * 0.4f));
    {   // a warm glow from the top right, as the launcher's background
        ImDrawList *bg = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
        for (int i = 0; i < 6; i++) {
            float r = s.y * (0.35f + 0.13f * i);
            bg->AddCircleFilled(ImVec2(o.x + s.x * 0.92f, o.y - s.y * 0.1f), r, ImGui::GetColorU32(ImVec4(0.98f, 0.55f, 0.13f, 0.035f)), 64);
        }
    }

}

static void dr_end(void) {
    touch_scroll();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(11);
    ImGui::PopFont();
    ImGui::End();
}

static int sAndroidPage; // the page shown (the touch menu's "Controller buttons" opens this window on Controls)

static void build_android(void) {
    PortVideo v, was;
    ImGuiIO &io = ImGui::GetIO();
    int &page = sAndroidPage;
    static bool askQuit;
    bool open = true;

    GsGpu_GetSettings(&v);
    was = v;
    if (sCapKind == 2 && SDL_GetTicks() > sCapUntil) {
        sCapKind = 0;
        gPortInputCapture = 0;
    }
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: back to the game
        if (askQuit) { askQuit = false; } else { open = false; }
    }
    const char *pages[] = {
        T("Video", "Vídeo", "Video"), T("Performance", "Desempenho", "Rendimiento"), T("Effects", "Efeitos", "Efectos"),
        T("Audio", "Áudio", "Audio"), T("Interface", "Interface", "Interfaz"), T("Controls", "Controles", "Controles"),
        T("Online", "Online", "Online"), T("Cheats", "Trapaças", "Trucos"),
    };
    const int npages = 8;
    if (pad_pressed(ImGuiKey_GamepadL1)) { page = (page + npages - 1) % npages; }
    if (pad_pressed(ImGuiKey_GamepadR1)) { page = (page + 1) % npages; }

    dr_begin("###DragonRagePause");

    // ---- the top line: name, state, and the two ways out
    {
        const float bh = fs() * 2.4f, right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        ImGui::PushFont(NULL, (ImGui::GetStyle().FontSizeBase > 0.0f ? ImGui::GetStyle().FontSizeBase : ImGui::GetFontSize()) * 1.45f);
        ImGui::TextColored(kGold, "DRAGON RAGE");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kDim, "  %s", T("PAUSED", "PAUSADO", "EN PAUSA"));
        const char *resume = T("Resume", "Continuar", "Continuar"), *quit = T("Quit to launcher", "Sair para o app", "Salir a la app");
        const float wr = ImGui::CalcTextSize(resume).x + fs() * 2.6f, wq = ImGui::CalcTextSize(quit).x + fs() * 2.0f;
        ImGui::SameLine(right - wr - wq - ImGui::GetStyle().ItemSpacing.x);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bh * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.17f, 0.26f, 1.0f));
        if (ImGui::Button(quit, ImVec2(wq, bh))) { askQuit = true; }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, kOrange);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.65f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.07f, 0.02f, 1.0f));
        if (ImGui::Button(resume, ImVec2(wr, bh))) { open = false; }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::Dummy(ImVec2(0.0f, fs() * 0.3f));

    // a lobby's match and a watched match: the ways back and out (the PC's window has them at its top)
    {
        PortLobbyView lv;
        const bool inLobby = Port_LobbyView(&lv) && lv.inMatch, watching = Port_NetSpectating() != 0;
        if (inLobby || watching) {
            const float bh = fs() * 2.3f;
            card_begin("lobby", inLobby ? T("Lobby", "Lobby", "Lobby") : T("Watching", "Assistindo", "Viendo"));
            if (inLobby && lv.code[0] != '\0') { ImGui::TextColored(kGold, "%s %s", T("Code", "Código", "Código"), lv.code); }
            if (watching) {
                ImGui::TextWrapped(T("Watching %s vs %s. View: %s", "Assistindo %s x %s. Câmera: %s", "Viendo %s vs %s. Cámara: %s"), Port_NetName(0),
                                   Port_NetName(1), Port_NetSpecView() == 2 ? T("both (split screen)", "os dois (tela dividida)", "los dos (pantalla dividida)")
                                                                           : spec_view_name(Port_NetSpecView()));
                if (ImGui::Button(T("Change view", "Trocar câmera", "Cambiar cámara"), ImVec2(0.0f, bh))) { spec_view_next(); }
                ImGui::SameLine();
            }
            if (inLobby && lv.owner) {
                if (ImGui::Button(T("Back to the lobby", "Voltar ao lobby", "Volver al lobby"), ImVec2(0.0f, bh))) {
                    Port_LobbyDo(LOBBY_BACK, 0, 0); // (the match ends for everyone: all go back to the lobby's page)
                    open = false;
                }
                ImGui::SameLine();
            }
            if (inLobby) {
                if (ImGui::Button(lv.owner ? T("Close the lobby", "Fechar o lobby", "Cerrar el lobby") : T("Leave the lobby", "Sair do lobby", "Salir del lobby"),
                                  ImVec2(0.0f, bh))) {
                    Port_LobbyDo(LOBBY_LEAVE, 0, 0);
                    open = false;
                }
            } else if (ImGui::Button(T("Stop watching", "Parar de assistir", "Dejar de ver"), ImVec2(0.0f, bh))) {
                Port_NetSpecLeave();
                open = false;
            }
            card_end();
        }
    }

    if (askQuit) {
        card_begin("quit", T("Quit to the launcher?", "Sair para o app?", "¿Salir a la app?"));
        ImGui::TextWrapped("%s", T("The game closes and you go back to the app. Progress not saved in the game is lost.",
                                   "O jogo fecha e você volta para o app. O progresso que não foi salvo no jogo é perdido.",
                                   "El juego se cierra y vuelves a la app. Se pierde el progreso no guardado en el juego."));
        ImGui::Spacing();
        if (ImGui::Button(T("Quit", "Sair", "Salir"), ImVec2(fs() * 9.0f, fs() * 2.3f))) { quit_to_app(); }
        ImGui::SameLine();
        if (ImGui::Button(T("Stay", "Ficar", "Quedarme"), ImVec2(fs() * 9.0f, fs() * 2.3f))) { askQuit = false; }
        card_end();
    }

    // ---- the side bar of pages and the page
    const float side = io.DisplaySize.x * 0.2f < fs() * 9.0f ? fs() * 9.0f : io.DisplaySize.x * 0.2f;
    const float foot = fs() * 1.6f;
    ImGui::BeginChild("side", ImVec2(side, -foot), ImGuiChildFlags_NavFlattened);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, fs() * 0.6f);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.08f, 0.5f));
    for (int i = 0; i < npages; i++) {
        bool on = i == page;
        ImGui::PushStyleColor(ImGuiCol_Button, on ? ImVec4(0.98f, 0.55f, 0.13f, 0.95f) : ImVec4(0.11f, 0.12f, 0.19f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(0.12f, 0.07f, 0.02f, 1.0f) : ImVec4(0.92f, 0.93f, 0.97f, 1.0f));
        ImGui::PushID(i);
        if (ImGui::Button(pages[i], ImVec2(-FLT_MIN, fs() * 2.2f))) { page = i; }
        ImGui::PopID();
        ImGui::PopStyleColor(2);
    }
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::SameLine(0.0f, fs() * 1.0f);
    ImGui::BeginChild("page", ImVec2(0.0f, -foot), ImGuiChildFlags_NavFlattened);
    switch (page) {
    case 0: page_video(v); break;
    case 1: page_performance(v); break;
    case 2: page_effects(v); break;
    case 3: page_audio(v); break;
    case 4: page_interface(v); break;
    case 5: page_controls(); break;
    case 6: page_online(); break;
    default: page_cheats(); break;
    }
    ImGui::EndChild();
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextUnformatted(T("Back button or START + SELECT: resume   ·   L1 / R1: page",
                             "Botão voltar ou START + SELECT: continuar   ·   L1 / R1: página",
                             "Botón atrás o START + SELECT: continuar   ·   L1 / R1: página"));
    ImGui::PopStyleColor();
    dr_end();
    if (memcmp(&v, &was, sizeof(v)) != 0) {
        GsGpu_SetSettings(&v);
    }
    if (!open) {
        askQuit = false;
        set_open(false);
    }
}

// ---- The touch menu (Dragon Rage 0.9): the game's own pause menu drawn by the app (engine/GameMenu.kt), over
// the picture, with the phone's own scrolling, switches and sliders. The app asks through libmain.so (loader.c) and
// these functions; a value it changes is queued and applied here, on the engine's thread, between two frames (as the
// window above applies its own). The keys are the names in bt3_settings.txt. While it is up the game waits, as with
// the window (Ui_Pauses), except in an online match.
static SDL_Mutex *sNmLock;
static volatile int sNmWant = -1; // asked by the app: 1 open, 0 closed
static int sNativeOpen;           // (the engine's side of it)
static int sNmGen, sNmDirty;
struct NmPair { char key[32]; int value; };
static NmPair sNmQueue[64], sNmSnap[64];
static int sNmQueueN, sNmSnapN;
static char sNmFonts[3][1024], sNmDevice[256];
static char sNmLobbyCode[8], sNmLobbyServer[12], sNmSpecNames[130]; // (a lobby's code and service; watched players, "a|b")
static volatile int sNmAction; // 1 resume, 2 quit, 3 the controller's buttons (this window, Controls); 4 back to the lobby, 5 leave it, 6 the next view, 7 stop watching,
                               // 8 the controller's automatic set-up (the PS2 layout)

static void nm_lock(void) {
    if (sNmLock == NULL) {
        static SDL_AtomicInt made;
        if (SDL_CompareAndSwapAtomicInt(&made, 0, 1)) {
            sNmLock = SDL_CreateMutex();
        } else {
            while (sNmLock == NULL) { SDL_Delay(1); }
        }
    }
    SDL_LockMutex(sNmLock);
}

static int nm_pauses(void) {
    return sNativeOpen;
}

static void nm_put(const char *key, int value) {
    if (sNmSnapN < (int)(sizeof(sNmSnap) / sizeof(sNmSnap[0]))) {
        SDL_strlcpy(sNmSnap[sNmSnapN].key, key, sizeof(sNmSnap[0].key));
        sNmSnap[sNmSnapN++].value = value;
    }
}

// What the app shows: read where the window above reads it (engine thread, the lock held).
static void nm_snapshot(void) {
    PortVideo v;
    GsGpu_GetSettings(&v);
    sNmSnapN = 0;
    nm_put("scale4", v.scale4);
    nm_put("aspect_milli", v.aspectMilli);
    nm_put("filter", v.filter);
    nm_put("smooth_2d", v.smooth2d);
    nm_put("texture_pack", v.texPack);
    nm_put("texture_count", v.texPackCount);
    nm_put("gpu_api", Port_Setting("gpu_api", 0) == 1 ? 1 : 0);
    nm_put("running_gl", sGL ? 1 : 0);
    nm_put("frame_skip", v.frameSkip);
    nm_put("frag_depth", gGsFragDepth);
    nm_put("interp", gsInterp < 0 ? 0 : gsInterp > 2 ? 2 : gsInterp);
    nm_put("present", sGL ? 0 : GsGpu_PresentGet());
    nm_put("meter", meter_on() ? 1 : 0);
    nm_put("fx_off", v.fxOff);
    nm_put("glow", v.glow);
    nm_put("music", v.music);
    nm_put("effects", v.effects);
    nm_put("voices", v.voices);
    nm_put("stage_sounds", v.stage);
    nm_put("music_follows_speed", gPortMusicFollows > 0 ? 1 : 0);
    nm_put("hud_layout", v.hudLayout);
    nm_put("menus_43", Port_Menus43() != 0);
    nm_put("side_bars", sBars < 0 ? Port_Setting("side_bars", 0) : sBars);
    nm_put("menu_font_id", v.menuFont);
    nm_put("battle_font_id", v.battleFont);
    nm_put("text_font_id", v.textFont);
    nm_put("clash_square", Port_ClashSquare() != 0);
    nm_put("clash_square_x", Port_ClashSquareX());
    nm_put("chat", PortChat_Enabled() != 0);
    nm_put("net_spectators", Port_Setting("net_spectators", 1) != 0);
    nm_put("game_speed", Port_GameSpeed());
    nm_put("skip_logos", Port_Setting("skip_logos", 0) != 0);
    nm_put("skip_intro", Port_Setting("skip_intro", 0) != 0);
    nm_put("skip_copyright", Port_Setting("skip_copyright", 0) != 0);
    nm_put("unlock_done", gPortUnlockDone != 0);
    nm_put("online", Port_NetActive() != 0);
    {
        PortLobbyView lv;
        const bool inLobby = Port_LobbyView(&lv) && lv.inMatch;
        nm_put("lobby_match", inLobby);
        nm_put("lobby_owner", inLobby && lv.owner);
        nm_put("spectating", Port_NetSpectating() != 0);
        nm_put("spec_view", Port_NetSpecView());
        SDL_snprintf(sNmLobbyCode, sizeof(sNmLobbyCode), "%s", inLobby ? lv.code : "");
        SDL_snprintf(sNmLobbyServer, sizeof(sNmLobbyServer), "%s", inLobby ? Port_LobbyServer() : "");
        SDL_snprintf(sNmSpecNames, sizeof(sNmSpecNames), "%s|%s", Port_NetName(0), Port_NetName(1));
    }
    nm_put("fps", (int)(ImGui::GetCurrentContext() != NULL ? ImGui::GetIO().Framerate : 0.0f));
    {
        const int uses[3] = {MENU_TEXT_MENU, MENU_TEXT_BATTLE, MENU_TEXT_DIALOGUE};
        for (int k = 0; k < 3; k++) {
            int n = 0;
            sNmFonts[k][0] = '\0';
            for (int i = 0; i < MenuText_FontCount(); i++) {
                if (i == 0 || MenuText_FontAvailable(i, uses[k])) {
                    char one[160];
                    snprintf(one, sizeof(one), "%s%d:%s", n > 0 ? "|" : "", i, i == 0 ? "" : MenuText_FontName(i));
                    SDL_strlcat(sNmFonts[k], one, sizeof(sNmFonts[k]));
                    n++;
                }
            }
        }
    }
    {
        char model[PROP_VALUE_MAX] = "", soc[PROP_VALUE_MAX] = "";
        __system_property_get("ro.product.model", model);
        __system_property_get("ro.soc.model", soc);
        snprintf(sNmDevice, sizeof(sNmDevice), "%s%s%s", model, soc[0] ? " · " : "", soc);
    }
    sNmGen++;
}

// One value from the app, applied as the window above applies it.
static void nm_apply(const char *key, int value) {
    PortVideo v, was;
    GsGpu_GetSettings(&v);
    was = v;
    if (strcmp(key, "scale4") == 0) { v.scale4 = value; }
    else if (strcmp(key, "aspect_milli") == 0) { v.aspectMilli = value; }
    else if (strcmp(key, "filter") == 0) { v.filter = value; }
    else if (strcmp(key, "smooth_2d") == 0) { v.smooth2d = value; }
    else if (strcmp(key, "texture_pack") == 0) { v.texPack = value; }
    else if (strcmp(key, "frame_skip") == 0) { v.frameSkip = value; }
    else if (strcmp(key, "fx_off") == 0) { v.fxOff = value; }
    else if (strcmp(key, "glow") == 0) { v.glow = value; }
    else if (strcmp(key, "music") == 0) { v.music = value; }
    else if (strcmp(key, "effects") == 0) { v.effects = value; }
    else if (strcmp(key, "voices") == 0) { v.voices = value; }
    else if (strcmp(key, "stage_sounds") == 0) { v.stage = value; }
    else if (strcmp(key, "hud_layout") == 0) { v.hudLayout = value; }
    else if (strcmp(key, "menu_font_id") == 0) { v.menuFont = value; }
    else if (strcmp(key, "battle_font_id") == 0) { v.battleFont = value; }
    else if (strcmp(key, "text_font_id") == 0) { v.textFont = value; }
    else if (strcmp(key, "gpu_api") == 0) {
        Port_SettingSave("gpu_api", value ? 1 : 0);
        Port_SettingSave("dr_renderer", (value ? 1 : 0) + 1);
        Port_SettingsWrite();
    } else if (strcmp(key, "frag_depth") == 0) {
        gGsFragDepth = value != 0;
        save_setting("frag_depth", gGsFragDepth);
    } else if (strcmp(key, "interp") == 0) {
        gsInterp = value < 0 ? 0 : value > 2 ? 2 : value;
        save_setting("interp", gsInterp);
    } else if (strcmp(key, "present") == 0) {
        if (!sGL) {
            GsGpu_PresentSet(value);
            Port_SettingsWrite();
        }
    } else if (strcmp(key, "meter") == 0) {
        sMeter = value != 0;
        save_setting("meter", sMeter);
    } else if (strcmp(key, "music_follows_speed") == 0) {
        gPortMusicFollows = value != 0;
        save_setting("music_follows_speed", gPortMusicFollows);
    } else if (strcmp(key, "menus_43") == 0) {
        Port_Menus43Set(value != 0);
        save_setting("menus_43", value != 0);
    } else if (strcmp(key, "side_bars") == 0) {
        sBars = value < 0 ? 0 : value > 2 ? 2 : value;
        save_setting("side_bars", sBars);
    } else if (strcmp(key, "clash_square") == 0) {
        Port_ClashSquareSet(value != 0);
    } else if (strcmp(key, "clash_square_x") == 0) {
        Port_ClashSquareXSet(value);
    } else if (strcmp(key, "chat") == 0) {
        PortChat_EnabledSet(value != 0);
    } else if (strcmp(key, "net_spectators") == 0) {
        save_setting("net_spectators", value != 0 ? 1 : 0);
    } else if (strcmp(key, "game_speed") == 0) {
        Port_GameSpeedSet(value < 50 ? 50 : value > 200 ? 200 : value);
        Port_AudioRefresh();
    } else if (strcmp(key, "skip_logos") == 0 || strcmp(key, "skip_intro") == 0 || strcmp(key, "skip_copyright") == 0) {
        save_setting(key, value != 0);
    } else if (strcmp(key, "unlock_all") == 0) {
        if (value) { gPortUnlockAll = 1; }
    }
    if (memcmp(&v, &was, sizeof(v)) != 0) {
        GsGpu_SetSettings(&v);
    }
}

// Once a frame (frame_build): the app's requests, on the engine's thread.
static void nm_frame(void) {
    NmPair q[64];
    int n, want, action;
    nm_lock();
    want = sNmWant;
    sNmWant = -1;
    n = sNmQueueN;
    memcpy(q, sNmQueue, sizeof(NmPair) * (size_t)n);
    sNmQueueN = 0;
    action = sNmAction;
    sNmAction = 0;
    SDL_UnlockMutex(sNmLock);
    if (want == 1 && !sNativeOpen) {
        if (sOpen) { set_open(false); } // (the window and the touch menu are not up together)
        sNativeOpen = 1;
        sNmDirty = 1;
    }
    for (int i = 0; i < n; i++) {
        nm_apply(q[i].key, q[i].value);
        sNmDirty = 1;
    }
    if (action == 3) { // the controller's buttons: this window, on its Controls page
        sNativeOpen = 0;
        sAndroidPage = 5;
        set_open(true);
    } else if (action == 2) {
        quit_to_app();
    } else if (action == 4 && sNativeOpen) { // a lobby's match, its owner: the match ends for everyone, all back to the lobby's page
        Port_LobbyDo(LOBBY_BACK, 0, 0);
    } else if (action == 5 && sNativeOpen) { // leave the lobby (its owner: close it)
        Port_LobbyDo(LOBBY_LEAVE, 0, 0);
    } else if (action == 6) { // watching: the next view (the menu stays)
        spec_view_next();
        sNmDirty = 1;
    } else if (action == 7 && sNativeOpen) { // stop watching
        Port_NetSpecLeave();
    } else if (action == 8) { // the controller's automatic set-up: the PS2 layout on the wireless one (gs_input.c)
        static char line[160];
        char name[96];
        int slot = PortInput_AutoConfigure(0, name, (int)sizeof(name));
        if (slot < 0) {
            SDL_strlcpy(line, T("No controller is connected: pair it by Bluetooth (or plug it in) and press again.",
                                "Nenhum controle conectado: pareie pelo Bluetooth (ou ligue no cabo) e aperte de novo.",
                                "No hay ningún control conectado: emparéjalo por Bluetooth (o conéctalo) y pulsa otra vez."), sizeof(line));
        } else {
            SDL_snprintf(line, sizeof(line), T("Controller %d (%s): PS2 layout, deadzone 10%%, vibration on.",
                                               "Controle %d (%s): layout de PS2, zona morta 10%%, vibração ligada.",
                                               "Control %d (%s): diseño de PS2, zona muerta 10%%, vibración activada."), slot + 1, name);
        }
        Port_UiNotice(line);
        sNmDirty = 1;
    }
    if ((want == 0 || action == 1) && sNativeOpen) {
        sNativeOpen = 0;
        PortInput_Swallow(); // (the touch that closed it is not the game's)
    }
    if (sNmDirty && (sNativeOpen || want == 1)) {
        nm_lock();
        nm_snapshot();
        SDL_UnlockMutex(sNmLock);
        sNmDirty = 0;
    }
}

extern "C" {
// (loader.c, for the app: GameMenu.kt through MenuBridge)
int Ui_NmOpen(int open) {
    nm_lock();
    sNmWant = open ? 1 : 0;
    SDL_UnlockMutex(sNmLock);
    return sReady ? 1 : 0;
}

int Ui_NmGen(void) {
    int g;
    nm_lock();
    g = sNmGen;
    SDL_UnlockMutex(sNmLock);
    return g;
}

int Ui_NmGet(const char *key, int def) {
    int v = def;
    nm_lock();
    for (int i = 0; i < sNmSnapN; i++) {
        if (strcmp(sNmSnap[i].key, key) == 0) {
            v = sNmSnap[i].value;
            break;
        }
    }
    SDL_UnlockMutex(sNmLock);
    return v;
}

void Ui_NmSet(const char *key, int value) {
    nm_lock();
    for (int i = 0; i < sNmSnapN; i++) { // (the app shows it at once; the snapshot agrees until it is made again)
        if (strcmp(sNmSnap[i].key, key) == 0) { sNmSnap[i].value = value; }
    }
    if (sNmQueueN < (int)(sizeof(sNmQueue) / sizeof(sNmQueue[0]))) {
        SDL_strlcpy(sNmQueue[sNmQueueN].key, key, sizeof(sNmQueue[0].key));
        sNmQueue[sNmQueueN++].value = value;
    }
    SDL_UnlockMutex(sNmLock);
}

int Ui_NmText(const char *key, char *out, int size) {
    const char *t = "";
    nm_lock();
    if (strcmp(key, "gpu") == 0) { t = sGpuName; }
    else if (strcmp(key, "device") == 0) { t = sNmDevice; }
    else if (strcmp(key, "fonts_menu") == 0) { t = sNmFonts[0]; }
    else if (strcmp(key, "fonts_battle") == 0) { t = sNmFonts[1]; }
    else if (strcmp(key, "fonts_text") == 0) { t = sNmFonts[2]; }
    else if (strcmp(key, "lobby_code") == 0) { t = sNmLobbyCode; }
    else if (strcmp(key, "lobby_server") == 0) { t = sNmLobbyServer; }
    else if (strcmp(key, "spec_names") == 0) { t = sNmSpecNames; }
    SDL_strlcpy(out, t, (size_t)size);
    SDL_UnlockMutex(sNmLock);
    return (int)strlen(out);
}

void Ui_NmAction(int action) {
    nm_lock();
    sNmAction = action;
    SDL_UnlockMutex(sNmLock);
}
}

// ---- Dragon Net Battle, the online window opened from the game's main menu: ui_dragonnet.inc.
