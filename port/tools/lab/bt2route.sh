#!/bin/bash
# bt2route.sh <out folder> <name>: the BT2 route on the RP5 (2026-11-05, docs/registro.md 09:29): the app's "Jogar BT2", title,
# New Game, play without saving, Main Menu > Dueling > COM VS COM > Single Battle, the selection by cross, the fight
# (Wasteland) to its end. A screenshot at each scene; mid-fight the CPU of each thread of :game_bt2 (top -H) and the clocks.
# Keys by bt2key.py (BT2 reads the keyboard). The ISO has to be chosen in the app already ("Selecionar ISO").
#   WAIT=<s>   the fight after the selection (default 180)
O=$1; N=$2
S=192.168.1.89:5555
cd "$(dirname "$0")"; mkdir -p "$O"
shot() { adb -s $S exec-out screencap -p > "$O/${N}_$1.png"; }
K="python3 bt2key.py $S"
adb -s $S shell am force-stop com.dfdx047.dragonrage
adb -s $S shell monkey -p com.dfdx047.dragonrage -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 5
if ! python3 ui.py rp5 tap 'Jogar BT2' >/dev/null 2>&1; then
  for i in 1 2 3; do python3 ui.py rp5 tap 'Tenkaichi 2' >/dev/null 2>&1 && break; adb -s $S shell input swipe 1000 900 1000 400 300; sleep 1; done
  sleep 3; python3 ui.py rp5 tap 'Jogar BT2' >/dev/null 2>&1
fi
sleep 50
shot 1title
$K "start:200 wait:3000 start:200 wait:3000 start:200 wait:4000"      # the intro and the title
$K "cross:200 wait:4000"                                              # New Game
$K "cross:200 wait:3000"                                              # (save?) "No"
$K "left:200 wait:600 cross:200 wait:6000"                            # "Yes": play without saving
shot 2menu
$K "down:200 wait:900 down:200 wait:1200"                             # Main Menu: Dragon Tournament
$K "down:200 wait:1000 cross:200 wait:4000"                           # Dueling
$K "down:200 wait:700 cross:200 wait:5000"                            # COM VS COM (1P VS 2P is grey and skipped)
$K "cross:200 wait:5000"                                              # Single Battle
shot 3select
for i in 1 2 3 4 5 6 7 8; do $K "cross:200 wait:2500"; done           # the fighters, colours, the map
sleep 20
shot 4fight
adb -s $S shell "top -H -b -n 2 -d 10 -p \$(pidof com.dfdx047.dragonrage:game_bt2)" > "$O/${N}_threads.txt" 2>&1
adb -s $S shell 'for p in /sys/devices/system/cpu/cpufreq/policy*; do echo "$p min $(cat $p/scaling_min_freq) max $(cat $p/scaling_max_freq) cur $(cat $p/scaling_cur_freq)"; done; echo "online $(cat /sys/devices/system/cpu/online) gpu $(cat /sys/class/kgsl/kgsl-3d0/max_gpuclk) $(cat /sys/class/kgsl/kgsl-3d0/gpuclk) busy $(cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage)"' > "$O/${N}_clocks.txt"
sleep $(( ${WAIT:-180} - 30 ))
shot 5end
adb -s $S logcat -d -t 4000 > "$O/${N}_logcat.txt" 2>/dev/null
adb -s $S shell am force-stop com.dfdx047.dragonrage
echo "clocks: $(grep policy4 "$O/${N}_clocks.txt") | $(grep online "$O/${N}_clocks.txt")"
awk '/%CPU/ && /TID/ {h=NR} {l[NR]=$0} END {for (i=h+1;i<=h+6;i++) print "  " l[i]}' "$O/${N}_threads.txt" | cut -c1-120
echo "crash: $(grep -a -c -E 'Fatal signal|SIGSEGV|SIGABRT' "$O/${N}_logcat.txt")"
echo BT2ROUTE-DONE
