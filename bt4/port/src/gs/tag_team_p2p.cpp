/* Participant-hosted Tag room, from lobby through rematch. Supabase carries
 * admission and ICE only; reliable RTC control and UDP-backed inputs stay P2P.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
#include <deque>
#include <array>
#include <map>
#include <atomic>
#include <mutex>
#include "tag_team_p2p.h"
#include "../../include/online_room_policy.h"
#include "tag_signal.h" /* (Supabase's signaling, or Dragon Net's: dragonnet_tag.cpp) */
#include "voice_platform.inc"
#include "../../third_party/voice/rtc/rtc.h"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
extern "C" {
#include "../../include/tag_team_wire.h"
unsigned char *Kernel_Input(void),*Kernel_Output(void);
void Kernel_Reset(void),Kernel_Pacing(int),Kernel_Seed(unsigned),Kernel_Snapshot(unsigned,int,const char*),Kernel_Leave(unsigned);
int Kernel_Command(unsigned),Kernel_TakeFrame(void),Kernel_Phase(void),Kernel_Seat(unsigned),Kernel_RoundEnded(void);
unsigned Kernel_DeadlineFrame(void),Kernel_Epoch(void),Kernel_PendingFrame(void);
}
using Pj=nlohmann::json;
namespace {
#include "p2p_rtc_api.inc"
constexpr unsigned magic=0x32505442u,pingCommand=107,pongCommand=108,replayCommand=110;
using Packet=std::array<unsigned char,512>;
struct Wire {Packet bytes;Uint64 at;};
struct Event {int kind=0,pc=-1,dc=-1;unsigned peer=0;bool reliable=false;std::string a,b;std::vector<unsigned char> bytes;Uint64 at=0;};
struct Service {
    std::mutex lock;SDL_Thread *thread=nullptr;SDL_Semaphore *wake=nullptr;
    std::atomic<bool> quit{false};std::atomic<int> status{0},ping{-1};
    std::deque<Event> events;std::deque<Packet> requests;std::deque<Wire> incoming;
    std::string descriptor;
    void notify(){if(wake)SDL_SignalSemaphore(wake);}
    void event(Event e){std::lock_guard<std::mutex> g(lock);if(quit)return;if(events.size()>=2048){status=-1;return;}events.push_back(std::move(e));notify();}
} service;
static std::atomic<unsigned> rpcSerial{1000};
static unsigned number(const Pj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_number_unsigned()?p->get<unsigned>():0;}
static std::string text(const Pj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_string()?p->get<std::string>():std::string();}
struct Peer {std::atomic<unsigned> id{0};std::atomic<int> pc{-1},control{-1},input{-1};bool remote=false,admitted=false;std::string name;std::vector<Pj> candidates;Uint64 heard=0;};
struct PendingSignal {Pj body;Uint64 at;};
struct OutgoingSignal {Pj body;Uint64 next,expires;};
static std::array<Peer,10> stablePeers;
struct Session {
    std::array<Peer,10> &peers=stablePeers;unsigned self=0,epoch=0,lo=0,hi=0,initSerial=0,reconcile=0;
    std::string room,generation,code,content;Packet opening={};Pj queueProfile;bool creating=false,initializing=false,clock=false,queued=false,waiting=false;int phase=TT_CLOSED;
    Uint64 nextTick=0,nextPing=0,nextLease=0,began=SDL_GetTicks(),lastReplay=0;int publishedPhase=-1;
    unsigned expected=0,probeSerial=0;std::array<std::pair<unsigned,Uint64>,32> probes={};
    std::array<unsigned,8> pingSamples={};unsigned pingCount=0,pingCursor=0;
    std::deque<Wire> history,recent;std::map<unsigned,Wire> reordered;
    std::deque<PendingSignal> earlySignals;
    std::map<unsigned,OutgoingSignal> outgoing;
    uint64_t signalCursor=0;unsigned recovery=0;Uint64 recoverAt=0;
    void pingSample(unsigned sample){
        if(sample>=10000)return;
        pingSamples[pingCursor++%pingSamples.size()]=sample;
        if(pingCount<pingSamples.size())++pingCount;
        unsigned best=sample;
        for(unsigned i=0;i<pingCount;++i)if(pingSamples[i]<best)best=pingSamples[i];
        service.ping=(int)best;
    }
    Peer *find(unsigned id){for(auto &p:peers)if(p.id==id)return &p;return nullptr;}
    void emit(const Packet &p,Uint64 at=0){std::lock_guard<std::mutex> g(service.lock);if(service.incoming.size()>=1024){service.status=-1;return;}service.incoming.push_back({p,at?at:SDL_GetTicks()});}
    static void RTC_API description(int pc,const char *sdp,const char *kind,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc||!sdp||!kind)return;Event e;e.kind=1;e.peer=p.id;e.pc=pc;e.a=kind;e.b=sdp;service.event(std::move(e));}
    static void RTC_API candidate(int pc,const char *value,const char *mid,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc||!value||!mid)return;Event e;e.kind=2;e.peer=p.id;e.pc=pc;e.a=value;e.b=mid;service.event(std::move(e));}
    static void RTC_API channel(int pc,int dc,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc)return;Event e;e.kind=3;e.peer=p.id;e.pc=pc;e.dc=dc;service.event(std::move(e));}
    static void RTC_API changed(int pc,rtcState state,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(p.pc!=pc)return;if(state==RTC_FAILED||state==RTC_CLOSED){Event e;e.kind=4;e.peer=p.id;e.pc=pc;service.event(std::move(e));}}
    static void RTC_API opened(int dc,void *ptr){auto &p=*static_cast<Peer*>(ptr);if(dc!=p.control&&dc!=p.input)return;Event e;e.kind=5;e.peer=p.id;e.pc=p.pc;e.dc=dc;service.event(std::move(e));}
    static void RTC_API message(int dc,const char *bytes,int size,void *ptr){auto &p=*static_cast<Peer*>(ptr);if((dc!=p.control&&dc!=p.input)||size!=528)return;Event e;e.kind=6;e.peer=p.id;e.pc=p.pc;e.dc=dc;e.reliable=dc==p.control;e.at=SDL_GetTicks();e.bytes.assign(bytes,bytes+size);service.event(std::move(e));}
    void close(Peer &p){int pc=p.pc.exchange(-1),control=p.control.exchange(-1),input=p.input.exchange(-1);p.id=0;
        if(control>=0)rtc.rtcDeleteDataChannel(control);if(input>=0)rtc.rtcDeleteDataChannel(input);
        if(pc>=0){rtc.rtcClosePeerConnection(pc);rtc.rtcDeletePeerConnection(pc);}p.remote=p.admitted=false;p.candidates.clear();p.name.clear();}
    bool send(Peer &p,const Packet &packet,bool reliable){int dc=reliable?p.control:p.input;if(dc<0||!rtc.rtcIsOpen(dc)||rtc.rtcGetBufferedAmount(dc)>=65536)return false;
        std::array<unsigned char,528> wire={};Ttw_Write(wire.data(),magic);Ttw_Write(wire.data()+4,lo);Ttw_Write(wire.data()+8,hi);Ttw_Write(wire.data()+12,self);memcpy(wire.data()+16,packet.data(),512);
        return rtc.rtcSendMessage(dc,(const char*)wire.data(),528)>=0;}
    void bind(Peer &p,int dc){char protocol[64]={};rtcReliability mode={};
        if(dc<0){service.status=-1;return;}
        if(rtc.rtcGetDataChannelProtocol(dc,protocol,sizeof(protocol))<0||rtc.rtcGetDataChannelReliability(dc,&mode)<0){rtc.rtcDeleteDataChannel(dc);return;}
        bool reliable=!strcmp(protocol,"bt4-room-control-v1")&&!mode.unreliable&&!mode.unordered;
        bool input=!strcmp(protocol,"bt4-tag-input-v1")&&mode.unreliable&&mode.unordered&&mode.maxRetransmits==0;
        if((!reliable&&!input)||(reliable?p.control.load():p.input.load())>=0){rtc.rtcDeleteDataChannel(dc);return;}
        if(reliable)p.control=dc;else p.input=dc;rtc.rtcSetUserPointer(dc,&p);rtc.rtcSetOpenCallback(dc,opened);rtc.rtcSetMessageCallback(dc,message);
    }
    void open(Peer &p){const char *stun[]={"stun:stun.l.google.com:19302","stun:stun1.l.google.com:19302","stun:global.stun.twilio.com:3478"};rtcConfiguration cfg={};cfg.iceServers=stun;cfg.iceServersCount=3;
        /* (BT4_TAG_STUN: other ICE servers, comma separated, or "none" for this network's own addresses only: LAN tests) */
        static std::vector<std::string> custom;static std::vector<const char*> customPointers;static bool customRead=false;
        if(!customRead){customRead=true;if(const char *e=SDL_getenv("BT4_TAG_STUN")){std::string list=e,item;
            for(size_t i=0;i<=list.size();++i){if(i==list.size()||list[i]==','){if(!item.empty()&&item!="none"&&custom.size()<4)custom.push_back(item);item.clear();}else item+=list[i];}
            for(auto &c:custom)customPointers.push_back(c.c_str());if(customPointers.empty())customPointers.push_back(nullptr);}}
        if(!customPointers.empty()){cfg.iceServers=customPointers[0]?customPointers.data():nullptr;cfg.iceServersCount=customPointers[0]?(int)customPointers.size():0;}cfg.disableAutoNegotiation=true;cfg.maxMessageSize=2048;
        int pc=rtc.rtcCreatePeerConnection(&cfg);if(pc<0){service.status=-1;return;}p.pc=pc;p.heard=SDL_GetTicks();rtc.rtcSetUserPointer(pc,&p);
        rtc.rtcSetLocalDescriptionCallback(pc,description);rtc.rtcSetLocalCandidateCallback(pc,candidate);rtc.rtcSetDataChannelCallback(pc,channel);rtc.rtcSetStateChangeCallback(pc,changed);
        if(self==1){rtcDataChannelInit control={};control.protocol="bt4-room-control-v1";bind(p,rtc.rtcCreateDataChannelEx(pc,"BT4 Room",&control));
            rtcDataChannelInit input={};input.protocol="bt4-tag-input-v1";input.reliability.unordered=true;input.reliability.unreliable=true;input.reliability.maxRetransmits=0;
            bind(p,rtc.rtcCreateDataChannelEx(pc,"BT4 Inputs",&input));rtc.rtcSetLocalDescription(pc,"offer");}}
    void rpc(const char *op,const Pj &args,unsigned serial=0){if(!TagSignal_Command(1,serial?serial:++rpcSerial,op,args.dump().c_str())){if(initializing)service.status=-1;}}
    void signal(unsigned to,const char *kind,const Pj &body){
        Pj args={{"generation",generation},{"to",to},{"channel","tag"},{"kind",kind},{"body",body}};
        unsigned id=++rpcSerial;
        if(outgoing.size()<96)outgoing.emplace(id,OutgoingSignal{args,SDL_GetTicks()+25000,SDL_GetTicks()+60000});
        rpc("signal",args,id);
    }
    void retrySignals(Uint64 now){
        for(auto it=outgoing.begin();it!=outgoing.end();){
            if(now>=it->second.expires){it=outgoing.erase(it);continue;}
            if(now>=it->second.next){auto item=it->second;it=outgoing.erase(it);item.next=now+25000;
                unsigned id=++rpcSerial;outgoing.emplace(id,item);rpc("signal",item.body,id);
            }else ++it;
        }
    }
    void snapshot(unsigned target,int result=TT_OK,unsigned ack=0){Kernel_Snapshot(target,result,code.c_str());Packet packet;memcpy(packet.data(),Kernel_Output(),512);Ttw_Write(packet.data()+508,ack);
        if(target==self)emit(packet);else if(auto *p=find(target))send(*p,packet,true);}
    void notify(){snapshot(self);for(auto &p:peers)if(p.id&&p.admitted)snapshot(p.id);}
    void clearRound(unsigned next){epoch=next;clock=false;expected=0;recent.clear();history.clear();reordered.clear();lastReplay=0;}
    bool validSignal(const Pj &body){
        auto b=body.find("body");unsigned from=number(body,"from"),to=number(body,"to");
        if(text(body,"channel")!="tag"||!from||from>10||!to||to>10||from==to||
           text(body,"generation").size()!=36||b==body.end()||!b->is_object())return false;
        auto kind=text(body,"kind");
        if(kind=="offer"||kind=="answer"){auto sdp=text(*b,"sdp");return !sdp.empty()&&sdp.size()<=14000;}
        if(kind=="candidate"){auto candidate=text(*b,"candidate"),mid=text(*b,"mid");return !candidate.empty()&&candidate.size()<=2048&&mid.size()<=64;}
        return false;
    }
    bool takeSignal(const Pj &body){
        // Realtime and HTTP admission are independent transports. An offer can
        // arrive before the join reply or the new member's room notification.
        if(generation.empty()||!self)return false;
        if(text(body,"generation")!=generation||number(body,"to")!=self)return true;
        auto *p=find(number(body,"from"));if(!p||p->pc<0)return false;
        auto &b=body["body"];auto kind=text(body,"kind");
        if(kind=="offer"||kind=="answer"){
            if((kind=="offer"&&(self==1||p->id!=1))||(kind=="answer"&&self!=1)||p->remote)return true;
            if(rtc.rtcSetRemoteDescription(p->pc,text(b,"sdp").c_str(),kind.c_str())<0)return true;
            p->remote=true;
            for(auto &c:p->candidates)rtc.rtcAddRemoteCandidate(p->pc,text(c,"candidate").c_str(),text(c,"mid").c_str());
            p->candidates.clear();if(kind=="offer")rtc.rtcSetLocalDescription(p->pc,"answer");
        }else if(kind=="candidate"){
            if(p->remote)rtc.rtcAddRemoteCandidate(p->pc,text(b,"candidate").c_str(),text(b,"mid").c_str());
            else if(p->candidates.size()<32)p->candidates.push_back(b);
        }
        return true;
    }
    void drainSignals(){
        Uint64 now=SDL_GetTicks();
        for(auto it=earlySignals.begin();it!=earlySignals.end();){
            if(now-it->at>10000||takeSignal(it->body))it=earlySignals.erase(it);else ++it;
        }
    }
    void receiveSignal(const Pj &body){
        if(!validSignal(body))return;
        drainSignals();
        if(!takeSignal(body)&&earlySignals.size()<32)earlySignals.push_back({body,SDL_GetTicks()});
    }
    void command(unsigned sender,const Packet &packet){unsigned cmd=Ttw_Read(packet.data()+4),atEpoch=Ttw_Read(packet.data()+8);
        if(cmd!=TTW_PING&&atEpoch!=Kernel_Epoch()&&(int32_t)(atEpoch-Kernel_Epoch())<0){if(cmd!=TTW_INPUT)snapshot(sender);return;}
        if(cmd==TTW_INPUT){if(atEpoch!=Kernel_Epoch()||Kernel_Phase()!=TT_FIGHT)return;memcpy(Kernel_Input(),packet.data(),512);Kernel_Command(sender);
            if(sender==self&&!clock){clock=true;nextTick=SDL_GetTicksNS();}return;}
        if(cmd==TTW_OPEN||cmd==TTW_JOIN||cmd==TTW_STATE||cmd==TTW_FRAME)return;
        if(cmd==TTW_CHAT){if(Kernel_Seat(sender)<0)return;Packet chat=packet;Ttw_Write(chat.data()+12,sender);chat[175]=0;emit(chat);for(auto &p:peers)if(p.id&&p.admitted)send(p,chat,true);return;}
        memcpy(Kernel_Input(),packet.data(),512);int result=Kernel_Command(sender);
        if(Kernel_Epoch()!=epoch)clearRound(Kernel_Epoch());
        if(Kernel_RoundEnded())clock=false;
        if(result==TT_OK&&cmd!=TTW_PING&&cmd!=TTW_HASH){notify();}
        if(cmd==TTW_PING||result!=TT_OK)snapshot(sender,result,Ttw_Read(packet.data()+508));
    }
    void metadata(const Pj &j){
        std::string nextRoom=text(j,"room"),nextGeneration=text(j,"generation");if(nextRoom.empty()||nextGeneration.empty())return;
        if(room.empty()){
            room=nextRoom;generation=nextGeneration;code=text(j,"code");self=number(j,"self");if(!self||number(j,"host")!=1){fprintf(stderr,"P2P admission identity is invalid\n");service.status=-1;return;}
            std::string id;for(char c:generation)if(c!='-')id+=c;if(id.size()!=32){service.status=-1;return;}
            lo=(unsigned)strtoul(id.substr(0,8).c_str(),nullptr,16);hi=(unsigned)strtoul(id.substr(8,8).c_str(),nullptr,16);
            if(queued)creating=self==1;
            if(creating){Kernel_Reset();Kernel_Pacing(1);memcpy(Kernel_Input(),opening.data(),512);int result=Kernel_Command(self);if(result!=TT_OK){fprintf(stderr,"P2P local room rejected: %d\n",result);service.status=-1;return;}Kernel_Seed((unsigned)SDL_GetTicksNS());epoch=Kernel_Epoch();snapshot(self);service.ping=0;service.status=1;}
            nextLease=SDL_GetTicks()+60000;began=SDL_GetTicks();initializing=false;waiting=false;
        }
        if(nextRoom!=room||nextGeneration!=generation)return;
        {std::lock_guard<std::mutex> g(service.lock);service.descriptor=j.dump();}
        auto members=j.find("members");if(members==j.end()||!members->is_array()||members->size()>10)return;
        for(const auto &m:*members){unsigned id=number(m,"peer");if(!id||id==self||(self!=1&&id!=1))continue;
            Peer *p=find(id);if(!p){for(auto &candidate:peers)if(!candidate.id){p=&candidate;break;}if(!p){service.status=-1;return;}p->id=id;p->name=text(m,"name");open(*p);}
        }
        // Database expiry/temporary signaling loss cannot delete a live P2P
        // player. Departures are detected on the peer transport itself.
        drainSignals();
    }
    void signaling(){char raw[24577];unsigned count=0;
        while(count++<64&&TagSignal_Read(1,raw,sizeof(raw))>0){Pj j=Pj::parse(raw,nullptr,false);if(!j.is_object())continue;auto body=j.find("body");if(body==j.end()||!body->is_object())continue;
            auto event=text(j,"event");
            if(event=="reply"){
                if(recovery&&number(j,"serial")==recovery){
                    recovery=0;
                    auto signals=body->find("signals");
                    if(signals!=body->end()&&signals->is_array())for(const auto &packet:*signals)receiveSignal(packet);
                    auto cursor=body->find("cursor");if(cursor!=body->end()&&cursor->is_number_unsigned())signalCursor=std::max(signalCursor,cursor->get<uint64_t>());
                    recoverAt=SDL_GetTicks()+1000;
                    continue;
                }
                auto pending=outgoing.find(number(j,"serial"));
                if(pending!=outgoing.end()){
                    if(text(*body,"state")=="sent")outgoing.erase(pending);
                    else pending->second.next=SDL_GetTicks()+2000;
                    continue;
                }
                if(number(j,"serial")==initSerial||number(j,"serial")==reconcile){if(body->contains("error")){if(!self){fprintf(stderr,"tag P2P admission failed: %s\n",text(*body,"error").c_str());service.status=-1;}}else if(text(*body,"state")=="waiting"){waiting=true;nextLease=SDL_GetTicks()+60000;}else metadata(*body);}
            }else if(event=="bt4.ready"&&!room.empty()){reconcile=++rpcSerial;rpc("get",Pj::object(),reconcile);
            }else if(event=="bt4.room"&&(!room.empty()||queued))metadata(*body);
            else if(event=="bt4.signal")receiveSignal(*body);
        }
    }
    void flushFrames(){if(phase!=TT_FIGHT)return;while(true){auto item=reordered.find(expected);if(item==reordered.end())break;emit(item->second.bytes,item->second.at);reordered.erase(item);++expected;}}
    void event(Event &e){auto *p=find(e.peer);if(!p||p->pc!=e.pc)return;
        if(e.kind==1)signal(p->id,e.a.c_str(),{{"sdp",e.b}});
        else if(e.kind==2)signal(p->id,"candidate",{{"candidate",e.a},{"mid",e.b}});
        else if(e.kind==3)bind(*p,e.dc);
        else if(e.kind==4){if(self==1){unsigned id=p->id;close(*p);Kernel_Leave(id);notify();}else service.status=-1;}
        else if(e.kind==5){if(self==1&&!p->admitted&&p->control>=0&&p->input>=0&&rtc.rtcIsOpen(p->control)&&rtc.rtcIsOpen(p->input)){
            Packet join={};Ttw_Request(join.data(),TTW_JOIN,Kernel_Epoch());uint64_t hash=strtoull(content.c_str(),nullptr,16);Ttw_Write(join.data()+24,(unsigned)hash);Ttw_Write(join.data()+28,(unsigned)(hash>>32));snprintf((char*)join.data()+32,20,"%s",p->name.c_str());
            memcpy(Kernel_Input(),join.data(),512);int result=Kernel_Command(p->id);if(result==TT_OK){p->admitted=true;notify();}else snapshot(p->id,result);
        }}else if(e.kind==6){
            if(e.bytes.size()!=528||Ttw_Read(e.bytes.data())!=magic||Ttw_Read(e.bytes.data()+4)!=lo||Ttw_Read(e.bytes.data()+8)!=hi||Ttw_Read(e.bytes.data()+12)!=p->id)return;
            Packet packet;memcpy(packet.data(),e.bytes.data()+16,512);if(Ttw_Read(packet.data())!=TT_PROTOCOL)return;p->heard=e.at;unsigned cmd=Ttw_Read(packet.data()+4),packetEpoch=Ttw_Read(packet.data()+8);
            if(cmd==pingCommand&&self==1){Ttw_Write(packet.data()+4,pongCommand);send(*p,packet,false);return;}
            if(cmd==pongCommand&&self!=1){unsigned sequence=Ttw_Read(packet.data()+12);auto sample=probes[sequence%32];if(sample.first==sequence&&e.at>=sample.second)pingSample((unsigned)(e.at-sample.second));return;}
            if(cmd==replayCommand&&e.reliable&&self==1&&packetEpoch==epoch){unsigned wanted=Ttw_Read(packet.data()+12),sent=0;for(auto &f:history)if(Ttw_Read(f.bytes.data()+12)>=wanted&&sent++<16)send(*p,f.bytes,true);return;}
            if(self==1){if(!p->admitted)return;if(cmd==TTW_INPUT||e.reliable)command(p->id,packet);return;}
            if(cmd==TTW_STATE&&e.reliable){TtRoom r;unsigned target;int result;char roomCode[7];if(!Ttw_Decode(packet.data(),&r,&target,&result,roomCode)||target!=self||r.host!=1)return;
                if(epoch&&((int32_t)(r.epoch-epoch)<0))return;if(r.epoch!=epoch)clearRound(r.epoch);phase=r.phase;p->admitted=true;service.status=1;emit(packet,e.at);flushFrames();return;}
            if(cmd==TTW_FRAME&&packetEpoch==epoch){unsigned frame=Ttw_Read(packet.data()+12);if(frame<expected)return;if(frame-expected>=512){service.status=-1;return;}reordered.emplace(frame,Wire{packet,e.at});flushFrames();}
            else if(cmd==TTW_CHAT&&e.reliable)emit(packet,e.at);
        }
    }
    void initialize(const Packet &packet){opening=packet;creating=Ttw_Read(packet.data()+4)==TTW_OPEN;initializing=true;initSerial=++rpcSerial;
        queued=creating&&Ttw_Read(packet.data()+64)==1;
        char hash[17];snprintf(hash,sizeof(hash),"%08x%08x",Ttw_Read(packet.data()+28),Ttw_Read(packet.data()+24));content=hash;
        char name[20];memcpy(name,packet.data()+32,19);name[19]=0;
        rpc("leave",Pj::object());
        if(creating){
            const char *rules=Ttw_Read(packet.data()+60)?"ffa6-114":Ttw_Read(packet.data()+12)==2?"2v2-114":"5v5-114";
            queueProfile={{"mode","tag"},{"wire","bt4-p2p-2"},{"content",content},{"rules",queued?rules:"tag-114"},{"name",name},{"capacity",Ttw_Read(packet.data()+12)*2}};
            rpc(queued?"queue":"create",queueProfile,initSerial);
        }else{char joinCode[7];memcpy(joinCode,packet.data()+12,6);joinCode[6]=0;rpc("join",{{"code",joinCode},{"wire","bt4-p2p-2"},{"content",content},{"name",name}},initSerial);}
    }
    void run(){
        while(!service.quit&&service.status>=0){
            signaling();std::deque<Event> events;std::deque<Packet> requests;
            {std::lock_guard<std::mutex> g(service.lock);events.swap(service.events);requests.swap(service.requests);}
            for(auto &e:events)event(e);
            for(auto &packet:requests){unsigned cmd=Ttw_Read(packet.data()+4);if(!self){if(!initializing&&(cmd==TTW_OPEN||cmd==TTW_JOIN))initialize(packet);continue;}
                if(self==1)command(self,packet);else if(auto *p=find(1)){if(cmd==TTW_INPUT)send(*p,packet,false);else send(*p,packet,true);}}
            Uint64 now=SDL_GetTicks();retrySignals(now);
            bool negotiating=!room.empty()&&std::any_of(peers.begin(),peers.end(),[](const Peer &p){return p.id&&!p.admitted;});
            /* (how soon news is needed: a negotiation, an admission or a search; the owner's lobby, where players join; else none) */
            TagSignal_Pace(negotiating||(room.empty()&&!waiting)?2:room.empty()||(self==1&&Kernel_Phase()==TT_LOBBY)?1:0);
            if(negotiating&&now>=recoverAt){recovery=++rpcSerial;recoverAt=now+25000;rpc("get_signals",{{"generation",generation},{"after",signalCursor}},recovery);}
            if(waiting&&now>=nextLease){initSerial=++rpcSerial;rpc("queue",queueProfile,initSerial);nextLease=now+60000;}
            if(service.status==0&&!waiting&&now-began>60000){service.status=-1;break;}
            if(self!=1&&self&&now>=nextPing){nextPing=now+500;if(auto *p=find(1)){Packet packet={};Ttw_Request(packet.data(),pingCommand,epoch);unsigned n=++probeSerial;probes[n%32]={n,now};Ttw_Write(packet.data()+12,n);send(*p,packet,false);
                if(p->admitted&&Bt4Room_IsIdleMs(now,p->heard)){service.status=-1;break;}}}
            if(self==1&&(now>=nextLease||publishedPhase!=Kernel_Phase())){nextLease=now+60000;publishedPhase=Kernel_Phase();rpc("lease",{{"joinable",publishedPhase==TT_LOBBY}});}
            if(self==1&&clock&&Kernel_Phase()==TT_FIGHT){const Uint64 step=1000000000ULL*10/342;unsigned burst=0;Uint64 ns=SDL_GetTicksNS();
                while(ns>=nextTick&&burst++<4){Kernel_DeadlineFrame();if(Kernel_TakeFrame()){Wire f;memcpy(f.bytes.data(),Kernel_Output(),512);f.at=SDL_GetTicks();emit(f.bytes,f.at);history.push_back(f);if(history.size()>512)history.pop_front();recent.push_back(f);if(recent.size()>3)recent.pop_front();
                    for(auto &p:peers)if(p.id&&p.admitted)for(auto &old:recent)send(p,old.bytes,false);}nextTick+=step;}
                if(nextTick+step*4<ns)nextTick=ns;
            }
            if(self!=1&&!reordered.empty()&&now-lastReplay>=100){lastReplay=now;if(auto *p=find(1)){Packet packet={};Ttw_Request(packet.data(),replayCommand,epoch);Ttw_Write(packet.data()+12,expected);send(*p,packet,true);}}
            SDL_WaitSemaphoreTimeout(service.wake,2);
        }
        for(auto &p:peers)if(p.id)close(p);
        if(queued&&!self)rpc("cancel",Pj::object());
        Pj leave=Pj::object();if(!generation.empty())leave["generation"]=generation;rpc("leave",leave);
    }
};
static int thread(void*){Session s;s.run();return 0;}
}
extern "C" int TagPeer_Open(void){
    TagPeer_Close();if(!rtc.load()||!TagSignal_Open())return 0;TagSignal_Listen(1,1);service.quit=false;service.status=0;service.ping=-1;
    service.wake=SDL_CreateSemaphore(0);if(!service.wake)return 0;service.thread=SDL_CreateThread(thread,"BT4 P2P room",nullptr);return service.thread!=nullptr;
}
extern "C" void TagPeer_Close(void){service.quit=true;service.notify();if(service.thread)SDL_WaitThread(service.thread,nullptr);service.thread=nullptr;if(service.wake)SDL_DestroySemaphore(service.wake);service.wake=nullptr;
    TagSignal_Listen(1,0);std::lock_guard<std::mutex> g(service.lock);service.events.clear();service.requests.clear();service.incoming.clear();service.descriptor.clear();service.status=0;}
extern "C" int TagPeer_Send(const unsigned char *packet){if(!packet||!service.thread||service.status<0)return 0;std::lock_guard<std::mutex> g(service.lock);if(service.requests.size()>=256)return 0;
    Packet p;memcpy(p.data(),packet,512);service.requests.push_back(p);service.notify();return 1;}
extern "C" int TagPeer_Read(unsigned char *packet,uint64_t *at){std::lock_guard<std::mutex> g(service.lock);if(service.incoming.empty())return service.status<0?-1:0;auto f=service.incoming.front();service.incoming.pop_front();memcpy(packet,f.bytes.data(),512);if(at)*at=f.at;return 1;}
extern "C" int TagPeer_Status(void){return service.status;}
extern "C" int TagPeer_Ping(void){return service.ping;}
extern "C" int TagPeer_Descriptor(char *json,unsigned capacity){std::lock_guard<std::mutex> g(service.lock);if(!json||service.descriptor.empty()||service.descriptor.size()+1>capacity)return 0;memcpy(json,service.descriptor.c_str(),service.descriptor.size()+1);return 1;}
