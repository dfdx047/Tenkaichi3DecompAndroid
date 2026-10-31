#!/usr/bin/env python3
"""res.py <file> show            : list conflict hunks (index, line, sizes)
   res.py <file> <idx> ours|theirs|both|theirs+ours : resolve hunk idx
   res.py <file> <idx> text <path> : replace hunk idx with the contents of <path>"""
import re, sys
p = sys.argv[1]
lines = open(p).read().split("\n")
hunks = []
i = 0
while i < len(lines):
    if lines[i].startswith("<<<<<<< "):
        a = i
        m = next(j for j in range(a, len(lines)) if lines[j] == "=======")
        b = next(j for j in range(m, len(lines)) if lines[j].startswith(">>>>>>> "))
        hunks.append((a, m, b))
        i = b + 1
    else:
        i += 1
if sys.argv[2] == "show":
    for k, (a, m, b) in enumerate(hunks):
        print(f"{k}: line {a+1}: ours {m-a-1} lines, theirs {b-m-1} lines")
    sys.exit()
k = int(sys.argv[2]); mode = sys.argv[3]
a, m, b = hunks[k]
ours, theirs = lines[a+1:m], lines[m+1:b]
new = {"ours": ours, "theirs": theirs, "both": ours + theirs, "theirs+ours": theirs + ours}.get(mode)
if mode == "text":
    new = open(sys.argv[4]).read().rstrip("\n").split("\n")
lines[a:b+1] = new
open(p, "w").write("\n".join(lines))
