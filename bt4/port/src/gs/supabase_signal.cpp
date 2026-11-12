/* Supabase Auth + PostgREST + private Realtime inbox. No game/audio relay API.
 * Network work is owned by one background thread. Credentials are never logged.
 */
#include <SDL3/SDL.h>
#include "../../include/online_room_policy.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
#include <array>
#include <mutex>
#include <atomic>
#include <memory>
#include <algorithm>
#ifndef _WIN32
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include "supabase_signal.h"
#include "net_update.h"
#include "online_route.h"
#include <ctime>
#include "voice_platform.inc"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
#ifdef _WIN32
#include "voice_transport.inc"
#else
#include "voice_transport_curl.inc"
#endif
using Sj=nlohmann::json;
namespace {
std::mutex lifecycle;

std::string str(const Sj &j,const char *key){auto i=j.find(key);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
struct Job{
    int consumer=0;unsigned serial=0;std::string op;Sj args;
    unsigned session=0;int project=-1;std::string generation;
    Uint64 notBefore=0;
};
struct Identity {
    std::string access,refresh,user,file;Uint64 renewAt=0;
#ifdef _WIN32
    HANDLE lock=INVALID_HANDLE_VALUE;
#else
    int lock=-1;
#endif
};
std::array<Identity,2> identities;
int activeProject=0;long lastHttpStatus=0;
const bt4route::Endpoint &endpoint(){return bt4route::endpoints[activeProject];}
Identity &currentIdentity(){return identities[activeProject];}
#ifdef _WIN32
static std::wstring pathWide(const std::string &path){int size=MultiByteToWideChar(CP_UTF8,0,path.c_str(),-1,nullptr,0);if(size<=0)return {};std::wstring wide(size,0);MultiByteToWideChar(CP_UTF8,0,path.c_str(),-1,&wide[0],size);wide.resize(size-1);return wide;}
#endif
static void unlockIdentities(){for(auto &i:identities){
#ifdef _WIN32
    if(i.lock!=INVALID_HANDLE_VALUE)CloseHandle(i.lock);i.lock=INVALID_HANDLE_VALUE;
#else
    if(i.lock>=0)close(i.lock);i.lock=-1;
#endif
}}
static std::string sessionFolder(){
    const char *override=SDL_getenv("BT4_SIGNAL_SESSION_DIR");if(override&&*override)return override;
    char *pref=SDL_GetPrefPath("NauTaro","BT4decomp");if(!pref)return {};
    std::string folder=std::string(pref)+"network";SDL_free(pref);return folder;
}
static bool identitySlot(){
    auto &identity=currentIdentity();if(!identity.file.empty())return true;
    auto folder=sessionFolder();if(folder.empty()||!SDL_CreateDirectory(folder.c_str()))return false;
    std::string projectRef=endpoint().host;projectRef=projectRef.substr(0,projectRef.find('.'));
    for(int slot=0;slot<16;++slot){std::string file=folder+"/"+projectRef+"-"+std::to_string(slot);
#ifdef _WIN32
        auto wide=pathWide(file+".lock");identity.lock=CreateFileW(wide.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(identity.lock==INVALID_HANDLE_VALUE)continue;
#else
        identity.lock=open((file+".lock").c_str(),O_RDWR|O_CREAT,0600);if(identity.lock<0)continue;
        if(flock(identity.lock,LOCK_EX|LOCK_NB)){close(identity.lock);identity.lock=-1;continue;}
#endif
        static bool registered=false;if(!registered){atexit(unlockIdentities);registered=true;}
        identity.file=file+".json";
        if(auto input=SDL_IOFromFile(identity.file.c_str(),"rb")){Sint64 size=SDL_GetIOSize(input);if(size>0&&size<4096){std::string data((size_t)size,0);if(SDL_ReadIO(input,&data[0],data.size())==data.size()){
            auto saved=Sj::parse(data,nullptr,false);if(saved.is_object()){auto token=str(saved,"refresh_token");if(token.size()<=1024)identity.refresh=token;}}}SDL_CloseIO(input);}
        return true;
    }return false;
}
static void saveIdentity(){
    auto &identity=currentIdentity();if(identity.file.empty())return;
    std::string temp=identity.file+".tmp",data=Sj{{"refresh_token",identity.refresh}}.dump();
    auto out=SDL_IOFromFile(temp.c_str(),"wb");if(!out)return;bool good=SDL_WriteIO(out,data.data(),data.size())==data.size();good=SDL_CloseIO(out)&&good;if(!good)return;
#ifdef _WIN32
    auto a=pathWide(temp),b=pathWide(identity.file);MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
#else
    chmod(temp.c_str(),0600);rename(temp.c_str(),identity.file.c_str());
#endif
}
struct State {
    std::mutex lock;SDL_Thread *thread=nullptr;SDL_Semaphore *wake=nullptr;
    std::atomic<bool> quit{false};std::atomic<int> status{0};
    unsigned consumers=0,realtime=0;
    std::atomic<int> projectIndex{0};
    std::deque<std::string> releasedGenerations;
    std::array<unsigned,6> sessions={};
    std::string roomGeneration;
    std::deque<Job> jobs;std::array<std::deque<std::string>,6> events;
} state;
static void wake(void*){if(state.wake)SDL_SignalSemaphore(state.wake);}
static bool emit(int consumer,const Sj &j){
    if(consumer<0||consumer>5)return false;
    std::string body=j.dump();if(body.size()>24576)return false;
    std::lock_guard<std::mutex> g(state.lock);
    if(!(state.consumers&(1u<<consumer)))return true;
    if(state.events[consumer].size()>=128){state.status=-1;return false;}
    state.events[consumer].push_back(std::move(body));return true;
}
static bool http(const std::string &path,const Sj &body,bool authenticated,Sj &answer){
    auto &identity=currentIdentity();lastHttpStatus=0;
    const std::string data=body.dump();std::string result;long code=0;
    if(data.size()>20000)return false;
#ifdef _WIN32
    HINTERNET session=WinHttpOpen(L"BT4decomp signaling",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)return false;WinHttpSetTimeouts(session,10000,10000,10000,20000);
    HINTERNET connection=WinHttpConnect(session,pathWide(endpoint().host).c_str(),443,0);
    std::wstring wide(path.begin(),path.end());
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"POST",wide.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr;
    std::string headers="Content-Type: application/json\r\napikey: "+std::string(endpoint().key)+"\r\n";
    if(authenticated)headers+="Authorization: Bearer "+identity.access+"\r\n";
    std::wstring wh(headers.begin(),headers.end());
    bool ok=request&&WinHttpSendRequest(request,wh.c_str(),(DWORD)wh.size(),(void*)data.data(),(DWORD)data.size(),(DWORD)data.size(),0)&&WinHttpReceiveResponse(request,nullptr);
    if(ok){DWORD status=0,size=sizeof(status),got;ok=WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);code=status;
        char bytes[4096];while(ok){if(!WinHttpReadData(request,bytes,sizeof(bytes),&got)){ok=false;break;}if(!got)break;if(result.size()+got>65536){ok=false;break;}result.append(bytes,got);}}
    if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
    if(!ok)return false;
#else
    CURL *request=curl_easy_init();if(!request)return false;curl_slist *headers=nullptr;
    headers=curl_slist_append(headers,"Content-Type: application/json");
    headers=curl_slist_append(headers,("apikey: "+std::string(endpoint().key)).c_str());
    if(authenticated)headers=curl_slist_append(headers,("Authorization: Bearer "+identity.access).c_str());
    std::string url=std::string(endpoint().url)+path;
    curl_easy_setopt(request,CURLOPT_URL,url.c_str());curl_easy_setopt(request,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(request,CURLOPT_POSTFIELDS,data.c_str());curl_easy_setopt(request,CURLOPT_POSTFIELDSIZE,(long)data.size());
    curl_easy_setopt(request,CURLOPT_TIMEOUT,8L);curl_easy_setopt(request,CURLOPT_CONNECTTIMEOUT,4L);curl_easy_setopt(request,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(request,CURLOPT_WRITEFUNCTION,+[](char *data,size_t size,size_t n,void *p)->size_t{auto &out=*static_cast<std::string*>(p);if(n>65536||size>65536||size*n>65536-out.size())return 0;out.append(data,size*n);return size*n;});
    curl_easy_setopt(request,CURLOPT_WRITEDATA,&result);
    auto rc=curl_easy_perform(request);curl_easy_getinfo(request,CURLINFO_RESPONSE_CODE,&code);
    curl_easy_cleanup(request);curl_slist_free_all(headers);if(rc!=CURLE_OK)return false;
#endif
    lastHttpStatus=code;answer=Sj::parse(result,nullptr,false);
    return code>=200&&code<300&&!answer.is_discarded();
}
static bool authenticate(){
    auto &identity=currentIdentity();
    if(!identity.access.empty()&&SDL_GetTicks()<identity.renewAt)return true;
    if(!identitySlot())return false;
    Sj reply;bool refreshing=!identity.refresh.empty();
    if(!http(refreshing?"/auth/v1/token?grant_type=refresh_token":"/auth/v1/signup",refreshing?Sj{{"refresh_token",identity.refresh}}:Sj{{"data",{{"client","BT4decomp"}}}},false,reply)){
        auto code=reply.is_object()?str(reply,"error_code"):std::string();
        if(!refreshing||(code!="refresh_token_not_found"&&code!="refresh_token_already_used"))return false;
        identity.refresh.clear();if(!http("/auth/v1/signup",{{"data",{{"client","BT4decomp"}}}},false,reply))return false;
    }
    std::string access=str(reply,"access_token"),refresh=str(reply,"refresh_token");auto user=reply.find("user");
    if(access.empty()||access.size()>8192||refresh.empty()||refresh.size()>1024||user==reply.end()||!user->is_object())return false;
    std::string id=str(*user,"id");if(id.size()!=36)return false;
    identity.access=std::move(access);identity.refresh=std::move(refresh);identity.user=std::move(id);
    unsigned ttl=300;auto duration=reply.find("expires_in");if(duration!=reply.end()&&duration->is_number_unsigned())ttl=duration->get<unsigned>();
    if(ttl>3600)ttl=3600;if(ttl<120)ttl=120;identity.renewAt=SDL_GetTicks()+(Uint64)(ttl-60)*1000;saveIdentity();return true;
}

static int entry(void*){
    std::unique_ptr<VoiceWs> ws;unsigned serial=0;std::string joinRef;bool joined=false;
    Uint64 nextBeat=0,joinAt=0,reconnect=0,nextPolicy=0,retryHttp=0;unsigned failures=0,httpFailures=0;
    bt4route::Policy policy;std::string pinnedGeneration;
    bool onlineMenu=false;
    auto setPin=[&](const std::string &generation){
        pinnedGeneration=generation;
        std::lock_guard<std::mutex> g(state.lock);state.roomGeneration=generation;
    };
    auto currentJob=[&](const Job &job){
        std::lock_guard<std::mutex> g(state.lock);
        return job.session==state.sessions[job.consumer];
    };
    std::array<Job,2> queueJob;std::array<bool,2> queued={false,false};
    std::array<Uint64,2> uncertain={0,0};
    auto disconnect=[&](){ws.reset();joined=false;joinAt=0;reconnect=0;state.status=0;};
    auto selectProject=[&](int selected){if(selected!=activeProject){disconnect();activeProject=selected;state.projectIndex=selected;retryHttp=0;httpFailures=0;}};
    auto post=[&](const char *event,const Sj &payload,const std::string &topic){std::string ref=std::to_string(++serial);if(ws)ws->text(Sj{{"topic",topic},{"event",event},{"payload",payload},{"ref",ref},{"join_ref",joinRef}}.dump());return ref;};
    auto rememberRoom=[&](int consumer,const Sj &body){
        if(!body.is_object()||str(body,"room").empty()||str(body,"generation").empty())return;
        if(pinnedGeneration.empty())setPin(str(body,"generation"));
        if(consumer>=0&&consumer<2)queued[consumer]=false;
    };
    auto readCache=[&](){
        auto path=sessionFolder()+"/online-route.json";auto f=SDL_IOFromFile(path.c_str(),"rb");if(!f)return std::string();
        Sint64 size=SDL_GetIOSize(f);std::string data;if(size>0&&size<=8192){data.resize((size_t)size);if(SDL_ReadIO(f,&data[0],data.size())!=data.size())data.clear();}SDL_CloseIO(f);return data;
    };
    auto writeCache=[&](const std::string &data){
        auto folder=sessionFolder();if(folder.empty()||!SDL_CreateDirectory(folder.c_str()))return;
        auto path=folder+"/online-route.json",temp=path+"."+std::to_string(SDL_GetTicksNS())+".tmp";
        auto f=SDL_IOFromFile(temp.c_str(),"wb");if(!f)return;bool ok=SDL_WriteIO(f,data.data(),data.size())==data.size();ok=SDL_CloseIO(f)&&ok;
        if(!ok){remove(temp.c_str());return;}
#ifdef _WIN32
        auto a=pathWide(temp),b=pathWide(path);if(!MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))remove(temp.c_str());
#else
        chmod(temp.c_str(),0600);if(rename(temp.c_str(),path.c_str()))remove(temp.c_str());
#endif
    };
    {bt4route::Policy cached;auto data=readCache();if(bt4route::decode<Sj>(data,(uint64_t)time(nullptr),policy,cached)){
        policy=std::move(cached);Update_SetOnlineRequired(policy.needsUpdate());
    }}
    while(!state.quit){
        Uint64 now=SDL_GetTicks();bool realtimeWanted,workWanted;std::deque<std::string> released;
        {std::lock_guard<std::mutex> g(state.lock);realtimeWanted=(state.realtime&7u)!=0;workWanted=realtimeWanted||onlineMenu||!state.jobs.empty();released.swap(state.releasedGenerations);}
        for(const auto &generation:released)if(generation==pinnedGeneration)setPin("");
        for(int c=0;c<2;c++)if(queued[c]&&!currentJob(queueJob[c]))queued[c]=false;
        if(workWanted&&now>=nextPolicy){
            nextPolicy=now+60000;char envelope[8193];bt4route::Policy next;
            if(Update_FetchOnlineRoute(envelope,sizeof(envelope))&&bt4route::decode<Sj>(envelope,(uint64_t)time(nullptr),policy,next)){
                bool switched=next.epoch>policy.epoch&&next.active!=policy.active;
                bool changed=next.epoch!=policy.epoch||next.active!=policy.active;policy=std::move(next);writeCache(envelope);
                Update_SetOnlineRequired(policy.needsUpdate());
                if(changed&&pinnedGeneration.empty()){
                    // A signed route change follows the old route's confirmed fence.
                    // Retrying an unpaired search needs no round trip to the failed endpoint.
                    if(switched)uncertain[activeProject]=0;
                    for(int c=0;c<2;c++)if(queued[c]){
                        Sj cancelled;bool acknowledged=switched||(authenticate()&&http("/rest/v1/rpc/bt4_api",{{"p_op","cancel"},{"p_args",Sj::object()}},true,cancelled)
                            &&str(cancelled,"state")=="cancelled"&&!cancelled.contains("error"));
                        // The queue may have matched just before cancellation. A
                        // leave acknowledgement also clears that newly formed room.
                        Sj left;if(acknowledged&&!switched)acknowledged=http("/rest/v1/rpc/bt4_api",{{"p_op","leave"},{"p_args",Sj::object()}},true,left)
                            &&str(left,"state")=="left"&&!left.contains("error");
                        // A lost queue response can already have formed a thirty-minute room.
                        // Do not re-enter another project before that lease can expire.
                        if(!acknowledged)uncertain[activeProject]=std::max(uncertain[activeProject],SDL_GetTicks()+BT4_ROOM_IDLE_MS+5000);
                        Job retry=queueJob[c];retry.project=-1;retry.generation.clear();
                        retry.notBefore=acknowledged?0:uncertain[activeProject];
                        std::lock_guard<std::mutex> g(state.lock);
                        state.jobs.erase(std::remove_if(state.jobs.begin(),state.jobs.end(),[&](const Job &j){return j.consumer==c&&(j.op=="queue"||j.op=="ranked_queue");}),state.jobs.end());
                        state.jobs.push_front(std::move(retry));queued[c]=false;
                    }
                }
            }
        }
        if(pinnedGeneration.empty())selectProject(policy.active);
        auto &identity=currentIdentity();
        if(!realtimeWanted&&ws)disconnect();
        if(!ws&&realtimeWanted&&now>=reconnect){
            if(!authenticate()){
                state.status=-1;failures=std::min(5u,failures+1);reconnect=now+(1000u<<failures);
                // Do not skip the HTTP queue. Room admission and cleanup must
                // continue even while the private Realtime inbox reconnects.
            }else{
                std::string url=std::string(endpoint().url)+"/realtime/v1/websocket?apikey="+endpoint().key+"&vsn=1.0.0";
                ws=std::make_unique<VoiceWs>(url,"",0,"",true,wake,nullptr);joined=false;joinAt=0;state.status=0;
            }
        }
        if(ws&&ws->status<0){disconnect();state.status=-1;failures=std::min(5u,failures+1);reconnect=now+(1000u<<failures);nextPolicy=std::min(nextPolicy,now+5000);}
        if(ws&&ws->status==1&&!joinAt){joinRef=std::to_string(serial+1);post("phx_join",{{"config",{{"private",true},{"broadcast",{{"ack",false},{"self",false}}},{"presence",{{"enabled",false}}},{"postgres_changes",Sj::array()}}},{"access_token",identity.access}},"realtime:bt4:inbox:"+identity.user);joinAt=now;nextBeat=now+20000;}
        VoiceWsMessage message;unsigned count=0;
        while(ws&&count++<128&&ws->read(message)){
            if(message.binary||message.bytes.size()>24576){state.status=-1;continue;}
            Sj j=Sj::parse(message.bytes.begin(),message.bytes.end(),nullptr,false);if(!j.is_object())continue;
            auto payload=j.find("payload");if(payload==j.end()||!payload->is_object())continue;
            if(str(j,"event")=="phx_reply"&&str(j,"ref")==joinRef){joined=str(*payload,"status")=="ok";state.status=joined?1:-1;failures=0;
                if(joined)for(int consumer=0;consumer<3;++consumer)emit(consumer,{{"event","bt4.ready"},{"body",Sj::object()}});}
            if(str(j,"event")=="broadcast"&&joined){
                auto event=str(*payload,"event");auto body=payload->find("payload");if(body==payload->end()||!body->is_object())continue;
                if(event=="bt4.signal"){auto channel=str(*body,"channel");int consumer=channel=="tag"?1:channel=="voice"?2:channel=="duel"?0:-1;if(consumer>=0)emit(consumer,{{"event",event},{"body",*body}});}
                else if(event=="bt4.room"||event=="bt4.closed"){
                    if(event=="bt4.room"&&pinnedGeneration.empty())rememberRoom(queued[1]?1:0,*body);
                    for(int consumer=0;consumer<3;++consumer)emit(consumer,{{"event",event},{"body",*body}});
                }
            }
        }
        if(ws&&ws->status==1&&joinAt&&!joined&&now>joinAt+10000){disconnect();state.status=-1;reconnect=now+30000;}
        if(joined&&now>=nextBeat){post("heartbeat",Sj::object(),"phoenix");nextBeat=now+20000;}
        if(joined&&now>=identity.renewAt){if(authenticate())post("access_token",{{"access_token",identity.access}},"realtime:bt4:inbox:"+identity.user);else{disconnect();state.status=-1;reconnect=now+30000;}}
        Job job;bool have=false;
        {std::lock_guard<std::mutex> g(state.lock);
            // A deferred search must not block a cancellation, presence or room leave.
            auto it=std::find_if(state.jobs.begin(),state.jobs.end(),[&](const Job &j){
                bool retainedUdp=j.consumer==0&&j.op=="signal"&&str(j.args,"channel")=="duel"&&str(j.args,"kind")=="udp";
                bool httpOnly=j.op=="presence"||j.op=="counts"||j.op=="list"||j.op=="leave"||j.op=="cancel"||j.op=="lease"||j.op=="get"||j.op=="get_signals"||j.op=="ranked_report"||retainedUdp||j.consumer>=4;
                bool admission=j.consumer!=1&&(j.op=="create"||j.op=="join"||j.op=="queue"||j.op=="ranked_queue");
                // Native UDP discovery can recover announcements through get.
                // Tag Team SDP/ICE is transient, so its recipient must finish
                // subscribing before admission can notify the host to offer.
                return j.notBefore<=now&&(joined||httpOnly||admission);
            });
            if(it!=state.jobs.end()){job=std::move(*it);state.jobs.erase(it);have=true;}
        }
        if(have&&!currentJob(job)&&job.op!="leave"&&job.op!="cancel"&&job.op!="ranked_report")have=false;
        if(have&&(job.op=="queue"||job.op=="ranked_queue")&&pinnedGeneration.empty()){
            Uint64 safeAt=std::max(uncertain[0],uncertain[1]);
            if(now<safeAt){
                job.notBefore=safeAt;
                std::lock_guard<std::mutex> g(state.lock);
                state.jobs.erase(std::remove_if(state.jobs.begin(),state.jobs.end(),[&](const Job &j){return j.consumer==job.consumer&&(j.op=="queue"||j.op=="ranked_queue");}),state.jobs.end());
                state.jobs.push_back(std::move(job));have=false;
            }
        }
        if(have){
            Sj reply;bool admit=bt4route::admission(job.op),blocked=false;
            if((job.project>=0&&job.project!=activeProject)||
                (!job.generation.empty()&&job.generation!=pinnedGeneration&&job.op!="leave"&&job.op!="ranked_report"&&job.consumer!=5)){
                reply={{"error","ROOM_ROUTE_EXPIRED"}};blocked=true;
            }
            else if(admit&&Update_OnlineRequired()){reply={{"error","UPDATE_REQUIRED"},{"minimum",policy.minimum}};blocked=true;}
            else if(admit&&!policy.fresh((uint64_t)time(nullptr))){reply={{"error","ROUTE_REFRESH_REQUIRED"}};blocked=true;nextPolicy=0;}
            else if(job.op=="join"&&bt4route::codeProject(str(job.args,"code"))!=activeProject){reply={{"error","ROOM_ROUTE_EXPIRED"}};blocked=true;}
            else if(admit&&pinnedGeneration.empty()&&uncertain[1-activeProject]>now){reply={{"error","ROUTE_RECOVERING"}};blocked=true;}
            else if(now<retryHttp){reply={{"error","SIGNALING_UNAVAILABLE"}};blocked=true;}
            if(!blocked){
                if(admit)job.args["_client"]={{"version",BT4_CANONICAL_VERSION},{"capability",1},{"epoch",policy.epoch}};
                bool rankedRpc=job.op.compare(0,7,"ranked_")==0;
                const char *rpcPath=rankedRpc?"/rest/v1/rpc/bt4_ranked_api":"/rest/v1/rpc/bt4_api";
                auto operation=rankedRpc?job.op.substr(7):job.op;
                bool ok=authenticate()&&http(rpcPath,{{"p_op",operation},{"p_args",job.args}},true,reply);
                if(!ok&&lastHttpStatus==401){identity.access.clear();identity.renewAt=0;ok=authenticate()&&http(rpcPath,{{"p_op",operation},{"p_args",job.args}},true,reply);}
                if(!ok){
                    fprintf(stderr,"bt4: signaling: RPC %s failed (HTTP %ld)\n",job.op.c_str(),lastHttpStatus);
                    auto business=reply.is_object()?str(reply,"error"):std::string();
                    if(business!="RATE_LIMITED"&&business!="AUTH_REQUIRED")reply={{"error","SIGNALING_UNAVAILABLE"}};
                    if(job.consumer!=4){httpFailures=std::min(5u,httpFailures+1);retryHttp=now+(1000u<<httpFailures);nextPolicy=std::min(nextPolicy,now+5000);}
                    if(lastHttpStatus==0&&(job.op=="create"||job.op=="join"||job.op=="queue"||job.op=="ranked_queue"))uncertain[activeProject]=SDL_GetTicks()+BT4_ROOM_IDLE_MS+5000;
                }else{if(job.consumer!=4){httpFailures=0;retryHttp=0;}
                    if(job.op=="presence")onlineMenu=job.args.value("active",false);
                    if(job.consumer<2&&(job.op=="queue"||job.op=="ranked_queue")&&str(reply,"state")=="waiting"){queueJob[job.consumer]=job;queued[job.consumer]=true;}
                    if(job.consumer<2&&job.op=="cancel")queued[job.consumer]=false;
                    rememberRoom(job.consumer,reply);
                    if(job.op=="leave"&&!reply.contains("error")&&(str(job.args,"generation").empty()||str(job.args,"generation")==pinnedGeneration)){
                        setPin("");uncertain[activeProject]=0;
                        // Listen() owns the Realtime lifetime. A room reset can
                        // queue leave immediately before create, so leave must
                        // not clear the subscription requested by that create.
                    }
                }
            }
            if(str(reply,"error")=="UPDATE_REQUIRED")Update_SetOnlineRequired(1);
            if(str(reply,"error")=="ROUTE_CHANGED")nextPolicy=0;
            // UI may have cancelled while HTTP was in flight. Clean up only this
            // identity's result; never deliver its late descriptor to a new session.
            if(!currentJob(job)&&admit){
                if(!str(reply,"generation").empty()){
                    Job cleanup;cleanup.consumer=job.consumer;cleanup.op="leave";
                    cleanup.args={{"generation",str(reply,"generation")}};
                    cleanup.project=activeProject;cleanup.generation=str(reply,"generation");
                    std::lock_guard<std::mutex> g(state.lock);cleanup.session=state.sessions[job.consumer];
                    state.jobs.push_front(std::move(cleanup));
                }else if(str(reply,"state")=="waiting"){
                    Job cleanup;cleanup.consumer=job.consumer;cleanup.op="cancel";cleanup.args=Sj::object();cleanup.project=activeProject;
                    std::lock_guard<std::mutex> g(state.lock);cleanup.session=state.sessions[job.consumer];state.jobs.push_front(std::move(cleanup));
                }
            }else if(currentJob(job))emit(job.consumer,{{"event","reply"},{"serial",job.serial},{"project",endpoint().url},{"body",reply}});
        }
        SDL_WaitSemaphoreTimeout(state.wake,have?0:20);
    }
    ws.reset();state.status=0;return 0;
}

}
extern "C" const char *Bt4Signal_Project(void){return bt4route::endpoints[state.projectIndex.load()].url;}
extern "C" void Bt4Signal_ReleaseRoom(const char *generation){if(!generation||strlen(generation)!=36)return;std::lock_guard<std::mutex> g(state.lock);if(state.releasedGenerations.size()<8)state.releasedGenerations.emplace_back(generation);wake(nullptr);}
extern "C" int Bt4Signal_Open(void){
    std::lock_guard<std::mutex> lifecycleGuard(lifecycle);
    if(state.thread)return 1;state.quit=false;state.status=0;state.wake=SDL_CreateSemaphore(0);if(!state.wake)return 0;
    state.thread=SDL_CreateThread(entry,"BT4 Supabase signaling",nullptr);if(!state.thread){SDL_DestroySemaphore(state.wake);state.wake=nullptr;return 0;}return 1;
}
extern "C" int Bt4Signal_Status(void){return state.status;}
extern "C" void Bt4Signal_Listen(int consumer,int enabled){if(consumer<0||consumer>5)return;std::lock_guard<std::mutex> g(state.lock);if(enabled){state.consumers|=1u<<consumer;if(consumer<3)state.realtime|=1u<<consumer;}else{state.consumers&=~(1u<<consumer);state.realtime&=~(1u<<consumer);state.events[consumer].clear();++state.sessions[consumer];
        state.jobs.erase(std::remove_if(state.jobs.begin(),state.jobs.end(),[&](const Job &j){return j.consumer==consumer&&j.op!="leave"&&j.op!="cancel"&&j.op!="ranked_report";}),state.jobs.end());}}
extern "C" int Bt4Signal_Command(int consumer,unsigned serial,const char *op,const char *args){
    if(consumer<0||consumer>5||!op||!args||strlen(args)>18000)return 0;
    if((consumer==3)!=(!strcmp(op,"presence")))return 0;
    bool lifecycle=!strcmp(op,"ranked_heartbeat")||!strcmp(op,"ranked_forfeit");if((consumer==5)!=lifecycle)return 0;
    bool profile=!strcmp(op,"ranked_status")||!strcmp(op,"ranked_top")||!strcmp(op,"nickname_get")||!strcmp(op,"nickname_set");if((consumer==4)!=profile)return 0;
    if((!strcmp(op,"ranked_queue")||!strcmp(op,"ranked_report"))&&consumer!=0)return 0;
    static const char *allowed[]={"create","join","get","get_signals","lease","leave","list","queue","cancel","signal","counts","voice","presence","ranked_status","ranked_top","ranked_queue","ranked_report","ranked_heartbeat","ranked_forfeit","nickname_get","nickname_set"};bool found=false;
    for(auto kind:allowed)if(!strcmp(op,kind)){found=true;break;}if(!found)return 0;
    Sj body=Sj::parse(args,nullptr,false);if(!body.is_object())return 0;
    if(!Bt4Signal_Open())return 0;std::lock_guard<std::mutex> g(state.lock);if(state.jobs.size()>=64)return 0;state.consumers|=1u<<consumer;
    if(consumer<3&&strcmp(op,"counts")&&strcmp(op,"list"))state.realtime|=1u<<consumer;
    if(!strcmp(op,"cancel"))state.jobs.erase(std::remove_if(state.jobs.begin(),state.jobs.end(),[&](const Job &j){return j.consumer==consumer&&(j.op=="queue"||j.op=="ranked_queue");}),state.jobs.end());
    Job job;job.consumer=consumer;job.serial=serial;job.op=op;job.args=std::move(body);job.session=state.sessions[consumer];
    // Admission uses the current signed authority. Room work is pinned when
    // enqueued, so a late operation cannot mutate a room on another project.
    if(!bt4route::admission(job.op)&&job.op!="counts"&&job.op!="presence"&&consumer!=4){
        job.project=state.projectIndex.load();job.generation=state.roomGeneration;
    }
    if(job.op=="leave"&&str(job.args,"generation").empty()&&!job.generation.empty())job.args["generation"]=job.generation;
    state.jobs.push_back(std::move(job));wake(nullptr);return 1;
}
extern "C" int Bt4Signal_Read(int consumer,char *out,size_t capacity){
    if(consumer<0||consumer>5||!out||!capacity)return 0;std::lock_guard<std::mutex> g(state.lock);
    auto &events=state.events[consumer];if(events.empty())return 0;if(events.front().size()+1>capacity)return -1;
    memcpy(out,events.front().c_str(),events.front().size()+1);events.pop_front();return 1;
}
extern "C" void Bt4Signal_Close(void){
    std::lock_guard<std::mutex> lifecycleGuard(lifecycle);
    state.quit=true;wake(nullptr);if(state.thread)SDL_WaitThread(state.thread,nullptr);state.thread=nullptr;
    if(state.wake)SDL_DestroySemaphore(state.wake);state.wake=nullptr;std::lock_guard<std::mutex> g(state.lock);state.jobs.clear();state.consumers=0;state.realtime=0;state.releasedGenerations.clear();state.roomGeneration.clear();for(auto &q:state.events)q.clear();
}
