// The BT3 renderer for BT2 (BT2_RENDERER=bt3): the GS drawn as BT3's Dragon Rage draws it, with the GPU's own
// rasteriser (gs/hw/bt3: a copy of Tenkaichi3DecompAndroid's port/src/gs, BT3 itself untouched), instead of
// paraLLEl-GS's per-pixel GS on compute shaders. The two games are the same engine and send the same kind of
// GIF stream; BT2's runtime walks the DMA and runs the VU1 programs itself and hands over the finished packets.
//
// The renderer draws into BT2's window, shows each frame itself at the flip (and leaves frames out when the game
// is behind, as BT3 does), and keeps its settings in BT2's own file (bt3_host.c: BT2_HW_SETTINGS).
#pragma once
#include <array>
#include <atomic>
#include <span>
#include <cstdint>

class GsCommandSink;
class ViewerWindow;

namespace HwGs {
// Whether the run draws with it (BT2_RENDERER=bt3).
bool wanted();
// Starts the renderer in the viewer's window and takes the sink's accepted GIF packets. False: it could not
// start (no GPU device): the run keeps paraLLEl-GS.
bool attach(GsCommandSink& sink,ViewerWindow& viewer);
// Each field the game is given (the renderer's frame-skip grid) and each flip (a frame ends: drawn and shown).
void field();
// How late the run is at this flip (the frame pacer's): the renderer's frame skipping decides with it.
void setLate(double seconds);
// `privileged`: the GS's display registers as the game left them at this flip (PMODE 0, DISPFB1 7, DISPFB2 9):
// the buffer they show is the frame's own, which BT3 learns from sceGsSwapDBuff.
void flip(const std::array<std::uint64_t,15>& privileged);
// Time spent so far taking the game's GIF packets (register writes, uploads, the vertices of each draw), for the
// frame budget in the viewer statistics (BT2_VIEWER_STATS).
extern std::atomic<double> gif_seconds;
// With "render_thread = 1": the game thread's waits for the GIF packets' thread at the flips, so far.
double renderWaitSeconds();
// Whether the GIF packets have a thread of their own ("render_thread = 1").
bool renderThread();
// The internal resolution the renderer draws at (1.0 = the PS2's), for the same statistics.
double scale();
// Local->host transfers: the renderer keeps GS memory as the game wrote it (uploads), not its drawings.
void readFifo(std::span<std::uint8_t> out);
}
