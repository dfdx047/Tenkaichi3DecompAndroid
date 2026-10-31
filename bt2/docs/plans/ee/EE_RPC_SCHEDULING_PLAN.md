# Queued EE/IOP RPC completion and persistent dispatch

## Why this is the next architecture scope

Normal startup stops at main ChangeThreadPriority(1, 24). The retained sound-worker diagnostic proves generated registration, both first waits and the actual reverse bind. It does not reproduce original ordering: the original EE child first-wait capture still carries SOUNDS bind RPC ID 29, whereas main-priority-before already shows reverse-bind completion. Our immediate EE-to-IOP response adapters prevented that earlier EE dispatch.

A live scheduler cannot be enabled by moving the diagnostic's successful effects to the final priority syscall. It must retain guest stacks across RPC blocking, own the queued completion, and schedule the eligible child before resuming the original wait. This changes the transport/scheduler architecture, rather than supplying another syscall result.

## Evidence already available

- Generated sound registration/loop and captured EE first wait: queue 0x3A6D00, owner 3, server 0x3A6D18, service 0x80000704, handler 0x3153F0, buffer 0x3A6CC0.
- Actual blocking IOP BindRpc input/return/client and first SleepThread caller; native helper resumes at its import once and parks at 0xA1304.
- Original first-wait outgoing request: command 0x80000009, RPC ID 29, client 0x6351C0, SOUNDS service 0x02000004. This is phase evidence, not exact hardware interrupt timing.
- Four helper identities/priorities/reset contexts; helpers 5/6 remain ready and unexecuted.
- Windows stack retention/resume, cancellation unwind, failures and nested ownership tests.

## Required bounded design and implementation

1. Trace the reached generated SOUNDS bind send/return/semaphore-wait path against the existing captures. Identify the actual host admission point and guest continuation; retain a queued request identity and completion ownership. Do not invent packet contents or delay successful callbacks arbitrarily.
2. Move guest invocation return/depth/exit state into persistent per-thread ownership. Retain main and sound stacks across dispatch, not just within a terminal diagnostic. Reclaim parked stacks by cancellation while their return ownership is selected; never DeleteFiber over live C++ guards.
3. Separate RPC submission, native IOP work, EE reply/callback delivery and semaphore wake. Preserve existing checked epoch/client/buffer contracts and synchronous service tests. A waiting main must not hold a transport stack that prevents the required child from running.
4. Dispatch the sound child at the earlier eligible blocked-main phase. Its generated registration admits the owned pending reverse bind. Keep its actual first sleep parked; the IOP helper then sleeps independently. Do not fabricate a callback command or wake it without an owned request.
5. Run the original main wait/cleanup continuation once. Compare registration phase, bind return/client, preserved main context and return accounting. Then follow the final priority transition through eligible helpers 5/6; their executed behavior is still unknown. Capture their first actual waits or effects before supplying them.
6. Verify failure/cancellation cleanup, stale/duplicate requests, per-thread return isolation and single wake ownership. Run the normal full startup checkpoint and all existing comparisons, plus the new diagnostic comparison. Only promote counts/status when normal startup advances.

## Limits and stop conditions

Do not use captured queue RAM as native registration, acknowledge priority 24 while skipping ready workers, replay main/child entries on resume, count parked functions as returned, or claim a completed command callback from a bind. The callback-entry observation timed out; handler payload/lifetime and later wake ownership remain unknown. Stop at a genuinely new reached subsystem, unavailable required evidence/access, or a speculative change after using available captures/tooling/references.

## Verified queued completion and next priority work

Normal startup now verifies queued SOUNDS completion, retained generated sound ownership and the original IOP reverse bind/first sleep. Main priority 24 is still pending. The separate helper diagnostic reaches priority request (5,1). Next implement main priority admission before helper dispatch, retain previous priority 2 for the API return, then own helper critical priority 1 and restores 16/18. Helper first-wait guards are provisional; helper 6 mapping remains pending. RAM flag 0x34BC84 and conditional BGCOLOR store 0x120000E0 must not be treated as CSR timing evidence.

## Priority admission completed

Main's priority 24 is admitted with previous priority 2 returned. Helpers 5/6 run through their observed critical/restore sequences to the captured first waits, and main resumes. This plan's scope is complete. Interrupt delivery, wakes and the first asynchronous request continue in [EE_INTERRUPT_LOAD_PLAN.md](EE_INTERRUPT_LOAD_PLAN.md).
