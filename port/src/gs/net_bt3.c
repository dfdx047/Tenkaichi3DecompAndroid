/* Community BT3 profile API. All transport runs on this worker, never on the render/game loop. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "net_bt3.h"
#include "net_match.h"

extern const char *Port_FileRoot(void);

static SDL_Mutex *sLock;
static SDL_Condition *sWake;
static SDL_AtomicInt sStarted;
static SDL_AtomicInt sIdentityBusy;
static Bt3Profile sView;
static char sIdentityId[160], sIdentityToken[512];
static int sIdentityReady;
static char sName[33] = "Player", sFlag[3] = "UN";
static int sOpen, sSave, sBoard;

static void lock_make(void) { if (!sLock) { sLock=SDL_CreateMutex(); sWake=SDL_CreateCondition(); } }
static void quote(const char *in,char *out,size_t cap) {
    size_t n=0; if(!cap)return;
    while(*in && n+2<cap) { unsigned char c=(unsigned char)*in++; if(c=='"'||c=='\\')out[n++]='\\'; if(c>=0x20)out[n++]=(char)c; }
    out[n]='\0';
}
static int value_string(const char *json,const char *key,char *out,size_t cap) {
    char find[48]; const char *p; size_t n=0;
    SDL_snprintf(find,sizeof(find),"\"%s\":\"",key); p=SDL_strstr(json,find); if(!p)return 0; p+=SDL_strlen(find);
    while(*p && *p!='"' && n+1<cap) { if(*p=='\\'&&p[1])p++; out[n++]=*p++; }
    if(*p!='"')return 0; out[n]='\0'; return 1;
}
static int value_int(const char *json,const char *key) {
    char find[48]; const char *p; SDL_snprintf(find,sizeof(find),"\"%s\":",key); p=SDL_strstr(json,find);
    return p?SDL_atoi(p+SDL_strlen(find)):0;
}
static int error_kind(int status,const char *json) {
    if(status==409 || SDL_strstr(json,"\"error\":\"version\"") || SDL_strstr(json,"\"error\":\"incompatible_version\"")) return 3;
    if(!json[0])return 1;
    return 2;
}
static void fail(int status,const char *json,int board) {
    int e=error_kind(status,json); SDL_LockMutex(sLock);
    if(board)sView.board_state=-1; else sView.state=-1;
    sView.error=e; sView.http_status=status; sView.error_field[0]='\0';
    value_string(json,"error",sView.error_field,sizeof(sView.error_field));
    if(board) {
        sView.board_http_status=status;
        SDL_strlcpy(sView.board_error_field,sView.error_field,sizeof(sView.board_error_field));
    }
    SDL_UnlockMutex(sLock);
}
static void log_board_diagnostic(void) {
    char line[128];
    SDL_LockMutex(sLock);
    SDL_snprintf(line,sizeof(line),"http=%d error=%s rows=%d me=%s",sView.board_http_status,
                 sView.board_error_field,sView.board_count,sView.board_me?"sim":"não");
    SDL_Log("BT3 leaderboard %s",line);
    SDL_UnlockMutex(sLock);
}
static int post(const char *route,const char *body,char *out,size_t cap,int *status) {
    out[0]='\0'; return Match_PostFriendlyRanked(route,body,out,cap,status);
}
static int valid_hex(const char *s,size_t need) {
    size_t i; if(SDL_strlen(s)!=need)return 0;
    for(i=0;i<need;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')||(s[i]>='A'&&s[i]<='F')))return 0;
    return 1;
}
int Bt3Profile_Identity(const char *name,const char *flag,char *id,size_t ids,char *token,size_t tokens,int *status,char *error,size_t errorSize) {
    char path[640],tmp[648],line[800],body[640],answer[2048],qn[100],qf[8]; FILE *f; int http=0,ok=0;
    if(status)*status=0; if(error&&errorSize)error[0]='\0';
    if(!id||ids<33||!token||tokens<65)return 0;
    while(!SDL_CompareAndSwapAtomicInt(&sIdentityBusy,0,1))SDL_Delay(1);
    id[0]=token[0]='\0';
    if(sIdentityReady) { SDL_strlcpy(id,sIdentityId,ids);SDL_strlcpy(token,sIdentityToken,tokens);ok=1;goto done; }
    SDL_snprintf(path,sizeof(path),"%s/bt3-community-private.txt",Port_FileRoot());
    f=fopen(path,"rb");
    if(f) {
        char savedId[160]={0},savedToken[512]={0};
        if(fscanf(f,"%159s %511s",savedId,savedToken)==2&&valid_hex(savedId,32)&&valid_hex(savedToken,64)) {
            SDL_strlcpy(id,savedId,ids);SDL_strlcpy(token,savedToken,tokens);
            SDL_strlcpy(sIdentityId,id,sizeof(sIdentityId));SDL_strlcpy(sIdentityToken,token,sizeof(sIdentityToken));sIdentityReady=1;ok=1;
        }
        fclose(f);
        if(ok)goto done;
    }
    quote(name&&name[0]?name:"Player",qn,sizeof(qn));
    if(!flag||SDL_strlen(flag)!=2||flag[0]<'A'||flag[0]>'Z'||flag[1]<'A'||flag[1]>'Z')flag="UN";
    quote(flag,qf,sizeof(qf));
    SDL_snprintf(body,sizeof(body),"{\"v\":6001,\"name\":\"%s\",\"flag\":\"%.2s\"}",qn,qf);
    if(!post("ranked/register",body,answer,sizeof(answer),&http)||value_string(answer,"error",line,sizeof(line))||
       !value_string(answer,"id",id,ids)||!value_string(answer,"token",token,tokens)||!valid_hex(id,32)||!valid_hex(token,64)) {
        if(error&&errorSize)value_string(answer,"error",error,errorSize);
        goto done;
    }
    SDL_snprintf(tmp,sizeof(tmp),"%s.tmp",path);
    f=fopen(tmp,"wb");
    if(!f) {
        SDL_strlcpy(sIdentityId,id,sizeof(sIdentityId));SDL_strlcpy(sIdentityToken,token,sizeof(sIdentityToken));sIdentityReady=1;
        if(error&&errorSize)SDL_strlcpy(error,"storage",errorSize); goto done;
    }
#ifndef _WIN32
    (void)chmod(tmp,0600);
#endif
    SDL_snprintf(line,sizeof(line),"%s %s\n",id,token);
    { int wrote=fwrite(line,1,SDL_strlen(line),f)==SDL_strlen(line); int closed=fclose(f)==0;
      if(!wrote||!closed||rename(tmp,path)!=0){
          SDL_strlcpy(sIdentityId,id,sizeof(sIdentityId));SDL_strlcpy(sIdentityToken,token,sizeof(sIdentityToken));sIdentityReady=1;
          if(error&&errorSize)SDL_strlcpy(error,"storage",errorSize);goto done;
      } }
    SDL_strlcpy(sIdentityId,id,sizeof(sIdentityId));SDL_strlcpy(sIdentityToken,token,sizeof(sIdentityToken));sIdentityReady=1;ok=1;
done:
    if(status)*status=http;
    SDL_SetAtomicInt(&sIdentityBusy,0);
    return ok;
}
static int authenticated(const char *id,const char *token,const char *route,const char *extra,char *out,size_t cap,int *status) {
    char body[1800],qid[320],qtok[1040];
    quote(id,qid,sizeof(qid)); quote(token,qtok,sizeof(qtok));
    SDL_snprintf(body,sizeof(body),"{\"v\":6001,\"id\":\"%s\",\"token\":\"%s\"%s}",qid,qtok,extra?extra:"");
    if(!post(route,body,out,cap,status))return 0;
    return !SDL_strstr(out,"\"error\"");
}
static int SDLCALL worker(void *unused) {
    char id[160],token[512],answer[12288],extra[256],qname[100],qflag[8],identityError[48]; int status;
    (void)unused;
    for(;;) {
        int open,save,board; char name[33],flag[3];
        status=0;
        lock_make(); SDL_LockMutex(sLock);
        while(!sOpen&&!sSave&&!sBoard)SDL_WaitCondition(sWake,sLock);
        open=sOpen; save=sSave; board=sBoard; sOpen=sSave=sBoard=0;
        SDL_strlcpy(name,sName,sizeof(name));SDL_strlcpy(flag,sFlag,sizeof(flag));SDL_UnlockMutex(sLock);
        if(!Bt3Profile_Identity(name,flag,id,sizeof(id),token,sizeof(token),&status,identityError,sizeof(identityError))) {
            SDL_LockMutex(sLock);
            if(open||save)sView.state=-1;
            if(board)sView.board_state=-1;
            sView.error=error_kind(status,identityError);sView.http_status=status;
            SDL_strlcpy(sView.error_field,identityError,sizeof(sView.error_field));
            if(board) {
                sView.board_http_status=status;
                sView.board_count=0;
                sView.board_me=0;
                SDL_strlcpy(sView.board_error_field,identityError,sizeof(sView.board_error_field));
            }
            SDL_UnlockMutex(sLock);
            if(board) log_board_diagnostic();
            continue;
        }
        if((open||save)&&id[0]) {
            quote(name,qname,sizeof(qname)); quote(flag,qflag,sizeof(qflag));
            SDL_snprintf(extra,sizeof(extra),",\"name\":\"%s\",\"flag\":\"%.2s\"",qname,qflag);
            if(authenticated(id,token,"ranked/profile",extra,answer,sizeof(answer),&status)) {
                SDL_LockMutex(sLock); SDL_strlcpy(sView.username,name,sizeof(sView.username));SDL_strlcpy(sView.flag,flag,sizeof(sView.flag));
                sView.rating=value_int(answer,"rating");sView.wins=value_int(answer,"wins");sView.losses=value_int(answer,"losses");sView.draws=value_int(answer,"draws");
                sView.position=value_int(answer,"position");value_string(answer,"flag",sView.flag,sizeof(sView.flag));value_string(answer,"rank",sView.rank,sizeof(sView.rank));
                sView.state=2;sView.error=0;sView.http_status=0;sView.error_field[0]='\0';SDL_UnlockMutex(sLock);
            } else fail(status,answer,0);
        } else if(open||save) { SDL_LockMutex(sLock);sView.state=0;SDL_UnlockMutex(sLock); }
        if(board && id[0]) {
            int count=0; const char *p;
            if(authenticated(id,token,"ranked/leaderboard","",answer,sizeof(answer),&status)) {
                int me=value_int(answer,"position");
                const char *meobj=SDL_strstr(answer,"\"me\":{");
                if(meobj) {
                    me=value_int(meobj,"position");
                    SDL_LockMutex(sLock);
                    value_string(meobj,"name",sView.username,sizeof(sView.username));
                    value_string(meobj,"flag",sView.flag,sizeof(sView.flag));
                    value_string(meobj,"rank",sView.rank,sizeof(sView.rank));
                    sView.rating=value_int(meobj,"rating");sView.wins=value_int(meobj,"wins");
                    sView.losses=value_int(meobj,"losses");sView.draws=value_int(meobj,"draws");
                    SDL_UnlockMutex(sLock);
                }
                p=SDL_strstr(answer,"\"rows\":["); if(p)p=SDL_strchr(p,'[')+1;
                SDL_LockMutex(sLock);sView.position=me;sView.board_count=0;
                while(p && count<10 && (p=SDL_strchr(p,'{'))!=NULL) {
                    const char *end=SDL_strchr(p,'}'); char row[512]; size_t n; if(!end)break; n=(size_t)(end-p);if(n>=sizeof(row))n=sizeof(row)-1;
                    SDL_memcpy(row,p,n);row[n]='\0';
                    value_string(row,"flag",sView.board_flag[count],sizeof(sView.board_flag[count]));
                    if(!sView.board_flag[count][0])value_string(row,"country",sView.board_flag[count],sizeof(sView.board_flag[count]));
                    value_string(row,"name",sView.board_name[count],sizeof(sView.board_name[count]));
                    value_string(row,"rank",sView.board_rank[count],sizeof(sView.board_rank[count]));
                    sView.board_rating[count]=value_int(row,"rating");sView.board_position[count]=value_int(row,"position");
                    sView.board_wins[count]=value_int(row,"wins");sView.board_losses[count]=value_int(row,"losses");count++;p=end+1;
                }
                sView.board_count=count;sView.board_state=2;sView.error=0;sView.http_status=0;sView.error_field[0]='\0';
                sView.board_me=meobj!=NULL;sView.board_http_status=status;sView.board_error_field[0]='\0';
                SDL_UnlockMutex(sLock);
            } else fail(status,answer,1);
        } else if(board) { SDL_LockMutex(sLock);sView.board_count=0;sView.board_state=2;sView.position=0;SDL_UnlockMutex(sLock); }
        if(board) {
            log_board_diagnostic();
        }
    }
    return 0;
}
static void start(void) {
    lock_make(); if(SDL_CompareAndSwapAtomicInt(&sStarted,0,1)) { SDL_Thread *t=SDL_CreateThread(worker,"bt3-community",NULL);if(t)SDL_DetachThread(t);else SDL_SetAtomicInt(&sStarted,0); }
}
void Bt3Profile_Open(void) { start();lock_make();SDL_LockMutex(sLock);sOpen=1;sView.state=1;sView.error=0;sView.http_status=0;sView.error_field[0]='\0';SDL_SignalCondition(sWake);SDL_UnlockMutex(sLock); }
void Bt3Profile_Save(const char *name,const char *flag) { start();lock_make();SDL_LockMutex(sLock);SDL_strlcpy(sName,name&&name[0]?name:"Player",sizeof(sName));SDL_strlcpy(sFlag,flag&&flag[0]?flag:"UN",sizeof(sFlag));sSave=1;sView.state=1;sView.error=0;sView.http_status=0;sView.error_field[0]='\0';SDL_SignalCondition(sWake);SDL_UnlockMutex(sLock); }
void Bt3Profile_Board(void) { start();lock_make();SDL_LockMutex(sLock);sBoard=1;sView.board_state=1;sView.board_count=0;sView.board_me=0;sView.board_http_status=0;sView.board_error_field[0]='\0';sView.error=0;sView.http_status=0;sView.error_field[0]='\0';SDL_SignalCondition(sWake);SDL_UnlockMutex(sLock); }
void Bt3Profile_Poll(Bt3Profile *out) { if(!out)return;lock_make();SDL_LockMutex(sLock);*out=sView;SDL_UnlockMutex(sLock); }
