# IOP heap startup boundary

## Verified service identity

Correction to the previous file I/O plan: RPC server 0x80000003 is the IOP heap service hosted inside FILEIO. Ordinary file I/O uses a different server. The pinned SDK heap RPC handler supports allocation (1), free (2) and load-to-heap (3); these public conventions do not establish BT2 request payloads or successful outcomes.

BT2's generated FUN_00336CD8 binds this server through client 0x3AC840. A unique local caller-return capture at 0x1005A4 establishes return 0, server 0x48438, buffer 0x48480 and queue 0x48420. FILEIO at 0x44430 matches the supplied IOPRP image's relocated code and module ownership. The tagged heap worker is asleep on RPC, priority 96, entry 0x46D3C, stack 0x1FA600/0x800, GP 0x4F710.

The optional EE adapter supplies only the verified reference-backed service lookup and executes the generated RPC_END callback. This does not execute the boot FILEIO worker natively. A separate translated handler now executes the first validated allocation; file loading remains unsupported. Thirteen separately recompiled native modules remain the native resident count.

## Completed bounded request

The first captured request allocates 0x360 bytes and returns 0xCE500. Boot FILEIO is translated locally at 0x140000; only its actual heap handler and helper execute. The scoped AllocSysMemory provider accepts mode 0, size 0x360, null requested address once, maps an owned nonoverlapping 0x400-byte block (256-byte rounding), then is removed. Initial allocation contents are undefined and are not equated with reference fill.

Generated EE copies 864 bytes from 0x369F90 into that block. Reference destination equals source; native destination matches independently. The adapter supplies an owned logical transfer token and one completed status query after synchronous copying, with no physical DMA timing claim. Source payload is an embedded IRX, not an ordinary asset file.

Native heap client, handler result, allocation, copy and completion are compared. Other sizes/modes, repeat allocations/transfers and stale epochs reject. Boot FILEIO module entry/worker startup, general allocator/free and file I/O remain unimplemented. Thirteen resident modules remain the count.

## Next boundary

Generated startup reaches LOADFILE operation 6 for the copied buffer. Follow [memory-backed module loading](MODULE_BUFFER_STARTUP_PLAN.md), validating identity, relocation and entry effects before residency. Continue through related blockers, test/update progress and commit/push at pass end. Inputs, captures and generated code remain ignored; unknown functions retain neutral names.


## First source-block cleanup verified

After the embedded module's nonresident return, generated FUN_00336EC8/FUN_00336E50 requests heap operation 2 for the owned 0xCE500 block. Unique retail return at 0x320650 records result 0. The translated FILEIO handler executes 49 further IOP instructions, invokes a scoped FreeSysMemory provider once and relinquishes the full rounded 0x400-byte source range. Stale epochs, wrong pointers, repeated free and accesses to released ownership reject. This completes only the first allocation/copy/load/free lifecycle; general allocation/free and FILEIO boot/worker execution remain unsupported. Continue at VIF_STARTUP_PLAN.md.

## Later capacity handoff (2026-10-05)

After the completed first lifecycle, VIF/GS setup and generated EE pool allocation, a second request asks for 0x81800 bytes. Retail reuses 0xCE500; that larger interval overlaps the authored translated FILEIO placement. See [capacity/layout plan](IOP_HEAP_CAPACITY_PLAN.md). The original first-allocation/free checks remain intact; native does not yet accept the second request.
