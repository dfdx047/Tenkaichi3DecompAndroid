"""Check actual camera-local weather simulation through the native GPU draw path."""
import argparse
from collections import defaultdict
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
ROW = re.compile(r"weather frame=(\d+) epoch=(\d+) view=(\d+) bank=([0-9a-f]+) tracker=([0-9a-f]+) count=(\d+) move=(\d+) hash=([0-9a-f]+)")


def run(exe, views):
    output = ROOT / f"port/build/tag-weather-tests/{views}-views"
    output.mkdir(parents=True, exist_ok=True)
    settings = output / "settings.txt"
    settings.write_text("tag_extra_intros=0\n", encoding="utf-8")
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="gpu", BT3_TAG_TEAM="1", BT3_TAG_PLAY="4",
               BT3_TAG_VIEWS=str(views), BT3_TAG_ROSTER="2,2", BT3_DEMO="1",
               BT3_SETTINGS=str(settings), BT3_SAVES=str(output / "saves"),
               BT3_UPDATE_CHECK="0", BT3_TAG_WEATHER_TRACE="1",
               # Input scripts count rendered frames; the trace uses vblanks
               # (two per battle frame). Pause after the voiced entrances.
               BT3_PAD="900:start,960:start", SDL_AUDIO_DRIVER="dummy")
    log = output / "runtime.log"
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                   stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                text = log.read_text(encoding="utf-8", errors="replace")
                rows = list(ROW.finditer(text))
                if rows and int(rows[-1][1]) >= 2200:
                    break
                assert process.poll() is None, f"Game stopped: {log}"
                time.sleep(.2)
            else:
                raise AssertionError(f"No complete weather run: {log}")
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)
    banks, trackers, moves, draws = defaultdict(set), defaultdict(set), defaultdict(list), defaultdict(list)
    for row in rows:
        frame, epoch, view = map(int, row.group(1, 2, 3))
        if frame < 1400:
            continue
        bank, tracker = int(row[4], 16), int(row[5], 16)
        banks[view].add(bank)
        trackers[view].add(tracker)
        key = (epoch, view)
        if row[7] == "1":
            moves[key].append(frame)
        else:
            draws[key].append((frame, int(row[8], 16)))
    assert set(banks) == set(range(views)), f"Missing rendered cameras: {banks}"
    assert all(len(x) == 1 for x in banks.values()), banks
    assert len(set.union(*banks.values())) == views, f"Cameras shared particles: {banks}"
    assert len(set.union(*trackers.values())) == views, f"Cameras shared movement tracking: {trackers}"
    assert all(len(x) == 1 for x in moves.values()), "A weather bank advanced twice in one game update"
    for view in range(views):
        assert sum(v == view for _, v in moves) > 100, f"Weather stopped on view {view}"
        # The pause flag changes between the update and draw callbacks. The
        # first resumed draw can therefore move under the paused epoch before
        # the next update advances it; exclude that explicitly observed step.
        paused = [[(frame, h) for frame, h in values if frame not in moves[key]]
                  for key, values in draws.items()
                  if key[1] == view and len(values) >= 20]
        assert paused, f"Pause was not exercised on view {view}"
        assert all(len(values) >= 20 for values in paused), "No sustained paused frames"
        assert all(len({h for _, h in values}) == 1 for values in paused), "Dust moved while paused"
        assert any(v == view and frames[-1] > 1950 for (_, v), frames in moves.items()), "Weather failed to resume"
    print(f"PASS {views} views: independent dust and camera tracking, one step per update, pause/resume", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default="port/build/bt3.exe")
    parser.add_argument("--views", type=int, choices=(2, 3, 4), action="append")
    args = parser.parse_args()
    for views in args.views or (3, 4):
        run((ROOT / args.exe).resolve(), views)
