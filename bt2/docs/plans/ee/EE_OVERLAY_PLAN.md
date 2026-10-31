# EE overlay: DBZP.BIN

## Why this is a new subsystem

Main's first frame step (`0x1002D8`) loads `cdrom0:\BIN\DBZP.BIN;1` through the C library's file API, and main then calls straight into it (`jal 0x3B7FA8` at `0x100688`, right after the load step returns). The native FILEIO path now loads the image byte-identically to the original (`verify_dbzp_load.py`). The call target is not in the executable: every analysis, map and generation tool today covers only the ELF's file-backed `.text`. Running the loaded code needs a second, separately generated code segment that is valid only while the loaded bytes match.

## Evidence (2026-10-07)

- `DBZP.BIN` is the only file in `BIN/`: LBA `0x64C`, 0x7D48F bytes, loaded at EE `0x3B3100`. That is just past the executable's last section (`.vubss` ends at `0x3B3038`), so the image is linked for a fixed address; it is not a relocatable module.
- It is raw R5900 code and data with no header: the first word is a function prologue (`addiu sp,sp,-64`), and it uses absolute `lui` references inside its own range (for example `0x42xxxx`).
- Main links against it directly (`jal 0x3B7FA8`), so the executable and the overlay form one program. Calls in both directions are expected.
- The loaded bytes equal the ISO file and the `rpc-dbzp-loaded` capture (all 0x7D48F bytes).

Open questions:
- whether the overlay's zero-initialized data extends beyond the file size;
- whether any other code is later loaded at the same address.

Both get checked against later captures, not assumed.

## Bounded design

1. **Local image.** A tool extracts `DBZP.BIN` from the local ISO, checks its pinned hash and size, and wraps it as an ignored local program with one executable section at `0x3B3100`. Nothing derived from it is committed.
2. **Analysis and map.** Run the existing neutral Ghidra discovery on the wrapped program, starting from observed entries such as `0x3B7FA8`. Then prepare a separate overlay candidate map with the same structural quarantine rules as `prepare_map.py`: contiguous bodies, unreachable padding, proven switch tables. The map's section metadata comes from the wrapped image, not from the ELF.
3. **Generation.** Generate overlay functions into a separate ignored output with their own receipt. Their calls into executable code use the existing dispatch, and executable calls into the overlay use the overlay table.
4. **Runtime guard.** Register overlay functions only behind a load guard: dispatch into `[0x3B3100, 0x43058F)` is allowed only while the EE bytes in that range equal the image the code was generated from. The guard is checked when the load completes and before each dispatch while the range is unchanged. Any later write into the code range invalidates it as a checked boundary.
5. **Selection loop.** `inspect_blocker.py` and `analyze_game.py --discover-blocker` classify targets inside the overlay range against the overlay program and map instead of reporting an ELF map gap. Expand and select overlay entries like executable entries.

## Limits and stop conditions

- Do not assume the BT3 project's overlay format or base. Its overlay tooling is a structural reference only ([REFERENCE_ARCHITECTURE.md](../../reference/REFERENCE_ARCHITECTURE.md)).
- Stop if the overlay is modified or replaced at runtime, or if it needs relocation, without evidence of how.
- Generated overlay code, the wrapped image and its maps stay in ignored `work/` folders.

## Progress (2026-10-07)

All five design steps are implemented:
1. The wrapper and identity check live in `tools/ps2recomp/overlay_programs.py`.
2. Ghidra uses a separate project and exports; the separate map has 380 entries.
3. Generation goes into `game/generated/dbzp`.
4. Overlay bindings are registered with their exact bytes and checked on every dispatch.
5. The selection loop classifies overlay targets.

Two overlay entries run (`0x3B7FA8`, `0x3BC310`) and call back into executable code. Their state matches the original at `rpc-overlay-dispenv` (`verify_overlay_dispenv.py`). The overlay has not been modified or replaced at runtime so far. Execution continues in the [frame GIF plan](../graphics/FRAME_GIF_PLAN.md).
