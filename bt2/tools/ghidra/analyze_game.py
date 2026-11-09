"""Import the local candidate into Ghidra and export unreviewed function candidates."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import time
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/ps2recomp"))
from inspect_blocker import stop_address
import overlay_programs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ghidra-home", type=Path, default=ROOT.parent / "tools" / "ghidra_12.1.4_PUBLIC")
    parser.add_argument("--java-home", type=Path, default=Path("C:/Program Files/Eclipse Adoptium/jdk-25.0.3.9-hotspot"))
    parser.add_argument("--discover-blocker", action="store_true", help="Discover only the latest observed missing handler/target")
    parser.add_argument("--discover-prologues", action="store_true", help="Create every function that opens a stack frame right after a completed function and is not part of one (pointer-only targets), in one pass")
    parser.add_argument("--program", choices=sorted(overlay_programs.PROGRAMS), help="Analyze a wrapped runtime-loaded overlay instead of the executable")
    parser.add_argument("--analysis-timeout", type=int, default=600, help="Seconds Ghidra's auto-analysis may take per file (a slower machine needs more)")
    args = parser.parse_args()
    blocker = None
    if args.discover_blocker:
        blocker = json.loads((ROOT / "work/bt2-generation/blocker-map.json").read_text(encoding="utf-8"))
        if args.program and blocker.get("program") != args.program:
            parser.error("Blocker belongs to a different program.")
        args.program = blocker.get("program")
    folder = ROOT / "work" / "ghidra"
    if args.program:
        # Wrapped overlay: identity checked against its pinned image; separate project and exports.
        _, data = overlay_programs.load(args.program)
        files = overlay_programs.paths(args.program)
        executable, project, exports = files["elf"], files["ghidra_project"], files["exports"]
        expected_hash = hashlib.sha256(data).hexdigest()
    else:
        executable, project, exports = ROOT / "rom" / "SLUS_214.41", "BT2", folder / "exports"
        if executable.stat().st_size != 2675936:
            parser.error("Input size differs from the inspected local candidate.")
        data = executable.read_bytes()
        expected_hash = "f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e"
        if hashlib.sha256(data).hexdigest() != expected_hash:
            parser.error("Input differs from the inspected local candidate.")
    entry = struct.unpack_from("<I", data, 24)[0]
    for child in ("projects", "settings", "cache", "temp"):
        (folder / child).mkdir(parents=True, exist_ok=True)
    exports.mkdir(parents=True, exist_ok=True)
    java = args.java_home / "bin" / ("java.exe" if os.name == "nt" else "java")
    utility = args.ghidra_home / "Ghidra" / "Framework" / "Utility" / "lib" / "Utility.jar"
    program_options = ["-process", executable.name, "-noanalysis"] if (folder / "projects" / f"{project}.gpr").exists() else ["-import", str(executable)]
    discovery = []
    discovered_address = None
    if args.discover_blocker:
        if blocker["map_status"] != "map gap":
            parser.error("Discovery requires an observed map gap.")
        stop = json.loads((ROOT / "work/bt2-generation/entry-stop.json").read_text(encoding="utf-8"))
        discovered_address = stop_address(stop)
        if blocker.get("requires_kernel_payload_mapping"):
            parser.error("Copied kernel target requires a separate relocation-aware payload map; it is not original ELF .text.")
        if discovered_address != blocker["address"]:
            parser.error("Blocker diagnostic differs from the latest execution stop.")
        discovery = ["-preScript", "DiscoverBT2Target.java", hex(blocker["address"])]
    if args.discover_prologues:
        if args.discover_blocker:
            parser.error("Choose one discovery mode.")
        discovery = ["-preScript", "DiscoverBT2Prologues.java"]
    command = [str(java), "-Xmx2G", "-Xshare:off", "--enable-native-access=ALL-UNNAMED",
               "-Djava.system.class.loader=ghidra.GhidraClassLoader", "-Dfile.encoding=UTF8",
               "-Duser.language=en", "-Duser.country=US", "-Dlog4j.skipJansi=true",
               *[f"-Dapplication.{name}dir={folder / child}" for name, child in (("settings", "settings"), ("cache", "cache"), ("temp", "temp"))],
               f"-Djava.io.tmpdir={folder / 'temp'}", "-cp", str(utility), "ghidra.Ghidra",
               "ghidra.app.util.headless.AnalyzeHeadless", str(folder / "projects"), project,
               *program_options, "-processor", "r5900:LE:32:default", "-cspec", "default",
               "-analysisTimeoutPerFile", str(args.analysis_timeout), "-max-cpu", "4",
               "-scriptPath", str(ROOT / "tools" / "ghidra"), *discovery, "-postScript", "ExportBT2Candidates.java", str(exports), hex(entry),
               "-log", str(folder / "analysis.log"), "-scriptlog", str(folder / "scripts.log")]
    started = time.time()
    subprocess.run(command, cwd=ROOT, check=True)
    report_path = exports / "analysis.json"
    if not report_path.exists() or report_path.stat().st_mtime < started:
        raise RuntimeError("Ghidra did not produce a fresh candidate report; inspect the logs.")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report["language"] != "r5900:LE:32:default" or report["executable_sha256"].lower() != expected_hash:
        raise RuntimeError("Ghidra report language or executable hash does not match.")
    if discovered_address is not None and not any(row["start"] == discovered_address for row in report["functions"]):
        raise RuntimeError("Observed target was not discovered; inspect Ghidra script logs.")
    print(f"Unreviewed candidates exported under {exports}; no BT2 C++ generated.")


if __name__ == "__main__":
    main()
