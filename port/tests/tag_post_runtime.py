"""Exercise actual GPU water/panorama/radial passes in split-screen arenas."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
CASES = {
    "water-4": (2, 2, 4, (1, 3), "800x600"),
    "pan-3": (0, 3, 3, (2, 3), "1280x960"),
}

def complete_ppm(path):
    try:
        with path.open("rb") as f:
            if f.readline().strip() != b"P6":
                return False
            width, height = map(int, f.readline().split())
            if f.readline().strip() != b"255":
                return False
            return path.stat().st_size == f.tell() + width * height * 3
    except (OSError, ValueError):
        return False


def run(name):
    stage, probe, views, kinds, size = CASES[name]
    out = ROOT / "port/build/tag-post-tests" / name
    out.mkdir(parents=True, exist_ok=True)
    settings = out / "settings.txt"
    settings.write_text("tag_map_size=200\ntag_extra_intros=0\n")
    shot = out / "frame.ppm"
    # A previous frame must not make a failed capture appear successful.
    if shot.exists():
        shot.unlink()
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(
        BT3_GS="gpu", BT3_DEMO="1", BT3_TAG_TEAM="1",
        BT3_TAG_PLAY=str(views), BT3_TAG_VIEWS=str(views),
        BT3_TAG_ROSTER="2,2", BT3_TEST_STAGE=str(stage),
        BT3_TAG_TEST_STAGE=str(probe), BT3_TAG_POST_TRACE="1",
        BT3_SETTINGS=str(settings), BT3_SAVES=str(out / "saves"),
        BT3_WINDOW=size, BT3_UI_SHOT=f"950:{shot.as_posix()}",
        BT3_UPDATE_CHECK="0", SDL_AUDIO_DRIVER="dummy",
    )
    log = out / "runtime.log"
    with log.open("w") as stream:
        child = subprocess.Popen([str(ROOT / "port/build/bt3.exe")],
                                 cwd=ROOT, env=env, stdout=stream,
                                 stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 75
            while time.monotonic() < deadline:
                if child.poll() is not None:
                    raise AssertionError(f"Game exited before capture: {log}")
                if complete_ppm(shot):
                    break
                time.sleep(.2)
            else:
                raise AssertionError(f"Capture timed out: {log}")
        finally:
            if child.poll() is None:
                child.terminate()
            child.wait(timeout=10)
    text = log.read_text(errors="replace")
    assert "FAIL check=" not in text and "not implemented yet" not in text, log
    for kind in kinds:
        records = re.findall(rf"kind={kind} view=(\d+) split=1 rect=([\d,]+)", text)
        by_view = {int(v): tuple(map(int, r.split(","))) for v, r in records}
        expected = {0: (0, 0, 256, 224), 1: (256, 0, 256, 224)}
        if views == 3:
            expected[2] = (0, 224, 512, 224)
        else:
            expected.update({2: (0, 224, 256, 224), 3: (256, 224, 256, 224)})
        assert by_view == expected, (kind, by_view, log)
    print(f"PASS {name}: native GPU effects in every viewport", flush=True)
    return {"case": name, "log": str(log.relative_to(ROOT)),
            "screenshot": str(shot.relative_to(ROOT))}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=tuple(CASES), action="append")
    args = parser.parse_args()
    results = [run(case) for case in args.case or CASES]
    (ROOT / "port/build/tag-post-tests/results.json").write_text(
        json.dumps(results, indent=2) + "\n")
