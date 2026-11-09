"""Cross-check entry candidate control transfers against raw ELF instruction words.

This is a limited static audit, not a CPU interpreter or semantic validator.
Reports remain local; no candidate is automatically promoted to reviewed.
"""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]


def control(word, address):
    """Decode only the control encodings needed by the current entry candidate."""
    opcode = word >> 26
    if opcode in (2, 3):
        target = ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
        return {"kind": "j" if opcode == 2 else "jal", "target": target, "delay_slot": True}
    regimm = opcode == 1 and (word >> 16) & 31 in (0, 1, 2, 3, 16, 17, 18, 19)
    if opcode in (4, 5, 6, 7, 20, 21, 22, 23) or regimm:
        immediate = word & 0xFFFF
        if immediate & 0x8000:
            immediate -= 0x10000
        names = {4: "beq", 5: "bne", 6: "blez", 7: "bgtz",
                 20: "beql", 21: "bnel", 22: "blezl", 23: "bgtzl"}
        regimm_names = {0: "bltz", 1: "bgez", 2: "bltzl", 3: "bgezl",
                        16: "bltzal", 17: "bgezal", 18: "bltzall", 19: "bgezall"}
        rt = (word >> 16) & 31
        kind = regimm_names[rt] if regimm else names[opcode]
        return {"kind": kind, "target": address + 4 + immediate * 4,
                "delay_slot": True, "branch_likely": opcode >= 20 or regimm and rt in (2, 3, 18, 19)}
    if opcode == 0 and word & 63 in (8, 9):
        return {"kind": "jr" if word & 63 == 8 else "jalr",
                "register": (word >> 21) & 31, "delay_slot": True}
    if opcode == 0 and word & 63 == 12:
        return {"kind": "syscall", "code": (word >> 6) & 0xFFFFF, "delay_slot": False}
    # Other branch families are deliberately unsupported in this limited audit.
    if opcode in (16, 17, 18) and (word >> 21) & 31 == 8:
        raise ValueError(f"Unsupported control encoding at 0x{address:08X}")
    return None


def audit(data, metadata, report, allow_return=False):
    if report["executable_sha256"].lower() != hashlib.sha256(data).hexdigest():
        raise ValueError("Export and executable hashes differ.")
    if report["language"] != "r5900:LE:32:default":
        raise ValueError("Export has the wrong processor language.")
    entry = metadata["entry_point"]
    candidates = [row for row in report["functions"] if row["start"] == entry]
    if len(candidates) != 1 or not candidates[0]["contiguous_aligned"]:
        raise ValueError("Expected one contiguous aligned entry candidate.")
    candidate = candidates[0]
    end = candidate["end_exclusive"]
    if end <= entry or (end - entry) % 4 or candidate["body_size"] != end - entry:
        raise ValueError("Invalid entry body range.")
    sections = [section for section in metadata["sections"]
                if section["flags"] & 4 and section["occupies_file_bytes"]
                and section["address"] <= entry < end <= section["address"] + section["size"]]
    if len(sections) != 1:
        raise ValueError("Entry body is not uniquely backed by an executable section.")
    section = sections[0]
    instructions = report["entry_instructions"]
    addresses = [int(row["address"], 16) for row in instructions]
    if addresses != list(range(entry, end, 4)):
        raise ValueError("Entry instruction coverage is incomplete or duplicated.")
    transfers = []
    for row, address in zip(instructions, addresses):
        offset = section["file_offset"] + address - section["address"]
        word = struct.unpack_from("<I", data, offset)[0]
        decoded = control(word, address)
        flows = [int(value, 16) for value in row["flows"]]
        if decoded is None:
            if flows or row["flow_type"] != "FALL_THROUGH":
                raise ValueError(f"Unaccounted Ghidra transfer at 0x{address:08X}")
            continue
        expected_flows = [decoded["target"]] if "target" in decoded else []
        if flows != expected_flows:
            raise ValueError(f"Raw/Ghidra target mismatch at 0x{address:08X}")
        if decoded["delay_slot"] and address + 4 not in addresses:
            raise ValueError("Candidate omits a transfer delay slot.")
        if decoded["kind"].startswith("b") and not entry <= decoded["target"] < end:
            raise ValueError("Conditional branch leaves the entry candidate.")
        transfers.append({"address": address, **decoded})
    terminal = next((row for row in transfers if row["address"] == end - 8), None)
    external_jump = terminal is not None and terminal["kind"] == "j" and not entry <= terminal["target"] < end
    return_jump = terminal is not None and terminal["kind"] == "jr" and terminal["register"] == 31
    if not (external_jump or allow_return and return_jump):
        raise ValueError("Expected a final external jump or allowed return with included delay slot.")
    return {"status": "limited static control audit passed; boundary review recorded separately",
            "sha256": report["executable_sha256"], "name": candidate["name"],
            "start": entry, "end_exclusive": end, "instruction_count": len(addresses),
            "transfers": transfers,
            "limitations": "No full opcode/semantic verification, execution, syscall identification, or callee boundary review."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "rom/SLUS_214.41")
    parser.add_argument("--export", type=Path, default=ROOT / "work/ghidra/exports/analysis.json")
    args = parser.parse_args()
    try:
        spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
        inspector = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(inspector)
        data = args.executable.read_bytes()
        result = audit(data, inspector.inspect(data), json.loads(args.export.read_text(encoding="utf-8")))
        folder = ROOT / "work/ghidra/review"
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / "entry-control.json"
        path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(f"Entry control audit passed: {result['instruction_count']} instructions, {len(result['transfers'])} transfers. Report: {path}")
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, f"Entry audit stopped: {error}\n")


if __name__ == "__main__":
    main()
