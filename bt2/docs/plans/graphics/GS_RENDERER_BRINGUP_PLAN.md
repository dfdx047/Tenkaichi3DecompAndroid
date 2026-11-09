# First GIF drawing packet and renderer bring-up

## Observed boundary

The generated probe stages the first normal-mode GIF request and stops before CHCR start `0x101` at `0x003101A8`, SP `0x01FFEAF0`. Source is `0x01FFEB50`, QWC 15. Local reference captures establish an initially idle channel, DMA enable state, count programming and the helper's zero return after submission. That return does not establish transfer completion.

The complete source packet has one PACKED A+D tag and 14 register writes, including two XYZ2 kicks. Its observed primitive type is 6 (sprite), context 0. FRAME1 selects PSMCT32, width 640, base page 140, with framebuffer mask zero. XYOFFSET1 and SCISSOR1 plus the two vertices describe the 640 by 448 rectangle. This is an observed startup drawing request, not proof of a rendered or presented frame.

## Scoped implementation for the next pass

1. Consult REFERENCE_ARCHITECTURE.md and pinned BT3 command recording/presentation separation. Read pinned PCSX2 GS register, GIF packet, pixel layout and sprite behavior. Adapt only the behavior required by this BT2 packet; retain neutral function names.
2. Define an authored, transactional command sink for the reached single PACKED A+D form. Validate complete length/tag/address list before effects. Decode register state and vertex kicks in order; reject unsupported packets, formats, contexts and primitive behavior explicitly.
3. Establish the narrow PSMCT32 VRAM/framebuffer and untextured sprite operation needed by the observed first rectangle. Derive pixel layout, offsets, clipping, tests and masks from public definitions and local evidence. Do not silently discard unimplemented Z/test/blend effects or fabricate visible pixels. Gather local GS evidence when available; preserve unknowns when it cannot support a safe operation.
4. Keep native guest bytes intact. PRMODECONT, COLCLAMP and DTHE define only bit 0; generated code preserves stack residue in other bits. The packet comparison permits only those documented unused bits to differ. Every other tag/address/value remains strict. Use meaningful synthetic tests for semantic mutations, bounds, ordering, rectangle edges and rejected modes.
5. Once the sink actually consumes the supported request, define the reached DMA busy/count/address/completion contract separately, respecting DMA enable state and later polls. A captured helper return is not authorization to invent instant completion.
6. Rerun and follow generated execution through display setup and subsequent requests. Capture actual privileged GS display registers before adding presentation/window code. A cleared VRAM region or diagnostic image is not yet a presented game frame. Continue through supported blockers until the documented stopping conditions apply.

Implemented scope: transactional software consumption of the two observed PACKED A+D sprite packets, PSMCT32 color and PSMZ24 depth writes, clipping/masks and explicit unknown VRAM bits. Five new independent checkpoints establish first synchronization and the second source packet. A bounded logical completion sequence is reused for the second same-form transfer, with no intervening VIF/VU submissions; its elapsed timing is not established. The idle VU status checkpoint is read-only and does not implement VU execution.

Both clear helpers now return. The next reached boundary is [post-reset SIF transport initialization](../iop/POST_RESET_SIF_PLAN.md). No display register setup, presented frame, textures, blending, general GS packet handling or native video clock is implemented. Build/test meaningful steps, update PROG.md and history, then commit/push once at the end.

## References

- **[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs)** (see [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)): GS rendering reference for the primitive, depth, test and mask rules of the software rasterizer this plan introduced. Adapt its behavior; do not copy code.
