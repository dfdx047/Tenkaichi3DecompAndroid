/* The exit question uses the same animated Yes / No window as the memory-card flow. The game owns gDialog,
   so each call temporarily selects our separate Dialog and restores the screen's one before returning. */
#include "types.h"
#include "sys/dialog.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/loading.h"
#include "quit_dialog.h"
#include <stdlib.h>

extern s32 Snd_PlaySe(u32 mask, s32 id);
extern void *Sprite_Unpack(void *src, void *dst, s32 *rawSize);
extern int Port_NetActive(void);
extern s32 sceGsSyncPath(s32 mode, u16 timeout);
#define QUIT_PACK_AT(pack, n) ((u8 *)(pack) + ((((u32 *)(pack))[n] >> 2) << 2))

static Dialog *sQuitDialog;
static void *sQuitArchive, *sQuitResource;
static int sQuitState; /* 0 idle, 1 requested, 2 choosing, 3 cancelling, 4 confirmed, 5 fallback,
                          6 exiting, 7/8 waiting for the final drawing to finish before freeing/exiting */
static int sQuitAction;
static int sQuitFrames;
static int sQuitWaitFrames;
static struct {
    u32 offsets[2];
    u16 text[80];
} sQuitMessage;

static void quit_select(Dialog **previous) {
    *previous = gDialog;
    gDialog = sQuitDialog;
}

static void quit_restore(Dialog *previous) {
    sQuitDialog = gDialog;
    gDialog = previous;
}

static void quit_free(void) {
    Dialog *previous;
    if (sQuitDialog != NULL) {
        quit_select(&previous);
        Dialog_Term();
        quit_restore(previous);
    }
    if (sQuitResource != NULL) { Heap_Free(sQuitResource); sQuitResource = NULL; }
    if (sQuitArchive != NULL) { Heap_Free(sQuitArchive); sQuitArchive = NULL; }
}

void PortQuit_Request(void) {
    if (sQuitState == 0) {
        sQuitAction = 0;
        sQuitWaitFrames = 0;
        sQuitState = 1;
    }
}

int PortQuit_Active(void) { return sQuitState >= 1 && sQuitState != 5; }
int PortQuit_Failed(void) { return sQuitState == 5; }
int PortQuit_Ready(void) { return sQuitState == 2 && sQuitDialog != NULL && (sQuitDialog->flash[0].flags & 2); }
int PortQuit_Cursor(void) { return sQuitDialog != NULL ? sQuitDialog->cursor : 1; }
void PortQuit_Dismiss(void) { if (sQuitState == 5) { sQuitState = 0; } }

void PortQuit_Action(int action) {
    if (sQuitState == 1 || sQuitState == 2) { sQuitAction = action; }
}

/* Archive 0, section 2 is the boot memory-card screen. Its unpacked part 1 is the Dialog resource.
   Returns 0 while the archive partition is still loading, -1 on failure, and 1 when ready. */
static int quit_init(void) {
    static const char message[] = "Close the game?\nUnsaved progress will be lost.";
    Dialog *previous;
    unsigned i;

    /* The native object allocates from the game's heap; an online session must keep that heap deterministic. */
    if (Port_NetActive()) { return -1; }
    if (gProgress == NULL || gProgress->unk0[1] <= 0 || !File_IsPartitionLoaded(1)) { return 0; }
    sQuitArchive = File_LoadSync(gProgress->unk0[1] + 0x19, NULL, 0);
    if (sQuitArchive == NULL) { return -1; }
    sQuitResource = Sprite_Unpack(QUIT_PACK_AT(sQuitArchive, 2), NULL, NULL);
    if (sQuitResource == NULL) { return -1; }
    sQuitMessage.offsets[0] = 0;
    sQuitMessage.offsets[1] = sizeof(sQuitMessage.offsets);
    for (i = 0; i < sizeof(message) && i < sizeof(sQuitMessage.text) / sizeof(sQuitMessage.text[0]); i++) {
        sQuitMessage.text[i] = (u8)message[i];
    }
    quit_select(&previous);
    Dialog_Init((u32 *)QUIT_PACK_AT(sQuitResource, 1), sQuitMessage.offsets, 0);
    if (gDialog != NULL) {
        Dialog_SetLayout(0);
        Dialog_SetMsg(0);
        Dialog_SetChoices(1);
        Dialog_SetCursor(1); /* No is the safe initial choice. */
        Dialog_Start(DIALOG_CMD_OPEN);
    }
    quit_restore(previous);
    return sQuitDialog != NULL ? 1 : -1;
}

void PortQuit_Draw(void) {
    Dialog *previous;
    int action;

    if (sQuitState == 0 || sQuitState == 5) { return; }
    if (sQuitState == 7 || sQuitState == 8) {
        int exitGame = sQuitState == 8;
        sceGsSyncPath(0, 0);
        quit_free();
        sQuitState = 0;
        if (exitGame) { exit(0); }
        return;
    }
    if (sQuitState == 1) {
        int init = quit_init();
        if (init == 0 && ++sQuitWaitFrames < 600) { return; }
        if (init <= 0) {
            quit_free();
            sQuitState = 5;
            return;
        }
        sQuitState = 2;
    }
    quit_select(&previous);
    action = sQuitAction;
    if (sQuitState == 2 && (gDialog->flash[0].flags & 2)) {
        sQuitAction = 0;
        if (action == PORT_QUIT_MOVE) {
            Dialog_SetCursor(1 - gDialog->cursor);
            Snd_PlaySe(1, 0);
        } else if (action == PORT_QUIT_CONFIRM && gDialog->cursor == 0) {
            Dialog_PlayCursorPlate(0, "fl_ok");
            Snd_PlaySe(1, 1);
            sQuitState = 4;
            sQuitFrames = 12;
        } else if (action == PORT_QUIT_CANCEL || action == PORT_QUIT_CONFIRM) {
            Snd_PlaySe(1, action == PORT_QUIT_CANCEL ? 2 : 1);
            Dialog_SetChoices(0);
            Dialog_Start(DIALOG_CMD_CLOSE);
            sQuitState = 3;
        }
    }
    Dialog_Draw(1);
    if (sQuitState == 3 && Dialog_IsClosed()) {
        sQuitState = 7;
    }
    if (sQuitState == 4 && --sQuitFrames <= 0) {
        Dialog_SetChoices(0);
        Dialog_Start(DIALOG_CMD_CLOSE);
        sQuitState = 6;
    }
    if (sQuitState == 6 && Dialog_IsClosed()) {
        sQuitState = 8;
    }
    quit_restore(previous);
}
