# DBCMAN EE request scope

## Reached boundary

After valid EE sound thread start, owned SOUNDS bind and native command 0x10, FUN_0028BCF8 reaches FUN_003131B8. Its first bind requests service **0x80001300**, client **0x3A03C0**, RPC ID **31**. The current checked stop is CreateSema 0x40 at 0x32EC68, descriptor at initial SP minus 400. It asks max 1 / init 0 / option 0x3838D8; attr contains prior stack data 0x203A97C0, consistent with the synchronous RPC helper's unused attr field. No DBCMAN client request completes natively.

Native DBCMAN registers four owned services: 0x80001300, 0x8000131C, 0x8000131E and 0x8000131F. First owned descriptor 0x62DE0, buffer 0x62F08, handler 0x61C88, queue 0x62D80 and GP 0x6A720 belong to its existing parked priority-20 worker. The new probe inventory reports registrations without asserting command support.

Independent retail first-bind return at **0x313254** publishes return 0 / SP 0x1FFEC40. Client [0,31,7,0,0,0x7AB38,0,0,0,0x7AA10] resolves the actual retail registered server. These captures stay ignored. FUN_003131B8 also binds 0x8000131C at return 0x3132F4; its later paths still need evidence.

## Bounded next pass

1. Consult REFERENCE_ARCHITECTURE.md and pinned public SIFRPC/module contracts. Trace this actual local module's command dispatch and callbacks without guessing DBCMAN's purpose or copying another game's implementation.
2. Capture/validate the related bind and first command group. Adapt the owned registration mapping, generated RPC_END and semaphore cleanup together, rather than ending after the first bind.
3. Deliver only reached command shapes to actual translated handlers on their retained GP/stack. Record cross-space destination ownership, callbacks, allocations and worker waits. Resolve the bounded initialization/request group using local before/after evidence.
4. Establish any requested hardware/service, EE callback or scheduling transition before supplying a result. Resident entry and initial SIO2 startup polls do not validate DBCMAN requests or later device traffic.
5. Continue through the group toward the next real loading/rendering blocker. Keep the existing sound-ready state and waiters intact. Reject stale epochs, unknown services and unvalidated replies.

This is a new EE/IOP command-protocol scope; the existing SIO2 startup plan does not implement these requests. No device capability, controller input, file/asset loading, sound playback or physical timing is inferred from the service IDs.

## Verified request group and new transport boundary

Four binds now use actual native registered servers/parked workers and generated RPC_END/wait/delete. Actual clients are 0x3A03C0, **0x3A0410**, 0x3A0438 and 0x3A0460, with IDs 31/33/34/35; command 0x80001363 uses ID 32 between the first two binds. This corrects the initial expectation of consecutive bind IDs. The real handler executes 54 IOP instructions and returns [0x310, zero remainder] over 144 bytes. Independent guarded captures validate request/input/result, registered-server mapping, relocated code and metadata.

The persistent initialization semaphore has max 127 / initial 1 / option 0x381C28. Reached generated code does not initialize its unused attr; native stack residue differs from retail. The adapter admits that field only at the exact observed group/address/count/option scope.

Second command 0x80001304 / ID 36 uses temporary semaphore 8 and stages EE destination 0x3A27C0. Its translated handler retains that address, then stops at sceSifSetDma before completing. Retail result 1 is evidence for the future completed path, not supplied success. Follow [reverse DMA scope](DBCMAN_DMA_PLAN.md); no device purpose is inferred. The checked failed native service rejects retry without additional mutation.

## Completed bounded request group

Four owned binds and four commands now finish: 0x80001363, 0x80001304 and two 0x80001301 requests. See DBCMAN_DMA_PLAN.md for source-ownership limits and independent related-request evidence. Persistent lock waits/signals are counted separately from RPC cleanup. This does not validate input device behavior. Execution next reaches MCSERV 0x80000400 / client 0x3A6F80 / ID 39.
