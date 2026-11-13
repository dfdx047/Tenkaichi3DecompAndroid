#ifndef BT4_PROFILE_H
#define BT4_PROFILE_H
/* BT4 on Android: the player's online profile (gs/bt4_profile.c and the service port/bt4perfil-worker): a unique
   nickname, the medals of online battles (gold for a win, silver for a loss, normal battles, rematches and Ranked),
   the weekly Ranked season's rating and the trophies of its first three. On the PC these come from NauTaro's service
   (gs/match_supabase.cpp) and this file's functions answer "nothing". */
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Bt4ProfileStats {
    int state;              /* 1 known, 0 loading, -1 the service did not answer, -2 no account yet (no nickname) */
    int gold, silver;       /* medals */
    int wins, losses;       /* every online battle counted */
    int rating, rwins, rlosses, rank, players; /* this week's Ranked: rank 0 = not ranked yet */
    int trophies[3];        /* gold, silver, bronze trophies of past weeks */
    int hoursLeft;          /* until this week's season ends */
    char last[3][20];       /* last week's first three ("" when nobody) */
    int rated, opponents, eligible; /* this week's Ranked battles that moved points, rivals in them, in the trophy race */
} Bt4ProfileStats;
void Bt4Profile_Stats(Bt4ProfileStats *out);
/* The BT4 Android online service (port/bt4online-worker): profiles and the Ranked queue's rooms. BT4_ONLINE_URL in the
   environment names another (tests). */
const char *Bt4Online_Url(void);
int Bt4Profile_HasAccount(void);
/* the Global Top page's row i (0..4): the player's medals and trophies, out[5] = gold, silver, trophies 1st/2nd/3rd */
void Bt4Profile_TopRow(int i, int out[5]);
/* gs/net.c: a session (one or more battles with one rival) begins and ends; every battle's state as Ranked's was */
void Bt4Profile_SessionBegin(int ranked, const char *code);
void Bt4Profile_SessionEnd(void);
void Bt4Profile_Observe(int battleState, int bits, unsigned tick, unsigned confirmed, int resimulating, int localConcession,
                        int me, const char *name0, const char *name1);
void Bt4Profile_Pump(void); /* the game's thread: the results' lines (Port_UiBanner) */
#ifdef __cplusplus
}
#endif
#endif
