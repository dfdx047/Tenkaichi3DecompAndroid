#!/usr/bin/env python3
"""Keyboard presses held for a moment (adb input keycombination) on a device without root: kpad.py <serial> "start wait:1500 cross ..." """
import subprocess, sys, time
K = {"select": "KEYCODE_SHIFT_RIGHT", "cross": "KEYCODE_K", "circle": "KEYCODE_L", "square": "KEYCODE_J", "triangle": "KEYCODE_I", "start": "KEYCODE_ENTER",
     "up": "KEYCODE_DPAD_UP", "down": "KEYCODE_DPAD_DOWN", "left": "KEYCODE_DPAD_LEFT", "right": "KEYCODE_DPAD_RIGHT"}
ser = sys.argv[1]
for tok in " ".join(sys.argv[2:]).split():
    name, _, ms = tok.partition(":")
    if name == "wait":
        time.sleep(int(ms) / 1000)
        continue
    subprocess.run(["adb", "-s", ser, "shell", "input", "keycombination", "-t", ms or "150", "KEYCODE_SCROLL_LOCK", K[name]], check=True)
    time.sleep(0.15)
