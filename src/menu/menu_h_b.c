#include "common.h"
#include "menu/menu_h.h"
#include "sys/pad.h"
#include "sys/save.h"

CharRef *gCharRef = NULL; /* 0x3B4854 */

/*
 * CharRef: the character reference (progress mode 60), 0x3562B8..0x3590A8. Chi-Chi guides: a scrolling list of
 * the unlocked characters; for each one a large picture (loaded in the background), a profile text, a voice
 * sample, the guide's spoken comment and the model viewer (ChrView) with a costume choice.
 */

void CharRef_StateIntro(void);
void CharRef_StateList(void);
void CharRef_StateDetails(void);
void CharRef_StateProfile(void);
void CharRef_StateVoice(void);
void CharRef_StateComment(void);
void CharRef_StateCostume(void);
void CharRef_StateView(void);
void CharRef_StateViewWait(void);
void CharRef_ClipGoto(s32 unused, s32 kind, char *label);
void CharRef_BuildList(void);
void CharRef_MeasureText(void);
void CharRef_UpdateImage(void);
void CharRef_ChangeImage(void);
void CharRef_ScissorOn(void);
void CharRef_ScissorOff(void);
void CharRef_Term(void);
void CharRef_Draw(void);

#define CHARREF_IMAGE_FILE 0x2F9     /* file id of character 0's large picture */
#define CHARREF_IMAGE_SIZE 0x16800

#define CR_RES(n) \
    res = (MTexRes *)MPACK_AT(gCharRef->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 9) and builds the character list. */
void CharRef_Init(s32 section) {
    MTexRes *res = NULL;
    s32 *table;
    s32 i;

    gCharRef = Heap_Alloc(0x1638, 0x20, 0, 2);
    memset(gCharRef, 0, 0x1638);
    gCharRef->pack = (u32 *)MPACK_AT(gMenuArc9, section);
    gCharRef->res = Sprite_Unpack(gCharRef->pack, NULL, NULL);
    CR_RES(15);
    gCharRef->bg = res;
    CR_RES(13);
    gCharRef->tex[3] = MTEX(res, 0);
    gCharRef->tex[4] = MTEX(res, 1);
    gCharRef->tex[5] = MTEX(res, 2);
    gCharRef->tex[6] = MTEX(res, 3);
    CR_RES(16);
    gCharRef->tex[0] = MTEX(res, 0);
    gCharRef->tex[14] = MTEX(res, 3);
    gCharRef->tex[15] = MTEX(res, 2);
    gCharRef->tex[26] = MTEX(res, 1);
    gCharRef->tex[39] = MTEX(res, 4);
    gCharRef->tex[1] = MTEX(res, 5);
    CR_RES(12);
    gCharRef->tex[32] = MTEX(res, 0);
    gCharRef->tex[42] = MTEX(res, 0);
    gCharRef->tex[43] = MTEX(res, 5);
    gCharRef->tex[33] = MTEX(res, 6);
    gCharRef->tex[34] = MTEX(res, 8);
    gCharRef->tex[35] = MTEX(res, 1);
    gCharRef->tex[36] = MTEX(res, 3);
    CR_RES(9);
    gCharRef->tex[7] = MTEX(res, 0);
    gCharRef->tex[10] = MTEX(res, 1);
    gCharRef->tex[16] = MTEX(res, 2);
    gCharRef->tex[20] = MTEX(res, 3);
    gCharRef->tex[9] = MTEX(res, 5);
    gCharRef->tex[19] = MTEX(res, 4);
    CR_RES(11);
    gCharRef->tex[28] = MTEX(res, 0);
    gCharRef->tex[29] = MTEX(res, 1);
    gCharRef->tex[30] = MTEX(res, 2);
    CR_RES(5);
    gCharRef->tex[22] = MTEX(res, 0);
    gCharRef->tex[23] = MTEX(res, 1);
    gCharRef->tex[21] = MTEX(res, 2);
    gCharRef->tex[24] = MTEX(res, 3);
    CR_RES(10);
    gCharRef->tex[40] = MTEX(res, 0);
    CR_RES(8);
    gCharRef->tex[8] = MTEX(res, 0);
    gCharRef->tex[12] = MTEX(res, 1);
    CR_RES(3);
    gCharRef->tex[37] = MTEX(res, 0);
    gCharRef->tex[38] = MTEX(res, 1);
    CR_RES(14);
    gCharRef->tex[11] = MTEX(res, 0);
    CR_RES(2);
    gCharRef->tex[41] = MTEX(res, 0);
    CR_RES(24);
    gCharRef->tex[25] = MTEX(res, 0);
    CR_RES(7);
    gCharRef->tex[13] = MTEX(res, 0);
    /* One variable carries every table pointer: with the pointers stored directly the four additions come out
       as `addu v1,v1,v0` instead of `addu v0,v1,v0`, and the store to `total` has to sit between the last
       computation and its store. */
    table = (s32 *)MPACK_AT(gCharRef->res, 26);
    gCharRef->commentTbl = table;
    table = (s32 *)MPACK_AT(gCharRef->res, 28);
    gCharRef->commentNumTbl = table;
    table = (s32 *)MPACK_AT(gCharRef->res, 27);
    gCharRef->poseTbl = table;
    table = (s32 *)MPACK_AT(gCharRef->res, 17);
    gCharRef->total = CHARREF_CHARA_MAX;
    gCharRef->charaTbl = table;
    gCharRef->imageFile = Heap_Alloc(CHARREF_IMAGE_SIZE, 0x40, 0, 2);
    gCharRef->imageRes = Heap_Alloc(0x20800, 0x20, 0, 2);
    File_LoadSync(CHARREF_IMAGE_FILE, gCharRef->imageFile, CHARREF_IMAGE_SIZE);
    Sprite_Unpack(gCharRef->imageFile, gCharRef->imageRes, NULL);
    res = gCharRef->imageRes;
    Res_RelocateOffsets(&res, res, res);
    gCharRef->tex[2] = MTEX(res, 0);
    for (i = 0; i < 5; i++) {
        gCharRef->nameText[i] = MPACK_AT(gCharRef->res, 22);
        gCharRef->formText[i] = MPACK_AT(gCharRef->res, 23);
    }
    gCharRef->text[0] = MPACK_AT(gCharRef->res, 22);
    gCharRef->text[1] = MPACK_AT(gCharRef->res, 23);
    gCharRef->text[2] = MPACK_AT(gCharRef->res, 25);
    gCharRef->text[3] = MPACK_AT(gCharRef->res, 25);
    TextBox_Init(&gCharRef->box[CHARREF_BOX_NAME], gCharRef->text[0], 2);
    TextBox_Init(&gCharRef->box[CHARREF_BOX_FORM], gCharRef->text[1], 4);
    TextBox_Init(&gCharRef->box[CHARREF_BOX_VOICE], gCharRef->text[2], 2);
    TextBox_Init(&gCharRef->box[CHARREF_BOX_PROFILE], gCharRef->text[3], 0);
    TextBox_SetRect(&gCharRef->box[CHARREF_BOX_PROFILE], 0, 0x200, 0x128, 0x198);
    for (i = 0; i < 5; i++) {
        TextBox_Init(&gCharRef->box[CHARREF_BOX_LIST_NAME + i], gCharRef->nameText[i], 2);
        TextBox_SetRect(&gCharRef->box[CHARREF_BOX_LIST_NAME + i], 0, 0x200, 0x49, 0x135);
        TextBox_Init(&gCharRef->box[CHARREF_BOX_LIST_FORM + i], gCharRef->formText[i], 4);
        TextBox_SetRect(&gCharRef->box[CHARREF_BOX_LIST_FORM + i], 0, 0x200, 0x49, 0x135);
    }
    for (i = 0; i < CHARREF_BOX_NUM; i++) {
        TextBox_SetUnk80(&gCharRef->box[i], 1);
    }
    gCharRef->subtitles = MPACK_AT(gCharRef->res, 20);
    Flash_Create(&gCharRef->flash[0], MPACK_AT(gCharRef->res, 1), gCharRef->tex);
    Flash_Play(&gCharRef->flash[0], 1);
    Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_list_in", 1);
    CR_RES(4);
    IconWin_Init(MPACK_AT(gCharRef->res, 19), res);
    IconWin_Open();
    gCharRef->msgText = MPACK_AT(gCharRef->res, 21);
    MsgWin_Init(MPACK_AT(gCharRef->res, 18), gCharRef->msgText, 0, (s32)gCharRef->unk150);
    MsgWin_Open();
    ChrView_Init();
    gCharRef->state = CHARREF_ST_INTRO;
    gCharRef->loadState = CHARREF_LOAD_SHOWN;
    gCharRef->imageAlpha = 1.0f;
    gCharRef->commentBase = 9;
    gCharRef->voiceLine = -1;
    gCharRefState[CHARREF_ST_INTRO] = CharRef_StateIntro;
    gCharRefState[CHARREF_ST_LIST] = CharRef_StateList;
    gCharRefState[CHARREF_ST_DETAILS] = CharRef_StateDetails;
    gCharRefState[CHARREF_ST_PROFILE] = CharRef_StateProfile;
    gCharRefState[CHARREF_ST_VOICE] = CharRef_StateVoice;
    gCharRefState[CHARREF_ST_COMMENT] = CharRef_StateComment;
    gCharRefState[CHARREF_ST_COSTUME] = CharRef_StateCostume;
    gCharRefState[CHARREF_ST_VIEW] = CharRef_StateView;
    gCharRefState[CHARREF_ST_VIEW_WAIT] = CharRef_StateViewWait;
    CharRef_BuildList();
}

#ifdef PORT
extern void Port_MenuScene3D(int on); /* port/src/headless.c */
#endif

/* Runs the screen until its fade out is over and the picture loader is idle; returns 0. */
s32 CharRef_Run(s32 section) {
    CharRef_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        CharRef_UpdateImage();
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
#ifdef PORT
        /* The model viewer is a 3D scene, not a page composed for 4:3: in a picture wider than 4:3 it is shown as a
           fight is, with the wider view, and not stretched like the menus around it. */
        Port_MenuScene3D(gCharRef->state == CHARREF_ST_VIEW);
#endif
        if (gCharRef->state == CHARREF_ST_VIEW) {
            ChrView_Update();
        } else {
            CharRef_Draw();
        }
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        switch (gCharRef->state) {
        case CHARREF_ST_LEAVE:
            if (ColorFade_IsFadingOut()) {
                Voice_FadeOutStep(0);
                Bgm_FadeOutStep();
            } else if (ColorFade_IsOutDone()) {
                if (gCharRef->loadState == CHARREF_LOAD_SHOWN) {
                    goto done;
                }
                CharRef_UpdateImage();
            }
            break;
        default:
            if (!(gProgress->flags & MPROG_FREEZE)) {
                gCharRefState[gCharRef->state]();
            }
            break;
        }
    }
done:
#ifdef PORT
    Port_MenuScene3D(0);
#endif
    Dma_ResetBuffers();
    CharRef_Term();
    return 0;
}

/* The clip of list row / details plate `n` + 1. */
#define CR_PLATE2(n) sprintf(name, "mc_menu_plate2_%d", n)

/* Draws the background and the movie with everything that depends on the state, the windows and the fade. */
void CharRef_Draw(void) {
    MFlashRef ref;
    MFlashRef refEye;
    MFlashRef refMouth;
    char name[64];
    MFlashUv uv;
    MFlash *flash = &gCharRef->flash[0];
    s32 state = gCharRef->state;
    s32 pose = gCharRef->pose;
    s32 chara = gCharRef->entry[gCharRef->cursor].chara;
    s32 line;
    s32 next;
    s32 c;
    s32 i;
    s32 n;
    s32 y;
    s16 tex;   /* s16: as s32 the `& 1` is followed by a 16-bit zero extension */
    f32 scale;
    MChrTblEntry *tbl;

    Job_Run();
    Sprite_DrawPicture(gCharRef->bg, 0, 0, 0x80);
    Flash_Advance(&gCharRef->flash[0]);
    if (pose) {
        gCharRef->tex[32] = gCharRef->tex[43];
    } else {
        gCharRef->tex[32] = gCharRef->tex[42];
    }
    Flash_FindLabel(flash, NULL, pose ? "mc_guide_b_chichi_eye" : "mc_guide_a_chichi_eye", &refEye);
    FlashAnim_Blink(flash, &refEye, &gCharRef->blink, 0);
    Flash_FindLabel(flash, NULL, pose ? "mc_guide_b_chichi_mouth" : "mc_guide_a_chichi_mouth", &refMouth);
    FlashAnim_Talk(flash, &refMouth, &gCharRef->talk, 0);
    Flash_FindLabel(flash, NULL, !pose ? "mc_guide_b_chichi_mouth" : "mc_guide_a_chichi_mouth", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);
    Flash_FindLabel(flash, NULL, !pose ? "mc_guide_b_chichi_eye" : "mc_guide_a_chichi_eye", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);
    Flash_FindLabel(flash, NULL, "mc_name_text_1", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &gCharRef->box[CHARREF_BOX_NAME]);
    Flash_FindLabel(flash, NULL, "mc_form_text_1", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, chara, &gCharRef->box[CHARREF_BOX_FORM]);
    Flash_FindLabel(flash, NULL, "mc_voice_text_1", &ref);
    line = chara * 2;
    TextBox_AttachLine(flash, &ref, 0, 0, line, &gCharRef->box[CHARREF_BOX_VOICE]);
    CR_PLATE2(1);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipSetCallbackA(flash, &ref, state == CHARREF_ST_LIST ? CharRef_ScissorOn : NULL, NULL);
    Flash_ClipSetCallbackB(flash, &ref, CharRef_ScissorOff, NULL);
    CR_PLATE2(4);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipSetCallbackA(flash, &ref, state == CHARREF_ST_LIST ? CharRef_ScissorOn : NULL, NULL);
    Flash_ClipSetCallbackB(flash, &ref, CharRef_ScissorOff, NULL);
    CR_PLATE2(5);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipSetCallbackA(flash, &ref, state == CHARREF_ST_LIST ? CharRef_ScissorOn : NULL, NULL);
    Flash_ClipSetCallbackB(flash, &ref, CharRef_ScissorOff, NULL);
    for (i = 0; i < 5; i++) {
        n = gCharRef->cursor - gCharRef->row + i;
        if (n > gCharRef->count - 1) {
            n -= gCharRef->count;
        } else {
            n += (n < 0) * gCharRef->count;
        }
        CR_PLATE2(i + 1);
        Flash_FindLabel(flash, name, "mc_name_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gCharRef->entry[n].chara, &gCharRef->box[CHARREF_BOX_LIST_NAME + i]);
        CR_PLATE2(i + 1);
        Flash_FindLabel(flash, name, "mc_form_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gCharRef->entry[n].chara, &gCharRef->box[CHARREF_BOX_LIST_FORM + i]);
    }
    if (gCharRef->scrollDir == 4) {
        n = gCharRef->cursor - gCharRef->row - 1;
        if (gCharRef->cursor < 3) {
            n = gCharRef->count + gCharRef->cursor - 4;
        }
        CR_PLATE2(i);
        Flash_FindLabel(flash, name, "mc_name_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gCharRef->entry[n].chara, &gCharRef->box[CHARREF_BOX_LIST_NAME + 4]);
        CR_PLATE2(i);
        Flash_FindLabel(flash, name, "mc_form_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, gCharRef->entry[n].chara, &gCharRef->box[CHARREF_BOX_LIST_FORM + 4]);
    }
    uv.x0 = 0;
    uv.x1 = 0x100;
    for (i = 0; i < 5; i++) {
        uv.y0 = i * 0x20;
        uv.y1 = i * 0x20 + 0x20;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    if (gCharRef->flags & CHARREF_IMAGE_READY) {
        gCharRef->imageAlpha += 0.075f;
        if (gCharRef->imageAlpha >= 1.0f) {
            gCharRef->imageAlpha = 1.0f;
        }
    } else {
        gCharRef->imageAlpha = 0.0f;
    }
    Flash_FindLabel(flash, NULL, "mc_single_chara_l", &ref);
    Flash_ClipSetAlpha(flash, &ref, gCharRef->imageAlpha);
    uv.y0 = 0;
    uv.x0 = 0x20;
    uv.y1 = 0x20;
    uv.x1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_right", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_down", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x0 = 0;
    uv.x1 = 0x20;
    Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_up", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    scale = 4.0f / (f32)gCharRef->count * 7.0f;
    y = (gCharRef->cursor - gCharRef->row) * 0xE0 / gCharRef->count;
    if (y < 0) {
        y = 0;
    } else if (y > 0xDA) {
        y = 0xDA;
    }
    Flash_FindLabel(flash, NULL, "mc_scroll_bar_point", &ref);
    Flash_ClipSetScale(flash, &ref, 1.0f, scale);
    Flash_ClipSetOffset(flash, &ref, 0, y);
    c = gCharRef->costume + 1;
    switch (c) {
    case 0:
        uv.y0 = 0;
        uv.y1 = c + 0x20;
        uv.x0 = 0;
        uv.x1 = c + 0x20;
        break;
    case 1:
        uv.y0 = 0;
        uv.y1 = 0x20;
        uv.x0 = 0x20;
        uv.x1 = 0x40;
        break;
    case 2:
        uv.y0 = 0;
        uv.y1 = 0x20;
        uv.x0 = 0x40;
        uv.x1 = 0x60;
        break;
    case 3:
        uv.y0 = 0;
        uv.y1 = 0x20;
        uv.x0 = 0x60;
        uv.x1 = 0x80;
        break;
    }
    sprintf(name, "mc_menu_plate_%d", 5);
    Flash_FindLabel(flash, name, "mc_num", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    tbl = (MChrTblEntry *)MPACK_AT(gCommonRes->data[2], 1);
    tex = (tbl[gCharRef->entry[gCharRef->cursor].chara].flags ^ 1) & 1;
    Flash_FindLabel(flash, NULL, "mc_ch_base_1", &ref);
    Flash_ClipSetTex(flash, &ref, tex);
    if (gCharRef->pageDir) {
        y = gCharRef->cursor - 1;
    } else {
        y = gCharRef->cursor + 1;
    }
    next = gCharRef->entry[y].chara;
    if (y >= gCharRef->count) {
        next = gCharRef->entry[0].chara;
    } else if (y < 0) {
        next = gCharRef->entry[gCharRef->count - 1].chara;
    }
    tex = (tbl[next].flags ^ 1) & 1;
    Flash_FindLabel(flash, NULL, "mc_ch_base_2", &ref);
    Flash_ClipSetTex(flash, &ref, tex);
    Flash_FindLabel(flash, NULL, "mc_ch_base_3", &ref);
    Flash_ClipSetTex(flash, &ref, tex);
    if (state == CHARREF_ST_PROFILE) {
        Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_up", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, gCharRef->scroll > 0);
        Flash_FindLabel(flash, NULL, "mc_yajirusi_icon_down", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, gCharRef->scroll < gCharRef->textHeight - 0x6E);
    }
    for (i = 0; i < CHARREF_FLASH_NUM; i++) {
        Flash_Draw(&gCharRef->flash[i]);
    }
    Flash_FindLabel(flash, NULL, "mc_dammy_text", &ref);
    TextBox_AttachLine(flash, &ref, 0, -gCharRef->scroll, line + 1, &gCharRef->box[CHARREF_BOX_PROFILE]);
    IconWin_Draw();
    if (state == CHARREF_ST_INTRO || state == CHARREF_ST_LIST || state == CHARREF_ST_LEAVE) {
        MsgWin_Draw(0, 0, gCharRef->voiceLine);
    } else if (state == CHARREF_ST_DETAILS) {
        if (gCharRef->prevState == CHARREF_ST_LIST) {
            MsgWin_Draw(0, 0, gCharRef->voiceLine);
        } else {
            MsgWin_Draw(0, 0, gCharRef->voiceLine + gCharRef->commentBase + gCharRef->comment);
        }
    } else {
        MsgWin_Draw(0, 0, gCharRef->voiceLine + gCharRef->commentBase + gCharRef->comment);
    }
    ColorFade_Draw();
}

/* State 0: waits for the movie's intro, then lights the cursor row and greets. */
void CharRef_StateIntro(void) {
    if (gCharRef->flash[0].flags & MFLASH_PAD) {
        CharRef_ClipGoto(0, 0, "fl_on_start");
        gCharRef->state = CHARREF_ST_LIST;
        gCharRef->voiceLine = 0;
        Voice_PlayWithSubtitle(gCharRef->subtitles, CHARREF_VOICE_BASE, gCharRef->voiceLine);
    }
}

/* State 1, the list: up / down one character, left / right four, confirm opens the details, cancel leaves. */
void CharRef_StateList(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gPad[0].gameRepeat & PADG_UP) {
        CharRef_ClipGoto(0, 0, "fl_off_start");
        if (--gCharRef->row < 0) {
            gCharRef->row = 0;
            gCharRef->scrollDir = 8;
            Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_list_down", 1);
        }
        if (--gCharRef->cursor < 0) {
            gCharRef->cursor = gCharRef->count - 1;
        }
        CharRef_ClipGoto(0, 0, "fl_on_start");
        Snd_PlaySe(1, 0);
        gCharRef->idle = 0;
    } else if (gPad[0].gameRepeat & PADG_DOWN) {
        CharRef_ClipGoto(0, 0, "fl_off_start");
        if (++gCharRef->row >= CHARREF_ROWS) {
            gCharRef->row = CHARREF_ROWS - 1;
            gCharRef->scrollDir = 4;
            Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_list_up", 1);
        }
        if (++gCharRef->cursor > gCharRef->count - 1) {
            gCharRef->cursor = 0;
        }
        CharRef_ClipGoto(0, 0, "fl_on_start");
        Snd_PlaySe(1, 0);
        gCharRef->idle = 0;
    } else if (gPad[0].gameRepeat & PADG_LEFT) {
        gCharRef->cursor -= 4;
        if (gCharRef->cursor < 0) {
            gCharRef->cursor = gCharRef->count - 1;
        }
        Snd_PlaySe(1, 0);
        gCharRef->idle = 0;
    } else if (gPad[0].gameRepeat & PADG_RIGHT) {
        gCharRef->cursor += 4;
        if (gCharRef->cursor > gCharRef->count - 1) {
            gCharRef->cursor = 0;
        }
        Snd_PlaySe(1, 0);
        gCharRef->idle = 0;
    } else if (gPad[0].gamePressed & PADG_CROSS) {
        CharRef_ClipGoto(0, 0, "fl_ok");
        gCharRef->prevState = gCharRef->state;
        gCharRef->state = CHARREF_ST_DETAILS;
        MsgWin_Close();
        IconWin_Close();
        CharRef_ClipGoto(0, 1, "fl_on_start");
        CharRef_ChangeImage();
        Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_details_in", 1);
        gCharRef->idle = 0;
        Snd_PlaySe(1, 1);
        if (Voice_GetStat(0) != MVOICE_IDLE) {
            Voice_StopWithLip();
        }
    } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
        ColorFade_StartOut(0, 0, 0, 0x14);
        gCharRef->state = CHARREF_ST_LEAVE;
        gCharRef->idle = 0;
        Snd_PlaySe(1, 2);
    } else if (Voice_GetStat(0) == MVOICE_IDLE) {
        gCharRef->idle++;
        if (gCharRef->idle > 300) {
            gCharRef->idle = 0;
            while (1) {
                gCharRef->voiceLine = Rand_Range(7) + 2;
                if (gCharRef->voiceLine != gCharRef->lastLine) {
                    gCharRef->lastLine = gCharRef->voiceLine;
                    break;
                }
            }
            Voice_PlayWithSubtitle(gCharRef->subtitles, CHARREF_VOICE_BASE, gCharRef->voiceLine);
        }
    }
}

/* Starts line gCharRef->comment of the current character's comment and shows the pose that goes with it. */
#define CR_SAY_COMMENT() \
    gCharRef->pose = gCharRef->poseTbl[gCharRef->commentBase + gCharRef->comment - 9]; \
    Voice_PlayWithSubtitle(gCharRef->subtitles, CHARREF_VOICE_BASE, \
                           gCharRef->voiceLine + gCharRef->commentBase + gCharRef->comment)

/* State 2, the details menu: up / down the four items, left / right the previous / next character. */
void CharRef_StateDetails(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gPad[0].gameRepeat & PADG_UP) {
        CharRef_ClipGoto(0, 1, "fl_off_start");
        if (--gCharRef->menuCursor < 0) {
            gCharRef->menuCursor = 3;
        }
        CharRef_ClipGoto(0, 1, "fl_on_start");
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & PADG_DOWN) {
        CharRef_ClipGoto(0, 1, "fl_off_start");
        if (++gCharRef->menuCursor >= 4) {
            gCharRef->menuCursor = 0;
        }
        CharRef_ClipGoto(0, 1, "fl_on_start");
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gameRepeat & PADG_LEFT) {
        if (--gCharRef->cursor < 0) {
            gCharRef->cursor = gCharRef->count - 1;
        }
        gCharRef->pageDir = 0;
        Snd_PlaySe(1, 0);
        Flash_GotoLabel(&gCharRef->flash[0], "fl_page_down", 1);
        CharRef_ChangeImage();
    } else if (gPad[0].gameRepeat & PADG_RIGHT) {
        if (++gCharRef->cursor > gCharRef->count - 1) {
            gCharRef->cursor = 0;
        }
        gCharRef->pageDir = 1;
        Snd_PlaySe(1, 0);
        Flash_GotoLabel(&gCharRef->flash[0], "fl_page_up", 1);
        CharRef_ChangeImage();
    } else if (gPad[0].gamePressed & PADG_CROSS) {
        CharRef_ClipGoto(0, 1, "fl_ok");
        if (gCharRef->menuCursor == 0) {
            gCharRef->scroll = 0;
            CharRef_MeasureText();
            gCharRef->state = CHARREF_ST_PROFILE;
            Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_1_in", 1);
        } else if (gCharRef->menuCursor == 1) {
            Voice_PlayChara(0, gCharRef->entry[gCharRef->cursor].chara, rand() % 2);
            gCharRef->state = CHARREF_ST_VOICE;
            Flash_GotoLabel(&gCharRef->flash[0], "fl_voise_in", 1);
        } else if (gCharRef->menuCursor == 2) {
            gCharRef->voiceLine = 0;
            gCharRef->commentBase = 9;
            gCharRef->commentBase += gCharRef->entry[gCharRef->cursor].commentLine;
            gCharRef->comment = 0;
            gCharRef->state = CHARREF_ST_COMMENT;
            MsgWin_SetSide(1);
            MsgWin_Open();
            CR_SAY_COMMENT();
            Flash_GotoLabel(&gCharRef->flash[0], "fl_chichi_comment_in", 1);
        } else {
            gCharRef->costume = 0;
            gCharRef->menuCursor = 5;
            CharRef_ClipGoto(0, 1, "fl_off_start");
            CharRef_ClipGoto(0, 2, "fl_on_start");
            gCharRef->state = CHARREF_ST_COSTUME;
            Flash_GotoLabel(&gCharRef->flash[0], "fl_model_view_in", 1);
        }
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
        MsgWin_SetSide(0);
        Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_details_cancel", 1);
        CharRef_ClipGoto(0, 1, "fl_off_start");
        gCharRef->menuCursor = 0;
        MsgWin_Open();
        IconWin_Open();
        gCharRef->state = CHARREF_ST_LIST;
        gCharRef->pose = 0;
        Voice_PlayWithSubtitle(gCharRef->subtitles, CHARREF_VOICE_BASE, 1);
        Snd_PlaySe(1, 2);
        gCharRef->voiceLine = 1;
    }
}

/* State 3, the profile: up / down scroll the text while held, cancel goes back. */
void CharRef_StateProfile(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gPad[0].gameHeld & PADG_UP) {
        if (gCharRef->scroll > 0) {
            gCharRef->scroll -= 2;
        }
    } else if (gPad[0].gameHeld & PADG_DOWN) {
        if (!(gCharRef->textHeight - 0x6E < gCharRef->scroll)) {
            gCharRef->scroll += 2;
        }
    } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
        Flash_GotoLabel(&gCharRef->flash[0], "fl_chara_1_out", 1);
        CharRef_ClipGoto(0, 1, "fl_on_start");
        gCharRef->state = CHARREF_ST_DETAILS;
        Snd_PlaySe(1, 2);
    }
}

/* State 4, the voice sample: confirm plays one of the character's two lines again, cancel goes back. */
void CharRef_StateVoice(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gPad[0].gamePressed & PADG_CROSS) {
        Voice_Stop(0);
        Voice_PlayChara(0, gCharRef->entry[gCharRef->cursor].chara, rand() % 2);
    }
    if (gPad[0].gamePressed & PADG_TRIANGLE) {
        Flash_GotoLabel(&gCharRef->flash[0], "fl_voise_out", 1);
        CharRef_ClipGoto(0, 1, "fl_on_start");
        gCharRef->state = CHARREF_ST_DETAILS;
        Snd_PlaySe(1, 2);
        Voice_Stop(0);
    }
}

/* State 5, the guide's comment: each line follows the last by itself or on confirm; cancel goes back. */
void CharRef_StateComment(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (Voice_GetStat(0) == MVOICE_IDLE) {
        if (gCharRef->comment < gCharRef->entry[gCharRef->cursor].commentNum - 1) {
            gCharRef->comment++;
            CR_SAY_COMMENT();
        } else {
            MsgWin_Close();
            Flash_GotoLabel(&gCharRef->flash[0], "fl_chichi_comment_out", 1);
            CharRef_ClipGoto(0, 1, "fl_on_start");
            gCharRef->prevState = gCharRef->state;
            gCharRef->state = CHARREF_ST_DETAILS;
            Snd_PlaySe(1, 2);
        }
    } else if (gPad[0].gamePressed & PADG_CROSS) {
        if (gCharRef->comment < gCharRef->entry[gCharRef->cursor].commentNum - 1) {
            gCharRef->comment++;
            gCharRef->pose = gCharRef->poseTbl[gCharRef->commentBase + gCharRef->comment - 9];
            Snd_PlaySe(1, 1);
            Voice_PlayWithSubtitle(gCharRef->subtitles, CHARREF_VOICE_BASE,
                                   gCharRef->voiceLine + gCharRef->commentBase + gCharRef->comment);
        } else {
            MsgWin_Close();
            Flash_GotoLabel(&gCharRef->flash[0], "fl_chichi_comment_out", 1);
            CharRef_ClipGoto(0, 1, "fl_on_start");
            gCharRef->prevState = gCharRef->state;
            gCharRef->state = CHARREF_ST_DETAILS;
            Snd_PlaySe(1, 1);
            Voice_StopWithLip();
        }
    } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
        MsgWin_Close();
        Flash_GotoLabel(&gCharRef->flash[0], "fl_chichi_comment_out", 1);
        CharRef_ClipGoto(0, 1, "fl_on_start");
        gCharRef->prevState = gCharRef->state;
        gCharRef->state = CHARREF_ST_DETAILS;
        Voice_StopWithLip();
        Snd_PlaySe(1, 2);
    }
}

/* State 6, the costume choice before the model viewer: up / down, confirm shows the model, cancel goes back. */
void CharRef_StateCostume(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gPad[0].gamePressed & PADG_UP) {
        if (--gCharRef->costume < 0) {
            gCharRef->costume = ChrTbl_WrapCostume(gCharRef->entry[gCharRef->cursor].chara, NULL) - 1;
        }
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gamePressed & PADG_DOWN) {
        if (++gCharRef->costume > ChrTbl_WrapCostume(gCharRef->entry[gCharRef->cursor].chara, NULL) - 1) {
            gCharRef->costume = 0;
        }
        Snd_PlaySe(1, 0);
    } else if (gPad[0].gamePressed & PADG_CROSS) {
        CharRef_ClipGoto(0, 2, "fl_ok");
        gCharRef->state = CHARREF_ST_VIEW_WAIT;
        ChrView_Show(gCharRef->entry[gCharRef->cursor].chara, gCharRef->costume, 0);
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & PADG_TRIANGLE) {
        CharRef_ClipGoto(0, 2, "fl_off_start");
        gCharRef->menuCursor = 3;
        Flash_GotoLabel(&gCharRef->flash[0], "fl_model_view_out", 1);
        CharRef_ClipGoto(0, 1, "fl_on_start");
        gCharRef->state = CHARREF_ST_DETAILS;
        Snd_PlaySe(1, 2);
    }
}

/* State 7, the model viewer (CharRef_Run runs ChrView_Update instead of the screen): cancel closes it. */
void CharRef_StateView(void) {
    if (!(gCharRef->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (gPad[0].gamePressed & PADG_TRIANGLE) {
        ChrView_Hide();
        CharRef_ClipGoto(0, 2, "fl_on_start");
        gCharRef->state = CHARREF_ST_COSTUME;
        Snd_PlaySe(1, 2);
    }
}

/* State 8: waits until the model is loaded and shown. */
void CharRef_StateViewWait(void) {
    if (gCharRef->flash[0].flags & MFLASH_PAD) {
        if (ChrView_IsVisible()) {
            gCharRef->state = CHARREF_ST_VIEW;
        }
    }
}

/* Sends a plate to a label: kind 0 the list row under the cursor, 1 the details item, 2 the fifth plate. */
void CharRef_ClipGoto(s32 unused, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gCharRef->flash[0];

    switch (kind) {
    case 1:
        sprintf(name, "mc_menu_plate_%d", gCharRef->menuCursor + 1);
        break;
    case 0:
        sprintf(name, "mc_menu_plate2_%d", gCharRef->row + 1);
        break;
    case 2:
        sprintf(name, "mc_menu_plate_%d", 5);
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Lists the unlocked characters in the order of the pack's table. */
void CharRef_BuildList(void) {
    CharRefEntry *prev = NULL;
    s32 i = 0;
    s32 n;
    s32 chara;
    s32 line;
    s32 num;
    s32 page;

    gCharRef->count = 0;
    gCharRef->head = NULL;
    memset(gCharRef->entry, 0, sizeof(gCharRef->entry));
    n = 0;
    for (; i < gCharRef->total; i++) {
        chara = gCharRef->charaTbl[i];
        line = gCharRef->commentTbl[i];
        num = gCharRef->commentNumTbl[i];
        if (chara < 0) {
            continue;
        }
        /* The quotient is never used: only the divide-by-zero check of the division is left in the code, once
           in each arm (dividend and operator are a guess; the divisor is a variable holding 3). */
        if (!(s32)((gSaveData->charaBits[chara >> 6] >> (chara - ((chara >> 6) << 6))) & 1)) {
            s32 perPage = 3;

            page = i / perPage;
            continue;
        }
        {
            s32 perPage = 3;

            page = i / perPage;
        }
        if (gCharRef->head == NULL) {
            gCharRef->head = &gCharRef->entry[n];
        }
        gCharRef->entry[n].prev = prev;
        gCharRef->entry[n].chara = chara;
        gCharRef->entry[n].commentLine = line;
        gCharRef->entry[n].commentNum = num;
        gCharRef->count++;
        if (prev != NULL) {
            prev->next = &gCharRef->entry[n];
        }
        prev = &gCharRef->entry[n];
        n++;
    }
}

/* Measures the current character's profile text for the scroll limit. */
void CharRef_MeasureText(void) {
    CharRefEntry *e = gCharRef->head;
    s32 i;

    for (i = 0; i < gCharRef->cursor; i++) {
        e = e->next;
    }
    gCharRef->textHeight = Font_GetHeight((u16 *)MPACK_AT(gCharRef->text[3], e->chara * 2 + 2)) + 4;
}

/* One step of the large picture's background loader (the same machine as ModeMenu_UpdateImage). */
void CharRef_UpdateImage(void) {
    MTexRes *res;
    CharRefEntry *e;
    s32 i;

    switch (gCharRef->loadState) {
    case CHARREF_LOAD_SHOWN:
        if (gCharRef->flags & CHARREF_IMAGE_CHANGE) {
            gCharRef->loadState = CHARREF_LOAD_RESTART;
        }
        break;
    case CHARREF_LOAD_RESTART:
        if (gCharRef->flags & CHARREF_IMAGE_CHANGE) {
            gCharRef->flags ^= CHARREF_IMAGE_CHANGE;
        }
        gCharRef->loadState = CHARREF_LOAD_REQUEST;
        break;
    case CHARREF_LOAD_REQUEST:
        e = gCharRef->head;
        for (i = 0; i < gCharRef->cursor; i++) {
            e = e->next;
        }
        File_CancelRequests();
        File_Request(e->chara + CHARREF_IMAGE_FILE, gCharRef->imageFile, CHARREF_IMAGE_SIZE);
        gCharRef->loadState = CHARREF_LOAD_READ;
        break;
    case CHARREF_LOAD_READ:
        if (File_UpdateRequests()) {
            gCharRef->loadState = CHARREF_LOAD_UNPACK;
        }
        break;
    case CHARREF_LOAD_UNPACK:
        res = NULL;
        Sprite_Unpack(gCharRef->imageFile, gCharRef->imageRes, NULL);
        res = gCharRef->imageRes;
        Res_RelocateOffsets(&res, res, res);
        gCharRef->tex[2] = MTEX(res, 0);
        gCharRef->flags |= CHARREF_IMAGE_READY;
        gCharRef->loadState = CHARREF_LOAD_SHOWN;
        break;
    case CHARREF_LOAD_ABORT:
        gCharRef->loadState = CHARREF_LOAD_RESTART;
        break;
    }
}

/* The current character changed: hide the picture, or abort a load in progress, and ask for the new one. */
void CharRef_ChangeImage(void) {
    if (gCharRef->flags & CHARREF_IMAGE_READY) {
        gCharRef->flags ^= CHARREF_IMAGE_READY;
    } else {
        gCharRef->loadState = CHARREF_LOAD_ABORT;
    }
    gCharRef->flags |= CHARREF_IMAGE_CHANGE;
}

/* Clip callback: scissor to the list. */
void CharRef_ScissorOn(void) {
    Sprite_SetScissor(0, 0x200, 0x49, 0x135);
}

/* Clip callback: scissor back to the full screen. */
void CharRef_ScissorOff(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Frees the screen. */
void CharRef_Term(void) {
    s32 i;

    ChrView_Term();
    MsgWin_Term();
    IconWin_Term();
    for (i = 0; i < CHARREF_FLASH_NUM; i++) {
        Flash_Destroy(&gCharRef->flash[i]);
    }
    if (gCharRef->imageFile != NULL) {
        Heap_Free(gCharRef->imageFile);
        gCharRef->imageFile = NULL;
    }
    if (gCharRef->imageRes != NULL) {
        Heap_Free(gCharRef->imageRes);
        gCharRef->imageRes = NULL;
    }
    if (gCharRef->res != NULL) {
        Heap_Free(gCharRef->res);
        gCharRef->res = NULL;
    }
    if (gCharRef != NULL) {
        Heap_Free(gCharRef);
        gCharRef = NULL;
    }
}
