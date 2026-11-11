#include <winsock2.h>
#include <ws2tcpip.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <string>
#include "../../src/gs/match_supabase.h"
#include "../../src/gs/supabase_signal.h"
#include "../../src/gs/voice.h"
#include "../../include/bt4_online_compat.h"
extern "C" void PortVoice_P2PSession(int,const char*,const PortVoiceRoomMember*,int){}
static bool address(const char *text,sockaddr_in &out){char copy[64];snprintf(copy,sizeof(copy),"%s",text);char *colon=strrchr(copy,':');if(!colon)return false;*colon++=0;out={};out.sin_family=AF_INET;out.sin_port=htons((unsigned short)atoi(colon));return inet_pton(AF_INET,copy,&out.sin_addr)==1;}
int main(int argc,char **argv){
    if(argc<3)return 1;WSADATA w;WSAStartup(MAKEWORD(2,2),&w);SDL_Init(0);
    bool host=!strcmp(argv[1],"host"),watch=!strcmp(argv[1],"watch");const char *name=host?"Host":watch?"Observer":"Fighter";
    SOCKET socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);sockaddr_in local={};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(socket,(sockaddr*)&local,sizeof(local)))return 2;int length=sizeof(local);getsockname(socket,(sockaddr*)&local,&length);u_long one=1;ioctlsocket(socket,FIONBIO,&one);
    char addresses[100];snprintf(addresses,sizeof(addresses),"[\"127.0.0.1:%u\"]",ntohs(local.sin_port));
    if(!Bt4Discovery_Begin(1,host?1:watch?4:2,0x42543414,BT4_ONLINE_COMPAT_ID,name,argc>3?argv[3]:"",addresses,"S"))return 3;
    FILE *log=fopen(argv[2],"w");bool code=false,onlyWatcher=false,foundFighter=false;unsigned received=0,next=0;Uint64 deadline=SDL_GetTicks()+30000,last=0,at=0;std::set<unsigned short> finished;
    while(SDL_GetTicks()<deadline){Bt4DiscoveryStatus status={};Bt4Discovery_Poll(1,&status);
        if(status.state<0){fprintf(log,"ERROR %s\n",status.error);fflush(log);break;}
        if(host&&status.code[0]&&!code){fprintf(log,"CODE %s\n",status.code);fflush(log);code=true;}
        if(host&&status.watchCount&&!foundFighter&&!onlyWatcher){if(status.state!=2)return 4;onlyWatcher=true;fprintf(log,"ONLY_SPECTATOR state=2\n");fflush(log);}
        if(host&&status.state==3){if(strcmp(status.peerName,"Fighter"))return 5;foundFighter=true;}
        if(!host&&status.state==3&&SDL_GetTicks()-last>=5&&received<100){sockaddr_in peer;if(!address(status.peerAddr[0],peer))return 6;
            unsigned payload[2]={0x53555041u,next};at=SDL_GetTicksNS();sendto(socket,(const char*)payload,sizeof(payload),0,(sockaddr*)&peer,sizeof(peer));last=SDL_GetTicks();}
        unsigned payload[2];sockaddr_in from;int size=sizeof(from),n;
        while((n=recvfrom(socket,(char*)payload,sizeof(payload),0,(sockaddr*)&from,&size))>0){
            if(n!=sizeof(payload))continue;
            if(host){if(payload[0]==0x53555041u){sendto(socket,(const char*)payload,n,0,(sockaddr*)&from,sizeof(from));}else if(payload[0]==0x444f4e45u)finished.insert(from.sin_port);}
            else if(payload[0]==0x53555041u&&payload[1]==next){++received;++next;if(received==100){fprintf(log,"UDP_ECHO 100 rtt_us=%llu\n",(unsigned long long)((SDL_GetTicksNS()-at)/1000));fflush(log);payload[0]=0x444f4e45u;sendto(socket,(const char*)payload,n,0,(sockaddr*)&from,sizeof(from));}}
        }
        if(host&&finished.size()==2&&foundFighter&&onlyWatcher){fprintf(log,"DONE ok=1 observer-separate=1\n");fflush(log);break;}
        if(!host&&received==100){SDL_Delay(250);break;}SDL_Delay(2);
    }
    bool ok=host?finished.size()==2&&foundFighter&&onlyWatcher:received==100;
    fprintf(log,"EXIT ok=%d\n",ok);fclose(log);Bt4Discovery_End(1);SDL_Delay(250);Bt4Signal_Close();closesocket(socket);SDL_Quit();return ok?0:7;
}
