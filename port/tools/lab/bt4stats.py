#!/usr/bin/env python3
"""bt4stats.py <name_log.txt> [<name_threads.txt>]: where BT4's time goes, from a bt4route.sh run.
From the log: the fight's updates a second ([logicrate], the 5 s lines over 5 a second), the pictures a second
([fps] GAME=) and the game code's share of each picture (guest_ms: the recompiled EE code, VU0 macro included) against
its whole time (wall_ms), during the fight (the [fps] lines whose guest_ms is over 5). From the threads (top -H, the second
sample): the CPU of each thread of the :bt4 process, grouped: EE (the game's thread), VU1 (the runtime's VU1 thread when
it has one), GS (the GS stream's reader and the GS thread: MTGS, ARMSX2's or Dragon Rage's), GPU driver (the Vulkan or GL
driver's threads), sound (audio, AAudio, SPU), IOP (the IOP / file threads) and the rest."""
import re
import sys


def stats(vals):
    if not vals:
        return "-"
    v = sorted(vals)
    return f"{sum(v) / len(v):.1f} (min {v[0]:.1f}, p10 {v[len(v) // 10]:.1f})"


log = open(sys.argv[1], errors="replace").read().splitlines()
logic = [float(m.group(1)) for ln in log if (m := re.match(r"\[logicrate\] ([\d.]+)", ln)) and float(m.group(1)) > 5]
fps = []
for ln in log:
    m = re.match(r"\[fps\] GAME=([\d.]+) .*guest_ms=([\d.]+) wall_ms=([\d.]+)", ln)
    if m and float(m.group(2)) > 5:
        fps.append(tuple(float(x) for x in m.groups()))
print(f"fight updates/s: {stats(logic)}   [{len(logic)} x 5 s]")
print(f"pictures/s:      {stats([f[0] for f in fps])}")
if fps:
    g = sum(f[1] for f in fps) / len(fps)
    w = sum(f[2] for f in fps) / len(fps)
    print(f"per picture:     game code (EE) {g:.1f} ms of {w:.1f} ms ({100 * g / w:.0f} %); the rest waits on VU1/GS/GPU/sound")
print(f"stall lines:     {sum(1 for ln in log if 'stallprobe' in ln)}")
for k in ("readback", "ReadFIFO", "PSMCT24", "DEVICE_LOST", "Surface lost", "surface gone"):
    n = sum(1 for ln in log if k in ln)
    if n:
        print(f"  '{k}': {n}   {next(ln for ln in log if k in ln)[:110]}")

# The threads of the :bt4 process, by part (names: BT3-Recomp's runtime and ARMSX2; a thread made without a name of
# its own takes its creator's: the runtime's second "GameThread", by thread id, is the VIF1/VU1 kick worker that the
# game thread starts at its first kick, PS2Memory::ensureKickWorker).
PARTS = [("EE (game code)", r"^GameThread#0$"), ("VIF1/VU1 (the kick worker: the 2nd GameThread)", r"^GameThread#[1-9]|^MTVU|^VU1"),
         ("GS stream (runtime: GIF parse, stage 2)", r"^GsThread"), ("GS (ARMSX2: replay CPU thread)", r"^CPU Thread"),
         ("GS/GPU (ARMSX2's GS thread, Vulkan submit)", r"^GS$|^GSThread|^MTGS"), ("GPU driver", r"vk|gl|adreno|kgsl|RenderThread"),
         ("sound", r"Audio|SDLAudio|AAudio|Oboe"), ("IOP / kernel threads (PS2Thread_n)", r"^PS2Thread"),
         ("app (SDL, binder, Java)", r".")]


def threads(path):
    top = open(path, errors="replace").read().splitlines()
    heads = [i for i, ln in enumerate(top) if "%CPU" in ln and "TID" in ln]
    if not heads:
        return []
    h = top[heads[-1]].split()
    ci = next(i for i, t in enumerate(h) if "%CPU" in t) + (1 if any(t.startswith("S[") for t in h) else 0)
    rows = []
    for ln in top[heads[-1] + 1:]:
        f = ln.split()
        if len(f) < ci + 5 or not f[0].isdigit():
            continue
        try:
            rows.append((int(f[0]), float(f[ci]), " ".join(f[ci + 3:-1])))
        except ValueError:
            continue
    out, seen = [], {}
    for tid, cpu, name in sorted(rows):  # (by thread id: the order they were made in)
        n = seen.get(name, 0)
        seen[name] = n + 1
        out.append((cpu, name, f"{name}#{n}"))
    return out


if len(sys.argv) > 2:
    per = threads(sys.argv[2])
    groups = {}
    for cpu, name, key in per:
        g = next(label for label, rx in PARTS if re.search(rx, key) or re.search(rx, name))
        groups[g] = groups.get(g, 0.0) + cpu
    print("threads (CPU %, 100 = one core; top -H over 10 s):")
    for cpu, name, key in sorted(per, reverse=True)[:9]:
        print(f"  {cpu:5.1f}  {name}")
    print("  by part: " + "; ".join(f"{k} {v:.0f}" for k, v in sorted(groups.items(), key=lambda kv: -kv[1]) if v >= 0.5))
