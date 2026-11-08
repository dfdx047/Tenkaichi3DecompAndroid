# EE LOADFILE and native IOP lifetime bridge

## Starting checkpoint (historical)

EE is paused at its real first LOADFILE RPC 0. An isolated native SIO2MAN instance now returns entry result 0 and retains its worker waiting for event bits 0x4155. Normalized startup effects match the local reference capture. This does not yet fulfill the pending EE request: no persistent native IOP instance is connected to that adapter.

## Implementation sequence

1. Refactor the isolated module initialization into an owned runtime component shared by the diagnostic executable and EE runtime. Preserve image/source identity, separate ABI, relocation placement, explicit providers and retained contexts. Do not run a separate process and replay its report as LOADFILE success.
2. Give that runtime a fresh service epoch/module registry tied to post-reset transport ownership. Allocate an owned module ID, record resident entry result and validate the request path/argument length. Never reuse captured module IDs or stale IOP pointers.
3. Compile/link the ignored local module into the optional EE probe target. Keep ordinary bt2-port builds independent of local proprietary code. Failed/unresolved imports, hardware or invalid input must leave the RPC pending or report a justified loader error.
4. On validated resident completion, write the owned module ID/result into the checked eight-byte LOADFILE response and deliver it through the existing generated EE RPC_END callback, semaphore and packet cleanup. Preserve native module/kernel objects for subsequent calls.
5. Rerun the full EE pipeline, inspect the next real module/RPC/hardware request, generate the needed local module and continue actual entry/import blockers. Extend dispatch/module allocation from reached evidence; no speculative resident services.
6. Add meaningful tests for epoch isolation, repeated requests, request mismatch, retained worker lifetime and completion ordering. Compare module-load helper return/next request with local PCSX2 captures, update progress, commit and push.

## Limits

The kernel provider is a scoped compatibility model, not a recompiled IOP boot. Entry return alone does not validate controller transfers or full scheduling. The [SIO2 scope](../input/SIO2_BRINGUP_PLAN.md) keeps unsupported starts/status/FIFO/DMA/IRQ behavior explicit. Opaque reference kernel options and physical timing remain unverified.

## Implemented checkpoint: 2026-10-04

Shared NativeIopService owns epoch 1, module IDs, RAM, library/kernel state and retained worker contexts. The optional EE link executes compiled SIO2MAN in that instance, validates resident effects, returns owned module ID 1/result 0 and runs the generated RPC_END callback and semaphore/packet cleanup. No reference module ID or isolated report is replayed.

The next actual request loads compiled MCMAN into the same instance at 0x20000. Reached callback setters, memset and priority semaphore creation execute; IOMAN DelDrv remains unsupported. Failed entry latches the service failure and leaves the EE request pending without response mutation. Tests cover ownership, malformed requests, failure isolation and retained SIO2MAN worker lifetime. Continue with the [IOMAN plan](IOP_IOMAN_DRIVER_PLAN.md).
