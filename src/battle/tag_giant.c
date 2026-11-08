#include "common.h"
#include "battle/tag_giant.h"
#include "battle/tag_team.h"
#include "battle/tag_options.h"
#include "battle/bobj_a.h"
#include "battle/btl_char_mgr.h"
extern BtlMgrChr *BtlChar_Get(s32);
extern BobjObj *BtlChar_GetObj(BtlMgrChr *);
extern BobjObj *BtlObj_Get(s32);

static s32 giant(BobjObj *obj) {
    return TagTeam_Active() && obj && obj->type==0 && obj->mdl.param &&
        ((u8 *)obj->mdl.param)[2]==4;
}
f32 TagGiant_ObjectScale(void *object) {
    return giant(object) && Port_TagOption(TAG_OPT_GIANTS) ?
        Port_TagOption(TAG_OPT_GIANT_SIZE)*0.01f : 1.0f;
}
void TagGiant_UpdateBody(void *object) {
    BobjObj *obj=object;
    BobjBody *body;
    f32 scale;
    s32 i;
    if(!giant(obj) || !obj->mdl.model) return;
    scale=TagGiant_ObjectScale(obj);body=&obj->body;
    /* Always derive from immutable model dimensions, including when a player
       turns the setting back off mid-match. Never multiply yesterday's size. */
    for(i=0;i<4;i++) body->size[i]=obj->mdl.model->size[i]*scale;
    body->radius=obj->mdl.param->radius>0 ? obj->mdl.param->radius*scale : body->size[3]*2;
    if(obj->mdl.body) body->unkA0=obj->mdl.body->unk04*scale;
}
static BobjObj *actor_object(s32 actor) {
    if(!TagTeam_Active() || (u32)actor>=(u32)TagTeam_Count()) return NULL;
    return BtlChar_GetObj(BtlChar_Get(actor));
}
f32 TagGiant_AnimationStep(void *object) {
    BobjObj *obj=object;
    s32 action;
    if(!giant(obj) || !Port_TagOption(TAG_OPT_GIANTS) ||
       (u32)obj->id>=(u32)TagTeam_Count()) return 1;
    action=BtlChar_Get(obj->id)->action;
    return action>=55 && action<180 ? Port_TagOption(TAG_OPT_GIANT_SPEED)*0.01f : 1;
}
s32 TagGiant_Damage(s32 actor,s32 damage) {
    if(giant(actor_object(actor)) && Port_TagOption(TAG_OPT_GIANTS) && damage>0) {
        s32 percent=Port_TagOption(TAG_OPT_GIANT_DAMAGE);
        /* Bound corrupt data before the multiplication; normal native damage
           is several orders of magnitude below this limit. */
        if(damage>10000000) damage=10000000;
        return (damage/100)*percent+(damage%100)*percent/100;
    }
    return damage;
}
s32 TagGiant_Armor(s32 actor) {
    return giant(actor_object(actor)) && Port_TagOption(TAG_OPT_GIANTS) ?
        Port_TagOption(TAG_OPT_GIANT_ARMOR) : 0;
}
f32 TagGiant_Camera(s32 object) {
    return TagTeam_Active() && giant(BtlObj_Get(object)) && Port_TagOption(TAG_OPT_GIANTS) ?
        Port_TagOption(TAG_OPT_GIANT_CAMERA)*0.01f : 1;
}
