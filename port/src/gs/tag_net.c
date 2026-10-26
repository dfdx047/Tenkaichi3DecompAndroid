/* Native Tag Team rooms. Up to ten input producers, up to ten read-only observers.
 * Reliable framed messages carry inputs, never another machine's raw memory.
 * Players predict up to eight blanks; observers consume confirmed inputs only.
 * Socket queues and camera choices are never restored by game rollback. */
#include "tag_net.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET tn_socket;
#define TN_BAD INVALID_SOCKET
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
typedef int tn_socket;
#define TN_BAD (-1)
#define closesocket close
#endif
#include "../../include/plat_game.h"

extern void Gs_HoldFrame(void);
extern int Port_RollCan(void),Port_RollSave(void),Port_PadRead(int,unsigned char *);
extern void Port_RollBack(int),Port_UiNotice(const char *);
extern int Port_TagOption(int);
extern unsigned long long Port_NetDigest(void);
extern int gPortResim;

enum { PROTOCOL=5, MAGIC=0x544e4554, PAD=18, RING=8192, MAX_PACKET=4096, QUEUE=262144,
       TN_HELLO=1, TN_CONFIG, TN_READY, TN_START, TN_LOADED, TN_GO, TN_INPUT, TN_DIGEST, TN_LEAVE, TN_REJECT, TN_EDIT, TN_FINISHED, TN_ROOM, TN_PING, TN_PEERS, TN_ROLE, TN_NOTICE, TN_PRACTICE_END,
       TN_HISTORY,TN_REPLAY_PROGRESS,TN_MEMBER,TN_PRACTICE_EDIT,TN_PRACTICE_LOADOUT,TN_BRACKET,TN_PONG,TN_ROSTER,TN_SCORES,
       PEERS=TN_PLAYERS+TN_SPECTATORS-1, DELAY=2, ROLLBACK=8 };
typedef struct Peer {
    tn_socket fd;
    int role,hello,loaded,connecting,finished;
    unsigned char rx[MAX_PACKET+8],tx[QUEUE];
    unsigned rn,tn,off;
    Uint64 last;
    uint64_t hash[RING];
    unsigned hashStamp[RING];
    unsigned replayCursor,replayAck;
    int catching,ping;
    unsigned pingToken;Uint64 pingSent;
} Peer;
static Peer peers[PEERS];
static tn_socket listener=TN_BAD;
static SDL_SpinLock uiLock;
static TagNetStatus status;
static TagNetConfig config,pendingConfig;
static int role=-1,host,phase,ready[TN_PLAYERS],pendingStart,pendingRole,pendingPort,pendingEdit,pendingReady=-1,pendingLeave,pendingSwitch=-1,pendingFighter=-1;
static char pendingAddress[128];
static int watchActor,freeCam,resim,loaded,go,roomInitialized,returned;
static unsigned baseBlank,frame,live,checked,have[TN_PLAYERS],usedStamp[RING],inputStamp[TN_PLAYERS][RING],sentDigest;
static int begun;
static unsigned nextSend;
static unsigned char inputs[TN_PLAYERS][RING][PAD],used[TN_PLAYERS][RING][PAD];
static uint64_t digest[RING];
static unsigned digestStamp[RING];
static uint64_t identity,dataIdentity;
static unsigned roundId;
static int roomSpectators,roomMask,lastPeerState=-1;
static Uint64 lastPing;
static char message[192];
static int partyEnabled=-1,pendingMode=-1,chosenMode=-1,pendingPractice;
static int hostMenu,chosenPreset,pendingPreset;
static Uint64 coordinatorClock,coordinatorLoadStart,lastRoster;
static unsigned coordinatorLogged;
static int rosterMask,rosterPing[TN_PLAYERS];
static void finish_room(void);
static void coordinator(void);
static void send_packet(Peer *,int,const void *,unsigned);
static unsigned practiceEnd;
static TlRoom partyRoom;
static int practiceMenu,practiceChoice;
static unsigned practiceButtons;
typedef struct PracticeInput {unsigned char pads[TN_PLAYERS][PAD];} PracticeInput;
typedef TagPracticeEvent PracticeEvent;
static PracticeInput *practiceHistory;
static unsigned historyCount,historyCapacity,inputStart[TN_PLAYERS],inputEnd[TN_PLAYERS];
static unsigned disconnectedMask;
static PracticeEvent *practiceEvents;
static unsigned eventCount,eventCapacity;
static int lateJoining;
static unsigned replayTarget,replayReported,localJoinAt,localGeneration;
static int pendingPracticeEdit,pendingLoadout;
static int pendingKick=-1;
static TlTournament tournament;
static int pendingSkip,battleWinner=-1;
static TlLoadout editedLoadout;
static void failed(const char *s);
static unsigned player_mask(void);
static int practice_state(TlRoom *out,unsigned at);
static const unsigned char idle[PAD]={255,255,128,128,128,128};

static int enabled=-1,nativeMenu;
int TagNet_Enabled(void) { if(enabled<0)enabled=getenv("BT3_TAG_ONLINE")!=NULL;return enabled; }
void TagNet_Enable(int on) { enabled=on!=0; }
void TagNet_Party(int on) {partyEnabled=on!=0;}
int TagNet_IsParty(void) {if(partyEnabled<0)partyEnabled=getenv("BT3_TAG_PARTY")!=NULL;return partyEnabled;}
int TagNet_IsPractice(void) {return TagNet_IsParty() && config.practice && phase>=3 && phase<=4;}
int TagNet_Observer(void) {
    TagNetStatus s;int i;
    TagNet_Status(&s);
    if(s.role==TN_OBSERVER)return 1;
    if(!s.config.party || s.config.practice)return 0;
    for(i=0;i<s.config.size[0]+s.config.size[1];i++)if(s.config.owner[i]==s.role)return 0;
    return 1;
}
void TagNet_ChooseMode(int mode) {TagNet_ChoosePreset(mode,0);}
void TagNet_ChoosePreset(int mode,int preset) {
    if(mode<0 || mode>TL_TOURNAMENT || preset<0 || preset>127)return;
    SDL_LockSpinlock(&uiLock);pendingMode=mode;pendingPreset=preset;SDL_UnlockSpinlock(&uiLock);
}
int TagNet_HostMenu(void) {return hostMenu;}
int TagNet_HostChoosing(void) {return hostMenu && practiceEnd!=0;}
void TagNet_Roster(unsigned address) {
    TagLobbyRoster r;int i;memset(&r,0,sizeof(r));r.role=role;r.host=host;r.waiting=practiceEnd!=0;
    for(i=0;i<TN_PLAYERS;i++)if((host?partyRoom.members[i].present:rosterMask&(1<<i))) {
        r.member[r.count]=i;r.ping[r.count]=rosterPing[i];r.count++;
    }
    memcpy((void *)(uintptr_t)address,&r,sizeof(r));
}
void TagNet_ReturnPractice(void) {SDL_LockSpinlock(&uiLock);pendingPractice=1;SDL_UnlockSpinlock(&uiLock);}
int TagNet_PracticeEnding(void) {return TagNet_IsPractice() && practiceEnd && frame>=practiceEnd;}
void TagNet_PracticeUi(unsigned address) {
    TagPracticeUi ui;memset(&ui,0,sizeof(ui));
    ui.visible=TagNet_IsPractice() && !host && practiceMenu;ui.host=host;ui.choice=practiceChoice;ui.role=role;
    practice_state(&ui.lobby,frame);
    {int i;for(i=0;i<TL_MEMBERS;i++){ui.lobby.members[i].wins=partyRoom.members[i].wins;ui.lobby.members[i].losses=partyRoom.members[i].losses;ui.lobby.members[i].draws=partyRoom.members[i].draws;}}
    memcpy((void *)(uintptr_t)address,&ui,sizeof(ui));
}
static int practice_state(TlRoom *out,unsigned at) {
    unsigned i;*out=config.lobby;
    for(i=0;i<eventCount && practiceEvents[i].at<=at;i++) {
        PracticeEvent *e=&practiceEvents[i];TlMember *m=&out->members[e->member];
        if(e->kind==0){if(e->generation!=m->generation)m->wins=m->losses=m->draws=0;m->present=e->present;m->generation=e->generation;m->loadout=e->fighter;}
        else if(e->kind==2)m->loadout=e->fighter;
    }
    return 1;
}
void TagNet_PracticeState(unsigned address) {
    TlRoom value;practice_state(&value,frame);memcpy((void *)(uintptr_t)address,&value,sizeof(value));
}
void TagNet_PracticeScores(unsigned address) {
    const TlPractice *w=(const TlPractice *)(uintptr_t)address;int i;
    if(!TagNet_IsPractice())return;
    for(i=0;i<TL_MEMBERS;i++){partyRoom.members[i].wins=w->wins[i];partyRoom.members[i].losses=w->losses[i];partyRoom.members[i].draws=w->draws[i];}
    /* The host has no practice world. Its scoreboard is supplied by the
       earliest present guest, while all guests still simulate and compare it. */
    if(!host && !resim && frame%60==0) {
        int authority=1;unsigned b[2+3*TL_MEMBERS];
        for(i=1;i<role;i++)if(TagNet_MemberPresent(i))authority=0;
        if(authority){b[0]=htonl(roundId);b[1]=htonl(frame);
            for(i=0;i<TL_MEMBERS;i++){b[2+i*3]=htonl(w->wins[i]);b[3+i*3]=htonl(w->losses[i]);b[4+i*3]=htonl(w->draws[i]);}
            send_packet(&peers[0],TN_SCORES,b,sizeof(b));}
    }
}
int TagNet_PracticeEvent(unsigned index,unsigned address) {
    if(index>=eventCount || practiceEvents[index].at>frame)return 0;
    memcpy((void *)(uintptr_t)address,&practiceEvents[index],sizeof(PracticeEvent));return 1;
}
int TagNet_MemberPresent(int member) {
    TlRoom r;if(!config.party || member<0 || member>=TL_MEMBERS)return 1;
    practice_state(&r,frame);return r.members[member].present;
}
void TagNet_Kick(int member) {if(member>0 && member<TL_MEMBERS){SDL_LockSpinlock(&uiLock);pendingKick=member;SDL_UnlockSpinlock(&uiLock);}}
void TagNet_Tournament(unsigned address) {memcpy((void *)(uintptr_t)address,&tournament,sizeof(tournament));}
void TagNet_SkipTournament(void) {pendingSkip=1;}
void TagNet_BattleResult(int side) {battleWinner=side;}
void TagNet_PracticeLoadout(int member,unsigned address) {
    if(member!=role || !TagNet_IsPractice())return;
    memcpy(&editedLoadout,(void *)(uintptr_t)address,sizeof(editedLoadout));pendingLoadout=1;
}
/* A persistent room keeps its ten input streams, including guests watching a
   CPU match. This also supplies a clock when no human has claimed a fighter. */
static unsigned input_mask(void) {return config.party?(1u<<TN_PLAYERS)-1:player_mask();}
static int produces(int member,unsigned at) {
    if(member<0 || member>=TN_PLAYERS)return 0;
    return !config.party || (at>=inputStart[member] && at<inputEnd[member]);
}
static int local_produces(unsigned at) {
    return produces(role,at) && (!lateJoining || at>=localJoinAt);
}
static int event_add(const PracticeEvent *e) {
    unsigned n=eventCount;PracticeEvent *next;
    if(e->member<1 || e->member>=TL_MEMBERS || e->kind<0 || e->kind>2 || e->present<0 || e->present>1 ||
       !PortGame_IsCharacter(e->fighter.chara) || e->fighter.costume<0 || e->fighter.costume>=PortGame_Costumes(e->fighter.chara) ||
       (n && e->at<practiceEvents[n-1].at))return 0;
    {int k;for(k=0;k<8;k++)if(e->fighter.items[k]<0 || e->fighter.items[k]>350)return 0;}
    if(n==eventCapacity) {
        unsigned size=eventCapacity?eventCapacity*2:64;
        if(size>65536 || !(next=realloc(practiceEvents,size*sizeof(*next)))){failed("The lobby event history is full. Reopen the lobby.");return 0;}
        practiceEvents=next;eventCapacity=size;
    }
    practiceEvents[eventCount++]=*e;
    if(e->kind==0){if(e->present){inputStart[e->member]=e->at;inputEnd[e->member]=~0u;}else inputEnd[e->member]=e->at;}
    if(e->kind==0 && e->present && e->member==role && (unsigned)e->generation==localGeneration)localJoinAt=e->at;
    return 1;
}
static void practice_ui_input(void) {
    unsigned char pad[PAD];unsigned buttons,pressed;int rows=host?6:3;
    if(!TagNet_IsPractice())return;
    memcpy(pad,idle,PAD);Port_PadRead(0,pad);buttons=(~(pad[0]|pad[1]<<8))&65535;
    if(pad[4]<64)buttons|=0x80;if(pad[4]>192)buttons|=0x20;
    if(pad[5]<64)buttons|=0x10;if(pad[5]>192)buttons|=0x40;
    pressed=buttons&~practiceButtons;practiceButtons=buttons;
    {extern int TagLobby_IsSelecting(int);if(TagLobby_IsSelecting(role)){practiceMenu=0;return;}}
    if(!host && (pressed&8)){practiceMenu=!practiceMenu;practiceChoice=0;}
    if(!host && (pressed&0x1000))practiceMenu=0;
    if(host || practiceMenu) {
        if(host && (pressed&0xA0)) {
            int i;if(practiceChoice>=100)practiceChoice=0;
            else for(i=1;i<TL_MEMBERS;i++)if(partyRoom.members[i].present){practiceChoice=100+i;break;}
        }
        if(host && practiceChoice>=100) {
            int id=practiceChoice-100,k;
            if(pressed&0x50)for(k=0;k<TL_MEMBERS;k++) {id=(id+(pressed&0x10?TL_MEMBERS-1:1))%TL_MEMBERS;if(id && partyRoom.members[id].present){practiceChoice=100+id;break;}}
            if(pressed&0x4000){TagNet_Kick(practiceChoice-100);practiceChoice=0;}
            return;
        }
        if(pressed&0x10)practiceChoice=(practiceChoice+rows-1)%rows;
        if(pressed&0x40)practiceChoice=(practiceChoice+1)%rows;
        if(pressed&0x4000) {
            if(host && practiceChoice<5)TagNet_ChooseMode(practiceChoice);
            else if(practiceChoice==rows-1)TagNet_Leave();
            else if(!host && practiceChoice==0)practiceMenu=0;
            else if(!host && practiceChoice==1){pendingPracticeEdit=1;practiceMenu=0;}
        }
    }
}
/* Both the room and the host's mode picker use native game input. */
int TagNet_NativeMenu(void) { return nativeMenu || hostMenu; }
int TagNet_Active(void) {
    int active;SDL_LockSpinlock(&uiLock);active=status.phase>=3 && status.phase<=4;SDL_UnlockSpinlock(&uiLock);
    return TagNet_Enabled() && !nativeMenu && !hostMenu && active;
}
int TagNet_Role(void) {
    int value;SDL_LockSpinlock(&uiLock);value=status.role;SDL_UnlockSpinlock(&uiLock);return value;
}
int TagNet_MenuInput(void) {
    int value;SDL_LockSpinlock(&uiLock);value=status.phase!=4 || hostMenu;SDL_UnlockSpinlock(&uiLock);
    return TagNet_Enabled() && value;
}
int TagNet_TestCase(void) {const char *s=getenv("BT3_TAG_NET_CASE");return s?atoi(s):0;}
static int test_practice_round(void) {
    const char *s=getenv("BT3_TAG_TEST_ROUNDS");int rounds=s?atoi(s):1;
    return rounds>0 && rounds<=20 && (roundId&1) && roundId/2<(unsigned)rounds;
}
int TagNet_TestInputs(void) {return getenv("BT3_TAG_TEST_INPUT")!=NULL;}
void TagNet_TestCheck(int passed,int id) {
    fprintf(stderr,"tagnet test: %s %d\n",passed?"PASS":"FAIL",id);
    if(!passed){phase=5;go=0;resim=0;gPortResim=0;}
}
/* A joining GPU client must replay faster than real time. Rendering/pacing
   archived practice would leave it permanently behind an active host. */
int TagNet_Resim(void) { return resim || (lateJoining && TagNet_IsPractice() && !local_produces(frame) && frame+60<replayTarget); }
int TagNet_Stopped(void) { return TagNet_Enabled() && phase==5; }
static void say(const char *s) { snprintf(message,sizeof(message),"%s",s);fprintf(stderr,"tagnet: %s\n",s); }
static void close_peer(Peer *p) { if(p->fd!=TN_BAD)closesocket(p->fd);memset(p,0,sizeof(*p));p->fd=TN_BAD;p->ping=-1; }
static int blocked(void) {
#ifdef _WIN32
    return WSAGetLastError()==WSAEWOULDBLOCK || WSAGetLastError()==WSAEINPROGRESS;
#else
    return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINPROGRESS;
#endif
}
static void nonblock(tn_socket fd) {
    int on=1;
#ifdef _WIN32
    u_long one=1;ioctlsocket(fd,FIONBIO,&one);
#else
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK);
#endif
    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,(const char *)&on,sizeof(on));
}
static uint32_t get32(const void *p) { uint32_t v;memcpy(&v,p,4);return ntohl(v); }
static void put32(void *p,uint32_t v) { v=htonl(v);memcpy(p,&v,4); }
static unsigned player_mask(void) {
    unsigned mask=0;int i;
    if(config.party && config.practice){for(i=0;i<TN_PLAYERS;i++)if(config.lobby.members[i].present)mask|=1u<<i;return mask;}
    for(i=0;i<config.size[0]+config.size[1];i++)if(config.owner[i]>=0 && config.owner[i]<TN_PLAYERS)mask|=1u<<config.owner[i];
    return mask;
}
static int popcount(unsigned mask) {int n=0;for(;mask;mask>>=1)n+=mask&1;return n;}
static int spectators(void) {
    int i,n=role==TN_OBSERVER;
    if(!host)return roomSpectators;
    if(config.party) {
        unsigned mask=player_mask();n=0;
        for(i=0;i<TL_MEMBERS;i++)if(partyRoom.members[i].present && !(mask&(1u<<i)))n++;
        return n;
    }
    for(i=0;i<PEERS;i++)if(peers[i].hello && peers[i].role==TN_OBSERVER)n++;
    return n;
}
static void publish(void) {
    unsigned mask=host?player_mask():(unsigned)roomMask;
    SDL_LockSpinlock(&uiLock);
    status.phase=phase;status.role=role;status.host=host;status.connected=popcount(mask);status.playerMask=mask;
    memcpy(status.ready,ready,sizeof(ready));status.spectators=spectators();
    status.config=config;status.frame=frame;status.view=watchActor;status.freeCam=freeCam;
    snprintf(status.message,sizeof(status.message),"%s",message);
    SDL_UnlockSpinlock(&uiLock);
}
void TagNet_Status(TagNetStatus *out) { SDL_LockSpinlock(&uiLock);*out=status;SDL_UnlockSpinlock(&uiLock); }
void TagNet_Start(int r,const char *address,int port) {
    SDL_LockSpinlock(&uiLock);pendingRole=r;pendingPort=port;
    snprintf(pendingAddress,sizeof(pendingAddress),"%s",address);pendingStart=1;SDL_UnlockSpinlock(&uiLock);
}
void TagNet_Edit(const TagNetConfig *c) { SDL_LockSpinlock(&uiLock);pendingConfig=*c;pendingEdit=1;SDL_UnlockSpinlock(&uiLock); }
void TagNet_Ready(int r) { SDL_LockSpinlock(&uiLock);pendingReady=r!=0;SDL_UnlockSpinlock(&uiLock); }
void TagNet_SwitchRole(int playing,int fighter) {
    SDL_LockSpinlock(&uiLock);pendingSwitch=playing!=0;pendingFighter=fighter;SDL_UnlockSpinlock(&uiLock);
}
void TagNet_Leave(void) { SDL_LockSpinlock(&uiLock);pendingLeave=1;SDL_UnlockSpinlock(&uiLock); }
void TagNet_Watch(int a,int f) { SDL_LockSpinlock(&uiLock);watchActor=a<0?0:a>9?9:a;freeCam=f!=0;SDL_UnlockSpinlock(&uiLock); }
int TagNet_View(void) {
    int a;static unsigned previous;unsigned buttons;unsigned char pad[PAD];
    if(TagNet_Observer()) {
        memcpy(pad,idle,PAD);Port_PadRead(0,pad);buttons=(~(pad[0]|pad[1]<<8))&0xffff;
        SDL_LockSpinlock(&uiLock);
        if((buttons&~previous)&0x100)watchActor=(watchActor+config.size[0]+config.size[1]-1)%(config.size[0]+config.size[1]);
        if((buttons&~previous)&0x200)watchActor=(watchActor+1)%(config.size[0]+config.size[1]);
        if((buttons&~previous)&1)freeCam=!freeCam;
        previous=buttons;SDL_UnlockSpinlock(&uiLock);
    }
    SDL_LockSpinlock(&uiLock);a=watchActor;SDL_UnlockSpinlock(&uiLock);
    if(TagNet_Observer() && getenv("BT3_TAG_NET_WATCH_TEST"))a=(frame/60)%(config.size[0]+config.size[1]);
    return a;
}
int TagNet_Option(int id,int fallback) {
    int value=fallback;
    /* Renderer and game thread both read these. Publish one immutable rules
       snapshot rather than let the renderer race a room edit or teardown. */
    SDL_LockSpinlock(&uiLock);
    if(!hostMenu && status.phase>=3 && status.phase<=4 && id>=0 && id<TN_OPTIONS && !TagOption_IsPresentation(id))value=status.config.options[id];
    SDL_UnlockSpinlock(&uiLock);return value;
}
void TagNet_CopyConfig(unsigned addr) { memcpy((void *)(uintptr_t)addr,&config,sizeof(config)); }
void TagNet_Audit(int actor,unsigned addr,unsigned size) {
    unsigned i,*p=(unsigned *)(uintptr_t)addr;
    if(!getenv("BT3_TAG_NET_AUDIT"))return;
    fprintf(stderr,"audit frame=%u actor=%d:",frame,actor);
    for(i=0;i<size/4;i++)fprintf(stderr," %08x",p[i]);fputc('\n',stderr);
}

static uint64_t build_identity(void) {
    char path[1200];unsigned char buf[65536];FILE *f;size_t n,i;uint64_t h=14695981039346656037ull;
#ifdef _WIN32
    GetModuleFileNameA(NULL,path,sizeof(path));
#else
    int len=(int)readlink("/proc/self/exe",path,sizeof(path)-1);if(len<0)return 0;path[len]=0;
#endif
    f=fopen(path,"rb");if(!f)return 0;
    while((n=fread(buf,1,sizeof(buf),f)))for(i=0;i<n;i++)h=(h^buf[i])*1099511628211ull;
    fclose(f);return h;
}
static int valid(const TagNetConfig *c) {
    int i,n,owners[TN_PLAYERS]={0};
    if(c->disc!=PortGame_IsBT4() || c->revision<=0 || c->size[0]<1 || c->size[0]>5 || c->size[1]<1 || c->size[1]>5 ||
       c->stage<0 || c->stage>=PortGame_StageCount() || c->stage==35 || c->music<0 || c->music>24 || c->time<0 || c->time>5 || c->cpu<0 || c->cpu>4)return 0;
    if(c->party<0 || c->party>1 || c->practice<0 || c->practice>1 || c->mode<0 || c->mode>TL_TOURNAMENT || c->preset<0 || c->preset>127 || c->heal<0 || c->heal>4)return 0;
    if(c->party)for(i=0;i<TL_MEMBERS;i++) {
        const TlMember *m=&c->lobby.members[i];int k;
        if(m->present<0 || m->present>1 || !PortGame_IsCharacter(m->loadout.chara) ||
           m->loadout.costume<0 || m->loadout.costume>=PortGame_Costumes(m->loadout.chara))return 0;
        for(k=0;k<8;k++)if(m->loadout.items[k]<0 || m->loadout.items[k]>350)return 0;
    }
    n=c->size[0]+c->size[1];
    if(c->party && c->mode==TL_TOURNAMENT && !c->practice) {
        int counts[4]={0};for(i=0;i<n;i++){int t=c->lobby.draft.team[i];if(t<0 || t>3 || ++counts[t]>5)return 0;}
    }
    for(i=0;i<n;i++) {
        int k;
        if(!PortGame_IsCharacter(c->chara[i]) || c->costume[i]<0 || c->costume[i]>=PortGame_Costumes(c->chara[i]) || c->owner[i]<-1 || c->owner[i]>=TN_PLAYERS)return 0;
        if(c->cpuLevel[i]<-1 || c->cpuLevel[i]>4)return 0;
        for(k=0;k<8;k++)if(c->items[i][k]<0 || c->items[i][k]>350)return 0;
        if(c->owner[i]>=0)owners[c->owner[i]]++;
    }
    for(i=0;i<TN_OPTIONS;i++)if(c->options[i]<tagOptionInfo[i].min || c->options[i]>tagOptionInfo[i].max)return 0;
    for(i=0;i<TN_PLAYERS;i++)if(owners[i]>1)return 0;
    return 1;
}
static void failed(const char *s) { if(phase==5)return;say(s);phase=5;go=0;resim=0;gPortResim=0;Port_UiNotice(s); }
static void desync(unsigned at,Peer *p) {
    fprintf(stderr,"tagnet: divergence at %u with player %d: local %016llx, remote %016llx\n",at,p->role+1,(unsigned long long)digest[at%RING],(unsigned long long)p->hash[at%RING]);
    failed("The games went out of sync. Match stopped.");
}
static void send_packet(Peer *p,int type,const void *data,unsigned size) {
    if(p->fd==TN_BAD)return;
    if(size>MAX_PACKET-8 || p->tn-p->off+size+8>QUEUE) {
        if(p->hello && (!host || p->role<TN_PLAYERS))failed("The connection could not keep up. Match stopped.");
        close_peer(p);return;
    }
    if(p->off){memmove(p->tx,p->tx+p->off,p->tn-p->off);p->tn-=p->off;p->off=0;}
    put32(p->tx+p->tn,size+8);put32(p->tx+p->tn+4,type);
    if(size)memcpy(p->tx+p->tn+8,data,size);p->tn+=size+8;
}
static void broadcast(int type,const void *data,unsigned size,Peer *except) {
    int i;for(i=0;i<PEERS;i++)if(peers[i].hello && &peers[i]!=except && !(type==TN_INPUT && peers[i].catching))send_packet(&peers[i],type,data,size);
}
static void send_event(Peer *p,const PracticeEvent *e) {
    unsigned char b[4+sizeof(*e)];unsigned i;put32(b,roundId);
    for(i=0;i<sizeof(*e)/4;i++)put32(b+4+i*4,((const unsigned *)e)[i]);
    send_packet(p,TN_MEMBER,b,sizeof(b));
}
static void practice_event(int id,int present,int kind) {
    PracticeEvent e;int i;memset(&e,0,sizeof(e));e.at=frame+120;
    if(eventCount && e.at<practiceEvents[eventCount-1].at)e.at=practiceEvents[eventCount-1].at;
    e.member=id;e.present=present;e.kind=kind;e.generation=partyRoom.members[id].generation;e.fighter=partyRoom.members[id].loadout;
    if(!event_add(&e)){failed("Could not synchronize the lobby membership.");return;}
    for(i=0;i<PEERS;i++)if(peers[i].hello && peers[i].loaded)send_event(&peers[i],&e);
}
static void member_event(int id,int present) {if(present)disconnectedMask&=~(1u<<id);practice_event(id,present,0);}
static void record_history(unsigned end) {
    unsigned t,p;PracticeInput *next;
    if(!host || !TagNet_IsPractice() || end<=historyCount)return;
    if(end>historyCapacity) {
        unsigned size=historyCapacity?historyCapacity:8192;while(size<end)size*=2;
        if(size>8388608 || !(next=realloc(practiceHistory,(size_t)size*sizeof(*next)))){failed("The practice history is full. Reopen the lobby to continue.");return;}
        practiceHistory=next;historyCapacity=size;
    }
    for(t=historyCount;t<end;t++)for(p=0;p<TN_PLAYERS;p++) {
        if(inputStamp[p][t%RING]!=t){failed("The lobby input history is incomplete.");return;}
        memcpy(practiceHistory[t].pads[p],inputs[p][t%RING],PAD);
    }
    historyCount=end;
}
static void stream_history(Peer *p) {
    unsigned char b[16+16*sizeof(PracticeInput)];unsigned n,end;
    if(!host || !p->catching || !p->loaded || !TagNet_IsPractice())return;
    end=historyCount;if(end>p->replayAck+RING/2)end=p->replayAck+RING/2;
    while(p->replayCursor<end && p->tn-p->off<32768) {
        n=end-p->replayCursor;if(n>16)n=16;
        put32(b,roundId);put32(b+4,p->replayCursor);put32(b+8,n);put32(b+12,historyCount);
        memcpy(b+16,practiceHistory+p->replayCursor,n*sizeof(PracticeInput));
        send_packet(p,TN_HISTORY,b,16+n*sizeof(PracticeInput));p->replayCursor+=n;
    }
    if(p->replayCursor==historyCount && p->replayAck+120>=historyCount) {
        unsigned t;int who;p->catching=0;
        /* Bridge the last archived blank to the live stream before allowing
           this guest to produce inputs. No raw process state crosses the wire. */
        for(who=0;who<TN_PLAYERS;who++)for(t=historyCount;t<have[who];t++) {
            unsigned char data[12+PAD];put32(data,who);put32(data+4,t);put32(data+8,roundId);memcpy(data+12,inputs[who][t%RING],PAD);send_packet(p,TN_INPUT,data,sizeof(data));
        }
        member_event(p->role,1);
        fprintf(stderr,"tagnet: guest %d caught up to practice frame %u\n",p->role+1,historyCount);
    }
}
static void send_config(Peer *p) {
    unsigned char data[sizeof(config)];unsigned i;
    for(i=0;i<sizeof(config)/4;i++)put32(data+i*4,((unsigned *)&config)[i]);
    send_packet(p,TN_CONFIG,data,sizeof(data));
}
static void send_tournament(Peer *p) {
    unsigned char b[sizeof(tournament)];unsigned i;
    for(i=0;i<sizeof(tournament)/4;i++)put32(b+i*4,((unsigned *)&tournament)[i]);
    if(p)send_packet(p,TN_BRACKET,b,sizeof(b));else broadcast(TN_BRACKET,b,sizeof(b),NULL);
}
static void ready_packet(void) {
    unsigned char data[4+4*TN_PLAYERS];int i;put32(data,config.revision);
    for(i=0;i<TN_PLAYERS;i++)put32(data+4+i*4,ready[i]);
    broadcast(TN_READY,data,sizeof(data),NULL);
}
static void apply_edit(const TagNetConfig *c) {
    int i;config=*c;config.revision++;memset(ready,0,sizeof(ready));lastPeerState=-1;
    if(config.party && !config.practice && !tournament.active)for(i=0;i<config.size[0]+config.size[1];i++)if(config.owner[i]>=0) {
        int p=config.owner[i];TlLoadout *f=&partyRoom.members[p].loadout;
        f->chara=config.chara[i];f->costume=config.costume[i];memcpy(f->items,config.items[i],sizeof(f->items));config.lobby.members[p].loadout=*f;
    }
    for(i=0;i<PEERS;i++)if(peers[i].hello)send_config(&peers[i]);ready_packet();
    if(config.party)send_tournament(NULL);
}
static void tournament_match(void) {
    TagNetConfig c=config;int team,k,n=0;c.lobby=partyRoom;
    if(tournament.active==1)for(team=0;team<2;team++) {
        int id=team?tournament.right[tournament.match]:tournament.left[tournament.match];
        c.size[team]=tournament.count[id];
        for(k=0;k<c.size[team];k++,n++) {
            TlLoadout *f=&tournament.fighters[id][k];int p=tournament.owner[id][k];
            c.chara[n]=f->chara;c.costume[n]=f->costume;memcpy(c.items[n],f->items,sizeof(f->items));
            c.owner[n]=p>=0 && partyRoom.members[p].present?p:-1;c.cpuLevel[n]=tournament.cpu[id][k];
        }
    }
    /* The room draft is not the current match's roster, including after the
       final. Describe the two actual opponents for the shared config check. */
    for(k=0;k<c.size[0]+c.size[1];k++)c.lobby.draft.team[k]=k<c.size[0]?0:1;
    c.time=0;
    if(c.preset==0)c.stage=tournament.match==6?27:4;
    if(c.preset==2)c.stage=tournament.match==6?6:28;
    apply_edit(&c);
}
static int begin_tournament(void) {
    TlDraft d;int i,k;unsigned seed=(unsigned)config.seed;
    memset(&d,0,sizeof(d));d.count=config.size[0]+config.size[1];d.difficulty=config.cpu;
    for(i=0;i<d.count;i++) {
        d.team[i]=config.lobby.draft.team[i];d.owner[i]=config.owner[i];d.cpu[i]=config.cpuLevel[i]<0?config.cpu:config.cpuLevel[i];
        d.fighters[i].chara=config.chara[i];d.fighters[i].costume=config.costume[i];memcpy(d.fighters[i].items,config.items[i],sizeof(d.fighters[i].items));
    }
    if(TlTournament_Build(&tournament,&d)!=TL_OK){say("Each tournament team needs between one and five fighters.");return 0;}
    for(i=tournament.entered;i<8;i++)for(k=0;k<tournament.count[i];k++) {
        int id;do{seed=seed*1664525u+1013904223u;id=(int)(seed%(unsigned)PortGame_CharacterCount());}while(!PortGame_IsCharacter(id));
        tournament.fighters[i][k].chara=id;tournament.fighters[i][k].costume=0;memset(tournament.fighters[i][k].items,0,sizeof(tournament.fighters[i][k].items));
    }
    tournament_match();say("Tournament drawn. Ready for the next match, or skip a CPU match.");return 1;
}
static void end_practice(int mode) {
    unsigned char b[12];
    if(!host || !TagNet_IsPractice())return;
    if(!practiceEnd)practiceEnd=frame+60;
    if(mode>=0)chosenMode=mode;
    put32(b,roundId);put32(b+4,practiceEnd);put32(b+8,chosenMode);
    broadcast(TN_PRACTICE_END,b,sizeof(b),NULL);
}
static void peer_lost(Peer *p,const char *why) {
    if(host && config.party && p->hello) {
        int old=p->role,t,k;
        close_peer(p);
        if(old>=1 && old<TL_MEMBERS)partyRoom.members[old].present=0;
        for(t=0;t<8;t++)for(k=0;k<5;k++)if(tournament.owner[t][k]==old)tournament.owner[t][k]=-1;
        if(phase==2) {
            TagNetConfig c=config;int i;memcpy(c.lobby.members,partyRoom.members,sizeof(c.lobby.members));
            for(i=0;i<TN_FIGHTERS;i++)if(c.owner[i]==old)c.owner[i]=-1;
            apply_edit(&c);say("A guest left the lobby.");return;
        }
        if(old>0 && old<TL_MEMBERS && phase>=3){disconnectedMask|=1u<<old;member_event(old,0);say(config.practice?"A guest left the practice map.":"A guest left. Their fighter is now a CPU.");return;}
        failed(why);return;
    }
    if(p->hello && (!host || p->role<TN_PLAYERS)) {
        if(host && phase==2) {
            TagNetConfig c=config;int i,old=p->role;
            close_peer(p);
            for(i=0;i<TN_FIGHTERS;i++)if(c.owner[i]==old)c.owner[i]=-1;
            apply_edit(&c);say("A player left. Their fighter is now a CPU.");return;
        }
        failed(why);
    }
    close_peer(p);
}
/* Only the host allocates input slots. Hosting remains independent of playing:
   the host may watch while other players fight. No occupied slot is stolen. */
static int switch_role(Peer *p,int playing,int fighter) {
    TagNetConfig c=config;int old=p?p->role:role,chosen=TN_OBSERVER,i,n=c.size[0]+c.size[1];
    const char *error=NULL;
    if(c.party) {
        if(tournament.active)return 0;
        chosen=old;
        if(old<0 || old>=TL_MEMBERS || !c.lobby.members[old].present || c.practice)return 0;
        if(playing) {
            if(fighter<0)for(i=0;i<n;i++)if(c.owner[i]==old){fighter=i;break;}
            if(fighter<0)for(i=0;i<n;i++)if(c.owner[i]<0){fighter=i;break;}
            if(fighter<0 || fighter>=n || (c.owner[fighter]>=0 && c.owner[fighter]!=old))return 0;
            if(c.mode==TL_GAUNTLET && fighter>=c.size[0])return 0;
        }
        for(i=0;i<n;i++)if(c.owner[i]==old)c.owner[i]=-1;
        if(playing) {
            TlLoadout *f=&c.lobby.members[old].loadout;
            c.owner[fighter]=old;c.chara[fighter]=f->chara;c.costume[fighter]=f->costume;memcpy(c.items[fighter],f->items,sizeof(f->items));
        }
        apply_edit(&c);return 1;
    }
    if(playing) {
        unsigned mask=player_mask();
        if(old<TN_PLAYERS)chosen=old;
        else for(chosen=0;chosen<TN_PLAYERS && (mask&(1u<<chosen));chosen++){}
        if(chosen==TN_PLAYERS)error="All ten player slots are occupied.";
        else {
            if(fighter<0) {
                /* Keep the two team leaders as the first defaults. */
                fighter=TagNet_TestCase()==2 && c.owner[1]<0?1:c.owner[c.size[0]]<0?c.size[0]:-1;
                for(i=0;fighter<0 && i<n;i++)if(c.owner[i]<0 || c.owner[i]==chosen)fighter=i;
            }
            if(fighter<0 && n<TN_FIGHTERS) {
                int team=c.size[1]<5?1:0,at=team?n:c.size[0];
                for(i=n;i>at;i--)TagNet_CopyFighter(&c,i,i-1);
                c.size[team]++;fighter=at;c.chara[at]=3;c.costume[at]=0;c.owner[at]=-1;c.cpuLevel[at]=-1;memset(c.items[at],0,sizeof(c.items[at]));n++;
            }
            if(fighter<0 || fighter>=n || (c.owner[fighter]>=0 && c.owner[fighter]!=chosen))error="Choose an available CPU fighter.";
        }
    } else if(old!=TN_OBSERVER && spectators()>=TN_SPECTATORS)error="All ten spectator seats are occupied.";
    if(error){if(p)send_packet(p,TN_NOTICE,error,(unsigned)strlen(error)+1);else say(error);return 0;}
    for(i=0;i<n;i++)if(c.owner[i]==old)c.owner[i]=-1;
    if(playing)c.owner[fighter]=chosen;
    if(p){unsigned char b[4];p->role=chosen;put32(b,chosen);send_packet(p,TN_ROLE,b,4);}
    else role=chosen;
    apply_edit(&c);
    fprintf(stderr,"tagnet: role %d -> %d, roster mask %03x\n",old,chosen,player_mask());
    return 1;
}
static void input_take(int who,unsigned at,const unsigned char *pad) {
    if(who<0 || who>=TN_PLAYERS || at>=have[who]+RING-64)return;
    if(at<have[who]) { if(inputStamp[who][at%RING]==at && memcmp(inputs[who][at%RING],pad,PAD))failed("A player sent conflicting inputs. Match stopped.");return; }
    memcpy(inputs[who][at%RING],pad,PAD);inputStamp[who][at%RING]=at;
    while(inputStamp[who][have[who]%RING]==have[who])have[who]++;
}
static void handle(Peer *p,int type,const unsigned char *data,unsigned n) {
    unsigned i;
    if(!p->hello && type!=TN_HELLO && !(!host && type==TN_REJECT))return;
    if(type==TN_HELLO) {
        if(p->hello)return;
        if(n!=36 || get32(data)!=MAGIC || get32(data+4)!=PROTOCOL || get32(data+8)!=(unsigned)config.disc || get32(data+32)!=(unsigned)TagNet_IsParty() ||
           get32(data+12)!=(uint32_t)(identity>>32) || get32(data+16)!=(uint32_t)identity ||
           get32(data+24)!=(uint32_t)(dataIdentity>>32) || get32(data+28)!=(uint32_t)dataIdentity) {
            const char *s="The game build or disc does not match the host.";send_packet(p,TN_REJECT,s,(unsigned)strlen(s)+1);return;
        }
        if(host) {
            int r=(int)get32(data+20);
            if(config.party && (phase==2 || phase==4)) {
                unsigned char b[8];int id;
                for(id=1;id<TL_MEMBERS && partyRoom.members[id].present;id++){}
                if(id==TL_MEMBERS){const char *s="All ten lobby seats are occupied.";send_packet(p,TN_REJECT,s,(unsigned)strlen(s)+1);return;}
                partyRoom.members[id].present=1;partyRoom.members[id].generation=++partyRoom.generation;
                memset(&partyRoom.members[id].loadout,0,sizeof(TlLoadout));
                partyRoom.members[id].wins=partyRoom.members[id].losses=partyRoom.members[id].draws=0;
                partyRoom.members[id].slot=partyRoom.members[id].team=-1;
                p->role=id;p->hello=1;send_packet(p,TN_HELLO,data,n);put32(b,id);put32(b+4,partyRoom.members[id].generation);send_packet(p,TN_ROLE,b,8);
                send_config(p);
                if(phase==2){TagNetConfig c=config;c.lobby=partyRoom;apply_edit(&c);}
                else if(config.practice) {unsigned char start[12];p->catching=1;p->loaded=0;put32(start,config.revision);put32(start+4,roundId);put32(start+8,historyCount);send_packet(p,TN_START,start,sizeof(start));}
                else {const char *s="A match is in progress. You will join the lobby when it ends.";send_packet(p,TN_NOTICE,s,(unsigned)strlen(s)+1);send_tournament(p);}
                say("A guest joined the lobby.");return;
            }
            if(phase!=2 || (r!=1 && r!=2) || (r==1 && popcount(player_mask())>=TN_PLAYERS) || (r==2 && spectators()>=TN_SPECTATORS)) {
                const char *s="This room is full or the match has already started.";send_packet(p,TN_REJECT,s,(unsigned)strlen(s)+1);return;
            }
            p->role=TN_OBSERVER;p->hello=1;send_packet(p,TN_HELLO,data,n);
            if(r==1)switch_role(p,1,-1);
            else {unsigned char b[4];put32(b,TN_OBSERVER);send_packet(p,TN_ROLE,b,4);send_config(p);ready_packet();}
            say("Connected. Choose the teams and ready up.");
        } else {p->role=TN_OBSERVER;p->hello=1;}
    } else if((type==TN_REJECT || type==TN_NOTICE) && !host) {
        char s[192];if(n>191)n=191;memcpy(s,data,n);s[n]=0;if(type==TN_REJECT)failed(s);else say(s);
    } else if(type==TN_ROLE && phase==2) {
        if(host && n==12 && get32(data)==(unsigned)config.revision && get32(data+4)<=1)switch_role(p,(int)get32(data+4),(int)get32(data+8));
        else if(!host && (n==4 || n==8) && get32(data)<=TN_OBSERVER){role=(int)get32(data);localGeneration=n==8?get32(data+4):0;TagNet_Watch(0,0);}
    } else if(type==TN_CONFIG && !host && phase==2 && n==sizeof(config)) {
        TagNetConfig c;for(i=0;i<n/4;i++)((unsigned *)&c)[i]=get32(data+i*4);
        if(!valid(&c)){failed("The room contains unsupported fighters, outfits or rules.");return;}
        config=c;memset(ready,0,sizeof(ready));say("Choose your fighter, then ready up.");
    } else if(type==TN_BRACKET && !host && config.party && phase==2 && n==sizeof(tournament)) {
        TlTournament t;int k;for(i=0;i<n/4;i++)((unsigned *)&t)[i]=get32(data+i*4);
        if(t.active<0 || t.active>2 || t.match<0 || t.match>7 || t.entered<0 || t.entered>4)return;
        if(t.active) {
            for(i=0;i<8;i++){if(t.count[i]<1 || t.count[i]>5)return;for(k=0;k<t.count[i];k++) {
                TlLoadout *f=&t.fighters[i][k];int j;if(!PortGame_IsCharacter(f->chara) || f->costume<0 || f->costume>=PortGame_Costumes(f->chara) || t.owner[i][k]<-1 || t.owner[i][k]>=TN_PLAYERS || t.cpu[i][k]<0 || t.cpu[i][k]>4)return;
                for(j=0;j<8;j++)if(f->items[j]<0 || f->items[j]>350)return;
            }}
            for(i=0;i<7;i++)if(t.left[i]<-1 || t.left[i]>7 || t.right[i]<-1 || t.right[i]>7 || t.winner[i]<-1 || t.winner[i]>7)return;
        }
        tournament=t;
    } else if(type==TN_READY && phase==2 && n==4+4*TN_PLAYERS && get32(data)==(unsigned)config.revision) {
        if(host && p->role<TN_PLAYERS){ready[p->role]=get32(data+4+4*p->role)!=0;ready_packet();}
        else if(!host)for(i=0;i<TN_PLAYERS;i++)ready[i]=get32(data+4+4*i)!=0;
    } else if(type==TN_EDIT && host && phase==2 && !tournament.active && p->role<TN_PLAYERS && n==sizeof(config)) {
        TagNetConfig request,c=config;int selected=-1,previous=-1;
        for(i=0;i<n/4;i++)((unsigned *)&request)[i]=get32(data+i*4);
        if(request.revision!=config.revision)return;
        for(i=0;i<(unsigned)(config.size[0]+config.size[1]);i++) {
            if(config.owner[i]==p->role)previous=(int)i;
            if(request.owner[i]==p->role){if(selected>=0)return;selected=(int)i;}
        }
        if(selected<0 || previous<0 || (config.owner[selected]>=0 && config.owner[selected]!=p->role))return;
        c.owner[previous]=-1;c.owner[selected]=p->role;
        c.chara[selected]=request.chara[selected];c.costume[selected]=request.costume[selected];
        if(c.party && c.mode==TL_TOURNAMENT && !tournament.active)c.lobby.draft.team[selected]=request.lobby.draft.team[selected];
        memcpy(c.items[selected],request.items[selected],sizeof(c.items[selected]));
        if(valid(&c))apply_edit(&c);
    } else if(type==TN_PRACTICE_END && !host && TagNet_IsPractice() && n==12 && get32(data)==roundId) {
        unsigned end=get32(data+4);int mode=(int)get32(data+8);
        if(end<frame || end>8388608 || mode< -1 || mode>TL_TOURNAMENT){failed("Invalid lobby transition.");return;}
        practiceEnd=end;chosenMode=mode;
    } else if(type==TN_START && !host && phase==2 && (n==8 || n==12) && get32(data)==(unsigned)config.revision) {
        roundId=get32(data+4);lateJoining=n==12;localJoinAt=lateJoining?~0u:0;replayTarget=n==12?get32(data+8):0;phase=3;say("Loading the match...");
    } else if(type==TN_LOADED && host && phase>=3 && n==4 && get32(data)==roundId) {p->loaded=1;
        if(phase==3){unsigned k;for(k=0;k<eventCount;k++)send_event(p,&practiceEvents[k]);}
        if(TagNet_IsPractice() && phase==4 && p->catching) {
            unsigned k;send_packet(p,TN_GO,data,4);for(k=0;k<eventCount;k++)send_event(p,&practiceEvents[k]);
            if(practiceEnd){unsigned char b[12];put32(b,roundId);put32(b+4,practiceEnd);put32(b+8,chosenMode);send_packet(p,TN_PRACTICE_END,b,sizeof(b));}
        }
    } else if(type==TN_HISTORY && !host && TagNet_IsPractice() && n>=16 && get32(data)==roundId) {
        unsigned start=get32(data+4),count=get32(data+8),t,j;
        if(!count || count>16 || n!=16+count*sizeof(PracticeInput) || start>frame+RING/2+16 || start+count<start){failed("Invalid practice replay packet.");return;}
        replayTarget=get32(data+12);
        for(t=0;t<count;t++)for(j=0;j<TN_PLAYERS;j++)input_take(j,start+t,data+16+t*sizeof(PracticeInput)+j*PAD);
    } else if(type==TN_REPLAY_PROGRESS && host && p->catching && n==8 && get32(data)==roundId) {
        unsigned at=get32(data+4);if(at>=p->replayAck && at<=historyCount)p->replayAck=at;
    } else if(type==TN_MEMBER && !host && config.party && phase>=3 && phase<=4 && n==4+sizeof(PracticeEvent) && get32(data)==roundId) {
        PracticeEvent e;for(i=0;i<sizeof(e)/4;i++)((unsigned *)&e)[i]=get32(data+4+i*4);
        if(!event_add(&e)){failed("Invalid practice membership event.");return;}
    } else if(type==TN_PRACTICE_EDIT && host && TagNet_IsPractice() && n==4 && get32(data)==roundId && p->role>0 && p->role<TL_MEMBERS && produces(p->role,frame)) {
        practice_event(p->role,1,1);
    } else if(type==TN_PRACTICE_LOADOUT && host && TagNet_IsPractice() && n==4+sizeof(TlLoadout) && get32(data)==roundId && p->role>0 && p->role<TL_MEMBERS && produces(p->role,frame)) {
        TlLoadout f;int k;for(k=0;k<sizeof(f)/4;k++)((unsigned *)&f)[k]=get32(data+4+k*4);
        if(!PortGame_IsCharacter(f.chara) || f.costume<0 || f.costume>=PortGame_Costumes(f.chara))return;
        for(k=0;k<8;k++)if(f.items[k]<0 || f.items[k]>350)return;
        partyRoom.members[p->role].loadout=f;practice_event(p->role,1,2);
    } else if(type==TN_GO && !host && phase==3 && n==4 && get32(data)==roundId) {go=1;
    } else if(type==TN_FINISHED && host && phase==4 && n==4 && get32(data)==roundId) {p->finished=1;
    } else if(type==TN_ROOM && !host && phase==4 && n==4 && get32(data)==roundId) {returned=1;if(config.party)phase=2;
    } else if(type==TN_INPUT && phase>=3 && phase<=4 && n==12+PAD && get32(data+8)==roundId) {
        int who=(int)get32(data);unsigned at=get32(data+4);
        if(who<0 || who>=TN_PLAYERS || !(input_mask()&(1u<<who)))return;
        if(host && (p->role>=TN_PLAYERS || who!=p->role || !produces(who,at)))return;
        if(!host && who==role && produces(who,at))return;
        input_take(who,at,data+12);
        if(host)broadcast(TN_INPUT,data,n,p);
    } else if(type==TN_DIGEST && phase==4 && n==16 && get32(data+12)==roundId) {
        unsigned at=get32(data);uint64_t h=(uint64_t)get32(data+4)<<32|get32(data+8);
        if(host && p->role>=TN_PLAYERS)return;
        if(at+RING>frame && at<frame+RING){
            p->hashStamp[at%RING]=at;p->hash[at%RING]=h;
            if(hostMenu && config.practice) {
                if(digestStamp[at%RING]==at && digest[at%RING]!=h){desync(at,p);return;}
                if(digestStamp[at%RING]!=at){digestStamp[at%RING]=at;digest[at%RING]=h;broadcast(TN_DIGEST,data,n,NULL);}
            }
        }
    } else if(type==TN_PING && n==4) {
        send_packet(p,TN_PONG,data,n);
    } else if(type==TN_PONG && n==4 && get32(data)==p->pingToken && p->pingSent) {
        unsigned elapsed=(unsigned)(SDL_GetTicks()-p->pingSent);
        if(elapsed<15000)p->ping=p->ping<0?(int)elapsed:(p->ping*3+(int)elapsed)/4;
        p->pingSent=0;
    } else if(type==TN_ROSTER && !host && n==4+4*TN_PLAYERS) {
        rosterMask=get32(data)&1023;for(i=0;i<TN_PLAYERS;i++)rosterPing[i]=(int)get32(data+4+4*i);
    } else if(type==TN_SCORES && host && hostMenu && n==8+12*TL_MEMBERS && get32(data)==roundId) {
        int authority=1,k;for(k=1;k<p->role;k++)if(partyRoom.members[k].present)authority=0;
        if(authority && get32(data+4)<=frame+ROLLBACK)for(k=0;k<TL_MEMBERS;k++) {
            partyRoom.members[k].wins=get32(data+8+k*12);partyRoom.members[k].losses=get32(data+12+k*12);partyRoom.members[k].draws=get32(data+16+k*12);
        }
    } else if(type==TN_PEERS && !host && n==8) {
        roomSpectators=(int)get32(data);roomMask=(int)get32(data+4);
    } else if(type==TN_LEAVE && !host && config.party && n==4 && get32(data)==1)peer_lost(p,"The host removed you from the lobby.");
    else if(type==TN_LEAVE && n==0)peer_lost(p,"A player left. The match has stopped.");
}
static void pump(void) {
    int i;
    if(SDL_GetTicks()-lastPing>1000){
        unsigned char b[4];lastPing=SDL_GetTicks();
        for(i=0;i<PEERS;i++)if(peers[i].hello){peers[i].pingToken++;peers[i].pingSent=lastPing;put32(b,peers[i].pingToken);send_packet(&peers[i],TN_PING,b,4);}
    }
    if(host && config.party && SDL_GetTicks()-lastRoster>1000) {
        unsigned char b[4+4*TN_PLAYERS];int mask=0;
        lastRoster=SDL_GetTicks();for(i=0;i<TN_PLAYERS;i++){if(partyRoom.members[i].present)mask|=1<<i;rosterPing[i]=i? -1:0;}
        for(i=0;i<PEERS;i++)if(peers[i].hello && peers[i].role<TN_PLAYERS)rosterPing[peers[i].role]=peers[i].ping;
        put32(b,mask);for(i=0;i<TN_PLAYERS;i++)put32(b+4+4*i,rosterPing[i]);broadcast(TN_ROSTER,b,sizeof(b),NULL);
    }
    if(listener!=TN_BAD) {
        tn_socket fd=accept(listener,NULL,NULL);
        if(fd!=TN_BAD){for(i=0;i<PEERS;i++)if(peers[i].fd==TN_BAD)break;
            if(i==PEERS)closesocket(fd);else {nonblock(fd);peers[i].fd=fd;peers[i].last=SDL_GetTicks();}}
    }
    for(i=0;i<PEERS;i++) {
        Peer *p=&peers[i];int n;
        if(p->fd==TN_BAD)continue;
        stream_history(p);
        if(p->connecting) {
            fd_set writeSet,errorSet;struct timeval wait={0,0};int error=0;
#ifdef _WIN32
            int size=sizeof(error);
#else
            socklen_t size=sizeof(error);
#endif
            FD_ZERO(&writeSet);FD_ZERO(&errorSet);FD_SET(p->fd,&writeSet);FD_SET(p->fd,&errorSet);
            n=select((int)p->fd+1,NULL,&writeSet,&errorSet,&wait);
            if(n<0 || SDL_GetTicks()-p->last>10000){failed("Could not reach the host. Check its address and port.");close_peer(p);continue;}
            if(!n)continue;
            getsockopt(p->fd,SOL_SOCKET,SO_ERROR,(char *)&error,&size);
            if(error || FD_ISSET(p->fd,&errorSet)){failed("Could not connect to the host.");close_peer(p);continue;}
            p->connecting=0;
        }
        if(p->off<p->tn) {
#ifdef _WIN32
            n=send(p->fd,(const char *)p->tx+p->off,(int)(p->tn-p->off),0);
#else
            n=send(p->fd,p->tx+p->off,p->tn-p->off,MSG_NOSIGNAL);
#endif
            if(n>0)p->off+=(unsigned)n;else if(n<0 && !blocked()){peer_lost(p,"Connection lost. The match has stopped.");continue;}
            if(p->off==p->tn)p->off=p->tn=0;
        }
        n=recv(p->fd,(char *)p->rx+p->rn,(int)(sizeof(p->rx)-p->rn),0);
        if(n==0 || (n<0 && !blocked())) {
            peer_lost(p,"Connection lost. The match has stopped.");continue;
        }
        if(n>0){p->rn+=(unsigned)n;p->last=SDL_GetTicks();}
        while(p->rn>=8 && p->fd!=TN_BAD) {
            unsigned size=get32(p->rx);if(size<8 || size>MAX_PACKET){
                if(p->hello && (!host||p->role<TN_PLAYERS))failed("Invalid network message. Match stopped.");
                close_peer(p);break;
            }
            if(p->rn<size)break;
            handle(p,(int)get32(p->rx+4),p->rx+8,size-8);
            if(p->fd==TN_BAD)break;
            memmove(p->rx,p->rx+size,p->rn-size);p->rn-=size;
        }
        if(p->fd!=TN_BAD && !p->hello && SDL_GetTicks()-p->last>10000)close_peer(p);
        if(p->fd!=TN_BAD && p->hello && SDL_GetTicks()-p->last>(phase==3?120000:15000)) {
            peer_lost(p,"Connection timed out. The match has stopped.");
        }
    }
    if(host) {
        int mask=(int)player_mask(),n=spectators(),state=(mask<<5)|n;unsigned char data[8];
        if(state!=lastPeerState){lastPeerState=state;put32(data,n);put32(data+4,mask);broadcast(TN_PEERS,data,8,NULL);}
    }
}
static void begin_socket(int r,const char *address,int port) {
    struct sockaddr_in addr;tn_socket fd;int i,on=1;
    if(r<0 || r>2 || port<1024 || port>65535)return;
    for(i=0;i<PEERS;i++)close_peer(&peers[i]);
    if(listener!=TN_BAD){closesocket(listener);listener=TN_BAD;}
    memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)port);
    fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(fd==TN_BAD){say("Could not create the room connection.");return;}
    host=r==0;role=host?0:TN_OBSERVER;memset(ready,0,sizeof(ready));phase=2;lastPeerState=-1;roomSpectators=roomMask=0;
    config.party=TagNet_IsParty();config.practice=config.party;config.mode=TL_TEAM;config.preset=0;config.heal=2;
    memset(&tournament,0,sizeof(tournament));pendingSkip=0;
    if(host){for(i=0;i<TN_FIGHTERS;i++)config.owner[i]=-1;config.owner[0]=0;config.revision++;
        if(config.party){
            memset(&partyRoom,0,sizeof(partyRoom));partyRoom.revision=partyRoom.generation=1;partyRoom.members[0].present=1;
            for(i=0;i<TL_MEMBERS;i++){partyRoom.members[i].loadout.chara=0;partyRoom.members[i].generation=1;}
            config.lobby=partyRoom;
        }
    }
    if(r==0) {
        addr.sin_addr.s_addr=htonl(INADDR_ANY);
        setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char *)&on,sizeof(on));
        if(bind(fd,(struct sockaddr *)&addr,sizeof(addr)) || listen(fd,32)){closesocket(fd);phase=1;say("Could not open that port. Try a different port.");return;}
        listener=fd;nonblock(fd);say("Room open. Invite players or spectators.");
        if(getenv("BT3_TAG_TEST_HOST_SPECTATOR"))TagNet_SwitchRole(0,-1);
    } else {
        /* Numeric addresses keep connection setup bounded; DNS/UI can be
           added separately without blocking the simulation thread. */
        if(inet_pton(AF_INET,address,&addr.sin_addr)!=1){closesocket(fd);phase=1;say("Enter the host's IPv4 address.");return;}
        nonblock(fd);
        i=connect(fd,(struct sockaddr *)&addr,sizeof(addr));
        if(i && !blocked()){closesocket(fd);phase=1;say("Could not connect to the host.");return;}
        peers[0].fd=fd;peers[0].last=SDL_GetTicks();peers[0].connecting=i!=0;
        {unsigned char h[36];put32(h,MAGIC);put32(h+4,PROTOCOL);put32(h+8,config.disc);put32(h+12,(unsigned)(identity>>32));put32(h+16,(unsigned)identity);put32(h+20,r);put32(h+24,(unsigned)(dataIdentity>>32));put32(h+28,(unsigned)dataIdentity);put32(h+32,TagNet_IsParty());send_packet(&peers[0],TN_HELLO,h,36);}
        say("Connecting to the host...");
    }
}
static void commands(void) {
    int start,edit,r,port,rd,leave,sw,fighter,mode,practice;char address[128];TagNetConfig c;
    SDL_LockSpinlock(&uiLock);start=pendingStart;pendingStart=0;r=pendingRole;port=pendingPort;
    memcpy(address,pendingAddress,sizeof(address));edit=pendingEdit;pendingEdit=0;c=pendingConfig;
    rd=pendingReady;pendingReady=-1;leave=pendingLeave;pendingLeave=0;
    sw=pendingSwitch;pendingSwitch=-1;fighter=pendingFighter;mode=pendingMode;if(mode>=0)chosenPreset=pendingPreset;pendingMode=-1;practice=pendingPractice;pendingPractice=0;SDL_UnlockSpinlock(&uiLock);
    if(mode>=0 && host && TagNet_IsPractice())end_practice(mode);
    if(pendingKick>0) {
        int k,id=pendingKick;pendingKick=-1;
        if(host && config.party)for(k=0;k<PEERS;k++)if(peers[k].hello && peers[k].role==id){unsigned char reason[4];put32(reason,1);send_packet(&peers[k],TN_LEAVE,reason,4);pump();if(peers[k].hello && peers[k].role==id)peer_lost(&peers[k],"Removed from the lobby.");break;}
    }
    if(pendingPracticeEdit) {unsigned char b[4];put32(b,roundId);if(!host && TagNet_IsPractice())send_packet(&peers[0],TN_PRACTICE_EDIT,b,4);pendingPracticeEdit=0;}
    if(pendingLoadout) {
        unsigned char b[4+sizeof(TlLoadout)];unsigned k;put32(b,roundId);for(k=0;k<sizeof(TlLoadout)/4;k++)put32(b+4+k*4,((unsigned *)&editedLoadout)[k]);
        if(!host && TagNet_IsPractice())send_packet(&peers[0],TN_PRACTICE_LOADOUT,b,sizeof(b));pendingLoadout=0;
    }
    if(practice && host && config.party && phase==2) {TagNetConfig next=config;next.practice=1;partyRoom.phase=TL_WAITING;next.lobby=partyRoom;memset(&tournament,0,sizeof(tournament));apply_edit(&next);}
    if(pendingSkip) {pendingSkip=0;if(host && phase==2 && config.party && TlTournament_CpuMatch(&tournament)){TlTournament_Advance(&tournament,(config.seed+tournament.match)&1);tournament_match();}}
    if(leave) {
        if(phase==5 || (nativeMenu && phase==2)) {
            if(phase==2){broadcast(TN_LEAVE,NULL,0,NULL);pump();}
            int i;for(i=0;i<PEERS;i++)close_peer(&peers[i]);
            if(listener!=TN_BAD){closesocket(listener);listener=TN_BAD;}
            phase=1;role=-1;host=0;say("");
        } else {broadcast(TN_LEAVE,NULL,0,NULL);pump();failed("You left the room.");}
    }
    if(start && phase<=2)begin_socket(r,address,port);
    if(edit && phase==2 && !tournament.active && c.revision==config.revision && host && valid(&c)) {
        /* UI may move a claimed fighter, but must not create/remove a client. */
        unsigned old=player_mask(),next=0;int i;
        for(i=0;i<c.size[0]+c.size[1];i++)if(c.owner[i]>=0)next|=1u<<c.owner[i];
        if(next==old){c.seed=config.seed;c.disc=config.disc;apply_edit(&c);}
    } else if(edit && phase==2 && !host && role<TN_PLAYERS) {
        unsigned char data[sizeof(c)];unsigned i;
        for(i=0;i<sizeof(c)/4;i++)put32(data+i*4,((unsigned *)&c)[i]);send_packet(&peers[0],TN_EDIT,data,sizeof(data));
    }
    if(sw>=0 && phase==2) {
        if(host)switch_role(NULL,sw,fighter);
        else {unsigned char p[12];put32(p,config.revision);put32(p+4,sw);put32(p+8,fighter);send_packet(&peers[0],TN_ROLE,p,12);}
    }
    if(rd>=0 && phase==2 && role<TN_PLAYERS && role>=0 && sw<0 && !edit) {
        ready[role]=rd;if(host)ready_packet();else {
            unsigned char p[4+4*TN_PLAYERS]={0};put32(p,config.revision);put32(p+4+4*role,rd);send_packet(&peers[0],TN_READY,p,sizeof(p));
        }
    }
}
static int all_ready(void) {
    int i;unsigned mask=player_mask();
    if(tournament.active==2)return 0;
    if(config.party && !config.practice){if(!ready[0])return 0;}
    else if(!mask)return 0;
    /* Opt-in room regression: exchange the host and first observer's seats
       before the first match, then exchange them back before the rematch. */
    if(getenv("BT3_TAG_TEST_ROLE_SWAP") && role!=(roundId==0?TN_OBSERVER:0))return 0;
    if(getenv("BT3_TAG_TEST_PLAYERS") && (!config.party || config.practice) && popcount(mask)<atoi(getenv("BT3_TAG_TEST_PLAYERS")))return 0;
    if(config.party && !config.practice && !tournament.active && getenv("BT3_TAG_PARTY_FLOW") && getenv("BT3_TAG_TEST_PLAYERS")) {
        int wanted=atoi(getenv("BT3_TAG_TEST_PLAYERS"));if(config.mode==TL_GAUNTLET && wanted>5)wanted=5;
        if(popcount(mask)<wanted)return 0;
    }
    for(i=0;i<TN_PLAYERS;i++)if((mask&(1u<<i)) && !ready[i])return 0;
    return 1;
}
void TagNet_MenuPoll(void) {commands();pump();publish();}
int TagNet_HostPoll(void) {
    if(!hostMenu)return 0;
    commands();pump();coordinator();publish();
    return phase==5 || phase<2 || (phase==2 && !config.practice);
}
int TagNet_Room(void) {
    extern void TagOnlineMenu_Open(void),TagOnlineMenu_Close(void);
    extern void TagOnline_SaveScope(int);
    extern unsigned long long TagOnline_SaveFingerprint(void);
    extern int TagOnlineMenu_Frame(void);
    static unsigned long long offlineSaveHash;
    static int checkingSave;
    int i,leaveMenu=0;
    if(getenv("BT3_TAG_SAVE_CHECK") && !checkingSave){offlineSaveHash=TagOnline_SaveFingerprint();checkingSave=1;}
    TagOnline_SaveScope(1);
    hostMenu=0;nativeMenu=1;TagOnlineMenu_Open();
    if(!roomInitialized) {
#ifdef _WIN32
        WSADATA w;WSAStartup(MAKEWORD(2,2),&w);
#endif
        for(i=0;i<PEERS;i++){peers[i].fd=TN_BAD;peers[i].ping=-1;}
        identity=build_identity();roomInitialized=1;phase=1;
        { extern unsigned long long TagNet_DataIdentity(void);
          say("Checking game compatibility...");publish();TagOnlineMenu_Frame();
          dataIdentity=TagNet_DataIdentity();
          if(!identity || !dataIdentity){failed("Could not check the native build or game files. Restore the complete game data.");} }
        config.revision=1;config.disc=PortGame_IsBT4();config.seed=0x127391;
        config.stage=2;config.music=0;config.time=3;config.cpu=2;config.size[0]=config.size[1]=3;
        for(i=0;i<10;i++){config.chara[i]=i%2?31:3;config.owner[i]=-1;config.cpuLevel[i]=-1;}
        config.owner[0]=0;
        for(i=0;i<TN_OPTIONS;i++)config.options[i]=Port_TagOption(i);
        if(getenv("BT3_TAG_TEST_SIZE")) {
            int a=3,b=3;sscanf(getenv("BT3_TAG_TEST_SIZE"),"%d,%d",&a,&b);
            if(a>=1&&a<=5&&b>=1&&b<=5){config.size[0]=a;config.size[1]=b;for(i=0;i<10;i++)config.owner[i]=-1;config.owner[0]=0;}
        }
        if(TagNet_TestCase()) {
            for(i=0;i<10;i++)config.owner[i]=-1;
            config.owner[0]=0;/* Player seats are assigned when peers join. */
            config.options[TAG_OPT_CINEMATIC_PAUSE]=2;
            config.options[TAG_OPT_EXTRA_INTROS]=0;
            if(TagNet_TestCase()==2) {

                config.chara[0]=3;config.chara[1]=PortGame_IsBT4()?31:34;
                config.options[TAG_OPT_FUSION_SECONDS]=10;config.options[TAG_OPT_FUSION_SWAP_SECONDS]=5;
            }
            if(TagNet_TestCase()==3 || TagNet_TestCase()==4) {
                for(i=0;i<10;i++)config.chara[i]=10;
                config.options[TAG_OPT_SHARED_CLASH]=0;
                config.options[TAG_OPT_BEAM_ASSIST_CPU]=0;
            }
            if(TagNet_TestCase()==6 || TagNet_TestCase()==7) {
                config.chara[0]=0;config.chara[1]=3;
                config.options[TAG_OPT_LOCAL_CINEMATICS]=1;
                config.options[TAG_OPT_SHARED_TRANSFORM]=0;
                config.options[TAG_OPT_SHARED_ULTIMATE]=0;
            }
        }
        if(phase!=5)say("");
        if(phase!=5){const char *h=getenv("BT3_TAG_HOST"),*j=getenv("BT3_TAG_JOIN"),*w=getenv("BT3_TAG_WATCH");
         if(h)TagNet_Start(0,"",atoi(h));
         else if(j||w){char a[128];int p=7001;if(sscanf(j?j:w,"%127[^:]:%d",a,&p)>=1)TagNet_Start(w?2:1,a,p);}}
    }
    if(!Port_RollCan())failed("Rollback state files are missing. Rebuild or restore the complete native build.");
    while(phase!=3) {
        commands();pump();
        if((!identity || !dataIdentity) && phase!=5)failed("Could not check the native build or game files. Restore the complete game data.");
        if(phase==2 && getenv("BT3_TAG_TEST_ROLE_SWAP")) {
            if(host) {
                if(roundId==0 && role==0 && player_mask()==3 && spectators()>0)TagNet_SwitchRole(0,-1);
                if(roundId==1 && role==TN_OBSERVER && player_mask()==2)TagNet_SwitchRole(1,0);
            } else if(getenv("BT3_TAG_WATCH")) {
                if(roundId==0 && role==TN_OBSERVER && roomMask==2)TagNet_SwitchRole(1,0);
                if(roundId==1 && role==0)TagNet_SwitchRole(0,-1);
            }
        }
        if((getenv("BT3_TAG_AUTO_READY") || (config.party && config.practice)) && phase==2 && role>=0 && role<TN_PLAYERS && (host || peers[0].hello) && !ready[role])TagNet_Ready(1);
        if(config.party && !config.practice && !tournament.active && getenv("BT3_TAG_PARTY_FLOW") && phase==2 && role>0 && role<TN_PLAYERS && !(player_mask()&(1u<<role)))TagNet_SwitchRole(1,-1);
        if(host && config.party && tournament.active==2 && getenv("BT3_TAG_PARTY_FLOW"))TagNet_ReturnPractice();
        if(host && phase==2 && all_ready() && valid(&config)) {
            if(config.party && !config.practice && config.mode==TL_TOURNAMENT && !tournament.active){begin_tournament();continue;}
            if(getenv("BT3_TAG_TEST_HIGH_FUSION") && popcount(player_mask())==10 && config.owner[0]==0) {
                TagNetConfig c=config;
                for(i=0;i<TN_FIGHTERS;i++) {
                    int p=c.owner[i];
                    c.owner[i]=p==0?8:p==8?0:p==1?9:p==9?1:p;
                }
                apply_edit(&c);continue;
            }
            unsigned char p[8];put32(p,config.revision);put32(p+4,++roundId);broadcast(TN_START,p,8,NULL);phase=3;say("Loading the match...");
        }
        publish();
        if(TagOnlineMenu_Frame()){leaveMenu=1;break;}
        if(phase==5 && getenv("BT3_TAG_AUTO_READY")){leaveMenu=1;break;}
    }
    TagOnlineMenu_Close();nativeMenu=0;
    if(leaveMenu || phase==5) {
        broadcast(TN_LEAVE,NULL,0,NULL);pump();
        for(i=0;i<PEERS;i++)close_peer(&peers[i]);
        if(listener!=TN_BAD){closesocket(listener);listener=TN_BAD;}
        phase=1;role=-1;host=0;roomMask=roomSpectators=0;
        memset(ready,0,sizeof(ready));pendingLeave=pendingStart=pendingEdit=0;
        pendingReady=pendingSwitch=-1;publish();TagNet_Enable(0);TagOnline_SaveScope(0);
        if(checkingSave){TagNet_TestCheck(offlineSaveHash==TagOnline_SaveFingerprint(),2600);checkingSave=0;}
        return 0;
    }
    frame=live=checked=sentDigest=0;memset(have,0,sizeof(have));TagNet_Watch(0,0);
    nextSend=DELAY+1;
    begun=resim=loaded=go=returned=0;
    practiceEnd=0;chosenMode=-1;
    battleWinner=-1;
    practiceMenu=practiceChoice=0;practiceButtons=0;
    historyCount=eventCount=replayReported=0;
    disconnectedMask=0;
    for(i=0;i<TN_PLAYERS;i++){inputStart[i]=config.lobby.members[i].present?0:~0u;inputEnd[i]=~0u;}
    for(i=0;i<PEERS;i++){peers[i].loaded=peers[i].finished=peers[i].catching=0;peers[i].replayCursor=peers[i].replayAck=0;memset(peers[i].hashStamp,255,sizeof(peers[i].hashStamp));}
    memset(inputStamp,255,sizeof(inputStamp));memset(usedStamp,255,sizeof(usedStamp));memset(digestStamp,255,sizeof(digestStamp));
    for(i=0;i<=DELAY;i++) {int p;for(p=0;p<TN_PLAYERS;p++)if(input_mask()&(1u<<p))input_take(p,i,idle);}
    if(host && config.party && config.practice) {
        hostMenu=1;loaded=1;coordinatorClock=coordinatorLoadStart=SDL_GetTicks();coordinatorLogged=0;
        fprintf(stderr,"tagnet: host remains on native mode menu; no practice battle loaded\n");
        return 2;
    }
    return 1;
}
void TagNet_Loaded(void) {
    Uint64 since=SDL_GetTicks();int i;
    if(!TagNet_Active())return;
    if(getenv("BT3_TAG_TEST_LOAD_DELAY_MS")) {
        unsigned delay=(unsigned)atoi(getenv("BT3_TAG_TEST_LOAD_DELAY_MS"));
        while(phase!=5 && SDL_GetTicks()-since<delay){commands();pump();Gs_HoldFrame();SDL_Delay(2);}
        if(phase==5)return;
        since=SDL_GetTicks();
    }
    unsigned char packet[4];put32(packet,roundId);
    loaded=1;if(!host)send_packet(&peers[0],TN_LOADED,packet,4);
    while(!go && phase!=5) {
        commands();pump();
        if(host) {
            int complete=1;for(i=0;i<PEERS;i++)if(peers[i].hello && peers[i].role<TN_PLAYERS && !peers[i].loaded)complete=0;
            if(complete){go=1;broadcast(TN_GO,packet,4,NULL);}
        }
        if(SDL_GetTicks()-since>120000)failed("A player could not finish loading. The match has stopped.");
        publish();Gs_HoldFrame();SDL_Delay(2);
    }
    if(go){
        /* Loading can reuse cached resources on existing clients while a
           newcomer loads them for the first time. Start the simulation from
           the agreed RNG seed after both paths have completed. */
        extern void TagOnline_Seed(void);TagOnline_Seed();
        phase=4;say("Match started.");publish();
    }
}
static void send_input(int who,unsigned at,const unsigned char *pad) {
    unsigned char p[12+PAD];put32(p,who);put32(p+4,at);put32(p+8,roundId);memcpy(p+12,pad,PAD);
    if(host)broadcast(TN_INPUT,p,sizeof(p),NULL);else send_packet(&peers[0],TN_INPUT,p,sizeof(p));
}
/* The slowest active input producer bounds prediction. Empty player slots
   and spectators never hold the battle in the input barrier. */
static unsigned confirmed(void) {
    unsigned limit=~0u,mask=input_mask();int p;
    for(p=0;p<TN_PLAYERS;p++)if((mask&(1u<<p)) && have[p]<limit)limit=have[p];
    return limit==~0u?0:limit;
}
/* The host coordinates practice without loading Battle_Main, stage assets,
   voices or fighter state. Only guests own a practice simulation. */
static void coordinator(void) {
    Uint64 now=SDL_GetTicks();int i,complete=1,guests=0;unsigned target,limit;
    if(!hostMenu || !config.practice || phase<3 || phase>4)return;
    for(i=0;i<PEERS;i++)if(peers[i].hello){guests++;if(!peers[i].loaded)complete=0;}
    if(phase==3) {
        if(!complete){if(now-coordinatorLoadStart>120000)failed("A guest could not finish loading practice.");return;}
        {unsigned char b[4];put32(b,roundId);broadcast(TN_GO,b,4,NULL);}
        go=1;phase=4;coordinatorClock=now;say("Practice is ready. Choose a mode from the native menu.");
    }
    /* No guests means no practice world needs time. Resuming still uses the
       same archive and generation-tagged membership events. */
    if(!guests){coordinatorClock=now;if(practiceEnd)frame=practiceEnd;}
    target=frame+(unsigned)((now-coordinatorClock)*60/1000);
    if(target>frame+16)target=frame+16;
    /* Keep supplying inputs through the native exit fade and pending jobs.
       Guests acknowledge FINISHED only after that transition has completed. */
    if(target>frame)coordinatorClock+=((Uint64)(target-frame)*1000+59)/60;
    for(i=0;i<TN_PLAYERS;i++) {
        unsigned end=target+DELAY+1;
        while(have[i]<end && (!i || !produces(i,have[i]) || (disconnectedMask&(1u<<i)))) {
            unsigned at=have[i];input_take(i,at,idle);send_input(i,at,idle);
        }
    }
    limit=confirmed();if(target>limit)target=limit;
    if(target>frame)frame=target;
    record_history(frame);
    /* A paced host can cross 600 between two updates. Trigger the opt-in
       editor fixture once at that boundary, before advancing its log marker. */
    if(frame>=600 && coordinatorLogged<600 && (TagNet_TestCase()==10 || TagNet_TestCase()==12))practice_event(1,1,1);
    if(frame/600>coordinatorLogged/600){coordinatorLogged=frame;fprintf(stderr,"tagnet: frame %u, confirmed %u, players %d, host menu only\n",frame/600*600,confirmed(),popcount(player_mask()));}
    if(!practiceEnd && frame>=600 && test_practice_round() && getenv("BT3_TAG_PARTY_FLOW"))end_practice(atoi(getenv("BT3_TAG_PARTY_FLOW")));
    if(frame>=600 && getenv("BT3_TAG_TEST_KICK") && partyRoom.members[1].present)TagNet_Kick(1);
    if(practiceEnd && frame>=practiceEnd) {
        complete=1;for(i=0;i<PEERS;i++)if(peers[i].hello && peers[i].loaded && !peers[i].finished)complete=0;
        if(complete){unsigned char b[4];put32(b,roundId);broadcast(TN_ROOM,b,4,NULL);returned=1;finish_room();}
    }
}
static int inputs_match(unsigned at) {
    int p;unsigned mask=input_mask();
    if(usedStamp[at%RING]!=at)return 0;
    for(p=0;p<TN_PLAYERS;p++)if((mask&(1u<<p)) && p!=role &&
        memcmp(used[p][at%RING],inputs[p][at%RING],PAD))return 0;
    return 1;
}
static void check_digests(unsigned limit) {
    unsigned t;int i;
    for(t=limit>120?limit-120:0;t<limit;t++)if(digestStamp[t%RING]==t)
        for(i=0;i<PEERS;i++)if(peers[i].hello && (!host || peers[i].role<TN_PLAYERS) &&
            peers[i].hashStamp[t%RING]==t && peers[i].hash[t%RING]!=digest[t%RING]){desync(t,&peers[i]);return;}
}
void TagNet_Tick(unsigned blank) {
    /* Native room pages render game frames too. They must never advance the
       previous match's input stream while the next roster is being loaded. */
    if(nativeMenu)return;
    unsigned t,limit;int p;Uint64 since;
    if(phase==5){gPortResim=0;return;}
    if(!TagNet_Active())return;
    if(!loaded || !go){commands();pump();publish();Gs_HoldFrame();return;}
    if(!begun){begun=1;baseBlank=blank;checked=0;}
    frame=blank-baseBlank;
    if(resim && frame>=live)resim=0;
    digest[frame%RING]=Port_NetDigest();digestStamp[frame%RING]=frame;
    if(!resim) {
        unsigned char pad[PAD];practice_ui_input();commands();pump();
        if(host && TagNet_IsPractice() && getenv("BT3_TAG_PARTY_FLOW") && frame==600 && test_practice_round())end_practice(atoi(getenv("BT3_TAG_PARTY_FLOW")));
        if(host && TagNet_IsPractice() && (TagNet_TestCase()==10 || TagNet_TestCase()==12) && frame==600)practice_event(1,1,1);
        if(host && TagNet_IsPractice() && getenv("BT3_TAG_TEST_KICK") && frame==600)TagNet_Kick(1);
        /* Keep a departed guest's immutable input stream neutral until the
           agreed practice transition. Changing masks mid-frame would desync. */
        if(host && config.party)for(p=1;p<TL_MEMBERS;p++) {
            while((input_mask()&(1u<<p)) && have[p]<=frame+DELAY && (!produces(p,have[p]) || (disconnectedMask&(1u<<p)))){unsigned at=have[p];input_take(p,at,idle);send_input(p,at,idle);}
        }
        if(role<TN_PLAYERS && local_produces(frame+DELAY) && frame+DELAY>=have[role]) {
            memcpy(pad,idle,PAD);Port_PadRead(0,pad);
            if(config.party && !config.practice && !(player_mask()&(1u<<role)))memcpy(pad,idle,PAD);
            if(TagNet_IsPractice()) {
                if(host || practiceMenu)memcpy(pad,idle,PAD);
                /* Start belongs to the local practice overlay, never the
                   shared battle pause state or another guest's controller. */
                pad[0]|=8;
            }
            if(getenv("BT3_TAG_TEST_INPUT")) {
                /* A changing input stream for the local socket/rollback harness. */
                pad[6]=(unsigned char)((role+1)*10+(frame/17)%2);
                if(!TagNet_TestCase()) {
                    if((frame/45)%2)pad[4]=(unsigned char)(role?200:55);
                    if(frame%90<10)pad[1]&=~128;
                }
            }
            if(TagNet_TestCase()==8 && role==popcount(player_mask())-1) {
                /* Actual remote pad inputs exercise open/cancel, then the
                   pause menu's Return to room confirmation. */
                static const unsigned at[]={2000,2060,2120,2180,2220,2260,2300,2340};
                static const unsigned key[]={8,0x1000,8,0x40,0x40,0x4000,0x10,0x4000};
                unsigned k;for(k=0;k<sizeof(at)/sizeof(*at);k++)if(frame>=at[k] && frame<at[k]+4) {
                    pad[0]&=~key[k];pad[1]&=~(key[k]>>8);
                }
            }
            if(TagNet_TestCase()==10 && role==1 && frame>=850 && frame<1700 && frame%150<4)pad[1]&=~0x40;
            if(TagNet_TestCase()==10 && role==1 && frame>=800 && frame<804)pad[0]&=~0x20;
            if(TagNet_TestCase()==12 && role==1 && frame>=800 && frame<804)pad[1]&=~0x10;
            input_take(role,frame+DELAY,pad);
        }
        if(role<TN_PLAYERS && local_produces(frame+DELAY)) {
            unsigned lag=getenv("BT3_TAG_TEST_LAG")?6:0;
            if(config.party && config.practice && nextSend<inputStart[role])nextSend=inputStart[role];
            while(nextSend<have[role] && frame+DELAY>=nextSend+lag) {
                send_input(role,nextSend,inputs[role][nextSend%RING]);nextSend++;
            }
        }
        since=SDL_GetTicks();
        while(phase!=5 && frame>=confirmed()+((role==TN_OBSERVER || (lateJoining && !local_produces(frame)))?0:ROLLBACK)) {
            pump();commands();if(SDL_GetTicks()-since>10000)failed("Input from a player stopped arriving. Match stopped.");
            Gs_HoldFrame();SDL_Delay(1);
        }
        if(phase==5)return;
        limit=confirmed();if(limit>frame)limit=frame;
        record_history(limit);
        if(lateJoining && frame>=replayReported+64) {
            unsigned char b[8];put32(b,roundId);put32(b+4,frame);send_packet(&peers[0],TN_REPLAY_PROGRESS,b,8);replayReported=frame;
        }
        if(role<TN_PLAYERS) {
            for(t=checked;t<limit && inputs_match(t);t++){}
            checked=t;
            if(t<limit){live=frame;resim=1;gPortResim=1;SDL_LockSpinlock(&uiLock);status.rollbacks++;SDL_UnlockSpinlock(&uiLock);Port_RollBack((int)(frame-t));}
        } else checked=limit;
        for(t=sentDigest;t<checked;t++)if(t%30==0 && digestStamp[t%RING]==t && (host || role<TN_PLAYERS)) {
            unsigned char b[16];uint64_t h=digest[t%RING];
            put32(b,t);put32(b+4,(unsigned)(h>>32));put32(b+8,(unsigned)h);put32(b+12,roundId);
            if(host)broadcast(TN_DIGEST,b,16,NULL);else send_packet(&peers[0],TN_DIGEST,b,16);
        }
        check_digests(checked);sentDigest=checked;
        if(frame%600==0)fprintf(stderr,"tagnet: frame %u, confirmed %u, players %d, rewinds %u\n",frame,confirmed(),popcount(player_mask()),status.rollbacks);
        pump();publish();
    }
    if(role<TN_PLAYERS) {
        if(Port_RollSave()!=0)frame=blank-baseBlank;
        for(p=0;p<TN_PLAYERS;p++)if((input_mask()&(1u<<p)) && p!=role) {
            t=frame<have[p]?frame:have[p]-1;
            memcpy(used[p][frame%RING],inputs[p][t%RING],PAD);
        }
        usedStamp[frame%RING]=frame;
    }
    gPortResim=TagNet_Resim();
}
/* Do not free battle resources until every producer confirms the result. */
void TagNet_Finished(void) {
    Uint64 since=SDL_GetTicks();unsigned t;int i;
    unsigned char packet[4];put32(packet,roundId);
    if(!TagNet_Active())return;
    if(role<TN_PLAYERS && local_produces(frame))while(nextSend<have[role]){send_input(role,nextSend,inputs[role][nextSend%RING]);nextSend++;}
    while(phase!=5 && confirmed()<frame) {
        commands();pump();Gs_HoldFrame();SDL_Delay(1);
        if(SDL_GetTicks()-since>10000)failed("A player disconnected before the result was confirmed.");
    }
    if(phase==5)return;
    record_history(confirmed());
    if(role<TN_PLAYERS)for(t=checked;t<frame;t++)if(!inputs_match(t)) {
        live=frame;resim=1;gPortResim=1;Port_RollBack((int)(frame-t+1));
    }
    if(!host)send_packet(&peers[0],TN_FINISHED,packet,4);
    since=SDL_GetTicks();
    while(phase!=5 && !returned) {
        commands();pump();
        if(host) {
            int complete=1;
            for(i=0;i<PEERS;i++)if(peers[i].hello && (!config.party || peers[i].loaded) && !peers[i].finished) {
                if(peers[i].role==TN_OBSERVER && SDL_GetTicks()-since>10000)close_peer(&peers[i]);
                else complete=0;
            }
            if(complete){returned=1;broadcast(TN_ROOM,packet,4,NULL);}
        }
        if(SDL_GetTicks()-since>120000)failed("A player did not return from the match.");
        Gs_HoldFrame();SDL_Delay(2);
    }
    finish_room();
}
static void finish_room(void) {
    int i;
    if(phase!=5){phase=2;memset(ready,0,sizeof(ready));
        if(host && config.party) {
            TagNetConfig c=config;
            if(!c.practice && c.mode==TL_TOURNAMENT && tournament.active==1 && battleWinner>=0) {
                TlTournament_Advance(&tournament,battleWinner);tournament_match();
                say(tournament.active==2?"Tournament complete!":"Round complete. The next match is ready.");pump();publish();return;
            }
            if(c.practice && chosenMode>=0) {
                c.practice=0;c.mode=chosenMode;c.size[0]=c.size[1]=5;
                c.preset=chosenPreset;memset(&tournament,0,sizeof(tournament));
                c.stage=2;c.music=24;c.time=c.mode==TL_TRAINING || c.mode==TL_GAUNTLET || c.mode==TL_TOURNAMENT?0:3;
                for(i=0;i<10;i++){c.owner[i]=-1;c.chara[i]=0;c.costume[i]=0;memset(c.items[i],0,sizeof(c.items[i]));}
                c.owner[0]=0;partyRoom.phase=TL_READY_CHECK;
                if(c.mode==TL_GAUNTLET) {
                    extern void TagChallenge_Choose(int);
                    extern int TagChallenge_Capacity(void),TagChallenge_FirstCount(void),TagChallenge_FirstFighter(int,int);
                    TagChallenge_Choose(c.preset);c.size[1]=TagChallenge_Capacity();
                    for(i=0;i<c.size[1];i++){int k=c.size[0]+i,j=i%TagChallenge_FirstCount();c.chara[k]=TagChallenge_FirstFighter(j,0);c.costume[k]=TagChallenge_FirstFighter(j,1);}
                }
            } else {c.practice=1;partyRoom.phase=TL_WAITING;memset(&tournament,0,sizeof(tournament));}
            c.lobby=partyRoom;for(i=0;i<10;i++)c.lobby.draft.team[i]=i<c.size[0]?0:1;
            if(!TagNet_TestCase())for(i=0;i<TN_OPTIONS;i++)c.options[i]=Port_TagOption(i);
            apply_edit(&c);
        }
        say(config.party?(config.practice?"Back in the lobby. Returning to practice.":"Choose teams. Press Start to take a place."):"Back in the room. Change fighters or ready up for another match.");pump();publish();}
}
void TagNet_Input(int player,unsigned char *data) {
    const unsigned char *p=idle;
    if(phase==4 && begun && player>=0 && player<TN_PLAYERS && (input_mask()&(1u<<player))) {
        if(role<TN_PLAYERS && player!=role) { if(usedStamp[frame%RING]==frame)p=used[player][frame%RING]; }
        else if(inputStamp[player][frame%RING]==frame)p=inputs[player][frame%RING];
    }
    memcpy(data,p,PAD);
}

/* Camera state is local presentation, never sent as an input or saved by rollback. */
int TagNet_FreeCamera(float *p,float *r) {
    static float pos[4],rot[4];static int initialized;unsigned char pad[PAD];int on;
    on=TagNet_Observer();SDL_LockSpinlock(&uiLock);on=on&&freeCam;SDL_UnlockSpinlock(&uiLock);
    if(TagNet_Observer() && getenv("BT3_TAG_NET_WATCH_TEST"))on=(frame/120)%2;
    if(!on){initialized=0;return 0;}
    if(!initialized){memcpy(pos,p,16);memcpy(rot,r,16);initialized=1;}
    memcpy(pad,idle,PAD);Port_PadRead(0,pad);
    if(getenv("BT3_TAG_NET_WATCH_TEST")){pad[4]=180;pad[2]=145;}
    {float x=(pad[4]-128)/128.0f,z=(pad[5]-128)/128.0f;extern float sinf(float),cosf(float);
     rot[1]+=(pad[2]-128)*0.0003f;rot[0]+=(pad[3]-128)*0.0003f;
     pos[0]+=(x*cosf(rot[1])+z*sinf(rot[1]))*2;pos[2]+=(z*cosf(rot[1])-x*sinf(rot[1]))*2;
     if(!(pad[1]&4))pos[1]-=2;if(!(pad[1]&8))pos[1]+=2;}
    memcpy(p,pos,16);memcpy(r,rot,16);return 1;
}
