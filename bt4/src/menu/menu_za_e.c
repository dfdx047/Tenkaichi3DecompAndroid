#include "common.h"
#include "menu/menu_za.h"

/* The helper's work (overlay .data, 0x3BC9FC). */
DcSave *gDcSave = NULL;

/*
 * Menu overlay DBZP.BIN, 0x3B0C08..0x3B0E04 (the end of the overlay's code): DcSave, the "save the game" card
 * flow that the Data Center's top menu (DcMenu, previous chunk) runs when it is left after the save changed
 * (a custom character was stored or deleted). A whole object: `.data` gDcSave (0x3BC9FC), no `.rodata`.
 */

/* McFlow "finished" callback, registered for both the "done" and the "not done" outcome. */
void DcSave_OnDone(void) {
    gDcSave->flags |= DCSAVE_DONE;
}

/* Creates the confirmation window (from its pack) and the card flow module. */
void DcSave_Init(void *pack) {
    gDcSave = Heap_Alloc(sizeof(DcSave), 0x20, 0, 2);
    memset(gDcSave, 0, sizeof(DcSave));
    Dialog_Init(pack, NULL, 0);
    McFlow_Init(1);
    McFlow_SetDoneCb(0, DcSave_OnDone, NULL);
    McFlow_SetDoneCb(1, DcSave_OnDone, NULL);
}

/* Frees them. If a save was started, the "save changed" flag of gProgress is flipped back. */
void DcSave_Term(void) {
    if (gDcSave->flags & DCSAVE_STARTED) {
        ZAPROG->flags ^= ZAPROG_DIRTY;
    }
    Dialog_Term();
    McFlow_Term();
    if (gDcSave != NULL) {
        Heap_Free(gDcSave);
        gDcSave = NULL;
    }
}

/* Starts the system save (McFlow mode 0: writes the 0x4000-byte gSaveData block after a question). */
void DcSave_Start(void) {
    McFlow_Start(0);
    gDcSave->flags ^= DCSAVE_STARTED;
}

/* Runs the card flow once it was started. */
void DcSave_Update(void) {
    if (gDcSave->flags & DCSAVE_STARTED) {
        gDcSave->state = McFlow_Update();
    }
}

/* The card flow's state (0 = idle), or 0 before the save was started. */
s32 DcSave_GetState(void) {
    if (gDcSave->flags & DCSAVE_STARTED) {
        return gDcSave->state;
    }
    return 0;
}

/* Whether the menu may go on: no save was started, or the flow has ended. */
s32 DcSave_IsDone(void) {
    if (!(gDcSave->flags & DCSAVE_STARTED)) {
        return 1;
    }
    return gDcSave->flags & DCSAVE_DONE;
}

/* Whether the save was started. */
s32 DcSave_IsStarted(void) {
    return gDcSave->flags & DCSAVE_STARTED;
}
