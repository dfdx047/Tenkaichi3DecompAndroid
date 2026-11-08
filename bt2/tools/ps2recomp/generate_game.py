"""Generate local BT2 C++ from an explicit reviewed or automated provisional map."""

import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
from build_tools import recompiler
import uuid

ROOT = Path(__file__).resolve().parents[2]
LOCAL_CANDIDATE_SHA256 = "f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e"


def validate_map(path, metadata):
    with path.open(encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames is None or [name.lower() for name in reader.fieldnames] != ["name", "start", "end", "size"]:
            raise ValueError("Function map must have exactly Name,Start,End,Size columns.")
        raw_rows = list(reader)
        if any(None in row or any(value is None for value in row.values()) for row in raw_rows):
            raise ValueError("Function map has missing or extra CSV fields.")
        rows = [{key.lower(): value for key, value in row.items()} for row in raw_rows]
    if not rows:
        raise ValueError("Function map is empty.")
    starts = set()
    ranges = []
    for row in rows:
        start, end, size = (int(row[key], 0) for key in ("start", "end", "size"))
        if start in starts or start % 4 or end % 4 or end <= start or end - start != size:
            raise ValueError("Function map has duplicate starts or invalid contiguous aligned ranges/sizes.")
        starts.add(start)
        ranges.append((start, end))
        if not re.fullmatch(r"FUN_[0-9a-fA-F]{8}", row["name"]) or int(row["name"][4:], 16) != start:
            raise ValueError("Initial BT2 function names must be FUN_<eight-digit-start-address>.")
        if not any(section["flags"] & 4 and section["address"] <= start < end <= section["address"] + section["size"] for section in metadata["sections"]):
            raise ValueError("Function range is outside an executable section.")
    ranges.sort()
    if any(current[0] < previous[1] for previous, current in zip(ranges, ranges[1:])):
        raise ValueError("Function map has overlapping ranges.")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    maps = parser.add_mutually_exclusive_group(required=True)
    maps.add_argument("--function-map", type=Path, help="Reviewed CSV with Name,Start,End,Size columns.")
    maps.add_argument("--candidate-map", type=Path, help="Automated provisional map from prepare_map.py; requires its manifest.")
    parser.add_argument("--executable", type=Path, default=ROOT / "rom" / "SLUS_214.41")
    parser.add_argument("--program", help="Generate a wrapped runtime-loaded overlay into its own output")
    args = parser.parse_args()
    try:
        if args.program:
            import overlay_programs
            if args.program not in overlay_programs.PROGRAMS:
                raise ValueError("Unknown overlay program.")
            _, data = overlay_programs.load(args.program)
            files = overlay_programs.paths(args.program)
            args.executable, identity = files["elf"], hashlib.sha256(data).hexdigest()
            folder, output = files["generation"], files["output"]
        else:
            if args.executable.stat().st_size != 2675936:
                raise ValueError("Input size differs from the locally inspected executable candidate.")
            data = args.executable.read_bytes()
            identity = LOCAL_CANDIDATE_SHA256
            folder, output = ROOT / "work" / "bt2-generation", ROOT / "game" / "generated" / "bt2"
        if hashlib.sha256(data).hexdigest() != identity:
            raise ValueError("Input differs from the locally inspected candidate; inspect and document the new file first.")
        spec = importlib.util.spec_from_file_location("bt2_inspector", ROOT / "tools" / "inspect_elf" / "inspect_elf.py")
        inspector = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(inspector)
        metadata = inspector.inspect(data)
        function_map = args.function_map or args.candidate_map
        validate_map(function_map, metadata)
        if args.candidate_map:
            manifest = json.loads(function_map.with_suffix(".manifest.json").read_text(encoding="utf-8"))
            if (manifest["status"] != "automated provisional EE map"
                    or manifest["executable_sha256"] != identity
                    or manifest["map_sha256"] != hashlib.sha256(function_map.read_bytes()).hexdigest()
                    or manifest["language"] != "r5900:LE:32:default"):
                raise ValueError("Candidate map provenance does not match.")
        build = ROOT / "build" / "ps2recomp-tools"
        tool = recompiler()
        provenance = json.loads((build / "bt2-tool-provenance.json").read_text(encoding="utf-8"))
        if provenance["lock_sha256"] != hashlib.sha256((ROOT / "config" / "ps2recomp.lock.json").read_bytes()).hexdigest() or provenance["recompiler_sha256"] != hashlib.sha256(tool.read_bytes()).hexdigest():
            raise ValueError("Tool provenance does not match the lock file; rebuild with build_tools.py.")
        folder.mkdir(parents=True, exist_ok=True)
        if not output.resolve().is_relative_to(ROOT.resolve()) or not folder.resolve().is_relative_to(ROOT.resolve()):
            raise ValueError("Generation paths must stay within the project root.")
        # Preserve prior output and start from an empty directory to prevent stale functions.
        if output.exists():
            if output.is_symlink() or output.resolve().parent != (ROOT / "game/generated").resolve():
                raise ValueError("Unexpected generated-output location.")
            output.rename(folder / f"previous-output-{uuid.uuid4().hex}")
        output.mkdir(parents=True)
        # Entry hints (map_gaps.py): addresses inside functions the generator makes labels of.
        hints_path = folder / "entry-hints.json"
        hints = sorted(json.loads(hints_path.read_text(encoding="utf-8"))) if hints_path.exists() else []
        if any(not isinstance(h, int) or h % 4 or h <= 0 for h in hints):
            raise ValueError("Entry hints must be aligned addresses.")
        config = folder / "config.toml"
        config.write_text(
            '[general]\n' + '\n'.join(f'{key} = {json.dumps(path.resolve().as_posix())}' for key, path in (
                ("input", args.executable), ("ghidra_output", function_map), ("output", output))) +
            '\nsingle_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
            'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\nstubs = []\nskip = []\n'
            'entry_points = [' + ', '.join(f'"0x{h:08X}"' for h in hints) + ']\n', encoding="utf-8")
        log = folder / "generation.log"
        with log.open("w", encoding="utf-8") as stream:
            subprocess.run([str(tool), str(config)], cwd=folder, check=True, stdout=stream, stderr=subprocess.STDOUT)
        files = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in sorted(output.iterdir()) if path.is_file() and path.suffix in (".cpp", ".h")}
        if not files:
            raise ValueError("Recompiler produced no source files.")
        (folder / "generation.manifest.json").write_text(json.dumps({
            "executable_sha256": identity, "program": args.program or "executable",
            "map_sha256": hashlib.sha256(function_map.read_bytes()).hexdigest(),
            "lock_sha256": provenance["lock_sha256"],
            "recompiler_sha256": provenance["recompiler_sha256"], "map_kind": "provisional" if args.candidate_map else "reviewed",
            "entry_hints": len(hints),
            "files": files}, indent=2) + "\n", encoding="utf-8")
        print(f"Generated {len(files)} local C++/header files. Log: {log}. Compile/execution is a separate stage.")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Generation stopped: {error}\n")


if __name__ == "__main__":
    main()
