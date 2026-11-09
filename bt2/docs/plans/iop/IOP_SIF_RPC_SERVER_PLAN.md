# Native IOP SIF/RPC server bringup

## Reached boundary

After native SIO2MAN and MCMAN resident completion, EE requests MCSERV. Its entry registers mcserv and checks the module-path argument with strncmp, then reaches sifman:29 (sceSifCheckInit), PC 0x51074. Cumulative IOP execution: 1,426 instructions / 15 distinct physical targets. EE remains at 475 calls / 467 returns with two acknowledged resident modules.

This is a new ownership scope: native IOP SIF initialization, RPC queues/server callbacks and a worker waiting for requests. Existing EE transport evidence does not automatically establish these native objects or initialized flags.

## Implementation sequence

1. Inspect reached arguments and pinned PS2SDK SIFMAN/SIFCMD headers/source, alongside the BT3 native module architecture. Establish when the owned IOP transport becomes initialized and which service epoch owns it. Do not return initialized/ready merely to bypass a check.
2. Implement only reached initialization/query behavior. Keep EE packet callbacks, IOP queues, worker contexts and registered native server handlers separate; tie reset invalidation to service epoch ownership.
3. Continue MCSERV entry imports and its actual native worker. Registered library exports must dispatch to compiled native code with module GP, argument, stack and return ownership. Unsupported transfers or scheduler effects remain explicit stops.
4. Model real reached RPC queue/server registration and worker wait. Keep request buffers bounded, preserve callback ordering and do not synthesize memory-card/authentication success. Gather another isolated local capture at the MCSERV return/worker wait when needed.
5. Add authored tests for transport initialization lifetime, reset/stale requests, server ownership, native callbacks and failure leaving EE pending. Validate resident effects before acknowledging MCSERV; rerun the full pipeline and continue later modules.

## Evidence and limits

Pinned public [sifman.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sifman/include/sifman.h) identifies ordinal 29; public sifcmd startup checks SIFMAN before initializing its command/RPC state. [Reference architecture](../../reference/REFERENCE_ARCHITECTURE.md) records prior BT3 inspection. Adapt those patterns; no copied implementations or permissive import defaults.

No memory-card backing store, controller transfer, SIO2 IRQ completion, full scheduler or physical SIF DMA equivalence is established. Current MCSERV partial entry is not resident/acknowledged.

## Implemented checkpoint: 2026-10-04

The native compatibility service explicitly attaches to the already validated fresh EE transport epoch. It reports SIFMAN initialized only after attachment; an unattached/new epoch reports zero. The local post-MCMAN capture's linked SIFMAN ordinal-29 routine reads flag 1, confirming the reached reference state. No physical DMA result or unowned epoch is imported.

Reached RPC init mode zero owns logical protocol state. Compiled MCSERV/DBCMAN workers get owned thread identities, register bounded native queues/servers and yield at actual RPC loops with empty request lists. Queue linkage, server pointers and retained GP/stack are modeled; requests and transfer completions remain unsupported. The resident validators guard callbacks/workers/kernel objects. Normalized reference code/data/BSS, guarded thread records and waits match.

MCSERV returns 2 with one RPC worker; DBCMAN returns 0 with four RPC workers and one semaphore. Both EE loads complete, yielding four total residents and six waiting workers including SIO2MAN. Next reached SIO2D WaitVblankEnd is covered by the [Vblank plan](IOP_VBLANK_PLAN.md). No full scheduler or card/authentication behavior is established.
