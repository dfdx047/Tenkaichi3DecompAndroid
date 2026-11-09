# EE thread start and sound callback scope

## Confirmed boundary

After the first real SDRDRV command, generated FUN_00315148 creates owned dormant EE thread ID 3: entry 0x315380, stack 0x3A5CC0, size 0x1000, GP 0x38BDF0, priority 5. It calls FUN_0032FDA0 at 0x315210 with ID 3 and argument zero. This was the previous checked boundary; the bounded transition now completes, as recorded below.

Independent before/after retail captures publish CP0 Status **0x70030C11** and successful return **3** at 0x315218. The native context starts with Status zero; generated EI leaves 0x10000 at the reached boundary, still without IE bit 0. FUN_0032F730 tests its interrupt-enable bit, so the earlier exploratory run reported failure and continued down a different error path. Those exploratory downstream call counts are invalid as a boot-progress measure. The authoritative checkpoint is 1,007 calls / 1,003 returns.

SDRDRV also starts an IOP helper at module+0xFF0, priority 24, stack 0x800. It is ready natively and running in retail. Its generated path includes an outbound bind to service 0x80000704. Registration, invocation and completion of the corresponding EE callback are still unknown.

## Required design and evidence

- Consult REFERENCE_ARCHITECTURE.md and pinned public EE kernel/thread and SIFRPC contracts. Establish ownership of CP0 Status, interrupt transitions, saved EE contexts and thread state without copying retail kernel records or reference implementations.
- Trace generated StartThread through its status/thread query and actual syscall; compare the existing CreateThread record with independent retail descriptor/context captures. Validate changes before exposing ready state. Do not merely return success or set the full retail Status word as a universal default.
- Define scheduling at the actual blocking/yield boundary with main priority 2 and the new priority-5 thread. Preserve registers, stack, GP, arguments and return state. Reject unsupported thread operations explicitly.
- Trace the EE thread to its first wait/RPC registration and the IOP helper to its outbound request. Resolve this bounded callback pair together; determine ownership and required transport/event behavior before dispatch.
- Rerun toward the independently captured SOUNDS bind to 0x02000004, client 0x6351C0, RPC ID 29, caller return 0x297608. Existing native registration is available, but its bind/request delivery has not been retained as completed work while the earlier EE transition remains invalid.

## Forward evidence and limits

A local DECI2 subfunction-16 checkpoint returns all ones and contains a disc-file diagnostic. Authored console decoding and generated formatter ABI support have synthetic tests, but the valid native path has not reached that message. A diagnostic saying a file was found does not prove asset reads or loading.

No runnable EE scheduler or IOP-to-EE callback completion, disc-file read, asset loading, audio playback or displayed frame is claimed. This is a new execution/scheduling scope requiring its own design before changing startup state.


## Verified ready transition and continued sound command (2026-10-05)

A new unique ID-3 ReferThreadStatus capture returns 0x10 and the full 48-byte dormant descriptor. Its hook preserves earlier ID-2 calls, including displaced branch/delay-slot behavior. Caller Status is 0x70030C11 before/after; the query runs with EIE cleared, 0x70020C11. A checked 56-byte local bundle supplies this scope. The adapter enables only IE/EIE at the exact handoff, preserving other native bits and rejecting absent/stale evidence. It does not assign the full retail Status as a universal default.

Generated StartThread now executes its real helper, queries owned state, clears the saved context and invokes selector 0x22. The new child's RA uses the owned SetupThread exit **0x100250**, independently matching retail; older dormant bootstrap thread ID 2 retains its previously documented reference profile. The started child is ready with entry 0x315380 / GP 0x38BDF0 / SP 0x3A6CA0 / argument zero. Main priority 2 continues ahead of child priority 5. No child instructions execute and no general scheduler is claimed.

The SOUNDS bind now returns owned native addresses. Command **0x10**, RPC ID 30, input **256 bytes**, response **64 bytes**, executes its actual handler for **54 native IOP instructions** and returns sixty-four zero bytes. Independent capture validates completed client, response, input buffer and selected request metadata. Generated EE RPC_END/wait/cleanup complete. This does not establish playback, asset loading or callback timing.

The actual next blocker is the separate [DBCMAN request scope](../input/DBCMAN_RPC_PLAN.md). The ready EE thread and ready SDRDRV helper remain pending until a modeled wait/dispatch event requires their execution. The authoritative checkpoint is 1,077 EE calls / 1,071 returns and 31,126 IOP instructions / 131 physical targets.


## Main status and next worker lifecycle (2026-10-06)

The reached main query now supplies the two evidenced fields consumed by its exact caller: running status 1 and current priority 2 at descriptor offset 0x18. Other live descriptor bytes are preserved and unmodeled; the reference's function/wait fields are not treated as a general owned main-thread structure. Original follow-up captures establish helper IDs 4/5 at entries 0x2EDE58 / 0x2EDF48 before the later workers 6/7 at 0x2EE050 / 0x2EE140. The first helper's extra ready query returns status 2 and selector 0x37 suspends it; it is not a wakeup. See EE_SOUND_SCHEDULING_PLAN.md for the exact four-worker signatures, suspension and subsequent scheduling scope. The resumed full run, lifecycle tests and original capture comparisons now verify these bounded transitions.
