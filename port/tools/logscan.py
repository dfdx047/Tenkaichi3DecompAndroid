#!/usr/bin/env python3
"""Reads the logs players export from the app ("Save log to Download") and sums them up.

  logscan.py <folder> [--csv runs.csv] [--top 40]

One CSV line per game run found in the logs, then the groups on stdout: crashes, performance by GPU, import failures,
online failures, repeated warnings. A player is counted by device model + codename: the logs have nothing else, and
one player sends many logs, so everything is counted by device and not by file. The same app_activity lines come
back in each export from a device, so they are counted once per device, time and text.

The logs belong to the players: keep the CSV out of the repo.
"""
import argparse
import collections
import csv
import os
import re
import statistics
import sys

SECTION = re.compile(r"^===== (.+?) =====\s*$")
RUN_SECTION = re.compile(r"^(bt[0-9]_log|armsx2|emulog)[.\w]*\.txt|^bt4/.*log", re.I)
ACTIVITY = re.compile(r"^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) ([IWE]) \[([\w-]+)\] (.*)$")
SPEED = re.compile(r"speed (\d+)% \(([\d.]+) game blanks/s, ([\d.]+) pictures/s over ([\d.]+) s, (\w+)")
WORK = re.compile(r"work per frame: average ([\d.]+) ms, worst ([\d.]+) ms")
GPU = re.compile(r"GPU renderer: (\w+), (.+?), driver (\S+)")
GL_GPU = re.compile(r"GPU renderer: (\w+)[,:] (.+)")
CRASH = re.compile(r"CRASH: signal (\d+) \(code (-?\d+)\) at (\S+), fault address (0x[0-9a-f]+)")
TOMBSTONE = re.compile(r"Fatal signal (\d+) \((\w+)\), code (-?\d+) \((\w+)\), fault addr (\S+) in tid \d+ \(([^)]*)\)")
BACKTRACE = re.compile(r"#00 pc [0-9a-f]+\s+(\S+)(?: \(([^)]*)\))?")
LOGIC = re.compile(r"logicrate[^\d]*([\d.]+)")
GUEST = re.compile(r"guest_ms[=: ]+([\d.]+)")
RES = re.compile(r"internal resolution (.+)")
NET = re.compile(r"(HTTP [45]\d\d|error code: \d+|\b429\b|net: [^\n]*(?:fail|error|busy|limit|timeout|refused)|desync|out of sync"
                 r"|Dragon Net: (?:Unreachable|Busy|Rate\w*|Limit\w*)|UnknownHost\w*|lobby[^\n]*(?:fail|error)|ranked[^\n]*(?:fail|error|unavailable))", re.I)
HOST = re.compile(r"\b([a-z0-9-]+(?:\.[a-z0-9-]+)*\.(?:workers\.dev|com|net|org|io))\b", re.I)
NOISE_NUM = re.compile(r"0x[0-9a-f]+|\b\d[\d.,]*\b", re.I)
NOISE_PATH = re.compile(r"(/storage|/sdcard|/data)/\S+")
QUOTED = re.compile(r'"[^"]*"')


def norm(text, n=120):
    """A line without what changes between runs (numbers, paths, file names), to group by"""
    text = NOISE_PATH.sub("<path>", text)
    text = QUOTED.sub('"…"', text)
    return NOISE_NUM.sub("#", text).strip()[:n]


def split_sections(text):
    name, body = "header", []
    for line in text.splitlines():
        m = SECTION.match(line)
        if m:
            yield name, body
            name, body = m.group(1), []
        else:
            body.append(line)
    yield name, body


def settings_of(body):
    out = {}
    for line in body:
        if "=" in line and " " not in line.split("=", 1)[0]:
            k, v = line.split("=", 1)
            out.setdefault(k.strip(), v.strip())
    return out


class Export:
    def __init__(self, path):
        self.file = os.path.basename(path)
        self.version = self.device = self.android = self.soc = "?"
        self.memory = ""
        self.activity = []
        self.settings = {}
        self.runs = []
        self.logcat = []
        self.logcat_crashes = []
        text = open(path, encoding="utf-8", errors="replace").read()
        for name, body in split_sections(text):
            if name == "header":
                self.read_header(body)
            elif name.startswith("app_activity"):
                self.read_activity(body)
            elif name == "the game folder":
                self.settings = settings_of(body)
                self.folder = body
            elif name.startswith("logcat"):
                self.read_logcat(body)
            elif RUN_SECTION.match(name):
                self.runs.append(Run(self, name, body))

    def read_header(self, body):
        for line in body:
            if line.startswith("app "):
                self.version = line.split()[-1]
            elif line.startswith("device "):
                m = re.match(r"device (.+?), Android (\S+)", line)
                self.device, self.android = (m.group(1), m.group(2)) if m else (line[7:], "?")
            elif line.startswith("SoC "):
                self.soc = line[4:].strip()
            elif line.startswith("memory "):
                self.memory = line[7:].strip()

    def read_activity(self, body):
        for line in body:
            m = ACTIVITY.match(line)
            if m:
                self.activity.append(m.groups())

    def read_logcat(self, body):
        self.logcat = body
        for i, line in enumerate(body):
            m = TOMBSTONE.search(line)
            if m:
                frame = ""
                for later in body[i + 1:i + 80]:
                    b = BACKTRACE.search(later)
                    if b:
                        frame = os.path.basename(b.group(1)) + (" " + b.group(2) if b.group(2) else "")
                        break
                self.logcat_crashes.append((m.group(2), m.group(3), m.group(5), m.group(6), frame))


class Run:
    def __init__(self, export, section, body):
        self.section = section
        self.game = "BT3"
        self.gpu = self.driver = self.api = self.res = self.renderer = ""
        self.lite = export.settings.get("dr_lite", "")
        self.tier = export.settings.get("dr_gpu_tier", "")
        speeds, pics, works, worsts, logic, guest = [], [], [], [], [], []
        self.crash = None
        self.net = collections.Counter()
        self.warn = collections.Counter()
        in_fight = False
        for line in body:
            if "Budokai Tenkaichi 4" in line or "libbt4" in line or "logicrate" in line or "bt4:" in line[:8]:
                self.game = "BT4"
            if "libbt2" in line or "bt2:" in line[:8]:
                self.game = "BT2"
            if "ARMSX2" in line or "bt4gsx" in line.lower():
                self.renderer = "ARMSX2"
            if "lite mode" in line:
                self.lite = "1"
            m = GPU.search(line)
            if m:
                self.api, self.gpu, self.driver = m.groups()
            elif "GPU renderer:" in line and not self.gpu:
                m = GL_GPU.search(line)
                if m:
                    self.api, self.gpu = m.group(1), m.group(2)
            m = RES.search(line)
            if m and not self.res:
                self.res = m.group(1).strip()
            m = SPEED.search(line)
            if m:
                in_fight = m.group(5) == "fight"
                if in_fight:
                    speeds.append(int(m.group(1)))
                    pics.append(float(m.group(3)))
            m = WORK.search(line)
            if m and in_fight:
                works.append(float(m.group(1)))
                worsts.append(float(m.group(2)))
            m = LOGIC.search(line)
            if m:
                logic.append(float(m.group(1)))
            m = GUEST.search(line)
            if m:
                guest.append(float(m.group(1)))
            m = CRASH.search(line)
            if m and not self.crash:
                self.crash = m.groups()
            m = NET.search(line)
            if m:
                self.net[norm(line, 90)] += 1
            if re.search(r"\b(warning|error|failed|not found|cannot|can't)\b", line, re.I) and not line.startswith("WARNING: linker"):
                self.warn[norm(line, 90)] += 1
        mean = lambda xs: round(statistics.mean(xs), 1) if xs else ""
        self.speed, self.fps, self.work = mean(speeds[1:] or speeds), mean(pics[1:] or pics), mean(works[1:] or works)
        self.worst = max(worsts) if worsts else ""
        self.logic = mean(logic[1:] or logic)
        self.guest = mean(guest[1:] or guest)
        if not self.renderer:
            self.renderer = self.api.capitalize() if self.api else ""


def crash_sig(crash):
    return f"{crash[2]} (signal {crash[0]})"


def scan(folder):
    exports = []
    for name in sorted(os.listdir(folder)):
        path = os.path.join(folder, name)
        if name.endswith(".txt") and os.path.isfile(path):
            exports.append(Export(path))
    return exports


def write_csv(exports, path):
    fields = ["file", "version", "game", "device", "soc", "gpu", "driver", "android", "memory", "renderer", "resolution", "tier",
              "lite", "fight_speed", "fight_fps", "work_ms", "worst_ms", "logicrate", "guest_ms", "crash", "fault_address",
              "import_errors", "net_errors", "top_warning", "top_warning_count"]
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(fields)
        for e in exports:
            imp = sum(1 for a in e.activity if a[1] == "E" and a[2] == "iso")
            for r in e.runs:
                top = r.warn.most_common(1)
                w.writerow([e.file, e.version, r.game, e.device, e.soc, r.gpu, r.driver, e.android, e.memory, r.renderer, r.res, r.tier, r.lite,
                            r.speed, r.fps, r.work, r.worst, r.logic, r.guest, crash_sig(r.crash) if r.crash else "",
                            r.crash[3] if r.crash else "", imp, sum(r.net.values()),
                            top[0][0] if top else "", top[0][1] if top else ""])


def groups(exports, top):
    devices = {e.device for e in exports}
    print(f"{len(exports)} logs, {sum(len(e.runs) for e in exports)} runs, {len(devices)} devices")
    print("versions:", collections.Counter(e.version for e in exports).most_common())
    print("games:", collections.Counter(r.game for e in exports for r in e.runs).most_common())

    # crashes: the game's own line in the runs, plus "the last run ended badly" in the activity diary, once per device and time
    sig_dev, sig_ver, sig_n, sig_addr = collections.defaultdict(set), collections.defaultdict(set), collections.Counter(), collections.defaultdict(collections.Counter)
    seen = set()

    def add(dev, ver, key, addr, when):
        """when: the diary's date of the line, or None for a run's own crash (the same crash the next start's diary
        line tells: it counts only when no diary line told any of that device's)."""
        sig_dev[key].add(dev)
        sig_ver[key].add(ver)
        if when is None:
            run_crashes[(dev, key)] += 1
            run_addr[(dev, key)] = addr
        elif (dev, key, when) not in seen:
            seen.add((dev, key, when))
            told[(dev, key)] += 1
            sig_addr[key][addr] += 1

    run_crashes, told, run_addr = collections.Counter(), collections.Counter(), {}
    bad = collections.defaultdict(lambda: (set(), set(), collections.Counter()))
    for e in exports:
        for r in e.runs:
            if r.crash:
                add(e.device, e.version, crash_sig(r.crash), r.crash[3], None)
        for when, lvl, tag, text in e.activity:
            if "ended badly" not in text:
                continue
            m = CRASH.search(text)
            if m:
                add(e.device, e.version, crash_sig(m.groups()), m.group(4), when)
            else:
                key = norm(text.split("ended badly:", 1)[1], 80)
                k = (e.device, key, when)
                if k not in seen:
                    seen.add(k)
                    bad[key][0].add(e.device)
                    bad[key][1].add(e.version)
                    bad[key][2]["n"] += 1
    print("\n## Crashes by signature (devices, events, versions, top fault addresses)")
    for (dev, key), n in run_crashes.items():
        sig_n[key] += max(n, told[(dev, key)])
        if not told[(dev, key)]:
            sig_addr[key][run_addr[(dev, key)]] += n
    for (dev, key), n in told.items():
        if not run_crashes[(dev, key)]:
            sig_n[key] += n
    for key, devs in sorted(sig_dev.items(), key=lambda kv: (-len(kv[1]), -sig_n[kv[0]]))[:top]:
        print(f"{len(devs):3d} devices {sig_n[key]:4d} events  {key}  versions {sorted(sig_ver[key])}  addr {sig_addr[key].most_common(3)}")
    print("\n## Other runs that ended badly")
    for key, (devs, vers, n) in sorted(bad.items(), key=lambda kv: -len(kv[1][0]))[:top]:
        print(f"{len(devs):3d} devices {n['n']:4d} events  {key}  versions {sorted(vers)}")

    tomb = collections.defaultdict(set)
    for e in exports:
        for c in e.logcat_crashes:
            tomb[f"{c[0]} code {c[1]} thread {c[3]} in {c[4][:60]}"].add(e.device)
    print("\n## Native crashes in logcat (signal, thread, top frame)")
    for key, devs in sorted(tomb.items(), key=lambda kv: -len(kv[1]))[:top]:
        print(f"{len(devs):3d} devices  {key}")

    print("\n## Performance by GPU, version 0.9.0 and 0.8.1 fights (mean speed %, fps, work ms, devices, runs)")
    perf = collections.defaultdict(list)
    for e in exports:
        for r in e.runs:
            if r.gpu and r.speed != "":
                perf[(r.gpu, r.game, r.api)].append((r.speed, r.fps, r.work, e.device))
    for key, rows in sorted(perf.items(), key=lambda kv: -len({x[3] for x in kv[1]})):
        works = [x[2] for x in rows if x[2] != ""]
        print(f"{statistics.mean(x[0] for x in rows):6.1f}% {statistics.mean(x[1] for x in rows):5.1f} fps "
              f"{(statistics.mean(works) if works else 0):6.1f} ms {len({x[3] for x in rows}):3d} dev {len(rows):3d} runs  {key[0]} {key[1]} {key[2]}")

    def activity_group(title, tags, level="E"):
        g = collections.defaultdict(set)
        cnt = collections.Counter()
        done = set()
        for e in exports:
            for when, lvl, tag, text in e.activity:
                if tag in tags and lvl in level and (e.device, when, text) not in done:
                    done.add((e.device, when, text))
                    key = norm(text, 110)
                    g[key].add(e.device)
                    cnt[key] += 1
        print(f"\n## {title}")
        for key, devs in sorted(g.items(), key=lambda kv: -len(kv[1]))[:top]:
            print(f"{len(devs):3d} devices {cnt[key]:4d} events  {key}")

    activity_group("Import failures", {"iso"}, "EW")
    activity_group("Texture pack, mods and other work", {"textures", "work"}, "EW")
    activity_group("Online and update, diary", {"online", "update", "net"}, "EW")

    net = collections.defaultdict(set)
    cnt = collections.Counter()
    for e in exports:
        for r in e.runs:
            for k, n in r.net.items():
                net[k].add(e.device)
                cnt[k] += n
        for line in e.logcat:
            if " 429" in line or "error code: 1027" in line:
                k = norm(line.split(":", 3)[-1], 90)
                net[k].add(e.device)
                cnt[k] += 1
    print("\n## Online failures in the game log")
    for key, devs in sorted(net.items(), key=lambda kv: -len(kv[1]))[:top]:
        print(f"{len(devs):3d} devices {cnt[key]:6d} lines  {key}")

    warn = collections.defaultdict(set)
    cnt = collections.Counter()
    for e in exports:
        for r in e.runs:
            for k, n in r.warn.items():
                warn[k].add(e.device)
                cnt[k] += n
    print("\n## Repeated warnings in the game log")
    for key, devs in sorted(warn.items(), key=lambda kv: -len(kv[1]))[:top]:
        print(f"{len(devs):3d} devices {cnt[key]:7d} lines  {key}")

    biggest = sorted(((sum(n for r in e.runs for n in r.warn.values()), e.device, e.version) for e in exports), reverse=True)[:8]
    print("\n## Logs with most warning lines", biggest)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder")
    ap.add_argument("--csv")
    ap.add_argument("--top", type=int, default=40)
    a = ap.parse_args()
    exports = scan(a.folder)
    if a.csv:
        write_csv(exports, a.csv)
    groups(exports, a.top)


if __name__ == "__main__":
    sys.exit(main())
