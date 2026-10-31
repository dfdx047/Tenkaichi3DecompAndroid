# Memory cards and saves

## Plan of 2026-10-13: folder cards behind the native card service

The sections further down describe the earlier model, in which the original IOP modules ran and an empty slot was answered on the SIO2 bus. No IOP code runs any more: the game's card requests arrive at the native card service (`runtime/services/memory_card_service.cpp`) as library calls with the public parameter layout. Today it answers the initialization and the status query, with "no card".

At that level a card does not have to be a card image. **A card is a host folder** (`saves/<name>/` next to the programs; `user/memcards/<name>/` before 2026-10-19) with one sub-folder per save, holding the save's files as the game wrote them, plus `entries.txt` with what the host file system cannot keep: each entry's mode and its two time stamps in the card's own form. This is the smallest layer that gives the game a card, it keeps saves visible and copyable for the player, and it is the idea of PCSX2's folder cards (whose own folders can be read later; the user has one in `work/memcards/`).

**Steps:**

1. **Bringing a save in — done for the CodeBreaker format.** `tools/saves/import_save.py SAVE.cbs CARD_FOLDER` unpacks a `.cbs` file into a card folder (header, RC4 from a fixed starting permutation, zlib, then files with 64-byte headers; format as documented by mymc, public domain, which is the behavioural reference). Damaged files are refused by what is wrong; an existing save is never overwritten. The user's save (`work/100%_savefile/`, 14.8 KB) unpacks to its three files (the icon description, the icon and 16,384 bytes of save data) in `user/memcards/Card 1/`.
2. **The service answers from a card folder — reading done 2026-10-13** (`runtime/services/card_folder.h`, `memory_card_service.cpp`; the folder comes from `BT2_CARD_FOLDER`, which the game program sets from the settings). The status query reports a formatted PS2 card (type 2, free space in 1 KB clusters, format flag; -1 on the first query, then 0). The card folder sits in port 0; port 1 (memory card slot 2, which Dragon Tournament asks about) answers as an empty port (-11, type 0), as the original answered for an empty port; a multitap slot stops the run (`runtime/services/memory_card_info.h`, 2026-10-19). Implemented from the public library's parameter layout: open for reading (2), close (3), seek (4), read (5), change directory (12), list (13). Every other request stops the run naming its number and parameter words.
   - **Seen on the scripted run with the user's save:** the game opens and reads the save file at startup, shows no "no memory card" dialogue, applies the save's sound option (SOUNDS request `0x60`, now accepted) and runs to the title screen and a demo fight (5,000 frames), which shows other characters than without the save.
   - **Not checked:** the directory listing and change-directory requests (no run has made them yet; their reply layout is the public one), the replies against a PCSX2 capture, and what the game shows as unlocked (the user's to judge).
   - **Read alignment:** the original passes the unaligned head and tail of read data through the end parameter; the service writes all of it to the buffer and reports both sizes as zero.
3. **Launcher — done 2026-10-13, folders since 2026-10-19:** the Memory card screen lists the card folders in `saves` with their save counts and puts one, or none, in slot 1 (`memory_card` in the settings). Since the same day the slot is never empty: the game program makes a new card folder ("Card N", the lowest number not taken) when none is chosen or the chosen one is gone, the screen can make one too. Since 2026-10-19 only the `saves` folder is listed (no folder dialog); a card moved in appears by itself.
4. **Saving — implemented 2026-10-13, not yet exercised by a run:** open for writing (flags: write 2, create `0x200`, truncate `0x400`), make a folder (open with flag `0x40`, as the public library does), write (6: up to 16 unaligned head bytes in the parameter from +32 with their count in the origin word, the aligned rest in the buffer), flush (`0xA`), set an entry's data (`0xE`: created 1, modified 2, mode 4, from one 64-byte listing entry), delete (`0xF`: a file, or an empty folder). An open file is held in memory; a flush or close writes a temporary file and renames it over the old one, and `entries.txt` gets the entry's mode and time stamps (the card's 8-byte form, Japanese time). A full card (8,000 clusters of 1 KB) is reported, open flags outside the known ones stop the run, and so does every request number not listed here (format, unformat, rename and others).
   - **To check with the user's play:** that a save made by the port is written without a stop and read back by the game at the next start; the result codes the game expects where they differ from the public library's.
5. **Other sources:** PCSX2 folder cards as they are; raw 8 MB card images (a parser for the card's own file system) and the other exchange formats (`.psu`, `.max`, `.xps`, `.sps`) when the user needs them; importing from the launcher. Unknown formats are refused, never guessed.

**Verification:** tests for each request on a synthetic card folder; a save written by the port read back; the user's save loaded by the game and judged by the user (what is unlocked); a PCSX2 capture of the same requests for comparison where the replies can be captured.

---

## Earlier model (IOP modules): absent cards


Memory card requests run through the original IOP modules (MCSERV, MCMAN, SIO2MAN). On the SIO2 bus, the card ports answer as an empty slot. This is the device policy `sio2_card_absent` in `runtime/iop/core/native_runtime.h`.

**Startup query.** MCSERV's GetInfo for port 0 / slot 0 calls MCMAN's card detection. Because the card type stays "no card", every detection runs the PS2 probe and then the PS1 probe, matching the public `mcman_detectcard`:
- five `0x11` DMA probe packets and one `0xF3` authentication packet, each 144 bytes;
- a 300 µs delay;
- five `81 52 00` FIFO probe packets, each guarded by a 100 µs alarm.

**Frame-phase queries (2026-10-08).** After the controller check, the game polls the card every frame with a no-wait GetInfo that requests the type, free space and format (end callback `0x31DD60`, end parameter `0x3A7480`). Each query repeats the probe sequence above, so the card path checks its counters relative to the start of each query.

Two pieces of state carry over between queries:
- **Pending alarm.** The last timeout alarm of a query is still pending when MCSERV returns. On hardware it fires 100 µs later and signals the card semaphore. Logical IOP time therefore advances by one field at each VBlank, and a pending card alarm fires then, in interrupt context.
- **Restart write.** SIO2MAN's next transfer writes `ctrl|0xC`. The control register still reads back `0x3BD`, so that write is a restart over the consumed reply of the previous query's last packet.

**Original comparison.** The capture `rpc-mc-frame-callback`, taken at the first frame-phase end callback, shows:
- reply −11;
- type 0, free 0;
- end-parameter argument `0x3A7480`.

`verify_mc_frame_query.py` compares it with the native report.

## Later: real cards and save import (requested)

Players must be able to bring saves from anywhere:
- **Emulator card images:** PCSX2 `.ps2` 8 MB images and other emulators' raw card dumps.
- **Saves exported from an original console:** the common exchange formats `.psu`, `.max` (Action Replay Max), `.cbs` (CodeBreaker), `.xps` (X-Port) and `.sps` (SharkPort).
- **Saves downloaded from the internet:** in the same formats.

The port does not need to work the way PCSX2 does. The design:

1. **Card backend.** A mounted-card device answering the SIO2 card protocol (PS2 card commands, page reads/writes, erase, ECC), backed by an 8 MB card image on the host. It replaces the absent-card policy per slot.
2. **Import/export layer.** Converts single-save exchange formats into directory entries on the virtual card, and exports them back out. It validates headers and checksums, and refuses unknown formats instead of guessing.
3. **Persistence.** Writes the host card file atomically after each guest write sequence, so that a crash can't corrupt a save.
4. **Verification.**
   - Compare format, directory and file results from GetInfo and GetDir with PCSX2 captures that use a formatted card and a card holding a BT2 save.
   - Round-trip each exchange format through import and export.

This begins once gameplay reaches the save menus. Until then, frames keep using absent cards for repeatability.
