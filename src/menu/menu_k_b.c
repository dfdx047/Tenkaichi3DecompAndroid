#include "common.h"
#include "menu/menu_k.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "menu/tag_tournament.h"

/*
 * Bracket, 0x364DA8..0x3660A0: the tournament bracket screen (progress mode 35), first source file: the screen's
 * own functions, which all work on gBracket. The guide's speech (menu_k_c.c), the clip set-up (menu_k_e.c) and the
 * loader and bracket logic (menu_k_f.c, menu_l) take the work pointer as an argument and are other source files.
 */

static s32 bracket_music(void) {
#ifdef PORT
    if(PortGame_IsBT4()) {static const s32 tracks[]={0x8C1,0x851,0x824,0x851,0x896};return tracks[TOUR_PROG2->tour];}
#endif
    return TOUR_PROG2->tour==TOUR_WORLD?BRK_BGM_WORLD:TOUR_PROG2->tour==TOUR_CELL?BRK_BGM_CELL:BRK_BGM_OTHER;
}
Bracket *gBracket = NULL; /* 0x3B5918 */

#define BRK_ENT(n) (gBracket->ents.e[n])
#define BRK_MATCH (gBracket->matches.m[gBracket->match])

/* Done callback of the save flow (both outcomes): leave the screen, the tournament is over. */
void Bracket_OnSaveDone(void) {
    gBracket->flags |= BRK_DONE;
    gBracket->flags |= BRK_LEAVING;
    gBracket->timer = 0xF;
    gBracket->result = 0;
}

/* Allocates and loads the screen, then takes the tournament over from gProgress (or draws a new bracket). */
void Bracket_Init(void) {
    s32 i;

    gBracket = Heap_Alloc(sizeof(Bracket), 0x20, 0, 2);
    memset(gBracket, 0, sizeof(Bracket));
    Bracket_Load(gBracket);
    McFlow_Init(1);
    McFlow_SetDoneCb(0, Bracket_OnSaveDone, 0);
    McFlow_SetDoneCb(1, Bracket_OnSaveDone, 0);
    ChrGrid_Build(&gBracket->gridOutCount, gBracket->gridBuf, &gBracket->gridCount, gBracket->grid, NULL, NULL);
    gBracket->gridCount = gBracket->gridOutCount;
    gBracket->grid = gBracket->gridBuf;
    gBracket->imageFile[0] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gBracket->imageFile[1] = Heap_Alloc(0x16800, 0x40, 0, 2);
    gBracket->imageRes[0] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gBracket->imageRes[1] = Heap_Alloc(0x20800, 0x20, 0, 2);
    gBracket->ents = TOUR_PROG2->ents;
    if (!(gProgress->flags & MPROG_TOUR_RUNNING)) {
        /* a new tournament */
        memset(&TOUR_PROG2->matches, 0, sizeof(TourMatches));
        TOUR_PROG2->round = 0;
        TOUR_PROG2->match = 0;
        Bracket_FillEntrants(gBracket);
        Bracket_ShuffleEntrants(gBracket);
#ifdef PORT
        { extern void TagTournament_BuildTeams(void *);TagTournament_BuildTeams(gBracket); }
#endif
        Bracket_InitMatches(gBracket);
        StreamSe_PlayDefault(0, PortGame_IsBT4()?0x151:BRK_SE_FANFARE);
        gBracket->flags |= BRK_FANFARE;
    }
    if (gProgress->flags & MPROG_TOUR_RUNNING) {
        /* back from a battle */
        gBracket->round = TOUR_PROG2->round;
        gBracket->match = TOUR_PROG2->match;
        gBracket->matches = TOUR_PROG2->matches;
        Bracket_LoadImages(gBracket);
        Bracket_ApplyResult(gBracket);
        Bracket_PlayCpuMatches(gBracket);
        Bgm_Play(bracket_music());
    }
    /* skip the matches no player entrant takes part in */
    while (!TagTournament_Active()) {
        if ((BRK_ENT(BRK_MATCH.ent[0]).flags & TOUR_ENT_PLAYER) || (BRK_ENT(BRK_MATCH.ent[1]).flags & TOUR_ENT_PLAYER)) {
            break;
        }
        gBracket->match++;
    }
    Bracket_SetChipTex(gBracket);
    gBracket->voiceLine = -1;
    for (i = 0; i < 2; i++) {
        TextBox_Init(&gBracket->nameBox[i], gBracket->nameText, i + 1);
        TextBox_Init(&gBracket->formBox[i], gBracket->formText, i + 3);
    }
}

/* Frees everything Init and Load allocated. */
void Bracket_Term(void) {
    s32 i;

    GetWin_Term();
    Dialog_Term();
    MsgWin_Term();
    McFlow_Term();
    TourBg_Term();
    for (i = 0; i < BRACKET_FLASH_NUM; i++) {
        Flash_Destroy(&gBracket->flash[i]);
    }
    if (gBracket->imageFile[0] != NULL) {
        Heap_Free(gBracket->imageFile[0]);
        gBracket->imageFile[0] = NULL;
    }
    if (gBracket->imageFile[1] != NULL) {
        Heap_Free(gBracket->imageFile[1]);
        gBracket->imageFile[1] = NULL;
    }
    if (gBracket->imageRes[0] != NULL) {
        Heap_Free(gBracket->imageRes[0]);
        gBracket->imageRes[0] = NULL;
    }
    if (gBracket->imageRes[1] != NULL) {
        Heap_Free(gBracket->imageRes[1]);
        gBracket->imageRes[1] = NULL;
    }
    if (gBracket->res != NULL) {
        Heap_Free(gBracket->res);
        gBracket->res = NULL;
    }
    if (gBracket->file != NULL) {
        Heap_Free(gBracket->file);
        gBracket->file = NULL;
    }
    if (gBracket->pack != NULL) {
        Heap_Free(gBracket->pack);
        gBracket->pack = NULL;
    }
    if (gBracket != NULL) {
        Heap_Free(gBracket);
        gBracket = NULL;
    }
}

/* Draws the backdrop, the tree, the moving chips, the guide with its message window, the panels and the reward window. */
void Bracket_Draw(void) {
    TourBg_Draw();
    Bracket_SetupDraw(gBracket);
    Flash_Draw(&gBracket->flash[BRK_FL_TREE]);
    Flash_Draw(&gBracket->flash[BRK_FL_MOVE_A]);
    Flash_Draw(&gBracket->flash[BRK_FL_MOVE_B]);
    if (gBracket->flags & BRK_VS_FRONT) {
        Flash_Draw(&gBracket->flash[BRK_FL_VS]);
        Flash_Draw(&gBracket->flash[BRK_FL_TITLE]);
        MsgWin_Draw(0, 0, (gBracket->flags & BRK_NO_TEXT) ? -1 : gBracket->voiceLine);
        Flash_Draw(&gBracket->flash[BRK_FL_GUIDE]);
    } else {
        MsgWin_Draw(0, 0, (gBracket->flags & BRK_NO_TEXT) ? -1 : gBracket->voiceLine);
        Flash_Draw(&gBracket->flash[BRK_FL_GUIDE]);
        Flash_Draw(&gBracket->flash[BRK_FL_VS]);
        Flash_Draw(&gBracket->flash[BRK_FL_TITLE]);
    }
    GetWin_Draw();
    TagTournament_DrawBracket(gBracket);
}

/* Advances the movies and the bracket animation: the two chips meet, the winner moves up, the next step is chosen. */
void Bracket_Update(void) {
    s32 i;

    for (i = 0; i < BRACKET_FLASH_NUM; i++) {
        Flash_Advance(&gBracket->flash[i]);
    }
    if (gBracket->flags & BRK_FANFARE) {
        if (StreamSe_IsStopped(0)) {
            Bgm_Play(bracket_music());
            gBracket->flags &= ~BRK_FANFARE;
        }
    } else if (!(gBracket->flags&BRK_LEAVING) && Bgm_IsStopped()) {
        Bgm_Play(bracket_music());
    }
    if (gBracket->flags & BRK_SCROLL_BACK) {
        gBracket->scroll += gBracket->scrollSpeed;
        if ((s32)gBracket->scroll >= 0x200) {
            gBracket->scroll = 512.0f;
            gBracket->flags ^= BRK_SCROLL_BACK;
        }
    }
    if (gBracket->flags & BRK_BUSY) {
        return;
    }
    if (!(gBracket->flags & (BRK_INPUT | BRK_MOVE_B))) {
        if (gBracket->flash[BRK_FL_TREE].flags & 1) {
            if (gBracket->flash[BRK_FL_TREE].flags & 8) {
                if (gProgress->flags & MPROG_TOUR_RUNNING) {
                    Bracket_StartWin(gBracket);
                } else {
                    gBracket->flags |= BRK_INPUT;
                }
            }
        }
    }
    if (gBracket->flags & BRK_MOVE_A) {
        if (gBracket->flash[BRK_FL_MOVE_A].flags & 1) {
            if (gBracket->flash[BRK_FL_MOVE_A].se & 1) {
                Snd_PlaySe(2, 0x3A);
            }
            if (gBracket->flash[BRK_FL_MOVE_A].flags & 8) {
                s32 frames;

                gBracket->flags ^= BRK_MOVE_A;
                gBracket->seq = 1;
                Flash_Reset(&gBracket->flash[BRK_FL_VS], 1);
                Flash_Play(&gBracket->flash[BRK_FL_VS], 1);
                Flash_GotoLabel(&gBracket->flash[BRK_FL_GUIDE], "fl_guide_out", 1);
                MsgWin_Close();
                gBracket->flags |= BRK_SCROLL_BACK;
                frames = 30;
                gBracket->scrollSpeed = (0x200 - (s32)gBracket->scroll) / frames;
            }
        }
    }
    if (gBracket->flags & BRK_MOVE_B) {
        if (gBracket->flash[BRK_FL_MOVE_B].flags & 8) {
            gBracket->flags ^= BRK_MOVE_B;
            gBracket->flags |= BRK_INPUT;
            BRK_ENT(BRK_MATCH.ent[gBracket->winSide ^ 1]).flags |= TOUR_ENT_LOST;
            BRK_ENT(BRK_MATCH.ent[0]).flags &= ~TOUR_ENT_HIDDEN;
            BRK_ENT(BRK_MATCH.ent[1]).flags &= ~TOUR_ENT_HIDDEN;
            Flash_SetFlag(&gBracket->flash[BRK_FL_MOVE_B], 4, 1);
            BRK_ENT(BRK_MATCH.ent[gBracket->winSide]).pos = BRK_MATCH.pos;
            if (gBracket->flags & BRK_PLAYER_OUT) {
                gBracket->flags |= BRK_BUSY;
                gBracket->flags ^= BRK_INPUT;
                if (TOUR_PROG2->entry == 0 && (BRK_MATCH.flags & TOUR_MATCH_FINAL)) {
                    gBracket->flags |= BRK_RUNNER_UP;
                    gBracket->seq = 0x15F;
                } else {
                    gBracket->seq = 0xC9;
                }
            } else {
                if (BRK_MATCH.flags & TOUR_MATCH_FINAL) {
                    if (TagTournament_Active() || (BRK_ENT(BRK_MATCH.ent[gBracket->winSide]).flags & TOUR_ENT_PLAYER)) {
                        gBracket->flags ^= BRK_INPUT;
                        if (TOUR_PROG2->entry == 0) {
                            gBracket->seq = 0xFB;
                        } else {
                            gBracket->seq = 0x12D;
                        }
                        gBracket->flags |= BRK_BUSY;
                        return;
                    }
                } else if (BRK_MATCH.flags & TOUR_MATCH_TO_LEFT) {
                    gBracket->matches.m[BRK_MATCH.next].ent[0] = BRK_MATCH.ent[gBracket->winSide];
                } else if (BRK_MATCH.flags & TOUR_MATCH_TO_RIGHT) {
                    gBracket->matches.m[BRK_MATCH.next].ent[1] = BRK_MATCH.ent[gBracket->winSide];
                }
                Bracket_NextMatch(gBracket);
            }
        }
    }
}

/* Left / right scroll the tree while the player may look at it; confirm starts the next match. */
void Bracket_Input(s32 *result) {
    if (gBracket->flags & BRK_INPUT) {
        if(!gBracket->seq && (gPad[0].gamePressed&PADG_SQUARE) && TagTournament_CpuMatch(gBracket)) {TagTournament_SkipMatch(gBracket);Snd_PlaySe(1,1);return;}
        if (gPad[0].gameHeld & 1) {
            if (gBracket->scroll < 0.0f) {
                gBracket->scroll += 3.0f;
                if (gBracket->scroll > 0.0f) {
                    gBracket->scroll = 0.0f;
                }
            }
        } else if (gPad[0].gameHeld & 2) {
            if (gBracket->scroll > -512.0f) {
                gBracket->scroll -= 3.0f;
                if (gBracket->scroll < -512.0f) {
                    gBracket->scroll = -512.0f;
                }
            }
        } else if (gPad[0].gamePressed & 0x200) {
            Bracket_StartVs(gBracket);
            gBracket->flags ^= BRK_INPUT;
            Snd_PlaySe(1, 1);
        }
    }
}

/*
 * Runs the bracket screen until it fades out. Returns 1 to go on to a battle (the tournament state is written back
 * to gProgress and Bracket_SetupBattle sets the battle up) or 0 when the tournament is over (the state is cleared).
 */
s32 Bracket_Run(void) {
    s32 result = 1;

    Bracket_Init();
    gBracket->result = result;
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Bracket_UpdateImages(gBracket);
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            Bracket_UpdateSeq(gBracket);
            Bracket_Update();
        }
        Bracket_Draw();
        gBracket->mcState = McFlow_Update();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gProgress->flags & MPROG_TOUR_RUNNING)) {
                if (gBracket->started == 0) {
                    gBracket->started = 1;
                    gBracket->seq = 0x33;
                }
            } else if (gBracket->started == 0) {
                gBracket->started = 1;
                gBracket->seq = 0x97;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gBracket->loadState != 0) {
                continue;
            }
            break;
        }
        if (gBracket->flags & BRK_LEAVING) {
            if (--gBracket->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gBracket->mcState == 0) {
            Bracket_Input(&gBracket->result);
        }
    }
    if (gBracket->result != 0) {
        gProgress->flags |= MPROG_TOUR_RUNNING;
        TOUR_PROG2->round = gBracket->round;
        TOUR_PROG2->match = gBracket->match;
        TOUR_PROG2->ents = gBracket->ents;
        TOUR_PROG2->matches = gBracket->matches;
        Bracket_SetupBattle(gBracket);
    } else {
        TOUR_PROG2->round = 0;
        TOUR_PROG2->match = 0;
        memset(&TOUR_PROG2->ents, 0, sizeof(TourEntrants));
        memset(&TOUR_PROG2->matches, 0, sizeof(TourMatches));
        gProgress->flags &= ~MPROG_TOUR_RUNNING;
        gProgress->flags &= ~MPROG_TOUR_GREETED;
    }
    result = gBracket->result;
    Bracket_Term();
    Dma_ResetBuffers();
    return result;
}
