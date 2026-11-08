# DBCMAN reverse SIF DMA scope

## Original boundary (resolved below)

The four DBCMAN EE binds and command 0x80001363 complete. Its first native 144-byte response starts with 0x310. Generated initialization creates semaphore 7 (max 127, initial 1, option 0x381C28). Command 0x80001304 stages [0x310, 0x3A27C0, zero remainder], uses temporary semaphore 8 / RPC ID 36, and executes the registered native handler.

The handler retains EE destination 0x3A27C0 at owned DBCMAN offset 0x2774, then stops at **sifman:7 / sceSifSetDma**, native IOP PC **0x621DC**. A stack descriptor at 0x1E1EE0 contains [0x62778, 0x3A27C0, 0x80, 0]. Those are owned IOP source, EE destination, 128 bytes and zero flags. The second RPC remains pending; its result, wait and deletion are not acknowledged. The native service is marked failed after this partial execution and rejects retries without further mutation.

Retail second-command captures return RPC result [1, 0x3A27C0, zero remainder]. That return alone does not establish a safe native DMA completion. Existing snapshots leave IOP workers running and do not stabilize the exact first transferred payload or its completion sequence. Do not equate a later controller-buffer sample with the first transfer.

## Separate implementation and evidence work

1. Add an isolated local IOP capture at the actual DBCMAN DMA submit and completion/poll caller. Validate relocated code, caller/worker ownership, descriptor count, source bytes and destination before patching. Preserve original delay/load behavior and keep patched code/captures ignored.
2. Consult the pinned PS2SDK sifman.h/sifman.c and BT3 transport interfaces. Distinguish IOP-to-EE transfer from existing EE-to-IOP boot copy. Establish the exact reached sceSifDmaStat ordinal-8 poll behavior and whether any interrupt/callback executes.
3. Design an owned cross-space transport endpoint linking NativeIopService to checked EE RAM. Validate the full source/destination spans and flags before mutation. Keep token lifecycle and completion separate from guest RPC completion; do not introduce generic successful DMA.
4. Implement only the observed initial 128-byte transfer, compare its bytes/effects with the isolated capture, then allow the translated handler to finish and generated RPC_END/wait/delete to execute. Preserve the persistent initialization semaphore and parked workers.
5. Continue the bounded request group through its actual next blocker. Later recurring device transfers, callbacks, Vblank timing and controller input need their own evidence.

This is a new cross-address-space transport implementation scope. No frame, assets, input capability or scheduling equivalence follows from registration or the captured retail return. No reference implementation is copied.

## Completed initial scope (2026-10-05)

The initial 128-byte reverse copy, one negative sceSifDmaStat poll, real handler return [1, 0x3A27C0, zero remainder], RPC_END and generated wait/delete now complete. Temporary sifman providers validate exact caller/GP/descriptor/count/token and are removed afterward. Full endpoint bounds are checked before mutation; native token 0x710001 models owned logical completion only. The submitted payload is retained independently of mutable IOP buffers and compared with actual EE readback. Service tests reject short endpoints without mutation and verify all EE bytes outside the destination remain unchanged.

The isolated interpreter capture intercepts the loaded DBCMAN imports, invokes actual SIF submission/poll and verifies destination bytes. An initial live-source comparison exposed concurrent double-buffer changes. The successful diagnostic explicitly redirects this single descriptor to an immutable 128-byte source copy in ignored scratch space; metadata and validator identify the alteration. It establishes actual transport/readback and negative poll, not identical live controller samples or physical timing.

Both following 0x80001301 requests complete. Inputs begin [3,1,slot] for slots 0/1, with buffer pairs 0x3B0B00/0x3B0B80 and 0x3B0C80/0x3B0D00. Responses preserve input and set word 9 to the slot. Independent first-return and second-parent-return captures match native results. Recurring transfers, callbacks, controller behavior and Vblank scheduling remain unsupported. The next reached subsystem is the MCSERV bind; no frame milestone follows.
