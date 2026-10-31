# CRI RPC subsystem boundary

## Observed handoff

After the owned directory-sector read, generated parsing, five listed file lookups and further generated initialization, EE execution submits a bind at selector 0x77 / PC 0x32F028. RPC ID is 52, client 0x355C70, service 0x90000200. No native bind response or command completion is supplied.

The resident CRI_ADXI module already owns that server. Existing validation checks its child RPC worker (entry +0x7D4, priority 48, 0x800-byte stack), queue +0xED80, descriptor +0xED30, original handler +0x720, buffer +0xE7C0 and module GP. Its parent waits for Vblank-end. These registrations establish ownership, not request execution, streaming or native audio playback.

## Design and evidence required

- Capture the original EE bind/semaphore/result and identify the first actual CRI command, input/output extents and completion mode. Verify retail module code, GP and worker/server ownership.
- Consult REFERENCE_ARCHITECTURE.md and its pinned BT3 runtime/IOP routing patterns. Adapt the retained native worker pattern; do not copy game-specific implementations or assume command semantics.
- Define request lifecycle, buffer ownership and synchronization with the parked parent/worker. Inspect actual reached imports before deciding whether existing kernel providers suffice or streaming/timer/SPU2 behavior is required.
- Execute the original translated handler for a bounded observed command; validate primary/secondary transfers and generated EE cleanup. Preserve unknown function names and reject unsupported commands or completion conditions.
- Follow subsequent blockers within the bounded subsystem. Keep stream contents, captures and generated code ignored. Test, update PROG.md/history, commit and push at the end of the pass.

The command protocol and required streaming/scheduling scope remain unknown. Do not return successful initialization merely because a server was registered. The completed disc scope is documented in DISC_COMMAND_PLAN.md.


## Verified bind/allocation pass (2026-10-06)

Independent absent-card captures establish semaphore 10, bind return 0, registered retail server/worker, command 2 inputs `[0, 0x365BC0, 0x14FD40, 0x880]`, 16 input bytes and four output bytes. Native bind returns actual owned registration addresses, runs original generated RPC_END, and preserves interrupted CPU/stack state.

The preceding third heap request is operation 1 on the existing FILEIO heap service, size 0x8D0, result 0x14FD00, rounded extent ending at 0x150600. The authored SOUNDS placement at 0x150000 conflicted. Regenerated SOUNDS at 0x1C0000 and expanded the layout inventory to include auxiliary CDVDFSV, the third heap and disc workspace/buffer. Retail heap addresses are retained. Reproduction:

```powershell
python tools/iop/generate_native.py rom/IRX/SOUNDS.IRX --disc "rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso" --base 0x1C0000 --output work/iop/sounds_native.cpp --symbol bt2_iop_sounds --prefix iop_sounds
python tools/iop/verify_layout.py
```

The real translated CRI handler executes on its retained worker GP/stack and returns slot 0x1768C0 (retail 0xC67F0). Stable slot fields and relocated internal pointers match independent evidence; generated EE request and cleanup complete. Unknown fields are not assigned semantic names.

The next command 0x403 has zero input/result bytes and returns normally in retail and the isolated native original-handler test. Its full EE path is not yet validated: generated cache maintenance intervenes before submission. The authored adapter remains strictly gated and its full transport awaits that cache contract. A third retail capture reaches command 0x40C with twelve input/output bytes and inputs `[0xBB80, 0xFFFF, 0xFFFF]`; no meaning or native completion is inferred.

Full checkpoint: 2381 calls / 2374 returns, 43416 IOP instructions / 172 targets. See [EE cache scope](../ee/EE_CACHE_TRANSFER_PLAN.md) for the new major subsystem boundary. No stream payload or native audio playback is claimed.


## Cache/control completion and next scheduling boundary (2026-10-06)

Both reached cache helpers now execute after independent both-way TagLo/caller validation. Full command 0x403 now runs its original handler and generated RPC_END cleanup; its native completion count advances to two CRI calls. Additional reached entries and checked generated constant halfword reads carry execution to command 0x40C, with the independently captured twelve-byte input.

The full native 0x40C path transfers the original input and executes the real handler, then stops at thbase:6 / 0x1745F0. A priority-39 child (entry 0x16D890, GP 0x17DDF0, stack 0x1E6E00/0x800) would preempt its priority-48 borrowed CRI worker. Explicit rejection leaves completion count at two; failed native services reject retry. The isolated original-handler test verifies this same boundary and preflight rejection of wrong epoch, command, size and input. It does not report 0x40C success.

A fourth retail request captures 0x422, twelve input bytes `[1, 0x150640, 0x100]`, four result bytes, caller return 0x2FD1F8. It observes the preceding 0x40C result `[0, 1, 0]`; no native return or meaning is substituted. Follow [the new child-start scope](CRI_THREAD_START_PLAN.md) before dispatching this worker.

The fourteen earlier-scope evidence validators run against ignored `cache-completed.json`, retained at the next command's semaphore boundary with no IOP failure. A separate `verify_cri_preemption.py` validates the latest failing full checkpoint. This keeps completed cache/0x403 claims separate from the unsupported scheduler transition.


## CRI child start and AutoDMA handoff (2026-10-06)

The bounded 0x40C child start now completes its original first WaitSema and restores the RPC parent. Native execution continues through nested interrupt handling and captured initial DMA control/ADMAS reads, then rejects ADMAS enable (halfword 1 at 0x1F9001B0, PC 0x92DB8). No 0x40C result or generated cleanup is substituted. Follow CRI_AUTODMA_PLAN.md; earlier commands 2/0x403 remain complete.


## Completed first-channel constructor group (2026-10-06)

The full generated EE path completes 35 CRI calls: initial command 2, 0x403, 0x40C and sixteen 0x422/0x400 pairs. Exact input buffers, phase ordering and owned original result pointers are gated. The initial AutoDMA half remains pending. The next fifth heap request is rejected before dispatch because its captured interval overlaps FILEIO; follow CRI_SECOND_CHANNEL_PLAN.md.
