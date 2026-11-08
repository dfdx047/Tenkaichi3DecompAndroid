#!/usr/bin/env python3
"""Compare two folders of BT3_SHOT pictures (gpu_NNNNN.ppm), frame by frame, in pixels.

  ppmcmp.py <folder a> <folder b>

One line per frame both have: pixels that differ, the largest difference in a channel. Exit 0 when nothing differs."""
import pathlib
import sys


def read(path):
    data = path.read_bytes()
    pos, fields = 0, []
    while len(fields) < 4:
        end = pos
        while data[end] not in b" \t\r\n":
            end += 1
        fields.append(data[pos:end])
        pos = end + 1
    return int(fields[1]), int(fields[2]), data[pos:]


def compare(a, b):
    wa, ha, pa = read(a)
    wb, hb, pb = read(b)
    if (wa, ha) != (wb, hb):
        return None, f"{wa}x{ha} against {wb}x{hb}"
    if pa == pb:
        return 0, 0
    count, top = 0, 0
    for i in range(0, min(len(pa), len(pb)), 3):
        d = max(abs(pa[i] - pb[i]), abs(pa[i + 1] - pb[i + 1]), abs(pa[i + 2] - pb[i + 2]))
        if d:
            count += 1
            top = max(top, d)
    return count, top


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    first, second = map(pathlib.Path, sys.argv[1:])
    names = sorted({p.name for p in first.glob("*.ppm")} & {p.name for p in second.glob("*.ppm")})
    only = sorted({p.name for p in first.glob("*.ppm")} ^ {p.name for p in second.glob("*.ppm")})
    if not names:
        sys.exit("no frame in common")
    worst = 0
    for name in names:
        count, top = compare(first / name, second / name)
        print(f"{name}  {'different size: ' + top if count is None else f'{count} pixels differ, by at most {top}'}")
        worst = max(worst, 1 if count is None else count)
    if only:
        print(f"{len(only)} frame(s) only on one side: {' '.join(only[:6])}")
    print(f"{len(names)} frames, {'all identical' if worst == 0 else f'worst {worst} pixels'}")
    return 1 if worst else 0


if __name__ == "__main__":
    sys.exit(main())
