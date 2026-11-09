# Scoped SIO2 startup and scheduling

## Evidence and scope

The first native SIO2MAN entry writes 0x3BC to control 0x1F808268. Pinned PCSX2 identifies SIO2MAN_RESET and START_TRANSFER bit 0 separately. The initial reset checkpoint recorded owned control state while other control/FIFO/status accesses remained unsupported. The scoped initial-poll extension below now supplies only directly validated replies/status and an explicit native IRQ path; DMA and physical timing remain unsupported.

Public PS2SDK supplies event/semaphore/thread parameter layouts, first thread startup, interrupt registration/suspend/resume and SIO2 DMA priority/enable semantics. BT3's module/import/scheduler separation informs structure; its permissive success defaults are not adopted. Only reached providers are installed.

## Implemented checkpoint

- Actual entry returns resident result 0 after 151 native instructions.
- Library registration, one event, one semaphore, one dormant-then-started thread, IRQ 17 registration/enabling and DMA channels 11/12 priority 3/enabling retain owned state.
- A separate thread stack and context retain PC/GP/HI/LO/branch/load state. The created thread runs to WaitEventFlag for mask 0x4155, OR mode, and yields. No event completion is supplied.
- The owned diagnostic kernel begins with interrupt dispatch enabled, matching public THREADMAN startup convention. Nested suspend/error handling and general scheduling are explicitly unsupported.
- Started worker execution is scoped to this observed initialization path. It does not establish general priority/preemption/timing fidelity. Synthetic native tests verify parked import resumption, register/return state, result bits and WEF_CLEAR behavior.
- Local capture comparison validates normalized module BSS and reference semaphore/event/thread wait ownership. Retail kernel record offsets are observations of this scenario, not a portable SDK structure. Opaque options differ because the reference caller stack contains prior data; no semantic equivalence is claimed for those options. Full reference IOP snapshot remains live.

## Continue from here

Follow [the EE LOADFILE lifetime bridge plan](../iop/IOP_LOADFILE_BRIDGE_PLAN.md) to connect the completed entry and retained worker. Then gather reached SIO2 accesses/requests and before/after state. Implement only observed control, command, FIFO, DMA and IRQ behavior with explicit known/unknown fields. Resume waiting native threads only when a real modeled event satisfies their masks. Validate transfer results against local captures before acknowledging completion. Controller/memory-card protocol work is a separate scope when actually reached.

## Reached device-transfer boundary (2026-10-04)

DS2U_D executes registered compiled exports using the registering module GP. Its startup acquires a positive semaphore count, copies owned RAM, sets/clears real event flags and resumes the actual SIO2MAN worker. That worker reads its previously written control value and writes command/port configuration. These values are retained without producing a device response. The prior pass stopped at native PC 0x10148 writing byte 1 to 0x1F808260 (SIO2 transmit FIFO); the scoped initial polls below extend that checkpoint. Five residents are acknowledged, DS2U_D remains pending, and six other workers wait. Current SIO2MAN state is active, so the earlier parked startup snapshot is historical.

This is a new device/protocol scope beyond module initialization. Before accepting FIFO traffic, establish the complete observed packet and configured port/device, command queue lengths, transmit/receive ownership, and completion ordering from local evidence. Consult pinned PCSX2 Sio2.cpp/IopHwWrite.cpp and BT3 runtime structure. Choose the bounded device model needed by this actual startup request, retaining unknown fields rather than default replies. Validate response bytes and native event/IRQ/return effects against local reference evidence before completing DS2U_D. Do not equate a retained configuration write with a completed transfer.

Current scheduling is cooperative and scoped: FIFO event-wait satisfaction, priority order among woken threads and actual saved-context execution. Nested worker wakes, blocking native-export call frames, general preemption, later device responses, SIO2 DMA/physical timing and Vblank production remain unsupported.

## Scoped initial polls and DS2U_D resident completion (2026-10-04)

Direct isolated interpreter captures resume from the SIO2D EE return and instrument the loaded IOP FIFO read helper. The hook performs each original FIFO read once, preserves scratch registers/SP, records reply/status and halts that worker after the fifth or tenth byte. Capture files and patched guest code stay ignored. Guarded initial scratch fill and loaded helper bytes prevent silently patching a different scenario. The later DS2U_D EE return snapshot is live and already contains evolved controller state; it cannot validate the first reply by itself.

Observed first request: port 0 command 0x140540, bytes 01 42 00 00 00, port controls 0xFFC00505/0x20014. Reply FF 41 5A FF FF, command status 0x1100, port status 0xF, FIFO status 0, control 0x3BD and acknowledged IRQ status 0. Port 1 command 0x140541 has the same poll bytes and controls, reply FF FF FF FF FF and command status 0x2D100. Runtime supports exactly these initial digital/no-buttons and disconnected-port scenarios. Unknown packets and FIFO underflow fail; these are not gameplay controls.

An explicit START control write dispatches the registered native IRQ 17 handler with its retained GP/argument and separate context. Interrupt event set records actual flag state; result is consumed by the native worker. Only this cooperative, synchronous completion is modeled, not physical timing. Command-zero writes reset consumed FIFO state; unknown/unread state remains rejected. Repeated START writes can raise the explicit modeled IRQ without inventing another reply. Callback jumps across owned modules now continue through checked module dispatch; invalid/unmapped targets still fail.

DS2U_D returns 0. Its port-0 worker waits on the shared Vblank start flag (mask 1/OR); its port-1 worker remains dormant. Thread status queries use owned public-layout fields. Dedicated verification guards local source/disc/capture hashes, relocated code, return, both worker parameters/wait/dormant state and direct poll publications. Live DS2U_D data/BSS evolves under Vblank and is intentionally not equated. Native startup does not advance Vblank, configure analog mode or process later packets. The next requested module is LIBSD, a separate sound initialization scope.
