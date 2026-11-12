#include "common.h"
#include "menu/menu_k.h"

/*
 * Bracket, 0x3673F8..0x368068: the per-frame clip set-up of the bracket screen. Its read-only data (0x3B6BC0..
 * 0x3B6FC4) ends before a 16-byte alignment gap: Bracket_Load, the next function, starts another object.
 */

#define BK_ENT(n) (b->ents.e[n])
#define BK_MATCH (b->matches.m[b->match])

/* Sets the texture rectangles, positions and visibility of every clip that depends on the bracket's state. */
void Bracket_SetupDraw(Bracket *b) {
    MFlashRef ref;
    MFlashUv uv;
    char name[0x40];
    MFlash *flash;
    s32 i;

    /* title plate: the round's name; result banner */
    flash = &b->flash[BRK_FL_TITLE];
    uv.x0 = 0;
    uv.y0 = b->round * 0x20;
    uv.x1 = 0x100;
    uv.y1 = uv.y0 + 0x20;
    Flash_FindLabel(flash, "mc_menu_title_plate", "mc_menu_title_text02", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = 0x80;
    uv.x1 = 0x200;
    uv.x0 = 0;
    uv.y1 = 0x100;
    Flash_FindLabel(flash, NULL, "mc_victory_lose_result_text01", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.y0 = (b->flags & BRK_RUNNER_UP) ? 0x40 : 0;
    uv.y1 = uv.y0 + 0x40;
    uv.x1 = 0x100;
    uv.x0 = 0;
    Flash_FindLabel(flash, "mc_victory_plate", "mc_menu_victory_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);

    /* the guide: blink and mouth */
    flash = &b->flash[BRK_FL_GUIDE];
    switch (TOUR_PROG2->tour) {
    case TOUR_WORLD:
        Flash_FindLabel(flash, "mc_guide_tenkaichi", "mc_guide_tenkaichi_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &b->talk[0], 0);
        break;
    case TOUR_BIG:
        for (i = 0; i < 2; i++) {
            if (b->pose == i) {
                char *clip = i ? "mc_guide_satan_s_02" : "mc_guide_satan_s_01";

                Flash_FindLabel(flash, clip, i ? "mc_guide_satan_s_02_eye" : "mc_guide_satan_s_01_eye", &ref);
                FlashAnim_Blink(flash, &ref, &b->blink[0], 0);
                Flash_FindLabel(flash, clip, i ? "mc_guide_satan_s_02_mouth" : "mc_guide_satan_s_01_mouth", &ref);
                FlashAnim_Talk(flash, &ref, &b->talk[0], 0);
            }
        }
        break;
    case TOUR_CELL:
        for (i = 0; i < 2; i++) {
            if (b->pose == i) {
                char *clip = i ? "mc_guide_seru_02" : "mc_guide_seru_01";

                Flash_FindLabel(flash, clip, i ? "mc_guide_ceru_02_eye" : "mc_guide_ceru_01_eye", &ref);
                FlashAnim_Blink(flash, &ref, &b->blink[0], 0);
                Flash_FindLabel(flash, clip, i ? "mc_guide_ceru_02_mouth" : "mc_guide_ceru_01_mouth", &ref);
                FlashAnim_Talk(flash, &ref, &b->talk[0], 0);
            }
        }
        break;
    case TOUR_OTHERWORLD:
        Flash_FindLabel(flash, "mc_guide_anoyo", "mc_guide_anoyo_mouth", &ref);
        FlashAnim_Talk(flash, &ref, &b->talk[0], 0);
        break;
    case TOUR_YAMCHA:
        for (i = 0; i < 2; i++) {
            Flash_FindLabel(flash, i ? "mc_guide_puaru" : "mc_guide_yamucha",
                            i ? (b->pose ? "guide_puaru_eye02" : "mc_guide_puaru_eye01") : "mc_guide_yamucha_eye", &ref);
            FlashAnim_Blink(flash, &ref, &b->blink[i], 0);
            Flash_FindLabel(flash, i ? "mc_guide_puaru" : "mc_guide_yamucha",
                            i ? (b->pose ? "mc_guide_puaru_mouth02" : "mc_guide_puaru_mouth01") : "mc_guide_yamucha_mouth",
                            &ref);
            if (b->talker == i) {
                FlashAnim_Talk(flash, &ref, &b->talk[i], 0);
            } else {
                FlashAnim_ShowNext2(flash, &ref, 0);
            }
        }
        break;
    }

    /* the tree: scroll, arrows, the 17 chips */
    flash = &b->flash[BRK_FL_TREE];
    Flash_SetOffset(flash, (s32)b->scroll, 0);
    Flash_FindLabel(flash, NULL, "mc_scroll_yazirusi", &ref);
    Flash_ClipSetOffset(flash, &ref, -(s32)b->scroll, 0);
    for (i = 0; i < 2; i++) {
        uv.x0 = i * 0x40;
        uv.y0 = 0;
        uv.x1 = uv.x0 + 0x40;
        uv.y1 = 0x40;
        Flash_FindLabel(flash, "mc_scroll_yazirusi", i ? "mc_yajirusi_l" : "mc_yajirusi_r", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (b->flags & BRK_INPUT) {
            if (((s32)b->scroll <= -0x200 && i == 0) || ((s32)b->scroll >= 0 && i != 0)) {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            }
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    }
    for (i = 0; i < TOUR_ENTRANT_MAX; i++) {
        s32 x;
        s32 y;

        sprintf(name, "mc_tournament_frame_%d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        if (BK_ENT(i).flags & TOUR_ENT_PLAYER) {
            uv.x0 = (BK_ENT(i).player % 4) * 0x40;
            uv.y0 = (BK_ENT(i).player / 4) * 0x40;
            uv.y1 = uv.y0 + 0x40;
            uv.x1 = uv.x0 + 0x40;
            Flash_ClipSetTex(flash, &ref, 1);
            Flash_ClipSetUv(flash, &ref, &uv);
        }
        sprintf(name, "mc_tournament_chara%d", i);
        Flash_FindLabel(flash, NULL, name, &ref);
        x = Bracket_GetPosX(BK_ENT(i).pos);
        y = Bracket_GetPosY(BK_ENT(i).pos);
        Flash_ClipSetOffset(flash, &ref, x, y);
        if (BK_ENT(i).flags & TOUR_ENT_HIDDEN) {
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(flash, &ref, 2, 1);
            if (BK_ENT(i).flags & TOUR_ENT_LOST) {
                Flash_ClipSetColor(flash, &ref, 0.59999997f);
            }
        }
    }

    /* the two chips of the current match, moving */
    flash = &b->flash[BRK_FL_MOVE_A];
    if (b->flash[BRK_FL_MOVE_A].flags & 1) {
        s32 x;
        s32 y;

        for (i = 0; i < 2; i++) {
            if (BK_ENT(BK_MATCH.ent[i]).flags & TOUR_ENT_PLAYER) {
                uv.x0 = (BK_ENT(BK_MATCH.ent[i]).player % 4) * 0x40;
                uv.y0 = (BK_ENT(BK_MATCH.ent[i]).player / 4) * 0x40;
                uv.y1 = uv.y0 + 0x40;
                uv.x1 = uv.x0 + 0x40;
                Flash_FindLabel(flash, NULL, i ? "mc_tournament_frame_1" : "mc_tournament_frame_0", &ref);
                Flash_ClipSetTex(flash, &ref, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            }
        }
        x = Bracket_GetPosX(BK_ENT(BK_MATCH.ent[0]).pos) + (s32)b->scroll;
        y = Bracket_GetPosY(BK_ENT(BK_MATCH.ent[0]).pos);
        Flash_SetOffset(flash, x, y);
    }
    flash = &b->flash[BRK_FL_MOVE_B];
    if (b->flash[BRK_FL_MOVE_B].flags & 1) {
        s32 x;
        s32 y;

        for (i = 0; i < 2; i++) {
            if (BK_ENT(BK_MATCH.ent[i]).flags & TOUR_ENT_PLAYER) {
                uv.x0 = (BK_ENT(BK_MATCH.ent[i]).player % 4) * 0x40;
                uv.y0 = (BK_ENT(BK_MATCH.ent[i]).player / 4) * 0x40;
                uv.y1 = uv.y0 + 0x40;
                uv.x1 = uv.x0 + 0x40;
                Flash_FindLabel(flash, NULL, i ? "mc_tournament_frame_1" : "mc_tournament_frame_0", &ref);
                Flash_ClipSetTex(flash, &ref, 1);
                Flash_ClipSetUv(flash, &ref, &uv);
            }
        }
        x = Bracket_GetPosX(BK_ENT(BK_MATCH.ent[0]).pos) + (s32)b->scroll;
        y = Bracket_GetPosY(BK_ENT(BK_MATCH.ent[0]).pos);
        Flash_SetOffset(flash, x, y);
    }

    /* the versus panel: names, forms, player numbers, win / lose plates */
    flash = &b->flash[BRK_FL_VS];
    if (b->flash[BRK_FL_VS].flags & 1) {
        for (i = 0; i < 2; i++) {
            s32 chara = BK_ENT(BK_MATCH.ent[i]).chara;
            s32 cols = 5;
            s32 plate;

            if (BK_ENT(BK_MATCH.ent[i]).flags & TOUR_ENT_REC) {
                chara = 0xA3;
            } else {
                /* two pairs of characters share a name line */
                switch (chara) {
                case 0x16:
                case 0x17:
                    chara = 0xA7;
                    break;
                case 0x3A:
                case 0x3B:
                    chara = 0xA5;
                    break;
                }
            }
            Flash_FindLabel(flash, NULL, i ? "mc_name_text_r" : "mc_name_text_l", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &b->nameBox[i]);
            Flash_FindLabel(flash, NULL, i ? "mc_form_text_r" : "mc_form_text_l", &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, chara, &b->formBox[i]);
            if (BK_ENT(BK_MATCH.ent[i]).flags & TOUR_ENT_PLAYER) {
                uv.x0 = (BK_ENT(BK_MATCH.ent[i]).player % cols) * 0x60;
                uv.y0 = (BK_ENT(BK_MATCH.ent[i]).player / cols) * 0x40;
                uv.x1 = uv.x0 + 0x60;
                uv.y1 = uv.y0 + 0x40;
            } else {
                uv.x0 = 0x120;
                uv.y0 = 0x40;
                uv.x1 = 0x180;
                uv.y1 = 0x80;
            }
            Flash_FindLabel(flash, NULL, i ? "mc_vs_number_r" : "mc_vs_number_l", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
            if (b->round == 4) {
                plate = (b->winSide == i) ? 3 : 2;
            } else {
                plate = b->winSide == i;
            }
            uv.x1 = 0x100;
            uv.x0 = 0;
            uv.y0 = plate * 0x40;
            uv.y1 = uv.y0 + 0x40;
            Flash_FindLabel(flash, NULL, i ? "mc_victory_or_defeat_r" : "mc_victory_or_defeat_l", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        }
    }
}
