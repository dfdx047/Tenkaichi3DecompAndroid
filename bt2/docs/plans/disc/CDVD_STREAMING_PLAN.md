# Bounded CDVD streaming startup

Target the reached CDVDSTM load after ten validated residents. Follow the native module/import pattern in REFERENCE_ARCHITECTURE.md and consult pinned SDK CDVDMAN/IOMAN/timer definitions before adding providers.

1. Generate the supplied local module at an owned base, compile/link and run its actual entry.
2. Resolve only reached initialization imports using owned state or independently captured reference results. Retain callback addresses/GP and created objects; do not turn callbacks into successful transfers.
3. Capture the unique EE return and compare resident result, relocated code, normalized startup data and object/driver ownership before acknowledgement.
4. Continue through related startup calls and module loads. Actual sector reads require validated local ISO bounds, command/request semantics, destinations and completion evidence. Timer/alarm waits require owned scheduling; do not return ready or complete disc reads without evidence.
5. Stop only at a new major subsystem, unobtainable evidence/access or a speculative change. Build/test, record limits, commit and push.

Streaming transfers and asset loading are not implemented by registering the driver. Keep all captures/images/derived code under ignored work/.

## Verified startup checkpoint (2026-10-05)

Steps 1-3 complete for CDVDSTM residency: actual result 2, registered cdrom_stm driver, count-1/max-1 semaphore, and external CDVD interrupt event. SC(-11) returns that owned handle without dereferencing its parameter; local pre/post-load evidence establishes attr 2 / option 2 / bits 0x29. No new event transition or disc completion is produced. Local normalized comparison guards those objects and all module code/data/BSS. Actual streaming commands/timer callbacks/sector reads remain future execution boundaries. Continued through SOUNDS and into CRI_ADXI; the next reached major subsystem is SPU2 hardware initialization.
