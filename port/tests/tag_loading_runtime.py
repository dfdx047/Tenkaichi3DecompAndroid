"""Measure real four-fighter loading through the game's normal loading screen.

Uses the user's extracted game data; no memory snapshots or prepared-match cache.
The first run retains the original file queue cadence for a like-for-like comparison.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def run(exe, output, legacy, renderer):
    name = "legacy" if legacy else "batched"
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS=renderer, BT3_DEMO="1", BT3_TAG_TEAM="1", BT3_TAG_PLAY="4",
               BT3_TAG_ROSTER="2,2", BT3_TAG_TRACE="1", BT3_LOAD_TRACE="1",
               BT3_PACED="1", BT3_SOUND="1", SDL_AUDIO_DRIVER="dummy",
               BT3_SETTINGS="", BT3_SAVES=str(output / "saves"), BT3_UPDATE_CHECK="0")
    if legacy:
        env["BT3_TAG_LEGACY_LOADING"] = "1"
    path = output / (name + ".log")
    with path.open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 90
        try:
            while time.monotonic() < deadline:
                text = path.read_text(encoding="utf-8", errors="replace")
                if "tagintro actor=" in text:
                    break
                if process.poll() is not None:
                    raise AssertionError(f"{name} exited before its intros; see {path}")
                time.sleep(0.1)
            else:
                raise AssertionError(f"{name} never reached its intros; see {path}")
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)
    points = re.findall(r"tagload batch=(\d+) point=(\d+) elapsed_ms=(\d+)", text)
    batches = {int(batch) for batch, _, _ in points}
    assert batches == {1}, f"Unexpected second loading phase: {points}"
    phases = {int(point): int(ms) for _, point, ms in points}
    assert all(p in phases for p in (0, 1, 2, 3)), points
    assert phases[1] <= phases[2] <= phases[3]
    return {"path": name, "assets_ready_ms": phases[1], "loading_screen_ms": phases[2],
            "intro_ready_ms": phases[3], "log": str(path.relative_to(ROOT))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default="port/build/bt3.exe")
    parser.add_argument("--renderer", choices=("none", "gpu"), default="none")
    args = parser.parse_args()
    exe = (ROOT / args.exe).resolve()
    output = ROOT / "port/build/tag-loading-tests" / args.renderer
    output.mkdir(parents=True, exist_ok=True)
    results = [run(exe, output, old, args.renderer) for old in (True, False)]
    assert results[1]["assets_ready_ms"] < results[0]["assets_ready_ms"], results
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(results, indent=2), flush=True)


if __name__ == "__main__":
    main()
