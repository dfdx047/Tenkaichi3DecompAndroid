#ifndef TAG_LOBBY_H
#define TAG_LOBBY_H
#include "battle/tag_lobby_rules.h"
#include "battle/battle.h"
int TagLobby_Practice(void);
void TagLobby_EnterPractice(const TlRoom *room);
void TagLobby_LeavePractice(void);
int TagLobby_BuildSides(BattleSide *sides,int *teams,int *nativeSides,int *assignment);
void TagLobby_Reset(void);
void TagLobby_Update(void);
void TagLobby_AfterUpdate(void);
int TagLobby_Hidden(int actor);
int TagLobby_Editing(int actor);
int TagLobby_LocalMenu(void);
int TagLobby_Enemy(int actor,int other);
int TagLobby_CanContact(int actor,int other);
void TagLobby_Contact(int actor,int other);
int TagLobby_CpuActive(int actor);
const char *TagLobby_Label(int actor);
void TagLobby_TestSetup(void);
void TagLobby_TestTick(void);
void TagLobby_Draw(void);
#endif
