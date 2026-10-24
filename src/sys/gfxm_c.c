#include "common.h"
/*
 * The Flash-like movie player (0x10AD58..0x10EC18). The data is a converted SWF: see include/sys/gfxm_c.h and
 * docs. Continues src/sys/gfxm_b_c.c (the low-level readers), very likely the same original object.
 * Proper name: flash.c.
 */
#include "sys/gfxm_c.h"
#include "sys/heap.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/gfxm_b_b.h"

extern void *memcpy(void *dst, const void *src, u32 size);
extern void *memset(void *dst, s32 c, u32 n);
extern s32 strcmp(const char *a, const char *b);
extern u32 strlen(const char *s);
extern s32 atoi(const char *s);

void FlashTl_TagPlace(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagPlace2(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagRemove(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagRemove2(FlashTl *tl, u8 *data, s32 *ofs);
void FlashTl_TagActions(FlashTl *tl, u8 *data, s32 *ofs);
void FlashSlot_SetShape(FlashSlot *slot, u16 id, FlashMtx *mtx, FlashCxform *cx, Flash *flash);
void FlashSlot_SetClip(FlashSlot *slot, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx, Flash *flash);
void FlashSlot_Clear(FlashSlot *slot);
void FlashSlot_Move(FlashSlot *slot, FlashMtx *mtx, FlashCxform *cx);
void FlashShape_Free(FlashShape *shape);
void FlashClip_Reset(FlashClip *clip, s32 arg);

/* Sets a colour transform to the identity. */
void Flash_CxformIdentity(FlashCxform *cx) {
    s32 i;

    for (i = 0; i < 4; i++) {
        cx->mul[i] = 1.0f;
        cx->add[i] = 0;
    }
}

/* Reads a bit-packed colour transform at data + *ofs: a flag byte (bits 0..3 multipliers present, 4..7 offsets
   present), a 4-bit field width, then the fields (multipliers in 8.8). Leaves *ofs on the next whole byte. */
void Flash_ReadCxform(u8 *data, FlashCxform *cx, s32 *ofs) {
    u32 bitPos = 0;
    u8 flags;
    u8 bits;
    u32 i;

    Flash_CxformIdentity(cx);
    flags = data[*ofs];
    *ofs += 1;
    if (flags != 0) {
        bits = Flash_ReadBits(data + *ofs, &bitPos, 4);
        if (flags & 0xF) {
            for (i = 0; i < 4; i++) {
                if ((flags >> i) & 1) {
                    cx->mul[i] = (s16)Flash_ReadSBits(data + *ofs, &bitPos, bits) * (1.0f / 256.0f);
                }
            }
        }
        if (flags & 0xF0) {
            for (i = 0; i < 4; i++) {
                if (flags & (0x10U << i)) {
                    cx->add[i] = Flash_ReadSBits(data + *ofs, &bitPos, bits);
                }
            }
        }
    }
    *ofs += bitPos >> 3;
    if (bitPos & 7) {
        *ofs += 1;
    }
}

/* Runs the tag list of one frame block (name, u16 size, then tags: u16 code + data, ended by code 1 = ShowFrame).
   A block of size 0 is the end of the timeline: stops and steps back onto the last frame. */
void FlashTl_RunFrame(FlashTl *tl, u8 *frame) {
    u16 size;
    u16 code;
    s32 ofs;

    ofs = 0;
    size = 0;
    code = 0;
    if (frame == NULL) {
        goto end;
    }
    while (frame[ofs++] != 0) {
    }
    memcpy(&size, frame + ofs, 2);
    ofs += 2;
    if (size == 0) {
    end:
        tl->state &= ~FLASH_TL_PLAY;
        if (tl->frame != 0) {
            tl->frame--;
        }
        return;
    }
    do {
        memcpy(&code, frame + ofs, 2);
        ofs += 2;
        switch (code) {
        case 4:
            FlashTl_TagPlace(tl, frame, &ofs);
            break;
        case 26:
            FlashTl_TagPlace2(tl, frame, &ofs);
            break;
        case 5:
            FlashTl_TagRemove(tl, frame, &ofs);
            break;
        case 28:
            FlashTl_TagRemove2(tl, frame, &ofs);
            break;
        case 12:
            FlashTl_TagActions(tl, frame, &ofs);
            break;
        case 1:
            break;
        }
    } while (code != 1);
}

/* Tag 4 (PlaceObject): u16 depth, u8 kind (4 shape, 6 sprite), u16 id, matrix, and for a sprite a colour transform. */
void FlashTl_TagPlace(FlashTl *tl, u8 *data, s32 *ofs) {
    FlashMtx mtx;
    FlashCxform cx;
    u16 depth;
    u16 id;
    u8 kind;

    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    kind = data[*ofs];
    *ofs += 1;
    memcpy(&id, data + *ofs, 2);
    *ofs += 2;
    switch (kind) {
    case 4:
        Flash_ReadMtx(data, &mtx, ofs);
        Flash_CxformIdentity(&cx);
        FlashSlot_SetShape(&tl->slots[depth - 1], id, &mtx, &cx, tl->owner);
        break;
    case 6:
        Flash_ReadMtx(data, &mtx, ofs);
        Flash_ReadCxform(data, &cx, ofs);
        FlashSlot_SetClip(&tl->slots[depth - 1], id, NULL, &mtx, &cx, tl->owner);
        break;
    }
}

/* Tag 26 (PlaceObject2): u8 flags (2 new object, 4 matrix, 8 colour transform, 0x20 name), u16 depth; a new object
   adds u8 kind and u16 id. Without flag 2 the object already at that depth is moved. */
void FlashTl_TagPlace2(FlashTl *tl, u8 *data, s32 *ofs) {
    FlashMtx mtx;
    FlashCxform cx;
    char name[0x40];
    u16 depth;
    u16 id;
    u8 flags;
    u8 kind;
    FlashMtx *pm;
    FlashCxform *pc;

    Flash_MtxIdentity(&mtx);
    Flash_CxformIdentity(&cx);
    flags = data[*ofs];
    *ofs += 1;
    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    if (flags & 2) {
        kind = data[*ofs];
        *ofs += 1;
        memcpy(&id, data + *ofs, 2);
        *ofs += 2;
        if (flags & 4) {
            Flash_ReadMtx(data, &mtx, ofs);
        }
        if (flags & 8) {
            Flash_ReadCxform(data, &cx, ofs);
        }
        if (flags & 0x20) {
            memset(name, 0, sizeof(name));
            Flash_ReadString(name, data, ofs);
        }
        switch (kind) {
        case 4:
            FlashSlot_SetShape(&tl->slots[depth - 1], id, &mtx, &cx, tl->owner);
            break;
        case 6:
            FlashSlot_SetClip(&tl->slots[depth - 1], id, name, &mtx, &cx, tl->owner);
            break;
        }
    } else {
        pm = NULL;
        pc = NULL;
        if (flags & 4) {
            Flash_ReadMtx(data, &mtx, ofs);
            pm = &mtx;
        }
        if (flags & 8) {
            Flash_ReadCxform(data, &cx, ofs);
            pc = &cx;
        }
        FlashSlot_Move(&tl->slots[depth - 1], pm, pc);
    }
}

/* Tag 5 (RemoveObject): u16 depth, u8 kind, u16 id: frees the shape or resets the sprite at that depth. */
void FlashTl_TagRemove(FlashTl *tl, u8 *data, s32 *ofs) {
    u16 depth;
    u16 id;
    u8 kind;
    FlashSlot *slot;

    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    kind = data[*ofs];
    *ofs += 1;
    memcpy(&id, data + *ofs, 2);
    *ofs += 2;
    slot = &tl->slots[depth - 1];
    switch (kind) {
    case 4:
        if (slot->shape != NULL) {
            FlashShape_Free(slot->shape);
            slot->shape = NULL;
        }
        break;
    case 6:
        if (slot->clip != NULL) {
            FlashClip_Reset(slot->clip, 1);
            slot->clip = NULL;
        }
        break;
    }
}

/* Tag 28 (RemoveObject2): u16 depth: empties that depth. */
void FlashTl_TagRemove2(FlashTl *tl, u8 *data, s32 *ofs) {
    u16 depth;

    memcpy(&depth, data + *ofs, 2);
    *ofs += 2;
    FlashSlot_Clear(&tl->slots[depth - 1]);
}

/* Action list (tag 12) while a jump replays earlier frames: every action is skipped except GetURL "pad". */
void FlashTl_SkipActions(FlashTl *tl, u8 *data, s32 *ofs) {
    char url[0x40];
    char arg[0x40];
    char name[0x40];
    char label[0x100];
    u8 code;

    do {
        code = data[*ofs];
        *ofs += 1;
        switch (code) {
        case 0x81:
            *ofs += 2;
            break;
        case 0x83:
            memset(url, 0, sizeof(url));
            memset(arg, 0, sizeof(arg));
            Flash_ReadString(url, data, ofs);
            Flash_ReadString(arg, data, ofs);
            if (strcmp(url, "pad") == 0) {
                if (strcmp(arg, "true") == 0) {
                    tl->owner->flags |= FLASH_PAD;
                } else if (strcmp(arg, "false") == 0) {
                    tl->owner->flags &= ~FLASH_PAD;
                }
            }
            break;
        case 0x8A:
            *ofs += 3;
            break;
        case 0x8B:
            memset(name, 0, sizeof(name));
            Flash_ReadString(name, data, ofs);
            break;
        case 0x8C:
            memset(label, 0, 0xFF);
            Flash_ReadString(label, data, ofs);
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            break;
        }
    } while (code != 0);
}

FlashClip *FlashClipList_FindByName(FlashClipList *list, char *name);

/* Tag 12 (DoAction): runs SWF actions until the end code 0. 0x81 GotoFrame, 4 NextFrame, 5 PrevFrame, 6 Play,
   7 Stop, 0x83 GetURL (the game's command channel), 0x8A WaitForFrame (skipped), 0x8B SetTarget, 0x8C GotoLabel. */
void FlashTl_TagActions(FlashTl *tl, u8 *data, s32 *ofs) {
    FlashTl *cur;
    u8 code;

    if (tl->state & FLASH_TL_SEEK) {
        FlashTl_SkipActions(tl, data, ofs);
        return;
    }
    cur = tl;
    do {
        code = data[*ofs];
        *ofs += 1;
        switch (code) {
        case 4:
            cur->frame = cur->frame + 1;
            cur->state |= FLASH_TL_HOLD;
            break;
        case 5:
            if (cur->frame != 0) {
                cur->frame--;
            }
            cur->state |= FLASH_TL_HOLD;
            break;
        case 6:
            cur->state |= FLASH_TL_PLAY;
            break;
        case 7:
            cur->state &= ~FLASH_TL_PLAY;
            break;
        case 0x81: {
            u16 frame;

            memcpy(&frame, data + *ofs, 2);
            *ofs += 2;
            cur->frame = frame - 1;
            cur->state |= FLASH_TL_GOTO;
            break;
        }
        case 0x83: {
            char url[0x40];
            char arg[0x40];

            memset(url, 0, sizeof(url));
            memset(arg, 0, sizeof(arg));
            Flash_ReadString(url, data, ofs);
            Flash_ReadString(arg, data, ofs);
            if (strcmp(url, "pad") == 0) {
                if (strcmp(arg, "true") == 0) {
                    tl->owner->flags |= FLASH_PAD;
                } else if (strcmp(arg, "false") == 0) {
                    tl->owner->flags &= ~FLASH_PAD;
                }
            } else if (strcmp(url, "trig") == 0) {
                tl->owner->trig |= 1 << atoi(arg);
            } else if (strcmp(url, "se") == 0) {
                tl->owner->se |= 1 << atoi(arg);
            } else if (strcmp(url, "trigger") == 0) {
                if (strcmp(arg, "end") == 0) {
                    tl->owner->flags |= FLASH_END;
                }
            }
            break;
        }
        case 0x8A:
            *ofs += 3;
            break;
        case 0x8B: {
            char name[0x40];
            FlashClip *clip;

            memset(name, 0, sizeof(name));
            Flash_ReadString(name, data, ofs);
            if (name[0] == 0) {
                cur = tl;
            } else {
                clip = FlashClipList_FindByName(tl->owner->clips, name);
                cur = clip != NULL ? &clip->tl : tl;
            }
            break;
        }
        case 0x8C: {
            char label[0x100];
            s32 index;

            memset(label, 0, 0xFF);
            Flash_ReadString(label, data, ofs);
            index = Flash_FindName(cur->frames, label);
            if (index >= 0) {
                cur->frame = index;
                cur->state |= FLASH_TL_GOTO;
            }
            break;
        }
        }
    } while (code != 0);
}

/* Sets a timeline up on a frame record (root: tag 7, sprite: a record of tag 6) and allocates its depth slots. */
void FlashTl_Init(FlashTl *tl, u8 *record, Flash *owner) {
    FlashTag hdr;

    memset(&hdr, 0, sizeof(hdr));
    memset(tl, 0, sizeof(FlashTl));
    memcpy(&hdr, record, sizeof(hdr));
    record += 8;
    tl->owner = owner;
    tl->depths = hdr.count;
    tl->frames = record;
    if (tl->slots == NULL) {
        tl->slots = Heap_Alloc(tl->depths * 8, 0x20, 0, 2);
        memset(tl->slots, 0, tl->depths * 8);
    }
}

/* Frees the depth slots and clears the timeline. */
void FlashTl_Term(FlashTl *tl) {
    if (tl->slots != NULL) {
        Heap_Free(tl->slots);
        tl->slots = NULL;
    }
    memset(tl, 0, sizeof(FlashTl));
}

/* Back to frame 0, stopped, every depth emptied. The second argument is not used. */
void FlashTl_Rewind(FlashTl *tl, s32 unused) {
    s32 i;

    tl->prev = 0;
    tl->frame = 0;
    tl->unk14 = 0;
    tl->state = 0;
    for (i = 0; i < tl->depths; i++) {
        FlashSlot_Clear(&tl->slots[i]);
    }
}

void FlashSlot_Advance(FlashSlot *slot, u32 count);

/* Runs `count` ticks: a pending jump first replays the frames up to the target (from 0, or from the frame the
   jump left when FLASH_TL_KEEP), then the current frame's tags run and the play head steps. `children` ticks the
   sprites placed on it after each frame (when 0 they are ticked once at the end); `force` does so even when this
   timeline is stopped. */
void FlashTl_Advance(FlashTl *tl, u32 count, u8 children, u8 force) {
    u32 n;
    u32 i;
    u32 target;
    u32 start;

    for (n = 0; n < count; n++) {
        if (tl->state & (FLASH_TL_PLAY | FLASH_TL_GOTO)) {
            if (tl->state & FLASH_TL_GOTO) {
                target = tl->frame;
                if (tl->state & FLASH_TL_KEEP) {
                    start = tl->prev;
                } else {
                    FlashTl_Rewind(tl, 1);
                    start = 0;
                }
                tl->state |= FLASH_TL_PLAY | FLASH_TL_SEEK;
                for (i = start; i < target; i++) {
                    FlashTl_RunFrame(tl, Flash_SkipNamed(tl->frames, i));
                }
                tl->frame = target;
                tl->state &= ~(FLASH_TL_GOTO | FLASH_TL_SEEK | FLASH_TL_KEEP);
            }
            FlashTl_RunFrame(tl, Flash_SkipNamed(tl->frames, tl->frame));
            if (!(tl->state & FLASH_TL_GOTO)) {
                if (!(tl->state & FLASH_TL_HOLD)) {
                    if ((u8)(tl->state & FLASH_TL_PLAY)) {
                        tl->frame++;
                    }
                }
            }
            if (children) {
                for (i = 0; i < tl->depths; i++) {
                    FlashSlot_Advance(&tl->slots[i], count);
                }
            }
        } else if (children && force) {
            for (i = 0; i < tl->depths; i++) {
                FlashSlot_Advance(&tl->slots[i], count);
            }
        }
    }
    if (!children) {
        for (i = 0; i < tl->depths; i++) {
            FlashSlot_Advance(&tl->slots[i], count);
        }
    }
}

void FlashSlot_Draw(FlashSlot *slot, FlashProp *prop, Flash *flash);

/* Draws every depth, lowest first. `prop` is the drawing state of the sprite this timeline belongs to (NULL: root). */
void FlashTl_Draw(FlashTl *tl, FlashProp *prop) {
    s32 i;

    for (i = 0; i < tl->depths; i++) {
        FlashSlot_Draw(&tl->slots[i], prop, tl->owner);
    }
}

/* Jumps to a frame label. `restart` rebuilds the display list from frame 0, else from the current frame. */
void FlashTl_GotoLabel(FlashTl *tl, char *label, u8 restart) {
    s32 index = Flash_FindName(tl->frames, label);

    if (index >= 0) {
        tl->prev = tl->frame;
        tl->frame = index;
        tl->state |= FLASH_TL_GOTO;
        if (!restart) {
            tl->state |= FLASH_TL_KEEP;
        }
    }
}

/* Moves the play head by `step` frames and plays; any step but +1 goes through the jump path. */
void FlashTl_StepFrames(FlashTl *tl, s32 step) {
    s32 frame = tl->frame + step;

    if (frame >= 0 && Flash_SkipNamed(tl->frames, frame) != NULL) {
        tl->frame = frame;
        tl->state |= FLASH_TL_PLAY;
        if (step != 1) {
            tl->state |= FLASH_TL_GOTO;
        }
    }
}

FlashShape *FlashShape_Create(FlashShapePool *pool, u16 id, FlashMtx *mtx, FlashCxform *cx);
FlashClip *FlashClip_Start(FlashClipList *list, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx);
void FlashShape_Set(FlashShape *shape, FlashMtx *mtx, FlashCxform *cx);
void FlashClip_Set(FlashClip *clip, FlashMtx *mtx, FlashCxform *cx);

/* Puts a shape on a depth, replacing the shape that was there. */
void FlashSlot_SetShape(FlashSlot *slot, u16 id, FlashMtx *mtx, FlashCxform *cx, Flash *flash) {
    if (slot->shape != NULL) {
        FlashShape_Free(slot->shape);
        slot->shape = NULL;
    }
    slot->shape = FlashShape_Create(flash->shapes, id, mtx, cx);
}

/* Puts a sprite on a depth, replacing the sprite that was there. */
void FlashSlot_SetClip(FlashSlot *slot, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx, Flash *flash) {
    if (slot->clip != NULL) {
        FlashClip_Reset(slot->clip, 1);
        slot->clip = NULL;
    }
    slot->clip = FlashClip_Start(flash->clips, id, name, mtx, cx);
}

/* Empties a depth: frees the shape, marks the sprite unused (its state is kept). */
void FlashSlot_Clear(FlashSlot *slot) {
    if (slot->shape != NULL) {
        FlashShape_Free(slot->shape);
        slot->shape = NULL;
    }
    if (slot->clip != NULL) {
        slot->clip->flags &= ~FLASH_CLIP_USED;
        slot->clip = NULL;
    }
}

/* Gives what is on a depth a new matrix and / or colour transform (NULL: keep). */
void FlashSlot_Move(FlashSlot *slot, FlashMtx *mtx, FlashCxform *cx) {
    if (slot->shape != NULL) {
        FlashShape_Set(slot->shape, mtx, cx);
    }
    if (slot->clip != NULL) {
        FlashClip_Set(slot->clip, mtx, cx);
    }
}

/* Ticks the sprite on a depth. */
void FlashSlot_Advance(FlashSlot *slot, u32 count) {
    FlashClip *clip = slot->clip;

    if (clip != NULL) {
        if (clip->flags & FLASH_CLIP_FREE_RUN) {
            FlashTl_Advance(&clip->tl, count, 1, 1);
        } else {
            FlashTl_Advance(&clip->tl, count, 1, 0);
        }
    }
}

void FlashShape_Draw(FlashShape *shape, FlashProp *prop, Flash *flash);

static inline s32 Flash_Clamp255(s32 v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static inline s16 Flash_ClampAdd(s32 v) {
    if (v >= 0) {
        if (v > 255) {
            v = 255;
        }
        return v;
    }
    return 0;
}

/* Draws what is on a depth: the shape, then the sprite. The sprite's drawing state is combined with its
   parent's (matrix product, colour transforms multiplied and added, the parent's overrides applied). */
void FlashSlot_Draw(FlashSlot *slot, FlashProp *parent, Flash *flash) {
    FlashProp prop;
    FlashMtx m;
    FlashClip *clip = slot->clip;
    s32 i;
    s32 j;

    if (slot->shape != NULL) {
        FlashShape_Draw(slot->shape, parent, flash);
    }
    if (clip != NULL && (clip->flags & FLASH_CLIP_VISIBLE)) {
        prop = clip->prop;
        if (parent != NULL) {
            prop.flags |= parent->flags & (FLASH_OV_MASK | FLASH_OV_MASK_SET);
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    m.m[i * 3 + j] = prop.mtx.m[j] * parent->mtx.m[i * 3] + prop.mtx.m[3 + j] * parent->mtx.m[i * 3 + 1] +
                                     prop.mtx.m[6 + j] * parent->mtx.m[i * 3 + 2];
                }
            }
            prop.mtx = m;
            if (parent->flags & FLASH_OV_POS) {
                prop.mtx.m[2] += parent->x;
                prop.mtx.m[5] += parent->y;
            }
            for (i = 0; i < 4; i++) {
                prop.cx.mul[i] *= parent->cx.mul[i];
                if (i < 3) {
                    if (parent->flags & FLASH_OV_COLOR) {
                        prop.cx.mul[i] *= parent->color;
                    }
                } else {
                    if (parent->flags & FLASH_OV_ALPHA) {
                        prop.cx.mul[i] *= parent->alpha;
                    }
                }
                if (prop.cx.mul[i] > 1.0f) {
                    prop.cx.mul[i] = 1.0f;
                } else if (prop.cx.mul[i] < 0.0f) {
                    prop.cx.mul[i] = 0.0f;
                }
                /* The statement expression makes the compiler take the destination's address before the clamp. */
                prop.cx.add[i] = ({ Flash_ClampAdd(prop.cx.add[i] + parent->cx.add[i]); });
            }
        }
        if (clip->preDraw != NULL) {
            clip->preDraw(clip->preArg);
        }
        FlashTl_Draw(&clip->tl, &prop);
        if (clip->drawOver != NULL) {
            clip->drawOver(clip->overArg, &prop);
        }
        if (clip->postDraw != NULL) {
            clip->postDraw(clip->postArg);
        }
    }
}

/* Returns its argument: every screen Y goes through it (a stripped vertical adjustment). */
f32 FlashDraw_AdjustY(f32 y) {
    return y;
}

/* Queues the upload of texture `index` of an image's entry table to block `tbp` and its CLUT to the blocks behind
   it (+0x40), and returns the TEX0 value. Entry `index` holds the CLUT; when it has no pixels of its own the
   pixels are entry 0's (several palettes for one picture). */
u64 FlashDraw_UploadTex(FlashTex *tex, s32 index, s32 tbp) {
    TexBltPacket pkt = { { 0x10000002, 0, 0, 0x50000002 }, { 0x1000000000008001, 0xE }, 0, 0x50 };
    TexEntry *clut = (TexEntry *)((index << 6) + (u32)tex);
    TexEntry *pix;
    s64 base = tbp;
    u64 tex0;

    if (clut->pixSize == 0) {
        index = 0;
    }
    pix = (TexEntry *)((index << 6) + (u32)tex);
    pkt.bitbltbuf = (base << 32) | ((u64)pix->pixBlt << 48);
    Dma_AddData(&pkt, sizeof(pkt));
    Dma_AddRef(pix->pix, pix->pixSize);
    tbp += pix->tbpStep;
    tbp += 0x40;
    pkt.bitbltbuf = ((s64)tbp << 32) | ((u64)clut->clutBlt << 48);
    Dma_AddData(&pkt, sizeof(pkt));
    Dma_AddRef(clut->clut, clut->clutSize);
    tex0 = pix->tex0;
    tex0 |= (s64)tbp << 37;
    tex0 |= base;
    tex0 |= 1LL << 34;
    return tex0;
}

/* XYZ2 of a screen point: 12.4 fixed point, window origin at (1792, 1824), Z 0. */
#define FLASH_XY(x, y) ((s64)((s32)((x) * 16.0f) + ox) | ((s64)((s32)(FlashDraw_AdjustY(y) * 16.0f) + oy) << 16))
#define FLASH_RGBA(q) ((s64)(q)->rgba[0] | ((s64)(q)->rgba[1] << 8) | ((s64)(q)->rgba[2] << 16) | ((s64)(q)->rgba[3] << 24))

/* Writes a textured triangle strip of four vertices (REGLIST: RGBAQ, TEX0, PRIM, then UV / XYZ2 pairs) and
   returns the end of the packet. The texture rectangle is shrunk by one texel on each side unless flipped. */
u64 *FlashDraw_PutSprite(u64 *p, FlashQuad *q, u64 tex0) {
    s32 uv[4];
    s32 unused[4]; /* the original frame has 0x10 more bytes of locals */
    s32 oy;
    s32 ox;

    p[0] = 0xC400000000008001;
    p[1] = 0x5353535306E1;
    p += 2;
    p[0] = FLASH_RGBA(q);
    p[1] = 0;
    p += 2;
    p[0] = tex0;
    p[1] = 0x154;
    p += 2;
    uv[0] = q->uv.u0 + 1;
    uv[2] = q->uv.u1 - 1;
    uv[1] = q->uv.v0 + 1;
    uv[3] = q->uv.v1 - 1;
    if (q->flags & FLASH_QUAD_FLIPX) {
        uv[0] = q->uv.u1;
        uv[2] = q->uv.u0;
    }
    if (q->flags & FLASH_QUAD_FLIPY) {
        uv[1] = q->uv.v1;
        uv[3] = q->uv.v0;
    }
    oy = 0x7200;
    ox = 0x7000;
    p[0] = (uv[0] << 4) | ((s64)(uv[1] << 4) << 16);
    p[1] = FLASH_XY(q->pt[0][0], q->pt[0][1]);
    p += 2;
    p[0] = (uv[2] << 4) | ((s64)(uv[1] << 4) << 16);
    p[1] = FLASH_XY(q->pt[1][0], q->pt[1][1]);
    p += 2;
    p[0] = (uv[0] << 4) | ((s64)(uv[3] << 4) << 16);
    p[1] = FLASH_XY(q->pt[2][0], q->pt[2][1]);
    p += 2;
    p[0] = (uv[2] << 4) | ((s64)(uv[3] << 4) << 16);
    p[1] = FLASH_XY(q->pt[3][0], q->pt[3][1]);
    p += 2;
    return p;
}

/* Draws an untextured quad in its colour. */
void FlashDraw_FlatQuad(FlashQuad *q) {
    u64 *p = Dma_BeginDirect();
    s32 oy;
    s32 ox;

    p[0] = 0x1000000000008004;
    p[1] = 0xE;
    p += 2;
    p[0] = GFX_FRAME_REG();
    p[1] = 0x4C;
    p += 2;
    p[0] = 1;
    p[1] = 0x46;
    p += 2;
    if (q->flags & FLASH_QUAD_ADD) {
        p[0] = 0x48;
    } else if (q->flags & FLASH_QUAD_SUB) {
        p[0] = 0x42;
    } else {
        p[0] = 0x44;
    }
    p[1] = 0x42;
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47;
    p += 2;
    p[0] = 0x6400000000008001;
    p[1] = 0x555510;
    p += 2;
    p[0] = 0x44;
    oy = 0x7200;
    ox = 0x7000;
    p[1] = FLASH_RGBA(q);
    p += 2;
    p[0] = FLASH_XY(q->pt[0][0], q->pt[0][1]);
    p[1] = FLASH_XY(q->pt[1][0], q->pt[1][1]);
    p += 2;
    p[0] = FLASH_XY(q->pt[2][0], q->pt[2][1]);
    p[1] = FLASH_XY(q->pt[3][0], q->pt[3][1]);
    p += 2;
    Dma_EndDirect(p);
}

/* Draws a textured quad through the frame buffer's alpha plane. `set`: write only alpha (builds the mask).
   Otherwise: write only colour, where the texture's alpha is above 0x3C, blended by the alpha already there. */
void FlashDraw_MaskQuad(FlashQuad *q, u64 tex0, s32 set) {
    u64 *p = Dma_BeginDirect();
    u64 frame;
    u64 reg;

    p[0] = 0x1000000000008007;
    p[1] = 0xE;
    p += 2;
    frame = GFX_FRAME_REG();
    if (set) {
        reg = frame | 0x00FFFFFF00000000;
    } else {
        reg = frame | 0xFF00000000000000;
    }
    p[0] = reg;
    p[1] = 0x4C;
    p += 2;
    p[0] = 1;
    p[1] = 0x46;
    p += 2;
    if (set) {
        p[0] = 0x50000;
    } else {
        p[0] = 0x503CD;
    }
    p[1] = 0x47;
    p += 2;
    if (q->flags & FLASH_QUAD_ADD) {
        p[0] = !set ? 0x58 : 0x48;
    } else if (q->flags & FLASH_QUAD_SUB) {
        p[0] = !set ? 0x52 : 0x42;
    } else {
        p[0] = !set ? 0x54 : 0x44;
    }
    p[1] = 0x42;
    p += 2;
    p[0] = 0;
    p[1] = 0x3F;
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14;
    p += 2;
    if (q->flags & FLASH_QUAD_REPEAT) {
        p[0] = 0;
    } else {
        p[0] = 5;
    }
    p[1] = 8;
    p += 2;
    Dma_EndDirect(FlashDraw_PutSprite(p, q, tex0));
}

/* Draws one quad: nothing when its alpha is 0, flat without a texture, else uploads the texture to block 0x3000
   and draws it normally or through the alpha-plane mask. */
void FlashDraw_Quad(FlashTex *tex, FlashQuad *q) {
    u64 tex0;
    u64 *p;

    if (q->rgba[3] == 0) {
        return;
    }
    if (tex != NULL) {
        tex0 = FlashDraw_UploadTex(tex, q->uv.tex, 0x3000);
        Dma_AddTexFlush();
    } else {
        FlashDraw_FlatQuad(q);
        return;
    }
    if (q->flags & FLASH_QUAD_MASK_SET) {
        Gfx_ClearScreen(0xFFFFFF, 0);
        FlashDraw_MaskQuad(q, tex0, 1);
        return;
    }
    if (q->flags & FLASH_QUAD_MASK) {
        FlashDraw_MaskQuad(q, tex0, 0);
        return;
    }
    p = Dma_BeginDirect();
    p[0] = 0x1000000000008006;
    p[1] = 0xE;
    p += 2;
    p[0] = GFX_FRAME_REG();
    p[1] = 0x4C;
    p += 2;
    p[0] = 1;
    p[1] = 0x46;
    p += 2;
    if (q->flags & FLASH_QUAD_ADD) {
        p[0] = 0x48;
    } else if (q->flags & FLASH_QUAD_SUB) {
        p[0] = 0x42;
    } else {
        p[0] = 0x44;
    }
    p[1] = 0x42;
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14;
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47;
    p += 2;
    if (q->flags & FLASH_QUAD_REPEAT) {
        p[0] = 0;
    } else {
        p[0] = 5;
    }
    p[1] = 8;
    p += 2;
    Dma_EndDirect(FlashDraw_PutSprite(p, q, tex0));
}

/* Brings a shape's quad to the screen: the shape's matrix and colour transform, then the owning sprite's drawing
   state (matrix, position / scale / colour / alpha overrides, texture rectangle, draw flags), then the movie's
   screen offset. Only three corners are transformed; the fourth closes the parallelogram. */
void FlashDraw_TransformQuad(FlashQuad *q, FlashMtx *mtx, FlashCxform *cx, FlashProp *prop, s32 *ofs) {
    s32 i;
    f32 x;
    f32 y;
    f32 sx;
    f32 sy;

    {
        for (i = 0; i < 3; i++) {
            f32 *xs = &q->pt[0][0];
            f32 *ys = &q->pt[0][1];
            f32 *px = &xs[i * 2];
            f32 *py = &ys[i * 2];

            x = *px;
            y = *py;
            *px = x * mtx->m[0] + y * mtx->m[3] + mtx->m[2];
            *py = x * mtx->m[1] + y * mtx->m[4] + mtx->m[5];
            if (prop == NULL) {
                *px += (f32)ofs[0];
                *py += (f32)ofs[1];
            }
        }
    }
    q->pt[3][0] = q->pt[1][0] + q->pt[2][0] - q->pt[0][0];
    q->pt[3][1] = q->pt[1][1] + q->pt[2][1] - q->pt[0][1];
    q->rgba[0] = Flash_Clamp255((s32)((f32)q->rgba[0] * cx->mul[0]) + cx->add[0]);
    q->rgba[1] = Flash_Clamp255((s32)((f32)q->rgba[1] * cx->mul[1]) + cx->add[1]);
    q->rgba[2] = Flash_Clamp255((s32)((f32)q->rgba[2] * cx->mul[2]) + cx->add[2]);
    q->rgba[3] = Flash_Clamp255((s32)((f32)q->rgba[3] * cx->mul[3]) + cx->add[3]);
    if (prop != NULL) {
        sx = prop->mtx.m[0];
        sy = prop->mtx.m[4];
        if (prop->flags & FLASH_OV_SCALE) {
            sx *= prop->scaleX;
            sy *= prop->scaleY;
        }
        {
            for (i = 0; i < 3; i++) {
                f32 *xs = &q->pt[0][0];
                f32 *ys = &q->pt[0][1];
                f32 *px = &xs[i * 2];
                f32 *py = &ys[i * 2];

                x = *px;
                y = *py;
                *px = x * sx + y * prop->mtx.m[3] + prop->mtx.m[2];
                *py = x * prop->mtx.m[1] + y * sy + prop->mtx.m[5];
                if (prop->flags & FLASH_OV_POS) {
                    *px += prop->x;
                    *py += prop->y;
                }
                if (!(prop->flags & FLASH_OV_NO_OFS)) {
                    *px += (f32)ofs[0];
                    *py += (f32)ofs[1];
                }
            }
        }
        q->pt[3][0] = q->pt[1][0] + q->pt[2][0] - q->pt[0][0];
        q->pt[3][1] = q->pt[1][1] + q->pt[2][1] - q->pt[0][1];
        q->rgba[0] = Flash_Clamp255((s32)((f32)q->rgba[0] * prop->cx.mul[0]) + prop->cx.add[0]);
        q->rgba[1] = Flash_Clamp255((s32)((f32)q->rgba[1] * prop->cx.mul[1]) + prop->cx.add[1]);
        q->rgba[2] = Flash_Clamp255((s32)((f32)q->rgba[2] * prop->cx.mul[2]) + prop->cx.add[2]);
        q->rgba[3] = Flash_Clamp255((s32)((f32)q->rgba[3] * prop->cx.mul[3]) + prop->cx.add[3]);
        if (prop->flags & FLASH_OV_COLOR) {
            q->rgba[0] = Flash_Clamp255((s32)((f32)q->rgba[0] * prop->color));
            q->rgba[1] = Flash_Clamp255((s32)((f32)q->rgba[1] * prop->color));
            q->rgba[2] = Flash_Clamp255((s32)((f32)q->rgba[2] * prop->color));
        }
        if (prop->flags & FLASH_OV_ALPHA) {
            q->rgba[3] = Flash_Clamp255((s32)((f32)q->rgba[3] * prop->alpha));
        }
        if (prop->flags & FLASH_OV_UV) {
            q->uv = prop->uv;
            q->uv.tex = 0;
        }
        if (prop->flags & FLASH_OV_TEX) {
            q->uv.tex = prop->uv.tex;
        }
        if (prop->flags & FLASH_OV_FLIPX) {
            q->flags |= FLASH_QUAD_FLIPX;
        }
        if (prop->flags & FLASH_OV_FLIPY) {
            q->flags |= FLASH_QUAD_FLIPY;
        }
        if (prop->flags & FLASH_OV_REPEAT) {
            q->flags |= FLASH_QUAD_REPEAT;
        }
        if (prop->flags & FLASH_OV_ADD) {
            q->flags |= FLASH_QUAD_ADD;
        }
        if (prop->flags & FLASH_OV_SUB) {
            q->flags |= FLASH_QUAD_SUB;
        }
        if (prop->flags & FLASH_OV_MASK) {
            q->flags |= FLASH_QUAD_MASK;
        }
        if (prop->flags & FLASH_OV_MASK_SET) {
            q->flags |= FLASH_QUAD_MASK_SET;
        }
    }
}

void FlashClipList_Init(FlashClipList *list, Flash *flash);
void FlashClipList_Term(FlashClipList *list);
void FlashClipList_Reset(FlashClipList *list);
void FlashClipList_FindRef(FlashClipList *list, char *parent, char *name, FlashRef *out);
void FlashShapePool_Init(FlashShapePool *pool, u32 count);
void FlashShapePool_Term(FlashShapePool *pool);
void FlashShapePool_Clear(FlashShapePool *pool);

/* Sets a movie up on a FOD file and its textures: allocates the root timeline, the sprite instances and the
   shape pool (one shape per depth of every timeline). The object must be zeroed or freshly destroyed. */
void Flash_Create(Flash *flash, u8 *file, FlashTex **tex) {
    Flash_CheckHeader(file);
    memset(flash, 0, sizeof(Flash));
    flash->tex = tex;
    flash->data = file + 8;
    flash->speed = 0;
    flash->ofs[0] = 0;
    flash->ofs[1] = 0;
    if (flash->root == NULL) {
        flash->root = Heap_Alloc(sizeof(FlashTl), 0x20, 0, 2);
        memset(flash->root, 0, sizeof(FlashTl));
    }
    FlashTl_Init(flash->root, Flash_FindTag(flash->data, 7) + 8, flash);
    flash->root->state |= FLASH_TL_PLAY;
    if (flash->clips == NULL) {
        flash->clips = Heap_Alloc(sizeof(FlashClipList), 0x20, 0, 2);
        memset(flash->clips, 0, sizeof(FlashClipList));
    }
    FlashClipList_Init(flash->clips, flash);
    if (flash->shapes == NULL) {
        flash->shapes = Heap_Alloc(sizeof(FlashShapePool), 0x20, 0, 2);
        memset(flash->shapes, 0, sizeof(FlashShapePool));
    }
    FlashShapePool_Init(flash->shapes, flash->root->depths + flash->clips->depths);
}

/* Frees everything Flash_Create allocated. */
void Flash_Destroy(Flash *flash) {
    if (flash->shapes != NULL) {
        FlashShapePool_Term(flash->shapes);
        if (flash->shapes != NULL) {
            Heap_Free(flash->shapes);
            flash->shapes = NULL;
        }
    }
    if (flash->clips != NULL) {
        FlashClipList_Term(flash->clips);
        if (flash->clips != NULL) {
            Heap_Free(flash->clips);
            flash->clips = NULL;
        }
    }
    if (flash->root != NULL) {
        FlashTl_Term(flash->root);
        if (flash->root != NULL) {
            Heap_Free(flash->root);
            flash->root = NULL;
        }
    }
}

/* One tick: clears the event bits of the last tick and, while playing, runs `speed` frames of the root. */
/* The play test reads the flags back through a pointer to the field that is set AFTER the masking store (the
   same habit as `speed`): the compiler knows the value but keeps it as a second register, which is the
   original's `and v1,v1,v0 / move v0,v1 / andi v0,v0,1`. Testing a local or the field itself gives
   `andi v0,v1,1` (one instruction short), and a pointer initialised at its declaration makes the function
   two instructions longer. Found from a decomp-permuter candidate
   (`if ((*(p = &flash->flags) & ~FLASH_END) & FLASH_PLAY)`), of which only the late pointer is needed. */
void Flash_Advance(Flash *flash) {
    s32 *speed = &flash->speed;
    u32 *flags;

    flash->trig = 0;
    flash->se = 0;
    flash->flags &= ~FLASH_END;
    flags = &flash->flags;
    if (*flags & FLASH_PLAY) {
        FlashTl_Advance(flash->root, *speed, 0, 0);
    }
}

/* Draws the movie (after the default drawing environment) unless it is hidden. */
void Flash_Draw(Flash *flash) {
    Gfx_AddDefaultEnv();
    if (!(flash->flags & FLASH_HIDE)) {
        FlashTl_Draw(flash->root, NULL);
    }
}

/* Back to the first frame, playing, every other flag cleared. Unless `keepClips`, every sprite instance and
   shape is cleared as well. */
void Flash_Reset(Flash *flash, s32 keepClips) {
    flash->flags = FLASH_PLAY;
    FlashTl_Rewind(flash->root, keepClips);
    flash->root->state |= FLASH_TL_PLAY;
    if (!keepClips) {
        FlashClipList_Reset(flash->clips);
        FlashShapePool_Clear(flash->shapes);
    }
}

/* Starts playing at `speed` frames per tick. */
void Flash_Play(Flash *flash, s32 speed) {
    flash->speed = speed;
    flash->flags |= FLASH_PLAY;
}

/* Stops Flash_Advance from running the timelines. */
void Flash_Stop(Flash *flash) {
    flash->flags &= ~FLASH_PLAY;
}

/* Sets the screen offset added to everything drawn. */
void Flash_SetOffset(Flash *flash, s32 x, s32 y) {
    flash->ofs[0] = x;
    flash->ofs[1] = y;
}

/* Sets or clears bits of Flash.flags. */
void Flash_SetFlag(Flash *flash, u32 mask, u8 on) {
    if (on) {
        flash->flags |= mask;
    } else {
        flash->flags &= ~mask;
    }
}

/* Jumps the root timeline to a frame label ("fl_..."). */
void Flash_GotoLabel(Flash *flash, char *label, u8 restart) {
    FlashTl_GotoLabel(flash->root, label, restart);
}

/* Moves the root timeline by `step` frames. */
void Flash_StepFrames(Flash *flash, s32 step) {
    FlashTl_StepFrames(flash->root, step);
}

/* Looks a sprite instance up by name ("mc_..."), optionally among the children of the instance called `parent`.
   out->index is -1 when there is none. (Existing name; it finds a clip, not a label.) */
void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out) {
    memset(out, 0, sizeof(FlashRef));
    FlashClipList_FindRef(flash->clips, parent, name, out);
}

/* The clip-list functions of the next file (src/sys/gfxm_d.c), declared here as this file calls them. */
extern void FlashClipList_GotoLabel(FlashClipList *list, FlashRef *ref, char *label);
extern void FlashClipList_SetPreDraw(FlashClipList *list, FlashRef *ref, void *fn, void *arg);
extern void FlashClipList_SetPostDraw(FlashClipList *list, FlashRef *ref, void *fn, void *arg);
extern void FlashClipList_SetDrawOver(FlashClipList *list, FlashRef *ref, void *fn, void *arg);
extern void FlashClipList_SetFlags(FlashClipList *list, FlashRef *ref, s32 mask, u8 on);
extern void FlashClipList_SetOverride(FlashClipList *list, FlashRef *ref, s32 mask, u8 on);
extern void FlashClipList_SetOffset(FlashClipList *list, FlashRef *ref, s32 x, s32 y);
extern void FlashClipList_SetScale(FlashClipList *list, FlashRef *ref, f32 x, f32 y);
extern void FlashClipList_SetAlpha(FlashClipList *list, FlashRef *ref, f32 alpha);
extern void FlashClipList_SetColor(FlashClipList *list, FlashRef *ref, f32 value);
extern void FlashClipList_SetTex(FlashClipList *list, FlashRef *ref, s32 user);
extern void FlashClipList_SetUv(FlashClipList *list, FlashRef *ref, void *rect);
extern void FlashClipList_GetPos(FlashClipList *list, FlashRef *ref, s32 *x, s32 *y);
extern f32 FlashClipList_GetAlpha(FlashClipList *list, FlashRef *ref);
void FlashClipList_Play(FlashClipList *list, FlashRef *ref);
void FlashClipList_Stop(FlashClipList *list, FlashRef *ref);

/* Jumps a clip (and its namesakes) to a label of its own timeline. */
void Flash_ClipGotoLabel(Flash *flash, FlashRef *ref, char *label) {
    if (ref->index >= 0) {
        FlashClipList_GotoLabel(flash->clips, ref, label);
    }
}

/* Sets a clip's callback run before its children are drawn (FlashClip.preDraw / preArg). */
void Flash_ClipSetCallbackA(Flash *flash, FlashRef *ref, void *fn, void *arg) {
    if (ref->index >= 0) {
        FlashClipList_SetPreDraw(flash->clips, ref, fn, arg);
    }
}

/* Sets a clip's callback run after everything of it is drawn (FlashClip.postDraw / postArg). */
void Flash_ClipSetCallbackB(Flash *flash, FlashRef *ref, void *fn, void *arg) {
    if (ref->index >= 0) {
        FlashClipList_SetPostDraw(flash->clips, ref, fn, arg);
    }
}

/* Sets a clip's callback run after its children, which gets the clip's drawing state (FlashClip.drawOver). */
void Flash_ClipSetCallbackC(Flash *flash, FlashRef *ref, void *fn, void *arg) {
    if (ref->index >= 0) {
        FlashClipList_SetDrawOver(flash->clips, ref, fn, arg);
    }
}

/* Sets or clears properties of a clip, one bit of `props` each: 1 playing, 2 visible, 4 flip x, 8 flip y,
   0x10 no screen offset, 0x20 additive, 0x40 subtractive, 0x80 masked, 0x100 mask writer, 0x200 repeat,
   0x400 keeps running while its parent is stopped. */
s32 Flash_ClipSetFlags(Flash *flash, FlashRef *ref, s32 props, u8 on) {
    if (ref->index >= 0) {
        if (props & 1) {
            if (on) {
                FlashClipList_Play(flash->clips, ref);
            } else {
                FlashClipList_Stop(flash->clips, ref);
            }
        }
        if (props & 2) {
            FlashClipList_SetFlags(flash->clips, ref, FLASH_CLIP_VISIBLE, on);
        }
        if (props & 0x400) {
            FlashClipList_SetFlags(flash->clips, ref, FLASH_CLIP_FREE_RUN, on);
        }
        if (props & 4) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_FLIPX, on);
        }
        if (props & 8) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_FLIPY, on);
        }
        if (props & 0x10) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_NO_OFS, on);
        }
        if (props & 0x20) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_ADD, on);
        }
        if (props & 0x40) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_SUB, on);
        }
        if (props & 0x80) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_MASK, on);
        }
        if (props & 0x100) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_MASK_SET, on);
        }
        if (props & 0x200) {
            FlashClipList_SetOverride(flash->clips, ref, FLASH_OV_REPEAT, on);
        }
    }
}

/* Gives a clip a position offset. */
void Flash_ClipSetOffset(Flash *flash, FlashRef *ref, s32 x, s32 y) {
    if (ref->index >= 0) {
        FlashClipList_SetOffset(flash->clips, ref, x, y);
    }
}

/* Gives a clip a scale; negative values are refused. */
void Flash_ClipSetScale(Flash *flash, FlashRef *ref, f32 x, f32 y) {
    if (ref->index >= 0 && !(x < 0.0f) && !(y < 0.0f)) {
        FlashClipList_SetScale(flash->clips, ref, x, y);
    }
}

/* Gives a clip an alpha multiplier; a negative value is refused. */
void Flash_ClipSetAlpha(Flash *flash, FlashRef *ref, f32 alpha) {
    if (ref->index >= 0 && !(alpha < 0.0f)) {
        FlashClipList_SetAlpha(flash->clips, ref, alpha);
    }
}

/* Gives a clip a colour multiplier (r, g, b); a negative value is refused. */
void Flash_ClipSetColor(Flash *flash, FlashRef *ref, f32 color) {
    if (ref->index >= 0 && !(color < 0.0f)) {
        FlashClipList_SetColor(flash->clips, ref, color);
    }
}

/* Makes a clip draw with texture `tex` of its image's texture file (a palette or a frame of a strip). */
void Flash_ClipSetTex(Flash *flash, FlashRef *ref, s32 tex) {
    if (ref->index >= 0) {
        FlashClipList_SetTex(flash->clips, ref, tex);
    }
}

/* Replaces a clip's texture rectangle (four texel coordinates). */
void Flash_ClipSetUv(Flash *flash, FlashRef *ref, void *rect) {
    if (ref->index >= 0) {
        FlashClipList_SetUv(flash->clips, ref, rect);
    }
}

/* Returns a clip's screen position. */
void Flash_ClipGetPos(Flash *flash, FlashRef *ref, s32 *x, s32 *y) {
    if (ref->index >= 0) {
        FlashClipList_GetPos(flash->clips, ref, x, y);
    }
}

#ifdef PORT
/* Let a custom menu page temporarily hide stock clips without changing
   their timeline's visibility when it returns to the original menu. */
s32 Flash_ClipIsVisible(Flash *flash, FlashRef *ref) {
    return ref->index>=0 && (u32)ref->index<flash->clips->count &&
           (flash->clips->items[ref->index].flags&FLASH_CLIP_VISIBLE)!=0;
}
#endif
/* Returns a clip's alpha, 0 when the reference is empty. */
f32 Flash_ClipGetAlpha(Flash *flash, FlashRef *ref) {
    f32 alpha = 0.0f;

    if (ref->index < 0) {
        return alpha;
    }
    return FlashClipList_GetAlpha(flash->clips, ref);
}

/* Allocates the pool of placed shapes. */
void FlashShapePool_Init(FlashShapePool *pool, u32 count) {
    pool->count = count;
    if (pool->items == NULL) {
        pool->items = Heap_Alloc(count * sizeof(FlashShape), 0x20, 0, 2);
        memset(pool->items, 0, pool->count * sizeof(FlashShape));
    }
}

/* Frees the pool's storage. */
void FlashShapePool_Term(FlashShapePool *pool) {
    if (pool->items != NULL) {
        Heap_Free(pool->items);
        pool->items = NULL;
    }
}

/* Frees every shape of the pool. */
void FlashShapePool_Clear(FlashShapePool *pool) {
    if (pool->items != NULL) {
        memset(pool->items, 0, pool->count * sizeof(FlashShape));
    }
}

/* Returns a cleared free shape, or NULL when the pool is full. */
FlashShape *FlashShapePool_Alloc(FlashShapePool *pool) {
    u32 i;
    FlashShape *shape = pool->items;

    for (i = 0; i < pool->count; i++, shape++) {
        if (shape->used == 0) {
            memset(shape, 0, sizeof(FlashShape));
            return shape;
        }
    }
    return NULL;
}

/* Places shape record `id` with a matrix and a colour transform (NULL: identity). */
FlashShape *FlashShape_Create(FlashShapePool *pool, u16 id, FlashMtx *mtx, FlashCxform *cx) {
    FlashShape *shape = FlashShapePool_Alloc(pool);

    if (shape == NULL) {
        return NULL;
    }
    shape->id = id;
    shape->used = 1;
    if (mtx != NULL) {
        shape->mtx = *mtx;
    } else {
        Flash_MtxIdentity(&shape->mtx);
    }
    if (cx != NULL) {
        shape->cx = *cx;
    } else {
        Flash_CxformIdentity(&shape->cx);
    }
    return shape;
}

/* Gives a shape back to the pool. */
void FlashShape_Free(FlashShape *shape) {
    memset(shape, 0, sizeof(FlashShape));
}

/* Replaces a shape's matrix and / or colour transform (NULL: keep). */
void FlashShape_Set(FlashShape *shape, FlashMtx *mtx, FlashCxform *cx) {
    if (mtx != NULL) {
        shape->mtx = *mtx;
    }
    if (cx != NULL) {
        shape->cx = *cx;
    }
}

/* Draws a placed shape: each quad of its record (tag 4), textured with its image's texture file or flat when
   the image index is negative. A quad whose image has no texture file is skipped. */
void FlashShape_Draw(FlashShape *shape, FlashProp *prop, Flash *flash) {
    FlashShapeQuad rec;
    FlashQuad quad;
    u16 count;
    u16 w;
    u16 h;
    u8 *data;
    u8 *img;
    FlashTex *tex;
    s32 i;
    FlashQuad *pq;

    data = Flash_GetRecord(flash->data, 4, shape->id);
    memcpy(&count, data + 8, 2);
    for (i = 0; i < count; i++) {
        FlashQuad *pm = &quad;

        pq = pm;
        tex = NULL;
        memset(pm, 0, sizeof(quad));
        memset(&rec, 0, sizeof(rec));
        memcpy(&rec, data + 10 + i * sizeof(rec), sizeof(rec));
        if (rec.image >= 0) {
            img = Flash_GetRecord(flash->data, 3, rec.image);
            tex = flash->tex[rec.image];
            if (tex == NULL) {
                continue;
            }
            quad.uv.v0 = 0;
            quad.uv.u0 = 0;
            quad.uv.tex = 0;
            memcpy(&w, img + 8, 2);
            memcpy(&h, img + 10, 2);
            quad.uv.u1 = w;
            quad.uv.v1 = h;
        }
        quad.pt[2][0] = rec.x0;
        quad.pt[3][0] = rec.x1;
        quad.pt[1][1] = rec.y0;
        quad.pt[3][1] = rec.y1;
        quad.pt[0][0] = rec.x0;
        quad.pt[1][0] = rec.x1;
        quad.pt[0][1] = rec.y0;
        quad.pt[2][1] = rec.y1;
        quad.rgba[0] = rec.rgba >> 24;
        quad.rgba[1] = (rec.rgba >> 16) & 0xFF;
        quad.rgba[2] = (rec.rgba & 0xFF00) >> 8;
        quad.rgba[3] = rec.rgba & 0xFF;
        FlashDraw_TransformQuad(pq, &shape->mtx, &shape->cx, prop, flash->ofs);
        FlashDraw_Quad(tex, pq);
    }
}

/* Makes one sprite instance per record of the sprite list (tag 2), each on its own frame record of tag 6. */
void FlashClipList_Init(FlashClipList *list, Flash *flash) {
    FlashTag hdr;
    u8 *tag;
    u8 *rec;
    FlashClip *clip;
    s32 i;

    memset(&hdr, 0, sizeof(hdr));
    tag = Flash_FindTag(flash->data, 2);
    if (tag == NULL) {
        return;
    }
    memcpy(&hdr, tag, sizeof(hdr));
    if (list->items == NULL) {
        list->items = Heap_Alloc(hdr.count * sizeof(FlashClip), 0x20, 0, 2);
        memset(list->items, 0, hdr.count * sizeof(FlashClip));
    }
    list->count = hdr.count;
    for (i = 0; i < hdr.count; i++) {
        clip = &list->items[i];
        clip->index = i;
        rec = Flash_SkipRecords(tag, clip->index);
        memcpy(&clip->id, rec + 8, 2);
        clip->flags = 0;
        memset(&clip->prop, 0, sizeof(FlashProp));
        Flash_MtxIdentity(&clip->prop.mtx);
        Flash_CxformIdentity(&clip->prop.cx);
        clip->prop.alpha = 1.0f;
        FlashTl_Init(&clip->tl, Flash_GetRecord(flash->data, 6, clip->id), flash);
        list->depths += clip->tl.depths;
    }
}

/* Frees every instance's depth slots and the list. */
void FlashClipList_Term(FlashClipList *list) {
    u32 i;

    for (i = 0; i < list->count; i++) {
        FlashTl_Term(&list->items[i].tl);
    }
    if (list->items != NULL) {
        Heap_Free(list->items);
        list->items = NULL;
    }
}

/* Ticks every instance's timeline `count` times. Not called by anything in this file. */
void FlashClipList_Advance(FlashClipList *list, u32 count) {
    u32 i;

    for (i = 0; i < list->count; i++) {
        if (list->items[i].flags & FLASH_CLIP_FREE_RUN) {
            FlashTl_Advance(&list->items[i].tl, count, 1, 1);
        } else {
            FlashTl_Advance(&list->items[i].tl, count, 1, 0);
        }
    }
}

/* Resets every instance. */
void FlashClipList_Reset(FlashClipList *list) {
    u32 i;

    for (i = 0; i < list->count; i++) {
        FlashClip_Reset(&list->items[i], 0);
    }
}

/* Returns the first unused instance of sprite `id`, or NULL. */
FlashClip *FlashClipList_FindFree(FlashClipList *list, u16 id) {
    u32 i;
    FlashClip *clip = list->items;

    for (i = 0; i < list->count; i++, clip++) {
        if (!(clip->flags & FLASH_CLIP_USED) && clip->id == id) {
            return clip;
        }
    }
    return NULL;
}

/* Places sprite `id` under an instance name: takes a free instance, makes it used and visible, resets its
   overrides and callbacks and lets its timeline play (from the frame a jump left pending, if any). */
FlashClip *FlashClip_Start(FlashClipList *list, u16 id, char *name, FlashMtx *mtx, FlashCxform *cx) {
    FlashClip *clip = FlashClipList_FindFree(list, id);

    if (clip == NULL) {
        return NULL;
    }
    memset(clip->name, 0, sizeof(clip->name));
    memcpy(clip->name, name, strlen(name));
    clip->flags |= FLASH_CLIP_USED | FLASH_CLIP_VISIBLE;
    memset(&clip->prop, 0, sizeof(FlashProp));
    clip->prop.alpha = 1.0f;
    if (mtx != NULL) {
        clip->prop.mtx = *mtx;
    } else {
        Flash_MtxIdentity(&clip->prop.mtx);
    }
    if (cx != NULL) {
        clip->prop.cx = *cx;
    } else {
        Flash_CxformIdentity(&clip->prop.cx);
    }
    clip->preDraw = NULL;
    clip->postDraw = NULL;
    clip->preArg = NULL;
    clip->postArg = NULL;
    clip->tl.state |= FLASH_TL_PLAY;
    if (clip->tl.prev != 0) {
        clip->tl.frame = clip->tl.prev;
        clip->tl.state |= FLASH_TL_GOTO;
        clip->tl.prev = 0;
    }
    return clip;
}

/* Replaces an instance's matrix and / or colour transform (NULL: keep). */
void FlashClip_Set(FlashClip *clip, FlashMtx *mtx, FlashCxform *cx) {
    if (mtx != NULL) {
        clip->prop.mtx = *mtx;
    }
    if (cx != NULL) {
        clip->prop.cx = *cx;
    }
}

/* Clears an instance: name, flags, overrides, callbacks, and its timeline back to frame 0. */
void FlashClip_Reset(FlashClip *clip, s32 arg) {
    memset(clip->name, 0, sizeof(clip->name));
    clip->flags = 0;
    memset(&clip->prop, 0, sizeof(FlashProp));
    clip->prop.alpha = 1.0f;
    Flash_MtxIdentity(&clip->prop.mtx);
    Flash_CxformIdentity(&clip->prop.cx);
    clip->preDraw = NULL;
    clip->postDraw = NULL;
    clip->preArg = NULL;
    clip->postArg = NULL;
    FlashTl_Rewind(&clip->tl, arg);
}

/* Returns the first instance with the given name, or NULL. */
FlashClip *FlashClipList_FindByName(FlashClipList *list, char *name) {
    u32 i;
    FlashClip *clip = list->items;

    for (i = 0; i < list->count; i++, clip++) {
        if (strcmp(clip->name, name) == 0) {
            return clip;
        }
    }
    return NULL;
}

/* Fills a reference to the instances called `name`. With `parent`: the one placed on a depth of the first
   instance called `parent`. Without: the first in the list, counting the further ones in out->more. */
void FlashClipList_FindRef(FlashClipList *list, char *parent, char *name, FlashRef *out) {
    s32 found = 0;
    u32 i;
    FlashClip *clip;

    memset(out, 0, sizeof(FlashRef));
    if (parent != NULL) {
        clip = list->items;
        for (i = 0; i < list->count; i++, clip++) {
            if (strcmp(clip->name, parent) == 0) {
                FlashSlot *slot = clip->tl.slots;
                s32 j;

                for (j = 0; j < clip->tl.depths; j++) {
                    FlashClip *child = slot->clip;

                    slot++;
                    if (child != NULL && strcmp(child->name, name) == 0) {
                        found = 1;
                        out->index = child->index;
                        break;
                    }
                }
                break;
            }
        }
    } else {
        clip = list->items;
        for (i = 0; i < list->count; i++, clip++) {
            if (strcmp(clip->name, name) == 0) {
                if (found) {
                    out->more++;
                } else {
                    out->index = i;
                    found = 1;
                }
            }
        }
    }
    if (!found) {
        out->more = 0;
        out->index = -1;
    }
}

/* Lets the referenced instance and its namesakes play. */
void FlashClipList_Play(FlashClipList *list, FlashRef *ref) {
    FlashClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->tl.state |= FLASH_TL_PLAY;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].tl.state |= FLASH_TL_PLAY;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}

/* Stops the referenced instance and its namesakes. */
void FlashClipList_Stop(FlashClipList *list, FlashRef *ref) {
    FlashClip *clip = &list->items[ref->index];
    s32 n;
    u32 i;

    clip->tl.state &= ~FLASH_TL_PLAY;
    n = ref->more;
    if (n != 0) {
        for (i = 1; i < list->count - ref->index; i++) {
            if (strcmp(clip[i].name, clip->name) == 0) {
                clip[i].tl.state &= ~FLASH_TL_PLAY;
                n--;
                if (n == 0) {
                    break;
                }
            }
        }
    }
}
