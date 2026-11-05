#!/bin/bash
# dia_final.sh <tag>: the closing route of 2026-11-05 on the RP5 (sd480p profile held by tools/device-profile.sh), the 4 modes
# in a row with the APK that is installed: BT3 (dia_smoke.sh: the recorded fight, its checksums against the merges', and a
# Dragon Net lobby), the Tag Team 5 against 5 (COM x COM), BT4 (bt4route.sh, the renderer of the device's settings) and BT2
# (bt2route.sh). Everything in ~/DragonRageRefs/dia/<tag>/; a summary of each at the end.
cd "$(dirname "$0")"; T=$1; O=$HOME/DragonRageRefs/dia/$T; mkdir -p "$O"
R=192.168.1.89:5555
echo "== apk: $(adb -s $R shell dumpsys package com.dfdx047.dragonrage | grep -m1 versionName | tr -d ' ')"
echo "== BT3 (the recorded fight) and Dragon Net"
./dia_smoke.sh "$T" 2>&1 | grep -v WARNING | grep -E "rp5_fight|hash lines|crash lines|dragon net|SMOKE"
python3 fightcmp.py "$HOME/DragonRageRefs/dia/m3/hash.txt" "$O/hash.txt" 2>&1 | tail -1
echo "== Tag Team 5 x 5"
./bt3route.sh "$O" tag5 150 "fx_off=0 frame_skip=0 interp=0 meter=1 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=8 scale=2" \
  BT3_TAG_TEAM=1 BT3_TAG_ROSTER=5,5 BT3_DEMO=1 2>&1 | grep -v WARNING | grep -E "speed|crash" | cut -c1-200
echo "== BT4 ($(adb -s $R shell grep bt4_renderer= /sdcard/Android/data/com.dfdx047.dragonrage/files/bt4/bt3_settings.txt))"
./bt4route.sh "$O" bt4 > "$O/bt4_route.out" 2>&1
python3 bt4stats.py "$O/bt4_log.txt" "$O/bt4_threads.txt" | head -3
grep -a -c -iE 'SIGSEGV|SIGABRT|fatal signal' "$O/bt4_log.txt" | sed 's/^/crash: /'
echo "== BT2"
./bt2route.sh "$O" bt2 2>&1 | grep -v WARNING | grep -E "clocks|SDLThread|crash" | cut -c1-120
python3 -c "import lab; lab.settings_set('$R', ['dr_tag_team=0'])"
adb -s $R shell rm -f /sdcard/Android/data/com.dfdx047.dragonrage/files/bt3_env.txt /sdcard/Android/data/com.dfdx047.dragonrage/files/bt4/bt3_env.txt
echo FINAL-DONE
