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
extern "C" {
#include "gs_internal.h"
}
#undef getenv

namespace dn {

enum Screen { HOME, FRIENDLY_MENU, TAG_MENU, RANKED_MENU, PROFILE, CREATE, CREATE_WAIT, JOIN, JOIN_WAIT, QUICK, QUICK_WAIT, RANKED_WAIT, BOARD, CHAT, REMATCH, WATCH_WAIT };

static Screen sScreen = HOME;
static int sRow, sHomeRow;
static float sEnter;        // the screen's entrance (0..1): the cards slide in as on the Switch
static Uint64 sEnterAt;
static char sName[33] = "Player";
static char sFlag[3] = "UN";
static bool sNameRead, sEditCode, sEditChat;
static char sCode[8], sChatLine[161];
static int sBattle, sQueue2, sRankMode = 0;  // ranked menu: solo (no mode selector)
static int sCountryIndex, sProfileSynced;
static bool sTagMenu;
static bool sRankedNameError;
static const char *const kCountries[][2] = {{"UN","Sin especificar"},{"AR","Argentina"},{"BO","Bolivia"},{"BR","Brasil"},{"CA","Canadá"},{"CL","Chile"},{"CO","Colombia"},{"CR","Costa Rica"},{"CU","Cuba"},{"DO","República Dominicana"},{"EC","Ecuador"},{"SV","El Salvador"},{"ES","España"},{"US","Estados Unidos"},{"GT","Guatemala"},{"HN","Honduras"},{"MX","México"},{"NI","Nicaragua"},{"PA","Panamá"},{"PY","Paraguay"},{"PE","Perú"},{"PT","Portugal"},{"PR","Puerto Rico"},{"UY","Uruguay"},{"VE","Venezuela"},{"JP","Japón"},{"KR","Corea del Sur"},{"FR","Francia"},{"DE","Alemania"},{"IT","Italia"},{"GB","Reino Unido"}};
static int sLobbyLast;                       // the lobby's last state worth telling (errors)
static Uint64 sSearchSince, sFoundAt;
static bool sRankedLobby;                    // the lobby was started for a ranked match
static char sRankedLobbyMatch[33], sRankedLobbyCode[8];
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
    ImGui::InvisibleButton("##name", ImVec2(bw, bh));
    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), col(0.08f, 0.04f, 0.16f, 0.9f), bh * 0.25f);
    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), col(0.60f, 0.45f, 1.0f), bh * 0.25f, 0, 2.0f);
    char badge[44]; SDL_snprintf(badge,sizeof(badge),"%.2s  %.32s",sFlag,sName);
    ImVec2 ts = ImGui::CalcTextSize(badge);
    dl->AddText(ImVec2(bx + (bw - ts.x) * 0.5f, by + (bh - ts.y) * 0.5f), ImGui::GetColorU32(kGoldT), badge);
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
        const bool active = enabled == NULL || enabled[i];
        if (ImGui::InvisibleButton("##plate", ImVec2(w, h)) && active) { clicked = i; }
        ImGui::PopID();
        const bool on = sHomeRow == i;
        if (on) { // the chosen one: a gold band from the left edge
            dl->AddRectFilledMultiColor(ImVec2(o.x, y), ImVec2(x + w + u * 1.5f, y + h), col(1.0f, 0.88f, 0.35f), col(0.95f, 0.75f, 0.15f, 0.0f),
                                        col(0.95f, 0.75f, 0.15f, 0.0f), col(1.0f, 0.88f, 0.35f));
        }
        ImVec2 q[4] = {ImVec2(x + h * 0.35f, y), ImVec2(x + w, y), ImVec2(x + w - h * 0.35f, y + h), ImVec2(x, y + h)};
        ImU32 top = active ? (on ? col(0.98f, 0.92f, 0.55f) : col(0.40f, 0.95f, 1.0f)) : col(0.35f, 0.40f, 0.48f);
        ImU32 bot = active ? (on ? col(0.92f, 0.70f, 0.12f) : col(0.05f, 0.55f, 0.75f)) : col(0.20f, 0.22f, 0.28f);
        dl->AddQuadFilled(q[0], q[1], ImVec2(q[1].x - h * 0.17f, y + h * 0.5f), ImVec2(x + h * 0.17f, y + h * 0.5f), top);
        dl->AddQuadFilled(ImVec2(x + h * 0.17f, y + h * 0.5f), ImVec2(q[1].x - h * 0.17f, y + h * 0.5f), q[2], q[3], bot);
        dl->AddQuad(q[0], q[1], q[2], q[3], col(1.0f, 1.0f, 1.0f, 0.55f), 2.0f);
        ImVec2 ts = ImGui::CalcTextSize(names[i]);
        outlined(dl, ImVec2(x + (w - ts.x) * 0.5f, y + (h - ts.y) * 0.5f), active ? col(1, 1, 1) : col(0.7f, 0.7f, 0.75f), names[i]);
        if (!active) {
            const float lx = x + w - u * 1.05f, ly = y + h * 0.5f;
            ImU32 lock = col(0.72f, 0.72f, 0.78f);
            dl->AddCircle(ImVec2(lx, ly - u * 0.05f), u * 0.20f, lock, 12, u * 0.07f);
            dl->AddRectFilled(ImVec2(lx - u * 0.30f, ly), ImVec2(lx + u * 0.30f, ly + u * 0.34f), lock, u * 0.05f);
            dl->AddRectFilled(ImVec2(lx - u * 0.08f, ly + u * 0.11f), ImVec2(lx + u * 0.08f, ly + u * 0.23f), col(0.25f, 0.25f, 0.30f));
        }
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
    case DRMSG_VERSION: return T("Incompatible version (409).", "Versão incompatível (409).", "Versión incompatible (409).");
    case DRMSG_UPDATED: return T("Profile updated.", "Perfil atualizado.", "Perfil actualizado.");
    case DRMSG_BUSY: return T("Too many requests: wait a moment.", "Muitos pedidos: espere um pouco.", "Demasiadas solicitudes: espera un momento.");
    case DRMSG_LIMIT: return limit_text();
    case DRMSG_BACKGROUND_TIMEOUT: return T("Ranked search canceled after 3 minutes in the background.", "Busca Ranked cancelada após 3 minutos em segundo plano.", "Búsqueda Ranked cancelada tras 3 minutos en segundo plano.");
    case DRMSG_NO_CONNECTION: return T("No connection to the Ranked service.", "Sem conexão com o serviço Ranked.", "Sin conexión con el servicio Ranked.");
    case DRMSG_NO_RIVAL: return T("No rival found in 3 minutes.", "Nenhum rival encontrado em 3 minutos.", "No se encontró rival en 3 minutos.");
    case DRMSG_CANCELLED: return T("Search canceled.", "Busca cancelada.", "Búsqueda cancelada.");
    case DRMSG_HOST_NO_ROOM: return T("The host did not publish the room.", "O anfitrião não publicou a sala", "El anfitrión no publicó la sala.");
    default: return "";
    }
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
    const char *tagTeamSetting = Port_GetEnv("BT3_TAG_TEAM");
    const bool tagEnabled = tagTeamSetting != NULL && strcmp(tagTeamSetting, "1") == 0;
    const bool enabled[5] = {true, tagEnabled, true, true, true};
    const char *labels[5] = {T("Friendly battle", "Combate amistoso", "Combate amistoso"),
        T("Tag Team battle", "Combate Tag Team", "Combate Tag Team"),
        T("Ranked battle", "Combate classificatorio", "Combate clasificatorio"),
        T("Ranking", "Classificação", "Clasificación"), T("Options", "Opções", "Opciones")};
    if (header("Dragon Net Battle", true)) { close = true; }
    int move = padMove();
    if (move) { do { sHomeRow = (sHomeRow + move + 5) % 5; } while (!enabled[sHomeRow]); }
    int click = home_plates(labels, enabled, 5);
    if (click >= 0) { sHomeRow = click; }
    if ((padA() && enabled[sHomeRow]) || click >= 0) {
        static const Screen dest[5] = {FRIENDLY_MENU, TAG_MENU, RANKED_MENU, BOARD, PROFILE};
        goto_screen(dest[sHomeRow]);
        if (sHomeRow == 1) { sTagMenu = true; }
        if (sHomeRow == 3 && DrNet_Mode() == 0) { Bt3Profile_Board(); }
        if (sHomeRow == 4) { sProfileSynced = 0; Bt3Profile_Open(); }
    }
    if (padB()) { close = true; }
    footer(T("Choose a mode.", "Escolha um modo.", "Elige un modo."), "", controls_hint());
}

static void screen_friendly_menu(bool tagTeam) {
    const char *labels[3]={T("Quick match","Partida rápida","Partida rápida"),T("Create room","Crear sala","Crear sala"),T("Join with code","Unirse con código","Unirse con código")};
    const char *title = tagTeam ? T("Tag Team battle","Combate Tag Team","Combate Tag Team") : T("Friendly battle","Combate amistoso","Combate amistoso");
    if(header(title,true)||padB()){goto_screen(HOME,tagTeam?1:0);sTagMenu=false;return;}
    int move=padMove();if(move)sRow=(sRow+move+3)%3; sHomeRow=sRow;
    int click=home_plates(labels,NULL,3);if(click>=0)sRow=click;
    if(padA()||click>=0){sTagMenu=tagTeam;if(tagTeam)DrNet_SetMode(1);DrNet_PresenceActive(0);Port_LobbyHelloAsk();static const Screen dest[3]={QUICK,CREATE,JOIN};goto_screen(dest[sRow]);}
}

static void screen_ranked_menu(void) {
    const char *labels[1]={T("Search for a rival","Buscar rival","Buscar rival")};
    if(header(T("Ranked battle","Combate classificatorio","Combate clasificatorio"),true)||padB()){goto_screen(HOME,2);return;}
    sHomeRow = 0;
    int click = home_plates(labels,NULL,1);
    if (click >= 0) { sRow = click; }
    if (click >= 0 || padA()) {
        if (sName[0] == '\0') {
            sRankedNameError = true;
        } else {
            sRankedNameError = false;
            sRankedLobby = false;
            sRankedLobbyMatch[0] = sRankedLobbyCode[0] = '\0';
            DrRanked_ResetForNewSearch();
            DrRanked_Search(sName, sRankMode);
            sSearchSince = SDL_GetTicks();
            sFoundAt = 0;
            sLobbyLast = 0;
            goto_screen(RANKED_WAIT);
            return;
        }
    }
    footer(sRankedNameError
               ? T("Enter a username in Options before searching.", "Escreva um nome em Opções antes de buscar.", "Escribe un nombre en Opciones antes de buscar.")
               : T("Ranked search.", "Busca classificatória.", "Búsqueda clasificatoria."),
           "", controls_hint());
}

static void screen_profile(void) {
    Bt3Profile p; Bt3Profile_Poll(&p);
    if (header(T("Options","Opções","Opciones"),true)||padB()){goto_screen(HOME,4);return;}
    const float u=U();
    card_start(u*3.5f,u*6.4f,T("PLAYER PROFILE","PERFIL DO JOGADOR","PERFIL DEL JUGADOR"));
    ImDrawList *dl=ImGui::GetWindowDrawList(); font_px(u*.7f);
    dl->AddText(ImVec2(sCardMin.x+u*.6f,sCardMin.y+u*.55f),ImGui::GetColorU32(kText),T("Username","Nome de usuário","Nombre de usuario"));
    ImGui::SetCursorScreenPos(ImVec2(sCardMin.x+u*5.8f,sCardMin.y+u*.35f)); ImGui::PushItemWidth(u*11.0f);
    ImGui::InputText("##profile_name",sName,sizeof(sName)); ImGui::PopItemWidth();
    dl->AddText(ImVec2(sCardMin.x+u*.6f,sCardMin.y+u*1.9f),ImGui::GetColorU32(kText),T("Flag","Bandeira","Bandera"));
    ImGui::SetCursorScreenPos(ImVec2(sCardMin.x+u*5.8f,sCardMin.y+u*1.65f)); ImGui::PushItemWidth(u*11.0f);
    char preview[64]; SDL_snprintf(preview,sizeof(preview),"%s — %s",kCountries[sCountryIndex][0],kCountries[sCountryIndex][1]);
    if(ImGui::BeginCombo("##profile_country",preview)) {
        for(int i=0;i<(int)(sizeof(kCountries)/sizeof(kCountries[0]));i++) {
            char label[64]; SDL_snprintf(label,sizeof(label),"%s — %s",kCountries[i][0],kCountries[i][1]);
            if(ImGui::Selectable(label,sCountryIndex==i))sCountryIndex=i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth(); ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(sCardMin.x+u*5.8f,sCardMin.y+u*3.1f));
    if(ImGui::Button(T("Save profile","Guardar perfil","Guardar perfil"),ImVec2(u*6.0f,u*1.0f))) {
        if(sName[0]=='\0')SDL_strlcpy(sName,"Player",sizeof(sName));
        SDL_strlcpy(sFlag,kCountries[sCountryIndex][0],sizeof(sFlag));
        Bt3Profile_Save(sName,sFlag);
        Port_SettingTextSave("net_name",sName); Port_SettingTextSave("net_flag",sFlag); Port_SettingsWrite(); Port_NetNameSet(sName);
    }
    if(p.state==2) {
        char stats[112]; SDL_snprintf(stats,sizeof(stats),"Elo %d  ·  %s  ·  %dW / %dL  ·  #%d",p.rating,p.rank,p.wins,p.losses,p.position);
        font_px(u*.58f); dl->AddText(ImVec2(sCardMin.x+u*.6f,sCardMin.y+u*4.45f),ImGui::GetColorU32(kGoldT),stats); ImGui::PopFont();
    }
    if(p.state==2 && !sProfileSynced) {
        if(p.username[0]) SDL_strlcpy(sName,p.username,sizeof(sName));
        if(p.flag[0]) SDL_strlcpy(sFlag,p.flag,sizeof(sFlag));
        for(int i=0;i<(int)(sizeof(kCountries)/sizeof(kCountries[0]));i++)if(SDL_strcmp(sFlag,kCountries[i][0])==0)sCountryIndex=i;
        Port_SettingTextSave("net_name",sName); Port_SettingTextSave("net_flag",sFlag); Port_SettingsWrite(); Port_NetNameSet(sName); sProfileSynced=1;
    }
    char diagnostic[96];
    const char *status;
    if (p.state < 0 && (p.http_status || p.error_field[0])) {
        SDL_snprintf(diagnostic,sizeof(diagnostic),"http=%d error=%s",p.http_status,p.error_field);
        status=diagnostic;
    } else status=p.error==3?T("Incompatible version (409).","Versão incompatível (409).","Versión incompatible (409)."):p.error==2?T("Server error.","Erro do servidor.","Error del servidor."):p.state<0?T("No connection.","Sem conexão.","Sin conexión."):p.state==1?T("Loading profile…","Carregando perfil…","Cargando perfil…"):T("Your name and flag are shown on your public profile.","Seu nome e bandeira aparecem no perfil público.","Tu nombre y bandera aparecen en el perfil público.");
    footer(status,"",controls_hint());
}

static void screen_create(void) {
    if (header(T("Create room", "Criar sala", "Crear sala"), true) || padB()) { goto_screen(sTagMenu ? TAG_MENU : FRIENDLY_MENU); return; }
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
            goto_screen(sTagMenu ? TAG_MENU : FRIENDLY_MENU, 1);
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
    if (ImGui::InputText("##code", sCode, 5, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        sEditCode = false;
        sRow = 1;
    }
    { char digits[5]; int n=0; for(int i=0;sCode[i]&&n<4;i++)if(sCode[i]>='0'&&sCode[i]<='9')digits[n++]=sCode[i]; digits[n]='\0'; SDL_strlcpy(sCode,digits,sizeof(sCode)); }
    if (ImGui::IsItemDeactivated()) { sEditCode = false; }
    ImGui::PopFont();
    ImGui::PopItemWidth();
}

static void screen_join(void) {
    if (header(T("Join room", "Entrar em sala", "Unirse con código"), true) || (!sEditCode && padB())) { goto_screen(sTagMenu ? TAG_MENU : FRIENDLY_MENU, 2); return; }
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
            for (const char *c = clip; c != NULL && *c != '\0' && n < 4; c++) {
                if (*c >= '0' && *c <= '9') { sCode[n++] = *c; }
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
    footer(err[0] ? err : T("Enter the 4-digit room code.", "Escribe el código de sala de 4 dígitos.", "Escribe el código de sala de 4 dígitos."),
           T("Use the 4 digits shared by the host.", "Usa los 4 dígitos que te compartió quien creó la sala.", "Usa los 4 dígitos que te compartió quien creó la sala."),
           controls_hint());
    if (go && strlen(sCode) == 4) {
        sLobbyLast = 0;
        Port_NetNameSet(sName);
        Port_NetOptions(-1, -1); // (the host's choices)
        Port_LobbyStartCode(0, sCode);
        goto_screen(JOIN_WAIT);
    }
    if (watch && strlen(sCode) == 4) {
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
    if (header(T("Quick match", "Partida rápida", "Partida rápida"), true) || padB()) { goto_screen(sTagMenu ? TAG_MENU : FRIENDLY_MENU, 0); return; }
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
        Port_NetNameSet(sName);
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

static void screen_ranked_wait(void) {
    DrRanked r;
    DrRanked_Poll(&r);
    int st = sRankedLobby ? lobby_step() : 0;
    if (!sOpen) { return; }
    bool back = header(T("Searching for a rival", "Buscando rival", "Buscando rival"), true) || padB();
    if (r.match[0] && !sRankedLobbyMatch[0]) SDL_strlcpy(sRankedLobbyMatch, r.match, sizeof(sRankedLobbyMatch));
    if (r.match[0] && sRankedLobbyMatch[0] && SDL_strcmp(r.match, sRankedLobbyMatch) != 0) {
        if (sRankedLobby) Port_LobbyCancel();
        sRankedLobby = false;
        sRankedLobbyCode[0] = '\0';
        sFoundAt = SDL_GetTicks();
        SDL_strlcpy(sRankedLobbyMatch, r.match, sizeof(sRankedLobbyMatch));
    }
    if (back && r.searching) {
        sRankedLobby = false;
        sRankedLobbyCode[0] = sRankedLobbyMatch[0] = '\0';
        sFoundAt=0;
        DrRanked_AbortBeforeStart();
        Port_UiNotice(T("Search canceled.", "Busca cancelada.", "Búsqueda cancelada."));
        goto_screen(RANKED_MENU, 0);
        return;
    }
    if (st < 0 && r.state == 3) {
        sRankedLobby=false;sRankedLobbyCode[0]=sRankedLobbyMatch[0]='\0';sFoundAt=0;
        DrRanked_AbortBeforeStart();
        const char *why=lobby_error(st);
        Port_UiNotice(why[0]?why:T("The rival did not connect.","O adversário não conectou","El rival no se conectó."));
        goto_screen(RANKED_MENU,0);return;
    }
    if (back || (r.state == 0 && !r.searching) || st < 0) {
        if (r.state == 3 || sRankedLobby) DrRanked_AbortBeforeStart();
        sRankedLobby = false;
        sRankedLobbyCode[0] = sRankedLobbyMatch[0] = '\0';
        sFoundAt=0;
        goto_screen(RANKED_MENU, 0);
        return;
    }
    if (r.state == 3 && sFoundAt == 0) sFoundAt=SDL_GetTicks();
    if (r.state == 3 && sFoundAt != 0 && SDL_GetTicks()-sFoundAt >= 30000) {
        sRankedLobby=false;sRankedLobbyCode[0]=sRankedLobbyMatch[0]='\0';sFoundAt=0;
        DrRanked_AbortBeforeStart();
        Port_UiNotice(T("The rival did not connect.","O adversário não conectou","El rival no se conectó."));
        goto_screen(RANKED_MENU,0);return;
    }
    // paired: the host opens the room and tells the service its code; the other joins it once the code is there
    if (r.state == 3 && !sRankedLobby) {
        if (sFoundAt == 0) { sFoundAt = SDL_GetTicks(); }
        rules_set(4); // dp20-bo3-v1
        if (r.role == 1) {
            Port_LobbyStartCode(1, "");
            sRankedLobby = true;
        } else if (r.role == 2 && strlen(r.code) == 6) {
            Port_NetNameSet(sName);
            Port_NetOptions(-1, -1);
            Port_LobbyStartCode(0, r.code);
            sRankedLobby = true;
            SDL_strlcpy(sRankedLobbyCode, r.code, sizeof(sRankedLobbyCode));
        }
    } else if (r.state == 3 && r.role == 2 && sRankedLobby && strlen(r.code) == 6 &&
               SDL_strcmp(r.code, sRankedLobbyCode) != 0) {
        /* The server changed the room code before connect/ready: tear down the old lobby socket and join the current code. */
        Port_LobbyCancel();
        sRankedLobby = false;
        sRankedLobbyCode[0] = '\0';
        Port_NetNameSet(sName);
        Port_NetOptions(-1, -1);
        Port_LobbyStartCode(0, r.code);
        sRankedLobby = true;
        SDL_strlcpy(sRankedLobbyCode, r.code, sizeof(sRankedLobbyCode));
    }
    if (r.state == 3) {
        char diag[256];
        const char *code = r.code;
        const char *match = r.match;
        SDL_snprintf(diag, sizeof(diag), "role=%d lobby=%d lobby_error=%s match=%.4s code=%.6s",
                     r.role, st,
                     sRankedLobby && Port_LobbyError()[0] ? Port_LobbyError() : "", match, code);
        fprintf(stderr, "bt3: ranked diagnostic: %s\n", diag);
        if (r.role == 2 && !sRankedLobby && strlen(r.code) != 6) {
            SDL_snprintf(diag, sizeof(diag), "aguardando code · tentativas=%d", r.code_poll_count);
            fprintf(stderr, "bt3: ranked diagnostic: %s\n", diag);
        }
        SDL_strlcat(diag, "\n", sizeof(diag));
        for (int route = 0; route < 4; ++route) {
            char line[96];
            static const char *names[] = {"queue", "match", "room", "lifecycle"};
            SDL_snprintf(line, sizeof(line), "%s http=%d error=%s\n", names[route], r.route_http[route], r.route_error[route]);
            SDL_strlcat(diag, line, sizeof(diag));
            fprintf(stderr, "bt3: ranked diagnostic: %s", line);
        }
        ImDrawList *dl = ImGui::GetWindowDrawList();
        font_px(U() * 0.34f);
        const float line_h = U() * 0.38f;
        float y = sCardMin.y + U() * 3.55f;
        SDL_snprintf(diag, sizeof(diag), "role=%d lobby=%d lobby_error=%s match=%.4s code=%.6s",
                     r.role, st,
                     sRankedLobby && Port_LobbyError()[0] ? Port_LobbyError() : "", match, code);
        dl->AddText(ImVec2(sCardMin.x + U(), y), ImGui::GetColorU32(kText), diag); y += line_h;
        if (r.role == 2 && !sRankedLobby && strlen(r.code) != 6) {
            SDL_snprintf(diag, sizeof(diag), "aguardando code · tentativas=%d", r.code_poll_count);
            dl->AddText(ImVec2(sCardMin.x + U(), y), ImGui::GetColorU32(kText), diag); y += line_h;
        }
        if (r.queue_match[0]) {
            SDL_snprintf(diag,sizeof(diag),"queue id=%s; mesmo match=%s",r.queue_match,r.queue_same_previous?"sim":"não");
            dl->AddText(ImVec2(sCardMin.x + U(), y),ImGui::GetColorU32(kText),diag); y += line_h;
        }
        SDL_snprintf(diag,sizeof(diag),"code servidor=%.6s; em uso=%.6s; mudou=%d; mesmo match=%s",
                     r.server_code, sRankedLobbyCode, r.code_change_count, r.queue_same_previous?"sim":"não");
        dl->AddText(ImVec2(sCardMin.x + U(), y),ImGui::GetColorU32(kText),diag); y += line_h;
        for (int route = 0; route < 4; ++route) {
            char line[96];
            static const char *names[] = {"queue", "match", "room", "lifecycle"};
            SDL_snprintf(line, sizeof(line), "%s http=%d error=%s", names[route], r.route_http[route], r.route_error[route]);
            dl->AddText(ImVec2(sCardMin.x + U(), y), ImGui::GetColorU32(kText), line); y += line_h;
        }
        ImGui::PopFont();
    }
    if (sRankedLobby && r.role == 1 && Port_LobbyCode()[0] != '\0') { DrRanked_SetRoom(Port_LobbyCode()); }
    char big[64];
    SDL_strlcpy(big, r.state == 3 && r.peer[0] ? r.peer : (r.mode ? T("Team", "Equipe", "Equipos") : T("Single", "Individual", "Individual")), sizeof(big));
    char elapsed[48];
    Uint64 seconds = sSearchSince != 0 ? (SDL_GetTicks() - sSearchSince) / 1000 : 0;
    SDL_snprintf(elapsed, sizeof(elapsed), T("Elapsed: %02u:%02u", "Tempo: %02u:%02u", "Tiempo: %02u:%02u"),
                 (unsigned)(seconds / 60), (unsigned)(seconds % 60));
    const char *note = r.msg == DRMSG_SEARCHING || r.msg == DRMSG_NO_ANSWER
                           ? T("Waiting for a rival...", "Aguardando um rival...", "Esperando rival...")
                           : ranked_msg(r.msg);
    char detail[128];
    if(r.queue_match[0]) SDL_snprintf(detail,sizeof(detail),"%s · id %.4s · mesmo match=%s",elapsed,r.queue_match,r.queue_same_previous?"sim":"não");
    else SDL_snprintf(detail,sizeof(detail),"%s  ·  %s",elapsed,note);
    const char *label = r.state < 0 ? T("SEARCH ENDED", "BUSCA ENCERRADA", "BÚSQUEDA FINALIZADA")
                                    : r.state == 3 ? T("RIVAL FOUND", "RIVAL ENCONTRADO", "RIVAL ENCONTRADO")
                                                   : T("SEARCH UNDER WAY", "BUSCA EM ANDAMENTO", "BÚSQUEDA EN CURSO");
    wait_card(U() * (r.state == 3 ? 7.2f : 4.6f), label,
              big, detail, col(1.0f, 0.86f, 0.30f));
    ImGui::SetCursorScreenPos(ImVec2(sCardMin.x + U(), sCardMax.y - U() * 1.05f));
    if (ImGui::Button(r.searching ? T("Cancel search", "Cancelar busca", "Cancelar búsqueda") : T("Back", "Voltar", "Volver"), ImVec2(U() * 7.0f, U() * 0.8f))) {
        if (r.searching) {
            sRankedLobby = false;
            sRankedLobbyCode[0] = sRankedLobbyMatch[0] = '\0';
            DrRanked_ResetForNewSearch();
            Port_UiNotice(T("Search canceled.", "Busca cancelada.", "Búsqueda cancelada."));
        }
        goto_screen(RANKED_MENU, 0);
    }
    char diagnostic[96];
    const char *status = (r.http_status || r.error_field[0])
                             ? (SDL_snprintf(diagnostic,sizeof(diagnostic),"http=%d error=%s",r.http_status,r.error_field), diagnostic)
                             : r.msg == DRMSG_VERSION ? ranked_msg(r.msg) : T("Ranked search. You can cancel at any time.", "Busca ranqueada. Você pode cancelar quando quiser.", "Búsqueda Ranked. Puedes cancelar cuando quieras.");
    footer(status,
           "", T("B: cancel", "B: cancelar", "B: cancelar"));
}

static void screen_board(void) {
    Bt3Profile p; Bt3Profile_Poll(&p);
    if(header(T("Ranking","Clasificación","Clasificación"),true)||padB()){goto_screen(HOME,3);return;}
    const float u=U();
    if(DrNet_Mode()!=0) { footer(T("Coming soon.","Em breve.","Próximamente."),"",controls_hint()); return; }
    card_start(u*3.8f,u*10.2f,T("WORLD TOP 10","TOP 10 MUNDIAL","TOP 10 MUNDIAL"));
    ImDrawList *dl=ImGui::GetWindowDrawList(); font_px(u*.65f);
    for(int i=0;i<10;i++) {
        float y=sCardMin.y+u*.2f+(float)i*u*.86f;
        dl->AddRectFilled(ImVec2(sCardMin.x+u*.25f,y),ImVec2(sCardMax.x-u*.25f,y+u*.76f),col(.12f,.07f,.24f,i%2?.72f:.95f),u*.1f);
        if(i<p.board_count) {
            char pos[12],elo[48]; SDL_snprintf(pos,sizeof(pos),"%d",p.board_position[i]?p.board_position[i]:i+1); SDL_snprintf(elo,sizeof(elo),"%d  %s",p.board_rating[i],p.board_rank[i]);
            dl->AddText(ImVec2(sCardMin.x+u*.55f,y+u*.1f),ImGui::GetColorU32(kGoldT),pos);
            char flagname[40]; SDL_snprintf(flagname,sizeof(flagname),"%.2s  %.32s",p.board_flag[i],p.board_name[i]);
            dl->AddText(ImVec2(sCardMin.x+u*2.0f,y+u*.1f),ImGui::GetColorU32(kText),flagname);
            dl->AddText(ImVec2(sCardMax.x-u*4.3f,y+u*.1f),ImGui::GetColorU32(kText),elo);
        }
    }
    ImGui::PopFont();
    font_px(u*.66f);
    char positionText[80]; if(p.position) SDL_snprintf(positionText,sizeof(positionText),T("Your position: %d","Sua posição: %d","Tu posición: %d"),p.position); else SDL_strlcpy(positionText,T("Your position: Unranked","Sua posição: Sem classificação","Tu posición: Sin clasificar"),sizeof(positionText));
    dl->AddText(ImVec2(sCardMin.x+u*.6f,sCardMax.y-u*.15f),ImGui::GetColorU32(kGoldT),positionText); ImGui::PopFont();
    ImGui::SetCursorScreenPos(sCardMin);
    if(ImGui::InvisibleButton("##worldrefresh",ImVec2(sCardMax.x-sCardMin.x,sCardMax.y-sCardMin.y))||padA())Bt3Profile_Board();
    footer(p.error==3?T("Incompatible version (409).","Versão incompatível (409).","Versión incompatible (409)."):p.error==2?T("Server error.","Erro do servidor.","Error del servidor."):p.board_state<0?T("No connection.","Sem conexão.","Sin conexión."):
        p.board_state==1?T("Loading ranking…","Carregando classificação…","Cargando clasificación…"):
        p.board_count?T("Worldwide top 10.","Top 10 mundial.","Top 10 mundial."):
        T("No ranked players yet.","Sem classificados","Sin clasificados"),"",T("A: update   B: back","A: atualizar   B: voltar","A: actualizar   B: volver"));
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
        Port_SettingText("net_flag", "UN", sFlag, sizeof(sFlag));
        for(int i=0;i<(int)(sizeof(kCountries)/sizeof(kCountries[0]));i++)if(SDL_strcmp(sFlag,kCountries[i][0])==0)sCountryIndex=i;
        sBattle = Port_Setting("net_battle", 0);
        sQueue2 = Port_Setting("net_queue", 0);
        if (sBattle < 0 || sBattle > 4) { sBattle = 0; }
        if (sQueue2 < 0 || sQueue2 > 4) { sQueue2 = 0; }
        sEnterAt = SDL_GetTicks();
    }
    DrNet_PresenceActive(0);
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
    switch (sScreen) {
    case HOME: screen_home(close); break;
    case FRIENDLY_MENU: screen_friendly_menu(false); break;
    case TAG_MENU: screen_friendly_menu(true); break;
    case RANKED_MENU: screen_ranked_menu(); break;
    case PROFILE: screen_profile(); break;
    case CREATE: screen_create(); break;
    case CREATE_WAIT: screen_create_wait(); break;
    case JOIN: screen_join(); break;
    case JOIN_WAIT: screen_join_wait(); break;
    case QUICK: screen_quick(); break;
    case QUICK_WAIT: screen_quick_wait(); break;
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
