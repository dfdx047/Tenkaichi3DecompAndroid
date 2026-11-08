"""Publish allowlisted startup counters; never copy raw reports."""
import argparse
from datetime import date
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIELDS = (
    "distinct_ee_entered", "distinct_ee_completed", "distinct_kernel_entered",
    "distinct_kernel_completed", "calls", "returns",
    "native_module_loads", "ee_thread_count", "cri_rpc_calls", "heap_calls",
    "ncmd_read_calls", "native_disc_requests", "disc_rpc_calls",
    "gs_packets",
    "gs_sprites", "gs_pixels", "startup_worker_queries", "startup_worker_starts",
    "startup_worker_priority_changes", "startup_ready_queries", "startup_worker_suspends",
)


def snapshot(raw, measured_on, verification="unverified"):
    date.fromisoformat(measured_on)
    report = json.loads(raw)
    metrics = {}
    for key in (*FIELDS, "selector", "pc", "target"):
        value = report[key]
        if type(value) is not int or not 0 <= value <= 0xFFFFFFFFFFFFFFFF:
            raise ValueError(f"Invalid numeric metric: {key}")
        metrics[key] = value
    for entered, completed in (("distinct_ee_entered", "distinct_ee_completed"),
                               ("distinct_kernel_entered", "distinct_kernel_completed")):
        if metrics[completed] > metrics[entered]:
            raise ValueError("Completion count exceeds entered count")
    return {
        "schema_version": 1,
        "measured_on": measured_on,
        "scope": "bounded native startup probe with local ISO and prepared inputs",
        "code_state": "bounded startup verified by build/tests and capture comparison" if verification=="verified" else "local implementation; final pass verification incomplete",
        "source_report_sha256": hashlib.sha256(raw).hexdigest(),
        "metrics": {key: metrics[key] for key in FIELDS},
        "boundary": {key: f"0x{metrics[key]:X}" for key in ("selector", "pc", "target")},
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--report", type=Path, required=True)
    p.add_argument("--measured-on", required=True, help="YYYY-MM-DD: actual measurement date")
    p.add_argument("--verification", choices=("unverified", "verified"), default="unverified", help="Use verified only after required build/tests and comparisons pass")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    data = snapshot(args.report.read_bytes(), args.measured_on, args.verification)
    outputs = {
        ROOT / "docs/progress/latest.json": json.dumps(data, indent=2) + "\n",
    }
    for path, content in outputs.items():
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                raise SystemExit(f"Stale generated progress: {path.name}")
        else:
            with path.open("w", encoding="utf-8", newline="\n") as output:
                output.write(content)
    print("Startup snapshot checked." if args.check else "Published numeric startup snapshot.")


if __name__ == "__main__":
    main()