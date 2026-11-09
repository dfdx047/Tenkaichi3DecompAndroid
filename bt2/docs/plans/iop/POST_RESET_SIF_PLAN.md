# Post-reset SIF transport bring-up

## Reached boundary

Both initial GS clear requests and their scoped synchronization calls return. Main startup calls FUN_00331FE8 again at 0x001003E0; FUN_00331888 reads DMAC_STAT at 0x1000E010, PC 0x003319CC, SP 0x01FFECA0. The full native probe stops at 265 calls / 261 returns. The earlier DMAC_STAT and channel-5 samples have already been consumed in the preceding IOP lifetime.

The diagnostic reset-ready branch does not execute an IOP boot or make the old RPC services valid. IOP epoch 1 remains pending and old service state stays invalid. Treat this as a new transport/service lifetime, not a replay of first initialization.

## Next implementation scope

1. Consult REFERENCE_ARCHITECTURE.md, pinned BT3 IOP/runtime guidance and PCSX2 SIF/DMAC code. Read the existing ignored reset-ready IOP capture and its limitations before selecting fresh evidence.
2. Capture the unique post-GS caller at 0x001003E0 and fresh DMAC_STAT/channel-5/SIF registers. A trap at the shared helper alone would stop at the earlier invocation; make the capture distinguish the reached lifetime and preserve earlier execution. Validate original ELF identity, patch shape, publication stability, caller/SP and reference environment.
3. Follow the actual second initialization sequence. Model its status acknowledgement, channel ownership, interrupt registration and SIF address queries separately from invalidated RPC clients. Do not reset counters or reuse one-shot old checkpoints to bypass the blocker.
4. Establish fresh EE/IOP buffer and handler ownership before accepting a new CHANGE_SADDR, bind or RPC reply. Available live IOP RAM is diagnostic evidence, not proof of import execution, scheduler behavior or successful service startup.
5. Implement the narrow observed transport behavior; plan loader/import/scheduler execution separately if the path actually requires it. Keep neutral function names and proprietary output ignored.
6. Rerun through multiple supported blockers, test meaningful changes, update progress, commit and push.

The GS sink has no display register/presentation support. If execution next requests display setup, follow GS_RENDERER_BRINGUP_PLAN.md and obtain actual register evidence before presenting a frame.

## Completed transport scope (2026-10-04)

Separate disc-backed traps at main 0x001003E0 and 0x001003E8 preserve the earlier SIF initialization. EE publications and selected live IOP ownership windows are stable in two reads. The 136-byte extension validates ELF/trap/prefix identity, reference environment, whole RAM hashes and fresh buffer/SET_SREG effects.

The authored adapter accepts the observed DMAC status/channel reads, chain setup, handler registration/mask enable, address queries/mutations, two INIT_CMD submissions and one logical SET_SREG reply. Generated FUN_0032F040 and FUN_00331840 now complete, and the second FUN_00331FE8 returns. DMA submission IDs are authored nonzero tokens; this is diagnostic transport delivery without native IOP instructions, interrupt entry or physical timing.

Execution continues through synchronous RPC semaphore creation to the fresh bind request at 0x0032F028: 305 calls / 298 returns. Old service bindings remain invalid. Follow [the service lifetime plan](POST_RESET_IOP_SERVICES_PLAN.md) for the next implementation scope.
