#ifndef SYS_GSC_H
#define SYS_GSC_H

#include "types.h"
#include "sys/list.h"

/*
 * GSC script engine: src/sys/gsc.c = 0x257870..0x258D20.
 * See the comment at the top of gsc.c for the file format and the stepping rules.
 */

/* Chunk tags as they appear in the file (the code byte-swaps the word before comparing). */
#define GSC_TAG_GSAC 0x47534143 /* "GSAC": one action */

/* Low nibble of a script word. */
#define GSC_WORD_END 0      /* end of the action */
#define GSC_WORD_COMMAND 1  /* byte 1 = operand count, u16 at +2 = command id */
#define GSC_WORD_CALL 2     /* s16 at +2 = action id: start it and go on */
#define GSC_WORD_CALL_WAIT 3 /* s16 at +2 = action id: start it and wait until it ends */
#define GSC_WORD_OPTION 8   /* byte 1 = option letter, byte 2 = operand count */
#define GSC_WORD_OPERAND 10 /* bits 8..31 = index of a 4-byte value in the GSDT chunk, bits 4..7 = type (unread) */
#define GSC_WORD_ARG 8      /* bit set in every word that belongs to the preceding command */

/* Phase handed to a command handler. */
#define GSC_PHASE_BEGIN 0       /* first frame; a non-zero return ends the command at once */
#define GSC_PHASE_END 1         /* the command is over */
#define GSC_PHASE_UPDATE 2      /* every frame, the first included; non-zero return = done */
#define GSC_PHASE_NOTIFY 3      /* Gsc_NotifyAll */
#define GSC_PHASE_BEGIN_ABORT 4 /* replaces BEGIN when the task has been told to abort */

#define GSC_TIME_STEP 10 /* added to a task's timer for every frame its command keeps waiting */

/* Header of every chunk. The next chunk is at chunk + headSize + dataSize + 0x10 (an "EOFC" trailer). */
typedef struct GscChunk {
    /* 0x00 */ u32 tag;      /* four characters, in file order */
    /* 0x04 */ u32 headSize; /* 0x10 */
    /* 0x08 */ u32 dataSize;
    /* 0x0C */ s32 arg;      /* GSAC: action id */
} GscChunk;

/* The tag word as the file's four characters read big-endian ("GSAC" -> 0x47534143). */
#define GSC_SWAP(x) ((((x) & 0xFF000000) >> 24) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF00) << 8) | (((x) & 0xFF) << 24))
/* The chunk after c: past its data and past the 0x10-byte "EOFC" trailer. */
#define GSC_NEXT_CHUNK(c) ((GscChunk *)((u8 *)((c) + 1) + (c)->headSize + (c)->dataSize))

typedef s32 (*GscHandler)(s32 phase, void *work);

/* Command table entry; the table ends at an entry whose handler is NULL. */
typedef struct GscCommand {
    /* 0x00 */ s32 id;
    /* 0x04 */ GscHandler handler;
} GscCommand;

/* A loaded script file. Size 0x24. */
typedef struct GscFile {
    /* 0x00 */ ListNode link;
    /* 0x08 */ s32 unk8;       /* never written */
    /* 0x0C */ s32 handle;     /* serial << 16 */
    /* 0x10 */ s32 taskSerial; /* number of tasks started from this file */
    /* 0x14 */ GscChunk *file; /* "GSCF" */
    /* 0x18 */ GscChunk *head; /* "GSHD" */
    /* 0x1C */ GscChunk *code; /* "GSCD": holds the GSAC chunks */
    /* 0x20 */ GscChunk *data; /* "GSDT": operand values */
} GscFile;

/* A running action. Size 0x50. */
typedef struct GscTask {
    /* 0x00 */ ListNode link;
    /* 0x08 */ s32 unk8;      /* never written */
    /* 0x0C */ s32 id;        /* file handle | task serial */
    /* 0x10 */ s32 action;    /* action id it was started with */
    /* 0x14 */ GscChunk *data; /* the file's GSDT chunk */
    /* 0x18 */ u32 *pc;       /* current command word */
    /* 0x1C */ u32 *operand;  /* next operand word of the current command */
    /* 0x20 */ s32 flags;     /* bit 0: command begun (or child started), bit 1: abort begun */
    /* 0x24 */ s32 time;      /* GSC_TIME_STEP per frame spent waiting in the current command */
    /* 0x28 */ s32 child;     /* task id started by a CALL_WAIT word */
    /* 0x2C */ s32 abort;     /* set by Gsc_AbortTask */
    /* 0x30 */ u8 work[0x20]; /* handed to the command handlers; cleared by nobody */
} GscTask;

void Gsc_Init(GscCommand *commands, s32 fileMax, s32 taskMax);
void Gsc_InitDefault(GscCommand *commands);
void Gsc_Exit(void);
void Gsc_Nop(void);
void Gsc_SetCommands(GscCommand *commands);
void Gsc_Update(void);
void Gsc_StepTaskById(s32 id);
void Gsc_NotifyAll(void);
void Gsc_StepTask(GscTask *task);
s32 Gsc_LoadFile(GscChunk *file);
void Gsc_UnloadFile(s32 handle);
GscFile *Gsc_FindFile(s32 handle);
s32 Gsc_IsFileLoaded(s32 handle);
s32 Gsc_GetFileCount(void);
GscTask *Gsc_FindTask(s32 id);
s32 Gsc_IsTaskAlive(s32 id);
s32 Gsc_GetTaskCount(void);
s32 Gsc_GetTaskTime(s32 id);
s32 Gsc_StartMain(s32 handle);
void Gsc_RunAction(s32 handle, s32 action);
s32 Gsc_StartAction(s32 handle, s32 action);
void Gsc_KillTask(s32 id);
void Gsc_AbortTask(s32 id);
s32 Gsc_HasAction(s32 handle, s32 action);
GscTask *Gsc_GetCurrentTask(void);
s32 Gsc_CallAction(s32 action);
s32 Gsc_GetCurrentTime(void);
s32 Gsc_IsCurrentBegun(void);
void Gsc_DumpFile(s32 handle);
void Gsc_DumpCurrent(void);
void Gsc_DumpCommand(u32 *cmd, GscChunk *data);
void Gsc_DumpFileNode(GscFile *file);
void Gsc_DumpCommandsById(GscFile *file, s32 id);
s32 Gsc_GetInt(void);
f32 Gsc_GetFloat(void);
char *Gsc_GetString(void);
s32 Gsc_GetIntOr(s32 def);
f32 Gsc_GetFloatOr(f32 def);
char *Gsc_GetStringOr(char *def);
s32 Gsc_SkipOperand(void);
void Gsc_ResetOperands(void);
s32 Gsc_FindOption(s8 letter);
s32 Gsc_FindOptionN(s8 letter, s32 n);
s32 Gsc_SeekOption(s32 n);
s32 Gsc_CountOptions(void);
s32 Gsc_CountOption(s8 letter);
s32 Gsc_HasOperand(void);
s32 Gsc_MakeFileHandle(s32 serial);
s32 Gsc_MakeTaskId(s32 handle, s32 serial);
s32 Gsc_GetFileHandle(s32 id);
u32 *Gsc_NextCommand(u32 *cmd);
void *GscTask_PeekOperand(GscTask *task);
void *GscTask_NextOperand(GscTask *task, s32 type);
GscHandler Gsc_FindHandler(s32 id);
GscFile *Gsc_GetFreeFile(void);
void Gsc_LinkFile(GscFile *file);
void Gsc_FreeFile(GscFile *file);
GscTask *Gsc_GetFreeTask(void);
void Gsc_LinkTask(GscTask *task);
void Gsc_FreeTask(GscTask *task);
u32 *Gsc_FindAction(GscChunk *code, s32 action, s32 unused);

#endif
