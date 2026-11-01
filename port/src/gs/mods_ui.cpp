// Script mods (gs/mods.c, docs/port/mods.md), the render thread's side: the shapes and text the scripts' on_hud drew
// during the last blank, drawn over the game's picture into ImGui's background list (ui.cpp, each frame), and the
// settings window's Mods tab: each mod switched on and off, its settings, why it stopped, and loading them again.
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "mods.h"
#include "ui.h"

ImFont *Ui_FontBold(void); // ui.cpp: the first of the game's fonts (NULL: the window's own)

static ModsHud sDraw; // (the copy drawn: large, so not on the stack)

static ImU32 col(unsigned c) {
    return IM_COL32((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

void ModsUi_Draw(void) {
    if (gUiPresentW <= 0 || gUiPresentH <= 0 || !Mods_HudRead(&sDraw)) {
        return;
    }
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float k = (float)gUiPresentH / MODS_HUD_H; // HUD units -> window pixels
    // (a picture that changed shape since the scripts drew: their layout keeps its own width, centred)
    float ox = (float)gUiPresentX + ((float)gUiPresentW - sDraw.w * k) * 0.5f, oy = (float)gUiPresentY;
    ImFont *font = Ui_FontBold() != NULL ? Ui_FontBold() : ImGui::GetFont();
    dl->PushClipRect(ImVec2((float)gUiPresentX, oy), ImVec2((float)(gUiPresentX + gUiPresentW), oy + (float)gUiPresentH), true);
    for (int i = 0; i < sDraw.count; i++) {
        const ModsCmd &c = sDraw.cmd[i];
        ImVec2 a(ox + c.a * k, oy + c.b * k);
        ImU32 cc = col(c.color);
        float t = c.e > 0.0f ? c.e * k : 1.0f;
        switch (c.kind) {
        case MODS_CMD_RECT:
            dl->AddRect(a, ImVec2(a.x + c.c * k, a.y + c.d * k), cc, 0.0f, 0, t);
            break;
        case MODS_CMD_FILL:
            dl->AddRectFilled(a, ImVec2(a.x + c.c * k, a.y + c.d * k), cc, c.e > 0.0f ? c.e * k : 0.0f);
            break;
        case MODS_CMD_LINE:
            dl->AddLine(a, ImVec2(ox + c.c * k, oy + c.d * k), cc, t);
            break;
        case MODS_CMD_CIRCLE:
            dl->AddCircle(a, c.c * k, cc, 0, t);
            break;
        case MODS_CMD_DISC:
            dl->AddCircleFilled(a, c.c * k, cc, 0);
            break;
        case MODS_CMD_TEXT: {
            if (c.at + c.text > sDraw.chars) {
                break;
            }
            const char *s = sDraw.text + c.at, *e = s + c.text;
            float size = c.c * k;
            if (c.align != 0) {
                ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, s, e);
                a.x -= c.align == 1 ? ts.x * 0.5f : ts.x;
            }
            dl->AddText(font, size, a, cc, s, e);
            break;
        }
        }
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
    } else if (s.step >= 1.0 && floor(s.step) == s.step && floor(s.min) == s.min) {
        int iv = (int)lround(v);
        if (ImGui::SliderInt(s.label, &iv, (int)s.min, (int)s.max)) {
            int step = (int)s.step;
            iv = (int)s.min + (iv - (int)s.min + step / 2) / step * step;
            v = iv;
            changed = true;
        }
    } else {
        float fv = (float)v;
        if (ImGui::SliderFloat(s.label, &fv, (float)s.min, (float)s.max, "%.2f")) {
            v = fv;
            if (s.step > 0.0) {
                v = s.min + floor((v - s.min) / s.step + 0.5) * s.step;
            }
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
        bool open = ImGui::CollapsingHeader(head, ImGuiTreeNodeFlags_AllowOverlap);
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
