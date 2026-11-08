# Controller input (DBC, SIO2 pad protocol)

## Why this is a new subsystem

Startup now renders its first real screen: the overlay's frames draw a 512×128 PSMT4 texture reading "No DUALSHOCK®2 analog controller connected. Verify that a DUALSHOCK®2 analog controller is properly inserted into controller port 1." The game repeats this screen every frame while it waits for a controller.

The game asks for controller state through two no-wait DBC requests per frame (services `0x8000131E` and `0x8000131F`). Both run through the original IOP handlers, and both replies report no device (last word `0xFFFFFFE9`). Nothing models a pad on the SIO2 bus, so the driver modules never see a DualShock 2.

## Evidence (2026-10-08)

- **Modules:** the retail IRX set includes SIO2MAN, SIO2D, DBCMAN, DS2U_D and DS2O_D.
- **Existing SIO2 model:** it covers the absent-memory-card path (`verify_mc_sio2*.py`) and the single startup pad poll below. There is no general pad device model.
- **Requests:** per frame, the controller requests use 144-byte buffers at `0x3A2640` and `0x3A2700`. DBCMAN's reverse DMA (128 bytes to EE `0x3A27C0`) was observed only during initialization.
- **Initial probe:** startup already sends one SIO2 pad poll per port (command `0x140540`, bytes `01 42 00 00 00`). Port 1 answers `FF 41 5A FF FF`: a connected pad, digital mode (ID `0x41`), no buttons pressed. Port 2 answers all `0xFF`, meaning absent.
- **Pad threads:** after that probe, DS2U_D's port 1 worker (priority 46) waits on the IOP VBlank event (bit 1, WaitVblankStart), and its port 2 worker stays dormant. SIO2D's worker waits on the same event (bit 4, WaitVblankEnd).
- **Root cause:** the native IOP creates that VBlank event flag but never sets it. No IOP VBlank start or end interrupt is delivered per logical field, so the DS2U_D worker never continues its setup (configuration and analog mode) or its per-frame polling. The game therefore never sees a DUALSHOCK 2.

## Bounded design

0. **IOP VBlank.** Per logical field, set the IOP VBlank event bits for start (1) and end (4) at the right points relative to the existing EE VBlank and SOUNDS timer intervals. Run the woken IOP threads in priority order at that idle point, as the SOUNDS timer handler does. Unit-test the wake order.
1. **Trace the driver path.** Find out which IOP thread or timer polls the pad: DBCMAN/DS2U_D threads, the SIO2D transfer queue, IOP VBlank or timer wakeups. Capture what the game reads back, either the EE buffers above or a DBCMAN shared area. Compare with an original capture in which PCSX2 has a DualShock 2 connected, at the same frame point.
2. **SIO2 pad device.** Answer SIO2 transfers addressed to port 1 as a DualShock 2. Follow the public pad protocol (0x42 poll; 0x43 config enter/exit; 0x44 mode; 0x45 status; 0x4D motor map; 0x4F pressure config) as implemented in public references. Adapt the protocol, but don't copy it. Report neutral sticks, no buttons pressed and analog mode. Port 2 stays absent. Other devices (multitap, memory card on the pad bus) stay checked.
3. **Timing.** Pad transfers complete in logical time at the driver's own polling cadence. No physical SIO2 timing is claimed.
4. **Input source (later).** Add a host input interface feeding the button and stick state only once the live window exists. The probe keeps the neutral pad for repeatability.
5. **Verification.**
   - Compare the IOP driver state and EE replies with an original capture where a controller is connected.
   - Expect the controller-missing screen to give way to the next screen (presumably the publisher/developer logos). Check that with the PNG renderer (`render_gs_frame.py`).

## Limits and stop conditions

- No vibration output or host input is claimed in the first step.
- Stop if the driver needs undocumented device responses that a capture can't show.

## Progress (2026-10-08)

Steps 0–3 are implemented:
- IOP VBlank edges every field;
- the DualShock 2 model on port 1, with neutral input;
- the DBCMAN pad DMA to the registered socket buffers;
- DBCMAN frame requests through the original handler.

The game now leaves the controller-missing screen and proceeds to a memory card check.

**Not done yet.** A comparison with an original capture with a controller connected, and host input (step 4).

## Next (2026-10-08)

Without a memory card, the game shows a notice that waits for a button press (user note). Add a deterministic frame-indexed button script to the DualShock 2 model, for example `BT2_PAD_SCRIPT="300:cross,310:-"`, so probes stay repeatable. Then add host input (XInput/SDL, keyboard) for the live window.
