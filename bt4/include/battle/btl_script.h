#ifndef BATTLE_BTL_SCRIPT_H
#define BATTLE_BTL_SCRIPT_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Battle event script, first part: src/battle/btl_script.c = 0x258D20..0x259EC8.
 * The module goes on to about 0x25C200 (the command handlers of the table at 0x2C6EC0); see btl_script.c.
 */

/* Action ids the battle gives a meaning to. Every other id is only reached through CALL words. */
#define BTL_SCRIPT_ACTION_MAIN (-1)    /* started by Battle_ResetWork in mode 1 */
#define BTL_SCRIPT_ACTION_SETUP 1000   /* run to its end while the battle loads */
#define BTL_SCRIPT_ACTION_EVENT 10000  /* first of the 52 event actions, 10000..10051 */
#define BTL_SCRIPT_EVENT_COUNT 52

/* What BtlScript_ScanEvents found out about one event action. One byte, bit 0 first. */
typedef struct BtlScriptEvent {
    u8 present : 1; /* 0x01: the file has this action */
    u8 group : 3;   /* 0x0E: from the id: 0 <10030, 1 <10040, 2 <10050, 3 = 10050, 4 = 10051 */
    u8 cmd4B1 : 1;  /* 0x10: the action uses command 0x4B1 */
    u8 side : 1;    /* 0x20: bit 15 of the first operand of its command 8 (the last one, if several) */
    u8 kind : 2;    /* 0xC0: 1 when the low 12 bits of that operand are 0x10..0x14, 2 for 0x15..0x17 */
} BtlScriptEvent;

/* One key of the scripted camera move. */
typedef struct BtlScriptCamKey {
    /* 0x00 */ s32 flags; /* bit 0: the key is used; a clear bit ends the move */
    /* 0x04 */ f32 time;  /* how long the move from the key before takes, in units of 30 frames */
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ Vec4 pos;
    /* 0x20 */ Vec4 rot;
} BtlScriptCamKey; /* size 0x30 */

/* Scripted camera move: work + 0x190. key[0] is the start; the move goes through key[1], key[2].. */
typedef struct BtlScriptCam {
    /* 0x00 */ s32 active;
    /* 0x04 */ s32 time;    /* +10 per frame, like the engine's task time */
    /* 0x08 */ s32 index;   /* current segment: from key[index] to key[index + 1] */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ BtlScriptCamKey key[9];
} BtlScriptCam; /* size 0x1C0 */

/* A text window. Only the anchor is read here; the whole block is handed to the text drawing code. */
typedef struct BtlScriptWindow {
    /* 0x00 */ u8 unk0[0x48];
    /* 0x48 */ s32 unk48; /* x handed to the text draw */
    /* 0x4C */ s32 unk4C; /* y of the bottom line: the draw starts at unk4C - text height */
} BtlScriptWindow; /* size 0x50 */

#define BTL_SCRIPT_NO_MEMBER 0x7FFF
#define BTL_SCRIPT_TRIGGER_WAIT_OFF 0xFFFF /* trigger: the battle's wait flag is off (BtlFacade_IsWaitOff) */
#define BTL_SCRIPT_REQ_LINE 1
#define BTL_SCRIPT_REQ_EVENT 2

/* Who speaks a line: work->talk.speakers[line]. */
typedef struct BtlScriptSpeaker {
    /* 0x00 */ s16 side;
    /* 0x02 */ s16 member; /* team member index, BTL_SCRIPT_NO_MEMBER = never set */
} BtlScriptSpeaker;

/* Payload of a LINE request: a voiced text line said when the trigger fires. */
typedef struct BtlScriptLine {
    /* 0x00 */ s8 enabled; /* 1 when registered; cleared after the line was said if once is set */
    /* 0x01 */ s8 once;
    /* 0x02 */ s16 timer;  /* +10 per frame after the voice has stopped; the text goes at 600 */
    /* 0x04 */ s32 id;     /* line number: text entry, speaker slot and voice file offset */
} BtlScriptLine;

/* Payload of an EVENT request, also the "running event" record at work + 0x180. */
typedef struct BtlScriptStart {
    /* 0x00 */ s32 action;            /* 10000..10051 */
    /* 0x04 */ BtlScriptEvent *event; /* &work->events[action - 10000] */
} BtlScriptStart;

/* One registered trigger. */
typedef struct BtlScriptRequest {
    /* 0x00 */ s32 kind;    /* BTL_SCRIPT_REQ_* */
    /* 0x04 */ s32 trigger; /* 0xFFFF, or side << 15 | battle event id */
    /* 0x08 */ union {
        BtlScriptLine line;
        BtlScriptStart start;
    } u;
} BtlScriptRequest; /* size 0x10 */

/* Windows, speakers and triggers: work + 0x354. The code reaches all four through one pointer to this block. */
typedef struct BtlScriptTalk {
    /* 0x000 */ BtlScriptWindow windows[2];     /* work + 0x354 */
    /* 0x0A0 */ BtlScriptSpeaker speakers[300]; /* work + 0x3F4 */
    /* 0x550 */ BtlScriptRequest requests[50];  /* work + 0x8A4 */
    /* 0x870 */ s32 requestCount;               /* work + 0xBC4 */
} BtlScriptTalk; /* size 0x874 */

/* Work of the battle script module: gBtlScript, 0x333BC0. */
typedef struct BtlScriptWork {
    /* 0x000 */ s32 story;         /* -1 at init; set by script command 2; picks the voice file block */
    /* 0x004 */ void *textFile;    /* offset table + texts; file story + 0x231 / 0x263 (command 0x2BD) */
    /* 0x008 */ void *file8;       /* same layout; file story + 0x295 / 0x2C7 by voice set (command 0x2BE) */
    /* 0x00C */ u8 unkC[0x140];
    /* 0x14C */ BtlScriptEvent events[BTL_SCRIPT_EVENT_COUNT]; /* by action id - 10000 */
    /* 0x180 */ BtlScriptStart cur; /* the event action started last */
    /* 0x188 */ s16 textWindow;    /* index into windows */
    /* 0x18A */ s16 textId;        /* line whose text is drawn; -1 = none */
    /* 0x18C */ f32 shakeTime;     /* camera shake kept up while not 0 */
    /* 0x190 */ BtlScriptCam cam;
    /* 0x350 */ BtlScriptRequest *pending; /* event request that fired and waits for the fight to allow it */
    /* 0x354 */ BtlScriptTalk talk;
    /* 0xBC8 */ BtlScriptLine *voice[4]; /* line being said, by side * 2 + window */
    /* 0xBD8 */ u8 unkBD8[8];
} BtlScriptWork; /* size 0xBE0 */

void BtlScript_InitWork(void);
void BtlScript_Init(void);
void BtlScript_Nop(void);
void BtlScript_ScanEvents(s32 script);
void BtlScript_Restart(void);
void BtlScript_Update(void);
void BtlScript_UpdateView(void);
void BtlScript_Free(void);
s32 BtlScript_IsEventRunning(void);
void BtlScript_AbortEvents(void);
void BtlScript_SetStory(s32 story);
s32 BtlScript_GetStory(void);
void BtlScript_RequestTextFile(s32 id);
void *BtlScript_GetText(s32 n);
void BtlScript_RequestFile8(s32 id);
void *BtlScript_GetFile8Entry(s32 n);
BtlScriptWindow *BtlScript_GetWindow(s32 n);
void BtlScript_SetText(s32 window, s32 text);
void BtlScript_ClearText(s32 text);
void BtlScript_SetSpeaker(s32 n, s32 who);
BtlScriptSpeaker *BtlScript_GetSpeaker(s32 n);
BtlScriptEvent *BtlScript_GetCurrentEvent(void);
void BtlScript_ResetView(void);
s32 BtlScript_IsTriggered(s32 trigger, s32 range);
s32 BtlScript_GetVoiceFile(s32 line);
void BtlScript_AddLine(s32 trigger, s32 line, s32 once);
void BtlScript_AddEvent(s32 trigger, s32 action);
s32 BtlScript_IsLineVoiceStopped(BtlScriptLine *line);
void BtlScript_PlayLine(BtlScriptLine *line);
void BtlScript_GetSpeakerSlot(s32 *side, s32 *window, BtlScriptSpeaker *speaker);
s32 BtlScript_IsLinePlaying(BtlScriptLine *line);
void BtlScript_StartWaitEvents(void);
void BtlScript_UpdateEvents(void);
void BtlScript_UpdateLines(void);
void BtlScript_UpdateRequests(void);
void BtlScript_StartPending(void);
void BtlScript_UpdateHud(void);
s32 BtlScript_IsSlotBusy(s32 side, s32 window);
void BtlScript_StopVoice(s32 side);
void BtlScript_StopVoices(void);
s32 BtlScript_IsTextShown(void);

#endif
