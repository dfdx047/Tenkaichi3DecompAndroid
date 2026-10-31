# CRI nested IOP thread-start boundary

## Observed boundary

Original CRI command 0x40C accepts twelve bytes `[0xBB80, 0xFFFF, 0xFFFF]` and returns twelve bytes in retail. The subsequent captured 0x422 request observes its result `[0, 1, 0]`. Native original-handler execution reaches thbase ordinal 6 (StartThread) at import PC 0x1745F0. Its borrowed CRI RPC worker has priority 48. The isolated original-handler test creates child ID 0x2000E; the full probe uses ID 0x2000F. The child has entry 0x16D890 (CRI_ADXI +0x5890), GP 0x17DDF0, stack 0x1E6E00, size 0x800, priority 39. Unknown child purpose remains unassigned.

The runtime rejects higher-priority nested StartThread before initializing or dispatching this child. The original handler has already performed real work; the failed service cannot be retried. No 0x40C response, generated RPC_END completion or native audio is substituted. The existing MCSERV/SIO2 event-preemption path and timed-wait scope do not establish this new child-start/CRI worker lifecycle.

## Design/evidence needed

- Consult REFERENCE_ARCHITECTURE.md, pinned BT3 scheduler/worker ownership patterns and public PS2SDK StartThread semantics. Adapt the existing retained-parent approach, without copying a scheduler or assuming CRI behavior.
- Capture the original child start arguments, actual entry/GP/priority, first blocking API and original parent return. The later retail result establishes completion but not the intervening child lifecycle.
- Define priority dispatch and preserve the active borrowed parent CPU, stack and RPC queue while the child runs. Restore current-thread ownership on yield and failure; distinguish a supported child wait from a failed service.
- Trace the child's bounded initialization to its first wait, including actual imports, callbacks and any SPU2/timer or further thread dependencies. Reject unrelated preemption until evidenced.
- Verify suspension/return ownership with distinct synthetic parent/child continuations and independently captured retail effects. Then complete original 0x40C result transport and follow 0x422, its local buffer ownership and subsequent requests.

This newly reached scheduling boundary requires its own implementation design. General EE/IOP scheduling, audio playback, stream payload loading and presentation remain incomplete.


## Verified bounded child start (2026-10-06)

Original StartThread takes argument 0. The higher-priority child reaches WaitSema at retail import 0xC456C, caller RA 0xBD800, GP 0xCDD20. StartThread returns 0, with parent GP and SP unchanged. Three isolated profiles publish the original API/wait context before later DMA setup; no kernel return or RPC completion is injected.

Adapted retained-parent priority dispatch from the existing bounded event-preemption pattern. The child executes original translated code until its zero-count semaphore wait, then keeps its exact import continuation (native PC 0x17463C / RA 0x16D8D0). Parent current-thread/CPU ownership is restored on yield and exception. Unrelated higher-priority starts remain rejected. Synthetic tests distinguish parent/child continuations, validate the actual wait provider and reject an unsupported priority without starting the child.

The full probe child (0x2000F) waits on native semaphore 0x30008; kernel handle equality with retail is not claimed. Original 0x40C continues into LIBSD. Nested CpuSuspendIntr stores prior state 0 and returns public KE_CPUDI (-102), while inner CpuResumeIntr(0) leaves interrupts disabled. Existing retained ADMAS parameter reads and the independently captured initial CHCR=0 are now supported. The new boundary is [first AutoDMA enable](CRI_AUTODMA_PLAN.md); 0x40C still has no native completion.
