#!/usr/bin/env python3
"""BT2's keyboard mapping (bt2/runtime/host/input_mapping.h) by adb: bt2key.py <serial> "cross:200 wait:500 down ..." """
import subprocess, sys, time
K = {"cross": "Z", "circle": "X", "square": "A", "triangle": "S", "start": "ENTER", "select": "DEL", "l1": "Q", "r1": "W", "l2": "E", "r2": "R",
     "up": "DPAD_UP", "down": "DPAD_DOWN", "left": "DPAD_LEFT", "right": "DPAD_RIGHT"}
ser = sys.argv[1]
for tok in " ".join(sys.argv[2:]).split():
    name, _, ms = tok.partition(":")
    if name == "wait":
        time.sleep(int(ms) / 1000)
        continue
    subprocess.run(["adb", "-s", ser, "shell", "input", "keycombination", "-t", ms or "200", "KEYCODE_SCROLL_LOCK", "KEYCODE_" + K[name]], check=True)
    time.sleep(0.2)
