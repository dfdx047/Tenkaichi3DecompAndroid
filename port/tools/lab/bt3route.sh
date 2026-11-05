#!/bin/bash
# bt3route.sh <out folder> <name> <secs> <settings> [env lines...]: BT3 (or the Tag Team's test fight) on the RP5 with
# these settings (bt3_settings.txt) and env lines (bt3_env.txt), for <secs> s; mid-way, the CPU of each thread (top -H)
# and the clocks; at the end a picture, the log, and bench.py's summary of it (where each picture's time goes).
#   BT3, the recorded route (menu, selection, fight, victory): env BT3_PAD_PLAY=<files>/fight_rp5.rec
#   Tag Team 5 against 5, COM against COM (NATIVE_TAG_TEAM.md): env BT3_TAG_TEAM=1 BT3_TAG_ROSTER=5,5 BT3_DEMO=1
O=$1; N=$2; T=$3; C=$4; shift 4
S=192.168.1.89:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
cd "$(dirname "$0")"; mkdir -p "$O"
python3 -c "import lab,sys; lab.stop('$S'); lab.settings_set('$S', sys.argv[1].split())" "$C"
printf '%s\n' BT3_GS_VERBOSE=1 "$@" | adb -s $S shell "cat > $F/bt3_env.txt"
sleep 1; adb -s $S shell am start -n com.dfdx047.dragonrage/.PlayShortcutActivity >/dev/null
sleep $((T / 2))
adb -s $S shell "top -H -b -n 2 -d 10 -p \$(pidof com.dfdx047.dragonrage)" > "$O/${N}_threads.txt" 2>&1
adb -s $S shell 'for p in /sys/devices/system/cpu/cpufreq/policy*; do echo "$p min $(cat $p/scaling_min_freq) max $(cat $p/scaling_max_freq) cur $(cat $p/scaling_cur_freq)"; done; echo "online $(cat /sys/devices/system/cpu/online) gpu $(cat /sys/class/kgsl/kgsl-3d0/max_gpuclk) $(cat /sys/class/kgsl/kgsl-3d0/gpuclk) busy $(cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage)"' > "$O/${N}_clocks.txt"
sleep $((T - T / 2 - 12))
adb -s $S exec-out screencap -p > "$O/${N}_end.png"
python3 -c "import lab; lab.stop('$S')"; sleep 2
adb -s $S pull $F/bt3_log.txt "$O/${N}_log.txt" >/dev/null
adb -s $S shell rm -f $F/bt3_env.txt
echo "clocks: $(grep policy4 "$O/${N}_clocks.txt") | $(grep online "$O/${N}_clocks.txt")"
python3 bench.py summary "$O/${N}_log.txt"
grep -a -E "^time: |speed .*per frame" "$O/${N}_log.txt" | tail -2 | cut -c1-260
echo "crash: $(grep -a -c -iE 'SIGSEGV|SIGABRT|fatal signal' "$O/${N}_log.txt")"
echo ROUTE-DONE
