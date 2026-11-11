#include "common.h"
#include "port_variant.h"
#include "menu/menu_c.h"
#include "sys/pad.h"
#include "sys/save.h"
#ifdef PORT
#include <stdlib.h>
#include "story_audit.inc"
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

HistSel *gHistSel = NULL; /* 0x3B12FC */

/*
 * HistSel, 0x33CFC8..0x33F700 (one object; it was written as two halves cut at 0x33E108, the former menu_c.c,
 * which has the rest of the screen and its frame loop HistSel_Run): the saga select of the story mode
 * (progress mode 6).
 */

extern void Voice_StopWithLip(void);

/* BT4 reuses resource 33 for every guest (SLUS 0x33D9E4). The face is
   baked into that portrait. Apply this to every instance, including outgoing
   guests, rather than only the currently selected saga. */
void HistGuide_OriginalPortrait(MFlash *flash, s32 guide) {
    MFlashRef ref;
    char name[64];
    char sub[64];
    if (!PORT_V(0,1) || guide < 0 || guide >= 8) return;
    sprintf(name, "mc_guide_%02d", guide);
    sprintf(sub, "mc_guide_%02d_eye", guide);
    Flash_FindLabel(flash, name, sub, &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);
    sprintf(sub, "mc_guide_%02d_mouth", guide);
    Flash_FindLabel(flash, name, sub, &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);
}

/* Says line voiceLine with the voice set of guide `guide` (0..7 the sagas' guides, 8 this screen's own). */
void HistSel_PlayVoice(void) {
    switch (gHistSel->guide) {
    case 0:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8537, gHistSel->voiceLine);
        break;
    case 1:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8423, gHistSel->voiceLine);
        break;
    case 2:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x83B4, gHistSel->voiceLine);
        break;
    case 3:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x83DB, gHistSel->voiceLine);
        break;
    case 4:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8484, gHistSel->voiceLine);
        break;
    case 5:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8440, gHistSel->voiceLine);
        break;
    case 6:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8402, gHistSel->voiceLine);
        break;
    case 7:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x8466, gHistSel->voiceLine);
        break;
    case 8:
        Voice_PlayWithSubtitle(gHistSel->subtitles[gHistSel->guide], 0x855B, gHistSel->voiceLine);
        break;
    }
}

/* Counts the frames without input: after 3600 the screen's guide says one of four idle lines. */
void HistSel_Idle(void) {
    gHistSel->idle++;
    if (gHistSel->idle == 3600) {
        gHistSel->guide = 8;
        gHistSel->talker = 0;
        gHistSel->voiceLine = Rand_Range(4) + 10;
        HistSel_PlayVoice();
        gHistSel->idle = 0;
    }
}

/* The guide presents the saga under the cursor (one of two lines each); an empty plate stops the voice. */
void HistSel_SayItem(void) {
    switch (gHistSel->items[gHistSel->cursor[1]]) {
    case 0:
        gHistSel->voiceLine = Rand_Range(2) + 0x16;
        HistSel_PlayVoice();
        break;
    case 1:
        gHistSel->voiceLine = Rand_Range(2) + 0x18;
        HistSel_PlayVoice();
        break;
    case 2:
        gHistSel->voiceLine = Rand_Range(2) + 0x1A;
        HistSel_PlayVoice();
        break;
    case 3:
        gHistSel->voiceLine = Rand_Range(2) + 0x1C;
        HistSel_PlayVoice();
        break;
    case 4:
        gHistSel->voiceLine = Rand_Range(2) + 0x20;
        HistSel_PlayVoice();
        break;
    case 5:
        gHistSel->voiceLine = Rand_Range(2) + 0x1E;
        HistSel_PlayVoice();
        break;
    case 6:
        gHistSel->voiceLine = Rand_Range(2) + 0x22;
        HistSel_PlayVoice();
        break;
    case 7:
        gHistSel->voiceLine = Rand_Range(2) + 0x24;
        HistSel_PlayVoice();
        break;
    case 8:
        gHistSel->voiceLine = 0x2F;
        HistSel_PlayVoice();
        break;
    default:
        gHistSel->voiceLine = -1;
        Voice_StopWithLip();
        break;
    }
}

/* Each saga whose outro was seen and that has not had its event (slot flag 0x20) gets a 4 % chance, in order;
   the first hit becomes this visit's event saga. */
void HistSel_RollEvent(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        SaveSlot *slot = &gSaveData->slot[i];
        s32 flags = slot->flags;
        s32 done = flags & MSLOT_EVENT_DONE;

        flags &= MSLOT_OUTRO_SEEN;
        if (flags && !done && (s32)Rand_Range(100) < 4) {
            gHistSel->guest = i;
            return;
        }
    }
}

#define SEL_RES(n) \
    res = (MTexRes *)MPACK_AT(gHistSel->res, n); \
    Res_RelocateOffsets(&res, res, res)

#define SEL_TEX(n, k) gHistSel->tex[n] = MTEX(res, k)

#define SEL_ICONS(a, b, c) \
    SEL_TEX(a, 0); \
    SEL_TEX(b, 1); \
    SEL_TEX(c, 3)

/* Loads the screen (section `section` of archive 2), lists the unlocked sagas and works out the completion
   percentage of the story mode. */
void HistSel_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    s32 j;
    s32 cleared;

    gHistSel = Heap_Alloc(0x258, 0x20, 0, 2);
    memset(gHistSel, 0, 0x258);
    gHistSel->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gHistSel->res = Sprite_Unpack(gHistSel->pack, NULL, NULL);
    StreamSe_PlayDefault(0, 0x158);
    SEL_RES(1);
    gHistSel->bg = res;
    SEL_RES(2);
    SEL_TEX(30, 0);
    SEL_TEX(29, 1);
    SEL_TEX(22, 2);
    SEL_TEX(0, 3);
    SEL_RES(3);
    SEL_TEX(1, 0);
    SEL_TEX(2, 1);
    SEL_TEX(3, 2);
    SEL_TEX(4, 3);
    SEL_RES(4);
    SEL_TEX(23, 0);
    SEL_TEX(24, 1);
    SEL_TEX(25, 2);
    SEL_TEX(26, 3);
    SEL_TEX(27, 4);
    SEL_TEX(28, 5);
    SEL_RES(5);
    SEL_TEX(74, 0);
    SEL_TEX(75, 1);
    SEL_TEX(5, 2);
    SEL_TEX(71, 3);
    SEL_TEX(70, 4);
    SEL_TEX(73, 5);
    SEL_TEX(72, 6);
    for (i = 0; i < 16; i++) {
        gHistSel->tex[6 + i] = MTEX(res, i) + 7 * 0x40;
    }
    SEL_RES(6);
    SEL_ICONS(31, 32, 33);
    SEL_RES(7);
    SEL_TEX(36, 0);
    SEL_RES(8);
    SEL_TEX(44, 0);
    SEL_TEX(40, 1);
    SEL_TEX(43, 2);
    SEL_TEX(42, 3);
    SEL_TEX(37, 4);
    SEL_TEX(76, 5);
    SEL_TEX(79, 6);
    SEL_TEX(78, 7);
    SEL_TEX(81, 8);
    SEL_RES(9);
    SEL_TEX(39, 0);
    SEL_TEX(82, 1);
    SEL_TEX(34, 2);
    SEL_TEX(35, 3);
    SEL_TEX(41, 4);
    SEL_TEX(45, 5);
    SEL_TEX(38, 6);
    SEL_TEX(77, 8);
    for (i = 0; i < 8; i++) {
        SEL_RES(33);
        switch (i) {
        case 0:
            SEL_ICONS(46, 47, 48);
            break;
        case 1:
            SEL_ICONS(49, 50, 51);
            break;
        case 2:
            SEL_ICONS(52, 53, 54);
            break;
        case 3:
            SEL_ICONS(55, 57, 56);
            break;
        case 4:
            SEL_ICONS(58, 59, 60);
            break;
        case 5:
            SEL_ICONS(61, 62, 63);
            break;
        case 6:
            SEL_ICONS(64, 65, 66);
            break;
        case 7:
            SEL_ICONS(67, 68, 69);
            break;
        }
    }
    SEL_RES(12);
    SEL_TEX(80, 1);
    Flash_Create(&gHistSel->flash[0], MPACK_AT(gHistSel->res, 10), gHistSel->tex);
    Flash_Play(&gHistSel->flash[0], 1);
    SEL_RES(11);
    IconWin_Init(MPACK_AT(gHistSel->res, 13), res);
    IconWin_Open();
    for (i = 0; i < 9; i++) {
        if (i == 8) {
            gHistSel->msgText[i] = MPACK_AT(gHistSel->res, 15);
            gHistSel->subtitles[i] = MPACK_AT(gHistSel->res, 24);
        } else {
            gHistSel->msgText[i] = MPACK_AT(gHistSel->res, 16 + i);
            gHistSel->subtitles[i] = MPACK_AT(gHistSel->res, 25 + i);
        }
    }
    MsgWin_Init(MPACK_AT(gHistSel->res, 14), NULL, 1, 0);
    MsgWin_Open();
    {
        s32 count[8] = { 3, 4, 7, 5, 16, 5, 4, 4 };

        gHistSel->itemCount = 0;
        cleared = 0;
#ifdef PORT
        /* PC build: saves made before the fix in HistResult_BuildRewards can hold a saga with episodes open that is
           not on the list. It is listed here, as new, and announced, as the result screen would have done. */
        for (i = 0; i < 8; i++) {
            if (gSaveData->slot[i].val[0] != 0 && !(gSaveData->slot[i].flags & MSLOT_LISTED)) {
                gSaveData->slot[i].flags |= MSLOT_LISTED | MSLOT_NEW;
                gSaveData->unlockFlags |= MUNLOCK_HIST_NEW_SAGA;
            }
        }
#endif
        for (i = 0; i < 9; i++) {
            SaveSlot *slot = &gSaveData->slot[i];

            if (slot->flags & 1) {
                gHistSel->items[gHistSel->itemCount++] = i;
            }
            if (i < 8) {
                for (j = 0; j < count[i]; j++) {
                    if (gSaveData->slot[i].val[1] & (s32)(1U << j)) {
                        cleared++;
                    }
                }
            }
        }
    }
    for (i = gHistSel->itemCount; i < 3; i++) {
        gHistSel->items[i] = 9;
        gHistSel->itemCount++;
    }
    gHistSel->percent = (f32)cleared / 48.0f * 100.0f;
    if (cleared != 0 && gHistSel->percent == 0) {
        gHistSel->percent = 1;
    }
    if (gHistSel->percent == 100 && !(gSaveData->unlockFlags & 0x100)) {
        gHistSel->flags |= HISTSEL_COMPLETE;
    }
    for (i = 0; i < gHistSel->itemCount; i++) {
        if (gHistSel->items[i] == gProgress->subMenu) {
            gHistSel->cursor[1] = i;
            break;
        }
    }
    gHistSel->top = gHistSel->cursor[1] - 1;
    if (gHistSel->top < 0) {
        gHistSel->top += gHistSel->itemCount;
    }
    gHistSel->voiceLine = -1;
    gHistSel->guide = 8;
    gHistSel->guest = -1;
    for (i = 0; i < 2; i++) {
        gHistSel->blink[i] = Rand_Range(0x20);
    }
    if (gSaveData->unlockFlags & 0x80) {
        /* BT4 removes the random guide event at SLUS 0x33E09C. */
        if (gHistSel->guest >= 0) {
            gHistSel->flags |= HISTSEL_EVENT;
        }
    } else {
        gHistSel->flags |= HISTSEL_FIRST_VISIT;
    }
    gHistSel->seTimer = Rand_Range(10) * 60 + 300;
}

/*
 * 0x33E108..0x33F700: what was the tail half (menu_c.c; the voice helpers and Init are above). The menu of the
 * history mode (progress mode 6): Goku as guide, a two-item menu (episodes / level), a ring of the unlocked sub menus of which three plates are visible, three level plates,
 * and the completion percentage. A guide script (menu_c_b.c) takes the pad away while it runs.
 */

/*
 * HistSel_Update only matches when the compiler has seen the definition of HistSel_PlayVoice, the first
 * function of this object (a `bnel` instead of a `bne` after the Voice_GetStat call): while the object was two
 * files, the tail carried a copy of it between ASM_STUB_BEGIN / ASM_STUB_END.
 */

#define HM_FREE(p) \
    if ((p) != NULL) { \
        Heap_Free(p); \
        (p) = NULL; \
    }

/* Frees the screen. */
void HistSel_Term(void) {
    s32 i;

    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < HISTSEL_FLASH_NUM; i++) {
        Flash_Destroy(&gHistSel->flash[i]);
    }
    HM_FREE(gHistSel->res);
    HM_FREE(gHistSel);
}

/* Sets up every clip of the movie for this frame and draws it and the two windows. */
void HistSel_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    char sub[64];
    s32 i;
    MFlash *flash;

#ifdef PORT
    Port_MenuPage(13); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gHistSel->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gHistSel->bg, 0, 0, 0x80);
#endif
    flash = &gHistSel->flash[0];
    Flash_FindLabel(flash, "mc_guide_gokuu", "mc_guide_gokuu_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gHistSel->blink[0], 0);
    Flash_FindLabel(flash, "mc_guide_gokuu", "mc_guide_gokuu_mouth", &ref);
    if (gHistSel->talker == 0) {
        FlashAnim_Talk(flash, &ref, &gHistSel->talk[0], 0);
    } else {
        FlashAnim_ShowNext2(flash, &ref, 0);
    }
    if (gHistSel->guest >= 0) {
        sprintf(name, "mc_guide_%02d", gHistSel->guest);
        sprintf(sub, "mc_guide_%02d_eye", gHistSel->guest);
        Flash_FindLabel(flash, name, sub, &ref);
        FlashAnim_Blink(flash, &ref, &gHistSel->blink[1], 0);
        sprintf(sub, "mc_guide_%02d_mouth", gHistSel->guest);
        Flash_FindLabel(flash, name, sub, &ref);
        if (gHistSel->talker == 1) {
            FlashAnim_Talk(flash, &ref, &gHistSel->talk[1], 0);
        } else {
            FlashAnim_ShowNext2(flash, &ref, 0);
        }
    }
    for (i = 0; i < 8; i++) HistGuide_OriginalPortrait(flash, i);
    if (gHistSel->flags & 0x80) {
        gHistSel->foregroundAlpha -= 0.075f;
        if (gHistSel->foregroundAlpha < 0.0f) gHistSel->foregroundAlpha = 0.0f;
    } else {
        gHistSel->foregroundAlpha = 1.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_taki", &ref);
    Flash_ClipSetAlpha(flash, &ref, gHistSel->foregroundAlpha);
    Num_Draw(flash, "mc_tassei_num_%d", 0, 3, gHistSel->percent, 0x20, 0x20, 0);

    for (i = 0; i < 2; i++) {
        uv.x0 = i * 0x20;
        uv.y0 = 0;
        uv.x1 = uv.x0 + 0x20;
        uv.y1 = 0x20;
        switch (i) {
        case 0:
            Flash_FindLabel(flash, "mc_yajirusi_left", "mc_yajirusi_icon_left", &ref);
            if (gHistSel->items[gHistSel->top] == HISTSEL_ITEM_NONE) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
            break;
        case 1:
            Flash_FindLabel(flash, "mc_yajirusi_right", "mc_yajirusi_icon_right", &ref);
            if (gHistSel->items[(gHistSel->cursor[1] + 1) % gHistSel->itemCount] == HISTSEL_ITEM_NONE) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
            break;
        }
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    for (i = 0; i < 2; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_level_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_level_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_level_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_scenario_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetTex(flash, &ref, gHistSel->items[(gHistSel->top + i) % gHistSel->itemCount]);
    }
    Flash_FindLabel(flash, NULL, "mc_scenario_4", &ref);
    Flash_ClipSetTex(flash, &ref, gHistSel->items[gHistSel->extra]);

    for (i = 0; i < 2; i++) {
        sprintf(name, "mc_scenario_text_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (i != 0) {
            uv.x0 = 0;
            uv.y0 = (gHistSel->items[gHistSel->prev] % 8) * 0x20;
            uv.x1 = 0x200;
            uv.y1 = uv.y0 + 0x20;
            Flash_ClipSetTex(flash, &ref, gHistSel->items[gHistSel->prev] / 8);
        } else {
            uv.x0 = 0;
            uv.y0 = (gHistSel->items[gHistSel->cursor[1]] % 8) * 0x20;
            uv.x1 = 0x200;
            uv.y1 = uv.y0 + 0x20;
            Flash_ClipSetTex(flash, &ref, gHistSel->items[gHistSel->cursor[1]] / 8);
        }
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    Flash_FindLabel(flash, NULL, "mc_icon_new", &ref);
    if (MSAVE->slot[gHistSel->items[gHistSel->cursor[1]]].flags & MSLOT_NEW) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }

    {
        f32 speed[3] = { 0.35555556f, 0.35555556f, 0.53333333f };

        uv.x0 = 0;
        uv.y0 = 0;
        uv.x1 = 0x40;
        uv.y1 = 0x40;
        for (i = 0; i < 3; i++) {
            sprintf(name, "mc_yuragi_%d", i + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            FlashAnim_Scroll(flash, &ref, &uv, NULL, &gHistSel->scroll[i], 0.0f, speed[i]);
        }
    }

    for (i = 0; i < HISTSEL_FLASH_NUM; i++) {
        Flash_Draw(&gHistSel->flash[i]);
    }
    IconWin_Draw();
    MsgWin_SetText(gHistSel->msgText[gHistSel->guide]);
    MsgWin_Draw(0, 0, gHistSel->voiceLine);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Sends the plate under a cursor to a label: kind 0 the menu plate, kind 2 the level plate. */
void HistSel_PlateGoto(s32 idx, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gHistSel->flash[idx];

    switch (kind) {
    case 0:
        sprintf(name, "mc_menu_plate_%d", gHistSel->cursor[0] + 1);
        break;
    case 2:
        sprintf(name, "mc_level_plate_%d", gHistSel->cursor[2] + 1);
        break;
    default:
        return;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Steps the movie, plays the sound it asks for and the ambient sound; after the greeting Goku reads the item. */
void HistSel_Update(void) {
    s32 i;

    if (gHistSel->flags & HISTSEL_GREETED) {
        if (gHistSel->voiceLine >= 14 && gHistSel->voiceLine <= 19) {
            if (Voice_GetStat(0) == 5) {
                gHistSel->talker = 0;
                gHistSel->voiceLine = gHistSel->cursor[gHistSel->focus] + 5;
                HistSel_PlayVoice();
            }
        }
    }
    for (i = 0; i < HISTSEL_FLASH_NUM; i++) {
        Flash_Advance(&gHistSel->flash[i]);
    }
    if (gHistSel->flash[0].se & 1) {
        Snd_PlaySe(2, 0xF);
    }
    if (--gHistSel->seTimer == -1) {
        Snd_PlaySe(2, 0xD);
        gHistSel->seTimer = Rand_Range(10) * 60 + 300;
    }
}

/* Pad 0, by focus: the two-item menu, the ring of sub menus (left / right), the level plates. */
void HistSel_Input(s32 *result) {
    if (!(gHistSel->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gHistSel->flags & HISTSEL_STARTED)) {
        HistSel_PlateGoto(0, 0, "fl_on_start");
        gHistSel->flags |= HISTSEL_STARTED;
    }
    switch (gHistSel->focus) {
    case 0:
        if (gPad[0].gameRepeat & 8) {
            gHistSel->idle = 0;
            HistSel_PlateGoto(0, 0, "fl_off_start");
            gHistSel->cursor[gHistSel->focus]--;
            if (gHistSel->cursor[gHistSel->focus] < 0) {
                gHistSel->cursor[gHistSel->focus] = 1;
            }
            HistSel_PlateGoto(0, 0, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[gHistSel->focus] + 5;
            HistSel_PlayVoice();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 4) {
            gHistSel->idle = 0;
            HistSel_PlateGoto(0, 0, "fl_off_start");
            gHistSel->cursor[gHistSel->focus]++;
            if (gHistSel->cursor[gHistSel->focus] >= 2) {
                gHistSel->cursor[gHistSel->focus] = 0;
            }
            HistSel_PlateGoto(0, 0, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[gHistSel->focus] + 5;
            HistSel_PlayVoice();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            gHistSel->idle = 0;
            HistSel_PlateGoto(0, 0, "fl_ok");
            switch (gHistSel->cursor[gHistSel->focus]) {
            case 0:
                Flash_GotoLabel(&gHistSel->flash[0], "fl_scenario_in", 1);
                if (MSAVE->unlockFlags & MUNLOCK_HIST_NEW_SAGA) {
                    MSAVE->unlockFlags &= ~MUNLOCK_HIST_NEW_SAGA;
                    gHistSel->voiceLine = 0x2A;
                } else {
                    gHistSel->voiceLine = Rand_Range(3) + 0x13;
                }
                HistSel_PlayVoice();
                gHistSel->focus = 1;
                break;
            case 1:
                Flash_GotoLabel(&gHistSel->flash[0], "fl_level_in", 1);
                gHistSel->cursor[2] = MSAVE->level;
                HistSel_PlateGoto(0, 2, "fl_on_start");
                gHistSel->voiceLine = gHistSel->cursor[2] + 7;
                HistSel_PlayVoice();
                gHistSel->focus = 2;
                break;
            }
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gHistSel->idle = 0;
            gHistSel->script = 0x33;
            *result = 0;
            Snd_PlaySe(1, 2);
        } else {
            HistSel_Idle();
        }
        break;
    case 1:
        if (gPad[0].gameRepeat & 1) {
            gHistSel->idle = 0;
            if (gHistSel->items[gHistSel->top] == HISTSEL_ITEM_NONE) {
                return;
            }
            Flash_GotoLabel(&gHistSel->flash[0], "fl_scenario_right", 1);
            gHistSel->extra = (gHistSel->cursor[gHistSel->focus] + 1) % gHistSel->itemCount;
            gHistSel->prev = gHistSel->cursor[gHistSel->focus];
            gHistSel->cursor[gHistSel->focus]--;
            if (gHistSel->cursor[gHistSel->focus] < 0) {
                gHistSel->cursor[gHistSel->focus] = gHistSel->itemCount - 1;
            }
            gHistSel->top = gHistSel->cursor[gHistSel->focus] - 1;
            if (gHistSel->top < 0) {
                gHistSel->top += gHistSel->itemCount;
            }
            HistSel_SayItem();
            Snd_PlaySe(2, 0xA);
        } else if (gPad[0].gameRepeat & 2) {
            gHistSel->idle = 0;
            if (gHistSel->items[(gHistSel->cursor[gHistSel->focus] + 1) % gHistSel->itemCount] == HISTSEL_ITEM_NONE) {
                return;
            }
            Flash_GotoLabel(&gHistSel->flash[0], "fl_scenario_left", 1);
            gHistSel->extra = gHistSel->top;
            gHistSel->prev = gHistSel->cursor[gHistSel->focus];
            gHistSel->cursor[gHistSel->focus]++;
            if (gHistSel->cursor[gHistSel->focus] >= gHistSel->itemCount) {
                gHistSel->cursor[gHistSel->focus] = 0;
            }
            gHistSel->top = gHistSel->cursor[gHistSel->focus] - 1;
            if (gHistSel->top < 0) {
                gHistSel->top += gHistSel->itemCount;
            }
            HistSel_SayItem();
            Snd_PlaySe(2, 0xA);
        } else if (gPad[0].gamePressed & 0x200) {
            gHistSel->idle = 0;
            switch (gHistSel->items[gHistSel->cursor[1]]) {
            case HISTSEL_ITEM_NONE:
                Snd_PlaySe(1, 7);
                break;
            case HISTSEL_ITEM_ENDING:
                MSAVE->slot[8].flags &= ~MSLOT_NEW;
                *result = 2;
                gHistSel->flags |= HISTSEL_CHOSEN;
                gHistSel->flags |= HISTSEL_LEAVING;
                gHistSel->timer = 15;
                Snd_PlaySe(2, 0xB);
                break;
            default:
                if (MSAVE->slot[gHistSel->items[gHistSel->cursor[1]]].flags & MSLOT_INTRODUCED) {
                    gHistSel->script = 0x65;
                } else {
                    gHistSel->script = 0x97;
                }
                Snd_PlaySe(2, 0xB);
                break;
            }
        } else if (gPad[0].gamePressed & 0x400) {
            gHistSel->idle = 0;
            Flash_GotoLabel(&gHistSel->flash[0], "fl_scenario_cansel", 1);
            HistSel_PlateGoto(0, 0, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[0] + 5;
            HistSel_PlayVoice();
            gHistSel->focus = 0;
            Snd_PlaySe(1, 2);
        } else {
            HistSel_Idle();
        }
        break;
    case 2:
        if (gPad[0].gameRepeat & 8) {
            gHistSel->idle = 0;
            HistSel_PlateGoto(0, 2, "fl_off_start");
            gHistSel->cursor[gHistSel->focus]--;
            if (gHistSel->cursor[gHistSel->focus] < 0) {
                gHistSel->cursor[gHistSel->focus] = 2;
            }
            HistSel_PlateGoto(0, 2, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[gHistSel->focus] + 7;
            HistSel_PlayVoice();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gameRepeat & 4) {
            gHistSel->idle = 0;
            HistSel_PlateGoto(0, 2, "fl_off_start");
            gHistSel->cursor[gHistSel->focus]++;
            if (gHistSel->cursor[gHistSel->focus] >= 3) {
                gHistSel->cursor[gHistSel->focus] = 0;
            }
            HistSel_PlateGoto(0, 2, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[gHistSel->focus] + 7;
            HistSel_PlayVoice();
            Snd_PlaySe(1, 0);
        } else if (gPad[0].gamePressed & 0x200) {
            gHistSel->idle = 0;
            MSAVE->level = gHistSel->cursor[gHistSel->focus];
            Flash_GotoLabel(&gHistSel->flash[0], "fl_level_out", 1);
            HistSel_PlateGoto(0, 2, "fl_ok");
            HistSel_PlateGoto(0, 0, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[0] + 5;
            HistSel_PlayVoice();
            gHistSel->focus = 0;
            Snd_PlaySe(1, 1);
        } else if (gPad[0].gamePressed & 0x400) {
            gHistSel->idle = 0;
            Flash_GotoLabel(&gHistSel->flash[0], "fl_level_out", 1);
            HistSel_PlateGoto(0, 2, "fl_off_start");
            HistSel_PlateGoto(0, 0, "fl_on_start");
            gHistSel->voiceLine = gHistSel->cursor[0] + 5;
            HistSel_PlayVoice();
            gHistSel->focus = 0;
            Snd_PlaySe(1, 2);
        } else {
            HistSel_Idle();
        }
        break;
    }
}

/* The history menu's frame loop. Returns 0 to go back to the main menu, 1 when a sub menu was chosen
   (gProgress->subMenu), 2 for the ending movie. */
s32 HistSel_Run(s32 section) {
    s32 result = 1;
#ifdef PORT
    s32 previewFrames=0;
#endif

    #ifdef PORT
    if(getenv("BT4_TEST_STORY_GUIDE")){
        Save_UnlockAll(gSaveData);
        gProgress->subMenu=atoi(getenv("BT4_TEST_STORY_GUIDE"));
    }
    #endif
    HistSel_Init(section);
#ifdef PORT
    if (getenv("BT4_TEST_STORY_GUIDE"))gHistSel->flags=HISTSEL_GREETED;
#endif
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            HistSel_Update();
#ifdef PORT
            if (getenv("BT4_TEST_STORY_GUIDE") && ++previewFrames==120) {
                s32 guide=atoi(getenv("BT4_TEST_STORY_GUIDE"));
                if(guide>=0 && guide<8){
                    gHistSel->guest=PORT_V(guide,6);gHistSel->guide=guide;
                    Flash_GotoLabel(&gHistSel->flash[0],"fl_scenario_ok",1);
                    HistGuide_ClipGoto(gHistSel,gHistSel->guest,"fl_in");
                    gHistSel->script=getenv("BT4_TEST_STORY_GUIDE_STEP")?atoi(getenv("BT4_TEST_STORY_GUIDE_STEP")):251+50*guide;
                    HistGuide_Update(gHistSel,&result);
                }
            }
            if (!getenv("BT4_TEST_STORY_GUIDE"))
#endif
            HistGuide_Update(gHistSel, &result);
        }
        HistSel_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gHistSel->flags & HISTSEL_GREETED) && (gHistSel->flash[0].flags & MFLASH_PAD)) {
                gHistSel->flags |= HISTSEL_GREETED;
                if (gHistSel->flags & HISTSEL_COMPLETE) {
                    gHistSel->script = 1151;
                } else if (gHistSel->flags & HISTSEL_FIRST_VISIT) {
                    gHistSel->script = 1;
                } else if (gHistSel->flags & HISTSEL_EVENT) {
                    gHistSel->script = 651;
                } else {
                    Flash_GotoLabel(&gHistSel->flash[0], "fl_menu_in", 1);
                    gHistSel->guide = 8;
                    gHistSel->talker = 0;
                    gHistSel->voiceLine = Rand_Range(5) + 14;
                    HistSel_PlayVoice();
                    Snd_PlaySe(2, 0xE);
                }
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            StreamSe_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gHistSel->flags & HISTSEL_LEAVING) {
            if (--gHistSel->timer == -1) {
                if (result != 1) {
                    ColorFade_StartOut(0, 0, 0, 0x14);
                } else {
                    ColorFade_StartOut(0xFF, 0xFF, 0xFF, 0x14);
                }
                gProgress->subMenu = gHistSel->items[gHistSel->cursor[1]];
                HPROG->level = MSAVE->level;
            }
        } else if (gHistSel->script == 0) {
            HistSel_Input(&result);
        }
    }
    HistSel_Term();
    Dma_ResetBuffers();
    return result;
}
