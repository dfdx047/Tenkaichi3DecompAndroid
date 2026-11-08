"""Run the generated handler against a user-captured raw 32 MiB EE RAM dump."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from run_pipeline import ROOT, verify_generated


def extract_kernel(data):
    if len(data) != 32 * 1024 * 1024:
        raise ValueError("Expected raw 32 MiB EE RAM, not a BIOS ROM or savestate.")
    # Handler reads a word at its upper bound before testing the loop condition.
    return data[:0x80004]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    parser.add_argument("--capture-note", required=True, help="Record PCSX2 version and capture point/timing")
    args = parser.parse_args()
    verify_generated()
    if args.dump.stat().st_size != 32 * 1024 * 1024:
        parser.error("Expected raw 32 MiB EE RAM capture.")
    data = args.dump.read_bytes()
    snapshot = extract_kernel(data)
    folder = ROOT / "work/bt2-generation"
    path = folder / "kernel-snapshot.bin"
    path.write_bytes(snapshot)
    manifest = {"source": str(args.dump.resolve()), "source_sha256": hashlib.sha256(data).hexdigest(),
                "capture_note": args.capture_note, "physical_start": 0, "size": len(snapshot),
                "snapshot_sha256": hashlib.sha256(snapshot).hexdigest(),
                "limitations": "User-provided capture, not authenticated; diagnostic handler execution only."}
    (folder / "kernel-snapshot.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    subprocess.run([str(ROOT / "build/probe/Debug/bt2-kernel-handler-tests.exe"), str(path)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
