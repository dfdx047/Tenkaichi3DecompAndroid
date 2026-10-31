#ifndef BATTLE_VIEW_B_H
#define BATTLE_VIEW_B_H

#include "types.h"

/*
 * Menu support code of the main executable, 0x2600B0..0x263098 (placeholder stem "view_b"). Five parts:
 *
 *   view_b.c    0x2600B0..0x260D20  TextBox    the rest of the text box module (it starts in view_a_e.c and is
 *                                              the same object); TextBox_DrawClip is INCLUDE_ASM
 *   view_b_b.c  0x260D20..0x2614B0  ChrTbl / ItemSet / ItemTbl  readers of the character and item tables
 *   view_b_c.c  0x2614B0..0x261ED8  MenuUtil   voice line with mouth movement (LipSync), sound options, CPU level
 *                                              mapping, the CPU against CPU demo battle, small text helpers
 *   view_b_d.c  0x261ED8..0x262FF0  ShenScene  the 3D backdrop of the dragon (wish) screen; ShenScene_StepSeq is
 *                                              INCLUDE_ASM
 *   view_b_e.c  0x262FF0..0x263098  Dbg        empty debug functions (the head of sys/debug.c)
 */

/* ---- TextBox (view_b.c): the full layout of the 0x8C-byte TextBox of battle/view_a.h ---- */

#define TEXTBOX_FLAG_MAX_W 8      /* shrink the line horizontally to maxW */
#define TEXTBOX_FLAG_MAX_H 0x10   /* shrink the line vertically to maxH */
#define TEXTBOX_FLAG_LINE_Y 0x20  /* add lineY[rows - 1] to y */
#define TEXTBOX_FLAG_SPACING 0x40 /* use spacingX / spacingY */

#define TEXTBOX_LINE_Y_COUNT 5

/* What the movie clip's callback draws: one string with its complete style. */
typedef struct TextBoxDraw {
    /* 0x00 */ s32 align;      /* Font_SetAlign */
    /* 0x04 */ u16 *str;
    /* 0x08 */ s32 x;          /* the clip's position is added by the callback, every time it runs */
    /* 0x0C */ s32 y;
    /* 0x10 */ f32 scaleX;
    /* 0x14 */ f32 scaleY;
    /* 0x18 */ u8 color[4];    /* r, g, b, a (0x80 = opaque) */
    /* 0x1C */ u8 shadow[4];
    /* 0x20 */ s32 clip[4];    /* Font_SetClip */
    /* 0x30 */ s32 noFlush;    /* 1: the text is left in the font queue instead of being drawn at once */
    /* 0x34 */ s32 spacingX;   /* Font_SetSpacing */
    /* 0x38 */ s32 spacingY;
} TextBoxDraw; /* size 0x3C */

typedef struct TextBoxFull {
    /* 0x00 */ s32 flags;      /* TEXTBOX_FLAG_* */
    /* 0x04 */ void *text;     /* text file: line n is at text + (((u32 *)text)[n + 1] & ~3) */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 x;          /* offset added to the position of every line (TextBox_SetUnkC) */
    /* 0x10 */ s32 y;
    /* 0x14 */ s32 lineY[TEXTBOX_LINE_Y_COUNT]; /* extra y for a text of 1..5 rows */
    /* 0x28 */ s32 maxW;
    /* 0x2C */ s32 maxH;
    /* 0x30 */ u8 color[4];
    /* 0x34 */ u8 shadow[4];   /* "color2" in view_a.h */
    /* 0x38 */ s32 clip[4];    /* "rect" in view_a.h */
    /* 0x48 */ s32 spacingX;
    /* 0x4C */ s32 spacingY;
    /* 0x50 */ TextBoxDraw draw; /* +0x50 align (TextBox_SetUnk50), +0x80 noFlush (TextBox_SetUnk80) */
} TextBoxFull; /* size 0x8C */

/* The drawing state a movie clip hands to its "draw over" callback (FlashProp in sys/gfxm_c.h). Local view. */
typedef struct TextBoxClipProp {
    /* 0x00 */ u8 unk0[0x38];
    /* 0x38 */ f32 x;          /* translation of the clip's matrix */
    /* 0x3C */ f32 unk3C[2];
    /* 0x44 */ f32 y;
    /* 0x48 */ f32 unk48[3];
    /* 0x54 */ f32 mul[4];     /* colour transform: c * mul + add */
    /* 0x64 */ s16 add[4];
} TextBoxClipProp; /* size 0x6C */

struct TextBox;
struct Flash;
struct FlashRef;

void TextBox_SetMaxWidth(struct TextBox *box, s32 w);
void TextBox_SetMaxHeight(struct TextBox *box, s32 h);
void TextBox_SetMaxSize(struct TextBox *box, s32 w, s32 h);
void TextBox_SetLineOffsets(struct TextBox *box, s32 y1, s32 y2, s32 y3, s32 y4, s32 y5);
void TextBox_SetSpacing(struct TextBox *box, s32 x, s32 y);
s32 TextBox_DrawClip(TextBoxDraw *draw, TextBoxClipProp *prop);
void TextBox_AttachLine(struct Flash *flash, struct FlashRef *ref, s32 x, s32 y, s32 line, struct TextBox *box);
void TextBox_AttachString(struct Flash *flash, struct FlashRef *ref, s32 x, s32 y, u16 *str, struct TextBox *box);

/* ---- ChrTbl / ItemSet (view_b_b.c): the tables of common file 4 (gCommonRes->data[2]) ---- */

#define CHRTBL_EXP_COUNT 7
#define CHRTBL_LINK_COUNT 4
#define CHRTBL_LINK_NONE 0xFF
#define CHRTBL_FLAG_0 1         /* picks which ItemTblEntry flag says "this character may equip the item" */

/* Character entry, 0x3C bytes, section 1 of the file (ChrViewInfo in battle/view_a.h is the same entry). */
typedef struct ChrTblEntry {
    /* 0x00 */ s32 aiType;      /* AI type of the character when no AI item is equipped */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u16 flags;       /* CHRTBL_FLAG_* */
    /* 0x0A */ u16 costumes;    /* number of costumes (guess) */
    /* 0x0C */ u16 cost;        /* summed over a team and compared with a limit by the menu: DP cost (guess) */
    /* 0x0E */ u16 baseLevel;   /* added to the saved level */
    /* 0x10 */ s32 exp[CHRTBL_EXP_COUNT]; /* indexed by the saved level; a zero ends the list */
    /* 0x2C */ u8 link[CHRTBL_LINK_COUNT]; /* ids of the other forms of the character, 0xFF = none */
    /* 0x30 */ f32 targetY;     /* character viewer camera */
    /* 0x34 */ f32 dist;
    /* 0x38 */ u8 unk38[4];
} ChrTblEntry; /* size 0x3C */

#define ITEMTBL_TYPE_SLOTS 1    /* its `slots` are added to the first total of ItemSet_GetBonus */
#define ITEMTBL_TYPE_AI 2       /* sets the AI type to id - ITEMTBL_AI_FIRST */
#define ITEMTBL_AI_FIRST 0x87

#define ITEMTBL_FLAG_NONE_A 1   /* flags 1 and 4: nobody may equip it */
#define ITEMTBL_FLAG_NONE_B 4
#define ITEMTBL_FLAG_8 8        /* may be equipped by a character with CHRTBL_FLAG_0 */
#define ITEMTBL_FLAG_10 0x10    /* may be equipped by a character without it */

#define ITEMSET_COUNT 8         /* ids of a set; ItemSet_Fit only looks at the first ITEMSET_USABLE */
#define ITEMSET_USABLE 7
#define ITEMSET_STAT_COUNT 4

/* Item entry, 0x28 bytes, section 2 of the file (ItemInfo in sys/save.h, GetWinItem in battle/view_a.h). */
typedef struct ItemTblEntry {
    /* 0x00 */ u8 type;         /* ITEMTBL_TYPE_* */
    /* 0x01 */ u8 unk1[2];
    /* 0x03 */ u8 slots;        /* how many of a character's item slots it takes */
    /* 0x04 */ u8 unk4[8];
    /* 0x0C */ s16 stat[ITEMSET_STAT_COUNT];   /* change of four stats */
    /* 0x14 */ s32 flags;       /* ITEMTBL_FLAG_*; 2 = owned in a new save (sys/save.h) */
    /* 0x18 */ s32 ability[4];  /* ability bits */
} ItemTblEntry; /* size 0x28 */

/* Header of common file 4: byte offsets of the sections, the low two bits are not part of the offset. */
typedef struct ChrTblFile {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ u32 charaOffset;
    /* 0x08 */ u32 itemOffset;
} ChrTblFile;

/* What a set of items adds up to. */
typedef struct ItemSetStats {
    /* 0x00 */ s32 stat[ITEMSET_STAT_COUNT];
    /* 0x10 */ s32 aiType;
} ItemSetStats; /* size 0x14 */

s32 ChrTbl_WrapCostume(s32 chara, s32 *costume);
s32 ChrTbl_GetCost(s32 chara);
s32 ChrTbl_GetLevel(s32 chara, s32 slot, s32 fromRec);
s32 ChrTbl_GetExp(s32 chara, s32 slot);
s32 ChrTbl_GetMaxExp(s32 chara);
s32 ItemSet_Fit(u16 *ids, ItemTblEntry *table, s32 capacity, s32 *used);
void ItemSet_GetBonus(u16 *ids, ItemTblEntry *table, s32 *out);
void ItemSet_GetStats(u16 *ids, s32 *stats, s32 *ability, s32 chara);
void ItemSet_GetStatsList(s32 *ids, s32 count, s32 *stats, s32 *ability);
s32 ItemTbl_CanEquip(s32 item, s32 chara, ItemTblEntry *table);
s32 ItemTbl_GetClass(s32 item, ItemTblEntry *table);

/* ---- MenuUtil (view_b_c.c) ---- */

/* Mouth movement that goes with a voice line: key frames of 4 bytes {u16 frame, u16 open}, read with
 * Mem_ReadU16 (unaligned). The data block starts with 8 unknown bytes, the key count (s32) at +8 and the
 * keys at +0x10; the frame of the last key is the length. */
typedef struct LipSync {
    /* 0x00 */ s32 active;
    /* 0x04 */ u8 *keys;
    /* 0x08 */ u16 index;       /* key the search starts from */
    /* 0x0A */ u16 count;
    /* 0x0C */ f32 time;        /* frames since the voice started playing */
    /* 0x10 */ f32 length;
} LipSync; /* size 0x14 */

#define LIPSYNC_KEY_SIZE 4

/* The lip data pack handed to Voice_PlayWithSubtitle: a pair of section offsets per voice line. */
typedef struct LipPackEntry {
    /* 0x00 */ u32 ofs[2];      /* [0] used with the first voice language, [1] with the second */
} LipPackEntry;

typedef struct LipPack {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ LipPackEntry line[1];
} LipPack;

#define VOICE_LANG2_OFFSET 0x55C /* added to a menu voice id when SaveData.flags bit 0 is set */

/* A team record of the progress block: two rows of five character ids. */
#define PROGRESS_TEAM_COUNT 7
#define PROGRESS_TEAM_EMPTY 0xA4  /* CHRGRID_ID_EMPTY */

typedef struct ProgressTeam {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 chara[2][5];
} ProgressTeam; /* size 0x2C */

/* gProgress as this file uses it (ViewProgress in battle/view_a.h has the other fields). Local view. */
typedef struct MenuUtilProgress {
    /* 0x000 */ u8 unk0[0x14];
    /* 0x014 */ s32 flags;       /* 0x100: the menu animations are frozen */
    /* 0x018 */ u8 unk18[0xC];
    /* 0x024 */ s32 demoPick;    /* the pairing Demo_SetupBattle chose last, -1 at start */
    /* 0x028 */ u8 unk28[0x69C - 0x28];
    /* 0x69C */ ProgressTeam team[PROGRESS_TEAM_COUNT];
    /* 0x7D0 */ u8 unk7D0[0x2C];
} MenuUtilProgress; /* size 0x7FC */

/* The characters that are never "the same person" as another (ChrTbl_IsRelated). */
#define CHRTBL_ID_4F 0x4F
#define CHRTBL_ID_6D 0x6D
#define CHRTBL_ID_80 0x80

/* A text file's lines are 16-bit characters stored high byte first; read as a u16 the fullwidth digits
 * U+FF10..U+FF19 are 0x10FF..0x19FF and the black circle U+25CF is 0xCF25. */
#define TEXT_CHAR_BULLET 0xCF25
#define TEXT_NUMBER_DIGITS 5
#define TEXT_NUMBER_MAX 99999

void Voice_PlayWithSubtitle(LipPack *pack, s32 base, s32 line);
void Voice_StopWithLip(void);
void SndOpt_Apply(void);
void Progress_ClearTeams(void);
s32 ChrTbl_IsRelated(s32 a, s32 b);
s32 CpuLevel_FromSetting(u32 setting);
const char *CpuLevel_SettingName(s32 setting);
s32 CpuLevel_ToSetting(s32 level);
void Demo_SetupBattle(void);
s32 MenuUtil_ClassifyId(s32 id);
void Text_FindBullet(u8 *text, s32 line, u16 **out);
void Text_PutNumber(u16 *dst, u32 value);
void LipSync_Start(u8 *data);
void LipSync_Update(void);
s32 LipSync_IsOpen(void);
void LipSync_Clear(void);

/* ---- ShenScene (view_b_d.c): the 3D backdrop of the dragon (wish) screen ---- */

#define SHENSCENE_ACTOR_COUNT 2
#define SHENSCENE_ACTOR_DRAGON 0  /* BtlObj type 1 */
#define SHENSCENE_ACTOR_BALLS 1   /* BtlObj type 3: what is on screen before the dragon appears */
#define SHENSCENE_FILE_STAGE 0x194 /* + dragon: the backdrop */

/* ShenSeq.state: what the wish screen (sys/late_a.c) reads and requests. */
#define SHENSCENE_STATE_INTRO 0   /* the summoning plays; also set again when the farewell starts */
#define SHENSCENE_STATE_READY 1   /* the dragon is there: the screen may take input */
#define SHENSCENE_STATE_ENDED 2   /* the sequence is over */
#define SHENSCENE_STATE_LEAVE 3   /* requested by the screen: play the farewell (only accepted in READY) */

struct ShenSeq;

/* The sequence of the scene. */
typedef struct ShenSeq {
    /* 0x00 */ s32 step;        /* 0..6, see ShenScene_StepSeq */
    /* 0x04 */ s32 seStep;      /* which sound comes next */
    /* 0x08 */ void *cam;       /* camera animation ShenScene_NextCam starts next, NULL = none left */
    /* 0x0C */ void *cams[2];   /* the dragon model's two camera animations */
    /* 0x14 */ s32 camIdx;
    /* 0x18 */ s32 (*fn)(struct ShenSeq *seq); /* NULL once the sequence has ended */
    /* 0x1C */ s32 state;       /* SHENSCENE_STATE_* */
} ShenSeq; /* size 0x20 */

/* A model of the scene. */
typedef struct ShenActor {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ s32 id;          /* resource handle while loading, then the BtlObj id */
    /* 0x0C */ u8 *obj;         /* BtlObj_Get(id) */
    /* 0x10 */ s32 ramp[6];     /* a Ramp (sys/ramp.h): its value is the model's glow level */
} ShenActor; /* size 0x28 */

/* The load job (the first 0xC bytes of the work block). */
typedef struct ShenJob {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 (*step)(struct ShenJob *job);
    /* 0x08 */ s32 state;
} ShenJob; /* size 0xC */

typedef struct ShenScene {
    /* 0x00 */ ShenJob job;
    /* 0x0C */ ShenActor actor[SHENSCENE_ACTOR_COUNT];
    /* 0x5C */ ShenSeq seq;
    /* 0x7C */ s32 dragon;      /* 0..2: which dragon (SHEN_DRAGON_* in sys/late_a.h) */
    /* 0x80 */ f32 time;        /* + 2 per frame; never read */
    /* 0x84 */ s32 frame;       /* frames run; times the repeating sound */
} ShenScene; /* size 0x88 */

s32 ShenScene_StepSeq(ShenSeq *seq);
s32 ShenScene_StepLoad(ShenJob *job);
void ShenScene_InitCams(ShenSeq *seq);
s32 ShenScene_NextCam(ShenSeq *seq);
s32 ShenScene_IsCamEnd(void);
s32 ShenScene_RunSeq(void);
void ShenScene_StartSeq(void);
void ShenScene_Nop(void);
s32 ShenScene_GetState(void);
void ShenScene_SetState(s32 state);
void ShenScene_Nop2(void);
s32 ShenScene_Update(void);
void ShenScene_Init(s32 dragon);
void ShenScene_Term(void);

#endif
