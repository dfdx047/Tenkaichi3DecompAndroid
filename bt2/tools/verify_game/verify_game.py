"""Read-only preliminary validation of a locally supplied BT2 executable."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def validate(executable: Path, system_cnf: Path | None, expected_hash: str | None) -> dict:
    with executable.open("rb") as source:
        header = source.read(52)
        if len(header) != 52:
            raise ValueError("File is shorter than an ELF32 header.")
        if header[:7] != b"\x7fELF\x01\x01\x01":
            raise ValueError("Expected an ELF32 little-endian file with identification version 1.")
        file_type, machine, version = struct.unpack_from("<HHI", header, 16)
        header_size = struct.unpack_from("<H", header, 40)[0]
        if (file_type, machine, version, header_size) != (2, 8, 1, 52):
            raise ValueError("Expected an executable MIPS ELF32 header (version 1, size 52).")
        if executable.name.upper() != "SLUS_214.41":
            raise ValueError("Expected the local executable filename SLUS_214.41.")
        digest = hashlib.sha256()
        digest.update(header)
        size = len(header)
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
            size += len(chunk)
    actual_hash = digest.hexdigest()
    if expected_hash is not None and actual_hash != expected_hash.lower():
        raise ValueError("SHA-256 does not match the supplied reference hash.")

    boot_reference = None
    if system_cnf is not None:
        text = system_cnf.read_text(encoding="ascii")
        matches = re.findall(r"^\s*BOOT2\s*=\s*(.*?)\s*$", text, re.MULTILINE | re.IGNORECASE)
        if len(matches) != 1 or not re.fullmatch(
            r"cdrom0:\\SLUS_214\.41;1", matches[0], re.IGNORECASE
        ):
            raise ValueError("SYSTEM.CNF must contain one BOOT2 reference to cdrom0:\\SLUS_214.41;1.")
        boot_reference = matches[0]
    return {
        "executable": str(executable.resolve()),
        "size_bytes": size,
        "sha256": actual_hash,
        "format": "ELF32 little-endian MIPS executable",
        "boot_reference": boot_reference,
        "reference_hash_matched": expected_hash is not None,
        "status": "preliminary validation passed",
        "limitation": "Filename, boot reference, and basic header do not authenticate the retail revision. No ELF tables or game code were analyzed.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--system-cnf", type=Path, help="Defaults to sibling SYSTEM.CNF if present.")
    parser.add_argument("--expected-sha256", help="Optional trusted reference SHA-256; never inferred from filename.")
    args = parser.parse_args()
    if args.expected_sha256 and not re.fullmatch(r"[0-9a-fA-F]{64}", args.expected_sha256):
        parser.error("--expected-sha256 must contain exactly 64 hexadecimal characters")
    system_cnf = args.system_cnf
    if system_cnf is None:
        sibling = args.executable.parent / "SYSTEM.CNF"
        if sibling.is_file():
            system_cnf = sibling
    try:
        report = validate(args.executable, system_cnf, args.expected_sha256)
    except (OSError, UnicodeError, ValueError) as error:
        print(json.dumps({"status": "validation failed", "error": str(error)}, indent=2))
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
