/*
 * BT4 on Android: the online profile. Nickname (unique, kept by the service), medals, the weekly Ranked season and
 * its trophies, through the service in port/bt4perfil-worker (worker.js; its own Cloudflare Worker, not Dragon Net's
 * and not NauTaro's). The PC build gets these functions from gs/match_supabase.cpp instead: all of this file is
 * Android's.
 *
 * The account: a random id and secret the service gives at the first nickname, kept in bt4_profile_account.txt (the
 * game's folder), in the settings (bt4_profile_id / bt4_profile_key) and in the app's internal storage (which removing or
 * installing the game's disc again does not touch: before 1.0.3 the first two went with the disc's folder), so that losing
 * one keeps the others. With it its recovery code: the nickname and the code bring the account back on any installation
 * (Port_OnlineRecover; the Profile shows the code, the name screen's R1 asks for it).
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
#include <jni.h>
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
extern void Port_UiMedal(int kind, int delta, int rating, int left, int capped, int ms); /* ui.cpp: the medal won, in the middle */
extern int Port_TrLine(const char *original, char *out, int size);
extern const char *PortOnline_Text(const char *text);
extern void Port_SettingText(const char *name, const char *def, char *out, int size);
extern void Port_SettingTextSave(const char *name, const char *value);
extern void Port_SettingsWrite(void);

typedef struct {
    int used, tries;
    Uint64 next, until;
    char body[1024];
    int ranked, battle;     /* (battle: its number in the session) */
} Report;

static struct {
    SDL_Mutex *lock;
    SDL_Condition *wake;
    SDL_Thread *thread;
    /* the account */
    int loaded, has, saveSettings;
    char id[33], secret[65];
    char code[24];          /* the recovery code ("" until known) */
    int wantCode;           /* ask the service for one (an account from before codes) */
    int codeAsked;          /* ... asked once this run (a service without codes is not asked again every minute) */
    int wantRecover;        /* the name screen asked for the account of recName back with recCode */
    char recName[20], recCode[24];
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
    char topCountry[TOP_MAX][4]; /* each one's country ("" unknown) */
    int topCount;
    /* who is online (/v1/presence): asked every two minutes while the online screens are open */
    int wantPresence, presenceActive, online;
    Uint64 presenceAt;
    char who[20];
    /* reports of battles, and their answers for the game's thread */
    Report rep[REPORTS];
    char banner[4][160];
    int banners;
    int medal[4][5];        /* medals to show: kind (1 gold, 2 silver), delta, rating, rematches left, capped */
    int medals;
    int resultBattle, resultLeft; /* the last battle whose answer came (its number in the session), its rematches left */
    /* a Ranked battle being played: said every 3 s while the game runs it (/v1/beat), so that the service can tell who
       vanished from it; and the battles that broke off, asked about until decided (/v1/gone) */
    struct { int on; Uint64 alive, next; char body[420]; } live;
    struct { int used, crash; Uint64 next, until, since; char body[220]; } left[2];
} sP;

/* the game's thread only */
static struct {
    int session, ranked, battle, sawFight, outcomeBits, warned;
    int liveN;          /* the battle being said to the service, and its room */
    char liveCode[24];
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

/* The online screens' language for the service's words (the country's name): "pt", "es" or "en" (Automatic: the menus'). */
static const char *screens_lang(void) {
    extern int MenuTranslation_TboLanguage(void);
    extern const char *MenuTranslation_Locale(void);
    int l = MenuTranslation_TboLanguage();
    const char *loc;
    if (l == 2) return "pt";
    if (l == 3) return "es";
    if (l == 1) return "en";
    loc = MenuTranslation_Locale();
    return loc != NULL && SDL_strncmp(loc, "pt", 2) == 0 ? "pt" : loc != NULL && SDL_strncmp(loc, "es", 2) == 0 ? "es" : "en";
}

/* ---- tiny JSON readers (the service's answers are plain: no escapes in names, see worker.js cleanName) ---- */
static const char *jfind(const char *json, const char *key) {
    char k[40];
    const char *p;
    SDL_snprintf(k, sizeof(k), "\"%s\":", key);
    p = SDL_strstr(json, k);
    return p != NULL ? p + SDL_strlen(k) : NULL;
}
static int jwhole(const char *json, const char *key, int def) {
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
/* the copy in the app's internal storage ("" when there is none: not Android's app process) */
static const char *account_durable(void) {
    static char path[512];
    if (path[0] == '\0') {
        const char *dir = SDL_GetAndroidInternalStoragePath();
        if (dir == NULL || dir[0] == '\0') return "";
        SDL_snprintf(path, sizeof(path), "%s/%s", dir, ACCOUNT_FILE);
    }
    return path;
}
/* one file: "id secret [recovery code]" */
static int account_read(const char *path, char id[64], char secret[96], char code[24]) {
    char line[256] = "";
    char c[32] = "";
    size_t n;
    FILE *f;
    if (path == NULL || path[0] == '\0' || (f = fopen(path, "rb")) == NULL) return 0;
    n = fread(line, 1, sizeof(line) - 1, f);
    fclose(f);
    line[n] = '\0';
    if (SDL_sscanf(line, "%63s %95s %31s", id, secret, c) < 2 || !account_valid(id, secret)) return 0;
    SDL_strlcpy(code, SDL_strlen(c) == 19 ? c : "", 24);
    return 1;
}
static void account_write(const char *path) {
    char tmp[600];
    FILE *f;
    if (path == NULL || path[0] == '\0') return;
    SDL_snprintf(tmp, sizeof(tmp), "%s.new", path);
    f = fopen(tmp, "wb");
    if (f != NULL) {
        fprintf(f, "%s %s %s\n", sP.id, sP.secret, sP.code);
        fclose(f);
        remove(path);
        rename(tmp, path);
    }
}
/* (the menus' thread, the first time: the settings are read there) */
static void account_load(void) {
    char id[64] = "", secret[96] = "", code[24] = "";
    int from = 0;
    if (sP.loaded) return;
    sP.loaded = 1;
    if (account_read(ACCOUNT_FILE, id, secret, code)) from = 1;
    if (!from) {
        Port_SettingText("bt4_profile_id", "", id, sizeof(id));
        Port_SettingText("bt4_profile_key", "", secret, sizeof(secret));
        if (account_valid(id, secret)) from = 2;
    }
    if (!from && account_read(account_durable(), id, secret, code)) from = 3;
    if (from) {
        char other[64], otherSecret[96], otherCode[24];
        SDL_strlcpy(sP.id, id, sizeof(sP.id));
        SDL_strlcpy(sP.secret, secret, sizeof(sP.secret));
        sP.has = 1;
        sP.saveSettings = 1; /* (every place holds it) */
        if (code[0] == '\0' && account_read(account_durable(), other, otherSecret, otherCode) && SDL_strcmp(other, id) == 0) {
            SDL_strlcpy(code, otherCode, sizeof(code));
        }
        SDL_strlcpy(sP.code, code, sizeof(sP.code));
        if (from != 1) account_write(ACCOUNT_FILE);
        account_write(account_durable());
        SDL_Log("bt4 profile: account %.8s... from %s%s", id, from == 1 ? "the game's folder" : from == 2 ? "the settings" : "the app's storage",
                sP.code[0] ? ", with its recovery code" : "");
    }
}
/* (the service's thread, under the lock) */
static void account_keep(const char *id, const char *secret, const char *code) {
    SDL_strlcpy(sP.id, id, sizeof(sP.id));
    SDL_strlcpy(sP.secret, secret, sizeof(sP.secret));
    SDL_strlcpy(sP.code, code != NULL ? code : "", sizeof(sP.code));
    sP.has = 1;
    sP.saveSettings = 1;
    account_write(ACCOUNT_FILE);
    account_write(account_durable());
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
        char was[96], wasKey[96];
        Port_SettingText("bt4_profile_id", "", was, sizeof(was));
        Port_SettingText("bt4_profile_key", "", wasKey, sizeof(wasKey));
        if (SDL_strcmp(was, id) != 0 || SDL_strcmp(wasKey, secret) != 0) { /* (a recovered account keeps its id, with a new secret) */
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
    s->gold = jwhole(a, "gold", 0);
    s->silver = jwhole(a, "silver", 0);
    s->wins = jwhole(a, "wins", 0);
    s->losses = jwhole(a, "losses", 0);
    s->rating = jwhole(a, "rating", 1000);
    s->rwins = jwhole(a, "rwins", 0);
    s->rlosses = jwhole(a, "rlosses", 0);
    s->rank = jwhole(a, "rank", 0);
    s->players = jwhole(a, "players", 0);
    s->rated = jwhole(a, "rated", s->rwins + s->rlosses);
    s->opponents = jwhole(a, "opponents", 0);
    s->eligible = SDL_strstr(a, "\"eligible\":true") != NULL;
    s->seasonGold = jwhole(a, "season_gold", 0);
    s->seasonSilver = jwhole(a, "season_silver", 0);
    s->bestRating = jwhole(a, "best_rating", 1000);
    s->rankedTotal = jwhole(a, "ranked_total", 0);
    s->seasonsPlayed = jwhole(a, "seasons_played", 0);
    {   /* "season_name":"Temporada 3" */
        char sn[40] = "";
        jstr(a, "season_name", sn, sizeof(sn));
        s->seasonNumber = SDL_strncmp(sn, "Temporada ", 10) == 0 ? SDL_atoi(sn + 10) : 0;
    }
    if (SDL_GetCurrentTime(&real)) {
        double left = ends - (double)(real / 1000000);
        s->hoursLeft = left > 0 ? (int)(left / 3600000.0) : 0;
    }
    jstr(a, "country", s->country, sizeof(s->country));
    if (!jstr(a, "country_name", s->countryName, sizeof(s->countryName))) SDL_strlcpy(s->countryName, s->country, sizeof(s->countryName));
    s->favChara = -1;
    s->favBattles = 0;
    s->favName[0] = '\0';
    t = jfind(a, "fav");
    if (t != NULL && *t == '[') { /* "fav":[chara,battles,"name"] (null: none yet) */
        const char *q;
        if (SDL_sscanf(t, "[%d,%d", &s->favChara, &s->favBattles) != 2) s->favChara = -1;
        q = SDL_strchr(t, '"');
        if (s->favChara >= 0 && q != NULL && q < SDL_strchr(t, ']')) {
            const char *e = SDL_strchr(q + 1, '"');
            if (e != NULL && (size_t)(e - q - 1) < sizeof(s->favName)) {
                SDL_memcpy(s->favName, q + 1, (size_t)(e - q - 1));
                s->favName[e - q - 1] = '\0';
            }
        }
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
    sP.topTotal = jwhole(a, "total", 0);
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
            sP.topCountry[n][0] = '\0';
            {   /* the row's country, its last item: ,"BR"] (an older service: none) */
                const char *end = SDL_strchr(qe, ']'), *c0 = SDL_strchr(qe + 1, '"');
                if (end != NULL && c0 != NULL && c0 < end && end - c0 == 4 && c0[3] == '"') {
                    sP.topCountry[n][0] = c0[1];
                    sP.topCountry[n][1] = c0[2];
                    sP.topCountry[n][2] = '\0';
                }
            }
            n++;
            p = SDL_strchr(qe, ']');
            if (p == NULL) break;
            p++;
            if (*p == ',') p++;
        }
    }
    sP.topCount = n;
}

static void take_result(const char *a, int ranked, int battle) {
    char state[16] = "", medal[16] = "";
    int capped = SDL_strstr(a, "\"capped\":true") != NULL;
    int left = jwhole(a, "rematches_left", -1);
    jstr(a, "state", state, sizeof(state));
    jstr(a, "medal", medal, sizeof(medal));
    sP.resultBattle = battle; /* (answered, whatever the answer: the rematch box waits for it, net.c) */
    sP.resultLeft = SDL_strcmp(state, "settled") == 0 ? left : -1;
    if (SDL_strcmp(state, "settled") != 0) return;
    if (SDL_strcmp(medal, "gold") == 0 || SDL_strcmp(medal, "silver") == 0) { /* a Ranked battle that counted: its medal on screen */
        if (sP.medals < 4) {
            int *m = sP.medal[sP.medals++];
            m[0] = SDL_strcmp(medal, "gold") == 0 ? 1 : 2;
            m[1] = jwhole(a, "delta", 0);
            m[2] = jwhole(a, "rating", 0);
            m[3] = left;
            m[4] = 0;
        }
    } else if (ranked && capped) { /* past the match and its three rematches today with this rival */
        if (sP.medals < 4) {
            int *m = sP.medal[sP.medals++];
            m[0] = 0; m[1] = 0; m[2] = 0; m[3] = 0; m[4] = 1;
        }
    }
    /* (a battle that is not Ranked gives nothing since 1.0.3: medals are Ranked's) */
    sP.wantMe = 1;
    sP.wantTop = sP.wantTop || ranked;
}

static void wake(void);
static void profile_start(void);
#define LIVE_FILE "bt4_live.txt" /* the Ranked battle being played: left behind only by a game that stopped */

/* What became of a Ranked battle that broke off, said on the screen (the lock held). */
static void left_banner(const char *a, const char *state) {
    const char *l = screens_lang();
    int pt = l[0] == 'p', es = l[0] == 'e' && l[1] == 's';
    const char *line;
    if (SDL_strcmp(state, "crash") == 0) {
        line = pt ? "O jogo travou durante a luta: ningu\303\251m perdeu nada." : es ? "El juego fall\303\263 durante la pelea: nadie pierde nada." :
               "The game crashed during the match: nobody loses anything.";
    } else if (SDL_strcmp(state, "link") == 0) {
        line = pt ? "A conex\303\243o entre voc\303\252s caiu: ningu\303\251m perdeu." :
               es ? "Se cort\303\263 la conexi\303\263n entre los dos: nadie pierde." : "The line between you failed: nobody loses.";
    } else if (SDL_strstr(a, "\"left\":true") == NULL) {
        line = pt ? "O advers\303\241rio saiu da partida: vit\303\263ria sua!" : es ? "El rival sali\303\263 de la partida: \302\241ganas t\303\272!" :
               "Your rival left the match: you win!";
    } else if (SDL_strstr(a, "\"forgiven\":true") != NULL) {
        line = pt ? "Voc\303\252 saiu da partida. Desta vez n\303\243o houve puni\303\247\303\243o." :
               es ? "Saliste de la partida. Esta vez no hay castigo." : "You left the match. No penalty this time.";
    } else {
        line = pt ? "Voc\303\252 saiu da partida ranqueada: derrota e -1 medalha de ouro." :
               es ? "Saliste de la partida clasificatoria: derrota y -1 medalla de oro." : "You left the Ranked match: a loss and -1 gold medal.";
    }
    if (sP.banners < 4) SDL_strlcpy(sP.banner[sP.banners++], line, sizeof(sP.banner[0]));
}

/* The Ranked battle being played, and those that broke off: a thread of their own, so that a slow answer to anything
   else never holds a "still playing" back (the service reads silence as having left). */
static SDL_AtomicInt sInBackground; /* the app is not on the screen (a game at rest then is not a frozen one) */
static bool SDLCALL app_watch(void *arg, SDL_Event *e) {
    (void)arg;
    if (e->type == SDL_EVENT_DID_ENTER_BACKGROUND) SDL_SetAtomicInt(&sInBackground, 1);
    if (e->type == SDL_EVENT_DID_ENTER_FOREGROUND) SDL_SetAtomicInt(&sInBackground, 0);
    return true;
}
/* (the lock held) "room battle frozen" */
static void live_file(void) {
    FILE *f = fopen(LIVE_FILE, "w");
    if (f != NULL) {
        fprintf(f, "%s %d %d\n", sG.liveCode, sG.liveN, !SDL_GetAtomicInt(&sInBackground) && SDL_GetTicks() - sP.live.alive > 9000);
        fclose(f);
    }
}

static int SDLCALL live_thread(void *arg) {
    char body[420], a[1024];
    (void)arg;
    for (;;) {
        Uint64 now;
        int k, i, beat;
        SDL_Delay(500);
        SDL_LockMutex(sP.lock);
        now = SDL_GetTicks();
        if (sP.live.on) live_file();
        beat = sP.live.on && now >= sP.live.next && now - sP.live.alive < 9000;
        if (beat) {
            SDL_strlcpy(body, sP.live.body, sizeof(body));
            sP.live.next = now + 3000;
            SDL_UnlockMutex(sP.lock);
            i = post("beat", body, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            if (i && (SDL_strstr(a, "\"off\"") != NULL || SDL_strstr(a, "\"path\"") != NULL)) sP.live.on = 0; /* (a service without the rule) */
        }
        for (k = 0; k < 2; k++) {
            char state[16] = "";
            if (!sP.left[k].used || SDL_GetTicks() < sP.left[k].next) continue;
            SDL_strlcpy(body, sP.left[k].body, sizeof(body));
            /* (a service waits some minutes for a silent side, which may come back saying it crashed: asked often at
               first, then now and then) */
            sP.left[k].next = SDL_GetTicks() + (SDL_GetTicks() - sP.left[k].since < 60000 ? 3000 : 20000);
            {
                int crashed = sP.left[k].crash;
                SDL_UnlockMutex(sP.lock);
                i = post(crashed ? "exit" : "gone", body, a, sizeof(a));
            }
            SDL_LockMutex(sP.lock);
            if (i) jstr(a, "state", state, sizeof(state));
            if (i && SDL_strcmp(state, "waiting") != 0) {
                if (SDL_strcmp(state, "abandon") == 0 || SDL_strcmp(state, "link") == 0 || SDL_strcmp(state, "crash") == 0) left_banner(a, state);
                sP.left[k].used = 0;
                sP.wantMe = 1;
                sP.wantTop = 1;
                wake();
            } else if (SDL_GetTicks() >= sP.left[k].until) {
                sP.left[k].used = 0;
            }
        }
        SDL_UnlockMutex(sP.lock);
    }
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
            if (sP.wantRecover) { job = 5; break; }
            if (sP.wantCode && sP.has) { job = 6; break; }
            if (sP.wantMe && sP.has) { job = 2; break; }
            if (sP.wantTop) { job = 3; break; }
            if (sP.wantPresence) { job = 4; break; }
            for (i = 0; i < REPORTS; i++) {
                if (sP.rep[i].used && now >= sP.rep[i].next) { job = 10 + i; break; }
            }
            if (job) break;
            SDL_WaitConditionTimeout(sP.wake, sP.lock, 500);
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
                char id[40], secret[80], got[40], code[32] = "";
                if (!had && jstr(a, "id", id, sizeof(id)) && jstr(a, "secret", secret, sizeof(secret)) && account_valid(id, secret)) {
                    jstr(a, "recovery", code, sizeof(code));
                    account_keep(id, secret, SDL_strlen(code) == 19 ? code : "");
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
            SDL_snprintf(body, sizeof(body), "{\"id\":\"%s\",\"secret\":\"%s\",\"lang\":\"%s\"}", sP.id, sP.secret, screens_lang());
            SDL_UnlockMutex(sP.lock);
            i = post("me", body, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            sP.meAt = SDL_GetTicks();
            if (i && SDL_strstr(a, "\"ok\":true") != NULL) {
                char got[40];
                take_me(a);
                sP.meState = 1;
                if (sP.code[0] == '\0' && !sP.codeAsked) sP.wantCode = 1; /* (an account from before recovery codes, or a copy without it) */
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
        } else if (job == 5) { /* the account back: its nickname and recovery code */
            char name[40], code[48], id[40], secret[80], got[40];
            sP.wantRecover = 0;
            jesc(name, sizeof(name), sP.recName);
            jesc(code, sizeof(code), sP.recCode);
            SDL_snprintf(body, sizeof(body), "{\"name\":\"%s\",\"code\":\"%s\"}", name, code);
            SDL_UnlockMutex(sP.lock);
            i = post("recover", body, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            if (i && SDL_strstr(a, "\"ok\":true") != NULL && jstr(a, "id", id, sizeof(id)) && jstr(a, "secret", secret, sizeof(secret)) &&
                account_valid(id, secret)) {
                char typed[24];
                const char *c = sP.recCode;
                int n = 0;
                for (; *c && n < 19; c++) { /* the code as the service gave it: XXXX-XXXX-XXXX-XXXX */
                    char ch = (char)SDL_toupper((unsigned char)*c);
                    if (!SDL_isalnum((unsigned char)ch)) continue;
                    if (n == 4 || n == 9 || n == 14) typed[n++] = '-';
                    typed[n++] = ch;
                }
                typed[n] = '\0';
                account_keep(id, secret, n == 19 ? typed : "");
                if (n != 19) sP.wantCode = 1;
                if (jstr(a, "name", got, sizeof(got))) SDL_strlcpy(sP.name, got, sizeof(sP.name));
                sP.nick = BT4_NICKNAME_READY;
                sP.wantMe = 1;
                sP.wantTop = 1;
                SDL_Log("bt4 profile: account of %s recovered", sP.name);
            } else if (i && (SDL_strstr(a, "\"wrong\"") != NULL || SDL_strstr(a, "\"invalid\"") != NULL)) {
                sP.nick = BT4_NICKNAME_WRONG_CODE;
            } else {
                sP.nick = BT4_NICKNAME_UNAVAILABLE;
            }
            SDL_UnlockMutex(sP.lock);
        } else if (job == 6) { /* a recovery code for an account that has none here */
            char code[32] = "";
            sP.wantCode = 0;
            sP.codeAsked = 1;
            SDL_snprintf(body, sizeof(body), "{\"id\":\"%s\",\"secret\":\"%s\"}", sP.id, sP.secret);
            SDL_UnlockMutex(sP.lock);
            i = post("recovery", body, a, sizeof(a));
            SDL_LockMutex(sP.lock);
            if (i && SDL_strstr(a, "\"ok\":true") != NULL && jstr(a, "recovery", code, sizeof(code)) && SDL_strlen(code) == 19) {
                char id[33], secret[65];
                SDL_strlcpy(id, sP.id, sizeof(id));
                SDL_strlcpy(secret, sP.secret, sizeof(secret));
                account_keep(id, secret, code);
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
                sP.online = jwhole(a, "online", -1);
            }
            SDL_UnlockMutex(sP.lock);
        } else { /* a battle's report: again until both have told of it (or three minutes) */
            Report *r = &sP.rep[job - 10];
            char send[sizeof(r->body)];
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
                take_result(a, ranked, r->battle);
                r->used = 0;
            } else if (i && SDL_strstr(a, "\"error\"") != NULL && SDL_strstr(a, "\"slow\"") == NULL) {
                SDL_Log("bt4 profile: report refused: %.120s", a);
                sP.resultBattle = r->battle; sP.resultLeft = -1; /* (no answer will come: the rematch box goes on) */
                r->used = 0;
            } else if (now >= r->until) {
                SDL_Log("bt4 profile: a battle's report was not paired in time (%s)", i ? state : "no answer");
                r->used = 0;
            } else {
                r->next = now + (r->tries < 8 ? 1500 : r->tries < 12 ? 10000 : 30000); /* (the first to tell hears the answer at
                                                     its next ask: soon, for its medal before the rematch box; a rival who never tells: few asks) */
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
    {
        SDL_Thread *t = SDL_CreateThread(live_thread, "bt4-live", NULL);
        SDL_AddEventWatch(app_watch, NULL);
        if (t != NULL) SDL_DetachThread(t);
    }
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
void Bt4Profile_TopCountry(int i, char out[4]) {
    int k;
    profile_start();
    SDL_LockMutex(sP.lock);
    k = sP.page * 5 + i;
    if (i >= 0 && i < 5 && k >= 0 && k < sP.topCount) SDL_memcpy(out, sP.topCountry[k], 4);
    else out[0] = '\0';
    SDL_UnlockMutex(sP.lock);
}
/* the same by the place in the table (0: the first), whatever page of it is shown */
void Bt4Profile_TopCountryAt(int k, char out[4]) {
    profile_start();
    SDL_LockMutex(sP.lock);
    if (k >= 0 && k < sP.topCount) SDL_memcpy(out, sP.topCountry[k], 4);
    else out[0] = '\0';
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
/* The session is being left (gs/net.c). how: 1 this player's own choice, 0 the other side or the line, -1 the two
   games had come apart (nobody's doing). Only a Ranked battle still being played counts. */
void Bt4Profile_Left(int how) {
    int k;
    if (sP.lock == NULL) return;
    SDL_LockMutex(sP.lock);
    if (sP.live.on && how >= 0 && sP.has) {
        for (k = 0; k < 1 && sP.left[k].used; k++) {}
        sP.left[k].crash = 0;
        sP.left[k].since = SDL_GetTicks();
        SDL_snprintf(sP.left[k].body, sizeof(sP.left[k].body), "{\"id\":\"%s\",\"secret\":\"%s\",\"code\":\"%s\",\"n\":%d,\"self\":%s}",
                     sP.id, sP.secret, sG.liveCode, sG.liveN, how == 1 ? "true" : "false");
        sP.left[k].used = 1;
        sP.left[k].next = SDL_GetTicks();
        sP.left[k].until = sP.left[k].next + 1200000;
    }
    sP.live.on = 0;
    remove(LIVE_FILE); /* (left, not crashed) */
    SDL_UnlockMutex(sP.lock);
}
/* Whether the player closed the game the last time (GameActivity.lastExitReason: Android's own record of it). */
static int closed_by_player(void) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject act;
    jclass cls;
    jmethodID m;
    jstring r = NULL;
    int closed = 0;
    if (env == NULL || (act = (jobject)SDL_GetAndroidActivity()) == NULL) return 0;
    cls = (*env)->GetObjectClass(env, act);
    m = (*env)->GetMethodID(env, cls, "lastExitReason", "()Ljava/lang/String;");
    if (m != NULL) r = (jstring)(*env)->CallObjectMethod(env, act, m);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        r = NULL;
    }
    if (r != NULL) {
        const char *t = (*env)->GetStringUTFChars(env, r, NULL);
        if (t != NULL) {
            closed = SDL_strcmp(t, "closed") == 0;
            (*env)->ReleaseStringUTFChars(env, r, t);
        }
        (*env)->DeleteLocalRef(env, r);
    }
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, act);
    return closed;
}

/* The game was opened again with a Ranked battle's file still here: the game stopped inside it. Say why: the player's
   own closing, or a fault (a crash in the run before, whose log the loader kept; frozen on the screen; the system). */
static void crash_report(void) {
    char code[24] = "", line[512];
    int n = 0, stalled = 0, crashed = 0, k;
    const char *why;
    FILE *f = fopen(LIVE_FILE, "r");
    if (f == NULL) return;
    if (fscanf(f, "%23s %d %d", code, &n, &stalled) < 2) n = 0;
    fclose(f);
    if ((f = fopen("bt3_log.1.txt", "r")) != NULL) {
        while (!crashed && fgets(line, sizeof(line), f) != NULL) crashed = SDL_strstr(line, "CRASH: signal") != NULL;
        fclose(f);
    }
    remove(LIVE_FILE);
    if (n <= 0) return;
    why = !crashed && !stalled && closed_by_player() ? "closed" : "fault";
    SDL_Log("bt4 profile: a Ranked battle was left on: %s (crash %d, frozen %d)", why, crashed, stalled);
    profile_start();
    SDL_LockMutex(sP.lock);
    if (sP.has) {
        for (k = 0; k < 1 && sP.left[k].used; k++) {}
        SDL_snprintf(sP.left[k].body, sizeof(sP.left[k].body), "{\"id\":\"%s\",\"secret\":\"%s\",\"code\":\"%s\",\"n\":%d,\"why\":\"%s\"}",
                     sP.id, sP.secret, code, n, why);
        sP.left[k].crash = 1;
        sP.left[k].used = 1;
        sP.left[k].since = sP.left[k].next = SDL_GetTicks();
        sP.left[k].until = sP.left[k].next + 600000;
    }
    SDL_UnlockMutex(sP.lock);
}
int Port_RankedTakeSettlement(int *rating, int *delta) {
    static int looked;
    (void)rating; (void)delta;
    if (!looked) { /* (called every blank outside a session: once, for a crash of the run before) */
        looked = 1;
        crash_report();
    }
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
    if (sP.lock != NULL) { /* (medals of an earlier session not shown yet: not this match's) */
        SDL_LockMutex(sP.lock);
        sP.medals = 0;
        SDL_UnlockMutex(sP.lock);
    }
    SDL_Log("bt4 profile: session begins (%s, room %s)", sG.ranked ? "Ranked" : "normal", sG.code[0] ? sG.code : "-");
}
void Bt4Profile_SessionEnd(void) {
    sG.session = 0;
    SDL_SetAtomicInt(&sRankedActive, 0);
}

static void report(int winner, int me, const char *name0, const char *name1) {
    extern int Port_BattleTeam(int side, int out[5]); /* headless.c */
    extern int Port_CharaName(int chara, char *out, int cap); /* a11y_game.c: as the select wrote it */
    char rkey[17], mine[40], theirs[40], code[24], chars[400];
    int i, team[5], n = Port_BattleTeam(me, team);
    size_t at = 0;
    /* this player's team, [[id,"name"],...]: the profile's most used character */
    chars[at++] = '[';
    for (i = 0; i < n && at < sizeof(chars) - 72; i++) {
        char name[48], safe[64];
        Port_CharaName(team[i], name, sizeof(name));
        jesc(safe, sizeof(safe), name);
        at += (size_t)SDL_snprintf(chars + at, sizeof(chars) - at, "%s[%d,\"%s\"]", i ? "," : "", team[i], safe);
    }
    chars[at++] = ']';
    chars[at] = '\0';
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
    sP.rep[i].battle = sG.battle;
    sP.rep[i].next = SDL_GetTicks();
    sP.rep[i].until = sP.rep[i].next + 180000;
    SDL_snprintf(sP.rep[i].body, sizeof(sP.rep[i].body),
                 "{\"id\":\"%s\",\"secret\":\"%s\",\"rkey\":\"%s\",\"code\":\"%s\",\"n\":%d,\"me\":\"%s\",\"opp\":\"%s\",\"side\":%d,\"winner\":%d,\"ranked\":%s,\"chars\":%s}",
                 sP.id, sP.secret, rkey, code, sG.battle, mine, theirs, me, winner, sG.ranked ? "true" : "false", chars);
    SDL_UnlockMutex(sP.lock);
    SDL_Log("bt4 profile: battle %d of room %s: winner side %d (this player side %d)", sG.battle, sG.code[0] ? sG.code : "-", winner, me);
    wake();
}

/* A battle's end, as Ranked's observer saw it on the PC (gs/match_supabase.cpp Port_RankedObserve): the fight seen,
   then a result that holds past the rollback window and the other's confirmed input. A surrender from the pause menu
   (bits 8) is a loss for the one who gave up (BT4decomp R5.4). */
static int observe(int battleState, int bits, unsigned tick, unsigned confirmed, int resimulating, int localConcession,
                   int me, const char *name0, const char *name1);

/* The game ran a tick of a Ranked battle: say so (the thread does, every 3 s), until its result is told. */
static void live_tick(int me, const char *name0, const char *name1) {
    char mine[40], theirs[40], code[24];
    const char *myName = me ? name1 : name0, *their = me ? name0 : name1;
    if (sP.lock == NULL || myName == NULL || their == NULL || myName[0] == '\0' || their[0] == '\0' || sG.code[0] == '\0') return;
    SDL_LockMutex(sP.lock);
    if (sP.has) {
        sP.live.alive = SDL_GetTicks();
        if (!sP.live.on || sG.liveN != sG.battle) {
            jesc(mine, sizeof(mine), myName);
            jesc(theirs, sizeof(theirs), their);
            jesc(code, sizeof(code), sG.code);
            sG.liveN = sG.battle;
            SDL_strlcpy(sG.liveCode, code, sizeof(sG.liveCode));
            SDL_snprintf(sP.live.body, sizeof(sP.live.body), "{\"id\":\"%s\",\"secret\":\"%s\",\"code\":\"%s\",\"n\":%d,\"me\":\"%s\",\"opp\":\"%s\",\"side\":%d}",
                         sP.id, sP.secret, code, sG.battle, mine, theirs, me);
            sP.live.on = 1;
            sP.live.next = 0;
            live_file();
        }
    }
    SDL_UnlockMutex(sP.lock);
}

int Bt4Profile_Observe(int battleState, int bits, unsigned tick, unsigned confirmed, int resimulating, int localConcession,
                       int me, const char *name0, const char *name1) {
    int over = observe(battleState, bits, tick, confirmed, resimulating, localConcession, me, name0, name1);
    if (!sG.session || !sG.ranked || resimulating || (me != 0 && me != 1)) return over;
    if (over > 0) { /* (told: a battle that ended is nobody's abandonment) */
        if (sP.lock != NULL) {
            SDL_LockMutex(sP.lock);
            sP.live.on = 0;
            remove(LIVE_FILE);
            SDL_UnlockMutex(sP.lock);
        }
    } else if (sG.sawFight) {
        live_tick(me, name0, name1);
    }
    return over;
}

static int observe(int battleState, int bits, unsigned tick, unsigned confirmed, int resimulating, int localConcession,
                   int me, const char *name0, const char *name1) {
    int winner;
    if (!sG.session || resimulating || (me != 0 && me != 1)) return 0;
    if (battleState == 3) {
        if (!sG.sawFight) {
            sG.sawFight = 1;
            sG.battle++;
        }
        sG.outcomeBits = 0;
        return 0;
    }
    if (!sG.sawFight || battleState < 4 || battleState > 6 || !(bits & 15)) {
        sG.outcomeBits = 0;
        return 0;
    }
    bits = bits & 8 ? 8 : bits & 7;
    if (sG.outcomeBits != bits) {
        sG.outcomeBits = bits;
        sG.outcomeTick = tick;
        return 0;
    }
    if (bits == 8) {
        if (localConcession < 0 || confirmed < sG.outcomeTick + 2) return 0;
        winner = localConcession ? 1 - me : me;
    } else {
        if (tick - sG.outcomeTick < 20 || confirmed < sG.outcomeTick + 2) return 0;
        winner = (bits & 3) == 1 ? 0 : (bits & 3) == 2 ? 1 : 2;
    }
    sG.sawFight = 0;
    sG.outcomeBits = 0;
    report(winner, me, name0, name1);
    return sG.battle; /* (the battle just over: its number in the session) */
}

void Bt4Profile_Pump(void) {
    static Uint64 shown, showFor = 3800;
    char line[160];
    int have = 0, m[5];
    if (sP.lock == NULL || (shown != 0 && SDL_GetTicks() - shown < showFor)) return; /* (one at a time) */
    SDL_LockMutex(sP.lock);
    if (sP.medals > 0) { /* a medal won: the picture (ui.cpp), five seconds */
        SDL_memcpy(m, sP.medal[0], sizeof(m));
        SDL_memmove(sP.medal[0], sP.medal[1], sizeof(sP.medal[0]) * 3);
        sP.medals--;
        SDL_UnlockMutex(sP.lock);
        shown = SDL_GetTicks();
        showFor = 5300;
        Port_UiMedal(m[0], m[1], m[2], m[3], m[4], 5000);
        return;
    }
    showFor = 3800;
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
/* ---- the recovery code (src/menu/port_tbo.c: Profile, the name screen) ---- */
void Bt4Profile_RecoveryCode(char out[24]) {
    profile_start();
    SDL_LockMutex(sP.lock);
    SDL_strlcpy(out, sP.has ? sP.code : "", 24);
    SDL_UnlockMutex(sP.lock);
}
int Port_OnlineRecover(const char *name, const char *code) {
    profile_start();
    if (name == NULL || name[0] == '\0' || code == NULL || code[0] == '\0') return 0;
    SDL_LockMutex(sP.lock);
    if (sP.nick == BT4_NICKNAME_SAVING) {
        SDL_UnlockMutex(sP.lock);
        return 0;
    }
    SDL_strlcpy(sP.recName, name, sizeof(sP.recName));
    SDL_strlcpy(sP.recCode, code, sizeof(sP.recCode));
    sP.wantRecover = 1;
    sP.nick = BT4_NICKNAME_SAVING;
    SDL_UnlockMutex(sP.lock);
    wake();
    return 1;
}
/* ---- the Ranked rematch box (gs/net.c) ---- */
int Bt4Profile_RankedSession(void) {
    return sG.session && sG.ranked;
}
/* 1 once the service has answered battle `battle` of this session (or will not): *left its rematches still counting today
   (-1 not said); and nothing of it still to be shown (its medal). */
int Bt4Profile_ResultOf(int battle, int *left) {
    int got;
    if (sP.lock == NULL) return 0;
    SDL_LockMutex(sP.lock);
    got = sP.resultBattle == battle;
    if (left != NULL) *left = got ? sP.resultLeft : -1;
    SDL_UnlockMutex(sP.lock);
    return got;
}
int Bt4Profile_MedalsPending(void) {
    int n;
    if (sP.lock == NULL) return 0;
    SDL_LockMutex(sP.lock);
    n = sP.medals;
    SDL_UnlockMutex(sP.lock);
    return n;
}
#endif
