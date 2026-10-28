#include "common.h"
#include "battle/battle.h"
#include "sys/adx.h"
#include "sys/dma.h"
#include "sys/job.h"
#include "sys/pad.h"

/*
 * Battle scene top level, 0x12B570..0x12BD58. The frame order, the pause/restart/exit protocol and the
 * split-screen differences are described in include/battle/battle.h.
 *
 * Everything this file calls is another module; the ones still called func_XXXXXXXX are described here
 * from a first read of their code (not decompiled):
 *
 *   sound       Snd_StopBankAndResume(n)   two sound-driver commands (0xA, then 6 with (n, 0)); n = 0x3C at restart
 *               Snd_SendFighters      per-frame, walks both sides' fighters through 0x207460 / 0x2074B8
 *   scene mgr   BtlScene_Init(0) init / BtlScene_Term term / BtlScene_Reset(0) reset / BtlScene_Update and
 *               BtlScene_PostUpdate the two per-frame updates / BtlScene_Draw(first) draw / BtlScene_SetSingleView(v)
 *               stores v in mgr+0x24. Next module (0x12C9F0..), state at gp 0x2FE9A0 (0x34 bytes); its
 *               init also runs BtlPool_Init.
 *               BtlScene_CheckStageChange      per-frame, not in modes 4..7: walks a table of that module (count at +0x6400)
 *   loader      BtlLoad_PollCharaRequest / BtlLoad_PollObjectRequest   per side: poll 0x20B200 / 0x20B248 for a pending request and
 *               push a loader Job (step functions 0x1278B0 / 0x127680), i.e. in-battle streaming
 *               BattleResult_Finish      at Term: fills the BattleResult block (calls 0x128BC8)
 *   fighters    BtlChars_CheckStart, BtlChars_SampleInput, BtlChars_UpdateInput, BtlChars_UpdateMain, BtlChars_PostScene, BtlChars_EndFrame:
 *               each is a loop "for every BtlChar_Get(i): per-fighter step", skipped under
 *               BATTLE_FLAG_PAUSE and/or BATTLE_FLAG_LOADING. They are the phases of the fighter update.
 *               BtlAiMgr_Init init / BtlAiMgr_Term term / BtlAiMgr_Update update: 0xA60-byte block at gp
 *               0x2FEB10 holding two 0x520-byte entries (one per side)
 *               StgNav_Init init / StgNav_Term term: 0x14-byte manager at gp 0x2FEB0C with two 0xC00 buffers
 *               EftDet_Update      four sub-updates (0x1AF8D8, 0x1B0910, 0x1B10F0, 0x1B0030), skipped when paused
 *   stage       StgModel_InitStage binds the stage data from gCommonRes->0x24; BtlStage_Reset reset,
 *               BtlStage_Term term (state at gp 0x2FEBE0); StgModel_Cull(view) and StgModel_Draw(view)
 *               draw the stage for one view (skipped under BATTLE_FLAG_LOADING)
 *               BtlStage_Update      per-frame update of the 0x24xxxx stage-side systems (skipped when LOADING)
 *   effects     StgFx_Init init / StgFx_Term term / StgFx_Reset reset of a group of a dozen
 *               subsystems (0x105F30..0x109848 and 0x244000..0x248000), enable word at gp 0x2FF208;
 *               StgFx_DrawPre, StgFx_DrawNop, StgFx_DrawPost, StgFx_DrawOverlay are its four draw passes
 *   objects     BtlObj_UpdateAll per-frame update of the object list (lighting colour, list walk);
 *               BtlObj_UpdateVisibility(view) builds the visibility data of every listed object for one view;
 *               BtlObj_FinishVisibility(split) finishes it for 1 (split == 0) or 2 views
 *   gfx         Ot_Init init / Ot_Term term of a 0x48-byte block at gp 0x2FE8A0;
 *               Gfx_AddDefaultEnv emits a direct GS packet (0x102208) before the fighter pass;
 *               Ot_Draw ends the fighter pass; Gfx_MarkPass(n) is an empty stub taking a pass id
 *               BtlObjDraw_Draw      a full-screen pass between the first two effect passes
 *   overlays    Font_FlushAll (0x23A2D0(0)); FontIcon_Stub23D1E0 is an empty stub
 *   menu side   Gsc_Update walks the list at 0x333B80; BtlScript_Update acts in sequence states 3 and 5 only
 */

extern s32 gBtlCamView;    /* current view, set by BtlCam_SelectView / BtlCam_ApplyView */
extern s32 gBattleProf[];  /* argument of the stripped profiler stubs */

extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 vsyncs);
extern void Snd_Update(void);
extern void Fade_Start(s32 idx, s32 dir, f32 seconds);
extern void Fade_ResetAll(void);
extern void Dbg_ProfMark(s32 *prof);
extern void Dbg_ProfColor(s32 *prof, u32 rgba);

extern void Battle_ResetWork(void);
extern void Battle_Load(void);
extern void Battle_Unload(void);
extern void Battle_UpdateWork(void);

extern void BtlChar_AllocAll(s32 sides);
extern void BtlChar_FreeAll(void);
extern void BtlChar_ResetAll(void);

extern void BtlCam_Init(void);
extern void BtlCam_Reset(void);
extern void BtlCam_Term(void);
extern void BtlCam_SelectView(s32 view);
extern void BtlCam_ApplyView(s32 setScissor);
extern void BtlCam_UpdateView(s32 view);
extern s32 BtlCam_UpdateOverride(void);

extern void BtlGame_Reset(void);
extern void BtlGame_Init(void);
extern void BtlGame_Term(void);
extern void BtlGame_PreUpdate(void);
extern s32 BtlGame_Update(void);
extern void BtlGame_Draw(void);

extern void Gfx_AddDefaultEnv(void);
extern void Gfx_MarkPass(s32 pass);
extern void Ot_Init(void);
extern void Ot_Term(void);
extern void Ot_Draw(void);
extern void BtlObjDraw_Draw(void);
extern void StgModel_InitStage(void);
extern void StgModel_Cull(s32 view);
extern void StgModel_Draw(s32 view);
extern void Snd_StopBankAndResume(s32 arg);
extern void Snd_SendFighters(void);
extern void BtlLoad_PollObjectRequest(void);
extern void BtlLoad_PollCharaRequest(void);
extern void BattleResult_Finish(void);
extern void BtlScene_Init(s32 arg);
extern void BtlScene_Term(void);
extern void BtlScene_Update(void);
extern void BtlScene_PostUpdate(void);
extern void BtlScene_Reset(s32 arg);
extern void BtlScene_Draw(s32 first);
extern void BtlScene_SetSingleView(s32 singleView);
extern void BtlScene_CheckStageChange(void);
extern void EftDet_Update(void);
extern void StgNav_Term(void);
extern void StgNav_Init(void);
extern void BtlAiMgr_Init(void);
extern void BtlAiMgr_Term(void);
extern void BtlAiMgr_Update(void);
extern void BtlChars_SampleInput(void);
extern void BtlChars_CheckStart(void);
extern void BtlChars_UpdateInput(void);
extern void BtlChars_UpdateMain(void);
extern void BtlChars_PostScene(void);
extern void BtlChars_EndFrame(void);
extern void Font_FlushAll(void);
extern void FontIcon_Stub23D1E0(void);
extern void BtlStage_Reset(void);
extern void BtlStage_Term(void);
extern void BtlStage_Update(void);
extern void StgFx_Reset(void);
extern void StgFx_Init(void);
extern void StgFx_Term(void);
extern void StgFx_DrawPre(void);
extern void StgFx_DrawNop(void);
extern void StgFx_DrawPost(void);
extern void StgFx_DrawOverlay(void);
extern void BtlObj_UpdateAll(void);
extern void BtlObj_UpdateVisibility(s32 view);
extern void BtlObj_FinishVisibility(s32 split);
extern void Gsc_Update(void);
extern void BtlScript_Update(void);
/* Puts every battle subsystem back to the start of a match (first start and rematch) and fades in over 1 s. */
s32 Battle_Restart(void) {
    Snd_StopBankAndResume(0x3C);
    Battle_ResetWork();
    BtlCam_Reset();
    BtlScene_Reset(0);
    StgFx_Reset();
    BtlGame_Reset();
    BtlStage_Reset();
    BtlChar_ResetAll();
    Fade_ResetAll();
    Fade_Start(0, 1, 1.0f);
    return 0;
}

/* Loads the battle data, creates every battle subsystem, then starts the first match. */
s32 Battle_Init(void) {
    Battle_Load();
    StgFx_Init();
    StgModel_InitStage();
    BtlChar_AllocAll(2);
    BtlAiMgr_Init();
    BtlCam_Init();
    StgNav_Init();
    BtlGame_Init();
    Ot_Init();
    BtlScene_Init(0);
    Battle_Restart();
    return 0;
}

/* Stores the result and destroys every battle subsystem. */
s32 Battle_Term(void) {
    BattleResult_Finish();
    Adx_StopAll();
    BtlChar_FreeAll();
    BtlCam_Term();
    BtlScene_Term();
    Ot_Term();
    StgFx_Term();
    BtlStage_Term();
    BtlAiMgr_Term();
    StgNav_Term();
    BtlGame_Term();
    Battle_Unload();
    Fade_ResetAll();
    Dma_ResetBuffers();
    Job_Clear();
    return 0;
}

/* One frame of simulation: fighters, scene, loaders, cameras, visibility. Returns 0 when one full-screen view is forced. */
s32 Battle_Update(void) {
    s32 i;
    s32 singleView;

    BtlChars_UpdateInput();
    BtlChars_UpdateMain();
    BtlScene_Update();
    EftDet_Update();
    BtlScene_PostUpdate();
    BtlChars_PostScene();
    BtlScene_CheckStageChange();
    BtlLoad_PollCharaRequest();
    BtlLoad_PollObjectRequest();
    BtlStage_Update();
    BtlCam_SelectView(0);
    BtlCam_UpdateView(0);
    BtlCam_SelectView(1);
    BtlCam_UpdateView(1);
    singleView = BtlCam_UpdateOverride();
    BtlChars_EndFrame();
    BtlObj_UpdateAll();
    if (Battle_IsSplitScreen() && !singleView) {
        for (i = 0; i < 2; i++) {
            BtlCam_SelectView(i);
            BtlCam_ApplyView(0);
            BtlObj_UpdateVisibility(i);
        }
        BtlObj_FinishVisibility(1);
    } else {
        BtlObj_UpdateVisibility(0);
        BtlObj_FinishVisibility(0);
    }
    BtlScene_SetSingleView(singleView);
    return singleView == 0;
}

/* Draws the frame with one view: stage, effects, fighters, effects, HUD, overlays. */
s32 Battle_Draw(void) {
    s32 *prof = gBattleProf;
    s32 view = gBtlCamView;

    Dbg_ProfMark(prof);
    Gfx_MarkPass(1);
    StgModel_Cull(view);
    StgModel_Draw(view);
    Dbg_ProfColor(prof, 0x80FF4040);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(3);
    StgFx_DrawPre();
    Dbg_ProfColor(prof, 0x80FFFFFF);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(2);
    BtlObjDraw_Draw();
    Dbg_ProfColor(prof, 0x8040FF40);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(3);
    StgFx_DrawNop();
    Dbg_ProfColor(prof, 0x80FFFFFF);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(4);
    Gfx_AddDefaultEnv();
    BtlScene_Draw(1);
    Ot_Draw();
    Dbg_ProfColor(prof, 0x804040FF);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(3);
    StgFx_DrawPost();
    Dbg_ProfColor(prof, 0x80FFFFFF);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(0);
    BtlGame_Draw();
    Dbg_ProfColor(prof, 0x80FF40FF);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(0);
    Font_FlushAll();
    FontIcon_Stub23D1E0();
    Dbg_ProfColor(prof, 0x80FF40FF);
    Dbg_ProfMark(prof);
    Gfx_MarkPass(3);
    StgFx_DrawOverlay();
    Dbg_ProfColor(prof, 0x80FFFFFF);
    Gfx_MarkPass(0);
    return 0;
}

/* Draws the frame with two views: the stage pass and the fighter pass run once per view, the rest once. */
s32 Battle_DrawSplit(void) {
    s32 i;

    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(1);
    for (i = 0; i < 2; i++) {
        BtlCam_SelectView(i);
        BtlCam_ApplyView(1);
        StgModel_Cull(gBtlCamView);
        StgModel_Draw(gBtlCamView);
    }
    Dbg_ProfColor(gBattleProf, 0x80FF4040);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawPre();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(2);
    BtlObjDraw_Draw();
    Dbg_ProfColor(gBattleProf, 0x8040FF40);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawNop();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(4);
    Gfx_AddDefaultEnv();
    for (i = 0; i < 2; i++) {
        BtlCam_SelectView(i);
        BtlCam_ApplyView(1);
        BtlScene_Draw(i == 0);
        Ot_Draw();
    }
    Dbg_ProfColor(gBattleProf, 0x804040FF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawPost();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(0);
    BtlGame_Draw();
    Dbg_ProfColor(gBattleProf, 0x80FF40FF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(0);
    Font_FlushAll();
    FontIcon_Stub23D1E0();
    Dbg_ProfColor(gBattleProf, 0x80FF40FF);
    Dbg_ProfMark(gBattleProf);
    Gfx_MarkPass(3);
    StgFx_DrawOverlay();
    Dbg_ProfColor(gBattleProf, 0x80FFFFFF);
    Gfx_MarkPass(0);
    return 0;
}

/* Runs battle frames until the sequence reports the end; restarts the match when BATTLE_FLAG_RESTART is raised. */
void Battle_Loop(void) {
    s32 draw;
    s32 done;

    do {
        if (Battle_GetWork()->flags & BATTLE_FLAG_RESTART) {
            Battle_Restart();
            Battle_GetWork()->flags &= ~BATTLE_FLAG_RESTART;
        }
        Job_Run();
        Gfx_BeginFrame();
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            Gsc_Update();
            BtlScript_Update();
        }
        BtlChars_CheckStart();
        Pad_Update();
        Snd_Update();
        Snd_SendFighters();
        BtlGame_PreUpdate();
        Battle_UpdateWork();
        BtlAiMgr_Update();
        BtlChars_SampleInput();
        draw = Battle_Update();
        done = BtlGame_Update();
        if (Battle_IsSplitScreen() && draw) {
            Battle_DrawSplit();
        } else {
            Battle_Draw();
        }
        Gfx_EndFrame(2);
        Dma_Flush();
    } while (!done);
}

/* Runs one whole battle scene; called by Game_Main after the menu overlay returns. */
s32 Battle_Main(void) {
    Battle_GetWork()->running = 1;
    Battle_Init();
    Battle_Loop();
    Battle_Term();
    Battle_GetWork()->running = 0;
    return 1;
}
