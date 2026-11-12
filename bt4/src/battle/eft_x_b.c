#include "common.h"
#include "battle/eft_x.h"

/*
 * Effect scene layer 3, 0x190CC8..0x190DA8: the root task of the "common effect" modules. Its class is
 * gEftLayer3Class (0x2C3F98 = { update 0x190D98, init 0x190CC8, term 0x190D60, 0, reset 0x190DA0, 0 }), created by
 * the scene manager (btl_scene.c). Its init creates one child task per entry of gEftLayer3Classes (0x2C3FB0),
 * the manager classes of 31 effect modules; every one of them allocates its work from BtlPool slot 1.
 */

/* One entry of the module table. */
typedef struct EftLayer3Entry {
    /* 0x0 */ void *cls;       /* the module's manager class */
    /* 0x4 */ s32 flags;       /* bit 0: create it (set in all 31) */
} EftLayer3Entry;

typedef struct EftLayer3Work {
    /* 0x0 */ void *list;      /* child task list */
} EftLayer3Work;

extern EftLayer3Work *gEftLayer3;
extern EftLayer3Entry gEftLayer3Classes[31];

extern void BtlPool_SetCurrent(s32 slot);
extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void BtlPool_Reset(s32 slot);
extern void *BtlTask_CreateChildList(void *task, s32 count, s32 workSize);
extern void *BtlTaskList_AddTail(void *list, void *cls, void *arg);

/* Init callback: selects BtlPool slot 1, creates the child list and one manager task per module. */
void EftLayer3_Init(EftXTask *task) {
    s32 i;

    BtlPool_SetCurrent(1);
    gEftLayer3 = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftLayer3Work));
    gEftLayer3->list = NULL;
    gEftLayer3->list = BtlTask_CreateChildList(task, 31, 0);
    for (i = 0; i < 31; i++) {
        if (gEftLayer3Classes[i].flags & 1) {
            BtlTaskList_AddTail(gEftLayer3->list, gEftLayer3Classes[i].cls, NULL);
        }
    }
}

/* Term callback: frees the work and empties BtlPool slot 1. */
void EftLayer3_Term(EftXTask *task) {
    BtlPool_SetCurrent(1);
    BtlPool_Free(BtlPool_GetCurrent(), gEftLayer3);
    gEftLayer3 = NULL;
    BtlPool_Reset(1);
}

/* Update callback: nothing (the children update themselves). */
void EftLayer3_Update(EftXTask *task) {
}

/* Reset callback: nothing. */
void EftLayer3_Reset(EftXTask *task) {
}
