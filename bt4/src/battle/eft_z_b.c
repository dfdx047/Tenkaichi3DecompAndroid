#include "common.h"
#define EFT_Z_IMPL
#include "battle/eft_z.h"

/*
 * Ground dust, kind 4 (landing ring), 0x198BC0..0x199500. Continues eft_z.c; see include/battle/eft_z.h.
 * It was split off while EftGndDustLand_Init was INCLUDE_ASM; every function is C now and the file's .lit4 is
 * 0x2FCDCC..0x2FCE28 (EftGndDustLand_Init's 21 constants, then 0x2FCE20 and 0x2FCE24).
 */

/* EftGndDust_SpawnPieceEx with its real parameter order (the definition in src/battle/eft_aa.c: the eight
   register floats in front of life / fade / tex). eft_z.h declares the three integers first: the registers are the
   same, but the caller then loads them in another order. */
extern EftGndDustPart *EftGndDust_SpawnPieceX(EftGndDustEmit *w, Vec4 *pos, Vec4 *dir, u8 *colA, u8 *colB, f32 sizeX,
                                             f32 sizeY, f32 growX, f32 growY, f32 speed, f32 drag, f32 rot, f32 spin,
                                             s16 life, s16 fade, s16 tex, f32 unkB8, f32 gravity, s32 flags)
    __asm__("EftGndDust_SpawnPieceEx");

/* Kind 4 init: a ring of three low puffs, eight particles thrown outwards and one in the middle.
   Matching notes: the callback's argument is copied into a typed local declared BEHIND `w` (see
   EftGndDustImpact_Init in eft_z_c.c: without the copy one stack slot holds each colour pointer from the start);
   `ang` is a function-scope variable and `s` a local of each loop body (a block-local value is allocated before
   the function-wide ones: s gets f20, ang f21); the random spin is written inside the argument list (as a
   function-scope variable its division cannot move behind the next rand() call); life / fade are integers
   converted at the call, and the last call reuses the values of the eighth iteration. */
void EftGndDustLand_Init(EftZTask *task, void *param) {
    EftGndDustEmit *w = task->work;
    EftGndDustArg *arg = param;
    Vec4 pos;
    Vec4 dir;
    Vec4 v;
    Vec4 off;
    EftZCol colA __attribute__((aligned(16)));
    EftZCol colB __attribute__((aligned(16)));
    s32 i;
    s32 life;
    s32 fade;
    f32 start;
    f32 step = 2.0943951f;
    f32 ang;

    memset(w, 0, sizeof(EftGndDustEmit));
    List_Init(&w->parts);
    w->free = EftGndDust_GetFreeList(arg->pool);
    EftGndDust_GetLightColors(arg->colA, arg->colB);
    Vec4_Set(V(&arg->accel), 0.0f, -1.0f, 0.0f, 1.0f);
    arg->pool = 1;
    arg->tex = 2;
    arg->size = 10;
    arg->life = 30;
    arg->fade = 10;
    arg->grow = 0.1f;
    arg->blend = 0;
    arg->rMin = 0;
    arg->rMax = 0;
    arg->spin = 0;
    arg->speed = 0.0f;
    arg->unk54 = 0.0f;
    arg->drag = 1.0f;
    arg->unk5C = 0.0f;
    start = RANDF() * 6.2831853f;
    colA = *(EftZCol *)arg->colA;
    colB = *(EftZCol *)arg->colB;
    for (i = 0; i < 3; i++) {
        f32 s;

        ang = EftMath_WrapAngle(start + step * i);
        s = Mathf_SinFast(ang);
        Vec4_Set(&dir, s, 0.0f, Mathf_CosFast(ang), 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Scale(&off, &dir, 2.0f);
        Vec4_Add(&pos, V(&arg->pos), &off);
        Vec3_Lerp(&v, V(&arg->dir), &dir, RANDF() * 0.2f + 0.6f);
        EftGndDust_SpawnPieceX(w, &pos, &v, colA.c, colB.c, 5.0f, 6.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                               (f32)arg->life, (f32)arg->fade, 10, 0.7f, 0.0f, EFT_GDUST_PART_FAR);
    }
    start = RANDF() * 6.2831853f;
    step = 0.78539816f;
    for (i = 0; i < 8; i++) {
        f32 s;

        ang = EftMath_WrapAngle(start + step * i);
        s = Mathf_SinFast(ang);
        Vec4_Set(&dir, s, 0.0f, Mathf_CosFast(ang), 1.0f);
        Vec3_Normalize(&dir, &dir);
        Vec4_Scale(&off, &dir, 2.0f);
        Vec4_Add(&pos, V(&arg->pos), &off);
        pos.y -= 3.0f;
        life = arg->life + rand() % 10;
        fade = arg->fade + rand() % 5;
        EftGndDust_SpawnPieceX(w, &pos, &dir, arg->colA, arg->colB, 1.0f, 1.0f, 0.0f, 0.0f, arg->scale * 7.8f, 0.8f,
                               RANDF() * 6.2831853f, 0.3f, (f32)life, (f32)fade, 350, arg->scale * 0.7f,
                               (RANDF() * 0.01f + 0.07f) * arg->scale, 0);
    }
    EftGndDust_SpawnPieceX(w, V(&arg->pos), V(&arg->dir), arg->colA, arg->colB, 1.0f, 1.0f, 0.0f, 0.0f,
                           arg->scale * 0.8f, 0.8f, RANDF() * 6.2831853f, 0.3f, (f32)life, (f32)fade, 350,
                           arg->scale * 0.7f, (RANDF() * 0.01f + 0.03f) * arg->scale, 0);
    w->arg = *arg;
}

/* Kind 4 term. */
void EftGndDustLand_Term(EftZTask *task) {
    EftGndDustEmit *w = task->work;

    EftGndDust_FreeParts(w);
}

/* Kind 4 reset: kills the task. */
void EftGndDustLand_Reset(EftZTask *task) {
    BtlTask_SetDead(task);
}

/* Kind 4 update: follows the owner; the task ends with its last particle. */
void EftGndDustLand_Update(EftZTask *task) {
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
        w->texFar = EftGndDust_GetTex(gEftGndDust->texPtr, 2);
    } else {
        BtlTask_SetDead(task);
    }
}

/* Kind 4 draw: oriented quads; the low puffs fade in between 80 and 150 units from the camera. */
void EftGndDustLand_Draw(EftZTask *task) {
    EftGndDustEmit *w = task->work;
    EftGndDustPart *p = (EftGndDustPart *)List_GetHead(&w->parts);
    Vec4 color;
    Vec4 pos;
    Vec4 d;
    s32 far;
    f32 fade;
    f32 dist;

    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    while (p != NULL) {
        far = 0;
        if (p->flags & EFT_GDUST_PART_FAR) {
            far = 1;
        }
        fade = 1.0f;
        Vec3_Add(&pos, &p->pos, &p->base);
        if (far) {
            Vec3_Sub(&d, &pos, &gBtlCamView->eye);
            dist = Vec3_LengthSq(&d);
            fade = (dist - 6400.0f) / 16100.0f;
            if (22500.0f < dist) {
                fade = 1.0f;
            }
            if (dist < 6400.0f) {
                fade = 0.0f;
            }
        }
        Vec4_Set(&color, p->color.x * w->arg.bright, p->color.y * w->arg.bright, p->color.z * w->arg.bright,
                 p->color.w * p->alpha * fade);
        EftGndDust_DrawPiece(&pos, &color, &p->dir, w->arg.blend, (f32)p->scale * w->arg.scale * p->size.x,
                      (f32)p->scale * w->arg.scale * p->size.y, p->angle, far ? w->texFar : w->tex, far);
        p = (EftGndDustPart *)List_GetNext(&p->link);
    }
    Vu0Cur_Pop();
}
