# Reached EE cache and CRI transfer boundary

## Initial boundary evidence

After CRI bind and original command-2 allocation, generated FUN_002FCC58 calls FUN_0032F558 at 0x2FCD84 (resume 0x2FCD8C). Arguments are 0x365BC0 and 0x36643F. The candidate calls FUN_0032F4B0, whose reached body contains CACHE 0x10/0x14 on both indexed ways and MFC0 TagLo. At the initial boundary, the probe had no TagLo field or validated cache-tag contract; experimental linkage failed compilation. Those experimental routines were removed from the private selected bindings to preserve the working diagnostic executable. Neutral map boundaries remain provisional.

This is a new R5900 cache/transfer subsystem. The previous coherent owned-RAM copies do not establish indexed tag reads, valid/dirty bits, operation ordering or the helper's conditional behavior. A missing field initialized to zero would silently choose branches without evidence.

## Scope to establish before implementation

- Consult REFERENCE_ARCHITECTURE.md, pinned BT3 cache/transfer patterns and authoritative R5900/PCSX2 CACHE and TagLo semantics. Adapt the ownership contract; do not copy implementations.
- Capture the original unique caller inputs/return, status handoff and actual first CACHE/TagLo observations. Determine whether the bounded native model can use coherent owned RAM and explicit logical tags, or needs dirty-line/alias state. Do not equate physical cache timing.
- Define TagLo ownership and CACHE effects transactionally, including both indexed ways and addresses. Extend the authored ABI/preparation tooling, never the proprietary-derived source by hand.
- Link only the reached routines, run the original helper and compare its memory/interrupt effects. Continue through CRI 0x403 and the captured 0x40C request. Streaming, SPU2/audio effects and archive payload requests need their own observed contracts.

At that initial checkpoint, the protocol/hardware design was not yet validated. No native audio or presented game frame follows from current CRI residency/allocation.


## Indexed scan evidence (2026-10-06)

Consulted the pinned [PCSX2 CACHE implementation](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Cache.cpp), local SHA-256 `29c31323fe84695f4b1a06a9ced3760f3b14a6fbe3a1633277fcbf5875ef8d9b`, and BT3's pinned coherent-memory/cache patch configuration. PCSX2 identifies 0x10 as indexed tag load, 0x14 as indexed writeback/invalidate and 0x16 as indexed invalidate. Its tag-load implementation exposes flags only; this reference cannot establish real hardware physical tag addresses.

Two independent caller checkpoints now capture the original helpers: resume 0x2FCD8C after FUN_0032F4B0 and resume 0x2FCDA4 after FUN_0032F5F0. Each records all 64 indices on both ways, overwriting 128 nonzero diagnostic sentinels with zero TagLo. Authored observation hooks retain the original MFC0 and AND and resume at the original ADDU; they never inject a cache result or RPC completion. Diagnostic data occupies an added ELF segment, outside retail globals. Both callers return with range 0x365BC0..0x366400, original stack restoration and interrupt enable bits restored; their 0x880-byte buffers match.

The bounded native contract uses explicitly invalid logical tags with coherent owned EE RAM. It validates range alignment/bounds, disabled EIE, scan order, both ways and TagLo read ordering. Original translated conditions decide whether to invalidate/write back. A reached 0x14/0x16 operation remains a checked failure because zero invalid tags cannot select the observed nonzero range. No physical dirty cache, hardware timing or general alias model is claimed.


Both helpers and their wrappers are now linked and execute. Full command 0x403 cleanup subsequently completes. Reproduce the independent cache check with `python tools/capture_ee/verify_ee_cache.py --native work/bt2-generation/cache-completed.json`; raw captures, prepared code and the retained checkpoint remain ignored. The next boundary is [CRI child preemption](../sound/CRI_THREAD_START_PLAN.md).
