# Script engine and battle scripts

Sources: `src/sys/gsc.c` (generic engine), `src/battle/btl_script.c` (battle triggers, text,
scripted camera), `src/battle/btl_facade.c` (what the command handlers call).
Layouts: `include/sys/gsc.h`, `include/battle/btl_script.h`.

All names are guesses (no strings survive except three type names). The 47 command handlers at
0x259EC8..0x25C200 are NOT decompiled.

## GSC engine (verified)

A generic cooperative script machine with no battle knowledge; it is given a command table.

- `Gsc_Init(commands, fileMax, taskMax)` allocates a pool of files (0x24 bytes) and tasks
  (0x50 bytes). The battle uses 10 files and 50 tasks.
- `Gsc_LoadFile(ptr)` registers a file in place (no copy, no relocation) and returns a handle.
- `Gsc_StartAction(handle, actionId)` starts a task on an action; `Gsc_StartMain` uses action
  -1; `Gsc_RunAction` runs one to completion synchronously.
- `Gsc_Update` steps every running task once. A task executes words until one has to wait.

| Word kind | Behaviour |
|---|---|
| 0 | end of action: the task is freed |
| 1 | command: look the id up and call `handler(phase, task->work)` |
| 2 | start another action as a new task and continue |
| 3 | the same, but wait until that child task ends |

Command phases: 0 once (4 instead if the task was told to abort); then 2 every frame until it
returns non-zero; then 1. A waiting command adds 10 to the task's time each frame. An unknown
command id calls a NULL handler: there is no check.

Handlers read operands with `Gsc_GetInt` / `GetFloat` / `GetString` (and `...Or(default)`),
and find lettered options with `Gsc_FindOption(letter)`.

## Script file format (verified by the matching code and against file 0x1FF)

Little-endian chunks, each a 0x10-byte header `{char tag[4]; u32 headSize; u32 dataSize;
s32 arg}`, the data, then a 0x10-byte `EOFC` trailer.

- `GSCF` wraps the file and contains `GSHD` (not read), `GSCD` (a run of `GSAC` action chunks)
  and `GSDT` (a pool of 4-byte values).
- `GSAC`: `arg` is the action id; the data is 32-bit words:

| Bits | Meaning |
|---|---|
| `0x00` | end |
| `id<<16 \| n<<8 \| 0x01` | command with n plain operands |
| `action<<16 \| 0x02` / `0x03` | call / call-and-wait |
| `n<<16 \| letter<<8 \| 0x08` | option with n operands |
| `index<<8 \| type<<4 \| 0x0A` | operand: `index` selects a GSDT word; type 0 int, 1 float, 2 string |

The game never reads the operand type; the handler decides how to read. On-screen text is not
in the script file: lines are numbers indexing a separate text file.

Action id conventions: -1 main, 1000 setup, 10000..10051 event actions.

## Battle use (verified)

- Battle script files are ids `0x1FF + n`. The loader registers the file, runs action 1000
  synchronously, then `BtlScript_ScanEvents` records one byte of info per event action.
- In mode 1 (story battles) the main action is started at reset. Each unpaused frame
  `Battle_Loop` calls `Gsc_Update` then `BtlScript_Update`; the trigger side runs only in the
  Fight and WinTalk sequence states.
- Triggers: up to 50 requests filled by command 7 (no capacity check). A trigger is
  `side << 15 | battle event id` tested as a rising edge, or 0xFFFF for "wait flag off".
  - Line requests play a voice line and show its text for 60 frames after the voice stops.
    Voice file = `(voice flag ? 0x47E0 : 0xD48) + story * 300 + line`.
  - Event requests become pending and start when the fight allows it; starting one kills the
    running script task without an abort phase. A story battle runs one script task at a time.
- `BtlScript_UpdateView` interpolates a scripted camera between up to 9 keys, keeps a camera
  shake going and draws the current text line.
- Script time: command 1 ("wait N") is done when the task time reaches `N * 300`; time advances
  10 per stepped frame, so script time 1.0 is 30 frames (one second).

## Command table (`gBtlScriptCommands`, 47 entries; from the table and handler disassembly)

| Ids | Group |
|---|---|
| 1..16 | core: 1 wait, 2 story number, 7 triggers, 8 the 30-way battle command, 15 speakers |
| 701, 702 | file requests |
| 801..810 | fighter |
| 901, 902; 1001..1003 | unknown |
| 1201..1205 | camera |
| 1301, 1302 | text window setup |
| 1501, 1502; 1601..1603 | unknown |
| 1701 | wait for a pad button |

## Per-story files (read from handler disassembly)

Text file `story + 0x231` (or `+ 0x263`), a second set `story + 0x295` (or `+ 0x2C7` with the
voice flag), and 300 voice streams per story per voice language.

## What a port needs

The GSC loader and stepper, the 47 handlers with the phase protocol, the trigger layer driven
by the per-side battle event sets, the facade functions the handlers call, and the per-story
text and voice files. Stepped only on unpaused frames, at the game's 30 frames per second.

## Not determined

`GSHD` contents; parts of the 0xBE0-byte `gBtlScript` work block; what most battle event ids
mean; eight unknown bytes in each camera key.

## Command handlers (verified; `src/battle/btl_script_cmd.c`, 0x259EC8..0x25C2A8)

46 handlers (the table has 47 entries with its terminator). The full reference, with operands
and lettered options, is in `include/battle/btl_script_cmd.h`. Summary:

| Ids | Commands |
|---|---|
| 1 | wait N seconds |
| 2 | story number |
| 4 / 5 / 6 | event end / scene begin / scene end |
| 7 | register triggers (`-a` event actions, `-v` voice lines) |
| 8 | battle sub-command (30 values: start or resume the fight, stage change, force actions, change member, use a technique, power up) |
| 9 | set the result and wait |
| 10 / 11 / 12 | battle setup: rule, side, member (a story battle is configured by its own script) |
| 13 / 14 | player / enemy status: health, gauges, items, CPU level |
| 15 | speakers |
| 701 / 702 | request the text file / the lip-sync file |
| 801..810 | place a fighter, fighter control flags |
| 901 / 902 | play / stop a scripted animation on a fighter |
| 1001..1003 | fade in / out / reset |
| 1201..1205 | camera: set, move through up to 8 keys, off, shake, shake off |
| 1302 | text window setup (per language) |
| 1501 / 1502 | music |
| 1601 / 1602 / 1603 | sound effect / voice / talk (voice, subtitle, lip sync) |
| 1701 | wait for a pad button |

Conventions: a `who` operand has the side in bit 15; script angles are degrees; script seconds
are 30 frames. Handlers take an unsigned phase.

Waits on non-simulation state: 1602 and 1603 `-w` and line triggers wait on the voice stream;
1603 starts lip movement when the stream reports playing; 1502 `-w` waits on the music stream;
1701 reads pad 0 directly. Everything else waits on script time, fighter flags or frame-counted
fades.

Original bug: the `-B` option of commands 13 / 14 (add blast gauge) calls the ki adder, so the
blast adder has no caller.

From four dumped scripts (inferred): win scenes are event actions 1003x and lose scenes 1004x,
triggered by the KO event on side 0 / side 1.
