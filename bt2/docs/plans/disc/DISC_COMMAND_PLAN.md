# Bounded first disc command and sector read

1. Capture the reached 0x80000595 bind and actual semaphore/return. Validate the disc-matched CDVDFSV handler, buffer and GP; preserve boot-service limitations.
2. Capture the first real command, mode, sector range, destination and callback. Use local pinned BT3 ownership patterns and public CDVD/SIF contracts; adapt to BT2 observations.
3. Execute the original translated command handler with scoped imports. Supply read-only local ISO bytes only for validated ranges and modes, retaining buffer ownership and completion state. Do not substitute a captured asset payload.
4. Verify original secondary transfers and primary completion against isolated retail evidence; follow generated EE callback/cleanup and related command paths.
5. Continue bounded startup requests until a new major subsystem, inaccessible evidence or a speculative next step. Test, update progress/history, commit and push. Keep all proprietary inputs/captures/generated code ignored.

An accepted asynchronous command is not proof of completed sectors. No boot-entry residency, general CDVD hardware, arbitrary timing or rendering is inferred.

## Owned first read (2026-10-06)

The bind/readiness path completes. The first asynchronous command-1 request is RPC 46, LBA 0xE2A / two sectors / destination 0x39DE80, packed mode 0x100, generated callback 0x31ECF8 with argument 0x3694C0.

Independent original-import captures establish sceCdRV ordinal 63, 2064-byte DVD records (12-byte header, 2048 ISO bytes, four trailing bytes), original IOP callback entry/return, Sync(32)/Sync(5), error query, timeout registration/cancellation, completed DMA3 state, sector descriptors, 144-byte completion buffer and initial/final progress submissions. The pinned public SDK supplies shared-state, FSV-buffer and synchronization contracts.

The bounded implementation owns an inactive workspace, 42128-byte FSV buffer, completion event, request state and timeout. It reads the requested local ISO range before acceptance. The original IOP callback runs only after successful backend read at the synchronization boundary and sets the owned event. Original translated CDVDFSV code submits two sector descriptors, progress 0, the callback buffer, then progress 4096. Logical transport completes these owned copies; physical PCSX2 polling counts/timing are not equated. The original generated EE callback and cleanup execute; no captured payload or callback effects are substituted.

Generated directory parsing consumes the real index and tokenizes its line endings. The first archive metadata request is \DATA\ZS2US_1.AFS;1; an independent original search capture reports LBA 0x1FC26 / size 0x419B9800 / result 1. Independent original search captures also confirm ZS2US_2.AFS, ZS2USOP.SFD, ZS2USED.SFD and LOGO_O.SFD. The bounded metadata batch follows the five names actually parsed from the loaded index, with local ISO extent validation and original handler/transport. Follow the reached archive path until a genuinely new ownership boundary.

Scope: one validated sector request and the index lookup and its five listed file lookups. This is auxiliary original-handler execution, not full CDVDFSV/CDVDMAN boot residency, generic asynchronous scheduling, error recovery, streaming, native audio or presentation. General archive loading needs separately observed request, buffer and completion ownership.
