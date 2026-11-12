/* Load the shipped RTC C API and exchange real encrypted SCTP messages on
 * loopback. No microphone, audio device, STUN server, ISO or game is opened. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rtc/rtc.h"
#define API(name) static __typeof__(&name) p_##name
API(rtcInitLogger);API(rtcCreatePeerConnection);API(rtcDeletePeerConnection);
API(rtcSetUserPointer);API(rtcSetLocalDescriptionCallback);API(rtcSetLocalCandidateCallback);
API(rtcSetRemoteDescription);API(rtcSetLocalDescription);API(rtcAddRemoteCandidate);
API(rtcCreateDataChannelEx);API(rtcSetDataChannelCallback);API(rtcSetOpenCallback);
API(rtcSetMessageCallback);API(rtcSendMessage);API(rtcGetBufferedAmount);API(rtcCleanup);
API(rtcGetDataChannelProtocol);API(rtcGetDataChannelReliability);
#undef API
typedef struct {
    int index,pc;SDL_AtomicInt channel,opened,remoteReady;
    int pendingCount;char candidates[32][2048],mids[32][64];
} Peer;
static Peer peers[2];static SDL_Mutex *lock;
static SDL_AtomicInt failed,stopping,received;
static int unreliable_mode;

static void RTC_API message(int channel,const char *data,int size,void *ptr){
    Peer *peer=(Peer*)ptr;int i;
    if(SDL_GetAtomicInt(&stopping))return;
    if(size!=512){SDL_SetAtomicInt(&failed,1);return;}
    for(i=1;i<size;++i)if((unsigned char)data[i]!=(unsigned char)(i*31+(unsigned char)data[0])){SDL_SetAtomicInt(&failed,2);return;}
    if(peer->index==1){if(p_rtcSendMessage(channel,data,size)<0)SDL_SetAtomicInt(&failed,3);}
    else SDL_SetAtomicInt(&received,(unsigned char)data[0]+1);
}
static void RTC_API opened(int channel,void *ptr){
    Peer *peer=(Peer*)ptr;(void)channel;
    if(!SDL_GetAtomicInt(&stopping))SDL_SetAtomicInt(&peer->opened,1);
}
static void attach(Peer *peer,int channel){
    rtcReliability reliability={0};char protocol[64];
    if(p_rtcGetDataChannelProtocol(channel,protocol,sizeof(protocol))<0||strcmp(protocol,"bt4-tag-input-v1")||
       p_rtcGetDataChannelReliability(channel,&reliability)<0||!reliability.unordered||
       reliability.unreliable!=(unreliable_mode!=0)||(unreliable_mode&&reliability.maxRetransmits!=0)){
        SDL_SetAtomicInt(&failed,16);return;
    }
    SDL_SetAtomicInt(&peer->channel,channel);p_rtcSetUserPointer(channel,peer);
    p_rtcSetMessageCallback(channel,message);p_rtcSetOpenCallback(channel,opened);
}
static void RTC_API datachannel(int pc,int channel,void *ptr){
    Peer *peer=(Peer*)ptr;(void)pc;
    if(!SDL_GetAtomicInt(&stopping))attach(peer,channel);
}
static void RTC_API description(int pc,const char *sdp,const char *type,void *ptr){
    Peer *peer=(Peer*)ptr,*other=&peers[1-peer->index];int count,i;(void)pc;
    if(SDL_GetAtomicInt(&stopping))return;
    if(p_rtcSetRemoteDescription(other->pc,sdp,type)<0){SDL_SetAtomicInt(&failed,4);return;}
    SDL_LockMutex(lock);SDL_SetAtomicInt(&other->remoteReady,1);count=other->pendingCount;other->pendingCount=0;SDL_UnlockMutex(lock);
    for(i=0;i<count;++i)if(p_rtcAddRemoteCandidate(other->pc,other->candidates[i],other->mids[i])<0)SDL_SetAtomicInt(&failed,5);
    if(!strcmp(type,"offer")&&p_rtcSetLocalDescription(other->pc,"answer")<0)SDL_SetAtomicInt(&failed,6);
}
static void RTC_API candidate(int pc,const char *value,const char *mid,void *ptr){
    Peer *peer=(Peer*)ptr,*other=&peers[1-peer->index];int ready;(void)pc;
    if(SDL_GetAtomicInt(&stopping))return;
    SDL_LockMutex(lock);ready=SDL_GetAtomicInt(&other->remoteReady);
    if(!ready){
        int at=other->pendingCount;
        if(at>=32)SDL_SetAtomicInt(&failed,7);
        else{SDL_strlcpy(other->candidates[at],value,sizeof(other->candidates[at]));SDL_strlcpy(other->mids[at],mid,sizeof(other->mids[at]));++other->pendingCount;}
    }
    SDL_UnlockMutex(lock);
    if(ready&&p_rtcAddRemoteCandidate(other->pc,value,mid)<0)SDL_SetAtomicInt(&failed,8);
}

int main(int argc,char **argv){
    SDL_SharedObject *library;rtcConfiguration config={0};rtcDataChannelInit init={0};Uint64 deadline,start;int i,channel,ok=0;
    if(argc<2||argc>3)return 10;
    unreliable_mode=argc==3&&!strcmp(argv[2],"unreliable");
    library=SDL_LoadObject(argv[1]);if(!library){fprintf(stderr,"RTC library load failed: %s\n",SDL_GetError());return 11;}
#define LOAD(name) p_##name=(__typeof__(p_##name))SDL_LoadFunction(library,#name);if(!p_##name){fprintf(stderr,"Missing RTC export: %s\n",#name);return 12;}
    LOAD(rtcInitLogger);LOAD(rtcCreatePeerConnection);LOAD(rtcDeletePeerConnection);
    LOAD(rtcSetUserPointer);LOAD(rtcSetLocalDescriptionCallback);LOAD(rtcSetLocalCandidateCallback);
    LOAD(rtcSetRemoteDescription);LOAD(rtcSetLocalDescription);LOAD(rtcAddRemoteCandidate);
    LOAD(rtcCreateDataChannelEx);LOAD(rtcSetDataChannelCallback);LOAD(rtcSetOpenCallback);
    LOAD(rtcSetMessageCallback);LOAD(rtcSendMessage);LOAD(rtcGetBufferedAmount);LOAD(rtcCleanup);
    LOAD(rtcGetDataChannelProtocol);LOAD(rtcGetDataChannelReliability);
#undef LOAD
    p_rtcInitLogger(RTC_LOG_NONE,NULL);lock=SDL_CreateMutex();if(!lock)return 13;
    config.bindAddress="127.0.0.1";config.disableAutoNegotiation=true;
    for(i=0;i<2;++i){peers[i].index=i;peers[i].pc=p_rtcCreatePeerConnection(&config);SDL_SetAtomicInt(&peers[i].channel,-1);if(peers[i].pc<0){SDL_SetAtomicInt(&failed,9);goto done;}}
    for(i=0;i<2;++i){
        p_rtcSetUserPointer(peers[i].pc,&peers[i]);p_rtcSetLocalDescriptionCallback(peers[i].pc,description);
        p_rtcSetLocalCandidateCallback(peers[i].pc,candidate);p_rtcSetDataChannelCallback(peers[i].pc,datachannel);
    }
    init.reliability.unordered=true;init.reliability.unreliable=unreliable_mode!=0;
    init.reliability.maxRetransmits=0;init.protocol="bt4-tag-input-v1";
    channel=p_rtcCreateDataChannelEx(peers[0].pc,"bt4-local-regression",&init);
    if(channel<0){SDL_SetAtomicInt(&failed,10);goto done;}attach(&peers[0],channel);
    if(p_rtcSetLocalDescription(peers[0].pc,"offer")<0){SDL_SetAtomicInt(&failed,11);goto done;}
    deadline=SDL_GetTicks()+10000;
    while(!SDL_GetAtomicInt(&failed)&&SDL_GetTicks()<deadline&&(!SDL_GetAtomicInt(&peers[0].opened)||!SDL_GetAtomicInt(&peers[1].opened)))SDL_Delay(1);
    if(!SDL_GetAtomicInt(&peers[0].opened)||!SDL_GetAtomicInt(&peers[1].opened)){SDL_SetAtomicInt(&failed,12);goto done;}
    start=SDL_GetTicksNS();
    for(i=0;i<16&&!SDL_GetAtomicInt(&failed);++i){
        unsigned char bytes[512];int j;bytes[0]=(unsigned char)i;for(j=1;j<512;++j)bytes[j]=(unsigned char)(j*31+i);
        if(p_rtcGetBufferedAmount(channel)<0||p_rtcSendMessage(channel,(const char*)bytes,sizeof(bytes))<0){SDL_SetAtomicInt(&failed,13);break;}
        deadline=SDL_GetTicks()+2000;
        while(SDL_GetAtomicInt(&received)<i+1&&!SDL_GetAtomicInt(&failed)&&SDL_GetTicks()<deadline)SDL_Delay(1);
        if(SDL_GetAtomicInt(&received)!=i+1)SDL_SetAtomicInt(&failed,14);
    }
    if(!SDL_GetAtomicInt(&failed)){
        printf("{\"exports_verified\":true,\"native_sctp_peers\":2,\"messages\":32,\"bytes_per_message\":512,\"mode\":\"%s\",\"protocol\":\"bt4-tag-input-v1\",\"exchange_ms\":%.3f,\"loopback_only\":true,\"physical_audio\":false}\n",unreliable_mode?"unordered-unreliable-zero-retries":"unordered-reliable",(SDL_GetTicksNS()-start)/1000000.0);ok=1;
    }
done:
    SDL_SetAtomicInt(&stopping,1);
    for(i=0;i<2;++i)if(peers[i].pc>=0)p_rtcDeletePeerConnection(peers[i].pc);
    p_rtcCleanup();SDL_DestroyMutex(lock);
    if(!ok)fprintf(stderr,"RTC SCTP regression failed: %d\n",SDL_GetAtomicInt(&failed));
    return ok?0:15;
}
