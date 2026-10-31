#!/usr/bin/env python3
"""Makes docs/changes/by-file.md: for each file, the commits that touched it, newest first, with date, author, lines and
message, and the link to the area of docs/changes/ that explains it.

  python3 port/tools/change_map.py [--since <rev>] [--limit <commits per file>]

The default starts at the first commit of the Android app, so the PC port's own history (docs/port) does not bury it.
Generated folders and data the repository does not keep are left out."""
import argparse
import collections
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/changes/by-file.md"
SKIP = ("docs/changes/by-file.md", "bt2/game/generated/", "bt2/work/", "port/build/")
# the area file of a path: the first prefix that matches wins
AREAS = (
    ("port/src/gs/tag", "tag-team"), ("src/battle/tag", "tag-team"), ("src/menu/tag", "tag-team"), ("port/src/tagteam", "tag-team"),
    ("include/battle/tag", "tag-team"), ("include/menu/tag", "tag-team"),
    ("port/src/gs/net", "online"), ("port/src/gs/ui_dragonnet", "online"), ("src/menu/port_tbo", "online"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/engine/Relay", "online"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/data/OnlineStatus", "online"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/engine/Bt2", "bt2"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/data/Bt2", "bt2"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/ui/screens/bt2", "bt2"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/Bt2", "bt2"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/data/Bt4", "bt4"),
    ("android/app/src/main/java/com/dfdx047/dragonrage/engine/Touch", "touch-input"),
    ("android/app/src/main/cpp/", "bt3-engine"), ("android/", "launcher"),
    ("bt2/", "bt2"),
    ("port/bt4/armsx2", "bt4-armsx2"), ("port/bt4/", "bt4"),
    ("port/src/gs/gs_input", "touch-input"), ("port/src/gs/gs_touch", "touch-input"), ("src/sys/pad", "touch-input"),
    ("port/src/gs/", "renderer-gs"), ("port/third_party/sdl3-patches", "renderer-gs"),
    ("port/src/snd", "audio"), ("port/src/plat_snd", "audio"), ("src/sys/adx", "audio"), ("src/sys/snd", "audio"),
    ("port/tools/", "build-ci"), (".github/", "build-ci"),
    ("port/", "bt3-engine"), ("src/", "bt3-engine"), ("include/", "bt3-engine"),
)


def area(path):
    for prefix, name in AREAS:
        if path.startswith(prefix):
            return name
    return None


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True, text=True, errors="replace").stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--since", default="e1b9207~200", help="first revision (default: the last 200 before 0.9.0 and all after)")
    ap.add_argument("--limit", type=int, default=25)
    a = ap.parse_args()
    log = git("log", "--no-merges", "--numstat", "--date=short", "--format=%x01%h%x02%ad%x02%an%x02%s", f"{a.since}..HEAD")
    files = collections.defaultdict(list)
    commit = None
    for line in log.splitlines():
        if line.startswith("\x01"):
            commit = line[1:].split("\x02")
        elif line.strip() and commit:
            added, removed, path = line.split("\t", 2)
            if " => " in path or path.startswith(SKIP) or added == "-":
                continue
            files[path].append((commit, added, removed))
    tracked = set(git("ls-files").splitlines())
    with open(OUT, "w", encoding="utf-8") as out:
        out.write("# Mudanças por arquivo\n\nGerado por `python3 port/tools/change_map.py`: não edite à mão. Commits do mais novo ao "
                  f"mais velho, de `{a.since}` até o HEAD, no máximo {a.limit} por arquivo. Merges ficam de fora: os commits deles aparecem "
                  "com o autor original.\n")
        for path in sorted(p for p in files if p in tracked):
            name = area(path)
            link = f" ([{name}]({name}.md))" if name else ""
            out.write(f"\n## `{path}`{link}\n\n")
            for (h, date, who, msg), added, removed in files[path][:a.limit]:
                msg = msg.replace("|", "\\|")
                out.write(f"- `{h}` {date} {who}, +{added} -{removed}: {msg}\n")
            if len(files[path]) > a.limit:
                out.write(f"- ... e mais {len(files[path]) - a.limit} (`git log -- {path}`)\n")
    print(f"{OUT.relative_to(ROOT)}: {len([p for p in files if p in tracked])} files")


if __name__ == "__main__":
    main()
