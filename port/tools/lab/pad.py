#!/usr/bin/env python3
"""Presses the RG Cube's built-in controller (evdev, root) from the PC.
   pad.py cross [ms]  |  pad.py seq "cross:150 wait:500 down right:80 ..."  |  pad.py shot out.png
"""
import subprocess, sys, time

SER = "192.168.1.183:5555"
DEV = "/dev/input/event7"
KEYS = {"cross": 304, "circle": 305, "triangle": 307, "square": 308, "l1": 310, "r1": 311, "l2": 312, "r2": 313,
        "select": 314, "start": 315}
HAT = {"up": (17, -1), "down": (17, 1), "left": (16, -1), "right": (16, 1)}
# sticks: ABS_X 0, ABS_Y 1 ; ABS_Z 2 (right x), ABS_RZ 5
STICK = {"lsup": (1, -16384), "lsdown": (1, 16384), "lsleft": (0, -16384), "lsright": (0, 16384)}


def cmds(name, ms):
    if name in KEYS:
        c = KEYS[name]
        return [f"sendevent {DEV} 1 {c} 1", f"sendevent {DEV} 0 0 0", f"sleep {ms/1000:.3f}",
                f"sendevent {DEV} 1 {c} 0", f"sendevent {DEV} 0 0 0"]
    if name in HAT:
        a, v = HAT[name]
        return [f"sendevent {DEV} 3 {a} {v}", f"sendevent {DEV} 0 0 0", f"sleep {ms/1000:.3f}",
                f"sendevent {DEV} 3 {a} 0", f"sendevent {DEV} 0 0 0"]
    if name in STICK:
        a, v = STICK[name]
        return [f"sendevent {DEV} 3 {a} {v}", f"sendevent {DEV} 0 0 0", f"sleep {ms/1000:.3f}",
                f"sendevent {DEV} 3 {a} 0", f"sendevent {DEV} 0 0 0"]
    if name == "wait":
        return [f"sleep {ms/1000:.3f}"]
    raise SystemExit("unknown " + name)


def seq(text):
    out = []
    for tok in text.split():
        name, _, ms = tok.partition(":")
        out += cmds(name, int(ms) if ms else 120)
        if name != "wait":
            out.append("sleep 0.15")
    subprocess.run(["adb", "-s", SER, "shell", "su", "-c", "'" + "; ".join(out) + "'"], check=True)


if __name__ == "__main__":
    if sys.argv[1] == "seq":
        seq(" ".join(sys.argv[2:]))
    elif sys.argv[1] == "shot":
        with open(sys.argv[2], "wb") as f:
            f.write(subprocess.run(["adb", "-s", SER, "exec-out", "screencap", "-p"], capture_output=True).stdout)
    else:
        seq(" ".join(sys.argv[1:]))
