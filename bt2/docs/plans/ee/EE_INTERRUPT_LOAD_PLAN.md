# VBlank delivery, thread wakes and the first asynchronous load

## Why this is a new subsystem

Normal startup now admits main's ChangeThreadPriority(1, 24), runs helpers 5/6 to their first SleepThread, and resumes main. Main registers a VBLANK_START handler (INTC cause 2, handler 0x1006D8, `next=-1`), enables cause 2, starts an asynchronous request and enters a poll loop:

```text
0x1007F8  status = poll(handle)          ; 0x2EAFA8 -> 0x2EB030
0x100800  if status != done:
0x1007F0      idle()                     ; 0x2EF218 -> 0x30A770
```

The idle path services a 32-slot EE file-handle table (0x357110, stride 568) under the same critical section the helpers use. An unbounded run made no IOP request and no handle progress across the execution budget. On hardware, this loop is interleaved with VBlank interrupts; handler 0x1006D8 calls 0x2EF200 → 0x2EF0D8, whose effect (wake, semaphore signal or state update) is not yet traced. The probe therefore stops at a checked boundary: main's first idle call from 0x1007F0.

Interrupt delivery, timed wakes and a wake-capable scheduler did not exist before this boundary. They are not a further syscall result.

## Evidence already available

- Helper critical-section protocol: lock 0x2EDC10 / unlock 0x2EDC98; global depth 0x34BBA0; saved priority/ID 0x353490/0x353494; ResumeThread/SuspendThread of ID 4 around the body. Original captures confirm both helpers' requests, returns (16/18) and first waits.
- Helper 5 fills and copies CRI stream buffers 0x365BC0 → IOP 0x14FD40 and 0x35FD00 → IOP 0x191740 (0x880 bytes, attribute 0). The 0x40-byte trailer carries an EE sequence counter at +0x3C. The IOP consumer and acknowledgement path are not modeled.
- INTC checkpoint mask 0x1802; cause 2 was disabled before main enabled it.
- Thread state: 3 (sound), 5 and 6 sleeping; 4 and 7 suspended; main at 24.

## Bounded design and steps

1. Trace the reached request: identify the handle object, its state transitions and the code that advances it (0x300CD0 / 0x300B00 / 0x300BC8 / 0x3009D8). Determine whether progress requires an IOP request, a helper wake or VBlank work. Capture original handle state before/after the first idle call, and at the first VBlank handler entry, using the existing PCSX2 observer tooling.
2. Trace handler 0x1006D8 → 0x2EF0D8. Record the exact kernel calls (expected i-prefixed wake/signal variants) and which thread or semaphore they target.
3. Implement owned interrupt delivery: a logical VBlank source raised only at explicit host scheduling points (main idle/blocking calls), dispatching registered cause-2 handlers in chain order on a separate guest invocation with the caller's GP. Interrupt-context syscalls must be distinguished from thread context. No timing fidelity is claimed; ordering is evidence-checked.
4. Implement wakes for parked owned stacks: WakeupThread/iWakeupThread (and semaphore signals if reached) mark the target ready. Resume its retained stack at the next scheduling point when it outranks the current thread. SleepThread must then return normally (wakeup counts honored). Preemption remains fail-closed outside those points.
5. Follow the first helper wake through its loop (CRI stream refill, IOP consumer acknowledgement) and the load request through any IOP RPC it issues. Compare against captures before promoting counts.
6. Verify: unit tests for interrupt chain order, mask gating, wake/sleep counts, priority resumption and cancellation; normal startup and all applicable comparisons; a new helper-wake/handler comparison.

## Limits and stop conditions

Do not fabricate request completion, handle states, CRI acknowledgements or IOP data. Do not run interrupts on a free-running host timer. Do not resume a parked worker without an owned wake. Stop at a further new subsystem (for example, real archive decompression or GS presentation), when required evidence cannot be captured, or when the next change would be speculative.

## Outcome (2026-10-07)

Steps 1–5 are implemented and verified for the first request. The first VBlank is delivered at the cause-2 enable, matching original phase evidence. Helpers 5/6 wake and re-park, and one logical VBlank per idle call drives the CRI file system through original CDVDFSV readiness, read and error-query handlers. The loaded archive table is byte-identical to the original. IOP stream consumer acknowledgement remains open. Normal startup continues to the next CRI command service (see PROG.md).
