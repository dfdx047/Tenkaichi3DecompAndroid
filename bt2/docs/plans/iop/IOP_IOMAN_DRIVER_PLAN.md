# IOMAN driver lifecycle bringup

## Reached boundary

Compiled MCMAN entry reaches ioman ordinal 21 (DelDrv) at native PC 0x31BE4 after 1,338 cumulative IOP instructions. SIO2MAN is resident; MCMAN has no acknowledged module ID. This new driver/device lifecycle scope requires owned registry state and nested native callback execution.

## Evidence and sequence

1. Inspect the reached name argument and subsequent native registration descriptor from local generated execution; keep unknown targets neutral. Public pinned PS2SDK ioman.h defines a five-word device descriptor (name/type/version/description/operations). Validate bounded strings, descriptor and callback pointers against owned RAM before mutation.
2. Model a fresh owned IOMAN registry. Public ioman.c establishes DelDrv returns -1 for an absent name; an existing driver calls native deinit before removal. Do not invent preloaded drivers. Verify any needed preexisting device against available local evidence.
3. AddDrv (ordinal 20) selects a free slot and invokes native init with the device pointer; failed init removes registration and returns -1. Execute callbacks through the compiled dispatcher with preserved caller context, GP and stack ownership. Do not treat registration alone as success.
4. Add authored synthetic tests for absent deletion, capacity/duplicates, invalid descriptors, successful/failed native callbacks, context restoration and removal ordering. Ensure callback failure cannot acknowledge MCMAN.
5. Rerun actual MCMAN entry and continue subsequent imports. Implement only reached file/device operations backed by evidence. Entry return requires a separate validation of resident effects before EE completion.

## Limits

No memory-card backing store, authentication, SIO2 transfer, asynchronous device completion or generic filesystem success is implied. Unknown operations remain explicit blockers. Consult BT3 architecture first, then public SDK semantics and local BT2 execution; adapt rather than copy.

Public semantics: [ioman.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/ioman/src/ioman.c), [ioman.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/ioman/include/ioman.h).

## Implemented checkpoint: 2026-10-04

Owned 16-slot registry implements AddDrv/DelDrv with real compiled init/deinit callbacks, retained module GP and copied caller context. Absent deletion returns -1. Public SDK permits duplicate names; first-match deletion is preserved. Negative init or callback failure removes the pending registration; deinit failure retains the existing driver.

Actual MCMAN registers mc at native descriptor 0x3200C / operations 0x31FA0 and executes init 0x2FC10. Entry returns 2, retained/removable according to public MODLOAD semantics. A new isolated local capture at EE caller return 0x100434 confirms result 2, relocated code and normalized initialized data/BSS, including two priority semaphores. Opaque options and device transfers remain unvalidated. Native MCSERV proceeds to the separate [SIF/RPC scope](IOP_SIF_RPC_SERVER_PLAN.md).
