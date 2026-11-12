# Files and assets

Sources: `src/sys/file.c`, `common.c`, `bpe.c`, `loading.c`, `src/battle/battle_load.c`.
Layouts: `include/sys/file.h`, `include/sys/common.h`, `include/sys/loading.h`.

## Disc layout

| What | Size | Share |
|---|---:|---:|
| `DATA/PZS3US1.AFS`, `DATA/PZS3US2.AFS` (game data) | 2.68 GB | ~89% |
| `DATA/ZS3USOP.PSS`, `ZS3USED.PSS` (opening and ending movies) | 301 MB | ~10% |
| `DATA/ZS3USOP.ADX`, `ZS3USED.ADX` (their audio) | 12.6 MB | 0.4% |
| `DATA/PZS3US0.AFS` (one small file) | 14 KB | |
| Code: `SLUS_216.78`, `BIN/DBZP.BIN`, `IRX/` | 3.4 MB | 0.1% |

The archives have not been opened or catalogued yet.

## File ids (verified)

Every asset is one global integer id; names are never used for game data.

| id | archive | index in archive |
|---|---|---|
| <= 0 | `pzs3us0.afs` | id |
| 1 .. 0xD47 | `pzs3us1.afs` | id - 1 |
| >= 0xD48 | `pzs3us2.afs` | id - 0xD48 |

`File_OpenById` and `File_StartAdxById` (streamed audio) both apply this mapping.

### Known id formulas (verified by the matching loader code unless marked)

| Asset | id |
|---|---|
| Common files loaded at boot | 2, 3, 4, 5 |
| `6 + gProgress->unk0[0]` | unknown per-progress file |
| Fighter parameter file | `8 + chara*2 + side` |
| Common sound bank | 0x14B |
| Battle common sound bank | 0x14A |
| Stage sound bank | `0x14E + stage` |
| Stage model, single screen | `0x171 + stage` |
| Stage model, split screen | `0x198 + stage` |
| Stage transition files | 0x1BF (stage 3), 0x1C0 |
| Battle script | `0x1FF + n` |
| Loading screen | `0x3C1 + type` (type 0..2) |
| Character model | `0x590 + chara*10 + costume` (+4 for the variant) |
| Character animation files (inferred purpose) | `0x598 + n*10`, `0x599 + n*10` |
| Side voice bank | `0xBDA + chara`, or `0xC7B + chara` with the save's voice flag |
| Battle object | `0xC1C + id` for id >= 0x100 |
| Character voice line (streamed) | `chara*100 + line + 0x8D4E`, or `+ 0xCC32` with the voice flag |
| Music | `0x10B16 + n` (inferred, from `Bgm_Play` in the battle work reset) |

## File layer (verified)

- Startup: root dir `data/`, name cache `pzs3us.dir`, CRI threads, vsync handler, three partition
  info buffers, six ADX players. Partition 0 is loaded and waited on, then partition 1.
  Partition 2 is loaded later by the overlay (`File_LoadPartitionNw(2)` at 0x35DF80).
- Blocking load `File_LoadSyncEx(id, name, buf, unused)`: retries the open forever, retries the
  read until the full sector count is accepted, polls for completion, closes. A buffer it
  allocates is `Heap_Alloc(sectors << 11, 0x40, 0, 2)`.
- Async queue `gFileReq`: 32 entries, a free list and a pending list. `File_Request` opens the
  file only to get its size, then queues it. `File_UpdateRequests` does one step per call
  (idle, open, read, wait, next) and returns 1 when nothing is pending. `File_CancelRequests`
  stops the current read and frees queue-owned buffers.
- `File_Request` is declared with two parameters, but callers in `common.c` and `battle_load.c`
  pass a third (a buffer size, ignored). The prototype should gain a third parameter.

## Compression (verified)

`Bpe_Decode(src, dst, outSize)` expands byte-pair-encoded data: header `{rawSize, packSize}`,
then blocks with a pair table, a 16-bit big-endian length and a 256-byte stack. It allocates the
output when `dst` is NULL. Callers: a wrapper at 0x126608 and 0x24C5DC. That the routine is
Philip Gage's public "expand" is inferred.

`Res_RelocateOffsets` (545 call sites, not decompiled yet) turns file-relative offsets into
pointers after a load.

## Common data (verified)

`gCommonRes` (0x70 bytes): pointer to file 5, buffers for files 2, 3 and 4 with their sizes, and
a 0x50-byte battle resource block at +0x20 (stage buffer, sound-bank buffer, script and bank
files; see `BattleRes` in `include/battle/battle_work.h`). `Common_Reload` re-queues files 2, 3,
4 and sound bank 0x14B. The item table lives in common file 4: 350 entries of 0x28 bytes, flags
at +0x14.

## Loading screen (verified)

`Load_RunBlocking` loops until the job queue is empty and fade 0 is done. There are three screen
types; the same one never shows twice in a row. Each loads file `0x3C1 + type`, unpacks it and
builds 16 sprite records. The screen counts presses of pad bit 0x200 on either pad, steps a
two-frame animation and keeps a score capped at 100, 999 or 30 items depending on the type. What
each screen depicts and which button bit 0x200 is are not established.

## Disc check with no callers (verified code, inferred purpose)

`Disc_Identify` looks for `\SLUS_212.27;1`, `\SLUS_214.41;1` and `\SLUS_216.78;1` (Tenkaichi 1,
2 and 3) on a PS2 DVD and returns which it found. Neither it nor `Disc_GetDriveState` is called
from either binary, so the Disc Fusion feature must use other code.
