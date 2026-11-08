# Live presentation window

## Why

Startup now draws real screens: the controller-missing message is rendered every frame from the owned GS memory model. Until now that content has only been visible as an offline PNG (`render_gs_frame.py`). A host window that presents each flip makes every later rendering step visible as it happens, and shows how far a run gets.

## Bounded design

1. **Framebuffer decoding.** A host-side function decodes the displayed framebuffer from the owned GS memory into a BGRA image, using the recorded privileged DISPFB/DISPLAY registers (circuit 2, then circuit 1). It supports PSMCT32/PSMCT24 and the public CT32 layout. Never-written pixels show as a dark checker, as in the PNG tool. The decoder is unit-tested.
2. **Window.** A plain Win32 window in the entry probe, so no new dependency. It is created at the first flip at 2× size, and the frame number is in the title. Messages are pumped at each flip. Closing the window ends the run.
   - **Scaling:** integer scaling by default: the largest whole-number scale that fits the resizable client area, centred with black borders, so pixels stay exact. Stretch-to-fill (`BT2_VIEWER_SCALE=stretch`) is the alternative.
   - **Later:** aspect-correct 4:3 output and filtering stay later options.
   - **16:9 (later, requested).** Widescreen needs a game-side change, so the 3D projection renders a wider view, plus a matching 16:9 presentation mode. PCSX2's widescreen patches for this game are the reference for what to change: consult them and adapt them as checked game patches, not copied blindly. Not before gameplay rendering exists.
3. **Presentation point.** The GS library's flip (the counted privileged store at `0x311908`) presents the framebuffer named by the display registers at that moment.
4. **Pacing.** Flips are shown no faster than one per NTSC field pair (about 59.94 Hz). A run slower than real time presents as fast as it executes. Logical guest time remains independent of host time.
5. **Run mode.** `run_startup_probe.py --viewer` (`BT2_VIEWER=1`) disables the frame budget unless one is given (`--frames N`). When the run stops at a checked boundary, the report is written as usual, and the last frame stays visible until the window is closed.

## Limits

- **Display:** no interlace field emulation, CRT timing, PCRTC merge/blend of both circuits, or gamma. The chosen circuit's framebuffer is shown as is.
- **Speed:** the Debug probe build runs slower than real time. Real-time speed needs an optimized build, which is a separate step.
- **Input:** no host input yet (see [CONTROLLER_PLAN.md](../input/CONTROLLER_PLAN.md)).

## References

- **[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs)** (see [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)): reference for presentation beyond the current limits: privileged display registers, CRTC offsets, interlaced scanout with weave de-interlacing and field blending, and scaling. Adapt its behavior; do not copy code.

## Progress (2026-10-08)

Steps 1–5 are implemented, with integer scaling by default. The user confirmed the window shows the controller-missing screen. The Debug build presents about 12–13 frames per second; real time needs an optimized build.
