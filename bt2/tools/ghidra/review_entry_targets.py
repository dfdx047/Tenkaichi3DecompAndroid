"""Audit the five direct entry target bodies; keep raw evidence in ignored work/."""

import importlib.util
import json
from review_entry import ROOT, audit


def main():
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    data = (ROOT / "rom/SLUS_214.41").read_bytes()
    metadata = inspector.inspect(data)
    report = json.loads((ROOT / "work/ghidra/exports/analysis.json").read_text(encoding="utf-8"))
    targets = report["entry_targets"]
    if len(targets) != 5 or len({target["target"] for target in targets}) != 5:
        raise ValueError("Expected five distinct direct entry targets; review any changed export first.")
    results = []
    for target in targets:
        if not target["has_function_at_target"]:
            raise ValueError("A direct target has no candidate function.")
        local_metadata = {**metadata, "entry_point": target["target"]}
        local_report = {**report, "entry_instructions": target["instructions"]}
        result = audit(data, local_metadata, local_report, allow_return=True)
        result["unmapped_direct_targets"] = sorted({transfer["target"] for transfer in result["transfers"]
            if "target" in transfer and not any(section["flags"] & 4 and section["occupies_file_bytes"]
                and section["address"] <= transfer["target"] < section["address"] + section["size"]
                for section in metadata["sections"])})
        results.append(result)
        print(f"{result['name']}: {result['instruction_count']} instructions, {len(result['transfers'])} transfers; "
              f"{len(result['unmapped_direct_targets'])} direct targets outside file-backed executable sections.")
    folder = ROOT / "work/ghidra/review"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "entry-targets-control.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print("Limited static target audits passed. No full-game reviewed map or generated game code produced.")


if __name__ == "__main__":
    main()
