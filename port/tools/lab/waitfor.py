#!/usr/bin/env python3
"""waitfor.py <ref.png> <press> <tries>: press a button until the Cube's screen looks like ref (mean diff < 18)."""
import io, subprocess, sys, time
import numpy as np
from PIL import Image
ref = np.asarray(Image.open(sys.argv[1]).convert("L").resize((90, 90)), float)
for i in range(int(sys.argv[3])):
    png = subprocess.run(["adb", "-s", "192.168.1.183:5555", "exec-out", "screencap", "-p"], capture_output=True).stdout
    cur = np.asarray(Image.open(io.BytesIO(png)).convert("L").resize((90, 90)), float)
    d = np.abs(cur - ref).mean()
    if d < 18:
        print(f"reached {sys.argv[1]} after {i} presses (diff {d:.1f})"); sys.exit(0)
    subprocess.run(["python3", "pad.py", "seq", f"{sys.argv[2]}:200 wait:3000"])
print(f"not reached {sys.argv[1]} (diff {d:.1f})"); sys.exit(1)
