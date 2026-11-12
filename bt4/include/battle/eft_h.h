#ifndef BATTLE_EFT_H_H
#define BATTLE_EFT_H_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Technique effects, second part: src/battle/eft_h.c = 0x14B108..0x14F230 (61 functions).
 *
 * Three things live here.
 *
 * 1. The tail of the technique effect manager (state gp 0x2FE9F8, layer 1 of the effect scene; its head is
 *    0x14A8C0..0x14B108): EftShot_BuildParam .. EftShot_GetCharPack. Every character has five technique slots
 *    (0, 1 skills; 2, 3 techniques; 4 ultimate, see btl_tech_b.h). For each slot the manager keeps an
 *    EftHSlot and an EftShotParam, a flat copy of what the effect needs out of the character's
 *    BtlSkillData / BtlSuperData. A slot whose effect pack exists gets a "group" task (class chosen by
 *    EftShotParam.type through gEftShotClass) when the character is created, and an "instance" task as a
 *    child of the group every time the technique is started (EftShot_Start).
 *
 * 2. Two of the eleven technique effect types gEftShotClass lists:
 *      type -1 (index 0)  EftShotNull*: no effect; the instance kills itself on its first update.
 *      type  1 (index 2)  EftVolley*: a volley of up to 30 shots leaving the fighter's hands. The shots
 *                         themselves are tasks of the module at 0x16A400 (0x16A7D0 / 0x16A868 create one);
 *                         this file decides when one is fired, from which model node, in which direction,
 *                         and drives the effect pack parts around the fighter.
 *
 * 3. The first half of the emitter set library every technique effect type uses (EftEmit_*; the second half is
 *    eft_i.c, which calls a set EftEmitSet): parsing of an effect pack into an emitter set of groups ("types",
 *    one per particle module) and emitters ("parts"), EftEmit_LoadSet with 20 callers; the per-instance state
 *    (EftSetState); the decision which flags an emitter gets this frame (EftEmit_GetFlags); and seven of the
 *    per-type spawners (EftEmit_SpawnTypeN) that the dispatcher EftEmit_Spawn (0x14FF90) calls.
 *
 * Names: the manager functions continue eft_g.c (EftShot_*), the library continues into eft_i.c (EftEmit_*).
 * The struct types here are this file's own views (EftH*, EftSet*, EftShotParam = EftShotDef of eft_g.h,
 * EftSet = EftEmitSet of eft_i.h, EftSetDef = EftEmitDef, EFT_CMD_* = EFT_SPAWN_*).
 */

/* Callbacks of a task class; same layout as BtlTaskClass in btl_scene.h. */
typedef struct EftHTaskClass {
    /* 0x00 */ void *update;
    /* 0x04 */ void *init;
    /* 0x08 */ void *term;
    /* 0x0C */ void *postUpdate;
    /* 0x10 */ void *reset;
    /* 0x14 */ void *draw;
} EftHTaskClass; /* size 0x18 */

/* A task of the effect scene's task tree (0x1AD150..0x1ADC00); only what this file touches. */
typedef struct EftHTask {
    /* 0x00 */ u8 kill;        /* bit 0: BtlTaskList_Update kills the task (set by 0x1ADA58) */
    /* 0x01 */ u8 state;       /* free for the class: step of EftVolley_Update, character of the layer task */
    /* 0x02 */ u8 unk2[2];
    /* 0x04 */ s32 flags;      /* 0x800 / 0x1000: belongs to character 0 / 1 (0x1ADB78 ors a bit in) */
    /* 0x08 */ u16 unk8;       /* value 4 tested by EftVolley_PostUpdate: the instance starts ending */
    /* 0x0A */ u8 unkA[0x16];
    /* 0x20 */ void *list;     /* list the task is in; 0x1ADB98 returns *list, the task owning that list */
    /* 0x24 */ void *children; /* child list (BtlTask_CreateChildList) */
    /* 0x28 */ void *unk28[4];
    /* 0x38 */ void *data;     /* instance data, size given to BtlTask_CreateChildList by the parent */
    /* 0x3C */ s32 unk3C;
} EftHTask; /* size 0x40 */

/* What an effect needs to know about a technique: EftShot_BuildParam fills it from BtlSkillData (slots 0, 1)
   or BtlSuperData (slots 2..4). Offsets on the right are the source fields, [i] indexed by slot or slot - 2. */
typedef struct EftShotParam {
    /* 0x00 */ s16 id;        /* technique id                     skill +0x10[i]   super +0x18[i] */
    /* 0x02 */ s16 unk2;      /*                                  skill +0x14[i]   super +0x1E[i] */
    /* 0x04 */ s8 kind;       /* 0 skill, 1 technique, 2 ultimate */
    /* 0x05 */ s8 unk5;       /*                                  skill +0x62[i]   super +0x141[i] */
    /* 0x06 */ s8 unk6;       /*                                  skill +0x38[i]   super +0x90[i] */
    /* 0x07 */ s8 unk7;
    /* 0x08 */ s8 unk8;       /*                                  skill +0x3E[i]   super +0x99[i] */
    /* 0x09 */ s8 shots;      /* number of shots                  skill +0x3A[i]   super +0x93[i] */
    /* 0x0A */ s8 unkA;       /*                                  skill +0x40[i]   super +0x9C[i] */
    /* 0x0B */ s8 unkB;       /*                                  skill +0x42[i]   super +0x9F[i] */
    /* 0x0C */ s8 unkC;
    /* 0x0D */ s8 type;       /* effect type, -1 none             skill +0x60[i]   super +0x138[i] */
    /* 0x0E */ s8 unkE[6];    /*                                  skill +0x48..    super +0xA8.. (stride 3) */
    /* 0x14 */ s8 unk14;      /*                                  -1               super +0x162[i] */
    /* 0x15 */ s8 unk15;
    /* 0x16 */ s16 unk16[6];  /*                                  skill +0x54..    -1 */
    /* 0x22 */ s16 unk22;
    /* 0x24 */ s16 unk24;
    /* 0x26 */ s16 unk26;
    /* 0x28 */ s32 life;      /* frames: seconds * 30             skill +0x18[i]   super +0x24[i] */
    /* 0x2C */ f32 unk2C;     /*                                  0                super +0x30[i] */
    /* 0x30 */ f32 unk30;     /*                                  skill +0x30[i]   super +0x6C[i] */
    /* 0x34 */ f32 unk34;     /* shot speed, 10 km/h              skill +0x20[i]   super +0x54[i] */
    /* 0x38 */ f32 unk38;     /* shot turn rate, degrees / s      skill +0x28[i]   super +0x60[i] */
    /* 0x3C */ s32 flags;     /*                                  skill +0x00[i]   super +0x00[i] */
    /* 0x40 */ s16 unk40;     /*                                  skill +0x46[i]   super +0xA5[i] */
    /* 0x42 */ s16 unk42;     /*                                  skill +0x44[i]   super +0xA2[i] */
    /* 0x44 */ s16 volley;    /* shots per fire event             skill +0x3C[i]   super +0x96[i] */
    /* 0x46 */ s16 unk46;
    /* 0x48 */ s16 unk48;     /*                                  skill +0x6E[i]   super +0x15C[i] */
    /* 0x4A */ s16 unk4A;     /*                                  skill +0x70[i]   super +0x15F[i] */
    /* 0x4C */ f32 unk4C;     /*                                  0                super +0x214[i] */
    /* 0x50 */ f32 unk50;     /*                                  1.0              super +0x84[i] */
    /* 0x54 */ f32 unk54;     /*                                  1.0              super +0x78[i] */
    /* 0x58 */ s8 unk58[8];   /*                                  -1               super +0xBA.. (stride 3) */
    /* 0x60 */ f32 unk60[8];  /*                                  0                super +0xD4.. (stride 3) */
    /* 0x80 */ s16 unk80;     /*                                  0                super +0x134[i] */
    /* 0x82 */ s16 unk82;
    /* 0x84 */ f32 unk84;     /*                                  0.1              super +0x48[i] */
    /* 0x88 */ f32 unk88;     /*                                  1.0              super +0x3C[i] */
} EftShotParam; /* size 0x8C */

/* Request to start a technique's effect; the argument of EftShot_Start, kept at the start of the slot. */
typedef struct EftHStartArg {
    /* 0x00 */ s32 chr;   /* character index = fighter object id */
    /* 0x04 */ s32 slot;  /* technique slot 0..4 */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ f32 time;  /* seconds; replaces EftShotParam.life */
    /* 0x14 */ f32 unk14; /* replaces EftShotParam.unk34 */
    /* 0x18 */ f32 unk18; /* replaces EftShotParam.unk38 */
} EftHStartArg; /* size 0x1C */

/* One technique slot of one character. */
typedef struct EftHSlot {
    /* 0x00 */ EftHStartArg arg;      /* arg.chr is the fighter object id the effect modules use */
    /* 0x1C */ s32 *pack;           /* the technique's effect pack (fighter object +0x9C[slot]); NULL: no effect */
    /* 0x20 */ s32 chr;
    /* 0x24 */ EftShotParam *param;
    /* 0x28 */ EftHTask *task;       /* group task */
    /* 0x2C */ u8 unk2C[0x14];
    /* 0x40 */ s32 unk40;           /* zeroed by EftShot_Start */
    /* 0x44 */ u8 unk44[0xC];
} EftHSlot; /* size 0x50 */

/* Per-character part of the manager state. */
typedef struct EftHChar {
    /* 0x000 */ EftHSlot slot[6];
    /* 0x1E0 */ EftShotParam param[6];
    /* 0x528 */ void *list;   /* child list of the character task: the group tasks */
    /* 0x52C */ EftHTask *task; /* character task (class 0x2C36E8) */
    /* 0x530 */ s32 unk530;   /* slot last started (0x14ABB0) */
    /* 0x534 */ u8 unk534[0xC];
} EftHChar; /* size 0x540 */

/* gp 0x2FE9F8 */
typedef struct EftHMgr {
    /* 0x00 */ void *list; /* child list of the layer task: the character tasks */
    /* 0x04 */ EftHChar *chars;
} EftHMgr;

/* Row of gEftShotClass (0x2C3700), indexed by EftShotParam.type + 1. */
typedef struct EftShotClassRow {
    /* 0x00 */ EftHTaskClass *group;
    /* 0x04 */ EftHTaskClass *inst;
    /* 0x08 */ s32 unk8;
} EftShotClassRow; /* size 0xC */

/* Views of the character data EftShot_BuildParam reads (BtlSkillData / BtlSuperData in btl_tech_b.h). */
typedef struct EftSkillSrc {
    /* 0x00 */ s32 flags[2];
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ u16 id[2];
    /* 0x14 */ u16 unk14[2];
    /* 0x18 */ f32 time[2];
    /* 0x20 */ f32 unk20[2];
    /* 0x28 */ f32 unk28[2];
    /* 0x30 */ f32 unk30[2];
    /* 0x38 */ u8 unk38[2];
    /* 0x3A */ u8 unk3A[2];
    /* 0x3C */ u8 unk3C[2];
    /* 0x3E */ u8 unk3E[2];
    /* 0x40 */ u8 unk40[2];
    /* 0x42 */ u8 unk42[2];
    /* 0x44 */ u8 unk44[2];
    /* 0x46 */ u8 unk46[2];
    /* 0x48 */ u8 unk48[6][2];
    /* 0x54 */ u8 unk54[6][2];
    /* 0x60 */ u8 type[2];
    /* 0x62 */ u8 unk62[2];
    /* 0x64 */ u8 unk64[0xA];
    /* 0x6E */ u8 unk6E[2];
    /* 0x70 */ u8 unk70[2];
} EftSkillSrc;

typedef struct EftSuperSrc {
    /* 0x000 */ s32 flags[3];
    /* 0x00C */ s32 unkC[3];
    /* 0x018 */ u16 id[3];
    /* 0x01E */ u16 unk1E[3];
    /* 0x024 */ f32 time[3];
    /* 0x030 */ f32 unk30[3];
    /* 0x03C */ f32 unk3C[3];
    /* 0x048 */ f32 unk48[3];
    /* 0x054 */ f32 unk54[3];
    /* 0x060 */ f32 unk60[3];
    /* 0x06C */ f32 unk6C[3];
    /* 0x078 */ f32 unk78[3];
    /* 0x084 */ f32 unk84[3];
    /* 0x090 */ u8 unk90[3];
    /* 0x093 */ u8 unk93[3];
    /* 0x096 */ u8 unk96[3];
    /* 0x099 */ u8 unk99[3];
    /* 0x09C */ u8 unk9C[3];
    /* 0x09F */ u8 unk9F[3];
    /* 0x0A2 */ u8 unkA2[3];
    /* 0x0A5 */ u8 unkA5[3];
    /* 0x0A8 */ u8 unkA8[6][3];
    /* 0x0BA */ u8 unkBA[8][3];
    /* 0x0D2 */ u8 unkD2[2];
    /* 0x0D4 */ f32 unkD4[8][3];
    /* 0x134 */ u8 unk134[3];
    /* 0x137 */ u8 unk137;
    /* 0x138 */ u8 type[3];
    /* 0x13B */ u8 unk13B[3];
    /* 0x13E */ u8 unk13E[0x1E];
    /* 0x15C */ u8 unk15C[3];
    /* 0x15F */ u8 unk15F[3];
    /* 0x162 */ u8 unk162[3];
    /* 0x165 */ u8 unk165[0xAF];
    /* 0x214 */ f32 unk214[3];
} EftSuperSrc;

/* ---- emitter set (parsed effect pack) ----------------------------------------------------------------------- */

/* Number of group kinds a pack can hold (bit n of EftSetHead.mask = kind n present). */
#define EFT_SET_KIND_COUNT 19

/* Header of an effect pack (first entry of the technique's pack file, or a common entry). */
typedef struct EftSetHead {
    /* 0x00 */ u32 mask;    /* bit per group kind */
    /* 0x04 */ u8 nGroups;
    /* 0x05 */ u8 nParts;   /* total over all groups */
    /* 0x06 */ u8 unk6;     /* EftEmit_GetAimKind: 7 = straight up, 8 = lifted 45 degrees */
    /* 0x07 */ u8 unk7;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC[2];
    /* 0x14 */ f32 unk14;   /* seconds */
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ u8 unk20;    /* frames the shots effect keeps running after its end was asked */
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 unk23[2];
    /* 0x25 */ u8 unk25;
    /* 0x26 */ u8 unk26;
    /* 0x27 */ u8 phaseMask; /* bit per phase (EftSetDef.phase) the pack has parts for */
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
    /* 0x30 */ f32 unk30[2];
    /* 0x38 */ f32 unk38;   /* seconds */
    /* 0x3C */ f32 unk3C;
} EftSetHead; /* size 0x40 */

/* One group of parts of the same kind. */
typedef struct EftSetGroupDef {
    /* 0x00 */ u8 kind;
    /* 0x01 */ u8 nParts;
    /* 0x02 */ u8 nRes;     /* resources (pack entries) the group brings */
    /* 0x03 */ u8 unk3[0x1D];
} EftSetGroupDef; /* size 0x20 */

/* One part. */
typedef struct EftSetDef {
    /* 0x00 */ u8 flags;    /* 2, 0x20, 0x40, 0x80 */
    /* 0x01 */ u8 res;      /* resource index inside the group */
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 unk3;
    /* 0x04 */ u8 unk4;     /* kind 0: 0 or 1, which light */
    /* 0x05 */ u8 unk5;
    /* 0x06 */ u8 unk6;
    /* 0x07 */ u8 unk7;
    /* 0x08 */ u8 phase;    /* phase that starts the part: 0, 1, 3, 4, 5 by event; 2 at the end; 6 never stopped */
    /* 0x09 */ u8 endPhase; /* event that stops it: 1, 2, 4, 5, 6 */
    /* 0x0A */ u8 unkA;
    /* 0x0B */ u8 unkB[5];
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ u8 unk20[0x14];
    /* 0x34 */ u8 unk34;
    /* 0x35 */ u8 unk35;
    /* 0x36 */ u8 unk36;
    /* 0x37 */ u8 unk37[9];
} EftSetDef; /* size 0x40 */

typedef struct EftSetGroup {
    /* 0x00 */ EftSetGroupDef *entry;
    /* 0x04 */ EftSetDef *parts;
    /* 0x08 */ u8 firstPart;  /* index of the group's first part in EftSet.parts / EftSetState */
    /* 0x09 */ u8 firstPair;  /* index of its first part in EftSet.pair (kind 0 parts not counted) */
    /* 0x0A */ u8 firstRes;   /* index of its first resource in EftSet.res */
    /* 0x0B */ u8 cat;        /* EftEmit_TypeToResKind(kind) */
    /* 0x0C */ u8 catFirst;   /* index of its first resource in EftSet.array[cat] */
    /* 0x0D */ u8 unkD[3];
} EftSetGroup; /* size 0x10 */

typedef struct EftSetPair {
    /* 0x00 */ s32 *a;
    /* 0x04 */ s32 *b; /* absent for kinds 2 and 14 */
} EftSetPair;

/* Indices of the tables that follow the groups, counted from the start of the group table (see EftSet). */
#define EFT_SET_RES 76    /* in 4-byte units: 0x13C */
#define EFT_SET_PAIR 54   /* in 8-byte units: 0x1BC */
#define EFT_SET_COUNT 192 /* in 4-byte units: 0x30C */

/* A parsed emitter set: data of a group task. The code reaches the resource tables as elements of an array that
   starts at 0xC with a constant added to the index ((set + 0xC) + n * 8 + 0x1B0), which the union reproduces. */
typedef struct EftSet {
    /* 0x000 */ EftSetHead *head;
    /* 0x004 */ EftSetGroupDef *entries;
    /* 0x008 */ EftSetDef *parts;
    union {
        struct {
            /* 0x00C */ EftSetGroup group[EFT_SET_KIND_COUNT]; /* by type */
            /* 0x13C */ s32 *res[32];       /* resource entries, [EftSetGroup.firstRes + EftSetDef.res] */
            /* 0x1BC */ EftSetPair pair[40]; /* per emitter, [EftSetGroup.firstPair + index in the group] */
            /* 0x2FC */ u8 *array[4];       /* per resource kind: objects of 0x208, 0x108, 0x88, 0x48 bytes */
            /* 0x30C */ s32 count[4];
        };
        /* 0x00C */ s32 *resAt[EFT_SET_RES + 32];      /* resAt[EFT_SET_RES + n] is res[n] */
        /* 0x00C */ EftSetPair pairAt[EFT_SET_PAIR + 40]; /* pairAt[EFT_SET_PAIR + n] is pair[n] */
        /* 0x00C */ s32 countAt[EFT_SET_COUNT + 4];    /* countAt[EFT_SET_COUNT + n] is count[n] */
    };
    /* 0x31C */ void *owner;    /* EftHSlot */
    /* 0x320 */ s32 *extra;     /* EftVolleyGroup_Init: pack entry 1 when the technique has flag 0x20000 */
} EftSet; /* size 0x324 */

/* Per-instance state of a pack's parts. */
typedef struct EftSetState {
    /* 0x000 */ void *handle[2][40]; /* task of each part, by part index */
    /* 0x140 */ u8 flag[40];         /* 2 started, 4 stopped, 0x20, 0x40, 0x80 */
    /* 0x168 */ f32 unk168[40];      /* EftSetDef.unk1C */
    /* 0x208 */ u8 unk208[0xA0];
    /* 0x2A8 */ f32 unk2A8;
    /* 0x2AC */ f32 unk2AC;
    /* 0x2B0 */ f32 unk2B0;
    /* 0x2B4 */ f32 unk2B4;
    /* 0x2B8 */ f32 unk2B8;
    /* 0x2BC */ f32 unk2BC;
    /* 0x2C0 */ f32 unk2C0;
    /* 0x2C4 */ f32 unk2C4;
    /* 0x2C8 */ void *owner;
} EftSetState; /* size 0x2CC */

/* One half of EftSetState.handle as the spawners take it: indexing a member array (not a bare pointer) is what
   gives the original's address arithmetic. */
typedef struct EftSetHandles {
    /* 0x00 */ void *h[40];
} EftSetHandles;

/* Flags EftEmit_GetFlags returns for an emitter and the spawners act on (EFT_SPAWN_* in eft_i.h). */
#define EFT_CMD_START   0x001 /* create the module's object if the emitter has none */
#define EFT_CMD_STOP    0x002 /* let it finish */
#define EFT_CMD_FADE    0x004 /* with STOP: zero its second parameter first */
#define EFT_CMD_KILL    0x008 /* destroy it now and clear the handle */
#define EFT_CMD_MOVE    0x010 /* write the position */
#define EFT_CMD_WARP    0x020 /* with MOVE: the module's second "set position" entry */
#define EFT_CMD_SCALE   0x040 /* write the size */
#define EFT_CMD_DIR     0x080 /* write the direction */
#define EFT_CMD_RESTART 0x100 /* type 17: destroy and create again */
#define EFT_CMD_200     0x200 /* types 15 and 17, emitters with flag 2: write the second point without MOVE */

/* ---- arguments of the particle modules' create functions ---------------------------------------------------- */
/* Each is written as an initialiser list in the original: the compiler builds it in a temporary (clearing the
   zero vectors with memset) and copies it into the variable. Fields the initialiser never writes are padding. */

/* Four floats on a 16-byte boundary, as these arguments hold them. */
typedef struct Vec4Q {
    f32 x, y, z, w;
} __attribute__((aligned(16))) Vec4Q;

/* Type 0 (0x17D290, 0x17D710): a light. */
typedef struct EftEmitLightArg {
    /* 0x00 */ Vec4Q pos;
    /* 0x10 */ s32 color[4]; /* r, g, b, a */
    /* 0x20 */ f32 unk20;
    /* 0x24 */ f32 unk24;    /* scale * 100 (kind 0) or * 800 (kind 1) */
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;    /* EftSetDef.unk10 * scale */
    /* 0x30 */ f32 unk30;    /* EftSetDef.unk14 * scale */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;    /* EftSetDef.unk3 */
    /* 0x3C */ s32 chr;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 unk48;    /* EftSetDef.unk5 */
    /* 0x4C */ s32 unk4C;    /* EftSetDef.unk6 */
    /* 0x50 */ s32 unk50;
} EftEmitLightArg; /* size 0x60 */

/* Type 2 (0x168600). */
typedef struct EftEmitArg2 {
    /* 0x00 */ u8 chr;
    /* 0x01 */ u8 type;
    /* 0x02 */ u8 unk2;   /* EftSetDef.unk2 */
    /* 0x03 */ u8 unk3;
    /* 0x04 */ s32 *tex;  /* EftSetPair.a */
    /* 0x08 */ u8 *res;   /* resource object, kind 3 */
    /* 0x0C */ f32 rate;
} EftEmitArg2; /* size 0x10 */

/* Types 5, 16, 18 (0x186B50, 0x196A00, 0x17CCD0). */
typedef struct EftEmitArgA {
    /* 0x00 */ Vec4Q pos;
    /* 0x10 */ Vec4Q dir;
    /* 0x20 */ s32 chr;
    /* 0x24 */ s32 unk24; /* EftSetDef.unk2 */
    /* 0x28 */ f32 rate;
    /* 0x2C */ f32 size;
    /* 0x30 */ u8 *res;   /* resource object, kind 1 */
    /* 0x34 */ s32 *texA; /* EftSetPair.a */
    /* 0x38 */ s32 *texB; /* EftSetPair.b */
} EftEmitArgA; /* size 0x40 */

/* Type 17 (0x1A3640). */
typedef struct EftEmitArg17 {
    /* 0x00 */ Vec4Q pos;
    /* 0x10 */ Vec4Q dir;
    /* 0x20 */ s32 type;
    /* 0x24 */ s32 chr;
    /* 0x28 */ s32 unk28; /* EftSetDef.unk2 */
    /* 0x2C */ f32 rate;
    /* 0x30 */ f32 size;
    /* 0x34 */ u8 *res;   /* resource object, kind 1 */
    /* 0x38 */ s32 *texA;
    /* 0x3C */ s32 *texB;
} EftEmitArg17; /* size 0x40 */

/* Type 14 (0x19D730). */
typedef struct EftEmitArg14 {
    /* 0x00 */ s32 chr;
    /* 0x04 */ s32 unk4;  /* EftSetDef.unk4 */
    /* 0x08 */ f32 rate;
    /* 0x10 */ Vec4Q pos;
    /* 0x20 */ Vec4Q dir;
    /* 0x30 */ f32 size;
    /* 0x34 */ s32 *tex;  /* EftSetPair.a */
    /* 0x38 */ u8 *res;   /* resource object, kind 0 */
} EftEmitArg14; /* size 0x40 */

/* Four floats, 8-byte aligned: locals initialised from a constant are copied with 64-bit moves. */
typedef struct Vec4A {
    f32 x, y, z, w;
} __attribute__((aligned(8))) Vec4A;

/* ---- the "shots" effect type ------------------------------------------------------------------------------- */

/* Pose of the fighter's firing point (filled by 0x1512C8). Copied with 64-bit moves. */
typedef struct EftVolleyPose {
    /* 0x000 */ u8 unk0[0x20];
    /* 0x020 */ Vec4 target;
    /* 0x030 */ u8 unk30[0x20];
    /* 0x050 */ s32 node;      /* model node the shots leave from */
    /* 0x054 */ u8 unk54[0xC];
    /* 0x060 */ Vec4 nodePos;
    /* 0x070 */ u8 unk70[0x1E0];
} __attribute__((aligned(8))) EftVolleyPose; /* size 0x250 */

typedef struct EftVolleyShot {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 dir;
    /* 0x20 */ Vec4 unk20;
    /* 0x30 */ Vec4 unk30;
    /* 0x40 */ s32 flags;   /* 1 fired, 2, 4 */
    /* 0x44 */ f32 time;    /* frames since fired */
    /* 0x48 */ void *handle; /* shot task (0x16A400 class); NULL: free */
    /* 0x4C */ s32 unk4C;
} EftVolleyShot; /* size 0x50 */

/* Argument of 0x16A7D0 / 0x16A868. */
typedef struct EftVolleyShotArg {
    /* 0x00 */ EftHSlot *slot;
    /* 0x04 */ EftSet *pack;
    /* 0x08 */ EftVolleyPose *pose;
    /* 0x0C */ EftVolleyShot *shot;
    /* 0x10 */ Vec4 *dir;
    /* 0x14 */ s32 index;
    /* 0x18 */ s32 count;
    /* 0x1C */ s32 phase;
    /* 0x20 */ s32 sub;
    /* 0x24 */ f32 time;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
} EftVolleyShotArg; /* size 0x30 */

/* EftVolleyWork.flags */
#define EFT_VOLLEY_ENDING   0x0001 /* end asked: the end timer runs */
#define EFT_VOLLEY_DEAD     0x0002 /* kill the task once no shot is left */
#define EFT_VOLLEY_4        0x0004
#define EFT_VOLLEY_8        0x0008
#define EFT_VOLLEY_AIMED2   0x0010
#define EFT_VOLLEY_AIMED    0x0040
#define EFT_VOLLEY_RESET    0x0080
#define EFT_VOLLEY_FIRED    0x0100
#define EFT_VOLLEY_ID1C6    0x0200 /* technique 0x1C6: shots leave from nodes 0x1F and 0x2D */
#define EFT_VOLLEY_EXTRA    0x0400 /* technique flag 0x20000 */
#define EFT_VOLLEY_800      0x0800 /* techniques 0x26E, 0x26F */
#define EFT_VOLLEY_1000     0x1000 /* techniques 0x26E, 0x26F */

/* Data of an EftVolley instance task. */
typedef struct EftVolleyWork {
    /* 0x0000 */ s32 flags;
    /* 0x0004 */ f32 unk4;   /* EftShotParam.unk34 */
    /* 0x0008 */ f32 unk8;   /* EftShotParam.unk38 */
    /* 0x000C */ f32 unkC;   /* EftShotParam.unk30 */
    /* 0x0010 */ f32 unk10;  /* EftShotParam.unk30 */
    /* 0x0014 */ f32 unk14;  /* EftShotParam.unk30 */
    /* 0x0018 */ f32 unk18;
    /* 0x001C */ f32 endTime;  /* frames since the end was asked */
    /* 0x0020 */ f32 endLimit; /* EftSetHead.unk20 */
    /* 0x0024 */ u8 unk24[0xC];
    /* 0x0030 */ Vec4 aim;
    /* 0x0040 */ Vec4 dir;
    /* 0x0050 */ Vec4 unk50;
    /* 0x0060 */ Vec4 unk60;
    /* 0x0070 */ u8 unk70[0x20];
    /* 0x0090 */ EftHSlot *slot;
    /* 0x0094 */ EftSetState state;
    /* 0x0360 */ EftVolleyPose pose;
    /* 0x05B0 */ EftSet *pack;
    /* 0x05B4 */ u8 unk5B4[0xC];
    /* 0x05C0 */ EftVolleyShot shot[30];
    /* 0x0F20 */ EftVolleyPose shotPose[2];
    /* 0x13C0 */ s32 fired;   /* shots fired */
    /* 0x13C4 */ s32 volleys; /* fire events of phase 1 */
    /* 0x13C8 */ s32 unk13C8;
    /* 0x13CC */ s32 unk13CC;
} EftVolleyWork; /* size 0x13D0 */

/* Data of an EftShotNull instance task. */
typedef struct EftShotNullWork {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ EftHSlot *slot;
} EftShotNullWork; /* size 8 */

#endif
