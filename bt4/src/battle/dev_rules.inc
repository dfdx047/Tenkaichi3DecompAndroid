/* Part of battle.c's rollback state, not render-thread settings. */
#include "../../port/include/bt4_dev_rules.h"
extern s32 Port_NetActive(void),Port_NetDevRulesPad(s32,u8 *);
static Bt4DevRules sDevRules;
static s32 DevRules_Online(void){return Port_NetActive()||TagBattle_Network();}
s32 DevRules_Flags(void){return DevRules_Online()?(s32)sDevRules.flags:0;}
s32 DevRules_ProtectHealth(void){return (DevRules_Flags()&BT4_DEV_HP)!=0;}
static void DevRules_Reset(void){memset(&sDevRules,0,sizeof(sDevRules));}
static s32 DevRules_NetworkTick(void){
 s32 actor,count=TagBattle_Active()?TagBattle_Count():2;unsigned before=sDevRules.flags;
 if(!DevRules_Online())return 0;sDevRules.step=0;
 for(actor=0;actor<count;actor++){
  u8 raw[8];
  if(TagBattle_Network())memcpy(raw,TagBattle_NetPad(actor)+9,8);
  else if(!Port_NetDevRulesPad(actor,raw))continue;
  Bt4Dev_Consume(&sDevRules,actor,raw);
 }
 if(before!=sDevRules.flags)fprintf(stderr,"shared Dev rules: flags=%u all-fighters=1\n",sDevRules.flags);
 if(BtlSeq_GetState()!=3)return 0;
 for(actor=0;actor<count;actor++){
  s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(actor));if(!g)continue;
  if((sDevRules.flags&BT4_DEV_HP)&&g[0]>0)g[0]=g[1];
  /* Maximum Power may temporarily exceed the normal Ki ceiling. Refill spent
     Ki without resetting that overcharge on the following shared frame. */
  if((sDevRules.flags&BT4_DEV_KI)&&g[3]<g[4])g[3]=g[4];
  if(sDevRules.flags&BT4_DEV_STOCK)g[5]=g[6];
 }
 return (sDevRules.flags&BT4_DEV_PAUSE)&&!sDevRules.step;
}

/* Freeze through the original pause flags, while still rebuilding transient
   effect records, texture uploads and visibility lists for every draw. */
static s32 DevPause_Begin(s32 paused){
 s32 own=paused&&!(Battle_GetWork()->flags&BATTLE_FLAG_PAUSE);
 if(own)Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;return own;
}
static void DevPause_End(s32 own){if(own)Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;}
