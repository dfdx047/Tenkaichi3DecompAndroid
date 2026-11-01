// Script mods (gs/mods.c, docs/port/mods.md), the render thread's side: the shapes and text the scripts' on_hud drew
// during the last blank, drawn over the game's picture into ImGui's background list (ui.cpp, each frame), and the
// settings window's Mods tab: each mod switched on and off, its settings, why it stopped, and loading them again.
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h" // (ShadeVertsLinearColorGradientKeepAlpha)
#include "fight_panels.h"
#include "mods.h"
#include "ui.h"

ImFont *Ui_FontBold(void); // ui.cpp: the first of the game's fonts (NULL: the window's own)
SDL_GPUTexture *Ui_Texture(uint8_t *pix, uint32_t w, uint32_t h); // ui.cpp (takes pix)
void Ui_TextureFree(SDL_GPUTexture *tex);
bool FightPanels_Art(const char *path, int chara, int costume, SDL_GPUTexture **tex, float *hx, float *hy, float *hs, float *fx,
                     float *fy); // fight_panels_ui.cpp
#define ART_W 256.0f
#define ART_H 512.0f

static ModsHud sDraw; // (the copy drawn: large, so not on the stack)

static ImU32 col(unsigned c) {
    return IM_COL32((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

// ------------------------------------------------------------------------------------------------------ pictures
// The scripts' pictures (bt3.image) as textures, made the first time each is drawn; all let go when the scripts are
// loaded again (Mods_ImageGen).
struct ModImage {
    SDL_GPUTexture *tex;
    bool tried;
};
static ModImage sImg[257];
static unsigned sImgGen;

static SDL_GPUTexture *image(int id) {
    if (Mods_ImageGen() != sImgGen) {
        for (ModImage &m : sImg) {
            Ui_TextureFree(m.tex);
            m = ModImage{};
        }
        sImgGen = Mods_ImageGen();
    }
    if (id < 1 || id > 256) {
        return NULL;
    }
    ModImage &m = sImg[id];
    if (!m.tried) {
        char path[400];
        m.tried = true;
        if (Mods_ImagePath(id, path, sizeof(path)) == NULL) {
            return NULL;
        }
        SDL_Surface *s = SDL_LoadPNG(path), *rgba = s != NULL ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32) : NULL;
        if (s != NULL) {
            SDL_DestroySurface(s);
        }
        if (rgba == NULL || rgba->w <= 0 || rgba->h <= 0 || rgba->w > 4096 || rgba->h > 4096) {
            fprintf(stderr, "bt3: mods: picture %s cannot be read%s\n", path, rgba != NULL ? " (larger than 4096)" : "");
            if (rgba != NULL) {
                SDL_DestroySurface(rgba);
            }
            return NULL;
        }
        uint8_t *pix = (uint8_t *)malloc((size_t)rgba->w * rgba->h * 4);
        for (int y = 0; pix != NULL && y < rgba->h; y++) {
            memcpy(pix + (size_t)y * rgba->w * 4, (const uint8_t *)rgba->pixels + (size_t)y * rgba->pitch, (size_t)rgba->w * 4);
        }
        m.tex = pix != NULL ? Ui_Texture(pix, (uint32_t)rgba->w, (uint32_t)rgba->h) : NULL; // (takes pix)
        SDL_DestroySurface(rgba);
    }
    return m.tex;
}

static void picture(ImDrawList *dl, SDL_GPUTexture *tex, ImVec2 a, ImVec2 b, float u0, float v0, float u1, float v1, bool flip,
                    ImU32 tint) {
    if (flip) {
        float t = u0;
        u0 = u1;
        u1 = t;
    }
    dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)tex), a, b, ImVec2(u0, v0), ImVec2(u1, v1), tint);
}

// --------------------------------------------------------------------------------------------------------- drawing
// The text drawn last (window pixels), for a text placed after or before it (MODS_F_AFTER / MODS_F_BEFORE).
static float sTextL, sTextR;

static void text(ImDrawList *dl, ImFont *font, const ModsCmd &c, float k, float ox, ImVec2 a) {
    if (c.at + c.n > sDraw.chars) {
        return;
    }
    const char *s = sDraw.text + c.at, *e = s + c.n;
    float size = c.f[2] * k;
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, s, e);
    if (c.f[6] > 0.0f && ts.x > c.f[6] * k && ts.x > 0.0f) { // fit: smaller, to the width given
        size *= c.f[6] * k / ts.x;
        ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, s, e);
    }
    if (c.flags & MODS_F_AFTER) {
        a.x = sTextR + c.f[0] * k;
    } else if (c.flags & MODS_F_BEFORE) {
        a.x = sTextL - c.f[0] * k - ts.x;
    } else {
        a.x -= c.align == 1 ? ts.x * 0.5f : c.align == 2 ? ts.x : 0.0f;
    }
    (void)ox;
    if (c.flags & MODS_F_BASELINE) {
        a.y -= ts.y * 0.78f;
    }
    sTextL = a.x;
    sTextR = a.x + ts.x;
    int v0 = dl->VtxBuffer.Size;
    if ((c.color3 & 0xFF) != 0) {
        dl->AddText(font, size, ImVec2(a.x + c.f[4] * k, a.y + c.f[5] * k), col(c.color3), s, e);
    }
    if ((c.color2 & 0xFF) != 0) { // the outline: the text drawn all round itself
        float o = (c.f[3] > 0.0f ? c.f[3] : 1.0f) * k;
        for (int d = 0; d < 16; d++) {
            float ang = (float)d * 0.3926991f;
            dl->AddText(font, size, ImVec2(a.x + cosf(ang) * o, a.y + sinf(ang) * o), col(c.color2), s, e);
        }
    }
    int v1 = dl->VtxBuffer.Size;
    if (c.flags & MODS_F_GRAD) { // the fill shaded top to bottom, its alpha the top colour's
        dl->AddText(font, size, a, IM_COL32(255, 255, 255, c.color & 0xFF), s, e);
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v1, dl->VtxBuffer.Size, ImVec2(a.x, a.y + ts.y * 0.25f), ImVec2(a.x, a.y + ts.y * 0.8f),
                                                      col(c.color), col(c.color4));
    } else {
        dl->AddText(font, size, a, col(c.color), s, e);
    }
    if (c.f[7] != 0.0f) { // leaning: x moves with the height over the baseline
        const float base = a.y + ts.y * 0.78f;
        for (int i = v0; i < dl->VtxBuffer.Size; i++) {
            dl->VtxBuffer[i].pos.x -= c.f[7] * (dl->VtxBuffer[i].pos.y - base);
        }
    }
}

// A strip between two lines of points with a colour at each, filled as quads, with a one-pixel fade outside every
// edge so that slanted and curved edges are smooth.
static void band(ImDrawList *dl, const ImVec2 *top, const ImVec2 *bot, const ImU32 *ct, const ImU32 *cb, int n) {
    if (n < 2) {
        return;
    }
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve((n - 1) * 6 * 3 + 12, n * 4 + 8);
    unsigned base = dl->_VtxCurrentIdx;
    for (int i = 0; i < n; i++) {
        dl->PrimWriteVtx(top[i], uv, ct[i]);
        dl->PrimWriteVtx(bot[i], uv, cb[i]);
    }
    for (int i = 0; i < n; i++) { // the fade: one pixel out, along the line from the bottom to the top
        ImVec2 d(top[i].x - bot[i].x, top[i].y - bot[i].y);
        float l = sqrtf(d.x * d.x + d.y * d.y);
        d = l > 0.0f ? ImVec2(d.x / l, d.y / l) : ImVec2(0.0f, -1.0f);
        dl->PrimWriteVtx(ImVec2(top[i].x + d.x, top[i].y + d.y), uv, ct[i] & ~IM_COL32_A_MASK);
        dl->PrimWriteVtx(ImVec2(bot[i].x - d.x, bot[i].y - d.y), uv, cb[i] & ~IM_COL32_A_MASK);
    }
    for (int e = 0; e < 2; e++) { // the two ends, one pixel out along the strip
        int i = e == 0 ? 0 : n - 1, j = e == 0 ? 1 : n - 2;
        ImVec2 d(top[i].x - top[j].x, top[i].y - top[j].y);
        float l = sqrtf(d.x * d.x + d.y * d.y);
        d = l > 0.0f ? ImVec2(d.x / l, d.y / l) : ImVec2(1.0f, 0.0f);
        dl->PrimWriteVtx(top[i], uv, ct[i]);
        dl->PrimWriteVtx(bot[i], uv, cb[i]);
        dl->PrimWriteVtx(ImVec2(top[i].x + d.x, top[i].y + d.y), uv, ct[i] & ~IM_COL32_A_MASK);
        dl->PrimWriteVtx(ImVec2(bot[i].x + d.x, bot[i].y + d.y), uv, cb[i] & ~IM_COL32_A_MASK);
    }
    auto quad = [&](int a, int b, int c, int d) {
        dl->PrimWriteIdx((ImDrawIdx)(base + a)); dl->PrimWriteIdx((ImDrawIdx)(base + b)); dl->PrimWriteIdx((ImDrawIdx)(base + c));
        dl->PrimWriteIdx((ImDrawIdx)(base + a)); dl->PrimWriteIdx((ImDrawIdx)(base + c)); dl->PrimWriteIdx((ImDrawIdx)(base + d));
    };
    const int fo = n * 2, eo = n * 4;
    for (int i = 0; i + 1 < n; i++) {
        quad(i * 2, (i + 1) * 2, (i + 1) * 2 + 1, i * 2 + 1);                    // the strip
        quad(fo + i * 2, fo + (i + 1) * 2, (i + 1) * 2, i * 2);                   // the fade over the top
        quad(i * 2 + 1, (i + 1) * 2 + 1, fo + (i + 1) * 2 + 1, fo + i * 2 + 1);   // and under the bottom
    }
    quad(eo + 0, eo + 2, eo + 3, eo + 1); // the ends
    quad(eo + 4, eo + 6, eo + 7, eo + 5);
}

// A soft round shade: `inner` at the middle, `mid` at 45 % of the radius, nothing at the radius (pixels).
static void glow(ImDrawList *dl, ImVec2 c, float r, ImU32 inner, ImU32 mid) {
    const int seg = 48;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(seg * 9, 1 + seg * 2);
    unsigned base = dl->_VtxCurrentIdx;
    dl->PrimWriteVtx(c, uv, inner);
    for (int i = 0; i < seg; i++) {
        float a = 6.2831853f * i / seg;
        dl->PrimWriteVtx(ImVec2(c.x + cosf(a) * r * 0.45f, c.y + sinf(a) * r * 0.45f), uv, mid);
    }
    for (int i = 0; i < seg; i++) {
        float a = 6.2831853f * i / seg;
        dl->PrimWriteVtx(ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r), uv, mid & ~IM_COL32_A_MASK);
    }
    for (int i = 0; i < seg; i++) {
        int j = (i + 1) % seg;
        dl->PrimWriteIdx((ImDrawIdx)base); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + i)); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + j));
        dl->PrimWriteIdx((ImDrawIdx)(base + 1 + i)); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + seg + i)); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + seg + j));
        dl->PrimWriteIdx((ImDrawIdx)(base + 1 + i)); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + seg + j)); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + j));
    }
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

// A portrait placed by its face (MODS_F_FACE): a grid over the window whose points fade with their distance from the
// fade's middle, so that it is cut softly round (the PC's modern HUD did it this way).
static void portrait_face(ImDrawList *dl, SDL_GPUTexture *tex, const ModsCmd &c, float k, float ox, float oy, float fx, float fy,
                          float band, ImU32 tint) {
    const float pw = c.f[2], ph = c.f[2] * 2.0f; // HUD units; the portrait is 1 x 2
    bool flip = (c.flags & MODS_F_FLIP) != 0;
    float fu = fx / ART_W, fv = fy / ART_H;
    const float left = c.f[0] - (flip ? 1.0f - fu : fu) * pw, top = c.f[1] - fv * ph;
    const float wx0 = c.f[4], wy0 = c.f[5], wx1 = c.f[6], wy1 = c.f[7];
    const float mx = c.f[8], my = c.f[9], r0 = c.f[10], r1 = c.f[11];
    const int gx = 24, gy = 24;
    int alpha = (int)(tint >> IM_COL32_A_SHIFT) & 0xFF;
    dl->PushTexture(ImTextureRef((ImTextureID)(intptr_t)tex));
    dl->PrimReserve(gx * gy * 6, (gx + 1) * (gy + 1));
    unsigned base = dl->_VtxCurrentIdx;
    for (int j = 0; j <= gy; j++) {
        for (int i = 0; i <= gx; i++) {
            float x = wx0 + (wx1 - wx0) * i / gx, y = wy0 + (wy1 - wy0) * j / gy;
            float a = 1.0f;
            if (r1 > r0) {
                float d = sqrtf((x - mx) * (x - mx) + (y - my) * (y - my)), t = clamp01((d - r0) / (r1 - r0));
                a = t < 0.55f ? 1.0f - 0.15f * t / 0.55f : 0.85f * (1.0f - (t - 0.55f) / 0.45f);
            }
            float uu = (x - left) / pw, vv = (y - top) / ph;
            if (uu < 0.0f || uu > 1.0f || vv < 0.0f || vv > 1.0f) {
                a = 0.0f;
            }
            uu = clamp01(uu);
            vv = clamp01(vv);
            if (flip) {
                uu = 1.0f - uu;
            }
            dl->PrimWriteVtx(ImVec2(ox + x * k, oy + y * k), ImVec2(uu, (band + vv) / 3.0f), (tint & ~IM_COL32_A_MASK) | ((ImU32)(alpha * a) << IM_COL32_A_SHIFT));
        }
    }
    for (int j = 0; j < gy; j++) {
        for (int i = 0; i < gx; i++) {
            unsigned a = base + j * (gx + 1) + i, b = a + 1, cc = a + gx + 1, d = cc + 1;
            dl->PrimWriteIdx((ImDrawIdx)a); dl->PrimWriteIdx((ImDrawIdx)b); dl->PrimWriteIdx((ImDrawIdx)d);
            dl->PrimWriteIdx((ImDrawIdx)a); dl->PrimWriteIdx((ImDrawIdx)d); dl->PrimWriteIdx((ImDrawIdx)cc);
        }
    }
    dl->PopTexture();
}

void ModsUi_Draw(void) {
    if (gUiPresentW <= 0 || gUiPresentH <= 0 || !Mods_HudRead(&sDraw)) {
        return;
    }
    static FpState fp;
    bool haveFp = PortPanels_Read(&fp) && fp.battle;
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float k = (float)gUiPresentH / MODS_HUD_H; // HUD units -> window pixels
    // (a picture that changed shape since the scripts drew: their layout keeps its own width, centred)
    float ox = (float)gUiPresentX + ((float)gUiPresentW - sDraw.w * k) * 0.5f, oy = (float)gUiPresentY;
    ImFont *game = Ui_FontBold() != NULL ? Ui_FontBold() : ImGui::GetFont(), *ui = ImGui::GetFont();
    ImVec2 c0((float)gUiPresentX, oy), c1((float)(gUiPresentX + gUiPresentW), oy + (float)gUiPresentH);
    int clips = 0;
    auto P = [&](float x, float y) { return ImVec2(ox + x * k, oy + y * k); };
    static ImVec2 pts[MODS_HUD_POINTS];

    dl->PushClipRect(c0, c1, true);
    for (int i = 0; i < sDraw.count; i++) {
        const ModsCmd &c = sDraw.cmd[i];
        ImVec2 a = P(c.f[0], c.f[1]);
        ImVec2 b(a.x + c.f[2] * k, a.y + c.f[3] * k);
        ImU32 cc = col(c.color);
        switch (c.kind) {
        case MODS_CMD_RECT:
            dl->AddRect(a, b, cc, c.f[5] * k, 0, c.f[4] > 0.0f ? c.f[4] * k : 1.0f);
            break;
        case MODS_CMD_FILL:
            dl->AddRectFilled(a, b, cc, c.f[4] > 0.0f ? c.f[4] * k : 0.0f);
            break;
        case MODS_CMD_LINE:
            dl->AddLine(a, P(c.f[2], c.f[3]), cc, c.f[4] > 0.0f ? c.f[4] * k : 1.0f);
            break;
        case MODS_CMD_CIRCLE:
            dl->AddCircle(a, c.f[2] * k, cc, 0, c.f[3] > 0.0f ? c.f[3] * k : 1.0f);
            break;
        case MODS_CMD_DISC:
            dl->AddCircleFilled(a, c.f[2] * k, cc, 0);
            break;
        case MODS_CMD_GRADIENT:
            dl->AddRectFilledMultiColor(a, b, cc, col(c.color2), col(c.color3), col(c.color4));
            break;
        case MODS_CMD_TEXT:
            text(dl, c.font == 1 ? ui : game, c, k, ox, a);
            break;
        case MODS_CMD_POLY:
        case MODS_CMD_PATH: {
            if (c.at + c.n > sDraw.points || c.n < 2) {
                break;
            }
            for (unsigned j = 0; j < c.n; j++) {
                pts[j] = P(sDraw.pts[(c.at + j) * 2], sDraw.pts[(c.at + j) * 2 + 1]);
            }
            if (c.kind == MODS_CMD_POLY) {
                dl->AddConvexPolyFilled(pts, (int)c.n, cc);
            } else {
                dl->AddPolyline(pts, (int)c.n, cc, (c.flags & MODS_F_CLOSED) ? ImDrawFlags_Closed : 0, c.f[0] > 0.0f ? c.f[0] * k : 1.0f);
            }
            break;
        }
        case MODS_CMD_IMAGE: {
            SDL_GPUTexture *tex = image(c.i0);
            if (tex != NULL) {
                picture(dl, tex, a, b, c.f[4], c.f[5], c.f[6], c.f[7], (c.flags & MODS_F_FLIP) != 0, cc);
            }
            break;
        }
        case MODS_CMD_PORTRAIT: {
            if (!haveFp || c.i0 < 0 || c.i0 > 1) {
                break;
            }
            const FpSide &s = fp.side[c.i0];
            int m = c.i1 >= 0 ? c.i1 : s.active;
            SDL_GPUTexture *tex;
            float hx, hy, hs, fx, fy;
            if (m < 0 || m >= FP_MEMBERS || !s.m[m].present ||
                !FightPanels_Art(s.m[m].path, s.m[m].chara, s.m[m].costume, &tex, &hx, &hy, &hs, &fx, &fy)) {
                break;
            }
            if (c.flags & MODS_F_FACE) {
                portrait_face(dl, tex, c, k, ox, oy, fx, fy, (float)(c.i2 < 0 ? 0 : c.i2 > 2 ? 2 : c.i2), cc);
                break;
            }
            float u0 = c.f[4], v0 = c.f[5], u1 = c.f[6], v1 = c.f[7];
            if (c.flags & MODS_F_HEAD) { // a part about the head, the rectangle's shape, zoom f4 (1: the head's size)
                float side = hs * (c.f[4] > 0.0f ? c.f[4] : 1.0f), aspect = c.f[3] > 0.0f && c.f[2] > 0.0f ? c.f[3] / c.f[2] : 1.0f;
                float cx = hx + hs * 0.5f, cy = hy + hs * 0.5f;
                u0 = (cx - side * 0.5f) / ART_W;
                u1 = (cx + side * 0.5f) / ART_W;
                v0 = (cy - side * aspect * 0.5f) / ART_H;
                v1 = (cy + side * aspect * 0.5f) / ART_H;
            }
            v0 = v0 < 0.0f ? 0.0f : v0 > 1.0f ? 1.0f : v0; // (inside the band: the next one is another picture)
            v1 = v1 < 0.0f ? 0.0f : v1 > 1.0f ? 1.0f : v1;
            float band = (float)(c.i2 < 0 ? 0 : c.i2 > 2 ? 2 : c.i2);
            picture(dl, tex, a, b, u0, (band + v0) / 3.0f, u1, (band + v1) / 3.0f, (c.flags & MODS_F_FLIP) != 0, cc);
            break;
        }
        case MODS_CMD_BAND: {
            if (c.at + c.n * 2 > sDraw.points || c.i0 < 0 || (unsigned)c.i0 + c.n * 2 > sDraw.colours || c.n < 2 || c.n > 4096) {
                break;
            }
            static ImVec2 top[4096], bot[4096];
            static ImU32 ct[4096], cb[4096];
            for (unsigned j = 0; j < c.n; j++) {
                top[j] = P(sDraw.pts[(c.at + j) * 2], sDraw.pts[(c.at + j) * 2 + 1]);
                bot[j] = P(sDraw.pts[(c.at + c.n + j) * 2], sDraw.pts[(c.at + c.n + j) * 2 + 1]);
                ct[j] = col(sDraw.cols[c.i0 + j]);
                cb[j] = col(sDraw.cols[c.i0 + c.n + j]);
            }
            band(dl, top, bot, ct, cb, (int)c.n);
            break;
        }
        case MODS_CMD_GLOW:
            glow(dl, a, c.f[2] * k, cc, col(c.color2));
            break;
        case MODS_CMD_CLIP: {
            ImVec2 q0(a.x > c0.x ? a.x : c0.x, a.y > c0.y ? a.y : c0.y), q1(b.x < c1.x ? b.x : c1.x, b.y < c1.y ? b.y : c1.y);
            if (clips < 32) {
                dl->PushClipRect(q0, q1, true);
                clips++;
            }
            break;
        }
        case MODS_CMD_UNCLIP:
            if (clips > 0) {
                dl->PopClipRect();
                clips--;
            }
            break;
        }
    }
    while (clips-- > 0) {
        dl->PopClipRect();
    }
    dl->PopClipRect();
}

static void setting_ui(ModsSetting &s) {
    double v = s.value;
    bool changed = false;

    if (s.kind == MODS_SET_TOGGLE) {
        bool on = v != 0.0;
        if (ImGui::Checkbox(s.label, &on)) {
            v = on ? 1.0 : 0.0;
            changed = true;
        }
    } else if (s.kind == MODS_SET_CHOICE) {
        int at = (int)v;
        const char *names[8];
        for (int k = 0; k < s.choices; k++) {
            names[k] = s.choice[k];
        }
        if (ImGui::Combo(s.label, &at, names, s.choices)) {
            v = at;
            changed = true;
        }
    } else if (s.step > 0.0 && s.max > s.min) {
        // A slider with a step moves by whole steps: its position is the step's number, its text the value. (Moving
        // a value by one and rounding it back to a step, as before, kept it where it was: the keyboard and the
        // controller move a slider by one.)
        int steps = (int)floor((s.max - s.min) / s.step + 0.5), at = (int)floor((v - s.min) / s.step + 0.5);
        bool whole = floor(s.step) == s.step && floor(s.min) == s.min;
        char shown[32];
        snprintf(shown, sizeof(shown), whole ? "%.0f" : "%.2f", s.min + at * s.step);
        if (ImGui::SliderInt(s.label, &at, 0, steps, shown)) {
            at = at < 0 ? 0 : at > steps ? steps : at;
            v = s.min + at * s.step;
            changed = true;
        }
    } else {
        float fv = (float)v;
        if (ImGui::SliderFloat(s.label, &fv, (float)s.min, (float)s.max, "%.2f")) {
            v = fv;
            changed = true;
        }
    }
    if (s.tip[0] != '\0') {
        ImGui::SetItemTooltip("%s", s.tip);
    }
    if (changed) {
        s.value = v < s.min ? s.min : v > s.max ? s.max : v;
        Mods_SettingChanged();
    }
}

void ModsUi_Tab(void) {
    ImGui::TextWrapped("Script mods: Lua scripts in the folder \"%s\" next to the game, one folder per mod with a mod.lua "
                       "in it. They can draw over the picture and change how the fight camera is shown; they cannot "
                       "change a fight, so they can be used online. See docs/port/mods.md.", Mods_Folder());
    if (ImGui::Button("Reload the scripts")) {
        Mods_ReloadAsk();
    }
    ImGui::SetItemTooltip("Loads every mod again from its folder (after adding one, or after changing a script).");
    ImGui::Separator();
    Mods_Lock();
    int n = Mods_Count();
    if (n == 0) {
        ImGui::TextDisabled("No mods found.");
    }
    for (int i = 0; i < n; i++) {
        ModsInfo *m = Mods_Info(i);
        ImGui::PushID(i);
        char head[200];
        snprintf(head, sizeof(head), "%s%s%s###mod", m->name, m->version[0] != '\0' ? "  " : "", m->version);
        bool open = ImGui::CollapsingHeader(head, ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 3.0f);
        bool on = m->enabled != 0;
        if (ImGui::Checkbox("On", &on)) {
            m->enabled = on;
            Mods_SettingChanged();
        }
        if (m->error[0] != '\0') {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "Stopped: %s", m->error);
        }
        if (open) {
            ImGui::Indent();
            if (m->author[0] != '\0') {
                ImGui::TextDisabled("by %s", m->author);
            }
            if (m->about[0] != '\0') {
                ImGui::TextWrapped("%s", m->about);
            }
            ImGui::TextDisabled("Folder: %s/%s   Uses:%s%s%s", Mods_Folder(), m->folder, m->hooks & MODS_HOOK_FRAME ? " frame" : "",
                                m->hooks & MODS_HOOK_HUD ? " hud" : "", m->hooks & MODS_HOOK_CAMERA ? " camera" : "");
            ImGui::BeginDisabled(!on);
            for (int k = 0; k < m->settings; k++) {
                ImGui::PushID(k);
                setting_ui(m->set[k]);
                ImGui::PopID();
            }
            if (m->settings > 0 && ImGui::Button("Defaults")) {
                for (int k = 0; k < m->settings; k++) {
                    m->set[k].value = m->set[k].def;
                }
                Mods_SettingChanged();
            }
            ImGui::EndDisabled();
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    Mods_Unlock();
}
