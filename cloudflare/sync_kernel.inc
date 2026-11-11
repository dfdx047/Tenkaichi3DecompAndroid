/* Compare complete authenticated samples, not the arrival order of peers.
 * Presentation/language/revision are not sampled. Only positions have a tolerance.
 * Persistent divergence is reported separately from a load-data incompatibility. */
#include "../port/include/tag_team_sync.h"
static unsigned sync_frame[16],sync_mask[16],sync_detail[16],sync_hash[16][TT_MAX_PLAYERS];
static unsigned sync_words[16][TT_MAX_PLAYERS][TT_MAX_PLAYERS*TT_SYNC_WORDS];
static unsigned sync_last,sync_have_last,sync_streak,sync_serial,sync_kind,sync_at;
static int sync_actor=-1,sync_field=-1,sync_a=-1,sync_b=-1;
static unsigned sync_value_a,sync_value_b;
unsigned Kernel_SyncSerial(void){return sync_serial;}
unsigned Kernel_SyncKind(void){return sync_kind;}
unsigned Kernel_SyncFrame(void){return sync_at;}
unsigned Kernel_SyncStreak(void){return sync_streak;}
int Kernel_SyncActor(void){return sync_actor;}
int Kernel_SyncField(void){return sync_field;}
int Kernel_SyncSeatA(void){return sync_a;}
int Kernel_SyncSeatB(void){return sync_b;}
unsigned Kernel_SyncValueA(void){return sync_value_a;}
unsigned Kernel_SyncValueB(void){return sync_value_b;}
static void sync_reset(void){
    memset(sync_mask,0,sizeof(sync_mask));memset(sync_frame,0,sizeof(sync_frame));
    memset(sync_detail,0,sizeof(sync_detail));sync_last=sync_have_last=sync_streak=sync_kind=0;
}
static int sync_sample(unsigned member,unsigned epoch,unsigned frame,unsigned hash){
    int seat=Tt_Seat(&room,member),slot=(frame/30)&15,i,j,first=-1,count=Tt_Count(&room),different=0,rounding=0;
    unsigned mask=0,bit;
    if(room.phase!=TT_FIGHT||epoch!=room.epoch)return TT_BAD_PHASE;
    if(seat<0)return TT_BAD_MEMBER;
    if(frame%30||frame>0x7fffffff)return TT_BAD_CHOICE;
    if(sync_have_last&&frame<=sync_last)return TT_OK;
    if(sync_frame[slot]!=frame){sync_frame[slot]=frame;sync_mask[slot]=sync_detail[slot]=0;}
    bit=1u<<seat;
    if(sync_mask[slot]&bit)return sync_hash[slot][seat]==hash?TT_OK:TT_BAD_CHOICE;
    if(Ttw_Read(input+24)==TT_SYNC_MAGIC){
        if(Ttw_Read(input+20)!=(unsigned)count||count<2||count>TT_MAX_PLAYERS)return TT_BAD_CHOICE;
        for(j=0;j<count*TT_SYNC_WORDS;++j)sync_words[slot][seat][j]=Ttw_Read(input+TT_SYNC_OFFSET+j*4);
        sync_detail[slot]|=bit;
    }
    sync_hash[slot][seat]=hash;sync_mask[slot]|=bit;
    for(i=0;i<count;++i)if(room.players[i].member&&!room.players[i].cpu&&!(inactive_mask&(1u<<i))){mask|=1u<<i;if(first<0)first=i;}
    if((sync_mask[slot]&mask)!=mask)return TT_OK;
    sync_actor=sync_field=sync_a=sync_b=-1;sync_value_a=sync_value_b=0;
    for(i=0;i<count&&!different;++i)if(i!=first&&(mask&(1u<<i))){
        if((sync_detail[slot]&(1u<<first))&&(sync_detail[slot]&(1u<<i))){
            for(j=0;j<count*TT_SYNC_WORDS;++j){
                unsigned a=sync_words[slot][first][j],b=sync_words[slot][i][j];
                if(a==b)continue;
                if(j%TT_SYNC_WORDS<3){
                    long long delta=(long long)(int)a-(long long)(int)b;
                    if(delta>=-TT_SYNC_POSITION_TOLERANCE&&delta<=TT_SYNC_POSITION_TOLERANCE){rounding=1;continue;}
                }
                different=1;sync_actor=j/TT_SYNC_WORDS;sync_field=j%TT_SYNC_WORDS;
                sync_a=first;sync_b=i;sync_value_a=a;sync_value_b=b;break;
            }
        }else if(sync_hash[slot][first]!=sync_hash[slot][i]){
            different=1;sync_a=first;sync_b=i;sync_value_a=sync_hash[slot][first];sync_value_b=sync_hash[slot][i];
        }
    }
    sync_kind=different?TT_SYNC_DIFFERENT:rounding?TT_SYNC_ROUNDING:TT_SYNC_EQUAL;
    sync_streak=different?(sync_have_last&&frame==sync_last+30?sync_streak+1:1):0;
    sync_at=sync_last=frame;sync_have_last=1;++sync_serial;
    if(sync_streak>=TT_SYNC_FAILURE_LIMIT){room.phase=TT_ABORTED;++room.revision;return TT_DESYNC;}
    return TT_OK;
}
