/* Authenticated, voice-independent Tag Team transport. Only the committed host
 * runs the shared kernel. RTC and TLS carry duplicates of that same timeline. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <deque>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <memory>
#include "tag_team_direct.h"
#include "update_sign.h"
#include "voice_platform.inc"
#include "../../third_party/voice/rtc/rtc.h"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
#ifdef _WIN32
#include "voice_transport.inc"
#else
#include "voice_transport_curl.inc"
#endif
extern "C" {
#include "../../include/tag_team_wire.h"
int Kernel_ImportArmed(const unsigned char *);
unsigned char *Kernel_Input(void),*Kernel_Output(void);
int Kernel_Command(unsigned),Kernel_TakeFrame(void);
unsigned Kernel_DeadlineFrame(void),Kernel_Epoch(void),Kernel_Host(void),Kernel_PendingFrame(void);
int Kernel_Phase(void),Kernel_Seat(unsigned);
void Kernel_Leave(unsigned),Kernel_Abort(void);
}
using LinkJson=nlohmann::json;
namespace {
constexpr unsigned LINK_MAGIC=0x314b4c54u,LINK_PING=107,LINK_PONG=108;
struct DirectRtcApi {
    VoiceModule module=nullptr;bool ready=false,attempted=false;std::mutex lock;
#define LF(n) decltype(&::n) n=nullptr;
    LF(rtcCreatePeerConnection) LF(rtcClosePeerConnection) LF(rtcDeletePeerConnection)
    LF(rtcSetUserPointer) LF(rtcSetLocalDescriptionCallback) LF(rtcSetLocalCandidateCallback)
    LF(rtcSetRemoteDescription) LF(rtcSetLocalDescription) LF(rtcAddRemoteCandidate)
    LF(rtcSetStateChangeCallback) LF(rtcSetDataChannelCallback) LF(rtcCreateDataChannelEx)
    LF(rtcDeleteDataChannel) LF(rtcSetOpenCallback) LF(rtcSetClosedCallback)
    LF(rtcSetMessageCallback) LF(rtcSendMessage) LF(rtcIsOpen) LF(rtcGetBufferedAmount)
    LF(rtcGetDataChannelProtocol) LF(rtcGetDataChannelReliability) LF(rtcGetSelectedCandidatePair)
#undef LF
    bool load(){std::lock_guard<std::mutex> g(lock);if(attempted)return ready;attempted=true;
        module=voiceLibrary("libdatachannel.dll","libdatachannel.so","libdatachannel.dylib");if(!module)return false;
#define LF(n) n=reinterpret_cast<decltype(n)>(voiceFunction(module,#n));if(!n)return false;
        LF(rtcCreatePeerConnection) LF(rtcClosePeerConnection) LF(rtcDeletePeerConnection)
        LF(rtcSetUserPointer) LF(rtcSetLocalDescriptionCallback) LF(rtcSetLocalCandidateCallback)
        LF(rtcSetRemoteDescription) LF(rtcSetLocalDescription) LF(rtcAddRemoteCandidate)
        LF(rtcSetStateChangeCallback) LF(rtcSetDataChannelCallback) LF(rtcCreateDataChannelEx)
        LF(rtcDeleteDataChannel) LF(rtcSetOpenCallback) LF(rtcSetClosedCallback)
        LF(rtcSetMessageCallback) LF(rtcSendMessage) LF(rtcIsOpen) LF(rtcGetBufferedAmount)
        LF(rtcGetDataChannelProtocol) LF(rtcGetDataChannelReliability) LF(rtcGetSelectedCandidatePair)
#undef LF
        ready=true;return true;}
} rtc;
struct Wire {std::array<unsigned char,512> bytes;Uint64 at;};
struct Event {int kind=0,pc=-1,channel=-1;unsigned from=0;Uint64 at=0;std::string a,b;std::vector<unsigned char> bytes;};
struct DirectService {
    std::mutex lock;SDL_Thread *thread=nullptr;SDL_Semaphore *wake=nullptr;
    std::atomic<bool> quit{false};std::atomic<int> active{0},host{0},ping{-1},relayPing{-1},queuedFrames{0};
    std::string endpoint,code,cap;unsigned self=0;
    std::deque<Wire> input,frames;std::deque<Event> events;
    std::array<unsigned char,512> snapshot={};unsigned snapshotSerial=0;
    static void notify(void *context){auto *s=static_cast<DirectService*>(context);if(s->wake)SDL_SignalSemaphore(s->wake);}
    void event(Event e){std::lock_guard<std::mutex> g(lock);if(quit)return;if(events.size()>=4096){active=-1;return;}events.push_back(std::move(e));notify(this);}
} service;
static unsigned number(const LinkJson &j,const char *key,unsigned fallback=0){auto i=j.find(key);return i!=j.end()&&i->is_number_unsigned()?i->get<unsigned>():fallback;}
static std::string text(const LinkJson &j,const char *key){auto i=j.find(key);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
static std::string hex(const unsigned char *bytes,size_t size){const char h[]="0123456789abcdef";std::string out;out.reserve(size*2);for(size_t i=0;i<size;++i){out+=h[bytes[i]>>4];out+=h[bytes[i]&15];}return out;}
struct Peer {
    std::atomic<unsigned> id{0};std::atomic<int> pc{-1},channel{-1};
    bool remote=false,probed=false;int rtt=-1;std::vector<LinkJson> candidates;
    struct Probe {unsigned serial=0;Uint64 at=0;};std::array<Probe,32> probes={};unsigned nextProbe=0;
};
/* Callback records outlive any session thread. Stale callbacks must meet a
 * current PC/channel id before they can enqueue work for a later session. */
static std::array<Peer,10> stablePeers;
struct Session {
    std::unique_ptr<VoiceWs> ws;std::array<Peer,10> &peers=stablePeers;
    unsigned self=0,host=0,epoch=0,lo=0,hi=0;std::string generation,digest;
    std::vector<std::string> ice={"stun:stun.cloudflare.com:3478"};
    bool prepared=false,committed=false,clockRunning=false,seenFightSnapshot=false;Uint64 nextTick=0,nextPing=0,nextReady=0,reconnect=0,nextReplay=0,diagnostic=0,keepalive=0,resumeAt=0;
    unsigned snapshotSerial=0,relayNext=0;bool relaySynchronized=false;std::deque<Wire> recentInput,recentFrames,hostHistory,recoveryHistory;
    std::array<unsigned,8> pingSamples={};unsigned pingCount=0,pingCursor=0;
    struct Seen {bool valid=false;unsigned frame=0;std::array<unsigned char,512> bytes;};std::array<Seen,512> seen;
    Session(){self=service.self;}
    Peer *find(unsigned id){for(auto &p:peers)if(p.id==id)return &p;return nullptr;}
    void post(const LinkJson &j){if(ws)ws->text(j.dump());}
    LinkJson control(const char *type){return {{"type",type},{"version",1},{"epoch",epoch},{"generation",generation}};}
    void close(Peer &p){int pc=p.pc.exchange(-1),dc=p.channel.exchange(-1);p.id=0;
        if(dc>=0)rtc.rtcDeleteDataChannel(dc);if(pc>=0){rtc.rtcClosePeerConnection(pc);rtc.rtcDeletePeerConnection(pc);}
        p.remote=p.probed=false;p.rtt=-1;p.candidates.clear();p.probes={};}
    void relayOnly(Peer &p){unsigned id=p.id;close(p);p.id=id;}
    void reset(bool disconnect){for(auto &p:peers)if(p.id)close(p);prepared=committed=clockRunning=seenFightSnapshot=relaySynchronized=false;relayNext=0;digest.clear();recentInput.clear();recentFrames.clear();hostHistory.clear();recoveryHistory.clear();for(auto &f:seen)f.valid=false;pingCount=pingCursor=0;service.active=0;service.host=0;service.ping=-1;Kernel_Abort();{std::lock_guard<std::mutex> g(service.lock);service.frames.clear();service.queuedFrames=0;service.input.clear();}if(disconnect)ws.reset();}
    static void RTC_API description(int pc,const char *sdp,const char *type,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc||!sdp||!type)return;Event e;e.kind=1;e.pc=pc;e.from=p.id;e.a=type;e.b=sdp;service.event(std::move(e));}
    static void RTC_API candidate(int pc,const char *candidate,const char *mid,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc||!candidate||!mid)return;Event e;e.kind=2;e.pc=pc;e.from=p.id;e.a=candidate;e.b=mid;service.event(std::move(e));}
    static void RTC_API channel(int pc,int dc,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc)return;Event e;e.kind=3;e.pc=pc;e.channel=dc;e.from=p.id;service.event(std::move(e));}
    static void RTC_API changed(int pc,rtcState state,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc)return;if(state==RTC_FAILED||state==RTC_CLOSED){Event e;e.kind=4;e.pc=pc;e.from=p.id;service.event(std::move(e));}}
    static void RTC_API opened(int dc,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.channel!=dc)return;Event e;e.kind=6;e.pc=p.pc;e.channel=dc;e.from=p.id;service.event(std::move(e));}
    static void RTC_API message(int dc,const char *data,int size,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.channel!=dc||size!=528)return;Event e;e.kind=5;e.channel=dc;e.pc=p.pc;e.from=p.id;e.at=SDL_GetTicks();e.bytes.assign(data,data+size);service.event(std::move(e));}
    void bind(Peer &p,int dc){char protocol[64];rtcReliability reliability={};
        if(rtc.rtcGetDataChannelProtocol(dc,protocol,sizeof(protocol))<0||strcmp(protocol,"bt4-tag-input-v1")||rtc.rtcGetDataChannelReliability(dc,&reliability)<0||!reliability.unordered||!reliability.unreliable||reliability.maxRetransmits!=0||(p.channel>=0&&p.channel!=dc)){rtc.rtcDeleteDataChannel(dc);return;}
        p.channel=dc;rtc.rtcSetUserPointer(dc,&p);rtc.rtcSetOpenCallback(dc,opened);rtc.rtcSetMessageCallback(dc,message);}
    void open(Peer &p){if(!rtc.ready)return;std::vector<const char*> list;for(auto &x:ice)list.push_back(x.c_str());rtcConfiguration cfg={};cfg.iceServers=list.data();cfg.iceServersCount=(int)list.size();cfg.disableAutoNegotiation=true;cfg.maxMessageSize=2048;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ICE_RELAY_TEST"))cfg.iceTransportPolicy=RTC_TRANSPORT_POLICY_RELAY;
        int pc=rtc.rtcCreatePeerConnection(&cfg);if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag direct RTC: self=%u peer=%u pc=%d\n",self,p.id.load(),pc);if(pc<0)return;p.pc=pc;rtc.rtcSetUserPointer(pc,&p);rtc.rtcSetLocalDescriptionCallback(pc,description);rtc.rtcSetLocalCandidateCallback(pc,candidate);rtc.rtcSetDataChannelCallback(pc,channel);rtc.rtcSetStateChangeCallback(pc,changed);
        if(self==host){rtcDataChannelInit init={};init.reliability.unordered=true;init.reliability.unreliable=true;init.reliability.maxRetransmits=0;init.protocol="bt4-tag-input-v1";int dc=rtc.rtcCreateDataChannelEx(pc,"BT4 Tag Team",&init);if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag direct RTC channel: peer=%u dc=%d\n",p.id.load(),dc);if(dc<0){close(p);return;}bind(p,dc);int result=rtc.rtcSetLocalDescription(pc,"offer");if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag direct RTC offer: peer=%u result=%d\n",p.id.load(),result);}}
    void configureIce(const LinkJson &j){auto values=j.find("iceServers");if(values==j.end()||!values->is_array())return;
        auto encode=[](const std::string &s){const char h[]="0123456789ABCDEF";std::string out;for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=c;else{out+='%';out+=h[c>>4];out+=h[c&15];}}return out;};
        std::vector<std::string> list={"stun:stun.cloudflare.com:3478"};for(const auto &entry:*values){if(!entry.is_object())continue;auto urls=entry.find("urls");if(urls==entry.end())continue;std::vector<std::string> strings;if(urls->is_string())strings.push_back(urls->get<std::string>());else if(urls->is_array())for(auto &u:*urls)if(u.is_string())strings.push_back(u.get<std::string>());
            auto user=text(entry,"username"),pass=text(entry,"credential");if(user.size()>512||pass.size()>512)continue;for(auto &url:strings){size_t prefix=url.rfind("turn:",0)==0?5:url.rfind("turns:",0)==0?6:0;if(prefix&&!user.empty()&&!pass.empty()&&url.size()<1024&&list.size()<10)list.push_back(url.substr(0,prefix)+encode(user)+":"+encode(pass)+"@"+url.substr(prefix));}}
        ice=std::move(list);}
    std::array<unsigned char,528> envelope(const unsigned char *packet){std::array<unsigned char,528> b={};Ttw_Write(b.data(),LINK_MAGIC);Ttw_Write(b.data()+4,lo);Ttw_Write(b.data()+8,hi);Ttw_Write(b.data()+12,self);memcpy(b.data()+16,packet,512);return b;}
    void rtcSend(Peer &p,const unsigned char *packet){int dc=p.channel;if(dc>=0&&rtc.rtcIsOpen(dc)&&rtc.rtcGetBufferedAmount(dc)<65536){auto b=envelope(packet);rtc.rtcSendMessage(dc,(const char*)b.data(),(int)b.size());}}
    void relay(const unsigned char *packet){if(ws&&ws->status==1){auto b=envelope(packet);ws->send(b.data(),b.size(),true);}}
    void deliver(const unsigned char *packet,Uint64 at){unsigned frame=Ttw_Read(packet+12);auto &seenFrame=seen[frame%seen.size()];if(seenFrame.valid&&seenFrame.frame==frame){if(memcmp(seenFrame.bytes.data(),packet,512))service.active=-1;return;}seenFrame.valid=true;seenFrame.frame=frame;memcpy(seenFrame.bytes.data(),packet,512);
        std::lock_guard<std::mutex> g(service.lock);if(service.frames.size()>=1024){service.active=-1;return;}Wire f;memcpy(f.bytes.data(),packet,512);f.at=at;service.frames.push_back(std::move(f));service.queuedFrames=(int)service.frames.size();Port_TagIncomingWake();}
    void sample(unsigned ms){if(ms>=10000)return;pingSamples[pingCursor++%8]=ms;if(pingCount<8)++pingCount;unsigned best=ms;for(unsigned i=0;i<pingCount;++i)best=std::min(best,pingSamples[i]);service.ping=(int)best;}
    void input(unsigned from,const unsigned char *packet){if(!committed||self!=host||Ttw_Read(packet+8)!=epoch)return;memcpy(Kernel_Input(),packet,512);int rc=Kernel_Command(from);if(rc!=TT_OK&&rc!=TT_STALE)return;
        if(from==self&&!clockRunning){clockRunning=true;nextTick=SDL_GetTicksNS();}}
    void binary(const unsigned char *bytes,size_t size,unsigned source,bool direct,Uint64 at){if(size!=528||Ttw_Read(bytes)!=LINK_MAGIC||Ttw_Read(bytes+4)!=lo||Ttw_Read(bytes+8)!=hi)return;const auto *packet=bytes+16;unsigned from=Ttw_Read(bytes+12),cmd=Ttw_Read(packet+4);if(direct&&from!=source)return;if(Ttw_Read(packet)!=TT_PROTOCOL||Ttw_Read(packet+8)!=epoch)return;
        auto *p=find(from);if(!p||p->id==self)return;
        if(cmd==LINK_PING&&self==host){if(direct)p->probed=true;unsigned char reply[512];memcpy(reply,packet,512);Ttw_Write(reply+4,LINK_PONG);Ttw_Write(reply+12,from);
            /* Echo on the request's path only. A TLS duplicate returned over
             * RTC would otherwise be misclassified as a slow direct probe. */
            if(direct)rtcSend(*p,reply);else relay(reply);}
        else if(cmd==LINK_PONG&&from==host&&self!=host&&Ttw_Read(packet+12)==self){unsigned serial=Ttw_Read(packet+16);auto &probe=p->probes[serial%32];if(probe.serial==serial&&at>=probe.at){unsigned ms=(unsigned)(at-probe.at);sample(ms);if(direct){p->probed=true;p->rtt=(int)ms;}}}
        else if(cmd==TTW_INPUT&&self==host)input(from,packet);
        else if(cmd==TTW_FRAME&&from==host&&committed&&self!=host)deliver(packet,at);
    }
    void controlMessage(const unsigned char *bytes,size_t size){auto j=LinkJson::parse(bytes,bytes+size,nullptr,false);if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag direct TLS: self=%u bytes=%u object=%d version=%u\n",self,(unsigned)size,j.is_object(),j.is_object()?number(j,"version"):0);if(!j.is_object()||number(j,"version")!=1)return;auto type=text(j,"type");
        if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag direct control: self=%u type=%s epoch=%u\n",self,type.c_str(),number(j,"epoch"));
        if(type=="hello"){
            unsigned nextSelf=number(j,"self"),nextHost=number(j,"host"),nextEpoch=number(j,"epoch");auto gen=text(j,"generation");unsigned char raw[8];if(nextSelf!=self||!nextHost||!nextEpoch||gen.size()!=16||!Upd_HexBytes(gen.c_str(),raw,8))return;
            if(generation!=gen||host!=nextHost||epoch!=nextEpoch){reset(false);generation=gen;host=nextHost;epoch=nextEpoch;hi=(unsigned)strtoul(gen.substr(0,8).c_str(),nullptr,16);lo=(unsigned)strtoul(gen.substr(8,8).c_str(),nullptr,16);nextPing=nextReady=0;}
            configureIce(j);auto list=j.find("peers");if(list==j.end()||!list->is_array()||list->size()>9)return;
            if(committed&&self==host&&j.contains("active")&&j["active"]==true){relayNext=number(j,"nextFrame");relaySynchronized=true;nextReplay=0;}
            for(auto &p:peers)if(p.id){bool keep=false;for(auto &m:*list)if(number(m,"id")==p.id&&(self==host||p.id==host))keep=true;if(!keep){if(committed&&self==host)Kernel_Leave(p.id);close(p);}}
            for(auto &m:*list){unsigned id=number(m,"id");if(!id||id==self||(self!=host&&id!=host))continue;auto *p=find(id);if(!p){for(auto &q:peers)if(!q.id){p=&q;break;}if(!p)continue;p->id=id;open(*p);}}
            return;}
        if(text(j,"generation")!=generation||number(j,"epoch")!=epoch)return;
        if(type=="signal"){
            auto *p=find(number(j,"from"));auto s=j.find("signal");if(!p||p->pc<0||s==j.end()||!s->is_object())return;auto kind=text(*s,"type");
            if(kind=="offer"||kind=="answer"){auto sdp=text(*s,"sdp");if(sdp.empty()||sdp.size()>14000||rtc.rtcSetRemoteDescription(p->pc,sdp.c_str(),kind.c_str())<0)return;p->remote=true;for(auto &c:p->candidates)rtc.rtcAddRemoteCandidate(p->pc,text(c,"candidate").c_str(),text(c,"mid").c_str());p->candidates.clear();if(kind=="offer")rtc.rtcSetLocalDescription(p->pc,"answer");}
            else if(kind=="candidate"){auto c=text(*s,"candidate"),mid=text(*s,"mid");if(c.size()>2048||mid.size()>32)return;if(p->remote)rtc.rtcAddRemoteCandidate(p->pc,c.c_str(),mid.c_str());else if(p->candidates.size()<32)p->candidates.push_back(*s);}
        }else if(type=="prepare"){
            auto encoded=text(j,"snapshot"),wanted=text(j,"digest");std::array<unsigned char,512> snapshot;char actual[65];if(committed||encoded.size()!=1024||wanted.size()!=64||!Upd_HexBytes(encoded.c_str(),snapshot.data(),512))return;UpdSha256_Hex(snapshot.data(),snapshot.size(),actual);
            if(wanted!=actual||Ttw_Read(snapshot.data()+8)!=epoch||Ttw_Read(snapshot.data()+24)!=host||!Kernel_ImportArmed(snapshot.data()))return;
            prepared=true;digest=wanted;auto ack=control("prepared");ack["digest"]=digest;post(ack);
        }else if(type=="commit"){
            if(!prepared||digest!=text(j,"digest")||committed)return;
            if(self==host){Ttw_Request(Kernel_Input(),TTW_START,epoch);if(Kernel_Command(self)!=TT_OK)return;}
            committed=true;relayNext=0;relaySynchronized=self==host;service.host=self==host;service.active=1;if(self==host)service.ping=0;
            fprintf(stderr,"tag direct: committed epoch=%u host=%u self=%u\n",epoch,host,self);
        }else if(type=="resume-ack"&&committed&&self==host){unsigned next=number(j,"nextFrame",0xffffffffu);if(next<=Kernel_PendingFrame()){relayNext=next;relaySynchronized=true;nextReplay=0;}}
        else if(type=="relay"&&committed){for(auto &p:peers)if(p.id)relayOnly(p);fprintf(stderr,"tag direct: same host retained, TLS backup active epoch=%u\n",epoch);}
        else if(type=="cancel"){reset(false);}
    }
    void event(Event &e){auto *p=find(e.from);if(getenv("BT4_RENDER_TEST")&&e.kind!=5)fprintf(stderr,"tag direct event: kind=%d peer=%u pc=%d current=%d\n",e.kind,e.from,e.pc,p?p->pc.load():-1);if(!p||p->pc!=e.pc)return;
        if(e.kind==1||e.kind==2){auto j=control("signal");j["to"]=e.from;if(e.kind==1)j["signal"]={{"type",e.a},{"sdp",e.b}};else j["signal"]={{"type","candidate"},{"candidate",e.a},{"mid",e.b}};post(j);}
        else if(e.kind==3)bind(*p,e.channel);
        else if(e.kind==4)relayOnly(*p);
        else if(e.kind==5&&p->channel==e.channel)binary(e.bytes.data(),e.bytes.size(),e.from,true,e.at);}
    void periodic(){Uint64 now=SDL_GetTicks();
        if(now>=keepalive&&ws&&ws->status==1){keepalive=now+2000;post(control("ping"));}
        if(now>=nextPing&&!generation.empty()){nextPing=now+250;if(self!=host)if(auto *p=find(host)){unsigned serial=++p->nextProbe;p->probes[serial%32]={serial,SDL_GetTicks()};unsigned char packet[512];Ttw_Request(packet,LINK_PING,epoch);Ttw_Write(packet+12,host);Ttw_Write(packet+16,serial);rtcSend(*p,packet);relay(packet);}}
        if(now>=nextReady&&!committed&&!generation.empty()){nextReady=now+500;auto j=control("ready");j["peers"]=LinkJson::array();bool okay=true;unsigned count=0;
            for(auto &p:peers)if(p.id){++count;if(!p.probed||p.channel<0||!rtc.rtcIsOpen(p.channel))okay=false;else if(self!=host&&(p.rtt<0||service.relayPing<0||p.rtt>=service.relayPing))okay=false;j["peers"].push_back({{"id",p.id.load()},{"rttMs",p.rtt<0?0:p.rtt}});}
            if(okay&&count){post(j);if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag direct READY: self=%u peers=%u RTT=%d relay=%d\n",self,count,service.ping.load(),service.relayPing.load());}}
        if(committed&&self==host&&clockRunning){const Uint64 step=1000000000ULL*10/342;unsigned burst=0;now=SDL_GetTicksNS();while(now>=nextTick&&burst++<4){Kernel_DeadlineFrame();if(Kernel_TakeFrame()){Wire f;memcpy(f.bytes.data(),Kernel_Output(),512);f.at=SDL_GetTicks();deliver(f.bytes.data(),f.at);recentFrames.push_back(f);if(recentFrames.size()>3)recentFrames.pop_front();hostHistory.push_back(f);if(hostHistory.size()>512)hostHistory.pop_front();for(auto &p:peers)if(p.id)for(auto &old:recentFrames)rtcSend(p,old.bytes.data());
                    if(relaySynchronized&&relayNext==Ttw_Read(f.bytes.data()+12)&&ws&&ws->status==1){relay(f.bytes.data());++relayNext;}}
                nextTick+=step;}if(nextTick+step*4<now)nextTick=now;
            if(ws&&ws->status==1){
                if(relaySynchronized&&!hostHistory.empty()&&relayNext<Ttw_Read(hostHistory.front().bytes.data()+12)){
                    bool retained=false;for(auto &f:recoveryHistory)if(Ttw_Read(f.bytes.data()+12)==relayNext){retained=true;break;}
                    if(!retained){recoveryHistory=hostHistory;relaySynchronized=false;resumeAt=0;}
                }
                if(!relaySynchronized&&!recoveryHistory.empty()&&SDL_GetTicks()>=resumeAt){auto request=control("resume");request["firstFrame"]=Ttw_Read(recoveryHistory.front().bytes.data()+12);post(request);resumeAt=SDL_GetTicks()+2000;}
                if(relaySynchronized&&SDL_GetTicks()>=nextReplay){nextReplay=SDL_GetTicks()+250;for(unsigned sent=0;sent<16;++sent){const Wire *next=nullptr;for(auto &f:recoveryHistory)if(Ttw_Read(f.bytes.data()+12)==relayNext){next=&f;break;}if(!next)for(auto &f:hostHistory)if(Ttw_Read(f.bytes.data()+12)==relayNext){next=&f;break;}if(!next)break;relay(next->bytes.data());++relayNext;}}
            }
        }
        if(committed&&getenv("BT4_RENDER_TEST")&&SDL_GetTicks()>=diagnostic){diagnostic=SDL_GetTicks()+5000;fprintf(stderr,"tag direct RTT: host=%u self=%u measured-ms=%d relay-ms=%d next=%u\n",host,self,service.ping.load(),service.relayPing.load(),self==host?Kernel_PendingFrame():0);for(auto &p:peers)if(p.id&&p.pc>=0){char local[2048],remote[2048];if(rtc.rtcGetSelectedCandidatePair(p.pc,local,sizeof(local),remote,sizeof(remote))>=0)fprintf(stderr,"tag direct ICE: peer=%u local-relay=%d remote-relay=%d\n",p.id.load(),strstr(local," typ relay")!=nullptr,strstr(remote," typ relay")!=nullptr);}}
    }
    int run(){while(!service.quit){
        if(!ws&&SDL_GetTicks()>=reconnect){ws=std::make_unique<VoiceWs>(service.endpoint+"/v4/taglink/"+service.code,service.cap,0,"BT4 Tag Team",true,DirectService::notify,&service);reconnect=SDL_GetTicks()+1500;}
        if(ws&&ws->status<0){ws.reset();relaySynchronized=false;reconnect=SDL_GetTicks()+1500;}
        VoiceWsMessage m;unsigned work=0;while(ws&&work++<2048&&ws->read(m)){if(m.binary)binary(m.bytes.data(),m.bytes.size(),0,false,m.at);else controlMessage(m.bytes.data(),m.bytes.size());}
        std::deque<Event> events;std::deque<Wire> inputs;std::array<unsigned char,512> snapshot;unsigned serial;
        {std::lock_guard<std::mutex> g(service.lock);events.swap(service.events);if(committed)inputs.swap(service.input);serial=service.snapshotSerial;snapshot=service.snapshot;}
        for(auto &e:events)event(e);
        if(serial!=snapshotSerial){snapshotSerial=serial;unsigned phase=Ttw_Read(snapshot.data()+28);if(committed){if(phase==TT_FIGHT)seenFightSnapshot=true;if(Ttw_Read(snapshot.data()+8)!=epoch||phase==TT_ABORTED||(seenFightSnapshot&&phase!=TT_FIGHT))reset(false);}}
        for(auto &f:inputs)if(committed){if(self==host)input(self,f.bytes.data());else if(auto *p=find(host)){recentInput.push_back(f);if(recentInput.size()>3)recentInput.pop_front();for(auto &old:recentInput){rtcSend(*p,old.bytes.data());relay(old.bytes.data());}}}
        periodic();unsigned wait=50;if(committed&&clockRunning){auto now=SDL_GetTicksNS();wait=nextTick>now?(unsigned)std::min<Uint64>(50,(nextTick-now+999999)/1000000):0;}if(wait)SDL_WaitSemaphoreTimeout(service.wake,wait);
    }reset(true);return 0;}
};
static int thread(void*){SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);Session session;return session.run();}
}
extern "C" int TagDirect_Available(void){return rtc.load();}
extern "C" void TagDirect_Session(const char *endpoint,const char *code,const char *cap,unsigned self){
    if(!endpoint||!code||!cap||strlen(code)!=6||strlen(cap)!=64||!self||!TagDirect_Available())return;
    if(service.thread&&service.self==self&&service.code==code&&service.cap==cap)return;
    TagDirect_Close();service.endpoint=endpoint;while(!service.endpoint.empty()&&service.endpoint.back()=='/')service.endpoint.pop_back();service.code=code;service.cap=cap;service.self=self;service.quit=false;service.wake=SDL_CreateSemaphore(0);if(!service.wake)return;service.thread=SDL_CreateThread(thread,"BT4 Tag network",nullptr);
}
extern "C" void TagDirect_State(const unsigned char *packet){if(!service.thread)return;std::lock_guard<std::mutex> g(service.lock);memcpy(service.snapshot.data(),packet,512);++service.snapshotSerial;DirectService::notify(&service);}
extern "C" void TagDirect_Close(void){service.quit=true;DirectService::notify(&service);if(service.thread){SDL_WaitThread(service.thread,nullptr);service.thread=nullptr;}if(service.wake){SDL_DestroySemaphore(service.wake);service.wake=nullptr;}std::lock_guard<std::mutex> g(service.lock);service.input.clear();service.frames.clear();service.queuedFrames=0;service.events.clear();service.snapshotSerial=0;service.active=service.host=0;service.ping=service.relayPing=-1;}
extern "C" int TagDirect_Active(void){return service.active;}
extern "C" int TagDirect_Host(void){return service.host;}
extern "C" int TagDirect_Ping(void){return service.ping;}
extern "C" void TagDirect_RelayPing(int ms){service.relayPing=ms;}
extern "C" int TagDirect_Send(const unsigned char *packet){if(!service.thread||service.active<0||!packet)return 0;std::lock_guard<std::mutex> g(service.lock);if(service.input.size()>=512){service.active=-1;return 0;}Wire f;memcpy(f.bytes.data(),packet,512);f.at=SDL_GetTicks();service.input.push_back(f);DirectService::notify(&service);return 1;}
extern "C" int TagDirect_Read(unsigned char *packet,uint64_t *arrived){std::lock_guard<std::mutex> g(service.lock);if(service.frames.empty())return 0;auto f=std::move(service.frames.front());service.frames.pop_front();service.queuedFrames=(int)service.frames.size();memcpy(packet,f.bytes.data(),512);if(arrived)*arrived=f.at;return 1;}
extern "C" int TagDirect_Pending(void){return service.queuedFrames;}
