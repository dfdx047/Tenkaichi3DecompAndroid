#!/usr/bin/env python3
"""UI helper: ui.py <dev> dump | tap <text or desc substring> | tapxy x y | key <KEYCODE> | text <s> | shot <file>"""
import re, subprocess, sys, time, xml.etree.ElementTree as ET
DEVS = {"cube": "192.168.1.184:5555", "rp5": "192.168.1.89:5555", "xiaomi": "192.168.1.136:5555"}

def adb(ser, *a, **k):
    return subprocess.run(["adb", "-s", ser, *a], capture_output=True, text=k.get("text", True))

def dump(ser):
    adb(ser, "shell", "uiautomator dump /sdcard/ui.xml")
    x = adb(ser, "shell", "cat /sdcard/ui.xml").stdout
    i = x.find("<?xml")
    return ET.fromstring(x[i:]) if i >= 0 else None

def nodes(root):
    out = []
    for n in root.iter("node"):
        t, d, b = n.get("text", ""), n.get("content-desc", ""), n.get("bounds", "")
        m = re.findall(r"\d+", b)
        if m:
            x1, y1, x2, y2 = map(int, m)
            out.append((t, d, n.get("clickable"), (x1 + x2) // 2, (y1 + y2) // 2, n.get("resource-id", "")))
    return out

def main():
    ser = DEVS.get(sys.argv[1], sys.argv[1]); cmd = sys.argv[2]; a = sys.argv[3:]
    if cmd == "dump":
        for t, d, c, x, y, rid in nodes(dump(ser)):
            if t or d:
                print(f"{x:5d} {y:5d} {'C' if c == 'true' else ' '} {t!r} {d!r} {rid}")
    elif cmd == "tap":
        want = " ".join(a).lower()
        for t, d, c, x, y, rid in nodes(dump(ser)):
            if want in t.lower() or want in d.lower():
                adb(ser, "shell", f"input tap {x} {y}"); print("tap", x, y, t or d); return
        print("not found:", want); sys.exit(1)
    elif cmd == "tapxy":
        adb(ser, "shell", f"input tap {a[0]} {a[1]}")
    elif cmd == "key":
        adb(ser, "shell", f"input keyevent {a[0]}")
    elif cmd == "text":
        adb(ser, "shell", f"input text {a[0]!r}")
    elif cmd == "shot":
        open(a[0], "wb").write(subprocess.run(["adb", "-s", ser, "exec-out", "screencap", "-p"], capture_output=True).stdout)

main()
