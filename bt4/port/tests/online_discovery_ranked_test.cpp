// Offline tests of the actual adapters. No sockets, sessions or HTTP worker.
#include <SDL3/SDL.h>
#include <cassert>
#include <iostream>
#include "../src/gs/supabase_signal.cpp"
struct CapturedCommand {unsigned serial;std::string op;Sj args;};
static std::vector<CapturedCommand> capturedCommands;
static int CaptureDiscoveryCommand(int,unsigned id,const char *op,const char *args){
    capturedCommands.push_back({id,op,Sj::parse(args)});return 1;
}
#define Bt4Signal_Command CaptureDiscoveryCommand
#include "../src/gs/match_supabase.cpp"
#undef Bt4Signal_Command

extern "C" void PortVoice_P2PSession(int,const char*,const PortVoiceRoomMember*,int){}
extern "C" int Update_FetchOnlineRoute(char*,size_t){return 0;}
extern "C" void Update_SetOnlineRequired(int){}
extern "C" int Update_OnlineRequired(void){return 0;}

int main(){
    assert(SDL_Init(0));
    assert(!Bt4Room_IsIdleMs(1799999,0));
    assert(Bt4Room_IsIdleMs(1800000,0));
    assert(!Bt4Room_IsIdleMs(1800000,1000));
    assert(!Bt4Room_IsIdleMs(999,1000));
    assert(!Bt4Room_IsIdleNs(UINT64_C(1799999999999),0));
    assert(Bt4Room_IsIdleNs(UINT64_C(1800000000000),0));
    watchQuit=true; // run the liveness state machine deterministically below
    Bt4Signal_Listen(4,1);profileRoute();rankedStatusRequest=1;rankedTopRequest=2;
    emit(4,{{"event","reply"},{"serial",1u},{"body",{{"enabled",true},{"season","2026-11-01"},{"rating",1234u},{"wins",3u},{"losses",1u}}}});
    Dj entries=Dj::array();
    for(int i=0;i<110;++i)entries.push_back({{"name","Fighter "+std::to_string(i+1)},{"rating",2000u-i},{"wins",2u}});
    emit(4,{{"event","reply"},{"serial",2u},{"body",{{"entries",entries}}}});
    Bt4RankedView v={};Port_RankedView(&v);
    assert(v.state==1&&v.rating==1234&&v.count==5&&v.page==0&&v.pages==20);
    assert(!strcmp(v.top[0].name,"Fighter 1"));
    Port_RankedPage(1);Port_RankedView(&v);
    assert(v.page==1&&!strcmp(v.top[0].name,"Fighter 6"));
    for(int i=0;i<30;++i)Port_RankedPage(1);
    Port_RankedView(&v);assert(v.page==19&&!strcmp(v.top[4].name,"Fighter 100"));
    for(int i=0;i<30;++i)Port_RankedPage(-1);
    Port_RankedView(&v);assert(v.page==0&&!strcmp(v.top[0].name,"Fighter 1"));
    emit(4,{{"event","reply"},{"serial",99u},{"body",{{"rating",99999u}}}});
    Port_RankedView(&v);assert(v.rating==1234);
    Bt4RankedEntry top[10]={};
    assert(Port_RankedTopTen(top)==10&&!strcmp(top[9].name,"Fighter 10"));
    Port_RankedRefresh(0);assert(capturedCommands.back().op=="ranked_top");
    assert(nextRankedTop>=600000&&capturedCommands.back().args["ruleset"]=="solo");
    assert(Port_RankedTopTen(top)==10); // A refresh does not clear the visible ranking.
    emit(4,{{"event","reply"},{"serial",rankedTopRequest},{"body",{{"error","SIGNALING_UNAVAILABLE"}}}});
    Port_RankedView(&v);assert(v.count==5&&Port_RankedTopTen(top)==10);
    rankedTopRequest=3;
    emit(4,{{"event","reply"},{"serial",3u},{"body",{{"entries",Dj::array()}}}});
    Port_RankedView(&v);assert(v.count==0&&v.pages==1);
    char nickname[20];
    Port_OnlineNicknameBegin();assert(Port_OnlineNicknameView(nickname)==BT4_NICKNAME_LOADING);
    emit(4,{{"event","reply"},{"serial",nicknameRequest},{"body",{{"state","required"}}}});
    assert(Port_OnlineNicknameView(nickname)==BT4_NICKNAME_REQUIRED);
    Port_OnlineNicknameSubmit("Nova");
    emit(4,{{"event","reply"},{"serial",nicknameRequest},{"body",{{"error","NICKNAME_TAKEN"}}}});
    assert(Port_OnlineNicknameView(nickname)==BT4_NICKNAME_TAKEN);
    Port_OnlineNicknameSubmit("Blue Phoenix");
    emit(4,{{"event","reply"},{"serial",nicknameRequest},{"body",{{"state","ready"},{"name","Blue Phoenix"}}}});
    assert(Port_OnlineNicknameView(nickname)==BT4_NICKNAME_READY&&!strcmp(nickname,"Blue Phoenix"));
    assert(!Bt4Signal_Command(0,50,"ranked_top","{}"));
    assert(!Bt4Signal_Command(4,51,"create","{}"));
    Bt4Signal_Listen(4,0);
    capturedCommands.clear();

    Bt4Signal_Listen(0,1);d.on=true;d.init=77;d.descriptor=Dj();d.waiting=false;
    Dj oldRoom={{"generation","11111111-1111-4111-8111-111111111111"},{"code","OLD123"},
                {"self",1u},{"mode","duel"},{"members",Dj::array({{{"peer",1u},{"name","Host"},{"role","player"}}})}};
    emit(0,{{"event","bt4.room"},{"body",oldRoom}});pump();
    assert(!d.descriptor.is_object()); // old notification cannot win the create/join race
    Dj admitted=oldRoom;admitted["generation"]="22222222-2222-4222-8222-222222222222";admitted["code"]="NEW123";
    emit(0,{{"event","reply"},{"serial",77u},{"body",admitted}});pump();
    assert(text(d.descriptor,"code")=="NEW123");
    emit(0,{{"event","bt4.room"},{"body",oldRoom}});pump();
    assert(text(d.descriptor,"code")=="NEW123"); // a late old generation also stays excluded
    d.descriptor=Dj();d.waiting=true;
    emit(0,{{"event","bt4.room"},{"body",admitted}});pump();
    assert(text(d.descriptor,"code")=="NEW123"); // queued pairing notification remains supported

    d.ranked=true;rankedActive=true;d.sawFight=false;d.reportSerial=0;d.reportState=0;
    d.profile={{"ruleset","solo"}};d.content="0123456789abcdef";
    Port_RankedObserve(3,0,100,100,0,-1);
    Port_RankedObserve(4,1,101,80,0,-1);
    Port_RankedObserve(4,1,120,120,0,-1);assert(capturedCommands.empty());
    Port_RankedObserve(4,1,122,100,0,-1);assert(capturedCommands.empty());
    Port_RankedObserve(4,1,122,122,1,-1);assert(capturedCommands.empty());
    Port_RankedObserve(3,0,123,123,0,-1); // rollback takes back a tentative finish
    Port_RankedObserve(4,1,130,130,0,-1);Port_RankedObserve(4,1,150,150,0,-1);
    assert(capturedCommands.size()==1&&capturedCommands[0].op=="ranked_report");
    assert(capturedCommands[0].args["outcome"]==0&&text(capturedCommands[0].args,"digest").size()==64);
    std::string firstDigest=text(capturedCommands[0].args,"digest");
    Port_RankedObserve(4,1,151,151,0,-1);assert(capturedCommands.size()==1); // no duplicate in-flight request
    emit(0,{{"event","reply"},{"serial",d.reportSerial},{"body",{{"state","waiting"}}}});
    Port_RankedObserve(4,1,152,152,0,-1);assert(!d.reportPending);
    d.reportAt=0;Port_RankedObserve(4,1,152,152,0,-1);assert(capturedCommands.size()==2);
    assert(text(capturedCommands[1].args,"digest")==firstDigest);
    emit(0,{{"event","reply"},{"serial",d.reportSerial},{"body",{{"state","applied"}}}});
    Port_RankedObserve(3,0,160,160,0,-1);assert(capturedCommands.size()==2&&d.reportState==2);
    // Presentation/rollback catch-up can first observe the same finish later.
    // Its agreement digest must still identify the same result.
    d.reportSerial=0;d.reportState=0;d.reportPending=false;d.outcomeBits=0;d.sawFight=true;
    Port_RankedObserve(4,1,200,200,0,-1);Port_RankedObserve(4,1,225,225,0,-1);
    assert(capturedCommands.size()==3&&text(capturedCommands.back().args,"digest")==firstDigest);
    // Liveness is HTTP-only and continues after the room is left. A local
    // concession names only our own match; remote loss keeps proving presence.
    Bt4Signal_Listen(5,1);
    capturedCommands.clear();rankedWatches.clear();d.reportSerial=0;d.sawFight=false;
    Port_RankedDisconnected(1);assert(rankedWatches.empty());
    Port_RankedObserve(3,0,300,300,0,-1);assert(rankedWatches.size()==1);
    const Uint64 now=SDL_GetTicks()+1000;
    watchStep(now);assert(capturedCommands.size()==1&&capturedCommands.back().op=="ranked_heartbeat");
    assert(text(capturedCommands.back().args,"match")==text(d.descriptor,"generation"));
    watchStep(now+1);assert(capturedCommands.size()==1); // no burst behind an in-flight request
    Port_RankedObserve(6,8,310,310,0,0);
    Port_RankedObserve(6,8,312,312,0,0);
    assert(!rankedWatches[0].concede&&rankedWatches[0].peerGone&&rankedWatches[0].until);
    Port_RankedDisconnected(0);assert(rankedWatches[0].peerGone); // the shared teardown cannot change the cause
    watchStep(now+2);assert(capturedCommands.size()==1);
    auto reply=[](const Dj &body){emit(5,{{"event","reply"},{"serial",rankedWatches[0].request},{"body",body}});};
    reply({{"state","applied"},{"rating",1016},{"delta",16}});
    watchStep(now+3);assert(rankedWatches.empty());
    int rating=0,delta=0;
    assert(Port_RankedTakeSettlement(&rating,&delta)&&rating==1016&&delta==16);
    assert(!Port_RankedTakeSettlement(&rating,&delta));

    // An acknowledged heartbeat must not swallow the player's own surrender.
    capturedCommands.clear();d.sawFight=false;nextSettlement=0;
    Port_RankedObserve(3,0,320,320,0,-1);watchStep(now+4);
    Port_RankedObserve(6,8,321,320,0,1);
    Port_RankedObserve(6,8,323,320,0,1);assert(!rankedWatches[0].concede); // rollback can still take it back
    Port_RankedObserve(6,8,323,323,0,1);watchStep(now+5);
    assert(rankedWatches[0].concede&&capturedCommands.size()==1);
    reply({{"state","waiting"}});watchStep(now+6);
    assert(capturedCommands.size()==2&&capturedCommands.back().op=="ranked_forfeit");
    assert(text(capturedCommands.back().args,"reason")=="local_quit");
    reply({{"state","already_applied"},{"rating",984},{"delta",-16}});
    watchStep(now+7);assert(rankedWatches.empty());
    assert(Port_RankedTakeSettlement(&rating,&delta)&&rating==984&&delta==-16);

    // The result can arrive before the peer's goodbye reaches the gameplay socket.
    d.sawFight=false;nextSettlement=0;
    Port_RankedObserve(3,0,330,330,0,-1);watchStep(now+8);
    reply({{"state","already_applied"},{"rating",1032},{"delta",16}});
    watchStep(now+9);assert(rankedWatches.size()==1&&!Port_RankedTakeSettlement(&rating,&delta));
    Port_RankedDisconnected(1);watchStep(now+10);
    assert(rankedWatches.empty()&&Port_RankedTakeSettlement(&rating,&delta)&&rating==1032&&delta==16);
    assert(!Bt4Signal_Command(0,52,"ranked_forfeit","{}"));
    assert(!Bt4Signal_Command(5,53,"create","{}"));
    Port_RankedShutdown();Bt4Signal_Listen(5,0);
    rankedActive=false;
    Bt4Signal_Listen(0,0);
    assert(!state.thread&&state.jobs.empty());
    SDL_Quit();
    std::cout<<"PASS native Ranked pagination, stale-room recovery, confirmed results and abandonments\n";
}
