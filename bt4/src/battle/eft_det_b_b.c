/*
 * Stage navigation graph: 0x1B3510..0x1B3F78 (include/battle/eft_det_b.h has the layouts).
 *
 * The stage file carries a graph of way points (0x30 bytes each: position, flags, object index, eight links).
 * The AI's move action asks for a path between two positions when the straight line is blocked
 * (btl_ai_mgr.c); nothing else reads the graph. SIMULATION only through the CPU player's input. No random
 * draw; the search depends only on the graph and the two positions.
 */
#include "common.h"
#include "sys/common.h"
#include "sys/heap.h"
#include "battle/eft_det_b.h"

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec4_Set(StgColVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Sub(StgColVec *out, StgColVec *a, StgColVec *b);
extern f32 Vec3_Length(StgColVec *v);
extern s32 BtlStage_IsReady(void);
extern s32 Battle_GetStage(void);

/* gCommonRes + 0x24: the loaded stage file (a pack whose header is a table of byte offsets). */
typedef struct StgNavRes {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ u32 *stageFile;
} StgNavRes;

#define STGNAV_RES ((StgNavRes *)gCommonRes)

/* The graph is bound and the stage can be queried. */
s32 StgNav_IsReady(void) {
    if (gStgNav == NULL) {
        return 0;
    }
    if (!gStgNav->ready) {
        return 0;
    }
    return BtlStage_IsReady() != 0;
}

/* Binds the graph of the loaded stage file (header words +0x40 / +0x44: start and end offset of the member).
   0 on success, -1 already bound, -2 the stage has no graph, -3 wrong version. */
s32 StgNav_Bind(void) {
    u32 *file;
    u32 start;
    u32 end;
    StgNavData *data;

    if (gStgNav->ready == 1) {
        return -1;
    }
    file = STGNAV_RES->stageFile;
    start = file[0x10];
    end = file[0x11];
    data = (StgNavData *)&file[start / 4];
    if (end == start) {
        return -2;
    }
    gStgNav->data = data;
    gStgNav->data->nodes = (StgNavNode *)((u8 *)data + 0x10);
    if (gStgNav->data->version != 1) {
        return -3;
    }
    gStgNav->ready = gStgNav->data->version;
    return 0;
}

/* Frees the manager and its two lists. */
void StgNav_Term(void) {
    if (gStgNav != NULL) {
        if (gStgNav->open != NULL) {
            Heap_Free(gStgNav->open);
            gStgNav->open = NULL;
        }
        if (gStgNav->closed != NULL) {
            Heap_Free(gStgNav->closed);
            gStgNav->closed = NULL;
        }
        if (gStgNav != NULL) {
            Heap_Free(gStgNav);
            gStgNav = NULL;
        }
    }
}

/* After a stage change: binds the new stage's graph and marks every node that carries an object. */
void StgNav_Rebind(void) {
    if (StgNav_IsReady()) {
        gStgNav->ready = 0;
        if (StgNav_Bind() == 0) {
            StgNav_BlockObj(-1);
        }
    }
}

/* Allocates the manager and binds the graph. Declared with a result it never sets (the call to StgNav_Bind is
   not a tail call). */
s32 StgNav_Init(void) {
    if (gStgNav == NULL) {
        gStgNav = Heap_Alloc(sizeof(StgNav), 0x20, 0, 2);
        memset(gStgNav, 0, sizeof(StgNav));
        gStgNav->ready = 0;
        gStgNav->data = NULL;
        gStgNav->open = NULL;
        gStgNav->closed = NULL;
        gStgNav->stage = Battle_GetStage();
        gStgNav->open = Heap_Alloc(0xC00, 0x20, 0, 2);
        memset(gStgNav->open, 0, 0xC00);
        gStgNav->closed = Heap_Alloc(0xC00, 0x20, 0, 2);
        memset(gStgNav->closed, 0, 0xC00);
        StgNav_Bind();
    }
}

/* Index of the usable node nearest to a position (within 10000), -1 when none. The first of equally near nodes
   loses to a later one. */
s32 StgNav_FindNearest(StgColVec *pos) {
    StgColVec d;
    s32 best = -1;
    f32 bestDist = 10000.0f;
    s32 i;

    for (i = 0; i < gStgNav->data->count; i++) {
        StgNavNode *node = &gStgNav->data->nodes[i];
        f32 dist;

        if (node->flags & STGNAV_NODE_OFF) {
            continue;
        }
        Vec3_Sub(&d, pos, (StgColVec *)node);
        dist = Vec3_Length(&d);
        if (bestDist < dist) {
            continue;
        }
        bestDist = dist;
        best = i;
    }
    return best;
}

/* Clears a node: no object, no links. No caller. */
void StgNavNode_Clear(StgNavNode *node) {
    s32 i;
    s16 none;

    Vec4_Set((StgColVec *)node, 0.0f, 0.0f, 0.0f, 0.0f);
    node->flags = 0;
    node->obj = -1;
    for (i = 2; i >= 0; i--) {
        node->unk18[i] = 0;
    }
    none = -1;
    for (i = 7; i >= 0; i--) {
        node->link[i] = none;
    }
}

/* Swaps two search entries. */
void StgNav_SwapEntries(StgNavEntry *a, StgNavEntry *b) {
    StgNavEntry tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Quicksort of list[lo..hi] by step count, largest first. */
void StgNav_SortEntries(s32 lo, s32 hi, StgNavEntry *list) {
    s32 last;
    s32 i;

    if (lo >= hi) {
        return;
    }
    StgNav_SwapEntries(&list[lo], &list[(lo + hi) / 2]);
    last = lo;
    for (i = lo + 1; i <= hi; i++) {
        if (list[i].steps > list[lo].steps) {
            StgNav_SwapEntries(&list[++last], &list[i]);
        }
    }
    StgNav_SwapEntries(&list[last], &list[lo]);
    StgNav_SortEntries(lo, last - 1, list);
    StgNav_SortEntries(last + 1, hi, list);
}

/* Path from the node nearest `from` to the node nearest `to`, fewest links first. path->pts[0] is `to` itself,
   then the goal node and the nodes back towards the start (16 points at most: a longer path loses its start
   side). path->count stays 0 when there is no graph, no usable node or no connection. */
void StgNav_FindPath(StgColVec *from, StgNavPoint *to, StgNavPath *path) {
    s32 closedCount = 0;
    s32 openCount;
    s32 start;
    s32 goal;
    StgNavEntry *e;
    StgNavEntry *p;
    StgNavNode *node;
    StgNavNode *n; /* one variable for both path copies: the node address is formed as a value, base first */
    s32 found;
    s32 i;
    s32 j;

    path->count = 0;
    if (!StgNav_IsReady()) {
        return;
    }
    start = StgNav_FindNearest(from);
    goal = StgNav_FindNearest((StgColVec *)to);
    if (start == -1) {
        return;
    }
    if (goal == -1) {
        return;
    }
    openCount = 1;
    e = gStgNav->open;
    e->node = start;
    e->from = NULL;
    e->steps = 0;
    do {
        if (e->node == goal) {
            path->pts[path->count++] = *to;
            if (path->count >= 16) {
                return;
            }
            n = &gStgNav->data->nodes[e->node];
            path->pts[path->count++] = n->pos;
            p = e;
            while (p->from != NULL) {
                p = p->from;
                if (path->count >= 16) {
                    return;
                }
                n = &gStgNav->data->nodes[p->node];
                path->pts[path->count++] = n->pos;
            }
            return;
        }
        gStgNav->closed[closedCount] = *e;
        closedCount++;
        openCount--;
        node = &gStgNav->data->nodes[e->node];
        for (i = 0; i < 8; i++) {
            if (node->link[i] == -1) {
                continue;
            }
            if (node->flags & STGNAV_NODE_OFF) {
                continue;
            }
            found = 0;
            for (j = 0; j < closedCount; j++) {
                if (gStgNav->closed[j].node == node->link[i]) {
                    found = 1;
                    break;
                }
            }
            if (found == 1) {
                continue;
            }
            for (j = 0; j < openCount; j++) {
                if (gStgNav->open[j].node == node->link[i]) {
                    found = 1;
                    break;
                }
            }
            if (found == 1) {
                continue;
            }
            gStgNav->open[openCount].node = node->link[i];
            gStgNav->open[openCount].from = &gStgNav->closed[closedCount - 1];
            gStgNav->open[openCount].steps = e->steps + 1;
            openCount++;
        }
        StgNav_SortEntries(0, openCount - 1, gStgNav->open);
        e = &gStgNav->open[openCount - 1];
    } while (openCount != 0);
}

/* Clears node flag 2 of the nodes of object idx (-1: of every node). Called when the object breaks. */
void StgNav_UnblockObj(s32 idx) {
    StgNavNode *node;

    if (!StgNav_IsReady()) {
        return;
    }
    for (node = gStgNav->data->nodes; node != &gStgNav->data->nodes[gStgNav->data->count]; node++) {
        if (node->flags & STGNAV_NODE_OBJ) {
            if (idx == -1 || idx == node->obj) {
                node->flags &= ~STGNAV_NODE_OBJ;
            }
        }
    }
}

/* Sets node flag 2 on the nodes of object idx (-1: on every node that has an object). */
void StgNav_BlockObj(s32 idx) {
    StgNavNode *node;

    if (!StgNav_IsReady()) {
        return;
    }
    for (node = gStgNav->data->nodes; node != &gStgNav->data->nodes[gStgNav->data->count]; node++) {
        if (node->obj != -1) {
            if (idx == -1 || idx == node->obj) {
                node->flags |= STGNAV_NODE_OBJ;
            }
        }
    }
}
