# LOADFILE and IOP module loading

## Reached boundary

Post-reset CD initialization and media-mode setup return. Main then reaches FUN_00333B48 (returned) and the chain FUN_00337C28 -> FUN_00337A00 -> FUN_00337228. These three functions remain partial at a bind to service 0x80000006, client 0x003ACC00.

Checked boundary: syscall 0x77 at PC 0x0032F028, SP/descriptor 0x01FFEB50, source 0x203A97C0, destination 0x1E640, size 64, flags 0x44. Request RPC id 7, bind command 0x80000009. Native probe: 400 calls / 391 returns. The bind semaphore is fresh ID 7, count zero/max one; no loader reply is supplied.

Pinned public PS2SDK loadfile.c identifies 0x80000006 as LOADFILE and separates binding from load/start/stop/unload and buffer-load requests. This supports the service interpretation; it does not establish BT2 module names or function symbols. Keep function names neutral.

## Implementation sequence

1. Read REFERENCE_ARCHITECTURE.md and pinned BT3 IOP loader/import/scheduler guidance first. Audit our actual pinned PS2Recomp R3000/IRX capabilities before choosing a native-module pipeline; do not assume BT3 tooling features exist here.
2. Capture the unique loader caller/bind and subsequent first request without removing earlier boot/service execution. Validate fresh client, IOP service/queue, request operation, module path or buffer, return values and effects. Inspect available local disc/module evidence only; never download proprietary modules.
3. Identify the minimal requested module set and dependencies from evidence. Keep any extracted IRX files, module metadata derived from binaries, relocation output and generated code ignored. Establish owned IOP allocation and module lifetime separately from transport and existing CD clients.
4. Plan and implement the required ELF/IRX validation, relocation, import/export resolution and entry dispatch. Adapt BT3 architecture to BT2 observations. Choose a narrowly validated loader diagnostic only if its effects are established, and label it explicitly; do not invent successful load IDs or skip module entry/import effects.
5. Continue through actual requests and entry blockers until the next distinct subsystem or evidence limit. Test malformed module/bounds/lifetime cases and reached native execution, update progress/history, commit and push.

No native IOP reboot, full IOP emulator, displayed frame or complete game boot is established. The existing service adapter is scoped evidence delivery, not a replacement for module execution.

## Toolchain audit and next implementation scope

Fresh LOADFILE bind/version evidence and the first module path are now established. The first local IRX has a bounded provisional map and relocation comparison against the reference loaded module. The pinned recompiler lacks the R3000/import backend described by BT3. Follow [native IOP execution](IOP_NATIVE_EXECUTION_PLAN.md) for that new architecture scope; do not acknowledge module loading from the captured result alone.
