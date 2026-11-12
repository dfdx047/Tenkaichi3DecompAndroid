/* Native-only diagnostics and optional per-actor detail. No game state is changed. */
#include "../../include/tag_team_sync.h"
static int tag_caps_sent,tag_paced,tag_ping=-1,tag_input_sent,tag_sync_count;
static uint32_t tag_input_epoch,tag_input_latest,tag_input_first,tag_ping_ack,tag_sync_last_frame;
static uint32_t tag_pending_buttons;
static unsigned char tag_input_pad[TT_PAD_BYTES],tag_pending_l3,tag_pending_skip;
static unsigned tag_ping_samples[8],tag_ping_count,tag_ping_cursor;
static unsigned tag_delay_last_adjust_frame;static int tag_delay_adjusted;
static Uint64 tag_ping_at;
static uint32_t tag_received_epoch,tag_received_latest;
static int tag_received_valid;
static SDL_AtomicInt tag_catchup;
int Port_TagCatchup(void){return SDL_GetAtomicInt(&tag_catchup);}
static int tag_backlog(void){
    int distance;
    if(!tag_paced||state.phase!=TT_FIGHT||!tag_received_valid||tag_received_epoch!=received.epoch||received.epoch!=state.epoch)return 0;
    distance=(int32_t)(tag_received_latest-received.next);
    return distance>=0?distance+1:0;
}
static void tag_update_catchup(void){
    int backlog=tag_backlog(),old=Port_TagCatchup(),active=old;
    if(backlog>=6)active=1;else if(backlog<=2)active=0;
    SDL_SetAtomicInt(&tag_catchup,active);
    if(active!=old&&getenv("BT4_RENDER_TEST"))
        fprintf(stderr,"tag catchup: active=%d backlog=%d next=%u latest=%u\n",active,backlog,received.next,tag_received_latest);
}
static unsigned tag_sync_words[TT_MAX_PLAYERS][TT_SYNC_WORDS];
static char tag_sync_log[2048];
int Port_TagPing(void){return local_room||solo_cpu_fight()?0:peer_mode?TagPeer_Ping():tag_direct_route?TagDirect_Ping():tag_ping;}
/* Measure arrival at the transport, before a slow render/game thread polls it.
 * Like the duel meter, use the best recent RTT rather than game-loop backlog. */
static void tag_ping_sample(unsigned sample){
    unsigned i,best=sample;if(sample>=10000)return;
    tag_ping_samples[tag_ping_cursor++%8]=sample;
    if(tag_ping_count<8)++tag_ping_count;
    for(i=0;i<tag_ping_count;++i)if(tag_ping_samples[i]<best)best=tag_ping_samples[i];
    tag_ping=(int)best;
}
static int tag_delay_margin=1;
static unsigned tag_delay_clean_frames;
static uint32_t tag_delay_feedback_epoch,tag_delay_feedback_frame;
static int tag_delay_feedback_seen;
static void tag_delay_feedback(unsigned frame,unsigned late_mask){
    int seat=Tt_Seat(&state,member);
    if(!tag_paced||seat<0||!tag_input_sent||tag_input_epoch!=state.epoch||frame<tag_input_first||frame>tag_input_latest)return;
    /* Redundant authoritative packets are transport recovery, not more
     * evidence of late input. Count each timeline frame at most once. */
    if(tag_delay_feedback_seen&&tag_delay_feedback_epoch==state.epoch&&(int32_t)(frame-tag_delay_feedback_frame)<=0)return;
    tag_delay_feedback_epoch=state.epoch;tag_delay_feedback_frame=frame;tag_delay_feedback_seen=1;
    /* A stalled game thread may not have sampled these authoritative frames
     * at all. Their missing input is not evidence of a late network packet;
     * backlog compensation already schedules the next input on the live clock. */
    if(late_mask&(1u<<seat)){
        tag_delay_clean_frames=0;
        /* Startup padding and a burst of old notices must not ratchet the
         * input lead on every received packet. */
        if(tag_delay_margin<8&&(!tag_delay_adjusted||frame-tag_delay_last_adjust_frame>=8)){
            ++tag_delay_margin;tag_delay_last_adjust_frame=frame;tag_delay_adjusted=1;
        }
    }else if(++tag_delay_clean_frames>=342){
        if(tag_delay_margin>1)--tag_delay_margin;
        tag_delay_clean_frames=0;
    }
}
void Port_TagHashActor(int actor,unsigned x,unsigned y,unsigned z,unsigned hp,unsigned ki,unsigned action,unsigned target,unsigned team,unsigned chara,unsigned costume){
    unsigned words[TT_SYNC_WORDS]={x,y,z,hp,ki,action,target,team,chara,costume};
    if(actor==0)tag_sync_count=0;
    if(actor<0||actor>=TT_MAX_PLAYERS)return;
    memcpy(tag_sync_words[actor],words,sizeof(words));tag_sync_count=actor+1;
}
static void tag_sync_notice(const unsigned char *p){
    static const char *fields[]={"position-x","position-y","position-z","hp","ki","action","target","team","character","costume"};
    int field=(int)Ttw_Read(p+28),i;char line[640];FILE *f;const char *base=SDL_GetBasePath();
    snprintf(line,sizeof(line),"tag sync: epoch=%u frame=%u kind=%u streak=%u actor=%d field=%s peer-a=%d value-a=%d peer-b=%d value-b=%d local-last-sent-frame=%u\n",
        Ttw_Read(p+8),Ttw_Read(p+12),Ttw_Read(p+16),Ttw_Read(p+20),(int)Ttw_Read(p+24),
        field>=0&&field<TT_SYNC_WORDS?fields[field]:"legacy-aggregate",(int)Ttw_Read(p+32),(int)Ttw_Read(p+40),
        (int)Ttw_Read(p+36),(int)Ttw_Read(p+44),tag_sync_last_frame);
    fputs(line,stderr);
    if(!tag_sync_log[0]){
        char folder[1800];unsigned pid=0;
#ifdef _WIN32
        pid=(unsigned)GetCurrentProcessId();
#else
        pid=(unsigned)getpid();
#endif
        snprintf(folder,sizeof(folder),"%stbo/logs",base?base:"");SDL_CreateDirectory(folder);
        snprintf(tag_sync_log,sizeof(tag_sync_log),"%s/tag-sync-%lld-%u.txt",folder,(long long)time(NULL),pid);
    }
    f=fopen(tag_sync_log,"a");if(!f)return;fputs(line,f);
    for(i=0;i<tag_sync_count;++i){unsigned *w=tag_sync_words[i];
        fprintf(f,"local-last-sent actor=%d pos16=%d,%d,%d hp=%u ki=%u action=%u target=%u team=%u char=%u costume=%u\n",i,
            (int)w[0],(int)w[1],(int)w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9]);
    }fclose(f);
}
