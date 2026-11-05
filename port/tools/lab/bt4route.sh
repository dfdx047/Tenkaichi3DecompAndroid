#!/bin/bash
# bt4route.sh <out folder> <name> [env lines...]: the BT4 route on the RP5 (the keys of bt4fight.sh): title, menu,
# selection, a COM-against-COM fight to its end. A screenshot at each scene; with BT4_GS_DUMP=<device folder> among
# the env lines, a PCSX2 GS dump (.gs.zst, 300 vsyncs) of each scene too (bt4_main.cpp: gsdump_now.txt). Mid-fight,
# the CPU of each thread (top -H, no root). Log, pictures and threads in <out folder>/<name>_*.
#   WAIT=<s>   the fight after the selection (default 150: long enough for the end of the round on the RP5)
O=$1; N=$2; shift 2
S=192.168.1.89:5555; B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4
cd "$(dirname "$0")"; mkdir -p "$O"
adb -s $S shell am force-stop com.dfdx047.dragonrage
if [ $# -gt 0 ]; then printf '%s\n' "$@" | adb -s $S shell "cat > $B/bt3_env.txt"; else adb -s $S shell rm -f $B/bt3_env.txt; fi
adb -s $S shell rm -f $B/bt3_log.txt $B/bt3_log.1.txt $B/bt3_log.2.txt $B/gsdump_now.txt
shot() { adb -s $S exec-out screencap -p > "$O/${N}_$1.png"; }
args=("$@")
adb -s $S shell monkey -p com.dfdx047.dragonrage -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 4
python3 ui.py rp5 tap JOGAR >/dev/null || { python3 ui.py rp5 tap "BUDOKAI TENKAICHI 4" >/dev/null; sleep 3; python3 ui.py rp5 tap JOGAR >/dev/null; }
sleep 30
shot 1title
printf '%s\n' "${args[@]}" | grep -q '^BT4_GS_DUMP=' && adb -s $S shell "echo 'title 120' > $B/gsdump_now.txt"
sleep 6
P="python3 kpad.py $S"
$P "start wait:3000 start wait:3000 start wait:4000 start wait:3000 cross wait:4000 start wait:3000 cross wait:4000"
shot 2menu
printf '%s\n' "${args[@]}" | grep -q '^BT4_GS_DUMP=' && adb -s $S shell "echo 'menu 120' > $B/gsdump_now.txt"
sleep 5
$P "down wait:500 down wait:500 down wait:500 cross wait:3000 down wait:500 down wait:500 cross wait:4000 cross wait:5000"
shot 3select
printf '%s\n' "${args[@]}" | grep -q '^BT4_GS_DUMP=' && adb -s $S shell "echo 'select 120' > $B/gsdump_now.txt"
sleep 5
for i in 1 2 3 4 5 6 7 8 9; do $P "cross wait:3000"; done
# the fight's start (the fade: ReadFIFO2 of a PSMCT24 frame) and the fight itself
printf '%s\n' "${args[@]}" | grep -q '^BT4_GS_DUMP=' && adb -s $S shell "echo 'fightstart 400' > $B/gsdump_now.txt"
sleep 25
shot 4fight
printf '%s\n' "${args[@]}" | grep -q '^BT4_GS_DUMP=' && adb -s $S shell "echo 'fight 300' > $B/gsdump_now.txt"
adb -s $S shell "top -H -b -n 2 -d 10 -p \$(pidof com.dfdx047.dragonrage:bt4)" > "$O/${N}_threads.txt" 2>&1
adb -s $S shell 'for p in /sys/devices/system/cpu/cpufreq/policy*; do echo "$p min $(cat $p/scaling_min_freq) max $(cat $p/scaling_max_freq) cur $(cat $p/scaling_cur_freq)"; done; echo "online $(cat /sys/devices/system/cpu/online) gpu $(cat /sys/class/kgsl/kgsl-3d0/max_gpuclk) $(cat /sys/class/kgsl/kgsl-3d0/gpuclk) busy $(cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage)"' > "$O/${N}_clocks.txt"
sleep $(( ${WAIT:-150} - 45 ))
shot 5end
printf '%s\n' "${args[@]}" | grep -q '^BT4_GS_DUMP=' && adb -s $S shell "echo 'end 300' > $B/gsdump_now.txt" && sleep 20
adb -s $S shell "cat $B/bt3_log.2.txt $B/bt3_log.1.txt $B/bt3_log.txt 2>/dev/null" > "$O/${N}_log.txt"
adb -s $S shell am force-stop com.dfdx047.dragonrage
adb -s $S shell rm -f $B/bt3_env.txt
L="$O/${N}_log.txt"
echo "clocks mid-fight: $(grep policy4 $O/${N}_clocks.txt) | $(grep online $O/${N}_clocks.txt)"
echo "$N: renderer: $(grep -a -m1 'bt4: renderer' $L | cut -c1-80) | crash: $(grep -a -c -iE 'SIGSEGV|SIGABRT|fatal|DEVICE_LOST' $L) | stalls: $(grep -a -c stallprobe $L) | dumps: $(grep -a -c 'GS dump .* done' $L)"
grep -a -E '^\[fps\]|logicrate' $L | tail -4
echo ROUTE-DONE
