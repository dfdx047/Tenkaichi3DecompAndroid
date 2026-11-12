#!/usr/bin/env python3
"""Prints symbol_addrs.txt entries for functions whose name appears in an error string they reference.

Only 1:1 cases are emitted: the function references exactly one name, and no other function
references that name. Run after a split, from the repo root.
"""

import collections
import re

DATA_FILES = [
    "asm/data/cod/1EB380.rodata.s",
    "asm/data/cod/1C3400.data.s",
    "asm/data/cod/1FE880.sdata.s",
]
TEXT_FILE = "asm/cod/000000.s"

# Strings that name something other than the function referencing them
# (a callee, a hardware register, a printf literal).
NOT_OWN_NAME = re.compile(r"^(sce\w+|BDEC|null)$")

# "...(name)" at the end, "E123 name: ..." at the start, or ":name:" in the middle.
NAME_RE = re.compile(
    r"\(([A-Za-z_]\w{3,})\)\s*(?:\\n)?$"
    r"|^(?:E?\d+[: ]\s*)?([A-Za-z_]\w*_\w+)\s*:"
    r"|:([A-Za-z_]\w*_\w+):"
)


def main():
    strings = {}
    for path in DATA_FILES:
        cur = None
        for line in open(path):
            if m := re.match(r"dlabel (\S+)", line):
                cur = m.group(1)
            elif (m := re.search(r'\.asciz? "(.*)"', line)) and cur and cur not in strings:
                strings[cur] = m.group(1)

    names = collections.defaultdict(set)
    cur = None
    for line in open(TEXT_FILE):
        if m := re.match(r"glabel (\S+)", line):
            cur = m.group(1)
            continue
        for sym in re.findall(r"%(?:hi|lo|gp_rel)\((D_[0-9A-F]+)", line):
            for m in NAME_RE.finditer(strings.get(sym, "")):
                names[cur].add(next(g for g in m.groups() if g))

    owners = collections.defaultdict(set)
    for func, ns in names.items():
        for n in ns:
            owners[n].add(func)

    for func, ns in sorted(names.items()):
        (name,) = ns if len(ns) == 1 else (None,)
        if name and len(owners[name]) == 1 and func.startswith("func_") and not NOT_OWN_NAME.match(name):
            print(f"{name} = 0x{func[5:]}; // type:func")


if __name__ == "__main__":
    main()
