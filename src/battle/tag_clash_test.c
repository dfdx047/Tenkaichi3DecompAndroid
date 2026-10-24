/* Opt-in native-engine regression. Collision ownership is supplied explicitly;
 * the game's real action handlers, placement, prompts and result logic run. */
#include "common.h"
#include "battle/tag_clash.h"
#include "battle/btl_char_flag.h"
#include "battle/tag_options.h"
#include "battle/btl_cam.h"
extern BtlFlagChr *BtlChar_Get(s32);
extern void BtlAct_Request(BtlFlagChr *,s32), BtlAct_ClearQueue(BtlFlagChr *);
extern s32 BtlAct_GetCurrent(BtlFlagChr *);
extern void Port_TagTestCheck(s32,s32);

static void start(s32 a,s32 b,s32 flag) {
    Port_TagTestCheck(BtlClash_Join(a,b),40);
    BtlChar_SetFlag(BtlChar_Get(a),flag);
    BtlChar_SetFlag(BtlChar_Get(b),flag);
}
static void camera_options(s32 actor,s32 option,s32 check) {
    s32 old=Port_TagOption(option);
    Port_TagOptionSet(option,1);
    Port_TagTestCheck(TagTeam_ShareClashCamera(actor) && BtlCam_GetPriorityView()>=0,check);
    Port_TagOptionSet(option,0);
    Port_TagTestCheck(!TagTeam_ShareClashCamera(actor) && BtlCam_GetPriorityView()<0,check+1);
    Port_TagOptionSet(option,old);
}
void TagClash_TestTick(s32 tick) {
    s32 i;
    extern void Port_TagClashTrace(s32,s32,s32,s32,s32);
    if((tick>=29 && tick<=45) || tick==250 || tick==650 || tick==1100) for(i=0;i<4;i++) {
        BtlFlagChr *chr=BtlChar_Get(i);
        Port_TagClashTrace(tick,i,chr->player,chr->objId,BtlAct_GetCurrent(chr));
        Port_TagClashTrace(tick,i,BtlClash_State(i),BtlClash_Opponent(i),chr->clashCountB);
    }
    if(tick==30) { start(0,3,0x61); start(2,1,0x61); }
    if(tick==40) {
        camera_options(0,TAG_OPT_SHARED_MELEE,65);
        Port_TagTestCheck(BtlClash_State(0)==7 && BtlClash_State(2)==7,41);
        Port_TagTestCheck(TagTeam_Target(0)==3 && TagTeam_Target(3)==0,42);
        Port_TagTestCheck(TagTeam_Target(1)==2 && TagTeam_Target(2)==1,43);
        Port_TagTestCheck(!BtlClash_Join(0,1) && !BtlClash_Join(0,2),44);
    }
    if(tick>40 && tick<110) {
        BtlChar_Get(0)->clashCountB=30;
        BtlChar_Get(1)->clashCountB=30;
    }
    if(tick==250) {
        for(i=0;i<4;i++) Port_TagTestCheck(BtlClash_Opponent(i)==-1,45+i);
        start(2,3,0x62);
    }
    if(tick==270) {
        camera_options(2,TAG_OPT_SHARED_VANISH,67);
        Port_TagTestCheck(BtlClash_State(2)==10 && BtlAct_GetCurrent(BtlChar_Get(2))==0xFC,50);
        Port_TagTestCheck(BtlClash_Exchange(2)==BtlClash_Exchange(3),51);
        Port_TagTestCheck(BtlClash_Side(2)==0 && BtlClash_Side(3)==1,52);
        Port_TagTestCheck(BtlClash_Opponent(0)==-1 && BtlClash_Opponent(1)==-1,53);
    }
    if(tick>270 && tick<600) BtlChar_Get(2)->clashCountB=30;
    if(tick==650) {
        Port_TagTestCheck(BtlClash_Opponent(2)==-1 && BtlClash_Opponent(3)==-1,54);
        start(0,3,0x62);
    }
    if(tick==680) { BtlAct_ClearQueue(BtlChar_Get(3)); BtlAct_Request(BtlChar_Get(3),0xB); }
    if(tick==720) {
        Port_TagTestCheck(BtlClash_Opponent(0)==-1 && BtlClash_Opponent(3)==-1,55);
        Port_TagTestCheck(BtlAct_GetCurrent(BtlChar_Get(0))!=0xFC,56);
        Port_TagTestCheck(BtlClash_Join(0,3),57);
        BtlAct_Request(BtlChar_Get(0),0x130);
        BtlAct_Request(BtlChar_Get(3),0x130);
    }
    if(tick==740) {
        /* This interruption fixture enters the struggle handler without
           emitting beams. Its camera is covered by TagBeam_TestTick. */
        Port_TagTestCheck(BtlClash_State(3)==2 && BtlClash_State(0)==2,58);
        Port_TagTestCheck(BtlClash_State(1)==0 && BtlClash_State(2)==0,59);
    }
    if(tick>740 && tick<850) BtlChar_Get(0)->clashCountA=50;
    if(tick==1100) {
        for(i=0;i<4;i++) Port_TagTestCheck(BtlClash_Opponent(i)==-1,60+i);
        Port_TagTestCheck(1,99);
    }
}
