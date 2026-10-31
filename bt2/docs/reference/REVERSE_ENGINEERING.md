# Reverse-engineering record

Record conclusions with their source, method, and reproducible evidence. Distinguish observations from hypotheses. Do not assign function names, structures, addresses, or subsystem behavior without evidence. Preserve unknown names such as `FUN_00123400`; this is an example, not a discovered BT2 function.

## Executable

Planned target: North American SLUS-21441. Preliminary validation of local `rom/SLUS_214.41` passed on 2026-10-03 using `tools/verify_game/verify_game.py`:

- Size: 2,675,936 bytes.
- SHA-256: `f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e`.
- Basic header: ELF32, little-endian, MIPS, executable.
- Sibling `SYSTEM.CNF` boot reference: `cdrom0:\SLUS_214.41;1`.

Retail revision is now confirmed: the local 2,998,829,056-byte ISO matches the official [Redump PS2 datfile](http://redump.org/datfile/ps2/) USA entry (MD5 `37393b158051af0ab5cb3c86d04d51b7`, SHA-1 `0b11f7a2a4c178b555c45a58ce8ee725e57a1423`). Its root `SLUS_214.41;1` extent at byte offset 624,640 has the same size and SHA-256 as the local executable above. `config/retail_revision.json` records reference provenance; `verify_disc.py` repeats the read-only comparison. No independent published ELF hash is claimed.

ELF metadata inspected with `tools/inspect_elf/inspect_elf.py` on 2026-10-03 using the same SHA-256 above. The header declares entry point `0x00100008`, four program headers, and 33 section headers. The declared entry lies within an executable load region. This does not mean the entry has executed.

## Memory Map

Declared ELF load regions (end addresses are exclusive; these are file metadata, not a confirmed hardware memory map):

| Program index | Virtual start | Memory end | File offset | File bytes | Memory bytes | Flags |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `0x00100000` | `0x0033f390` | `0x1000` | 2,356,112 | 2,356,112 | 7 |
| 2 | `0x0033f400` | `0x003b3038` | `0x240400` | 297,500 | 474,168 | 6 |
| 3 | `0x00000000` | `0x00000000` | `0x288ed0` | 0 | 0 | 7 |

Program 2 declares 176,668 bytes beyond its file payload. Program 3 is empty and declares physical address `0x003b3180`; its file offset and virtual address do not satisfy the generic ELF alignment relationship for its declared `0x1000` alignment. Its purpose is unknown and no loader behavior is inferred.

## Functions

Ghidra 12.1.4 with the pinned R5900 extension analyzed the recorded local executable. It discovered 9,541 unreviewed function candidates; 7,241 have contiguous aligned bodies exported to the local CSV. These counts describe analyzer output, not verified boundaries or purposes. The entry candidate is `FUN_00100008`, with proposed exclusive end `0x00100248` (576 bytes). No semantic function name has been established.

### Entry boundary review, 2026-10-03

For the executable hash above, `tools/ghidra/review_entry.py` cross-checked raw ELF control instruction words against the Ghidra export. All 144 instruction addresses cover `[0x00100008, 0x00100248)` without gaps. Sixteen control transfers were audited, including two syscall instructions. Direct targets agree with the export, conditional branches stay within the body, and all delayed transfers include their following instruction.

The final transfer at `0x00100240` jumps to `0x00324398`; its delay slot at `0x00100244` is included. This supports the exclusive end `0x00100248` for a static entry-body map. It does not establish that the external target never returns, certify all opcode semantics, or prove behavior during execution. No full-game reviewed map exists.

| Direct call site | Neutral target |
| --- | --- |
| `0x001001E8` | `FUN_00338520` |
| `0x001001F0` | `FUN_0032EEE0` |
| `0x00100220` | `FUN_003242D0` |
| `0x00100238` | `FUN_00100388` |

These are encoded targets, not established purposes or reviewed callee boundaries. An indirect call through register 16 occurs at `0x00100208`; its reachability and target require separate analysis.

### Direct entry target boundary review, 2026-10-03

The exporter now includes complete instruction listings for the five direct targets. `tools/ghidra/review_entry_targets.py` checks their file-backed ranges, raw control targets, complete instruction coverage, internal conditional branches, and included delay slots against the same executable hash. All five passed the limited static audit:

| Neutral name | Exclusive end | Instructions | Transfers | Final transfer |
| --- | --- | --- | --- | --- |
| `FUN_00338520` | `0x00338574` | 21 | 9 | Jump to `0x0033A238` |
| `FUN_0032EEE0` | `0x0032EEF0` | 4 | 2 | Return through `ra` |
| `FUN_003242D0` | `0x00324370` | 40 | 5 | Return through `ra` |
| `FUN_00100388` | `0x001006D8` | 212 | 72 | Return through `ra` |
| `FUN_00324398` | `0x00324444` | 43 | 8 | Jump to `0x00100248` |

These extents are supported for static body mapping; runtime semantics and deeper callees remain unverified. `FUN_00324398` includes branch-likely instructions and two indirect calls through `v0`; the audit records likely branches without claiming to execute their annulled delay-slot behavior.

At `0x00100688`, `FUN_00100388` directly calls `0x003B7FA8`, outside the ELF's file-backed executable sections. Its origin and contents are unknown. Do not create a function body or stub for it without evidence. Ghidra assigned a semantic label to `0x0032EEE0`; retain the neutral name because the label's provenance has not been confirmed.

### Unmapped target investigation, 2026-10-03

`tools/ghidra/audit_startup.py` independently checked the raw entry constants and mapped sections for the recorded hash. `0x003B7FA8` is outside all declared PT_LOAD memory extents, including zero-fill regions. The entry constructs bounds `0x00387E80` and `0x0043058F`; its observed byte/quadword zero-store loops appear to clear that interval before the first direct callee. The unresolved target lies within this larger interval. This is static evidence, not an observed memory write during execution.

An aligned scan of allocated file-backed sections found no exact 32-bit literal equal to the target. Executable-section scanning found one encoded JAL to it, at `0x00100688`. These scans do not exclude split address constants, calculated pointers, encoded data, or code copied/produced later. The target's contents and the writer remain unknown. A future memory layer must account for observed startup accesses beyond ELF load extents rather than assuming those extents describe every runtime access.

## Subsystems

No BT2 subsystem behavior has been established.

## File Formats

Only executable container metadata has been inspected. The section table includes `.text`, `.vutext`, `.data`, `.rodata`, `.bss`, and `.DVP.*` names; their game-specific purpose has not been established. No SHT_SYMTAB or SHT_DYNSYM section was observed. Other game file formats remain uninspected.

## Runtime Calls

Entry contains syscall instructions at `0x001001C8` and `0x001001E4`, each with encoded code field zero. The preceding Ghidra instructions load register `v1` with `0x3C` and `0x3D`, respectively. These observations do not assign syscall names or establish the required runtime ABI. No runtime implementation has been added.

The direct callee at `0x0032EEE0` loads `v1` with `0x64`, executes a syscall with encoded code field zero at `0x0032EEE4`, and returns through `ra`.

### Startup ABI reference, 2026-10-03

PS2SDK revision `ac92a9f657d2e531dd8f060250b07f2a5ac6dea5` defines selectors `0x3C` as SetupThread, `0x3D` as SetupHeap, and `0x64` as FlushCache. Its assembly wrapper puts the selector in register 3 (`v1`) and executes syscall; its startup code consumes the SetupThread return from register 2 (`v0`) as the stack pointer. These primary-source conventions match the reviewed local instruction sequences. This supports the following ABI interpretation, while actual retail kernel behavior remains untested. Sources: [selector definitions](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/include/syscallnr.h), [assembly wrapper](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/kernel.S), and [startup](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/startup/src/crt0.c).

| Local syscall site | Selector / public ABI | Observed arguments |
| --- | --- | --- |
| `0x001001C8` | `0x3C` / SetupThread | `a0=0x0038BDF0`, `a1=0xFFFFFFFF`, `a2=0x00100000`, `a3=0x0038B980`, `t0=0x00100250`; returned `v0` copied to `sp` |
| `0x001001E4` | `0x3D` / SetupHeap | `a0=0x0043058F`, `a1=0xFFFFFFFF` |
| `0x0032EEE4` | `0x64` / FlushCache | Entry call sets `a0=0` in its delay slot |

PS2SDK declares SetupThread parameters as gp, stack, stack size, args, root function; SetupHeap takes heap start and size. Its FlushCache operation zero is WRITEBACK_DCACHE. This identifies `FUN_0032EEE0` as a matching FlushCache syscall wrapper; retain its neutral working name in maps. Pointer contents, sentinel handling for `0xFFFFFFFF`, alignment, stack placement, heap limits, and cache behavior require further evidence before implementation. [Public declarations and cache modes](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/include/kernel.h).

Reference revision and downloaded file hashes are recorded in `config/startup_abi.sources.json`. Reference files remain ignored under `work/abi-reference/ps2sdk/`; no SDK implementation is linked or copied into the runtime.

## Confirmed Facts

The native host builds and prints its startup message without loading game data. This is a host observation, not a conclusion about the retail game.

The optional native generated probe executes PS2Recomp's local translation of `FUN_0032EEE0` from the inspected executable through an authored adapter. It reaches a recorded FlushCache request and returns to a host sentinel. It performs no guest memory access and establishes no whole-game boot or cache-equivalence behavior. The broader provisional EE map and generation results are recorded in [the detailed progress history](../progress/2026-10.md); [PROG.md](../PROG.md) summarizes current status.

On 2026-10-03, the generated entry also compiled/linked and executed through an authored bounded-RAM bootstrap adapter. With the observed uninitialized region poisoned for verification, it cleared `[0x00387E80, 0x0043058F)` with 43,120 quadword and 15 byte stores. Execution reached the first syscall with selector `0x3C`, resume PC `0x001001CC`, and argument registers matching the static review above. The runtime deliberately rejected that unsupported syscall. Its return value, stack behavior, later code and whole-game boot remain unobserved. Local evidence is in ignored `work/bt2-generation/entry-stop.json` and test logs.

## Hypotheses

The unresolved target may acquire executable contents after the startup zero-store loops. This is an investigation hypothesis; no writer, source bytes, or executed contents have been identified.

## Unknowns

Actual runtime memory behavior, the empty load record's purpose, general function boundaries, game file formats, and required PS2 runtime behavior remain unverified. Ghidra reported a conflicting `rom0` block, unsupported byte mapping for DVP overlays, and decompiler flow errors. Candidate maps, disassembly, logs, and projects stay under ignored `work/ghidra/`; raw ELF metadata stays in ignored `work/elf_report.json`. Experimental generation uses structurally filtered provisional maps; focus further manual review on actual build and execution blockers.

### Bootstrap stack model (not retail BIOS confirmation)

[Play! kernel source](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/PS2OS.cpp) and [context reserve](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/PS2OS.h) provide the current default-stack model. The authored adapter supports only the observed sentinel and empty launch arguments. Execution now reaches SetupHeap; its implementation and retail BIOS stack comparison remain pending.

### SetupHeap bootstrap model and executed call boundary

The pinned Play! `sc_SetupHeap` returns main-thread stack base for size `0xFFFFFFFF`. Our bounded adapter records the requested lower boundary without changing its alignment and returns `0x01EFF000`. This model has not been compared against retail BIOS behavior. Generated entry now reaches JAL at `0x001001E8` to `FUN_00338520`, return PC `0x001001F0`; the callee has not executed yet.

### Executed first callee prologue

Generated `FUN_00338520` now executes its stack allocation and saves RA=`0x001001F0` at guest address `0x01FFED50`. Runtime then observes the JAL at `0x00338528` to `FUN_003382D0`, resume `0x00338530`. This establishes execution of the prologue only; no subsystem purpose has been assigned and no deeper target is yet executed.

### Executed nested startup syscall

`FUN_003382D0` builds a stack frame and calls `FUN_0032EC60` at `0x00338304`, with a0=`0x01FFED00` and resume `0x0033830C`. The generated wrapper sets v1=`0x40` and executes syscall at `0x0032EC64`, resume `0x0032EC68`. Pinned PS2SDK `syscallnr.h` identifies selector `0x40` as CreateSema; no subsystem purpose is inferred for the enclosing unknown functions. The syscall has not been implemented or returned yet.

### Executed semaphore creation and return paths

Generated startup now stores CreateSema IDs 0 and 1 at `0x0036AB58` and `0x0036AB5C` under the bounded compatibility model. Both objects initialize count/max_count to 1, with option values `0x00383BF0` and `0x00383C00`. Three generated JR return paths complete, and the outer caller reaches JAL at `0x00338530` to neutral `FUN_00338410`, return PC `0x00338538`. No semantic name has been assigned to the enclosing functions. Wait/signal/scheduler behavior and retail BIOS equivalence remain unverified.

### Executed SetSyscall registrations

Pinned PS2SDK names selector `0x74` SetSyscall and declares a void result. Generated startup now records `0x83` -> `0x00338388` and `0x5A` -> `0x00338350` from loaded guest globals. Invocation of custom handlers remains unsupported. Execution next reaches `FUN_003383C8` from JAL `0x00338464`, return `0x0033846C`, with a0/a1/a2=`0x80000000`/`0x80080000`/`0x00338388`. Handler purposes and memory behavior remain unknown; no semantic function names assigned.

### Registered handler discovery blocker

Execution enters `FUN_003383C8` and invokes selector `0x83` at `0x003383CC`, resume `0x003383D0`. The table contains handler `0x00338388`, which has no function entry in the current Ghidra export or provisional map. No handler boundary or purpose is established. `inspect_blocker.py` records range membership and exclusions in ignored `blocker-map.json`; it never guesses a boundary.

### Discovered handler's actual memory blocker

Focused Ghidra discovery produced provisional `[0x00338388, 0x003383C8)`. The regenerated handler compiles and is entered, but its first word read at `0x80000000` is rejected by the current main-RAM-only adapter. Required address mapping and kernel-memory contents have not been established. The callback adapter is a host execution model, not confirmed BIOS exception behavior; no missing memory was synthesized to advance the scan.

### Kernel address evidence and fixture limits

[PCSX2 memMapKernelMem](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Memory.cpp) maps `0x80000000` to physical zero. Our adapter exposes only a read-only captured window; it does not treat ELF zero-fill as kernel contents. Generated handler match/no-match behavior passes authored synthetic fixtures. Real capture comparison at PC `0x00338388` is pending, as are kernel table mutation and BIOS exception semantics.

### Snapshot-driven entry execution

With the explicitly supplied local capture, generated `FUN_00338388` returns twice through `FUN_003383C8`. `FUN_00338410` completes and stores `0x80014D00` at `0x0036AB40`; the two searches converge on the same value after their emitted offsets. Execution then attempts `FUN_00338A00` from `0x00338538`, resume `0x00338540`. This target is already provisionally mapped/generated, but is not registered in the bounded adapter. The capture window remains read-only and separate from main RAM; no kernel writes or BIOS exception semantics are established.

### First observed Timer 3 MMIO boundary

Generated `FUN_00338A00` now compiles/links/registers. It saves four doublewords in its 64-byte frame, then attempts a word read at `0x10001810`, PC `0x00338A1C`, through emitted `Load32`. The authored forwarding API uses the same checked read path and rejects MMIO. Pinned PS2SDK `common/include/ee_regs.h` (`A_EE_T3_MODE`) and `ee/kernel/include/timer.h` (`T3_MODE`) identify this register; hashes are recorded in `config/startup_abi.sources.json`. No value is supplied and no timer state or game-specific purpose is inferred. The emitted following instructions mask bit `0x100`; they have not executed in the native probe yet.

### Captured Timer 3 mode read and explicit replay

An instrumented local ELF copy reads Timer 3 mode once at the `0x00338A1C` boundary, stores the result beside an authored marker, and spins. PCSX2 2.9.81 with the local scph39001 BIOS returned `0x83`; the original ELF remains unchanged. The capture preserves PCSX2/BIOS/ELF/RAM hashes under ignored output. Pinned PCSX2 `HwRead.cpp` dispatches the counter read, `Counters.cpp` returns the low 16 bits of mode, and `Counters.h` identifies bit 8 as TargetInterrupt; hashes are in `config/startup_abi.sources.json`. The native probe now replays this explicitly supplied single read. It follows the clear-bit branch and reaches unregistered `FUN_00338998` from `0x00338A40`, resume `0x00338A48`, with arguments a0 `0x5A`, a1 `0x003389B8`. No general timer behavior or function purpose is claimed.

### Discovered registered handler reaches a kernel store

Generated `FUN_00338998` executes selector `0x74`, registers `0x003389B8` for selector `0x5A`, and returns. Generated `FUN_003389A8` then invokes `0x5A`; the observed handler was a map gap. Focused discovery produced provisional contiguous `[0x003389B8, 0x003389F0)`, 56 bytes, and regenerated the local map/output. The handler executes a word-count shift (`0x740 >> 2 = 0x1D0`), reads the first source word from `0x0036B328`, and reaches a word store to `0x80076000`, PC `0x003389D8`. The adapter rejects the write; no kernel bytes changed. The handler's complete loop, copied payload purpose, writable kernel behavior and BIOS/cache equivalence remain unverified. The function keeps its neutral name.

### Bounded patch overlay and returned generated copies

The entry probe explicitly enables aligned word stores only for `[0x80076000, 0x80076740)` after loading the captured kernel. Stores populate a separate overlay; KSEG0 word reads consult it, and baseline bytes remain immutable. No general KSEG1/physical alias coherency, guest kernel execution or instruction-cache invalidation is implemented. Generated `FUN_003389B8` copies 464 words from `0x0036B328`, returns through `FUN_003389A8`, then copies ten words from `0x0036BA68` to main RAM `0x00082000` and returns again. The probe compares every destination word against its source and checks the full baseline against its pre-run copy. Linked `FUN_0032EEE0` records an operation-0 FlushCache request and returns, then a second invocation stops at unsupported operation `2`, selector `0x64`, PC `0x0032EEE8`. This establishes returned copy paths, not cache/BIOS equivalence or a full boot.

## 2026-10-03 - Cache boundary and kernel handler registration

Pinned PS2SDK `kernel.h` defines FlushCache operation `2` as instruction-cache invalidation and declares a void return. The probe now accepts only observed operations `0`/`2`, preserves return registers, and records separate invalidation/total request counts. Its stores are immediately visible and dispatch is fixed native C++; no guest cache exists and no patched kernel instructions are translated by this call. This is a bounded compatibility behavior, not hardware cache validation.

Generated entry completes both requests, then SetSyscall (`0x74`) attempts to register `0x80076000` for `0x5B` at resume PC `0x003389A0`. Main-RAM registration validation rejects that address before installing it. This is handler-address validation, not a guest load fault. Captured baseline/copy comparisons remain unchanged. Handler meaning, execution and aliases remain unknown.

## 2026-10-03 - Copied kernel registration and dispatch

The bounded adapter now accepts aligned SetSyscall destinations in the enabled kernel overlay only if the entry word was actually written. This registers a pointer without translating or executing its bytes. Baseline-only, unwritten, adjacent, unsupported-alias and wrong-memory-pointer destinations remain rejected; null unregister still works and the void ABI preserves return registers.

Actual generated `FUN_00338998` registers `0x80076000` for `0x5B` and returns. Linked neutral wrapper `FUN_003389F0` invokes `0x5B` with observed `a0=0xFC`; dispatch rejects the uncompiled target before entering it, at resume PC `0x003389F8`. All previous copies/cache requests remain verified. The kernel destination is outside original file-backed ELF `.text`; it needs a separate payload map with source/copy provenance and runtime-address relocation. No purpose is assigned to the handler or argument.

## 2026-10-03 - Generated copied-kernel entry executes

The observed `0x740` bytes at source VA `0x0036B328` are extracted from the hash-checked retail ELF's single file-backed load and packaged unchanged at observed runtime VA `0x80076000`. PS2Recomp's provisional automatic scan discovers entry range `[0x80076000, 0x80076038)`. A second generation uses a neutral function map for that entry, retaining the rest of the payload as non-executable data for this scoped translation. No instructions are rewritten and no semantic function name is assigned. The exploratory full-payload scan treated trailing bytes as code and reported one unhandled-instruction error; those additional candidates are not accepted. Scoped generation has one function, zero warnings/errors/decode failures.

`FUN_80076000` executes its table search and returns five times through generated `FUN_003389F0`. Four returned targets are registered for slots `0xFC` through `0xFF`, matching the copied table (the table order is not numeric slot order). The next SetSyscall request is extended selector `0x12C`, handler `0x80076488`, resume PC `0x003389A0`; the runtime rejects the selector before installing it. No behavior is inferred for that handler.

Before every native kernel entry dispatch, the complete written overlay must equal the payload bound at compile time. Missing/tampered words prevent entry, including changes to remaining payload data; broader mutable kernel behavior is not supported by this binding. Every source/destination word and the unchanged captured baseline remain checked. These results validate this specific generated lookup path, not full BIOS exception/privilege/cache equivalence.

## 2026-10-03 - Extended Timer 3 registration and subsequent startup

Pinned Play! `sc_SetSyscall` treats selector `0x12C` as INTC line 12 / Timer 3 registration and requests line enable. The adapter accepts one aligned entry word written in the bounded kernel overlay, records its original KSEG0 address and the enable request, and preserves the void SDK ABI. It does not translate that handler, map its physical alias, update MMIO or deliver/schedule interrupts. Null, duplicate, other extended selectors and unsupported destinations remain rejected. Reference hash/role is recorded in `config/startup_abi.sources.json`; retail BIOS equivalence remains unverified.

Actual `FUN_00338A00` completes: the generated lookup returns six times, slots `0xFC` through `0xFF` and `8` match the copied table, and the Timer 3 handler request is `0x80076488`. `FUN_00338B78` then executes generated `FUN_003251DC` to zero an `0x2000`-byte buffer at `0x003ACCC0`; subsequent code creates 128 links with 64-byte stride, terminal zero and zero remaining block payload. `FUN_00339E48` also returns. Unknown routines retain neutral names. The next wrapper `FUN_0032E950` requests SDK AddIntcHandler (`0x10`), cause `11`, handler `0x00339118`, next/argument `0`, resume PC `0x0032E958`; the observed registration now returns ID 2, which generated code stores at `0x0036BAD8`. The pinned Play! reference uses a shared 128-entry INTC pool with IDs starting at one; Timer 3 reserves ID 1. The probe captures GP and front-inserts the cause 11 handler without delivering interrupts. Generated `FUN_00338268` returns with explicit probe COP0 Status zero; this does not confirm retail BIOS status. The next actual blocker is the Timer 2 mode read at `0x10001010`, PC `0x00338C58`.

Signed low-word register access and vector conversion/PCPYLD support are exercised by these generated paths. The independent generated fill test checks nonzero byte replication, alignment/short/zero cases, return value and all outside bytes unchanged. Initial entry zeroing is now checked at SetupThread, before later startup legitimately changes cleared memory; original zero-store counts and current stack/copy/registration assertions remain active.

### Timer 2 captured read and generated write boundary

Isolated standalone-ELF PCSX2 instrumentation at the reached `0x00338C58` read observed `0x80` from `0x10001010`. With explicit verified replay, generated `FUN_00338B78` retains bit 7, combines its caller's argument `2` and mode bits `0x300`, then calls `FUN_00338B58` with `0x382`. That wrapper passes address `0xB0001010` to generated `FUN_00338AD8`. The helper changes the probe's COP0 Status from zero to `4` and attempts a word store at PC `0x00338B30`; it is rejected before memory mutation. These two routines remain partial. Their following ErrorEPC/ERET path is compiled but has not executed; no successful privileged return or hardware behavior is inferred. The runtime context adds explicit EPC/ErrorEPC and LL-bit storage plus clear operation to compile the generated path. Retail BIOS Status, Timer 2 write effects, counter progression and interrupt execution remain unknown/unimplemented.

### Observed Timer 2 store and generated ErrorEPC return

Pinned PCSX2 `rcntWmode` establishes control bits `0x3FF` and write-one-to-clear status flags `0xC00`. The authored startup adapter accepts only one observed value `0x382` at the reached alias `0xB0001010` after explicit capture consumption; it records the resulting mode bits, without implementing counter sync/rate/gating/events. Independent generated-helper tests with synthetic initial modes/status validate preservation of set flags, EIE clear/set, ErrorEPC return instead of EPC, restored Status, cleared LL bit and unchanged RAM. Real generated `FUN_00338B58`/`FUN_00338AD8` now return to `0x00338C90`. That ERET return is validated through callee resume checks and the independent fixture; it does not increment the adapter's JR return counter. The next linked functions `FUN_0032F7A8` and `FUN_0032E9A0` reach EnableIntc (`0x14`) with cause 11, resume PC `0x0032E9A8`; both remain partial. No BIOS exception equivalence or interrupt execution is established.

### Captured INTC mask and EnableIntc return

Pinned Play! `sc_EnableIntc` returns whether the requested mask bit changed. Isolated PCSX2 standalone-ELF instrumentation immediately before syscall PC `0x0032E9A4` reads `0x1000F010` and observes mask `0x1002`, including Timer 3's bit 12 and excluding cause 11. This is a checkpoint at that call, not a BIOS reset default. With explicit checkpoint input, the authored cause 11 adapter returns 1 and records mask `0x1802`; repeated enable returns 0. Other bits and upper v0 register half stay unchanged. Generated `FUN_0032E9A0`, `FUN_0032F7A8` and `FUN_00338B78` return. Newly linked neutral `FUN_00338DB8` calls the existing status helper, then reads Timer 2 mode again at PC `0x00338DD8`; the one-read replay rejects that access. Timer progression, pending interrupt processing and handler delivery remain unimplemented.

### Timer 2 second-read validation and CreateThread boundary

Isolated PCSX2 standalone-ELF instrumentation at second read `0x00338DD8` observed Timer 2 mode `0x382`, matching the authored adapter's recorded mode after startup write. Explicit validation permits one readback of recorded state and rejects mismatch or later reads; no evolving timer flags/counter are inferred. Generated `FUN_00338DB8` returns with the probe's EIE clear. Newly linked neutral `FUN_0032FA38` creates semaphore ID 2 (initial 0, maximum 255, option `0x00383660`) and stores it at `0x003A8C40`, then calls generated wrapper `FUN_0032EA60`. Actual blocker is CreateThread (`0x20`), resume PC `0x0032EA68`, descriptor address `0x01FFECC0`. No thread was created or scheduled. Descriptor fields and callback purpose require pinned ABI validation before implementation; unknown routines remain neutrally named.

### Bounded dormant thread creation and main-thread query

Pinned PS2SDK `ee_thread_t` and Play! `THREADPARAM` agree on the 36-byte descriptor offsets. Actual request consumes entry `0x0032F960`, stack base `0x003A8840`, size `0x400`, GP `0x0038BDF0`, initial priority 0. The authored reference model reserves main ID 1 / idle ID 256 and returns ID 2 for first creation; retail allocation identity is unconfirmed. Metadata remains dormant, inherits the modeled heap base and records context at `0x003A89A0`. ThreadReset evidence writes low words GP, SP/FP `0x003A8C20` and RA `0x1FC03000` at register offsets 28..31; all other context bytes remain unchanged. That RA is a pinned reference-model epilog address, not a confirmed retail BIOS function, and is not bound/executed.

Generated `FUN_0032EA60` returns and the caller stores ID 2 at `0x0036AA84`. Newly linked `FUN_0032FDA0` calls `FUN_0032F730`, which tests the probe Status IE bit and returns 1 with IE clear; the caller returns -1 through its guard. This validates only that guard path. Generated GetThreadId wrapper `FUN_0032EB50` returns modeled main ID 1. Actual next blocker: `FUN_0032EAF0` ChangeThreadPriority (`0x29`), arguments ID 1 / priority 1, PC `0x0032EAF8`. No created thread or interrupt handler has executed.


## Main priority change and OSD configuration blocker (2026-10-03)

Pinned Play! `sc_ChangeThreadPriority` returns the previous current priority, records the requested priority and reschedules runnable threads. The authored probe accepts only the observed main ID 1 / priority 1 request with syscall code zero after SetupThread. Main is its sole runnable thread; created threads remain dormant. It returns previous priority 0, records 1 and preserves upper register bits. Unsupported IDs/priorities/code and missing setup fail without mutation. This validates a bounded reference model, not retail BIOS scheduling.

Generated `FUN_0032EAF0` and `FUN_0032FA38` now return. Linked reached functions `FUN_00338648`, `FUN_003385E0` and `FUN_0032ED10` lead to GetOsdConfigParam selector `0x4B`, resume PC `0x0032ED18`, output pointer `0x01FFECE0`. No OSD value is synthesized. Full checkpoint execution verifies 58 guest calls, 53 JR returns, 699 word / 40 doubleword / 43,632 quadword / 15 byte writes, one priority change and unchanged dormant-thread state. Counts are 34 functions entered / 29 completed across probes; five startup frames remain partial. No gameplay or full boot is claimed.

Verification: full checkpoint pipeline and MSVC build passed, six CTests passed, 29 recomp-tool tests and eight capture-tool tests passed. Proprietary-derived output and capture receipts remain ignored. Next: validate the OSD configuration ABI and local BIOS value at this actual blocker.


## Captured first OSD configuration read (2026-10-03)

SDK kernel.h declares GetOsdConfigParam as void with a pointer parameter; pinned Play! sc_GetOsdConfigParam writes one 32-bit word. Play! preference defaults are not used. An authored 32-byte trap at caller return PC `0x003385F4` reads the completed syscall output from SP, publishes it beside marker `0xB707`, and loops before the setter. Isolated local PCSX2/BIOS capture observed `0x21C12010`. This applies to that instrumented standalone ELF/profile, not a universal BIOS default or confirmation of all OSD bit meanings.

Capture validation checks source hash, full RAM size/hash, exact trap, point/marker and payload. The pipeline requires all prior captures and records OSD provenance in its ignored receipt. The adapter accepts one supplied read into an aligned bounded RAM word, preserves the void result register and rejects missing/duplicate inputs, repeated reads, wrong pointers, code and bounds without consuming the checkpoint.

Generated `FUN_0032ED10` returns. Startup clears `0xE000` and sets `0x2000`, writes the prepared word at SP+4 and calls newly linked `FUN_0032ED00`. Actual stop: SetOsdConfigParam selector `0x4A`, resume PC `0x0032ED08`, pointer `0x01FFECE4`. Setter behavior remains unsupported. Counts: 35 functions entered / 30 completed across probes; 59 guest calls, 54 JR returns, 701 word / 40 doubleword / 43,632 quadword / 15 byte writes. Five startup frames remain partial; no full boot/gameplay.

Full checkpoint pipeline/MSVC build, six CTests, 30 recomp tooling tests and nine capture tests passed. Without OSD input the earlier GetOsdConfigParam boundary still passes. All retail data, patched ELF, BIOS, RAM dumps and generated code remain ignored. Next: capture setter/readback behavior before implementing mutation.


## First OSD setter and readback validated (2026-10-03)

An authored 40-byte trap at `0x00338620`, after the second GetOsdConfigParam returns and before the restore setter, publishes the readback word from SP+4 and saved original word from SP beside marker `0xB708`. Both captured words are `0x21C12010`. The first setter's expression `(original & 0xFFFF1FFF) | 0x2000` equals the original. This establishes one same-value write/readback in the local isolated PCSX2/BIOS/profile; it does not establish handling of changed values or a general OSD setter.

The pipeline validates original retail hash, RAM size/hash, exact trap/marker/payload, equality of original/readback and agreement with the first OSD capture. New `--osd-readback-capture` requires the original OSD capture and all prior inputs. The authored runtime accepts only the sequence first read -> one same-value setter -> second read, preserves void result registers and consumes no state on rejected calls. Missing/duplicate/mismatched checkpoints, mutation, wrong order, extra calls, unsupported code, wrong RAM pointers and alignment/bounds fail.

Generated `FUN_0032ED00` completes its first invocation; `FUN_0032ED10` completes its second. Actual next stop is the second SetOsdConfigParam at resume PC `0x0032ED08`, pointer `0x01FFECE0`, restoring the saved original. Function counts: 35 distinct entered, 31 completed at least once. `FUN_0032ED00` belongs to both the completed set and currently partial stack because its second invocation has not returned. Full probe: 61 guest calls, 56 JR returns, 702 word / 40 doubleword / 43,632 quadword / 15 byte writes, two OSD reads and one setter.

Verification: full checkpoint pipeline and MSVC build passed, six CTests passed, 31 recomp tooling tests and ten capture-tool tests passed. Omitting the readback checkpoint preserves the first setter boundary. Game data, BIOS, patched ELF, RAM dumps, receipts and generated code remain ignored. No full boot/gameplay. Next: validate the restore call and continue to the next actual blocker.


## OSD restore completed; next registered handler reached (2026-10-03)

The generated restore request supplies the saved original `0x21C12010`, equal to the modeled current word. The authored same-value adapter now accepts a second setter only after the first read/set/read sequence, preserving registers and RAM. A third setter/read remains rejected; configuration mutation remains unsupported. Existing checkpoints establish the equal-word path; the restore continuation is a bounded reference model, not a separate post-restore retail BIOS capture.

Generated `FUN_0032ED00` returns on its second invocation, `FUN_003385E0` returns zero through its bit test, and `FUN_00338648` takes its skip branch and returns. Linked `FUN_0033A820`, `FUN_0033A810` and `FUN_0033A7B8` execute the next reached path. SetSyscall changes slot `0x5A` to `0x0033A7C8`; the next request is destination `0x80075000`, source `0x0036BAF8`, size `0x330`. Runtime rejects the unbound registered handler before executing it. No kernel payload is copied or skipped.

The stop-report catch previously omitted the exception's registered handler. It now preserves that field, so blocker classification inspects `0x0033A7C8` rather than wrapper resume PC `0x0033A7C0`. Correct classification is a map gap; the preceding wrapper ends at `0x0033A7C8`, and the next analysis candidate starts at `0x0033A800`. Do not expand/rename the wrapper to cover the handler without discovery evidence.

Counts: 38 functions entered / 34 completed across probes; four startup frames remain partial. Full execution: 64 guest calls, 60 JR returns, 11 syscall registrations, two OSD reads/two setters, 702 word / 44 doubleword / 43,632 quadword / 15 byte writes. Current stack is initial SP minus `0x50`. Prior kernel payload/register/timer/thread assertions remain checked.

Verification: MSVC/full checkpoint pipeline, six CTests, 31 recomp tooling tests and ten capture-tool tests passed. Without OSD readback validation the first setter stop is preserved. UTF-8/proprietary ignore checks passed. No complete startup/gameplay. Next: automate discovery/mapping of the actually reached handler, regenerate, link and run to its next blocker.


## Reached copy handler discovered, generated and executed (2026-10-03)

Used existing `analyze_game.py --discover-blocker` at observed map-gap `0x0033A7C8`. Ghidra provisionally discovered a contiguous aligned 56-byte body ending at `0x0033A800`; the wrapper remains separate. Re-export selected 7,244 eligible EE candidates and quarantined 2,300; 9,544 total Ghidra functions. Rebuilt the automatic map and regenerated 7,247 raw main C++/header files; scoped kernel output adds one function/four files. All local maps/code remain ignored and neutrally named.

Linked/registers generated `FUN_0033A7C8` for the actual syscall handler. It shifts byte length right by two, loads a source word and reaches its first destination store. Native startup rejects store `0x80075000`, size 4, PC `0x0033A7E8`; request originally passed source `0x0036BAF8`, size `0x330`. At stop source has advanced to `0x0036BAFC`, word count is `0xCC`, loop index 1; destination has not advanced and no new kernel word was written. Reports preserve selector `0x5A`, handler and the actual attempted store word, rather than mislabeling the handler's overwritten v1 as a syscall selector or reporting the old timer write.

Added generated-copy-handler CTest: actual emitted function copies 12 and `0x330` bytes in bounded RAM, handles zero size, returns zero through the strict guest return path and leaves all other RAM unchanged. This validates these aligned non-overlapping cases, not general memcpy semantics or kernel execution. Startup still stops before its first store; no handwritten replacement/stub or broader kernel access was added.

Counts: 7,245 generated functions (7,244 main + one scoped kernel), 7,251 raw C++/header files; 39 entered and 35 completed across probes. The new handler completed only in isolated RAM tests and is partial during startup. Full startup records 65 guest calls, 11 custom callbacks, 60 JR returns, 11 registrations, 702 word / 44 doubleword / 43,632 quadword / 15 byte writes; old kernel overlay remains 464 words.

Verification: full checkpoint pipeline/MSVC build, seven CTests, 31 recomp tooling tests and ten capture-tool tests passed. Earlier first OSD setter stop remains checked without readback input. Next: bounded second kernel overlay for the observed payload, preserve captured baseline and existing compiled binding checks, then execute to the next blocker. No full startup/gameplay is claimed.


## Second bounded kernel payload copy completed (2026-10-04)

Added an explicit startup payload capability only for `[0x80075000,0x80075330)`, enabled from the validated local ELF source `[0x0036BAF8,0x0036BE28)` in the full OSD-readback probe. It requires the existing captured baseline and primary overlay, rejects overlapping/duplicate capabilities and wrong size, and checks each aligned store against the expected original word. Wrong RAM pointers, changed/duplicate stores, aliases and adjacent/unrelated kernel addresses remain rejected. Stored words use the existing separate overlay; captured kernel bytes are never mutated.

SetSyscall permits only the exact secondary entry after all 204 unique expected words have been written. Partial copies and interior entry registrations fail. Existing primary range and compiled-binding verification remain intact. Bootstrap tests check these rejection cases, read-through of copied words, adjacent baseline preservation and unchanged full baseline. No broad kernel mapping or host memcpy replacement was added.

Generated `FUN_0033A7C8` and its caller wrapper complete. Startup records FlushCache operations 0/2, registers `0x5B -> 0x80075000`, then registers `0x54 -> 0x0033AD40`. Newly linked `FUN_0033A800` reaches syscall `0x5B`, resume PC `0x0033A808`, lookup argument `0x55`; runtime stops before executing the unbound copied entry. All 204 words compare equal to their ELF source in the entry probe. The first payload and its six compiled dispatch checks remain validated.

Counts: 40 functions entered / 36 completed across probes; entry, startup parent, `FUN_0033A820` and `FUN_0033A800` remain partial. Full probe: 70 guest calls, 66 JR returns, 11 custom callbacks, 13 registrations; 668 kernel word writes (464 primary + 204 secondary), 906 total word writes, 44 doubleword / 43,632 quadword / 15 byte writes; four cache requests/two instruction-cache invalidations. Generation totals remain 7,245 functions and 7,251 raw files.

Verification: MSVC/full checkpoint pipeline, seven CTests, 31 recomp tooling tests and ten capture-tool tests passed. Omitting OSD readback still preserves the first setter stop. Proprietary payload/captures/generated code remain ignored. Next: scoped relocated generation and binding for actual reached entry `0x80075000`, preserving complete payload verification and prior kernel binding. No full boot/gameplay.


## Second copied kernel lookup generated and executed (2026-10-04)

Generalized the existing generator with `--payload startup` for source `0x0036BAF8`, destination `0x80075000`, full size `0x330`. Separate ignored raw/prepared directories, uniquely named headers and manifest prevent collisions with the first payload. Pipeline enables its CMake option only with full OSD-readback inputs and records both generation manifests.

Automatic recompiler discovery initially merged the lookup and neighboring routines through `0x80075158`, including unreachable TLB operations. Added a conservative contiguous leaf control-flow closure for the entry: follow direct conditional branches and their delay slots, stop paths at JR RA, reject escaping/indirect flow or control flow in delay slots. Tests cover closure exclusion of adjacent code and malformed/escaping paths. The scoped reachable interval is `[0x80075000,0x80075038)`; the complete unchanged payload remains available as data and binding evidence. This does not certify boundaries of neighboring functions or implement TLB operations. The first payload keeps its existing `[0x80076000,0x80076038)` scope.

Runtime binding accepts the secondary entry only with a payload identical to the explicit expected-word capability. Dispatch checks every one of the 204 copied words. Bootstrap tests reject wrong sizes/bytes/interior/duplicate bindings and incomplete copied words; successful synthetic binding verifies complete validation and return. Primary binding checks remain unchanged.

Generated `FUN_80075000` executes its table lookup, returns `0x80075038` for selector `0x55`, and `FUN_0033A800` returns. SetSyscall then requests `0x55 -> 0x80075038`; registration fails at wrapper resume PC `0x0033A818`, before registration or execution, because the secondary capability currently permits only its exact entry. Blocker address selection now handles `access: handler registration` explicitly and classifies the handler, shared by Ghidra discovery tooling; memory read/write stops still classify their executing PC. Kernel targets remain separate from the original ELF map.

Counts: 7,246 generated functions (7,244 main + two scoped kernel entries), 7,255 raw C++/header files; 41 entered / 38 completed across probes. A previously completed SetSyscall wrapper is partial on its current invocation. Full probe: 72 guest calls, 68 JR returns, 12 custom callbacks, 13 completed registrations, seven compiled kernel payload validations; write/cache counts remain 906 words including 668 kernel words, 44 doublewords, 43,632 quadwords, 15 bytes and four cache requests/two invalidations.

Verification: full checkpoint pipeline/MSVC build, seven CTests, 34 recomp tooling tests and ten capture-tool tests passed. Earlier optional-input boundaries remain preserved. All payloads, maps, raw/prepared generated code, packaged ELF and diagnostics remain ignored. Next: bounded registration of lookup-returned interior entries, then translate only code actually reached. No complete boot/gameplay.


## 2026-10-04 - Platform initialization continuation

Exact copied-table selector/address registration allows generated startup to finish; registration alone does not allow execution of uncompiled kernel handlers. Generated cleanup and entry continuation reach the unlinked FUN_00100388 call at 0x00100238. EI at 0x00100228 sets modeled Status 0x10000; interrupt delivery remains unimplemented. Arguments are a0=0 and a1=0x0038B984. The routine's purpose remains unknown. See [detailed pass evidence](../progress/2026-10.md#bounded-platform-initialization-returned-to-entry-2026-10-04).


## 2026-10-04 - First helper chain returned; DMA boundary

Generated FUN_00321D90/FUN_00322F90 use semaphore 1 in a nonblocking WaitSema/SignalSema pair; bounded semantics and pinned ABI reference are recorded in config/startup_abi.sources.json. FUN_003242D0 returns through the helper tail chain to FUN_00100388. Newly reached FUN_00331FE8/FUN_00331888 execute generated RAM initialization and status/cache helpers, then read 0x1000E010 at 0x003319CC. PS2SDK ee_regs.h names this register D_STAT; its actual initial value and required DMA behavior remain unknown. No semantic function renaming, register default or DMA transfer is inferred. See [pass evidence](../progress/2026-10.md#multiple-execution-blockers-resolved-first-dma-status-read-reached-2026-10-04).


## 2026-10-04 - First command receive effects and pending RPC bind

Pinned PS2SDK headers define command 0x80000000 as CHANGE_SADDR, 0x80000002 as INIT_CMD (also the separate RPCINIT software register selector), and 0x80000009 as RPC_BIND. Captured first-query value 0x19600 selects the already initialized EE startup branch. Paired EE/IOP captures show the first 20-byte packet consumed at IOP 0x19600 and the field at 0x196C8 changing from 0x93640 to 0x3A94C0, matching the public CHANGE_SADDR handler effect. The field interpretation is supported by matching buffer/table structure and before/after evidence; no retail IOP function is renamed or executed by the native model.

Generated code later reads RPCINIT selector 0x80000002; a separate capture returns 1. The initialization helper returns, then the generated next path uses PS2SDK's 0x20000000 uncached main-RAM alias, allocates a packet and prepares RPC_BIND for server ID 0x80000592. The server's identity/purpose, RPC response, timing and EE delivery path remain unknown/unimplemented. See [detailed pass evidence](../progress/2026-10.md#first-sif-command-received-generated-initialization-returned-rpc-bind-reached-2026-10-04).

## 2026-10-04 - CD initialization RPC bind and first call

Public pinned PS2SDK registration identifies server ID `0x80000592` as CD initialization. Local captures confirm its registered IOP queue, server `0x0003F570`, buffer `0x0003FB20`, RPC_END bind response and completed EE client. A bounded logical response now invokes the original generated handler while preserving caller CPU/stack; binding, its semaphore wait and deletion return natively. This does not execute the IOP service callback or emulate hardware interrupt timing.

The next generated path constructs RPC_CALL `0x8000000A`, function number 0, four-byte input and sixteen-byte reply; its third SifSetDma remains unsupported. Public synchronous RPC leaves its semaphore attr uninitialized. A local checkpoint confirms successful creation with max 1/init 0/option `0x003838E8`; the native residual attr differs from the BIOS capture. The unused field is permitted only for this exact checkpoint-gated call, with no invented stack clearing or broader semaphore attributes. See the [detailed history](../progress/2026-10.md) and [current blocker](../PROG.md).
