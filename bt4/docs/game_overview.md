# BT3 (SLUS-21678) game code overview

Top-level structure of the developer's own code in `SLUS_216.78` and how it uses the `DBZP.BIN` overlay.
Names used here are the ones in `config/symbols/game.txt`.

Every claim is tagged:

- **[V]** verified: the function was read and clearly does this.
- **[I]** inferred: consistent with the code, but not proven (unread callee, role guessed from usage, or SDK call identified only by argument shape).

SDK/CRI function identities mentioned in passing (sceSifLoadModule, scePad2Read, ADXF_ReadNw, ...) are all **[I]**; naming library code is out of scope here.

## 1. Boot path

| Step | Address | What it does |
|---|---|---|
| `_start` | 0x100008 | [V] Clears registers and bss, syscall 0x3C (SetupThread, gp = 0x304270), syscall 0x3D (SetupHeap) with heap base **0x3BE71C**, then `jal 0x100258` and passes its result to 0x2A8EB8 (exit). |
| `main` | 0x100258 | [V] Calls `__main` (0x2A6AB0, run-once ctor walker) then `Game_Main`. Returns 0, but `Game_Main` never returns. |
| `Game_Main` | 0x100558 | [V] Init sequence below, then the outer loop. |

0x3BE71C = 0x334C00 + 563996, exactly the end of `DBZP.BIN` in memory [V], so the linker reserved the overlay's space ahead of the heap.

`Game_Main` init order [V for the order and for each named function; unnamed entries are I]:

1. `Sys_RebootIop` 0x100330: reboot IOP with `cdrom0:\IRX\IOPRP300.IMG;1`, wait for sync.
2. `Sys_LoadIopModules` 0x1003A0: loads, each in a retry loop, SIO2MAN, MCMAN, MCSERV, DBCMAN, SIO2D, DS2U_D, LIBSD, SDRDRV, MODHSYN, MODSESQ2, CDVDSTM, SOUNDS, CRI_ADXI. (`DS2O_D`, `MODMIDI`, `MODSEIN`, `MODSESQ` are on disc but not referenced here.)
3. syscalls 0x2F then 0x29 with priority 2 [I: GetThreadId / ChangeThreadPriority].
4. `Heap_Init` 0x2554B8 -> `Heap_Create`.
5. `Gfx_Init` 0x101F18 (640x448 double buffer in `gGfx`).
6. `Snd_Init` 0x124938 (sound manager + RPC bind to SOUNDS.IRX).
7. `File_Init` 0x264BD0 (CRI ADXF/ADXT, AFS partitions 0 and 1, six ADX players, vsync handler).
8. 0x1259F0 -> 0x297B60 [I: MPEG/IPU init; it pokes the IPU registers at 0x1000B000].
9. `Common_Init` 0x263240, `Common_LoadBoot` 0x2630E8, `Common_Reload` 0x263198: load the always-resident files (ids 5, 2, 3, 4) and sound bank 0x14B.
10. `Progress_Init` 0x25DE68 (allocates `gProgress`, mode = 1, flag 0x40 set).
11. `Save_Init` 0x266608 (0x4000-byte settings block `gSaveData`) [I for the role].
12. `Job_Init` 0x263490.
13. 0x121DA8: maths init; seeds the VU0 random register, loads identity rows into vf1-vf3, calls `Rand_Init`.
14. `Dma_InitBuffers` 0x100670 (two 1 MB display-list buffers).
15. 0x263098: empty stub.
16. `Pad_Init` 0x122940.
17. 0x116BA8, 0x239FF0(1), 0x23D0E0: not analysed.
18. `Fade_Init` 0x252BE8.
19. 0x268188: allocates a 0x28 struct at gp 0x2FF158 [I: pad-disconnect / pause watcher, polled from `Gfx_EndFrame`].

Outer loop [V]:

```c
for (;;) {
    Overlay_Load(0);        // 0x100280: read gOverlayTbl[0].path into 0x334C00, FlushCache
    func_336A90(0);         // DBZP.BIN entry: menu / mode dispatcher
    Battle_Main(0);         // 0x12BD10
}
```

`gOverlayTbl[0]` is `{"PROGRESS\n", "cdrom0:\BIN\DBZP.BIN;1"}` [V], so the developers called the overlay **PROGRESS**. It is reloaded from disc after every battle.

## 2. Main loops and state machines

There is no generic task/process list. There are three layers, each a plain state machine, and many separate frame loops (six in the main ELF, about 43 more in the overlay) that all share the same skeleton.

### Frame skeleton [V]

Every frame loop calls, in this order: `Gfx_BeginFrame` (0x102038) ... `Pad_Update` (0x122A38) ... work ... `Gfx_EndFrame(2)` (0x102060) ... `Dma_Flush` (0x100798).

The six loops in the main ELF: `Battle_Loop` 0x12BBD0, `Load_RunBlocking` 0x2635C8, `Movie_Run` 0x125D50 (three exits), 0x2649A8, 0x2BEA48, 0x2BF588. `DBZP.BIN` additionally calls `Gfx_BeginFrame`, `Gfx_EndFrame` and `Dma_Flush` 43 times each and `Pad_Update` 44 times [V counts], so each menu screen in the overlay carries its own copy of this loop [I].

- **Vsync**: `Vsync_Handler` 0x264A58 increments `gVsyncCount` (gp 0x2FF128). `Gfx_EndFrame(n)` calls `Vsync_Wait(n)` 0x264D98, which blocks until the counter reaches `n` and then zeroes it. All loops pass `n = 2` [V]. Whether that means 30 fps or the handler fires once per field is open.
- **Pads**: `Pad_Update` reads both pads into `gPad` (0x333800, 2 x 0x1C0).
- **Display lists**: draw code appends to `gDmaCur`; `Dma_Flush` closes the chain, sends it on DMA channel 1 (VIF1) and swaps to the other 1 MB buffer.

### Layer 1: progress modes (overlay) [V for the dispatcher, I for what each mode is]

`func_336A90` in `DBZP.BIN` loops on `gProgress->mode` (`*(0x2FF10C) + 0x18`, values 1..70) through a 70-entry jump table at 0x3B1130. Each handler runs its own screen and returns; when a handler returns non-zero the function frees its buffers and returns to `Game_Main`, which starts the battle. Groups seen in the table: mode 1/2/4 boot and opening (mode 2 calls `Movie_PlayOpening` 0x125CF0, "zs3usop.pss"), 6-10 -> 0x33CBF8, 13-30 -> 0x379A58, 33-35 -> 0x362160, 38-41 -> 0x352CB8, 44-45 -> 0x3591C8, 48-50 -> 0x39E940, 53-56 -> 0x3A9850, 60 -> 0x3590A8, 62 -> 0x39FAA8, 70 -> 0x2BD230 (in the main ELF).

`gProgress` (0x7FC bytes, allocated by `Progress_Init`): +0x04 base file id (0x1C1), +0x08/+0x0C/+0x10 work buffers (0x3000, 0x6800, 0x380), +0x14 flags, +0x18 mode, +0x24 = -1 [V].

### Layer 2: battle scene [V]

```c
Battle_Main:  work->0x19F8 = 1; Battle_Init(); Battle_Loop(); Battle_Term(); work->0x19F8 = 0;

Battle_Loop:
  do {
      if (work->flags19F0 & 0x8000) { Battle_Restart(); clear flag; }
      Job_Run(); Gfx_BeginFrame();
      if (!(work->flags19F0 & 0x100)) { 0x257A50(); 0x259030(); }
      0x1C2AA8(); Pad_Update(); Snd_Update(); 0x125330();
      0x212990();                 // -> BtlSeq_PreUpdate
      0x126FB0(); 0x1BB620(); 0x1C2A28();
      upd  = Battle_Update();     // 0x12B6E0
      done = BtlSeq_Update();     // via 0x2129B0
      if (Battle_IsSplitScreen() && upd) Battle_DrawSplit(); else Battle_Draw();
      Gfx_EndFrame(2); Dma_Flush();
  } while (!done);
```

`gBattleWork` is a static 0x1A00-byte block at 0x331DC8 returned by `Battle_GetWork` (205 call sites). Known fields: +0x08 battle mode, +0x24 (== 1 selects the two-view draw), +0x1938 and +0x1980 sub-structs, +0x19F0 64-bit flags (0x100 skip, 0x8000 restart request), +0x19F8 "battle running".

`Battle_Draw` / `Battle_DrawSplit` are fixed sequences of render passes, each bracketed by the empty stubs `Dbg_ProfMark` / `Dbg_ProfColor` (a stripped profiler taking RGBA colours) and 0x102448 (also empty).

### Layer 3: battle sequence [V]

`gBtlSeq` (gp 0x2FEB38) points at a 0x12C-byte struct: +0x00 current state, +0x04 current state's argument, +0x104 pointer to a state table, +0x108 and +0x118 small sub-structs.

A state table has 7 entries of 0x18 bytes: `{enter, preUpdate, update, exit, extra, arg}`. `update` returns the next state; on change `BtlSeq_Update` calls `exit` of the old and `enter` of the new one. State 99 ends the battle (or sets the restart flag 0x8000). An empty slot falls through to state 6.

Three tables, chosen by `Battle_GetMode()` in `BtlSeq_Reset`: 0x2C6070 (default), 0x2C6118 (mode 1), 0x2C61C0 (modes 5-7). The start state is the first non-empty entry.

## 3. File loading

Files are identified by **one global integer id**; there are no file names in game code [V].

`File_OpenById` 0x2654D8 maps the id onto three AFS partitions:

| id | partition | index |
|---|---|---|
| <= 0 | 0 (`pzs3us0.afs`) | id |
| 1 .. 0xD47 | 1 (`pzs3us1.afs`) | id - 1 |
| >= 0xD48 | 2 (`pzs3us2.afs`) | id - 0xD48 |

Partition descriptors are in `gAfsPartitionTbl` 0x2C7038 (`{name, ptinfo size, ptinfo buffer}` x 3). `File_InitAdx` sets the root directory to `data/`, registers `pzs3us.dir`, loads partition 0; `File_LoadPartition1` loads partition 1. `DBZP.BIN` calls `File_LoadPartitionNw` once, from 0x35DF70, which the dispatcher runs when `gProgress` flag 0x40 is set (it is set by `Progress_Init`); this is presumably the partition 2 load [V call site, I purpose].

Game-side API:

- `File_GetSize(id)` 0x265418 [V]: size in bytes (sectors << 11).
- `File_LoadSync(id, buf, x)` 0x2654A0 -> `File_LoadSyncEx(id, name, buf)` 0x264FF8 [V]: blocking whole-file read; allocates with `Heap_Alloc(size, 0x40, 0, 2)` when `buf` is NULL; opens by name when `id < 0`. 45 call sites (40 from the overlay).
- `File_Request(id, buf)` 0x2651C0 / `File_UpdateRequests()` 0x265298 / `File_CancelRequests()` 0x265108 [V]: asynchronous queue of up to 32 reads in `gFileReq` (0x31E760). The update function is a 5-state machine (pick next, open, start read, wait for status 3, close) and returns 1 when nothing is pending.
- `Job_Push(job)` / `Job_Run()` [V]: a 4-entry queue of objects whose callback at +4 is polled once per frame; the battle loader uses it to chain `File_Request` batches. `Load_RunBlocking` 0x2635C8 spins a frame loop until the queue drains [I: this is the loading screen].

Streams: six ADXT players in `gAdxPlayerTbl` 0x2C7060, labelled `BGM`, `MAP`, `SE `, `SE `, `VIC`, `VIC` [V]. `Adx_Play(player, fileId, vol, x)`, `Adx_Stop`, `Adx_GetStat`, `Adx_StopAll`; `Bgm_*` wrap player 0, `Voice_*` wrap players 4-5.

## 4. Memory allocation

**Main heap** [V]. One contiguous region from `_end` (0x3BE71C) to 0x1EFB000, set up by `Heap_Create`. `gHeapStart[2]` / `gHeapEnd[2]` (gp 0x2FF080 / 0x2FF088) allow two heaps but only index 0 is populated.

Block layout: 0x20-byte header `{magic 'TBHS' (0x53484254), used, fromTail, align, blockSize, userPtr, payloadSize}`, and the block size repeated in the block's last word so blocks can be walked backwards.

- `Heap_Alloc(size, align, fromTail, heap)` 0x2554D8: first fit from the head, or from the tail when `fromTail != 0`; `heap == 2` means any heap. Returns the user pointer. Nearly every caller passes `(size, 0x20, 0, 2)`.
- `Heap_Free(ptr)` 0x255508: finds the block by **linear walk** comparing `userPtr`, then merges with free neighbours.

**The 0x1A7240 / 0x1A71B8 pair** [V]: these are not the heap itself. They belong to a battle-side pool manager (`gBtlPool`, gp 0x2FEAE0, 0x9C bytes): nine slots plus a "current slot" at +0x94 and an arena bitmask at +0x98.

- `BtlPool_GetCurrent()` 0x1A7240 returns the current slot id.
- `BtlPool_Free(slot, ptr)` 0x1A71B8 calls `Heap_Free(ptr)` only if the slot is not a bump arena.
- `BtlPool_Alloc(slot, size)` 0x1A7110 bump-allocates from the slot's arena, or falls back to `Heap_Alloc(size, 0x20, 0, 2)`.

So `f1A71B8(f1A7240(), ptr)` is "free `ptr` in the current pool"; it is a no-op for arena slots, which are released wholesale by `BtlPool_Reset(slot)`.

## 5. Most-called game functions

Call-site counts include the 7,213 calls from `DBZP.BIN`. Library functions are listed only so they are not mistaken for game code.

| Address | Calls | Name | What it does |
|---|---|---|---|
| 0x10D8B0 | 983 | `Flash_FindLabel` | [V body, I name] looks up a clip by name, then a label in it by name, writes an 8-byte handle |
| 0x2A9ACC | 867 | (libc memset) | [V] |
| 0x121FA8 | 842 | `Vec4_Copy` | [V] 16-byte copy |
| 0x1DABE8 | 837 | `BtlChar_SetFlag` | [V body] sets bit `n` in a per-character flag array, stamps a frame counter |
| 0x1DAC78 | 823 | `BtlChar_TestFlag` | [V body] tests bit `n` in two OR-ed flag arrays |
| 0x10A028 | 545 | `Res_RelocateOffsets` | [V body] converts word offsets in a resource header to pointers |
| 0x121E28 | 512 | `Vec4_Set` | [V] stores four floats |
| 0x2A9C78 | 504 | (libc rand) | [V] 64-bit LCG |
| 0x124F68 | 472 | `Snd_PlaySe` | [V] `Snd_PlaySeEx(bank, id, 0x40, 0, 0)` |
| 0x10D878 | 456 | - | [V] UI object: forwards to 0x10BEB8 with `obj->0x20` (label lookup + play flags) |
| 0x2AA120 | 423 | (libc sprintf) | [V] |
| 0x10DCA0 | 378 | - | [V] UI object: if handle valid, 0x10F8E0 copies 4 words (colour/params) into matching clips |
| 0x1DC298 | 367 | - | [V] returns `a0 + 0x10` |
| 0x1E0290 | 345 | - | [V] `if (a1 != -1) a0->0x94C = a1` (character field setter) |
| 0x10D9D8 | 340 | - | [V] UI object: applies a bitmask of play/visibility operations to a handle |
| 0x121E50 | 310 | `Vec3_Normalize` | [I] raw VU0 opcodes |
| 0x255508 | 309 | `Heap_Free` | [V] |
| 0x1DC280 | 304 | - | [V] `BtlObj_Get(a0->0xC)` |
| 0x224B90 | 270 | - | [V] sets bit 0 of `*a0` to `(a1 == 0)` (HUD element hide flag) |
| 0x254DE8 | 263 | `Rand_Range` | [V] Mersenne Twister modulo n |
| 0x121ED8 | 262 | `Vec4_Sub` | [V] |
| 0x2554D8 | 257 | `Heap_Alloc` | [V] |
| 0x2058E0 | 252 | - | [V] copies a position vector (object part `a1`, else `obj+0x970`, else zero) into `a2` |
| 0x121F38 | 251 | `Vec4_Scale` | [V] |
| 0x1DA9D0 | 247 | - | [V] like `BtlChar_SetFlag` on a second flag array |
| 0x121EA8 | 238 | `Vec4_Add` | [V] |
| 0x1DC178 | 236 | `BtlChar_Get` | [V body] `gBtlChars->array + i * 0x1600` |
| 0x1C4638 | 219 | - | [V] returns `a0->0x974` |
| 0x126EC8 | 205 | `Battle_GetWork` | [V] returns `&gBattleWork` |
| 0x1CF578 | 205 | - | [V] sets bit `n` in the byte array at `a0 + 0x1262` |
| 0x1A7240 | 196 | `BtlPool_GetCurrent` | [V] |
| 0x2614B0 | 188 | `Voice_PlayWithSubtitle` | [V body, I name] |
| 0x122088 | 182 | `Vec3_Dot` | [V] |
| 0x121F50 | 181 | `Vec3_Scale` | [V] |

## 6. Leftover names and debug strings

Debug output was stripped; no file names, function names or assert macros survive in game code. What remains:

- `"PROGRESS\n"` (0x2EB398): label of the overlay in `gOverlayTbl` [V].
- `"tpChar->data.pparam_com"` (0x2EE230), referenced from 0x1BAF68 [V]: a stringified assert expression, so the character struct was accessed as `tpChar->data.pparam_com` (Hungarian `tp` prefix).
- `"No Debug\n"` (0x2F2CA0, 0x2F2EB0): returned by the two-instruction stubs 0x253EB8 and 0x254A10 [V].
- Heap magic `'TBHS'` [V].
- ADX player labels `BGM`, `MAP`, `SE `, `VIC` (gp 0x2FF130-0x2FF148) [V].
- `FREE`, `USED`, `idle`, `nodata`, `full`, `int   `, `float `, `string`, `true`, `false`, `trig`, `trigger` in `.sdata`: debug-menu / status vocabulary; `idle` is used by `Movie_Run` [V], the rest were not traced.
- UI resource names `mc_*` (movie clip) and `fl_*` (frame label), e.g. `mc_menu_plate_%d`, `fl_window_s_in` [V that they are passed to `Flash_FindLabel`; I that the format is Flash-derived].
- Save data: `BASLUS-21678`, `BASLUS-21678DBZT3`, `DBZT3`, `DBZT3R`, `dbzsm.ico`, `dbzsmr.ico`, used around 0x118984-0x118E40 [V location, code not read].
- `\SLUS_214.41;1`, `\SLUS_212.27;1` (0x2F34E0): the BT2 and BT1 executables, for the disc-swap feature. No direct code reference found; they are reached through a table [I].
- Streams: `zs3usop.pss/.adx`, `zs3used.pss/.adx` (opening / ending), confirming the `zs3` project prefix [V].

## Subsystems and where they live

| Subsystem | Address range (approx.) | Key globals |
|---|---|---|
| Boot, DMA buffers, GS setup | 0x100258-0x102800 | `gGfx`, `gDmaBuf` |
| UI animation player (`Flash_*`), resource relocation | 0x10A000-0x10FFFF | - |
| Memory card | 0x118000-0x119000 [I] | - |
| Maths (VU0 vectors/matrices, VU random) | 0x11F400-0x1222FF | - |
| Pad | 0x122940-0x1230FF, 0x2574F0-0x257830 | `gPad` |
| Sound effects | 0x123F00-0x125330 | `gSndMgr`, `gSndRpc` |
| Movie playback | 0x1259F0-0x1266FF | - |
| Battle scene core, loaders | 0x126EC8-0x12D4FF | `gBattleWork` |
| Fighters, actions, effects | 0x12D500-0x211FFF [I] | `gBtlChars`, `gBtlPool` |
| Battle sequence, HUD | 0x212000-0x230000 | `gBtlSeq` |
| Camera, stage, objects, fades | 0x239000-0x254000 [I] | `gBtlObjTbl`, `gFade` |
| Random, heap, lists/queues | 0x254A00-0x2562FF | `gHeapStart`, `gRandState` |
| Progress / menu support used by the overlay | 0x257000-0x263000 | `gProgress`, `gCommonRes` |
| Jobs, loading loop, file layer, ADX players, options | 0x263000-0x268FFF | `gJobQueue`, `gFileReq`, `gSaveData` |
| Late game code built without gp-relative addressing | 0x2BD000-0x2BF6B0 | uses overlay data (e.g. 0x3B0EB4) |

## Open questions

1. Frame rate: `Vsync_Wait(2)` waits for two handler ticks; confirm what `Vsync_InstallHandler`'s callee (0x294220) hooks.
2. The functions at 0x2BD230-0x2BF6B0 sit after the SDK/newlib code, do not use `$gp`, and read overlay data directly. They look like progress-mode code linked into the main ELF (mode 70 dispatches to 0x2BD230); two of the six frame loops are there. The library-boundary map needs to treat this tail as game code.
3. Confirm that overlay function 0x35DF70 loads partition 2 (`pzs3us2.afs`), and identify the three un-named main-ELF frame loops (0x2649A8, 0x2BEA48, 0x2BF588).
4. `Battle_GetMode` values 1..7 and the meaning of `work->0x24` (named `Battle_IsSplitScreen` from the two-view draw only).
5. The UI object family at 0x10D4F0-0x10DD38 (`Flash_*`): struct layout and the remaining methods are unnamed.
6. Init callees 0x116BA8, 0x239FF0, 0x23D0E0 and the struct at gp 0x2FF158.
7. `Heap_Create` calls 0x2A8F68 with the heap size; whether that is `malloc` or a direct `sbrk`-style grab was not checked.

## Source-level clarifications

These supersede the static-analysis notes above where they differ; each is confirmed by C that
compiles to the original bytes.

- **Random numbers** (`src/sys/rand.c`): MT19937 seeding and tempering, but the state refill only
  runs the first loop and the final wrap-around word, so 396 of 624 state words are never
  regenerated. The sequence is not a true MT19937 sequence.
- **Battle pool** (`src/battle/btl_pool.c`): nine bump arenas carved from one block; the arena
  mask is always 0x3FE, so the heap fall-through in `BtlPool_Alloc`/`BtlPool_Free` is never taken.
  No capacity checks. The third slot field is the arena size.
- **File layer** (`src/sys/file.c`): `File_LoadSyncEx` takes four arguments `(id, name, buf, unused)`.
  The overlay's `File_LoadPartitionNw(2)` call at 0x35DF80 is the `pzs3us2.afs` load (was open
  question 3). The async queue has 32 entries and no full-queue check.
- **Lists** (`src/sys/list.c`): there are two list types; the singly linked `SList_*` is the one
  the game actually uses.
