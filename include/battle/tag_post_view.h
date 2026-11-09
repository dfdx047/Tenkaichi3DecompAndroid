#ifndef BATTLE_TAG_POST_VIEW_H
#define BATTLE_TAG_POST_VIEW_H
#include "types.h"
extern s32 TagTeam_Active(void);
extern s32 TagTeam_ViewCount(void);
extern s32 TagTeam_VisibleViews(void);
extern s32 TagTeam_ViewVisible(s32 view);
extern void TagTeam_ViewRect(s32 view,s32 *rect);
extern s32 BtlScene_IsSingleView(void);
extern void Port_TagPostTrace(s32 kind,s32 view,s32 split,u32 rect);
/* Post effects run after all scene views. Their source and destination must
 * use the final layout, including merged fusion views and cinematic overrides. */
static s32 TagPost_Split(void) {
    return TagTeam_Active() && !BtlScene_IsSingleView() && TagTeam_VisibleViews() > 1;
}
static void TagPost_Rect(s32 view, s32 split, s32 *r) {
    r[0]=r[1]=0; r[2]=512; r[3]=448;
    if (split) TagTeam_ViewRect(view,r);
}
#endif
