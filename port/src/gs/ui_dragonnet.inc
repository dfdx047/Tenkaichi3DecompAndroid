// Dragon Net Battle (Android): the online screen in DBT-NX's look (the Switch port: dark violet cards, the chosen row
// in gold, the home menu's cyan plates), with its five entries: Create room, Join room, Quick match, Ranked and the
// general chat; for touch and controller; English, Portuguese and Spanish (T(), ui_android.inc).
//
// Two kinds of opponents: Dragon Net (Android and Switch, its own service: Ranked, chat and the players online,
// net_dragon.c) or the PC (the PC port's service: rooms and quick match). The rooms, codes and quick match are the
// lobby's (net.c, net_match.c) in both. Opened from the main menu's Dragon Net Battle and from the in-game menu.
//
// Included by ui.cpp after ui_android.inc (its T(), fs(), the colours, ainput() and touch_scroll()).

#include "net_dragon.h"

namespace dn {

enum Screen { HOME, CREATE, CREATE_WAIT, JOIN, JOIN_WAIT, QUICK, QUICK_WAIT, RANKED, RANKED_WAIT, BOARD, CHAT, REMATCH, WATCH_WAIT };

static Screen sScreen = HOME;
static int sRow, sHomeRow;
static float sEnter;        // the screen's entrance (0..1): the cards slide in as on the Switch
static Uint64 sEnterAt;
static char sName[17] = "Player";
static bool sNameRead, sEditName, sEditCode, sEditChat;
static char sCode[8], sChatLine[161];
static int sBattle, sQueue2, sRankMode;      // create: battle type 0..4; quick: queue 0..4; ranked: 0 single, 1 team
static int sLobbyLast;                       // the lobby's last state worth telling (errors)
static Uint64 sSearchSince, sFoundAt;
static bool sRankedLobby;                    // the lobby was started for a ranked match
static float sK = 1.0f;                      // FontSizeBase per pixel of text
// The PC's Quick Match regions (src/menu/port_tbo.c): what the matchmaking service is told, and their names here.
static const char *const kRegionArea[9] = {"auto", "NA", "SA", "EU", "ME", "AF", "AS", "OC", "any"};

// ---- colours (DBT-NX's): night violet, violet lines, gold choice, cyan plates
static ImU32 col(float r, float g, float b, float a = 1.0f) { return ImGui::GetColorU32(ImVec4(r, g, b, a)); }
static const ImVec4 kText(0.93f, 0.92f, 0.98f, 1.0f), kSub(0.70f, 0.66f, 0.84f, 1.0f), kGoldT(1.0f, 0.86f, 0.30f, 1.0f);

static void font_px(float px) { ImGui::PushFont(NULL, px * sK); }
static float U(void) { return ImGui::GetIO().DisplaySize.y / 30.0f; } // the screen's unit: a line of normal text

static void goto_screen(Screen s, int row = 0) {
    sScreen = s;
    sRow = row;
    sEnterAt = SDL_GetTicks();
    sEditCode = sEditChat = false;
}

// ---- the background: a night sky over a fighting ring (drawn; dragonnet/background.png is not read: no picture of the
// game or of others is shipped)
static void background(ImDrawList *dl, ImVec2 o, ImVec2 s) {
    dl->AddRectFilledMultiColor(o, ImVec2(o.x + s.x, o.y + s.y), col(0.10f, 0.04f, 0.20f), col(0.16f, 0.06f, 0.30f), col(0.05f, 0.02f, 0.10f), col(0.03f, 0.01f, 0.07f));
    for (int i = 0; i < 7; i++) { // nebula
        float fx = 0.62f + 0.05f * (float)((i * 37) % 7 - 3), fy = 0.18f + 0.09f * (float)((i * 53) % 5);
        dl->AddCircleFilled(ImVec2(o.x + s.x * fx, o.y + s.y * fy), s.y * (0.18f + 0.04f * (float)(i % 3)), col(0.45f, 0.12f, 0.75f, 0.07f), 48);
    }
    for (int i = 0; i < 90; i++) { // stars
        unsigned h = (unsigned)i * 2654435761u;
        float x = (float)(h % 1000) / 1000.0f, y = (float)((h >> 10) % 1000) / 1000.0f * 0.75f;
        dl->AddCircleFilled(ImVec2(o.x + s.x * x, o.y + s.y * y), 1.0f + (float)(h >> 20 & 1), col(0.85f, 0.80f, 1.0f, 0.25f + 0.35f * (float)((h >> 22) % 3) / 2.0f), 6);
    }
    // the arena: a stone ring seen from above, to the right
    ImVec2 c(o.x + s.x * 0.80f, o.y + s.y * 0.86f);
    dl->AddEllipseFilled(c, ImVec2(s.x * 0.42f, s.y * 0.30f), col(0.10f, 0.08f, 0.16f), 0.0f, 96);
    dl->AddEllipseFilled(c, ImVec2(s.x * 0.38f, s.y * 0.26f), col(0.36f, 0.32f, 0.40f), 0.0f, 96);
    dl->AddEllipseFilled(c, ImVec2(s.x * 0.33f, s.y * 0.215f), col(0.55f, 0.50f, 0.52f), 0.0f, 96);
    for (int k = 0; k < 24; k++) { // the tiles' seams
        float a = (float)k / 24.0f * 6.2831853f;
        dl->AddLine(ImVec2(c.x + cosf(a) * s.x * 0.11f, c.y + sinf(a) * s.y * 0.07f), ImVec2(c.x + cosf(a) * s.x * 0.33f, c.y + sinf(a) * s.y * 0.215f),
                    col(0.30f, 0.27f, 0.32f, 0.8f), 1.5f);
    }
    dl->AddEllipseFilled(c, ImVec2(s.x * 0.11f, s.y * 0.07f), col(0.25f, 0.22f, 0.28f), 0.0f, 64);
    // a glow over the ring's middle (where the fighters would stand)
    for (int k = 0; k < 5; k++) {
        dl->AddEllipseFilled(ImVec2(c.x, c.y - s.y * 0.02f), ImVec2(s.x * (0.05f + 0.03f * (float)k), s.y * (0.035f + 0.02f * (float)k)), col(1.0f, 0.75f, 0.30f, 0.06f), 0.0f, 48);
    }
    // a darker veil on the left, where the cards are
    dl->AddRectFilledMultiColor(o, ImVec2(o.x + s.x * 0.62f, o.y + s.y), col(0.02f, 0.01f, 0.05f, 0.55f), col(0.02f, 0.01f, 0.05f, 0.0f),
                                col(0.02f, 0.01f, 0.05f, 0.0f), col(0.02f, 0.01f, 0.05f, 0.55f));
}

// text with a dark outline (the game's plates)
static void outlined(ImDrawList *dl, ImVec2 p, ImU32 c, const char *t) {
    for (int dx = -2; dx <= 2; dx += 2) {
        for (int dy = -2; dy <= 2; dy += 2) {
            if (dx != 0 || dy != 0) { dl->AddText(ImVec2(p.x + dx, p.y + dy), col(0.02f, 0.05f, 0.10f, 0.9f), t); }
        }
    }
    dl->AddText(p, c, t);
}

// ---- the header: the title, a violet line under it, the player's name in a badge on the right (touch: edits it)
static bool header(const char *title, bool back) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    const float u = U(), x0 = o.x + u * 1.6f, y0 = o.y + u * 1.0f;
    bool goBack = false;
    font_px(u * 1.15f);
    if (back) {
        ImGui::SetCursorScreenPos(ImVec2(o.x + u * 0.3f, y0 - u * 0.2f));
        if (ImGui::InvisibleButton("##back", ImVec2(u * 1.2f, u * 1.6f))) { goBack = true; }
        dl->AddText(ImVec2(o.x + u * 0.55f, y0), col(0.85f, 0.80f, 1.0f, ImGui::IsItemHovered() ? 1.0f : 0.7f), "<");
    }
    dl->AddText(ImVec2(x0, y0), ImGui::GetColorU32(kText), title);
    ImGui::PopFont();
    dl->AddLine(ImVec2(x0 - u * 0.4f, y0 + u * 1.6f), ImVec2(o.x + s.x * 0.62f, y0 + u * 1.6f), col(0.58f, 0.42f, 0.95f, 0.85f), 2.0f);
    // the name's badge
    font_px(u * 0.62f);
    const float bw = u * 6.4f, bh = u * 1.05f, bx = o.x + s.x * 0.62f - bw, by = y0 + u * 0.15f;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (ImGui::InvisibleButton("##name", ImVec2(bw, bh))) { sEditName = !sEditName; }
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), col(0.08f, 0.04f, 0.16f, 0.9f), bh * 0.25f);
    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), col(0.60f, 0.45f, 1.0f), bh * 0.25f, 0, 2.0f);
    ImVec2 ts = ImGui::CalcTextSize(sName);
    dl->AddText(ImVec2(bx + (bw - ts.x) * 0.5f, by + (bh - ts.y) * 0.5f), ImGui::GetColorU32(kGoldT), sName);
    ImGui::PopFont();
    return goBack;
}

// ---- the footer: up to three lines, bottom left
static void footer(const char *a, const char *b, const char *c) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    const float u = U();
    font_px(u * 0.62f);
    float y = o.y + s.y - u * 1.4f;
    const char *lines[3] = {c, b, a};
    for (int i = 0; i < 3; i++) {
        if (lines[i] != NULL && lines[i][0] != '\0') {
            dl->AddText(ImVec2(o.x + u * 1.6f, y), ImGui::GetColorU32(i == 0 ? kText : kSub), lines[i]);
            y -= u * 0.85f;
        }
    }
    ImGui::PopFont();
}

static const char *controls_hint(void) {
    return T("Touch or D-pad: choose   A: accept   B: back   Start: name", "Toque ou direcional: escolher   A: aceitar   B: voltar   Start: nome",
             "Toque o cruceta: elegir   A: aceptar   B: volver   Start: nombre");
}

// ---- the home menu's plates (the game's main menu look): cyan, the chosen one gold, from the left edge
static int home_plates(const char *const *names, const bool *enabled, int n) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos();
    const float u = U(), h = u * 1.25f, gap = u * 0.35f;
    int clicked = -1;
    font_px(u * 1.05f);
    for (int i = 0; i < n; i++) {
        const float slide = (1.0f - sEnter) * u * 8.0f * (float)(i + 1);
        const float x = o.x + u * (2.6f - 0.35f * (float)i) - slide, y = o.y + u * 3.4f + (float)i * (h + gap), w = u * 13.0f;
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##plate", ImVec2(w, h)) && enabled[i]) { clicked = i; }
        ImGui::PopID();
        const bool on = sHomeRow == i;
        if (on) { // the chosen one: a gold band from the left edge
            dl->AddRectFilledMultiColor(ImVec2(o.x, y), ImVec2(x + w + u * 1.5f, y + h), col(1.0f, 0.88f, 0.35f), col(0.95f, 0.75f, 0.15f, 0.0f),
                                        col(0.95f, 0.75f, 0.15f, 0.0f), col(1.0f, 0.88f, 0.35f));
        }
        ImVec2 q[4] = {ImVec2(x + h * 0.35f, y), ImVec2(x + w, y), ImVec2(x + w - h * 0.35f, y + h), ImVec2(x, y + h)};
        ImU32 top = enabled[i] ? (on ? col(0.98f, 0.92f, 0.55f) : col(0.40f, 0.95f, 1.0f)) : col(0.35f, 0.40f, 0.48f);
        ImU32 bot = enabled[i] ? (on ? col(0.92f, 0.70f, 0.12f) : col(0.05f, 0.55f, 0.75f)) : col(0.20f, 0.22f, 0.28f);
        dl->AddQuadFilled(q[0], q[1], ImVec2(q[1].x - h * 0.17f, y + h * 0.5f), ImVec2(x + h * 0.17f, y + h * 0.5f), top);
        dl->AddQuadFilled(ImVec2(x + h * 0.17f, y + h * 0.5f), ImVec2(q[1].x - h * 0.17f, y + h * 0.5f), q[2], q[3], bot);
        dl->AddQuad(q[0], q[1], q[2], q[3], col(1.0f, 1.0f, 1.0f, 0.55f), 2.0f);
        ImVec2 ts = ImGui::CalcTextSize(names[i]);
        outlined(dl, ImVec2(x + (w - ts.x) * 0.5f, y + (h - ts.y) * 0.5f), enabled[i] ? col(1, 1, 1) : col(0.7f, 0.7f, 0.75f), names[i]);
    }
    ImGui::PopFont();
    return clicked;
}

// ---- a card: dark violet with a violet border; its section label in gold above it
static ImVec2 sCardMin, sCardMax;
static void card_start(float y, float h, const char *label) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    const float u = U(), slide = (1.0f - sEnter) * s.x * 0.4f;
    if (label != NULL && label[0] != '\0') {
        font_px(u * 0.66f);
        dl->AddText(ImVec2(o.x + u * 1.7f - slide, o.y + y - u * 0.95f), ImGui::GetColorU32(kGoldT), label);
        ImGui::PopFont();
    }
    sCardMin = ImVec2(o.x + u * 1.3f - slide, o.y + y);
    sCardMax = ImVec2(o.x + s.x * 0.60f - slide, o.y + y + h);
    dl->AddRectFilled(sCardMin, sCardMax, col(0.07f, 0.03f, 0.15f, 0.93f), u * 0.25f);
    dl->AddRect(sCardMin, sCardMax, col(0.48f, 0.34f, 0.85f, 0.95f), u * 0.25f, 0, 2.0f);
}

// a row in a card: the label on the left, its value on the right; the chosen one gold. Returns 1 on a tap (or A),
// -1 / +1 in `change` for a value (left / right on the pad, the arrows by touch)
static bool card_row(int index, float y, const char *label, const char *value, int *change = NULL) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float u = U(), h = u * 1.35f, x0 = sCardMin.x + u * 0.5f, x1 = sCardMax.x - u * 0.5f, y0 = sCardMin.y + y;
    bool hit = false;
    ImGui::SetCursorScreenPos(ImVec2(x0, y0));
    ImGui::PushID(index);
    if (ImGui::InvisibleButton("##row", ImVec2(x1 - x0, h))) {
        if (sRow == index || change == NULL) { hit = true; }
        if (change != NULL && ImGui::GetIO().MousePos.x > x1 - u * 2.5f) { *change = 1; hit = false; }
        else if (change != NULL && sRow == index) { *change = 1; hit = false; }
        sRow = index;
    }
    ImGui::PopID();
    const bool on = sRow == index;
    if (on) {
        dl->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y0 + h), col(0.62f, 0.42f, 0.05f), col(0.78f, 0.55f, 0.10f), col(0.55f, 0.36f, 0.04f),
                                    col(0.45f, 0.28f, 0.02f));
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y0 + h), col(1.0f, 0.84f, 0.30f), u * 0.15f, 0, 3.0f);
        dl->AddRectFilled(ImVec2(x1 - u * 0.35f, y0 + h * 0.4f), ImVec2(x1 - u * 0.2f, y0 + h * 0.6f), col(1.0f, 0.95f, 0.75f));
    } else {
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y0 + h), col(0.12f, 0.07f, 0.24f, 0.95f), u * 0.15f);
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y0 + h), col(0.42f, 0.30f, 0.78f), u * 0.15f, 0, 2.0f);
    }
    dl->AddRectFilled(ImVec2(x0 + u * 0.25f, y0 + h * 0.2f), ImVec2(x0 + u * 0.32f, y0 + h * 0.8f), on ? col(1.0f, 0.9f, 0.5f) : col(0.55f, 0.42f, 0.95f));
    font_px(u * 0.78f);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(x0 + u * 0.7f, y0 + (h - ts.y) * 0.5f), col(1, 1, 1), label);
    if (value != NULL && value[0] != '\0') {
        font_px(u * 0.66f);
        ImVec2 vs = ImGui::CalcTextSize(value);
        float vx = x0 + (x1 - x0) * 0.62f;
        if (vx + vs.x > x1 - u * 0.8f) { vx = x1 - u * 0.8f - vs.x; }
        dl->AddText(ImVec2(vx, y0 + (h - vs.y) * 0.5f), on ? col(1.0f, 0.97f, 0.85f) : ImGui::GetColorU32(kSub), value);
        if (change != NULL) {
            dl->AddText(ImVec2(vx - u * 0.75f, y0 + (h - vs.y) * 0.5f), col(1.0f, 1.0f, 1.0f, on ? 0.9f : 0.35f), "<");
            dl->AddText(ImVec2(vx + vs.x + u * 0.35f, y0 + (h - vs.y) * 0.5f), col(1.0f, 1.0f, 1.0f, on ? 0.9f : 0.35f), ">");
        }
        ImGui::PopFont();
    }
    ImGui::PopFont();
    return hit;
}

// three dots that move along (something is under way)
static void dots(ImDrawList *dl, ImVec2 p) {
    const float u = U();
    int k = (int)(SDL_GetTicks() / 300 % 4);
    for (int i = 0; i < 4; i++) {
        dl->AddRectFilled(ImVec2(p.x + (float)i * u * 0.9f, p.y), ImVec2(p.x + (float)i * u * 0.9f + u * 0.55f, p.y + u * 0.08f),
                          i == k ? col(0.55f, 0.42f, 1.0f) : col(0.35f, 0.28f, 0.60f, 0.6f));
    }
}

// a waiting card: a gold-marked label, a big line, a note, the moving dots
static void wait_card(float y, const char *label, const char *big, const char *note, ImU32 bigCol) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float u = U();
    card_start(y, u * 5.6f, NULL);
    dl->AddRectFilled(ImVec2(sCardMin.x + u * 0.6f, sCardMin.y + u * 0.6f), ImVec2(sCardMin.x + u * 0.68f, sCardMin.y + u * 1.4f), col(1.0f, 0.84f, 0.30f));
    font_px(u * 0.7f);
    dl->AddText(ImVec2(sCardMin.x + u * 1.0f, sCardMin.y + u * 0.62f), col(1, 1, 1), label);
    ImGui::PopFont();
    font_px(u * 1.45f);
    dl->AddText(ImVec2(sCardMin.x + u * 1.1f, sCardMin.y + u * 1.75f), bigCol, big);
    ImGui::PopFont();
    font_px(u * 0.62f);
    dl->AddText(ImVec2(sCardMin.x + u * 1.0f, sCardMin.y + u * 3.6f), ImGui::GetColorU32(kText), note);
    ImGui::PopFont();
    dots(dl, ImVec2((sCardMin.x + sCardMax.x) * 0.5f - u * 1.6f, sCardMin.y + u * 4.7f));
}

// ---- pad input: up/down move, left/right change, A accept, B back
static bool pad(ImGuiKey k) { return pad_pressed(k); }
static bool padA(void) { return pad(ImGuiKey_GamepadFaceDown) || pad(ImGuiKey_Enter) || pad(ImGuiKey_KeypadEnter); }
static bool padB(void) { return pad(ImGuiKey_GamepadFaceRight) || pad(ImGuiKey_Escape); }
static int padMove(void) { return (pad(ImGuiKey_GamepadDpadDown) || pad(ImGuiKey_GamepadLStickDown) || pad(ImGuiKey_DownArrow)) ? 1 :
                                  (pad(ImGuiKey_GamepadDpadUp) || pad(ImGuiKey_GamepadLStickUp) || pad(ImGuiKey_UpArrow)) ? -1 : 0; }
static int padSide(void) { return (pad(ImGuiKey_GamepadDpadRight) || pad(ImGuiKey_GamepadLStickRight) || pad(ImGuiKey_RightArrow)) ? 1 :
                                  (pad(ImGuiKey_GamepadDpadLeft) || pad(ImGuiKey_GamepadLStickLeft) || pad(ImGuiKey_LeftArrow)) ? -1 : 0; }

static const char *battle_name(int b) {
    switch (b) {
    case 0: return T("Single", "Individual", "Individual");
    case 1: return T("Team", "Equipe", "Equipos");
    case 2: return "DP 10";
    case 3: return "DP 15";
    default: return "DP 20";
    }
}

static void rules_set(int battle) {
    Port_NetNameSet(sName);
    Port_SettingTextSave("net_name", sName);
    Port_NetOptions(Port_RollCan() ? 8 : 0, -2); // rollback when the build has it, the delay from the connection
    Port_NetSpeedSet(Port_NetSpeedStandard());
    Port_NetRulesSet(battle == 0 ? 0 : battle == 1 ? 1 : 2, battle >= 2 ? battle - 2 : 0, 4); // no time limit
    Port_SettingsWrite();
}

static const char *rules_line(void) {
    static char t[96];
    SDL_snprintf(t, sizeof(t), T("No time limit / Speed %d%%", "Sem limite de tempo / Velocidade %d%%", "Sin límite de tiempo / Velocidad %d%%"),
                 Port_NetSpeedStandard());
    return t;
}

// a service over its limit (HTTP 429, net_match.c): its daily requests, most often
static const char *limit_text(void) {
    return T("Server busy (daily limit), try again later.", "Servidor ocupado (limite diário), tente mais tarde.",
             "Servidor ocupado (límite diario), inténtalo más tarde.");
}

static const char *ranked_msg(int m) {
    switch (m) {
    case DRMSG_SEARCHING: return T("Searching for a rival...", "Procurando um rival...", "Buscando rival...");
    case DRMSG_SEARCHING_LEVEL: return T("Searching for a rival of your level...", "Procurando um rival do seu nível...", "Buscando rival de tu nivel...");
    case DRMSG_FOUND: return T("Rival found. Connecting...", "Rival encontrado. Conectando...", "Rival asignado. Conectando...");
    case DRMSG_CONFIRMED: return T("Result confirmed.", "Resultado confirmado.", "Resultado confirmado.");
    case DRMSG_ENDED_NO_POINTS: return T("Search over. No points.", "Busca encerrada. Sem pontos.", "Búsqueda finalizada. Sin puntos.");
    case DRMSG_WAIT_RIVAL: return T("Waiting for your rival's result.", "Esperando o resultado do rival.", "Esperando confirmación del rival.");
    case DRMSG_DISPUTED: return T("The two results differ: no points.", "Os dois resultados não batem: sem pontos.", "Resultados distintos: sin puntos.");
    case DRMSG_VOID: return T("Match void: no points.", "Partida anulada: sem pontos.", "Partida anulada: sin puntos.");
    case DRMSG_NO_ANSWER: return T("Ranked did not answer. Trying again...", "O ranqueado não respondeu. Tentando de novo...", "Ranked sin respuesta. Reintentando...");
    case DRMSG_REGISTER_FAILED: return T("The Ranked profile could not be made.", "Não foi possível criar o perfil ranqueado.", "No se pudo registrar el perfil Ranked.");
    case DRMSG_PROFILE_FAILED: return T("The ranking could not be updated.", "Não foi possível atualizar a classificação.", "No se pudo actualizar la clasificación.");
    case DRMSG_ROOM_FAILED: return T("The ranked room could not be published.", "Não foi possível publicar a sala ranqueada.", "No se pudo publicar la sala Ranked.");
    case DRMSG_FINISH_FIRST: return T("Your last ranked match is still being confirmed.", "Sua última partida ranqueada ainda está sendo confirmada.",
                                      "Tu última partida Ranked aún se está confirmando.");
    case DRMSG_VERSION: return T("The Dragon Net service does not take this version yet.", "O serviço Dragon Net ainda não aceita esta versão.",
                                 "El servicio Dragon Net aún no acepta esta versión.");
    case DRMSG_UPDATED: return T("Profile updated.", "Perfil atualizado.", "Perfil actualizado.");
    case DRMSG_BUSY: return T("Too many requests: wait a moment.", "Muitos pedidos: espere um pouco.", "Demasiadas solicitudes: espera un momento.");
    case DRMSG_LIMIT: return limit_text();
    default: return "";
    }
}

static const char *rank_tier(const char *server) {
    // the service names the tiers in Spanish (Bronce .. Maestro): worded for the player
    if (strcmp(server, "Bronce") == 0) { return T("Bronze", "Bronze", "Bronce"); }
    if (strcmp(server, "Plata") == 0) { return T("Silver", "Prata", "Plata"); }
    if (strcmp(server, "Oro") == 0) { return T("Gold", "Ouro", "Oro"); }
    if (strcmp(server, "Platino") == 0) { return T("Platinum", "Platina", "Platino"); }
    if (strcmp(server, "Diamante") == 0) { return T("Diamond", "Diamante", "Diamante"); }
    if (strcmp(server, "Maestro") == 0) { return T("Master", "Mestre", "Maestro"); }
    return server;
}

static const char *lobby_error(int last) {
    if (last == -3) { return strcmp(Port_LobbyError(), MATCH_LIMIT_TEXT) == 0 ? limit_text() : Port_LobbyError(); }
    if (last == -4) { return T("The two games could not reach each other. Try again.", "Os dois jogos não se alcançaram. Tente de novo.",
                               "Los dos juegos no pudieron conectarse. Inténtalo de nuevo."); }
    if (last == -5) { return T("Nobody searched in the last three minutes.", "Ninguém procurou nos últimos três minutos.", "Nadie buscó en los últimos tres minutos."); }
    if (last == -2) { return T("The other game is a different version.", "O outro jogo é de outra versão.", "El otro juego es de otra versión."); }
    return "";
}

// the lobby's state; a match found launches it (the window closes)
static int lobby_step(void) {
    int st = Port_LobbyPoll();
    sNetBusy = st == 1 || st == 3;
    if (st == 5) { // the code was a lobby's (the PC's 0.1.38.9): this game is its member, and the lobby's page is the game's own (the main menu opens it)
        Port_LobbyCancel(); // (the lobby has the socket now)
        const char *said = MenuTranslation_PortText(PORT_TEXT_LOBBY_JOINED);
        Port_UiNotice(said != NULL ? said : T("You are in the lobby. Its screen opens from the main menu (Dragon Net Battle).",
                                               "Você está no lobby. A tela dele abre pelo menu principal (Dragon Net Battle).",
                                               "Estás en el lobby. Su pantalla se abre desde el menú principal (Dragon Net Battle)."));
        set_open(false);
        sNet = false;
        DrNet_PresenceActive(0);
        DrChat_Close();
        goto_screen(HOME, sHomeRow);
        return st;
    }
    if (st < 0) { sLobbyLast = st; }
    if (st == -3 || st == -4 || st == -2) { sLobbyLast = st; }
    if (st == 2) {
        if (sRankedLobby) { DrRanked_Connection(1); DrRanked_Handover(); }
        sRankedLobby = false;
        Port_LobbyLaunch();
        set_open(false);
        sNet = false;
        DrNet_PresenceActive(0);
        DrChat_Close();
        goto_screen(HOME, sHomeRow);
    }
    return st;
}

static void leave_window(void) {
    Port_LobbyCancel();
    DrRanked_Cancel();
    DrNet_PresenceActive(0);
    DrChat_Close();
    sRankedLobby = false;
    goto_screen(HOME, 0);
    set_open(false);
}

// ---- the screens

static void screen_home(bool &close) {
    const bool dragon = DrNet_Mode() == 0;
    const char *names[5] = {T("Create room", "Criar sala", "Crear sala"), T("Join room", "Entrar em sala", "Entrar a sala"),
                            T("Quick match", "Partida rápida", "Partida rápida"), T("Ranked", "Classificatória", "Clasificatoria"),
                            T("General chat", "Chat geral", "Chat general")};
    const bool enabled[5] = {true, true, true, dragon, dragon && DRNET_CHAT != 0}; // (the chat: off for now, net_dragon.h)
    if (header("Dragon Net Battle", true)) { close = true; }
    int m = padMove();
    if (m != 0) { do { sHomeRow = (sHomeRow + m + 5) % 5; } while (!enabled[sHomeRow]); }
    int clicked = home_plates(names, enabled, 5);
    if (clicked >= 0) { sHomeRow = clicked; }
    if (padA() || clicked >= 0) {
        static const Screen to[5] = {CREATE, JOIN, QUICK, RANKED, CHAT};
        goto_screen(to[sHomeRow]);
        if (sHomeRow == 3) { DrRanked_Open(sName); }
        if (sHomeRow == 4) { DrChat_Open(sName); sEditChat = false; }
    }
    if (padB()) { close = true; }
    // the opponents: Dragon Net (Android + Switch) or PC
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetWindowPos();
        const float u = U(), y = o.y + u * 3.4f + 5.0f * u * 1.6f + u * 0.4f, x = o.x + u * 1.6f;
        font_px(u * 0.66f);
        const char *lab = T("Play against:", "Jogar contra:", "Jugar contra:");
        dl->AddText(ImVec2(x, y + u * 0.18f), ImGui::GetColorU32(kSub), lab);
        float cx = x + ImGui::CalcTextSize(lab).x + u * 0.5f;
        const char *opt[2] = {T("Android + Switch", "Android + Switch", "Android + Switch"), "PC"};
        for (int i = 0; i < 2; i++) {
            ImVec2 ts = ImGui::CalcTextSize(opt[i]);
            ImVec2 a(cx, y), b(cx + ts.x + u * 0.9f, y + u * 0.95f);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(100 + i);
            if (ImGui::InvisibleButton("##mode", ImVec2(b.x - a.x, b.y - a.y)) && DrNet_Mode() != i) {
                DrNet_SetMode(i);
                DrNet_PresenceActive(i == 0);
                Port_LobbyHelloAsk();
                if (i == 1 && sHomeRow >= 3) { sHomeRow = 0; }
            }
            ImGui::PopID();
            const bool on = DrNet_Mode() == i;
            dl->AddRectFilled(a, b, on ? col(0.80f, 0.58f, 0.10f) : col(0.10f, 0.06f, 0.20f, 0.9f), u * 0.45f);
            dl->AddRect(a, b, on ? col(1.0f, 0.86f, 0.35f) : col(0.48f, 0.34f, 0.85f), u * 0.45f, 0, 2.0f);
            dl->AddText(ImVec2(a.x + u * 0.45f, a.y + (u * 0.95f - ts.y) * 0.5f), on ? col(1, 1, 1) : ImGui::GetColorU32(kSub), opt[i]);
            cx = b.x + u * 0.35f;
        }
        ImGui::PopFont();
        if (pad(ImGuiKey_GamepadFaceUp)) { // Y switches the opponents
            int i = 1 - DrNet_Mode();
            DrNet_SetMode(i);
            DrNet_PresenceActive(i == 0);
            Port_LobbyHelloAsk();
            if (i == 1 && sHomeRow >= 3) { sHomeRow = 0; }
        }
    }
    // the players online, or the PC's release check
    static char line[160];
    if (dragon) {
        int n = DrNet_Online();
        if (n >= 0) { SDL_snprintf(line, sizeof(line), T("Online: %d players (approx.)", "Online: %d jogadores (aprox.)", "En línea: %d jugadores (aprox.)"), n); }
        else if (n == -1) { SDL_strlcpy(line, T("Online: checking...", "Online: consultando...", "En línea: consultando..."), sizeof(line)); }
        else { SDL_strlcpy(line, T("Online: not available", "Online: indisponível", "En línea: no disponible"), sizeof(line)); }
    } else {
        int min = 0, latest = 0, mine = Port_LobbyRelease(&min, &latest);
        if (min > mine) {
            SDL_snprintf(line, sizeof(line), T("Against PC: the PC service asks for release 0.%d.%d (this is 0.%d.%d). Update Dragon Rage.",
                                               "Contra PC: o serviço do PC pede a versão 0.%d.%d (esta é 0.%d.%d). Atualize o Dragon Rage.",
                                               "Contra PC: el servicio del PC pide la versión 0.%d.%d (esta es 0.%d.%d). Actualiza Dragon Rage."),
                         min / 100, min % 100, mine / 100, mine % 100);
        } else {
            SDL_strlcpy(line, T("Against PC: rooms and quick match on the PC port's service.", "Contra PC: salas e partida rápida no serviço do port de PC.",
                                "Contra PC: salas y partida rápida en el servicio del port de PC."), sizeof(line));
        }
    }
    footer(line, T("Use your ORIGINAL copy, legally obtained.", "Use sua cópia ORIGINAL, obtida legalmente.", "Usa tu copia ORIGINAL obtenida legalmente."),
           controls_hint());
}

static void screen_create(void) {
    if (header(T("Create room", "Criar sala", "Crear sala"), true) || padB()) { goto_screen(HOME); return; }
    const float u = U();
    card_start(u * 4.2f, u * 6.7f, T("PREPARE YOUR FIGHT", "PREPARE SEU COMBATE", "PREPARA TU COMBATE"));
    int m = padMove();
    if (m != 0) { sRow = (sRow + m + 4) % 4; }
    int ch = 0;
    card_row(0, u * 0.45f, T("Battle", "Combate", "Combate"), battle_name(sBattle), &ch);
    if (sRow == 0) { ch += padSide(); }
    if (ch != 0) { sBattle = (sBattle + ch + 5) % 5; }
    bool go = card_row(1, u * 2.0f, T("Open room", "Abrir sala", "Abrir sala"), "") || (sRow == 1 && padA());
    if (sRow == 0 && padA()) { sBattle = (sBattle + 1) % 5; }
    // a lobby (the PC's 0.1.38.9, net_lobby.c): up to eight games meet at this one, two fight and the rest watch; its page is the game's own
    // (Tenkaichi Budokai Online's, worked with the pad: the on-screen controller), which the main menu opens once a lobby is on
    bool lobby = card_row(2, u * 3.55f, T("Open a lobby (up to 8)", "Abrir um lobby (até 8)", "Abrir un lobby (hasta 8)"), "") || (sRow == 2 && padA());
    // spectators of this game's matches (the setting net_spectators, as the PC's Host tab and the touch menu's card)
    {
        int wch = 0;
        const bool watch = Port_Setting("net_spectators", 1) != 0;
        card_row(3, u * 5.1f, T("Let others watch", "Deixar outros assistirem", "Dejar que otros miren"), watch ? T("Yes", "Sim", "Sí") : T("No", "Não", "No"), &wch);
        if (sRow == 3) { wch += padSide(); if (padA()) { wch = 1; } }
        if (wch != 0) { Port_SettingSave("net_spectators", watch ? 0 : 1); Port_SettingsWrite(); }
    }
    if (lobby) {
        sLobbyLast = 0;
        rules_set(sBattle);
        Port_SettingSave("net_battle", sBattle);
        Port_LobbyCancel();
        Port_NetNameSet(sName);
        // (the rules as Create Room has them: no time limit, the competitive speed, rollback 8, the delay from the connection
        // (the lobby's 0 = auto, as rules_set's -2); the lobby's page changes them)
        if (Port_LobbyCreate(1, 0, sBattle, 4, Port_Setting("net_speed_host", Port_NetSpeedStandard()), Port_RollCan() ? 8 : 0, 0) >= 0) {
            set_open(false);
            DrNet_PresenceActive(0);
            DrChat_Close();
            goto_screen(HOME, 0);
            return;
        }
        sLobbyLast = -3;
    }
    const char *err = lobby_error(sLobbyLast);
    footer(err[0] ? err : rules_line(), "", controls_hint());
    if (go) {
        sLobbyLast = 0;
        rules_set(sBattle);
        Port_SettingSave("net_battle", sBattle);
        Port_LobbyStartCode(1, "");
        goto_screen(CREATE_WAIT);
    }
}

static void screen_create_wait(void) {
    const char *code = Port_LobbyCode();
    int st = lobby_step();
    if (!sOpen) { return; }
    if (header(code[0] ? T("Room created", "Sala criada", "Sala creada") : T("Opening room", "Abrindo sala", "Abriendo sala"), true) || padB() || st < 0) {
        if (st >= 0) { Port_LobbyCancel(); }
        goto_screen(CREATE, 1);
        return;
    }
    const float u = U();
    wait_card(u * 4.6f, T("ROOM CODE", "CÓDIGO DA SALA", "CÓDIGO DE SALA"), code[0] ? code : "------",
              T("Share the code with your rival.", "Passe o código para o seu rival.", "Comparte el código con tu rival."), col(1.0f, 0.86f, 0.30f));
    if (code[0]) { // copy: a tap on the card
        static Uint64 copied;
        ImGui::SetCursorScreenPos(sCardMin);
        if (ImGui::InvisibleButton("##copy", ImVec2(sCardMax.x - sCardMin.x, sCardMax.y - sCardMin.y)) || pad(ImGuiKey_GamepadFaceUp)) {
            copied = SDL_SetClipboardText(code) ? SDL_GetTicks() : 0;
        }
        font_px(u * 0.6f);
        ImGui::GetWindowDrawList()->AddText(ImVec2(sCardMax.x - u * 5.2f, sCardMin.y + u * 0.62f), ImGui::GetColorU32(kSub),
                                            copied && SDL_GetTicks() - copied < 2000 ? T("Copied", "Copiado", "Copiado") : T("Tap: copy", "Toque: copiar", "Toca: copiar"));
        ImGui::PopFont();
    }
    footer(st == 1 ? T("Your rival is here. Connecting...", "Seu rival chegou. Conectando...", "Tu rival llegó. Conectando...")
                   : T("Waiting for another player to join...", "Esperando outro jogador entrar...", "Esperando a que se una otro jugador..."),
           "", T("B: cancel", "B: cancelar", "B: cancelar"));
}

static void code_field(float y) {
    // the code typed: an input over the first row while it is edited (the phone's keyboard opens)
    const float u = U();
    ImGui::SetCursorScreenPos(ImVec2(sCardMin.x + u * 6.0f, sCardMin.y + y + u * 0.18f));
    ImGui::PushItemWidth(sCardMax.x - sCardMin.x - u * 7.0f);
    font_px(u * 0.8f);
    if (!ImGui::IsAnyItemActive()) { ImGui::SetKeyboardFocusHere(); }
    if (ImGui::InputText("##code", sCode, 7, ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue)) {
        sEditCode = false;
        sRow = 1;
    }
    if (ImGui::IsItemDeactivated()) { sEditCode = false; }
    ImGui::PopFont();
    ImGui::PopItemWidth();
}

static void screen_join(void) {
    if (header(T("Join room", "Entrar em sala", "Entrar a sala"), true) || (!sEditCode && padB())) { goto_screen(HOME, 1); sHomeRow = 1; return; }
    const float u = U();
    card_start(u * 4.2f, u * 5.15f, T("INVITATION BY CODE", "CONVITE POR CÓDIGO", "INVITACIÓN POR CÓDIGO"));
    if (!sEditCode) {
        int m = padMove();
        if (m != 0) { sRow = (sRow + m + 3) % 3; }
    }
    if ((card_row(0, u * 0.45f, T("Room code", "Código da sala", "Código de sala"),
                  sCode[0] ? sCode : T("Type the code", "Escrever código", "Escribir código")) || (sRow == 0 && !sEditCode && padA()))) {
        sEditCode = true;
    }
    if (sEditCode) { code_field(u * 0.45f); }
    bool go = card_row(1, u * 2.0f, T("Join the room", "Entrar na sala", "Unirse a sala"), "") || (sRow == 1 && !sEditCode && padA());
    // the same room's match, only watched (a spectator, Port_LobbySpectateCode: up to six; the controller does nothing in it)
    bool watch = card_row(2, u * 3.55f, T("Watch the match", "Assistir à partida", "Ver la partida"), "") || (sRow == 2 && !sEditCode && padA());
    // paste
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        font_px(u * 0.62f);
        const char *p = T("Paste", "Colar", "Pegar");
        ImVec2 ts = ImGui::CalcTextSize(p), a(sCardMax.x + u * 0.4f, sCardMin.y + u * 0.6f), b(a.x + ts.x + u * 1.0f, a.y + u * 1.05f);
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton("##paste", ImVec2(b.x - a.x, b.y - a.y)) || pad(ImGuiKey_GamepadFaceUp)) {
            char *clip = SDL_GetClipboardText();
            size_t n = 0;
            for (const char *c = clip; c != NULL && *c != '\0' && n < 6; c++) {
                if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) { sCode[n++] = (char)(*c >= 'a' ? *c - 32 : *c); }
            }
            sCode[n] = '\0';
            SDL_free(clip);
        }
        dl->AddRectFilled(a, b, col(0.10f, 0.06f, 0.20f, 0.9f), u * 0.3f);
        dl->AddRect(a, b, col(0.48f, 0.34f, 0.85f), u * 0.3f, 0, 2.0f);
        dl->AddText(ImVec2(a.x + u * 0.5f, a.y + (u * 1.05f - ts.y) * 0.5f), ImGui::GetColorU32(kText), p);
        ImGui::PopFont();
    }
    const char *err = lobby_error(sLobbyLast);
    footer(err[0] ? err : T("Enter a code of 6 letters or numbers.", "Digite um código de 6 letras ou números.", "Introduce un código de 6 letras o números."),
           T("Use the 6 characters the host sent you.", "Use os 6 caracteres que o anfitrião te mandou.", "Usa los 6 caracteres que te envió el anfitrión."),
           controls_hint());
    if (go && strlen(sCode) == 6) {
        sLobbyLast = 0;
        Port_NetNameSet(sName);
        Port_NetOptions(-1, -1); // (the host's choices)
        Port_LobbyStartCode(0, sCode);
        goto_screen(JOIN_WAIT);
    }
    if (watch && strlen(sCode) == 6) {
        sLobbyLast = 0;
        Port_NetNameSet(sName);
        Port_SettingTextSave("net_name", sName);
        Port_SettingsWrite();
        Port_LobbySpectateCode(sCode);
        goto_screen(WATCH_WAIT);
    }
}

static void screen_watch_wait(void) {
    int st = lobby_step();
    if (!sOpen) { return; }
    if (header(T("Watch the match", "Assistir à partida", "Ver la partida"), true) || padB() || st < 0) {
        if (st >= 0) { Port_LobbyCancel(); }
        goto_screen(JOIN, 2);
        return;
    }
    wait_card(U() * 4.6f, T("ROOM CODE", "CÓDIGO DA SALA", "CÓDIGO DE SALA"), sCode,
              Port_LobbySpectateWaiting() ? T("The host is there: waiting for the match to begin...", "O anfitrião está lá: esperando a partida começar...",
                                              "El anfitrión está: esperando que empiece la partida...")
              : st == 1 ? T("Room found. Connecting...", "Sala encontrada. Conectando...", "Sala encontrada. Conectando...")
                        : T("Looking for the room...", "Procurando a sala...", "Buscando la sala..."), col(1.0f, 0.86f, 0.30f));
    footer(T("You will see the match about a second behind; your controller does nothing in it. R1: view. START: menu (view, stop watching).",
             "Você verá a partida cerca de um segundo atrás; seu controle não mexe nela. R1: câmera. START: menu (câmera, parar de assistir).",
             "Verás la partida un segundo atrás; tu control no hace nada en ella. R1: cámara. START: menú (cámara, dejar de ver)."), "", T("B: cancel", "B: cancelar", "B: cancelar"));
}

static void screen_join_wait(void) {
    int st = lobby_step();
    if (!sOpen) { return; }
    if (header(T("Join room", "Entrar em sala", "Entrar a sala"), true) || padB() || st < 0) {
        if (st >= 0) { Port_LobbyCancel(); }
        goto_screen(JOIN, 1);
        return;
    }
    wait_card(U() * 4.6f, T("ROOM CODE", "CÓDIGO DA SALA", "CÓDIGO DE SALA"), sCode,
              st == 1 ? T("Room found. Connecting...", "Sala encontrada. Conectando...", "Sala encontrada. Conectando...")
                      : T("Looking for the room...", "Procurando a sala...", "Buscando la sala..."), col(1.0f, 0.86f, 0.30f));
    footer("", "", T("B: cancel", "B: cancelar", "B: cancelar"));
}

static void screen_quick(void) {
    if (header(T("Quick match", "Partida rápida", "Partida rápida"), true) || padB()) { goto_screen(HOME, 2); return; }
    static Uint64 asked;
    if (asked == 0 || SDL_GetTicks() - asked > 30000) { asked = SDL_GetTicks() + 1; Port_LobbyQueuesAsk(); }
    const float u = U();
    card_start(u * 4.2f, u * 5.15f, T("CHOOSE YOUR FIGHT", "ESCOLHA SEU COMBATE", "ELIGE TU COMBATE"));
    int m = padMove();
    if (m != 0) { sRow = (sRow + m + 3) % 3; }
    int region = Port_Setting("tbo_region", 0); // (the PC's screens' setting: the same choice here and in Tenkaichi Budokai Online)
    if (region < 0 || region > 8) { region = 0; }
    char val[64];
    int waiting = Port_LobbyQueueWaiting(sQueue2);
    if (waiting > 0) { SDL_snprintf(val, sizeof(val), T("%s (%d waiting)", "%s (%d esperando)", "%s (%d esperando)"), battle_name(sQueue2), waiting); }
    else { SDL_strlcpy(val, battle_name(sQueue2), sizeof(val)); }
    int ch = 0;
    card_row(0, u * 0.45f, T("Battle", "Combate", "Combate"), val, &ch);
    if (sRow == 0) { ch += padSide(); }
    if (sRow == 0 && padA()) { ch = 1; }
    if (ch != 0) { sQueue2 = (sQueue2 + ch + 5) % 5; }
    // the region (the PC's 0.1.38.9): players there first, everyone after a while
    {
        static const char *const names[9][3] = {{"Auto (nearest first)", "Auto (mais perto primeiro)", "Auto (más cerca primero)"},
            {"North America", "América do Norte", "América del Norte"}, {"South America", "América do Sul", "América del Sur"},
            {"Europe", "Europa", "Europa"}, {"Middle East", "Oriente Médio", "Oriente Medio"}, {"Africa", "África", "África"},
            {"Asia", "Ásia", "Asia"}, {"Oceania", "Oceania", "Oceanía"}, {"Anywhere", "Qualquer lugar", "Cualquier lugar"}};
        int rch = 0;
        card_row(1, u * 2.0f, T("Region", "Região", "Región"), T(names[region][0], names[region][1], names[region][2]), &rch);
        if (sRow == 1) { rch += padSide(); if (padA()) { rch = 1; } }
        if (rch != 0) { region = (region + rch + 9) % 9; Port_SettingSave("tbo_region", region); Port_SettingsWrite(); }
    }
    bool go = card_row(2, u * 3.55f, T("Search for a rival", "Buscar rival", "Buscar rival"), "") || (sRow == 2 && padA());
    const char *err = lobby_error(sLobbyLast);
    footer(err[0] ? err : rules_line(), "", controls_hint());
    if (go) {
        sLobbyLast = 0;
        rules_set(sQueue2);
        Port_SettingSave("net_queue", sQueue2);
        Port_LobbyQuickArea(kRegionArea[region]);
        Port_LobbyStartQuick(sQueue2);
        sSearchSince = SDL_GetTicks();
        sFoundAt = 0;
        goto_screen(QUICK_WAIT);
    }
}

static void screen_quick_wait(void) {
    int st = lobby_step();
    if (!sOpen) { return; }
    if (st == 3 && SDL_GetTicks() - sSearchSince > 180000) { // three minutes: nobody
        Port_LobbyCancel();
        sLobbyLast = -5;
        st = -5;
    }
    if (header(T("Searching for a rival", "Buscando rival", "Buscando rival"), true) || padB() || st < 0) {
        if (st >= 0) { Port_LobbyCancel(); }
        goto_screen(QUICK, 2);
        return;
    }
    wait_card(U() * 4.6f, st == 1 ? T("RIVAL FOUND", "RIVAL ENCONTRADO", "RIVAL ENCONTRADO") : T("SEARCH UNDER WAY", "BUSCA EM ANDAMENTO", "BÚSQUEDA EN CURSO"),
              battle_name(sQueue2), T("We will tell you when a rival is found.", "Avisamos quando acharmos um rival.", "Te avisaremos cuando encontremos rival."),
              col(1.0f, 0.86f, 0.30f));
    footer(T("You can cancel and go back to the menu.", "Você pode cancelar e voltar ao menu.", "Puedes cancelar y volver al menú."), "", T("B: cancel", "B: cancelar", "B: cancelar"));
}

static void profile_card(const DrRanked &r, float y) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float u = U();
    card_start(y, u * 2.2f, NULL);
    dl->AddRectFilled(ImVec2(sCardMin.x + u * 0.5f, sCardMin.y + u * 0.35f), ImVec2(sCardMin.x + u * 0.58f, sCardMin.y + u * 1.85f), col(1.0f, 0.84f, 0.30f));
    font_px(u * 0.8f);
    if (!r.profile) {
        dl->AddText(ImVec2(sCardMin.x + u * 0.9f, sCardMin.y + u * 0.6f), col(1, 1, 1), T("Loading profile...", "Carregando perfil...", "Cargando perfil..."));
        ImGui::PopFont();
        return;
    }
    char t[64];
    dl->AddText(ImVec2(sCardMin.x + u * 0.9f, sCardMin.y + u * 0.3f), ImGui::GetColorU32(kGoldT), rank_tier(r.rank));
    SDL_snprintf(t, sizeof(t), "%d Elo", r.rating);
    dl->AddText(ImVec2(sCardMin.x + (sCardMax.x - sCardMin.x) * 0.62f, sCardMin.y + u * 0.3f), col(1, 1, 1), t);
    ImGui::PopFont();
    font_px(u * 0.66f);
    SDL_snprintf(t, sizeof(t), T("%d W / %d L", "%d V / %d D", "%d V / %d D"), r.wins, r.losses);
    dl->AddText(ImVec2(sCardMin.x + u * 0.9f, sCardMin.y + u * 1.25f), ImGui::GetColorU32(kText), t);
    SDL_snprintf(t, sizeof(t), T("Position %d", "Posição %d", "Puesto %d"), r.position);
    dl->AddText(ImVec2(sCardMin.x + (sCardMax.x - sCardMin.x) * 0.62f, sCardMin.y + u * 1.25f), ImGui::GetColorU32(kText), t);
    ImGui::PopFont();
}

static void screen_ranked(void) {
    DrRanked r;
    DrRanked_Poll(&r);
    if (header(T("Ranked", "Classificatória", "Clasificatoria"), true) || padB()) { goto_screen(HOME, 3); return; }
    const float u = U();
    profile_card(r, u * 3.3f);
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        font_px(u * 0.62f);
        dl->AddText(ImVec2(ImGui::GetWindowPos().x + u * 1.6f, ImGui::GetWindowPos().y + u * 5.75f), ImGui::GetColorU32(kGoldT), rules_line());
        ImGui::PopFont();
    }
    card_start(u * 6.5f, u * 6.6f, NULL);
    int m = padMove();
    if (m != 0) { sRow = (sRow + m + 4) % 4; }
    int ch = 0;
    card_row(0, u * 0.35f, T("Battle", "Combate", "Combate"), sRankMode ? T("Team", "Equipe", "Equipos") : T("Single", "Individual", "Individual"), &ch);
    if (sRow == 0) { ch += padSide(); }
    if (sRow == 0 && padA()) { ch = 1; }
    if (ch != 0) { sRankMode = 1 - sRankMode; }
    bool search = card_row(1, u * 1.9f, T("Search for a rival", "Buscar rival", "Buscar rival"), "") || (sRow == 1 && padA());
    bool board = card_row(2, u * 3.45f, T("Ranking", "Classificação", "Clasificación"), "") || (sRow == 2 && padA());
    bool update = card_row(3, u * 5.0f, T("Update profile", "Atualizar perfil", "Actualizar perfil"), "") || (sRow == 3 && padA());
    const char *msg = ranked_msg(r.msg);
    footer(msg[0] && r.state < 0 ? msg : T("Points change when both results agree.", "Os pontos mudam quando os dois resultados batem.",
                                           "Los puntos cambian al confirmar ambos resultados."), "", controls_hint());
    if (search) {
        DrRanked_Search(sName, sRankMode);
        sFoundAt = 0;
        sLobbyLast = 0;
        goto_screen(RANKED_WAIT);
    }
    if (board) { DrRanked_Board(); goto_screen(BOARD); }
    if (update) { DrRanked_Open(sName); }
}

static void screen_ranked_wait(void) {
    DrRanked r;
    DrRanked_Poll(&r);
    int st = sRankedLobby ? lobby_step() : 0;
    if (!sOpen) { return; }
    if (header(T("Searching for a rival", "Buscando rival", "Buscando rival"), true) || padB() || (r.state == 0 && !r.searching) || st < 0) {
        if (sRankedLobby && st >= 0) { Port_LobbyCancel(); }
        sRankedLobby = false;
        DrRanked_Cancel();
        goto_screen(RANKED, 1);
        return;
    }
    // paired: the host opens the room and tells the service its code; the other joins it once the code is there
    if (r.state == 3 && !sRankedLobby) {
        if (sFoundAt == 0) { sFoundAt = SDL_GetTicks(); }
        if (r.role == 1) {
            rules_set(r.mode ? 1 : 0);
            Port_LobbyStartCode(1, "");
            sRankedLobby = true;
        } else if (r.role == 2 && strlen(r.code) == 6) {
            Port_NetNameSet(sName);
            Port_NetOptions(-1, -1);
            Port_LobbyStartCode(0, r.code);
            sRankedLobby = true;
        }
    }
    if (sRankedLobby && r.role == 1 && Port_LobbyCode()[0] != '\0') { DrRanked_SetRoom(Port_LobbyCode()); }
    char big[64];
    SDL_strlcpy(big, r.state == 3 && r.peer[0] ? r.peer : (r.mode ? T("Team", "Equipe", "Equipos") : T("Single", "Individual", "Individual")), sizeof(big));
    wait_card(U() * 4.6f, r.state == 3 ? T("RIVAL FOUND", "RIVAL ENCONTRADO", "RIVAL ENCONTRADO") : T("SEARCH UNDER WAY", "BUSCA EM ANDAMENTO", "BÚSQUEDA EN CURSO"),
              big, ranked_msg(r.msg), col(1.0f, 0.86f, 0.30f));
    footer(T("Points change when both results agree.", "Os pontos mudam quando os dois resultados batem.", "Los puntos cambian al confirmar ambos resultados."),
           "", T("B: cancel", "B: cancelar", "B: cancelar"));
}

static void screen_board(void) {
    DrRanked r;
    DrRanked_Poll(&r);
    if (header(T("Ranking", "Classificação", "Clasificación"), true) || padB()) { goto_screen(RANKED, 2); return; }
    const float u = U();
    card_start(u * 4.2f, u * 9.8f, T("BEST FIGHTERS", "MELHORES LUTADORES", "MEJORES LUCHADORES"));
    ImDrawList *dl = ImGui::GetWindowDrawList();
    font_px(u * 0.72f);
    for (int i = 0; i < 9; i++) {
        float y = sCardMin.y + u * 0.35f + (float)i * u * 1.03f;
        dl->AddRectFilled(ImVec2(sCardMin.x + u * 0.4f, y), ImVec2(sCardMax.x - u * 0.4f, y + u * 0.9f), col(0.12f, 0.07f, 0.24f, i % 2 ? 0.7f : 0.95f), u * 0.1f);
        if (r.board == 2 && i < r.boardCount) {
            char t[48];
            SDL_snprintf(t, sizeof(t), "%d.  %s", i + 1, r.boardName[i]);
            dl->AddText(ImVec2(sCardMin.x + u * 0.8f, y + u * 0.08f), i == 0 ? ImGui::GetColorU32(kGoldT) : col(1, 1, 1), t);
            SDL_snprintf(t, sizeof(t), "%d", r.boardRating[i]);
            dl->AddText(ImVec2(sCardMin.x + (sCardMax.x - sCardMin.x) * 0.62f, y + u * 0.08f), col(1, 1, 1), t);
        }
    }
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(sCardMin);
    if (ImGui::InvisibleButton("##refresh", ImVec2(sCardMax.x - sCardMin.x, sCardMax.y - sCardMin.y)) || padA()) { DrRanked_Board(); }
    footer(r.board == 1 ? T("Updating the ranking...", "Atualizando a classificação...", "Actualizando clasificación...")
                        : r.board < 0 ? T("The ranking did not answer: tap to try again.", "A classificação não respondeu: toque para tentar de novo.",
                                          "La clasificación no respondió: toca para reintentar.")
                                      : T("The table shows the community's Elo points.", "A tabela mostra os pontos Elo da comunidade.",
                                          "La tabla muestra los puntos Elo de la comunidad."),
           "", T("Tap or A: update   B: back", "Toque ou A: atualizar   B: voltar", "Toca o A: actualizar   B: volver"));
}

// ---- Rematch (flow:2): after the result of a ranked match. While it is open the game waits in MainMenu_Run
// (menu_a.c); gPortRematchDone says how it ended: 1 = leave (the online menu), 2 = a new match was saved
// (dragonnet-next.txt: the game restarts and rematch_boot() picks it up).
// gPortRematchRequest/Done e gPortNetWindowClose: declarados no escopo global em ui.cpp.
// Dentro de dn virariam outro simbolo (dn::...) e o link falharia.
static Uint64 sRematchSince, sRematchAccept, sRematchLeave;
static bool sRematchBooted;

static void rematch_open(void) {
    sRematchSince = SDL_GetTicks();
    sRematchAccept = sRematchLeave = 0;
    DrRanked_FlowAct(1);
    goto_screen(REMATCH, 0);
}

static void rematch_done(int how) {
    gPortRematchDone = how;
    gPortNetWindowClose = 1; // the window closes by the same way as when a session ends
}

// The game has restarted after a rematch was agreed: the new match is resumed as a paired search and the lobby takes over.
static void rematch_boot(void) {
    char m[33];
    int r, md;
    if (!DrRanked_FlowNext(m, &r, &md)) {
        return;
    }
    DrRanked_Open(sName);
    if (DrRanked_Resume()) {
        net_open();
        sFoundAt = 0;
        sLobbyLast = 0;
        goto_screen(RANKED_WAIT, 0);
    }
}

static void screen_rematch(void) {
    const float u = U();
    const Uint64 now = SDL_GetTicks();
    const int st = DrRanked_FlowState();
    const bool back = header(T("Rematch", "Revanche", "Revancha"), true) || padB();
    if (!sRematchLeave && back) { // B / back: the rival is told, then the game goes on to the online menu
        DrRanked_FlowAct(3);
        sRematchLeave = now;
    }
    if (st == 5) { // both accepted: the new match is saved
        rematch_done(2);
        return;
    }
    if (sRematchLeave) {
        if (st == 4 || now - sRematchLeave > 2500) {
            rematch_done(1);
            return;
        }
    } else if (st == 4) {
        rematch_done(1);
        return;
    } else if (!sRematchAccept && st < 2 && now - sRematchSince > 20000) { // the service does not answer
        rematch_done(1);
        return;
    } else if (sRematchAccept && now - sRematchAccept > 15000) { // the rival did not accept in time
        DrRanked_FlowAct(3);
        sRematchLeave = now;
    }
    const char *big;
    const char *note;
    if (sRematchLeave) {
        big = T("LEAVING", "SAINDO", "SALIENDO");
        note = T("Telling the rival...", "Avisando o rival...", "Avisando al rival...");
    } else if (sRematchAccept) {
        big = T("WAITING", "AGUARDANDO", "ESPERANDO");
        note = T("Waiting for the rival to accept...", "Esperando o rival aceitar...", "Esperando que el rival acepte...");
    } else if (st == 2) {
        big = T("REMATCH?", "REVANCHE?", "¿REVANCHA?");
        note = T("Both must accept. The result is saved.", "Os dois precisam aceitar. O resultado ja esta salvo.",
                 "Ambos deben aceptar. El resultado ya esta guardado.");
    } else {
        big = T("CONFIRMING", "CONFIRMANDO", "CONFIRMANDO");
        note = T("Confirming the result...", "Confirmando o resultado...", "Confirmando el resultado...");
    }
    wait_card(u * 3.3f, T("Online match", "Partida online", "Partida en linea"), big, note, col(1.0f, 0.86f, 0.30f));
    if (st == 2 && !sRematchAccept && !sRematchLeave) {
        card_start(u * 9.2f, u * 3.3f, NULL);
        int m = padMove();
        if (m != 0) {
            sRow = (sRow + m + 2) % 2;
        }
        bool again = card_row(0, u * 0.35f, T("Rematch", "Revanche", "Revancha"), "") || (sRow == 0 && padA());
        bool menu = card_row(1, u * 1.9f, T("Online menu", "Menu online", "Menu en linea"), "") || (sRow == 1 && padA());
        if (again) {
            DrRanked_FlowAct(2);
            sRematchAccept = now;
        }
        if (menu) {
            DrRanked_FlowAct(3);
            sRematchLeave = now;
        }
    }
    footer(T("A: choose", "A: escolher", "A: elegir"), "", T("B: back", "B: voltar", "B: volver"));
}

static void screen_chat(void) {
    DrChat c;
    DrChat_Poll(&c);
    if (header("Dragon Net Battle", true) || (!sEditChat && padB())) { DrChat_Close(); goto_screen(HOME, 4); sHomeRow = 4; return; }
    const float u = U();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    card_start(u * 3.3f, u * 11.0f, NULL);
    dl->AddRectFilled(sCardMin, ImVec2(sCardMax.x, sCardMin.y + u * 1.0f), col(0.22f, 0.12f, 0.42f, 0.95f), u * 0.25f);
    font_px(u * 0.7f);
    dl->AddText(ImVec2(sCardMin.x + u * 0.5f, sCardMin.y + u * 0.15f), col(1, 1, 1), T("GENERAL CHAT", "CHAT GERAL", "CHAT GENERAL"));
    ImGui::PopFont();
    font_px(u * 0.66f);
    const float top = sCardMin.y + u * 1.25f, bottom = sCardMax.y - u * 2.0f, line = u * 0.8f;
    if (c.state == 1 && c.count == 0) {
        dl->AddText(ImVec2(sCardMin.x + u * 0.5f, top), ImGui::GetColorU32(kText), T("Connecting to the chat...", "Conectando ao chat...", "Conectando al chat..."));
    } else if (c.state < 0 && c.count == 0) {
        dl->AddText(ImVec2(sCardMin.x + u * 0.5f, top), ImGui::GetColorU32(kText),
                    c.state == -2 ? T("The Dragon Net service does not take this version yet.", "O serviço Dragon Net ainda não aceita esta versão.",
                                      "El servicio Dragon Net aún no acepta esta versión.")
                                  : T("The chat did not answer. Trying again...", "O chat não respondeu. Tentando de novo...", "El chat no respondió. Reintentando..."));
    } else if (c.count == 0) {
        dl->AddText(ImVec2(sCardMin.x + u * 0.5f, top), ImGui::GetColorU32(kText), T("No messages yet.", "Ainda não há mensagens.", "Todavía no hay mensajes."));
    } else {
        // the newest at the bottom, as many as fit (long ones wrapped)
        float y = bottom;
        const float wrap = sCardMax.x - sCardMin.x - u * 1.0f;
        for (int i = c.count - 1; i >= 0 && y > top; i--) {
            char t[200];
            SDL_snprintf(t, sizeof(t), "%s: %s", c.mine[i] ? T("You", "Você", "Tú") : c.name[i], c.text[i]);
            ImVec2 sz = ImGui::CalcTextSize(t, NULL, false, wrap);
            y -= sz.y + u * 0.12f;
            if (y < top) { break; }
            dl->AddText(NULL, 0.0f, ImVec2(sCardMin.x + u * 0.5f, y), c.mine[i] ? ImGui::GetColorU32(kGoldT) : col(1, 1, 1), t, NULL, wrap);
        }
    }
    (void)line;
    ImGui::PopFont();
    // the line to send, and its button
    {
        const float y = sCardMax.y - u * 1.6f, x0 = sCardMin.x + u * 0.4f, bw = u * 4.2f;
        ImGui::SetCursorScreenPos(ImVec2(x0, y));
        ImGui::PushItemWidth(sCardMax.x - x0 - bw - u * 0.9f);
        font_px(u * 0.7f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.07f, 0.24f, 1.0f));
        if (sEditChat && !ImGui::IsAnyItemActive()) { ImGui::SetKeyboardFocusHere(); }
        bool enter = ImGui::InputTextWithHint("##chat", T("Write a message", "Escreva uma mensagem", "Escribe un mensaje"), sChatLine, sizeof(sChatLine),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemActive()) { sEditChat = true; } else if (ImGui::IsItemDeactivated()) { sEditChat = false; }
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + bw, a.y + ImGui::GetFrameHeight());
        bool send = ImGui::InvisibleButton("##send", ImVec2(bw, ImGui::GetFrameHeight()));
        dl->AddRectFilled(a, b, col(0.80f, 0.58f, 0.10f), u * 0.2f);
        dl->AddRect(a, b, col(1.0f, 0.86f, 0.35f), u * 0.2f, 0, 2.0f);
        const char *st = T("Send", "Enviar", "Enviar");
        ImVec2 ts = ImGui::CalcTextSize(st);
        dl->AddText(ImVec2(a.x + (bw - ts.x) * 0.5f, a.y + (b.y - a.y - ts.y) * 0.5f), col(1, 1, 1), st);
        ImGui::PopFont();
        if ((enter || send) && sChatLine[0] != '\0') {
            DrChat_Send(sChatLine);
            sChatLine[0] = '\0';
        }
        if (!sEditChat && padA()) { sEditChat = true; }
    }
    footer(T("General: seen by everyone on the chat. Be kind.", "Geral: todos no chat veem. Seja gentil.", "General: visible a todos los usuarios del chat."),
           c.sending ? T("Sending...", "Enviando...", "Enviando...") : "", T("A: write   B: back", "A: escrever   B: voltar", "A: escribir   B: volver"));
}

// The window: the background, then the screen; its own text size (a thirtieth of the screen's height)
static void window(void) {
    ImGuiIO &io = ImGui::GetIO();
    bool close = false;
    if (!sNameRead) {
        sNameRead = true;
        Port_SettingText("net_name", "Player", sName, sizeof(sName));
        sBattle = Port_Setting("net_battle", 0);
        sQueue2 = Port_Setting("net_queue", 0);
        if (sBattle < 0 || sBattle > 4) { sBattle = 0; }
        if (sQueue2 < 0 || sQueue2 > 4) { sQueue2 = 0; }
        sEnterAt = SDL_GetTicks();
    }
    DrNet_PresenceActive(DrNet_Mode() == 0); // the players online: counted while this is open, against Android + Switch
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.01f, 0.07f, 1.0f));
    ImGui::Begin("###DragonNetBattle", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                                                ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    {
        float base = ImGui::GetStyle().FontSizeBase > 0.0f ? ImGui::GetStyle().FontSizeBase : ImGui::GetFontSize();
        sK = base / ImGui::GetFontSize();
    }
    background(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize());
    sEnter = SDL_GetTicks() - sEnterAt > 260 ? 1.0f : (float)(SDL_GetTicks() - sEnterAt) / 260.0f;
    sEnter = 1.0f - (1.0f - sEnter) * (1.0f - sEnter);
    if (sEditName) { // the name: an input under the badge (the phone's keyboard opens)
        const float u = U();
        ImGui::SetCursorScreenPos(ImVec2(io.DisplaySize.x * 0.62f - u * 6.4f, u * 2.3f));
        ImGui::PushItemWidth(u * 6.4f);
        font_px(u * 0.7f);
        if (!ImGui::IsAnyItemActive()) { ImGui::SetKeyboardFocusHere(); }
        if (ImGui::InputText("##nameedit", sName, sizeof(sName), ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivated()) {
            if (sName[0] == '\0') { SDL_strlcpy(sName, "Player", sizeof(sName)); }
            Port_SettingTextSave("net_name", sName);
            Port_SettingsWrite();
            Port_NetNameSet(sName);
            sEditName = false;
        }
        ImGui::PopFont();
        ImGui::PopItemWidth();
    }
    if (pad(ImGuiKey_GamepadStart)) { sEditName = !sEditName; }
    switch (sScreen) {
    case HOME: screen_home(close); break;
    case CREATE: screen_create(); break;
    case CREATE_WAIT: screen_create_wait(); break;
    case JOIN: screen_join(); break;
    case JOIN_WAIT: screen_join_wait(); break;
    case QUICK: screen_quick(); break;
    case QUICK_WAIT: screen_quick_wait(); break;
    case RANKED: screen_ranked(); break;
    case RANKED_WAIT: screen_ranked_wait(); break;
    case BOARD: screen_board(); break;
    case CHAT: screen_chat(); break;
    case REMATCH: screen_rematch(); break;
    case WATCH_WAIT: screen_watch_wait(); break;
    }
    ImGui::End();
    if (close) {
        leave_window();
    }
}

} // namespace dn

static void build_net_android(void) { dn::window(); }
