# EE sound worker dispatch and reverse RPC ownership

## Confirmed next scheduling scope

Independent original creation/start captures now establish all four helpers between main status and the final priority request. The previously proposed native IDs 4/5 for the last two workers were wrong: two earlier helper lifecycles had not been traced. Neutral entries remain unnamed.

| ID | Entry | Stack / size | Create return | Priority return / requested priority |
| --- | --- | --- | --- | --- |
| 4 | 0x2EDE58 | 0x34BC90 / 0x800 | 0x2EE51C | 0x2EE564 / 8 |
| 5 | 0x2EDF48 | 0x34D490 / 0x1000 | 0x2EE684 | 0x2EE6A8 / 16 |
| 6 | 0x2EE050 | 0x34E490 / 0x1000 | 0x2EE70C | 0x2EE730 / 18 |
| 7 | 0x2EE140 | 0x34F490 / 0x2000 | 0x2EE794 | 0x2EE7B8 / 25 |

All four captured initial priorities are 2, GP is 0x38BDF0 and reset RA is the owned SetupThread exit 0x100250. Capture labels worker4/worker5 are retained for the last two historical observer points; they refer to original IDs 6/7. Do not infer identity from labels alone. The first helper additionally queries ready status 2 at 0x2EE494, then **SuspendThread selector 0x37** returns ID 4 at 0x2EE4B4. This is suspension, not a wakeup; its priority changes while it is suspended. The last helper (ID 7) also queries ready status 2 after priority 25 is assigned, then selector 0x37 returns 7. Selective observers preserve the earlier shared helper call and publish the final helper only.

After second CRI setup and generated main-thread initialization, the main thread queries its status and requests ChangeThreadPriority(1, 24) from 0x2EEA2C. Independent original before/after captures show configuration priority 0x18 and return 2 (previous priority). The native main priority is 2; ready EE thread 3 has priority 5, entry 0x315380, GP 0x38BDF0, SP 0x3A6CA0 and owned exit 0x100250. Acknowledging the priority change without dispatching that higher-priority ready child would omit required scheduling.

Original captures already contain queue 0x3A6D00, thread owner 3, server descriptor 0x3A6D18, service 0x80000704, handler 0x3153F0 and buffer 0x3A6CC0. The native SDRDRV helper is waiting for that reverse bind. Generated child code calls SifInitRpc(0), GetThreadId, queue/server registration and tails into 0x333060. The current synchronous EE-to-IOP adapter has never executed this ready child, so original asynchronous startup ordering is not established natively.

## Bounded implementation required

1. Consult REFERENCE_ARCHITECTURE.md, EE_THREAD_START_PLAN.md and SDRDRV_RPC_PLAN.md. Keep original game entries and reference scheduler patterns separate.
2. Original child registration at 0x3153DC and its first SleepThread call at 0x333090 are now captured. At the wait, SP is 0x3A6C80, GP 0x38BDF0, s0/queue 0x3A6D00, saved exit 0x100250 and original pre-observer Status 0x70030C11. The observer publishes registers before terminal DI/SYNC to prevent preemption; it never resumes the game. Queue owner 3 and service descriptor 0x3A6D18 / 0x80000704 / 0x3153F0 / 0x3A6CC0 agree. These snapshots do not establish actual IOP-to-EE bind delivery or a full callback lifecycle; capture those before admitting them.
3. Define a retained main invocation and a separate owned child CPU. Preserve main registers, stack, call-return validation and requested priority until the child reaches its evidenced wait. Do not replay the child's entry on later resumes or replace blocking code with a successful return.
4. Execute the original generated queue/server/loop path, keep waiting-thread ownership and establish the pending IOP-to-EE bind response using registered EE state. Trace subsequent actual callbacks before admitting them. Capture and compare context preservation and return to the main continuation.
5. Follow the newly reached related blockers through the bounded subsystem, then test, update progress, commit and push. Archive payload loading, presentation and audio consumption remain separate claims.

The bounded worker group passes native execution, ownership tests and capture comparison. The main-priority handoff is now the actual reached boundary. Implementing the handoff without first-wait and cross-CPU ownership evidence would be speculative. The earlier thread-start plan establishes ready state; this scope adds actual worker execution and reverse RPC service lifetime.


## Required scheduler design gate

The current EE dispatcher uses synchronous C++ invocations and shared return validation. Starting a different CPU at the priority syscall must not erase or replay the active main invocation, lose its guest continuation or treat a child SleepThread as a successful function return. Define explicit per-thread execution/continuation ownership, suspended versus ready versus waiting states and the pending reverse-bind completion transition. First-wait RAM alone cannot substitute for owned queue registration, wait delivery or preserved parent context. The current header's four-worker group provides bounded ready/suspended records, not this scheduler.

## Diagnostic execution verified (2026-10-06)

A Windows retained stack now preserves native guest frames across yield and resumes only for explicit diagnostic cancellation. Separate child return validation/depth/exit ownership leave the main invocation intact. Generated entry 0x315380, queue registration 0x332BB0, server registration 0x332C48, loop 0x333060, request inspection 0x332E40 and SleepThread wrapper 0x32EB80 execute to first wait. Child GP/SP/s0/saved exit and registration match independent original captures. Sleep is not counted as a successful return.

A separate actual SDRDRV blocking BindRpc observation confirms input [0x8A2D0, 0x80000704, 0], result 0 and client buffer/server 0x3A6CC0 / 0x3A6D18. A second observer lets that original API return and captures the first SleepThread call (RA module+0xF24, SP 64 bytes below bind caller). The native pending request resumes once through the real translated helper and parks at 0xA1304 / RA 0xA0F24. Unknown/stale registrations, wrong epoch, malformed request and duplicate completion are rejected.

The diagnostic has 302 entered / 295 completed EE entries and 81,641 IOP instructions; normal startup remains 296 / 292. No live EE wake or command callback completion is supplied. The EE stack is deliberately cancelled at the report boundary; owned ready-thread records are not promoted to a general waiting/scheduler model.

The original first-wait capture's shared outgoing request is the SOUNDS bind, RPC ID 29, client 0x6351C0 and service 0x02000004. Main-priority-before already contains the completed IOP reverse-bind client. Therefore late dispatch at the final priority request is validation tooling, not the original scheduling order. Follow [queued RPC scheduling](EE_RPC_SCHEDULING_PLAN.md) before enabling normal startup dispatch. The terminal callback-entry capture did not reach its marker in the isolated run; no callback payload or lifecycle is inferred.
