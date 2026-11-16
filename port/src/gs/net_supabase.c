#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "net_match.h"
#include "net_supabase.h"

#ifndef BT3_SUPABASE_URL
#define BT3_SUPABASE_URL ""
#endif
#ifndef BT3_SUPABASE_PUBLISHABLE_KEY
#define BT3_SUPABASE_PUBLISHABLE_KEY ""
#endif

extern const char *Port_FileRoot(void);
static SDL_Mutex *sLock;
static SDL_Condition *sWake;
static SDL_AtomicInt sStarted;
static DrProfile sProfile;
static char sAccess[2048], sRefresh[512], sUser[40];
static int sWantOpen, sWantSave, sWantBoard;
static char sSaveName[17], sSaveCountry[3];

static void lock_make(void) {
    if (!sLock) { sLock = SDL_CreateMutex(); sWake = SDL_CreateCondition(); }
}

static int json_value(const char *json, const char *key, char *out, size_t size) {
    char find[48];
    const char *p, *e;
    SDL_snprintf(find, sizeof(find), "\"%s\":\"", key);
    p = SDL_strstr(json, find);
    if (!p) return 0;
    p += SDL_strlen(find);
    e = SDL_strchr(p, '"');
    if (!e || (size_t)(e - p) >= size) return 0;
    SDL_memcpy(out, p, (size_t)(e - p)); out[e - p] = '\0';
    return 1;
}

static int json_number(const char *json, const char *key) {
    char find[48]; const char *p;
    SDL_snprintf(find, sizeof(find), "\"%s\":", key);
    p = SDL_strstr(json, find);
    return p ? SDL_atoi(p + SDL_strlen(find)) : 0;
}

static void json_quote(const char *in, char *out, size_t size) {
    size_t n = 0;
    if (size == 0) return;
    for (; *in && n + 2 < size; in++) {
        if (*in == '"' || *in == '\\') out[n++] = '\\';
        if ((unsigned char)*in >= 0x20) out[n++] = *in;
    }
    out[n] = '\0';
}

static int request(const char *path, const char *body, int auth, char *out, size_t cap) {
    char url[640], api[320], bearer[2200];
    const char *headers[4];
    int ok;
    if (!BT3_SUPABASE_URL[0] || !BT3_SUPABASE_PUBLISHABLE_KEY[0]) return 0;
    SDL_snprintf(url, sizeof(url), "%s%s", BT3_SUPABASE_URL, path);
    SDL_snprintf(api, sizeof(api), "apikey: %s", BT3_SUPABASE_PUBLISHABLE_KEY);
    headers[0] = api;
    if (auth) {
        SDL_snprintf(bearer, sizeof(bearer), "Authorization: Bearer %s", sAccess);
        headers[1] = bearer; headers[2] = "Prefer: resolution=merge-duplicates,return=representation"; headers[3] = NULL;
    } else { headers[1] = NULL; headers[2] = NULL; headers[3] = NULL; }
    ok=Match_HttpRequest(url, body, headers, out, cap);
    if (!ok || SDL_strstr(out,"\"code\"") || SDL_strstr(out,"\"message\"") || SDL_strstr(out,"\"error\"")) return 0;
    return 1;
}

static int session_load(void) {
    char path[512], line[2200]; FILE *f;
    SDL_snprintf(path, sizeof(path), "%s/bt3-supabase-session.txt", Port_FileRoot());
    f = fopen(path, "rb"); if (!f) return 0;
    if (!fgets(line, sizeof(line), f) || sscanf(line, "%39s %2047s %511s", sUser, sAccess, sRefresh) != 3) {
        fclose(f); return 0;
    }
    fclose(f); return 1;
}

static void session_save(void) {
    char path[512], tmp[520]; FILE *f;
    SDL_snprintf(path, sizeof(path), "%s/bt3-supabase-session.txt", Port_FileRoot());
    SDL_snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "wb"); if (!f) return;
    fprintf(f, "%s %s %s\n", sUser, sAccess, sRefresh); fclose(f); rename(tmp, path);
}

static int auth_session(char *out, size_t cap) {
    if (session_load()) {
        char body[700], escaped[600];
        json_quote(sRefresh,escaped,sizeof(escaped));
        SDL_snprintf(body,sizeof(body),"{\"refresh_token\":\"%s\"}",escaped);
        if (request("/auth/v1/token?grant_type=refresh_token",body,0,out,cap) &&
            json_value(out,"access_token",sAccess,sizeof(sAccess)) &&
            json_value(out,"refresh_token",sRefresh,sizeof(sRefresh))) { session_save(); return 1; }
        sAccess[0]=sRefresh[0]=sUser[0]='\0';
        return 0;
    }
    if (!request("/auth/v1/signup", "{\"data\":{}}", 0, out, cap) ||
        !json_value(out, "access_token", sAccess, sizeof(sAccess)) ||
        !json_value(out, "refresh_token", sRefresh, sizeof(sRefresh))) return 0;
    {
        char *user = SDL_strstr(out, "\"user\":{");
        if (!user || !json_value(user, "id", sUser, sizeof(sUser))) return 0;
    }
    session_save(); return 1;
}

static int profile_get(char *out, size_t cap) {
    char path[256];
    SDL_snprintf(path, sizeof(path), "/rest/v1/profiles?select=username,country_code&user_id=eq.%s", sUser);
    return request(path, NULL, 1, out, cap);
}

static int profile_put(const char *name, const char *country, char *out, size_t cap) {
    char body[128], qname[48], path[256];
    json_quote(name, qname, sizeof(qname));
    SDL_snprintf(path, sizeof(path), "/rest/v1/profiles?on_conflict=user_id");
    SDL_snprintf(body, sizeof(body), "{\"user_id\":\"%s\",\"username\":\"%s\",\"country_code\":\"%.2s\"}", sUser, qname, country);
    return request(path, body, 1, out, cap);
}

static int SDLCALL profile_worker(void *unused) {
    char answer[8192], name[17], country[3];
    (void)unused;
    for (;;) {
        int open, save, board;
        lock_make(); SDL_LockMutex(sLock);
        while (!sWantOpen && !sWantSave && !sWantBoard) SDL_WaitCondition(sWake, sLock);
        open=sWantOpen; save=sWantSave; board=sWantBoard;
        sWantOpen=sWantSave=sWantBoard=0;
        SDL_strlcpy(name, sSaveName, sizeof(name)); SDL_strlcpy(country, sSaveCountry, sizeof(country));
        SDL_UnlockMutex(sLock);

        if (!auth_session(answer, sizeof(answer))) {
            SDL_LockMutex(sLock); sProfile.state=-1; if(board)sProfile.board_state=-1; SDL_strlcpy(sProfile.error, "Supabase auth/configuration failed", sizeof(sProfile.error)); SDL_UnlockMutex(sLock);
            continue;
        }
        if (open || save) {
            int ok = save ? profile_put(name, country, answer, sizeof(answer)) : profile_get(answer, sizeof(answer));
            if (ok && (save || SDL_strstr(answer, "\"username\"") != NULL)) {
                if (!save) {
                    char *row=SDL_strchr(answer, '{');
                    if (row) { json_value(row,"username",name,sizeof(name)); json_value(row,"country_code",country,sizeof(country)); }
                }
                SDL_LockMutex(sLock); SDL_strlcpy(sProfile.username,name,sizeof(sProfile.username)); SDL_strlcpy(sProfile.country,country,sizeof(sProfile.country)); sProfile.state=2; sProfile.error[0]='\0'; SDL_UnlockMutex(sLock);
            } else {
                SDL_LockMutex(sLock); sProfile.state=-1; SDL_strlcpy(sProfile.error, "Profile request failed: check profiles table and RLS", sizeof(sProfile.error)); SDL_UnlockMutex(sLock);
            }
        }
        if (board) {
            int count=0; const char *p;
            int ok=request("/rest/v1/world_top10?select=position,country_code,username,elo&order=position.asc&limit=10",NULL,1,answer,sizeof(answer));
            if (ok) for (p=answer; count<10 && (p=SDL_strstr(p,"\"position\":"))!=NULL; p++) {
                DrProfile row; SDL_memset(&row,0,sizeof(row));
                row.position=json_number(p,"position"); json_value(p,"country_code",row.country,sizeof(row.country));
                json_value(p,"username",row.username,sizeof(row.username)); row.board_elo[0]=json_number(p,"elo");
                SDL_LockMutex(sLock); SDL_strlcpy(sProfile.board_country[count],row.country,3); SDL_strlcpy(sProfile.board_name[count],row.username,17); sProfile.board_elo[count]=row.board_elo[0]; SDL_UnlockMutex(sLock); count++;
            }
            SDL_LockMutex(sLock); sProfile.board_count=count; sProfile.board_state=ok?2:-1; sProfile.position=0; SDL_UnlockMutex(sLock);
        }
    }
    return 0;
}

static void start(void) {
    lock_make();
    if (SDL_CompareAndSwapAtomicInt(&sStarted,0,1)) {
        SDL_Thread *t=SDL_CreateThread(profile_worker,"bt3-supabase",NULL);
        if (t) SDL_DetachThread(t); else SDL_SetAtomicInt(&sStarted,0);
    }
}

void DrProfile_Open(void) {
    start(); lock_make(); SDL_LockMutex(sLock); sWantOpen=1; sProfile.state=1; SDL_SignalCondition(sWake); SDL_UnlockMutex(sLock);
}
void DrProfile_Save(const char *username, const char *country) {
    start(); lock_make(); SDL_LockMutex(sLock); SDL_strlcpy(sSaveName,username,sizeof(sSaveName)); SDL_strlcpy(sSaveCountry,country,sizeof(sSaveCountry)); sWantSave=1; sProfile.state=1; SDL_SignalCondition(sWake); SDL_UnlockMutex(sLock);
}
void DrProfile_Board(void) {
    start(); lock_make(); SDL_LockMutex(sLock); sWantBoard=1; sProfile.board_state=1; sProfile.board_count=0; sProfile.position=0; SDL_SignalCondition(sWake); SDL_UnlockMutex(sLock);
}
void DrProfile_Poll(DrProfile *out) {
    if (!out) return;
    lock_make(); SDL_LockMutex(sLock); *out=sProfile; SDL_UnlockMutex(sLock);
}
void DrProfile_Country(char *out, size_t size) {
    if (!out || !size) return;
    lock_make(); SDL_LockMutex(sLock);
    if (sProfile.country[0] >= 'A' && sProfile.country[0] <= 'Z' && sProfile.country[1] >= 'A' && sProfile.country[1] <= 'Z' && sProfile.country[2] == '\0')
        SDL_strlcpy(out,sProfile.country,size);
    else SDL_strlcpy(out,"UN",size);
    SDL_UnlockMutex(sLock);
}
