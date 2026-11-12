// The setup window of the PC port: pick your disc image, watch the steps, press Play.
//
// A small native program (SDL3 + Dear ImGui, the same library and look as the game's F1 settings window). It does
// none of the work itself: it runs `python3 install.py --machine --iso <file>` next to it and shows the events
// that script prints (one line each: @step, @note, @ok, @skip, @fail, @missing, @stopped, @done; see install.py).
//
//   build:  port/setup/build.sh   ->  ./BT4decomp-setup
//   test:   BT3_SETUP_ISO=<file> starts the installation at once; BT3_SETUP_SHOT=<prefix> writes the window's
//           picture to <prefix>_<n>.ppm whenever the page or the step changes (no person needed to check it)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mutex>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_internal.h" // ImGui::NavInitWindow
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h" // glViewport, glClear for the OpenGL window (set up by ImGui_ImplOpenGL3_Init)
#include "native.h"
#include "progress.h"
#include "ui.h" // UiA11y_*: the focused control said to the screen reader (port/src/gs/ui_a11y.cpp)

#define GAME "BT4decomp" /* the program this sets up: GAME, GAME.dat; this program is GAME-setup */

// The screen reader, as in the game: through Prism when prism.dll is beside this program (port/src/gs/a11y.c, with
// its settings in bt3_settings.txt when there is one: a11y=0 turns it off). ui_a11y.cpp says the focused control;
// said here are each page as it comes up (its heading and text), a disc image chosen, a problem under a field, and
// the installation's steps as they start and end.
extern "C" {
int PortA11y_Begin(void);
void PortA11y_Say(const char *text, int flags);
void PortA11y_Event(const SDL_Event *ev);
// a11y.c's links to the game's sound, which the setup never uses (it plays no cue and no voice)
SDL_AudioDeviceID Port_AudioDevice(void) { return 0; }
int PortA11y_VoicePercent(const void *player) { (void)player; return -1; }
}
#define A11Y_INTERRUPT 1 // (gs/a11y.c)

enum Page { PAGE_PICK, PAGE_RUN, PAGE_DONE, PAGE_FAILED };
enum State { PENDING, RUNNING, OK, SKIPPED, FAILED };
struct StepRow { std::string title, detail; State state = PENDING; Uint64 start = 0, end = 0; SetupProgress progress; };
struct Missing { std::string name, why, pkg; };

// The steps of the two ways to install: from a source checkout (install.py does the work and needs build tools) and
// in a release folder (BT4decomp and BT4decomp.dat next to this program: native.cpp unpacks the disc itself).
static const char *kSourceSteps[] = {"Requirements", "Imagen ISO", "Datos del juego", "Executable data", "Build", "Prueba del juego"};
static const char *kReleaseSteps[] = {"Disc image", "Game data", "Native data tables", "Self-test"};
static const struct { const char *title, *help; } kHelp[] = {
    {"Disc image", "Verify the LATINO or English BT4 B14 Rev2 disc profile"},
    {"Game data", "Extract and verify the files for the selected disc profile"},
    {"Native data tables", "Prepare the data tables for this executable and disc profile"},
    {"Self-test", "Run a test fight without opening a window"},
    {"European languages", "Spanish, German, French and Italian from the European disc"},
    {"Requirements", "Compilers and tools on this computer"}, {"Imagen ISO", "Comprobación de la ISO y su versión"},
    {"Datos del juego", "Extracción y verificación de modelos, sonidos y textos"}, {"Executable data", "The data tables of the game's programs"},
    {"Build", "The port is compiled (a few minutes)"}, {"Prueba del juego", "Pelea de prueba sin abrir una ventana"}};
static bool sRelease;
static bool sPackage=false;
static int sUiLanguage=0; // Spanish, English, Portuguese (Brazil)
static int sInstallMask=1,sPackagePercent=0;
static std::string sGithubCode, sActivity;
static bool sInstallTexturePack=false, sInstallPtbrPack=false;
static bool sTextureMaintenance=false;

static const char *L(const char *es,const char *en,const char *pt){
    return sUiLanguage==2?pt:sUiLanguage==1?en:es;
}


static const char *step_help(const std::string &title) {
    for (auto &h : kHelp) {
        if (title == h.title) {
            return h.help;
        }
    }
    return "";
}

static SDL_Window *sWindow;
static SDL_GPUDevice *sDevice;
// The window is drawn through SDL's GPU layer (Vulkan on Linux, Direct3D 12 on Windows) or, where that cannot
// start (no Vulkan driver, a graphics chip too old for it: reported for Intel Ivy Bridge), through OpenGL.
// BT3_GPU_API=gl asks for OpenGL at once, as it does for the game.
static SDL_GLContext sGl;
static bool sUseGl;
static Page sPage = PAGE_PICK;
static char sIso[1024];
// Optional: a disc image of the European release (SLES-54945), for Spanish, German, French and Italian. The game is
// always set up from the USA one; from this one only the languages' archive entries are taken (native.cpp step_eu,
// install.py --eu-iso).
static char sIsoEu[1024];
static bool sEuOnly;    // only the languages are added (the game was set up before: the "Listo para jugar" page)
static int sDialogFor;  // 0 BT4 ISO, 1 optional European ISO
static std::string sRoot, sError, sLauncher, sLine;
static std::vector<StepRow> sRows;
static std::vector<Missing> sMissing;
static SDL_Process *sProc;
static bool sNative; // the native installer is at work (release folder)
static std::mutex sDialogLock;
static std::string sDialogResult;
static bool sDialogReady, sQuit;
static Uint64 sRunStart, sRunEnd;
static const ImVec4 kAccent(0.96f, 0.55f, 0.13f, 1.0f), kGreen(0.36f, 0.80f, 0.45f, 1.0f), kRed(0.93f, 0.36f, 0.33f, 1.0f),
    kDim(0.60f, 0.63f, 0.70f, 1.0f);
// For the screen reader (see the top): the page's words as this frame drew them, the problem shown under a field, and
// what happened since the last frame (steps, a file chosen), said after the page so that the page does not cut it.
static std::string sPageWords, sProblem, sSaidProblem, sNews;
static int sSaidPage = -1, sNotePercent;

static void page_words(const std::string &text) {
    if (!text.empty()) {
        sPageWords += (sPageWords.empty() ? "" : " ") + text;
    }
}

static void news(const std::string &text) {
    if (!text.empty()) {
        sNews += (sNews.empty() ? "" : ". ") + text;
    }
}

// A file name without its folder (what is said of a disc image chosen).
static std::string file_name(const char *path) {
    const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    const char *p = slash > back ? slash : back;
    return p != NULL ? p + 1 : path;
}

static bool file_exists(const char *path) {
    SDL_PathInfo info;
    return path[0] != '\0' && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

// The folder to install into: a release folder (this program's own), or the source checkout that holds install.py.
static std::string find_root() {
    std::string dir = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    if ((file_exists((dir + GAME).c_str()) || file_exists((dir + GAME ".exe").c_str())) && file_exists((dir + GAME ".dat").c_str())) {
        sRelease = true; // a release folder: the finished program is here, only the game data is missing
        return dir;
    }
    for (int up = 0; up < 4; up++) {
        // The installer lives under tbo/setup to keep the portable root clean.
        if (file_exists((dir + GAME ".exe").c_str()) && file_exists((dir + GAME ".dat").c_str())) {
            sRelease = true;
            return dir;
        }
        if (file_exists((dir + "install.py").c_str())) {
            return dir;
        }
        dir += "../";
    }
    return "";
}

static void style() {
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 0.0f;
    st.FrameRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.WindowPadding = ImVec2(28.0f, 22.0f);
    st.FramePadding = ImVec2(12.0f, 8.0f);
    st.ItemSpacing = ImVec2(10.0f, 10.0f);
    st.WindowBorderSize = 0.0f;
    ImVec4 *c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.025f, 0.025f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.29f, 0.38f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.96f, 0.55f, 0.13f, 0.55f);
    c[ImGuiCol_ButtonActive] = kAccent;
    c[ImGuiCol_PlotHistogram] = kAccent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.96f, 0.55f, 0.13f, 0.35f);
}

// ---------------------------------------------------------------------------------------------- the engine script

// The rows as the run numbers its steps: the European languages, when asked for, before the self-test (native.cpp
// work, install.py main); only them when they are added to a game set up before.
static void reset_rows() {
    std::vector<std::string> titles;
    if (sRelease) {
        titles.assign(kReleaseSteps, kReleaseSteps + sizeof(kReleaseSteps) / sizeof(kReleaseSteps[0]));
    } else {
        titles.assign(kSourceSteps, kSourceSteps + sizeof(kSourceSteps) / sizeof(kSourceSteps[0]));
    }
    if (sEuOnly) {
        titles.assign(1, "European languages");
    } else if (sIsoEu[0] != '\0') {
        titles.insert(titles.end() - 1, "European languages");
    }
    if(sRelease && Native_BundledEnglish(sRoot)){
        if(!sIso[0] && !sEuOnly)titles.clear();
        titles.push_back("English language");titles.push_back("English self-test");
    }
    if(sRelease&&(sInstallMask==3||sInstallPtbrPack)){titles.push_back("Language package");titles.push_back("Self-test");}
    if(sRelease&&sInstallTexturePack)titles.push_back("Texture pack");
    if(sRelease&&sInstallPtbrPack)titles.push_back("Portuguese (Brazil) language pack");
    sPackage=false;sPackagePercent=0;sGithubCode.clear();sActivity.clear();
    sRows.assign(titles.size(), StepRow());
    for (size_t i = 0; i < titles.size(); i++) {
        sRows[i].title = titles[i];
    }
}

static void start_texture_update() {
    sRows.assign(1,StepRow());sRows[0].title="Texture pack";
    sError.clear();sLauncher.clear();sActivity.clear();sPackage=false;
    sRunStart=SDL_GetTicks();sNative=true;sInstallTexturePack=true;
    Native_StartTextures(sRoot);sPage=PAGE_RUN;
}

static void start_install() {
    static std::string iso, euIso;
    const char *args[10];
    SDL_PropertiesID props = SDL_CreateProperties();
    std::string script = sRoot + "install.py";
    int n = 0;

    if (sRelease) {
        SDL_DestroyProperties(props);
        reset_rows();
        sMissing.clear();
        sError.clear();
        sLauncher.clear();
        sRunStart = SDL_GetTicks();
        sNative = true;
        Native_Start(sRoot,sEuOnly?"":sIso,"",sInstallMask,
                     sInstallTexturePack?"@download":"",
                     sInstallPtbrPack?"@download":"");
        sPage = PAGE_RUN;
        return;
    }
    iso = sIso;
    args[n++] = "python3";
    args[n++] = script.c_str();
    args[n++] = "--machine";
    args[n++] = "--iso";
    args[n++] = iso.c_str();
    if (sIsoEu[0] != '\0') {
        euIso = sIsoEu;
        args[n++] = "--eu-iso";
        args[n++] = euIso.c_str();
    }
    if (getenv("BT3_SETUP_ARGS") != NULL) { // one extra argument for testing, such as --skip-test
        args[n++] = getenv("BT3_SETUP_ARGS");
    }
    args[n] = NULL;
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, sRoot.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    reset_rows();
    sMissing.clear();
    sError.clear();
    sLine.clear();
    sLauncher.clear();
    sProc = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    sRunStart = SDL_GetTicks();
    if (sProc == NULL) {
        sError = std::string("Could not start python3: ") + SDL_GetError() + "\nPython 3 has to be installed to set the game up.";
        sPage = PAGE_FAILED;
        return;
    }
    sPage = PAGE_RUN;
}

static StepRow *current() {
    for (auto &r : sRows) {
        if (r.state == RUNNING) {
            return &r;
        }
    }
    return NULL;
}

static std::string unescape(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') {
            out += '\n';
            i++;
            while (i + 1 < s.size() && s[i + 1] == ' ') { // the terminal version indents continuation lines
                i++;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static void event(const std::string &line) {
    if (line.empty() || line[0] != '@') {
        return;
    }
    size_t sp = line.find(' ');
    std::string kind = line.substr(1, sp == std::string::npos ? std::string::npos : sp - 1);
    std::string text = sp == std::string::npos ? "" : unescape(line.substr(sp + 1));
    StepRow *cur = current();

    if(kind=="activity"){sActivity=text;return;}
    if(cur && kind=="progress"){cur->progress.read(text.c_str());return;}
    if(cur && kind=="files"){
        unsigned done=0,total=0;
        if(sscanf(text.c_str(),"%u %u",&done,&total)==2 && total && done<=total){
            cur->progress.files=done;cur->progress.totalFiles=total;
        }
        return;
    }
    if(kind=="package"){
        sPackage=true;sPackagePercent=std::max(0,std::min(100,atoi(text.c_str())));sGithubCode.clear();
        if(cur)cur->progress.fraction=std::max(cur->progress.fraction,sPackagePercent/100.0);
        return;
    }
    if(kind=="github"){sGithubCode=text;return;}
    if (kind == "step") {
        int i = atoi(text.c_str());
        int count=0,titleAt=0;
        if(sscanf(text.c_str(),"%d %d %n",&i,&count,&titleAt)==2 &&
           i>=1 && i<=count && count<=32 && titleAt>0){
            // A language-only run, or a cached optional pack, has fewer steps
            // than a full ISO install. Follow the worker's actual step list.
            sRows.resize(count);
            sRows[i-1].title=text.substr(titleAt);
        }
        if (i >= 1 && i <= (int)sRows.size()) {
            sPackage=false;sPackagePercent=0;sActivity.clear();
            sRows[i - 1].progress=SetupProgress();
            sRows[i - 1].state = RUNNING;
            sRows[i - 1].start = SDL_GetTicks();
            sRows[i - 1].detail.clear();
            news(sRows[i - 1].title + ", step " + std::to_string(i) + " of " + std::to_string(sRows.size()));
            sNotePercent = 0;
        }
    } else if (cur != NULL && kind == "note") {
        cur->detail = text;
        // a progress note ("37%, 1200 files") comes for every percent: said every 10
        int percent = text.find('%') != std::string::npos ? atoi(text.c_str()) : -1;
        if (percent < 0) {
            news(text);
        } else if (percent / 10 > sNotePercent / 10) {
            news(text);
            sNotePercent = percent;
        }
    } else if (cur != NULL && (kind == "ok" || kind == "skip" || kind == "fail")) {
        cur->state = kind == "ok" ? OK : kind == "skip" ? SKIPPED : FAILED;
        cur->end = SDL_GetTicks();
        cur->detail = kind == "fail" ? "" : text;
        if (kind != "fail") { // (a failure is said by the page that comes up)
            news(cur->title + (kind == "ok" ? " done" : " skipped") + (text.empty() ? "" : ": " + text));
        }
    } else if (kind == "missing") {
        Missing m;
        size_t a = text.find('|'), b = text.find('|', a == std::string::npos ? 0 : a + 1);
        if (a != std::string::npos && b != std::string::npos) {
            m.name = text.substr(0, a);
            m.why = text.substr(a + 1, b - a - 1);
            m.pkg = text.substr(b + 1);
            sMissing.push_back(m);
        }
    } else if (kind == "stopped") {
        sError = text;
    } else if (kind == "done") {
        sLauncher = text;
    }
}

static void play_game() {
        const char *args[2] = {sLauncher.c_str(), NULL};
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_Environment *env = SDL_CreateEnvironment(true);
        SDL_SetEnvironmentVariable(env, "BT3_GS", "gpu", true);
        // Cancel/failure may leave the old pack installed. Permit this launch
        // without immediately reopening maintenance; retry next normal launch.
        if(sTextureMaintenance)SDL_SetEnvironmentVariable(env,"BT4_TEXTURE_UPDATE_SKIP","1",true); // the window and the GPU renderer
        for (const char *key : {"BT3_DEMO", "BT3_REPLAY", "BT3_DATA", "BT3_SETTINGS", "BT3_SAVES", "BT3_PAD_PLAY", "BT3_PAD_REC", "BT3_NET_SESSION", "BT3_NET_HOST", "BT3_NET_JOIN", "BT3_LOBBY", "BT3_NET_SCRIPT", "BT4_TEST_MODE", "BT4_TEST_TECH", "BT4_RENDER_TEST"}) {
            SDL_UnsetEnvironmentVariable(env, key);
        }
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, sRoot.c_str());
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
        SDL_Process *game = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        SDL_DestroyEnvironment(env);
        if (game != NULL) {
            SDL_DestroyProcess(game);
            sQuit = true;
        } else {
            sError = std::string(L("No se pudo abrir BT4: ","Could not start BT4: ","Não foi possível iniciar BT4: ")) + SDL_GetError();
#ifdef __APPLE__
            sError += L("\nRevisá los permisos de macOS indicados en README.txt.","\nCheck the macOS download permissions described in README.txt.","\nConfira as permissões de download do macOS descritas em README.txt.");
#endif
            sPage = PAGE_FAILED;
        }
}

static void pump_install() {
    char buf[4096];
    SDL_IOStream *out;
    int code = 0;

    if (sNative) {
        std::string line;
        bool ended = !Native_Running(); // asked before the queue is emptied: nothing is left behind
        while (Native_Poll(line)) {
            event(line);
        }
        if (ended) {
            sNative = false;
            sRunEnd = SDL_GetTicks();
            for (auto &r : sRows) {
                if (r.state == RUNNING) {
                    r.state = FAILED;
                    r.end = sRunEnd;
                }
            }
            if (!sLauncher.empty()) {
                sPage = PAGE_DONE;
                if(sTextureMaintenance)play_game();
            } else {
                if (sError.empty()) {
                    sError = "La instalación se detuvo. Revisa el registro para ver el motivo.";
                }
                sPage = PAGE_FAILED;
            }
        }
        return;
    }
    if (sProc == NULL) {
        return;
    }
    out = SDL_GetProcessOutput(sProc);
    for (;;) {
        size_t n = out != NULL ? SDL_ReadIO(out, buf, sizeof(buf)) : 0;
        if (n == 0) {
            break;
        }
        for (size_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                event(sLine);
                sLine.clear();
            } else if (buf[i] != '\r') {
                sLine += buf[i];
            }
        }
    }
    if (SDL_WaitProcess(sProc, false, &code)) {
        event(sLine);
        SDL_DestroyProcess(sProc);
        sProc = NULL;
        sRunEnd = SDL_GetTicks();
        for (auto &r : sRows) {
            if (r.state == RUNNING) {
                r.state = FAILED;
                r.end = sRunEnd;
            }
        }
        if (code == 0 && !sLauncher.empty()) {
            sPage = PAGE_DONE;
        } else {
            if (sError.empty()) {
                sError = "The setup script stopped unexpectedly. The details are in install.log.";
            }
            sPage = PAGE_FAILED;
        }
    }
}

// --------------------------------------------------------------------------------------------------------- pages

static void SDLCALL dialog_done(void *userdata, const char *const *files, int filter) {
    (void)filter;
    std::lock_guard<std::mutex> lock(sDialogLock);
    if (files != NULL && files[0] != NULL) {
        sDialogResult = files[0];
        sDialogFor = userdata == (void *)1 ? 1 : 0;
        sDialogReady = true;
    }
}

static void heading(const char *title, const char *sub) {
    page_words(std::string(title) + ".");
    page_words(sub);
    ImGui::PushFont(NULL, 30.0f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextWrapped("%s", sub);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

static bool big_button(const char *label, bool primary, bool enabled = true) {
    bool pressed;
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.55f, 0.13f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.63f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.06f, 0.03f, 1.0f));
    }
    ImGui::BeginDisabled(!enabled);
    pressed = ImGui::Button(label, ImVec2(150.0f, 40.0f));
    ImGui::EndDisabled();
    if (primary) {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

// Buttons of a page sit on one line at the bottom right.
static void bottom_row(int buttons) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float w = buttons * 150.0f + (buttons - 1) * ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x - w, ImGui::GetCursorPosY() + avail.y - 40.0f));
}

// The European disc image's field (optional). Returns false when something is in it that is not a file.
static bool eu_field() {
    static const SDL_DialogFileFilter filters[] = {{"PS2 disc image", "iso;bin;img"}, {"All files", "*"}};
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextUnformatted("European disc image (optional)");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 130.0f);
    ImGui::InputTextWithHint("##isoeu", "the European release (SLES-54945)", sIsoEu, sizeof(sIsoEu));
    UiA11y_Field("European disc image (optional)", sIsoEu);
    ImGui::SameLine();
    if (ImGui::Button("Browse...##eu", ImVec2(120.0f, 0.0f))) {
        SDL_ShowOpenFileDialog(dialog_done, (void *)1, sWindow, filters, 2, NULL, false);
    }
    if (sIsoEu[0] != '\0' && !file_exists(sIsoEu)) {
        ImGui::TextColored(kRed, "El archivo no existe.");
        sProblem += "European disc image: that file does not exist. ";
        return false;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextWrapped("Adds Spanish, German, French and Italian: menus, battle screen and text, as the European release has them. F1 > UI > Language picks one.");
    ImGui::PopStyleColor();
    return true;
}

static bool language_choice(const char *id, const char *title, const char *description,
                            bool selected, float width) {
    ImGui::PushID(id);
    const ImVec2 start=ImGui::GetCursorScreenPos();
    const ImVec2 finish(start.x+width,start.y+84.0f);
    const ImVec4 cardBg=selected?ImVec4(0.20f,0.13f,0.08f,1.0f):ImVec4(0.075f,0.085f,0.12f,1.0f);
    const ImVec4 cardBorder=selected?kAccent:ImVec4(0.24f,0.28f,0.35f,1.0f);
    const bool pressed=ImGui::InvisibleButton(title,ImVec2(width,84.0f),ImGuiButtonFlags_EnableNav);
    UiA11y_Radio(selected);
    ImDrawList *draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(start,finish,ImGui::GetColorU32(cardBg),7.0f);
    draw->AddRect(start,finish,ImGui::GetColorU32(cardBorder),7.0f,0,selected?1.6f:1.0f);
    const ImVec4 buttonBg=selected?kAccent:ImGui::IsItemHovered()?ImVec4(0.25f,0.29f,0.36f,1):ImVec4(0.17f,0.20f,0.27f,1);
    draw->AddRectFilled(ImVec2(start.x+8,start.y+7),ImVec2(finish.x-8,start.y+41),ImGui::GetColorU32(buttonBg),5);
    const float titleWidth=ImGui::CalcTextSize(title).x;
    draw->AddText(ImVec2(start.x+(width-titleWidth)*0.5f,start.y+14),
        ImGui::GetColorU32(selected?ImVec4(0.08f,0.05f,0.02f,1):ImGui::GetStyleColorVec4(ImGuiCol_Text)),title);
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(start.x+12,start.y+49),ImGui::GetColorU32(kDim),description,NULL,width-24);
    if(ImGui::IsItemFocused())draw->AddRect(ImVec2(start.x+3,start.y+3),ImVec2(finish.x-3,finish.y-3),ImGui::GetColorU32(kAccent),5,0,1.5f);
    ImGui::PopID();
    return pressed;
}

static void page_pick() {
    // SDL retains filters until its asynchronous callback. Keep one static set
    // per locale so changing the UI language cannot invalidate an open dialog.
    static const SDL_DialogFileFilter isoFilters[3][2]={
        {{"BT4 ISO","iso;bin;img"},{"Todos los archivos","*"}},
        {{"BT4 ISO","iso;bin;img"},{"All files","*"}},
        {{"BT4 ISO","iso;bin;img"},{"Todos os arquivos","*"}}};
    const auto *filters=isoFilters[sUiLanguage];
    // Reserve the footer so long translations and selected file paths can scroll
    // without overlapping the installation buttons.
    ImGui::BeginChild("installation-options",ImVec2(0,-58),ImGuiChildFlags_None);
    const ImVec2 header=ImGui::GetCursorPos();
    const float headerWidth=ImGui::GetContentRegionAvail().x;
    heading("DRAGON BALL Z BT4",L("Instalación","Installation","Instalação"));
    const float headerEnd=ImGui::GetCursorPosY();
    ImGui::SetCursorPos(ImVec2(header.x+headerWidth-230,header.y));
    ImGui::TextColored(kDim,"%s",L("Idioma del instalador","Installer language","Idioma do instalador"));
    ImGui::SetCursorPosX(header.x+headerWidth-230);
    ImGui::SetNextItemWidth(230);
    ImGui::Combo("##interface-language",&sUiLanguage,"Español\0English\0Português (Brasil)\0");
    ImGui::SetCursorPos(ImVec2(header.x,std::max(headerEnd,ImGui::GetCursorPosY())));
    ImGui::PushFont(NULL,22);
    ImGui::TextUnformatted(L("Elegí el idioma del juego","Choose the game language","Escolha o idioma do jogo"));
    ImGui::PopFont();

    const float gap=ImGui::GetStyle().ItemSpacing.x;
    const float cardWidth=(ImGui::GetContentRegionAvail().x-gap)*0.5f;
    const bool spanishSelected=!sInstallPtbrPack&&sInstallMask==1;
    const bool englishSelected=!sInstallPtbrPack&&sInstallMask==2;
    const bool bothSelected=!sInstallPtbrPack&&sInstallMask==3;
    const bool portugueseSelected=sInstallPtbrPack;
    if(language_choice("spanish",L("Español","Spanish","Espanhol"),L("Textos y voces en español.","Spanish text and audio.","Textos e vozes em espanhol."),spanishSelected,cardWidth)){
        sInstallMask=1;sInstallPtbrPack=false;
    }
    ImGui::SameLine();
    if(language_choice("english",L("Inglés","English","Inglês"),L("Textos y voces en inglés.","English text and audio.","Textos e vozes em inglês."),englishSelected,cardWidth)){
        sInstallMask=2;sInstallPtbrPack=false;
    }
    if(language_choice("both",L("Español e inglés","Spanish + English","Espanhol e inglês"),L("Instala ambos perfiles de idioma.","Installs both language profiles.","Instala os dois perfis de idioma."),bothSelected,cardWidth)){
        sInstallMask=3;sInstallPtbrPack=false;
    }
    ImGui::SameLine();
    if(language_choice("ptbr","Português (Brasil)",L("Textos en portugués de Brasil.","Brazilian Portuguese text.","Textos em português do Brasil."),portugueseSelected,cardWidth)){
        sInstallMask=2;sInstallPtbrPack=true;
    }

    ImGui::PushStyleColor(ImGuiCol_Text,kDim);
    if(sInstallPtbrPack){
        ImGui::TextWrapped("%s",L("El paquete PT-BR se descarga y verifica automáticamente. Si elegís una ISO latina, se agregan los datos ingleses necesarios.","The PT-BR pack downloads and verifies automatically. If you select a Spanish ISO, setup adds the required English data.","O pacote PT-BR é baixado e verificado automaticamente. Se você selecionar uma ISO em espanhol, o instalador adicionará os dados em inglês necessários."));
        ImGui::TextUnformatted("Português (Brasil) v1.2  •  59.1 MB");
    }else if(sInstallMask==3){
        ImGui::TextWrapped("%s",L("El perfil adicional se descarga y verifica automáticamente.","The additional profile downloads and verifies automatically.","O perfil adicional é baixado e verificado automaticamente."));
    }else{
        ImGui::TextWrapped("%s",L("Se instala un solo idioma y el juego inicia directamente en él.","Installs one language; the game starts directly in it.","Instala um idioma; o jogo inicia diretamente nele."));
    }
    ImGui::PopStyleColor();
    bool packsReady=true;
    if(sRelease){
        ImGui::Dummy(ImVec2(0,4));
        ImGui::Separator();
        ImGui::TextUnformatted(L("Paquetes adicionales","Additional packs","Pacotes adicionais"));
        ImGui::Checkbox(L("BT4 HD v1.0 Light (2,54 GB)","BT4 HD v1.0 Light (2.54 GB)","BT4 HD v1.0 Light (2,54 GB)"),&sInstallTexturePack);
        ImGui::PushStyleColor(ImGuiCol_Text,kDim);
        ImGui::TextWrapped("%s",L("Descarga e instalación automáticas desde GitHub.","Downloads and installs automatically from GitHub.","Download e instalação automáticos pelo GitHub."));
        ImGui::PopStyleColor();
    }
    const bool portugueseSourceReady=Native_Installed(sRoot)||file_exists(sIso);
    if(sInstallPtbrPack&&!portugueseSourceReady){
        packsReady=false;
        ImGui::TextColored(kRed,"%s",L("Seleccioná una ISO compatible de BT4 o una instalación existente.","Select a supported BT4 ISO or an existing installation.","Selecione uma ISO compatível de BT4 ou use uma instalação existente."));
    }
    ImGui::TextUnformatted(L("Tu ISO de BT4 B14 Rev2","Your BT4 B14 Rev2 ISO","Sua ISO de BT4 B14 Rev2"));
    const char *isoLabel=L("Elegir ISO","Choose ISO","Selecionar ISO");
    const float isoButtonWidth=ImGui::CalcTextSize(isoLabel).x+24;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-isoButtonWidth-ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputTextWithHint("##iso",L("Seleccionar archivo…","Select file…","Selecionar arquivo…"),sIso,sizeof(sIso));ImGui::SameLine();
    UiA11y_Field(L("Tu ISO de BT4 B14 Rev2","Your BT4 B14 Rev2 ISO","Sua ISO de BT4 B14 Rev2"),sIso);
    if(ImGui::Button(isoLabel,ImVec2(isoButtonWidth,0)))SDL_ShowOpenFileDialog(dialog_done,NULL,sWindow,filters,2,NULL,false);
    bool existing=Native_Installed(sRoot);
    bool languageReady=sInstallPtbrPack?portugueseSourceReady:sInstallMask==3||sIso[0]||
        (sInstallMask==2?Native_EnglishInstalled(sRoot):Native_SpanishInstalled(sRoot));
    bool ok=(file_exists(sIso)||(sIso[0]==0&&existing))&&languageReady;
    if(existing&&sIso[0]==0)ImGui::TextColored(kDim,"%s",L("Se usará la instalación actual.","Your existing installation will be used.","Sua instalação atual será utilizada."));
    else if(sIso[0]&&!file_exists(sIso))ImGui::TextColored(kRed,"%s",L("No se encuentra ese archivo.","That file could not be found.","O arquivo não foi encontrado."));
    ImGui::EndChild();
    bottom_row(2);
    if(big_button(L("Cerrar","Close","Fechar"),false))sQuit=true;
    ImGui::SameLine();
    if(big_button(L("Instalar","Install","Instalar"),true,ok&&packsReady&&!sRoot.empty())){sEuOnly=false;start_install();}
}

static void state_icon(State st, float t) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + 11.0f, p.y + 11.0f);

    if (st == OK || st == SKIPPED) {
        ImU32 col = ImGui::GetColorU32(kGreen);
        dl->AddCircle(c, 10.0f, col, 32, 1.6f);
        dl->AddLine(ImVec2(c.x - 5.0f, c.y), ImVec2(c.x - 1.5f, c.y + 4.0f), col, 2.0f);
        dl->AddLine(ImVec2(c.x - 1.5f, c.y + 4.0f), ImVec2(c.x + 5.5f, c.y - 4.0f), col, 2.0f);
    } else if (st == FAILED) {
        ImU32 col = ImGui::GetColorU32(kRed);
        dl->AddCircle(c, 10.0f, col, 32, 1.6f);
        dl->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), col, 2.0f);
        dl->AddLine(ImVec2(c.x - 4.0f, c.y + 4.0f), ImVec2(c.x + 4.0f, c.y - 4.0f), col, 2.0f);
    } else if (st == RUNNING) {
        float a = t * 5.0f;
        dl->AddCircle(c, 10.0f, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)), 32, 2.0f);
        dl->PathArcTo(c, 10.0f, a, a + 1.9f, 24);
        dl->PathStroke(ImGui::GetColorU32(kAccent), 0, 2.4f);
    } else {
        dl->AddCircle(c, 10.0f, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.18f)), 32, 1.4f);
    }
    ImGui::Dummy(ImVec2(22.0f, 22.0f));
}

static void step_list() {
    float t = (float)SDL_GetTicks() / 1000.0f;
    for (size_t i = 0; i < sRows.size(); i++) {
        StepRow &r = sRows[i];
        char clock[32] = "";
        state_icon(r.state, t);
        ImGui::SameLine(0.0f, 14.0f);
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 1.0f));
        ImGui::TextColored(r.state == PENDING ? kDim : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", r.title.c_str());
        ImGui::TextColored(kDim, "%s", !r.detail.empty() ? r.detail.c_str() : step_help(r.title));
        ImGui::PopStyleVar();
        ImGui::EndGroup();
        if (r.state == RUNNING) {
            snprintf(clock, sizeof(clock), "%d s", (int)((SDL_GetTicks() - r.start) / 1000));
        } else if ((r.state == OK || r.state == FAILED) && r.end - r.start >= 2000) {
            snprintf(clock, sizeof(clock), "%d s", (int)((r.end - r.start) / 1000));
        }
        if (clock[0] != '\0') {
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - ImGui::CalcTextSize(clock).x);
            ImGui::TextColored(kDim, "%s", clock);
        }
    }
}

static void page_run() {
    ImGui::Dummy(ImVec2(0,70));
    if(!sGithubCode.empty()){
        heading(L("Conectar con GitHub","Connect to GitHub","Conectar ao GitHub"),L("Autorizá tu cuenta para descargar el idioma.","Authorize your account to download the language.","Autorize sua conta para baixar o idioma."));
        if(sGithubCode!="login"){
            ImGui::PushFont(NULL,36);ImGui::TextUnformatted(sGithubCode.c_str());ImGui::PopFont();
            ImGui::TextUnformatted("github.com/login/device");
            if(ImGui::Button(L("Abrir GitHub","Open GitHub","Abrir GitHub")))SDL_OpenURL("https://github.com/login/device");
        }
    }else{
        heading(sPackage?L("Instalando paquete de idioma","Installing language pack","Instalando o pacote de idioma"):L("Instalando BT4","Installing BT4","Instalando BT4"),"");
        double done=0,total=0;
        for(auto &r:sRows){
            double weight=SetupStepWeight(r.title.c_str());total+=weight;
            done+=weight*((r.state==OK||r.state==SKIPPED)?1:r.progress.fraction);
        }
        double pct=SetupPercent(done,total,!sLauncher.empty());
        ImGui::PushFont(NULL,44);ImGui::Text("%.1f%%",pct);ImGui::PopFont();
        ImGui::Dummy(ImVec2(0,18));ImGui::ProgressBar((float)(pct/100),ImVec2(-FLT_MIN,16),"");
        ImGui::Dummy(ImVec2(0,18));
        StepRow *row=current();const char *task=L("Preparando instalación","Preparing installation","Preparando a instalação");
        if(sActivity=="finalize")task=L("Guardando la instalación","Saving installation","Salvando a instalação");
        else if(sActivity=="extract")task=L("Extrayendo y verificando archivos","Extracting and verifying files","Extraindo e verificando os arquivos");
        else if(sActivity=="download")task=row&&row->title=="Texture pack"?
            L("Descargando texturas HD","Downloading HD textures","Baixando texturas HD"):
            L("Descargando paquete de idioma","Downloading language pack","Baixando o pacote de idioma");
        else if(sActivity=="verify-package")task=L("Verificando la descarga","Verifying download","Verificando o download");
        else if(sActivity=="unpack")task=L("Extrayendo paquete de idioma","Extracting language pack","Extraindo o pacote de idioma");
        else if(sActivity=="texture-copy")task=L("Instalando el paquete de texturas HD","Installing the HD texture pack","Instalando o pacote de texturas HD");
        else if(sActivity=="ptbr-unpack")task=L("Instalando Português (Brasil)","Installing Português (Brasil)","Instalando Português (Brasil)");
        else if(row){
            if(row->title=="Disc image"||row->title=="Imagen ISO")task=L("Verificando ISO de BT4","Verifying BT4 ISO","Verificando a ISO de BT4");
            else if(row->title=="Native data tables"||row->title=="Executable data")task=L("Preparando datos del juego","Preparing game data","Preparando os dados do jogo");
            else if(row->title=="Self-test"||row->title=="Prueba del juego")task=L("Comprobando que el juego funcione","Checking the game installation","Verificando a instalação do jogo");
            else if(row->title=="Build")task=L("Compilando el juego","Building the game","Compilando o jogo");
            else if(row->title=="Game data"||row->title=="Datos del juego")task=L("Extrayendo archivos del juego","Extracting game files","Extraindo os arquivos do jogo");
        }
        const char spinner[]="|/-\\";
        ImGui::Text("%c  %s",spinner[(SDL_GetTicks()/150)%4],task);
        if(row && row->progress.totalFiles){
            ImGui::Text(L("%u de %u archivos verificados","%u of %u files verified","%u de %u arquivos verificados"),row->progress.files,row->progress.totalFiles);
            if(row->progress.totalBytes)ImGui::TextDisabled("%.1f / %.1f MiB",row->progress.bytes/1048576.0,row->progress.totalBytes/1048576.0);
        }
        Uint64 elapsed=sRunStart?(SDL_GetTicks()-sRunStart)/1000:0;
        ImGui::TextDisabled(L("Tiempo transcurrido: %02u:%02u","Elapsed time: %02u:%02u","Tempo decorrido: %02u:%02u"),(unsigned)(elapsed/60),(unsigned)(elapsed%60));
    }
    bottom_row(1);
    if(big_button(L("Cancelar","Cancel","Cancelar"),false)){Native_Cancel();sError="INSTALL_CANCELLED";}
}

static void page_done() {
    ImGui::Dummy(ImVec2(0,75));
    if(sTextureMaintenance)heading(L("Texturas actualizadas","Textures updated","Texturas atualizadas"),"BT4 HD v1.0 Light (2.54 GB)");
    else heading(L("BT4 está listo","BT4 is ready","BT4 está pronto"),sInstallPtbrPack?L("Português (Brasil) instalado.","Portuguese (Brazil) installed.","Português (Brasil) instalado."):sInstallMask==3?L("Español e inglés instalados.","Spanish and English installed.","Espanhol e inglês instalados."):sInstallMask==2?L("Inglés instalado.","English installed.","Inglês instalado."):L("Español instalado.","Spanish installed.","Espanhol instalado."));
    if(sInstallPtbrPack)ImGui::TextWrapped("%s",L("Português (Brasil) instalado y disponible en Idioma del juego.","Português (Brasil) installed and available under Game language.","Português (Brasil) instalado e disponível em Idioma do jogo."));
    if(sInstallTexturePack)ImGui::TextWrapped("%s",L("El paquete HD está en la carpeta textures.","The HD pack is in the textures folder.","O pacote HD está na pasta textures."));
    bottom_row(2);
    if(big_button(L("Volver","Back","Voltar"),false))sPage=PAGE_PICK;
    ImGui::SameLine();if(big_button(L("Jugar","Play","Jogar"),true))play_game();
}

static const char *error_text(){
    if(sError=="GITHUB_HELPER_MISSING")return L("Falta un archivo del instalador. Extraé el ZIP completo en una carpeta y abrí BT4decomp-setup.exe.","An installer file is missing. Extract the entire ZIP into a folder and open BT4decomp-setup.exe.","Falta um arquivo do instalador. Extraia o ZIP completo em uma pasta e abra BT4decomp-setup.exe.");
    if(sError=="LANGUAGE_ISO_MISMATCH")return L("La ISO no corresponde al idioma elegido. Elegí la ISO latina o inglesa correcta.","The ISO does not match the selected language. Choose the matching Spanish or English ISO.","A ISO não corresponde ao idioma escolhido. Selecione a ISO correta em espanhol ou inglês.");
    if(sError=="PUBLIC_DOWNLOAD_TOOL_MISSING")return L("No se pudo iniciar la descarga segura. Revisá la instalación de curl o elegí un paquete local.","The secure download could not start. Check that curl is installed or choose a local package.","Não foi possível iniciar o download seguro. Confira se curl está instalado ou selecione um pacote local.");
    if(sError.rfind("PUBLIC_DOWNLOAD_FAILED",0)==0)return L("No se pudo descargar el paquete. Revisá la conexión o elegí un archivo local.","The package could not be downloaded. Check your connection or choose a local file.","Não foi possível baixar o pacote. Confira a conexão ou selecione um arquivo local.");
    if(sError=="ISO_REQUIRED")return L("Elegí una ISO de BT4 del idioma que querés instalar.","Choose a BT4 ISO in the language you want to install.","Selecione uma ISO de BT4 no idioma que deseja instalar.");
    if(sError=="INSTALL_CANCELLED")return L("Instalación cancelada.","Installation cancelled.","Instalação cancelada.");
    if(sError=="LANGUAGE_VERIFY_FAILED")return L("La descarga no se pudo verificar. Volvé a intentarlo.","The download could not be verified. Try again.","Não foi possível verificar o download. Tente novamente.");
    if(sError=="PTBR_REQUIRES_ENGLISH")return L("El paquete portugués requiere los datos ingleses de BT4.","The Portuguese pack requires the English BT4 data.","O pacote em português requer os dados de BT4 em inglês.");
    if(sError=="PTBR_ARCHIVE_INVALID")return L("El ZIP de Português (Brasil) está incompleto o no es compatible.","The Português (Brasil) ZIP is incomplete or unsupported.","O ZIP de Português (Brasil) está incompleto ou não é compatível.");
    if(sError=="TEXTURE_PACK_INVALID")return L("La descarga del paquete HD está incompleta o dañada. Volvé a intentarlo.","The HD texture pack download is incomplete or damaged. Try again.","O download do pacote HD está incompleto ou corrompido. Tente novamente.");
    return L("No se pudo completar la instalación. Revisá el archivo ISO, la conexión y el espacio disponible.","Installation could not complete. Check the ISO, your connection and available disk space.","Não foi possível concluir a instalação. Confira a ISO, a conexão e o espaço disponível no disco.");
}
static void page_failed() {
    heading(L("Instalación detenida","Installation stopped","Instalação interrompida"),error_text());
    bottom_row(3);if(big_button(L("Ver registro","View log","Ver registro"),false))SDL_OpenURL(("file://"+sRoot+"tbo/setup/install.log").c_str());
    ImGui::SameLine();if(big_button(sTextureMaintenance?L("Reintentar","Retry","Tentar novamente"):L("Volver","Back","Voltar"),true)){
        if(sTextureMaintenance)start_texture_update();else sPage=PAGE_PICK;
    }
    ImGui::SameLine();if(big_button(sTextureMaintenance?L("Jugar sin actualizar","Play without updating","Jogar sem atualizar"):L("Cerrar","Close","Fechar"),false)){
        if(sTextureMaintenance){sLauncher=sRoot+(file_exists((sRoot+GAME ".exe").c_str())?GAME ".exe":GAME);play_game();}else sQuit=true;
    }
}

static void build_ui() {
    ImGuiIO &io = ImGui::GetIO();
    int drawn = sPage; // (a button can change the page while it is drawn: its words are this one's)
    sPageWords.clear();
    sProblem.clear();
    UiA11y_Frame(1, 0);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    // (no name before "###": the screen reader says the page's heading, not the window's name)
    ImGui::Begin("###setup", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    static int navPage = -1;
    if (sPage != navPage) { // the keyboard's focus to the new page's first control (it was on one of the page before: Tab
        navPage = sPage;    // and the arrows then went nowhere)
        ImGui::NavInitWindow(ImGui::GetCurrentWindow(), true);
    }
    switch (sPage) {
    case PAGE_PICK: page_pick(); break;
    case PAGE_RUN: page_run(); break;
    case PAGE_DONE: page_done(); break;
    case PAGE_FAILED: page_failed(); break;
    }
    ImGui::End();
    UiA11y_End();
    // a page that came up: all its words, interrupting; then what happened (queued), then a new problem under a field
    // once the field is left (not while a path is typed into it)
    if (drawn != sSaidPage && drawn == sPage) {
        sSaidPage = drawn;
        sSaidProblem.clear();
        PortA11y_Say(sPageWords.c_str(), A11Y_INTERRUPT);
        UiA11y_QueueNext(); // (the control the focus comes to is said after the page, not over it)
        if (drawn != PAGE_RUN) {
            sNews.clear(); // (the last step's end: "Ready to play" or "Setup stopped" says how it went)
        }
    }
    if (!sNews.empty()) {
        PortA11y_Say(sNews.c_str(), 0);
        sNews.clear();
    }
    if (sProblem != sSaidProblem && !ImGui::IsAnyItemActive()) {
        sSaidProblem = sProblem;
        PortA11y_Say(sProblem.c_str(), 0);
    }
}

// ---------------------------------------------------------------------------------------------------- the window

// BT3_SETUP_SHOT: the picture drawn a second time into a texture and written as a PPM file.
static void shot(ImDrawData *dd, Uint32 w, Uint32 h, int number) {
    SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(sDevice, sWindow);
    bool bgr = fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTransferBufferCreateInfo ti;
    SDL_GPUColorTargetInfo target;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    char name[512];

    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = fmt;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    SDL_GPUTexture *tex = SDL_CreateGPUTexture(sDevice, &ci);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(target);
    target.texture = tex;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
    SDL_zero(ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ti.size = w * h * 4;
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(sDevice, &ti);
    SDL_zero(src);
    src.texture = tex;
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_zero(dst);
    dst.transfer_buffer = tb;
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(cp, &src, &dst);
    SDL_EndGPUCopyPass(cp);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(sDevice, true, &fence, 1);
    SDL_ReleaseGPUFence(sDevice, fence);
    const Uint8 *px = (const Uint8 *)SDL_MapGPUTransferBuffer(sDevice, tb, false);
    snprintf(name, sizeof(name), "%s_%d.ppm", getenv("BT3_SETUP_SHOT"), number);
    FILE *fp = fopen(name, "wb");
    if (fp != NULL && px != NULL) {
        fprintf(fp, "P6\n%u %u\n255\n", w, h);
        for (Uint32 i = 0; i < w * h; i++) {
            Uint8 rgb[3] = {px[i * 4 + (bgr ? 2 : 0)], px[i * 4 + 1], px[i * 4 + (bgr ? 0 : 2)]};
            fwrite(rgb, 1, 3, fp);
        }
        fclose(fp);
    }
    SDL_UnmapGPUTransferBuffer(sDevice, tb);
    SDL_ReleaseGPUTransferBuffer(sDevice, tb);
    SDL_ReleaseGPUTexture(sDevice, tex);
}

#ifdef _WIN32
// The few functions of Windows used below, declared here: its headers define names this file has (FAILED, ...).
extern "C" {
__declspec(dllimport) wchar_t *__stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t **__stdcall CommandLineToArgvW(const wchar_t *line, int *count);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned page, unsigned long flags, const wchar_t *wide, int wideLen, char *out, int outLen,
                                                        const char *def, int *usedDef);
__declspec(dllimport) void *__stdcall LocalFree(void *mem);
}

// The program's arguments in UTF-8. Those main() gets on Windows are in the system's code page: a disc image whose
// path has letters outside it arrives with '?' in their place, and one with letters inside it as bytes that are not
// UTF-8, which is what SDL opens files by. Either way the image was "not found". NULL when Windows gives none.
static char **utf8_args(int *count) {
    int n = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &n);
    char **out = wide != NULL ? (char **)SDL_calloc((size_t)n + 1, sizeof(char *)) : NULL;

    for (int i = 0; out != NULL && i < n; i++) {
        int len = WideCharToMultiByte(65001 /* CP_UTF8 */, 0, wide[i], -1, NULL, 0, NULL, NULL);
        out[i] = (char *)SDL_calloc((size_t)(len > 0 ? len : 1), 1);
        if (out[i] == NULL) {
            out = NULL; // (out of memory at start-up: main's own arguments are used)
            break;
        }
        if (len > 0) {
            WideCharToMultiByte(65001, 0, wide[i], -1, out[i], len, NULL, NULL);
        }
    }
    if (wide != NULL) {
        LocalFree(wide);
    }
    *count = n;
    return out;
}
#endif

int main(int argc, char **argv) {
    static const char *fonts[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Geneva.ttf", // (macOS)
    };
    ImGui_ImplSDLGPU3_InitInfo info;
    int shots = 0, shotKey = -1;

#ifdef _WIN32
    {
        int count = 0;
        char **args = utf8_args(&count); // (kept for the whole run)
        if (args != NULL) {
            argc = count;
            argv = args;
        }
    }
#endif
#ifdef __APPLE__
    {
        // Started from Finder, a program gets only /usr/bin:/bin:/usr/sbin:/sbin: install.py would not find Homebrew's
        // tools (glslc, spirv-cross, 7z) and call them missing. Homebrew's folders first, as in a Terminal.
        const char *path = getenv("PATH");
        std::string more = std::string("/opt/homebrew/bin:/usr/local/bin:") + (path != NULL && path[0] != '\0' ? path : "/usr/bin:/bin:/usr/sbin:/sbin");
        setenv("PATH", more.c_str(), 1);
    }
#endif
    if (argc > 2 && strcmp(argv[1], "--install") == 0) {
        // Without a window: BT4decomp-setup --install <disc.iso> [<European disc.iso>] (an empty first one: only
        // the languages, for a folder set up before). A release folder only; prints the event lines.
        std::string line;
        bool ok = false;
        SDL_Init(0);
        sRoot = find_root();
        if (!sRelease) {
            fprintf(stderr, GAME "-setup --install works in a release folder (" GAME " and " GAME ".dat next to it).\nIn a source checkout: python3 install.py --iso <file>\n");
            return 2;
        }
        int mask=0; // headless installation defaults to the selected ISO's language
        for(int i=3;i<argc;i++)if(!strcmp(argv[i],"--language")&&i+1<argc){
            const char *language=argv[++i];
            mask=!strcmp(language,"es")?1:!strcmp(language,"en")?2:!strcmp(language,"both")?3:-1;
        }
        if(mask<0){fprintf(stderr,"--language must be es, en or both.\n");return 2;}
        Native_Start(sRoot, argv[2], "", mask);
        for (bool last = false; !last;) {
            last = !Native_Running(); // one more round after the worker has ended: nothing is left behind
            while (Native_Poll(line)) {
                printf("%s\n", line.c_str());
                fflush(stdout);
                ok = ok || line.rfind("@done", 0) == 0;
            }
            SDL_Delay(50);
        }
        Native_Join();
        return ok ? 0 : 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, GAME "-setup: %s\n", SDL_GetError());
        return 1;
    }
    const char *glsl = "#version 330";
    {
        const char *api = getenv("BT3_GPU_API");
        bool wantGl = api != NULL && (strcmp(api, "gl") == 0 || strcmp(api, "opengl") == 0);
        std::string why;
        if (!wantGl) {
            sWindow = SDL_CreateWindow(GAME " - Setup", 820, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE | (getenv("BT4_RENDER_TEST")?SDL_WINDOW_HIDDEN:0));
            sDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false, NULL);
            if (sWindow == NULL || sDevice == NULL || !SDL_ClaimWindowForGPUDevice(sDevice, sWindow)) {
                why = SDL_GetError();
                if (sDevice != NULL) {
                    SDL_DestroyGPUDevice(sDevice);
                    sDevice = NULL;
                }
                if (sWindow != NULL) {
                    SDL_DestroyWindow(sWindow);
                    sWindow = NULL;
                }
            }
        }
        if (sDevice == NULL) {
            // OpenGL 3.3 core, and failing that whatever 3.0 context the driver gives
            for (int attempt = 0; attempt < 2 && sGl == NULL; attempt++) {
                SDL_GL_ResetAttributes();
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __APPLE__
                // (macOS gives only a 3.2 or later core context, and only forward-compatible: GLSL 150)
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
                glsl = "#version 150";
#else
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, attempt == 0 ? 3 : 0);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, attempt == 0 ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
                glsl = attempt == 0 ? "#version 330" : "#version 130";
#endif
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                sWindow = SDL_CreateWindow(GAME " - Setup", 820, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | (getenv("BT4_RENDER_TEST")?SDL_WINDOW_HIDDEN:0));
                sGl = sWindow != NULL ? SDL_GL_CreateContext(sWindow) : NULL;
                if (sGl == NULL && sWindow != NULL) {
                    SDL_DestroyWindow(sWindow);
                    sWindow = NULL;
                }
            }
            if (sGl == NULL) {
                fprintf(stderr, GAME "-setup: no window: %s%s%s\nThe same setup runs without a window: " GAME "-setup --install <disc image>\n",
                        why.c_str(), why.empty() ? "" : "; OpenGL: ", SDL_GetError());
                return 1;
            }
            if (!wantGl) {
                fprintf(stderr, GAME "-setup: %s; the window uses OpenGL\n", why.c_str());
            }
            SDL_GL_MakeCurrent(sWindow, sGl);
            SDL_GL_SetSwapInterval(1);
            sUseGl = true;
        } else {
            SDL_SetGPUSwapchainParameters(sDevice, sWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
        }
    }
    SDL_SetWindowMinimumSize(sWindow,720,620);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    {SDL_Surface *icon=SDL_LoadPNG((sRoot+"fonts/icon_game.png").c_str());if(icon){SDL_SetWindowIcon(sWindow,icon);SDL_DestroySurface(icon);}}
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    // The keyboard reaches every control: Tab and the arrows move, Space or Enter presses. Without this, Tab stops at
    // the text field only and no button can be pressed without a mouse.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        if (file_exists(fonts[i])) {
            io.Fonts->AddFontFromFileTTF(fonts[i], 18.0f);
            break;
        }
    }
    if (sUseGl) {
        ImGui_ImplSDL3_InitForOpenGL(sWindow, sGl);
        ImGui_ImplOpenGL3_Init(glsl);
    } else {
        ImGui_ImplSDL3_InitForSDLGPU(sWindow);
        info.Device = sDevice;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(sDevice, sWindow);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        ImGui_ImplSDLGPU3_Init(&info);
    }
    sRoot = find_root();
    if(sRelease&&!sRoot.empty()){
        bool hasSpanish=Native_SpanishInstalled(sRoot),hasEnglish=Native_EnglishInstalled(sRoot);
        sInstallMask=hasSpanish&&hasEnglish?3:hasEnglish?2:1;
    }
#ifdef __APPLE__
    if (sRelease) {
        // A folder downloaded from the internet is marked "quarantined" by macOS, and a quarantined program started by
        // another one is refused (posix_spawn: Bad file descriptor), the game included. This setup has been let through
        // already (it runs): the mark is taken off the whole folder, as README.txt's step 0 does by hand.
        const char *args[] = {"/usr/bin/xattr", "-dr", "com.apple.quarantine", sRoot.c_str(), NULL};
        SDL_Process *x = SDL_CreateProcess(args, false);
        if (x != NULL) {
            SDL_WaitProcess(x, true, NULL);
            SDL_DestroyProcess(x);
        }
    }
#endif
    if (false && sRelease && Native_Installed(sRoot) && getenv("BT3_SETUP_ISO") == NULL) { // nothing left to do: offer Play
        if(Native_BundledEnglish(sRoot)&&!Native_EnglishInstalled(sRoot)){
            sIso[0]=0;start_install();
        }else{
        reset_rows();
        for (auto &r : sRows) {
            r.state = SKIPPED;
            r.detail = "already done";
        }
        sLauncher = sRoot + (file_exists((sRoot + GAME ".exe").c_str()) ? GAME ".exe" : GAME);
        sPage = PAGE_DONE;
        }
    }
    if (argc > 1 && !strcmp(argv[1],"--update-textures") && sRelease) {
        sTextureMaintenance=true;
        for(int i=2;i+1<argc;i++)if(!strcmp(argv[i],"--ui-language"))sUiLanguage=std::max(0,std::min(2,atoi(argv[++i])));
        start_texture_update();
    } else if (argc > 1) {
        SDL_strlcpy(sIso, argv[1], sizeof(sIso));
    }
    if (getenv("BT3_SETUP_EU_ISO") != NULL) { // (testing, with BT3_SETUP_ISO: the European image too)
        SDL_strlcpy(sIsoEu, getenv("BT3_SETUP_EU_ISO"), sizeof(sIsoEu));
    }
    if (getenv("BT3_SETUP_ISO") != NULL) {
        SDL_strlcpy(sIso, getenv("BT3_SETUP_ISO"), sizeof(sIso));
        if (!sRoot.empty() && file_exists(sIso)) {
            start_install();
        }
    }
    if(getenv("BT4_RENDER_TEST")){
        if(const char *lang=getenv("BT4_SETUP_UI_LANGUAGE"))
            sUiLanguage=(!strcmp(lang,"pt")||!strcmp(lang,"pt-BR"))?2:!strcmp(lang,"en")?1:0;
        if(getenv("BT4_SETUP_UI_PTBR"))sInstallPtbrPack=atoi(getenv("BT4_SETUP_UI_PTBR"))!=0;
        if(sInstallPtbrPack)sInstallMask=2;
        if(getenv("BT4_SETUP_UI_MASK"))sInstallMask=atoi(getenv("BT4_SETUP_UI_MASK"));
        if(getenv("BT4_SETUP_PROGRESS_PREVIEW")){
            // Render-only fixture: feed the same events as the worker, never
            // extract or launch a game and never show a desktop window.
            sRelease=true;reset_rows();sPage=PAGE_RUN;sRunStart=SDL_GetTicks();
            event("@step 1 4 Disc image");event("@ok verified");
            event("@step 2 4 Game data");event("@activity extract");
            unsigned pct=(unsigned)std::max(0,std::min(100,atoi(getenv("BT4_SETUP_PROGRESS_PREVIEW"))));
            event("@progress "+std::to_string(pct*40000000ULL)+" 4000000000 "+std::to_string(pct*250)+" 25000");
        }
    }
    while (!sQuit) {
        SDL_Event ev;
        PortA11y_Begin(); // (the first call starts the speaker, when prism.dll is there)
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            PortA11y_Event(&ev); // (F2 says the last words again)
            if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                sQuit = true;
            } else if (ev.type == SDL_EVENT_DROP_FILE && sPage == PAGE_PICK && ev.drop.data != NULL) {
                // (into the USA image's field, unless that one is filled already: then into the European one's)
                if (file_exists(sIso) && strcmp(sIso, ev.drop.data) != 0) {
                    SDL_strlcpy(sIsoEu, ev.drop.data, sizeof(sIsoEu));
                    news("European disc image: " + file_name(ev.drop.data));
                } else {
                    SDL_strlcpy(sIso, ev.drop.data, sizeof(sIso));
                    news("Disc image: " + file_name(ev.drop.data));
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(sDialogLock);
            if (sDialogReady) {
                const int selected=sDialogFor;
                if(selected==0)SDL_strlcpy(sIso,sDialogResult.c_str(),sizeof(sIso));
                else if(selected==1)SDL_strlcpy(sIsoEu,sDialogResult.c_str(),sizeof(sIsoEu));
                sDialogReady = false;
                news(file_name(sDialogResult.c_str()));
                if (selected==1 && sPage == PAGE_DONE && sRelease && !Native_Running()) { // "Add languages...": only that step
                    sEuOnly = true;
                    start_install();
                }
            }
        }
        pump_install();
        if (sUseGl) {
            ImGui_ImplOpenGL3_NewFrame();
        } else {
            ImGui_ImplSDLGPU3_NewFrame();
        }
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        build_ui();
        ImGui::Render();
        ImDrawData *dd = ImGui::GetDrawData();
        if (sUseGl) {
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(sWindow, &pw, &ph);
            glViewport(0, 0, pw, ph);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(dd);
            if (getenv("BT3_SETUP_SHOT") != NULL && ImGui::GetFrameCount() == 20 && pw > 0 && ph > 0) {
                // testing: the picture of the OpenGL window, <BT3_SETUP_SHOT>_gl.ppm (rows come bottom first)
                std::vector<unsigned char> px((size_t)pw * ph * 4);
                char name[512];
                glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                snprintf(name, sizeof(name), "%s_gl.ppm", getenv("BT3_SETUP_SHOT"));
                FILE *fp = fopen(name, "wb");
                if (fp != NULL) {
                    fprintf(fp, "P6\n%d %d\n255\n", pw, ph);
                    for (int y = ph - 1; y >= 0; y--) {
                        for (int x = 0; x < pw; x++) {
                            fwrite(&px[((size_t)y * pw + x) * 4], 1, 3, fp);
                        }
                    }
                    fclose(fp);
                }
            }
            SDL_GL_SwapWindow(sWindow);
            if (getenv("BT3_SETUP_QUIT") != NULL && ImGui::GetFrameCount() > 30) {
                sQuit = true; // testing: a few frames of the OpenGL window, then leave
            }
            continue;
        }
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sDevice);
        SDL_GPUTexture *swap = NULL;
        Uint32 w = 0, h = 0;
        if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sWindow, &swap, &w, &h) && swap != NULL) {
            SDL_GPUColorTargetInfo target;
            ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
            SDL_zero(target);
            target.texture = swap;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
            ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
            SDL_EndGPURenderPass(pass);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
        if(getenv("BT4_RENDER_TEST")&&getenv("BT3_SETUP_SHOT")&&!w){int tw,th;SDL_GetWindowSizeInPixels(sWindow,&tw,&th);w=tw;h=th;}
        if (getenv("BT3_SETUP_SHOT") != NULL && w != 0 && ImGui::GetFrameCount() > 3) {
            int running = 0;
            for (size_t i = 0; i < sRows.size(); i++) {
                running += sRows[i].state != PENDING;
            }
            int key = (int)sPage * 100 + running;
            if (key != shotKey) {
                shotKey = key;
                shot(dd, w, h, shots++);
            }
            if (getenv("BT3_SETUP_QUIT") != NULL && (sPage == PAGE_DONE || sPage == PAGE_FAILED || (sProc == NULL && !sNative && shots > 0 && getenv("BT3_SETUP_ISO") == NULL))) {
                shot(dd, w, h, shots++);
                sQuit = true; // testing: leave once the result is on screen
            }
        }
    }
    if (sProc != NULL) {
        SDL_KillProcess(sProc, false);
        SDL_DestroyProcess(sProc);
    }
    Native_Join();
    if (sUseGl) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(sGl);
        SDL_DestroyWindow(sWindow);
        SDL_Quit();
        return 0;
    }
    SDL_WaitForGPUIdle(sDevice);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
    SDL_ReleaseWindowFromGPUDevice(sDevice, sWindow);
    SDL_DestroyGPUDevice(sDevice);
    SDL_DestroyWindow(sWindow);
    SDL_Quit();
    return 0;
}
