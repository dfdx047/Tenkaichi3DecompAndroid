# SIO2 card DMA scope

Status: bounded initial absent-card transport verified; timed IOP suspension is the next separate scope.

## Completed behavior

- Real initial MCSERV command 1 enters MCMAN and SIO2. Priority-16 wake preserves the priority-104 caller.
- Channel 11 sends owned 144-byte slices from 0x32020; channel 12 receives into 0x32650. Both use 36 words / one block. Native START is distinct from programming.
- Five `81 11` probes (command 0x100472) and one `81 F3` fallback (0x140572) execute. Remaining transmit bytes are zero.
- Explicit absent-card policy returns six FF bytes plus 138 padding zeros, independently captured with both reference card slots disabled. Mounted cards and other commands are rejected.
- The registered native IRQ-17 handler/event path executes. Repeated control writes/readbacks retain 0x3BD. Final MADR 0x320B0/0x326E0, BCR 0x10024 and CHCR 0x201/0x40000200 match normalized owned reference buffers/registers.

## Evidence and limits

`capture_sio2_dma.py` captures actual slice programming; `capture_sio2_start.py` invokes actual first START APIs; `capture_sio2_flow.py` observes actual packet prefixes/control stores and stops before secondary SIF submission. `verify_mc_sio2_flow.py` checks environment hashes, observer bytes, relocated MCMAN ownership, six packets, final buffers/registers and native pending boundary. All raw output stays in ignored work/.

Observers affect scheduling; whole-system control counts and hardware timing are not equated. Native completion is bounded logical transport, not a general DMA scheduler. No retail result or uninitialized 192-byte secondary payload is used as a native template. No full card filesystem/authentication is implemented.

## Remaining path

Native reaches a 300 microsecond DelayThread at IOP PC 0x31B40. Follow [timed IOP scope](../iop/IOP_TIMED_WAIT_PLAN.md) before supplying a wake/completion. Retail later requests secondary SIF transfer to 0x3A7480, size 192, flags 0. Native must copy its own stack bytes, then deliver primary response/RPC_END and execute the guest callback.

FUN_0031DD60 is an observed indirect entry absent from the provisional EE map. Discover/generate it from evidence when reached; do not handwrite a success callback. Prior callback -2 and unrequested fields do not establish the explicit absent-card profile's result.
