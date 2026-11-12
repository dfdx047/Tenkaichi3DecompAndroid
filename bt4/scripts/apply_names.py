#!/usr/bin/env python3
"""Replaces func_XXXXXXXX / D_XXXXXXXX placeholders in src/ and include/ with their current names.

When one module names a function another module still calls by its placeholder, the placeholder
no longer exists at link time. Run this after adding names to the symbol files.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    names = {}
    files = sorted((ROOT / "config").glob("symbol_addrs*.txt")) + \
        sorted((ROOT / "config" / "symbols").glob("*.txt"))
    for path in files:
        for line in path.read_text().splitlines():
            m = re.match(r"(\w+) = 0x([0-9A-Fa-f]+);", line)
            if m:
                names[int(m.group(2), 16)] = m.group(1)

    def swap(m):
        new = names.get(int(m.group(2), 16))
        return new if new and not re.fullmatch(r"(func|D)_[0-9A-F]+", new) else m.group(0)

    # Optional arguments: path substrings to leave alone (files another process is still writing).
    skip = sys.argv[1:]
    for path in sorted((ROOT / "src").rglob("*.c")) + sorted((ROOT / "include").rglob("*.h")):
        if any(s in str(path) for s in skip):
            continue
        old = path.read_text()
        new = re.sub(r"\b(func|D)_([0-9A-F]{6,8})\b", swap, old)
        if new != old:
            path.write_text(new)
            print(f"updated {path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
