#!/usr/bin/env python3
"""Runs the recorded COM vs COM fight with several configurations and summarises the fight part of each log.
  bench.py <dev> <name> "<settings k=v ...>" "<env K=V ...>" [secs]     one run
  bench.py summary <log> ...                                          summaries of logs
"""
import os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lab  # noqa: E402

F = lab.F
OUT = os.environ.get("BENCH_OUT", os.path.join(HERE, "logs"))

SPEED = re.compile(r"speed (\d+)% \(([\d.]+) game blanks/s, ([\d.]+) pictures/s over ([\d.]+) s, (\w+)[^)]*\); per frame, ms: uploads ([\d.]+), "
                   r"draws ([\d.]+), to the driver ([\d.]+), waiting for the screen ([\d.]+), showing ([\d.]+), presenting ([\d.]+); "
                   r"([\d.]+) render passes, ([\d.]+) copies")
WORK = re.compile(r"work per frame: average ([\d.]+) ms, worst ([\d.]+) ms, (\d+) of 60")
SPLIT = re.compile(r"of the average: game code ([\d.]+) ms, display list ([\d.]+) ms")
GLSPEED = re.compile(r"speed (\d+)% \(([\d.]+) game blanks/s, ([\d.]+) pictures/s over ([\d.]+) s, (\w+)")


def summary(path, skip=1, take=12):
    lines = open(path, errors="replace").read().splitlines()
    fights = []
    works, splits = [], []
    in_fight = False
    info = {}
    for ln in lines:
        m = re.search(r"internal resolution (.*)", ln)
        if m:
            info["res"] = m.group(1)
        m = re.search(r"GPU renderer: (\w+)", ln)
        if m:
            info["api"] = m.group(1)
        m = re.search(r"present mode (\w+)", ln)
        if m:
            info["present"] = m.group(1)
        if "speed" in ln and "game blanks/s" in ln:
            m = SPEED.search(ln)
            g = GLSPEED.search(ln)
            kind = (m or g).group(5) if (m or g) else None
            if kind == "fight":
                in_fight = True
                fights.append((m.groups() if m else g.groups(), ln))
            else:
                in_fight = False
        m = WORK.search(ln)
        if m and in_fight:
            works.append(float(m.group(1)))
        m = SPLIT.search(ln)
        if m and in_fight:
            splits.append((float(m.group(1)), float(m.group(2))))
    sel = fights[skip:skip + take]
    if not sel:
        return info, None
    n = len(sel)
    avg = lambda i: sum(float(s[0][i]) for s in sel) / n
    res = {"n": n, "speed": avg(0), "pics": avg(2)}
    if len(sel[0][0]) > 5:
        keys = ["uploads", "draws", "driver", "wait", "show", "present", "passes", "copies"]
        for k, i in zip(keys, range(5, 13)):
            res[k] = avg(i)
    if works:
        w = works[len(works) // 6:]
        res["work"] = sum(w) / len(w)
    if splits:
        s = splits[len(splits) // 6:]
        res["game"] = sum(a for a, b in s) / len(s)
        res["list"] = sum(b for a, b in s) / len(s)
    return info, res


def fmt(name, info, res):
    if res is None:
        return f"{name}: no fight lines ({info})"
    s = f"{name:28s} {info.get('api','?'):6s} {info.get('res','?')[:22]:22s} speed {res['speed']:5.1f}% pics {res['pics']:5.1f}"
    if "passes" in res:
        s += f" passes {res['passes']:5.1f} copies {res['copies']:5.1f} draws {res['draws']:5.2f} present {res['present']:5.2f} wait {res['wait']:4.2f}"
    if "work" in res:
        s += f" work {res['work']:5.1f}"
    if "game" in res:
        s += f" (game {res['game']:4.1f} list {res['list']:4.1f})"
    return s + f" [{res['n']}]"


def run(dev, name, settings, env, secs):
    os.makedirs(OUT, exist_ok=True)
    ser = lab.DEVS.get(dev, dev)
    lab.stop(ser)
    if settings:
        lab.settings_set(ser, settings.split())
    rec = "fight_rp5.rec" if dev == "rp5" else "fight.rec"
    envs = ["BT3_GS_VERBOSE=1", f"BT3_PAD_PLAY={F}/{rec}"] + env.split()
    lab.env_set(ser, envs)
    time.sleep(1)
    lab.start(ser)
    time.sleep(secs)
    shot = os.path.join(OUT, f"{dev}_{name}.png")
    with open(shot, "wb") as f:
        f.write(subprocess.run(["adb", "-s", ser, "exec-out", "screencap", "-p"], capture_output=True).stdout)
    lab.stop(ser)
    time.sleep(2)
    out = os.path.join(OUT, f"{dev}_{name}.txt")
    subprocess.run(["adb", "-s", ser, "pull", f"{F}/bt3_log.txt", out], check=True, capture_output=True)
    info, res = summary(out)
    line = fmt(f"{dev}_{name}", info, res)
    print(line, flush=True)
    with open(os.path.join(OUT, "results.txt"), "a") as f:
        f.write(time.strftime("%H:%M ") + line + f"  | set: {settings} | env: {env}\n")


if __name__ == "__main__":
    if sys.argv[1] == "summary":
        for p in sys.argv[2:]:
            print(fmt(os.path.basename(p), *summary(p)))
    else:
        run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5]) if len(sys.argv) > 5 else 245)
