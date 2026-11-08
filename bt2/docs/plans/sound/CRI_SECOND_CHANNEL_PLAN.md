# Second CRI channel and IOP address-space capacity

## Confirmed frontier

The first channel completes command 0x40C, a fourth heap allocation and sixteen original 0x422/0x400 constructor pairs. Current full native execution stops at **selector 0x77 / PC 0x32F028**, pending FILEIO heap operation 1: RPC ID **90**, client **0x3AC840**, input **0x3AC8C0**, requested size **0x8D0**, output **0x3AC880**. Four allocations are complete; this fifth request is not accepted.

An independent original call-36 capture already reaches the subsequent CRI command 2 with input `[1, 0x35FD00, 0x191740, 0x880]`. The retained heap input/result is size 0x8D0 / pointer **0x191700**. With the verified 256-byte rounding, that next retail interval is **[0x191700, 0x192000)**. Our translated FILEIO image currently owns **[0x191700, 0x195980)**. Accepting the allocation at this layout would violate ownership. Do not overwrite FILEIO or change the EE-visible heap result.

The call-36 snapshot and prior completed call-35 result establish a second channel construction handoff. They do not establish its later commands, further allocation sizes, a second AutoDMA transfer, audio consumption or asset reads.

## Required broader layout work

- Follow REFERENCE_ARCHITECTURE.md and IOP_HEAP_CAPACITY_PLAN.md. Inventory every translated image, export/import/data region, captured heap, disc workspace and thread/call stack together. The current fourth-heap layout is verified but leaves no room at the next captured pointer.
- Gather subsequent second-channel allocation evidence before selecting placements, to avoid moving one image for each new request. Assess compact placements in the existing 2 MiB address space and any low-memory ABI assumptions; do not infer free ranges from a retail capture alone or introduce a larger emulated RAM default.
- Choose and document a consistent ownership layout. Regenerate affected private images, update all owned imports/GP/context/address gates and rerun independent normalized module comparisons. Preserve original captured heap pointers and all first-channel behavior.
- Admit the fifth operation only after its complete rounded interval is disjoint. Keep epoch, phase, overlap and no-mutation rejection tests. Then execute the original second-channel handler and follow its bounded request group through generated cleanup.
- Keep the first AutoDMA transaction pending until independently established consumption/completion behavior is implemented. Never replace an original callback or second-channel result with captured success bytes.

This is the next address-space/design scope. General IOP allocation, cross-CPU scheduling, audio output, archive payload loading and presentation remain unfinished.


## Confirmed bounded implementation (2026-10-06)

New original request captures 36..42 and a terminal original control-return-40 capture establish the second command-2 slot, sixth allocation (size 0x4040, pointer 0x192000), command 0x40A [0x10, 0x192000, 0x4040] and five command-0x408 pairs. The fifth rounded interval is [0x191700, 0x192000); the sixth is [0x192000, 0x196100). Attempts to capture later calls 43/44 timed out; they establish no subsequent request or allocation.

Inventory confirmed authored gaps between existing images. FILEIO now occupies [0x11000, 0x15280), MODSESQ2 [0x54000, 0x57CD4), CDVDSTM [0x66000, 0x6AC00). All three include relocated code/data/import/export/BSS ownership. The diagnostic kernel's only low ABI words are 0/4/8; providers execute authored host contracts rather than a retail kernel image. Earlier logical SIF snapshot RAM is a separate array and is not translated IOP RAM. Other images, captured heap pointers, disc workspace and worker stacks retain their ownership. No RAM expansion or heap-result substitution is used.

Both relocated resident modules pass independent normalized startup comparisons; original translated FILEIO executes both allocation handlers. The direct service test completes the entire confirmed group, checks stale epochs/wrong inputs/collisions before mutation, and retains the first pending AutoDMA transaction and parked child. Constructed pair objects match the independent final retail capture after pointer normalization. Slot offset 8 and offsets 48..64 are live worker state and are excluded from constructor-state comparison; native audio consumption/scheduling is not equated with retail.

Use verify_cri_second_channel.py for original hook/publication/RAM provenance, both allocation results, all five inputs/results, constructed objects and generated EE cleanup. Continue from the actual native frontier; these captures do not establish later asset or rendering behavior.


## Full EE checkpoint versus service-only coverage

The full generated EE run completes 37 CRI calls (both stream slots and 0x40A), both captured allocations and main-thread continuation. All five 0x408 objects are compared using the separately executed original IOP service test, not claimed as full EE call completions. Original captures reach these calls with additional thread lifecycles; the native worker sequence and pending reverse RPC require EE_SOUND_SCHEDULING_PLAN.md. Use `verify_cri_second_channel.py --native work/bt2-generation/entry-stop.json --service-ram work/iop/second-channel-native.bin` to keep these scopes explicit.
