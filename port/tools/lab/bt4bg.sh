#!/bin/bash
# bt4bg.sh <out folder> <name> [times]: known bug #12 on the RP5: with BT4 running (after bt4route.sh's selection, or
# any scene), the app to the background (HOME) and back from the recent apps, <times> times (default 4), a picture after
# each return; then the surface lines of the log (bt4gsx: surface gone, VK: Surface lost, NATIVE_WINDOW_IN_USE, crashes).
O=$1; N=$2; T=${3:-4}; S=192.168.1.89:5555; B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4
mkdir -p "$O"
for i in $(seq 1 $T); do
  adb -s $S shell input keyevent KEYCODE_HOME; sleep $((3 + i))
  adb -s $S shell monkey -p com.dfdx047.dragonrage -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 8  # (the task comes back with BT4 on top)
  adb -s $S exec-out screencap -p > "$O/${N}_back$i.png"
  echo "return $i: bt4 process $(adb -s $S shell pidof com.dfdx047.dragonrage:bt4 | tr -d '\r' || echo GONE)"
done
adb -s $S shell "cat $B/bt3_log.2.txt $B/bt3_log.1.txt $B/bt3_log.txt 2>/dev/null" > "$O/${N}_log.txt"
grep -a -E "surface gone|Surface lost|NATIVE_WINDOW_IN_USE|Window is gone|Failed to recreate|SIGSEGV|SIGABRT|DEVICE_LOST|swap chain" "$O/${N}_log.txt" | sort | uniq -c | head -12
echo BG-DONE
