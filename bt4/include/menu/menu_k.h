#ifndef MENU_MENU_K_H
#define MENU_MENU_K_H

#include "menu/menu_a.h"

/*
 * Menu overlay DBZP.BIN, 0x364358..0x368C18 (placeholder stem "menu_k"): Dragon World Tour (progress modes 33..35),
 * the end of the tournament menu and the bracket screen. Six pieces:
 *
 *   (menu_k.c)  0x364358..0x364DA8  TourMenu  tail of the tournament menu of mode 33: merged into menu_j_b.c
 *   menu_k_b.c  0x364DA8..0x3660A0  Bracket   the bracket screen of mode 35: init, frame loop, input
 *   menu_k_c.c  0x3660A0..0x366F58  Bracket   the guide's speech and the result sequence (another source file)
 *   menu_k_d.c  0x366F58..0x3673F8  TourBg    the cloud backdrop shared with the entrant select
 *   menu_k_e.c  0x3673F8..0x368068  Bracket   per-frame clip set-up
 *   menu_k_f.c  0x368068..0x36B3E0  Bracket   the loader and the bracket logic (merged with the former menu_l.c;
 *                                             that file uses the LBracket view of menu_l.h)
 *
 * Object boundaries the data proves: one between Bracket_Update and Bracket_UpdateSeq ("fl_guide_out" exists at
 * 0x3B6168 and at 0x3B6480; put at 0x3660A0, where the functions start taking the work pointer as an argument), and
 * one exactly at 0x368068 (the read-only data restarts 16-byte aligned at 0x3B6FD0 after the jump table that ends
 * at 0x3B6FC4, and repeats "fl_guide_in"). The cuts at 0x364DA8, 0x366F58 and 0x3673F8 are module cuts; the data
 * neither confirms nor excludes them (menu_k_c / _d / _e may be one source file).
 *
 * This header does not include menu_j.h (written in parallel, it changed while this chunk was done): BrkCell
 * and BrkEntrant below are this chunk's own views of the character-grid cell and TourEntrant.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);

extern s32 rand(void);
extern void Voice_StopWithLip(void);
extern s32 StreamSe_GetStat(s32 se);
extern void StreamSe_FadeOutStep(s32 se);
extern void GetWin_Init(void *pack, s32 lang);
extern void GetWin_Term(void);
extern void GetWin_Draw(void);
extern void GetWin_Open(void);
extern void GetWin_Close(void);
extern void GetWin_Next(void);
extern s32 GetWin_IsAnimating(void);
extern void GetWin_Setup(s32 kind, s32 value);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void Flash_Reset(MFlash *flash, s32 keepClips);
extern void Flash_SetOffset(MFlash *flash, s32 x, s32 y);
extern void Flash_SetFlag(MFlash *flash, u32 mask, u8 on);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Voice bank base of the tournament guides (Voice_PlayWithSubtitle). */
#define TOUR_VOICE_BASE 0x85D3

/* A cell of the character grid (ChrGridCell of include/battle/view_a.h; local view). */
typedef struct BrkCell {
    /* 0x00 */ s32 id;
    /* 0x04 */ s32 formCount;
    /* 0x08 */ s32 form[7];
} BrkCell; /* 0x24 */

#define BRK_CELL_MAX 165

extern void ChrGrid_Build(s32 *outCount, BrkCell *out, s32 *inCount, BrkCell *in, s32 *customCount, BrkCell *custom);

/* The five tournaments, in menu order. */
#define TOUR_WORLD 0             /* World Tournament ("mc_guide_tenkaichi") */
#define TOUR_BIG 1               /* World Martial Arts Big Tournament (Mr. Satan guide) */
#define TOUR_CELL 2              /* Cell Games */
#define TOUR_OTHERWORLD 3        /* Otherworld Tournament ("mc_guide_anoyo") */
#define TOUR_YAMCHA 4            /* Yamcha Game (Yamcha and Puar) */

/* TourMenu (the former menu_k.c, 0x364358..0x364DA8) is now part of menu_j_b.c; its view is TourMenu of menu_j.h. */

/* gSaveData->unkA08 */
#define TOUR_SAVE_STARTED 0x20  /* the first-visit speech was heard */

/* What the tournament keeps of one entrant: gProgress + 0x98, 0x28 bytes each (TourEntrant of menu_j.h). */
typedef struct BrkEntrant {
    /* 0x00 */ u16 flags;        /* TOUR_ENT_ */
    /* 0x02 */ u16 unk2;
    /* 0x04 */ s32 chara;
    /* 0x08 */ s32 costume;
    /* 0x0C */ s32 player;       /* index among the player's choices: picks the "1P".."8P" plate */
    /* 0x10 */ s32 pos;          /* bracket position the chip sits on (argument of Bracket_GetPosX / 00368F90) */
    /* 0x14 */ f32 health;       /* health left after the last battle (menu_l) */
    /* 0x18 */ u16 item[8];
} BrkEntrant; /* 0x28 */

#define TOUR_ENTRANT_MAX 17


/* MFlash.trig bits the screens test. */
#define MFLASH_TRIG_1 1
#define MFLASH_TRIG_8 8

/* ---- gProgress as the tournament uses it (complete view of 0x80..0x440) ---- */

/* One match of the bracket. */
typedef struct TourMatch {
    /* 0x00 */ u16 flags;       /* TOUR_MATCH_ */
    /* 0x02 */ u16 ent[2];      /* entrant indices, left and right */
    /* 0x06 */ u16 next;        /* match the winner goes to */
    /* 0x08 */ u16 pos;         /* bracket position the winner's chip moves to */
    /* 0x0A */ u16 vsAnim;      /* animation numbers and view (menu_l) */
    /* 0x0C */ u16 view;
    /* 0x0E */ u16 winAnim;
} TourMatch; /* 0x10 */

#define TOUR_MATCH_DONE 1
#define TOUR_MATCH_ROUND_END 2
#define TOUR_MATCH_TO_LEFT 4     /* the winner becomes ent[0] of match `next` */
#define TOUR_MATCH_TO_RIGHT 8    /* the winner becomes ent[1] of match `next` */
#define TOUR_MATCH_FINAL 0x10    /* the last match */

#define TOUR_MATCH_MAX 16

/* The two tables are copied between gProgress and the screen's work area by structure assignment. */
typedef struct TourEntrants {
    BrkEntrant e[TOUR_ENTRANT_MAX];
} TourEntrants; /* 0x2A8 */

typedef struct TourMatches {
    TourMatch m[TOUR_MATCH_MAX];
} TourMatches; /* 0x100 */

typedef struct TourProgress2 {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;       /* MPROG_; 0x10 = a tournament is running, 0x20 = its opening speech was heard */
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x64];
    /* 0x080 */ s32 entry;       /* TourMenu top-level item: 0 = the real tournament (prizes), 1 = free play */
    /* 0x084 */ s32 tour;        /* TOUR_ */
    /* 0x088 */ s32 level;       /* difficulty 0..2 */
    /* 0x08C */ s32 entryNum;    /* entrants the player chooses, 1..8 */
    /* 0x090 */ s32 round;       /* round being played, 0.. */
    /* 0x094 */ s32 match;       /* match being played */
    /* 0x098 */ TourEntrants ents;
    /* 0x340 */ TourMatches matches;
} TourProgress2; /* ..0x440 */

#define TOUR_PROG2 ((TourProgress2 *)gProgress)

#define MPROG_TOUR_RUNNING 0x10
#define MPROG_TOUR_GREETED 0x20

/* BrkEntrant.flags */
#define TOUR_ENT_PLAYER 1        /* chosen by the player */
#define TOUR_ENT_LOST 2          /* drawn darker in the tree */
#define TOUR_ENT_BOSS 4          /* the seeded seventeenth entrant (menu_l) */
#define TOUR_ENT_HIDDEN 8        /* chip hidden while the move animation of this match plays */
#define TOUR_ENT_REC 0x10        /* a saved custom character (menu_j): the versus panel shows name line 0xA3 */

/* gSaveData->unkA08, continued */
#define TOUR_SAVE_EXPLAINED 0x40 /* the guide's long explanation was heard */

/* ---- Bracket (menu_k_b.c, menu_k_d.c, menu_l) ---- */

#define BRACKET_FLASH_NUM 6

/* flash[] */
#define BRK_FL_TREE 0            /* the tree with the 17 chips; scrolls sideways */
#define BRK_FL_MOVE_A 1          /* the two chips of the current match */
#define BRK_FL_MOVE_B 2          /* the winner's chip moving up */
#define BRK_FL_GUIDE 3           /* the guide character(s) */
#define BRK_FL_VS 4              /* the "versus" panel: names, forms, numbers, win / lose */
#define BRK_FL_TITLE 5           /* title plate and the result banners */

typedef struct BracketReward {
    /* 0x00 */ s32 kind;
    /* 0x04 */ s32 value;
} BracketReward;

typedef struct Bracket {
    /* 0x0000 */ void *pack;         /* file baseFile + 6 + tournament (compressed) */
    /* 0x0004 */ u32 *res;           /* the same unpacked */
    /* 0x0008 */ void *imageFile[2]; /* 0x16800 bytes each */
    /* 0x0010 */ void *imageRes[2];  /* 0x20800 bytes each */
    /* 0x0018 */ u32 *chips;         /* section 23: pack of the small character pictures */
    /* 0x001C */ void *subtitles;    /* section 39 */
    /* 0x0020 */ void *msgText;      /* section 25 */
    /* 0x0024 */ void *nameText;     /* section 21 */
    /* 0x0028 */ void *formText;     /* section 22 */
    /* 0x002C */ void *file;         /* file 0x3C9 + tournament: the backdrop */
    /* 0x0030 */ MFlash flash[BRACKET_FLASH_NUM];
    /* 0x0138 */ s32 unk138;
    /* 0x013C */ MTexRes *guideTex[2]; /* sections 43 / 44: the two picture sets of the guide */
    /* 0x0144 */ s32 unk144;
    /* 0x0148 */ u8 *texTree[26];    /* textures of flash[0] */
    /* 0x01B0 */ u8 *texTitle[38];   /* flash[5] */
    /* 0x0248 */ u8 *texGuide[9];    /* flash[3] */
    /* 0x026C */ u8 *texMoveA[6];    /* flash[1] */
    /* 0x0284 */ u8 *texMoveB[6];    /* flash[2] */
    /* 0x029C */ u8 *texVs[16];      /* flash[4] */
    /* 0x02DC */ s32 flags;          /* BRK_ */
    /* 0x02E0 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x02E4 */ s32 winSide;        /* side of the current match that won (0 left, 1 right) */
    /* 0x02E8 */ s32 result;         /* Run's result: 1 = go on to a battle, 0 = the tournament is over */
    /* 0x02EC */ s32 pose;           /* which picture set / which of the pair's clips the guide uses */
    /* 0x02F0 */ s32 talker;         /* Yamcha Game: which of the two guides talks */
    /* 0x02F4 */ s32 gridCount;
    /* 0x02F8 */ BrkCell *grid;
    /* 0x02FC */ s32 gridOutCount;
    /* 0x0300 */ BrkCell gridBuf[BRK_CELL_MAX];
    /* 0x1A34 */ s32 timer;          /* frames until the fade out starts */
    /* 0x1A38 */ s32 started;        /* the opening step was chosen once the fade in ended */
    /* 0x1A3C */ s32 seq;            /* step of Bracket_UpdateSeq, 0 = idle */
    /* 0x1A40 */ s32 loadState;      /* loader of the two large pictures (menu_l); Run waits for 0 after the fade out */
    /* 0x1A44 */ s32 round;
    /* 0x1A48 */ s32 match;          /* index into match[] */
    /* 0x1A4C */ s32 mcState;        /* McFlow_Update result: non-zero while the save flow runs */
    /* 0x1A50 */ s32 scrollSpeed;    /* per frame, back to the right edge */
    /* 0x1A54 */ f32 scroll;         /* horizontal offset of the tree, -512..0 (.. 512 while it leaves) */
    /* 0x1A58 */ s32 blink[2];
    /* 0x1A60 */ s32 talk[2];
    /* 0x1A68 */ TourEntrants ents;
    /* 0x1D10 */ TourMatches matches;
    /* 0x1E10 */ MTextBox nameBox[2];
    /* 0x1F28 */ MTextBox formBox[2];
    /* 0x2040 */ void *cpu;          /* section 31: the CPU opponents' strength (menu_l) */
    /* 0x2044 */ void *prize;        /* section 41: the prize table (menu_l) */
    /* 0x2048 */ BracketReward reward[8];
    /* 0x2088 */ s32 rewardCount;
    /* 0x208C */ s32 rewardIdx;
} Bracket; /* 0x2090 */

#define BRK_DONE 1
#define BRK_LEAVING 2            /* the leave timer runs */
#define BRK_INPUT 4              /* the player can scroll the tree and confirm */
#define BRK_NO_TEXT 8            /* the message window shows no line */
#define BRK_RESULT 0x10          /* a battle result was taken over (menu_l) */
#define BRK_MOVE_B 0x20          /* flash[2] plays: the winner's chip moves up */
#define BRK_MOVE_A 0x40          /* flash[1] plays: the two chips meet */
#define BRK_BUSY 0x80            /* a result sequence runs: no bracket animation */
#define BRK_RESULT0 0x100
#define BRK_IMAGES 0x200         /* the two large pictures are loaded (menu_l): confirm may skip the speech */
#define BRK_SCROLL_BACK 0x400    /* the tree slides out to +512 */
#define BRK_RUNNER_UP 0x1000     /* the result plate reads "second place" */
#define BRK_PLAYER_OUT 0x4000    /* no player entrant is left (set in menu_l) */
#define BRK_VS_FRONT 0x40000     /* the versus panel and banners are drawn under the guide */
#define BRK_FANFARE 0x80000      /* the opening jingle plays; the music starts when it ends */

#define BRK_BGM_WORLD PORT_V(0x10B29, 0x829)
#define BRK_BGM_CELL PORT_V(0x10B24, 0x824)
#define BRK_BGM_OTHER PORT_V(0x10B18, 0x86A)
#define BRK_BGM_YAMCHA PORT_V(0x10B18, 0x8CF)
#define BRK_SE_FANFARE PORT_V(0x10BCA, 0x151)

extern Bracket *gBracket;        /* 0x3B5918 */

/* menu_l: the bracket's logic (names of config/symbols/menu_l.txt) */
extern s32 Bracket_GetPosX(s32 pos);                       /* screen x of tree position `pos` */
extern s32 Bracket_GetPosY(s32 pos);                       /* screen y of tree position `pos` */
extern void Bracket_InitMatches(Bracket *b);
extern void Bracket_FillEntrants(Bracket *b);              /* players' entrants + random CPU entrants */
extern void Bracket_ShuffleEntrants(Bracket *b);
extern void Bracket_SetChipTex(Bracket *b);
extern void Bracket_PlayCpuMatches(Bracket *b);            /* decides the CPU-only matches at random */
extern void Bracket_NextMatch(Bracket *b);
extern void Bracket_ApplyResult(Bracket *b);               /* takes over the result of the battle just fought */
extern void Bracket_StartVs(Bracket *b);                   /* starts flash[1]: the two chips meet */
extern void Bracket_StartWin(Bracket *b);                  /* starts flash[2]: the winner moves up */
extern void Bracket_UpdateImages(Bracket *b);              /* background loader of the two large pictures */
extern void Bracket_LoadImages(Bracket *b);
extern void Bracket_SetupBattle(Bracket *b);               /* the battle hand-off (BattleSetup_*) */
extern void Bracket_GivePrizes(Bracket *b, s32 second);    /* fills reward[] and the save */

void Bracket_OnSaveDone(void);
void Bracket_Init(void);
void Bracket_Term(void);
void Bracket_Draw(void);
void Bracket_Update(void);
void Bracket_Input(s32 *result);
s32 Bracket_Run(void);
void Bracket_SetTalker(Bracket *b, s32 line);
void Bracket_UpdateSeq(Bracket *b);
void Bracket_SetupDraw(Bracket *b);
void Bracket_Load(Bracket *b);

/* ---- TourBg (menu_k_c.c) ---- */

typedef struct TourBg {
    /* 0x00 */ u32 *res;         /* the backdrop file unpacked */
    /* 0x04 */ MFlash flash[1];
    /* 0x30 */ void *bg;         /* section 1: background picture */
    /* 0x34 */ u8 *tex[11];
    /* 0x60 */ s32 kind;         /* TOUR_: which tournament's backdrop */
    /* 0x64 */ f32 scroll;       /* cloud scroll position */
} TourBg; /* 0x68 */

extern TourBg *gTourBg;          /* 0x3B591C */

void TourBg_Init(void *file, u32 kind, u8 **tex);
void TourBg_Term(void);
void TourBg_Draw(void);

#endif
