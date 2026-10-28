#include "common.h"
#include "battle/btl_script.h"
#include "battle/btl_seq.h"
#include "sys/gsc.h"

#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "battle/btl_facade.h"
#include "sys/adx.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/save.h"

/*
 * Battle event script, first part: 0x258D20..0x259EC8.
 *
 * The battle's use of the GSC engine (src/sys/gsc.c): the module work, the scan of the script's event actions
 * at load time, the per-frame hooks, and everything that connects a script to the fight:
 *
 *   - Setup. The load job runs action 1000 to its end (Gsc_RunAction). Its commands fill this work: the story
 *     number, the text windows, who speaks which line (BtlScript_SetSpeaker) and the trigger list
 *     (BtlScript_AddLine: "say line L when trigger T fires"; BtlScript_AddEvent: "start action A when T fires").
 *   - Main task. In mode 1 Battle_ResetWork starts action -1 and keeps its task id in BattleEvents.unk4.
 *     Gsc_Update steps it every unpaused frame.
 *   - Triggers. BtlScript_Update (same frames, sequence states 3 and 5 only) tests every trigger against the
 *     battle events that are new this frame (BtlEvent_IsNew). A line is said at once if no other line is being
 *     said. An event action becomes `pending`, and BtlScript_StartPending starts it as soon as the fighters can
 *     be interrupted: the task in BattleEvents.unk4 is killed and replaced by the new action's task.
 *
 * The module goes on after 0x259EC8 with one handler per command of gBtlScriptCommands (0x259EC8..0x25C200,
 * the interpreter proper); that part is not decompiled.
 */

extern void *memset(void *dst, s32 c, u32 n);
/* file.h declares File_Request with two arguments; the callers here pass a third (0), as battle_load.c does */
#define File_Request3(id, buf, size) ((void *(*)(s32, void *, s32))File_Request)(id, buf, size)
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *in, f32 scale);
/* text drawing of the next module (0x23A2D0..0x23AD48), not decompiled */
extern s32 Font_GetCmdCount(void);
extern void Font_PushStyle(void);
extern void Font_SetStyle(void *window);
extern s32 Font_GetHeight(void *text);
extern void Font_PrintAt(s32 x, s32 y, void *text);
extern void Font_PopStyle(void);
extern void Font_Flush(s32 state);
/* fighter / HUD side, not decompiled */
extern s32 BtlCtrl_IsActiveDead(s32 side);
extern void HudPrompt_ShowCue(void);
extern void HudPrompt_HideCue(void);
extern void HudPrompt_SetCueDim(s32 arg);
extern s32 BtlCtrl_GetActiveMember(s32 side);

extern List gGscTaskList;

extern BtlScriptWork gBtlScript;

/* Clears the work: no story, no text, no speaker set for any line. */
void BtlScript_InitWork(void) {
    BtlScriptWork *work = &gBtlScript;
    s32 i;

    memset(work, 0, sizeof(BtlScriptWork));
    work->story = -1;
    work->textId = -1;
    work->textWindow = 0;
    for (i = 0; i < 300; i++) {
        work->talk.speakers[i].side = 0;
        work->talk.speakers[i].member = BTL_SCRIPT_NO_MEMBER;
    }
}

/* Battle load: clears the script work. */
void BtlScript_Init(void) {
    BtlScript_InitWork();
}

/* Does nothing. */
void BtlScript_Nop(void) {
}

/* Notes, for every event action (id 10000..10051) of the script, that it exists, its group and what it uses. */
void BtlScript_ScanEvents(s32 script) {
    BtlScriptWork *work = &gBtlScript;
    GscFile *file = Gsc_FindFile(script);
    GscChunk *action;
    u32 *values;
    u32 *cmd;
    BtlScriptEvent *ev;
    s32 id;
    s32 group;
    s32 value;

    action = (GscChunk *)((u8 *)file->code + file->code->headSize);
    values = (u32 *)((u8 *)file->data + file->data->headSize);
    while (GSC_SWAP(action->tag) == GSC_TAG_GSAC) {
        id = action->arg;
        cmd = (u32 *)((u8 *)action + action->headSize);
        if (id >= BTL_SCRIPT_ACTION_EVENT) {
            ev = &work->events[id - BTL_SCRIPT_ACTION_EVENT];
            ev->present = 1;
            group = 4;
            if (id < 10051) {
                group = 3;
                if (id < 10050) {
                    if (id < 10040) {
                        group = id >= 10030;
                    } else {
                        group = 2;
                    }
                }
            }
            ev->group = group;
            ev->cmd4B1 = 0;
            ev->kind = 0;
            while ((*cmd & 0xF) != GSC_WORD_END) {
                if ((*cmd & 0xF) == GSC_WORD_COMMAND) {
                    switch (((u16 *)cmd)[1]) {
                    case 0x4B1:
                        ev->cmd4B1 = 1;
                        break;
                    case 8:
                        value = values[cmd[1] >> 8];
                        ev->side = (value >> 15) & 1;
                        switch (value & 0xFFF) {
                        case 0x10:
                            ev->kind = 1;
                            break;
                        case 0x11:
                            ev->kind = 1;
                            break;
                        case 0x12:
                            ev->kind = 1;
                            break;
                        case 0x13:
                            ev->kind = 1;
                            break;
                        case 0x14:
                            ev->kind = 1;
                            break;
                        case 0x15:
                        case 0x16:
                        case 0x17:
                            ev->kind = 2;
                            break;
                        }
                        break;
                    }
                }
                cmd = Gsc_NextCommand(cmd);
            }
        }
        action = GSC_NEXT_CHUNK(action);
    }
}

/* Restarts the script state for a rematch. */
void BtlScript_Restart(void) {
    BtlScriptWork *work = &gBtlScript;

    work->cur.action = 0;
    work->cur.event = NULL;
    BtlScript_ResetView();
}

/* Per-frame update of the script side; only acts in sequence states 3 and 5. */
void BtlScript_Update(void) {
    if (BtlSeq_GetState() == 3 || BtlSeq_GetState() == 5) {
        BtlScript_UpdateEvents();
    }
}

/* Per-frame view update: keeps the camera shake going, runs the scripted camera move, draws the text. */
void BtlScript_UpdateView(void) {
    BtlScriptWork *work = &gBtlScript;
    BtlScriptCam *cam = &work->cam;
    BtlScriptCamKey *key;
    BtlScriptCamKey *next;
    Vec4 pos;
    Vec4 rot;
    s32 total;
    f32 t;
    s32 state;
    BtlScriptWindow *window;
    void *text;

    if (work->shakeTime != 0.0f) {
        BtlFacade_ShakeCamera(work->shakeTime);
    }
    if (cam->active) {
        key = &cam->key[cam->index];
        next = &cam->key[cam->index + 1];
        if (cam->index < 8 && (next->flags & 1)) {
            total = next->time * 300.0f;
            cam->time += 10;
            t = (f32)cam->time / (f32)total;
            if (t > 1.0f) {
                t = 1.0f;
            }
            Vec4_Sub(&pos, &key[1].pos, &key->pos);
            Vec4_Sub(&rot, &key[1].rot, &key->rot);
            Vec4_Scale(&pos, &pos, t);
            Vec4_Scale(&rot, &rot, t);
            Vec4_Add(&pos, &pos, &key->pos);
            Vec4_Add(&rot, &rot, &key->rot);
            BtlFacade_SetFixedCamera(&pos, &rot);
            if (total < cam->time) {
                cam->time = 0;
                cam->index++;
            }
        } else {
            cam->active = 0;
            cam->time = 0;
            cam->index = 0;
        }
    }
    if (work->textId >= 0) {
        state = Font_GetCmdCount();
        window = BtlScript_GetWindow(work->textWindow);
        Font_PushStyle();
        Font_SetStyle(window);
        text = BtlScript_GetText(work->textId);
        Font_PrintAt(window->unk48, window->unk4C - Font_GetHeight(text), text);
        Font_PopStyle();
        Font_Flush(state);
    }
}

/* Frees the two loaded files and clears the work. */
void BtlScript_Free(void) {
    BtlScriptWork *work = &gBtlScript;

    if (work->textFile != NULL) {
        Heap_Free(work->textFile);
    }
    if (work->file8 != NULL) {
        Heap_Free(work->file8);
    }
    BtlScript_InitWork();
}

#define BtlScript_IsEventAction(id) (((id) < 10000) ? 0 : ((id) < 10052))

/* Returns 1 while a task started from an event action (10000..10051) is running. */
s32 BtlScript_IsEventRunning(void) {
    GscTask *task;
    s32 found = 0;

    for (task = (GscTask *)List_GetHead(&gGscTaskList); task != NULL; task = (GscTask *)List_GetNext(&task->link)) {
        if (BtlScript_IsEventAction(task->action)) {
            found = 1;
            break;
        }
    }
    return found;
}

/* Tells every running event task to abort. */
void BtlScript_AbortEvents(void) {
    GscTask *task;

    for (task = (GscTask *)List_GetHead(&gGscTaskList); task != NULL; task = (GscTask *)List_GetNext(&task->link)) {
        if (BtlScript_IsEventAction(task->action)) {
            Gsc_AbortTask(task->id);
        }
    }
}

/* Stores the story number. */
void BtlScript_SetStory(s32 story) {
    gBtlScript.story = story;
}

/* Returns the story number. */
s32 BtlScript_GetStory(void) {
    return gBtlScript.story;
}

/* Queues the read of the text file. */
void BtlScript_RequestTextFile(s32 id) {
    gBtlScript.textFile = File_Request3(id, NULL, 0);
}

/* Returns entry n of the text file. */
void *BtlScript_GetText(s32 n) {
    u32 *file = gBtlScript.textFile;

    return (u8 *)file + (file[n + 1] / 4) * 4;
}

/* Queues the read of the second file. */
void BtlScript_RequestFile8(s32 id) {
    gBtlScript.file8 = File_Request3(id, NULL, 0);
}

/* Returns entry n of the second file. */
void *BtlScript_GetFile8Entry(s32 n) {
    u32 *file = gBtlScript.file8;

    return (u8 *)file + (file[n + 1] / 4) * 4;
}

/* Returns text window n. */
BtlScriptWindow *BtlScript_GetWindow(s32 n) {
    return &gBtlScript.talk.windows[n];
}

/* Shows a text in a window from the next frame on. */
void BtlScript_SetText(s32 window, s32 text) {
    BtlScriptWork *work = &gBtlScript;

    work->textWindow = window;
    work->textId = text;
}

/* Hides the text if it is still the one given. */
void BtlScript_ClearText(s32 text) {
    BtlScriptWork *work = &gBtlScript;

    if (work->textId == text) {
        work->textId = -1;
    }
}

/* Sets who speaks line n: bit 15 of who is the side, the low 12 bits the team member. */
void BtlScript_SetSpeaker(s32 n, s32 who) {
    BtlScriptSpeaker *speaker = &gBtlScript.talk.speakers[n];
    s32 side = (who >> 15) & 1;

    speaker->side = side;
    speaker->member = who & 0xFFF;
}

/* Returns the speaker of line n. */
BtlScriptSpeaker *BtlScript_GetSpeaker(s32 n) {
    return &gBtlScript.talk.speakers[n];
}

/* Returns what the scan found about the event action that is running (NULL before the first one). */
BtlScriptEvent *BtlScript_GetCurrentEvent(void) {
    return gBtlScript.cur.event;
}

/* Puts the view state back: no request, no text, no camera move, no shake; then resets the handlers' side. */
void BtlScript_ResetView(void) {
    BtlScriptWork *work = &gBtlScript;

    work->talk.requestCount = 0;
    work->pending = NULL;
    work->textWindow = 0;
    work->shakeTime = 0.0f;
    work->cam.active = 0;
    work->textId = -1;
    BtlScript_StopVoices();
}

/* Tests a trigger. 0xFFFF is "the wait flag is off"; otherwise bit 15 is the side and the low 12 bits a
 * battle event that must be new this frame. With `range`, events 0x16..0x1C also fire on any later one. */
s32 BtlScript_IsTriggered(s32 trigger, s32 range) {
    s32 side = (trigger >> 15) & 1;
    s32 result = 0;
    s32 ev = trigger & 0xFFF;

    if (trigger == BTL_SCRIPT_TRIGGER_WAIT_OFF) {
        if (BtlFacade_IsWaitOff()) {
            result = 1;
        }
    } else {
        if (!range) {
            goto single;
        }
        switch (ev) {
        case 0x16:
            result = BtlEvent_IsNew(side, 0x16);
        case 0x17:
            result |= BtlEvent_IsNew(side, 0x17);
        case 0x18:
            result |= BtlEvent_IsNew(side, 0x18);
        case 0x19:
            result |= BtlEvent_IsNew(side, 0x19);
        case 0x1A:
            result |= BtlEvent_IsNew(side, 0x1A);
        case 0x1B:
            result |= BtlEvent_IsNew(side, 0x1B);
        case 0x1C:
            result |= BtlEvent_IsNew(side, 0x1C);
            break;
        default:
        single:
            result = BtlEvent_IsNew(side, ev);
            break;
        }
    }
    return result;
}

/* Returns the id of the streamed voice file of a line: 300 lines per story, two language sets. */
s32 BtlScript_GetVoiceFile(s32 line) {
    s32 base = DISC_RESOURCE(0xD48, 0x20000);

    if (gSaveData->flags & SAVE_FLAG_VOICE) {
        base = DISC_RESOURCE(0x47E0, 0x23A98);
    }
    return base + BtlScript_GetStory() * DISC_RESOURCE(300, 200) + line;
}

/* Registers "say this line when the trigger fires"; once = the line is switched off after it was said. */
void BtlScript_AddLine(s32 trigger, s32 line, s32 once) {
    BtlScriptTalk *talk = &gBtlScript.talk;
    BtlScriptRequest *req = &talk->requests[talk->requestCount];
    s32 on = 1;

    req->trigger = trigger;
    req->kind = on;
    req->u.line.enabled = on;
    req->u.line.id = line;
    req->u.line.once = once;
    talk->requestCount++;
}

/* Registers "start this event action when the trigger fires". */
void BtlScript_AddEvent(s32 trigger, s32 action) {
    BtlScriptTalk *talk = &gBtlScript.talk;
    BtlScriptRequest *req = &talk->requests[talk->requestCount];
    /* the work is reached back from the talk block: the original adds the action to that pointer first */
    BtlScriptWork *work = (BtlScriptWork *)((u8 *)talk - 0x354);

    req->kind = BTL_SCRIPT_REQ_EVENT;
    req->trigger = trigger;
    req->u.start.action = action;
    req->u.start.event = &work->events[action - BTL_SCRIPT_ACTION_EVENT];
    talk->requestCount++;
}

/* Returns 1 when the voice channel of a line's speaker is silent. */
s32 BtlScript_IsLineVoiceStopped(BtlScriptLine *line) {
    s32 side;
    s32 window;

    BtlScript_GetSpeakerSlot(&side, &window, BtlScript_GetSpeaker(line->id));
    return Voice_IsStopped(side) != 0;
}

/* Says a line: plays its voice file on the speaker's side and shows its text, unless the slot is busy. */
void BtlScript_PlayLine(BtlScriptLine *line) {
    BtlScriptWork *work = &gBtlScript;
    BtlScriptSpeaker *speaker;
    s32 file;
    s32 side;
    s32 window;

    speaker = BtlScript_GetSpeaker(line->id);
    file = BtlScript_GetVoiceFile(line->id);
    BtlScript_GetSpeakerSlot(&side, &window, speaker);
    if (work->voice[side * 2 + window] == NULL) {
        Voice_PlayDefault(side, file);
        work->voice[side * 2 + window] = line;
        BtlScript_SetText(window, line->id);
    }
}

/* Gives the voice channel (the side) and the text window of a speaker: window 0 when the speaker is the
 * member that is fighting, window 1 when it is another member or no member was set. */
void BtlScript_GetSpeakerSlot(s32 *side, s32 *window, BtlScriptSpeaker *speaker) {
    *side = speaker->side;
    if (speaker->member != BTL_SCRIPT_NO_MEMBER && BtlCtrl_GetActiveMember(speaker->side) == speaker->member) {
        *window = 0;
    } else {
        *window = 1;
    }
}

/* Returns 1 while any of the four line slots is in use. */
s32 BtlScript_IsLinePlaying(BtlScriptLine *line) {
    BtlScriptWork *work = &gBtlScript;
    s32 result = 0;

    if (work->voice[0] != NULL || work->voice[1] != NULL || work->voice[2] != NULL || work->voice[3] != NULL) {
        result = 1;
    }
    return result;
}

/* Starts every event action whose trigger is "wait flag off" at once. Called by BtlEvent_Raise for the
 * battle events 0x5A and 0x5B. */
void BtlScript_StartWaitEvents(void) {
    s32 i = 0;
    BattleEvents *ev = Battle_GetEventWork();
    BtlScriptWork *work = &gBtlScript;
    BtlScriptTalk *talk = &work->talk;
    BtlScriptRequest *req;
    BtlScriptStart *cur;

    for (req = talk->requests; i < talk->requestCount; i++, req++) {
        if (req->kind == BTL_SCRIPT_REQ_EVENT && req->trigger == BTL_SCRIPT_TRIGGER_WAIT_OFF) {
            cur = &work->cur;
            *cur = req->u.start;
            Gsc_KillTask(ev->unk4);
            BtlScript_ResetView();
            ev->unk4 = Gsc_StartAction(ev->script, cur->action);
        }
    }
}

/* Per-frame event side of the script (sequence states 3 and 5). */
void BtlScript_UpdateEvents(void) {
    BtlScript_UpdateLines();
    BtlScript_UpdateRequests();
    BtlScript_StartPending();
    BtlScript_UpdateHud();
}

/* Ends lines: 60 frames after a line's voice has stopped its text is removed and its slot freed. */
void BtlScript_UpdateLines(void) {
    BtlScriptWork *work = &gBtlScript;
    BtlScriptLine *line;
    s32 i;

    /* The original loop is not strength-reduced: i counts up and the slot address is rebuilt from it on every
     * pass. Its real source form was not found; writing the step as -~i (= i + 1) keeps the loop pass from
     * seeing an induction variable and gives the same code. */
    for (i = 0; i < 4; i = -~i) {
        line = work->voice[i];
        if (line != NULL && BtlScript_IsLineVoiceStopped(line)) {
            line->timer += 10;
            if (line->timer > 600) {
                BtlScript_ClearText(line->id);
                line->timer = 0;
                if (line->once) {
                    line->enabled = 0;
                }
                work->voice[i] = NULL;
            }
        }
    }
}

/* Tests every registered trigger: says lines, and picks the event action to start (work->pending). */
void BtlScript_UpdateRequests(void) {
    BtlScriptWork *work = &gBtlScript;
    BtlScriptTalk *talk = &work->talk;
    BtlScriptRequest *req;
    BtlScriptLine *line;
    BtlScriptStart *start;
    s32 i;
    s32 kind;

    for (i = 0, req = talk->requests; i < talk->requestCount; i++, req++) {
        kind = req->kind;
        switch (kind) {
        case BTL_SCRIPT_REQ_LINE:
            line = &req->u.line;
            if (line->enabled) {
                if (!BtlScript_IsLinePlaying(line) && BtlScript_IsTriggered(req->trigger, 0)) {
                    BtlScript_PlayLine(line);
                }
            }
            break;
        case BTL_SCRIPT_REQ_EVENT:
            start = &req->u.start;
            if (BtlScript_IsTriggered(req->trigger, 1)) {
                if (work->pending == NULL) {
                    work->pending = req;
                } else {
                    if (start->event->group == 1 || start->event->group == 2 || start->event->group == 3 ||
                        start->event->group == 4) {
                        work->pending = req;
                    }
                    switch (req->trigger & 0xFFF) {
                    case 0x48:
                        work->pending = req;
                        break;
                    case 0x4A:
                        work->pending = req;
                        break;
                    }
                }
            }
            break;
        }
    }
}

/* Starts the pending event action once the fight allows it: the running script task is killed and replaced.
 * What "allows" means depends on what the scan found in the action:
 *   uses command 0x4B1: at once for the triggers 0xFFFF, 0x22 and 0x49; otherwise both fighters must be
 *     interruptible and (0x20B840 true for a side, or the action's side bit set, or event 0x50 new on side 0);
 *   kind 1 or 2: the same second condition, with "the action's side can act" in place of "both interruptible";
 *   neither: at once when 0x20B840 is false for both sides, else when both fighters are interruptible. */
void BtlScript_StartPending(void) {
    s32 ok = 0;
    BattleEvents *ev = Battle_GetEventWork();
    BtlScriptWork *work = &gBtlScript;
    BtlScriptEvent *info;
    BtlScriptStart *cur;
    s32 ready;

    if (work->pending != NULL) {
        info = work->pending->u.start.event;
        if (info->cmd4B1) {
            if (work->pending->trigger == BTL_SCRIPT_TRIGGER_WAIT_OFF || (work->pending->trigger & 0xFFF) == 0x22 ||
                (work->pending->trigger & 0xFFF) == 0x49) {
                ok = 1;
            } else {
                ready = BtlFacade_AreBothInterruptible();
            check:
                if (ready) {
                    if (BtlCtrl_IsActiveDead(0)) {
                        ok = 1;
                    } else if (BtlCtrl_IsActiveDead(1)) {
                        ok = 1;
                    } else if (info->side) {
                        ok = 1;
                    } else if (BtlEvent_IsNew(0, 0x50)) {
                        ok = 1;
                    }
                }
            }
        } else if (info->kind) {
            ready = BtlFacade_CanCharAct(info->side);
            goto check;
        } else {
            if (BtlCtrl_IsActiveDead(0) || BtlCtrl_IsActiveDead(1)) {
                if (BtlFacade_AreBothInterruptible()) {
                    ok = 1;
                }
            } else {
                ok = 1;
            }
        }
        cur = &work->cur;
        if (ok) {
            *cur = work->pending->u.start;
            Gsc_KillTask(ev->unk4);
            BtlScript_ResetView();
            ev->unk4 = Gsc_StartAction(ev->script, cur->action);
        }
    }
}

/* While an event action is pending on side 0 and nobody is in the state 0x20B840 reports, shows the HUD cue
 * (0x22D990 / 0x22DA00); otherwise hides it (0x22D9B0). */
void BtlScript_UpdateHud(void) {
    BtlScriptWork *work = &gBtlScript;

    if (work->pending != NULL) {
        if (!BtlCtrl_IsActiveDead(0) && !BtlCtrl_IsActiveDead(1)) {
            if (!work->pending->u.start.event->side) {
                HudPrompt_ShowCue();
                HudPrompt_SetCueDim(BtlFacade_CanCharAct(0));
                return;
            }
        }
    }
    HudPrompt_HideCue();
}

/* Returns 1 when a line slot is in use. */
s32 BtlScript_IsSlotBusy(s32 side, s32 window) {
    return gBtlScript.voice[side * 2 + window] != NULL;
}

/* Frees a side's first line slot and stops its voice. */
void BtlScript_StopVoice(s32 side) {
    gBtlScript.voice[side * 2] = NULL;
    Voice_Stop(side);
}

/* Frees the four line slots and stops both voices and both streamed sound effects. */
void BtlScript_StopVoices(void) {
    BtlScriptWork *work = &gBtlScript;

    work->voice[0] = NULL;
    work->voice[1] = NULL;
    work->voice[2] = NULL;
    work->voice[3] = NULL;
    for (s32 i = 0; i < TagTeam_Count(); i++) {
        Voice_Stop(i);
        StreamSe_Stop(i);
    }
}

/* Returns 1 while a text is shown. */
s32 BtlScript_IsTextShown(void) {
    return gBtlScript.textId >= 0;
}
