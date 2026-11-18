/*
 * Dragon Net: the online play of Dragon Rage (Android) and DBT-NX (Switch) on a service of their own
 * (port/dragonnet: DBT-NX's Worker, MIT, by Unknown, updated to take several versions), apart from the PC port's.
 * A new release of the PC port does not stop it: games meet when their netcode AND their simulation are the same
 * (DrNet_WireVersion), not their release number. Against the PC the PC port's service goes on being used (mode 1).
 *
 * Ranked uses the signed Amistoso service and a single identity shared with net_bt3.c; legacy Worker identities are
 * ignored. Dragon Net still supplies presence. Ranked requests run on this worker; the window reads its state
 * (DrRanked_Poll, ...).
 *
 * A fight's result (DrNet_BattleFinished, from BattleResult_Finish) is written to dragonnet-result.txt before it is
 * sent, and sent again until the service has settled it (the app may be closed in between). Leaving a ranked
 * match before its fight is over (the pause menu, closing the game) is a loss for the one who leaves, as on the
 * Switch; the other side going away gives nobody points.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_dragon.h"
#include "net_match.h"
#include "net_bt3.h"

extern int Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int value);
extern void Port_SettingsWrite(void);
extern const char *Port_FileRoot(void);
extern int Port_NetActive(void);
extern int gPortResim;
extern volatile int gPortInBackground;

/* ---- the mode and the version ---- */

static int sNetMode = -1;

int DrNet_Mode(void) {
    if (sNetMode < 0) {
        const char *e = SDL_getenv("BT3_NET_MODE");
        sNetMode = e != NULL ? (SDL_atoi(e) != 0) : (Port_Setting("net_mode", 0) != 0);
    }
    return sNetMode;
}

void DrNet_SetMode(int mode) {
    sNetMode = mode != 0;
    Port_SettingSave("net_mode", sNetMode);
    Port_SettingsWrite();
}

int DrNet_WireVersion(int netVersion) {
    return DrNet_Mode() == 0 ? netVersion * 1000 + DRNET_SIM : netVersion;
}

/* the `v` of Dragon Net's own requests (Ranked, chat, presence), whatever the mode */
static int wire(void) {
    extern int Port_NetVersion(void); /* net.c */
    return Port_NetVersion() * 1000 + DRNET_SIM;
}

/* ---- small things ---- */

static void path_of(const char *name, char *out, size_t size) {
    SDL_snprintf(out, size, "%s/%s", Port_FileRoot(), name);
}

static int read_text(const char *name, char *out, size_t size) {
    char path[600];
    FILE *f;
    size_t n;
    path_of(name, path, sizeof(path));
    out[0] = '\0';
    if ((f = fopen(path, "rb")) == NULL) {
        return 0;
    }
    n = fread(out, 1, size - 1, f);
    out[n] = '\0';
    fclose(f);
    return 1;
}

/* written whole or not at all (a temporary file, then renamed) */
static int write_text(const char *name, const char *text) {
    char path[600], tmp[620];
    FILE *f;
    int ok;
    path_of(name, path, sizeof(path));
    SDL_snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if ((f = fopen(tmp, "wb")) == NULL) {
        return 0;
    }
    ok = fwrite(text, 1, strlen(text), f) == strlen(text);
    ok = fclose(f) == 0 && ok;
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    if (!ok) {
        remove(tmp);
    }
    return ok;
}

static void remove_file(const char *name) {
    char path[600];
    path_of(name, path, sizeof(path));
    remove(path);
}

static int is_hex(const char *s, size_t n) {
    size_t i;
    if (strlen(s) != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static void random_hex(char *out, int n) {
    static const char d[] = "0123456789abcdef";
    unsigned char b[64];
    FILE *f = fopen("/dev/urandom", "rb");
    int i, got = f != NULL ? (int)fread(b, 1, (size_t)(n / 2), f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    for (i = got; i < n / 2; i++) {
        b[i] = (unsigned char)SDL_rand(256);
    }
    for (i = 0; i < n / 2; i++) {
        out[2 * i] = d[b[i] >> 4];
        out[2 * i + 1] = d[b[i] & 15];
    }
    out[n] = '\0';
}

/* the service's names: letters, digits, space and _.- (as it cleans them), at most 19 */
static void clean_name(const char *in, char *out, size_t size) {
    size_t k = 0;
    for (; in != NULL && *in != '\0' && k + 1 < size && k < 19; in++) {
        char c = *in;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '.' || c == '-') {
            out[k++] = c;
        }
    }
    out[k] = '\0';
    if (k == 0) {
        SDL_strlcpy(out, "Player", size);
    }
}

static int text_value(const char *json, const char *key, char *out, size_t size) {
    char find[48];
    const char *s;
    size_t k = 0;
    SDL_snprintf(find, sizeof(find), "\"%s\":", key);
    out[0] = '\0';
    if ((s = strstr(json, find)) == NULL) {
        return 0;
    }
    s += strlen(find);
    while (*s == ' ') {
        s++;
    }
    if (*s++ != '"') {
        return 0;
    }
    while (*s != '\0' && *s != '"' && k + 1 < size) {
        if (*s == '\\') {
            return 0;
        }
        out[k++] = *s++;
    }
    out[k] = '\0';
    return *s == '"';
}

static int number_value(const char *json, const char *key) {
    char find[48];
    const char *s;
    SDL_snprintf(find, sizeof(find), "\"%s\":", key);
    s = strstr(json, find);
    return s != NULL ? SDL_atoi(s + strlen(find)) : 0;
}

static void hex_encode(const char *in, char *out, size_t size) {
    static const char d[] = "0123456789abcdef";
    size_t k = 0;
    for (; *in != '\0' && k + 3 < size; in++) {
        unsigned char c = (unsigned char)*in;
        out[k++] = d[c >> 4];
        out[k++] = d[c & 15];
    }
    out[k] = '\0';
}

static void hex_decode(const char *in, size_t len, char *out, size_t size) {
    size_t k = 0, i;
    for (i = 0; i + 1 < len + 1 && in[i] != '\0' && in[i + 1] != '\0' && k + 1 < size; i += 2) {
        int hi = in[i] <= '9' ? in[i] - '0' : in[i] - 'a' + 10, lo = in[i + 1] <= '9' ? in[i + 1] - '0' : in[i + 1] - 'a' + 10;
        unsigned char c = (unsigned char)(hi << 4 | lo);
        out[k++] = (char)(c < 32 ? ' ' : c);
    }
    out[k] = '\0';
}

/* ---- the shared state (one lock) ---- */

static SDL_Mutex *sLock;
static SDL_Mutex *sOutboxLock; /* result queue file */
static SDL_AtomicInt sStarted;
static SDL_Condition *sWake;
static char sFinishDiag[192];
static char sLastResultDiag[160], sLastSeriesDiag[128];
static SDL_AtomicInt sRankedWinnerDecided;
static void lock_make(void);

static void finish_diag(const char *text) {
    lock_make();
    SDL_LockMutex(sLock);
    SDL_strlcpy(sFinishDiag, text, sizeof(sFinishDiag));
    SDL_UnlockMutex(sLock);
    fprintf(stderr, "dragonnet: ranked finish: %s\n", text);
}

void DrNet_FinishDiagCopy(char *out, size_t size) {
    if (out == NULL || size == 0) return;
    lock_make();
    SDL_LockMutex(sLock);
    SDL_strlcpy(out, sFinishDiag, size);
    SDL_UnlockMutex(sLock);
}

static void lock_make(void) {
    static SDL_AtomicInt made;
    if (SDL_GetAtomicInt(&made) == 2) {
        return;
    }
    if (SDL_CompareAndSwapAtomicInt(&made, 0, 1)) {
        sLock = SDL_CreateMutex();
        sOutboxLock = SDL_CreateMutex();
        sWake = SDL_CreateCondition();
        SDL_SetAtomicInt(&made, 2);
    }
    while (SDL_GetAtomicInt(&made) != 2) {
        SDL_Delay(1);
    }
}

static DrRanked sView;
static char sName[20] = "Player";
static char sId[33], sToken[65]; /* the device identity (the thread's) */
static int sRefresh, sWantSearch, sWantCancel, sWantBoard;
static unsigned sSearchGen;
static char sCancelMatch[33], sRoomPending[8], sHandedOver[33];
static char sLifeMatch[33], sStartSent[33]; /* flow:2: the match the lifecycle calls are for */
static char sConnectionDone[33];
static unsigned sRankedRound = 1;
static int sRankedStarted;
static char sLastQueueMatch[33], sPreviousQueueMatch[33];
static int sSeriesActive, sSeriesDirty, sSeriesChoice;
static unsigned sSeriesGap;
static DrRankedSeries sSeriesView;
static int sLifeReady, sLifeStarted, sLifeTries;
static int sLifeConnect;
/* flow:2 rematch: what the game asked for (act: 0 none, 1 status, 2 accept, 3 leave) and what the service answered
   (state: 0 none, 1 confirming, 2 choice, 3 waiting, 4 declined, 5 a new match was saved to dragonnet-next.txt). */
static int sFlowAct, sFlowNow, sFlowTries, sFlowState;
static char sFlowMatch[33];
static Uint64 sFlowAt;
static int sFlowMode;

static int sChatOpen, sChatState;
static unsigned sChatAfter;
static DrChat sChat;
static char sChatOut[4][200]; /* messages waiting to be sent (hex comes later) */
static int sChatOutN;

static int sPresenceOn, sPresenceLeave, sOnline = -1;
static Uint64 sSearchAt; /* when the search under way began (the queue is asked less often as it goes on) */
static Uint64 sPairedAt, sCodePollAt;
static int sCodePollCount;
static Uint64 sPresenceAt;

static int ranked_route(const char *what) {
    if (SDL_strcmp(what, "queue") == 0 || SDL_strcmp(what, "match") == 0) return 0;
    if (SDL_strcmp(what, "room") == 0) return 1;
    if (SDL_strcmp(what, "lifecycle") == 0) return 2;
    return -1;
}

static void ranked_diagnostic(const char *what, int status, const char *json) {
    char error[48] = "";
    int route = ranked_route(what);
    text_value(json ? json : "", "error", error, sizeof(error));
    SDL_LockMutex(sLock);
    sView.http_status = status;
    SDL_strlcpy(sView.error_field, error, sizeof(sView.error_field));
    if (route >= 0) {
        sView.route_http[route] = status;
        SDL_strlcpy(sView.route_error[route], error, sizeof(sView.route_error[route]));
    }
    SDL_UnlockMutex(sLock);
}

static void ranked_response_diagnostic(const char *what,int status,const char *json) {
    char safe[121]; size_t n=0; const char *p=json?json:"";
    while(*p && n<120) {
        if(strncmp(p,"\"token\":\"",9)==0) {
            const char *end=strchr(p+9,'\"');
            const char *red="\"token\":\"[redacted]\"";
            while(*red && n<120) safe[n++]=*red++;
            p=end?end+1:p+strlen(p); continue;
        }
        safe[n++]=*p++;
    }
    safe[n]=0;
    SDL_LockMutex(sLock);
    if(SDL_strcmp(what,"result")==0) SDL_snprintf(sLastResultDiag,sizeof(sLastResultDiag),"http=%d %.120s",status,safe);
    else SDL_snprintf(sLastSeriesDiag,sizeof(sLastSeriesDiag),"http=%d %.90s",status,safe);
    SDL_UnlockMutex(sLock);
}

void DrRanked_DiagnosticCopy(char *out,size_t size) {
    if(!out||!size)return;
    lock_make();
    SDL_LockMutex(sLock);
    SDL_snprintf(out,size,"result %s | series %s | gap=%u round=%u",sLastResultDiag[0]?sLastResultDiag:"http=0 -",sLastSeriesDiag[0]?sLastSeriesDiag:"http=0 -",sSeriesGap,sRankedRound);
    SDL_UnlockMutex(sLock);
}

static void message(int msg, int state) {
    SDL_LockMutex(sLock);
    sView.msg = msg;
    if (state != 99) {
        sView.state = state;
    }
    SDL_UnlockMutex(sLock);
}

/* ---- the service ---- */

/* A Ranked request with the shared Amistoso identity: "ranked/<what>". */
static int post_ranked(const char *what, const char *extra, char *answer, size_t size) {
    char body[1100], path[48];
    int status = 0;
    SDL_snprintf(path, sizeof(path), "ranked/%s", what);
    SDL_snprintf(body, sizeof(body), "{\"v\":6001,\"id\":\"%s\",\"token\":\"%s\"%s}", sId, sToken, extra != NULL ? extra : "");
    answer[0]='\0';
    if (!Match_PostFriendlyRanked(path, body, answer, size, &status)) {
        if(SDL_strcmp(what,"result")==0 || SDL_strcmp(what,"series")==0) ranked_response_diagnostic(what,status,answer);
        if (SDL_strcmp(what, "profile") == 0 || ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
        return 0;
    }
    if(SDL_strcmp(what,"result")==0 || SDL_strcmp(what,"series")==0) ranked_response_diagnostic(what,status,answer);
    if (SDL_strcmp(what, "profile") == 0 || ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
    if (strstr(answer, "\"error\":\"limit\"") != NULL) {
        message(DRMSG_LIMIT, 99); /* (a search goes on, without asking, until the pause is over: net_match.c) */
        return 0;
    }
    if (strstr(answer, "\"error\":\"version\"") != NULL) {
        message(DRMSG_VERSION, -1);
        return 0;
    }
    return strstr(answer, "\"error\"") == NULL;
}

/* Queue, room and cancel use the same signed Amistoso transport and v 6001 as the other Ranked routes. */
static int post_ranked_search(const char *what, const char *extra, char *answer, size_t size) {
    char body[1100], path[48];
    int status = 0;
    SDL_snprintf(path, sizeof(path), "ranked/%s", what);
    SDL_snprintf(body, sizeof(body), "{\"v\":6001,\"id\":\"%s\",\"token\":\"%s\"%s}", sId, sToken,
                 extra != NULL ? extra : "");
    answer[0]='\0';
    if (!Match_PostFriendlyRanked(path, body, answer, size, &status)) {
        if (ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
        message(DRMSG_NO_CONNECTION, 99);
        return 0;
    }
    if (ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
    if (strstr(answer, "\"error\":\"version\"") != NULL || strstr(answer, "\"http\":409") != NULL) {
        if (SDL_strcmp(what, "queue") == 0) {
            SDL_LockMutex(sLock);
            sWantSearch = sView.searching = 0;
            sView.state = -1;
            SDL_UnlockMutex(sLock);
        }
        message(DRMSG_VERSION, -1);
        return 0;
    }
    return strstr(answer, "\"error\"") == NULL;
}

/* Resolve the single Amistoso identity. The old dragonnet-device.txt Worker credentials are intentionally ignored. */
static int identity(void) {
    char name[20], error[48], flag[3] = "UN";
    char id[160], token[512];
    int status = 0;
    /* after a failed registration the next try waits (15 s, doubling to 5 minutes): a service out of its daily
       requests answered 429 and the worker asked again every 70 ms while the window kept refreshing */
    static Uint64 retryAt, retryWait;
    if (sId[0] != '\0') {
        return 1;
    }
    if (retryAt != 0 && SDL_GetTicks() < retryAt) {
        return 0;
    }
    SDL_LockMutex(sLock);
    SDL_strlcpy(name, sName, sizeof(name));
    SDL_UnlockMutex(sLock);
    if (!Bt3Profile_Identity(name, flag, id, sizeof(id), token, sizeof(token), &status, error, sizeof(error))) {
        sId[0] = sToken[0] = '\0';
        retryWait = retryWait == 0 ? 15000 : retryWait * 2 > 300000 ? 300000 : retryWait * 2;
        retryAt = SDL_GetTicks() + retryWait;
        SDL_LockMutex(sLock);
        sView.http_status = status;
        SDL_strlcpy(sView.error_field, error, sizeof(sView.error_field));
        SDL_UnlockMutex(sLock);
        message(error[0] && SDL_strcmp(error, "version") == 0 ? DRMSG_VERSION : error[0] && SDL_strcmp(error, "busy") == 0 ? DRMSG_LIMIT : DRMSG_REGISTER_FAILED, -1);
        return 0;
    }
    SDL_strlcpy(sId, id, sizeof(sId));
    SDL_strlcpy(sToken, token, sizeof(sToken));
    return 1;
}

/* ---- the results waiting to be sent: dragonnet-result.txt, one line "match score digest round" ---- */

static int outbox_first_nl(char *match, int *score, char *digest, unsigned *round) {
    char text[1024];
    if (!read_text("dragonnet-result.txt", text, sizeof(text)) || text[0] == '\0') {
        return 0;
    }
    return sscanf(text, "%32s %d %8s %u", match, score, digest, round) == 4 && is_hex(match, 32) && is_hex(digest, 8);
}

#define OUTBOX_MAX 1024

/* adds one match to the end of the queue (no overwrite) */
static int outbox_push_nl(const char *line) {
    char all[OUTBOX_MAX], id[33], old[33], digest[9];
    int score; unsigned round, oldround; char *p;
    size_t n;
    int nl;
    if (sscanf(line, "%32s %d %8s %u", id, &score, digest, &round) != 4) return 0;
    all[0] = '\0';
    read_text("dragonnet-result.txt", all, sizeof(all));
    for (p = all; *p;) { oldround=0; if (sscanf(p, "%32s %d %8s %u", old, &score, digest, &oldround)==4 && strcmp(old,id)==0 && oldround==round) return 1; p=strchr(p,'\n'); if(!p) break; ++p; }
    n = strlen(all);
    nl = (n > 0 && all[n - 1] != '\n');
    if (n + nl + strlen(line) + 1 > sizeof(all)) {
        fprintf(stderr, "dragonnet: the result queue is full\n");
        return 0;
    }
    if (nl) {
        all[n++] = '\n';
    }
    SDL_strlcpy(all + n, line, sizeof(all) - n);
    return write_text("dragonnet-result.txt", all);
}

/* settles the first match of the queue only */
static void outbox_done_nl(void) {
    char all[OUTBOX_MAX];
    const char *rest;
    if (!read_text("dragonnet-result.txt", all, sizeof(all))) {
        return;
    }
    rest = strchr(all, '\n');
    if (rest == NULL || rest[1] == '\0') {
        remove_file("dragonnet-result.txt");
        return;
    }
    write_text("dragonnet-result.txt", rest + 1);
}

/* the queue file is shared by the game thread (push) and the net thread
   (first/done): one lock around each read-modify-write */
static int outbox_first(char *match, int *score, char *digest, unsigned *round) {
    int r;
    SDL_LockMutex(sOutboxLock);
    r = outbox_first_nl(match, score, digest, round);
    SDL_UnlockMutex(sOutboxLock);
    return r;
}

static int outbox_push(const char *line) {
    int r;
    SDL_LockMutex(sOutboxLock);
    r = outbox_push_nl(line);
    SDL_UnlockMutex(sOutboxLock);
    return r;
}

static void outbox_done(void) {
    SDL_LockMutex(sOutboxLock);
    outbox_done_nl();
    SDL_UnlockMutex(sOutboxLock);
}

/* the ranked match being played: dragonnet-session.txt, "match role mode sent" */
static int session_read(char *match, int *role, int *mode, int *sent) {
    char text[160];
    unsigned round = 1;
    int started = 0;
    *role = *mode = *sent = 0;
    sRankedStarted = 0;
    if (!read_text("dragonnet-session.txt", text, sizeof(text))) {
        return 0;
    }
    if (sscanf(text, "%32s %d %d %d %u %d", match, role, mode, sent, &round, &started) < 3) return 0;
    sRankedRound = round ? round : 1;
    sRankedStarted = started != 0;
    return is_hex(match, 32) && (*role == 1 || *role == 2);
}

static void session_write(const char *match, int role, int mode, int sent) {
    char text[160];
    SDL_snprintf(text, sizeof(text), "%s %d %d %d %u %d\n", match, role, mode, sent, sRankedRound, sRankedStarted);
    write_text("dragonnet-session.txt", text);
}

/* ---- the thread ---- */

static void update_match(const char *answer, unsigned gen) {
    char state[24], assigned[33];
    text_value(answer, "state", state, sizeof(state));
    text_value(answer, "match", assigned, sizeof(assigned));
    SDL_LockMutex(sLock);
    if (gen != sSearchGen || !sWantSearch) { /* cancelled meanwhile: that search is given up (not a handed-over one) */
        int keep = strcmp(assigned, sHandedOver) == 0;
        SDL_UnlockMutex(sLock);
        if (!keep && is_hex(assigned, 32)) {
            char extra[64], ignored[512];
            SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", assigned);
            post_ranked_search("cancel", extra, ignored, sizeof(ignored));
        }
        return;
    }
    SDL_strlcpy(sView.match, assigned, sizeof(sView.match));
    text_value(answer, "code", sView.code, sizeof(sView.code));
    text_value(answer, "peer", sView.peer, sizeof(sView.peer));
    sView.role = number_value(answer, "role");
    sView.flow = 2;
    sView.state = strcmp(state, "paired") == 0 ? 3 : strcmp(state, "waiting") == 0 ? 2 : 0;
    if (sView.state == 3 && sView.role == 2 && sView.code[0] == '\0') {
        if (sPairedAt == 0) sPairedAt = SDL_GetTicks();
    } else {
        sPairedAt = sCodePollAt = 0;
        sCodePollCount = 0;
    }
    sView.code_poll_count = sCodePollCount;
    if (sView.state == 0) {
        sView.searching = sWantSearch = 0;
        sView.msg = strcmp(state, "final") == 0 ? DRMSG_CONFIRMED : DRMSG_ENDED_NO_POINTS;
    } else {
        sView.msg = sView.state == 3 ? DRMSG_FOUND : DRMSG_SEARCHING_LEVEL;
    }
    SDL_UnlockMutex(sLock);
}

static void do_result(int *refresh) {
    char match[33], digest[9], extra[160], answer[2048], state[24];
    int score;
    unsigned round;
    if (!outbox_first(match, &score, digest, &round)) {
        return;
    }
    if (score < 0) {
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"round\":%u", match, round);
    } else {
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"score\":%d,\"digest\":\"%s\",\"round\":%u", match, score, digest, round);
    }
    if (post_ranked(score < 0 ? "forfeit" : "result", extra, answer, sizeof(answer))) {
        text_value(answer, "state", state, sizeof(state));
        fprintf(stderr, "dragonnet: ranked result: %s\n", state);
        if (strcmp(state, "final") == 0 || strcmp(state, "disputed") == 0 || strcmp(state, "cancelled") == 0 || strcmp(state, "expired") == 0) {
            outbox_done();
            *refresh = 1;
            message(strcmp(state, "final") == 0 ? DRMSG_CONFIRMED : strcmp(state, "disputed") == 0 ? DRMSG_DISPUTED : DRMSG_VOID, 0);
        } else {
            message(DRMSG_WAIT_RIVAL, 99);
        }
    } else if (strstr(answer, "\"error\"") != NULL) { /* refused (a match that is over, or not this device's): its state */
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", match);
        if (post_ranked("match", extra, answer, sizeof(answer)) &&
            (strstr(answer, "\"state\":\"cancelled\"") || strstr(answer, "\"state\":\"expired\"") || strstr(answer, "\"state\":\"disputed\"") ||
             strstr(answer, "\"state\":\"final\""))) {
            outbox_done();
            *refresh = 1;
            message(strstr(answer, "\"state\":\"final\"") != NULL ? DRMSG_CONFIRMED : DRMSG_VOID, 0);
        } else if (strstr(answer, "\"error\":\"match\"") != NULL) {
            outbox_done(); /* (not known to the service: nothing to wait for) */
        }
    }
}

static void do_series(void) {
    char match[33], extra[192], answer[2048], decision[20] = "";
    int role, mode, sent, choice;
    unsigned gap;
    const char *nested;
    DrRankedPlayerResult *cards[2];
    if (!session_read(match, &role, &mode, &sent)) return;
    SDL_LockMutex(sLock);
    if (!sSeriesActive) { SDL_UnlockMutex(sLock); return; }
    choice=sSeriesChoice; gap=sSeriesGap;
    sSeriesDirty=0;
    SDL_UnlockMutex(sLock);
    SDL_snprintf(extra,sizeof(extra),",\"match\":\"%s\",\"gap\":%u,\"choice\":%d,\"wait\":20",match,gap,choice);
    if (!post_ranked("series",extra,answer,sizeof(answer))) return;
    text_value(answer,"decision",decision,sizeof(decision));
    SDL_LockMutex(sLock);
    if (sSeriesActive && sSeriesGap==gap) {
        DrRankedSeries *v=&sSeriesView;
        nested=strstr(answer,"\"series\":");
        if(nested){v->my_wins=number_value(nested,"mine");v->their_wins=number_value(nested,"theirs");v->played=number_value(nested,"played");}
        v->my_choice=number_value(answer,"my_choice");v->their_choice=number_value(answer,"their_choice");v->remaining=number_value(answer,"remaining");
        v->answered=1;v->gap=gap;v->round=sRankedRound;
        cards[0]=&v->me;cards[1]=&v->rival;
        nested=strstr(answer,"\"me\":");
        if(nested){text_value(nested,"name",cards[0]->name,sizeof(cards[0]->name));text_value(nested,"flag",cards[0]->flag,sizeof(cards[0]->flag));text_value(nested,"rank",cards[0]->rank,sizeof(cards[0]->rank));cards[0]->before=number_value(nested,"before");cards[0]->after=number_value(nested,"after");cards[0]->wins=number_value(nested,"wins");cards[0]->losses=number_value(nested,"losses");cards[0]->delta=cards[0]->after-cards[0]->before;cards[0]->elo=cards[0]->after;}
        nested=strstr(answer,"\"rival\":");
        if(nested){text_value(nested,"name",cards[1]->name,sizeof(cards[1]->name));text_value(nested,"flag",cards[1]->flag,sizeof(cards[1]->flag));text_value(nested,"rank",cards[1]->rank,sizeof(cards[1]->rank));cards[1]->before=number_value(nested,"before");cards[1]->after=number_value(nested,"after");cards[1]->wins=number_value(nested,"wins");cards[1]->losses=number_value(nested,"losses");cards[1]->delta=cards[1]->after-cards[1]->before;cards[1]->elo=cards[1]->after;}
        v->decision=strcmp(decision,"continue")==0?DR_SERIES_CONTINUE:strcmp(decision,"team")==0?DR_SERIES_TEAM:strcmp(decision,"over")==0?DR_SERIES_OVER:DR_SERIES_PENDING;
        if(v->decision) sSeriesActive=0;
    }
    SDL_UnlockMutex(sLock);
}

static void do_chat(void) {
    char body[900], answer[8192], hex[420], nonce[33], out[200];
    const char *row;
    int n;
    SDL_LockMutex(sLock);
    out[0] = '\0';
    if (sChatOutN > 0) {
        SDL_strlcpy(out, sChatOut[0], sizeof(out));
    }
    SDL_UnlockMutex(sLock);
    if (out[0] != '\0') {
        hex_encode(out, hex, sizeof(hex) > 321 ? 321 : sizeof(hex));
        random_hex(nonce, 32);
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\",\"token\":\"%s\",\"after\":%u,\"text_hex\":\"%s\",\"nonce\":\"%s\"}", wire(), sId, sToken,
                     sChatAfter, hex, nonce);
    } else {
        SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\",\"token\":\"%s\",\"after\":%u}", wire(), sId, sToken, sChatAfter);
    }
    if (!Match_PostDragon("chat/sync", body, answer, sizeof(answer)) || strstr(answer, "\"rows\"") == NULL) {
        SDL_LockMutex(sLock);
        sChat.state = strstr(answer, "\"version\"") != NULL ? -2 : -1;
        if (strstr(answer, "slow down") != NULL && sChatOutN > 0) { /* (too many at once: this one waits) */
            sChat.state = 2;
        }
        SDL_UnlockMutex(sLock);
        return;
    }
    SDL_LockMutex(sLock);
    if (out[0] != '\0' && sChatOutN > 0) {
        memmove(sChatOut[0], sChatOut[1], sizeof(sChatOut[0]) * (size_t)(sChatOutN - 1));
        sChatOutN--;
    }
    sChat.state = 2;
    sChat.sending = sChatOutN;
    for (row = strstr(answer, "{\"seq\":"); row != NULL; row = strstr(row + 1, "{\"seq\":")) {
        unsigned seq = (unsigned)number_value(row, "seq");
        char nh[64], th[400];
        if (seq <= sChatAfter && sChatAfter != 0) {
            continue;
        }
        text_value(row, "name_hex", nh, sizeof(nh));
        text_value(row, "text_hex", th, sizeof(th));
        if (sChat.count == 24) {
            memmove(sChat.name[0], sChat.name[1], sizeof(sChat.name[0]) * 23);
            memmove(sChat.text[0], sChat.text[1], sizeof(sChat.text[0]) * 23);
            memmove(&sChat.mine[0], &sChat.mine[1], sizeof(sChat.mine[0]) * 23);
            sChat.count = 23;
        }
        n = sChat.count++;
        hex_decode(nh, strlen(nh), sChat.name[n], sizeof(sChat.name[n]));
        hex_decode(th, strlen(th), sChat.text[n], sizeof(sChat.text[n]));
        {
            const char *next = strstr(row + 1, "{\"seq\":"), *mine = strstr(row, "\"mine\":true");
            sChat.mine[n] = mine != NULL && (next == NULL || mine < next);
        }
        if (seq > sChatAfter) {
            sChatAfter = seq;
        }
    }
    SDL_UnlockMutex(sLock);
}

static void do_presence(void) {
    char file[80], id[33], body[160], answer[512];
    if (!read_text("dragonnet-presence.txt", file, sizeof(file)) || sscanf(file, "%32s", id) != 1 || !is_hex(id, 32)) {
        random_hex(id, 32);
        SDL_snprintf(file, sizeof(file), "%s\n", id);
        write_text("dragonnet-presence.txt", file);
    }
    SDL_LockMutex(sLock);
    int leave = sPresenceLeave;
    sPresenceLeave = 0;
    SDL_UnlockMutex(sLock);
    SDL_snprintf(body, sizeof(body), "{\"v\":%d,\"id\":\"%s\"%s}", wire(), id, leave ? ",\"action\":\"leave\"" : "");
    if (Match_PostDragon("presence", body, answer, sizeof(answer)) && strstr(answer, "\"online\":") != NULL) {
        SDL_LockMutex(sLock);
        sOnline = number_value(answer, "online");
        SDL_UnlockMutex(sLock);
    } else if (!leave) {
        SDL_LockMutex(sLock);
        sOnline = -2;
        SDL_UnlockMutex(sLock);
    }
}

/* flow:2 (the service's lifecycle): "ready" once the lobby is connected, "started" when the fight begins.
   Sent by this thread (the game never waits for HTTP); gives up after a few failed tries. */
static void do_lifecycle(void) {
    char match[33], answer[512], extra[96];
    int ready, started, connect;
    SDL_LockMutex(sLock);
    ready = sLifeReady;
    started = sLifeStarted;
    connect = sLifeConnect;
    SDL_strlcpy(match, sLifeMatch, sizeof(match));
    SDL_UnlockMutex(sLock);
    if ((!connect && !ready && !started) || !is_hex(match, 32)) {
        return;
    }
    SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"action\":\"%s\"", match, connect ? "connect" : ready ? "ready" : "started");
    if (post_ranked_search("lifecycle", extra, answer, sizeof(answer)) || ++sLifeTries >= 5) {
        SDL_LockMutex(sLock);
        if (connect) {
            sLifeConnect = 0;
        } else if (ready) {
            sLifeReady = 0;
        } else {
            sLifeStarted = 0;
        }
        sLifeTries = 0;
        SDL_UnlockMutex(sLock);
    }
}

/* flow:2 rematch, on the worker thread: the game never waits for HTTP. "accept" is repeated until the service
   has both results (it answers "confirming" until then), then the state is asked every 4 seconds. */
static void do_flow(void) {
    char match[33], next[33], st[24], answer[512], extra[96], text[100];
    int act, fresh, role, mode;
    Uint64 now = SDL_GetTicks();

    SDL_LockMutex(sLock);
    act = sFlowAct;
    fresh = sFlowNow;
    sFlowNow = 0;
    SDL_strlcpy(match, sFlowMatch, sizeof(match));
    mode = sFlowMode;
    SDL_UnlockMutex(sLock);
    if (!act || !is_hex(match, 32)) {
        return;
    }
    if (fresh) {
        sFlowTries = 0;
    } else if (now - sFlowAt < 4000) {
        return;
    }
    sFlowAt = now;
    SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"action\":\"%s\"", match,
                 act == 3 ? "leave" : act == 2 ? "accept" : "status");
    if (!post_ranked("lifecycle", extra, answer, sizeof(answer))) {
        if (++sFlowTries >= 5) { /* the service does not answer (or is an old one): give up */
            SDL_LockMutex(sLock);
            if (act == sFlowAct && strcmp(match, sFlowMatch) == 0) {
                sFlowAct = 0;
                sFlowState = 4;
            }
            SDL_UnlockMutex(sLock);
        }
        return;
    }
    sFlowTries = 0;
    st[0] = next[0] = '\0';
    text_value(answer, "state", st, sizeof(st));
    text_value(answer, "match", next, sizeof(next));
    role = number_value(answer, "role");
    SDL_LockMutex(sLock);
    if (act != sFlowAct || strcmp(match, sFlowMatch) != 0) { /* the game asked for something else meanwhile */
        SDL_UnlockMutex(sLock);
        return;
    }
    if (is_hex(next, 32) && strcmp(next, match) != 0 && (role == 1 || role == 2)) { /* a NEW match: both accepted */
        SDL_snprintf(text, sizeof(text), "%s %d %d 0\n", next, role, mode);
        write_text("dragonnet-next.txt", text);
        sFlowState = 5;
        sFlowAct = 0;
    } else if (strcmp(st, "declined") == 0) {
        sFlowState = 4;
        sFlowAct = 0;
    } else if (strcmp(st, "waiting") == 0) {
        sFlowState = 3;
        if (act == 2) {
            sFlowAct = 1; /* the vote is in: from now on only ask */
        }
    } else if (strcmp(st, "choice") == 0) {
        sFlowState = 2;
        if (act == 2) {
            sFlowAct = 1;
        }
    } else {
        sFlowState = 1; /* confirming: the service does not have both results yet (accept is repeated) */
    }
    SDL_UnlockMutex(sLock);
}

static int SDLCALL worker(void *unused) {
    Uint64 chatAt = 0, resultAt = 0, resultWait = 0; /* (a result the rival has not reported yet: sent again in 5 s,
                                                         doubling to a minute, not every round) */
    (void)unused;
    for (;;) {
        char answer[4096], extra[256], name[20], match[33], code[8], cancelled[33];
        int refresh, search, cancel, board, state, role, chat, presence, leave, urgent, series, i;
        char viewcode[8];
        int background = gPortInBackground;
        unsigned gen;
        Uint64 now = SDL_GetTicks();

        SDL_LockMutex(sLock);
        SDL_strlcpy(name, sName, sizeof(name));
        refresh = sRefresh && !background;
        if (refresh) sRefresh = 0;
        search = sWantSearch;
        series = sSeriesActive;
        cancel = sWantCancel;
        sWantCancel = 0;
        board = sWantBoard && !background;
        if (board) sWantBoard = 0;
        gen = sSearchGen;
        state = sView.state;
        role = sView.role;
        SDL_strlcpy(viewcode, sView.code, sizeof(viewcode));
        SDL_strlcpy(cancelled, sCancelMatch, sizeof(cancelled));
        sCancelMatch[0] = '\0';
        SDL_strlcpy(match, sView.match, sizeof(match));
        SDL_strlcpy(code, sRoomPending, sizeof(code));
        sRoomPending[0] = '\0';
        chat = !background && DRNET_CHAT && sChatOpen && (now - chatAt > 3000 || sChatOutN > 0);
        presence = (sPresenceOn && (sPresenceAt == 0 || now - sPresenceAt > 300000)) || sPresenceLeave;
        leave = sPresenceLeave;
        SDL_UnlockMutex(sLock);

        if (presence) {
            do_presence();
            if (!leave) {
                sPresenceAt = now;
            }
        }
        {
            char om[33], od[16];
            int os; unsigned oround;
            urgent = outbox_first(om, &os, od, &oround) && now >= resultAt; /* (a result still to send) */
            if (!urgent && resultAt != 0 && !outbox_first(om, &os, od, &oround)) {
                resultAt = resultWait = 0;
            }
        }
        if ((refresh || search || cancel || board || chat || urgent || series) && identity()) {
            int pendingRefresh = refresh;
            if (urgent || refresh) {
                char om[33], od[16];
                int os;
                unsigned oround;
                do_result(&pendingRefresh);
                if (outbox_first(om, &os, od, &oround)) {
                    resultWait = resultWait == 0 ? 5000 : resultWait * 2 > 60000 ? 60000 : resultWait * 2;
                    resultAt = SDL_GetTicks() + resultWait;
                } else {
                    resultAt = resultWait = 0;
                }
            }
            if (cancel && is_hex(cancelled, 32)) {
                char cancelState[24] = "";
                SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", cancelled);
                search = 0; /* ranked/cancel must finish before the next ranked/queue */
                if (post_ranked_search("cancel", extra, answer, sizeof(answer))) {
                    text_value(answer, "state", cancelState, sizeof(cancelState));
                    if (SDL_strcmp(cancelState,"cancelled") && SDL_strcmp(cancelState,"expired") &&
                        SDL_strcmp(cancelState,"superseded") && SDL_strcmp(cancelState,"final") && SDL_strcmp(cancelState,"disputed")) {
                        SDL_LockMutex(sLock);
                        SDL_strlcpy(sCancelMatch,cancelled,sizeof(sCancelMatch));sWantCancel=1;
                        sWantSearch=sView.searching=0;sView.state=-1;sView.msg=DRMSG_NO_ANSWER;
                        sView.match[0]=sView.code[0]=sView.peer[0]=0;
                        SDL_UnlockMutex(sLock);
                        message(DRMSG_NO_ANSWER,-1); /* the service still considers the old match active */
                    }
                } else {
                    SDL_LockMutex(sLock);
                    SDL_strlcpy(sCancelMatch,cancelled,sizeof(sCancelMatch));sWantCancel=1;
                    sWantSearch=sView.searching=0;sView.state=-1;sView.msg=DRMSG_NO_ANSWER;
                    SDL_UnlockMutex(sLock); /* retry cancellation, never queue until the service releases the match */
                }
            }
            if (pendingRefresh) {
                SDL_snprintf(extra, sizeof(extra), ",\"name\":\"%s\"", name);
                if (post_ranked("profile", extra, answer, sizeof(answer))) {
                    SDL_LockMutex(sLock);
                    sView.rating = number_value(answer, "rating");
                    sView.wins = number_value(answer, "wins");
                    sView.losses = number_value(answer, "losses");
                    sView.draws = number_value(answer, "draws");
                    sView.position = number_value(answer, "position");
                    text_value(answer, "rank", sView.rank, sizeof(sView.rank));
                    sView.profile = 1;
                    if (!sView.searching && sView.state < 0 && sView.msg != DRMSG_VERSION) {
                        sView.state = 0;
                        sView.msg = DRMSG_UPDATED;
                    }
                    SDL_UnlockMutex(sLock);
                } else if (sView.msg == DRMSG_LIMIT) {
                    message(DRMSG_LIMIT, -1); /* (said on the screen as the server being busy, not as a failure) */
                } else if (sView.msg != DRMSG_VERSION) {
                    message(DRMSG_PROFILE_FAILED, -1);
                }
            }
            if (board) {
                int ok = post_ranked("leaderboard", "", answer, sizeof(answer));
                SDL_LockMutex(sLock);
                if (ok) {
                    const char *row = answer;
                    sView.boardCount = 0;
                    for (i = 0; i < 9; i++) {
                        row = strstr(row, "\"name\":");
                        if (row == NULL) {
                            break;
                        }
                        text_value(row, "name", sView.boardName[i], sizeof(sView.boardName[i]));
                        sView.boardRating[i] = number_value(row, "rating");
                        sView.boardCount++;
                        row++;
                    }
                }
                sView.board = ok ? 2 : -1;
                SDL_UnlockMutex(sLock);
            }
            if (search) {
                if (code[0] != '\0' && is_hex(match, 32)) {
                    SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"code\":\"%s\"", match, code);
                    if (post_ranked_search("room", extra, answer, sizeof(answer))) {
                        SDL_LockMutex(sLock);
                        if (gen == sSearchGen && sWantSearch) SDL_strlcpy(sView.code, code, sizeof(sView.code));
                        SDL_UnlockMutex(sLock);
                    } else {
                        if (sView.msg != DRMSG_VERSION && sView.msg != DRMSG_NO_CONNECTION) message(DRMSG_ROOM_FAILED, -1);
                    }
                } else if (state == 3 && role == 2 && viewcode[0] == '\0') {
                    Uint64 pairedFor = SDL_GetTicks() - sPairedAt;
                    if (pairedFor >= 30000) {
                        SDL_LockMutex(sLock);
                        if (gen == sSearchGen && sWantSearch && sView.state == 3 && sView.role == 2 && sView.code[0] == '\0') {
                            sWantSearch = sView.searching = 0;
                            sView.state = -1;
                            sView.msg = DRMSG_HOST_NO_ROOM;
                            SDL_strlcpy(sCancelMatch, sView.match, sizeof(sCancelMatch));
                            sWantCancel = 1;
                        }
                        SDL_UnlockMutex(sLock);
                    } else if (sCodePollAt == 0 || SDL_GetTicks() - sCodePollAt >= 2500) {
                        sCodePollAt = SDL_GetTicks();
                        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", match);
                        sCodePollCount++;
                        SDL_LockMutex(sLock);
                        sView.code_poll_count = sCodePollCount;
                        SDL_UnlockMutex(sLock);
                        if (post_ranked_search("match", extra, answer, sizeof(answer))) {
                            update_match(answer, gen);
                        }
                    }
                } else if (state != 3) {
                    if (SDL_GetTicks() - sSearchAt >= 180000) {
                        SDL_LockMutex(sLock);
                        if (gen == sSearchGen && sWantSearch) {
                            sWantSearch = sView.searching = 0;
                            sView.state = -1;
                            sView.msg = DRMSG_NO_RIVAL;
                            SDL_strlcpy(sCancelMatch, sView.match, sizeof(sCancelMatch));
                            sWantCancel = 1;
                        }
                        SDL_UnlockMutex(sLock);
                    } else {
                        SDL_snprintf(extra, sizeof(extra), ",\"rules\":\"dp20-bo3-v1\",\"flow\":2,\"rematches\":0,\"wait\":20");
                        if (post_ranked_search("queue", extra, answer, sizeof(answer))) {
                            char queued[33]="";
                            text_value(answer,"match",queued,sizeof(queued));
                            SDL_LockMutex(sLock);
                            if(gen==sSearchGen && sWantSearch && is_hex(queued,32)) {
                                SDL_strlcpy(sLastQueueMatch,queued,sizeof(sLastQueueMatch));
                                SDL_memcpy(sView.queue_match,queued,4);sView.queue_match[4]=0;
                                sView.queue_same_previous=sPreviousQueueMatch[0] && SDL_strcmp(sPreviousQueueMatch,queued)==0;
                            }
                            SDL_UnlockMutex(sLock);
                            update_match(answer, gen);
                        } else if (sView.msg != DRMSG_VERSION && sView.msg != DRMSG_LIMIT && sView.msg != DRMSG_NO_CONNECTION) {
                            message(strstr(answer, "busy") != NULL ? DRMSG_BUSY : DRMSG_NO_ANSWER, 99);
                        }
                    }
                }
            }
            if (chat) {
                chatAt = SDL_GetTicks();
                do_chat();
            }
        } else if (cancel && is_hex(cancelled,32)) {
            SDL_LockMutex(sLock);
            SDL_strlcpy(sCancelMatch,cancelled,sizeof(sCancelMatch));sWantCancel=1;
            SDL_UnlockMutex(sLock);
        } else if (chat) {
            chatAt = SDL_GetTicks();
            SDL_LockMutex(sLock);
            sChat.state = -1;
            SDL_UnlockMutex(sLock);
        }
        do_lifecycle();
        do_flow();
        if (series) do_series();
        /* The next round: at once when asked for something; a search asks the queue every 4 s in its first half
           minute, every 8 s to two minutes, then every 15 s (nobody there: the service's requests are a daily
           allowance shared by every player); paired, every 3 s (the room code is coming). Nothing searched: the
           loop only looks at what the window asked for, with no request. */
        {
            Uint64 waited = SDL_GetTicks() - sSearchAt;
            int paired, rounds;
            SDL_LockMutex(sLock);
            paired = sView.state == 3;
            SDL_UnlockMutex(sLock);
            rounds = !search ? 100 : gPortInBackground ? 1 : paired ? MATCH_RANKED_PAIRED_MS / 20 : waited < 30000 ? MATCH_RANKED_FIRST_MS / 20 : waited < 120000 ? MATCH_RANKED_MIDDLE_MS / 20 : MATCH_RANKED_LATE_MS / 20;
            for (i = 0; i < rounds; i++) {
                SDL_LockMutex(sLock);
                urgent = sFlowNow || (!gPortInBackground && (sRefresh || sWantBoard || sChatOutN > 0)) || sWantCancel || sRoomPending[0] != '\0' || sPresenceLeave || sLifeReady || sLifeStarted;
                SDL_UnlockMutex(sLock);
                if (urgent) {
                    break;
                }
                if (!Match_BackgroundWait(gPortInBackground ? MATCH_BACKGROUND_POLL_MS : 20, !paired)) {
                    SDL_LockMutex(sLock);
                    if (!paired && sView.searching) {
                        sWantCancel = 1;
                        SDL_strlcpy(sCancelMatch, sView.match, sizeof(sCancelMatch));
                        sView.searching = 0;
                        sView.state = -1;
                        sView.msg = DRMSG_BACKGROUND_TIMEOUT;
                    }
                    SDL_UnlockMutex(sLock);
                    break;
                }
            }
        }
    }
    return 0;
}

static void start(const char *name) {
    lock_make();
    SDL_LockMutex(sLock);
    if (name != NULL) {
        clean_name(name, sName, sizeof(sName));
    }
    SDL_UnlockMutex(sLock);
    if (SDL_CompareAndSwapAtomicInt(&sStarted, 0, 1)) {
        SDL_Thread *t = SDL_CreateThread(worker, "dragonnet", NULL);
        if (t != NULL) {
            SDL_DetachThread(t);
        } else {
            SDL_SetAtomicInt(&sStarted, 0);
        }
    }
}

/* ---- the window's side ---- */

/* flow:2 hooks (the game's side): only for matches whose rival also announced flow:2. */
/* The game restarts (relaunch = execv) between the lobby and the fight, and again after the session: the new process
   knows nothing of sView. Connection(1) leaves the match in dragonnet-flow.txt, which says "this match is flow:2". */
static int flow_file(const char *match) {
    char text[64], saved[33];
    if (!read_text("dragonnet-flow.txt", text, sizeof(text))) {
        return 0;
    }
    return sscanf(text, "%32s", saved) == 1 && strcmp(saved, match) == 0;
}

static void connection_queue(int ready) {
    if (!ready) {
        return;
    }
    SDL_LockMutex(sLock);
    if (sView.flow && is_hex(sView.match, 32)) {
        SDL_strlcpy(sLifeMatch, sView.match, sizeof(sLifeMatch));
        sLifeConnect = 1;
        sLifeReady = 1;
        sLifeTries = 0;
    }
    SDL_UnlockMutex(sLock);
    start(NULL);
}

/* The lobby is connected (flow:2). The game restarts right after (relaunch), so the worker could not be trusted to
   send `ready` in time: it is woken, and this waits (at most 2.5 s) until it has been sent. */
void DrRanked_Connection(int ready) {
    char match[33], line[64];
    int i, pending, flow;
    if (!ready) {
        return;
    }
    SDL_LockMutex(sLock);
    flow = sView.flow && is_hex(sView.match, 32);
    SDL_strlcpy(match, sView.match, sizeof(match));
    SDL_UnlockMutex(sLock);
    if (!flow || strcmp(sConnectionDone, match) == 0) {
        return;
    }
    SDL_strlcpy(sConnectionDone, match, sizeof(sConnectionDone));
    SDL_snprintf(line, sizeof(line), "%s 1\n", match);
    write_text("dragonnet-flow.txt", line);
    connection_queue(1);
    for (i = 0; i < 250; i++) {
        SDL_LockMutex(sLock);
        pending = sLifeReady && strcmp(sLifeMatch, match) == 0;
        SDL_UnlockMutex(sLock);
        if (!pending) {
            break;
        }
        SDL_Delay(10);
    }
}

void DrRanked_FightStarted(void) {
    char match[33];
    int role, mode, sent, ok;
    if (!session_read(match, &role, &mode, &sent) || sent) {
        return;
    }
    sRankedStarted=1;
    session_write(match,role,mode,sent);
    SDL_LockMutex(sLock);
    ok = (flow_file(match) || (sView.flow && strcmp(sView.match, match) == 0)) && strcmp(sStartSent, match) != 0;
    if (ok) {
        SDL_strlcpy(sLifeMatch, match, sizeof(sLifeMatch));
        SDL_strlcpy(sStartSent, match, sizeof(sStartSent));
        sLifeReady = 1; /* again: the process that sent it may be gone (the service takes a repeat) */
        sLifeStarted = 1;
        sLifeTries = 0;
    }
    SDL_UnlockMutex(sLock);
    if (ok) {
        start(NULL);
    }
}

void DrNet_PresenceActive(int on) {
    start(NULL);
    SDL_LockMutex(sLock);
    if (on && !sPresenceOn) {
        sPresenceAt = 0;
    }
    if (!on && sPresenceOn) {
        sPresenceLeave = 1;
    }
    sPresenceOn = on != 0;
    SDL_UnlockMutex(sLock);
}

int DrNet_Online(void) {
    int n;
    lock_make();
    SDL_LockMutex(sLock);
    n = sOnline;
    SDL_UnlockMutex(sLock);
    return n;
}

/* flow:2: the match of the session that just ended (dragonnet-prev.txt, written by DrNet_SessionEnding). */
static int prev_read(char *match, int *role, int *mode) {
    char text[160];
    int sent = 0;
    *role = *mode = 0;
    if (!read_text("dragonnet-prev.txt", text, sizeof(text))) {
        return 0;
    }
    return sscanf(text, "%32s %d %d %d", match, role, mode, &sent) >= 3 && is_hex(match, 32) && (*role == 1 || *role == 2);
}

/* flow:2 rematch (the game's side). The match is the one of the running session (dragonnet-session.txt). */
void DrRanked_FlowAct(int act) {
    char match[33];
    int role, mode, sent, after = 0;
    if (act < 1 || act > 3) {
        return;
    }
    if (!session_read(match, &role, &mode, &sent)) { /* no session any more: the one that just ended */
        after = prev_read(match, &role, &mode);
        if (!after) {
            return;
        }
    }
    SDL_LockMutex(sLock);
    if (after || flow_file(match) || (sView.flow && strcmp(sView.match, match) == 0)) {
        sFlowMode = mode;
        if (strcmp(sFlowMatch, match) != 0) {
            SDL_strlcpy(sFlowMatch, match, sizeof(sFlowMatch));
            sFlowState = 0;
        }
        if (sFlowState == 0) {
            sFlowState = 1;
        }
        sFlowAct = act;
        sFlowNow = 1;
    }
    SDL_UnlockMutex(sLock);
    start(NULL);
}

int DrRanked_FlowState(void) {
    int s;
    SDL_LockMutex(sLock);
    s = sFlowState;
    SDL_UnlockMutex(sLock);
    return s;
}

int DrRanked_FlowNext(char *match, int *role, int *mode) {
    char text[160];
    int sent = 0;
    *role = *mode = 0;
    if (!read_text("dragonnet-next.txt", text, sizeof(text))) {
        return 0;
    }
    if (sscanf(text, "%32s %d %d %d", match, role, mode, &sent) < 3 || !is_hex(match, 32) || (*role != 1 && *role != 2)) {
        return 0;
    }
    return 1;
}

void DrRanked_FlowClear(void) {
    write_text("dragonnet-next.txt", "0 0 0 0\n"); /* not a valid match: reads as "none" */
    write_text("dragonnet-prev.txt", "0 0 0 0\n");
    SDL_LockMutex(sLock);
    sFlowAct = sFlowNow = sFlowState = 0;
    sFlowMatch[0] = '\0';
    SDL_UnlockMutex(sLock);
}

/* flow:2 rematch accepted: the new match (dragonnet-next.txt) is already paired at the service. It resumes as a
   paired search: the worker's queue/room calls bring its code, and the lobby screen takes it from there. */
int DrRanked_Resume(void) {
    char text[160], match[33];
    int role = 0, mode = 0, sent = 0;
    if (!read_text("dragonnet-next.txt", text, sizeof(text))) {
        return 0;
    }
    if (sscanf(text, "%32s %d %d %d", match, &role, &mode, &sent) < 3 || !is_hex(match, 32) || (role != 1 && role != 2) || (mode != 0 && mode != 1)) {
        return 0;
    }
    start(NULL);
    SDL_LockMutex(sLock);
    sSearchGen++;
    sHandedOver[0] = '\0';
    sRoomPending[0] = '\0';
    sWantSearch = sView.searching = 1;
    sView.state = 3;
    sView.role = role;
    sView.mode = mode;
    sView.flow = 1;
    sView.code[0] = sView.peer[0] = '\0';
    SDL_strlcpy(sView.match, match, sizeof(sView.match));
    sView.msg = DRMSG_FOUND;
    sFlowAct = sFlowNow = sFlowState = 0;
    sFlowMatch[0] = '\0';
    SDL_UnlockMutex(sLock);
    write_text("dragonnet-next.txt", "0 0 0 0\n");
    write_text("dragonnet-prev.txt", "0 0 0 0\n");
    fprintf(stderr, "dragonnet: rematch resumed (role %d)\n", role);
    return 1;
}

void DrRanked_Open(const char *name) {
    start(name);
    SDL_LockMutex(sLock);
    sRefresh = 1;
    sView.http_status = 0;
    sView.error_field[0] = '\0';
    SDL_UnlockMutex(sLock);
}

void DrRanked_Search(const char *name, int mode) {
    char m[33], d[9];
    int s; unsigned round;
    char oldMatch[33] = ""; int oldRole, oldMode, oldSent;
    start(name);
    if (DrNet_Mode() != 0) {
        message(DRMSG_NO_CONNECTION, -1);
        return;
    }
    if (outbox_first(m, &s, d, &round)) {
        message(DRMSG_FINISH_FIRST, -1);
        return;
    }
    if (session_read(oldMatch,&oldRole,&oldMode,&oldSent) && sRankedStarted) {
        message(DRMSG_FINISH_FIRST,-1);
        return;
    }
    DrRanked_AbortBeforeStart();
    SDL_LockMutex(sLock);
    SDL_strlcpy(sPreviousQueueMatch,sLastQueueMatch,sizeof(sPreviousQueueMatch));
    sLastQueueMatch[0]='\0';
    sSearchGen++;
    sHandedOver[0] = '\0';
    sView.mode = mode != 0;
    sView.role=0;sView.flow=0;sView.code_poll_count=0;sView.queue_match[0]='\0';sView.queue_same_previous=0;
    sWantSearch = sView.searching = 1;
    sSearchAt = SDL_GetTicks();
    sView.state = 1;
    sView.msg = DRMSG_SEARCHING;
    sView.http_status = 0;
    sView.error_field[0] = '\0';
    sView.match[0] = sView.code[0] = sView.peer[0] = sRoomPending[0] = '\0';
    sPairedAt=sCodePollAt=0;sCodePollCount=0;
    sView.msg = DRMSG_SEARCHING;
    SDL_UnlockMutex(sLock);
}

void DrRanked_AbortBeforeStart(void) {
    char match[33]="", sessionMatch[33]="";
    int role=0, mode=0, sent=0, hasSession;
    lock_make();
    hasSession=session_read(sessionMatch,&role,&mode,&sent);
    if(hasSession && (sRankedStarted || sent)) return;
    SDL_LockMutex(sLock);
    if(hasSession) SDL_strlcpy(match,sessionMatch,sizeof(match));
    else if(is_hex(sView.match,32)) SDL_strlcpy(match,sView.match,sizeof(match));
    else if(is_hex(sHandedOver,32)) SDL_strlcpy(match,sHandedOver,sizeof(match));
    if(is_hex(match,32)) {
        SDL_strlcpy(sCancelMatch,match,sizeof(sCancelMatch));
        sWantCancel=1;
    }
    sSearchGen++;
    sWantSearch=sView.searching=0;
    sView.state=0;sView.role=0;sView.flow=0;sView.code_poll_count=0;sView.queue_match[0]=0;sView.queue_same_previous=0;
    sView.match[0]=sView.code[0]=sView.peer[0]=0;
    sRoomPending[0]=sHandedOver[0]=0;
    sLifeMatch[0]=sStartSent[0]=0;
    sConnectionDone[0]=0;
    sLifeConnect=sLifeReady=sLifeStarted=sLifeTries=0;
    sRankedStarted=0;sRankedRound=1;
    sSeriesActive=sSeriesDirty=sSeriesChoice=0;sSeriesGap=0;
    SDL_memset(&sSeriesView,0,sizeof(sSeriesView));
    sFlowAct=sFlowNow=sFlowState=0;sFlowMatch[0]=0;
    SDL_UnlockMutex(sLock);
    if(hasSession) remove_file("dragonnet-session.txt");
    remove_file("dragonnet-flow.txt");
    { extern void Port_LobbyCancel(void); Port_LobbyCancel(); }
    start(NULL);
}

void DrRanked_Cancel(void) {
    lock_make();
    SDL_LockMutex(sLock);
    if (sHandedOver[0] != '\0' || (!sWantSearch && !sView.searching)) {
        SDL_UnlockMutex(sLock);
        return;
    }
    SDL_strlcpy(sCancelMatch, sView.match, sizeof(sCancelMatch));
    sSearchGen++;
    sWantSearch = sView.searching = 0;
    sView.state = -1;
    sView.msg = DRMSG_CANCELLED;
    sView.match[0] = sView.code[0] = sRoomPending[0] = '\0';
    sWantCancel = 1;
    SDL_UnlockMutex(sLock);
}

void DrRanked_Board(void) {
    start(NULL);
    SDL_LockMutex(sLock);
    sWantBoard = 1;
    sView.board = 1;
    SDL_UnlockMutex(sLock);
}

void DrRanked_Poll(DrRanked *out) {
    static Uint64 lastRefresh;
    lock_make();
    SDL_LockMutex(sLock);
    *out = sView;
    if (sView.profile && !sView.searching && SDL_GetTicks() - lastRefresh > 60000) { /* (the profile kept fresh while shown) */
        lastRefresh = SDL_GetTicks();
        sRefresh = 1;
    }
    SDL_UnlockMutex(sLock);
}

unsigned DrRanked_SessionRound(void) { return sRankedRound; }
unsigned DrRanked_Ticks(void) { return (unsigned)SDL_GetTicks(); }

void DrRanked_SeriesChoice(unsigned gap, int choice) {
    char match[33]; int role, mode, sent;
    if (!gap || !session_read(match,&role,&mode,&sent)) return;
    SDL_LockMutex(sLock);
    if (gap != sSeriesGap) {
        SDL_memset(&sSeriesView,0,sizeof(sSeriesView));
        sSeriesGap=gap;sSeriesChoice=0;sSeriesActive=1;sSeriesDirty=1;
        sSeriesView.gap=gap;sSeriesView.round=sRankedRound;
    }
    if (choice >= 1 && choice <= 3 && !sSeriesChoice && sSeriesActive) {
        sSeriesChoice=choice;sSeriesDirty=1;
    }
    SDL_UnlockMutex(sLock);
    start(NULL);
}

void DrRanked_SeriesPoll(DrRankedSeries *out) {
    if (!out) return;
    SDL_LockMutex(sLock);*out=sSeriesView;SDL_UnlockMutex(sLock);
}

void DrRanked_SetRoom(const char *code) {
    lock_make();
    SDL_LockMutex(sLock);
    if (sWantSearch && sView.role == 1 && strcmp(code, sView.code) != 0) {
        SDL_strlcpy(sRoomPending, code, sizeof(sRoomPending));
    }
    SDL_UnlockMutex(sLock);
}

int DrRanked_Handover(void) {
    DrRanked v;
    DrRanked_Poll(&v);
    if (v.state != 3 || !is_hex(v.match, 32) || (v.role != 1 && v.role != 2)) {
        return 0;
    }
    sRankedRound=1;
    sRankedStarted=0;
    SDL_LockMutex(sLock);
    sSeriesActive=sSeriesDirty=sSeriesChoice=0;sSeriesGap=0;SDL_memset(&sSeriesView,0,sizeof(sSeriesView));
    SDL_UnlockMutex(sLock);
    session_write(v.match, v.role, v.mode, 0);
    SDL_SetAtomicInt(&sRankedWinnerDecided, 0);
    SDL_LockMutex(sLock);
    SDL_strlcpy(sHandedOver, v.match, sizeof(sHandedOver));
    sSearchGen++;
    sWantSearch = sView.searching = 0;
    SDL_UnlockMutex(sLock);
    fprintf(stderr, "dragonnet: ranked match handed over (role %d)\n", v.role);
    return 1;
}

/* flow:2: the game flags for the Revanche screen (ui.cpp shows it while MainMenu_Run waits, before the session ends). */
volatile int gPortRematchRequest, gPortRematchDone, gPortRematchUi;

/* Is the Revanche screen to be offered now: a session whose result is already registered, and that is flow:2? */
int DrRanked_RematchOffer(void) {
    char match[33];
    int role, mode, sent;
    if (!gPortRematchUi || !session_read(match, &role, &mode, &sent) || !sent) {
        return 0;
    }
    return flow_file(match);
}

int DrRanked_InSession(void) {
    char match[33];
    int role, mode, sent;
    return session_read(match, &role, &mode, &sent);
}

/* The fight is over (BattleResult_Finish): its result, as both games see it, for the service. The host is side 0.
   The digest is DBT-NX's (FNV-1a over the flags, the reason and the battle clock's ticks): the two reports of one
   fight are the same unless the two games disagreed about it. */
void DrNet_BattleFinished(int flags, int reason, int clock) {
    char match[33], line[128], digest[9];
    int role, mode, sent, score, i;
    uint32_t words[3], hash = 2166136261u;
    char diag[192];
    /* BattleResult_Finish normally applies these forced-winner reason bits before calling us. The
       natural-end hook runs earlier, so apply the same normalization here for the same score/digest. */
    if (reason & 0x20) flags |= 2;
    if (reason & 0x40) flags |= 1;
    score = (flags & 3) == 1 ? 2 : (flags & 3) == 2 ? 0 : (flags & 4) ? 1 : -1;
    if (gPortResim) {
        SDL_snprintf(diag, sizeof(diag), "finish=resim score=%d flags=%x", score, flags);
        finish_diag(diag);
        return;
    }
    if (!Port_NetActive()) {
        SDL_snprintf(diag, sizeof(diag), "finish=sem rede score=%d flags=%x", score, flags);
        finish_diag(diag);
        return;
    }
    if (!session_read(match, &role, &mode, &sent)) {
        SDL_snprintf(diag, sizeof(diag), "finish=sem ranked score=%d flags=%x", score, flags);
        finish_diag(diag);
        return;
    }
    if (!(flags & 8) && score >= 0) SDL_SetAtomicInt(&sRankedWinnerDecided, 1);
    if (sent) {
        /* Repeated finish hooks must not replace the diagnostic for the first real send. */
        return;
    }
    if (flags & 8) {
        SDL_snprintf(diag, sizeof(diag), "finish=abort score=%d flags=%x", score, flags);
        finish_diag(diag);
        return;
    }
    if (score < 0) {
        SDL_snprintf(diag, sizeof(diag), "finish=sem vencedor score=%d flags=%x", score, flags);
        finish_diag(diag);
        return;
    }
    words[0] = (uint32_t)flags;
    words[1] = (uint32_t)reason;
    words[2] = (uint32_t)clock;
    for (i = 0; i < (int)sizeof(words); i++) {
        hash = (hash ^ ((unsigned char *)words)[i]) * 16777619u;
    }
    SDL_snprintf(digest, sizeof(digest), "%08x", hash);
    SDL_snprintf(line, sizeof(line), "%s %d %s %u\n", match, score, digest, sRankedRound);
    if (!outbox_push(line)) {
        fprintf(stderr, "dragonnet: the ranked result could not be saved\n");
        SDL_snprintf(diag, sizeof(diag), "finish=falha ao salvar score=%d flags=%x", score, flags);
        finish_diag(diag);
        return;
    }
    session_write(match, role, mode, 1);
    fprintf(stderr, "dragonnet: ranked result saved: score %d (host's view), digest %s\n", score, digest);
    SDL_snprintf(diag, sizeof(diag), "finish=enviado ok score=%d digest=%s", score, digest);
    finish_diag(diag);
    start(NULL); /* (sent by the thread, now or the next time the online screen is open) */
}

/* A cleared result starts the next fight in the same best-of-three match. */
void DrRanked_BattleStart(void) {
    char match[33]; int role, mode, sent;
    if (!session_read(match, &role, &mode, &sent) || !sent) return;
    if (sRankedRound < 3) ++sRankedRound;
    session_write(match, role, mode, 0);
    SDL_SetAtomicInt(&sRankedWinnerDecided, 0);
}

/* The online session ends. `mine`: this player left (the pause menu, the game closed); 0: the other side went away
   or the two games disagreed. Leaving a ranked match before its fight was over is this player's loss. */
void DrNet_SessionEnding(int mine) {
    char match[33], line[128];
    int role, mode, sent;
    if (!session_read(match, &role, &mode, &sent)) {
        return;
    }
    if (!sRankedStarted && !sent) {
        /* No fight began: release the paired match without manufacturing a forfeit. */
        DrRanked_AbortBeforeStart();
        return;
    }
    { /* flow:2: after the session the Revanche screen asks the service about this match */
        int flow;
        SDL_LockMutex(sLock);
        flow = sent && (flow_file(match) || (sView.flow && strcmp(sView.match, match) == 0));
        SDL_UnlockMutex(sLock);
        if (flow) {
            SDL_snprintf(line, sizeof(line), "%s %d %d 1\n", match, role, mode);
            write_text("dragonnet-prev.txt", line);
        } else {
            remove_file("dragonnet-prev.txt");
        }
    }
    if (!sent && mine) {
        extern int BattleResult_GetFlags(void);
        int flags = BattleResult_GetFlags();
        if (SDL_GetAtomicInt(&sRankedWinnerDecided)) {
            fprintf(stderr, "dragonnet: ranked forfeit suppressed after winner sent=%d mine=%d flags=%x\n", sent, mine, flags);
            {
                char diag[192];
                SDL_snprintf(diag, sizeof(diag), "forfeit suppressed winner sent=%d mine=%d flags=%x", sent, mine, flags);
                finish_diag(diag);
            }
        } else {
            SDL_snprintf(line, sizeof(line), "%s -1 00000000 %u\n", match, sRankedRound);
            if (outbox_push(line)) {
                fprintf(stderr, "dragonnet: ranked forfeit queued sent=%d mine=%d flags=%x\n", sent, mine, flags);
                {
                    char diag[192];
                    SDL_snprintf(diag, sizeof(diag), "forfeit queued sent=%d mine=%d flags=%x", sent, mine, flags);
                    finish_diag(diag);
                }
            } else {
                fprintf(stderr, "dragonnet: ranked forfeit was not queued sent=%d mine=%d flags=%x\n", sent, mine, flags);
            }
        }
    }
    remove_file("dragonnet-session.txt");
}

void DrChat_Open(const char *name) {
    (void)name;
    lock_make();
    SDL_LockMutex(sLock);
    sChat.state = -1; /* chat desligado */
    SDL_UnlockMutex(sLock);
}

void DrChat_Close(void) {
    lock_make();
    SDL_LockMutex(sLock);
    sChatOpen = 0;
    SDL_UnlockMutex(sLock);
}

void DrChat_Send(const char *text) {
    char clean[200];
    size_t k = 0;
    lock_make();
    for (; *text != '\0' && k + 1 < sizeof(clean) && k < 160; text++) {
        clean[k++] = (unsigned char)*text < 32 ? ' ' : *text;
    }
    clean[k] = '\0';
    if (k == 0) {
        return;
    }
    SDL_LockMutex(sLock);
    if (sChatOutN < 4) {
        SDL_strlcpy(sChatOut[sChatOutN++], clean, sizeof(sChatOut[0]));
    }
    sChat.sending = sChatOutN;
    SDL_UnlockMutex(sLock);
}

void DrChat_Poll(DrChat *out) {
    lock_make();
    SDL_LockMutex(sLock);
    *out = sChat;
    SDL_UnlockMutex(sLock);
}

int DrRanked_Active(void) {
    int a;
    lock_make();
    SDL_LockMutex(sLock);
    a = sView.searching;
    SDL_UnlockMutex(sLock);
    return a;
}
