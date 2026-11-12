#include "common.h"
#include "sys/math3d.h"
#include "sys/rigid.h"
#include "battle/stg_d.h"

/*
 * Stage rigid bodies, the two list helpers in front of src/battle/stg_d.c (0x22FC40-0x22FD10). They belong to
 * that file (same module, same list idiom); see battle/stg_d.h.
 */

/* Number of nodes of a list. */
s32 StgRigidList_Count(StgRigidNode *head) {
    StgRigidNode **link = &head;
    s32 n = 0;

    while (*link != NULL) {
        n++;
        link = &(*link)->next;
    }
    return n;
}

/* Takes a node: the first of the free list, else the next unused one of the pool (at most 127 are handed out);
   NULL when the pool is used up. The node's resting word is cleared. */
StgRigidNode *StgRigid_AllocNode(void) {
    StgRigidNode *n;

    if (gStgRigid->free != NULL) {
        n = gStgRigid->free;
        gStgRigid->free = n->next;
    } else if (gStgRigid->used < STG_RIGID_MAX - 1) {
        n = &gStgRigid->nodes[gStgRigid->used++];
    } else {
        return NULL;
    }
    n->resting = 0;
    return n;
}
