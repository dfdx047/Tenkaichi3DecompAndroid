# Second IOP allocation and memory layout

## Actual boundary and evidence

After GIF configuration and generated EE allocation/init helpers, FUN_00297908 calls FUN_00336D60 with size 0x81800. That routine constructs heap operation 1 through client 0x3AC840, input 0x3AC8C0, result 0x3AC880 and the existing FILEIO heap server/buffer. Native currently stops at the RPC helper's CreateSema: selector 0x40, PC 0x32EC68, SP initial minus 288, max 1, initial 0, option 0x3838E8. Attr is uninitialized stack residue, as in earlier scoped RPC calls. No second native heap allocation succeeds.

Independent local `heap-second-after` capture at the unique caller return 0x297938 establishes size 0x81800, pointer 0xCE500, SP 0x1FFED30, client RPC ID 26 and the retained server/buffer. Source ELF, disc, BIOS/emulator, hook/stub and RAM hashes are checked. Allocated contents are undefined; full live IOP RAM is not equated.

The required retail interval is **[0xCE500, 0x14FD00)**. Our translated boot FILEIO helper is owned at **0x140000**. `IopRuntime::map` must reject that overlap; allowing the new allocation or overwriting the helper would corrupt native ownership. The existing first-allocation provider explicitly accepts only size 0x360 once. This is a new allocator/address-space scope beyond that first lifecycle.

## Implementation pass

1. Consult REFERENCE_ARCHITECTURE.md, BT3's IOP/runtime ownership patterns and pinned PS2SDK sysmem/FILEIO allocation contracts. Keep BT2 sizes, addresses and RPC observations separate.
2. Inventory every native IOP mapped range, compiled code/data/import region, stack and resident ownership against the retail capture. Determine an evidence-backed location for translated boot FILEIO or a consistent address-space design. Do not silently remap a retail heap result to a different EE-visible address.
3. Regenerate FILEIO and update checked dispatch/import/GP/data provenance if its authored translation base changes. Preserve the first allocation/free/module checks, resident counts and old buffer semantics. Reject overlap and stale epochs before any mutation.
4. Extend the owned heap provider and its real translated operation-1 handler for the captured second request. Verify rounding, free/reuse behavior and source block ownership against local evidence. General allocator behavior needs its own checked contracts; do not infer arbitrary successful allocations.
5. Enable only the reached synchronous RPC semaphore/descriptor path. Execute generated request construction and RPC_END cleanup; compare result/client/reply against retail evidence. Keep physical DMA timing and initial allocation contents unknown.
6. Run through related blockers, capture the next actual request/transfer and continue when safe. Build/test, update concise PROG and history, review, commit and push.

```powershell
python tools/capture_ee/verify_second_heap.py
```

This verifier confirms the current conflict and does not provide a native success bundle. Disc reads, asset loading, audio playback and presentation remain unimplemented.


## Verified completion (2026-10-05)

The initial conflict was broader than FILEIO: MODSESQ2, CDVDSTM, SOUNDS and CRI_ADXI also occupied the captured heap interval. Regenerated those images at authored bases 0x180000, 0x188000, 0x150000 and 0x168000; FILEIO is now 0x190000. Source/image provenance, all fourteen persistent translations, heap interval and reserved stacks are checked by `tools/iop/verify_layout.py`. Each relocated resident passes its existing independent code/data/BSS/worker comparison after address normalization.

The first allocation/copy/nonresident module/free lifecycle remains tested. The second allocation requires that completed lifecycle, exact size 0x81800, fresh epoch and thirteen residents. Overlap is rejected before service memory mutation or instruction execution. The actual FILEIO handler executes fifty additional IOP instructions, returns 0xCE500 and the real generated RPC_END/cleanup completes. Independent return/result checks pass. Allocated contents remain undefined; no general allocator behavior is inferred.

`verify_second_heap.py --native work/bt2-generation/entry-stop.json` now validates completion rather than requiring the obsolete conflict. Final checkpoint 983 calls / 975 returns, 31,001 IOP instructions. Next scope: [SDRDRV RPC delivery](../sound/SDRDRV_RPC_PLAN.md). The earlier boundary text above is retained as historical evidence.


## Fourth allocation verified; fifth collision (2026-10-06)

The original fourth request of 0x41040 bytes completes at 0x150600 after checked image/workspace relocation. Its rounded interval ends at 0x191700. The next original request is 0x8D0 at 0x191700 and conflicts with FILEIO; it is not admitted. Follow CRI_SECOND_CHANNEL_PLAN.md for a broader capacity inventory before choosing further placements.
