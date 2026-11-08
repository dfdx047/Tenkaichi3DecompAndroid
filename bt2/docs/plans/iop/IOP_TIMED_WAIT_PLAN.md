# Timed IOP wait and active RPC context scope

Status: bounded timed wait, priority dispatch, alarm callback and query return transport implemented. General cross-CPU scheduling remains incomplete.

## Reached evidence

The real initial MCSERV query executes six absent-card DMA pairs and stops at thbase ordinal 33 (DelayThread), import PC 0x31B40, argument 300 microseconds. The local MCMAN caller at 0x29E50 sets 300 in the delay slot; retain its neutral name. Thirteen modules are resident, twelve workers wait and a priority-24 sound helper is already ready. MCSERV worker priority is 104; SIO2 worker priority is 16.

An independent isolated retail flow, with both card slots disabled, passes this wait and reaches secondary SIF submission to 0x3A7480, 192 bytes, flags 0. That establishes a later path, not native scheduling equivalence or a replacement payload. Captures/generated modules remain ignored.

## Required design/evidence before a timed provider

- Consult REFERENCE_ARCHITECTURE.md and pinned BT3 cooperative scheduler/IOP ownership interfaces plus public PS2SDK DelayThread/thread contracts. Adapt the architecture; do not copy implementation or reference game behavior.
- Preserve the active borrowed RPC CPU/GP/SP, queue ownership and pending EE request across suspension. executeRegisteredRpc currently owns a local CPU and treats exceptions as a failed service; a timed yield must be distinguished from failure and retain the actual handler continuation.
- Establish owned logical time units and a single deadline for this 300-microsecond request. Capture pre/post wait and actual caller return/context where needed. Do not claim wall-clock or cycle accuracy from a logical clock.
- Dispatch eligible ready workers by established priority while the caller waits. Inspect the existing sound helper's actual execution blocker rather than skipping it or assuming it cannot affect startup.
- Resume the exact suspended handler once its deadline is reached. Preserve interrupt state, stack ownership and nested event/preemption contexts; reject unrelated delays until evidenced.
- Then follow secondary SIF descriptor/poll, copy actual native stack bytes and deliver actual primary RPC response and generated FUN_0031DD60 callback. Discover the callback from execution/capture provenance when needed.

## Verification gates

Use synthetic suspend/resume and priority-order cases with observable different continuations; reject duplicate wake, wrong owner/deadline/epoch and failed-context retry. Confirm the real query's owned stack/queue and pending EE request survive, and only one timer wake occurs. Compare actual final result/transport with an independently captured explicit absent-card profile, without equating unrequested stack contents or physical scheduling.

An immediate-success DelayThread stub is not an implementation of this scope. Keep the pending boundary until context retention and eligible worker dispatch are supported.

## Implemented bounded scope (2026-10-05)

The reached caller remains live on the host stack, including nested native exports and the borrowed RPC frame. Separate ready worker contexts dispatch by priority before logical idle advance. Five 300-microsecond delays execute; the priority-24 SDRDRV helper runs once and parks on an owned outgoing bind to EE service 0x80000704, currently unregistered. Its request/client/continuation are retained; no server pointer, successful return or reply is invented.

The real absent-card FIFO path sends `81 52 00` five times and receives three FF bytes each time. USec2SysClock converts 100 to [3686,0]. Five alarms register; four native callbacks signal the actual IOP semaphore. Logical time reaches 1900 microseconds; the fifth alarm remains pending at 2000. This is logical scheduling, not cycle/wall-clock equivalence. Retaining this alarm does not establish later recurring timer support.

The real MCSERV handler submits its owned 192-byte worker-stack payload to EE 0x3A7480 and polls its logical completion once. The primary result is -11, independently matched at retail callback entry with both card slots disabled. Generated RPC_END and neutral FUN_0031DD60 execute; the latter signals the EE completion semaphore. Undefined secondary stack fields are not equated with retail.

Synthetic priority/context retention and failure cases, native service lifecycle/endpoint checks, and independent delay/alarm/FIFO/flow/callback captures verify the bounded implementation. Capture sources and proprietary data stay ignored. Diagnostic binding against an already registered retail EE service is separate evidence; it is not substituted for the native pending request.

## Remaining scheduler boundary

The pending reverse bind requires actual EE server registration and command delivery before the sound helper may resume. General EE child dispatch, cross-CPU RPC routing, later alarm dispatch/cancellation and multiple simultaneous timed waits are not supplied by this scope. Follow their actual blockers and obtain evidence before extending it.
