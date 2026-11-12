#include "common.h"
#define EFT_Z_IMPL
#include "battle/eft_z.h"

/*
 * Ground dust, kind 5 (impact) and the particle helpers, 0x199500..0x199F28. Continues eft_z_b.c; see
 * include/battle/eft_z.h. It was split off while EftGndDustImpact_Init was INCLUDE_ASM; every function is C now
 * and the file's .lit4 is 0x2FCE28..0x2FCE54 (EftGndDustImpact_Init's ten constants, then 0x2FCE50).
 */

/* EftGndDust_SpawnPieceEx with its real parameter order (the definition in src/battle/eft_aa.c: the eight
   register floats in front of life / fade / tex). eft_z.h declares the three integers first: the registers are the
   same, but the caller then loads them in another order. */
extern EftGndDustPart *EftGndDust_SpawnPieceX(EftGndDustEmit *w, Vec4 *pos, Vec4 *dir, u8 *colA, u8 *colB, f32 sizeX,
                                             f32 sizeY, f32 growX, f32 growY, f32 speed, f32 drag, f32 rot, f32 spin,
                                             s16 life, s16 fade, s16 tex, f32 unkB8, f32 gravity, s32 flags)
    __asm__("EftGndDust_SpawnPieceEx");

/* Kind 5 init: two particles thrown outwards from the point.
   Matching notes: the callback's argument is copied into a typed local declared BEHIND `w` (declared first, or
   with the parameter used directly, gcse merges the `arg + 0x30` / `arg + 0x34` of the first call with those of
   the loop and spills one pointer from the start; with the copy the loop's addresses are hoisted on their own and
   become register copies of s0 / s1, as in the original). `arg->unk5C = 0.0f` is the LAST of the 0.0 stores: the
   first scheduling pass emits the store in which the constant's register dies first. */
void EftGndDustImpact_Init(EftZTask *task, void *param) {
    EftGndDustEmit *w = task->work;
    EftGndDustArg *arg = param;
    Vec4 pos;
    Vec4 dir;
    Vec4 off;
    s32 i;
    s32 life;
    s32 fade;
    f32 start;
    f32 step;
    f32 ang;
    f32 s;
    f32 spin;
    f32 grow;

    memset(w, 0, sizeof(EftGndDustEmit));
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    EftGndDust_GetLightColors(arg->colA, arg->colB);
    arg->scale = BtlScene_GetCharScale(arg->chr);
    if (arg->scale < 0.8f) {
        arg->scale = 0.8f;
    }
    Vec4_Set(V(&arg->accel), 0.0f, -1.0f, 0.0f, 1.0f);
    arg->pool = 1;
    arg->tex = 2;
    arg->grow = 0.1f;
    arg->size = 10;
    arg->life = 15;
    arg->fade = 5;
    arg->blend = 0;
    arg->rMin = 0;
    arg->rMax = 0;
    arg->spin = 0;
    arg->speed = 0.0f;
    arg->unk54 = 0.0f;
    arg->drag = 1.0f;
    arg->unk5C = 0.0f;
    step = 2.0943951f;
    start = RANDF() * 6.2831853f;
    for (i = 0; i < 2; i++) {
        ang = EftMath_WrapAngle(start + step * i);
        s = Mathf_SinFast(ang);
        Vec4_Set(&dir, s, 0.0f, Mathf_CosFast(ang), 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Scale(&off, &dir, 2.0f);
        Vec4_Add(&pos, V(&arg->pos), &off);
        pos.y -= RANDF() * 3.0f;
        life = arg->life + rand() % 5;
        fade = arg->fade + rand() % 5;
        spin = RANDF() * 6.2831853f;
        grow = (RANDF() * 0.005f + 0.005f) * arg->scale;
        EftGndDust_SpawnPieceX(w, &pos, &dir, arg->colA, arg->colB, 1.0f, 1.0f, 0.05f, 0.05f, arg->scale * 0.3f, 0.8f,
                               spin, 0.3f, (f32)life, (f32)fade, rand() % 10 + 25, 1.0f, grow, 0);
    }
    w->arg = *arg;
}

/* Kind 5 term. */
void EftGndDustImpact_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
}

/* Kind 5 reset: kills the task. */
void EftGndDustImpact_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 5 update: follows the owner; the task ends with its last particle. */
void EftGndDustImpact_Update(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    if (!BtlScene_IsTimeStopped()) {
        BtlCharApi_GetPos(w->arg.chr, &w->pos);
        BtlCharApi_GetDir(w->arg.chr, &w->dir);
        Vec3_Scale(&w->dir, &w->dir, -1.0f);
        Vec3_Normalize(&w->dir, &w->dir);
        EftGndDust_UpdateParts(w);
    }
    if (List_GetHead(&w->parts) != NULL) {
        w->tex = EftGndDust_GetTex(gEftGndDust->texPtr, 0);
    } else {
        BtlTask_SetDead(task);
    }
}

/* Kind 5 draw: oriented quads at the particles' own positions. */
void EftGndDustImpact_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    while (p != NULL) {
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha);
        EftGndDust_DrawPiece(&p->pos, &color, &p->dir, w->arg.blend, (f32)p->scale * w->arg.scale * p->size.x,
                      (f32)p->scale * w->arg.scale * p->size.y, p->angle, w->tex, 0);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    Vu0Cur_Pop();
}

/* Steps every particle of a task: life, attachment to the owner, colour, size, spin, velocity, fade-out; frees
   the finished ones. */
void EftGndDust_UpdateParts(EftGndDustEmit *w) {
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);

    while (p != NULL) {
        p->life--;
        if (p->life <= 0) {
            p->life = 0;
            p->flags |= EFT_GDUST_PART_FADING;
        }
        if (p->flags & EFT_GDUST_PART_FOLLOW) {
            Vec4_Copy(&p->base, &w->pos);
        }
        if (p->flags & EFT_GDUST_PART_TURN) {
            Vec4_Copy(&p->dir, &w->dir);
            Vec3_Scale(&p->vel, &p->dir, p->speed);
        }
        EftGndDust_LerpColor(&p->color, p->colA, p->colB, (f32)p->life / (f32)p->lifeMax);
        Vec3_Add(&p->size, &p->size, &p->grow);
        Vec3_Scale(&p->grow, &p->grow, p->growDamp);
        p->angle += p->spin * 3.14159265f / 180.0f;
        p->angle = EftMath_WrapAngle(p->angle);
        Vec3_Scale(&p->vel, &p->vel, p->drag);
        Vec3_Add(&p->vel, &p->vel, &p->accel);
        Vec3_Add(&p->pos, &p->pos, &p->vel);
        if (w->flags & EFT_GDUST_DROP) {
            if (w->partFlags & EFT_GDUST_PART_FOLLOW) {
                p->flags &= ~EFT_GDUST_PART_FOLLOW;
                Vec3_Add(&p->pos, &p->pos, &p->base);
                Vec4_Set(&p->base, 0.0f, 0.0f, 0.0f, 1.0f);
            }
        }
        if (p->flags & EFT_GDUST_PART_FADING) {
            do {
                if (p->fade > 0) {
                    p->fade--;
                    p->alpha = (f32)p->fade / (f32)p->fadeLen;
                    if (p->fade > 0) {
                        break;
                    }
                    p->alpha = 0.0f;
                }
                p->flags |= EFT_GDUST_PART_DONE;
            } while (0);
        }
        if (p->flags & EFT_GDUST_PART_DONE) {
            p->flags &= ~EFT_GDUST_PART_ALIVE;
            p = EftGndDust_MoveNode(&w->parts, w->free, p);
            gEftGndDust->count--;
        } else {
            p = (EftGndDustPart *)List_GetNext(&p->link);
        }
    }
}

/* out = a * t + b * (1 - t), per byte. */
void EftGndDust_LerpColor(Vec4 *out, u8 *a, u8 *b, f32 t) {
    f32 u = 1.0f - t;

    out->x = a[0] * t + b[0] * u;
    out->y = a[1] * t + b[1] * u;
    out->z = a[2] * t + b[2] * u;
    out->w = a[3] * t + b[3] * u;
}

/* The free list of particle pool 0 / 1. */
List *EftGndDust_GetFreeList(s32 pool) {
    return &gEftGndDust->free[pool];
}

/* Takes a particle from a pool, links it into a task's list and clears it. */
EftGndDustPart *EftGndDust_AllocPart(List *list, List *free) {
    EftGndDustPart *p = (EftGndDustPart *)List_PopFront(free);
    ListNode *prev;
    ListNode *next;

    if (p != NULL) {
        List_PushBack(list, &p->link);
        prev = p->link.prev;
        next = p->link.next;
        memset(p, 0, sizeof(EftGndDustPart));
        p->link.prev = prev;
        p->link.next = next;
        return p;
    }
    return p;
}
