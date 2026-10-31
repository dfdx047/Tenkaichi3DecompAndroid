/* What BT3's renderer (gs/hw/bt3: a copy of Tenkaichi3DecompAndroid's port/src/gs) asks of the BT3 port around it,
 * answered for Budokai Tenkaichi 2. BT3 itself is not touched: these are BT2's own answers.
 *
 *   settings        Port_Setting / Port_SettingSave: "name = value" lines in BT2's own file (BT2_HW_SETTINGS:
 *                   <root>/bt2/user/renderer), written by the Dragon Rage app's BT2 tab and in-game menu
 *   pace            the vertical blanks' grid the renderer's frame skipping reads (Port_PaceBehindUs, as BT3's
 *                   plat_stub.c): BT2's run marks each field (Bt2Hw_Field), so "behind" is how late the coming
 *                   field is
 *   the rest        BT3's menus, fonts, texture packs, settings window, online play and VU1 (BT2's runtime runs
 *                   its own VU1 and hands the renderer finished GIF packets): nothing to do here
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL3/SDL.h>
#include "bt3/gs_texpack.h"
#include "bt3/ui.h"
#include "bt3/gs_internal.h"
#include "bt3/tbo_art.h"

/* ---- settings */

#define MAX_SETTINGS 64
static struct { char name[40]; int value; } sSettings[MAX_SETTINGS];
static int sSettingCount = -1;
static pthread_mutex_t sSettingLock = PTHREAD_MUTEX_INITIALIZER;

static void settings_load(void) {
    const char *path = getenv("BT2_HW_SETTINGS");
    FILE *f = path != NULL ? fopen(path, "r") : NULL;
    char line[128], name[40];
    int value;

    sSettingCount = 0;
    if (f == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL && sSettingCount < MAX_SETTINGS) {
        if (sscanf(line, "%39s = %d", name, &value) == 2) {
            strcpy(sSettings[sSettingCount].name, name);
            sSettings[sSettingCount++].value = value;
        }
    }
    fclose(f);
}

int Port_Setting(const char *name, int def) {
    int v = def;
    pthread_mutex_lock(&sSettingLock);
    if (sSettingCount < 0) {
        settings_load();
    }
    for (int i = 0; i < sSettingCount; i++) {
        if (strcmp(sSettings[i].name, name) == 0) {
            v = sSettings[i].value;
            break;
        }
    }
    pthread_mutex_unlock(&sSettingLock);
    return v;
}

void Port_SettingsWrite(void) {
    const char *path = getenv("BT2_HW_SETTINGS");
    FILE *f = path != NULL ? fopen(path, "w") : NULL;
    if (f == NULL) {
        return;
    }
    pthread_mutex_lock(&sSettingLock);
    for (int i = 0; i < sSettingCount; i++) {
        fprintf(f, "%s = %d\n", sSettings[i].name, sSettings[i].value);
    }
    pthread_mutex_unlock(&sSettingLock);
    fclose(f);
}

void Port_SettingSave(const char *name, int value) {
    int i;
    pthread_mutex_lock(&sSettingLock);
    if (sSettingCount < 0) {
        settings_load();
    }
    for (i = 0; i < sSettingCount && strcmp(sSettings[i].name, name) != 0; i++) {
    }
    if (i == sSettingCount && sSettingCount < MAX_SETTINGS) {
        snprintf(sSettings[sSettingCount++].name, sizeof(sSettings[0].name), "%s", name);
    }
    if (i < MAX_SETTINGS) {
        sSettings[i].value = value;
    }
    pthread_mutex_unlock(&sSettingLock);
    Port_SettingsWrite();
}

/* The in-game menu changed the file: read again (Bt2Hw_ReloadSettings, any thread). */
void Bt2Hw_ReloadSettings(void) {
    pthread_mutex_lock(&sSettingLock);
    settings_load();
    pthread_mutex_unlock(&sSettingLock);
}


/* ---- the vertical blanks' grid (BT3: plat_stub.c) */

#define BLANK_NS 16683333ull /* NTSC: 1001 / 60000 s */
static unsigned long long sGridStart; /* when field 0 of the grid was due */
static unsigned long long sGridFields;
unsigned gPortVBlanks;
unsigned long long gPortSleptNs;

static unsigned long long now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000000000ull + (unsigned long long)t.tv_nsec;
}

/* BT2's run, at the start of each field the game is given: the grid moves one blank on. A run that falls more
   than a tenth of a second behind (a load, the menu's pause) starts the grid again from now, as BT3's does. */
void Bt2Hw_Field(void) {
    unsigned long long t = now_ns();
    gPortVBlanks++;
    sGridFields++;
    if (sGridStart == 0 || t > sGridStart + sGridFields * BLANK_NS + 100000000ull) {
        sGridStart = t;
        sGridFields = 0;
    }
}

static unsigned long long next_due(void) { return sGridStart != 0 ? sGridStart + (sGridFields + 1) * BLANK_NS : 0; }

unsigned long long Port_BlankNs(void) { return BLANK_NS; }

/* How late the game is: BT2's frame pacer measures it at each flip (Bt2Hw_SetLateUs), as BT3's grid does. */
static int sLateUs;
void Bt2Hw_SetLateUs(int us) { sLateUs = us < 100000 ? us : 0; }
int Port_PaceBehindUs(void) { return sLateUs; }

int Port_VBlankUsLeft(void) {
    unsigned long long t = now_ns(), next = next_due();
    return next > t && next - t < 100000000ull ? (int)((next - t) / 1000ull) : 0;
}

/* ---- the picture's shape: BT2's own widescreen patch (BT2_GAME_PATCHES "widescreen") draws a 16:9 scene into the
   PS2's picture, which is then shown 16:9 wide; otherwise 4:3. BT3's wide HUD placement (Port_IsWide) is for BT3's
   projection, which BT2's patch does its own way: it stays off. */
static int wide(void) {
    static int w = -1;
    if (w < 0) {
        const char *patches = getenv("BT2_GAME_PATCHES");
        w = patches != NULL && strstr(patches, "widescreen") != NULL;
    }
    return w;
}
int Port_AspectMilli(void) { return wide() ? 1778 : 1333; }
int Port_PageAspectMilli(void) { return wide() ? 1778 : 1333; }
int Port_IsWide(void) { return 0; }
void Port_SetAspectMilli(int milli) { (void)milli; }

/* ---- state of the BT3 port that BT2 does not have */
int gGsMainFbp = -1; /* (no sceGsSwapDBuff hook: the renderer shows the target that received most pixels) */
volatile uint32_t gPortDataStripped = 1;
int gPortInMenus, gPortMenuMode, gPortResim, gPortOverlayOpen;
volatile int gPortNetWindowClose;
int gPortMusicPercent = 100, gPortSePercent = 100, gPortVoicePercent = 100, gPortStagePercent = 100;
int Port_DataState(void) { return 0; }
int Port_LateInput(void) { return 0; }
int Port_NetSession(void) { return 0; }
void Port_LagMark(int what) { (void)what; }
void Port_AudioRefresh(void) {}
void ScrXfade_RequestCapture(void) {}
void ScrXfade_Start(int request, uint32_t seconds) { (void)request; (void)seconds; }
void PortA11y_Event(const SDL_Event *ev) { (void)ev; }
int PortInput_ComboPoll(void) { return 0; }
int PortInput_Hotkey(const SDL_Event *ev) { (void)ev; return 0; }

/* BT3's settings window (gs/ui.cpp): BT2 has the app's own menu over the game. */
int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context, const char *gpu_name) {
    (void)window; (void)device; (void)gl_context; (void)gpu_name;
    return 0;
}
void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) { (void)cmd; (void)target; }
void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) { (void)cmd; (void)target; }
void Ui_DrawGL(void) {}
int Ui_Event(const SDL_Event *ev) { (void)ev; return 0; }
int Ui_Pauses(void) { return 0; }
void Ui_Toggle(void) {}

/* BT3's menu fonts, labels, translations and online pictures. */
int MenuText_GetFont(void) { return 0; }
void MenuText_SetFont(int choice) { (void)choice; }
int MenuText_FontId(int choice) { (void)choice; return 0; }
void MenuText_SetFontId(int id) { (void)id; }
int MenuText_FontIdToSave(void) { return 0; }
int MenuText_GetBattleFont(void) { return 0; }
void MenuText_SetBattleFont(int choice) { (void)choice; }
void MenuText_SetBattleFontId(int id) { (void)id; }
int MenuText_BattleFontIdToSave(void) { return 0; }
int MenuText_GetSharedFont(void) { return 0; }
void MenuText_SetSharedFont(int choice) { (void)choice; }
void MenuText_SetSharedFontId(int id) { (void)id; }
void MenuText_Poll(void) {}
int MenuText_RasterScale(int internalScale) { return internalScale; }
uint8_t *MenuText_Render(const uint8_t *rgba, int width, int height, int indexed, int scale, int *outWidth, int *outHeight) {
    (void)rgba; (void)width; (void)height; (void)indexed; (void)scale; (void)outWidth; (void)outHeight;
    return NULL;
}
uint8_t *MenuText_RenderGlyphs(const uint8_t *rgba, int width, int height, int tbp, int cbp, int scale, int *outWidth, int *outHeight) {
    (void)rgba; (void)width; (void)height; (void)tbp; (void)cbp; (void)scale; (void)outWidth; (void)outHeight;
    return NULL;
}
int MenuText_IsBattleTexture(const uint8_t *rgba, int width, int height, int indexed) {
    (void)rgba; (void)width; (void)height; (void)indexed;
    return 0;
}
int MenuText_MayHaveBattleTexture(int width, int height, int indexed) { (void)width; (void)height; (void)indexed; return 0; }
uint32_t MenuText_TexKey(int width, int height, int indexed, int tbp, int cbp, int scale) {
    (void)width; (void)height; (void)indexed; (void)tbp; (void)cbp; (void)scale;
    return 0;
}
int MenuText_MapGlyph(int tbp, int cbp, int width, int height, float u0, float v0, float u1, float v1, float *rect) {
    (void)tbp; (void)cbp; (void)width; (void)height; (void)u0; (void)v0; (void)u1; (void)v1; (void)rect;
    return 0;
}
unsigned MenuLabels_ArtKey(void) { return 0; }
/* The main menu's eleventh item, Online Mode (game/main_menu_item.cpp): its name and plate in the sheets' free rows. */
extern int Bt2MenuItem_ArtSwap(unsigned char *rgba, int w, int h);
int MenuLabels_ArtSwap(unsigned char *rgba, int w, int h) { return Bt2MenuItem_ArtSwap(rgba, w, h); }
int MenuTranslation_ArtPath(int artId, char *out, size_t capacity) { (void)artId; (void)out; (void)capacity; return 0; }
/* The pictures BT2's own screens draw (game/main_menu_item.cpp: Online Mode), through the renderer's lines of letters
   (TBO_TEXT_BASE and on): RGBA with the GS's alpha, malloc'ed for the renderer to take. */
extern uint32_t *Bt2MenuItem_Pixels(uint32_t slot, uint32_t *version, uint32_t *w, uint32_t *h);
extern uint32_t Bt2MenuItem_Version(uint32_t slot);
uint32_t *TboText_Pixels(uint32_t id, uint32_t *version, uint32_t *w, uint32_t *h) {
    return Bt2MenuItem_Pixels(id - TBO_TEXT_BASE, version, w, h);
}
uint32_t TboText_Version(uint32_t id) { return Bt2MenuItem_Version(id - TBO_TEXT_BASE); }

/* One of those pictures drawn over the frame about to be shown (at the flip, after the game's own drawing): a sprite
   into the frame buffer `fbp` (FRAME's units, `fbw` x 64 wide, format `psm`), from (x0, y0) to (x1, y1) in its pixels,
   blended by the picture's alpha times `alpha` / 0x80. The GS's registers are given back as the game left them. */
uint64_t Bt2Hw_LastFrame(void) { return gGs.frame[0]; } /* FRAME_1 as the game left it */
void Bt2Hw_OverlaySprite(int fbp, int fbw, int psm, int width, int height, uint32_t slot, float x0, float y0, float x1, float y1, int alpha) {
    GsState kept = gGs;
    const uint64_t tex0 = (uint64_t)1 << 14 | (uint64_t)TBO_PSM << 20 | (uint64_t)10 << 26 | (uint64_t)10 << 30 | (uint64_t)1 << 34 |
                          (uint64_t)(TBO_TEXT_BASE + slot) << 37;
    union { float f; uint32_t u; } s0 = {0.0f}, s1 = {1.0f}, q = {1.0f};
    Gs_RegWrite(0x4C, (uint64_t)fbp | (uint64_t)fbw << 16 | (uint64_t)psm << 24); /* FRAME_1 */
    /* ZBUF_1: the game's depth buffer, not written (a ZBP equal to the FBP is a pass the renderer drops: gs_draw.c) */
    Gs_RegWrite(0x4E, kept.zbuf[0] | (uint64_t)1 << 32);
    Gs_RegWrite(0x47, (uint64_t)1 << 16 | (uint64_t)1 << 17);                          /* TEST_1: depth always passes */
    Gs_RegWrite(0x42, 0x44);                                                           /* ALPHA_1: (Cs - Cd) * As + Cd */
    Gs_RegWrite(0x4A, 0);                                                              /* FBA_1 */
    Gs_RegWrite(0x18, 0);                                                              /* XYOFFSET_1 */
    Gs_RegWrite(0x40, (uint64_t)(width - 1) << 16 | (uint64_t)(height - 1) << 48);     /* SCISSOR_1 */
    Gs_RegWrite(0x08, 0);                                                              /* CLAMP_1 */
    Gs_RegWrite(0x14, (uint64_t)1 << 5 | (uint64_t)1 << 6);                            /* TEX1_1: filtered */
    Gs_RegWrite(0x06, tex0);                                                           /* TEX0_1 */
    Gs_RegWrite(0x1A, 1);                                                              /* PRMODECONT: PRIM's own */
    Gs_RegWrite(0x00, 6 | 1 << 4 | 1 << 6);                                            /* PRIM: sprite, textured, blended */
    Gs_RegWrite(0x01, 0x808080u | (uint64_t)(alpha & 0xFF) << 24 | (uint64_t)q.u << 32);     /* RGBAQ: alpha 0x80 = the picture's own */
    Gs_RegWrite(0x02, s0.u | (uint64_t)s0.u << 32);                                    /* ST */
    Gs_RegWrite(0x05, (uint64_t)(uint32_t)(x0 * 16.0f) | (uint64_t)(uint32_t)(y0 * 16.0f) << 16);
    Gs_RegWrite(0x02, s1.u | (uint64_t)s1.u << 32);
    Gs_RegWrite(0x05, (uint64_t)(uint32_t)(x1 * 16.0f) | (uint64_t)(uint32_t)(y1 * 16.0f) << 16);
    gGs = kept;
}

/* BT3's texture packs (none for BT2 here: BT2 has its own, gs/textures, for paraLLEl-GS). */
void TexPack_Init(void) {}
int TexPack_Count(void) { return 0; }
const char *TexPack_Lookup(uint32_t tbp, uint32_t tbw, uint32_t psm, uint32_t tw, uint32_t th, uint32_t tcc, uint32_t cbp, uint32_t cpsm,
                           uint32_t *maxAlpha) {
    (void)tbp; (void)tbw; (void)psm; (void)tw; (void)th; (void)tcc; (void)cbp; (void)cpsm; (void)maxAlpha;
    return NULL;
}
int TexPack_Load(const char *path, TexPackImage *img) { (void)path; (void)img; return 0; }
void TexPack_Free(TexPackImage *img) { (void)img; }
int TexPack_Decompress(TexPackImage *img, uint32_t maxSide) { (void)img; (void)maxSide; return 0; }
int GsAstc_Load(const char *src, TexPackImage *img) { (void)src; (void)img; return 0; }
void GsAstc_Queue(const char *src) { (void)src; }

/* BT3's VU1 (gs_vu1.c): BT2's runtime runs the game's VU1 programs and gives the renderer their GIF packets. */
void GsVu1_SetCycle(uint32_t cl, uint32_t wl) { (void)cl; (void)wl; }
void GsVu1_SetBase(uint32_t v) { (void)v; }
void GsVu1_SetOffset(uint32_t v) { (void)v; }
void GsVu1_SetItop(uint32_t v) { (void)v; }
void GsVu1_SetMode(uint32_t v) { (void)v; }
void GsVu1_SetMask(uint32_t v) { (void)v; }
void GsVu1_SetRow(const uint32_t *v) { (void)v; }
void GsVu1_SetCol(const uint32_t *v) { (void)v; }
void GsVu1_Program(uint32_t addr, const uint32_t *words, uint32_t count) { (void)addr; (void)words; (void)count; }
uint32_t GsVu1_UnpackWords(uint32_t cmd, uint32_t num) { (void)cmd; (void)num; return 0; }
void GsVu1_Unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data) { (void)cmd; (void)num; (void)imm; (void)data; }
void GsVu1_Call(int addr) { (void)addr; }
void GsVu1_FrameEnd(void) {}
void GsVu1_StateKeep(int restore) { (void)restore; }

/* ---- settings changed while the game runs (the app's in-game menu, Bt2Bridge.reloadSettings): read again and
   given to the renderer between two frames, as BT3's own settings window gives them (GsGpu_SetSettings). */
static volatile int sReloadAsked;
void Bt2Hw_RequestReload(void) { sReloadAsked = 1; }

void Bt2Hw_ApplySettingsIfAsked(void) {
    extern int gsInterp, gGsFragDepth;
    extern void GsGpu_PresentSet(int mode);
    PortVideo v;
    if (!sReloadAsked) {
        return;
    }
    sReloadAsked = 0;
    Bt2Hw_ReloadSettings();
    GsGpu_GetSettings(&v);
    v.scale4 = Port_Setting("scale4", v.scale4);
    v.filter = Port_Setting("filter", v.filter);
    v.fxOff = Port_Setting("fx_off", v.fxOff);
    v.glow = Port_Setting("glow", v.glow);
    v.frameSkip = Port_Setting("frame_skip", v.frameSkip);
    v.vsync = Port_Setting("vsync", v.vsync);
    v.smooth2d = Port_Setting("smooth_2d", v.smooth2d);
    GsGpu_SetSettings(&v);
    gsInterp = Port_Setting("interp", gsInterp);
    gGsFragDepth = Port_Setting("frag_depth", gGsFragDepth);
    { extern int gBt2CelShade; gBt2CelShade = Port_Setting("cel_shade", 0); }
    GsGpu_PresentSet(Port_Setting("present", 0));
    fprintf(stderr, "BT3 renderer: settings now: scale %d/4, filter %d, effects off %d, glow %d%%, frame skip %d, smooth motion %d, precise depth %d\n",
            v.scale4, v.filter, v.fxOff, v.glow, v.frameSkip, gsInterp, gGsFragDepth);
}
