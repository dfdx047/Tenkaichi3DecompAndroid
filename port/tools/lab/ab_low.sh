#!/bin/bash
# ab_low.sh <a.apk> <b.apk> <rounds>: BT3 on the weak profile, the two versions in turn: heat and order show apart from the version
cd "$(dirname "$0")"; export BENCH_OUT=$HOME/DragonRageRefs/ab/logs; mkdir -p "$BENCH_OUT"
S=192.168.1.183:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
L="fx_off=48 frame_skip=1 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1 dr_lite=0"
bash lowend.sh on 512; sleep 15; bash lowend.sh show
for i in $(seq 1 $3); do
  adb -s $S install -r -d "$1" 2>/dev/null | tail -1; adb -s $S push ../fight.rec $F/fight.rec >/dev/null
  python3 bench.py cube low_a$i "$L" ""
  adb -s $S install -r -d "$2" 2>/dev/null | tail -1; adb -s $S push ../fight4.rec $F/fight.rec >/dev/null
  python3 bench.py cube low_b$i "$L" ""
done
adb -s $S shell am force-stop com.dfdx047.dragonrage
bash lowend.sh off; sleep 3; bash lowend.sh show
echo AB-DONE
