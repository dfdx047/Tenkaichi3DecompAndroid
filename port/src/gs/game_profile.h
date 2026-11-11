#ifndef BT4_GAME_PROFILE_H
#define BT4_GAME_PROFILE_H
#ifdef __cplusplus
extern "C" {
#endif
void Port_GameProfileInit(void);
int Port_GameProfileEnglish(void);
int Port_GameProfileLanguage(void);
int Port_GameProfilePortuguese(void);
int Port_GameProfileAvailable(int english);
int Port_GameProfilePrompt(void);
int Port_GameProfileChoose(int english);
const char *Port_GameProfileError(void);
/* Host-only presentation state; never part of battle snapshots. */
void Port_GameProfileMenuSet(int active, int choice, int alpha);
int Port_GameProfileMenuActive(void);
int Port_GameProfileMenuChoice(void);
int Port_GameProfileMenuAlpha(void);
#ifdef __cplusplus
}
#endif
#endif
