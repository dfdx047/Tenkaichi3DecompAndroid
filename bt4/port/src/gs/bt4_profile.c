/*
 * BT4 on Android: the online profile. Nickname (unique, kept by the service), medals, the weekly Ranked season and
 * its trophies, through the service in port/bt4perfil-worker (worker.js; its own Cloudflare Worker, not Dragon Net's
 * and not NauTaro's). The PC build gets these functions from gs/match_supabase.cpp instead: all of this file is
 * Android's.
 *
 * The account: a random id and secret the service gives at the first nickname, kept in bt4_profile_account.txt (the
 * app's own folder) and in the settings (bt4_profile_id / bt4_profile_key), so that losing one keeps the other.
 *
 * A battle counts when both players tell the service of it in agreement (worker.js): every copy reports its own
 * battles as they end (Bt4Profile_Observe, from gs/net.c), with the session's room code, the battle's number in the
 * session, both names, its side and the winner. Ranked is a quick match in a queue of its own (Dragon Net's "D20",
 * which BT4's one on one never uses): both players of a Ranked session are Ranked.
 *
 * Everything that talks to the service runs on one thread of its own; the game's and the menus' threads only read
 * what it last heard, under a lock.
 */
#ifdef __ANDROID__
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt4_profile.h"
#include "match_supabase.h"

#define BT4_ONLINE_URL_DEFAULT "https://bt4-online.rankedmatch.workers.dev"
#define ACCOUNT_FILE "bt4_profile_account.txt"
#define TOP_MAX 50
#define REPORTS 8

extern int Match_HttpPost(const char *url, const char *json, char *out, size_t size);
extern const char *Port_NetNameGet(void);
extern void Port_UiBanner(const char *text, int ms);
extern int Port_TrLine(const char *original, char *out, int size);
extern const char *PortOnline_Text(const char *text);
extern void Port_SettingText(const char *name, const char *def, char *out, int size);
extern void Port_SettingTextSave(const char *name, const char *value);
extern void Port_SettingsWrite(void);

typedef struct {
    int used, tries;
    Uint64 next, until;
    char body[512];
    int ranked;
} Report;

static struct {
    SDL_Mutex *lock;
    SDL_Condition *wake;
    SDL_Thread *thread;
    /* the account */
    int loaded, has, saveSettings;
    char id[33], secret[65];
    /* the nickname */
    int nick;              /* BT4_NICKNAME_* */
    char name[20], want[20];
    int wantName;
    /* the profile (/v1/me) */
    int meState, wantMe;
    Uint64 meAt;
    Bt4ProfileStats stats;
    /* the season's table (/v1/top) */
    int topState, wantTop, topTotal, page;
    Uint64 topAt;
    Bt4RankedEntry top[TOP_MAX];
    int topExtra[TOP_MAX][5]; /* each one's gold and silver medals, gold/silver/bronze trophies */
    int topCount;
    /* who is online (/v1/presence): asked every two minutes while the online screens are open */
    int wantPresence, presenceActive, online;
    Uint64 presenceAt;
    char who[20];
    /* reports of battles, and their answers for the game's thread */
    Report rep[REPORTS];
    char banner[4][160];
    int banners;
} sP;

/* the game's thread only */
static struct {
    int session, ranked, battle, sawFight, outcomeBits, warned;
    unsigned outcomeTick;
    char code[12];
} sG;
static int sRankedNext; /* (the menu asked for Ranked: kept for the PC's meaning; the queue decides) */
static SDL_AtomicInt sRankedActive;

const char *Bt4Online_Url(void) {
    const char *e = SDL_getenv("BT4_ONLINE_URL");
    return e != NULL && e[0] != '\0' ? e : BT4_ONLINE_URL_DEFAULT;
}
static const char *profile_url(void) { return Bt4Online_Url(); }

/* ---- tiny JSON readers (the service's answers are plain: no escapes in names, see worker.js cleanName) ---- */
static const char *jfind(const char *json, const char *key) {
    char k[40];
    const char *p;
    SDL_snprintf(k, sizeof(k), "\"%s\":", key);
    p = SDL_strstr(json, k);
    return p != NULL ? p + SDL_strlen(k) : NULL;
}
static int jint(const char *json, const char *key, int def) {
    const char *p = jfind(json, key);
    return p != NULL && (*p == '-' || (*p >= '0' && *p <= '9')) ? SDL_atoi(p) : def;
}
static double jnum(const char *json, const char *key) {
    const char *p = jfind(json, key);
    return p != NULL ? SDL_strtod(p, NULL) : 0.0;
}
static int jstr(const char *json, const char *key, char *out, size_t size) {
    const char *p = jfind(json, key), *e;
    out[0] = '\0';
    if (p == NULL || *p != '"') return 0;
    e = SDL_strchr(p + 1, '"');
    if (e == NULL || (size_t)(e - p - 1) >= size) return 0;
    SDL_memcpy(out, p + 1, (size_t)(e - p - 1));
    out[e - p - 1] = '\0';
    return 1;
}
/* a string for a JSON body: quotes and backslashes escaped, control characters dropped */
static void jesc(char *out, size_t size, const char *s) {
    size_t n = 0;
    for (; s != NULL && *s != '\0' && n + 3 < size; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20) continue;
        if (c == '"' || c == '\\') out[n++] = '\\';
        out[n++] = (char)c;
    }
    out[n] = '\0';
}
static void random_hex(char *out, int bytes) {
    static const char hex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < bytes; i++) {
        unsigned v = (unsigned)SDL_rand_bits();
        out[i * 2] = hex[(v >> 4) & 15];
        out[i * 2 + 1] = hex[v & 15];
    }
    out[bytes * 2] = '\0';
}

static int post(const char *what, const char *body, char *out, size_t size) {
    char url[256];
    SDL_snprintf(url, sizeof(url), "%s/v1/%s", profile_url(), what);
    if (!Match_HttpPost(url, body, out, size)) return 0;
    return out[0] == '{';
}

/* ---- the account ---- */
static int account_valid(const char *id, const char *secret) {
    size_t i;
    if (SDL_strlen(id) != 32 || SDL_strlen(secret) != 64) return 0;
    for (i = 0; id[i]; i++) if (!SDL_isxdigit((unsigned char)id[i])) return 0;
    for (i = 0; secret[i]; i++) if (!SDL_isxdigit((unsigned char)secret[i])) return 0;
    return 1;
}
/* (the menus' thread, the first time: the settings are read there) */
static void account_load(void) {
    char id[64] = "", secret[96] = "";
    FILE *f;
    if (sP.loaded) return;
    sP.loaded = 1;
    f = fopen(ACCOUNT_FILE, "rb");
    if (f != NULL) {
        char line[200] = "";
        size_t n = fread(line, 1, sizeof(line) - 1, f);
        fclose(f);
        line[n] = '\0';
        if (SDL_sscanf(line, "%63s %95s", id, secret) != 2) id[0] = secret[0] = '\0';
    }
    if (!account_valid(id, secret)) {
        Port_SettingText("bt4_profile_id", "", id, sizeof(id));
        Port_SettingText("bt4_profile_key", "", secret, sizeof(secret));
    }
    if (account_valid(id, secret)) {
        SDL_strlcpy(sP.id, id, sizeof(sP.id));
        SDL_strlcpy(sP.secret, secret, sizeof(sP.secret));
        sP.has = 1;
        sP.saveSettings = 1; /* (both places hold it) */
    }
}
/* (the service's thread, under the lock) */
static void account_keep(const char *id, const char *secret) {
    FILE *f;
    SDL_strlcpy(sP.id, id, sizeof(sP.id));
    SDL_strlcpy(sP.secret, secret, sizeof(sP.secret));
    sP.has = 1;
    sP.saveSettings = 1;
    f = fopen(ACCOUNT_FILE ".new", "wb");
    if (f != NULL) {
        fprintf(f, "%s %s\n", id, secret);
        fclose(f);
        remove(ACCOUNT_FILE);
        rename(ACCOUNT_FILE ".new", ACCOUNT_FILE);
    }
}
/* (the menus' or the game's thread: the settings file is theirs) */
static void account_settings(void) {
    char id[33], secret[65];
    int save;
    SDL_LockMutex(sP.lock);
    save = sP.saveSettings;
    sP.saveSettings = 0;
    SDL_strlcpy(id, sP.id, sizeof(id));
    SDL_strlcpy(secret, sP.secret, sizeof(secret));
    SDL_UnlockMutex(sP.lock);
    if (save) {
        char was[96];
        Port_SettingText("bt4_profile_id", "", was, sizeof(was));
        if (SDL_strcmp(was, id) != 0) {
            Port_SettingTextSave("bt4_profile_id", id);
            Port_SettingTextSave("bt4_profile_key", secret);
            Port_SettingsWrite();
        }
    }
}

/* ---- the service's thread ---- */
static void take_me(const char *a) {
    const char *t, *l;
    int i;
    Bt4ProfileStats *s = &sP.stats;
    double ends = jnum(a, "ends");
    SDL_Time real = 0;
    s->gold = jint(a, "gold", 0);
    s->silver = jint(a, "silver", 0);
    s->wins = jint(a, "wins", 0);
    s->losses = jint(a, "losses", 0);
    s->rating = jint(a, "rating", 1000);
    s->rwins = jint(a, "rwins", 0);
    s->rlosses = jint(a, "rlosses", 0);
    s->rank = jint(a, "rank", 0);
    s->players = jint(a, "players", 0);
    s->rated = jint(a, "rated", s->rwins + s->rlosses);
    s->opponents = jint(a, "opponents", 0);
    s->eligible = SDL_strstr(a, "\"eligible\":true") != NULL;
    if (SDL_GetCurrentTime(&real)) {
        double left = ends - (double)(real / 1000000);
        s->hoursLeft = left > 0 ? (int)(left / 3600000.0) : 0;
    }
    s->trophies[0] = s->trophies[1] = s->trophies[2] = 0;
    t = jfind(a, "trophies");
    if (t != NULL && *t == '[') {
        SDL_sscanf(t, "[%d,%d,%d]", &s->trophies[0], &s->trophies[1], &s->trophies[2]);
    }
    for (i = 0; i < 3; i++) s->last[i][0] = '\0';
    l = jfind(a, "last");
    if (l != NULL && *l == '[') { /* [[1,"name",1100],[2,"name",1050]] */
        const char *p = l + 1;
        while (*p == '[') {
            int place = SDL_atoi(p + 1);
            const char *q = SDL_strchr(p, '"'), *e = q != NULL ? SDL_strchr(q + 1, '"') : NULL;
            if (q == NULL || e == NULL) break;
            if (place >= 1 && place <= 3 && (size_t)(e - q - 1) < sizeof(s->last[0])) {
                SDL_memcpy(s->last[place - 1], q + 1, (size_t)(e - q - 1));
                s->last[place - 1][e - q - 1] = '\0';
            }
            p = SDL_strchr(e, ']');
            if (p == NULL) break;
            p++;
            if (*p == ',') p++;
        }
    }
}
static void take_top(const char *a) {
    const char *p = jfind(a, "top");
    int n = 0;
    sP.topTotal = jint(a, "total", 0);
    if (p != NULL && *p == '[') {
        p++;
        while (*p == '[' && n < TOP_MAX) {
            Bt4RankedEntry *e = &sP.top[n];
            const char *q = SDL_strchr(p, '"'), *qe = q != NULL ? SDL_strchr(q + 1, '"') : NULL;
            if (q == NULL || qe == NULL) break;
            int *x = sP.topExtra[n];
            SDL_memset(e, 0, sizeof(*e));
            SDL_memset(x, 0, sizeof(sP.topExtra[0]));
            if ((size_t)(qe - q - 1) < sizeof(e->name)) {
                SDL_memcpy(e->name, q + 1, (size_t)(qe - q - 1));
            }
            if (SDL_sscanf(qe + 1, ",%d,%d,%d", &e->rating, &e->wins, &e->losses) != 3) break;
            SDL_sscanf(qe + 1, ",%*d,%*d,%*d,%d,%d,%d,%d,%d", &x[0], &x[1], &x[2], &x[3], &x[4]); /* (an older service: zeros) */
            n++;
            p = SDL_strchr(qe, ']');
            if (p == NULL) break;
            p++;
            if (*p == ',') p++;
        }
    }
    sP.topCount = n;
}

/* the lines for the game's thread (shown by Bt4Profile_Pump) */
static void banner_add(const char *line) {
    if (sP.banners < 4) SDL_strlcpy(sP.banner[sP.banners++], line, sizeof(sP.banner[0]));
}
static void take_result(const char *a, int ranked) {
    char state[16] = "", medal[16] = "", line[160], fmt[160];
    jstr(a, "state", state, sizeof(state));
    jstr(a, "medal", medal, sizeof(medal));
    if (SDL_strcmp(state, "settled") != 0) return;
    if (SDL_strcmp(medal, "none") == 0) { /* (the day's limit of battles between these two players that give medals) */
        banner_add("Battle counted. No more medals today against this rival.");
        sP.wantMe = 1;
        return;
    }
    if (ranked && jfind(a, "delta") == NULL && SDL_strstr(a, "\"capped\":true") != NULL) {
        banner_add(SDL_strcmp(medal, "gold") == 0 ? "Gold medal! No more Ranked points today against this rival." : "Silver medal. No more Ranked points today against this rival.");
        sP.wantMe = 1;
        return;
    }
    if (ranked && jfind(a, "delta") != NULL) {
        const char *f = SDL_strcmp(medal, "gold") == 0 ? "Gold medal! Ranked: %+d points (%d total)" : "Silver medal. Ranked: %+d points (%d total)";
        if (!Port_TrLine(f, fmt, sizeof(fmt)) || SDL_strstr(fmt, "%+d") == NULL) SDL_strlcpy(fmt, PortOnline_Text(f), sizeof(fmt));
        SDL_snprintf(line, sizeof(line), fmt, jint(a, "delta", 0), jint(a, "rating", 0));
    } else {
        SDL_strlcpy(line, SDL_strcmp(medal, "gold") == 0 ? "Gold medal! Victory counted in your Profile." : "Silver medal. The battle counted in your Profile.", sizeof(line));
    }
    banner_add(line);
    sP.wantMe = 1;
    sP.wantTop = sP.wantTop || ranked;
}

static int SDLCALL profile_thread(void *arg) {
    char body[640], a[8192];
    (void)arg;
    for (;;) {
        int job = 0, i;
        Uint64 now;
        SDL_LockMutex(sP.lock);
        for (;;) {
            now = SDL_GetTicks();
            if (sP.wantName) { job = 1; break; }
            if (sP.wantMe && sP.has) { job = 2; break; }
            if (sP.wantTop) { job = 3; break; }
            if (sP.wantPresence) { job = 4; break; }
            for (i = 0; i < REPORTS; i++) {
                if (sP.rep[i].used && now >= sP.rep[i].next) { job = 10 + i; break; }
            }
            if (job) break;
            SDL_WaitConditionTimeout(sP.wake, sP.lock, 1000);
        }
        if (job == 1) { /* the nickname: a new account, or the account's new name */
            char name[40];
            int had = sP.has;
            jesc(name, sizeof(name), sP.want);
            sP.wantName = 0;
            if (had) SDL_snprintf(body, sizeof(body), "{\"id\":\"%s\",\"secret\":\"%s\",\"name\":\"%s\"}", sP.id, sP.secret, name);
            else SDL_snprintf(body, sizeof(body), "{\"name\":\"%s\"}", name);
            SDL_UnlockMutex(sP.lock);
            i = post(had ? "rename" : "register", body, a, sizeof(a));
            if (i && had && SDL_strstr(a, "\"error\":\"who\"") != NULL) { /* the service no longer knows this account */
                SDL_snprintf(body, sizeof(body), "{\"name\":\"%s\"}", name);
                had = 0;
                i = post("register", body, a, sizeof(a));
            }
            SDL_LockMutex(sP.lock);
            if (!i) {
                sP.nick = BT4_NICKNAME_UNAVAILABLE;
            } else if (SDL_strstr(a, "\"ok\":true") != NULL) {
                char id[40], secret[80], got[40];
                if (!had && jstr(a, "id", id, sizeof(id)) && jstr(a, "secret", secret, sizeof(secret)) && account_valid(id, secret)) {
                    account_keep(id, secret);
                }
                if (sP.has && jstr(a, "name", got, sizeof(got))) {
                    SDL_strlcpy(sP.name, got, sizeof(sP.name));
                    sP.nick = BT4_NICKNAME_READY;
                    sP.wantMe = 1;
                } else {
                    sP.nick = BT4_NICKNAME_UNAVAILABLE;
                }
            } else if (SDL_strstr(a, "\"taken\"") != NULL) {
                sP.nick = BT4_NICKNAME_TAKEN;
            } else if (SDL_strstr(a, "\"invalid\"") != NULL) {
                sP.nick = BT4_NICKNAME_INVALID;
            } else {
                sP.nick = BT4_NICKNAME_UNAVAILABLE;
            }
            SDL_UnlockMutex(sP.lock);
        } else if (job == 2) {
            sP.wantMe = 0;
            SDL_snprintf(body, sizeof(body), "{\"id\":\"%s\",\"secret\":\"%s\"}", sP.id, sP.secret);
            SDL_UnlockMutex(sP.lock);
            i = post("me", body, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            sP.meAt = SDL_GetTicks();
            if (i && SDL_strstr(a, "\"ok\":true") != NULL) {
                char got[40];
                take_me(a);
                sP.meState = 1;
                if (jstr(a, "name", got, sizeof(got)) && sP.nick != BT4_NICKNAME_SAVING) {
                    SDL_strlcpy(sP.name, got, sizeof(sP.name));
                    sP.nick = BT4_NICKNAME_READY;
                }
            } else if (i && SDL_strstr(a, "\"error\":\"who\"") != NULL) {
                /* (the account is not the service's: the nickname is to be chosen again, which makes a new one; the
                   old id stays in the file until then, as R5.7 keeps a rejected session's account) */
                sP.meState = -1;
                if (sP.nick != BT4_NICKNAME_SAVING) sP.nick = BT4_NICKNAME_OPTIONAL;
                sP.has = 0;
            } else {
                sP.meState = -1;
                if (sP.nick == BT4_NICKNAME_LOADING) sP.nick = BT4_NICKNAME_UNAVAILABLE;
            }
            SDL_UnlockMutex(sP.lock);
        } else if (job == 3) {
            sP.wantTop = 0;
            SDL_UnlockMutex(sP.lock);
            i = post("top", "{}", a, sizeof(a));
            SDL_LockMutex(sP.lock);
            sP.topAt = SDL_GetTicks();
            if (i && SDL_strstr(a, "\"ok\":true") != NULL) {
                take_top(a);
                sP.topState = 1;
            } else if (sP.topState != 1) {
                sP.topState = -1;
            }
            SDL_UnlockMutex(sP.lock);
        } else if (job == 4) {
            int active = sP.presenceActive;
            sP.wantPresence = 0;
            SDL_snprintf(body, sizeof(body), "{\"who\":\"%s\",\"active\":%s}", sP.who, active ? "true" : "false");
            SDL_UnlockMutex(sP.lock);
            i = post("presence", body, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            if (i && SDL_strstr(a, "\"ok\":true") != NULL) {
                sP.online = jint(a, "online", -1);
            }
            SDL_UnlockMutex(sP.lock);
        } else { /* a battle's report: again until both have told of it (or three minutes) */
            Report *r = &sP.rep[job - 10];
            char send[640];
            char state[16] = "";
            int ranked = r->ranked;
            SDL_strlcpy(send, r->body, sizeof(send));
            r->next = now + 60000; /* (taken; set again below) */
            SDL_UnlockMutex(sP.lock);
            i = post("report", send, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            now = SDL_GetTicks();
            r->tries++;
            if (i) jstr(a, "state", state, sizeof(state));
            if (i && SDL_strstr(a, "\"ok\":true") != NULL && SDL_strcmp(state, "waiting") != 0) {
                take_result(a, ranked);
                r->used = 0;
            } else if (i && SDL_strstr(a, "\"error\"") != NULL && SDL_strstr(a, "\"slow\"") == NULL) {
                SDL_Log("bt4 profile: report refused: %.120s", a);
                r->used = 0;
            } else if (now >= r->until) {
                SDL_Log("bt4 profile: a battle's report was not paired in time (%s)", i ? state : "no answer");
                r->used = 0;
            } else {
                r->next = now + (r->tries < 4 ? 3000 : r->tries < 8 ? 10000 : 30000); /* (a rival who never tells: few asks) */
            }
            SDL_UnlockMutex(sP.lock);
        }
    }
    return 0;
}

static void profile_start(void) {
    if (sP.lock != NULL) return;
    sP.lock = SDL_CreateMutex();
    sP.wake = SDL_CreateCondition();
    sP.nick = BT4_NICKNAME_LOADING;
    sP.topState = 0;
    sP.online = -1;
    random_hex(sP.who, 8); /* (this run's word for /v1/presence: not the account, nothing that stays) */
    account_load();
    if (!sP.has) sP.nick = BT4_NICKNAME_OPTIONAL;
    sP.thread = SDL_CreateThread(profile_thread, "bt4-profile", NULL);
    if (sP.thread != NULL) SDL_DetachThread(sP.thread);
}
static void wake(void) { SDL_SignalCondition(sP.wake); }

/* ---- the nickname (src/menu/port_tbo.c's Profile and name screens) ---- */
void Port_OnlineNicknameBegin(void) {
    profile_start();
    account_settings();
    SDL_LockMutex(sP.lock);
    if (sP.has) {
        if (sP.nick != BT4_NICKNAME_SAVING && sP.nick != BT4_NICKNAME_READY) sP.nick = BT4_NICKNAME_LOADING;
        if (sP.meAt == 0 || SDL_GetTicks() - sP.meAt > 5000) sP.wantMe = 1;
    } else if (sP.nick != BT4_NICKNAME_SAVING) {
        sP.nick = BT4_NICKNAME_OPTIONAL; /* (a name of this device's own until one is chosen: online play is open) */
    }
    SDL_UnlockMutex(sP.lock);
    wake();
}
int Port_OnlineNicknameView(char name[20]) {
    int state;
    profile_start();
    account_settings();
    SDL_LockMutex(sP.lock);
    state = sP.nick;
    if (name != NULL) SDL_strlcpy(name, sP.nick == BT4_NICKNAME_READY ? sP.name : Port_NetNameGet(), 20);
    SDL_UnlockMutex(sP.lock);
    return state;
}
int Port_OnlineNicknameSubmit(const char *name) {
    profile_start();
    if (name == NULL || name[0] == '\0') return 0;
    SDL_LockMutex(sP.lock);
    if (sP.nick == BT4_NICKNAME_SAVING) {
        SDL_UnlockMutex(sP.lock);
        return 0;
    }
    SDL_strlcpy(sP.want, name, sizeof(sP.want));
    sP.wantName = 1;
    sP.nick = BT4_NICKNAME_SAVING;
    SDL_UnlockMutex(sP.lock);
    wake();
    return 1;
}

/* ---- Ranked's screens ---- */
void Port_RankedRefresh(int refreshStatus) {
    Uint64 now;
    profile_start();
    SDL_LockMutex(sP.lock);
    now = SDL_GetTicks();
    if (refreshStatus && sP.has && (sP.meAt == 0 || now - sP.meAt > 5000)) sP.wantMe = 1;
    if (sP.topAt == 0 || now - sP.topAt > 30000) sP.wantTop = 1;
    SDL_UnlockMutex(sP.lock);
    wake();
}
void Port_RankedView(Bt4RankedView *out) {
    int i;
    profile_start();
    account_settings();
    SDL_memset(out, 0, sizeof(*out));
    SDL_LockMutex(sP.lock);
    out->state = !sP.has ? -1 : sP.meState;
    out->rating = sP.stats.rating;
    out->wins = sP.stats.rwins;
    out->losses = sP.stats.rlosses;
    out->topState = sP.topState == 1 && sP.topCount == 0 ? 1 : sP.topState;
    out->pages = sP.topCount > 0 ? (sP.topCount + 4) / 5 : 1;
    if (sP.page >= out->pages) sP.page = out->pages - 1;
    if (sP.page < 0) sP.page = 0;
    out->page = sP.page;
    out->count = sP.topCount - out->page * 5;
    if (out->count > 5) out->count = 5;
    if (out->count < 0) out->count = 0;
    for (i = 0; i < out->count; i++) out->top[i] = sP.top[out->page * 5 + i];
    SDL_UnlockMutex(sP.lock);
}
int Port_RankedTopTen(Bt4RankedEntry out[10]) {
    int n;
    profile_start();
    SDL_LockMutex(sP.lock);
    if (sP.topState == 0 && sP.topCount == 0) {
        n = -2; /* (loading: what port_tbo.c says "Loading Global Top..." for) */
    } else if (sP.topState < 0 && sP.topCount == 0) {
        n = -1; /* (unavailable) */
    } else {
        n = sP.topCount < 10 ? sP.topCount : 10;
        SDL_memcpy(out, sP.top, sizeof(out[0]) * (size_t)n);
    }
    SDL_UnlockMutex(sP.lock);
    return n;
}
void Bt4Profile_TopRow(int i, int out[5]) {
    int k;
    profile_start();
    SDL_LockMutex(sP.lock);
    k = sP.page * 5 + i;
    if (i >= 0 && i < 5 && k >= 0 && k < sP.topCount) SDL_memcpy(out, sP.topExtra[k], sizeof(sP.topExtra[0]));
    else SDL_memset(out, 0, sizeof(sP.topExtra[0]));
    SDL_UnlockMutex(sP.lock);
}
void Port_RankedPage(int direction) {
    profile_start();
    SDL_LockMutex(sP.lock);
    if (direction == 0) sP.page = 0;
    else {
        int pages = sP.topCount > 0 ? (sP.topCount + 4) / 5 : 1;
        sP.page = (sP.page + direction + pages) % pages;
    }
    SDL_UnlockMutex(sP.lock);
}
void Port_RankedSearch(int selected) { sRankedNext = selected != 0; }
int Port_RankedActive(void) { return SDL_GetAtomicInt(&sRankedActive); }
/* (Android: every battle, Ranked or not, goes through Bt4Profile_Observe; these keep the PC's names answering) */
void Port_RankedObserve(int battleState, int bits, unsigned tick, unsigned confirmed, int resimulating, int localConcession) {
    (void)battleState; (void)bits; (void)tick; (void)confirmed; (void)resimulating; (void)localConcession;
}
int Port_RankedResult(int *rating, int *delta) {
    if (rating) *rating = 0;
    if (delta) *delta = 0;
    return 0;
}
void Port_RankedDisconnected(int peerGone) { (void)peerGone; }
int Port_RankedTakeSettlement(int *rating, int *delta) {
    (void)rating; (void)delta;
    Bt4Profile_Pump();
    return 0;
}
/* The online screens' "Fighters online": this game says it is there every two minutes while they are open (and that it
   left, once, when they close); the count of the last answer, -1 until one came. */
int Port_OnlinePresence(int active) {
    int online;
    Uint64 now;
    profile_start();
    SDL_LockMutex(sP.lock);
    now = SDL_GetTicks();
    if (active && (!sP.presenceActive || sP.presenceAt == 0 || now - sP.presenceAt >= 120000)) {
        sP.presenceActive = 1;
        sP.presenceAt = now;
        sP.wantPresence = 1;
        SDL_SignalCondition(sP.wake);
    } else if (!active && sP.presenceActive) {
        sP.presenceActive = 0;
        sP.presenceAt = 0;
        sP.wantPresence = 1;
        SDL_SignalCondition(sP.wake);
    }
    online = sP.online;
    SDL_UnlockMutex(sP.lock);
    return online;
}

/* ---- the Profile's card ---- */
int Bt4Profile_HasAccount(void) {
    int has;
    profile_start();
    SDL_LockMutex(sP.lock);
    has = sP.has;
    SDL_UnlockMutex(sP.lock);
    return has;
}
void Bt4Profile_Stats(Bt4ProfileStats *out) {
    profile_start();
    account_settings();
    SDL_LockMutex(sP.lock);
    *out = sP.stats;
    out->state = !sP.has ? -2 : sP.meState;
    if (sP.has && (sP.meAt == 0 || SDL_GetTicks() - sP.meAt > 60000) && !sP.wantMe) {
        sP.wantMe = 1;
        SDL_SignalCondition(sP.wake);
    }
    SDL_UnlockMutex(sP.lock);
}

/* ---- battles (gs/net.c, the game's thread) ---- */
void Bt4Profile_SessionBegin(int ranked, const char *code) {
    profile_start();
    SDL_memset(&sG, 0, sizeof(sG));
    sG.session = 1;
    sG.ranked = ranked != 0;
    SDL_strlcpy(sG.code, code != NULL ? code : "", sizeof(sG.code));
    SDL_SetAtomicInt(&sRankedActive, sG.ranked);
    sRankedNext = 0;
    SDL_Log("bt4 profile: session begins (%s, room %s)", sG.ranked ? "Ranked" : "normal", sG.code[0] ? sG.code : "-");
}
void Bt4Profile_SessionEnd(void) {
    sG.session = 0;
    SDL_SetAtomicInt(&sRankedActive, 0);
}

static void report(int winner, int me, const char *name0, const char *name1) {
    char rkey[17], mine[40], theirs[40], code[24];
    int i;
    const char *myName = me ? name1 : name0, *their = me ? name0 : name1;
    if (myName == NULL || their == NULL || myName[0] == '\0' || their[0] == '\0') return;
    random_hex(rkey, 8);
    jesc(mine, sizeof(mine), myName);
    jesc(theirs, sizeof(theirs), their);
    jesc(code, sizeof(code), sG.code);
    SDL_LockMutex(sP.lock);
    if (!sP.has) {
        SDL_UnlockMutex(sP.lock);
        if (!sG.warned) {
            sG.warned = 1;
            Port_UiBanner("Choose a nickname in Profile to win medals.", 3000);
        }
        return;
    }
    for (i = 0; i < REPORTS && sP.rep[i].used; i++) {}
    if (i == REPORTS) i = 0; /* (the oldest place: eight battles unanswered in three minutes is a dead service) */
    SDL_memset(&sP.rep[i], 0, sizeof(sP.rep[i]));
    sP.rep[i].used = 1;
    sP.rep[i].ranked = sG.ranked;
    sP.rep[i].next = SDL_GetTicks();
    sP.rep[i].until = sP.rep[i].next + 180000;
    SDL_snprintf(sP.rep[i].body, sizeof(sP.rep[i].body),
                 "{\"id\":\"%s\",\"secret\":\"%s\",\"rkey\":\"%s\",\"code\":\"%s\",\"n\":%d,\"me\":\"%s\",\"opp\":\"%s\",\"side\":%d,\"winner\":%d,\"ranked\":%s}",
                 sP.id, sP.secret, rkey, code, sG.battle, mine, theirs, me, winner, sG.ranked ? "true" : "false");
    SDL_UnlockMutex(sP.lock);
    SDL_Log("bt4 profile: battle %d of room %s: winner side %d (this player side %d)", sG.battle, sG.code[0] ? sG.code : "-", winner, me);
    wake();
}

/* A battle's end, as Ranked's observer saw it on the PC (gs/match_supabase.cpp Port_RankedObserve): the fight seen,
   then a result that holds past the rollback window and the other's confirmed input. A surrender from the pause menu
   (bits 8) is a loss for the one who gave up (BT4decomp R5.4). */
void Bt4Profile_Observe(int battleState, int bits, unsigned tick, unsigned confirmed, int resimulating, int localConcession,
                        int me, const char *name0, const char *name1) {
    int winner;
    if (!sG.session || resimulating || (me != 0 && me != 1)) return;
    if (battleState == 3) {
        if (!sG.sawFight) {
            sG.sawFight = 1;
            sG.battle++;
        }
        sG.outcomeBits = 0;
        return;
    }
    if (!sG.sawFight || battleState < 4 || battleState > 6 || !(bits & 15)) {
        sG.outcomeBits = 0;
        return;
    }
    bits = bits & 8 ? 8 : bits & 7;
    if (sG.outcomeBits != bits) {
        sG.outcomeBits = bits;
        sG.outcomeTick = tick;
        return;
    }
    if (bits == 8) {
        if (localConcession < 0 || confirmed < sG.outcomeTick + 2) return;
        winner = localConcession ? 1 - me : me;
    } else {
        if (tick - sG.outcomeTick < 20 || confirmed < sG.outcomeTick + 2) return;
        winner = (bits & 3) == 1 ? 0 : (bits & 3) == 2 ? 1 : 2;
    }
    sG.sawFight = 0;
    sG.outcomeBits = 0;
    report(winner, me, name0, name1);
}

void Bt4Profile_Pump(void) {
    static Uint64 shown;
    char line[160];
    int have = 0;
    if (sP.lock == NULL || (shown != 0 && SDL_GetTicks() - shown < 3800)) return; /* (one line at a time) */
    SDL_LockMutex(sP.lock);
    if (sP.banners > 0) {
        SDL_strlcpy(line, sP.banner[0], sizeof(line));
        SDL_memmove(sP.banner[0], sP.banner[1], sizeof(sP.banner[0]) * 3);
        sP.banners--;
        have = line[0] != '\0';
    }
    SDL_UnlockMutex(sP.lock);
    if (have) {
        shown = SDL_GetTicks();
        Port_UiBanner(line, 3500);
    }
}
#endif
