# Post-reset IOP service ownership and delivery

## Reached boundary

The second SIF initialization returns to main at 0x001003E8. Generated code enters the same CD-initialization path used before reboot, creates a new synchronous bind semaphore and submits command 0x80000009 for service 0x80000592. Checked syscall boundary: 0x77 at PC 0x0032F028, descriptor at SP 0x01FFEB90, source 0x203A97C0, destination 0x1E640, size 64 and flags 0x44. Probe: 305 calls / 298 returns.

The live post-sif-after IOP capture has a candidate public-layout server record at 0x43388, callback address 0x3E36C, receive buffer 0x441B8 and queue pointer 0x43358. The preceding lifetime's server address 0x3F570 now contains instructions. These are diagnostic observations, not established symbol names, atomic snapshots or proof of service dispatch.

Transport ownership is now fresh, but native reboot/module startup and service execution remain absent. This scope must establish service ownership and reply delivery independently of the transport handshake; never reactivate old bindings globally.

## Implementation sequence

1. Consult REFERENCE_ARCHITECTURE.md, pinned BT3 IOP guidance, public PS2SDK RPC layouts and PCSX2 SIF delivery. Inspect local module/queue evidence without importing another game's service implementation.
2. Add a unique later caller capture that preserves both SIF initializations and the reboot. Validate the fresh bind request, completed client/reply, semaphore effects and IOP service/queue windows with explicit stability and environment checks. Capture the subsequent CD-init call separately, with the same local disc mounted.
3. Decide the narrow execution model from evidence: an explicit epoch-owned diagnostic service adapter with validated effects, or native IOP module/import/scheduler work if the reached call requires it. Record the choice and limits before implementation. Keep service ownership separate from generic transport and from old checkpoint counters.
4. Deliver only the established request/reply, execute the actual generated EE callback and validate semaphore/client transitions. Unknown calls remain unsupported. Do not transplant the old 0x3F570/0x3FB20 pointers or claim a BIOS reboot occurred natively.
5. Continue through actual CD initialization, subsequent bindings and module requests until the next distinct subsystem or evidence limit. Test, update current/history progress, commit and push.

Display/presentation remains a separate incomplete scope. No title screen, gameplay or complete startup is claimed.

## Completed diagnostic service scope (2026-10-04)

Separate captures validate a null CD bind, a later successful CD bind, CD-init return, successful SCMD bind and media-mode return. Conditional shared-helper traps preserve the earlier invocation and the displaced branch/delay-slot behavior. Their authored extra ELF load segment remains local/ignored; process memory is only read.

The epoch-owned adapter delivers one logical null reply, one successful reply and the observed calls through generated EE RPC_END callbacks. It validates request/descriptor/client/semaphore state and preserves interrupted CPU/stack state. One real guest retry loop executes; its 2^20+1 iterations need a bounded two-million checkpoint allowance for this evidence mode. Physical retry counts and timing are not replayed.

CD service ownership is 0x43388 / buffer 0x441B8; SCMD ownership is 0x434F0 / buffer 0x439A8. CD init yields (1,0x225,0x225,0); mode 2 yields result 1. Old service pointers remain invalid, and no native IOP module or general disc backend is claimed.

Continued through four additional mapped functions to service 0x80000006: 400 calls / 391 returns. The next distinct subsystem is [LOADFILE/module loading](LOADFILE_IOP_MODULE_PLAN.md), whose reply/execution is not fabricated.
