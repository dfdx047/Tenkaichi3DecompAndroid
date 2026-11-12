#include "common.h"
/*
 * Flash-like movie player, clip-list setters (0x10EC18..0x10FB40). Continuation of sys/gfxm_c.c: each function is
 * the body behind one Flash_Clip* wrapper there (the wrapper tests ref->index >= 0 and tail-calls with
 * flash->clips). Every setter applies to the instance ref->index and to the next ref->more instances whose name
 * (strcmp) equals the first one's; the search stops at the end of the list.
 * Proper file name: the same source as gfxm_c.c (sys/flash.c).
 */
#include "sys/gfxm_d.h"

extern int strcmp(const char *a, const char *b);
extern void FlashTl_GotoLabel(FlashDTl *tl, char *label, s32 fromStart); /* 0x10BEB8 */
extern void FlashTl_Advance(FlashDTl *tl, s32 speed, s32 a, s32 freeRun); /* 0x10BC28 */

/* Jumps a clip (and every further instance of the same name) to a label of its timeline, and runs the timeline
   at once when the jump moved the play head. */
void FlashClipList_GotoLabel(FlashDClipList *list, FlashDRef *ref, char *label) {
    FlashDClip *clip = &list->items[ref->index];
    FlashDClip *p;
    s32 n;
    u32 i;

    FlashTl_GotoLabel(&clip->tl, label, 1);
    if (clip->tl.state & FLASHD_TL_GOTO) {
        if (clip->flags & FLASHD_CLIP_FREE_RUN) {
            FlashTl_Advance(&clip->tl, clip->tl.owner->speed, 1, 1);
        } else {
            FlashTl_Advance(&clip->tl, clip->tl.owner->speed, 1, 0);
        }
    }
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            p = &clip[i];
            if (strcmp(p->name, clip->name) == 0) {
                FlashTl_GotoLabel(&p->tl, label, 1);
                if (p->tl.state & FLASHD_TL_GOTO) {
                    if (p->flags & FLASHD_CLIP_FREE_RUN) {
                        FlashTl_Advance(&p->tl, p->tl.owner->speed, 1, 1);
                    } else {
                        FlashTl_Advance(&p->tl, p->tl.owner->speed, 1, 0);
                    }
                }
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Sets the callback run before the clip is drawn, and its argument. */
void FlashClipList_SetPreDraw(FlashDClipList *list, FlashDRef *ref, s32 fn, s32 arg) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->preDraw = fn;
    clip->preArg = arg;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].preDraw = fn;
                clip[i].preArg = arg;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Sets the callback run after the clip is drawn, and its argument. */
void FlashClipList_SetPostDraw(FlashDClipList *list, FlashDRef *ref, s32 fn, s32 arg) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->postDraw = fn;
    clip->postArg = arg;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].postDraw = fn;
                clip[i].postArg = arg;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Sets the callback run after the clip's children are drawn, and its argument. */
void FlashClipList_SetDrawOver(FlashDClipList *list, FlashDRef *ref, s32 fn, s32 arg) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->drawOver = fn;
    clip->overArg = arg;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].drawOver = fn;
                clip[i].overArg = arg;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Sets or clears bits of the clip flags (FLASH_CLIP_*). */
void FlashClipList_SetFlags(FlashDClipList *list, FlashDRef *ref, s32 mask, u8 on) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    if (on) {
        clip->flags |= mask;
    } else {
        clip->flags &= ~mask;
    }
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                if (on) {
                    clip[i].flags |= mask;
                } else {
                    clip[i].flags &= ~mask;
                }
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Sets or clears bits of the override flags (FLASH_OV_*). */
void FlashClipList_SetOverride(FlashDClipList *list, FlashDRef *ref, s32 mask, u8 on) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    if (on) {
        clip->ovFlags |= mask;
    } else {
        clip->ovFlags &= ~mask;
    }
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                if (on) {
                    clip[i].ovFlags |= mask;
                } else {
                    clip[i].ovFlags &= ~mask;
                }
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Gives a clip a position offset in pixels and enables it. */
void FlashClipList_SetOffset(FlashDClipList *list, FlashDRef *ref, s32 x, s32 y) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;
    f32 fx = x;
    f32 fy = y;

    clip->ovX = fx;
    clip->ovY = fy;
    clip->ovFlags |= FLASHD_OV_POS;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].ovX = fx;
                clip[i].ovY = fy;
                clip[i].ovFlags |= FLASHD_OV_POS;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Gives a clip a scale (the wrapper refuses negative values) and enables it. */
void FlashClipList_SetScale(FlashDClipList *list, FlashDRef *ref, f32 x, f32 y) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->ovScaleX = x;
    clip->ovScaleY = y;
    clip->ovFlags |= FLASHD_OV_SCALE;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].ovScaleX = x;
                clip[i].ovScaleY = y;
                clip[i].ovFlags |= FLASHD_OV_SCALE;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Gives a clip an alpha factor, clamped to 0..1 (used by FlashClipList_GetAlpha), and enables it. */
void FlashClipList_SetAlpha(FlashDClipList *list, FlashDRef *ref, f32 alpha) {
    FlashDClip *clip = &list->items[ref->index];
    FlashDClip *p;
    s32 n;
    u32 i;

    if (alpha > 1.0f) {
        clip->ovAlpha = 1.0f;
    } else if (alpha < 0.0f) {
        clip->ovAlpha = 0.0f;
    } else {
        clip->ovAlpha = alpha;
    }
    clip->ovFlags |= FLASHD_OV_ALPHA;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            p = &clip[i];
            if (strcmp(p->name, clip->name) == 0) {
                if (alpha > 1.0f) {
                    p->ovAlpha = 1.0f;
                } else if (alpha < 0.0f) {
                    p->ovAlpha = 0.0f;
                } else {
                    p->ovAlpha = alpha;
                }
                p->ovFlags |= FLASHD_OV_ALPHA;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Gives a clip a colour factor (multiplies r, g, b), clamped to 0..1, and enables it. */
void FlashClipList_SetColor(FlashDClipList *list, FlashDRef *ref, f32 color) {
    FlashDClip *clip = &list->items[ref->index];
    FlashDClip *p;
    s32 n;
    u32 i;

    if (color > 1.0f) {
        clip->ovColor = 1.0f;
    } else if (color < 0.0f) {
        clip->ovColor = 0.0f;
    } else {
        clip->ovColor = color;
    }
    clip->ovFlags |= FLASHD_OV_COLOR;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            p = &clip[i];
            if (strcmp(p->name, clip->name) == 0) {
                if (color > 1.0f) {
                    p->ovColor = 1.0f;
                } else if (color < 0.0f) {
                    p->ovColor = 0.0f;
                } else {
                    p->ovColor = color;
                }
                p->ovFlags |= FLASHD_OV_COLOR;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Gives a clip a texture index and enables it. */
void FlashClipList_SetTex(FlashDClipList *list, FlashDRef *ref, s32 user) {
    FlashDClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->ovTex = user;
    clip->ovFlags |= FLASHD_OV_TEX;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].ovTex = user;
                clip[i].ovFlags |= FLASHD_OV_TEX;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Gives a clip a texture rectangle (four texel coordinates) and enables it. */
void FlashClipList_SetUv(FlashDClipList *list, FlashDRef *ref, FlashDUv *uv) {
    FlashDClip *clip = &list->items[ref->index];
    FlashDClip *p;
    s32 n;
    u32 i;

    clip->ovUv.u0 = uv->u0;
    clip->ovUv.v0 = uv->v0;
    clip->ovUv.u1 = uv->u1;
    clip->ovUv.v1 = uv->v1;
    clip->ovFlags |= FLASHD_OV_UV;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            p = &clip[i];
            if (strcmp(p->name, clip->name) == 0) {
                p->ovUv.u0 = uv->u0;
                p->ovUv.v0 = uv->v0;
                p->ovUv.u1 = uv->u1;
                p->ovUv.v1 = uv->v1;
                p->ovFlags |= FLASHD_OV_UV;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Returns a clip's screen position in pixels: animated position plus the caller's offset. */
void FlashClipList_GetPos(FlashDClipList *list, FlashDRef *ref, s32 *x, s32 *y) {
    FlashDClip *clip = &list->items[ref->index];

    *x = clip->x;
    *y = clip->y;
    if (clip->ovFlags & FLASHD_OV_POS) {
        *x += (s32)clip->ovX;
        *y += (s32)clip->ovY;
    }
}

/* Returns a clip's alpha: colour-transform alpha (0..255) times the caller's factor, over 128. */
f32 FlashClipList_GetAlpha(FlashDClipList *list, FlashDRef *ref) {
    FlashDClip *clip = &list->items[ref->index];
    s32 a = (s32)(clip->alphaMul * 128.0f) + clip->alphaAdd;
    f32 f;

    f = (a < 0) ? 0 : ((a > 255) ? 255 : a);
    if (clip->ovFlags & FLASHD_OV_ALPHA) {
        f = f * clip->ovAlpha * 0.0078125f;
    } else {
        f = f * 0.0078125f;
    }
    return f;
}
