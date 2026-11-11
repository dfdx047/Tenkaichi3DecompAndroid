/* TAG TEAM's room service through Dragon Net (Dragon Rage): rooms by code, quick match and the WebRTC negotiation of
 * tag_team_p2p.cpp's participant-hosted room, by POST /v1/tag of Dragon Net's Worker (port/dragonnet/tag.js in Dragon
 * Rage's repository) in place of BT4decomp's Supabase signaling (supabase_signal.cpp). The operations, their answers
 * and the room's events are Supabase's (bt4_api); what Supabase pushes (Realtime) is polled here (op "poll"), as often
 * as the room needs (TagSignal_Pace). Only admission and SDP/ICE go this way: the game itself is peer to peer.
 *
 * The service: Android, Match_ServiceUrl() (BT3_MATCH_URL, dragonnet_url.txt or Dragon Net's default, as BT4's 1 vs 1);
 * a PC, BT4_TAG_DRAGONNET when it is a URL, else BT3_MATCH_URL. The version sent is Dragon Net's number for BT4
 * (140001, the 1 vs 1's: BT3's games never meet these rooms); BT3_MATCH_V changes it on a PC (tests). The player is
 * a random key made for this run of the game: the service keeps only its hash.
 * One background thread does every request; the game's threads only queue them and read the answers. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include "tag_signal.h"
#ifndef __ANDROID__
#include "supabase_signal.h"
extern "C" int Bt4Discovery_TagCounts(uint64_t content,int out[3]);
#endif
#include "voice_platform.inc"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
extern "C" int Match_HttpPost(const char *url,const char *json,char *out,size_t size);
extern "C" const char *Match_ServiceUrl(void);
using Dj=nlohmann::json;
namespace {
constexpr int kVersion=140001; /* Dragon Net's number for BT4 (net_match.c's DRNET_BT4_VERSION) */
constexpr unsigned kAnswerBytes=65536;
std::string str(const Dj &j,const char *key){auto i=j.find(key);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
struct Job{unsigned serial=0;std::string op;Dj args;unsigned session=0;};
struct State{
    std::mutex lock,lifecycle;SDL_Thread *thread=nullptr;SDL_Semaphore *wake=nullptr;
    std::atomic<bool> quit{false};std::atomic<int> status{0},pace{2};
    bool listening=false;unsigned session=0;std::deque<Job> jobs;std::deque<std::string> events;
    std::string key,url;int version=kVersion;
    /* counts of the quick queues (the menu's) */
    std::string countsContent;Uint64 countsAsked=0,countsAt=0;int counts[3]={0,0,0};bool countsKnown=false;
} dn;
void wake(){if(dn.wake)SDL_SignalSemaphore(dn.wake);}
void emit(const Dj &j){
    std::string text=j.dump();if(text.size()>24576)return;
    std::lock_guard<std::mutex> g(dn.lock);if(!dn.listening)return;
    if(dn.events.size()>=256){dn.status=-1;return;}
    dn.events.push_back(std::move(text));
}
const char *serviceUrl(){
    static std::string url;
    if(url.empty()){
#ifdef __ANDROID__
        const char *u=Match_ServiceUrl();
#else
        const char *a=SDL_getenv("BT4_TAG_DRAGONNET"),*u=a&&(!strncmp(a,"http://",7)||!strncmp(a,"https://",8))?a:SDL_getenv("BT3_MATCH_URL");
#endif
        url=u?u:"";while(!url.empty()&&url.back()=='/')url.pop_back();
    }
    return url.c_str();
}
/* One request: true with the service's JSON answer (which can be {error}); false when nothing usable came */
bool post(const std::string &op,const Dj &args,Dj &answer,std::vector<char> &buffer){
    Dj body={{"v",dn.version},{"key",dn.key},{"op",op},{"args",args}};std::string text=body.dump();
    if(text.size()>19000)return false;
    buffer.assign(kAnswerBytes,0);
    if(!Match_HttpPost((dn.url+"/v1/tag").c_str(),text.c_str(),buffer.data(),buffer.size()))return false;
    answer=Dj::parse(buffer.data(),nullptr,false);
    if(!answer.is_object())return false;
    /* a request that was not taken (a 4xx of the Worker, or Android's note of a 429): said as the room service would */
    if(answer.contains("error")&&!answer.contains("version")){
        auto e=str(answer,"error");answer={{"error",e=="busy"||e=="limit"||e=="RATE_LIMITED"?"RATE_LIMITED":e=="version"?"UPDATE_REQUIRED":"SIGNALING_UNAVAILABLE"}};
    }
    return true;
}
bool admission(const std::string &op){return op=="create"||op=="join"||op=="queue";}
int entry(void*){
    std::vector<char> buffer;long long cursor=0;bool active=false;Uint64 nextPoll=0,fastUntil=0,retry=0;
    while(!dn.quit){
        Uint64 now=SDL_GetTicks();Job job;bool have=false,current=false;
        {std::lock_guard<std::mutex> g(dn.lock);
            if(!dn.jobs.empty()&&now>=retry){job=std::move(dn.jobs.front());dn.jobs.pop_front();have=true;current=job.session==dn.session;}}
        if(have&&!current&&job.op!="leave"&&job.op!="cancel")have=false; /* (a closed room's work, but its cleanup) */
        if(have){
            Dj answer;
            if(job.op=="get_signals"){
                /* Dragon Net keeps each message until it has been polled: nothing to recover */
                answer={{"signals",Dj::array()},{"cursor",job.args.value("after",0ull)}};
            }else if(!post(job.op,job.args,answer,buffer)){
                fprintf(stderr,"bt4: dragon net: %s failed (no answer from %s)\n",job.op.c_str(),dn.url.c_str());
                answer={{"error","SIGNALING_UNAVAILABLE"}};
                retry=now+1000;
            }else if(!answer.contains("error")){
                if(admission(job.op)&&(!str(answer,"generation").empty()||str(answer,"state")=="waiting")){active=true;nextPoll=0;fastUntil=now+5000;}
                if(job.op=="leave")active=false;
                if(job.op=="signal")fastUntil=SDL_GetTicks()+4000;
            }
            if(answer.contains("error"))fprintf(stderr,"bt4: dragon net: %s: %s\n",job.op.c_str(),str(answer,"error").c_str());
            {std::lock_guard<std::mutex> g(dn.lock);current=job.session==dn.session;}
            if(!current&&admission(job.op)){
                /* the room was closed while this was asked: leave what it made */
                Job cleanup;cleanup.op=str(answer,"generation").empty()?"cancel":"leave";cleanup.args=Dj::object();
                if(!str(answer,"generation").empty())cleanup.args["generation"]=str(answer,"generation");
                std::lock_guard<std::mutex> g(dn.lock);cleanup.session=dn.session;dn.jobs.push_front(std::move(cleanup));
            }else if(current)emit({{"event","reply"},{"serial",job.serial},{"project",dn.url},{"body",answer}});
            continue;
        }
        bool listening;{std::lock_guard<std::mutex> g(dn.lock);listening=dn.listening;}
        if(active&&listening&&now>=nextPoll){
            Dj answer;
            if(post("poll",{{"after",cursor}},answer,buffer)&&!answer.contains("error")){
                auto list=answer.find("events");
                if(list!=answer.end()&&list->is_array())for(auto &e:*list){
                    auto body=e.find("body");if(!e.is_object()||body==e.end()||!body->is_object())continue;
                    auto name=str(e,"event");if(name=="bt4.signal")fastUntil=SDL_GetTicks()+4000;
                    if(name=="bt4.signal"&&str(*body,"channel")!="tag")continue;
                    emit({{"event",name},{"body",*body}});
                }
                auto c=answer.find("cursor");if(c!=answer.end()&&c->is_number_integer()&&c->get<long long>()>=cursor)cursor=c->get<long long>();
                bool more=list!=answer.end()&&list->is_array()&&list->size()>=32;
                int level=dn.pace.load();Uint64 at=SDL_GetTicks();
                nextPoll=at+(more||level>=2||at<fastUntil?250:level==1?1000:15000);
            }else nextPoll=SDL_GetTicks()+(str(answer,"error")=="RATE_LIMITED"?5000:2000);
            continue;
        }
        {
            bool ask;std::string content;{std::lock_guard<std::mutex> g(dn.lock);content=dn.countsContent;
                ask=!content.empty()&&now-dn.countsAsked<5000&&(!dn.countsAt||now-dn.countsAt>=30000);if(ask)dn.countsAt=now;}
            if(ask){Dj answer;
                if(post("counts",{{"wire","bt4-p2p-2"},{"content",content}},answer,buffer)&&!answer.contains("error")){
                    std::lock_guard<std::mutex> g(dn.lock);const char *keys[3]={"F6","T2","T5"};
                    for(int i=0;i<3;++i){auto v=answer.find(keys[i]);dn.counts[i]=v!=answer.end()&&v->is_number_integer()?v->get<int>():0;}dn.countsKnown=true;
                }
                continue;
            }
        }
        Uint64 wait=active&&listening?(nextPoll>now?nextPoll-now:0):100;
        SDL_WaitSemaphoreTimeout(dn.wake,(Sint32)(wait>100?100:wait));
    }
    return 0;
}
int dnOpen(){
    std::lock_guard<std::mutex> life(dn.lifecycle);
    if(dn.thread)return 1;
    dn.url=serviceUrl();if(dn.url.empty())return 0;
    if(dn.key.empty()){unsigned char raw[32];char hex[65];if(!voiceRandom(raw))return 0;for(int i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",raw[i]);dn.key=hex;}
#ifndef __ANDROID__
    if(const char *v=SDL_getenv("BT3_MATCH_V"))dn.version=SDL_atoi(v);
#endif
    dn.quit=false;dn.status=1;dn.wake=SDL_CreateSemaphore(0);if(!dn.wake)return 0;
    dn.thread=SDL_CreateThread(entry,"BT4 Dragon Net TAG TEAM",nullptr);
    if(!dn.thread){SDL_DestroySemaphore(dn.wake);dn.wake=nullptr;return 0;}
    fprintf(stderr,"bt4: TAG TEAM rooms through Dragon Net (%s, v=%d)\n",dn.url.c_str(),dn.version);
    return 1;
}
}

extern "C" int TagSignal_DragonNet(void){
#ifdef __ANDROID__
    return 1;
#else
    static int on=-1;
    if(on<0){const char *a=SDL_getenv("BT4_TAG_DRAGONNET"),*b=SDL_getenv("BT3_MATCH_URL");on=(a&&a[0])||(b&&b[0]);}
    return on;
#endif
}
extern "C" const char *TagSignal_Project(void){
#ifndef __ANDROID__
    if(!TagSignal_DragonNet())return Bt4Signal_Project();
#endif
    return serviceUrl();
}
extern "C" int TagSignal_Open(void){
#ifndef __ANDROID__
    if(!TagSignal_DragonNet())return Bt4Signal_Open();
#endif
    return dnOpen();
}
extern "C" void TagSignal_Listen(int consumer,int enabled){
#ifndef __ANDROID__
    if(!TagSignal_DragonNet()){Bt4Signal_Listen(consumer,enabled);return;}
#endif
    if(consumer!=1)return;
    std::lock_guard<std::mutex> g(dn.lock);
    if(enabled){dn.listening=true;return;}
    /* closed: its answers and events go, so does its work (its leave and cancel stay) */
    dn.listening=false;dn.events.clear();++dn.session;dn.pace=2;
    for(auto it=dn.jobs.begin();it!=dn.jobs.end();)if(it->op!="leave"&&it->op!="cancel")it=dn.jobs.erase(it);else ++it;
}
extern "C" int TagSignal_Command(int consumer,unsigned serial,const char *op,const char *args){
#ifndef __ANDROID__
    if(!TagSignal_DragonNet())return Bt4Signal_Command(consumer,serial,op,args);
#endif
    static const char *allowed[]={"create","join","get","get_signals","lease","leave","queue","cancel","signal"};bool found=false;
    if(consumer!=1||!op||!args||strlen(args)>18000)return 0;
    for(auto a:allowed)if(!strcmp(op,a)){found=true;break;}if(!found)return 0;
    Dj body=Dj::parse(args,nullptr,false);if(!body.is_object())return 0;
    if(!dnOpen())return 0;
    std::lock_guard<std::mutex> g(dn.lock);if(dn.jobs.size()>=64)return 0;dn.listening=true;
    if(!strcmp(op,"cancel"))for(auto it=dn.jobs.begin();it!=dn.jobs.end();)if(it->op=="queue")it=dn.jobs.erase(it);else ++it;
    Job job;job.serial=serial;job.op=op;job.args=std::move(body);job.session=dn.session;
    dn.jobs.push_back(std::move(job));wake();return 1;
}
extern "C" int TagSignal_Read(int consumer,char *out,size_t capacity){
#ifndef __ANDROID__
    if(!TagSignal_DragonNet())return Bt4Signal_Read(consumer,out,capacity);
#endif
    if(consumer!=1||!out||!capacity)return 0;
    std::lock_guard<std::mutex> g(dn.lock);if(dn.events.empty())return 0;
    if(dn.events.front().size()+1>capacity)return -1;
    memcpy(out,dn.events.front().c_str(),dn.events.front().size()+1);dn.events.pop_front();return 1;
}
extern "C" void TagSignal_Pace(int level){
    if(!TagSignal_DragonNet())return;
    int old=dn.pace.exchange(level);if(level>old)wake();
}
extern "C" int TagSignal_QueueCounts(uint64_t content,int out[3]){
#ifndef __ANDROID__
    if(!TagSignal_DragonNet())return Bt4Discovery_TagCounts(content,out);
#endif
    if(!out||!dnOpen())return 0;
    char hex[17];snprintf(hex,sizeof(hex),"%016llx",(unsigned long long)content);
    std::lock_guard<std::mutex> g(dn.lock);
    if(dn.countsContent!=hex){dn.countsContent=hex;dn.countsKnown=false;dn.countsAt=0;}
    dn.countsAsked=SDL_GetTicks();wake();
    for(int i=0;i<3;++i)out[i]=dn.counts[i];
    return dn.countsKnown;
}
