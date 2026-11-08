# Native IOP services

Replace the emulated IOP (original modules running in `runtime/iop/`, plus the SPU2, SIO2 and CDVD models) with native host services that answer the game's SIF RPC requests directly. Part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). Evidence: the protocol inventory below, taken from `runtime/iop/ee_bridge/`, `runtime/iop/services/`, the probe reports and the captures; [Tenkaichi3Decomp](../../reference/REFERENCE_ARCHITECTURE.md#tenkaichi3decomp-native-port-reference) took the same approach for BT3.

## The boundary

Every request the game makes is a standard PS2SDK SIF RPC, issued through `sceSifSetDma` (syscall 0x77, EE PC `0x32F028`). r4 points at the descriptor array; r5 is 1 (packet only) or 2 (send data, then packet).

- **Packet** (at `0x3A97C0`): header `{dsize<<8|0x40, dest, cid, 0}`, then `rec_id`, `pkt_addr`, `rpc_id`, `client`, `rpc_number`, `send_size`, `receive`, `recv_size`, `rmode`, `server`. `cid` is `0x80000009` for a bind and `0x8000000A` for a call. Send data, when present, is the first descriptor's `{EE source, IOP buffer, size}`.
- **Completion:** the reply is a 64-byte RPC_END packet at the EE SIF receive buffer `0x3A94C0`; `deliverRpcEnd` runs the game's own RPC_END handler (`0x3322E8`). It stores the server and buffer pointers in the client (client+0x24 / +0x14), clears client+0, and either signals the client's semaphore (blocking calls) or runs the client's end function (client+28 with client+32; no-wait calls).
- **IOP addresses are opaque.** The EE keeps the server, buffer and object pointers the IOP returns and only passes them back. Native services may hand out any stable non-zero values, except where the game or a comparison uses the exact value (the heap and CRI objects below).
- **Today** every request completes synchronously inside the syscall. Native services keep that.

## Design

1. **One SIF RPC layer** (`runtime/services/`): decode any 0x77 packet; route binds by server ID and calls by the bound client; give each service the send bytes and the receive buffer; write the RPC_END reply and run `deliverRpcEnd`. `deliverRpcEnd` loses its fixed client list and checks against the client structure itself.
2. **Native services**, one focused file each, registered by server ID. Each produces the EE-visible results listed below and nothing more.
3. **IOP memory shadow:** a plain byte store for the data the EE uploads with raw `sceSifSetDma` (sound blocks, bank blocks, CRI stream buffers), so the sound service can read it. No IOP execution.
4. **Transition switch:** `BT2_NATIVE_SERVICES=disc,file,...` enables a native service in place of the emulated one. Both paths stay only while the native one is being verified; the emulated path and its IOP code are removed once it is.
5. **Gates stay satisfied:** native services keep the counters other EE-side gates read (`disc_rpc_calls`, `ncmd_read_calls`, `cri_rpc_calls`, `heap_calls` and so on) until those gates are generalized.

## Services and EE-visible results

| Service | Server / client | Requests (send → receive) | EE-visible result |
| --- | --- | --- | --- |
| **Disc search** | `0x80000597` / `0x3A8780` | fn 0: 300 bytes from `0x3A8600` (path NUL-terminated at +36) → 4 bytes at `0x3A8740` | Result 1 (found); file record (LBA, size, name) written to the address at input +292 (`0x3A8600`) |
| **Disc N-commands** | `0x80000595` / `0x369590` | fn 14 → 4 bytes at `0x368440` (value 2); fn 1 (no-wait): 24 bytes `{LBN, count, EE dest, mode, completion record, progress word}` | Sectors copied to dest; progress word = count×2048; completion record `{0, 0, dest, dest+bytes}` at `0x3694C0`; end function `0x31ECF8` runs |
| **Disc S-commands** | `0x80000593` / `0x369F40` | fn 4 (error) → 4 bytes at `0x3695C0` | 0 |
| **FILEIO** | `0x80000001` / `0x3AC740` | init `0xFF` (result buffers `0x3ABCC0`/`0x3AC100`), open (`cdrom0:\BIN\DBZP.BIN;1`), lseek ×2, read, close | File bytes in the EE buffer (DBZP.BIN at `0x3B3100`); completion through IOP→EE command `0x80000011` into the C library handler `0x333378` |
| **IOP heap** | `0x80000003` / `0x3AC840` | fn 1 alloc (size), fn 2 free | First-fit, 256-byte granularity from `0xCE500`: reproduces all nine captured placements (`0xCE500`, `0x14FD00`, `0x150600`, `0x191700`, `0x192000`, `0x196100`, `0x196400`) |
| **LOADFILE** | `0x80000006` / `0x3ACC00` | fn 0 load by path, fn 6 load from IOP buffer, fn 255 version | `{module ID, result}`; the embedded `ck01` returns 1 (nonresident). No module runs. |
| **Controller** (DBCMAN) | `0x80001300`, `0x8000131C`/`1E`/`1F` | 144-byte init/attach/device calls; per-port no-wait calls | Captured replies; each field, a 128-byte pad block DMA'd to `0x3B0B00`+slot×`0x180` (+`0x80`) |
| **Memory card** (MCSERV) | `0x80000400` / `0x3A6F80` | `0xFE` init → `[0, 0x210, 0x226]`; fn 1 GetInfo (no-wait) | No card: reply −11, type 0, free 0; 192 bytes to the end parameter `0x3A7480`, then end function `0x31DD60` |
| **Sound driver** (SDRDRV) | `0x80000701` / `0x3A5C80` | 64-byte commands (`0xE620`, `0x80D0`, one more) → 16 bytes | Zero replies; bank transfers read from the IOP memory shadow |
| **SOUNDS** | `0x02000004` / `0x6351C0` | 256-byte commands (`0x10`, `0x20`, four more) → 64 bytes at `0x635180` | `0x20` reply `[0,0,0,0,1,1,0,0, thread ID, 0,0,0, 2, IOP pointer, 0,0]`; the others still to be captured |
| **CRI ADX** | `0x90000200` / `0x355C70` | `2`, `0x403`, `0x40C`, 16× `0x422`/`0x400`, `2`, `0x40A`, `0x408` | Exact object addresses the EE passes back (`0x1B58C0`, `0x1B5904`, base+`0x14A40`+i×`0x30`, base+`0x15580`+i×`0x14`, base+`0xEDA0`+i×`0x1C`; `0x40C` → `[0,1,0]`); each VBlank, both 0x880-byte stream buffers are copied back to EE `0x365BC0`/`0x35FD00` with the advanced acknowledgement trailer |

Unknown and to be captured before the sound replacement: the SDRDRV follow-up command, the four later SOUNDS commands and their replies, the meaning of the CRI arguments, and whether ADX audio reaches the SPU2 through the core-0 AutoDMA stream. The 240 Hz SOUNDS tick and the IOP→EE sound server `0x80000704` have no observed EE-visible effect.

Before the IOP reset, the CD init, S-command mode and LOADFILE binds are replayed from capture bundles; they become native replies with the same values.

## Replacement order

1. **SIF RPC layer + disc** (search, N-commands, S-commands): the ISO readers in `runtime/iop/devices/disc_metadata.h` and `disc_sectors.h` already do the work.
2. **FILEIO, heap, LOADFILE.**
3. **Controller** (with a host-input hook; the pad script stays for tests) and **memory card** (absent card; host card files later, per the [memory card plan](../memory-card/MEMORY_CARD_PLAN.md)).
4. **Sound and CRI** after the unknown requests are captured: replies first (game progress), native audio output (ADX decoding, bank playback) second.
5. **Remove** `runtime/iop/` module execution, the SPU2/SIO2/CDVD models and the bridge files as their last user goes.

## Verification

- **A/B runs:** for each replaced service, the native and emulated paths must reach the same stop with the same EE RAM, except documented IOP-side values. Compare `entry-stop.json.ee.bin` byte for byte, and the report's EE counters.
- **Unit tests** per request type, including malformed and unsupported requests (which stop at a checked boundary).
- **Original comparisons:** game-visible comparisons stay; comparisons of IOP internals (module residency, IOP thread state, SPU2 registers) are retired with the code they checked. Record each retirement and its reason in the monthly history.
- **Performance:** record the Release run time before and after each removal.

## Stop conditions

Stop and capture when a request's reply cannot be derived from existing evidence (for example the later SOUNDS commands). Never return a fabricated success value.

## Progress (2026-10-11)

All services in the table run natively by default: disc, FILEIO, memory card, controller, IOP heap and uploads, LOADFILE, sound driver, SOUNDS and CRI ADX. The emulated IOP (IOP runtime, providers, module translation, bridge and their tests and tools) is removed; `runtime/services/` holds the replacement, one file per service.

- **Evidence:** each service's requests and replies were traced from the emulated path before removal. Each replacement was then checked by an A/B run against it: same stop (1254 / 1243 functions, 650 frames), and identical EE memory except two regions of stale IOP bytes the native services leave zero (the card info block `0x3A7488`–`0x3A7536` and the FILEIO completion payload `0x3AC124`–`0x3AC1A0`).
- **Notable findings:** the IOP heap is first-fit with 256-byte granularity from `0xCE500`; DBCMAN reports the pad after 27 fields of detection and writes one 128-byte block per VBlank; CRI returns each uploaded stream buffer at the next VBlank with its sequence counter advanced.
- **Boundaries:** CRI stream commands (ADX playback) stop at the VBlank that would need their answer; unknown requests stop at a checked boundary.
- **Cost:** the startup run no longer executes about 10.9 million IOP instructions; Release run time went from about 19.9 s to 19.1 s, because the CPU GS rasterizer dominates.
- **Still captured, not native:** the CD initialization, S-command media mode and LOADFILE bind/version replies around the IOP reset.
- **Next:** native audio output (ADX decoding, sound bank playback) for the sound and CRI services, and host input for the controller.
