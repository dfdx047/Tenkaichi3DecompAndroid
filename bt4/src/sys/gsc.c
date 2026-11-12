#include "common.h"
#include "sys/gsc.h"
#include "sys/heap.h"

/*
 * GSC script engine: 0x257870..0x258D20.
 *
 * A small cooperative script machine. A script file is registered with Gsc_LoadFile, an "action" of it is
 * started as a task with Gsc_StartAction, and Gsc_Update steps every task once per frame. The engine knows
 * nothing about the game: each command id is looked up in a table of handlers given to Gsc_Init.
 *
 * File ("GSCF"), all words little-endian, every chunk = 0x10-byte header {tag, headSize, dataSize, arg},
 * dataSize bytes, then a 0x10-byte "EOFC" trailer:
 *   GSCF  wraps the whole file
 *     GSHD  header data (not read by this file)
 *     GSCD  code: a run of GSAC chunks, ended by anything that is not a GSAC
 *       GSAC  one action; arg = action id (-1 = the main action); data = 32-bit script words
 *     GSDT  data: 4-byte values (int, float or anything a handler takes the address of)
 *
 * Script words (low nibble = kind):
 *   0   end of the action: the task is freed
 *   1   command: byte 1 = number of plain operand words that follow, bytes 2..3 = command id
 *   2   start action (s16 in bytes 2..3) as a new task, step it once, go on
 *   3   start action and wait until that task is gone
 *   8   option: byte 1 = a letter, byte 2 = number of operand words that follow it
 *   10  operand: bits 8..31 index a 4-byte value in GSDT. Bits 4..7 give its type in the files (0 int,
 *       1 float; the dump's name table adds 2 = string) but no code left in the game reads them: the handler
 *       decides by calling Gsc_GetInt, Gsc_GetFloat or Gsc_GetString.
 * Options follow the plain operands of a command; every word with bit 3 set belongs to the command before.
 * A handler reads its plain operands in order, and looks an option up by its letter (Gsc_FindOption) before
 * reading that option's operands, so a command line reads like "cmd 1 2 -l 4.0 -w".
 *
 * A command runs in phases (see GSC_PHASE_*): BEGIN once, UPDATE every frame until it returns non-zero, then
 * END. A command that is not done stops the task for this frame and adds 10 to the task's timer.
 */

extern s32 gGscActive;
extern GscCommand *gGscCommands;
extern s32 gGscFileSerial;
extern s32 gGscFileMax;
extern GscFile *gGscFilePool;
extern GscTask *gGscCurTask;
extern s32 gGscTaskMax;
extern GscTask *gGscTaskPool;
extern List gGscTaskList; /* running tasks */
extern List gGscFileFree;
extern List gGscFileList; /* loaded files */
extern List gGscTaskFree;

/* Allocates the file and task pools and puts every element on its free list. */
void Gsc_Init(GscCommand *commands, s32 fileMax, s32 taskMax) {
    s32 i;

    gGscActive = 1;
    gGscFilePool = Heap_Alloc(fileMax * sizeof(GscFile), 0x20, 0, 2);
    gGscTaskPool = Heap_Alloc(taskMax * sizeof(GscTask), 0x20, 0, 2);
    gGscCommands = commands;
    gGscFileMax = fileMax;
    gGscFileSerial = 0;
    List_Init(&gGscFileList);
    List_Init(&gGscFileFree);
    gGscTaskMax = taskMax;
    gGscCurTask = NULL;
    List_Init(&gGscTaskList);
    List_Init(&gGscTaskFree);
    for (i = 0; i < gGscFileMax; i++) {
        List_PushBack(&gGscFileFree, &gGscFilePool[i].link);
    }
    for (i = 0; i < gGscTaskMax; i++) {
        List_PushBack(&gGscTaskFree, &gGscTaskPool[i].link);
    }
}

/* Starts the engine with room for 10 files and 50 tasks. */
void Gsc_InitDefault(GscCommand *commands) {
    Gsc_Init(commands, 10, 50);
}

/* Shuts the engine down and frees both pools. */
void Gsc_Exit(void) {
    List_Init(&gGscFileList);
    List_Init(&gGscFileFree);
    List_Init(&gGscTaskList);
    List_Init(&gGscTaskFree);
    Heap_Free(gGscFilePool);
    Heap_Free(gGscTaskPool);
    gGscActive = 0;
    gGscCommands = NULL;
    gGscCurTask = NULL;
}

/* Does nothing. */
void Gsc_Nop(void) {
}

/* Replaces the command table. */
void Gsc_SetCommands(GscCommand *commands) {
    gGscCommands = commands;
}

/* Steps every running task once. */
void Gsc_Update(void) {
    GscTask *task;
    GscTask *next;

    if (gGscActive) {
        for (task = (GscTask *)List_GetHead(&gGscTaskList); task != NULL; task = next) {
            next = (GscTask *)List_GetNext(&task->link);
            Gsc_StepTask(task);
        }
    }
}

/* Steps the task with this id once. */
void Gsc_StepTaskById(s32 id) {
    if (gGscActive) {
        Gsc_StepTask(Gsc_FindTask(id));
    }
}

/* Calls the handler of every task's current command with phase 3. */
void Gsc_NotifyAll(void) {
    GscTask *task;
    GscHandler handler;

    if (gGscActive) {
        task = (GscTask *)List_GetHead(&gGscTaskList);
        if (task != NULL) {
            do {
                gGscCurTask = task;
                if ((*task->pc & 0xF) == GSC_WORD_COMMAND) {
                    handler = Gsc_FindHandler(((u16 *)task->pc)[1]);
                    Gsc_ResetOperands();
                    handler(GSC_PHASE_NOTIFY, task->work);
                }
                task = (GscTask *)List_GetNext(&task->link);
            } while (task != NULL);
            gGscCurTask = NULL;
        }
    }
}

/* Runs a task until one of its commands has to wait or its action ends. */
void Gsc_StepTask(GscTask *task) {
    s32 run;
    GscHandler handler;
    s32 done;
    u32 *cmd;
    s32 type;
    u32 *next;
    u32 *next2;
    s32 action;

    gGscCurTask = task;
    run = 1;
    do {
        cmd = task->pc;
        type = *cmd & 0xF;
        switch (type) {
        case GSC_WORD_END:
            Gsc_FreeTask(task);
            run = 0;
            break;
        case GSC_WORD_CALL:
            Gsc_CallAction(((s16 *)cmd)[1]);
            next = task->pc;
            task->flags = 0;
            task->time = 0;
            task->pc = next + 1;
            break;
        case GSC_WORD_CALL_WAIT:
            action = ((s16 *)cmd)[1];
            if (!(task->flags & 1)) {
                task->child = Gsc_CallAction(action);
                task->flags |= 1;
            }
            if (task->abort) {
                Gsc_AbortTask(task->child);
            }
            if (!Gsc_IsTaskAlive(task->child)) {
                next2 = task->pc;
                task->flags = 0;
                task->time = 0;
                task->pc = next2 + 1;
            } else {
                run = 0;
            }
            break;
        case GSC_WORD_COMMAND:
            done = 0;
            handler = Gsc_FindHandler(((u16 *)cmd)[1]);
            if (task->abort) {
                if (!(task->flags & 2)) {
                    Gsc_ResetOperands();
                    done = handler(GSC_PHASE_BEGIN_ABORT, task->work);
                    task->flags |= 2;
                }
            } else if (!(task->flags & 1)) {
                Gsc_ResetOperands();
                done = handler(GSC_PHASE_BEGIN, task->work);
                task->flags |= 1;
            }
            if (done == 0) {
                Gsc_ResetOperands();
                done = handler(GSC_PHASE_UPDATE, task->work);
            }
            if (done != 0) {
                Gsc_ResetOperands();
                handler(GSC_PHASE_END, task->work);
                cmd = Gsc_NextCommand(task->pc);
                task->flags = 0;
                task->pc = cmd;
                task->time = 0;
            } else {
                run = 0;
                task->time += GSC_TIME_STEP;
            }
            break;
        default:
            break;
        }
    } while (run);
    gGscCurTask = NULL;
}

/* Registers a loaded script file and returns its handle. */
s32 Gsc_LoadFile(GscChunk *file) {
    GscFile *node;
    GscChunk *chunk;
    u32 i;
    u32 count;

    chunk = file;
    count = (sizeof(GscChunk) + chunk->headSize + chunk->dataSize) >> 2;
    for (i = 0; i < count; i++) {
    }
    node = Gsc_GetFreeFile();
    node->handle = Gsc_MakeFileHandle(gGscFileSerial);
    node->taskSerial = 0;
    gGscFileSerial++;
    node->file = chunk;
    chunk = (GscChunk *)((u8 *)chunk + chunk->headSize);
    node->head = chunk;
    chunk = GSC_NEXT_CHUNK(chunk);
    node->code = chunk;
    chunk = GSC_NEXT_CHUNK(chunk);
    node->data = chunk;
    Gsc_LinkFile(node);
    return node->handle;
}

/* Kills the tasks of a file and releases it. */
void Gsc_UnloadFile(s32 handle) {
    GscFile *file = Gsc_FindFile(handle);

    if (file != NULL) {
        Gsc_FreeFile(file);
    }
}

/* Returns the loaded file with this handle, or NULL. */
GscFile *Gsc_FindFile(s32 handle) {
    GscFile *file;
    GscFile *found;

    file = (GscFile *)List_GetHead(&gGscFileList);
    found = NULL;
    while (file != NULL) {
        if (file->handle == handle) {
            found = file;
            break;
        }
        file = (GscFile *)List_GetNext(&file->link);
    }
    return found;
}

/* Returns 1 when a file with this handle is loaded. */
s32 Gsc_IsFileLoaded(s32 handle) {
    return Gsc_FindFile(handle) != NULL;
}

/* Returns the number of loaded files. */
s32 Gsc_GetFileCount(void) {
    return List_GetCount(&gGscFileList);
}

/* Returns the running task with this id, or NULL. */
GscTask *Gsc_FindTask(s32 id) {
    GscTask *task;
    GscTask *found;

    task = (GscTask *)List_GetHead(&gGscTaskList);
    found = NULL;
    while (task != NULL) {
        if (task->id == id) {
            found = task;
            break;
        }
        task = (GscTask *)List_GetNext(&task->link);
    }
    return found;
}

/* Returns 1 while the task with this id is running. */
s32 Gsc_IsTaskAlive(s32 id) {
    return Gsc_FindTask(id) != NULL;
}

/* Returns the number of running tasks. */
s32 Gsc_GetTaskCount(void) {
    return List_GetCount(&gGscTaskList);
}

/* Returns how long the task's current command has been waiting (10 per frame), 0 if there is no such task. */
s32 Gsc_GetTaskTime(s32 id) {
    GscTask *task = Gsc_FindTask(id);
    s32 time = 0;

    if (task != NULL) {
        time = task->time;
    }
    return time;
}

/* Starts the main action (id -1) of a file. */
s32 Gsc_StartMain(s32 handle) {
    return Gsc_StartAction(handle, -1);
}

/* Starts an action and steps it until it has ended. */
void Gsc_RunAction(s32 handle, s32 action) {
    s32 id = Gsc_StartAction(handle, action);

    while (Gsc_IsTaskAlive(id)) {
        Gsc_StepTaskById(id);
    }
}

/* Starts an action of a file as a new task and returns the task id. */
s32 Gsc_StartAction(s32 handle, s32 action) {
    GscFile *file = Gsc_FindFile(handle);
    GscTask *task = Gsc_GetFreeTask();

    task->action = action;
    task->id = Gsc_MakeTaskId(handle, file->taskSerial);
    file->taskSerial++;
    task->data = file->data;
    task->pc = Gsc_FindAction(file->code, action, 1);
    task->operand = NULL;
    task->flags = 0;
    task->time = 0;
    task->child = 0;
    task->abort = 0;
    Gsc_LinkTask(task);
    return task->id;
}

/* Stops a task at once. */
void Gsc_KillTask(s32 id) {
    GscTask *task = Gsc_FindTask(id);

    if (task != NULL) {
        Gsc_FreeTask(task);
    }
}

/* Tells a task to abort: its commands are begun with phase 4 from now on. */
void Gsc_AbortTask(s32 id) {
    GscTask *task = Gsc_FindTask(id);

    if (task != NULL) {
        task->abort = 1;
    }
}

/* Returns 1 when the file has an action with this id. */
s32 Gsc_HasAction(s32 handle, s32 action) {
    return Gsc_FindAction(Gsc_FindFile(handle)->code, action, 0) != NULL;
}

/* Returns the task being stepped (NULL outside a step). */
GscTask *Gsc_GetCurrentTask(void) {
    return gGscCurTask;
}

/* From inside a task: starts another action of the same file, steps it once and returns its task id. */
s32 Gsc_CallAction(s32 action) {
    GscTask *cur = Gsc_GetCurrentTask();
    s32 id = Gsc_StartAction(Gsc_GetFileHandle(cur->id), action);

    Gsc_StepTask(Gsc_FindTask(id));
    gGscCurTask = cur;
    return id;
}

/* Returns how long the current command has been waiting (10 per frame). */
s32 Gsc_GetCurrentTime(void) {
    return Gsc_GetTaskTime(Gsc_GetCurrentTask()->id);
}

/* Returns bit 0 of the current task's flags (the command has been begun). */
s32 Gsc_IsCurrentBegun(void) {
    return Gsc_GetCurrentTask()->flags & 1;
}

/* Debug dump of a whole file (output compiled out). */
void Gsc_DumpFile(s32 handle) {
    Gsc_DumpFileNode(Gsc_FindFile(handle));
}

/* Debug dump of the current command (output compiled out). */
void Gsc_DumpCurrent(void) {
    GscChunk *data = gGscCurTask->data;

    Gsc_DumpCommand(gGscCurTask->pc, (GscChunk *)((u8 *)data + data->headSize));
}

/* Debug dump of one command and its operands. The printing was compiled out: what is left is the table of
 * operand type names, the range check of the switch on the word kind, and the walk over the words. The case
 * bodies below are placeholders: any stores that are dead once the printing is gone give the original code. */
void Gsc_DumpCommand(u32 *cmd, GscChunk *data) {
    char *typeNames[5] = { "int   ", "float ", "string" };
    s32 kind;

    do {
        kind = *cmd & 0xF;
        switch (kind) {
        case GSC_WORD_COMMAND:
            kind = ((u8 *)cmd)[1];
            break;
        case GSC_WORD_CALL:
            kind = ((s16 *)cmd)[1];
            break;
        case GSC_WORD_CALL_WAIT:
            kind = ((s16 *)cmd)[1];
            break;
        case GSC_WORD_OPTION:
            kind = ((u8 *)cmd)[2];
            break;
        case GSC_WORD_OPERAND:
            kind = (*cmd >> 4) & 0xF;
            break;
        }
        cmd++;
    } while (*cmd & GSC_WORD_ARG);
}

/* Debug dump of every command of every action of a file (output compiled out). */
void Gsc_DumpFileNode(GscFile *file) {
    GscChunk *action;
    u32 *data;
    u32 *cmd;

    action = (GscChunk *)((u8 *)file->code + file->code->headSize);
    data = (u32 *)((u8 *)file->data + file->data->headSize);
    while (GSC_SWAP(action->tag) == GSC_TAG_GSAC) {
        cmd = (u32 *)((u8 *)action + action->headSize);
        while ((*cmd & 0xF) != GSC_WORD_END) {
            Gsc_DumpCommand(cmd, (GscChunk *)data);
            cmd = Gsc_NextCommand(cmd);
        }
        action = GSC_NEXT_CHUNK(action);
    }
}

/* Debug dump of every use of one command id in a file (output compiled out). */
void Gsc_DumpCommandsById(GscFile *file, s32 id) {
    GscChunk *action;
    u32 *data;
    u32 *cmd;

    action = (GscChunk *)((u8 *)file->code + file->code->headSize);
    data = (u32 *)((u8 *)file->data + file->data->headSize);
    while (GSC_SWAP(action->tag) == GSC_TAG_GSAC) {
        cmd = (u32 *)((u8 *)action + action->headSize);
        while ((*cmd & 0xF) != GSC_WORD_END) {
            if ((*cmd & 0xF) == GSC_WORD_COMMAND && ((u16 *)cmd)[1] == id) {
                Gsc_DumpCommand(cmd, (GscChunk *)data);
            }
            cmd = Gsc_NextCommand(cmd);
        }
        action = GSC_NEXT_CHUNK(action);
    }
}

/* Returns the next operand of the current command as an integer. */
s32 Gsc_GetInt(void) {
    return *(s32 *)GscTask_NextOperand(gGscCurTask, 0);
}

/* Returns the next operand of the current command as a float. */
f32 Gsc_GetFloat(void) {
    return *(f32 *)GscTask_NextOperand(gGscCurTask, 1);
}

/* Returns the next operand of the current command as a string (the address of its data). */
char *Gsc_GetString(void) {
    return GscTask_NextOperand(gGscCurTask, 2);
}

/* Returns the next integer operand, or def when the command has none left. */
s32 Gsc_GetIntOr(s32 def) {
    s32 *value = GscTask_NextOperand(gGscCurTask, 0);

    if (value == NULL) {
        return def;
    }
    return *value;
}

/* Returns the next float operand, or def when the command has none left. */
f32 Gsc_GetFloatOr(f32 def) {
    f32 *value = GscTask_NextOperand(gGscCurTask, 1);

    if (value == NULL) {
        return def;
    }
    return *value;
}

/* Returns the next string operand, or def when the command has none left. */
char *Gsc_GetStringOr(char *def) {
    char *value = GscTask_NextOperand(gGscCurTask, 2);

    if (value != NULL) {
        def = value;
    }
    return def;
}

/* Skips one operand of the current command; returns 0 when there was none. */
s32 Gsc_SkipOperand(void) {
    s32 has = Gsc_HasOperand();

    if (has) {
        gGscCurTask->operand++;
    }
    return has;
}

/* Puts the operand cursor on the word after the current command word. */
void Gsc_ResetOperands(void) {
    gGscCurTask->operand = gGscCurTask->pc + 1;
}

/* Puts the cursor on the operands of the first option with this letter; returns 0 if there is none. */
s32 Gsc_FindOption(s8 letter) {
    return Gsc_FindOptionN(letter, 0);
}

/* Puts the cursor on the operands of the n-th option with this letter; returns 0 if there is none. */
s32 Gsc_FindOptionN(s8 letter, s32 n) {
    u8 *word;
    s32 type;
    GscTask *task = gGscCurTask;
    s32 found = 0;
    s32 count = 0;

    word = (u8 *)(task->pc + ((u8 *)task->pc)[1] + 1);
    while (type = *(u32 *)word & 0xF, *(u32 *)word & GSC_WORD_ARG) {
        if (type == GSC_WORD_OPTION && word[1] == letter) {
            if (count >= n) {
                found = 1;
                task->operand = (u32 *)word + 1;
                break;
            }
            count++;
            word = word + word[2] * 4 + 4;
        } else {
            word += 4;
        }
    }
    return found;
}

/* Puts the cursor on the operands of the n-th option of any letter and returns its letter (0 if none). */
s32 Gsc_SeekOption(s32 n) {
    u8 *word;
    s32 type;
    GscTask *task = gGscCurTask;
    s32 letter = 0;
    s32 count = 0;

    word = (u8 *)(task->pc + ((u8 *)task->pc)[1] + 1);
    while (type = *(u32 *)word & 0xF, *(u32 *)word & GSC_WORD_ARG) {
        if (type == GSC_WORD_OPTION) {
            if (count >= n) {
                task->operand = (u32 *)word + 1;
                letter = (s8)word[1];
                break;
            }
            count++;
            word = word + word[2] * 4 + 4;
        } else {
            word += 4;
        }
    }
    return letter;
}

/* Returns the number of options of the current command. */
s32 Gsc_CountOptions(void) {
    s8 count = 0;
    u8 *word = (u8 *)(gGscCurTask->pc + ((u8 *)gGscCurTask->pc)[1] + 1);

    while (*(u32 *)word & GSC_WORD_ARG) {
        if ((*(u32 *)word & 0xF) == GSC_WORD_OPTION) {
            count++;
            word = word + word[2] * 4 + 4;
        } else {
            word += 4;
        }
    }
    return count;
}

/* Returns how many options of the current command have this letter. */
s32 Gsc_CountOption(s8 letter) {
    s8 count = 0;
    u8 *word = (u8 *)(gGscCurTask->pc + ((u8 *)gGscCurTask->pc)[1] + 1);

    while (*(u32 *)word & GSC_WORD_ARG) {
        if ((*(u32 *)word & 0xF) == GSC_WORD_OPTION) {
            if (word[1] == letter) {
                count++;
            }
            word = word + word[2] * 4 + 4;
        } else {
            word += 4;
        }
    }
    return count;
}

/* Returns 1 when the word at the operand cursor is an operand. */
s32 Gsc_HasOperand(void) {
    return (*gGscCurTask->operand & 0xF) == GSC_WORD_OPERAND;
}

/* Makes a file handle from a serial number. */
s32 Gsc_MakeFileHandle(s32 serial) {
    return serial << 16;
}

/* Makes a task id from a file handle and the file's task serial. */
s32 Gsc_MakeTaskId(s32 handle, s32 serial) {
    return handle | (serial & 0xFFFF);
}

/* Returns the file handle part of a task id. */
s32 Gsc_GetFileHandle(s32 id) {
    return id & 0xFFFF0000;
}

/* Returns the command word that follows a command, its operands and its options. */
u32 *Gsc_NextCommand(u32 *cmd) {
    s32 count = ((u8 *)cmd)[1];
    u8 *word = (u8 *)(cmd + count + 1);
    s32 n;

    while (*(u32 *)word & GSC_WORD_ARG) {
        if ((*(u32 *)word & 0xF) == GSC_WORD_OPTION) {
            n = word[2] + 1;
            word = word + word[2] * 4 + 4;
            count += n;
        } else {
            word += 4;
        }
    }
    return cmd + count + 1;
}

/* Returns the address of the value of the operand at the cursor, or NULL if the word is not an operand. */
void *GscTask_PeekOperand(GscTask *task) {
    u32 word = *task->operand;
    u32 *values = (u32 *)((u8 *)task->data + task->data->headSize);
    void *value = NULL;

    if ((word & 0xF) == GSC_WORD_OPERAND) {
        value = &values[word >> 8];
    }
    return value;
}

/* Returns the value address of the operand at the cursor and moves past it; NULL if there is none. */
void *GscTask_NextOperand(GscTask *task, s32 type) {
    void *value = GscTask_PeekOperand(task);

    if (value != NULL) {
        task->operand++;
    }
    return value;
}

/* Returns the handler of a command id, or NULL when the table does not have it. */
GscHandler Gsc_FindHandler(s32 id) {
    GscCommand *entry = gGscCommands;
    GscHandler found = NULL;

    while (entry->handler != NULL) {
        if (entry->id == id) {
            found = entry->handler;
            break;
        }
        entry++;
    }
    return found;
}

/* Returns the first unused file slot (it stays on the free list until Gsc_LinkFile). */
GscFile *Gsc_GetFreeFile(void) {
    return (GscFile *)List_GetHead(&gGscFileFree);
}

/* Moves a file slot from the free list to the loaded list. */
void Gsc_LinkFile(GscFile *file) {
    List_Remove(&gGscFileFree, &file->link);
    List_PushBack(&gGscFileList, &file->link);
}

/* Frees every task started from a file, then returns the file slot to the free list. */
void Gsc_FreeFile(GscFile *file) {
    GscTask *task;
    GscTask *next;

    for (task = (GscTask *)List_GetHead(&gGscTaskList); task != NULL; task = next) {
        next = (GscTask *)List_GetNext(&task->link);
        if (Gsc_GetFileHandle(task->id) == file->handle) {
            Gsc_FreeTask(task);
        }
    }
    List_Remove(&gGscFileList, &file->link);
    List_PushBack(&gGscFileFree, &file->link);
}

/* Returns the first unused task slot (it stays on the free list until Gsc_LinkTask). */
GscTask *Gsc_GetFreeTask(void) {
    return (GscTask *)List_GetHead(&gGscTaskFree);
}

/* Moves a task slot from the free list to the front of the running list. */
void Gsc_LinkTask(GscTask *task) {
    List_Remove(&gGscTaskFree, &task->link);
    List_PushFront(&gGscTaskList, &task->link);
}

/* Moves a task from the running list back to the free list. */
void Gsc_FreeTask(GscTask *task) {
    List_Remove(&gGscTaskList, &task->link);
    List_PushBack(&gGscTaskFree, &task->link);
}

/* Returns the first command word of the action with this id, or NULL when the file has no such action. */
u32 *Gsc_FindAction(GscChunk *code, s32 action, s32 unused) {
    GscChunk *chunk = (GscChunk *)((u8 *)code + code->headSize);
    u32 *found = NULL;

    while (GSC_SWAP(chunk->tag) == GSC_TAG_GSAC) {
        if (chunk->arg == action) {
            found = (u32 *)((u8 *)chunk + chunk->headSize);
            break;
        }
        chunk = GSC_NEXT_CHUNK(chunk);
    }
    return found;
}
