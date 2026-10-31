#!/bin/bash
# dense.sh <ver> <cube|rp5> <name> "<env>": the fight at 1x, a picture every 30 frames from 2430 to 7200
cd "$(dirname "$0")"; export BENCH_OUT=$HOME/DragonRageRefs/$1/logs; DEV=$2; N=$3; E=$4; mkdir -p "$BENCH_OUT"
[ $DEV = rp5 ] && S=192.168.1.89:5555 || S=192.168.1.183:5555
F=/sdcard/Android/data/com.dfdx047.dragonrage/files
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1"
adb -s $S shell "rm -rf $F/port/build/shots; mkdir -p $F/port/build/shots"
python3 bench.py $DEV ${N} "$C" "$E BT3_SHOT=30 BT3_SHOT_FROM=2430 BT3_SHOT_TO=7200" 165
adb -s $S pull $F/port/build/shots "$BENCH_OUT/${DEV}_${N}" >/dev/null
adb -s $S shell "rm -rf $F/port/build/shots"
