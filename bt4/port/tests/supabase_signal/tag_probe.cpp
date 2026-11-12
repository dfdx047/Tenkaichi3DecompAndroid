#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include "../../src/gs/tag_team_p2p.h"
#include "../../src/gs/supabase_signal.h"
extern "C" {
#include "../../include/tag_team_wire.h"
}
static void request(int command,unsigned epoch,int a=0,int b=0,int c=0,uint64_t hash=0){
    unsigned char packet[512];Ttw_Request(packet,command,epoch);Ttw_Write(packet+12,a);Ttw_Write(packet+16,b);Ttw_Write(packet+20,c);
    Ttw_Write(packet+24,(unsigned)hash);Ttw_Write(packet+28,(unsigned)(hash>>32));TagPeer_Send(packet);
}
static bool all(const TtRoom &r,int field){for(int i=0;i<Tt_Count(&r);++i){auto &p=r.players[i];if(field==0&&!p.confirmed)return false;if(field==1&&!p.loaded)return false;if(field==2&&!p.armed)return false;if(field==3&&!p.ended)return false;}return true;}
int main(int argc,char **argv){
    if(argc<3)return 1;FILE *boot=fopen((std::string(argv[2])+".boot").c_str(),"w");if(boot){fprintf(boot,"entered\n");fflush(boot);}
    if(!SDL_Init(0))return 1;if(boot){fprintf(boot,"SDL initialized\n");fflush(boot);}bool hosting=!strcmp(argv[1],"host");const char *output=argv[2];
    if(!TagPeer_Open()){fprintf(stderr,"RTC runtime unavailable\n");return 2;}
    if(boot){fprintf(boot,"P2P worker started\n");fclose(boot);}
    unsigned char packet[512];Ttw_Request(packet,hosting?TTW_OPEN:TTW_JOIN,0);
    if(hosting){Ttw_Write(packet+12,2);Ttw_Write(packet+20,0);Ttw_Write(packet+52,1);Ttw_Write(packet+56,2);}else{if(argc<4)return 3;memcpy(packet+12,argv[3],6);}
    Ttw_Write(packet+24,0x54540008);Ttw_Write(packet+28,0x54540007);snprintf((char*)packet+32,20,"%s",hosting?"P2P Host":"P2P Guest");TagPeer_Send(packet);
    TtRoom room={};unsigned self=0,lastRevision=0xffffffffu,sentFrame=0,received=0,round=0,hash=2166136261u;bool haveCode=false,ended=false,cut=false,started=false;Uint64 deadline=SDL_GetTicks()+120000,nextInput=0;
    FILE *log=fopen(output,"w");if(!log)return 4;
    while(SDL_GetTicks()<deadline&&TagPeer_Status()>=0){
        uint64_t at;int work=0;
        while(work++<128&&TagPeer_Read(packet,&at)>0){
            unsigned cmd=Ttw_Read(packet+4);
            if(cmd==TTW_STATE){int result;char code[7];TtRoom next;unsigned member;
                if(!Ttw_Decode(packet,&next,&member,&result,code)){fprintf(stderr,"Bad room snapshot\n");return 5;}
                if(result){fprintf(stderr,"Rejected command: %d epoch=%u phase=%d\n",result,next.epoch,next.phase);return 6;}
                if(!haveCode){fprintf(log,"CODE %s\n",code);fflush(log);haveCode=true;}
                if(next.epoch!=room.epoch){if(!hosting&&ended)++round;received=sentFrame=0;hash=2166136261u;ended=started=false;nextInput=0;lastRevision=0xffffffffu;}
                if(next.phase==TT_FIGHT&&room.phase!=TT_FIGHT){fprintf(log,"START epoch=%u actors=%d\n",next.epoch,Tt_Count(&next));fflush(log);}
                room=next;self=member;
            }else if(cmd==TTW_FRAME&&Ttw_Read(packet+8)==room.epoch){
                unsigned frame=Ttw_Read(packet+12);if(frame!=received){fprintf(stderr,"Frame gap %u != %u\n",frame,received);return 7;}
                if(received<900)for(int i=0;i<Tt_Count(&room)*TT_PAD_BYTES;++i)hash=(hash^packet[16+i])*16777619u;
                ++received;
                if(received%300==0){fprintf(log,"FRAME %u epoch=%u hash=%08x rtt=%d\n",received,room.epoch,hash,TagPeer_Ping());fflush(log);}
            }
        }
        if(haveCode&&room.phase==TT_LOBBY&&Tt_Humans(&room)>=2&&hosting&&lastRevision!=room.revision){lastRevision=room.revision;request(TTW_SELECT,room.epoch);}
        if(room.phase==TT_SELECT&&lastRevision!=room.revision){
            for(int team=0;team<2;++team){int turn=Tt_Turn(&room,team);if(turn>=0&&(room.players[turn].member==self||(hosting&&room.players[turn].cpu))){
                lastRevision=room.revision;request(TTW_CHOOSE,room.epoch,100+turn,0,turn);break;}}
            if(hosting&&all(room,0)&&lastRevision!=room.revision){lastRevision=room.revision;request(TTW_LOAD,room.epoch,0,0,0,Tt_LoadDigest(&room));}
        }
        int seat=Tt_Seat(&room,self);
        if(room.phase==TT_LOADING&&seat>=0&&lastRevision!=room.revision){
            if(!room.players[seat].loaded){lastRevision=room.revision;request(TTW_LOADED,room.epoch,0,0,0,Tt_LoadDigest(&room));}
            else if(hosting&&all(room,1)){lastRevision=room.revision;request(TTW_ARM,room.epoch);}
        }
        if(room.phase==TT_ARMED&&seat>=0&&lastRevision!=room.revision){
            if(!room.players[seat].armed){lastRevision=room.revision;request(TTW_ACK,room.epoch);}
            else if(hosting&&all(room,2)){lastRevision=room.revision;request(TTW_START,room.epoch);}
        }
        Uint64 now=SDL_GetTicks();
        if(room.phase==TT_FIGHT){
            if(now>=nextInput&&!ended){nextInput=now+20;unsigned frame=std::max(sentFrame,received+3);unsigned char pad[TT_PAD_BYTES]={};pad[4]=pad[5]=127;
                Ttw_Request(packet,TTW_INPUT,room.epoch);Ttw_Write(packet+12,frame);Ttw_Write(pad,(frame%12<6?0x1000u:0x2000u)+(hosting?1:2));if(frame==30)pad[8]=1;
                memcpy(packet+16,pad,TT_PAD_BYTES);TagPeer_Send(packet);sentFrame=frame+1;}
            if(received>=120&&!cut){cut=true;Bt4Signal_Close();fprintf(log,"SIGNALING_DISCONNECTED\n");fflush(log);}
            if(received>=900&&!ended){ended=true;fprintf(log,"ROUND %u epoch=%u frames=900 hash=%08x rtt=%d\n",round,room.epoch,hash,TagPeer_Ping());fflush(log);request(TTW_RESULT_ENDED,room.epoch);}
            if(hosting&&ended&&all(room,3)&&lastRevision!=room.revision){lastRevision=room.revision;if(round==2)break;request(TTW_RESULT,room.epoch,round==0?0:1);++round;}
            if(!hosting&&ended&&round==2)break;
        }
        SDL_Delay(2);
    }
    bool ok=TagPeer_Status()>=0&&round>=2;
    fprintf(log,"DONE ok=%d rounds=%u status=%d signaling=%d\n",ok,round,TagPeer_Status(),Bt4Signal_Status());fclose(log);TagPeer_Close();Bt4Signal_Close();SDL_Quit();return ok?0:8;
}
