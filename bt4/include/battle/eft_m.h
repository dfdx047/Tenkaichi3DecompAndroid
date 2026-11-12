#ifndef BATTLE_EFT_M_H
#define BATTLE_EFT_M_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Effect tasks, 0x15F728..0x1637A0 (src/battle/eft_m.c):
 *   0x15F728..0x1609C8  speed lines: the second half of the module whose spawners are in eft_l (0x15EF18, 0x15F3D8)
 *   0x1609C8..0x1637A0  the aura's particles: the first half of the aura module, which continues in the next file at 0x1637A0
 * Both are drawing only: nothing here writes a fighter, a battle object or a hit record.
 * All structs are this file's views.
 */

/* A vector passed by value. The original vector type is 16-byte aligned (argument copies use ld / sd);
   Vec4 in sys/math3d.h is not, so this file casts between the two. */
typedef struct EftVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
} __attribute__((aligned(16))) EftVec;

/* ---- speed lines ---------------------------------------------------------------------------------------- */

/* Trail segment: a short quad that follows its fighter for two frames. */
typedef struct EftSpdTrail {
    /* 0x00 */ Vec4 pos;      /* leading end */
    /* 0x10 */ Vec4 tail;     /* trailing end */
    /* 0x20 */ Vec4 dir;      /* axis the quad is built around */
    /* 0x30 */ s32 objId;     /* owner (battle object id) */
    /* 0x34 */ f32 width;     /* half width */
    /* 0x38 */ f32 alpha;     /* 64 */
    /* 0x3C */ f32 life;      /* frames left, starts at 2 */
    /* 0x40 */ s32 flags;     /* 0 = free slot, bit 0 = alive */
    /* 0x44 */ u8 tex;        /* index into EftSpdLineWork.tex */
    /* 0x45 */ u8 blend;      /* second argument of EftGfx_DrawPolyAvgZFront */
    /* 0x46 */ u8 unk46[2];
    /* 0x48 */ struct EftSpdTrail *prev;
    /* 0x4C */ struct EftSpdTrail *next;
} EftSpdTrail; /* size 0x50 */

/* Streak segment: a line that flies along its own direction and fades out. */
typedef struct EftSpdStreak {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 tail;
    /* 0x20 */ Vec4 dir;      /* unit direction of travel */
    /* 0x30 */ s32 objId;
    /* 0x34 */ f32 speed;     /* distance per frame */
    /* 0x38 */ f32 width;     /* full width */
    /* 0x3C */ f32 alpha;     /* starts at 160 */
    /* 0x40 */ f32 fade;      /* alpha lost per frame: 160 / (life * 30) */
    /* 0x44 */ s32 flags;     /* 0 = free slot, bit 0 = alive */
    /* 0x48 */ u8 tex;
    /* 0x49 */ u8 blend;
    /* 0x4A */ u8 unk4A[2];
    /* 0x4C */ struct EftSpdStreak *prev;
    /* 0x50 */ struct EftSpdStreak *next;
    /* 0x54 */ u8 unk54[0xC];
} EftSpdStreak; /* size 0x60 */

typedef struct EftSpdTex {
    /* 0x0 */ u64 tex;        /* passed to EftGfx_DrawPolyAvgZFront */
    /* 0x8 */ u64 unk8;
} EftSpdTex;

/* gEftSpdLine: 0x18D0 bytes from the effect pool. */
typedef struct EftSpdLineWork {
    /* 0x0000 */ EftSpdTrail trails[30];
    /* 0x0960 */ EftSpdStreak streaks[40];
    /* 0x1860 */ s32 trailCount;
    /* 0x1864 */ s32 streakCount;
    /* 0x1868 */ EftSpdTex tex[4];        /* filled by EftTexSet_Load4 from common entry 0x237 */
    /* 0x18A8 */ u8 unk18A8[8];
    /* 0x18B0 */ s32 texReady;            /* set by EftSpdLine_UpdateTexture; only the streak draw tests it */
    /* 0x18B4 */ EftSpdTrail *trailHead;
    /* 0x18B8 */ EftSpdTrail *trailTail;
    /* 0x18BC */ EftSpdStreak *streakHead;
    /* 0x18C0 */ EftSpdStreak *streakTail;
    /* 0x18C4 */ u8 unk18C4[0xC];
} EftSpdLineWork; /* size 0x18D0 */

extern EftSpdLineWork *gEftSpdLine;

s32 EftSpdLine_SpawnStreakRing(s32 objId, Vec4 *dir, s32 node, s32 count, f32 unused, f32 lifeMin, f32 lifeRange);
void EftSpdLine_Init(void);
void EftSpdLine_Term(void);
void EftSpdLine_Update(void);
void EftSpdLine_Stub(void);
void EftSpdLine_Draw(void);
EftSpdTrail *EftSpdLine_AllocTrail(void);
void EftSpdLine_AddTrail(s32 objId, EftVec pos, EftVec dir);
void EftSpdLine_UpdateTrails(void);
s32 EftSpdLine_StepTrail(EftSpdTrail *p);
void EftSpdLine_DrawTrails(void);
EftSpdStreak *EftSpdLine_AllocStreak(void);
void EftSpdLine_AddStreak(s32 objId, EftVec pos, EftVec tail, EftVec dir, f32 speed, f32 life, f32 width);
void EftSpdLine_UpdateStreaks(void);
s32 EftSpdLine_StepStreak(EftSpdStreak *p);
void EftSpdLine_DrawStreaks(void);
void EftSpdLine_BuildTrailQuad(Vec4 *out, EftSpdTrail *p);
void EftSpdLine_BuildStreakQuad(Vec4 *out, EftSpdStreak *p);
void EftSpdLine_DrawQuad(Vec4 *quad, Mtx44 *mtx, EftSpdTex *tex, f32 r, f32 g, f32 b, f32 a, u8 blend, s32 inView);
void EftSpdLine_UpdateTexture(s32 a, s32 b);

/* ---- aura particles ------------------------------------------------------------------------------------- */

#define EFT_AURA_PARTS 10   /* body parts that emit flames */
#define EFT_AURA_SPARKS 12  /* spark emitters */

/* One fighter's aura (allocated and filled by the aura task at 0x1637A0..; only the fields this file reads are named). */
typedef struct EftAura {
    /* 0x000 */ s32 flags;       /* 1 active, 2 faded out, 0x10 / 0x20 variant (set by EftAura_Start), 0x80 */
    /* 0x004 */ u32 altMask;     /* bit per body part, toggled at every flame spawn */
    /* 0x008 */ s32 fadeA;       /* 1 = unk50 falls to 0, else rises to 1 */
    /* 0x00C */ s32 fadeB;       /* the same for unk58 */
    /* 0x010 */ s32 type;        /* 9 and 10 select the long-lived variant */
    /* 0x014 */ s32 unk14;
    /* 0x018 */ s32 colorCount;  /* number of colour entries to pick from */
    /* 0x01C */ s32 unk1C;
    /* 0x020 */ s32 texCount;    /* flame flag 0x100: unk6 = rand() % texCount */
    /* 0x024 */ s32 partsStarted; /* body parts that have had their first flame, 0..10 */
    /* 0x028 */ s32 fade;        /* 0 steady, 1 fading in, 2 / 3 fading out (3 sets flag 2 at the end) */
    /* 0x02C */ s32 burst;       /* 0 none, 1 rising, 2 holding, 3 falling */
    /* 0x030 */ s32 unk30;
    /* 0x034 */ s32 flameCount;  /* live flames */
    /* 0x038 */ f32 alpha;       /* 0..1, driven by `fade` */
    /* 0x03C */ f32 burstSpeed;  /* cfg->burstSpeed * burstLevel, added to every flame's speed */
    /* 0x040 */ f32 burstTime;   /* frames spent in burst 2 */
    /* 0x044 */ f32 lifeScale;   /* 1, or 1.2 during a burst */
    /* 0x048 */ f32 level;       /* aura strength */
    /* 0x04C */ f32 burstLevel;  /* 0..1 */
    /* 0x050 */ f32 unk50;       /* 0..1, driven by fadeA; multiplies the spark alpha */
    /* 0x054 */ f32 unk54;       /* its rate */
    /* 0x058 */ f32 unk58;       /* 0..1, driven by fadeB */
    /* 0x05C */ f32 unk5C;       /* its rate */
    /* 0x060 */ f32 scale;       /* body scale */
    /* 0x064 */ u8 unk64[0xC];
    /* 0x070 */ Vec4 pos;        /* fighter root */
    /* 0x080 */ Vec4 prevPos;
    /* 0x090 */ Vec4 part[EFT_AURA_PARTS];      /* body part positions */
    /* 0x130 */ Vec4 partPrev[EFT_AURA_PARTS];  /* the frame before */
    /* 0x1D0 */ Vec4 partEnd[EFT_AURA_PARTS];   /* second node of a part (cfg->flame[i].node) */
    /* 0x270 */ u8 unk270[0xA0];
    /* 0x310 */ Vec4 unk310;     /* centre the flames are pushed away from (x, z) */
    /* 0x320 */ Vec4 axis;       /* aura axis (x, z) */
    /* 0x330 */ Vec4 sparkPos[EFT_AURA_SPARKS];
    /* 0x3F0 */ Vec4 colorStart; /* flame colour, w = alpha */
    /* 0x400 */ Vec4 colorEnd;
    /* 0x410 */ Vec4 colorAlt[4];   /* flame flag 0x800 */
    /* 0x450 */ Vec4 sparkColor[4];
    /* 0x490 */ Vec4 sparkColorAlt[4]; /* aura flag 0x20 */
} EftAura;

/* Flame particle. */
typedef struct EftAuraFlame {
    /* 0x00 */ s32 flags;   /* low bits: 1 fade-in done, 2 fading out, 4 successor spawned, 8 / 0x10 size phases 2 / 3
                               started; from EftAura_GetFlameFlags: 0x20 / 0x40 burst 1 / 2, 0x80, 0x100, 0x200 long life,
                               0x400 / 0x800 alternate kinds, 0x1000 invisible */
    /* 0x04 */ u8 unk4;
    /* 0x05 */ u8 kind;     /* 0, 1 or 2: row of the parameter tables */
    /* 0x06 */ u8 tex;
    /* 0x07 */ u8 objId;
    /* 0x08 */ u8 unk8;     /* cfg->flame[part].unk0 */
    /* 0x09 */ u8 part;
    /* 0x0A */ u8 flip;     /* rand() % 2 */
    /* 0x0B */ u8 alt;      /* 1 with flag 0x800 */
    /* 0x0C */ f32 endTime; /* time left at which the last size phase starts */
    /* 0x10 */ f32 time;    /* frames left */
    /* 0x14 */ f32 life;
    /* 0x18 */ f32 scale;
    /* 0x1C */ f32 size;
    /* 0x20 */ f32 stretch;
    /* 0x24 */ f32 sizeRate;
    /* 0x28 */ f32 stretchRate;
    /* 0x2C */ f32 end;
    /* 0x30 */ f32 speed;
    /* 0x34 */ f32 follow;  /* 0..1 along the part */
    /* 0x38 */ f32 power;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ Vec4 offset;
    /* 0x50 */ Vec4 pos;
    /* 0x60 */ Vec4 color;
    /* 0x70 */ Vec4 colorRate;
    /* 0x80 */ Vec4 dir;
    /* 0x90 */ Vec4 dirStart;
    /* 0xA0 */ Vec4 dirTurn;
    /* 0xB0 */ Vec4 move;   /* accumulated displacement */
    /* 0xC0 */ struct EftAuraFlame *next;
    /* 0xC4 */ u8 unkC4[0xC];
} EftAuraFlame; /* size 0xD0 */

/* Spark particle. */
typedef struct EftAuraSpark {
    /* 0x00 */ s32 flags;   /* 1 fading out, 2, 4, 8 (aura flag 0x80), 0x10 (aura type 9 / 10) */
    /* 0x04 */ u8 objId;
    /* 0x05 */ u8 emitter;
    /* 0x06 */ u8 kind;
    /* 0x07 */ u8 unk7;     /* 1 */
    /* 0x08 */ f32 age;
    /* 0x0C */ f32 life;
    /* 0x10 */ f32 rot;     /* -pi..pi */
    /* 0x14 */ f32 rotSpeed;
    /* 0x18 */ f32 size;
    /* 0x1C */ f32 speed;
    /* 0x20 */ f32 follow;
    /* 0x24 */ u8 unk24[0xC];
    /* 0x30 */ Vec4 pos;
    /* 0x40 */ Vec4 offset;
    /* 0x50 */ Vec4 move;
    /* 0x60 */ Vec4 dir;
    /* 0x70 */ Vec4 color;
    /* 0x80 */ Vec4 colorRate;
    /* 0x90 */ Vec4 uv;
    /* 0xA0 */ struct EftAuraSpark *next;
    /* 0xA4 */ u8 unkA4[0xC];
} EftAuraSpark; /* size 0xB0 */

/* gEftAuraPool */
typedef struct EftAuraPool {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 flameMax;
    /* 0x008 */ s32 sparkMax;
    /* 0x00C */ EftAuraFlame *flames;
    /* 0x010 */ EftAuraFlame *flameFree;
    /* 0x014 */ EftAuraFlame *flameUsed;
    /* 0x018 */ EftAuraSpark *sparks;
    /* 0x01C */ EftAuraSpark *sparkFree;
    /* 0x020 */ EftAuraSpark *sparkUsed;
    /* 0x024 */ s32 flameNext;   /* slots handed out so far */
    /* 0x028 */ s32 sparkNext;
    /* 0x02C */ u8 unk2C[0x25C];
    /* 0x288 */ s32 sparkTex;    /* passed by address to EftSpr_DrawRot */
} EftAuraPool;

typedef struct EftAuraCfgFlame {
    /* 0x0 */ u8 unk0;
    /* 0x4 */ s32 node;  /* second model node of the part, or negative */
    /* 0x8 */ s32 unk8;
} EftAuraCfgFlame;

typedef struct EftAuraCfgSpark {
    /* 0x0 */ s32 kind;
    /* 0x4 */ s32 node;  /* model node the spark is pulled towards, or negative */
} EftAuraCfgSpark;

/* gEftAuraCfg */
typedef struct EftAuraCfg {
    /* 0x000 */ u8 unk0[0x194];
    /* 0x194 */ f32 fadeInTime;
    /* 0x198 */ f32 fadeOutTime;
    /* 0x19C */ f32 burstHold;
    /* 0x1A0 */ f32 unk1A0;
    /* 0x1A4 */ f32 burstSpeed;
    /* 0x1A8 */ f32 unk1A8;
    /* 0x1AC */ f32 unk1AC;
    /* 0x1B0 */ f32 unk1B0;
    /* 0x1B4 */ f32 unk1B4;
    /* 0x1B8 */ f32 sparkFadeIn;
    /* 0x1BC */ f32 sparkFadeOut;
    /* 0x1C0 */ f32 sparkLife;
    /* 0x1C4 */ f32 sparkLifeRange;
    /* 0x1C8 */ f32 sparkLifeLong;
    /* 0x1CC */ f32 sparkLifeLongRange;
    /* 0x1D0 */ f32 sparkStep;
    /* 0x1D4 */ f32 sparkRotSpeed;
    /* 0x1D8 */ f32 sparkAccel;
    /* 0x1DC */ f32 sparkDamp;
    /* 0x1E0 */ f32 sparkSize[4][2];  /* {size, speed}: normal, flag 8, alt, alt + flag 8 */
    /* 0x200 */ f32 sparkMul[2][2];   /* {rotSpeed, speed} factors for flags 4 and 2 */
    /* 0x210 */ u8 unk210[0x200];
    /* 0x410 */ EftAuraCfgFlame flame[EFT_AURA_SPARKS];
    /* 0x4A0 */ s32 unk4A0;
    /* 0x4A4 */ EftAuraCfgSpark spark[EFT_AURA_SPARKS];
} EftAuraCfg;

typedef struct EftAuraPrmKind {
    /* 0x00 */ f32 side[2];   /* base, range */
    /* 0x08 */ f32 away[2];
    /* 0x10 */ f32 out[2];
    /* 0x18 */ f32 turn;
    /* 0x1C */ f32 accel;
} EftAuraPrmKind;

/* gEftAuraPrm */
typedef struct EftAuraPrm {
    /* 0x000 */ f32 endRatio;
    /* 0x004 */ f32 growRatio;
    /* 0x008 */ f32 startDist;
    /* 0x00C */ f32 sparkRise;
    /* 0x010 */ f32 flameStep;
    /* 0x014 */ f32 lean;
    /* 0x018 */ f32 unk18;
    /* 0x01C */ f32 life[3];       /* base, range, long */
    /* 0x028 */ f32 unk28;
    /* 0x02C */ f32 fadeIn;
    /* 0x030 */ f32 fadeOut;
    /* 0x034 */ f32 speed[6];
    /* 0x04C */ f32 speedRand[3][2];
    /* 0x064 */ f32 stretch[4];    /* by phase */
    /* 0x074 */ f32 size[4];
    /* 0x084 */ f32 stretchMul[3];
    /* 0x090 */ f32 sizeMul[3];
    /* 0x09C */ f32 stretchKind[4];
    /* 0x0AC */ f32 sizeKind[3];
    /* 0x0B8 */ f32 unkB8;
    /* 0x0BC */ f32 stretchRand[3][3];
    /* 0x0E0 */ f32 end[3][3];
    /* 0x104 */ EftAuraPrmKind kind[3];
    /* 0x164 */ f32 alpha[2];
    /* 0x16C */ f32 alphaMul[4];
    /* 0x17C */ f32 sparkVel;
    /* 0x180 */ f32 sparkVelAlt;
    /* 0x184 */ f32 sideScale;
    /* 0x188 */ f32 drag;
    /* 0x18C */ f32 speedRef;
    /* 0x190 */ f32 outRise;
} EftAuraPrm;

extern EftAuraPool *gEftAuraPool;
extern EftAuraCfg *gEftAuraCfg;
extern EftAuraPrm *gEftAuraPrm;

void EftAura_GetSparkDir(Vec4 *out, s32 objId);
void EftAura_GetSparkDirAlt(Vec4 *out, s32 objId);
void EftAura_GetFlameSideDir(EftAura *aura, Vec4 *out, s32 objId, f32 power, f32 away, f32 side);
void EftAura_GetFlameDragDir(EftAura *aura, Vec4 *pos, Vec4 *out, s32 objId, s32 part);
void EftAura_GetFlameOutDir(EftAura *aura, Vec4 *out, s32 objId, s32 part, f32 power, f32 amount);
void EftAura_TurnFlame(EftAuraFlame *f, Vec4 *pos, f32 t);
void EftAura_AccelFlame(EftAuraFlame *f, f32 t);
void EftAura_GetPartOffset(EftAura *aura, Vec4 *out, s32 objId, s32 part, f32 radial, f32 vertical);
EftAuraSpark *EftAura_AllocSpark(void);
void EftAura_FreeSparks(s32 objId);
s32 EftAura_SpawnSpark(EftAura *aura, s32 objId, s32 emitter, s32 kind, Vec4 *offset, f32 follow);
void EftAura_SpawnSparks(EftAura *aura, s32 objId, s32 emitter, s32 once);
void EftAura_StepSparks(EftAura *aura, s32 objId);
void EftAura_UpdateSparks(EftAura *aura, s32 objId, s32 *state);
void EftAura_DrawSparks(EftAura *aura, s32 objId, f32 alpha);
EftAuraFlame *EftAura_AllocFlame(void);
s32 EftAura_GetFlameFlags(EftAura *aura, s32 objId, s32 second, s32 alt);
f32 EftAura_GetFlameLife(s32 flags, f32 burst);
f32 EftAura_GetFlameSpeed(s32 flags, s32 objId, f32 level, f32 burst);
f32 EftAura_GetFlameAlpha(EftAura *aura, s32 flags, f32 alpha);
f32 EftAura_GetFlameStretch(EftAuraFlame *f, s32 objId, s32 phase);
f32 EftAura_GetFlameSize(EftAuraFlame *f, s32 objId, s32 phase);
f32 EftAura_GetFlameEnd(EftAuraFlame *f, s32 objId);
s32 EftAura_SpawnFlame(EftAura *aura, s32 objId, s32 part, Vec4 *pos, Vec4 *offset, f32 follow, s32 alt, s32 second);
s32 EftAura_SpawnFlames(EftAura *aura, s32 objId, s32 part, s32 once);
void EftAura_Start(EftAura *aura, s32 *arg, s32 reset);
void EftAura_FreeFlames(s32 objId);
void EftAura_StepFlames(EftAura *aura, s32 objId);
void EftAura_UpdateState(EftAura *aura, s32 objId);
void EftAura_BuildFlameMtx(Mtx44 *out, Vec4 *dir, Vec4 *pos);

#endif
