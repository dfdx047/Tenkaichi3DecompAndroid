#!/bin/bash
# compare_env.sh <ver> <cube|rp5> <scale> <name> "<env>" : the fight with those switches, its pictures kept
cd "$(dirname "$0")"; export BENCH_OUT=$HOME/DragonRageRefs/$1/logs; DEV=$2; SC=$3; N=$4; E=$5; mkdir -p "$BENCH_OUT"
[ $DEV = rp5 ] && S=192.168.1.89:5555 || S=192.168.1.183:5555
F=/sdcard/Android/data/com.dfdx047.dragonrage/files
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=$((SC*4)) scale=$SC"
adb -s $S shell "rm -rf $F/port/build/shots; mkdir -p $F/port/build/shots"
python3 bench.py $DEV ${N}_vk${SC}x "$C" "$E BT3_SHOT=150 BT3_SHOT_FROM=5700 BT3_SHOT_TO=7200" 165
adb -s $S pull $F/port/build/shots "$BENCH_OUT/${DEV}_${N}_vk${SC}x" >/dev/null
