#!/bin/bash
# series_low.sh <version dir>: the Cube as a G52 MC1: BT3 recorded fight; then back to normal
cd "$(dirname "$0")"; V=$1; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
B="python3 bench.py cube"; SER=192.168.1.183:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
C="fx_off=48 frame_skip=1 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1"
adb -s $SER shell "rm -rf $F/port/build/shots; mkdir -p $F/port/build/shots"
python3 bench.py cube shot_vk1x_a "fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1" "BT3_SHOT=150 BT3_SHOT_FROM=5700 BT3_SHOT_TO=7200" 165
rm -rf "$BENCH_OUT/cube_shot_vk1x_a"; adb -s $SER pull $F/port/build/shots "$BENCH_OUT/cube_shot_vk1x_a" >/dev/null
bash lowend.sh on 512; sleep 15; bash lowend.sh show
$B low_vk1x "$C dr_lite=0" ""
$B low_vk1x_lite "$C dr_lite=1" ""
bash lowend.sh off; sleep 3; bash lowend.sh show
