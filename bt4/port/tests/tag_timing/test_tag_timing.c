/* Native Tag input scheduling regression. No game assets, sockets or UI. */
#define BT4_TIMING_FIXED 1
#include <stdio.h>
#include <string.h>
#include "../../src/gs/tag_team_net.c"
void PortVoice_Leave(int mode){}
void Port_TagMenuSet(int a,int b){}
void Port_TagLobbySet(int a,int b,int c,int d){}
void Port_UiBanner(const char *text,int ms){}
void Port_PaceNetworkResume(unsigned long long waited){}
void PortTbo_Copy(const char *text){}
int Port_BuildIdentity(unsigned long long *out){return 1;}
int Bt4Discovery_TagCounts(unsigned long long content,int *out){return 0;}
/* This fixture exercises scheduling only; no discovery/RTC/media work. */
const char *Bt4Signal_Project(void){return "https://example.invalid";}
int TagPeer_Open(void){return 0;}
void TagPeer_Close(void){}
int TagPeer_Send(const unsigned char *p){return 0;}
int TagPeer_Read(unsigned char *p,uint64_t *at){return 0;}
int TagPeer_Status(void){return 0;}
int TagPeer_Ping(void){return -1;}
int TagPeer_Descriptor(char *p,unsigned n){return 0;}
int TagDirect_Available(void){return 0;}
void TagDirect_Close(void){}
int TagDirect_Active(void){return 0;}
int TagDirect_Pending(void){return 0;}
int TagDirect_Ping(void){return -1;}
void TagDirect_RelayPing(int ms){}
int TagDirect_Send(const unsigned char *p){return 0;}
int TagDirect_Read(unsigned char *p,uint64_t *at){return 0;}
void TagDirect_Session(const char *a,const char *b,const char *c,unsigned d){}
void TagDirect_State(const unsigned char *p){}
void PortVoice_P2PSession(int a,const char *b,const PortVoiceRoomMember *c,int d){}
int Upd_HexBytes(const char *a,unsigned char *b,size_t n){return 0;}
void Gs_NetworkWait(int ms){}
const char *Port_NetNameGet(void){return "Test";}
void Gs_HoldFrame(void){}
void PortVoice_Session(int a,const char *b,const char *c,const char *d,int e,const char *f){}
unsigned long long gPortNetWaitNs;
static void room(int humans){
    memset(&state,0,sizeof(state));member=101;
    Tt_Open(&state,member,"Test",2,123);
    Tt_FillCPU(&state,member,state.epoch,2);
    if(humans>1){state.players[1].member=102;state.players[1].cpu=0;}
    state.phase=TT_FIGHT;state.delay=0;
    Tt_InputsReset(&received,state.epoch,Tt_Count(&state));
    ws_mode=ws_started=1;local_room=0;tag_paced=0;tag_ping=170;
    tag_input_sent=0;tag_delay_margin=1;tag_delay_clean_frames=0;tag_delay_feedback_seen=0;
    tag_received_valid=0;request_serial=0;
    memset(&sTagWs,0,sizeof(sTagWs));InitializeCriticalSection(&sTagWs.lock);sTagWs.lockReady=1;
}
static void end(void){DeleteCriticalSection(&sTagWs.lock);memset(&sTagWs,0,sizeof(sTagWs));}
int main(void){
    unsigned char pad[18]={0,0,0,0,127,127},taken[10][18];
    int cpu_delay,cpu_ping,cpu_take,holes=0,i;unsigned previous=0;
    room(1);cpu_delay=Port_TagDelay();cpu_ping=Port_TagPing();
    Port_TagSendInput(cpu_delay,pad);cpu_take=Port_TagTakeFrame(taken);end();
    room(2);tag_paced=1;
    tag_delay_margin=8;tag_delay_feedback_seen=1;tag_delay_clean_frames=100;
    tag_input_epoch=state.epoch-1;
    Port_TagSendInput(3,pad);
    if(tag_delay_margin!=1||tag_delay_feedback_seen||tag_delay_clean_frames)return 11;
    pad[0]=0x20;pad[6]=pad[8]=1;Port_TagSendInput(8,pad);
    for(i=0;i<(int)sTagWs.outWrite;i++){
        unsigned frame=Ttw_Read(sTagWs.outgoing[i]+12);
        if(i&&frame!=previous+1)holes+=(int)(frame-previous-1);
        previous=frame;
    }
    printf("{\"cpu_delay\":%d,\"cpu_ping\":%d,\"cpu_first_frame_ready\":%d,\"network_schedule_holes\":%d,\"queued_frames\":%u",cpu_delay,cpu_ping,cpu_take,holes,sTagWs.outWrite);
#ifdef BT4_TIMING_FIXED
    { unsigned before=sTagWs.outWrite;
      unsigned char tap[18]={0x40,0,0,0,127,127,1,0,1},release[18]={0,0,0,0,127,127};
      int retained,once=1,margin_before;
      for(i=1;i<5;++i)if(sTagWs.outgoing[i][22]||sTagWs.outgoing[i][24])once=0;
      Port_TagSendInput(7,tap);Port_TagSendInput(9,release);
      retained=sTagWs.outWrite==before+1&&Ttw_Read(sTagWs.outgoing[before]+16)==0x40&&sTagWs.outgoing[before][22]==1&&sTagWs.outgoing[before][24]==1;
      tag_delay_margin=1;tag_delay_adjusted=0;
      for(i=0;i<3;++i)tag_delay_feedback(i,1);
      margin_before=tag_delay_margin;
      tag_delay_feedback(3,1);tag_delay_feedback(4,1);
      printf(",\"shrinking_lead_retains_taps\":%d,\"gap_does_not_repeat_commands\":%d,\"startup_margin\":%d,\"burst_margin\":%d",retained,once,margin_before,tag_delay_margin);
      if(!retained||!once||margin_before!=1||tag_delay_margin!=2)return 5;
    }
    end();room(1);
    { int ready=0;
      for(i=0;i<240;++i){pad[0]=i&255;if(!Port_TagSendInput(i,pad)||!Port_TagTakeFrame(taken)||taken[0][0]!=pad[0])return 6;++ready;}
      printf(",\"cpu_local_frames\":%d,\"cpu_input_packets_sent\":%u",ready,sTagWs.outWrite);
      if(sTagWs.outWrite!=0)return 7;
    }
    end();room(2);
    { unsigned char *packet=sTagWs.incoming[0];
      tag_ping=-1;tag_ping_count=tag_ping_cursor=0;tag_caps_sent=voice_ticket=1;
      request_history[1].serial=1;request_history[1].command=TTW_PING;request_history[1].at=100;tag_ping_ack=0;
      Ttw_Snapshot(packet,&state,member,0,"TEST00");Ttw_Write(packet+508,1);
      sTagWs.incomingAt[0]=140;sTagWs.inWrite=1;last_heard=time(NULL);
      Port_TagPoll();printf(",\"transport_rtt\":%d",Port_TagPing());if(Port_TagPing()!=40)return 8;
    }
    end();room(1);
    { TtRoom joined;
      Tt_Open(&state,member,"First entrant",2,123);state.delay=0;
      Tt_InputsReset(&received,state.epoch,1);joined=state;
      for(i=1;i<4;i++)if(Tt_Join(&joined,101+i,"Entrant",123)!=TT_OK)return 9;
      joined.phase=TT_ARMED;Ttw_Snapshot(sTagWs.incoming[0],&joined,member,0,"TEST00");
      joined.phase=TT_FIGHT;Ttw_Snapshot(sTagWs.incoming[1],&joined,member,0,"TEST00");
      Ttw_Request(sTagWs.incoming[2],TTW_FRAME,joined.epoch);Ttw_Write(sTagWs.incoming[2]+12,0);
      for(i=0;i<4;i++)memcpy(sTagWs.incoming[2]+16+i*TT_PAD_BYTES,pad,TT_PAD_BYTES);
      sTagWs.incomingAt[0]=sTagWs.incomingAt[1]=sTagWs.incomingAt[2]=SDL_GetTicks();
      sTagWs.inWrite=3;tag_caps_sent=voice_ticket=1;last_heard=time(NULL);error[0]=0;
      Port_TagPoll();
      if(error[0]||received.seats!=4||!Port_TagTakeFrame(taken))return 10;
      printf(",\"admission_roster_inputs\":1");
    }
#endif
    printf("}\n");
    end();return 0;
}
