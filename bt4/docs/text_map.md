# .text map of SLUS_216.78: game code vs library code

`.text` is 0x00100000-0x002BF6B0 (0x1BF6B0 = 1,832,624 bytes). Derived statically from the splat disassembly; nothing here was confirmed by building or matching. Names backing the evidence are in `config/symbols/lib.txt` and `config/symbol_addrs.txt`.

Function counts are splat's function labels (8,760 including a handful of data-in-text / handwritten labels such as `func_0011F6E0` and `D_0029F160`; the usual figure quoted is 8,753).

## Summary

| Class | Functions | Bytes | % of .text |
|---|---:|---:|---:|
| Game (Spike) | 6423 | 0x16B450 (1,487,952) | 81.2% |
| CRI middleware (ADX family) | 1476 | 0x25F08 (155,400) | 8.5% |
| Sony SDK libs (libkernl, libgraph, libdbc, libpad2, libsdr, libmpeg, libipu, libmc, libcdvd, libvib) | 679 | 0x1E618 (124,440) | 6.8% |
| newlib (libc + libm) | 128 | 0xB970 (47,472) | 2.6% |
| libgcc | 50 | 0x4178 (16,760) | 0.9% |
| crt0 | 4 | 0x258 (600) | 0.0% |
| **All library/runtime (everything except game)** | **2337** | **0x54260 (344,672)** | **18.8%** |
| **Total** | **8760** | **0x1BF6B0 (1,832,624)** | 100% |

Game code is two blocks: 0x00100258-0x00269228 and 0x002BD230-0x002BF6B0. Everything in 0x00269228-0x002BD230 is library code, plus crt0 at the very start.

## Range table

End addresses are exclusive. "gp" = number of functions in the row that use `%gp_rel`.

| Start | End | Funcs | gp | What | Evidence | Confidence |
|---|---|---:|---:|---|---|---|
| 0x00100000 | 0x00100258 | 4 | 0 | Sony crt0 (`_start`, `_exit` thunk, `_root`) | Entry point 0x100008; register-clear `padduw` block, inline syscalls 0x3C/0x3D, calls `_InitSys`, `FlushCache`, `atexit`, `main`, then jumps to `exit`. Handwritten asm. | high |
| 0x00100258 | 0x00269228 | 6381 | 2321 | Game (Spike) code, main body | `main` at 0x100258 (calls `__main`, then the real game entry 0x100558). 2,321 of the 6,381 functions use `%gp_rel` (first at 0x100670, last at 0x269218); no function outside this row does. Game strings (IRX paths, menu/flash label names). 20 gp-free runs of 0x1000-0x6600 bytes exist inside (largest 0x10AD58-0x111358, 0x11F830-0x123F48, 0x14C658-0x14FF90); all of them call into / are called from surrounding game code, so they are treated as game code, see notes. | high (interior gp-free runs: medium) |
| 0x00269228 | 0x0026C050 | 109 | 0 | CRI ADXF (adx_fs: AFS/partition file access) | Existing `adxf_*` names 0x269308-0x26BE70; "AFS", partition error strings. No gp_rel from here to 0x2BD230. Start boundary: last gp_rel function is 0x269218, first CRI-named is 0x269308; the two helpers at 0x269228/0x2692D8 are only called from adxf code. | high |
| 0x0026C050 | 0x00272308 | 327 | 0 | CRI ADXT part 1: init/version, seamless entry (adxt_Entry*), ADXM server threads, ADXRNA, ADXSJD, ADXSTM | "ADXT/PS2EE Ver.9.71 Build:Dec 13 2005" referenced at 0x26C080/0x26C158; "ADXM: UsrVsyncThread" strings; existing names `adxt_EntryFnameRange`..`adxstmf_stat_exec`. | high (internal split: low) |
| 0x00272308 | 0x00277CB0 | 231 | 0 | CRI ADXT part 2: ADXRT core API (adxt_Create ... adxt_ExecHndl) | "ADXRT Ver.3020" at 0x272308; 50+ existing `adxt_*` names 0x272930-0x2779A8. | high |
| 0x00277CB0 | 0x00279E20 | 60 | 0 | CRI CVFS (virtual file system) | 37 functions with "cvFs<Name> #n:" error strings (35 newly named; `cvFsIsExistFile` text appears in two functions so neither is named). | high |
| 0x00279E20 | 0x0027B1E0 | 53 | 0 | CRI SRD (sector read driver for DVD/host) | "SRD: ..." strings 0x279E20-0x27B168; callers of sceCd*/sceOpen etc. | high (end boundary 0x27B1E0-0x27B290 uncertain) |
| 0x0027B1E0 | 0x0027BB00 | 17 | 0 | CRI DTX (EE<->IOP data transfer) | `DTX_Create` 0x27B4A0, "DTX_Init bind errr" 0x27B830, "Hello from EE". | medium (both boundaries +-0x200) |
| 0x0027BB00 | 0x0027D560 | 53 | 0 | CRI DVCI (DVD file interface) | `dvCi*` names 0x27BE60-0x27D0F0, "DVCI: ..." strings to 0x27D270. | high (start 0x27B920-0x27BE60 uncertain) |
| 0x0027D560 | 0x0027E280 | 24 | 0 | CRI HTCI part 1 (host file interface, dir cache) | "host", "HTCI: ..." strings, `htCiLoadDirInfo` 0x27DF00. | medium |
| 0x0027E280 | 0x0027F410 | 51 | 0 | CRI LSC (stream controller) | `LSC_Create` 0x27E280, `LSC_ExecServer` 0x27F220, "E20050128xx: Illigal parameter lsc=NULL". | high |
| 0x0027F410 | 0x0027FF28 | 28 | 0 | CRI MFCI (memory file interface) | `mfCiOpen` 0x27F918, `mfCiReqRd`, `mfCiOpenEntry`; start uncertain in 0x27F410-0x27F918. | medium |
| 0x0027FF28 | 0x00280220 | 9 | 0 | CRI ADXT Dolby Pro Logic II / 3D-sound attach | "ADXT handle must be created for stereo output", "Fail to attach Dolby Pro Logic II". | medium |
| 0x00280220 | 0x00282428 | 67 | 0 | CRI PS2RNA (PS2 sound output) | `PS2RNA_*` names 0x280220-0x282278, "PS2RNA:" strings. | high |
| 0x00282428 | 0x00284A50 | 90 | 0 | CRI SJ (stream joint: SJMEM, SJRBF, SJUNI) | "SJMEM Error" 0x282428, "SJRBF Error" 0x282E40, "SJUNI Error" 0x283BF0, E20040902xx strings. | high |
| 0x00284A50 | 0x002856B0 | 27 | 0 | CRI SJX (EE<->IOP stream joint) | "SJX_Init can't allocate IOP Heap", "can't create SJX of IOP". | medium |
| 0x002856B0 | 0x00287168 | 87 | 0 | CRI SVM (server manager) + misc | `svm_unlock` 0x2856B0, `SVM_ExecSvrFuncId` 0x286118, "SVM_SetCbSvr" strings; 0x286200-0x287168 unattributed CRI helpers. | high for SVM, low for the tail |
| 0x00287168 | 0x00289F50 | 97 | 0 | CRI ADXB / ADX decoder front end | "CRI-MW", `ADXB_DecodeHeaderAdx` 0x287480, "SPSD", "RIFF"/"WAVE", "(c)CRI". | high |
| 0x00289F50 | 0x0028AE60 | 53 | 0 | CRI error handler (ADXERR), ADXF version, CRI CFG | "Error" at 0x289F50/0x289FC8 (0x289F50 is the error sink called by all adxf/adxt functions), "ADXF/PS2EE Ver.7.44", "CRI CFG/PS2EE Ver.1.01". | high |
| 0x0028AE60 | 0x0028B918 | 24 | 0 | CRI DTR (data transfer, IOP->EE) | `DTR_ExecHndl` 0x28AE60, `DTR_Init` 0x28B290. | high |
| 0x0028B918 | 0x0028C3A8 | 21 | 0 | CRI HTCI part 2 (host file handle ops) | `htCiGetFileSize` 0x28B918, `htCiOpen`, `htCiReqRd`, "HTCI: htCiStopTr timeout". | high |
| 0x0028C3A8 | 0x0028CCB8 | 23 | 0 | CRI PL2ENC (Dolby Pro Logic II encoder) | "PL2ENC Ver.1.02 Build:Dec 13 2005" at 0x28C490/0x28C560; start uncertain in 0x28C1B0-0x28C3A8. | high |
| 0x0028CCB8 | 0x0028F130 | 25 | 0 | CRI ADX sample decoders / format probes (AIFF, .snd/.sd, PCM cores) | "FORM"/"AIFF" 0x28D060, ".snd" 0x28D968; called only from ADXB (0x287818, 0x288458, 0x28AAC0) or via pointers; no gp_rel. | medium |
| 0x0028F130 | 0x002930B8 | 29 | 0 | newlib libm (float fdlibm: atanf cosf sinf tanf acosf asinf atan2f powf sqrtf pow ...) | Call graph matches fdlibm exactly (rem_pio2f/kernel_cosf/kernel_sinf, wrappers that tail-jump to __ieee754_*), pi/4 constant 0x3F490FD8; double paths call libgcc soft-float. 29 functions named. | high |
| 0x002930B8 | 0x00294B60 | 18 | 0 | Sony libgraph | "sceGsPutDrawEnv:", "sceGsSyncPath:", "sceGsExecStoreImage:", "sceGsDefDispEnv:", "rom0:ROMVER"; GsPutIMR/AddIntcHandler stubs. Version tag "PsIIlibgraph3000". | high |
| 0x00294B60 | 0x00295DB0 | 19 | 0 | Sony libdbc | "libdbc: bind failed", "sceDbc*: rpc error" x11. Tag "PsIIlibdbc  3020". | high |
| 0x00295DB0 | 0x00296668 | 15 | 0 | Sony libpad2 | "libpad2: buffer addr is not 64 byte align"; functions call sceDbc*; called from game pad init 0x122940/0x122A38. Start boundary uncertain within 0x295D88-0x295DF0. Tag "PsIIlibpad2 3020". | medium-high |
| 0x00296668 | 0x00296EE8 | 8 | 0 | Sony libsdr | "sceSdRemoteInit() RPC bind error!", "SceSdrCallbackThread". End boundary: 0x296DF0 (0xF8 bytes, no callers/callees) could belong to libsdr or libmpeg. Tag "PsIIlibsdr  3000". | high (end: medium) |
| 0x00296EE8 | 0x002A1408 | 127 | 0 | Sony libmpeg | "[MPEG ERROR]%s", "sceMpegGetPicture is aborted", "_sceMpegSliceA0(): error happens", MPEG-2 header/macroblock error text; IPU (0x10002000) and DMA ch3/ch4 register access. Tag "PsIIlibmpeg 3000". | high |
| 0x002A1408 | 0x002A1A48 | 7 | 0 | Sony libipu | Small IPU/DMA-register helpers (0x1000B000/0x1000B400/0x10002010/0x1000F520) called from libmpeg wrappers 0x298268/0x298290/0x297B60; follows libmpeg, matching the order of the "PsIIlibipu  3000" tag. Start could be anywhere in 0x2A11E8-0x2A1408. | medium |
| 0x002A1A48 | 0x002A3280 | 26 | 0 | Sony libmc | "sceMc_sema_regs", "bind error libmc", "libmc: too old release of mcserv.irx"; called from game memory-card code 0x116BA8-0x1190C8. Tag "PsIIlibmc   3020". | high |
| 0x002A3280 | 0x002A4E40 | 29 | 0 | Sony libcdvd | "SceCdCallbackThread", "SceCdNcmdSema", "Libcdvd bind err ...", "sceCdChgSys:". Start: 0x2A3280 already calls the cdvd N-cmd wait helper. Tag "PsIIlibcdvd 3000". | high |
| 0x002A4E40 | 0x002A4FB0 | 2 | 0 | Sony libvib | Two functions that only call sceDbcReceiveData / sceDbcSendData2; 0x2A4EC0 is called from the game pad code 0x122F10. Position matches the "PsIIlibvib  3000" tag (between cdvd and kernl). No strings. | medium |
| 0x002A4FB0 | 0x002A8DD8 | 44 | 0 | libgcc (64-bit mul/div, soft-float double, C++ EH/frame code, __main) | `__muldi3` .. `__umoddi3`, fp-bit double routines, `__main`/`__do_global_ctors`; 0x2A7488-0x2A8A18 is exception-handling/unwind support (uses WaitSema/SignalSema for locking) - attribution of that sub-block to libgcc vs Sony glue is inferred. | high (EH sub-block: medium) |
| 0x002A8DD8 | 0x002B07C0 | 99 | 0 | newlib libc (abort..strtoul, stdio/vfprintf, dtoa, mprec, malloc, reent stubs) | _impure_ptr-based code, R5900-optimised mem*/str*, `_vfprintf_r` (existing), `_dtoa_r` "Infinity"/"NaN", mprec.c order. 98 functions named. | high |
| 0x002B07C0 | 0x002BCEE0 | 428 | 0 | Sony libkernl (syscall table, thread/intc wrappers, TTY/deci2, kernel printf, SIF cmd/rpc, fileio, loadfile/iopheap/iopreset, kernel patches, alarm/timer) | 138 syscall stubs starting at 0x2B07C0; "## internel error in libkernl.a!", "SceSifrpc*", "SceStdio*Sema", "rom0:UDNL ", "SceKernelLibc", "# TLB ..." strings. Tag "PsIIlibkernl3000". | high |
| 0x002BCEE0 | 0x002BD230 | 6 | 0 | libgcc/libc float-conversion stragglers (linked late, pulled in by libkernl printf) | 5 functions calling soft-float helpers, called from libkernl printf 0x2B25B8/0x2B2790; plus 0x2BD220 (0x10 bytes, called from libkernl 0x2B12A0-0x2B13E0). Exact origin not established. | medium |
| 0x002BD230 | 0x002BF6B0 | 42 | 0 | Game (Spike) code, trailing block: memory-card menu UI | Strings "mc_menu_plate_%d", "mc_reconfir_plate_%d", "fl_list_up"; calls game functions at 0x10D8B0, 0x124F68, 0x25C2A8 ... No gp_rel in any of the 42 functions, so probably a separate object/section built with different flags (or linkonce/inline code); it is game code by content, not library code. | high (game); reason for placement: unknown |

## How the classification was made

Verified (read directly from the disassembly):

- **gp_rel split.** Every function that uses `%gp_rel` lies in 0x00100670-0x00269218. From 0x00269228 to the end of `.text` (2,375 functions) none does.
- **Library order in `.text` matches the order of the `PsIIlib*` version tags in `.data`** (graph, dbc, pad2, sdr, mpeg, ipu, mc, cdvd, vib, kernl), with newlib/libgcc sitting between libvib and libkernl.
- **Top-level boundaries** 0x00269228 (game -> CRI), 0x0028F130 (CRI -> libm), 0x002930B8 (libm -> libgraph), 0x002A4FB0 (libvib -> libgcc), 0x002B07C0 (start of the libkernl syscall table) and 0x002BD230 (library -> trailing game block) each have identified functions on both sides.
- **Trailing game block** 0x002BD230-0x002BF6B0: memory-card menu strings and direct calls into the main game body; its entry 0x002BD230 has no direct caller (reached through a pointer).

Inferred (reasonable, not proven):

- **CRI module sub-boundaries.** Rows inside 0x00269228-0x0028F130 are anchored on named functions and version/error strings; the functions between two anchors were assigned to the preceding module. Expect individual boundaries to be off by a few functions. The CRI-vs-not-CRI classification of the whole span is solid.
- **0x0028CCB8-0x0028F130** carries almost no strings; it is called only from ADXB code or through pointers, so it is counted as CRI.
- **libipu / libpad2 / libsdr / libvib edges**: see the candidate ranges in the table.
- **0x002A7488-0x002A8A18** (exception-handling support) and **0x002BCEE0-0x002BD230** are attributed to libgcc by position and call graph only.
- **gp-free runs inside the game block.** Absence of `gp_rel` alone does not make a function library code: 4,060 game-block functions simply touch no small globals. Twenty contiguous gp-free runs of 0x1000 bytes or more exist (0x100B98-0x101E58, 0x103600-0x105C58, 0x1094A8-0x10AB38, 0x10AD58-0x111358, 0x11F830-0x121DA8, 0x121DE0-0x123F48, 0x1288C8-0x129D60, 0x14C658-0x14FF90, 0x150B58-0x151D98, 0x15D948-0x15EF18, 0x18F3A8-0x190610, 0x1C0AA8-0x1C2098, 0x1C2C80-0x1C4520, 0x1CF578-0x1D0650, 0x203168-0x204200, 0x204918-0x205E38, 0x20DC40-0x20F070, 0x216ED8-0x218308, 0x224B50-0x226BA8, 0x25E5E8-0x25F610). All of them call out to, and are called from, gp-using game code, and none contains a library version or error string, so they are counted as game. Two are worth a second look:
  - **0x0011F830-0x00123F48** (223 functions, ~2,600 external call sites): VU0 macro-instruction vector/matrix helpers and small utilities. Counted as game; it could be a Spike in-house library or derived from Sony's libvu0 sample source. Origin not established.
  - **0x001094A8-0x00111358**: three functions here (0x10A6E0, 0x10B4F0, 0x10B6F8) reach short strings in `.sdata` ("pad", "true", "false") with absolute `lui/addiu` instead of `gp_rel`, which default-flag game code would not do. That suggests objects built with different small-data flags (an in-house library). Counted as game.

## EE kernel syscall stubs that were left unnamed

Number is certain, name is not (retail-reserved numbers, or the stub is the underscore-prefixed inner half of a wrapper):

| Address | Syscall | Note |
|---|---|---|
| 0x00294360 | 0x80 | in libgraph, called by the display-env setup at 0x00294370 (GS dx/dy offset query) |
| 0x002B07C0 | 0x0 | RFU / reset |
| 0x002B07F0 | 0x3 | RFU |
| 0x002B0810 | 0x5 | RFU |
| 0x002B0840 | 0x8 | RFU |
| 0x002B0850 | 0x9 | RFU |
| 0x002B0960 | 0xFC | alarm set (libkernl has its own alarm layer; stub has no direct callers) |
| 0x002B0970 | 0xFD | alarm release (same) |
| 0x002B09C0 | -0xFE | i-variant of 0xFC |
| 0x002B09D0 | -0xFF | i-variant of 0xFD |
| 0x002B0A00 | 0x22 | thread start; only caller is the wrapper named `StartThread` (0x002B1D20), so this is probably `_StartThread` |
| 0x002B0BA0 | 0x3C | main-thread setup (also inlined in `_start`) |
| 0x002B0BB0 | 0x3D | heap setup (also inlined in `_start`) |
| 0x002B0BD0 | 0x3F | RFU |
| 0x002B0C70 | 0x49 | RFU |
| 0x002B0CE0 | 0x50 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0CF0 | 0x51 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D00 | 0x52 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D10 | -0x53 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D20 | 0x54 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D30 | -0x55 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D40 | 0x56 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D50 | 0x57 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D60 | -0x58 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D70 | 0x59 | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D80 | -0x5A | event-flag range 0x50-0x5B, reserved in retail kernels |
| 0x002B0D90 | 0x5B | local stub in libkernl kernel-patch code (several copies) |
| 0x002B0DA0 | 0x5C | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0DB0 | -0x5C | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0DC0 | 0x5D | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0DD0 | -0x5D | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0DE0 | 0x5E | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0DF0 | -0x5E | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0E00 | 0x5F | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0E10 | -0x5F | Intc/Dmac handler enable/disable family (0x5C-0x5F, negative = i-variant); exact Sony spelling not confirmed |
| 0x002B0F70 | 0x75 | kernel print |
| 0x002B1050 | 0x82 | TLB init (called from the "# TLB" code at 0x002BC928) |

Local (static) stubs inside libkernl's patch/init code, not named because they duplicate table entries: 0x002BA2C0 (0x5A), 0x002BA348 (0x83), 0x002BA490 (0x74), 0x002BA4F8 (0x74), 0x002BA508 (0x5A), 0x002BA550 (0x5B), 0x002BA678 (0x5A), 0x002BA6B8 (0x74), 0x002BA918 (0x74), 0x002BA928 (0x5A), 0x002BA970 (0x5B), 0x002BC738 (0x5A), 0x002BC780 (0x5B), 0x002BC790 (0x74), 0x002BC868 (0x55), 0x002BC878 (-0x55), 0x002BC888 (0x56), 0x002BC8C8 (-0x56), 0x002BC8D8 (0x57), 0x002BC8E8 (-0x57), 0x002BC8F8 (0x58), 0x002BC908 (-0x58), 0x002BC918 (0x59).
