# Boot, top-level loop and frame timing

Sources: `src/main.c`, `src/sys/gfx.c`, `src/sys/file.c` (vsync), `src/battle/battle.c`.

## Boot (verified)

`_start` (hand-written crt0, still assembly) sets the heap base to 0x3BE71C, which is exactly the
end of `DBZP.BIN` in memory, and calls `main`. `main` is `Game_Main(); return 0;` (the compiler
inserts the `__main` call). `Game_Main` never returns.

`Game_Main` order:

1. `Sys_RebootIop`: `sceSifInitRpc`, `sceCdInit`, `sceCdMmode(2)`, reboot the I/O processor with
   `IOPRP300.IMG` (retry loop), wait for it, repeat the three inits, `sceFsReset`.
2. `Sys_LoadIopModules`: 13 `sceSifLoadModule` retry loops, in this order: SIO2MAN, MCMAN,
   MCSERV, DBCMAN, SIO2D, DS2U_D, LIBSD, SDRDRV, MODHSYN, MODSESQ2, CDVDSTM, SOUNDS, CRI_ADXI.
3. Thread priority 2.
4. `Heap_Init`, `Gfx_Init`, `Snd_Init`, `File_Init`, movie/MPEG init (`func_001259F0`, inferred),
   `Common_Init`, `Common_LoadBoot`, `Common_Reload`, `Progress_Init`, `Save_Init`, `Job_Init`,
   maths init (`func_00121DA8`), `Dma_InitBuffers`, `Dbg_Init` (empty), `Pad_Init`, three
   unidentified inits (`func_00116BA8`, `func_00239FF0`, `func_0023D0E0`), `Fade_Init`,
   `PadWatch_Init` (the controller-removed watcher).
5. Forever: `Overlay_Load(0)`; overlay entry `func_336A90(0)` (menus); `Battle_Main(0)`.

Every disc operation in boot retries forever on failure. The overlay is reloaded from disc after
every battle.

`Overlay_Load(idx)` opens `gOverlayTbl[idx].path`, gets the size by seeking, reads to 0x334C00,
flushes caches. The table has one entry: `{"PROGRESS\n", "cdrom0:\BIN\DBZP.BIN;1"}`.

## Program structure (verified for the main executable, inferred for the overlay)

There is no generic task or scene system. Each screen runs its own loop built from one skeleton:

    Gfx_BeginFrame();  Pad_Update();  ...work...;  Gfx_EndFrame(2);  Dma_Flush();

Loops in the main executable: `Battle_Loop`, `Load_RunBlocking`, `Movie_Run`, `Load_RunFileQueue`
(no callers), and two in the trailing memory-card block (0x2BEA48, 0x2BF588). The overlay calls
the same frame functions 43 times each, so each menu screen appears to have its own loop.

The overlay entry dispatches on `gProgress->mode` (+0x18, values 1..70) through a jump table and
returns to `Game_Main` when a handler returns non-zero. What each mode is has not been mapped.

## Timing (verified)

- `Gfx_EndFrame(n)` waits for `n` vertical blanks; every loop passes 2. On NTSC that is 30 frames
  per second.
- `Vsync_Wait(n)`: waits while the vsync count is below n-1, then waits once more if the count is
  below n or if it has not waited at all, then zeroes the counter. So a frame that overruns does
  not accumulate debt.
- The battle sequence timers count `seconds * 30` frames, and the battle clock adds 34, 32, 34 ms
  on successive ticks (100 ms per 3 ticks). Game logic is therefore tied to a fixed 30 Hz step.

## Frame begin and end (verified)

`Gfx_BeginFrame`: `Dbg_BeginFrame` (empty), `func_00248F38` (rebuilds the battle object table,
inferred), `Fade_UpdateAll`.

`Gfx_EndFrame(vsyncs)`, in order: `Fade_DrawScreen`; `func_0023D160(vsyncs)` (frame counters);
`PadWatch_Update`; `PadWatch_Draw`; debug hooks (empty); `func_00121DE0`; `sceGsSyncPath`;
`Vsync_Wait(vsyncs)`; `frame++`; flip the field flag; `sceGsSwapDBuff`; `Gfx_SetDisplayRegs`;
`sceGsSyncPath`.

## Debug leftovers (verified empty; purpose inferred)

Ten empty `Dbg_*` functions are called from boot, `Gfx_BeginFrame`, `Gfx_EndFrame` and after a
screen capture. `Gfx_MarkPass(passId)` is called around every battle draw pass and is empty. The
heap has setters that write never-read globals and an empty "bad free" report. `Fade` and other
modules need an empty `do {} while (0)` to match, which suggests compiled-out debug macros.

## Per-frame job queue (verified)

`gJobQueue` is a `Queue` of capacity 4 holding `Job *`. `Job_Run` steps the head job and pops it
when its step returns 1; it returns 0 when the queue is empty and 1 when a job was stepped.
`Job_Push` runs the job once immediately if it is the only entry. Battle loading is done with
jobs (see battle.md).
