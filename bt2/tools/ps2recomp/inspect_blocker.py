"""Classify reached targets against provisional maps without guessing boundaries."""
import csv
import json
from run_pipeline import ROOT, verify_generated
import overlay_programs


def body_contains(address, candidate):
    """Ghidra ranges have inclusive ends; omitted ranges retain legacy extent behavior."""
    if 'body_ranges' not in candidate:
        return candidate['start'] <= address < candidate['end_exclusive']
    for span in candidate['body_ranges']:
        first,last=span.split('-')
        start,end=int(first,16),int(last,16)
        if end<start:raise ValueError('Invalid analysis body range')
        if start<=address<=end:return True
    return False


def classify(address, rows, functions, excluded):
    containing = [row for row in rows if row["start"] <= address < row["end"]]
    exact = any(row["start"] == address for row in containing)
    bounding = [row for row in functions if row["start"] <= address < row["end_exclusive"]]
    candidates = [row for row in bounding if body_contains(address,row)]
    return {"address": address, "neutral_name": f"FUN_{containing[0]['start'] if len(containing) == 1 else address:08X}",
            "map_status": "mapped entry" if exact else "inside mapped function" if containing else "map gap",
            "mapped_ranges": containing, "analysis_candidates": candidates,
            "bounding_gap_candidates": [row for row in bounding if not body_contains(address,row)],
            "requires_kernel_payload_mapping": 0x80000000 <= address < 0x80080000,
            "exclusions": [row for row in excluded if row["start"] <= address < row["end"]]}


def stop_address(stop):
    if stop.get('target'):
        return stop['target']
    if stop.get('access') == 'handler registration' and stop.get('handler'):
        return stop['handler']
    return stop.get('pc') if 'memory' in stop.get('status', '') else stop.get('handler') or stop.get('pc')


def main():
    verify_generated()
    folder = ROOT / "work/bt2-generation"
    stop = json.loads((folder / "entry-stop.json").read_text(encoding="utf-8"))
    address = stop_address(stop)
    if address is None:
        print("Syscall blocker has no registered handler to map.")
        return
    # Targets inside a runtime-loaded overlay are classified against that overlay's own map.
    program = overlay_programs.program_for(address)
    files = overlay_programs.paths(program) if program else None
    maps = files["generation"] if program else folder
    exports = files["exports"] if program else ROOT / "work/ghidra/exports"
    with (maps / "candidates.csv").open(newline="") as stream:
        rows = [{"start": int(row["Start"], 0), "end": int(row["End"], 0)} for row in csv.DictReader(stream)]
    export = json.loads((exports / "analysis.json").read_text(encoding="utf-8"))
    manifest = json.loads((maps / "candidates.manifest.json").read_text(encoding="utf-8"))
    result = classify(address, rows, export["functions"], manifest["excluded"])
    if program:
        result["program"] = program
    result["limitations"] = "Actual analysis body membership is separated from bounding gaps. Exclusions retain bounding quarantine metadata; no boundary, handler semantics or execution is inferred."
    (folder / "blocker-map.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    where = f" in overlay {program}" if program else ""
    print(f"Reached {result['neutral_name']}{where}: {result['map_status']}; see {folder / 'blocker-map.json'}.")


if __name__ == "__main__":
    main()
