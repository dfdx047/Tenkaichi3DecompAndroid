/* Dedicated TAG TEAM TCP room service and native client. Existing 1v1

 * matchmaking/rollback is untouched. This first transport targets LAN/private

 * servers; it does not claim NAT traversal or production Internet hosting.

 * Compile TT_SERVER_MAIN for the standalone server, otherwise the game facade.

 */

#include "../../include/tag_team_wire.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <time.h>
int gPortTagUiCaptureRequest;
void Port_TagUiCapture(int index) {
    if(getenv("BT4_RENDER_TEST"))gPortTagUiCaptureRequest=index;
}

#ifdef _WIN32

#include <winsock2.h>

#include <ws2tcpip.h>

typedef SOCKET TtSocket;

#define TT_INVALID INVALID_SOCKET

static void sock_init(void){static int started;WSADATA w;if(!started&&WSAStartup(MAKEWORD(2,2),&w)==0)started=1;}

static int would_block(void){int e=WSAGetLastError();return e==WSAEWOULDBLOCK||e==WSAEINPROGRESS;}

static void nonblock(TtSocket s){u_long one=1;ioctlsocket(s,FIONBIO,&one);}

#else

#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <unistd.h>

#include <fcntl.h>

#include <errno.h>

typedef int TtSocket;

#define TT_INVALID (-1)

#define closesocket close

static void sock_init(void){}

static int would_block(void){return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINPROGRESS;}

static void nonblock(TtSocket s){fcntl(s,F_SETFL,O_NONBLOCK);}

#endif

static void no_delay(TtSocket s){int one=1;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char*)&one,sizeof(one));}

#include "../tag_team.c"

#include "../tag_team_wire.c"

#define TT_IO_QUEUE (TT_WIRE_BYTES*32)

typedef struct TtChannel {

    TtSocket sock;

    unsigned char in[TT_WIRE_BYTES],out[TT_IO_QUEUE];

    int used,begin,end,failed;

} TtChannel;

static void channel_close(TtChannel *c){if(c->sock!=TT_INVALID)closesocket(c->sock);memset(c,0,sizeof(*c));c->sock=TT_INVALID;}

static int channel_queue(TtChannel *c,const unsigned char *p){

    if(c->end+TT_WIRE_BYTES>TT_IO_QUEUE){

        memmove(c->out,c->out+c->begin,c->end-c->begin);c->end-=c->begin;c->begin=0;

    }

    if(c->end+TT_WIRE_BYTES>TT_IO_QUEUE)return 0;

    memcpy(c->out+c->end,p,TT_WIRE_BYTES);c->end+=TT_WIRE_BYTES;return 1;

}

static int channel_flush(TtChannel *c){

    if(c->failed)return 0;

    while(c->begin<c->end){int n=send(c->sock,(const char*)c->out+c->begin,c->end-c->begin,0);

        if(n<0)return would_block();

        if(!n)return 0;

        c->begin+=n;

    }c->begin=c->end=0;return 1;

}

/* 1 full message, 0 would block, -1 disconnected. Preserves fragmented input. */

static int channel_read(TtChannel *c){

    while(c->used<TT_WIRE_BYTES){int n=recv(c->sock,(char*)c->in+c->used,TT_WIRE_BYTES-c->used,0);

        if(n<0)return would_block()?0:-1;

        if(!n)return -1;

        c->used+=n;

    }c->used=0;return 1;

}

static uint64_t read64(const unsigned char*p){return Ttw_Read(p)|(uint64_t)Ttw_Read(p+4)<<32;}



#ifdef TT_SERVER_MAIN

#define TT_CONNECTIONS 48

#define TT_ROOMS 8

typedef struct {TtChannel io;uint32_t id,ack;int room;time_t heard;} Peer;

typedef struct {TtRoom state;char code[7];TtInputs inputs;} Room;

static Peer peers[TT_CONNECTIONS];static Room rooms[TT_ROOMS];static uint32_t serial=100;

static void notify(int room,int only,int result){

    int i;unsigned char packet[TT_WIRE_BYTES];TtRoom empty;

    memset(&empty,0,sizeof(empty));

    for(i=0;i<TT_CONNECTIONS;++i)if(peers[i].io.sock!=TT_INVALID&&(only==i||(only<0&&peers[i].room==room))){

        Ttw_Snapshot(packet,room>=0?&rooms[room].state:&empty,peers[i].id,result,room>=0?rooms[room].code:NULL);
        Ttw_Write(packet+508,peers[i].ack);

        if(!channel_queue(&peers[i].io,packet))peers[i].io.failed=1;

    }

}

static void drop(int i){

    int room=peers[i].room;channel_close(&peers[i].io);peers[i].room=-1;

    if(room>=0){Tt_Leave(&rooms[room].state,peers[i].id);notify(room,-1,TT_OK);

        if(!Tt_Humans(&rooms[room].state))memset(&rooms[room],0,sizeof(Room));}

}

static int code_exists(const char *code){int i;for(i=0;i<TT_ROOMS;++i)if(!memcmp(rooms[i].code,code,6))return 1;return 0;}

static void make_code(char *out){

    static const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";int i;

    do{for(i=0;i<6;++i)out[i]=alphabet[rand()%32];out[6]=0;}while(code_exists(out));

}

static void command(int i){

    Peer *p=&peers[i];unsigned char *b=p->io.in;int cmd=(int)Ttw_Read(b+4),rc=TT_BAD_PHASE,j;

    uint32_t epoch=Ttw_Read(b+8);TtRoom *r=p->room>=0?&rooms[p->room].state:NULL;

    if(Ttw_Read(b)!=TT_PROTOCOL){drop(i);return;}

    p->ack=Ttw_Read(b+508);

    if(cmd==TTW_PING){notify(p->room,i,TT_OK);return;}

    if(cmd==TTW_OPEN&&!r){

        int size=(int)Ttw_Read(b+12),time_limit=(int)Ttw_Read(b+16),delay=(int)Ttw_Read(b+20);

        int fill=(int)Ttw_Read(b+52),difficulty=(int)Ttw_Read(b+56);

        if(size<2||size>5||time_limit<0||time_limit>4||delay<0||delay>12||fill<0||fill>1||difficulty<0||difficulty>4){notify(-1,i,TT_BAD_CHOICE);return;}

        for(j=0;j<TT_ROOMS;++j)if(!rooms[j].state.phase){

            char name[20],code[7];memcpy(name,b+32,19);name[19]=0;make_code(code);

            Tt_Open(&rooms[j].state,p->id,name,size,read64(b+24));

            rooms[j].state.time=time_limit;rooms[j].state.delay=delay;

            if(fill)Tt_FillCPU(&rooms[j].state,p->id,rooms[j].state.epoch,difficulty);

            memcpy(rooms[j].code,code,7);p->room=j;notify(j,i,TT_OK);

            printf("room created %s seats=%d\n",code,size*2);fflush(stdout);return;

        }notify(-1,i,TT_FULL);return;

    }

    if(cmd==TTW_JOIN&&!r){

        char name[20];memcpy(name,b+32,19);name[19]=0;

        for(j=0;j<TT_ROOMS;++j)if(rooms[j].state.phase&&!memcmp(rooms[j].code,b+12,6)){

            rc=Tt_Join(&rooms[j].state,p->id,name,read64(b+24));

            if(!rc){p->room=j;notify(j,-1,TT_OK);printf("room joined %s seats=%d\n",rooms[j].code,Tt_Count(&rooms[j].state));fflush(stdout);}

            else notify(-1,i,rc);

            return;

        }notify(-1,i,TT_BAD_MEMBER);return;

    }

    if(r)switch(cmd){

    case TTW_SELECT:rc=Tt_BeginSelect(r,p->id,epoch);break;

    case TTW_CANCEL_SELECT:rc=Tt_CancelSelect(r,p->id,epoch);break;

    case TTW_CHOOSE:rc=Ttw_Read(b+20)==0xffffffffu?Tt_Choose(r,p->id,epoch,(int)Ttw_Read(b+12),(int)Ttw_Read(b+16)):

        Tt_ChooseSeat(r,p->id,epoch,(int)Ttw_Read(b+20),(int)Ttw_Read(b+12),(int)Ttw_Read(b+16));break;

    case TTW_LOAD:

        rc=Tt_BeginLoad(r,p->id,epoch,read64(b+24));

        if(!rc)Tt_InputsReset(&rooms[p->room].inputs,r->epoch,r->team_size*2);

        break;

    case TTW_LOADED:rc=Tt_Loaded(r,p->id,epoch,read64(b+24));break;

    case TTW_ARM:rc=Tt_Arm(r,p->id,epoch);break;

    case TTW_ACK:rc=Tt_ArmAck(r,p->id,epoch);break;

    case TTW_START:rc=Tt_Start(r,p->id,epoch);break;

    case TTW_INPUT:

        if(r->phase==TT_FIGHT){

            unsigned char pads[TT_MAX_PLAYERS][TT_PAD_BYTES],neutral[TT_PAD_BYTES]={0,0,0,0,127,127};

            TtInputs *q=&rooms[p->room].inputs;

            uint32_t frame=Ttw_Read(b+12);

            /* The connection owns its seat. A packet cannot select another fighter. */

            rc=Tt_InputPut(q,epoch,Tt_Seat(r,p->id),frame,b+16);

            if(!rc){

                for(j=0;j<r->team_size*2;++j)if(r->players[j].cpu)

                    Tt_InputPut(q,epoch,j,frame,neutral);

                while(Tt_InputTake(q,pads)==TT_OK){

                    unsigned char packet[TT_WIRE_BYTES];int k;

                    Ttw_Request(packet,TTW_FRAME,r->epoch);Ttw_Write(packet+12,q->next-1);

                    memcpy(packet+16,pads,sizeof(pads));

                    for(k=0;k<TT_CONNECTIONS;++k)if(peers[k].io.sock!=TT_INVALID&&peers[k].room==p->room)

                        if(!channel_queue(&peers[k].io,packet))peers[k].io.failed=1;

                }

                return;

            }

        }

        break;

    default:rc=TT_BAD_CHOICE;break;

    }

    notify(p->room,rc?i:-1,rc);

}

int main(int argc,char **argv){

    TtSocket listener;struct sockaddr_in address;int i,port=argc>1?atoi(argv[1]):26010;

    const char *bind_ip=argc>2?argv[2]:"127.0.0.1";

    if(port<1||port>65535)return 2;

    sock_init();srand((unsigned)time(NULL));

    for(i=0;i<TT_CONNECTIONS;++i){peers[i].io.sock=TT_INVALID;peers[i].room=-1;}

    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==TT_INVALID)return 3;

    memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_port=htons((unsigned short)port);

    if(inet_pton(AF_INET,bind_ip,&address.sin_addr)!=1)return 4;

    if(bind(listener,(struct sockaddr*)&address,sizeof(address))||listen(listener,16))return 5;

    nonblock(listener);printf("TAG TEAM development room server %s:%d protocol %08X\n",bind_ip,port,TT_PROTOCOL);fflush(stdout);

    for(;;){

        TtSocket accepted=accept(listener,NULL,NULL);

        if(accepted!=TT_INVALID){for(i=0;i<TT_CONNECTIONS;++i)if(peers[i].io.sock==TT_INVALID)break;

            if(i==TT_CONNECTIONS)closesocket(accepted);else{memset(&peers[i],0,sizeof(Peer));

                peers[i].io.sock=accepted;peers[i].id=++serial;peers[i].room=-1;peers[i].heard=time(NULL);nonblock(accepted);no_delay(accepted);notify(-1,i,TT_OK);}}

        for(i=0;i<TT_CONNECTIONS;++i)if(peers[i].io.sock!=TT_INVALID){

            int n,work=0;

            if(!channel_flush(&peers[i].io)||time(NULL)-peers[i].heard>30){drop(i);continue;}

            while(work++<16&&(n=channel_read(&peers[i].io))>0){peers[i].heard=time(NULL);command(i);if(peers[i].io.sock==TT_INVALID)break;}

            if(peers[i].io.sock!=TT_INVALID&&n<0)drop(i);

        }

        {struct timeval tv={0,5000};fd_set rd;FD_ZERO(&rd);FD_SET(listener,&rd);select((int)listener+1,&rd,NULL,NULL,&tv);}

    }

}

#else

static void write64(unsigned char*p,uint64_t v){Ttw_Write(p,(uint32_t)v);Ttw_Write(p+4,(uint32_t)(v>>32));}

static TtChannel client={TT_INVALID};static TtRoom state;static uint32_t member;

static char room_code[7],error[128];static time_t last_ping,last_heard;static int connecting;

#ifdef __ANDROID__
/* Android (Dragon Rage prototype): the local room only; inert stand-ins for the transports below (none is built) */
#include "tag_team_offline.inc"
#else
#include "tag_team_ws.h"
#include "tag_team_direct.h"
#include "tag_team_p2p.h"
#include "supabase_signal.h"
#endif
#include "update_sign.h"
static int tag_direct_supported,tag_direct_ticket,tag_direct_route;
static Uint64 tag_direct_ticket_at;
void Port_TagIncomingWake(void){
#ifdef _WIN32
    if(sTagWs.receiveWake)SetEvent(sTagWs.receiveWake);
#else
    if(sTagWs.lock){SDL_LockMutex(sTagWs.lock);if(sTagWs.receiveReady)SDL_SignalCondition(sTagWs.receiveReady);SDL_UnlockMutex(sTagWs.lock);}
#endif
}
#include "voice.h"
static int voice_ticket;static Uint64 voice_ticket_at;

#ifdef _WIN32
BOOLEAN WINAPI SystemFunction036(PVOID,ULONG);
#endif
static int tag_random(void *out,unsigned size){
#ifdef _WIN32
    return SystemFunction036(out,size)!=0;
#else
    FILE *f=fopen("/dev/urandom","rb");int ok;if(!f)return 0;
    ok=fread(out,1,size,f)==size;fclose(f);return ok;
#endif
}

static int cpu_fill,cpu_difficulty=2,local_room,ws_mode,ws_started,peer_mode;
static int configured_ffa,configured_quick;
void Port_TagModeOptions(int free_for_all,int quick){configured_ffa=free_for_all==1;configured_quick=quick==1;}
int Port_TagFreeForAll(void){return state.free_for_all;}
int Port_TagQueueWaiting(int queue){
#ifdef __ANDROID__
    (void)queue;return -1; /* (no matchmaking service on Android: match_supabase.cpp is not built) */
#else
    extern int Bt4Discovery_TagCounts(uint64_t,int *);int counts[3];unsigned long long content;
    if(queue<0||queue>=3||Port_TagDataFingerprint(&content)!=1||!Bt4Discovery_TagCounts(content,counts))return -1;
    return counts[queue];
#endif
}

static char ws_endpoint[384];

static void request(unsigned char *p);

static TtInputs received;
static int solo_cpu_fight(void);
#include "tag_team_sync_client.inc"

/* A room with one human has no remote fighter input to synchronize. Keep its
 * server membership and result protocol, but schedule its already-known input
 * locally so CPU play does not run at one round trip per simulation tick. A
 * second human cannot join a fight in progress; all multiplayer rooms retain
 * their authoritative frame queue. */
static int solo_cpu_fight(void){
    return state.phase==TT_FIGHT&&state.host==member&&Tt_Humans(&state)==1;
}

int Port_TagLocalBattle(void){return local_room||solo_cpu_fight();}

static int configured_left,configured_right,configure_pending;

static char chat_lines[6][192];static int chat_count;

static int cancel_pending;

void Port_TagWaitClear(void);
void Port_TagWaitResumed(void);

extern void Port_TagMenuSet(int,int);

int Port_TagDraftPending(void){return cancel_pending;}

void Port_TagInitialTeams(int left,int right){configured_left=left;configured_right=right;configure_pending=1;}

/* The room's load digest is a match-setup checksum, not a language or UI checksum.
 * Include the complete roster so a 2v2..5v5 session checks every selected fighter. */
static uint64_t tag_load_digest_word(uint64_t hash,uint32_t value){
    int i;for(i=0;i<4;++i){hash=(hash^(uint8_t)(value>>(i*8)))*0x100000001b3ULL;}return hash;
}
static uint64_t tag_load_digest(const TtRoom *r){
    uint64_t hash=0xcbf29ce484222325ULL;int i,count=Tt_Capacity(r);
    hash=tag_load_digest_word(hash,TT_PROTOCOL);
    hash=tag_load_digest_word(hash,(uint32_t)r->free_for_all);
    hash=tag_load_digest_word(hash,(uint32_t)r->content);
    hash=tag_load_digest_word(hash,(uint32_t)(r->content>>32));
    hash=tag_load_digest_word(hash,(uint32_t)r->team_size);
    hash=tag_load_digest_word(hash,(uint32_t)r->time);
    hash=tag_load_digest_word(hash,(uint32_t)r->delay);
    hash=tag_load_digest_word(hash,(uint32_t)count);
    if(count<0)count=0;if(count>TT_MAX_PLAYERS)count=TT_MAX_PLAYERS;
    for(i=0;i<count;++i){
        const TtPlayer *p=&r->players[i];
        hash=tag_load_digest_word(hash,(uint32_t)i);
        hash=tag_load_digest_word(hash,(uint32_t)p->team);
        hash=tag_load_digest_word(hash,(uint32_t)p->chara);
        hash=tag_load_digest_word(hash,(uint32_t)p->costume);
        hash=tag_load_digest_word(hash,(uint32_t)p->cpu);
        hash=tag_load_digest_word(hash,(uint32_t)p->difficulty);
        hash=tag_load_digest_word(hash,(uint32_t)p->confirmed);
    }
    return hash;
}

const char *Port_TagEndpoint(void){const char *s=getenv("BT4_TAG_SERVER");return s&&s[0]?s:Bt4Signal_Project();}

/* Keep clipboard strings and the 64-bit release fingerprint in the native
 * facade. The menu only passes its bounded code and receives a status. */
int Port_TagCopyCode(void){
    extern void PortTbo_Copy(const char *);
    if(strlen(room_code)!=6)return 0;
    PortTbo_Copy(room_code);return 1;
}
int Port_TagJoinCode(const char *code){
    unsigned long long content=0;char canonical[7];int i;
    extern const char *Port_NetNameGet(void);
    if(!code){snprintf(error,sizeof(error),"Enter a six-character room code.");return 0;}
    for(i=0;i<6;++i){unsigned char c=(unsigned char)code[i];
        if(c>='a'&&c<='z')c-=32;
        if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9'))){snprintf(error,sizeof(error),"Enter a six-character room code.");return 0;}
        canonical[i]=(char)c;
    }
    if(code[6]){snprintf(error,sizeof(error),"Enter a six-character room code.");return 0;}canonical[6]=0;
    if(Port_TagDataFingerprint(&content)!=1){snprintf(error,sizeof(error),"This is an old or incompatible version. Apply the complete BT4 patch.");return 0;}
    Port_TagCPUOptions(0,2);
    if(!Port_TagConnect(Port_TagEndpoint()))return 0;
    Port_TagJoin(canonical,Port_NetNameGet(),content);
    return !error[0];
}

static SDL_AtomicInt sTagRenderActive;
void Port_TagBattleRenderSet(int active){SDL_SetAtomicInt(&sTagRenderActive,active!=0);}
int Port_TagBattleRenderActive(void){return SDL_GetAtomicInt(&sTagRenderActive);}

/* Cached verification of the compatible base data, independent of release labels. */
int Port_TagDataFingerprint(unsigned long long *out){
    extern int Port_BuildIdentity(unsigned long long *);
    return Port_BuildIdentity(out);
}

int Port_TagChatCount(void){return chat_count;}

const char *Port_TagChatLine(int i){return i>=0&&i<chat_count?chat_lines[i]:"";}

void Port_TagChatSend(const char *text){unsigned char p[512];if(!text||!text[0]||local_room||Port_TagSeat()<0)return;Ttw_Request(p,TTW_CHAT,state.epoch);snprintf((char*)p+16,160,"%s",text);request(p);}



void Port_TagCPUOptions(int fill,int difficulty){cpu_fill=fill==1;cpu_difficulty=difficulty>=0&&difficulty<=4?difficulty:2;}

/* Keep a local transport incident even when a tester restarts the game and
 * bt3_log.txt is rotated. No room codes, names, voice tickets or credentials. */
static void tag_transport_report(const char *why){
    char path[2048],folder[1800];const char *base=SDL_GetBasePath();FILE *f;
    unsigned rx=0,tx=0;
    TagWs_QueueCounts(&rx,&tx);
    snprintf(folder,sizeof(folder),"%stbo/logs",base?base:"");SDL_CreateDirectory(folder);
    #ifdef _WIN32
    snprintf(path,sizeof(path),"%s/tag-transport-%lld-%u.txt",folder,(long long)time(NULL),(unsigned)GetCurrentProcessId());
#else
    snprintf(path,sizeof(path),"%s/tag-transport-%lld-%u.txt",folder,(long long)time(NULL),(unsigned)getpid());
#endif
    f=fopen(path,"a");if(!f)return;
    fprintf(f,"reason=%s\nphase=%d epoch=%u paced=%d ping_ms=%d relay_ping_ms=%d direct_route=%d next=%u latest=%u backlog=%d rx_queue=%u tx_queue=%u seconds_since_reply=%lld\n",
        why,state.phase,state.epoch,tag_paced,Port_TagPing(),tag_ping,tag_direct_route,received.next,tag_received_latest,tag_backlog(),rx,tx,(long long)(time(NULL)-last_heard));
    fclose(f);
}
static void fail(const char *why){tag_transport_report(why);SDL_SetAtomicInt(&tag_catchup,0);if(peer_mode)TagPeer_Close();TagDirect_Close();tag_direct_route=0;PortVoice_Leave(2);if(ws_mode)TagWs_Close();channel_close(&client);state.phase=TT_ABORTED;snprintf(error,sizeof(error),"%s",why);}

int Port_TagConnect(const char *server){

    char ip[64];const char *colon;int port;struct sockaddr_in a;

    Port_TagClose();sock_init();

    /* CPU practice uses the same validated room model entirely in-process. */

    if ((!server || !server[0])) {

        local_room=1; member=1; return 1;

    }

#ifdef __ANDROID__
    /* Android (Dragon Rage prototype): no TAG TEAM server yet, the room service included (Quick Match, Join). Refused
       before any socket or thread opens; only the local room above plays. */
    snprintf(error,sizeof(error),"Online TAG TEAM is not available on Android yet.");return 0;
#endif

    if(!strcmp(server,Bt4Signal_Project())){
        if(!TagPeer_Open()){snprintf(error,sizeof(error),"Cannot initialize the direct connection.");return 0;}
        peer_mode=ws_mode=ws_started=1;tag_paced=1;tag_caps_sent=1;
        snprintf(ws_endpoint,sizeof(ws_endpoint),"%s",server);last_ping=last_heard=time(NULL);return 1;
    }
    if(!strncmp(server,"https://",8) || (getenv("BT4_RENDER_TEST")&&!strncmp(server,"http://127.0.0.1:",17))){

        if(strlen(server)>=sizeof(ws_endpoint)){snprintf(error,sizeof(error),"TAG TEAM endpoint is too long.");return 0;}

        ws_mode=1;snprintf(ws_endpoint,sizeof(ws_endpoint),"%s",server);return 1;

    }

    if(!server||!(colon=strrchr(server,':'))||colon-server>63){snprintf(error,sizeof(error),"Set a TAG TEAM server (IPv4:port) first.");return 0;}

    memcpy(ip,server,colon-server);ip[colon-server]=0;port=atoi(colon+1);

    memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((unsigned short)port);

    if(port<1||port>65535||inet_pton(AF_INET,ip,&a.sin_addr)!=1){snprintf(error,sizeof(error),"Invalid TAG TEAM server address.");return 0;}

    client.sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(client.sock==TT_INVALID){fail("Cannot open TAG TEAM connection.");return 0;}

    nonblock(client.sock);no_delay(client.sock);connecting=1;last_ping=last_heard=time(NULL);

    if(connect(client.sock,(struct sockaddr*)&a,sizeof(a))<0&&!would_block()){fail("TAG TEAM server is unavailable.");return 0;}

    return 1;

}

void Port_TagClose(void){if(peer_mode)TagPeer_Close();peer_mode=0;Port_TagWaitResumed();TagDirect_Close();tag_direct_supported=tag_direct_ticket=tag_direct_route=0;tag_direct_ticket_at=0;PortVoice_Leave(2);voice_ticket=0;voice_ticket_at=0;Port_TagMenuSet(0,0);Port_TagWaitClear();TagWs_Close();ws_mode=ws_started=0;ws_endpoint[0]=0;channel_close(&client);memset(&state,0,sizeof(state));memset(&received,0,sizeof(received));member=0;configure_pending=chat_count=cancel_pending=0;room_code[0]=error[0]=0;connecting=0;local_room=0;tag_caps_sent=tag_paced=tag_input_sent=tag_sync_count=0;tag_ping=-1;tag_ping_ack=0;tag_ping_at=0;tag_delay_margin=1;tag_delay_clean_frames=0;tag_delay_feedback_seen=0;tag_delay_adjusted=0;tag_pending_buttons=tag_pending_l3=tag_pending_skip=0;tag_ping_count=tag_ping_cursor=0;tag_received_valid=0;SDL_SetAtomicInt(&tag_catchup,0);}

int Port_TagReselect(void){

    if(!local_room)return state.phase==TT_SELECT;

    if(state.host!=member || state.phase!=TT_SELECT)return 0;

    if(Tt_CancelSelect(&state,member,state.epoch)!=TT_OK)return 0;

    error[0]=0;

    return Tt_BeginSelect(&state,member,state.epoch)==TT_OK;

}

static unsigned request_serial;
static struct {unsigned serial,command,epoch;Uint64 at;} request_history[128];
static void request(unsigned char *p){
    if(ws_mode&&tag_direct_route&&Ttw_Read(p+4)==TTW_INPUT){if(!TagDirect_Send(p))fail("The TAG TEAM coordinator connection was lost.");return;}
    unsigned serial=++request_serial,slot=serial%128;
    request_history[slot].serial=serial;request_history[slot].command=Ttw_Read(p+4);request_history[slot].epoch=Ttw_Read(p+8);
    request_history[slot].at=SDL_GetTicks();
    Ttw_Write(p+508,serial);
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_TEST")&&Ttw_Read(p+4)!=11&&Ttw_Read(p+4)!=17&&Ttw_Read(p+4)!=10)
        fprintf(stderr,"tag request: serial=%u cmd=%u epoch=%u phase=%d\n",serial,Ttw_Read(p+4),Ttw_Read(p+8),state.phase);

    if(peer_mode){if(!TagPeer_Send(p))fail("The direct connection to the room was lost.");}
    else if(ws_mode){if(!ws_started||!TagWs_Send(p))fail("TAG TEAM WebSocket send failed.");}

    else if(client.sock!=TT_INVALID&&!channel_queue(&client,p))fail("TAG TEAM send queue overflow.");

}

static int ws_start(const char *code){

    if(peer_mode)return 1;
    if(!ws_mode)return 1;

    if(ws_started||!TagWs_Open(ws_endpoint,code)){fail("Cannot open secure TAG TEAM room.");return 0;}

    ws_started=1;last_ping=last_heard=time(NULL);return 1;

}

void Port_TagOpen(int size,int limit,int delay,const char *name,uint64_t content){
    delay=0; /* Tag Team no longer adds a user-configured simulation delay. */
    if(configured_ffa)size=3;

    if(local_room){

        Tt_Open(&state,member,name,size,content);state.time=limit;state.delay=delay;
        Tt_SetMode(&state,member,state.epoch,configured_ffa);

        if(cpu_fill)Tt_FillCPU(&state,member,state.epoch,cpu_difficulty);

        memcpy(room_code,"LOCAL0",7);return;

    }

    if(ws_mode&&!peer_mode){static const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

        unsigned char random[6];int i;

        if(!tag_random(random,sizeof(random))){fail("Cannot create a secure room code.");return;}

        for(i=0;i<6;++i)room_code[i]=alphabet[random[i]&31];room_code[6]=0;

        if(!ws_start(room_code))return;

    }

    unsigned char b[TT_WIRE_BYTES];Ttw_Request(b,TTW_OPEN,0);Ttw_Write(b+12,size);Ttw_Write(b+16,limit);Ttw_Write(b+20,delay);

    write64(b+24,content);if(name)snprintf((char*)b+32,20,"%s",name);

    Ttw_Write(b+52,cpu_fill);Ttw_Write(b+56,cpu_difficulty);
    Ttw_Write(b+60,configured_ffa);Ttw_Write(b+64,configured_quick);request(b);

}

void Port_TagJoin(const char *code,const char *name,uint64_t content){
    configured_quick=0;

    unsigned char b[TT_WIRE_BYTES];Ttw_Request(b,TTW_JOIN,0);if(!code||strlen(code)!=6)return;

    if(!ws_start(code))return;

    memcpy(b+12,code,6);write64(b+24,content);if(name)snprintf((char*)b+32,20,"%s",name);request(b);

}

void Port_TagCommand(int command,int a,int b){

    if(local_room){

        int rc=TT_BAD_PHASE;

        if(command==TTW_RESULT && a==1){
            int i;state.result_choice=1;++state.epoch;++state.revision;state.phase=TT_SELECT;
            for(i=0;i<Tt_Count(&state);++i){state.players[i].confirmed=0;state.players[i].chara=-1;state.players[i].ended=state.players[i].loaded=state.players[i].armed=0;}
            rc=TT_OK;
        }
        if(command==TTW_SELECT)rc=Tt_BeginSelect(&state,member,state.epoch);

        if(command==TTW_CANCEL_SELECT)rc=Tt_CancelSelect(&state,member,state.epoch);

        if(command==TTW_CHOOSE)rc=Tt_ChooseSeat(&state,member,state.epoch,Port_TagDraftSeat(),a,b);

        if(rc)snprintf(error,sizeof(error),"Cannot apply room action (%d).",rc);else error[0]=0;

        return;

    }

    unsigned char packet[TT_WIRE_BYTES];Ttw_Request(packet,command,state.epoch);

    if(command==TTW_CANCEL_SELECT)cancel_pending=1;

    Ttw_Write(packet+12,a);Ttw_Write(packet+16,b);
    /* Each client recomputes this from the shared, complete roster. The host
     * establishes it at LOAD; every LOADED acknowledgement must match it. */
    write64(packet+24,(command==TTW_LOAD||command==TTW_LOADED)?tag_load_digest(&state):state.load_digest);

    if(command==TTW_CHOOSE)Ttw_Write(packet+20,(uint32_t)Port_TagDraftSeat());

    request(packet);

}

static int tag_read_packet(unsigned char *packet,Uint64 *at,int *direct){
    int n;*direct=0;if(peer_mode){n=TagPeer_Read(packet,(uint64_t*)at);if(n>0)*direct=Ttw_Read(packet+4)==TTW_FRAME;return n;}if(!ws_mode)return channel_read(&client);
    n=TagWs_Read(packet,at);if(n)return n;
    if(tag_direct_route&&state.phase==TT_FIGHT&&TagDirect_Read(packet,(uint64_t*)at)){*direct=1;return 1;}
    return 0;
}
void Port_TagPoll(void){

    int n=0,work=0,direct=0;Uint64 arrived=0;if(ws_mode?!ws_started:client.sock==TT_INVALID)return;
    if(peer_mode&&TagPeer_Status()<0){fail("Could not establish or maintain a direct connection to the room.");return;}
    if(tag_direct_route&&TagDirect_Active()<0){fail("This client fell too far behind the TAG TEAM coordinator.");return;}

    if(connecting){fd_set wr,er;struct timeval tv={0,0};int e=0;int len=sizeof(e);

        FD_ZERO(&wr);FD_ZERO(&er);FD_SET(client.sock,&wr);FD_SET(client.sock,&er);

        select((int)client.sock+1,NULL,&wr,&er,&tv);

        if(FD_ISSET(client.sock,&er)){fail("TAG TEAM server is unavailable.");return;}

        if(!FD_ISSET(client.sock,&wr)){if(time(NULL)-last_ping>8)fail("TAG TEAM connection timed out.");return;}

        getsockopt(client.sock,SOL_SOCKET,SO_ERROR,(char*)&e,&len);

        if(e){fail("TAG TEAM server is unavailable.");return;}connecting=0;

    }

    if(!ws_mode&&!channel_flush(&client)){fail("TAG TEAM connection was lost.");return;}

    while(work++<64&&(n=tag_read_packet(client.in,&arrived,&direct))>0){int result;

        if(!direct)last_heard=time(NULL);

        if(Ttw_Read(client.in)==TT_PROTOCOL&&Ttw_Read(client.in+4)==TT_SYNC_NOTICE){tag_sync_notice(client.in);continue;}
        if(Ttw_Read(client.in)==TT_PROTOCOL&&Ttw_Read(client.in+4)==TT_CAPS_NOTICE){
            tag_paced=(Ttw_Read(client.in+12)&TT_CAP_PACED_INPUT)&&Ttw_Read(client.in+16)==TT_TAG_SPEED_PERCENT;
            tag_direct_supported=(Ttw_Read(client.in+44)&TT_CAP_DIRECT_INPUT)!=0;
            tag_direct_route=tag_direct_supported&&Ttw_Read(client.in+24)==1&&Ttw_Read(client.in+40)==state.epoch;
            fprintf(stderr,"tag transport: paced-input=%d speed=%u relay-ping-ms=%d direct=%d\n",tag_paced,Ttw_Read(client.in+16),tag_ping,tag_direct_route);continue;
        }
        if(Ttw_Read(client.in)==TT_PROTOCOL&&Ttw_Read(client.in+4)==TTW_FRAME){
            if(tag_direct_route&&!direct)continue;

            uint32_t epoch=Ttw_Read(client.in+8),frame=Ttw_Read(client.in+12);int seat;

            /* Input already in flight when a round ends belongs to that old
             * round. Discard it; it cannot mutate the next round's ring. A
             * future epoch is still a protocol error and is reported. */
            if(epoch!=state.epoch){if((int32_t)(epoch-state.epoch)<0)continue;fail("Unexpected TAG TEAM input epoch.");return;}
            if(state.phase!=TT_FIGHT)continue;

            if(received.epoch!=epoch)Tt_InputsReset(&received,epoch,Tt_Count(&state));

            /* A consumed authoritative frame is idempotent, including the
             * server echo used by one-human CPU rooms. */
            if((int32_t)(frame-received.next)<0)continue;

            if(tag_paced&&Ttw_Read(client.in+492)==0x32535442u)tag_delay_feedback(frame,Ttw_Read(client.in+496));

            if(tag_paced&&(int32_t)(frame-received.next)>=TT_INPUT_HISTORY){
                fprintf(stderr,"tag local backlog exceeded: next=%u incoming=%u capacity=%d\n",received.next,frame,TT_INPUT_HISTORY);
                fail("This client fell too far behind the battle. The other players can continue.");return;
            }

            for(seat=0;seat<Tt_Count(&state);++seat)

                if(Tt_InputPut(&received,epoch,seat,frame,client.in+16+seat*TT_PAD_BYTES)!=TT_OK){fail("Invalid TAG TEAM input frame.");return;}

            if(!tag_received_valid||tag_received_epoch!=epoch||(int32_t)(frame-tag_received_latest)>0)tag_received_latest=frame;
            tag_received_epoch=epoch;tag_received_valid=1;tag_update_catchup();

            continue;

        }

        if(Ttw_Read(client.in)==TT_PROTOCOL&&Ttw_Read(client.in+4)==105){
            char cap[65];unsigned char raw[32];memcpy(cap,client.in+16,64);cap[64]=0;
            if(tag_direct_supported&&Upd_HexBytes(cap,raw,32)&&Port_TagSeat()>=0){tag_direct_ticket=1;TagDirect_Session(ws_endpoint,room_code,cap,member);TagDirect_RelayPing(tag_ping);}
            continue;
        }
        if(Ttw_Read(client.in)==TT_PROTOCOL&&Ttw_Read(client.in+4)==102){
            char cap[65];int i,ok=1;memcpy(cap,client.in+16,64);cap[64]=0;
            for(i=0;i<64;++i)if(!((cap[i]>='0'&&cap[i]<='9')||(cap[i]>='a'&&cap[i]<='f')))ok=0;
            if(ok&&Port_TagSeat()>=0){voice_ticket=1;PortVoice_Session(2,ws_endpoint,room_code,cap,Port_TagSeat(),state.players[Port_TagSeat()].name);}
            continue;
        }
        if(Ttw_Read(client.in)==TT_PROTOCOL&&Ttw_Read(client.in+4)==TTW_CHAT){

            int seat=Tt_Seat(&state,Ttw_Read(client.in+12)),j;char text[160];

            for(j=0;j<159&&client.in[16+j];++j){unsigned char c=client.in[16+j];text[j]=c>=32&&c<127?c:'?';}text[j]=0;

            if(seat>=0){if(chat_count==6){memmove(chat_lines,chat_lines+1,5*sizeof(chat_lines[0]));--chat_count;}

                snprintf(chat_lines[chat_count++],192,"%s: %s",state.players[seat].name,text);}

            continue;

        }

        if(!Ttw_Decode(client.in,&state,&member,&result,room_code)){fail("Incompatible TAG TEAM server.");return;}
        if(peer_mode&&!result){
            char descriptor[8192];PortVoiceRoomMember people[TT_MAX_PLAYERS];int i,count=0;
            if(TagPeer_Descriptor(descriptor,sizeof(descriptor))){
                for(i=0;i<Tt_Count(&state);++i)if(!state.players[i].cpu&&state.players[i].member){
                    people[count].id=state.players[i].member;people[count].seat=i;people[count].team=state.players[i].team;
                    snprintf(people[count].name,sizeof(people[count].name),"%s",state.players[i].name);++count;
                }
                PortVoice_P2PSession(2,descriptor,people,count);
            }
        }
        if(received.epoch!=state.epoch || (state.phase!=TT_FIGHT&&received.seats!=Tt_Count(&state))){
            /* A result choice starts a new epoch before its first input arrives.
             * Buffered inputs from the preceding fight must not advance the
             * new simulation while the direct route is reconnecting. Lobby
             * admission can grow the roster without changing its epoch; bind
             * the ring to that final roster before any fight frames arrive. */
            Tt_InputsReset(&received,state.epoch,Tt_Count(&state));
            tag_received_valid=0;SDL_SetAtomicInt(&tag_catchup,0);
        }
        TagDirect_State(client.in);
        if(state.phase!=TT_FIGHT&&state.phase!=TT_ARMED)tag_direct_route=0;
        tag_update_catchup();
        {unsigned ack=Ttw_Read(client.in+508),slot=ack%128;
            if(ack&&ack!=tag_ping_ack&&request_history[slot].serial==ack&&request_history[slot].command==TTW_PING){
                Uint64 at=ws_mode?arrived:SDL_GetTicks();tag_ping_ack=ack;
                if(at>=request_history[slot].at){tag_ping_sample((unsigned)(at-request_history[slot].at));TagDirect_RelayPing(tag_ping);}
            }
        }

        if(state.phase==TT_LOBBY || result)cancel_pending=0;

        if(result){static const char *messages[]={"","Wrong room phase.","Room or player not found.","This room is full.","Only the host can do that.","Wait for your team's turn.","Invalid fighter or room settings.","Waiting for all players.","The room changed. Try again.","The room setup or base game data does not match.","The battle states remained different. A synchronization report was saved in tbo/logs."};
            unsigned ack=Ttw_Read(client.in+508),slot=ack%128;
            fprintf(stderr,"tag request rejected: result=%d epoch=%u phase=%d ack=%u command=%u sent-epoch=%u\n",result,state.epoch,state.phase,ack,request_history[slot].serial==ack?request_history[slot].command:0,request_history[slot].serial==ack?request_history[slot].epoch:0);

            snprintf(error,sizeof(error),"%s",result>0&&result<=TT_DESYNC?messages[result]:"TAG TEAM request rejected.");}

        else error[0]=0;

        if(!result&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_TEST")&&state.phase==TT_LOBBY&&(member==state.host||getenv("BT4_TAG_QUICK_TEST"))){fprintf(stderr,"tag online code=%s humans=%d\n",room_code,Tt_Humans(&state));fflush(stderr);}

        if(!result && state.phase==TT_LOBBY && member==state.host && configure_pending==1){

            configure_pending=2;Port_TagTeams(configured_left,configured_right);

        }else if(!result && state.phase==TT_LOBBY && state.epoch>1 && configure_pending==2)configure_pending=0;

    }

    if(ws_mode&&!peer_mode&&Port_TagSeat()>=0&&!tag_caps_sent){
        unsigned char b[512];Ttw_Request(b,TT_CAPS_COMMAND,state.epoch);Ttw_Write(b+12,TT_CAP_PACED_INPUT|TT_CAP_DETAILED_SYNC|(TagDirect_Available()?TT_CAP_DIRECT_INPUT:0));request(b);tag_caps_sent=1;
    }
    if(ws_mode&&!peer_mode&&tag_direct_supported&&!tag_direct_ticket&&Port_TagSeat()>=0&&TagDirect_Available()&&SDL_GetTicks()-tag_direct_ticket_at>=1000){
        unsigned char b[512];Ttw_Request(b,23,state.epoch);request(b);tag_direct_ticket_at=SDL_GetTicks();
    }
    if(ws_mode&&!peer_mode&&!voice_ticket&&Port_TagSeat()>=0&&SDL_GetTicks()-voice_ticket_at>=1500){
        unsigned char b[512];Ttw_Request(b,TTW_VOICE_TICKET,state.epoch);request(b);voice_ticket_at=SDL_GetTicks();
    }
    if(n<0){fail("TAG TEAM connection was lost.");return;}

    if((!peer_mode||member)&&time(NULL)-last_heard>10){fail("TAG TEAM server stopped responding.");return;}

    if(SDL_GetTicks()-tag_ping_at>=(state.phase==TT_FIGHT?500:1000)){unsigned char b[TT_WIRE_BYTES];Ttw_Request(b,TTW_PING,state.epoch);request(b);last_ping=time(NULL);tag_ping_at=SDL_GetTicks();}

}

void Port_TagYield(void){
    if(peer_mode){SDL_Delay(2);return;}
    if(!ws_mode){SDL_Delay(4);return;}
#ifdef _WIN32
    if(!(tag_direct_route&&state.phase==TT_FIGHT&&TagDirect_Pending()))TagWs_Wait(4);
#else
    if(!sTagWs.lock){SDL_Delay(4);return;}
    SDL_LockMutex(sTagWs.lock);
    if(sTagWs.inRead==sTagWs.inWrite&&!(tag_direct_route&&state.phase==TT_FIGHT&&TagDirect_Pending())&&SDL_GetAtomicInt(&sTagWs.status)>=0)
        SDL_WaitConditionTimeout(sTagWs.receiveReady,sTagWs.lock,4);
    SDL_UnlockMutex(sTagWs.lock);
#endif
}

static int tag_wait_banner;
static uint64_t tag_wait_started;

void Port_TagWaitResumed(void){
    extern void Gs_NetworkWait(int);
    Gs_NetworkWait(0);
    if(tag_wait_started){
        extern void Port_PaceNetworkResume(unsigned long long);
        extern uint64_t gPortNetWaitNs;
        uint64_t elapsed=SDL_GetTicksNS()-tag_wait_started;
        tag_wait_started=0;
        gPortNetWaitNs+=elapsed;
        Port_PaceNetworkResume(elapsed);
    }
}

void Port_TagWaitClear(void){extern void Port_UiBanner(const char *,int);if(tag_wait_banner){Port_UiBanner(NULL,0);tag_wait_banner=0;}}

void Port_TagWait(int phase,int menu,int cursor,int show){

    extern void Gs_NetworkWait(int),Port_UiBanner(const char *,int);

    const char *text=NULL;
    if(!tag_wait_started)tag_wait_started=SDL_GetTicksNS();

    Port_TagMenuSet(menu,cursor);

    if(!menu&&show)text=phase<5?"TAG TEAM ONLINE\nConnecting players...":"TAG TEAM ONLINE\nWaiting for players...";

    if(text){Port_UiBanner(text,0);tag_wait_banner=1;}else Port_TagWaitClear();

    Gs_NetworkWait(1);Port_TagYield();

}

volatile int gPortTagCaptureRequest;

void Port_TagCapture(void){if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_CAPTURE")){gPortTagCaptureRequest=1;fprintf(stderr,"tag online capture requested\n");}}

int Port_TagMovePlayer(int from,int to){if(local_room)return Tt_MovePlayer(&state,member,state.epoch,from,to)==TT_OK;Port_TagCommand(19,from,to);return !error[0];}

int Port_TagResultChoice(void){return state.result_choice;}

void Port_TagHash(unsigned frame,unsigned hash){
    unsigned char p[512];
    /* A round changes epoch before both clients finish their result fade.
     * Diagnostics from that transition are not fight commands. */
    if(state.phase!=TT_FIGHT){
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_TEST"))fprintf(stderr,"tag hash deferred: frame=%u epoch=%u phase=%d\n",frame,state.epoch,state.phase);
        return;
    }
    Ttw_Request(p,TTW_HASH,state.epoch);Ttw_Write(p+12,frame);Ttw_Write(p+16,hash);
    if(tag_sync_count==Tt_Count(&state)){
        int i,k;Ttw_Write(p+20,tag_sync_count);Ttw_Write(p+24,TT_SYNC_MAGIC);
        for(i=0;i<tag_sync_count;++i)for(k=0;k<TT_SYNC_WORDS;++k)Ttw_Write(p+TT_SYNC_OFFSET+(i*TT_SYNC_WORDS+k)*4,tag_sync_words[i][k]);
    }
    tag_sync_last_frame=frame;request(p);
}

int Port_TagPhase(void){return state.phase;}

int Port_TagCount(void){return Tt_Count(&state);}

int Port_TagCapacity(void){return state.phase>=TT_SELECT&&state.phase<=TT_FIGHT?Tt_Count(&state):Tt_Capacity(&state);}

int Port_TagSlots(void){return Tt_Capacity(&state);}

int Port_TagTeam(int seat){return seat>=0&&seat<Tt_Capacity(&state)?state.players[seat].team:seat&1;}

int Port_TagTeamIndex(int seat){int i,n=0;for(i=0;i<seat;++i)if(Port_TagTeam(i)==Port_TagTeam(seat))++n;return n;}

int Port_TagTeamCount(int team){return Tt_TeamCount(&state,team,1);}

int Port_TagTeamSlots(int team){return Tt_TeamCount(&state,team,0);}

int Port_TagTeams(int left,int right){

    if(local_room)return Tt_SetTeams(&state,member,state.epoch,left,right)==TT_OK;

    Port_TagCommand(TTW_TEAMS,left,right);return !error[0];

}

int Port_TagSlot(int seat,int cpu,int difficulty){

    if(local_room)return Tt_SetSlot(&state,member,state.epoch,seat,cpu,difficulty)==TT_OK;

    unsigned char p[512];Ttw_Request(p,TTW_SLOT,state.epoch);Ttw_Write(p+12,seat);Ttw_Write(p+16,cpu);Ttw_Write(p+20,difficulty);request(p);return !error[0];

}

int Port_TagNetwork(void){return !local_room&&(ws_mode||client.sock!=TT_INVALID);}

int Port_TagStage(void){return state.stage;}

int Port_TagBgm(void){return state.bgm;}

int Port_TagAttacks(void){return 1;}

unsigned Port_TagSeed(void){return state.seed;}

int Port_TagDelay(void){
    int lead=1,full=1,margin=0,total,ping=Port_TagPing();
    if(local_room||solo_cpu_fight()||!tag_paced)return state.delay;
    if((tag_direct_route||peer_mode)&&member==state.host)return tag_backlog();
    /* Use estimated one-way relay time, not the full RTT. The Worker reports
     * late seats in each authoritative frame; that feedback raises this margin
     * only when this player's input actually misses a deadline. */
    if(tag_paced&&ping>0){
        lead=(ping*342+19999)/20000;
        full=(ping*342+9999)/10000;
        if(lead<1)lead=1;
        margin=tag_delay_margin;
        if(margin>full-lead)margin=full-lead;
    }
    /* New input belongs to the live server timeline even while this client
     * consumes old authoritative history after a local stall. */
    total=lead+margin+tag_backlog();
    return total>120?120:total;
}

int Port_TagAllLoaded(void){int i;for(i=0;i<Tt_Count(&state);++i)if(!state.players[i].loaded)return 0;return Tt_Count(&state)>0;}

int Port_TagAllArmed(void){int i;for(i=0;i<Tt_Count(&state);++i)if(!state.players[i].armed)return 0;return Tt_Count(&state)>0;}

int Port_TagAllEnded(void){int i;for(i=0;i<Tt_Count(&state);++i)if(!state.players[i].ended)return 0;return Tt_Count(&state)>0;}

void Port_TagArena(int stage,int bgm,int attacks){unsigned char p[512];Ttw_Request(p,TTW_ARENA,state.epoch);Ttw_Write(p+12,stage);Ttw_Write(p+16,bgm);Ttw_Write(p+20,1);request(p);}



int Port_TagCanStart(void){return !configure_pending && state.phase==TT_LOBBY && (!configured_quick||Tt_Humans(&state)==Tt_Capacity(&state)) && Tt_TeamCount(&state,0,1)>0 && Tt_TeamCount(&state,1,1)>0;}



int Port_TagTime(void){return state.time;}

int Port_TagSeat(void){return Tt_Seat(&state,member);}

int Port_TagHumans(void){return Tt_Humans(&state);}

int Port_TagCPU(int seat){return seat>=0&&seat<TT_MAX_PLAYERS?state.players[seat].cpu:0;}

int Port_TagDifficulty(int seat){return seat>=0&&seat<TT_MAX_PLAYERS?state.players[seat].difficulty:0;}

int Port_TagDraftSeat(void){

    int team,seat=Port_TagSeat();

    if(seat<0)return -1;

    if(Tt_Turn(&state,Port_TagTeam(seat))==seat)return seat;

    if(member==state.host)for(team=0;team<2;++team){int turn=Tt_Turn(&state,team);if(turn>=0&&state.players[turn].cpu)return turn;}

    return -1;

}

int Port_TagTurn(void){int s=Port_TagSeat();return s>=0?Tt_Turn(&state,Port_TagTeam(s)):-1;}

int Port_TagHost(void){return member&&member==state.host;}

int Port_TagPlayerChara(int seat){return seat>=0&&seat<TT_MAX_PLAYERS?state.players[seat].chara:-1;}

int Port_TagPlayerCostume(int seat){return seat>=0&&seat<TT_MAX_PLAYERS?state.players[seat].costume:0;}

int Port_TagConfirmed(int seat){return seat>=0&&seat<TT_MAX_PLAYERS?state.players[seat].confirmed:0;}

const char *Port_TagPlayerName(int seat){return seat>=0&&seat<TT_MAX_PLAYERS?state.players[seat].name:"";}

const char *Port_TagCode(void){return room_code;}

const char *Port_TagError(void){return error;}

int Port_TagSendInput(uint32_t frame,const unsigned char pad[TT_PAD_BYTES]){

    unsigned char packet[TT_WIRE_BYTES];

    if(state.phase!=TT_FIGHT||!pad||(ws_mode?!ws_started:client.sock==TT_INVALID))return 0;
    if(solo_cpu_fight()){
        unsigned f;int i,seat=Tt_Seat(&state,member);
        const unsigned char neutral[TT_PAD_BYTES]={0,0,0,0,127,127};
        if(received.epoch!=state.epoch)Tt_InputsReset(&received,state.epoch,Tt_Count(&state));
        if(frame==(unsigned)state.delay&&received.next==0){
            /* Match the Worker's startup padding exactly. */
            for(f=0;f<(unsigned)state.delay;++f)for(i=0;i<Tt_Count(&state);++i)
                if(Tt_InputPut(&received,state.epoch,i,f,neutral)!=TT_OK){fail("Invalid CPU practice startup frame.");return 0;}
            fprintf(stderr,"tag CPU battle: local input scheduling, humans=1 delay=%d\n",state.delay);
        }
        for(i=0;i<Tt_Count(&state);++i)
            if(Tt_InputPut(&received,state.epoch,i,frame,i==seat?pad:neutral)!=TT_OK){fail("Invalid CPU practice input frame.");return 0;}
    }
    if(solo_cpu_fight())return 1;
    if(tag_paced){
        uint32_t next;
        if(tag_input_epoch!=state.epoch){
            tag_input_sent=0;tag_input_epoch=state.epoch;
            tag_pending_buttons=tag_pending_l3=tag_pending_skip=0;tag_delay_adjusted=0;
            tag_delay_margin=1;tag_delay_clean_frames=0;tag_delay_feedback_seen=0;
        }
        if(tag_input_sent&&frame<=tag_input_latest){
            /* A shrinking lead cannot replace an immutable submitted frame.
             * Retain short taps for the next writable frame instead. */
            tag_pending_buttons|=Ttw_Read(pad);tag_pending_l3|=pad[6];tag_pending_skip|=pad[8];return 1;
        }
        next=tag_input_sent?tag_input_latest+1:frame;
        if(!tag_input_sent)tag_input_first=frame;
        /* A larger lead must not leave holes for the server to replace with
         * neutral input. Preserve held controls but never repeat one-shot commands. */
        for(;next<=frame;++next){
            Ttw_Request(packet,TTW_INPUT,state.epoch);Ttw_Write(packet+12,next);
            if(next<frame){memcpy(packet+16,tag_input_pad,TT_PAD_BYTES);packet[22]=packet[24]=0;}
            else{
                memcpy(packet+16,pad,TT_PAD_BYTES);Ttw_Write(packet+16,Ttw_Read(pad)|tag_pending_buttons);
                packet[22]|=tag_pending_l3;packet[24]|=tag_pending_skip;
            }
            request(packet);if(state.phase!=TT_FIGHT)return 0;
        }
        memcpy(tag_input_pad,pad,TT_PAD_BYTES);
        tag_pending_buttons=tag_pending_l3=tag_pending_skip=0;
        tag_input_latest=frame;tag_input_sent=1;return 1;
    }
    Ttw_Request(packet,TTW_INPUT,state.epoch);Ttw_Write(packet+12,frame);memcpy(packet+16,pad,TT_PAD_BYTES);
    request(packet);
    return state.phase==TT_FIGHT;

}

int Port_TagTakeFrame(unsigned char out[TT_MAX_PLAYERS][TT_PAD_BYTES]){
    int ok=state.phase==TT_FIGHT&&received.epoch==state.epoch&&Tt_InputTake(&received,out)==TT_OK;
    tag_update_catchup();return ok;

}

#include <SDL3/SDL.h>

static uint32_t menu_test_offset;

unsigned Port_TagMenuMillis(void){

    if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_CLOCK_TEST"))return menu_test_offset;

    return (uint32_t)SDL_GetTicks()+menu_test_offset;

}

void Port_TagMenuTestAdvance(unsigned ms){if(getenv("BT4_RENDER_TEST"))menu_test_offset+=ms;}

int Port_TagCancelSelect(void){

    if(state.phase!=TT_SELECT || Port_TagSeat()<0)return 0;

    Port_TagCommand(TTW_CANCEL_SELECT,0,0);

    return !error[0];

}

#endif
