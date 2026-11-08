# Native IOP execution plan

## Why this is a separate architecture scope

The pinned PS2Recomp revision `c5a9d02573410a2085a4b4b831b0b68ba3515440` has no R3000 architecture selector or IRX import dispatch. Its configuration is EE-only and its emitter uses `R5900Context`. BT3's R3000 backend is an additional implementation, not a feature available in our pin. Reusing the EE ABI does not establish correct IOP execution.

LOADFILE bind and version query can be validated independently. A successful module-load reply requires actual module entry/import effects. The reference SIO2MAN load result is evidence, not a return value to replay as native success.

## Established inputs

- First BT2 request: LOADFILE RPC 0, 512-byte argument block, `cdrom0:\IRX\SIO2MAN.IRX;1`, no module arguments, eight-byte result.
- Local IRX equals the supplied disc file. Bounded metadata, neutral provisional map, imports, and private relocation preparation work in `tools/iop/inspect_irx.py`.
- All 239 relocation words and non-table code ranges match the independently captured reference module. Derived files remain under ignored `work/iop/`.
- Discovery currently seeds entry, export pointers and JALs. Pointer-only callbacks and indirect targets need discovery from relocation evidence or reached dispatch failures.

## Implementation sequence

1. Consult pinned BT3 R3000 emitter/import/scheduler code and public IOP specifications. Audit differences from our pinned PS2Recomp before choosing an isolated authored backend extension or a separately pinned dependency. Preserve the working MSVC EE pipeline.
2. Define a separate 32-bit IOP context, owned 2 MiB RAM, module allocation/lifetime, guest-address dispatch and checked imports. Validate branch delays, load delays, register zero, HI/LO and unaligned operations with synthetic programs. Do not build a general PS2 emulator.
3. Emit native C++ for validated MIPS-I instructions and explicit import stubs; reject unsupported instructions/targets. Keep generated IRX code/maps/images ignored. Preserve module-local neutral names and provenance.
4. Connect the validated relocation image to the owned IOP lifetime. Resolve imports only through implemented providers; unresolved ordinals stop explicitly. Extend discovery for entry-created threads and callbacks from evidence.
5. Execute the actual SIO2MAN entry with observed arguments and GP. Implement only reached library registration, interrupt, DMA, thread/event/semaphore and memory requests. Yield blocked threads with retained execution state; no unconditional success defaults.
6. Report LOADFILE success only after entry return and its effects are validated. Continue EE execution and native module/import blockers. SIO2 hardware/IRQ behavior is a separate major subsystem if entry or scheduled threads require it.

## Verification gates

- Synthetic translation/ABI tests pass; malformed IRX, relocation and unresolved import cases fail safely.
- Module RAM ownership is distinct from captured reference pointers and the older invalidated IOP lifetime.
- Local reference module entry/return, registration and thread effects match a recorded scenario.
- Full EE checkpoint pipeline and earlier GS/SIF/CD assertions still pass.
- Update current progress and history, review authored-only staging, commit and push.

## Execution checkpoint: 2026-10-04

Steps 1-4 now have an isolated authored prototype: separate 32-bit context, checked owned RAM, static C++ emission, delayed branches/loads, HI/LO, unaligned pairs and explicit import dispatch. It does not modify or claim R3000 support in upstream PS2Recomp. Synthetic compiled programs pass; unsupported instructions, targets, memory and imports stop explicitly. This is a diagnostic backend, not a general emulator or cycle-accurate IOP.

SIO2MAN runs at an owned base of 0x10000, with its relocated GP, argc 1 and argv containing the requested path (public modload startup convention). The private memory image must equal the image associated with compiled source. The first loadcore:6 call retains bounded exports/version in owned library state and applies the first-library linkage mutation; duplicate/version replacement and existing-client linking remain unsupported.

The next boundary is step 6's separate SIO2 hardware/IRQ scope: PC 0x10008 writes 0x3BC to physical 0x1F808268 after 44 instructions and three provisional range entries. Public PCSX2 identifies this value as SIO2MAN_RESET. No hardware write or module success was fabricated. Before implementing that scope, establish its reset/register ownership, gather before/after observations around reached accesses, and design retained IRQ/event/thread state. A plain register array or always-ready status is insufficient evidence for transfer completion. Then continue the actual entry imports and scheduled callbacks, keeping the EE LOADFILE request pending until effects are validated.

## Resident entry checkpoint

The reached reset/import setup is implemented and verified under [the scoped SIO2 plan](../input/SIO2_BRINGUP_PLAN.md). Actual entry returns result 0; its native worker waits on an unsatisfied event with retained context. Six actual entry targets are recorded, including the pointer-only worker discovered from reached CreateThread. IRQ callback registration alone does not count as execution. Follow [the EE lifetime bridge](IOP_LOADFILE_BRIDGE_PLAN.md) next. Transfer completion and general scheduling remain unsupported.
