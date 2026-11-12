// The screen reader in this program's own windows (ui.cpp): the settings window (F1, or START + SELECT) and Dragon Net
// Battle. They are Dear ImGui's, drawn over the picture and none of the game's screens, so a11y_game.c cannot read
// them: until now only the question before the game closes was said (build_quit). Here the item under the keyboard's
// or the controller's focus is said as the window shows it: its label, its value (a slider's number, a list's choice)
// and its state (checked, a tab, a combo box), and the window's name and tab when they change.
//
// The setup window (port/setup/setup.cpp) is read the same way: port/setup/build.sh builds this file, gs/a11y.c and
// plat_settings.c into it, and setup.cpp says its pages and the steps of the installation itself.
//
// How it sees the items: Dear ImGui calls a test engine's hooks for every item it adds (IMGUI_ENABLE_TEST_ENGINE,
// given by port/tools/undefined.py to the game's C++ and by port/setup/build.sh to the setup's).
// ImGuiTestEngineHook_ItemAdd says an item is there this frame; ImGuiTestEngineHook_ItemInfo gives its label and, for
// a checkbox or a collapsing header, whether it is checked or open. What an item shows besides its label (a slider's
// value, a list's choice) reaches no hook, so one line of ours in ImDrawList::AddText (imgui_draw.cpp, marked "BT3
// port") hands every text drawn to ImGuiA11yHook_Text, and the text drawn while the focused item is the last item
// (g.LastItemData) is its own. A text that is not an item of its own (ImGui::Text) is kept too: a list or a field
// whose label is hidden ("##name") is named by the text beside it on its line. A tooltip's text is said after the
// item when the tooltip comes up for the focused item (the description a seeing player gets).
//
// Everything runs on the render thread, inside ui.cpp's frame (UiA11y_Frame before ImGui::NewFrame, UiA11y_End after
// the windows are built). The hooks do nothing unless UiA11y_Frame turned them on: the speaker is on and one of the
// windows is open; ImGui then calls nothing (g.TestEngineHookItems is off) and the text hook returns at once.
#include <stdio.h>
#include <string.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "ui.h"

extern "C" void PortA11y_Say(const char *text, int flags); // gs/a11y.c
extern "C" const char *PortA11y_Label(const char *label);
#define A11Y_INTERRUPT 1 // (gs/a11y.c)

// What was drawn of the focused item in one frame.
struct UiA11yItem {
    ImGuiID id;                 // 0: the focused item was not drawn this frame
    ImGuiWindow *window;        // the window it is in
    ImRect rect;                // where (its line: for the text beside it)
    bool info;                  // ItemInfo came: label and flags are its own
    ImGuiItemStatusFlags flags; // ItemInfo's (checkable, checked, openable, opened, inputable)
    bool disabled;
    bool tab, tabSelected;      // an item of a tab bar, and the bar's selected tab
    bool popup;                 // in a popup (the list of a combo): no "button"
    int radio;                  // UiA11y_Radio: 0 none, 1 not selected, 2 selected
    bool field;                 // UiA11y_Field: a text field, with its name and its text below
    char name[160];
    char fieldText[512];
    char label[160];            // ItemInfo's label, without its "##" part
    char text[512];             // the texts it drew, in their order, separated by '\n'
    char before[160];           // the text drawn before it on its line (ImGui::Text)
    char after[160];            // ... and after it, up to the next item
};

static bool sOn;                    // the hooks are on this frame
static bool sWasOn;                 // they were last frame (the window just opened / closed)
static UiA11yItem sNow;             // this frame's focused item
static bool sAfterOpen;             // texts drawn now go to sNow.after (the focused item's line, nothing focusable since)
static char sLine[160];             // the last text that was no item of its own, its window and place
static ImGuiWindow *sLineWindow;
static ImRect sLineRect;
static char sTip[2048];              // the tooltip's text this frame
static char sTab[160];              // the selected tab of the focused window's tab bar this frame

static ImGuiID sSaidId;             // what was said: the item, its words, the window, the tab, the tooltip
static char sSaidValue[640], sSaidState[96], sSaidWindow[160], sSaidTab[160], sSaidTip[2048];

// Appends text (begin..end, or to its terminator) to a buffer; `sep` first when the buffer is not empty.
static void add(char *buf, size_t cap, const char *begin, const char *end, const char *sep) {
    size_t len = strlen(buf), n = end != NULL ? (size_t)(end - begin) : strlen(begin);
    if (n == 0) {
        return;
    }
    if (len > 0 && sep != NULL) {
        size_t s = strlen(sep);
        if (len + s >= cap) {
            return;
        }
        memcpy(buf + len, sep, s);
        len += s;
    }
    if (len + n >= cap) {
        n = cap - 1 - len;
    }
    memcpy(buf + len, begin, n);
    buf[len + n] = '\0';
}

// A label as it is shown: up to "##" (the rest is its id), and without the spaces around it.
static void shown(char *out, size_t cap, const char *label) {
    const char *end = strstr(label, "##");
    size_t n = end != NULL ? (size_t)(end - label) : strlen(label);
    while (n > 0 && label[n - 1] == ' ') {
        n--;
    }
    while (n > 0 && *label == ' ') {
        label++, n--;
    }
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, label, n);
    out[n] = '\0';
}

static bool same_line(const ImRect &a, const ImRect &b) {
    return a.Min.y < b.Max.y && b.Min.y < a.Max.y;
}

// ---- Dear ImGui's hooks (imgui_internal.h, "Test Engine specific hooks") ----

void ImGuiTestEngineHook_ItemAdd(ImGuiContext *ctx, ImGuiID id, const ImRect &bb, const ImGuiLastItemData *item_data) {
    (void)item_data;
    if (!sOn) {
        return;
    }
    sAfterOpen = false; // (a focusable item ends the focused one's line)
    if (id == ctx->NavId && id == sNow.id) { // a tab: its ItemInfo came first (TabItemEx); here is where it is
        sNow.window = ctx->CurrentWindow;
        sNow.rect = bb;
        return;
    }
    if (id != ctx->NavId || sNow.id != 0) {
        sLine[0] = '\0'; // (the text before an item names that item only)
        return;
    }
    sNow.id = id;
    sNow.window = ctx->CurrentWindow;
    sNow.rect = bb;
    sNow.disabled = (ctx->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    sNow.popup = (ctx->CurrentWindow->Flags & ImGuiWindowFlags_Popup) != 0;
    if (sLine[0] != '\0' && sLineWindow == ctx->CurrentWindow && same_line(sLineRect, bb)) {
        snprintf(sNow.before, sizeof(sNow.before), "%s", sLine);
    }
    sLine[0] = '\0';
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext *ctx, ImGuiID id, const char *label, ImGuiItemStatusFlags flags) {
    if (!sOn || label == NULL) {
        return;
    }
    ImGuiTabBar *bar = ctx->CurrentTabBar;
    bool tab = bar != NULL && ImGui::TabBarFindTabByID(bar, id) != NULL;
    if (tab && bar->SelectedTabId == id && ctx->NavWindow != NULL && ctx->CurrentWindow->RootWindow == ctx->NavWindow->RootWindow) {
        shown(sTab, sizeof(sTab), label); // (the focused window's tab bar: its tab that is open)
    }
    if (tab && id == ctx->NavId && sNow.id == 0) { // (a tab says its label before it is added: it starts here)
        sNow.id = id;
        sNow.window = ctx->CurrentWindow;
    }
    if (id != sNow.id || id == 0 || sNow.info) {
        return;
    }
    sNow.info = true;
    sNow.flags = flags;
    sNow.tab = tab;
    sNow.tabSelected = tab && bar->SelectedTabId == id;
    shown(sNow.label, sizeof(sNow.label), label);
    sAfterOpen = true; // (a text beside it, before the next item, belongs to it: "per press", a unit)
}

void ImGuiTestEngineHook_Log(ImGuiContext *ctx, const char *fmt, ...) {
    (void)ctx, (void)fmt;
}

const char *ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *ctx, ImGuiID id) {
    (void)ctx, (void)id;
    return NULL;
}

// Every text Dear ImGui draws (imgui_draw.cpp, ImDrawList::AddText). Whose it is: the last item's (g.LastItemData).
void ImGuiA11yHook_Text(const char *begin, const char *end) {
    if (!sOn || begin == NULL || begin == end || begin[0] == '\0') {
        return;
    }
    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    if (!g.WithinFrameScope || window == NULL) {
        return;
    }
    if (window->Flags & ImGuiWindowFlags_Tooltip) {
        add(sTip, sizeof(sTip), begin, end, "\n");
        return;
    }
    const ImGuiLastItemData &item = g.LastItemData;
    if (item.ID != 0) {
        if (item.ID == sNow.id && window == sNow.window) {
            add(sNow.text, sizeof(sNow.text), begin, end, "\n");
            if (!sNow.info) {
                sAfterOpen = true; // (a combo: it has no ItemInfo, its line is open once it has drawn its choice)
            }
        }
        return;
    }
    // a text of its own (ImGui::Text and the like): beside the focused item, or before the next one
    if (sAfterOpen && window == sNow.window && same_line(item.Rect, sNow.rect)) {
        add(sNow.after, sizeof(sNow.after), begin, end, " ");
        return;
    }
    if (sLineWindow != window || !same_line(sLineRect, item.Rect)) {
        sLine[0] = '\0';
    }
    add(sLine, sizeof(sLine), begin, end, " ");
    sLineWindow = window;
    sLineRect = item.Rect;
}

// ---- ui.cpp's calls ----

extern "C" void UiA11y_Radio(int selected) {
    if (sOn && sNow.id != 0 && GImGui->LastItemData.ID == sNow.id) {
        sNow.radio = selected ? 2 : 1;
    }
}

// A text field's text reaches no hook (InputText draws it with ImFont::RenderText, not ImDrawList::AddText): its caller
// gives it, and the field's name when its label is hidden.
extern "C" void UiA11y_Field(const char *name, const char *text) {
    if (sOn && sNow.id != 0 && GImGui->LastItemData.ID == sNow.id) {
        sNow.field = true;
        snprintf(sNow.name, sizeof(sNow.name), "%s", name);
        snprintf(sNow.fieldText, sizeof(sNow.fieldText), "%s", text);
    }
}

static bool sQueueNext; // UiA11y_QueueNext: the next item said waits for what is being said

extern "C" void UiA11y_QueueNext(void) {
    sQueueNext = true;
}

extern "C" int PortA11y_On(void); // gs/a11y.c

extern "C" void UiA11y_Frame(int windowOpen, int sayClosed) {
    ImGuiContext *ctx = ImGui::GetCurrentContext();
    bool on = windowOpen != 0 && PortA11y_On() != 0;

    if (sWasOn && !on) { // the window closed (not for the question before the game closes: it says itself)
        if (sayClosed) {
            PortA11y_Say(PortA11y_Label("Back to the game"), A11Y_INTERRUPT);
        }
        sSaidId = 0;
        sSaidWindow[0] = sSaidTab[0] = sSaidTip[0] = '\0';
    }
    sWasOn = on;
    sOn = on;
    if (ctx != NULL) {
        ctx->TestEngineHookItems = on;
    }
    memset(&sNow, 0, sizeof(sNow));
    sAfterOpen = false;
    sLine[0] = sTip[0] = sTab[0] = '\0';
    sLineWindow = NULL;
}

// The words of the focused item: its label, its value and its state.
static void describe(const UiA11yItem &it, char *label, size_t labelCap, char *value, size_t valueCap, char *state, size_t stateCap) {
    // the texts it drew; its own label among them is not its value
    const char *pieces[16];
    int count = 0;
    static char text[512];
    snprintf(text, sizeof(text), "%s", it.text);
    for (char *p = text; *p != '\0' && count < 16;) {
        pieces[count++] = p;
        char *nl = strchr(p, '\n');
        if (nl == NULL) {
            break;
        }
        *nl = '\0';
        p = nl + 1;
    }
    snprintf(label, labelCap, "%s", it.label);
    int skip = -1;
    if (label[0] != '\0') {
        for (int i = 0; i < count && skip < 0; i++) {
            skip = strcmp(pieces[i], label) == 0 ? i : -1;
        }
    } else if (!it.info && count >= 2) { // a combo: its choice, then its label
        skip = count - 1;
        snprintf(label, labelCap, "%s", pieces[skip]);
    }
    bool afterUsed = false;
    if (label[0] == '\0' && it.name[0] != '\0') { // a hidden label named by the caller (UiA11y_Field)
        snprintf(label, labelCap, "%s", it.name);
    }
    if (it.field) { // (what it drew is only its hint: its text is the caller's)
        snprintf(value, valueCap, "%s", it.fieldText);
        state[0] = '\0';
        add(state, stateCap, PortA11y_Label("text field"), NULL, ", ");
        if (GImGui->ActiveId == it.id) {
            add(state, stateCap, PortA11y_Label("editing"), NULL, ", ");
        }
        if (it.disabled) {
            add(state, stateCap, PortA11y_Label("unavailable"), NULL, ", ");
        }
        return;
    }
    if (label[0] == '\0') { // a hidden label: the text beside it names it
        if (it.before[0] != '\0') {
            snprintf(label, labelCap, "%s", it.before);
        } else if (it.after[0] != '\0') {
            snprintf(label, labelCap, "%s", it.after);
            afterUsed = true;
        }
    }
    value[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i != skip) {
            add(value, valueCap, pieces[i], NULL, " ");
        }
    }
    if (!afterUsed) {
        add(value, valueCap, it.after, NULL, " ");
    }
    state[0] = '\0';
    if (it.tab) {
        add(state, stateCap, PortA11y_Label("tab"), NULL, ", ");
        if (it.tabSelected) {
            add(state, stateCap, PortA11y_Label("selected"), NULL, ", ");
        }
    } else if (it.radio != 0) {
        add(state, stateCap, PortA11y_Label(it.radio == 2 ? "selected" : "not selected"), NULL, ", ");
    } else if (it.info && (it.flags & ImGuiItemStatusFlags_Checkable)) {
        add(state, stateCap, PortA11y_Label((it.flags & ImGuiItemStatusFlags_Checked) ? "checked" : "not checked"), NULL, ", ");
    } else if (it.info && (it.flags & ImGuiItemStatusFlags_Openable)) {
        add(state, stateCap, PortA11y_Label((it.flags & ImGuiItemStatusFlags_Opened) ? "expanded" : "collapsed"), NULL, ", ");
    } else if (!it.info) {
        add(state, stateCap, PortA11y_Label("combo box"), NULL, ", ");
    } else if (it.flags & ImGuiItemStatusFlags_Inputable) {
        ImGuiContext &g = *GImGui;
        if (g.ActiveId == it.id) {
            add(state, stateCap, PortA11y_Label("editing"), NULL, ", ");
        }
    } else if (!it.popup && count <= 1 && value[0] == '\0') {
        add(state, stateCap, PortA11y_Label("button"), NULL, ", ");
    }
    if (it.disabled) {
        add(state, stateCap, PortA11y_Label("unavailable"), NULL, ", ");
    }
}

extern "C" void UiA11y_End(void) {
    if (!sOn) {
        return;
    }
    ImGuiContext &g = *GImGui;
    char line[1024] = "";
    int flags = A11Y_INTERRUPT;

    // the window's name, when the focus comes into another one (not a list's popup: its name is only an id)
    char window[160] = "";
    if (g.NavWindow != NULL) {
        shown(window, sizeof(window), g.NavWindow->RootWindow->Name);
    }
    bool newWindow = window[0] != '\0' && strcmp(window, sSaidWindow) != 0;
    if (newWindow) {
        snprintf(sSaidWindow, sizeof(sSaidWindow), "%s", window);
        add(line, sizeof(line), PortA11y_Label(window), NULL, ", ");
    }
    // the tab that is open, when it changed (the shoulder buttons, or a tab chosen)
    if (sTab[0] != '\0' && strcmp(sTab, sSaidTab) != 0) {
        snprintf(sSaidTab, sizeof(sSaidTab), "%s", sTab);
        if (!(sNow.tab && sNow.id == sSaidId)) { // (the focused tab says "selected" itself)
            char tab[200];
            snprintf(tab, sizeof(tab), PortA11y_Label("%s tab"), sTab);
            add(line, sizeof(line), tab, NULL, ", ");
        }
    }
    if (sNow.id != 0) {
        char label[160], value[640], state[96];
        describe(sNow, label, sizeof(label), value, sizeof(value), state, sizeof(state));
        // a field's text while it is typed is not said at every letter: once the field is left (Enter, Escape)
        bool typing = sNow.field && g.ActiveId == sNow.id && sNow.id == sSaidId && !newWindow;
        if (sNow.id != sSaidId || newWindow) { // another item: all of it
            char item[1024] = "";
            if (sQueueNext) {
                flags = 0; // (after the page that came up)
            }
            add(item, sizeof(item), label, NULL, ", ");
            add(item, sizeof(item), value, NULL, ", ");
            add(item, sizeof(item), state, NULL, ", ");
            if (line[0] != '\0' && item[0] != '\0') {
                PortA11y_Say(line, flags); // (the window or the tab first, the item after it)
                line[0] = '\0';
                flags = 0;
            }
            add(line, sizeof(line), item, NULL, ", ");
            sSaidTip[0] = '\0';
        } else { // the same item: what changed of it (a slider moved, a box checked)
            if (strcmp(value, sSaidValue) != 0 && !typing) {
                add(line, sizeof(line), value, NULL, ", ");
            }
            if (strcmp(state, sSaidState) != 0) {
                add(line, sizeof(line), state, NULL, ", ");
            }
        }
        sSaidId = sNow.id;
        if (!typing) {
            snprintf(sSaidValue, sizeof(sSaidValue), "%s", value);
        }
        snprintf(sSaidState, sizeof(sSaidState), "%s", state);
        sQueueNext = false;
    }
    if (line[0] != '\0') {
        PortA11y_Say(line, flags);
    }
    // the focused item's description, once it comes up (after a moment, as for a seeing player); never the mouse's
    if (sTip[0] != '\0' && sNow.id != 0 && g.NavCursorVisible && strcmp(sTip, sSaidTip) != 0) {
        static char tip[sizeof(sTip)];
        snprintf(sSaidTip, sizeof(sSaidTip), "%s", sTip);
        snprintf(tip, sizeof(tip), "%s", sTip);
        for (char *p = tip; (p = strchr(p, '\n')) != NULL;) {
            *p = ' ';
        }
        // in pieces the speaker takes (a message of a11y.c holds 1024 bytes): each ends with a sentence where it can
        for (char *p = tip; *p != '\0';) {
            size_t n = strlen(p);
            if (n > 900) {
                char *cut = NULL;
                for (char *q = p; q < p + 900; q++) {
                    cut = (q[0] == '.' && q[1] == ' ') ? q + 1 : cut;
                }
                n = cut != NULL ? (size_t)(cut - p) : 900;
                while (n > 1 && ((unsigned char)p[n] & 0xC0) == 0x80) {
                    n--; // (not inside a letter of more than one byte)
                }
            }
            char keep = p[n];
            p[n] = '\0';
            PortA11y_Say(p, 0);
            p[n] = keep;
            p += n;
            while (*p == ' ') {
                p++;
            }
        }
    }
    sOn = false; // (the windows drawn after this one are not read)
    g.TestEngineHookItems = false;
}
