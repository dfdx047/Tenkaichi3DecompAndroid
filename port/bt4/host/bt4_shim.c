/*
 * Budokai Tenkaichi 4 (port/bt4): what Dragon Rage's GS (port/src/gs: gs_core, gs_draw, gs_gpu, gs_gl, gs_input...)
 * asks of the rest of the Budokai Tenkaichi 3 port, answered for the BT4 engine. The renderer is the same code as
 * BT3's, unchanged; the game here is the mod recompiled (BT3-Recomp), which drives the GS through its live stream
 * (bt4_main.cpp), so the BT3 port's menus text, online, pacing and game hooks have nothing to do.
 */
#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Bt4_Quit(void);                         /* bt4_main.cpp */
extern void Bt4_OpenMenu(void);
extern void Bt4_UiInit(const char *gpu, int gl);

/* ---- the BT3 port's variables the renderer reads */
int gGsMainFbp = -1;               /* the shown frame buffer: DISPFB1 from the live stream (bt4_main.cpp) */
int gGsOtherFbp = -1;              /* the buffer before it, where the game is drawing now (bt4_main.cpp) */
unsigned gPortVBlanks;             /* vertical blanks: one per frame of the stream */
int gPortInMenus;                  /* (BT3: the menus are running; aspect and pacing choices) */
int gPortMenuMode;
int gPortResim;                    /* (BT3 online: a frame run again) */
unsigned long long gPortSleptNs;
volatile uint32_t gPortDataStripped = 1;
volatile int gPortNetWindowClose;
int gPortMusicPercent = 100, gPortSePercent = 100, gPortVoicePercent = 100, gPortStagePercent = 100;
unsigned char gPad[0x400];         /* (BT3: the game's pad block; only a test of gs_input.c reads it) */
unsigned long long gPortNetWaitNs; /* (BT3 online: the time a frame waited for the other player; gs_core.c's report) */
unsigned char gNativePad[0x400];   /* (BT3: the game's pad block; only gs_input.c's BT3_STICK_TEST reads it) */

/* ---- the Tag Team mod and the spectators of BT3's online, which gs_input.c asks about: no such thing here */
int Port_NetSpectating(void) { return 0; }
int TagNet_Enabled(void) { return 0; }
int TagNet_Role(void) { return 0; }
int TagTeam_ViewActor(int player) { return player; }
int TagTeam_Active(void) { return 0; }
volatile int gPortInBackground;      /* (BT3: net_match.c, which holds its requests while the app is not on the screen; gs_gpu.c sets it) */
void Port_TagTestCheck(int ok, int where) { (void)ok; (void)where; }

/* ---- pacing: the recompiled game paces itself */
unsigned long long Port_BlankNs(void) { return 16683350ull; }
int Port_VBlankUsLeft(void) { return 0; }
int Port_PaceBehindUs(void) { return 0; }
void Port_LagMark(int what) { (void)what; }
int Port_LateInput(void) { return 0; }
int Port_DataState(void) { return 0; }
void Port_AudioRefresh(void) {}

/* ---- the picture's shape. The mod draws 16:9 squeezed into the PlayStation 2's 4:3 frame (the community widescreen
   values are built into its program: the 16:9 FOV, BT3-Recomp's bt4mod-widescreen notes), as Team BT4's PCSX2
   profile shows it. So the frame is shown stretched to 16:9 by default; "bt4_aspect" (bt3_settings.txt):
   0 16:9, 1 4:3 (the frame as it is, people look thinner), 2 the whole screen (stretched further). Only the
   presentation changes: the game draws the same picture. */
static int sAspect = 0;
static int screen_milli(void) {
    int count = 0, milli = 1778;
    SDL_DisplayID *d = SDL_GetDisplays(&count);
    SDL_Rect r;
    if (d != NULL && count > 0 && SDL_GetDisplayBounds(d[0], &r) && r.w > 0 && r.h > 0) {
        int big = r.w > r.h ? r.w : r.h, small = r.w > r.h ? r.h : r.w;
        milli = big * 1000 / small;
    }
    SDL_free(d);
    return milli;
}
int Bt4_AspectMode(void) { return Port_Setting("bt4_aspect", 0); }
void Bt4_AspectSet(int mode) {
    sAspect = mode == 1 ? 1333 : mode == 2 ? screen_milli() : 1778;
}
int Port_AspectMilli(void) {
    if (sAspect == 0) {
        Bt4_AspectSet(Bt4_AspectMode());
    }
    return sAspect;
}
void Port_SetAspectMilli(int milli) { (void)milli; }
int Port_IsWide(void) { return 0; } /* (BT3's native widening of the scene and its 2D: the mod does its own) */
int Port_PageAspectMilli(void) { return Port_AspectMilli(); }

/* ---- online (the BT3 engine's): none here */
int Port_NetActive(void) { return 0; }
int Port_NetMe(void) { return 0; }
int Port_NetSession(void) { return 0; }

/* ---- the BT3 port's menu text replacement (vector fonts over the disc's glyphs): off */
int MenuText_FontId(int choice) { (void)choice; return 0; }
int MenuText_FontIdToSave(void) { return 0; }
int MenuText_BattleFontIdToSave(void) { return 0; }
int MenuText_GetFont(void) { return 0; }
int MenuText_GetBattleFont(void) { return 0; }
int MenuText_GetSharedFont(void) { return 0; }
void MenuText_SetFont(int choice) { (void)choice; }
void MenuText_SetFontId(int id) { (void)id; }
void MenuText_SetBattleFont(int choice) { (void)choice; }
void MenuText_SetBattleFontId(int id) { (void)id; }
void MenuText_SetSharedFont(int choice) { (void)choice; }
void MenuText_SetSharedFontId(int id) { (void)id; }
void MenuText_Poll(void) {}
int MenuText_RasterScale(int internalScale) { return internalScale; }
int MenuText_MayHaveBattleTexture(int width, int height, int indexed) { (void)width; (void)height; (void)indexed; return 0; }
int MenuText_IsBattleTexture(const uint8_t *rgba, int width, int height, int indexed) { (void)rgba; (void)width; (void)height; (void)indexed; return 0; }
uint8_t *MenuText_Render(const uint8_t *rgba, int width, int height, int indexed, int scale, int *outWidth, int *outHeight) {
    (void)rgba; (void)width; (void)height; (void)indexed; (void)scale; (void)outWidth; (void)outHeight;
    return NULL;
}
uint8_t *MenuText_RenderGlyphs(const uint8_t *rgba, int width, int height, int tbp, int cbp, int scale, int *outWidth, int *outHeight) {
    (void)rgba; (void)width; (void)height; (void)tbp; (void)cbp; (void)scale; (void)outWidth; (void)outHeight;
    return NULL;
}
int MenuText_MapGlyph(int tbp, int cbp, int width, int height, float u0, float v0, float u1, float v1, float *rect) {
    (void)tbp; (void)cbp; (void)width; (void)height; (void)u0; (void)v0; (void)u1; (void)v1; (void)rect;
    return 0;
}
uint32_t MenuText_TexKey(int width, int height, int indexed, int tbp, int cbp, int scale) {
    (void)width; (void)height; (void)indexed; (void)tbp; (void)cbp; (void)scale;
    return 0;
}
unsigned MenuLabels_ArtKey(void) { return 0; }
int MenuLabels_LanguageArtPixels(int id, unsigned char **pixels, int *w, int *h) { (void)id; (void)pixels; (void)w; (void)h; return 0; }
void MenuText_NewTexture(void) {}
void Port_LagNote(int what, unsigned long long value) { (void)what; (void)value; }
void Port_UiNotice(const char *text) { fprintf(stderr, "bt4: notice: %s\n", text); } /* (BT3: a line on screen; here the log) */
int MenuLabels_ArtSwap(unsigned char *rgba, int w, int h) { (void)rgba; (void)w; (void)h; return 0; }
int MenuTranslation_ArtPath(int artId, char *out, size_t capacity) { (void)artId; (void)out; (void)capacity; return 0; }
uint32_t *TboText_Pixels(uint32_t id, uint32_t *version, uint32_t *w, uint32_t *h) { (void)id; (void)version; (void)w; (void)h; return NULL; }
uint32_t TboText_Version(uint32_t id) { (void)id; return 0; }

/* ---- BT3 game hooks of the renderer and the pad code: no BT3 game here */
void ScrXfade_RequestCapture(void) {}
void ScrXfade_Start(int32_t request, float seconds) { (void)request; (void)seconds; }
void *BtlChar_FindByObjId(int objId) { (void)objId; return NULL; }
int BtlAct_GetCurrent(void *chr) { (void)chr; return 0; }
void PortA11y_Event(const SDL_Event *ev) { (void)ev; }

/* ---- the BT3 port's settings window (ImGui): not in this engine. The back key and START + SELECT open the app's
   touch menu (GameMenu.kt; its side here: bt4_main.cpp), which pauses the game. */
int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context, const char *gpu_name) {
    (void)window; (void)device;
    Bt4_UiInit(gpu_name, gl_context != NULL);
    return 1;
}
void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) { (void)cmd; (void)target; }
void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) { (void)cmd; (void)target; }
void Ui_DrawGL(void) {}
int Ui_Event(const SDL_Event *ev) { (void)ev; return 0; }
int Ui_Pauses(void) { return 0; }
int Ui_IsOpen(void) { return 0; }
void Ui_Toggle(void) { Bt4_OpenMenu(); }
