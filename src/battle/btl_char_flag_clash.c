#include "common.h"
#include "battle/btl_char_flag.h"
#include "battle/tag_clash.h"
#include "battle/tag_options.h"
#include "battle/tag_assist.h"
#include "battle/btl_input.h"

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
extern f32 EftStruggle_GetMidDist(s32 actor);
extern void EftStruggle_EndFor(s32 actor,s32 winner);
extern s32 BtlStage_GetPathCount(void);                                       /* number of stage paths */
extern BtlClashPath *BtlStage_GetPath(s32 n);                            /* stage path n */
extern s32 BtlSuper_IsThrow(BtlFlagChr *chr, s32 cls);
extern s32 BtlSuper_GetFlags(BtlFlagChr *chr, s32 cls);                   /* attribute word of the technique */
extern s32 BtlSuper_GetDamage(BtlFlagChr *chr, s32 cls, s32 arg2, s32 arg3); /* damage of the technique */
extern void BtlColl_StartThrow(BtlFlagChr *chr, BtlFlagChr *target, s32 cls, s32 attr);
extern void BtlMember_Damage(BtlFlagChr *chr, s32 damage, s32 flags);    /* applies damage */

extern BtlFlagRoster *gBtlChars;
extern Vec4 D_002EC2A0; /* zero vector */

typedef struct BtlClashProgress {
    /* 0x00 */ u8 unk0[0x3C];
    /* 0x3C */ s32 unk3C; /* difficulty */
} BtlClashProgress;
extern BtlClashProgress *gProgress;

/* Two independent native pairs can coexist. The original serialized roster is
 * left intact; actions access their own pair rather than one global exchange. */
static BtlClashPair pairs[PORT_FIGHTER_CAPACITY/2];
extern void *memset(void *, s32, u32);

static BtlClashPair *pair_for(s32 actor) {
    s32 i;
    for(i=0;i<PORT_FIGHTER_CAPACITY/2;i++) if(pairs[i].used &&
        (pairs[i].actor[0]==actor || pairs[i].actor[1]==actor)) return &pairs[i];
    return NULL;
}
void BtlClash_Reset(void) { memset(pairs,0,sizeof(pairs)); }
s32 BtlClash_Join(s32 a,s32 b) {
    BtlClashPair *pa=pair_for(a), *pb=pair_for(b);
    s32 i;
    if(a<0 || b<0 || a>=TagTeam_Count() || b>=TagTeam_Count() || !TagTeam_IsEnemy(a,b)) return 0;
    if(pa || pb) return pa && pa==pb;
    for(i=0;i<PORT_FIGHTER_CAPACITY/2;i++) if(!pairs[i].used) {
        BtlClashPair *p=&pairs[i];
        memset(p,0,sizeof(*p)); p->used=1;
        p->actor[0]=a<b?a:b; p->actor[1]=a<b?b:a;
        p->c.prevPick=-1;
        {
            u32 seed=Port_TagOption(TAG_OPT_CLASH_QTE)?BtlChar_Rand():1;
            TagSequence_Init(&p->sequence[0],seed);
            TagSequence_Init(&p->sequence[1],seed);
        }
        return 1;
    }
    return 0;
}
s32 BtlClash_Opponent(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    return p ? p->actor[actor==p->actor[0]?1:0] : -1;
}
s32 BtlClash_Side(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    return p ? actor!=p->actor[0] : actor&1;
}
void *BtlClash_Exchange(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    return p ? &p->c : &gBtlChars->clashC;
}
f32 BtlClash_Bias(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    return p ? p->a.bias : 0.0f;
}
s32 BtlClash_State(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    return p ? p->state : 0;
}
s32 BtlClash_SequenceActive(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    return Port_TagOption(TAG_OPT_CLASH_QTE) && p &&
        (p->state==1 || p->state==2 || p->state==6 || p->state==7 || p->state==9 || p->state==10);
}
s32 BtlClash_SequenceInput(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    BtlInputChr *chr=(BtlInputChr *)BtlChar_Get(actor);
    TagClashSequence *s;
    u32 pressed;
    if(!BtlClash_SequenceActive(actor)) return 0;
    s=&p->sequence[actor==p->actor[1]];
    pressed=chr->input.pressed>>27;
    if(chr->injectOn) {
        extern s32 BtlAiMgr_GetLevel(s32);
        s32 level=BtlAiMgr_GetLevel(actor), interval;
        if(level<0) pressed=0;
        else {
            if(level>29) level=29;
            interval=18-level*12/29;
            pressed=p->age%interval==0 ? 1u<<s->button : 0;
        }
    }
    return TagSequence_Press(s,p->age,pressed);
}
s32 BtlClash_SequencePrompt(s32 actor) {
    BtlClashPair *p=pair_for(actor);
    BtlInputChr *chr=(BtlInputChr *)BtlChar_Get(actor);
    extern s32 BtlCharApi_GetButtonIcon(void *,u32);
    if(!BtlClash_SequenceActive(actor) || chr->injectOn) return -1;
    return BtlCharApi_GetButtonIcon(chr,27+p->sequence[actor==p->actor[1]].button);
}
static s32 random_below(s32 n) { return n>1 ? (BtlChar_Rand()>>2) % (u32)n : 0; }
static s32 is_clashing(s32 actor) {
    s32 action=BtlAct_GetCurrent(BtlChar_Get(actor));
    return (action>=0x130 && action<=0x132) || (action>=0xFA && action<=0xFC);
}

/* 1 while both fighters are in actions 0x130..0x132. */
s32 BtlClash_BothInClashA(BtlClashPair *pair) {
    if (BtlCharApi_IsInClashA(pair->actor[0]) && BtlCharApi_IsInClashA(pair->actor[1])) {
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
s32 BtlClash_BothInActionFA(BtlClashPair *pair) {
    s32 action = BtlAct_GetCurrent(BtlChar_Get(pair->actor[0]));

    if (action != 0xFA) {
        return 0;
    }
    return BtlAct_GetCurrent(BtlChar_Get(pair->actor[1])) == action;
}

/* Raises flag 0xBF on a fighter still in action 0xFA (ends it). */
void BtlClash_EndActionFA(s32 player) {
    BtlFlagChr *chr = BtlChar_Get(player);

    if (BtlAct_GetCurrent(chr) == 0xFA) {
        BtlChar_SetFlag(chr, 0xBF);
    }
}

/* 1 while both fighters are in action 0xFB. */
s32 BtlClash_BothInActionFB(BtlClashPair *pair) {
    s32 action = BtlAct_GetCurrent(BtlChar_Get(pair->actor[0]));

    if (action != 0xFB) {
        return 0;
    }
    return BtlAct_GetCurrent(BtlChar_Get(pair->actor[1])) == action;
}

/* 1 while both fighters are in action 0xFC. */
s32 BtlClash_BothInActionFC(BtlClashPair *pair) {
    s32 action = BtlAct_GetCurrent(BtlChar_Get(pair->actor[0]));

    if (action != 0xFC) {
        return 0;
    }
    return BtlAct_GetCurrent(BtlChar_Get(pair->actor[1])) == action;
}

/* Raises flag 0xC6 on a fighter still in action 0xFC (ends it). */
void BtlClash_EndActionFC(s32 player) {
    BtlFlagChr *chr = BtlChar_Get(player);

    if (BtlAct_GetCurrent(chr) == 0xFC) {
        BtlChar_SetFlag(chr, 0xC6);
    }
}

/* Clash A camera: a one-frame cut orbiting one fighter (type 0 wide, 1 from the side, 2 close on node 0x30). */
void BtlClash_SetOrbitCut(BtlClashPair *pair, s32 side, s32 type) {
    Vec4 d;
    BtlFlagChr *chr;
    s32 nodeA = 3;
    s32 nodeB = 0x11;
    f32 pitch;
    f32 yaw;
    f32 dist;

    s32 player = pair->actor[side];
    if (TagTeam_Active() && !Port_TagOption(TAG_OPT_SHARED_CLASH)) return;
    chr = BtlChar_Get(player);
    pitch = 0.0f;
    BtlOpp_GetDelta(chr, &d);
    yaw = 0.0f;
    dist = 0.0f;
    Vec3_Normalize(&d, &d);
    switch (type) {
    case 0:
        if (side == 0) {
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
        if (side == 0) {
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
void BtlClash_SetMidCut(BtlClashPair *pair, Vec4 *mid, f32 bias) {
    Vec4 d;
    Vec4 look;
    Vec4 a;
    Vec4 b;
    BtlFlagChr *chr = BtlChar_Get(pair->actor[0]);
    f32 ang = 1.5707963f;
    f32 half;
    f32 base;
    f32 yaw;
    f32 pitch;

    if (TagTeam_Active() && !Port_TagOption(TAG_OPT_SHARED_CLASH)) return;
    BtlOpp_GetDelta(chr, &d);
    half = 0.5f;
    ang = bias * ang;
    base = 50.0f;
    Vec3_Normalize(&d, &d);
    BtlCharApi_GetNodePos(pair->actor[0], 0x11, &a);
    ang = ang * half;
    BtlCharApi_GetNodePos(pair->actor[1], 0x11, &b);
    Vec4_Lerp(&look, &b, &a, bias * 0.6f + half);
    ang = ang + -1.5707963f;
    yaw = BtlUtil_WrapAngle(atan2f(d.x, d.z) + ang);
    pitch = -Mathf_Asin(d.y);
    pitch *= bias;
    ChrCam_SetCut(chr, mid, &D_002EC2A0, mid, &D_002EC2A0, &look, &D_002EC2A0, -1, yaw, 0.0f, pitch, 0.0f,
                  EftStruggle_GetMidDist(pair->actor[0]) + base, 0.0f, -1, -1, -1, 1, 0x45);
}

/* Clash C camera: a one-frame cut placed at the first point of a stage path, looking along its angles. */
void BtlClash_SetPathCut(BtlClashPair *pair, s32 n) {
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
        ChrCam_SetCut(BtlChar_Get(pair->actor[0]), &pos, &D_002EC2A0, &pos, &D_002EC2A0, &look, &D_002EC2A0, -1, 0.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, -1, -1, -1, 1, 0x45);
    }
}

/* Squashes the clash balance into -0.8..0.8: x / (|x| + 1) with x = lead / 20. */
f32 BtlClash_CalcBias(s32 lead) {
    f32 x = lead * 0.05f;

    return BtlUtil_ClampF(x / (__builtin_fabsf(x) + 1.0f), -0.8f, 0.8f);
}

/* Clash A, one frame. Returns the next state (0 when it is over). */
s32 BtlClash_UpdateA(BtlClashPair *pair, s32 state) {
    Vec4 a;
    Vec4 b;
    BtlFlagChr *chr0 = BtlChar_Get(pair->actor[0]);
    BtlFlagChr *chr1 = BtlChar_Get(pair->actor[1]);
    s32 count0 = chr0->clashCountA * TagAssist_Multiplier(pair->actor[0])/100;
    BtlClashA *c = &pair->a;
    s32 count1 = chr1->clashCountA * TagAssist_Multiplier(pair->actor[1])/100;

    if(Port_TagOption(TAG_OPT_CLASH_QTE)) {
        count0=pair->sequence[0].score*TagAssist_Multiplier(pair->actor[0])/100;
        count1=pair->sequence[1].score*TagAssist_Multiplier(pair->actor[1])/100;
    }

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
            BtlClash_SetOrbitCut(pair, 0, 0);
        } else if (c->timer < 0x3C) {
            BtlClash_SetOrbitCut(pair, 1, 0);
        } else {
            if (count1 < count0) {
                c->lead++;
            }
            if (count0 < count1) {
                c->lead--;
            }
            if (c->bias < -0.64f) {
                BtlClash_SetOrbitCut(pair, 0, 1);
            } else if (0.64f < c->bias) {
                BtlClash_SetOrbitCut(pair, 1, 1);
            } else {
                BtlClash_SetMidCut(pair, &c->mid, c->bias);
            }
        }
        c->timer++;
        if (c->timer >= 0x6A) {
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
            BtlClash_SetOrbitCut(pair, 0, 2);
        } else {
            BtlClash_SetOrbitCut(pair, 1, 2);
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
        BtlClash_SetMidCut(pair, &c->mid, c->bias);
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
                EftStruggle_EndFor(winner->objId,winner->objId);
            } else {
                damage = BtlSuper_GetDamage(winner, cls, 0, 1);
                damage += BtlSuper_GetDamage(loser, loserCls, 0, 1) / 2;
                damage = damage*TagAssist_DamageMultiplier(pair->actor[0],pair->actor[1])/100;
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
                EftStruggle_EndFor(winner->objId,-1);
            }
            BtlChar_RaiseFirstClash(winner);
            c->winner = winner->objId;
        }
        if (c->winner == pair->actor[0]) {
            BtlClash_SetOrbitCut(pair, 1, 0);
        } else {
            BtlClash_SetOrbitCut(pair, 0, 0);
        }
        c->lead = 0;
        state = 0;
        break;
    }
    c->bias = BtlClash_CalcBias(c->lead);
    BtlCharApi_GetNodePos(pair->actor[0], 0x11, &a);
    BtlCharApi_GetNodePos(pair->actor[1], 0x11, &b);
    Vec4_Lerp(&c->mid, &b, &a, c->bias * 0.5f + 0.5f);
    return state;
}

/* Clash B, one frame. Returns the next state (0 when it is over). */
s32 BtlClash_UpdateB(BtlClashPair *pair, s32 state) {
    BtlClashB *c = &pair->b;
    BtlFlagChr *chr0 = BtlChar_Get(pair->actor[0]);
    BtlFlagChr *chr1 = BtlChar_Get(pair->actor[1]);

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
        if (!BtlClash_BothInActionFA(pair)) {
            state = 0;
            BtlClash_EndActionFA(pair->actor[0]);
            BtlClash_EndActionFA(pair->actor[1]);
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
            winner = BtlChar_Get(pair->actor[win]);
            loser = BtlChar_Get(pair->actor[lose]);
            BtlChar_SetFlag(winner, 0xBF);
            BtlChar_SetFlag(loser, 0xC0);
            BtlChar_PlayVoice(winner, 0x19);
            BtlChar_RaiseFirstClash(winner);
            ChrCam_EndCut(chr0);
            c->winner = pair->actor[win];
        }
        c->timer++;
        if (!BtlClash_BothInActionFA(pair)) {
            state = 0;
        }
        break;
    }
    return state;
}

/* Clash C, one frame: every exchange moves the pair to a random point of a random stage path. */
s32 BtlClash_UpdateC(BtlClashPair *pair, s32 state) {
    BtlClashC *c = &pair->c;
    BtlFlagChr *chr0 = BtlChar_Get(pair->actor[0]);
    BtlFlagChr *chr1 = BtlChar_Get(pair->actor[1]);
    BtlClashPath *path;
    BtlFlagChr *winner;
    BtlFlagChr *loser;

    switch (state) {
    case 9:
        c->count = 0;
        c->path = random_below(BtlStage_GetPathCount() - 1) + 1;
        c->hold = (BtlChar_Rand() >> 2) % 10 < 7 ? 2 : 1;
        path = BtlStage_GetPath(c->path);
        if (path != NULL) {
            c->point = random_below(path->end - path->first) + path->first;
        } else {
            c->point = 0;
        }
        c->prevPick = -1;
        c->pick = (BtlChar_Rand() >> 2) & 3;
        if (Battle_GetMode() == 1) {
            c->level = gProgress->unk3C;
        } else {
            c->level = 1;
        }
        state++;
        break;
    case 10:
        if (BtlClash_BothInActionFC(pair)) {
            BtlClash_SetPathCut(pair, c->path);
        }
        if (BtlChar_TestFlag(chr0, 0xC4) && BtlChar_TestFlag(chr1, 0xC4)) {
            if (Port_TagOption(TAG_OPT_CLASH_QTE)) {
                chr0->clashCountB=pair->sequence[0].score;
                chr1->clashCountB=pair->sequence[1].score;
                /* Resolve ties after a short extension; never wait forever
                   for an absent controller to provide a deciding input. */
                if(pair->age>=300 && chr0->clashCountB==chr1->clashCountB)
                    BtlAct_CountAndMarkOpponent((BtlChar_Rand()&1)?chr0:chr1);
            } else if (chr0->unkE58 >= 0) {
                BtlAct_CountAndMarkOpponent(chr0);
            } else {
                BtlAct_CountAndMarkOpponent(chr1);
            }
            c->count++;
            if ((Port_TagOption(TAG_OPT_CLASH_QTE) ? pair->age>=240 : c->count>=7) && chr0->clashCountB != chr1->clashCountB) {
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
            } else {
                c->hold--;
                if (c->hold <= 0) {
                    s32 prev = c->path;
                    s32 n = random_below(BtlStage_GetPathCount() - 2);
                    c->path = n + 1;
                    if (c->path >= prev && BtlStage_GetPathCount() > 2) {
                        c->path = n + 2;
                    }
                    c->hold = (BtlChar_Rand() >> 2) % 10 < 7 ? 2 : 1;
                    c->point = 0;
                    path = BtlStage_GetPath(c->path);
                    if (path != NULL) {
                        c->point = random_below(path->end - path->first) + path->first;
                    }
                } else {
                    path = BtlStage_GetPath(c->path);
                    if (path != NULL) {
                        s32 prev = c->point;
                        c->point = random_below(path->end - path->first - 1) + path->first;
                        if (c->point >= prev && path->end-path->first > 1) {
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
        if (!BtlClash_BothInActionFB(pair) && !BtlClash_BothInActionFC(pair)) {
            state = 0;
            BtlClash_EndActionFC(pair->actor[0]);
            BtlClash_EndActionFC(pair->actor[1]);
        }
        break;
    case 11:
        if (!BtlClash_BothInActionFC(pair)) {
            state = 0;
        }
        break;
    }
    return state;
}

/* Release only the participants. Never leave a waiting clash action behind
 * when the other actor is interrupted, KO'd, transformed, or removed. */
static void release_pair(BtlClashPair *p) {
    s32 i;
    for(i=0;i<2;i++) {
        s32 actor=p->actor[i];
        BtlFlagChr *chr=BtlChar_Get(actor);
        BtlClash_HoldClashA(actor);
        BtlClash_EndActionFA(actor);
        BtlClash_EndActionFC(actor);
        if(BtlAct_GetCurrent(chr)==0xFB) {
            extern void BtlAct_Request(BtlFlagChr *,s32);
            BtlAct_Request(chr,0xB);
        }
        ChrCam_EndCut(chr);
    }
    p->used=0;
}

void BtlClash_Update(void) {
    s32 i;
    /* Native duel/script actions historically start together without an
     * explicit collision callback. Preserve that path only for a real duel. */
    if(!TagTeam_Active() && !pair_for(0) && is_clashing(0) && is_clashing(1)) BtlClash_Join(0,1);
    for(i=0;i<PORT_FIGHTER_CAPACITY/2;i++) {
        BtlClashPair *pair=&pairs[i];
        s32 *state=&pair->state;
        if(!pair->used) continue;
        {
            extern s32 BtlChar_IsFrozen(BtlFlagChr *);
            if(BtlChar_IsFrozen(BtlChar_Get(pair->actor[0])) ||
               BtlChar_IsFrozen(BtlChar_Get(pair->actor[1]))) continue;
        }
        pair->age++;
        if(*state==0) {
            if(BtlClash_BothInClashA(pair)) *state=1;
            else if(BtlClash_BothInActionFA(pair)) *state=6;
            else if(BtlClash_BothInActionFB(pair)) *state=9;
            else if(pair->age>30) release_pair(pair);
            /* Collision flags need one action update before both are ready. */
            if(!*state) continue;
        }
        if(*state<=5) {
            *state=BtlClash_UpdateA(pair,*state);
            if(!BtlClash_BothInClashA(pair)) *state=0;
        } else if(*state<=8) *state=BtlClash_UpdateB(pair,*state);
        else *state=BtlClash_UpdateC(pair,*state);
        /* A broken/missing stage path cannot trap the battle indefinitely. */
        if(!*state || pair->age>900) release_pair(pair);
    }
    for(i=0;i<TagTeam_Count();i++) if(!pair_for(i) && !TagAssist_Active(i) && is_clashing(i)) {
        BtlClash_HoldClashA(i);
        BtlClash_EndActionFA(i);
        BtlClash_EndActionFC(i);
    }
}
