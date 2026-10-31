#include "common.h"
/*
 * Draw pass of the battle objects (0x10FB40..0x10FFD0): the per-view loops. BtlObjDraw_Draw is called once per
 * frame by Battle_Draw (twice in battle.c: both draw variants) and by the character viewer (ChrView_Update,
 * "pass 2"). It walks the draw lists that BtlObj_UpdateView built (BtlObjVis, btl_obj.c):
 *   1. every object of every view: BtlObjDraw_DrawModel (gfxm_d_c.c);
 *   2. BtlObj_BeginDraw (the post passes that need the models in the frame: alpha copy, outline, glow);
 *   3. if any view record has BTL_OBJ_FLAG_FADE: the fade layer of those objects (BtlObjDraw_DrawModelFade);
 *   4. shadows, over the used list: objects with state flags 0x2 and 0x10 set and 0x20000000 (flat shadow) or
 *      0x10000000 (rendered shadow), for each view whose list holds the object;
 *   5. the default GS environment again.
 * In split screen (view count >= 2) each view is made current with its scissor before its list is drawn.
 * The file is followed by the VU0 routine ObjSeam_TransformVtx (0x10FFD0..0x1101D0, hand-written assembly) and by
 * gfxm_d_c.c; the three are probably one source file with gfxm_e.c (suggested name: battle/btl_obj_draw.c).
 */
#include "sys/gfx.h"
#include "sys/list.h"
#include "battle/btl_obj.h"
#include "battle/btl_cam.h"
#include "battle/tag_team.h"

void BtlObjDraw_DrawModel(BtlObj *obj);
void BtlObjDraw_DrawModelFade(BtlObj *obj);
void BtlObjDraw_SetEnv(void);
void BtlObjDraw_End(void);
extern void ObjGs_AddFlatShadowEnv(void);
extern void ObjShadow_BeginFlat(BtlObj *obj);
extern void ObjShadow_BeginRender(BtlObj *obj);
extern void ObjShadow_Draw(BtlObj *obj, void *view);
extern void ObjShadow_ResetEnv(void);

#define FLAG_BITS(p) ((BtlObjFlagBits *)&(p)->flags)

/* In split screen (two or more views) makes view `view` the current one, with its scissor. */
void BtlObjDraw_ApplyView(s32 view, s32 count) {
    if (count >= 2) {
        View_Apply(BtlObjVis_GetView(view), 1);
    }
}

/* Draws every object of every view's draw list. */
void BtlObjDraw_DrawLists(s32 count) {
    s32 v;
    s32 i;
    BtlObjDrawList *list;

    for (v = 0; v < count; v++) {
        if (count>1 && !TagTeam_ViewVisible(v)) continue;
        BtlObjDraw_ApplyView(v, count);
        list = BtlObjVis_GetList(v);
        for (i = 0; i < list->count; i++) {
            BtlObj *obj = list->objs[i];
            BtlObjState *state = &obj->state;

            state->view = BtlObj_View(obj, v);
            BtlObjDraw_DrawModel(obj);
        }
    }
}

/* Second pass over the draw lists: the objects whose view record has the distance fade flag. */
void BtlObjDraw_DrawFadeLists(s32 count) {
    s32 v;
    s32 i;
    BtlObjDrawList *list;

    for (v = 0; v < count; v++) {
        if (count>1 && !TagTeam_ViewVisible(v)) continue;
        BtlObjDraw_ApplyView(v, count);
        list = BtlObjVis_GetList(v);
        for (i = 0; i < list->count; i++) {
            BtlObj *obj = list->objs[i];
            BtlObjState *state = &obj->state;

            state->view = BtlObj_View(obj, v);
            if (state->view->flags & BTL_OBJ_FLAG_FADE) {
                BtlObjDraw_DrawModelFade(obj);
            }
        }
    }
}

/* Returns 1 when the object is in the draw list of the view. */
s32 BtlObjDraw_IsInList(s32 view, BtlObj *obj) {
    BtlObjDrawList *list = BtlObjVis_GetList(view);
    s32 i;

    for (i = 0; i < list->count; i++) {
        if (list->objs[i] == obj) {
            return 1;
        }
    }
    return 0;
}

/* Shadow pass, over the used list: objects with state flags 0x2 and 0x10, in the views that list them: the flat
   shadow (flag 0x20000000) or the rendered one (flag 0x10000000). Ends by restoring the environment. */
void BtlObjDraw_DrawShadows(s32 count) {
    ListNode *node;
    s32 i;
    s32 mask;

    BtlObjDraw_SetEnv();
    Gfx_ClearScreen(0xFFFFFF, 0);
    for (node = List_GetHead(BtlObj_GetUsedList()); node != NULL; node = List_GetNext(node)) {
        BtlObj *obj;
        BtlObjState *state;

        mask = 0;
        obj = (BtlObj *)((u8 *)node + 0x10);
        state = &obj->state;
        for (i = 0; i < count; i++) {
            if (count>1 && !TagTeam_ViewVisible(i)) continue;
            if (BtlObjDraw_IsInList(i, obj)) {
                mask |= 1 << i;
            }
        }
        if (mask != 0 && FLAG_BITS(&obj->state)->visible && FLAG_BITS(&obj->state)->bits4 & 1) {
            if (obj->state.flags & 0x20000000) {
                ObjGs_AddFlatShadowEnv();
                for (i = 0; i < count; i++) {
                    if ((mask >> i) & 1) {
                        state->view = BtlObj_View(obj, i);
                        ObjShadow_BeginFlat(obj);
                    }
                }
            } else if (obj->state.flags & 0x10000000) {
                ObjShadow_BeginRender(obj);
                for (i = 0; i < count; i++) {
                    if ((mask >> i) & 1) {
                        state->view = BtlObj_View(obj, i);
                        ObjShadow_Draw(obj, BtlObjVis_GetView(i));
                    }
                }
            }
        }
    }
    ObjShadow_ResetEnv();
}

/* The battle objects' draw pass (nothing when no object is visible): clear the frame's alpha, the models of every
   view, the post passes, the fade layers, the shadows. */
void BtlObjDraw_Draw(void) {
    s32 count = BtlObjVis_Get()->viewCount;

    if (BtlObjVis_TestFlags(BTL_OBJ_FLAG_VISIBLE)) {
        Gfx_ClearScreen(0xFFFFFF, 0xFF000000);
        BtlObjLight_FindRes0();
        BtlObjDraw_DrawLists(count);
        BtlObj_BeginDraw();
        if (BtlObjVis_TestFlags(BTL_OBJ_FLAG_FADE)) {
            BtlObjDraw_DrawFadeLists(count);
        }
        BtlObjDraw_DrawShadows(count);
        BtlObjDraw_End();
    }
}
