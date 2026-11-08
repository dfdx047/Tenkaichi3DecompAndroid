"""Record raw startup syscall selectors and mapping evidence for the unresolved target.

No guest execution, dynamic writer tracing, or automatic semantic naming.
"""

import hashlib
import importlib.util
import json
import struct
from review_entry import ROOT

EXPECTED_SHA256 = "f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e"
TARGET = 0x003B7FA8


def main():
    data = (ROOT / "rom/SLUS_214.41").read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise ValueError("Executable differs from the reviewed candidate.")
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    metadata = inspector.inspect(data)

    def word(address):
        sections = [section for section in metadata["sections"] if section["flags"] & 2
                    and section["occupies_file_bytes"]
                    and section["address"] <= address < address + 4 <= section["address"] + section["size"]]
        if len(sections) != 1:
            raise ValueError(f"Address 0x{address:08X} is not uniquely file-backed.")
        section = sections[0]
        return struct.unpack_from("<I", data, section["file_offset"] + address - section["address"])[0]

    syscalls = []
    for address, selector in ((0x001001C8, 0x3C), (0x001001E4, 0x3D), (0x0032EEE4, 0x64)):
        # The preceding reviewed instruction is addiu v1, zero, selector.
        if word(address - 4) != 0x24030000 | selector or word(address) != 12:
            raise ValueError("Reviewed selector/syscall instruction pair changed.")
        syscalls.append({"address": address, "v1_selector": selector, "encoded_syscall_code": 0})
    # Independently reproduce the constants used by the entry's zero-store loops.
    pairs = ((0x0010011C, 0x00100124, 2), (0x00100120, 0x00100128, 3))
    bounds = []
    for upper_address, lower_address, register in pairs:
        upper, lower = word(upper_address), word(lower_address)
        if upper >> 16 != 0x3C00 | register or lower >> 16 != 0x2400 | register << 5 | register:
            raise ValueError("Reviewed zero-loop address construction changed.")
        immediate = lower & 0xFFFF
        if immediate & 0x8000:
            immediate -= 0x10000
        bounds.append(((upper & 0xFFFF) << 16) + immediate)
    literal_locations = []
    direct_calls = []
    for section in metadata["sections"]:
        if not section["flags"] & 2 or not section["occupies_file_bytes"]:
            continue
        for relative in range(0, section["size"] - 3, 4):
            value = struct.unpack_from("<I", data, section["file_offset"] + relative)[0]
            address = section["address"] + relative
            if value == TARGET:
                literal_locations.append({"address": address, "section": section["name"]})
            if section["flags"] & 4 and value >> 26 == 3:
                destination = ((address + 4) & 0xF0000000) | ((value & 0x03FFFFFF) << 2)
                if destination == TARGET:
                    direct_calls.append(address)
    result = {"sha256": EXPECTED_SHA256, "syscalls": syscalls, "target": TARGET,
              "target_in_declared_load_memory": any(load["virtual_address"] <= TARGET < load["memory_end_exclusive"] for load in metadata["load_segments"]),
              "zero_loop_bounds": bounds, "target_in_zero_loop_interval": bounds[0] <= TARGET < bounds[1],
              "aligned_exact_literal_occurrences": literal_locations, "encoded_jal_occurrences": direct_calls,
              "limitations": "Zero-store loop behavior is a static inference; subsequent writes and actual target contents are unknown. Encoded matches are not automatically pointers or reachable calls."}
    folder = ROOT / "work/ghidra/review"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "startup-evidence.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
