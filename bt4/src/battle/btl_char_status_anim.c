#include "common.h"
#include "battle/btl_char_status.h"

/*
 * Fighter animation control, 0x1C3CA8..0x1C4BF8. See battle/btl_char_status.h for the structures.
 *
 * The fighter keeps the ids (fighter + 0x974: current, requested, previous, second layer); the frame counter
 * itself lives in the fighter's battle object (object + 0xB40). An id indexes the roster table gBtlChars + 0x20
 * (0x19E words of per-animation flags).
 *
 * The action code never starts an animation directly while it runs: it calls BtlAnim_Request (flag 0x2D,
 * 85 call sites) and the request is carried out by BtlAnim_FlushRequest from 0x1E23D0, or it calls
 * BtlAnim_Play (133 call sites) for an immediate change. Starting an animation raises the one-frame flag 0x2B.
 * Each action handler then advances its animation once a frame with BtlAnim_Advance (stops at the end and
 * holds flag 0x31), BtlAnim_AdvanceThen (requests a follow-up animation at the end) or BtlAnim_AdvanceLoop
 * (wraps to frame 0), and times its events with the BtlAnim_Passed... / BtlAnim_In...Range tests, which compare
 * the frame before and after the last advance.
 */

extern BtlStatObj *BtlChar_GetObj(BtlStatChr *chr);
extern s32 BtlChar_TestFlag(BtlStatChr *chr, s32 flag);
extern void BtlChar_SetFlag(BtlStatChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlStatChr *chr, s32 flag);
extern void BtlChar_ClearFlag(BtlStatChr *chr, s32 flag);
extern s32 BtlChar_TestPrevFlag(BtlStatChr *chr, s32 flag);              /* the flag in the second pair of flag arrays */
extern void BtlChar_ClearFlagRange(BtlStatChr *chr, s32 first, s32 last);  /* BtlChar_ClearFlag for first..last */
extern s32 BtlOpp_GetObj(BtlStatChr *chr);                        /* the opponent's battle object (inferred) */
extern s32 BtlCharApi_IsInRushSequence(s32 objId);
extern void BtlObj_SetSubState(BtlStatObj *obj, s32 state, s32 arg);
extern void BtlObjAnim_StartBlend(BtlStatObj *obj, f32 blend);            /* starts a blend of `blend` seconds */
extern void BtlObjAnim_ZeroRootAxes(void *handle, s32 a, s32 b, s32 c);
extern void BtlObjAnim_RebaseRoot(void *handle);
extern void BtlObjAnim_ClearNodeRot(void *handle, s32 node);
extern void BtlObjAnim_Play(BtlStatObj *obj, s32 layer, s32 anim, s32 reset);
extern void BtlObjAnim_PlayFrom(BtlStatObj *obj, s32 other, s32 anim, s32 arg);
extern void BtlObjAnim_PromoteLayer(BtlStatObj *obj);                       /* the second layer becomes the main one */
extern s32 BtlObjAnim_TestEvent(BtlStatObj *obj, u64 mask);              /* attribute test at the current frame */
extern void BtlObj_SetEyeFrame(BtlStatObj *obj, s32 state);
extern s32 BtlObj_GetMouthMode(BtlStatObj *obj);

extern BtlStatRoster *gBtlChars;

/* Applies an animation's table flags 0x40000000 / 0x80000000 / 2 to a layer handle. */
void BtlAnim_ApplyTableFlags(void *handle, s32 anim) {
    if (handle != NULL) {
        do {
            if (BtlAnim_GetFlags(anim) & BTL_ANIM_F_40000000) {
                BtlObjAnim_ZeroRootAxes(handle, 1, 1, 1);
            } else if (BtlAnim_GetFlags(anim) & BTL_ANIM_F_80000000) {
                BtlObjAnim_RebaseRoot(handle);
                if (BtlAnim_GetFlags(anim) & BTL_ANIM_F_2) {
                    BtlObjAnim_ZeroRootAxes(handle, 1, 0, 0);
                }
            }
        } while (0);
    }
}

/* Switches off the nodes that models 0x78, 0x98 and 0x99 do not animate. */
void BtlAnim_HideModelNodes(void *handle, s32 model) {
    if (handle != NULL) {
        do {
            switch (model) {
                case 0x98:
                    BtlObjAnim_ClearNodeRot(handle, 0x10);
                    BtlObjAnim_ClearNodeRot(handle, 0x11);
                    BtlObjAnim_ClearNodeRot(handle, 0x2E);
                    BtlObjAnim_ClearNodeRot(handle, 0x2F);
                    break;
                case 0x99:
                    BtlObjAnim_ClearNodeRot(handle, 0x11);
                    break;
                case 0x78:
                    BtlObjAnim_ClearNodeRot(handle, 0x11);
                    BtlObjAnim_ClearNodeRot(handle, 0x2E);
                    BtlObjAnim_ClearNodeRot(handle, 0x23);
                    BtlObjAnim_ClearNodeRot(handle, 0x15);
                    break;
            }
        } while (0);
    }
}

/* Switches off node 0x10 of model 0x78. */
void BtlAnim_HideModelNode10(void *handle, s32 model) {
    if (handle != NULL) {
        do {
            if (model == 0x78) {
                BtlObjAnim_ClearNodeRot(handle, 0x10);
            }
        } while (0);
    }
}

/* Starts an animation on the main layer now, raising flag 0x2B; blend > 0 blends into it over that many seconds. */
void BtlAnim_Play(BtlStatChr *chr, s32 anim, f32 blend) {
    BtlAnimIds *ids = &chr->anim;
    BtlStatObj *obj = BtlChar_GetObj(chr);
    s32 n;

    if (BtlAnim_GetFlags(anim) & BTL_ANIM_F_20000000) {
        s32 arg = 0;
        BtlStatObj *obj2;

        if (BtlCharApi_IsInRushSequence(chr->objId)) {
            arg = chr->unkEE4;
        }
        obj2 = BtlChar_GetObj(chr);
        BtlObjAnim_PlayFrom(obj2, BtlOpp_GetObj(chr), anim, arg);
        BtlAnim_HideModelNode10(obj->anim.handle, obj->model);
    } else {
        BtlObjAnim_Play(obj, 0, anim, 1);
    }
    obj->flags &= ~0x20;
    obj->flags &= ~0x40;
    n = BtlObj_GetMouthMode(obj);
    if (n < 13) {
        if (n >= 9) {
            BtlObj_SetSubState(obj, 0, 0);
        }
    }
    BtlObj_SetEyeFrame(obj, 9);
    BtlChar_ClearFlag(chr, BTL_ANIM_FLAG_30);
    BtlAnim_ApplyTableFlags(obj->anim.handle, anim);
    BtlAnim_HideModelNodes(obj->anim.handle, obj->model);
    BtlChar_ClearFlagRange(chr, BTL_ANIM_FLAG_REQ, BTL_ANIM_FLAG_END);
    BtlChar_SetFlag(chr, BTL_ANIM_FLAG_NEW);
    ids->next = -1;
    ids->cur = anim;
    chr->stall = 0;
    if (blend > 0.0f) {
        BtlAnim_SetBlend(chr, blend);
        BtlAnim_ApplyBlend(chr);
    }
}

/* Starts an animation on the main layer without resetting the object's state, raising flag 0x2C. */
void BtlAnim_PlayKeep(BtlStatChr *chr, s32 anim, f32 blend) {
    BtlStatObj *obj = BtlChar_GetObj(chr);
    BtlAnimIds *ids = &chr->anim;

    BtlObjAnim_Play(obj, 0, anim, 0);
    BtlAnim_ApplyTableFlags(obj->anim.handle, anim);
    BtlAnim_HideModelNodes(obj->anim.handle, obj->model);
    BtlChar_ClearFlagRange(chr, BTL_ANIM_FLAG_REQ, BTL_ANIM_FLAG_END);
    BtlChar_SetFlag(chr, BTL_ANIM_FLAG_NEW_KEEP);
    ids->cur = anim;
    ids->next = -1;
    if (blend > 0.0f) {
        BtlAnim_SetBlend(chr, blend);
        BtlAnim_ApplyBlend(chr);
    }
}

/* Makes the second layer's animation the main one, raising flag 0x2B. */
void BtlAnim_SubToMain(BtlStatChr *chr) {
    BtlStatObj *obj = BtlChar_GetObj(chr);
    BtlAnimIds *ids = &chr->anim;

    BtlObjAnim_PromoteLayer(obj);
    ids->cur = ids->sub;
    obj->flags &= ~0x20;
    obj->flags &= ~0x40;
    BtlChar_ClearFlagRange(chr, BTL_ANIM_FLAG_REQ, BTL_ANIM_FLAG_END);
    BtlChar_SetFlag(chr, BTL_ANIM_FLAG_NEW);
    chr->stall = 0;
}

/* Requests an animation: BtlAnim_FlushRequest starts it with BtlAnim_Play. */
void BtlAnim_Request(BtlStatChr *chr, s32 anim, f32 blend) {
    BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_REQ);
    chr->anim.next = anim;
    BtlAnim_SetBlend(chr, blend);
}

/* Requests an animation: BtlAnim_FlushRequestKeep starts it with BtlAnim_PlayKeep. */
void BtlAnim_RequestKeep(BtlStatChr *chr, s32 anim, f32 blend) {
    BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_REQ_KEEP);
    chr->anim.next = anim;
    BtlAnim_SetBlend(chr, blend);
}

/* Requests BtlAnim_SubToMain. */
void BtlAnim_RequestSubToMain(BtlStatChr *chr) {
    BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_REQ_SUB);
}

/* Stores the blend time for the next animation start and holds flag 0x32. */
void BtlAnim_SetBlend(BtlStatChr *chr, f32 blend) {
    BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_BLEND_REQ);
    chr->blendTime = blend;
}

/* The same with a blend time of 0. No caller. */
void BtlAnim_SetNoBlend(BtlStatChr *chr) {
    BtlAnim_SetBlend(chr, 0.0f);
}

/* Starts the animation requested with BtlAnim_Request. */
void BtlAnim_FlushRequest(BtlStatChr *chr) {
    if (BtlChar_TestFlag(chr, BTL_ANIM_FLAG_REQ)) {
        BtlAnim_Play(chr, chr->anim.next, chr->blendTime);
    }
}

/* Starts the animation requested with BtlAnim_RequestKeep. */
void BtlAnim_FlushRequestKeep(BtlStatChr *chr) {
    if (BtlChar_TestFlag(chr, BTL_ANIM_FLAG_REQ_KEEP)) {
        BtlAnim_PlayKeep(chr, chr->anim.next, chr->blendTime);
    }
}

/* Carries out BtlAnim_RequestSubToMain when the second layer has an animation. */
void BtlAnim_FlushSubToMain(BtlStatChr *chr) {
    if (BtlChar_TestFlag(chr, BTL_ANIM_FLAG_REQ_SUB)) {
        if (chr->anim.sub >= 0) {
            BtlAnim_SubToMain(chr);
        }
    }
}

/* Hands the stored blend time to the object (0 with flag 0x37) and takes one step off the blend counter. */
void BtlAnim_ApplyBlend(BtlStatChr *chr) {
    BtlStatObj *obj = BtlChar_GetObj(chr);

    if (BtlChar_TestFlag(chr, BTL_ANIM_FLAG_NO_BLEND)) {
        BtlObjAnim_StartBlend(obj, 0.0f);
        BtlChar_ClearFlag(chr, BTL_ANIM_FLAG_BLEND_REQ);
        chr->blendTime = 0.0f;
    } else if (BtlChar_TestFlag(chr, BTL_ANIM_FLAG_BLEND_REQ)) {
        BtlAnimObj *a;

        BtlObjAnim_StartBlend(obj, chr->blendTime);
        a = &obj->anim;
        BtlChar_ClearFlag(chr, BTL_ANIM_FLAG_BLEND_REQ);
        chr->blendTime = 0.0f;
        a->blend -= a->blendStep;
        if (a->blend < 0.0f) {
            a->blend = 0.0f;
        }
    }
}

/* Starts an animation on the second layer unless it is already there. */
void BtlAnim_PlaySub(BtlStatChr *chr, s32 anim) {
    if (chr->anim.sub != anim) {
        BtlAnimIds *ids = &chr->anim;
        BtlStatObj *obj = BtlChar_GetObj(chr);

        BtlObjAnim_Play(obj, 1, anim, 0);
        BtlAnim_ApplyTableFlags(obj->anim.subHandle, anim);
        BtlAnim_HideModelNodes(obj->anim.subHandle, obj->model);
        ids->sub = anim;
    }
}

/* Sets object + 0xC8C. */
void BtlAnim_SetUnkC8C(BtlStatChr *chr, f32 v) {
    BtlChar_GetObj(chr)->anim.unk14C = v;
}

/* Sets the frames added per advance. */
void BtlAnim_SetStep(BtlStatChr *chr, f32 step) {
    BtlChar_GetObj(chr)->anim.step = step;
}

/* Sets the step so that the whole animation takes `seconds` (one advance if shorter than a frame). */
void BtlAnim_SetDuration(BtlStatChr *chr, f32 seconds) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    if (seconds < 0.033333333f) {
        a->step = a->length;
    } else {
        a->step = a->length / (seconds * 30.0f);
    }
}

/* Sets object + 0xCB8 (1.0 from the manager every frame). */
void BtlAnim_SetObjRate(BtlStatChr *chr, f32 rate) {
    BtlChar_GetObj(chr)->anim.rate = rate;
}

/* Sets fighter + 0x988 (1.0 from the manager every frame). */
void BtlAnim_SetRate(BtlStatChr *chr, f32 rate) {
    chr->animRate = rate;
}

/* Puts the animation on its last frame, with no blend, and holds the end flag 0x31. */
void BtlAnim_JumpToEnd(BtlStatChr *chr) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    a->blend = 0.0f;
    BtlChar_ClearFlag(chr, BTL_ANIM_FLAG_BLENDING);
    BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_END);
    a->prevFrame = a->frame = a->length;
}

/* BtlObjAnim_ZeroRootAxes(main handle, 1, 1, 1): what table flag 0x40000000 does at an animation start. */
void BtlAnim_EnableHandle(BtlStatChr *chr) {
    BtlObjAnim_ZeroRootAxes(BtlChar_GetObj(chr)->anim.handle, 1, 1, 1);
}

/* Current animation id. */
s32 BtlAnim_GetId(BtlStatChr *chr) {
    return chr->anim.cur;
}

/* Requested animation id (-1 after a start). */
s32 BtlAnim_GetNextId(BtlStatChr *chr) {
    return chr->anim.next;
}

/* Last frame's animation id. */
s32 BtlAnim_GetPrevId(BtlStatChr *chr) {
    return chr->anim.prev;
}

/* Flag word of an animation id from the roster table; 0 when out of range. */
u32 BtlAnim_GetFlags(u32 anim) {
    BtlStatRoster *roster = gBtlChars;
    u32 *tbl;

    if (roster == NULL) {
        return 0;
    }
    tbl = roster->animFlags;
    if (tbl == NULL) {
        return 0;
    }
    if (anim >= BTL_ANIM_COUNT) {
        return 0;
    }
    return tbl[anim];
}

/* Current frame. */
f32 BtlAnim_GetFrame(BtlStatChr *chr) {
    return BtlChar_GetObj(chr)->anim.frame;
}

/* Current frame / length, 0..1. */
f32 BtlAnim_GetProgress(BtlStatChr *chr) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    return a->frame / a->length;
}

/* Length of the current animation in frames. */
f32 BtlAnim_GetLength(BtlStatChr *chr) {
    return BtlChar_GetObj(chr)->anim.length;
}

/* Frames added per advance. */
f32 BtlAnim_GetStep(BtlStatChr *chr) {
    return BtlChar_GetObj(chr)->anim.step;
}

/* Object + 0xCB8. */
f32 BtlAnim_GetObjRate(BtlStatChr *chr) {
    return BtlChar_GetObj(chr)->anim.rate;
}

/* Attribute test of the current animation frame; 0 while the frame did not move on the advance before last. */
s32 BtlAnim_TestAttr(BtlStatChr *chr, u64 mask) {
    BtlChar_GetObj(chr);
    if (chr->prevStall > 0) {
        return 0;
    }
    return BtlObjAnim_TestEvent(BtlChar_GetObj(chr), mask);
}

/* Advances the animation one step (flags & 1: not while blending); at the end clamps, holds flag 0x31 and returns 1. */
s32 BtlAnim_Advance(BtlStatChr *chr, s32 flags) {
    s32 done = 0;
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    a->blend -= a->blendStep;
    if (a->blend < 0.0f) {
        a->blend = 0.0f;
        BtlChar_ClearFlag(chr, BTL_ANIM_FLAG_BLENDING);
    } else {
        BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_BLENDING);
    }
    a->prevFrame = a->frame;
    if (!(flags & 1) || a->blend == 0.0f) {
        a->frame += a->step;
        if (a->frame >= a->length - 0.1f) {
            BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_END);
            done = 1;
            a->frame = a->length;
        }
    }
    chr->prevStall = chr->stall;
    if (a->prevFrame == a->frame) {
        chr->stall++;
    } else {
        chr->stall = 0;
    }
    return done;
}

/* BtlAnim_Advance, and at the end requests the animation `next`. */
s32 BtlAnim_AdvanceThen(BtlStatChr *chr, s32 next, f32 blend, s32 flags) {
    s32 done = BtlAnim_Advance(chr, flags);

    if (done) {
        BtlAnim_Request(chr, next, blend);
    }
    return done;
}

/* Advances a looping animation one step (flags & 1: not while blending); at the end goes back to frame 0. */
void BtlAnim_AdvanceLoop(BtlStatChr *chr, s32 flags) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    a->blend -= a->blendStep;
    if (a->blend < 0.0f) {
        a->blend = 0.0f;
        BtlChar_ClearFlag(chr, BTL_ANIM_FLAG_BLENDING);
    } else {
        BtlChar_SetHeldFlag(chr, BTL_ANIM_FLAG_BLENDING);
    }
    a->prevFrame = a->frame;
    if (!(flags & 1) || a->blend == 0.0f) {
        a->frame += a->step;
        if (a->frame >= a->length - 0.1f) {
            a->frame = 0.0f;
        }
    }
    chr->prevStall = chr->stall;
    if (a->prevFrame == a->frame) {
        chr->stall++;
    } else {
        chr->stall = 0;
    }
}

/* 1 when the last advance passed the point `ratio` (0..1) of the animation, wrap-around included. */
s32 BtlAnim_PassedRatio(BtlStatChr *chr, f32 ratio) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;
    f32 t = a->length * ratio;

    if (a->frame >= a->prevFrame) {
        if (a->prevFrame <= t && t < a->frame) {
            return 1;
        }
    } else {
        if (a->prevFrame <= t || t < a->frame) {
            return 1;
        }
    }
    return 0;
}

/* 1 when the last advance passed frame `frame`, wrap-around included. */
s32 BtlAnim_PassedFrame(BtlStatChr *chr, f32 frame) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    if (a->frame >= a->prevFrame) {
        if (a->prevFrame <= frame && frame < a->frame) {
            return 1;
        }
    } else {
        if (a->prevFrame <= frame || frame < a->frame) {
            return 1;
        }
    }
    return 0;
}

/* BtlAnim_InFrameRange with the bounds given as ratios of the length. No caller. */
s32 BtlAnim_InRatioRange(BtlStatChr *chr, f32 lo, f32 hi) {
    f32 len = BtlChar_GetObj(chr)->anim.length;

    return BtlAnim_InFrameRange(chr, len * lo, len * hi);
}

/* 1 when the frames covered by the last advance overlap lo..hi, wrap-around included. */
s32 BtlAnim_InFrameRange(BtlStatChr *chr, f32 lo, f32 hi) {
    BtlAnimObj *a = &BtlChar_GetObj(chr)->anim;

    if (a->frame >= a->prevFrame) {
        if (a->prevFrame <= hi && lo <= a->frame) {
            return 1;
        }
    } else {
        if (a->prevFrame <= hi || lo <= a->frame) {
            return 1;
        }
    }
    return 0;
}

/* 1 on the frame an animation was started (flag 0x2B). */
s32 BtlAnim_IsNew(BtlStatChr *chr) {
    return BtlChar_TestFlag(chr, BTL_ANIM_FLAG_NEW) != 0;
}

/* Flag 0x2B in the second pair of flag arrays (the frame before, inferred). */
s32 BtlAnim_WasNew(BtlStatChr *chr) {
    return BtlChar_TestPrevFlag(chr, BTL_ANIM_FLAG_NEW) != 0;
}
