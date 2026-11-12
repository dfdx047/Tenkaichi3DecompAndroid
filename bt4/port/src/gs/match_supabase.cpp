/* Adapts existing UDP discovery. Native duel rollback and lobby/tournament
 * authority remain in net.c/net_lobby.c, on the participants' machines.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <array>
#include <map>
#include <deque>
#include <set>
#include <mutex>
#include <atomic>
#include <vector>
#include "match_supabase.h"
#include "supabase_signal.h"
#include "voice.h"
#include "update_sign.h"
extern "C" void Port_RankedShutdown(void);
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
using Dj=nlohmann::json;
namespace {
std::mutex lock;unsigned serial=20000;
std::string text(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_string()?p->get<std::string>():std::string();}
unsigned number(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_number_unsigned()?p->get<unsigned>():0;}
struct Discovery {
    unsigned generation=0,init=0,countsRequest=0,tagCountsRequest=0,reconcile=0;bool on=false,waiting=false;
    Bt4DiscoveryStatus status={};Dj descriptor,profile,localAddresses;std::string wire,content;
    std::map<unsigned,Dj> addresses;std::map<unsigned,bool> announced;
    std::map<unsigned,unsigned> announcements;
    std::map<unsigned,Uint64> retryAt;
    std::deque<Dj> earlySignals;
    Uint64 leaseAt=0,began=0,lastCounts=0,reconcileAt=0;std::array<int,5> counts={};bool countsKnown=false;
    Uint64 lastTagCounts=0;std::array<int,3> tagCounts={};bool tagCountsKnown=false;
    bool ranked=false,sawFight=false,reportPending=false;
    unsigned outcomeTick=0,reportSerial=0;int outcomeBits=0,reportState=0;
    int resultRating=0,resultDelta=0;
    Uint64 reportAt=0;Dj report;
} d;
bool rankedNext=false;
std::atomic<bool> rankedActive{false};
// Keep an authenticated liveness proof while fighting, then briefly after a
// remote disconnect. No simulation, GPU or blocking network work runs here.
struct RankedWatch {
    std::string match,project;
    Uint64 next=0,until=0;
    unsigned request=0;
    bool concede=false,peerGone=false,pending=false,notify=false,sentConcede=false,heartbeatAcknowledged=false;
    bool settled=false;
    int rating=0,delta=0;
};
std::vector<RankedWatch> rankedWatches;
std::deque<std::pair<int,int>> rankedSettlements;
std::atomic<bool> settlementReady{false};
Uint64 nextSettlement;
SDL_Thread *rankedWatcher=nullptr;
std::atomic<bool> watchQuit{false};
unsigned watchSerial=900000;
void watchNotify(const RankedWatch &w){
    if(!w.notify||!w.settled)return;
    if(rankedSettlements.size()==4)rankedSettlements.pop_front();
    rankedSettlements.emplace_back(w.rating,w.delta);
    settlementReady=true;
}
void watchStep(Uint64 now){
    char raw[2048];
    while(Bt4Signal_Read(5,raw,sizeof(raw))>0){
        Dj e=Dj::parse(raw,nullptr,false);if(!e.is_object())continue;
        for(auto &w:rankedWatches)if(w.pending&&w.request==number(e,"serial")){
            w.pending=false;w.next=now+5000;
            auto b=e.find("body");if(b==e.end()||!b->is_object())continue;
            auto state=text(*b,"state");
            if(state=="waiting"&&!w.sentConcede){
                w.heartbeatAcknowledged=true;
                if(w.concede)w.next=now;
            }
            if(state=="applied"||state=="already_applied"){
                auto rating=b->find("rating"),delta=b->find("delta");
                if(rating!=b->end()&&rating->is_number_integer()&&delta!=b->end()&&delta->is_number_integer()){
                    w.settled=true;
                    w.rating=rating->get<int>();
                    w.delta=delta->get<int>();
                    // The server can settle a concession before UDP sees the peer leave.
                    if(!w.until)continue;
                    watchNotify(w);
                }
            }
            if(state=="applied"||state=="already_applied"||state=="not_started"||state=="expired"||
               text(*b,"error")=="NOT_A_PARTICIPANT")w.until=1;
        }
    }
    for(auto &w:rankedWatches){
        if(w.settled||(w.until&&now>=w.until))continue;
        if(w.project!=Bt4Signal_Project())continue; // a new route cannot settle an old route's match
        if(now>=w.next){
            w.request=++watchSerial;
            Dj args={{"match",w.match}};
            w.sentConcede=w.concede&&w.heartbeatAcknowledged;
            if(w.sentConcede)args["reason"]="local_quit";
            else args["peer_gone"]=w.peerGone;
            auto body=args.dump();
            w.pending=Bt4Signal_Command(5,w.request,w.sentConcede?"ranked_forfeit":"ranked_heartbeat",body.c_str());
            w.next=now+(w.pending?25000:5000);
        }
    }
    rankedWatches.erase(std::remove_if(rankedWatches.begin(),rankedWatches.end(),[&](const RankedWatch &w){return w.until&&now>=w.until;}),rankedWatches.end());
}
int SDLCALL watchThread(void*){
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    while(!watchQuit){SDL_Delay(250);std::lock_guard<std::mutex> guard(lock);watchStep(SDL_GetTicks());}
    return 0;
}
void watchFight(){
    auto match=text(d.descriptor,"generation");if(match.empty())return;
    auto it=std::find_if(rankedWatches.begin(),rankedWatches.end(),[&](const RankedWatch &w){return w.match==match;});
    if(it==rankedWatches.end()){
        if(rankedWatches.size()>=4)return;
        rankedWatches.push_back({match,Bt4Signal_Project()});
    }else if(it->until)return; // a completed/left match is not revived by a later presentation frame
    if(!rankedWatcher&&!watchQuit){
        rankedWatcher=SDL_CreateThread(watchThread,"BT4 Ranked liveness",nullptr);
        if(rankedWatcher)atexit(Port_RankedShutdown);
    }
}
void watchFinish(bool concede,bool disconnected=false,bool peerGone=false){
    auto match=text(d.descriptor,"generation");
    for(auto &w:rankedWatches)if(w.match==match&&!w.until){
        w.until=SDL_GetTicks()+120000;
        w.concede=concede&&!d.reportSerial;
        w.peerGone=peerGone&&!d.reportSerial;
        w.notify=disconnected&&!d.reportSerial;
        if(w.settled){watchNotify(w);w.until=1;continue;}
        if(!w.pending)w.next=0;
    }
}
bool rpc(const char *op,const Dj &args,unsigned id=0){
    if(d.ranked&&!strcmp(op,"queue"))op="ranked_queue";
    return Bt4Signal_Command(0,id?id:++serial,op,args.dump().c_str());
}
void error(const std::string &code){
    if(d.status.state==3)return;d.status.state=-1;
    fprintf(stderr,"bt4: discovery: admission failed (%s)\n",code.c_str());
    const char *message=code=="ROOM_FULL"?"Someone has already joined that room.":code=="ROOM_NOT_FOUND"?"No room with that code.":
        code=="INCOMPATIBLE_DATA"?"The host's game uses incompatible battle data.":code=="RATE_LIMITED"?"Too many requests. Wait a moment, then try again.":
        code=="ROOM_BUSY"?"That room has started and is no longer accepting players.":code=="ALREADY_IN_ROOM"?"Leave the current online room before joining another.":
        code=="BAD_CODE"?"Enter the six-character room code again.":
        code=="UPDATE_REQUIRED"?"This is an old or incompatible version. Please update the complete BT4 launcher.":
        code=="AUTH_REQUIRED"?"Online sign-in failed. Check your connection and try again.":
        code=="NICKNAME_REQUIRED"?"Choose a nickname before playing online.":
        code=="ROOM_ROUTE_EXPIRED"?"This room code belongs to an older online route. Ask the host for a new code.":
        code=="ROUTE_CHANGED"||code=="ROUTE_REFRESH_REQUIRED"||code=="ROUTE_RECOVERING"?"The online room service is changing over. Wait a moment and try again.":
        code=="BAD_REQUEST"||code=="BAD_PROFILE"||code=="BAD_CAPACITY"?"The room settings are invalid. Review them and try again.":
        code=="INTERNAL_ERROR"?"The online service had a problem. Please try again later.":
        code=="RANKED_NOT_ENABLED"||code=="RANKED_UNAVAILABLE"?"Ranked is temporarily unavailable. Please try again later.":
        "The matchmaking service could not be reached (is the network up?).";
    snprintf(d.status.error,sizeof(d.status.error),"%s",message);
}
void refreshStatus(){
    if(!d.descriptor.is_object())return;unsigned self=number(d.descriptor,"self");bool host=self==1;
    /* Some deployed descriptors omit mode. Fall back only in that case:
     * a join request starts as duel, but may discover a native lobby. */
    std::string mode=text(d.descriptor,"mode");
    if(mode.empty())mode=text(d.profile,"mode");
    d.status.role=host?1:2;d.status.lobby=mode!="duel";
    snprintf(d.status.code,sizeof(d.status.code),"%s",text(d.descriptor,"code").c_str());
    auto members=d.descriptor.find("members");if(members==d.descriptor.end()||!members->is_array())return;
    unsigned target=0;
    if(!host)target=1;
    else if(!d.status.lobby)for(const auto &member:*members){
        std::string role=text(member,"role");
        if(number(member,"peer")!=self&&(role=="player"||role.empty())){target=number(member,"peer");break;}
    }
    d.status.peerCount=0;memset(d.status.peerAddr,0,sizeof(d.status.peerAddr));
    d.status.peerName[0]=0;
    auto remote=d.addresses.find(target);
    if(target&&remote!=d.addresses.end()&&remote->second.is_array()&&!remote->second.empty()){
        d.status.peerCount=0;for(const auto &value:remote->second)if(value.is_string()&&value.get_ref<const std::string&>().size()<64&&d.status.peerCount<4)
            snprintf(d.status.peerAddr[d.status.peerCount++],64,"%s",value.get_ref<const std::string&>().c_str());
        for(const auto &member:*members)if(number(member,"peer")==target)snprintf(d.status.peerName,sizeof(d.status.peerName),"%s",text(member,"name").c_str());
        if(d.status.peerCount)d.status.state=3;
    }else if(host)d.status.state=2;
    if(host){char previous[32][64];memcpy(previous,d.status.watchAddr,sizeof(previous));int old=d.status.watchCount;d.status.watchCount=0;memset(d.status.watchAddr,0,sizeof(d.status.watchAddr));
        for(const auto &member:*members){unsigned peer=number(member,"peer");if(peer==self||(!d.status.lobby&&text(member,"role")!="spectator"))continue;
            auto p=d.addresses.find(peer);if(p==d.addresses.end()||!p->second.is_array())continue;
            std::string seen;
            for(const auto &address:p->second)if(address.is_string()&&address.get_ref<const std::string&>().size()<64&&d.status.watchCount<32){
                const auto &value=address.get_ref<const std::string&>();
                snprintf(d.status.watchAddr[d.status.watchCount++],64,"%s",value.c_str());
                if(seen.empty()&&value.rfind("relay=",0)!=0&&value.find(':')!=std::string::npos)seen="seen="+value.substr(0,value.rfind(':'));
            }
            // Native lobby pending-member tracking needs this alongside candidates.
            if(!seen.empty()&&d.status.watchCount<32)snprintf(d.status.watchAddr[d.status.watchCount++],64,"%s",seen.c_str());}
        if(old!=d.status.watchCount||memcmp(previous,d.status.watchAddr,sizeof(previous)))++d.status.watchGen;
    }
}
bool takeSignal(const Dj &body){
    if(text(body,"generation")!=text(d.descriptor,"generation"))return false;
    unsigned from=number(body,"from");auto members=d.descriptor.find("members");
    if(!from||members==d.descriptor.end()||!members->is_array())return false;
    bool member=false;for(const auto &m:*members)if(number(m,"peer")==from)member=true;
    auto value=body.find("body");
    if(!member||value==body.end()||!value->is_object())return false;
    auto addresses=value->find("addresses");
    if(addresses==value->end()||!addresses->is_array()||addresses->empty()||addresses->size()>4)return false;
    for(const auto &a:*addresses)if(!a.is_string()||a.get_ref<const std::string&>().size()>=64)return false;
    d.addresses[from]=*addresses;return true;
}
void metadata(const Dj &body){
    auto generation=text(body,"generation");if(generation.empty())return;
    if(d.descriptor.is_object()&&!text(d.descriptor,"generation").empty()&&generation!=text(d.descriptor,"generation"))return;
    d.descriptor=body;d.waiting=false;auto members=body.find("members");if(members==body.end()||!members->is_array())return;
    // Endpoint announcements are also retained in the authenticated descriptor.
    // A broadcast accepted before the recipient subscribed is not delivery.
    auto endpoints=body.find("udp");
    if(endpoints!=body.end()&&endpoints->is_array())for(const auto &entry:*endpoints){
        if(!entry.is_object())continue;
        Dj signal={{"generation",generation},{"from",number(entry,"peer")},{"body",{{"addresses",entry.value("addresses",Dj::array())}}}};
        if(number(entry,"peer")!=number(body,"self"))takeSignal(signal);
    }
    PortVoice_P2PSession(1,body.dump().c_str(),nullptr,0);
    unsigned self=number(body,"self");std::string nonce;for(char c:generation)if(c!='-')nonce+=c;
    std::set<unsigned> present;for(const auto &member:*members)present.insert(number(member,"peer"));
    for(auto it=d.retryAt.begin();it!=d.retryAt.end();)if(!present.count(it->first)){
        d.announced.erase(it->first);d.addresses.erase(it->first);it=d.retryAt.erase(it);
    }else ++it;
    for(auto it=d.announcements.begin();it!=d.announcements.end();)if(!present.count(it->second))it=d.announcements.erase(it);else ++it;
    for(const auto &member:*members){unsigned peer=number(member,"peer");if(!peer||peer==self||d.announced[peer]||SDL_GetTicks()<d.retryAt[peer])continue;
        bool pending=false;for(const auto &request:d.announcements)if(request.second==peer)pending=true;
        if(pending)continue;
        unsigned id=++serial;
        if(rpc("signal",{{"generation",generation},{"to",peer},{"channel","duel"},{"kind","udp"},{"body",{{"addresses",d.localAddresses},{"nonce",nonce}}}},id))d.announcements[id]=peer;
        d.retryAt[peer]=SDL_GetTicks()+2000;
    }
    for(auto it=d.earlySignals.begin();it!=d.earlySignals.end();)
        if(text(*it,"generation")!=generation||takeSignal(*it))it=d.earlySignals.erase(it);else ++it;
    refreshStatus();
}
void pump(){
    char buffer[24577];unsigned work=0;
    while(work++<64&&Bt4Signal_Read(0,buffer,sizeof(buffer))>0){Dj event=Dj::parse(buffer,nullptr,false);if(!event.is_object())continue;auto body=event.find("body");if(body==event.end())continue;
        auto kind=text(event,"event");
        if(kind=="reply"){
            unsigned reply=number(event,"serial");
            if(d.reportSerial&&reply==d.reportSerial){
                d.reportPending=false;d.reportAt=SDL_GetTicks()+3000;
                std::string state=text(*body,"state"),failure=text(*body,"error");
                if(state=="applied"||state=="already_applied"){
                    d.reportState=2;d.resultRating=(int)std::min(100000u,number(*body,"rating"));
                    auto delta=body->find("delta");d.resultDelta=delta!=body->end()&&delta->is_number_integer()
                        ?(int)std::max<int64_t>(-100000,std::min<int64_t>(100000,delta->get<int64_t>())):0;
                }
                else if(state=="disputed"||(!failure.empty()&&failure!="SIGNALING_UNAVAILABLE"&&failure!="RATE_LIMITED"))d.reportState=-1;
                continue;
            }
            auto announcement=d.announcements.find(reply);
            if(announcement!=d.announcements.end()){
                d.announced[announcement->second]=body->is_object()&&text(*body,"state")=="sent";
                if(body->is_object()&&body->contains("error"))fprintf(stderr,"bt4: discovery: endpoint announcement failed (%s)\n",text(*body,"error").c_str());
                d.announcements.erase(announcement);
            }else if(reply==d.countsRequest&&body->is_object()&&!body->contains("error")){const char *keys[]={"S","T","D10","D15","D20"};for(int i=0;i<5;++i)d.counts[i]=(int)number(*body,keys[i]);d.countsKnown=true;}
            else if(reply==d.tagCountsRequest&&body->is_object()&&!body->contains("error")){const char *keys[]={"F6","T2","T5"};for(int i=0;i<3;++i)d.tagCounts[i]=(int)number(*body,keys[i]);d.tagCountsKnown=true;}
            else if((reply==d.init||reply==d.reconcile)&&body->is_object()){
                if(body->contains("error")){error(text(*body,"error"));continue;}
                if(reply==d.reconcile)d.reconcile=0;
                if(text(*body,"state")=="waiting")d.waiting=true;else metadata(*body);
            }
        }else if(kind=="bt4.ready"&&d.on&&d.descriptor.is_object()){
            d.announced.clear();d.announcements.clear();d.retryAt.clear();d.reconcile=++serial;
            if(!rpc("get",Dj::object(),d.reconcile))d.reconcile=0;
        }else if(kind=="bt4.room"&&d.on&&body->is_object()&&(d.descriptor.is_object()||d.waiting)){
            // A delayed notification from the room just left must not establish
            // the identity of a new create/join before its admission reply.
            metadata(*body);
        }
        else if(kind=="bt4.signal"&&d.on&&body->is_object()&&text(*body,"channel")=="duel"&&text(*body,"kind")=="udp"){
            if(takeSignal(*body))refreshStatus();
            else if(d.earlySignals.size()<16)d.earlySignals.push_back(*body);
        }
    }
}
}
extern "C" int Bt4Discovery_Begin(unsigned generation,int role,int protocol,uint64_t content,const char *name,const char *code,const char *addresses,const char *queue){
    std::lock_guard<std::mutex> g(lock);if(generation<d.generation||!name||!addresses)return 0;
    Dj local=Dj::parse(addresses,nullptr,false);if(!local.is_array()||local.empty()||local.size()>4)return 0;
    d.generation=generation;d.on=true;d.waiting=false;d.status={};d.status.state=1;d.descriptor=Dj();d.addresses.clear();d.announced.clear();d.localAddresses=local;
    d.announcements.clear();d.retryAt.clear();d.earlySignals.clear();d.reconcile=0;d.reconcileAt=0;
    d.ranked=rankedNext&&role==3;rankedNext=false;d.sawFight=false;d.reportPending=false;
    rankedActive=d.ranked;
    d.outcomeTick=d.reportSerial=0;d.outcomeBits=d.reportState=0;d.reportAt=0;d.report=Dj();
    d.resultRating=d.resultDelta=0;
    char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.content=hash;d.wire="bt4-duel-"+std::to_string(protocol);
    d.profile={{"mode",role==5?"lobby":"duel"},{"wire",d.wire},{"content",d.content},{"rules",queue&&*queue?queue:"S"},{"name",std::string(name).substr(0,19)},{"capacity",8}};
    if(d.ranked){d.profile["ruleset"]="solo";d.profile["rules"]="S";}
    d.began=SDL_GetTicks();d.leaseAt=d.began+60000;
    if(!Bt4Signal_Open()){d.on=false;rankedActive=false;return 0;}
    // Explicit reset precedes the new admission in the single HTTP queue.
    if(!rpc("leave",Dj::object())){d.on=false;rankedActive=false;return 0;}
    d.init=++serial;
    if(role==1||role==5)return rpc("create",d.profile,d.init);
    if(role==3)return rpc("queue",d.profile,d.init);
    return rpc("join",{{"code",code?code:""},{"wire",d.wire},{"content",d.content},{"name",d.profile["name"]},{"role",role==4?"spectator":"player"}},d.init);
}
extern "C" int Bt4Discovery_Poll(unsigned generation,Bt4DiscoveryStatus *out){
    std::lock_guard<std::mutex> g(lock);if(!out||!d.on||generation!=d.generation)return 0;pump();Uint64 now=SDL_GetTicks();
    // Reconcile membership and retained endpoints while the UDP connection is
    // being established. Reads stop once discovery has both sides' addresses.
    if(d.descriptor.is_object()&&(d.status.state==1||d.status.state==2)&&!d.reconcile&&now>=d.reconcileAt){
        d.reconcile=++serial;d.reconcileAt=now+2000;
        if(!rpc("get",Dj::object(),d.reconcile))d.reconcile=0;
    }
    if(now>=d.leaseAt){d.leaseAt=now+60000;if(d.waiting){d.init=++serial;rpc("queue",d.profile,d.init);}else if(number(d.descriptor,"self")==1)rpc("lease",Dj::object());}
    // First-run admission may perform anonymous auth and then the room RPC;
    // both have bounded 20 s HTTP receive timeouts. Do not cancel the UI while
    // that request is still within its legitimate deadline.
    if(d.status.state==1&&Bt4Signal_Status()<0&&now-d.began>45000)error("SIGNALING_UNAVAILABLE");
    // A queued RPC is not an acknowledgement; retry failed announcements at a bounded rate.
    bool retry=false;for(const auto &entry:d.retryAt)if(!d.announced[entry.first]&&now>=entry.second){
        bool pending=false;for(const auto &request:d.announcements)if(request.second==entry.first)pending=true;
        if(!pending)retry=true;
    }
    if(retry&&d.descriptor.is_object())metadata(d.descriptor);
    *out=d.status;return 1;
}
extern "C" void Bt4Discovery_End(unsigned generation){std::lock_guard<std::mutex> g(lock);if(!d.on||generation!=d.generation)return;
    // Realtime is only the discovery channel. Cancellation and leave are HTTP
    // operations, so always queue them even if the inbox is disconnected.
    if(d.waiting)rpc("cancel",Dj::object());
    if(d.ranked&&d.sawFight){auto m=text(d.descriptor,"generation");for(auto &w:rankedWatches)if(w.match==m&&!w.until){w.until=SDL_GetTicks()+120000;w.next=0;}}
    Dj leave=Dj::object();auto generationId=text(d.descriptor,"generation");if(!generationId.empty())leave["generation"]=generationId;
    rpc("leave",leave);d.earlySignals.clear();d.on=false;rankedActive=false;}
extern "C" int Bt4Discovery_Counts(int protocol,uint64_t content,int out[5]){
    std::lock_guard<std::mutex> g(lock);if(!out)return 0;pump();Uint64 now=SDL_GetTicks();
    if(!d.lastCounts||now-d.lastCounts>=30000){d.lastCounts=now;char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.countsRequest=++serial;
        rpc("counts",{{"wire","bt4-duel-"+std::to_string(protocol)},{"content",hash}},d.countsRequest);}
    for(int i=0;i<5;++i)out[i]=d.counts[i];return d.countsKnown;
}
extern "C" int Bt4Discovery_TagCounts(uint64_t content,int out[3]){
    std::lock_guard<std::mutex> g(lock);if(!out)return 0;pump();Uint64 now=SDL_GetTicks();
    if(!d.lastTagCounts||now-d.lastTagCounts>=30000){d.lastTagCounts=now;char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.tagCountsRequest=++serial;
        rpc("counts",{{"wire","bt4-p2p-2"},{"content",hash}},d.tagCountsRequest);}
    for(int i=0;i<3;++i)out[i]=d.tagCounts[i];return d.tagCountsKnown;
}
extern "C" void Port_RankedSearch(int selected){std::lock_guard<std::mutex> g(lock);rankedNext=selected!=0;}
extern "C" int Port_RankedActive(void){return rankedActive.load();}
extern "C" void Port_RankedShutdown(void){
    watchQuit=true;
    if(rankedWatcher){SDL_WaitThread(rankedWatcher,nullptr);rankedWatcher=nullptr;}
    std::lock_guard<std::mutex> guard(lock);rankedWatches.clear();
}
extern "C" void Port_RankedDisconnected(int peerGone){
    std::lock_guard<std::mutex> guard(lock);
    if(d.ranked&&d.sawFight)watchFinish(false,true,peerGone!=0);
}
extern "C" int Port_RankedTakeSettlement(int *rating,int *delta){
    if(!settlementReady)return 0;
    std::lock_guard<std::mutex> guard(lock);
    Uint64 now=SDL_GetTicks();
    if(rankedSettlements.empty()||now<nextSettlement)return 0;
    *rating=rankedSettlements.front().first;
    *delta=rankedSettlements.front().second;
    rankedSettlements.pop_front();settlementReady=!rankedSettlements.empty();
    nextSettlement=now+2000;
    return 1;
}
extern "C" int Port_RankedResult(int *rating,int *delta){
    std::lock_guard<std::mutex> g(lock);
    if(rating)*rating=d.resultRating;if(delta)*delta=d.resultDelta;return d.reportState;
}
extern "C" void Port_RankedObserve(int battleState,int bits,unsigned tick,unsigned confirmed,int resimulating,int localConcession){
    std::lock_guard<std::mutex> g(lock);
    if(!d.on||!d.ranked||resimulating||!d.descriptor.is_object())return;
    // Discovery polling ends after the handshake. Consume result acknowledgments
    // here while the confirmed fight/result ticks continue.
    pump();
    Uint64 now=SDL_GetTicks();
    if(d.reportSerial){
        if(d.reportState==1&&!d.reportPending&&now>=d.reportAt){
            d.reportSerial=++serial;d.reportPending=rpc("ranked_report",d.report,d.reportSerial);
            d.reportAt=now+3000;
        }
        return;
    }
    if(battleState==3){if(!d.sawFight)watchFight();d.sawFight=true;d.outcomeBits=0;return;}
    if(!d.sawFight||battleState<4||battleState>6||!(bits&15)){d.outcomeBits=0;return;}
    bits=bits&8?8:bits&7;
    if(d.outcomeBits!=bits){d.outcomeBits=bits;d.outcomeTick=tick;return;}
    if(bits==8){
        if(localConcession>=0&&confirmed>=d.outcomeTick+2)watchFinish(localConcession!=0,true,localConcession==0);
        return;
    }
    // A tentative finish must outlive the rollback window and confirmed input.
    if(tick-d.outcomeTick<20||confirmed<d.outcomeTick+2)return;
    int winner=(bits&3)==1?0:(bits&3)==2?1:2;
    char canonical[256],digest[65];
    snprintf(canonical,sizeof(canonical),"BT4-RANKED-V1|%s|%s|%s|%d",text(d.descriptor,"generation").c_str(),
             d.content.c_str(),text(d.profile,"ruleset").c_str(),winner);
    UpdSha256_Hex(canonical,strlen(canonical),digest);
    d.report={{"match",text(d.descriptor,"generation")},{"outcome",winner},{"digest",digest}};
    d.reportState=1;d.reportSerial=++serial;d.reportAt=now+3000;
    watchFinish(false);
    d.reportPending=rpc("ranked_report",d.report,d.reportSerial);
}

/* UI presence is not a room and does not hold a Realtime subscription. Only
 * authenticated aggregate counts return to the menu, never identities/IPs. */
extern "C" int Port_OnlinePresence(int active){
    static bool wasActive=false,known=false;
    static unsigned request=0,nextSerial=400000;
    static Uint64 next=0;
    static int count=0;
    char buffer[1024];
    while(Bt4Signal_Read(3,buffer,sizeof(buffer))>0){
        Dj event=Dj::parse(buffer,nullptr,false);
        if(!event.is_object()||number(event,"serial")!=request)continue;
        auto body=event.find("body");
        if(body!=event.end()&&body->is_object()&&body->contains("online")&&(*body)["online"].is_number_unsigned()){
            count=(int)std::min(100000u,number(*body,"online"));known=true;
        }
    }
    Uint64 now=SDL_GetTicks();
    if(active&&(!wasActive||now>=next)){
        request=++nextSerial;
        Bt4Signal_Command(3,request,"presence","{\"active\":true}");
        next=now+300000;
    }else if(!active&&wasActive){
        request=++nextSerial;
        Bt4Signal_Command(3,request,"presence","{\"active\":false}");
    }
    wasActive=active!=0;
    return known?count:-1;
}

namespace {
std::mutex rankedLock;
Bt4RankedView rankedView={};
std::array<Bt4RankedEntry,100> rankedEntries={};int rankedTotal=0;
unsigned rankedStatusRequest=0,rankedTopRequest=0,nicknameRequest=0,rankedSerial=800000;
Uint64 nextRankedStatus=0,nextRankedTop=0,rankedStatusDeadline=0,rankedTopDeadline=0,nicknameDeadline=0;
Uint64 nextNicknameCheck=0;
int nicknameState=BT4_NICKNAME_LOADING;
std::string nickname,profileProject;
constexpr Uint64 ProfileTimeout=45000,TopInterval=600000;
void rankedPage(){
    rankedView.pages=std::max(1,(rankedTotal+4)/5);
    rankedView.page=std::max(0,std::min(rankedView.page,rankedView.pages-1));
    rankedView.count=std::min(5,rankedTotal-rankedView.page*5);
    memset(rankedView.top,0,sizeof(rankedView.top));
    for(int i=0;i<rankedView.count;++i)rankedView.top[i]=rankedEntries[rankedView.page*5+i];
}
void profileRoute(){
    const char *project=Bt4Signal_Project();
    if(profileProject==project)return;
    profileProject=project;
    rankedStatusRequest=rankedTopRequest=nicknameRequest=0;
    nextRankedStatus=nextRankedTop=0;
    rankedView.state=0;rankedView.rating=rankedView.wins=rankedView.losses=0;
    rankedView.season[0]=0;
    nicknameState=BT4_NICKNAME_LOADING;nickname.clear();
    nextNicknameCheck=0;
}
unsigned profileRequest(const char *op,const char *args){
    unsigned id=++rankedSerial;
    return Bt4Signal_Command(4,id,op,args)?id:0;
}
void profilePump(Uint64 now){
    char buffer[24577];
    while(Bt4Signal_Read(4,buffer,sizeof(buffer))>0){
        Dj event=Dj::parse(buffer,nullptr,false);unsigned id=number(event,"serial");
        auto project=text(event,"project");
        if(!id||(!project.empty()&&project!=profileProject))continue;
        auto body=event.find("body");if(body==event.end()||!body->is_object())continue;
        if(id==nicknameRequest){
            nicknameRequest=0;
            auto state=text(*body,"state"),error=text(*body,"error");
            if(state=="ready"){
                nickname=text(*body,"name");
                nicknameState=!nickname.empty()&&nickname.size()<=19?BT4_NICKNAME_READY:BT4_NICKNAME_UNAVAILABLE;
            }else if(state=="required")nicknameState=BT4_NICKNAME_REQUIRED;
            else if(state=="optional")nicknameState=BT4_NICKNAME_OPTIONAL;
            else if(state=="disabled"||(error=="BAD_REQUEST"&&nicknameState==BT4_NICKNAME_LOADING))nicknameState=BT4_NICKNAME_DISABLED;
            else if(error=="NICKNAME_TAKEN")nicknameState=BT4_NICKNAME_TAKEN;
            else if(error=="INVALID_NICKNAME")nicknameState=BT4_NICKNAME_INVALID;
            else nicknameState=BT4_NICKNAME_UNAVAILABLE;
            nextNicknameCheck=now+(nicknameState==BT4_NICKNAME_READY?300000:30000);
            continue;
        }
        if(id==rankedStatusRequest){
            rankedStatusRequest=0;
            auto enabled=body->find("enabled");
            rankedView.state=enabled!=body->end()&&enabled->is_boolean()&&enabled->get<bool>()?1:-1;
            snprintf(rankedView.season,sizeof(rankedView.season),"%s",text(*body,"season").c_str());
            rankedView.rating=(int)std::min(100000u,number(*body,"rating"));
            rankedView.wins=(int)std::min(100000u,number(*body,"wins"));
            rankedView.losses=(int)std::min(100000u,number(*body,"losses"));
        }else if(id==rankedTopRequest){
            rankedTopRequest=0;
            auto entries=body->find("entries");
            if(entries==body->end()||!entries->is_array()){
                if(rankedView.topState==0)rankedView.topState=-1;
                nextRankedTop=now+15000;
                continue;
            }
            rankedTotal=0;rankedView.topState=1;
            snprintf(rankedView.updated,sizeof(rankedView.updated),"%s",text(*body,"updated_at").c_str());
            for(const auto &row:*entries){
                if(!row.is_object()||rankedTotal>=100)continue;
                auto &r=rankedEntries[rankedTotal++];
                snprintf(r.name,sizeof(r.name),"%s",text(row,"name").c_str());
                r.rating=(int)std::min(100000u,number(row,"rating"));
                r.wins=(int)std::min(100000u,number(row,"wins"));
                r.losses=(int)std::min(100000u,number(row,"losses"));
            }
            rankedPage();
        }
    }
    if(rankedStatusRequest&&now>=rankedStatusDeadline){rankedStatusRequest=0;rankedView.state=-1;}
    if(rankedTopRequest&&now>=rankedTopDeadline){
        rankedTopRequest=0;nextRankedTop=now+15000;
        if(rankedView.topState==0)rankedView.topState=-1;
    }
    if(nicknameRequest&&now>=nicknameDeadline){nicknameRequest=0;nicknameState=BT4_NICKNAME_UNAVAILABLE;}
}
}
extern "C" void Port_RankedPage(int direction){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(direction)rankedView.page+=direction<0?-1:1;
    else rankedView.page=0;
    rankedPage();
}
extern "C" void Port_RankedRefresh(int refreshStatus){
    std::lock_guard<std::mutex> guard(rankedLock);
    Uint64 now=SDL_GetTicks();
    profileRoute();profilePump(now);
    const char *args="{\"ruleset\":\"solo\"}";
    if(refreshStatus&&!rankedStatusRequest&&now>=nextRankedStatus){
        rankedStatusRequest=profileRequest("ranked_status",args);
        nextRankedStatus=now+15000;rankedStatusDeadline=now+ProfileTimeout;
        if(!rankedStatusRequest)rankedView.state=-1;
    }
    if(!rankedTopRequest&&now>=nextRankedTop){
        rankedTopRequest=profileRequest("ranked_top",args);
        nextRankedTop=now+(rankedTopRequest?TopInterval:15000);rankedTopDeadline=now+ProfileTimeout;
        if(!rankedTopRequest&&rankedView.topState==0)rankedView.topState=-1;
    }
}
extern "C" void Port_RankedView(Bt4RankedView *out){
    if(!out)return;
    std::lock_guard<std::mutex> guard(rankedLock);
    profilePump(SDL_GetTicks());
    *out=rankedView;
}
extern "C" int Port_RankedTopTen(Bt4RankedEntry out[10]){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(rankedView.topState!=1)return rankedView.topState<0?-1:-2;
    int count=std::min(10,rankedTotal);
    std::copy_n(rankedEntries.begin(),count,out);
    return count;
}
extern "C" void Port_OnlineNicknameBegin(void){
    std::lock_guard<std::mutex> guard(rankedLock);
    profileRoute();
    if(nicknameState==BT4_NICKNAME_SAVING)return;
    nicknameRequest=0;nicknameState=BT4_NICKNAME_LOADING;
}
extern "C" int Port_OnlineNicknameView(char name[20]){
    std::lock_guard<std::mutex> guard(rankedLock);
    Uint64 now=SDL_GetTicks();
    profileRoute();profilePump(now);
    bool recheck=(nicknameState==BT4_NICKNAME_OPTIONAL||nicknameState==BT4_NICKNAME_REQUIRED)&&now>=nextNicknameCheck;
    if(!nicknameRequest&&(nicknameState==BT4_NICKNAME_LOADING||recheck)){
        nicknameRequest=profileRequest("nickname_get","{}");nicknameDeadline=now+ProfileTimeout;
        if(!nicknameRequest)nicknameState=BT4_NICKNAME_UNAVAILABLE;
    }
    if(name)snprintf(name,20,"%s",nickname.c_str());
    return nicknameState;
}
extern "C" int Port_OnlineNicknameSubmit(const char *name){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(nicknameRequest||!name||!*name)return 0;
    std::string args=Dj{{"name",name}}.dump(-1,' ',false,Dj::error_handler_t::replace);
    nicknameRequest=profileRequest("nickname_set",args.c_str());nicknameDeadline=SDL_GetTicks()+ProfileTimeout;
    nicknameState=nicknameRequest?BT4_NICKNAME_SAVING:BT4_NICKNAME_UNAVAILABLE;
    return nicknameRequest!=0;
}
