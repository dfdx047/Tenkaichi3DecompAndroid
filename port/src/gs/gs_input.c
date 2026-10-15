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
 * The sticks of a controller are passed through, or given a round deadzone first (stick_bytes, below). The pressure
 * bytes of the buffer are left at zero. The game's motor commands go to the player's controller (Port_PadVibrate).
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "ui.h"

int gPortOverlayOpen, gPortInputCapture, gPortChatTyping;
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
/* Per player: the deadzone of the left and of the right stick (-1 = the game's own, as the pad's bytes always were
   passed; 0..PORT_DEADZONE_MAX = a round one of that many percent, stick_bytes below), and vibration (on, strength
   in percent). */
static int sDeadzone[2][2], sVib[2][2];
/* What opens and closes the settings window: a key (F1) and a controller button (none), both of the settings
   window's list; and START + SELECT of any controller together, always. */
static int sOverlayKey = SDL_SCANCODE_F1, sOverlayPad = -1;
static int sChatKey = SDL_SCANCODE_TAB; /* opens the online chat's line (setting key_chat; Tab by default) */
static int sComboLatch; /* START + SELECT has opened or closed the window and is still held */
static int sSwallow; /* a controller opened or closed the window: nothing of the controllers reaches the game until those buttons are let go */
static volatile int sKeySwallow; /* the chat's line was sent or put away: nothing of the keyboard reaches the game until no bound key is held
                                    (Enter, which sends it, is START) */
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
    sDeadzone[player][0] = sDeadzone[player][1] = -1;
    sVib[player][0] = 1;
    sVib[player][1] = 100;
}

static int range(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

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
        for (a = 0; a < 2; a++) {
            snprintf(name, sizeof(name), "deadzone_p%d_%s", p + 1, a == 0 ? "ls" : "rs");
            sDeadzone[p][a] = range(Port_Setting(name, -1), -1, PORT_DEADZONE_MAX);
        }
        snprintf(name, sizeof(name), "vibration_p%d", p + 1);
        sVib[p][0] = Port_Setting(name, 1) != 0;
        snprintf(name, sizeof(name), "vibration_p%d_strength", p + 1);
        sVib[p][1] = range(Port_Setting(name, 100), 0, 100);
    }
    sOverlayKey = Port_Setting("key_overlay", SDL_SCANCODE_F1);
    sOverlayPad = Port_Setting("pad_overlay", -1);
    sChatKey = Port_Setting("key_chat", SDL_SCANCODE_TAB);
}

const char *PortInput_ActionLabel(int action) { return kActions[action].label; }
int *PortInput_Keys(int player) { pthread_once(&sLoadOnce, load); return sKeys[player]; }
int *PortInput_PadButtons(int player) { pthread_once(&sLoadOnce, load); return sPadBtn[player]; }
int *PortInput_PadSlot(int player) { pthread_once(&sLoadOnce, load); return &sPadSlot[player]; }
int *PortInput_Deadzone(int player) { pthread_once(&sLoadOnce, load); return sDeadzone[player]; }
int *PortInput_Vibration(int player) { pthread_once(&sLoadOnce, load); return sVib[player]; }
int *PortInput_OverlayKey(void) { pthread_once(&sLoadOnce, load); return &sOverlayKey; }
int *PortInput_OverlayPad(void) { pthread_once(&sLoadOnce, load); return &sOverlayPad; }

/* 1 = this event opens or closes the settings window. */
int PortInput_Hotkey(const SDL_Event *ev) {
    pthread_once(&sLoadOnce, load);
    if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        (ev->gbutton.button == SDL_GAMEPAD_BUTTON_START || ev->gbutton.button == SDL_GAMEPAD_BUTTON_BACK)) {
        sComboLatch = 0;
        return 0;
    }
    if (gPortInputCapture) { /* (a binding is being taken, or the online screen is up) */
        return 0;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
        return sOverlayKey > 0 && (int)ev->key.scancode == sOverlayKey;
    }
    if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *g = SDL_GetGamepadFromID(ev->gbutton.which);
        int b = (int)ev->gbutton.button;
        if (sOverlayPad >= 0 && b == sOverlayPad) {
            sSwallow = 1;
            return 1;
        }
        /* START + SELECT counts once per press of the two (sComboLatch, cleared when either is let go). Pressed
           in the same moment, both "down" events come in one round and each already finds the other held:
           without the latch the window opened and closed again at once, and only "hold one, press the other"
           worked. */
        if (g != NULL && (b == SDL_GAMEPAD_BUTTON_START || b == SDL_GAMEPAD_BUTTON_BACK) &&
            SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_START) && SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_BACK)) {
            sSwallow = 1;
            if (sComboLatch) {
                return 0;
            }
            sComboLatch = 1;
            return 1;
        }
    }
    return 0;
}
/* The settings window closed, or its exit question was answered, with a controller button: that press is not the game's. */
void PortInput_Swallow(void) { sSwallow = 2; }
void PortInput_SwallowKeys(void) { sKeySwallow = 1; }

/* Tenkaichi Budokai Online's keypads (src/menu/port_tbo.c): while one takes text (gPortTboTyping), what is typed
   goes to it, a character at a time (PortTbo_TextRead: the character, 8 backspace, 13 enter, 27 escape, 0 none),
   and the keys that type (letters, digits, space, punctuation, enter, backspace, escape) are not the pad's: the
   arrow keys and the controller still move on the keypad. The window's events come in on the renderer's thread
   (PortInput_TboEvent, from Ui_Event), the game reads them on its own. */
int gPortTboTyping;
static char sTboKeys[64];
static unsigned sTboHead, sTboTail;
static SDL_SpinLock sTboLock;

static int typing_key(int sc) {
    return (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_SLASH && sc != SDL_SCANCODE_TAB) ||
           (sc >= SDL_SCANCODE_KP_DIVIDE && sc <= SDL_SCANCODE_KP_PERIOD);
}

static void tbo_push(int c) {
    SDL_LockSpinlock(&sTboLock);
    if (sTboHead - sTboTail < sizeof(sTboKeys)) {
        sTboKeys[sTboHead++ % sizeof(sTboKeys)] = (char)c;
    }
    SDL_UnlockSpinlock(&sTboLock);
}

void PortTbo_TextMode(int on) {
    SDL_LockSpinlock(&sTboLock);
    sTboHead = sTboTail = 0;
    SDL_UnlockSpinlock(&sTboLock);
    if (gPortTboTyping && !on) {
        sKeySwallow = 1; /* (Enter, which ended it, is START: not the game's until it is let go) */
    }
    gPortTboTyping = on != 0;
}

int PortTbo_TextRead(void) {
    int c = 0;
    SDL_LockSpinlock(&sTboLock);
    if (sTboTail != sTboHead) {
        c = (unsigned char)sTboKeys[sTboTail++ % sizeof(sTboKeys)];
    }
    SDL_UnlockSpinlock(&sTboLock);
    return c;
}

/* "Copy Code" (the room's code, to give the rival): the game's thread hands the text over, the window's thread
   puts it on the clipboard (PortInput_TboClipboard, from the overlay's frame: SDL wants the clipboard there). */
static char sTboCopy[64];
static int sTboCopyDue;
static int sTboPasteDue; /* the keypad's PASTE (Join Room): the clipboard's room code, typed on the window's thread */

void PortTbo_Copy(const char *text) {
    SDL_LockSpinlock(&sTboLock);
    SDL_strlcpy(sTboCopy, text != NULL ? text : "", sizeof(sTboCopy));
    sTboCopyDue = sTboCopy[0] != '\0';
    SDL_UnlockSpinlock(&sTboLock);
}

void PortTbo_Paste(void) {
    SDL_LockSpinlock(&sTboLock);
    sTboPasteDue = 1;
    SDL_UnlockSpinlock(&sTboLock);
}

/* The room code in a copied text: the last run of exactly six letters and digits ("Room code: MFT8FD"); without
   one, every letter and digit of it. Typed as if by hand. */
static void tbo_paste_code(const char *clip) {
    const unsigned char *p = (const unsigned char *)clip, *code = NULL;
    int run = 0, n = 0;

    for (; p != NULL && *p; p++) {
        int alnum = (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z');
        run = alnum ? run + 1 : 0;
        if (run == 6 && !((p[1] >= '0' && p[1] <= '9') || (p[1] >= 'A' && p[1] <= 'Z') || (p[1] >= 'a' && p[1] <= 'z'))) {
            code = p - 5;
        }
    }
    if (code != NULL) {
        for (n = 0; n < 6; n++) {
            tbo_push(code[n]);
        }
        return;
    }
    for (p = (const unsigned char *)clip; p != NULL && *p && n < 48; p++) {
        if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
            tbo_push(*p);
            n++;
        }
    }
}

void PortInput_TboClipboard(void) {
    char text[sizeof(sTboCopy)];
    int due, paste;
    SDL_LockSpinlock(&sTboLock);
    due = sTboCopyDue;
    sTboCopyDue = 0;
    paste = sTboPasteDue;
    sTboPasteDue = 0;
    memcpy(text, sTboCopy, sizeof(text));
    SDL_UnlockSpinlock(&sTboLock);
    if (due) {
        SDL_SetClipboardText(text);
    }
    if (paste) {
        char *clip = SDL_GetClipboardText();
        tbo_paste_code(clip);
        SDL_free(clip);
    }
}

/* 1: the event was the keypad's. Only plain characters (32..126) are typed: a name or a room code. */
int PortInput_TboEvent(const SDL_Event *ev) {
    if (!gPortTboTyping) {
        return 0;
    }
    if (ev->type == SDL_EVENT_TEXT_INPUT) {
        const unsigned char *p = (const unsigned char *)ev->text.text;
        for (; p != NULL && *p; p++) {
            if (*p >= 32 && *p < 127) {
                tbo_push(*p);
            }
        }
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN) {
        SDL_Scancode sc = ev->key.scancode;
        if (sc == SDL_SCANCODE_V && (ev->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) && !ev->key.repeat) {
            /* Ctrl+V (Cmd+V): the clipboard's text, as if typed (a room code copied from a message); only its
               plain characters, and no more than the keypad can hold */
            char *clip = SDL_GetClipboardText();
            const unsigned char *p = (const unsigned char *)clip;
            int n = 0;
            for (; p != NULL && *p && n < 48; p++) {
                if (*p >= 32 && *p < 127) {
                    tbo_push(*p);
                    n++;
                }
            }
            SDL_free(clip);
            return 1;
        }
        if (sc == SDL_SCANCODE_BACKSPACE) {
            tbo_push(8);
        } else if ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && !ev->key.repeat) {
            tbo_push(13);
        } else if (sc == SDL_SCANCODE_ESCAPE && !ev->key.repeat) {
            tbo_push(27);
        }
        return typing_key(sc);
    }
    return ev->type == SDL_EVENT_KEY_UP && typing_key(ev->key.scancode);
}
int *PortInput_ChatKey(void) { pthread_once(&sLoadOnce, load); return &sChatKey; }
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
        for (a = 0; a < 2; a++) {
            snprintf(name, sizeof(name), "deadzone_p%d_%s", p + 1, a == 0 ? "ls" : "rs");
            Port_SettingSave(name, sDeadzone[p][a]);
        }
        snprintf(name, sizeof(name), "vibration_p%d", p + 1);
        Port_SettingSave(name, sVib[p][0]);
        snprintf(name, sizeof(name), "vibration_p%d_strength", p + 1);
        Port_SettingSave(name, sVib[p][1]);
    }
    Port_SettingSave("key_overlay", sOverlayKey);
    Port_SettingSave("pad_overlay", sOverlayPad);
    Port_SettingSave("key_chat", sChatKey);
    Port_SettingsWrite();
}

/* Testing without a controller or a person. BT3_STICK_TEST="lx,ly,rx,ry[,lt,rt]" (the sticks -32768..32767, + =
   right / down; the triggers 0..32767) and BT3_RUMBLE_LOG=1 each attach a controller made by SDL itself ("virtual"),
   which becomes controller 1: its sticks are held at those values, and what SDL hands its motors is printed. The
   rest of the program does not know it from a real one. */
static bool SDLCALL test_pad_rumble(void *userdata, Uint16 low, Uint16 high) {
    (void)userdata;
    fprintf(stderr, "rumble: the test controller's motors: low %u, high %u\n", low, high);
    return true;
}

static SDL_Joystick *sTestPad;

/* (Said again after the program has opened the controller: an axis SDL has not seen move reads as resting.) */
static void test_pad_axes(void) {
    const char *t = getenv("BT3_STICK_TEST");
    int v[6] = {0, 0, 0, 0, 0, 0}, i, pass;

    if (sTestPad == NULL) {
        return;
    }
    if (t != NULL) {
        sscanf(t, "%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    }
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < 6; i++) { /* (a trigger is an axis whose rest is its lowest value) */
            int x = range(i < 4 ? v[i] : v[i] * 2 - 32768, -32768, 32767);
            SDL_SetJoystickVirtualAxis(sTestPad, i, (Sint16)(pass == 0 ? x ^ 1 : x));
        }
        SDL_UpdateJoysticks();
    }
}

static void test_pad(void) {
    SDL_VirtualJoystickDesc desc;
    SDL_JoystickID id;

    if (getenv("BT3_STICK_TEST") == NULL && getenv("BT3_RUMBLE_LOG") == NULL) {
        return;
    }
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "Test controller";
    desc.Rumble = test_pad_rumble;
    id = SDL_AttachVirtualJoystick(&desc);
    sTestPad = id != 0 ? SDL_OpenJoystick(id) : NULL;
    if (sTestPad == NULL) {
        fprintf(stderr, "bt3: no test controller: %s\n", SDL_GetError());
    }
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
        test_pad();
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
    if (calls < 400) {
        test_pad_axes();
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

/* Fills `data` (18 bytes) for controller port `socket`. Returns 0 when there is no window (the caller keeps the
   idle pad). */
/* "Clashes: Square counts as a direction" (setting clash_square, off by default; BT3_CLASH_SQUARE=1).
 *
 * In a clash (two beams or two rushes meeting) the game counts one thing: the frames in which a direction is newly
 * pressed (input 51 = BTLC_DIR_P; BtlAct_ClashStruggleHandler and BtlAct_ClashBlowsHandler). Turning the stick is
 * the usual way to make them, four a turn; tapping directions makes them as well. With the setting on, while this
 * player's fighter is in a clash action, Square is taken off the pad and each press of it holds the stick in the
 * next of up, right, down, left for as long as it is held: one press, one newly pressed direction, as a tap of
 * the d-pad gives. The stick and the d-pad go on working beside it.
 *
 * A press can be made to count for more than one (setting clash_square_x, in quarters: 4, 6, 7 or 8 for 1, 1.5,
 * 1.75 or 2; 1.75 unless set; BT3_CLASH_SQUARE_X): the d-pad has four buttons to tap in turn and the stick four
 * directions a turn, Square is one button. What a press is worth beyond one is kept as credit, and whenever a
 * whole point of it is owed the stick is put in the next direction for one frame without a press: four presses at
 * 1.75 give 2, 2, 2, 1. Still at most one newly pressed direction a frame, the game's own limit, and credit does
 * not pile up beyond two points or outlast the clash.
 *
 * This is done to the pad's own bytes, before they reach the game or the other player of an online match: the
 * game sees an ordinary pad, so the two copies of an online match compute the same fight whoever has it on. */
static int sClashSquare = -1;

int Port_ClashSquare(void) {
    if (sClashSquare < 0) {
        sClashSquare = getenv("BT3_CLASH_SQUARE") != NULL ? atoi(getenv("BT3_CLASH_SQUARE")) != 0 : Port_Setting("clash_square", 0) != 0;
    }
    return sClashSquare;
}

/* What a press of Square is worth in a clash, in quarters of a point (4, 6, 7 or 8). */
static int sClashSquareX = -1;

int Port_ClashSquareX(void) {
    if (sClashSquareX < 0) {
        int x = getenv("BT3_CLASH_SQUARE_X") != NULL ? atoi(getenv("BT3_CLASH_SQUARE_X")) : Port_Setting("clash_square_x", 7);
        sClashSquareX = x == 4 || x == 6 || x == 7 || x == 8 ? x : 7;
    }
    return sClashSquareX;
}

void Port_ClashSquareXSet(int quarters) {
    sClashSquareX = quarters == 4 || quarters == 6 || quarters == 8 ? quarters : 7;
    Port_SettingSave("clash_square_x", sClashSquareX);
}

void Port_ClashSquareSet(int on) {
    sClashSquare = on != 0;
    Port_SettingSave("clash_square", sClashSquare);
    Port_SettingsWrite();
}

static void clash_square(int socket, unsigned *btn, unsigned char *lx, unsigned char *ly) {
    extern void *BtlChar_FindByObjId(int objId); /* the game: the fighter of a side, NULL outside a fight */
    extern int BtlAct_GetCurrent(void *chr);     /* its action */
    extern int Port_NetActive(void), Port_NetMe(void);
    static int dir[2], was[2], credit[2]; /* (credit: quarters of a point owed beyond the press's own) */
    int side = Port_NetActive() ? Port_NetMe() : socket, in = 0, down = (*btn & B_SQUARE) != 0, extra = 0;
    void *chr;

    if (!Port_ClashSquare()) {
        return;
    }
    if (getenv("BT3_CLASH_TEST") != NULL) {
        in = 1; /* testing: as if in a clash all the time */
    } else if ((chr = BtlChar_FindByObjId(side)) != NULL) {
        int action = BtlAct_GetCurrent(chr);
        in = (action >= 0x130 && action <= 0x132) || action == 0xFA || action == 0xFB || action == 0xFC;
    }
    if (!in) {
        was[socket] = down; /* (a Square held into the clash is not a press in it) */
        credit[socket] = 0;
        return;
    }
    if (down && !was[socket]) {
        dir[socket] = (dir[socket] + 1) & 3;
        credit[socket] += Port_ClashSquareX() - 4;
        if (credit[socket] > 8) {
            credit[socket] = 8;
        }
    } else if (credit[socket] >= 4) { /* a whole point is owed: the next direction for this frame, without a press */
        dir[socket] = (dir[socket] + 1) & 3;
        credit[socket] -= 4;
        extra = 1;
    }
    was[socket] = down;
    *btn &= ~(unsigned)B_SQUARE;
    if (down || extra) {
        switch (dir[socket]) {
        case 0: *ly = 0x00; break; /* up */
        case 1: *lx = 0xFF; break; /* right */
        case 2: *ly = 0xFF; break; /* down */
        default: *lx = 0x00; break; /* left */
        }
    }
}

/* BT3_CLASH_LOG=1: one line for every point a clash counts from a newly pressed direction (the stick, the d-pad, or
   Square with the option above), and the total as a beam clash ends. Called from the game's clash handlers. */
void Port_ClashLog(int player, const char *what, int fromInput, int total, int frame) {
    static int on = -1;

    if (on < 0) {
        on = getenv("BT3_CLASH_LOG") != NULL;
    }
    if (!on) {
        return;
    }
    if (fromInput) {
        fprintf(stderr, "clash: player %d, %s, frame %d: +1 from input, total %d%s\n", player + 1, what, frame, total,
                Port_ClashSquare() ? " (Square option on)" : "");
    } else {
        fprintf(stderr, "clash: player %d, %s ended after %d frames with %d points\n", player + 1, what, frame, total);
    }
}

/* The sticks.
 *
 * The game (src/sys/pad.c, Pad_Update) takes each axis as a byte, 0..255 with the rest between 127 and 128, and
 * gives every axis by itself a deadzone of PAD_STICK_DEADZONE = 50 of the 127.5 (39%): value = (|byte - 127.5| -
 * 50) / 77.5, zero below. The pair is then brought inside the unit circle, and the stick counts as a direction
 * (the menus, dashes, the clash counter) when the larger of the two values is beyond 0.5: 69.6% of the way on an
 * axis. Its zone of rest is therefore a square, and a diagonal starts late.
 *
 * Deadzone -1 ("Original") passes the controller's bytes as they are: that. A deadzone d of 0..PORT_DEADZONE_MAX
 * percent is a round one instead: with m the stick's distance from the centre (0..1, the corners of a square gate
 * cut to 1), nothing below d, and from there
 *     out = (m - d) / (1 - d)        in the stick's own direction
 * which is zero at the edge of the zone and 1 at full tilt. The bytes are then the ones the game's own formula
 * turns back into `out` (0x80 for zero, else 127.5 +- (50 + |out| * 77.5), to the nearest of the 78 steps a side),
 * so the game is left as it is and its direction test, 0.5 of `out`, falls at d + (1 - d) / 2 of the way: half way
 * between the zone's edge and full tilt, in every direction. It is done to the pad's bytes, before they reach the
 * game or the other player of an online match (as the Square option above).
 */
static unsigned char game_byte(double v) {
    double a = fabs(v) * 77.5;
    int k;

    if (a < 0.25) { /* (nearer to nothing than to the first step, 0.5) */
        return 0x80;
    }
    k = range((int)a, 0, 77); /* the steps are at k + 0.5 */
    return (unsigned char)(v > 0.0 ? 178 + k : 77 - k);
}

static void stick_bytes(int deadzone, int x, int y, unsigned char *bx, unsigned char *by) {
    double fx = x < -32767 ? -1.0 : x / 32767.0, fy = y < -32767 ? -1.0 : y / 32767.0;
    double m = sqrt(fx * fx + fy * fy), d = deadzone / 100.0, out;

    if (deadzone < 0) {
        *bx = (unsigned char)((x + 32768) >> 8);
        *by = (unsigned char)((y + 32768) >> 8);
        return;
    }
    if (m <= d || m <= 0.0) {
        *bx = *by = 0x80;
        return;
    }
    out = ((m > 1.0 ? 1.0 : m) - d) / (1.0 - d);
    *bx = game_byte(fx / m * out);
    *by = game_byte(fy / m * out);
}

/* What the game makes of a stick's two bytes: Pad_StickToFloat and Pad_Update's steps (src/sys/pad.c), done again
   here in the same single-precision arithmetic. (The game's own function cannot be called from this side: its
   floats are not passed as the host's are. BT3_STICK_TEST prints this beside what the game itself then has.) */
static float game_axis(unsigned char raw) {
    float value = (float)raw - 127.5f;

    if (0.0f < value) {
        value -= 50.0f;
        if (value < 0.0f) {
            value = 0.0f;
        }
    } else {
        value += 50.0f;
        if (0.0f < value) {
            value = 0.0f;
        }
    }
    return value / (127.5f - 50.0f);
}

static void game_stick(const unsigned char b[2], float out[2], int *dir) {
    float len;

    out[0] = game_axis(b[0]);
    out[1] = game_axis(b[1]);
    len = out[0] * out[0] + out[1] * out[1];
    if (len > 1.0f) {
        len = 1.0f / sqrtf(len);
        out[0] *= len;
        out[1] *= len;
    }
    if (fabsf(out[0]) > fabsf(out[1])) {
        *dir = out[0] > 0.5f ? 4 : out[0] < -0.5f ? 3 : 0;
    } else {
        *dir = out[1] > 0.5f ? 2 : out[1] < -0.5f ? 1 : 0;
    }
}

/* For the settings window: the sticks and triggers of a player's controller now, and what the game would get. */
void PortInput_StickView(int player, PortStickView *out) {
    static const SDL_GamepadAxis axes[2][2] = {{SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY}, {SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY}};
    SDL_Gamepad *g;
    int s, i;

    pthread_once(&sLoadOnce, load);
    memset(out, 0, sizeof(*out));
    g = pad_of(player);
    out->connected = g != NULL;
    out->canVibrate = g != NULL && SDL_GetBooleanProperty(SDL_GetGamepadProperties(g), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    for (s = 0; s < 2; s++) {
        int v[2] = {0, 0};
        for (i = 0; i < 2 && g != NULL; i++) {
            v[i] = SDL_GetGamepadAxis(g, axes[s][i]);
            out->raw[s][i] = v[i] < -32767 ? -1.0f : (float)v[i] / 32767.0f;
        }
        stick_bytes(sDeadzone[player][s], v[0], v[1], &out->bytes[s][0], &out->bytes[s][1]);
        game_stick(out->bytes[s], out->game[s], &out->dir[s]);
        if (g != NULL) {
            int t = SDL_GetGamepadAxis(g, s == 0 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            out->trigger[s] = (float)t / 32767.0f;
            out->triggerHeld[s] = t > PORT_TRIGGER_AT;
        }
    }
}

/* Vibration.
 *
 * The game drives the DualShock 2's two motors itself: every frame Pad_Update (src/sys/pad.c) ends with
 * Pad_SendActuator for each port, which calls sceVibSetActParam with two bytes: bit 0 = the small motor (on / off),
 * bits 1..7 = the large motor's power (0..127). What it sends is what the fight asked for in that frame
 * (BtlChar_UpdateVibration -> Pad_AddVibration: hits taken and given, clashes, charging; nothing in a replay, for a
 * CPU side, or with the game's own Options > Controller > Vibration off for that player), and zero otherwise:
 * a motor stops by the game sending zero. That call comes here (plat_stub.c).
 *
 * The large motor is SDL's low-frequency one (its power, of 127, becomes SDL's, of 65535), the small motor the
 * high-frequency one (all or nothing), each times the player's strength. The game runs 30 steps a second and
 * sends a new command with every one for as long as an effect lasts: the large motor's power swings between 50%
 * and 100% of what was asked, six times a second, and the small motor is on in every other step. That is passed on
 * as it comes; a command that only repeats the last one is not sent again until half its time is over.
 * Nothing is sent while the settings window is open or the game's window is not the one being used, with the
 * player's setting off, or in a frame that is only being re-run (online play's rollback). Every command is given
 * RUMBLE_MS to live, so a controller is never left humming by a game that stopped coming here; and the
 * controllers are stopped as the program ends.
 *
 * Nothing here is seen by the game: two bytes are read, nothing is written and the result is the constant it
 * always was. A fight, a recording of one and both sides of an online match are the same with it on or off. Online,
 * both ports' commands are computed on both sides: only this side's (Port_NetMe) are felt, on player 1's
 * controller, which is the one that plays there.
 */
#define RUMBLE_MS 250
static struct { SDL_Gamepad *pad; Uint16 low, high; Uint64 at; int lastLarge, lastSmall; } sRumble[2];

static void rumble_send(int player, SDL_Gamepad *g, Uint16 low, Uint16 high, int why) {
    Uint64 now = SDL_GetTicks();

    if (sRumble[player].pad != g && sRumble[player].pad != NULL && (sRumble[player].low | sRumble[player].high) != 0) {
        int k; /* another controller became this player's: the old one, if it is still there, is stopped */
        for (k = 0; k < MAX_PADS; k++) {
            if (sOpen[k].pad == sRumble[player].pad) {
                SDL_RumbleGamepad(sOpen[k].pad, 0, 0, 0);
            }
        }
        sRumble[player].low = sRumble[player].high = 0;
    }
    sRumble[player].pad = g;
    if (low == sRumble[player].low && high == sRumble[player].high && ((low | high) == 0 || now - sRumble[player].at < RUMBLE_MS / 2)) {
        return; /* (the same again: only said again before it would run out) */
    }
    if (getenv("BT3_RUMBLE_LOG") != NULL) {
        extern unsigned gPortVBlanks;
        static const char *const whys[] = {"", " (the settings window is open)", " (the window is not in use)", " (vibration is off)"};
        fprintf(stderr, "rumble: vblank %u, player %d: small %d, large %d/127 -> %s(low %u, high %u, %d ms)%s\n", gPortVBlanks, player + 1,
                sRumble[player].lastSmall, sRumble[player].lastLarge, g != NULL ? "SDL_RumbleGamepad" : "no controller", low, high,
                (low | high) != 0 ? RUMBLE_MS : 0, whys[why]);
    }
    if (g != NULL) {
        SDL_RumbleGamepad(g, low, high, (low | high) != 0 ? RUMBLE_MS : 0);
    }
    sRumble[player].low = low;
    sRumble[player].high = high;
    sRumble[player].at = now;
}

static void rumble_stop(void) {
    int k;
    for (k = 0; k < MAX_PADS; k++) {
        if (sOpen[k].pad != NULL) {
            SDL_RumbleGamepad(sOpen[k].pad, 0, 0, 0);
        }
    }
}

/* The game's motor command for a port: `small` 0 / 1, `large` 0..127. Once per port and frame. */
void Port_PadVibrate(int socket, int small, int large) {
    extern int Port_NetActive(void), Port_NetMe(void);
    extern int gPortResim;
    static int atExit;
    SDL_Window *w;
    int player = socket, why = 0;
    unsigned low, high;

    if (!sInit || gPortResim || socket < 0 || socket > 1) {
        return;
    }
    if (Port_NetActive()) {
        if (socket != Port_NetMe()) {
            return;
        }
        player = 0;
    }
    if (!atExit) {
        atExit = 1;
        atexit(rumble_stop);
    }
    sRumble[player].lastLarge = large;
    sRumble[player].lastSmall = small;
    low = (unsigned)large * 65535u / 127u * (unsigned)sVib[player][1] / 100u;
    high = small ? 65535u * (unsigned)sVib[player][1] / 100u : 0;
    w = GsDraw_Window();
    if (gPortOverlayOpen || gPortInputCapture) {
        why = 1;
    } else if (w != NULL && !(SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) && getenv("BT3_RUMBLE_LOG") == NULL) {
        why = 2; /* (the test's window, which nobody sees, never is) */
    } else if (!sVib[player][0]) {
        why = 3;
    }
    if (why != 0) {
        low = high = 0;
    }
    rumble_send(player, pad_of(player), (Uint16)low, (Uint16)high, why);
}

/* 1 with BT3_RUMBLE_TEST set: the game's rule "no vibration in a replay" is lifted (src/battle/btl_char_get.c). */
int Port_RumbleTest(void) {
    static int on = -1;
    if (on < 0) {
        on = getenv("BT3_RUMBLE_TEST") != NULL;
    }
    return on;
}

/* The settings window's "Test": half a second of both motors at the player's strength (the one thing that is felt
   while that window is open). */
void PortInput_VibrationTest(int player) {
    SDL_Gamepad *g = pad_of(player);
    Uint16 s = (Uint16)(65535u * (unsigned)sVib[player][1] / 100u);

    if (getenv("BT3_RUMBLE_LOG") != NULL) {
        fprintf(stderr, "rumble: test, player %d -> %s(low %u, high %u, 500 ms)\n", player + 1, g != NULL ? "SDL_RumbleGamepad" : "no controller", s, s);
    }
    if (g != NULL) {
        SDL_RumbleGamepad(g, s, s, 500);
    }
}

/* The controllers' and the keyboard's state as it is at this moment (plat_stub.c, "Low input lag": the pads are
   read long after the window's events were last taken). The events themselves stay queued for the window. */
void PortInput_Fresh(void) {
    SDL_PumpEvents();
}

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
    if (sSwallow) { /* the buttons that opened or closed the settings window are not the game's (START would pause) */
        int k, b, held = 0;
        for (k = 0; k < MAX_PADS && sSwallow == 2; k++) { /* (2: whichever button it was) */
            for (b = 0; sOpen[k].pad != NULL && b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
                if (SDL_GetGamepadButton(sOpen[k].pad, (SDL_GamepadButton)b)) {
                    held = 2;
                }
            }
        }
        for (k = 0; k < MAX_PADS; k++) {
            if (sOpen[k].pad != NULL &&
                (SDL_GetGamepadButton(sOpen[k].pad, SDL_GAMEPAD_BUTTON_START) || SDL_GetGamepadButton(sOpen[k].pad, SDL_GAMEPAD_BUTTON_BACK) ||
                 (sOverlayPad >= 0 && sOverlayPad < SDL_GAMEPAD_BUTTON_COUNT && SDL_GetGamepadButton(sOpen[k].pad, (SDL_GamepadButton)sOverlayPad)))) {
                held = held != 0 ? held : 1;
            }
        }
        sSwallow = held;
    }
    /* (the settings window is worked with the controller too while it is open: nothing of it for the game) */
    g = gPortInputCapture || gPortOverlayOpen || sSwallow ? NULL : pad_of(socket);
    if (g != NULL) {
        for (a = 0; a < PORT_PAD_BUTTONS; a++) {
            int src = sPadBtn[socket][a];
            if (src == PORT_PAD_LT ? SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > PORT_TRIGGER_AT :
                src == PORT_PAD_RT ? SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > PORT_TRIGGER_AT :
                src >= 0 && SDL_GetGamepadButton(g, (SDL_GamepadButton)src)) {
                btn |= kActions[a].bit;
            }
        }
        stick_bytes(sDeadzone[socket][0], SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX), SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY), &lx, &ly);
        stick_bytes(sDeadzone[socket][1], SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX), SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY), &rx, &ry);
    }
    if (sKeySwallow) {
        const bool *ks = SDL_GetKeyboardState(NULL);
        int p, held = 0;
        for (p = 0; p < 2; p++) {
            for (a = 0; a < PORT_ACTIONS; a++) {
                held = held || (sKeys[p][a] > 0 && sKeys[p][a] < SDL_SCANCODE_COUNT && ks[sKeys[p][a]]);
            }
        }
        sKeySwallow = held;
    }
    /* the settings window takes the keyboard while it is open, and the online chat while a line is typed (the
       controller still plays then) */
    if (!gPortOverlayOpen && !gPortChatTyping && !sKeySwallow) {
        const bool *ks = SDL_GetKeyboardState(NULL);
        const int *k = sKeys[socket];
        for (a = 0; a < PORT_PAD_BUTTONS; a++) {
            if (k[a] > 0 && k[a] < SDL_SCANCODE_COUNT && ks[k[a]] && !(gPortTboTyping && typing_key(k[a]))) {
                btn |= kActions[a].bit;
            }
        }
#define HELD(n) (k[n] > 0 && k[n] < SDL_SCANCODE_COUNT && ks[k[n]] && !(gPortTboTyping && typing_key(k[n])))
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
    }
    if (socket == 0 && getenv("BT3_STICK_TEST") != NULL) { /* the numbers, once: what was read, what was passed, what the game has */
        static const char *const dirs[5] = {"-", "up", "down", "left", "right"};
        extern unsigned char gPad[]; /* the game's (include/sys/pad.h): 0x130 left x, y, 0x138 right x, y, 0x148 held */
        static int reads;
        if (++reads == 40 && g != NULL) {
            PortStickView v;
            int s;
            PortInput_StickView(0, &v);
            for (s = 0; s < 2; s++) {
                fprintf(stderr, "sticks: %s: raw %6d %6d (%+.3f %+.3f), deadzone %d -> bytes %3u %3u -> game %+.4f %+.4f, direction %s\n",
                        s == 0 ? "left " : "right", SDL_GetGamepadAxis(g, s == 0 ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_RIGHTX),
                        SDL_GetGamepadAxis(g, s == 0 ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_RIGHTY), v.raw[s][0], v.raw[s][1],
                        sDeadzone[0][s], v.bytes[s][0], v.bytes[s][1], v.game[s][0], v.game[s][1], dirs[v.dir[s]]);
            }
        } else if (reads == 44) {
            float f[4];
            unsigned held;
            memcpy(f, gPad + 0x130, sizeof(f));
            memcpy(&held, gPad + 0x148, sizeof(held));
            fprintf(stderr, "sticks: the game's pad 1 has left %+.4f %+.4f, right %+.4f %+.4f, held %08X\n", f[0], f[1], f[2], f[3], held);
        }
    }
    clash_square(socket, &btn, &lx, &ly);
    memset(data, 0, 18);
    data[0] = (unsigned char)(~btn & 0xFF);       /* active low */
    data[1] = (unsigned char)(~(btn >> 8) & 0xFF);
    data[2] = rx; data[3] = ry; data[4] = lx; data[5] = ly;
    return 18;
}
