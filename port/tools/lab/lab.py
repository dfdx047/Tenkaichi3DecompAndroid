#!/usr/bin/env python3
"""Device lab helper.
  lab.py <dev> set key=val ...        edit bt3_settings.txt (bt4: prefix path with bt4/ via --bt4)
  lab.py <dev> env K=V ...            write bt3_env.txt (replaces it); `env` alone deletes it
  lab.py <dev> start | stop           start the game (PlayShortcutActivity) / force-stop the app
  lab.py <dev> log <out>              pull bt3_log.txt
  lab.py <dev> run <secs> <out> [K=V ...]   env, restore saves, start, wait, stop, pull log
  dev: cube | rp5
"""
import subprocess, sys, time

DEVS = {"cube": "192.168.1.184:5555", "rp5": "192.168.1.89:5555", "xiaomi": "192.168.1.136:5555"}
PKG = "com.dfdx047.dragonrage"
F = f"/sdcard/Android/data/{PKG}/files"


def sh(ser, cmd, check=True):
    r = subprocess.run(["adb", "-s", ser, "shell", cmd], capture_output=True, text=True)
    if check and r.returncode:
        raise SystemExit(r.stderr)
    return r.stdout


def settings_set(ser, kv, base=F):
    cur = sh(ser, f"cat {base}/bt3_settings.txt", check=False)
    vals = {}
    order = []
    for line in cur.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            vals[k] = v
            order.append(k)
    for item in kv:
        k, v = item.split("=", 1)
        if k not in vals:
            order.append(k)
        vals[k] = v
    text = "".join(f"{k}={vals[k]}\n" for k in order)
    subprocess.run(["adb", "-s", ser, "shell", f"cat > {base}/bt3_settings.txt"], input=text, text=True, check=True)


def env_set(ser, kv, base=F):
    if not kv:
        sh(ser, f"rm -f {base}/bt3_env.txt", check=False)
        return
    subprocess.run(["adb", "-s", ser, "shell", f"cat > {base}/bt3_env.txt"], input="".join(x + "\n" for x in kv), text=True, check=True)


def stop(ser):
    sh(ser, f"am force-stop {PKG}", check=False)


def start(ser):
    sh(ser, f"am start -n {PKG}/.PlayShortcutActivity", check=False)


def main():
    dev = DEVS.get(sys.argv[1], sys.argv[1])
    cmd = sys.argv[2]
    args = sys.argv[3:]
    if cmd == "set":
        settings_set(dev, args)
    elif cmd == "env":
        env_set(dev, args)
    elif cmd == "start":
        start(dev)
    elif cmd == "stop":
        stop(dev)
    elif cmd == "log":
        subprocess.run(["adb", "-s", dev, "pull", f"{F}/bt3_log.txt", args[0]], check=True, capture_output=True)
    elif cmd == "run":
        secs, out = float(args[0]), args[1]
        stop(dev)
        env_set(dev, args[2:])
        # the same memory card as the recording
        sh(dev, f"cd {F} && rm -rf saves && tar xf /sdcard/Download/lab_saves.tar", check=False)
        time.sleep(1)
        start(dev)
        time.sleep(secs)
        stop(dev)
        time.sleep(1)
        subprocess.run(["adb", "-s", dev, "pull", f"{F}/bt3_log.txt", out], check=True, capture_output=True)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
