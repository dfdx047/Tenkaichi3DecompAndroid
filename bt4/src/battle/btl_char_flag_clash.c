extern int TagBattle_Team(int),TagBattle_TeamIndex(int),TagBattle_TeamCount(int);
extern int TagBattle_Active(void);
#include "common.h"
#include "battle/btl_char_flag.h"
#include <stdio.h>

/*
 * Clash sequences between the two fighters: 0x1D8750..0x1D9A20.
 *
 * BtlClash_Update runs once per frame after the per-fighter passes. While roster +0x40 is 0 it looks
 * for both fighters being in a clash action at once and starts the matching sequence:
 *   A (states 1..5)   both in actions 0x130..0x132
 *   B (states 6..8)   both in action 0xFA
 *   C (states 9..11)  both in action 0xFB (then 0xFC)
 * The sequences drive the fighter camera, voices and the flags that tell the action code who won.
 */

extern f32 atan2f(f32 y, f32 x);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 Mathf_Asin(f32 x);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t); /* a + (b - a) * t */
extern f32 BtlUtil_WrapAngle(f32 a);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern BtlFlagChr *BtlChar_Get(s32 i);
extern BtlFlagPose *BtlChar_GetPos(BtlFlagChr *chr);
extern s32 BtlChar_FrameMod(s32 n);
extern u32 BtlChar_Rand(void);
extern void BtlChar_PlayVoice(BtlFlagChr *chr, s32 kind);
extern s32 Battle_GetMode(void);
extern void ChrCam_SetCut(BtlFlagChr *chr, Vec4 *vecA, Vec4 *vecADelta, Vec4 *vecB, Vec4 *vecBDelta, Vec4 *vecC,
                          Vec4 *vecCDelta, s32 unk88, f32 valA, f32 valADelta, f32 valB, f32 valBDelta, f32 valC,
                          f32 valCDelta, s32 unk8C, s32 unk90, s32 unk94, s32 time, s32 flags);
extern void ChrCam_RequestCut(BtlFlagChr *chr, s32 arg1, s32 arg2);
extern void ChrCam_EndCut(BtlFlagChr *chr);
extern s32 BtlCharApi_IsInClashA(s32 objId);                                  /* action id in 0x130..0x132 */
extern s32 BtlAct_GetCurrent(BtlFlagChr *chr);                            /* action id */
extern s32 BtlAct_GetCurrentClass(BtlFlagChr *chr);                            /* class of the technique in use (0..4), or -1 */
extern void BtlAct_CountAndMarkOpponent(BtlFlagChr *chr);                           /* chr->clashCountB++, opponent +0xD4C = 1 */
extern f32 BtlCharApi_GetHeight(s32 objId);                                  /* height */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);            /* world position of a model node */
extern f32 EftStruggle_GetCamDist(s32 side, s32 type);
extern f32 EftStruggle_GetMidDist(void);
extern void EftStruggle_End(s32 objId);
extern s32 BtlStage_GetPathCount(void);                                       /* number of stage paths */
extern BtlClashPath *BtlStage_GetPath(s32 n);                            /* stage path n */
extern s32 BtlSuper_IsThrow(BtlFlagChr *chr, s32 cls);
extern s32 BtlSuper_GetFlags(BtlFlagChr *chr, s32 cls);                   /* attribute word of the technique */
extern s32 BtlSuper_GetDamage(BtlFlagChr *chr, s32 cls, s32 arg2, s32 arg3); /* damage of the technique */
extern void BtlColl_StartThrow(BtlFlagChr *chr, BtlFlagChr *target, s32 cls, s32 attr);
extern void BtlMember_Damage(BtlFlagChr *chr, s32 damage, s32 flags);    /* applies damage */

extern BtlFlagRoster *gBtlChars;
extern s32 TagBattle_Count(void),TagBattle_Target(s32 actor);
static s32 sTagBlowTicks[10][10];
void TagClash_Reset(void) {
    s32 i,j;
    for(i=0;i<10;++i)for(j=0;j<10;++j)sTagBlowTicks[i][j]=0;
}
/* Punch clashes keep their own timer and result for each participating pair.
   The original manager only observes actors 0 and 1, so later roster seats
   otherwise remain in action FA indefinitely. */
static void TagClash_UpdateBlows(void) {
    s32 i,j,count=TagBattle_Count();
    for(i=0;i<count;++i)for(j=i+1;j<count;++j){
        BtlFlagChr *a=BtlChar_Get(i),*b=BtlChar_Get(j);
        s32 *timer=&sTagBlowTicks[i][j];
        if(TagBattle_Allied(i,j) || BtlAct_GetCurrent(a)!=0xFA || BtlAct_GetCurrent(b)!=0xFA ||
           TagBattle_Target(i)!=j || TagBattle_Target(j)!=i){*timer=0;continue;}
        if(*timer==0)ChrCam_RequestCut(a,1,6);
        if(*timer==1)BtlChar_PlayVoice(a,0x18);
        if(*timer==16)BtlChar_PlayVoice(b,0x18);
        if(++*timer==78){
            BtlFlagChr *winner=a,*loser=b;
            if(a->clashCountB<b->clashCountB ||
               (a->clashCountB==b->clashCountB && BtlChar_FrameMod(2)==0)){winner=b;loser=a;}
            BtlChar_SetFlag(winner,0xBF);BtlChar_SetFlag(loser,0xC0);
            BtlChar_PlayVoice(winner,0x19);BtlChar_RaiseFirstClash(winner);
            ChrCam_EndCut(a);ChrCam_EndCut(b);
        }
    }
    for(i=0;i<count;++i){
        BtlFlagChr *chr=BtlChar_Get(i);
        j=TagBattle_Target(i);
        if(BtlAct_GetCurrent(chr)==0xFA &&
           (j<0 || j>=count || BtlAct_GetCurrent(BtlChar_Get(j))!=0xFA || TagBattle_Target(j)!=i))
            BtlChar_SetFlag(chr,0xBF);
    }
}
extern Vec4 D_002EC2A0; /* zero vector */

typedef struct BtlClashProgress {
    /* 0x00 */ u8 unk0[0x3C];
    /* 0x3C */ s32 unk3C; /* difficulty */
} BtlClashProgress;
extern BtlClashProgress *gProgress;

/* 1 while both fighters are in actions 0x130..0x132. */
s32 BtlClash_BothInClashA(void) {
    if (BtlCharApi_IsInClashA(0) && BtlCharApi_IsInClashA(1)) {
        return 1;
    }
    return 0;
}

/* Raises held flag 0xC3 on a fighter that is in actions 0x130..0x132. */
void BtlClash_HoldClashA(s32 player) {
    if (BtlCharApi_IsInClashA(player)) {
        BtlChar_SetHeldFlag(BtlChar_Get(player), 0xC3);
    }
}

/* 1 while both fighters are in action 0xFA. */
s32 BtlClash_BothInActionFA(void) {
    s32 action = BtlAct_GetCurrent(BtlChar_Get(0));

    if (action != 0xFA) {
        return 0;
    }
    return BtlAct_GetCurrent(BtlChar_Get(1)) == action;
}

/* Raises flag 0xBF on a fighter still in action 0xFA (ends it). */
void BtlClash_EndActionFA(s32 player) {
    BtlFlagChr *chr = BtlChar_Get(player);

    if (BtlAct_GetCurrent(chr) == 0xFA) {
        BtlChar_SetFlag(chr, 0xBF);
    }
}

/* 1 while both fighters are in action 0xFB. */
s32 BtlClash_BothInActionFB(void) {
    s32 action = BtlAct_GetCurrent(BtlChar_Get(0));

    if (action != 0xFB) {
        return 0;
    }
    return BtlAct_GetCurrent(BtlChar_Get(1)) == action;
}

/* 1 while both fighters are in action 0xFC. */
s32 BtlClash_BothInActionFC(void) {
    s32 action = BtlAct_GetCurrent(BtlChar_Get(0));

    if (action != 0xFC) {
        return 0;
    }
    return BtlAct_GetCurrent(BtlChar_Get(1)) == action;
}

/* Raises flag 0xC6 on a fighter still in action 0xFC (ends it). */
void BtlClash_EndActionFC(s32 player) {
    BtlFlagChr *chr = BtlChar_Get(player);

    if (BtlAct_GetCurrent(chr) == 0xFC) {
        BtlChar_SetFlag(chr, 0xC6);
    }
}

/* Clash A camera: a one-frame cut orbiting one fighter (type 0 wide, 1 from the side, 2 close on node 0x30). */
void BtlClash_SetOrbitCut(s32 player, s32 type) {
    Vec4 d;
    BtlFlagChr *chr;
    s32 nodeA = 3;
    s32 nodeB = 0x11;
    f32 pitch;
    f32 yaw;
    f32 dist;

    chr = BtlChar_Get(player);
    pitch = 0.0f;
    BtlOpp_GetDelta(chr, &d);
    yaw = 0.0f;
    dist = 0.0f;
    Vec3_Normalize(&d, &d);
    switch (type) {
    case 0:
        if (player == 0) {
            dist = 2.4f;
        } else {
            dist = -2.4f;
        }
        pitch = 0.0f;
        yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw - dist);
        dist = BtlCharApi_GetHeight(chr->objId);
        dist += EftStruggle_GetCamDist(player, type);
        break;
    case 1:
        if (player == 0) {
            dist = 0.5f;
        } else {
            dist = -0.5f;
        }
        pitch = 0.0f;
        yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw - dist);
        dist = BtlCharApi_GetHeight(chr->objId);
        dist += EftStruggle_GetCamDist(player, type);
        break;
    case 2:
        dist = 3.14159265f;
        nodeA = 0x30;
        nodeB = 0x30;
        yaw = BtlUtil_WrapAngle(BtlChar_GetPos(chr)->yaw - dist);
        pitch = Mathf_Asin(d.y) - 0.2f;
        dist = BtlCharApi_GetHeight(chr->objId) * 0.4f;
        dist += EftStruggle_GetCamDist(player, type);
        break;
    }
    ChrCam_SetCut(chr, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, &D_002EC2A0, nodeA, yaw, 0.0f,
                  pitch, 0.0f, dist, 0.0f, nodeA, nodeB, nodeB, 1, 0x45);
}

/* Clash A camera: a one-frame cut around the point between the fighters, swinging with the balance. */
void BtlClash_SetMidCut(Vec4 *mid, f32 bias) {
    Vec4 d;
    Vec4 look;
    Vec4 a;
    Vec4 b;
    BtlFlagChr *chr = BtlChar_Get(0);
    f32 ang = 1.5707963f;
    f32 half;
    f32 base;
    f32 yaw;
    f32 pitch;

    BtlOpp_GetDelta(chr, &d);
    half = 0.5f;
    ang = bias * ang;
    base = 50.0f;
    Vec3_Normalize(&d, &d);
    BtlCharApi_GetNodePos(0, 0x11, &a);
    ang = ang * half;
    BtlCharApi_GetNodePos(1, 0x11, &b);
    Vec4_Lerp(&look, &b, &a, bias * 0.6f + half);
    ang = ang + -1.5707963f;
    yaw = BtlUtil_WrapAngle(atan2f(d.x, d.z) + ang);
    pitch = -Mathf_Asin(d.y);
    pitch *= bias;
    ChrCam_SetCut(chr, mid, &D_002EC2A0, mid, &D_002EC2A0, &look, &D_002EC2A0, -1, yaw, 0.0f, pitch, 0.0f,
                  EftStruggle_GetMidDist() + base, 0.0f, -1, -1, -1, 1, 0x45);
}

/* Clash C camera: a one-frame cut placed at the first point of a stage path, looking along its angles. */
void BtlClash_SetPathCut(s32 n) {
    Vec4 pos;
    Vec4 look;
    BtlClashPath *path = BtlStage_GetPath(n);

    if (path != NULL) {
        f32 *p = path->points;

        pos.x = p[0];
        pos.y = p[1];
        pos.z = p[2];
        pos.w = 1.0f;
        look.x = pos.x + sinf(p[5]);
        look.y = pos.y - sinf(p[4]);
        look.z = pos.z + cosf(p[5]);
        look.w = 1.0f;
        ChrCam_SetCut(BtlChar_Get(0), &pos, &D_002EC2A0, &pos, &D_002EC2A0, &look, &D_002EC2A0, -1, 0.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, -1, -1, -1, 1, 0x45);
    }
}

/* Squashes the clash balance into -0.8..0.8: x / (|x| + 1) with x = lead / 20. */
f32 BtlClash_CalcBias(s32 lead) {
    f32 x = lead * 0.05f;

    return BtlUtil_ClampF(x / (__builtin_fabsf(x) + 1.0f), -0.8f, 0.8f);
}

/* Clash A, one frame. Returns the next state (0 when it is over). */
s32 BtlClash_UpdateA(s32 state) {
    Vec4 a;
    Vec4 b;
    BtlFlagRoster *roster = gBtlChars;
    BtlFlagChr *chr0 = BtlChar_Get(0);
    BtlFlagChr *chr1 = BtlChar_Get(1);
    s32 count0 = chr0->clashCountA;
    BtlClashA *c = &roster->clashA;
    s32 count1 = chr1->clashCountA;

    switch (state) {
    case 1:
        c->timer = 0;
        c->lead = 0;
        state++;
        BtlCharSnd_PlayStream(chr0, 0x8D30);
        break;
    case 2:
        if (c->timer == 0) {
            BtlChar_PlayVoice(chr0, 0x14);
        }
        if (c->timer == 0x1E) {
            BtlChar_PlayVoice(chr1, 0x14);
        }
        if (c->timer < 0x1E) {
            BtlClash_SetOrbitCut(0, 0);
        } else if (c->timer < 0x3C) {
            BtlClash_SetOrbitCut(1, 0);
        } else {
            if (count1 < count0) {
                c->lead++;
            }
            if (count0 < count1) {
                c->lead--;
            }
            if (c->bias < -0.64f) {
                BtlClash_SetOrbitCut(0, 1);
            } else if (0.64f < c->bias) {
                BtlClash_SetOrbitCut(1, 1);
            } else {
                BtlClash_SetMidCut(&c->mid, c->bias);
            }
        }
        c->timer++;
        if (c->timer >= 0xA4) {
            c->timer = 0;
            state++;
        }
        break;
    case 3:
        if (c->timer == 0) {
            if (c->lead == 0) {
                c->lead = BtlChar_FrameMod(2) ? 1 : -1;
            }
            if (c->lead > 0) {
                BtlChar_PlayVoice(chr0, 0x15);
            } else {
                BtlChar_PlayVoice(chr1, 0x15);
            }
        }
        if (c->lead > 0) {
            BtlClash_SetOrbitCut(0, 2);
        } else {
            BtlClash_SetOrbitCut(1, 2);
        }
        c->timer++;
        if (c->timer >= 0x10) {
            c->timer = 0;
            state++;
        }
        break;
    case 4:
        if (c->lead > 0) {
            c->lead += 4;
        } else {
            c->lead -= 4;
        }
        BtlClash_SetMidCut(&c->mid, c->bias);
        if (__builtin_abs(c->lead) >= 0x47) {
            c->timer = 0;
            state++;
        }
        break;
    case 5:
        if (c->timer == 0) {
            BtlFlagChr *winner;
            BtlFlagChr *loser;
            s32 flags = 0;
            s32 cls;
            s32 loserCls;
            s32 damage;

            winner = chr0;
            loser = chr1;
            if (!(c->lead > 0)) {
                if (c->lead < 0) {
                    winner = chr1;
                    loser = chr0;
                } else if (BtlChar_FrameMod(2) == 0) {
                    winner = chr1;
                    loser = chr0;
                }
            }
            cls = BtlAct_GetCurrentClass(winner);
            loserCls = BtlAct_GetCurrentClass(loser);
            BtlChar_SetHeldFlag(winner, 0xC1);
            BtlChar_SetHeldFlag(winner, 0xC3);
            BtlChar_SetHeldFlag(loser, 0xC2);
            if (BtlSuper_IsThrow(winner, cls)) {
                BtlColl_StartThrow(winner, loser, cls, BtlSuper_GetFlags(winner, cls));
                BtlChar_SetHeldFlag(winner, 0xA0);
                EftStruggle_End(winner->objId);
            } else {
                damage = BtlSuper_GetDamage(winner, cls, 0, 1);
                damage += BtlSuper_GetDamage(loser, loserCls, 0, 1) / 2;
                switch (cls) {
                case 2:
                    flags = 0x800000;
                    break;
                case 3:
                    flags = 0x1000000;
                    break;
                case 4:
                    flags = 0x2000000;
                    break;
                }
                BtlMember_Damage(loser, damage, flags);
                BtlChar_PlayVoice(loser, 0x16);
                BtlCharSnd_PlayStream(loser, 0x8D31);
                EftStruggle_End(-1);
            }
            BtlChar_RaiseFirstClash(winner);
            c->winner = winner->player;
        }
        if (c->winner == 0) {
            BtlClash_SetOrbitCut(1, 0);
        } else {
            BtlClash_SetOrbitCut(0, 0);
        }
        c->lead = 0;
        state = 0;
        break;
    }
    c->bias = BtlClash_CalcBias(c->lead);
    BtlCharApi_GetNodePos(0, 0x11, &a);
    BtlCharApi_GetNodePos(1, 0x11, &b);
    Vec4_Lerp(&c->mid, &b, &a, c->bias * 0.5f + 0.5f);
    return state;
}

/* Clash B, one frame. Returns the next state (0 when it is over). */
s32 BtlClash_UpdateB(s32 state) {
    BtlClashB *c = &gBtlChars->clashB;
    BtlFlagChr *chr0 = BtlChar_Get(0);
    BtlFlagChr *chr1 = BtlChar_Get(1);

    switch (state) {
    case 6:
        c->timer = 0;
        state = 7;
        ChrCam_RequestCut(chr0, 1, 6);
        break;
    case 7:
        if (c->timer == 0) {
            BtlChar_PlayVoice(chr0, 0x18);
        }
        if (c->timer == 0xF) {
            BtlChar_PlayVoice(chr1, 0x18);
        }
        c->timer++;
        if (c->timer >= 0x4C) {
            c->timer = 0;
            state = 8;
        }
        if (!BtlClash_BothInActionFA()) {
            state = 0;
            BtlClash_EndActionFA(0);
            BtlClash_EndActionFA(1);
        }
        break;
    case 8:
        if (c->timer == 0) {
            s32 win;
            s32 lose;
            BtlFlagChr *winner;
            BtlFlagChr *loser;

            if (chr1->clashCountB < chr0->clashCountB) {
                win = 0;
            } else if (chr0->clashCountB < chr1->clashCountB) {
                win = 1;
            } else {
                win = BtlChar_FrameMod(2) == 0;
            }
            lose = !win;
            winner = BtlChar_Get(win);
            loser = BtlChar_Get(lose);
            BtlChar_SetFlag(winner, 0xBF);
            BtlChar_SetFlag(loser, 0xC0);
            BtlChar_PlayVoice(winner, 0x19);
            BtlChar_RaiseFirstClash(winner);
            ChrCam_EndCut(chr0);
            c->winner = win;
        }
        c->timer++;
        if (!BtlClash_BothInActionFA()) {
            state = 0;
        }
        break;
    }
    return state;
}

/* Clash C, one frame: every exchange moves the pair to a random point of a random stage path. */
s32 BtlClash_UpdateC(s32 state) {
    BtlClashC *c = &gBtlChars->clashC;
    BtlFlagChr *chr0 = BtlChar_Get(0);
    BtlFlagChr *chr1 = BtlChar_Get(1);
    BtlClashPath *path;
    BtlFlagChr *winner;
    BtlFlagChr *loser;

    switch (state) {
    case 9:
        c->count = 0;
        c->path = (BtlChar_Rand() >> 2) % (u32)(BtlStage_GetPathCount() - 1) + 1;
        c->hold = (BtlChar_Rand() >> 2) % 10 < 7 ? 2 : 1;
        path = BtlStage_GetPath(c->path);
        if (path != NULL) {
            c->point = (BtlChar_Rand() >> 2) % (u32)(path->end - path->first) + path->first;
        } else {
            c->point = 0;
        }
        c->prevPick = -1;
        c->pick = (BtlChar_Rand() >> 2) & 3;
        if (Battle_GetMode() == 1) {
            c->level = gProgress->unk3C;
        } else {
            c->level = 2;
        }
        state++;
        break;
    case 10:
        if (BtlClash_BothInActionFC()) {
            BtlClash_SetPathCut(c->path);
        }
        if (BtlChar_TestFlag(chr0, 0xC4) && BtlChar_TestFlag(chr1, 0xC4)) {
            /* Two human players each answer the same exchange. A valid later
               answer still earns its point; arrival order does not consume
               the other player's opportunity. Retain the CPU difficulty rule
               when an injected fighter is taking part. */
            if (!*(s32 *)((u8 *)chr0 + 0x1278) && !*(s32 *)((u8 *)chr1 + 0x1278)) {
                if (chr0->unkE58 >= 0) BtlAct_CountAndMarkOpponent(chr0);
                if (chr1->unkE58 >= 0) BtlAct_CountAndMarkOpponent(chr1);
            } else if (!*(s32 *)((u8 *)chr0 + 0x1278)) {
                if (chr0->unkE58 >= 0) BtlAct_CountAndMarkOpponent(chr0);
                else if (*(s32 *)((u8 *)chr1 + 0x1278) || chr1->unkE58 >= 0)
                    BtlAct_CountAndMarkOpponent(chr1);
            } else if (chr1->unkE58 >= 0) BtlAct_CountAndMarkOpponent(chr1);
            c->count++;
            if (c->count >= 7) {
                if (chr1->clashCountB == chr0->clashCountB) {
                    BtlChar_SetFlag(chr0, 0xC7);
                    BtlChar_SetFlag(chr1, 0xC7);
                    state++;
                } else {
                if (chr1->clashCountB < chr0->clashCountB) {
                    winner = chr0;
                    loser = chr1;
                } else {
                    winner = chr1;
                    loser = chr0;
                }
                BtlChar_SetFlag(winner, 0xC7);
                state++;
                BtlChar_SetFlag(loser, 0xC8);
                }
            } else {
                c->hold--;
                if (c->hold <= 0) {
                    s32 prev = c->path;
                    s32 n = (BtlChar_Rand() >> 2) % (u32)(BtlStage_GetPathCount() - 2);
                    c->path = n + 1;
                    if (c->path >= prev) {
                        c->path = n + 2;
                    }
                    c->hold = (BtlChar_Rand() >> 2) % 10 < 7 ? 2 : 1;
                    c->point = 0;
                    path = BtlStage_GetPath(c->path);
                    if (path != NULL) {
                        c->point = (BtlChar_Rand() >> 2) % (u32)(path->end - path->first) + path->first;
                    }
                } else {
                    path = BtlStage_GetPath(c->path);
                    if (path != NULL) {
                        s32 prev = c->point;
                        c->point = (BtlChar_Rand() >> 2) % (u32)(path->end - path->first - 1) + path->first;
                        if (c->point >= prev) {
                            c->point++;
                        }
                    } else {
                        c->point = 0;
                    }
                }
                c->prevPick = c->pick;
                c->pick = (BtlChar_Rand() >> 2) & 3;
                BtlChar_SetFlag(chr0, 0xC5);
                BtlChar_SetFlag(chr1, 0xC5);
            }
        }
        if (!BtlClash_BothInActionFB() && !BtlClash_BothInActionFC()) {
            state = 0;
            BtlClash_EndActionFC(0);
            BtlClash_EndActionFC(1);
        }
        break;
    case 11:
        if (!BtlClash_BothInActionFC()) {
            state = 0;
        }
        break;
    }
    return state;
}

/* Runs the clash sequences: starts one when both fighters enter a clash action, then steps it. */
void BtlClash_Update(void) {
    s32 *state = &gBtlChars->clashState;

    if(TagBattle_Active()) {
        TagClash_UpdateBlows();
        /* FA pairs are handled above, including seats 0 and 1. */
        if(*state>=6 && *state<=8)*state=0;
        if(BtlClash_BothInActionFA())return;
    }

    if (*state == 0) {
        if (BtlClash_BothInClashA()) {
            *state = 1;
        }
        if (BtlClash_BothInActionFA()) {
            *state = 6;
        }
        if (BtlClash_BothInActionFB()) {
            *state = 9;
        }
    }
    switch (*state) {
    case 0:
        BtlClash_HoldClashA(0);
        BtlClash_HoldClashA(1);
        if(!TagBattle_Active()){
            BtlClash_EndActionFA(0);
            BtlClash_EndActionFA(1);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        *state = BtlClash_UpdateA(*state);
        if (!BtlClash_BothInClashA()) {
            BtlClash_HoldClashA(0);
            BtlClash_HoldClashA(1);
            *state = 0;
        }
        break;
    case 6:
    case 7:
    case 8:
        *state = BtlClash_UpdateB(*state);
        break;
    case 9:
    case 10:
    case 11:
        *state = BtlClash_UpdateC(*state);
        break;
    }
}

/* Hidden native regression: two concurrent clashes, one with a nonadjacent
   target, using the real ten-fighter roster and the production result flags. */
s32 TagClash_RunRegression(void) {
    s32 i,ok=1;
    if(TagBattle_Count()!=10)return 1;
    TagClash_Reset();TagBattle_ResetPairs();gBtlChars->clashState=0;
    for(i=0;i<10;++i){
        BtlFlagChr *chr=BtlChar_Get(i);
        *(s32 *)((u8 *)chr+0x948)=0xB;
        BtlChar_ClearFlag(chr,0xBF);BtlChar_ClearFlag(chr,0xC0);
        chr->clashCountB=0;
    }
    /* Real CPUs may already have retargeted before the fixture starts. */
    TagBattle_SetTarget(0,5);TagBattle_SetTarget(5,0);
    TagBattle_SetTarget(6,7);TagBattle_SetTarget(7,6);
    for(i=0;i<10;++i)if(i==0||i==5||i==6||i==7)*(s32 *)((u8 *)BtlChar_Get(i)+0x948)=0xFA;
    BtlChar_Get(0)->clashCountB=30;BtlChar_Get(7)->clashCountB=40;
    ok &= TagBattle_Target(0)==5 && TagBattle_Target(5)==0;
    for(i=0;i<80;++i)BtlClash_Update();
    ok &= BtlChar_TestFlag(BtlChar_Get(0),0xBF) && !BtlChar_TestFlag(BtlChar_Get(0),0xC0);
    ok &= BtlChar_TestFlag(BtlChar_Get(5),0xC0) && !BtlChar_TestFlag(BtlChar_Get(5),0xBF);
    ok &= BtlChar_TestFlag(BtlChar_Get(6),0xC0) && BtlChar_TestFlag(BtlChar_Get(7),0xBF);
    for(i=1;i<5;++i)ok &= !BtlChar_TestFlag(BtlChar_Get(i),0xBF) && !BtlChar_TestFlag(BtlChar_Get(i),0xC0);
    printf("tag clash regression: independent pairs 0/5 and 6/7, winners and bystanders %s\n",ok?"PASS":"FAIL");
    return !ok;
}

/* Hidden native fixture: exercise the production end-of-exchange code with
   two humans, including a correct answer that arrives later, errors and ties. */
s32 ClashAnswers_RunRegression(void) {
    static const s32 cases[][6]={{2,9,3,3,4,4},{9,2,3,3,4,4},{-2,8,3,3,3,4},
        {8,-2,3,3,4,3},{-1,-2,3,3,3,3},{12,2,5,2,6,3},{2,12,2,5,3,6}};
    BtlFlagChr *a=BtlChar_Get(0),*b=BtlChar_Get(1);s32 n,ok=1;
    for(n=0;n<(s32)(sizeof(cases)/sizeof(cases[0]));++n){
        const s32 *c=cases[n];
        *(s32 *)((u8 *)a+0x948)=*(s32 *)((u8 *)b+0x948)=0xFC;
        *(s32 *)((u8 *)a+0x1278)=*(s32 *)((u8 *)b+0x1278)=0;
        a->unkE58=c[0];b->unkE58=c[1];a->clashCountB=c[2];b->clashCountB=c[3];
        BtlChar_ClearFlag(a,0xC7);BtlChar_ClearFlag(a,0xC8);BtlChar_ClearFlag(b,0xC7);BtlChar_ClearFlag(b,0xC8);
        BtlChar_SetFlag(a,0xC4);BtlChar_SetFlag(b,0xC4);gBtlChars->clashC.count=6;
        BtlClash_UpdateC(10);
        ok &= a->clashCountB==c[4] && b->clashCountB==c[5];
        if(c[4]==c[5])ok &= BtlChar_TestFlag(a,0xC7)&&BtlChar_TestFlag(b,0xC7);
        else ok &= BtlChar_TestFlag(c[4]>c[5]?a:b,0xC7)&&BtlChar_TestFlag(c[4]>c[5]?b:a,0xC8);
    }
    printf("clash answers regression: seven scoring/arrival-order/error/tie cases %s\n",ok?"PASS":"FAIL");
    return !ok;
}
