#include "common.h"
#include "sys/late_a.h"
#include "sys/heap.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/snd.h"

/*
 * ShenCfm, 0x2BEB20..0x2BF370: the confirmation window of the wish screen ("reconfir": yes / no / details).
 * Built with -G0. See sys/late_a.h.
 *
 * A separate object from late_a.c: its strings (0x2FC0F0..0x2FC280) repeat "fl_on_start", "fl_off_start" and
 * "fl_ok", which one translation unit would have emitted once. The first two come first because the inline
 * helper that uses them is defined above ShenCfm_Init.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 sprintf(char *dst, const char *fmt, ...);

extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Flash_Create(Flash *flash, void *data, void *tex);
extern void Flash_Destroy(Flash *flash);
extern void Flash_Advance(Flash *flash);
extern void Flash_Draw(Flash *flash);
extern void Flash_Play(Flash *flash, s32 arg);
extern void Flash_GotoLabel(Flash *flash, char *label, s32 arg);
extern void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out);
extern void Flash_ClipGotoLabel(Flash *flash, FlashRef *ref, char *label);
extern void Flash_ClipSetColor(Flash *flash, FlashRef *ref, f32 v);
extern void Flash_ClipSetUv(Flash *flash, FlashRef *ref, FlashUv *uv);

/* The item details page, in the menu overlay (DBZP.BIN). */
extern void ItemHelp_Init(void *pack);
extern void ItemHelp_Term(void);
extern void ItemHelp_Draw(s32 item);
extern void ItemHelp_Open(void);
extern void ItemHelp_Close(void);

/* Defined here: this object's .data (0x2EB34C). */
ShenCfm *gShenCfm = NULL;

/* A debug build loaded each resource from the host by name; here only the names are left. */
static inline void ShenCfm_DebugFile(const char *name) {
}

/* Starts the "lit" or "dim" animation of a choice plate. */
static inline void ShenCfm_PlatePlay(Flash *flash, FlashRef *ref, s32 on) {
    if (on) {
        Flash_ClipGotoLabel(flash, ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, ref, "fl_off_start");
    }
}

/* Allocates the window and builds its movie from a pack file; also sets up the item details page. */
void ShenCfm_Init(u32 *pack) {
    FlashTexRes *res = NULL;

    gShenCfm = Heap_Alloc(sizeof(ShenCfm), 0x20, 0, HEAP_ANY);
    memset(gShenCfm, 0, sizeof(ShenCfm));

    ShenCfm_DebugFile("host:data/ps2/test/main/SR/reconfir/sr_reconfir_tex_PS2_.dbt");
    res = PACK_AT(pack, 2);
    Res_RelocateOffsets(&res, res, res);
    gShenCfm->tex[0] = res->tex;
    gShenCfm->tex[1] = res->tex + FLASH_TEX_SIZE;
    gShenCfm->tex[2] = res->tex + FLASH_TEX_SIZE * 2;
    gShenCfm->tex[4] = res->tex + FLASH_TEX_SIZE * 3;

    ShenCfm_DebugFile("host:data/ps2/test/main/SR/reconfir/sr_reconfir_text_JP_PS2_.dbt");
    res = PACK_AT(pack, 3);
    Res_RelocateOffsets(&res, res, res);
    gShenCfm->tex[3] = res->tex;
    gShenCfm->tex[5] = res->tex + FLASH_TEX_SIZE;

    ShenCfm_DebugFile("host:data/ps2/test/main/SR/reconfir/reconfir_PS2_.fod");
    Flash_Create(gShenCfm->flash, PACK_AT(pack, 1), gShenCfm->tex);
    Flash_Play(gShenCfm->flash, 1);

    ShenCfm_DebugFile("host:data/ps2/test/main/SR/reconfir/ez_item_details_JP_PS2_.pak");
    ItemHelp_Init(PACK_AT(pack, 4));
}

/* Frees the window and the item details page. */
void ShenCfm_Term(void) {
    s32 i;

    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Destroy(&gShenCfm->flash[i]);
    }
    ItemHelp_Term();
    if (gShenCfm != NULL) {
        Heap_Free(gShenCfm);
        gShenCfm = NULL;
    }
}

/* Non-zero when the save already has what the wish asked about gives. */
s32 ShenCfm_IsOwned(ShenCfm *cfm) {
    switch (cfm->kind) {
    case SHEN_WISH_ITEM:
        return gSaveData->item[cfm->id] & 1;
    case SHEN_WISH_STAGE:
        return (gSaveData->stageBits >> cfm->id) & 1;
    case SHEN_WISH_CHARA:
        return (SAVE_CHARA_WORD(gSaveData, cfm->id) >> (cfm->id % 64)) & 1;
    case SHEN_WISH_MONEY:
        return 0;
    }
    return 0;
}

/* Sets the texture rectangle of a named clip. */
static inline void ShenCfm_SetClipUv(Flash *flash, char *parent, char *name, FlashUv *uv) {
    FlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/*
 * Per frame: steps and draws the window; an item also gets its details page drawn. "Details" is dimmed for a
 * wish that is not an item, "yes" for a wish the save already has.
 */
void ShenCfm_Draw(void) {
    FlashUv uv;
    char name[0x100];
    s32 i;

    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Advance(&gShenCfm->flash[i]);
    }
    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i << 5;
        uv.x1 = 0x100;
        uv.y1 = (i << 5) + 0x20;
        sprintf(name, "mc_reconfir_plate_%d", i + 1);
        ShenCfm_SetClipUv(gShenCfm->flash, name, "mc_reconfir_text_on", &uv);
        ShenCfm_SetClipUv(gShenCfm->flash, name, "mc_reconfir_text_off", &uv);
    }
    if (!gShenCfm->hasDetail) {
        char name2[0x100];
        FlashRef ref;

        sprintf(name2, "mc_reconfir_plate_%d", 3);
        Flash_FindLabel(gShenCfm->flash, NULL, name2, &ref);
        Flash_ClipSetColor(gShenCfm->flash, &ref, 0.5f);
    }
    if (ShenCfm_IsOwned(gShenCfm)) {
        char name2[0x100];
        FlashRef ref;

        sprintf(name2, "mc_reconfir_plate_%d", 1);
        Flash_FindLabel(gShenCfm->flash, NULL, name2, &ref);
        Flash_ClipSetColor(gShenCfm->flash, &ref, 0.5f);
    }
    for (i = 0; i < SHEN_FLASH_COUNT; i++) {
        Flash_Draw(&gShenCfm->flash[i]);
    }
    if (gShenCfm->kind == SHEN_WISH_ITEM) {
        ItemHelp_Draw(gShenCfm->id);
    }
}

/* Lights or dims the plate under the cursor. */
static inline void ShenCfm_SetPlate(ShenCfm *cfm, s32 on) {
    FlashRef ref;
    char name[0x100];

    sprintf(name, "mc_reconfir_plate_%d", cfm->cursor + 1);
    Flash_FindLabel(cfm->flash, NULL, name, &ref);
    ShenCfm_PlatePlay(cfm->flash, &ref, on);
}

/* Opens the window for a wish, with the cursor on "no". */
void ShenCfm_Open(s32 kind, s32 id) {
    gShenCfm->id = id;
    gShenCfm->kind = kind;
    ShenCfm_SetPlate(gShenCfm, 0);
    gShenCfm->cursor = 1;
    ShenCfm_SetPlate(gShenCfm, 1);
    gShenCfm->hasDetail = kind == SHEN_WISH_ITEM;
    Flash_GotoLabel(gShenCfm->flash, "fl_window_in", 1);
}

/* Keeps the cursor inside 0..2 by wrapping around. */
static inline void ShenCfm_WrapCursor(s32 *cursor) {
    s32 max = 2;

    if (*cursor < 0) {
        *cursor = max;
    } else if (*cursor > max) {
        *cursor = 0;
    }
}

/* Moves the cursor by `dir` (-1 or 1), wrapping around. */
void ShenCfm_MoveCursor(ShenCfm *cfm, s32 dir) {
    ShenCfm_SetPlate(cfm, 0);
    cfm->detail = 0;
    cfm->cursor += dir;
    ShenCfm_WrapCursor(&cfm->cursor);
    ShenCfm_SetPlate(cfm, 1);
    Snd_PlaySe(1, 0);
}

/* Plays "fl_ok" on the plate under the cursor. */
void ShenCfm_PlayPlateOk(ShenCfm *cfm) {
    FlashRef ref;
    char name[0x100];

    sprintf(name, "mc_reconfir_plate_%d", cfm->cursor + 1);
    Flash_FindLabel(cfm->flash, NULL, name, &ref);
    Flash_ClipGotoLabel(cfm->flash, &ref, "fl_ok");
}

/*
 * One frame of the window's input (pad 0). Returns 1 when an answer was given: *answer = 0 for "yes", 1 for
 * "no" or cancel. A choice that is dimmed only buzzes; "details" shows the item page until confirm or cancel.
 */
s32 ShenCfm_Update(s32 *answer) {
    switch (gShenCfm->detail) {
    case 0:
        if (gPad[0].gameRepeat & PADG_UP) {
            ShenCfm_MoveCursor(gShenCfm, -1);
        } else if (gPad[0].gameRepeat & PADG_DOWN) {
            ShenCfm_MoveCursor(gShenCfm, 1);
        } else if (gPad[0].gamePressed & PADG_CROSS) {
            ShenCfm_PlayPlateOk(gShenCfm);
            if ((!gShenCfm->hasDetail && gShenCfm->cursor == 2) || (ShenCfm_IsOwned(gShenCfm) && gShenCfm->cursor == 0)) {
                Snd_PlaySe(1, 7);
            } else {
                Snd_PlaySe(1, 1);
                if (gShenCfm->cursor == 2) {
                    gShenCfm->detail = 1;
                    ItemHelp_Open();
                } else {
                    *answer = gShenCfm->cursor;
                    return 1;
                }
            }
        } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
            *answer = 1;
            Snd_PlaySe(1, 2);
            return 1;
        }
        break;
    case 1:
        if (SHEN_PRESSED_CROSS_OR_TRIANGLE()) {
            gShenCfm->detail = 0;
            ItemHelp_Close();
        }
        break;
    }
    return 0;
}

/* Plays the window's closing animation. */
void ShenCfm_Close(void) {
    Flash_GotoLabel(gShenCfm->flash, "fl_window_out", 1);
}
