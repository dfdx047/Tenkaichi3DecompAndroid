/*
 * Controller input for the windowed PC build: fills the game's pad buffer (the libpad2 layout the game reads in
 * src/sys/pad.c) from the keyboard and from SDL gamepads, through bindings the settings window (ui.cpp) edits and
 * the settings file keeps.
 *
 * Each player has: one key per action (the 16 buttons and the 8 stick directions), one controller button or
 * trigger per pad button, and a controller slot (the n-th connected controller, or none). Defaults:
 *   player 1   first controller; arrows = d-pad, W A S D = left stick, Enter = START, Right Shift = SELECT,
 *              K = cross, L = circle, J = square, I = triangle, U = L1, O = R1, 7 = L2, 9 = R2
 *   player 2   second controller, no keys
 *   controller the usual layout (south = cross, east = circle, west = square, north = triangle, triggers = L2 / R2)
 * The sticks of a controller are passed through. The pressure bytes of the buffer are left at zero.
 */
#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "ui.h"

int gPortOverlayOpen, gPortInputCapture;

/* The on-screen controller (Android: android/.../touch/TouchPadView.kt through the loader): player 1's buttons
   (1 = pressed, the bits below) and sticks, added to whatever the keyboard and controllers give. */
static volatile unsigned sTouchButtons;
static volatile unsigned char sTouchStick[4] = {0x80, 0x80, 0x80, 0x80}; /* lx, ly, rx, ry */
void Port_TouchPad(unsigned buttons, int lx, int ly, int rx, int ry) {
    sTouchButtons = buttons & 0xFFFF;
    sTouchStick[0] = (unsigned char)lx;
    sTouchStick[1] = (unsigned char)ly;
    sTouchStick[2] = (unsigned char)rx;
    sTouchStick[3] = (unsigned char)ry;
}
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Port_SettingSave(const char *name, int value), Port_SettingsWrite(void);

/* bits of the two button bytes together (byte 0 low), as include/sys/pad.h has them; 1 = pressed here */
enum { B_SELECT = 0x0001, B_L3 = 0x0002, B_R3 = 0x0004, B_START = 0x0008, B_UP = 0x0010, B_RIGHT = 0x0020, B_DOWN = 0x0040,
       B_LEFT = 0x0080, B_L2 = 0x0100, B_R2 = 0x0200, B_L1 = 0x0400, B_R1 = 0x0800, B_TRIANGLE = 0x1000, B_CIRCLE = 0x2000,
       B_CROSS = 0x4000, B_SQUARE = 0x8000 };

/* The actions in the order the settings window lists them. 16..23: left stick up, down, left, right, then right. */
static const struct { const char *name, *label; unsigned bit; int key; int pad; } kActions[PORT_ACTIONS] = {
    {"up", "D-pad up", B_UP, SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"down", "D-pad down", B_DOWN, SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"left", "D-pad left", B_LEFT, SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"right", "D-pad right", B_RIGHT, SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"cross", "Cross", B_CROSS, SDL_SCANCODE_K, SDL_GAMEPAD_BUTTON_SOUTH},
    {"circle", "Circle", B_CIRCLE, SDL_SCANCODE_L, SDL_GAMEPAD_BUTTON_EAST},
    {"square", "Square", B_SQUARE, SDL_SCANCODE_J, SDL_GAMEPAD_BUTTON_WEST},
    {"triangle", "Triangle", B_TRIANGLE, SDL_SCANCODE_I, SDL_GAMEPAD_BUTTON_NORTH},
    {"l1", "L1", B_L1, SDL_SCANCODE_U, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"r1", "R1", B_R1, SDL_SCANCODE_O, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"l2", "L2", B_L2, SDL_SCANCODE_7, PORT_PAD_LT},
    {"r2", "R2", B_R2, SDL_SCANCODE_9, PORT_PAD_RT},
    {"l3", "L3", B_L3, 0, SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"r3", "R3", B_R3, 0, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"start", "START", B_START, SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_START},
    {"select", "SELECT", B_SELECT, SDL_SCANCODE_RSHIFT, SDL_GAMEPAD_BUTTON_BACK},
    {"ls_up", "Left stick up", 0, SDL_SCANCODE_W, -1}, {"ls_down", "Left stick down", 0, SDL_SCANCODE_S, -1},
    {"ls_left", "Left stick left", 0, SDL_SCANCODE_A, -1}, {"ls_right", "Left stick right", 0, SDL_SCANCODE_D, -1},
    {"rs_up", "Right stick up", 0, 0, -1}, {"rs_down", "Right stick down", 0, 0, -1},
    {"rs_left", "Right stick left", 0, 0, -1}, {"rs_right", "Right stick right", 0, 0, -1},
};

static int sKeys[2][PORT_ACTIONS], sPadBtn[2][PORT_PAD_BUTTONS], sPadSlot[2];
static pthread_once_t sLoadOnce = PTHREAD_ONCE_INIT;

static void defaults(int player) {
    int a;
    for (a = 0; a < PORT_ACTIONS; a++) {
        sKeys[player][a] = player == 0 ? kActions[a].key : 0;
        if (a < PORT_PAD_BUTTONS) {
            sPadBtn[player][a] = kActions[a].pad;
        }
    }
    sPadSlot[player] = player;
    if (player == 1 && getenv("BT3_PAD_SHARED") != NULL) {
        sPadSlot[player] = 0; /* BT3_PAD_SHARED: both players on controller 1 (testing 1P vs 2P with one pad) */
    }
}

static void load(void) {
    char name[32];
    int p, a;
    for (p = 0; p < 2; p++) {
        defaults(p);
        for (a = 0; a < PORT_ACTIONS; a++) {
            snprintf(name, sizeof(name), "key_p%d_%s", p + 1, kActions[a].name);
            sKeys[p][a] = Port_Setting(name, sKeys[p][a]);
            if (a < PORT_PAD_BUTTONS) {
                snprintf(name, sizeof(name), "pad_p%d_%s", p + 1, kActions[a].name);
                sPadBtn[p][a] = Port_Setting(name, sPadBtn[p][a]);
            }
        }
        snprintf(name, sizeof(name), "padslot_p%d", p + 1);
        sPadSlot[p] = Port_Setting(name, sPadSlot[p]);
        if (p == 1 && getenv("BT3_PAD_SHARED") != NULL) {
            sPadSlot[p] = 0; /* overrides the saved setting too */
        }
    }
}

const char *PortInput_ActionLabel(int action) { return kActions[action].label; }
int *PortInput_Keys(int player) { pthread_once(&sLoadOnce, load); return sKeys[player]; }
int *PortInput_PadButtons(int player) { pthread_once(&sLoadOnce, load); return sPadBtn[player]; }
int *PortInput_PadSlot(int player) { pthread_once(&sLoadOnce, load); return &sPadSlot[player]; }
void PortInput_ResetDefaults(int player) { pthread_once(&sLoadOnce, load); defaults(player); }

void PortInput_Save(void) {
    char name[32];
    int p, a;
    for (p = 0; p < 2; p++) {
        for (a = 0; a < PORT_ACTIONS; a++) {
            snprintf(name, sizeof(name), "key_p%d_%s", p + 1, kActions[a].name);
            Port_SettingSave(name, sKeys[p][a]);
            if (a < PORT_PAD_BUTTONS) {
                snprintf(name, sizeof(name), "pad_p%d_%s", p + 1, kActions[a].name);
                Port_SettingSave(name, sPadBtn[p][a]);
            }
        }
        snprintf(name, sizeof(name), "padslot_p%d", p + 1);
        Port_SettingSave(name, sPadSlot[p]);
    }
    Port_SettingsWrite();
}

/* The connected controllers, in SDL's order (the order the settings window lists them in), kept open. */
#define MAX_PADS 8
static struct { SDL_JoystickID id; SDL_Gamepad *pad; } sOpen[MAX_PADS];
static SDL_JoystickID sIds[MAX_PADS];
static int sIdCount, sInit;

static void pads_refresh(void) {
    static unsigned calls;
    SDL_JoystickID *ids;
    int n = 0, i, k;

    if (!sInit) {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        sInit = 1;
    } else if (calls++ % 120 != 0) { /* look for plugged and unplugged controllers about once a second */
        return;
    }
    ids = SDL_GetGamepads(&n);
    sIdCount = 0;
    for (i = 0; ids != NULL && i < n && sIdCount < MAX_PADS; i++) {
        sIds[sIdCount++] = ids[i];
    }
    SDL_free(ids);
    for (k = 0; k < MAX_PADS; k++) { /* close the ones that are gone */
        for (i = 0; i < sIdCount && sIds[i] != sOpen[k].id; i++) {
        }
        if (sOpen[k].pad != NULL && i == sIdCount) {
            SDL_CloseGamepad(sOpen[k].pad);
            sOpen[k].pad = NULL;
        }
    }
    for (i = 0; i < sIdCount; i++) { /* open the new ones */
        for (k = 0; k < MAX_PADS && !(sOpen[k].pad != NULL && sOpen[k].id == sIds[i]); k++) {
        }
        if (k == MAX_PADS) {
            for (k = 0; k < MAX_PADS && sOpen[k].pad != NULL; k++) {
            }
            if (k < MAX_PADS) {
                sOpen[k].id = sIds[i];
                sOpen[k].pad = SDL_OpenGamepad(sIds[i]);
            }
        }
    }
}

static SDL_Gamepad *pad_of(int player) {
    int slot = sPadSlot[player], k;
    if (slot < 0 || slot >= sIdCount) {
        return NULL;
    }
    for (k = 0; k < MAX_PADS; k++) {
        if (sOpen[k].pad != NULL && sOpen[k].id == sIds[slot]) {
            return sOpen[k].pad;
        }
    }
    return NULL;
}

/* BT3_PAD="frame:button,frame:button,...": scripted presses for player 1, each held for 4 frames from that frame
   (frame = the renderer's frame counter). Buttons: start select up down left right cross circle square triangle
   l1 r1 l2 r2. For tests without a person at the keyboard. */
static unsigned script_buttons(void) {
    static const struct { const char *name; unsigned bit; } names[] = {
        {"start", B_START}, {"select", B_SELECT}, {"up", B_UP}, {"down", B_DOWN}, {"left", B_LEFT}, {"right", B_RIGHT},
        {"cross", B_CROSS}, {"circle", B_CIRCLE}, {"square", B_SQUARE}, {"triangle", B_TRIANGLE}, {"l1", B_L1}, {"r1", B_R1},
        {"l2", B_L2}, {"r2", B_R2},
    };
    static struct { unsigned frame, bit; } ev[256];
    static int count = -1;
    unsigned btn = 0;
    int i;

    if (count < 0) {
        const char *p = getenv("BT3_PAD");
        count = 0;
        while (p != NULL && *p != 0 && count < 256) {
            char name[16];
            unsigned frame;
            int used = 0;
            size_t k;
            if (sscanf(p, "%u:%15[a-z0-9]%n", &frame, name, &used) != 2) {
                break;
            }
            for (k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
                if (strcmp(name, names[k].name) == 0) {
                    ev[count].frame = frame;
                    ev[count++].bit = names[k].bit;
                }
            }
            p += used;
            if (*p == ',') { p++; }
        }
    }
    for (i = 0; i < count; i++) {
        if (gGsFrame >= ev[i].frame && gGsFrame < ev[i].frame + 4) {
            btn |= ev[i].bit;
        }
    }
    return btn;
}

static unsigned char axis(SDL_Gamepad *g, SDL_GamepadAxis a) {
    return (unsigned char)((SDL_GetGamepadAxis(g, a) + 32768) >> 8);
}

/* Fills `data` (18 bytes) for controller port `socket`. Returns 0 when there is no window (the caller keeps the
   idle pad). */
int Port_PadRead(int socket, unsigned char *data) {
    unsigned btn = 0;
    unsigned char rx = 0x80, ry = 0x80, lx = 0x80, ly = 0x80;
    SDL_Gamepad *g;
    int a;

    if (!GsGpu_Enabled() || socket < 0 || socket > 1) {
        return 0;
    }
    pthread_once(&sLoadOnce, load);
    pads_refresh();
    g = gPortInputCapture ? NULL : pad_of(socket);
    if (g != NULL) {
        for (a = 0; a < PORT_PAD_BUTTONS; a++) {
            int src = sPadBtn[socket][a];
            if (src == PORT_PAD_LT ? SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000 :
                src == PORT_PAD_RT ? SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000 :
                src >= 0 && SDL_GetGamepadButton(g, (SDL_GamepadButton)src)) {
                btn |= kActions[a].bit;
            }
        }
        lx = axis(g, SDL_GAMEPAD_AXIS_LEFTX); ly = axis(g, SDL_GAMEPAD_AXIS_LEFTY);
        rx = axis(g, SDL_GAMEPAD_AXIS_RIGHTX); ry = axis(g, SDL_GAMEPAD_AXIS_RIGHTY);
    }
    if (!gPortOverlayOpen) { /* the settings window takes the keyboard while it is open */
        const bool *ks = SDL_GetKeyboardState(NULL);
        const int *k = sKeys[socket];
        for (a = 0; a < PORT_PAD_BUTTONS; a++) {
            if (k[a] > 0 && k[a] < SDL_SCANCODE_COUNT && ks[k[a]]) {
                btn |= kActions[a].bit;
            }
        }
#define HELD(n) (k[n] > 0 && k[n] < SDL_SCANCODE_COUNT && ks[k[n]])
        if (HELD(16)) { ly = 0x00; }
        if (HELD(17)) { ly = 0xFF; }
        if (HELD(18)) { lx = 0x00; }
        if (HELD(19)) { lx = 0xFF; }
        if (HELD(20)) { ry = 0x00; }
        if (HELD(21)) { ry = 0xFF; }
        if (HELD(22)) { rx = 0x00; }
        if (HELD(23)) { rx = 0xFF; }
#undef HELD
    }
    if (socket == 0) {
        btn |= script_buttons();
        if (!gPortInputCapture) {
            btn |= sTouchButtons;
            if (sTouchStick[0] != 0x80 || sTouchStick[1] != 0x80) {
                lx = sTouchStick[0];
                ly = sTouchStick[1];
            }
            if (sTouchStick[2] != 0x80 || sTouchStick[3] != 0x80) {
                rx = sTouchStick[2];
                ry = sTouchStick[3];
            }
        }
    }
    memset(data, 0, 18);
    data[0] = (unsigned char)(~btn & 0xFF);       /* active low */
    data[1] = (unsigned char)(~(btn >> 8) & 0xFF);
    data[2] = rx; data[3] = ry; data[4] = lx; data[5] = ly;
    return 18;
}
