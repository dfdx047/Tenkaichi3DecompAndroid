#!/bin/bash
# series_base.sh <version dir>: the reference runs on one device. Usage: DEV=cube|rp5
cd "$(dirname "$0")"; V=$1; DEV=$2; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
B="python3 bench.py $DEV"
SER=$(python3 -c "import lab; print(lab.DEVS['$DEV'])")
F=/sdcard/Android/data/com.dfdx047.dragonrage/files
shots() { # name settings env
  adb -s $SER shell "rm -rf $F/port/build/shots"; adb -s $SER shell "mkdir -p $F/port/build/shots"
  $B $1 "$2" "BT3_SHOT=150 BT3_SHOT_FROM=5700 BT3_SHOT_TO=7200 $3" 165
  adb -s $SER pull $F/port/build/shots "$BENCH_OUT/$1" >/dev/null
}
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0"
if [ $DEV = cube ]; then
  shots ${DEV}_shot_vk1x_a "$C dr_renderer=1 gpu_api=0 scale4=4 scale=1" ""
  shots ${DEV}_shot_vk1x_b "$C dr_renderer=1 gpu_api=0 scale4=4 scale=1" ""
  $B vk1x "$C dr_renderer=1 gpu_api=0 scale4=4 scale=1" ""
  $B vk2x "$C dr_renderer=1 gpu_api=0 scale4=8 scale=2" ""
  $B gl2x "$C dr_renderer=2 gpu_api=1 scale4=8 scale=2" ""
else
  shots ${DEV}_shot_vk1x_a "$C dr_renderer=1 gpu_api=0 scale4=4 scale=1" ""
  shots ${DEV}_shot_vk2x_a "$C dr_renderer=1 gpu_api=0 scale4=8 scale=2" ""
  shots ${DEV}_shot_vk2x_b "$C dr_renderer=1 gpu_api=0 scale4=8 scale=2" ""
  $B vk2x "$C dr_renderer=1 gpu_api=0 scale4=8 scale=2" ""
  $B vk4x "$C dr_renderer=1 gpu_api=0 scale4=16 scale=4" ""
  $B gl2x "$C dr_renderer=2 gpu_api=1 scale4=8 scale=2" ""
fi
