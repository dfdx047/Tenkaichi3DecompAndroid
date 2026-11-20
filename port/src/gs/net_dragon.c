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
static Uint64 sOutboxDeadline;
static char sOutboxTrackedMatch[33];
static int sOutboxForceRetry;
static SDL_AtomicInt sStarted;
static SDL_Condition *sWake;
static char sFinishDiag[192];
static char sOutboxDiag[192];
static char sLastResultDiag[160], sLastSeriesDiag[128];
static int sLastPostStatus;
static int sPostFightPhase;
static unsigned sPostFightElapsed;
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
    SDL_snprintf(out, size, "%s%s%s", sFinishDiag,
                 sFinishDiag[0] && sOutboxDiag[0] ? " | " : "",
                 sOutboxDiag);
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
static char sCancelMatch[33], sCancelTarget[33], sRoomPending[8], sHandedOver[33];
static int sCancelPending, sCancelUnconfirmed;
static Uint64 sCancelAt, sCancelRetryAt;
static char sLifeMatch[33], sStartSent[33]; /* flow:2: the match the lifecycle calls are for */
static char sConnectionDone[33];
static unsigned sRankedRound = 1;
static int sRankedLaunchPending; /* DrRanked_Handover since the last session began (DrRanked_SessionBegins) */
static int sRankedStarted;
static char sLastQueueMatch[33], sPreviousQueueMatch[33];
static char sLastQueueCode[8], sPreviousQueueCode[8];
static char sRoomPendingMatch[33];
static char sBurnedCodes[16][8], sAttemptCodes[16][8];
static Uint64 sBurnedAt[16];
static unsigned sBurnedCount, sBurnedNext, sAttemptCodeCount;
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
    if (SDL_strcmp(what, "queue") == 0) return 0;
    if (SDL_strcmp(what, "match") == 0) return 1;
    if (SDL_strcmp(what, "room") == 0) return 2;
    if (SDL_strcmp(what, "lifecycle") == 0) return 3;
    return -1;
}

static int code_index(char codes[][8], unsigned count, const char *code) {
    unsigned i;
    if (code == NULL || SDL_strlen(code) != 6) return -1;
    for (i = 0; i < count; i++) if (SDL_strcmp(codes[i], code) == 0) return (int)i;
    return -1;
}

/* Rejected codes live only in memory and expire ten minutes after being burned. */
static void burned_codes_prune_locked(void) {
    Uint64 now = SDL_GetTicks();
    unsigned i = 0;
    while (i < sBurnedCount) {
        if (now - sBurnedAt[i] >= 600000) {
            unsigned tail = sBurnedCount - i - 1;
            if (tail) {
                SDL_memmove(&sBurnedCodes[i], &sBurnedCodes[i + 1], tail * sizeof(sBurnedCodes[0]));
                SDL_memmove(&sBurnedAt[i], &sBurnedAt[i + 1], tail * sizeof(sBurnedAt[0]));
            }
            sBurnedCount--;
        } else {
            i++;
        }
    }
}

static int code_burned_locked(const char *code) {
    burned_codes_prune_locked();
    return code_index(sBurnedCodes, sBurnedCount, code) >= 0;
}

static void burn_code_locked(const char *code) {
    unsigned at;
    burned_codes_prune_locked();
    if (code == NULL || SDL_strlen(code) != 6 || code_index(sBurnedCodes, sBurnedCount, code) >= 0) return;
    if (sBurnedCount < 16) {
        at = sBurnedCount++;
    } else {
        at = sBurnedNext++ % 16;
    }
    SDL_strlcpy(sBurnedCodes[at], code, sizeof(sBurnedCodes[at]));
    sBurnedAt[at] = SDL_GetTicks();
}

static void attempt_code_locked(const char *code) {
    if (code == NULL || SDL_strlen(code) != 6 || code_index(sAttemptCodes, sAttemptCodeCount, code) >= 0) return;
    if (sAttemptCodeCount < 16) SDL_strlcpy(sAttemptCodes[sAttemptCodeCount++], code, sizeof(sAttemptCodes[0]));
    else SDL_strlcpy(sAttemptCodes[15], code, sizeof(sAttemptCodes[0]));
}

static void ranked_redact_body(const char *json, char *safe, size_t size) {
    size_t n = 0;
    const char *p = json != NULL ? json : "";
    if (size == 0) return;
    while (*p && n + 1 < size) {
        if (SDL_strncmp(p, "\"token\"", 7) == 0) {
            const char *value = p + 7, *end;
            while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') value++;
            if (*value++ == ':') {
                while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') value++;
                if (*value == '"') {
                    end = SDL_strchr(value + 1, '"');
                    if (end != NULL) {
                        const char *redacted = "\"token\":\"[redacted]\"";
                        while (*redacted && n + 1 < size) safe[n++] = *redacted++;
                        p = end + 1;
                        continue;
                    }
                }
            }
            safe[n++] = *p++;
        } else {
            safe[n++] = *p++;
        }
    }
    safe[n] = '\0';
}

static void ranked_debug_log_line(const char *event) {
    enum { LOG_LIMIT = 160 * 1024, LOG_KEEP = 80 * 1024 };
    char path[600], safe[512];
    FILE *f;
    long end;
    lock_make();
    ranked_redact_body(event, safe, sizeof(safe));
    SDL_LockMutex(sLock);
    path_of("ranked-debug.log", path, sizeof(path));
    f = fopen(path, "ab+");
    if (f != NULL) {
        fseek(f, 0, SEEK_END);
        end = ftell(f);
        if (end >= LOG_LIMIT) {
            char *tail = (char *)malloc(LOG_KEEP + 1);
            fclose(f);
            f = NULL;
            if (tail != NULL) {
                FILE *old = fopen(path, "rb");
                size_t got = 0;
                if (old != NULL) {
                    long from = end - LOG_KEEP;
                    if (from < 0) from = 0;
                    if (fseek(old, from, SEEK_SET) == 0) got = fread(tail, 1, LOG_KEEP, old);
                    fclose(old);
                }
                tail[got] = '\0';
                write_text("ranked-debug.log", tail);
                free(tail);
            } else {
                write_text("ranked-debug.log", "");
            }
            f = fopen(path, "ab");
        }
        if (f != NULL) {
            fprintf(f, "%u %s\n", (unsigned)SDL_GetTicks(), safe);
            fclose(f);
        }
    }
    SDL_UnlockMutex(sLock);
}

void DrRanked_DebugLog(const char *event) {
    if (event != NULL && event[0] != '\0') ranked_debug_log_line(event);
}

static void ranked_log_http(const char *route, int status, const char *json) {
    char safe[320], event[400];
    ranked_redact_body(json, safe, sizeof(safe));
    SDL_snprintf(event, sizeof(event), "ranked/%s http=%d response=%.280s", route, status, safe);
    ranked_debug_log_line(event);
}

static void ranked_log_code_response(const char *what, int status, const char *json) {
    char code[8] = "", safe[4096];
    char publish[8] = "", publishMatch[33] = "";
    if (SDL_strcmp(what, "queue") != 0 && SDL_strcmp(what, "match") != 0 && SDL_strcmp(what, "room") != 0) return;
    text_value(json != NULL ? json : "", "code", code, sizeof(code));
    if (code[0] == '\0') return;
    ranked_redact_body(json, safe, sizeof(safe));
    if (SDL_strcmp(what, "room") == 0) {
        SDL_LockMutex(sLock);
        SDL_strlcpy(publish, sView.room_published_code, sizeof(publish));
        SDL_strlcpy(publishMatch, sView.room_published_match, sizeof(publishMatch));
        SDL_UnlockMutex(sLock);
    }
    fprintf(stderr, "dragonnet: ranked/%s http=%d%s%s code=%s body=%s\n", what, status,
            publish[0] ? " publish=" : "", publish[0] ? publish : "",
            code, safe);
    if (publishMatch[0]) fprintf(stderr, "dragonnet: ranked/room published_match=%s\n", publishMatch);
}

static void ranked_room_report(const char *match, const char *published, const char *json) {
    char returned[8] = "", safe[4096];
    int status, old;
    text_value(json != NULL ? json : "", "code", returned, sizeof(returned));
    ranked_redact_body(json, safe, sizeof(safe));
    SDL_LockMutex(sLock);
    status = sView.route_http[2];
    old = status == 200 && returned[0] && SDL_strcmp(returned, published) != 0;
    if (SDL_strcmp(match, sView.match) == 0 && SDL_strcmp(match, sView.room_published_match) == 0) {
        SDL_strlcpy(sView.room_response_code, returned, sizeof(sView.room_response_code));
        sView.room_kept_old = old;
    }
    SDL_UnlockMutex(sLock);
    fprintf(stderr, "dragonnet: ranked/room publish match=%s code=%s http=%d response_code=%s body=%s\n",
            match, published, status, returned[0] ? returned : "-", safe);
    if (old) fprintf(stderr, "dragonnet: ranked/room servidor manteve a sala antiga: match=%s published=%s returned=%s http=%d\n",
                     match, published, returned, status);
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
    SDL_snprintf(out,size,"result %s | series %s | gap=%u round=%u | postfight phase=%d ms=%u",sLastResultDiag[0]?sLastResultDiag:"http=0 -",sLastSeriesDiag[0]?sLastSeriesDiag:"http=0 -",sSeriesGap,sRankedRound,sPostFightPhase,sPostFightElapsed);
    SDL_UnlockMutex(sLock);
}

void DrRanked_PostFightState(int phase, unsigned elapsed_ms) {
    static const char *names[] = {"idle", "menu", "waiting", "native-fallback", "server-decision"};
    int changed;
    char event[128];
    if (phase < 0 || phase > 4) phase = 0;
    lock_make();
    SDL_LockMutex(sLock);
    changed = sPostFightPhase != phase;
    sPostFightPhase = phase;
    sPostFightElapsed = elapsed_ms;
    SDL_UnlockMutex(sLock);
    if (changed) {
        SDL_snprintf(event, sizeof(event), "ranked/postfight phase=%s elapsed_ms=%u", names[phase], elapsed_ms);
        ranked_debug_log_line(event);
    }
}

static void message(int msg, int state) {
    SDL_LockMutex(sLock);
    sView.msg = msg;
    if (state != 99) {
        sView.state = state;
    }
    SDL_UnlockMutex(sLock);
}

static void ranked_cancel_request_locked(const char *match) {
    if (!is_hex(match, 32)) return;
    if (!sCancelPending || SDL_strcmp(sCancelTarget, match) != 0) {
        sCancelAt = SDL_GetTicks();
        sCancelUnconfirmed = 0;
    }
    sCancelPending = 1;
    sCancelRetryAt = 0;
    SDL_strlcpy(sCancelTarget, match, sizeof(sCancelTarget));
    SDL_strlcpy(sCancelMatch, match, sizeof(sCancelMatch));
    sWantCancel = 1;
    sView.cancel_pending = 1;
    sView.cancel_unconfirmed = 0;
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
        sLastPostStatus = status;
        ranked_log_http(what, status, answer);
        if(SDL_strcmp(what,"result")==0 || SDL_strcmp(what,"series")==0) ranked_response_diagnostic(what,status,answer);
        if (SDL_strcmp(what, "profile") == 0 || ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
        return 0;
    }
    sLastPostStatus = status;
    ranked_log_http(what, status, answer);
    if(SDL_strcmp(what,"result")==0 || SDL_strcmp(what,"series")==0) ranked_response_diagnostic(what,status,answer);
    if (SDL_strcmp(what, "profile") == 0 || ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
    if (strstr(answer, "\"error\":\"limit\"") != NULL) {
        if (SDL_strcmp(what, "result") != 0) message(DRMSG_LIMIT, 99); /* result retries must not alter a newer queue's UI */
        return 0;
    }
    if (strstr(answer, "\"error\":\"version\"") != NULL) {
        if (SDL_strcmp(what, "result") != 0) message(DRMSG_VERSION, -1);
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
        ranked_log_http(what, status, answer);
        if (ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
        ranked_log_code_response(what, status, answer);
        message(DRMSG_NO_CONNECTION, 99);
        return 0;
    }
    ranked_log_http(what, status, answer);
    if (ranked_route(what) >= 0) ranked_diagnostic(what, status, answer);
    ranked_log_code_response(what, status, answer);
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

static void outbox_watch(const char *match, Uint64 now) {
    SDL_LockMutex(sOutboxLock);
    if (SDL_strcmp(sOutboxTrackedMatch, match) != 0) {
        SDL_strlcpy(sOutboxTrackedMatch, match, sizeof(sOutboxTrackedMatch));
        sOutboxDeadline = now + 10000;
        sOutboxForceRetry = 1;
    }
    SDL_UnlockMutex(sOutboxLock);
}

static int outbox_watch_expired(const char *match, Uint64 now) {
    int expired;
    SDL_LockMutex(sOutboxLock);
    expired = sOutboxDeadline != 0 && SDL_strcmp(sOutboxTrackedMatch, match) == 0 && now >= sOutboxDeadline;
    SDL_UnlockMutex(sOutboxLock);
    return expired;
}

static int outbox_take_force_retry(void) {
    int force;
    SDL_LockMutex(sOutboxLock);
    force = sOutboxForceRetry;
    sOutboxForceRetry = 0;
    SDL_UnlockMutex(sOutboxLock);
    return force;
}

static void outbox_unwatch(const char *match) {
    SDL_LockMutex(sOutboxLock);
    if (SDL_strcmp(sOutboxTrackedMatch, match) == 0) {
        sOutboxTrackedMatch[0] = '\0';
        sOutboxDeadline = 0;
        sOutboxForceRetry = 0;
    }
    SDL_UnlockMutex(sOutboxLock);
}

static void outbox_remove_report(const char *match, int score, unsigned round, const char *reason, int discarded) {
    char diag[192];
    outbox_done();
    outbox_unwatch(match);
    SDL_snprintf(diag, sizeof(diag), "%s: %.48s match=%.8s round=%u score=%d",
                 discarded ? "resultado antigo descartado" : "resultado ranked encerrado",
                 reason ? reason : "estado terminal", match, round, score);
    SDL_LockMutex(sLock);
    SDL_strlcpy(sOutboxDiag, diag, sizeof(sOutboxDiag));
    SDL_UnlockMutex(sLock);
    ranked_debug_log_line(diag);
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
    char state[24], assigned[33], incomingCode[8], oldServerCode[8];
    int changedMatch, stalePreviousCode, incomingRole, burnedCode;
    text_value(answer, "state", state, sizeof(state));
    text_value(answer, "match", assigned, sizeof(assigned));
    text_value(answer, "code", incomingCode, sizeof(incomingCode));
    incomingRole = number_value(answer, "role");
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
    changedMatch = sView.match[0] && SDL_strcmp(sView.match, assigned) != 0;
    if (changedMatch) {
        sView.code[0] = sView.server_code[0] = '\0';
        sView.code_poll_count = sView.code_change_count = 0;
        sCodePollCount = 0;
        sPairedAt = sCodePollAt = 0;
    }
    SDL_strlcpy(oldServerCode, sView.server_code, sizeof(oldServerCode));
    stalePreviousCode = incomingRole == 2 && assigned[0] &&
        SDL_strcmp(assigned, sPreviousQueueMatch) == 0 && sPreviousQueueCode[0] &&
        SDL_strcmp(incomingCode, sPreviousQueueCode) == 0;
    burnedCode = incomingRole == 2 && code_burned_locked(incomingCode);
    SDL_strlcpy(sView.match, assigned, sizeof(sView.match));
    SDL_strlcpy(sView.server_code, incomingCode, sizeof(sView.server_code));
    sView.server_code_burned = burnedCode;
    text_value(answer, "peer", sView.peer, sizeof(sView.peer));
    sView.role = incomingRole;
    if (sView.role == 2 && (stalePreviousCode || burnedCode)) {
        sView.code[0] = '\0'; /* same match and code as last attempt: wait for ranked/match to publish a new code */
    } else {
        SDL_strlcpy(sView.code, incomingCode, sizeof(sView.code));
        if (incomingCode[0]) SDL_strlcpy(sLastQueueCode, incomingCode, sizeof(sLastQueueCode));
    }
    if (incomingCode[0] && oldServerCode[0] && SDL_strcmp(incomingCode, oldServerCode) != 0)
        sView.code_change_count++;
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

static void ranked_clear_search_view_locked(void) {
    sWantSearch = sView.searching = 0;
    sView.state = 0;
    sView.role = sView.flow = 0;
    sView.mode = 0;
    sView.code_poll_count = sView.code_change_count = sView.server_code_burned = 0;
    sView.cancel_pending = sView.cancel_unconfirmed = 0;
    sView.queue_match[0] = sView.queue_same_previous = 0;
    sView.match[0] = sView.code[0] = sView.server_code[0] = sView.peer[0] = '\0';
    sView.room_published_code[0] = sView.room_published_match[0] = sView.room_response_code[0] = '\0';
    sView.room_kept_old = 0;
    sRoomPending[0] = sRoomPendingMatch[0] = '\0';
    sHandedOver[0] = sLifeMatch[0] = sStartSent[0] = sConnectionDone[0] = '\0';
    sLifeConnect = sLifeReady = sLifeStarted = sLifeTries = 0;
    sCodePollCount = 0;
    sPairedAt = sCodePollAt = 0;
    sRankedStarted = 0;
    sRankedRound = 1;
    sSeriesActive = sSeriesDirty = sSeriesChoice = 0;
    sSeriesGap = 0;
    SDL_memset(&sSeriesView, 0, sizeof(sSeriesView));
    sFlowAct = sFlowNow = sFlowState = sFlowTries = 0;
    sFlowMatch[0] = '\0';
    sFlowAt = 0;
    sFlowMode = 0;
}

static void ranked_burn_attempt_locked(void) {
    unsigned i;
    if (sRankedStarted) {
        sAttemptCodeCount = 0;
        SDL_memset(sAttemptCodes, 0, sizeof(sAttemptCodes));
        return;
    }
    for (i = 0; i < sAttemptCodeCount; i++) burn_code_locked(sAttemptCodes[i]);
    sAttemptCodeCount = 0;
    SDL_memset(sAttemptCodes, 0, sizeof(sAttemptCodes));
}

/* Terminal replies retire a persisted result instead of leaving it to block later ranked searches. */
static int result_terminal(const char *answer, char *reason, size_t size) {
    char state[24] = "", error[32] = "";
    text_value(answer, "state", state, sizeof(state));
    if (SDL_strcmp(state, "final") == 0 || SDL_strcmp(state, "disputed") == 0 ||
        SDL_strcmp(state, "cancelled") == 0 || SDL_strcmp(state, "expired") == 0 ||
        SDL_strcmp(state, "finished") == 0 || SDL_strcmp(state, "complete") == 0 ||
        SDL_strcmp(state, "closed") == 0 || SDL_strcmp(state, "void") == 0) {
        SDL_snprintf(reason, size, "server state=%s", state);
        return 2;
    }
    text_value(answer, "error", error, sizeof(error));
    if (SDL_strcmp(error, "match") == 0 || SDL_strcmp(error, "not_found") == 0 ||
        SDL_strcmp(error, "match_not_found") == 0 || SDL_strcmp(error, "not found") == 0 ||
        SDL_strcmp(error, "expired") == 0 || SDL_strcmp(error, "cancelled") == 0 ||
        SDL_strcmp(error, "finished") == 0 || SDL_strcmp(error, "already_finished") == 0 ||
        SDL_strcmp(error, "final") == 0 || SDL_strcmp(error, "disputed") == 0 ||
        SDL_strcmp(error, "already_final") == 0 || SDL_strcmp(error, "closed") == 0) {
        SDL_snprintf(reason, size, "server error=%s", error);
        return 1;
    }
    return 0;
}

static void do_result(int *refresh) {
    char match[33], digest[9], extra[160], answer[2048], state[24];
    char terminalReason[64] = "";
    int score, posted, terminal, resultStatus, matchPosted, matchStatus;
    unsigned round;
    if (!outbox_first(match, &score, digest, &round)) {
        return;
    }
    if (score < 0) {
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"round\":%u", match, round);
    } else {
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"score\":%d,\"digest\":\"%s\",\"round\":%u", match, score, digest, round);
    }
    posted = post_ranked(score < 0 ? "forfeit" : "result", extra, answer, sizeof(answer));
    resultStatus = sLastPostStatus;
    terminal = result_terminal(answer, terminalReason, sizeof(terminalReason));
    if (terminal) {
        outbox_remove_report(match, score, round, terminalReason, terminal == 1);
        *refresh = 1;
        return;
    }
    if (!posted && (resultStatus == 404 || resultStatus == 410)) {
        SDL_snprintf(terminalReason, sizeof(terminalReason), "server HTTP %d match inexistente/expirado", resultStatus);
        outbox_remove_report(match, score, round, terminalReason, 1);
        *refresh = 1;
        return;
    }
    if (posted) {
        text_value(answer, "state", state, sizeof(state));
        fprintf(stderr, "dragonnet: ranked result: %s\n", state);
    } else if (strstr(answer, "\"error\"") != NULL) { /* refused (a match that is over, or not this device's): its state */
        SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", match);
        matchPosted = post_ranked("match", extra, answer, sizeof(answer));
        matchStatus = sLastPostStatus;
        terminal = result_terminal(answer, terminalReason, sizeof(terminalReason));
        if (terminal) {
            outbox_remove_report(match, score, round, terminalReason, terminal == 1);
            *refresh = 1;
        } else if (!matchPosted && (matchStatus == 404 || matchStatus == 410)) {
            SDL_snprintf(terminalReason, sizeof(terminalReason), "server HTTP %d match inexistente/expirado", matchStatus);
            outbox_remove_report(match, score, round, terminalReason, 1);
            *refresh = 1;
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
    SDL_snprintf(extra,sizeof(extra),",\"match\":\"%s\",\"gap\":%u,\"choice\":%d,\"wait\":4",match,gap,choice); /* (short: a choice made meanwhile goes in the next request) */
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
        char answer[4096], extra[256], name[20], match[33], code[8], roomMatch[33], cancelled[33];
        int refresh, search, cancel, board, state, role, chat, presence, leave, urgent, series, i, cancelTimedOut, discardPending;
        char viewcode[8];
        char pendingMatch[33], pendingDigest[9];
        int pendingScore;
        unsigned pendingRound;
        int background = gPortInBackground;
        unsigned gen;
        Uint64 now = SDL_GetTicks();

        cancelTimedOut = 0;
        SDL_LockMutex(sLock);
        if (sCancelPending && now - sCancelAt >= 10000) {
            sCancelPending = 0;
            sCancelUnconfirmed = 1;
            sCancelMatch[0] = '\0';
            sCancelTarget[0] = '\0';
            sWantCancel = 0;
            sView.cancel_pending = 0;
            sView.cancel_unconfirmed = 1;
            cancelTimedOut = 1;
        }
        SDL_strlcpy(name, sName, sizeof(name));
        refresh = sRefresh && !background;
        if (refresh) sRefresh = 0;
        search = sWantSearch && !sCancelPending;
        series = sSeriesActive;
        cancel = sWantCancel && (sCancelRetryAt == 0 || now >= sCancelRetryAt);
        if (cancel) {
            sWantCancel = 0;
            sCancelRetryAt = 0;
        }
        board = sWantBoard && !background;
        if (board) sWantBoard = 0;
        gen = sSearchGen;
        state = sView.state;
        role = sView.role;
        SDL_strlcpy(viewcode, sView.code, sizeof(viewcode));
        cancelled[0] = '\0';
        if (cancel) {
            SDL_strlcpy(cancelled, sCancelMatch, sizeof(cancelled));
            sCancelMatch[0] = '\0';
        }
        SDL_strlcpy(match, sView.match, sizeof(match));
        SDL_strlcpy(code, sRoomPending, sizeof(code));
        SDL_strlcpy(roomMatch, sRoomPendingMatch, sizeof(roomMatch));
        sRoomPending[0] = '\0';
        sRoomPendingMatch[0] = '\0';
        chat = !background && DRNET_CHAT && sChatOpen && (now - chatAt > 3000 || sChatOutN > 0);
        presence = (sPresenceOn && (sPresenceAt == 0 || now - sPresenceAt > 300000)) || sPresenceLeave;
        leave = sPresenceLeave;
        SDL_UnlockMutex(sLock);

        if (cancelTimedOut) ranked_debug_log_line("ranked/cancel unconfirmed after 10s; allowing queue");

        if (presence) {
            do_presence();
            if (!leave) {
                sPresenceAt = now;
            }
        }
        if (outbox_take_force_retry()) resultAt = resultWait = 0;
        {
            int hasPending = outbox_first(pendingMatch, &pendingScore, pendingDigest, &pendingRound);
            urgent = hasPending && now >= resultAt; /* (a result still to send) */
            discardPending = hasPending && outbox_watch_expired(pendingMatch, now);
            if (!urgent && resultAt != 0 && !hasPending) {
                resultAt = resultWait = 0;
            }
        }
        if (discardPending) {
            outbox_remove_report(pendingMatch, pendingScore, pendingRound, "sem estado terminal do servidor em 10 s", 1);
            resultAt = resultWait = 0;
            urgent = 0;
        }
        if ((refresh || search || cancel || board || chat || urgent || series) && identity()) {
            int pendingRefresh = refresh;
            if ((urgent || refresh) && !search) {
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
                int terminal = 0;
                SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\"", cancelled);
                if (post_ranked_search("cancel", extra, answer, sizeof(answer))) {
                    text_value(answer, "state", cancelState, sizeof(cancelState));
                    terminal = !SDL_strcmp(cancelState,"cancelled") || !SDL_strcmp(cancelState,"expired") ||
                               !SDL_strcmp(cancelState,"superseded") || !SDL_strcmp(cancelState,"final") ||
                               !SDL_strcmp(cancelState,"disputed");
                }
                SDL_LockMutex(sLock);
                if (terminal && SDL_strcmp(sCancelTarget, cancelled) == 0) {
                    sCancelPending = sCancelUnconfirmed = 0;
                    sCancelTarget[0] = sCancelMatch[0] = '\0';
                    sCancelRetryAt = 0;
                    sView.cancel_pending = sView.cancel_unconfirmed = 0;
                } else if (sCancelPending && SDL_strcmp(sCancelTarget, cancelled) == 0) {
                    SDL_strlcpy(sCancelMatch, cancelled, sizeof(sCancelMatch));
                    sWantCancel = 1;
                    sCancelRetryAt = SDL_GetTicks() + 1000;
                    sView.cancel_pending = 1;
                }
                SDL_UnlockMutex(sLock);
                {
                    char event[128];
                    SDL_snprintf(event, sizeof(event), "ranked/cancel match=%.32s state=%s confirmed=%d",
                                 cancelled, cancelState[0] ? cancelState : "no-response", terminal);
                    ranked_debug_log_line(event);
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
                if (code[0] != '\0' && is_hex(match, 32) && SDL_strcmp(roomMatch, match) == 0) {
                    int canPublish = 0;
                    SDL_snprintf(extra, sizeof(extra), ",\"match\":\"%s\",\"code\":\"%s\"", match, code);
                    SDL_LockMutex(sLock);
                    if (gen == sSearchGen && sWantSearch && SDL_strcmp(match, sView.match) == 0) {
                        SDL_strlcpy(sView.room_published_match, match, sizeof(sView.room_published_match));
                        SDL_strlcpy(sView.room_published_code, code, sizeof(sView.room_published_code));
                        sView.room_response_code[0] = '\0';
                        sView.room_kept_old = 0;
                        canPublish = 1;
                    }
                    SDL_UnlockMutex(sLock);
                    if (!canPublish) {
                        /* This lobby result belongs to a search that has already ended. */
                    } else if (post_ranked_search("room", extra, answer, sizeof(answer))) {
                        ranked_room_report(match, code, answer);
                        SDL_LockMutex(sLock);
                        if (gen == sSearchGen && sWantSearch) SDL_strlcpy(sView.code, code, sizeof(sView.code));
                        SDL_UnlockMutex(sLock);
                    } else {
                        ranked_room_report(match, code, answer);
                        if (sView.msg != DRMSG_VERSION && sView.msg != DRMSG_NO_CONNECTION) message(DRMSG_ROOM_FAILED, -1);
                    }
                } else if (state == 3 && role == 2) {
                    Uint64 pairedFor = SDL_GetTicks() - sPairedAt;
                    if (pairedFor >= 30000 && viewcode[0] == '\0') {
                        SDL_LockMutex(sLock);
                        if (gen == sSearchGen && sWantSearch && sView.state == 3 && sView.role == 2 && sView.code[0] == '\0') {
                            sWantSearch = sView.searching = 0;
                            sView.state = -1;
                            sView.msg = DRMSG_HOST_NO_ROOM;
                            ranked_cancel_request_locked(sView.match);
                        }
                        SDL_UnlockMutex(sLock);
                    } else if (!sLifeConnect && !sLifeReady && (sCodePollAt == 0 || SDL_GetTicks() - sCodePollAt >= 2500)) {
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
                            ranked_cancel_request_locked(sView.match);
                        }
                        SDL_UnlockMutex(sLock);
                    } else {
                        SDL_snprintf(extra, sizeof(extra), ",\"rules\":\"team3-bo3-v1\",\"flow\":2,\"rematches\":0,\"wait\":20");
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
            /* Give a new queue/match request priority over a persisted result from an older match. */
            if (search && (urgent || refresh)) {
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
                if (pendingRefresh) {
                    SDL_LockMutex(sLock);
                    sRefresh = 1;
                    SDL_UnlockMutex(sLock);
                }
            }
            if (chat) {
                chatAt = SDL_GetTicks();
                do_chat();
            }
        } else if (cancel && is_hex(cancelled,32)) {
            SDL_LockMutex(sLock);
            SDL_strlcpy(sCancelMatch, cancelled, sizeof(sCancelMatch));
            sWantCancel = 1;
            sCancelRetryAt = SDL_GetTicks() + 1000;
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
                urgent = sFlowNow || (!gPortInBackground && (sRefresh || sWantBoard || sChatOutN > 0)) ||
                         (sWantCancel && (sCancelRetryAt == 0 || SDL_GetTicks() >= sCancelRetryAt)) ||
                         sRoomPending[0] != '\0' || sPresenceLeave || sLifeReady || sLifeStarted;
                SDL_UnlockMutex(sLock);
                if (urgent) {
                    break;
                }
                if (!Match_BackgroundWait(gPortInBackground ? MATCH_BACKGROUND_POLL_MS : 20, !paired)) {
                    SDL_LockMutex(sLock);
                    if (!paired && sView.searching) {
                        sView.searching = 0;
                        sView.state = -1;
                        sView.msg = DRMSG_BACKGROUND_TIMEOUT;
                        ranked_cancel_request_locked(sView.match);
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
        /* Only "ready": the service's ready also sets the connect bit, and one request (not connect, then ready on
           the next round of the worker) is what fits before the relaunch. */
        sLifeConnect = 0;
        sLifeReady = 1;
        sLifeTries = 0;
    }
    SDL_UnlockMutex(sLock);
    start(NULL);
}

/* The lobby is connected (flow:2). The game restarts right after (relaunch), so the worker could not be trusted to
   send `ready` in time: it is woken, and this waits (at most 6 s: the worker may still be inside a poll, and the
   service is far from some players) until it has been sent. */
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
    for (i = 0; i < 600; i++) {
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
        outbox_watch(m, SDL_GetTicks()); /* old results retry in the background; they never gate the new queue */
    }
    if (session_read(oldMatch,&oldRole,&oldMode,&oldSent) && sRankedStarted && !oldSent) {
        char event[128];
        SDL_snprintf(event, sizeof(event), "ranked/stale-session discarded match=%.32s round=%u; new queue allowed", oldMatch, sRankedRound);
        ranked_debug_log_line(event);
        remove_file("dragonnet-session.txt"); /* a relaunch returned to the hub without saving a result */
        sRankedStarted = 0;
    }
    SDL_LockMutex(sLock);
    SDL_strlcpy(sPreviousQueueMatch,sLastQueueMatch,sizeof(sPreviousQueueMatch));
    SDL_strlcpy(sPreviousQueueCode,sLastQueueCode,sizeof(sPreviousQueueCode));
    SDL_UnlockMutex(sLock);
    SDL_LockMutex(sLock);
    sLastQueueMatch[0]='\0';
    sLastQueueCode[0]='\0';
    sSearchGen++;
    sHandedOver[0] = '\0';
    sView.mode = mode != 0;
    sWantSearch = sView.searching = 1;
    sSearchAt = SDL_GetTicks();
    sView.state = 1;
    sView.msg = DRMSG_SEARCHING;
    sView.http_status = 0;
    sView.error_field[0] = '\0';
    sView.match[0] = sView.code[0] = sView.server_code[0] = sView.peer[0] = sRoomPending[0] = '\0';
    sPairedAt=sCodePollAt=0;sCodePollCount=0;
    sView.msg = DRMSG_SEARCHING;
    SDL_UnlockMutex(sLock);
}

void DrRanked_ResetForNewSearch(void) {
    char match[33]="", sessionMatch[33]="";
    int role=0, mode=0, sent=0, hasSession;
    lock_make();
    hasSession=session_read(sessionMatch,&role,&mode,&sent);
    if(hasSession && (sRankedStarted || sent)) return;
    SDL_LockMutex(sLock);
    if(hasSession) SDL_strlcpy(match,sessionMatch,sizeof(match));
    else if(is_hex(sView.match,32)) SDL_strlcpy(match,sView.match,sizeof(match));
    else if(is_hex(sHandedOver,32)) SDL_strlcpy(match,sHandedOver,sizeof(match));
    if (!sCancelPending) sCancelUnconfirmed = 0;
    ranked_burn_attempt_locked();
    if(is_hex(match,32)) ranked_cancel_request_locked(match);
    sSearchGen++;
    ranked_clear_search_view_locked();
    sView.msg = DRMSG_CANCELLED;
    sView.http_status = 0;
    sView.error_field[0] = '\0';
    SDL_memset(sView.route_http, 0, sizeof(sView.route_http));
    SDL_memset(sView.route_error, 0, sizeof(sView.route_error));
    sView.cancel_pending = sCancelPending;
    sView.cancel_unconfirmed = sCancelUnconfirmed;
    SDL_UnlockMutex(sLock);
    if(hasSession) remove_file("dragonnet-session.txt");
    remove_file("dragonnet-flow.txt");
    { extern void Port_LobbyCancel(void); Port_LobbyCancel(); }
    start(NULL);
}

void DrRanked_AbortBeforeStart(void) { DrRanked_ResetForNewSearch(); }

void DrRanked_Cancel(void) {
    DrRanked_ResetForNewSearch();
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

/* ---- The choice between two fights, with the game standing still (DBT-NX's way: both games stop at the same
   blank of the result menu, so they go on alike whatever the decision is and whenever each one hears it). ---- */
volatile int gPortSeriesRequest, gPortSeriesPick, gPortSeriesLeave;
static char sSeriesAppliedMatch[33];
static unsigned sSeriesAppliedGap;
static int sSeriesAppliedReason;

int DrRanked_SeriesApplied(unsigned gap) {
    char match[33];
    int role, mode, sent;
    if (!gap || !sSeriesAppliedReason || gap != sSeriesAppliedGap || !session_read(match, &role, &mode, &sent)) return 0;
    return SDL_strcmp(match, sSeriesAppliedMatch) == 0 ? sSeriesAppliedReason : 0;
}

int DrRanked_SeriesHold(unsigned gap) {
    extern void Gs_HoldFrame(void);
    extern volatile int gPortRematchUi;
    char match[33];
    int role, mode, sent, reason = 8, picked = 0;
    Uint64 opened = SDL_GetTicks(), overAt = 0;
    DrRankedSeries s;
    if (!gap || !session_read(match, &role, &mode, &sent)) return 8;
    DrRanked_SeriesChoice(gap, 0);
    gPortSeriesPick = gPortSeriesLeave = 0;
    gPortSeriesRequest = (int)gap; /* (ui.cpp opens the SERIES screen) */
    fprintf(stderr, "dragonnet: series hold gap=%u\n", gap);
    for (;;) {
        Uint64 now = SDL_GetTicks();
        DrRanked_SeriesPoll(&s);
        if (s.decision == DR_SERIES_CONTINUE) { reason = 0x8000; break; }
        if (s.decision == DR_SERIES_TEAM) { reason = 0x10; break; }
        if (s.decision == DR_SERIES_OVER) {
            if (!overAt) overAt = now ? now : 1;
            /* the series' result stays up until the player goes on (after 2 s) or 15 s, or at once without an overlay */
            if (!gPortRematchUi || gPortSeriesLeave || now - overAt >= 15000) { reason = 8; break; }
        }
        if (!picked && gPortSeriesPick >= 1 && gPortSeriesPick <= 3) {
            picked = gPortSeriesPick;
            DrRanked_SeriesChoice(gap, picked);
        }
        if (!picked && !s.decision && now - opened >= 20000) { /* (nobody chose: "continue", as the service decides) */
            picked = 1;
            DrRanked_SeriesChoice(gap, 1);
        }
        if (!s.decision && now - opened >= 90000) { /* the service never answered: the series ends here */
            reason = 8;
            break;
        }
        Gs_HoldFrame();
        SDL_Delay(16);
    }
    gPortSeriesRequest = 0;
    SDL_strlcpy(sSeriesAppliedMatch, match, sizeof(sSeriesAppliedMatch));
    sSeriesAppliedGap = gap;
    sSeriesAppliedReason = reason;
    fprintf(stderr, "dragonnet: series gap=%u decision=%d applied=0x%x\n", gap, s.decision, (unsigned)reason);
    return reason;
}

void DrRanked_SetRoom(const char *code) {
    char match[33];
    if (code == NULL) return;
    lock_make();
    SDL_LockMutex(sLock);
    SDL_strlcpy(match, sView.match, sizeof(match));
    SDL_UnlockMutex(sLock);
    DrRanked_SetRoomForMatch(match, code);
}

void DrRanked_SetRoomForMatch(const char *match, const char *code) {
    if (match == NULL || code == NULL || !is_hex(match, 32) || SDL_strlen(code) != 6) return;
    lock_make();
    SDL_LockMutex(sLock);
    if (sWantSearch && sView.role == 1 && SDL_strcmp(match, sView.match) == 0 && SDL_strcmp(code, sView.code) != 0) {
        SDL_strlcpy(sRoomPending, code, sizeof(sRoomPending));
        SDL_strlcpy(sRoomPendingMatch, match, sizeof(sRoomPendingMatch));
    }
    SDL_UnlockMutex(sLock);
}

void DrRanked_RecordLobbyCode(const char *match, const char *code, int host) {
    if (match == NULL || code == NULL || !is_hex(match, 32) || SDL_strlen(code) != 6) return;
    lock_make();
    SDL_LockMutex(sLock);
    if (sWantSearch && SDL_strcmp(match, sView.match) == 0 && (host == 0 || sView.role == 1)) attempt_code_locked(code);
    SDL_UnlockMutex(sLock);
}

void DrRanked_BurnLobbyCode(const char *match, const char *code) {
    char event[128];
    if (code == NULL || SDL_strlen(code) != 6) return;
    lock_make();
    SDL_LockMutex(sLock);
    if (match == NULL || !sView.match[0] || SDL_strcmp(match, sView.match) == 0) burn_code_locked(code);
    SDL_UnlockMutex(sLock);
    SDL_snprintf(event, sizeof(event), "ranked/code burned match=%.32s code=%.6s", match ? match : "-", code);
    ranked_debug_log_line(event);
}

int DrRanked_CodeIsBurned(const char *code) {
    int burned;
    if (code == NULL) return 0;
    lock_make();
    SDL_LockMutex(sLock);burned=code_burned_locked(code);SDL_UnlockMutex(sLock);
    return burned;
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
    sRankedLaunchPending = 1;
    fprintf(stderr, "dragonnet: ranked match handed over (role %d)\n", v.role);
    return 1;
}

/* A session begins (net.c). One that Ranked did not hand over (a friendly room, a lobby, a quick match) must not
   be taken for a ranked one because of a dragonnet-session.txt left behind by a ranked match whose session never
   began: that file makes the series' hold, the forced rules and the ranked results apply. (A program started as the
   session, without the in-process change, has only the file to go by: it is kept.) */
void DrRanked_SessionBegins(void) {
    extern int Port_SessionCan(void);
    char match[33];
    int role, mode, sent;
    if (!sRankedLaunchPending && Port_SessionCan() && session_read(match, &role, &mode, &sent)) {
        remove_file("dragonnet-session.txt");
        fprintf(stderr, "dragonnet: a ranked session file was left from match %.8s: this session is not a ranked one\n", match);
    }
    sRankedLaunchPending = 0;
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
    /* Result scores use the host's viewpoint on both devices: 2 = host won, 0 = host lost, 1 = draw. */
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
    SDL_snprintf(diag, sizeof(diag), "finish=enviado score=%d(host:2 ganhou/0 perdeu/1 empate) round=%u match=%.8s digest=%s",
                 score, sRankedRound, match, digest);
    finish_diag(diag);
    start(NULL); /* (sent by the thread, now or the next time the online screen is open) */
}

/* A cleared result starts the next fight in the same best-of-three match. */
void DrRanked_BattleStart(void) {
    char match[33]; int role, mode, sent;
    if (!session_read(match, &role, &mode, &sent) || !sent) return;
    if (sRankedRound < 6) ++sRankedRound; /* (a series has up to six fights: draws are played again) */
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
