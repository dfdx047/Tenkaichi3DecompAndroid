# GPU rendering through paraLLEl-GS

Render the game's GS output on the GPU with [paraLLEl-GS](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference) (Vulkan compute, LGPL-3.0, snapshot `cc6184af`) as the default renderer. The CPU GS model stays as the fallback for systems without a suitable Vulkan GPU (decided 2026-10-11) and as the test reference. Step 3 of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). The CPU model stays as the test reference until the GPU path matches it.

## Evidence (2026-10-11)

- **Build:** the snapshot builds standalone with MSVC (Release static libraries `parallel-gs`, `granite-vulkan`, `granite-util`, `granite-math`, `granite-volk`) in ignored `work/dependencies/parallel-gs-build`. Vulkan SDK 1.4.363; AMD RX 6800 XT on the development machine.
- **API** (`gs/gs_interface.hpp`): `GSInterface::init(Vulkan::Device*, GSOptions)`, `gif_transfer(path, data, size)`, `write_register(RegisterAddr, payload)` for privileged registers, `vsync(VSyncInfo)` returning a `ScanoutResult` image, `map_vram_read` for readback, `flush()`.
- **Our feed points:**
  - GIF channel 2 and VIF1 DIRECT data reach `GsCommandSink::transfer` / `transferAll` (`runtime/ee/ps2_runtime_memory.cpp`), plus `consume` / `consumeConfiguration` from the startup staging models.
  - Privileged registers (PMODE, SMODE2, DISPFB1/2, DISPLAY1/2, BGCOLOR) are recorded in `gs_privileged` at the game's stores; the library flip (`0x311908`, DISPLAY2) calls `on_display_flip`.
  - The viewer presents `decodeDisplay(gs, dispfb, display)` at each flip (`runtime/probe/entry_probe.cpp`, `runtime/host/viewer_window.h`).
- **Fail-explicit today:** the CPU sink validates every transfer and refuses a whole transaction (with rollback) when it reaches an unsupported GS feature. paraLLEl-GS accepts anything.

## Design

1. **Dependency:** pin the snapshot as a fetched dependency (like PS2Recomp), built through `add_subdirectory` so Debug and Release use matching MSVC runtimes; not committed. Off by default behind `BT2_GPU_GS` until it matches.
2. **One backend interface** (`runtime/gs/gs_backend.h`): `transfer(path, bytes)`, `privileged(register, value)`, `vsync(field)` → presented image. Two implementations: the existing CPU model and `runtime/gs/gpu/gpu_gs_backend.cpp` (paraLLEl-GS). The runtime feeds the backend, not `GsCommandSink` directly. This also gives the single GIF channel-2 feed that step 4 (chain mode) needs.
3. **Validation stays:** GIF packets keep passing the existing tag/register validation before either backend sees them, so unsupported features still stop at a checked boundary. Only rasterization moves to the GPU.
4. **Mirror mode first:** with `BT2_GPU_GS=mirror`, both backends receive every transfer. At each flip, read the displayed framebuffer back from paraLLEl-GS (`map_vram_read`) and compare it with the CPU model's. Record mismatches per frame.
5. **Presentation:** the viewer presents the `ScanoutResult` image (Granite swapchain, or a readback into the existing window at first). Interlace handling follows paraLLEl-GS's CRTC model.
6. **GPU only:** once the 650 startup frames match, `BT2_GPU_GS=on` keeps the CPU model as validator only; the CPU renderer remains selectable as the fallback.

## Verification

- **Unit test:** a small GIF packet set (sprite, triangle, PSMT8 + CLUT upload) rendered by both backends; framebuffers equal.
- **Startup frames:** in mirror mode, all 650 presented frames compare equal to the CPU model, or any difference is explained (paraLLEl-GS is not bit-exact in varying interpolation; such tolerance is documented, not assumed).
- **Original comparison:** spot frames against PCSX2 captures, as with the CPU renderer.
- **Performance:** Release run time before and after (CPU model is about 75% of the 19 s startup run).

## Stop conditions

- No Vulkan 1.3 device: fail at startup with a named error; the CPU backend remains selectable.
- A frame mismatch that is not explained by documented paraLLEl-GS behavior stops the GPU-only switch.

## Progress (2026-10-11)

- **Done:** pinned dependency (`config/parallel_gs.lock.json`, `tools/graphics/gpu/fetch_parallel_gs.py`), built through `add_subdirectory` behind `BT2_GPU_GS`; `GpuGsBackend` (Vulkan device, PATH3 transfers, VRAM readback); a unit test where a sprite renders identically on both backends; mirror mode (`BT2_GPU_GS=mirror`) fed by an `on_accepted` observer on the CPU sink.
- **Mirror result (Release, 650 frames):** frames 1–29 match word for word. From frame 30, a small and growing number of words differ by one step in one color channel (first: frame 30, word `0x1DC0D`, CPU `0x100000`, GPU `0x110000`; at most a few hundred words per frame). Same stop as without the mirror.
- **Open:** which renderer is right for those words. The pattern fits interpolation rounding (Gouraud or bilinear), which paraLLEl-GS documents as not bit-exact. Deciding needs a reference: a PCSX2 GS dump of the startup frames (paraLLEl-GS's dump parser reads PCSX2's format), not available locally.
- **Presentation:** at each flip the privileged registers the game wrote, plus SMODE1 derived from its `SetGsCrt` request (NTSC/PAL, analog), go to paraLLEl-GS; its scanout is read back and shown in the viewer (`--viewer` with `BT2_GPU_GS`), or saved with `BT2_GPU_GS_SNAPSHOT=<flip>:<path>`. The GPU scanout of flips 120 and 400 matches the CPU frames visually.
- **GPU-only mode** (`BT2_GPU_GS=on`): the CPU model still validates every packet and keeps its counters (`rasterize=false`) but draws nothing; paraLLEl-GS flushes at each flip. Same stop and counters; flip 400 is pixel-identical to mirror mode. Release startup run: **2.3 s instead of 19.1 s**.
- **Deferred until the port is done:** automatic fallback to the CPU renderer when no suitable Vulkan device exists. **Not yet:** GPU as the default everywhere once the rounding question is settled; CSR reset forwarding (not needed so far).

## Licensing

paraLLEl-GS is LGPL-3.0, linked statically. Before a distributed build, decide between dynamic linking and providing relinkable objects; record the choice in [LEGAL](../../project/LEGAL.md). Not needed while nothing is distributed.
