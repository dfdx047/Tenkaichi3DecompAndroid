# IOP Vblank timing and SIO2D startup

## Reached boundary

Native SIO2D entry starts an actual worker that calls vblank:5 (WaitVblankEnd) at 0x71AA4. Four preceding modules are resident; six native workers wait on real event/RPC conditions. No video-timing event producer exists in the scoped runtime.

Pinned public VBLANK source implements WaitVblankEnd as an OR wait on the module's end-event flag. Returning immediately or setting that bit without an owned timing event would fabricate behavior. This boundary requires a bounded video-timing plan separate from RPC server registration.

## Next bounded sequence

1. Inspect the reached SIO2D worker context, expected Vblank flag phases and public VBLANK interrupt handlers. Consult reference renderer/timing architecture. Preserve observed callbacks and neutral target names; do not infer a controller protocol from the module name.
2. Define owned Vblank event/phase state per service epoch. Park an actual native worker while the required event is absent, preserving its import return/context. Identify whether startup can return resident with the worker parked; validate against an isolated local return/wait capture before EE completion.
3. Establish an explicit logical event source tied to the intended host/GS presentation clock. Define event ordering, initialization phase and reset behavior from local before/after observations and public behavior. Do not equate synthetic test ticks with game/reference evidence or physical timing.
4. Resume native workers only on a genuine modeled phase transition. Follow reached SIO2MAN callbacks/exports, semaphore waits and transfer accesses. Native library calls need retained module GP/context and actual compiled targets; no successful default imports.
5. Add tests for absent event, wait/resume ordering, phase/reset ownership and callback failure. Rerun the full startup path toward asset loading/drawing/presentation. Card/controller/authentication and physical transfers remain separate evidence scopes when reached.

Evidence: pinned PS2SDK [vblank.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/vblank/include/vblank.h) and [vblank.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/vblank/src/vblank.c). [SIO2 limits](../input/SIO2_BRINGUP_PLAN.md), [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md).

## Validated startup parking (2026-10-04)

SIO2D now returns resident result 0 with its native worker parked at WaitVblankEnd, mask 4 / OR mode 1, on an owned attribute-2 event initialized to zero. No Vblank phase is generated. Local unique-caller capture at 0x100494 validates result, relocated code, normalized data/BSS, worker parameters/wait and two semaphore objects. The live worker counter at module offset 0x1BD8 is explicitly excluded (native 0/reference 9); native/reference event bits 0/8 both lack the end bit, but their phase/timing is not equivalent.

Owned event set/clear can resume a satisfied worker from its saved import; result bits are published and CLEAR is applied before execution, with a retained satisfaction token. This is tested synchronization, not a Vblank producer. No physical or logical video clock is connected. Nested event wake and suspension inside native export calls remain rejected. Current execution instead reaches the SIO2 device transfer boundary during DS2U_D startup.

DS2U_D now also uses WaitVblankStart (ordinal 4), parked on mask 1 / OR of the shared event. Its connected-port worker is retained; the disconnected-port worker is dormant. No phase transition is supplied. DS2U_D entry can complete with these observed states, independently of implementing a video clock.
