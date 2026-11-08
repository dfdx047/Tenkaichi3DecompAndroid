#!/bin/bash
# auto_rp5.sh <ver>: BT4 with no renderer chosen on a device that holds ARMSX2's (it must stay), then BT3 with a read that fails
cd "$(dirname "$0")"; V=$1; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
S=192.168.1.89:5555; B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4
adb -s $S shell "sed -i '/^bt4_renderer/d' $B/bt3_settings.txt"
WAIT=75 ./bt4fight.sh $S rp5_auto BT3_GS_VERBOSE=1; cp logs/bt4_rp5_auto.* "$BENCH_OUT/"
adb -s $S shell am force-stop com.dfdx047.dragonrage
adb -s $S shell "grep bt4_renderer $B/bt3_settings.txt"
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=8 scale=2"
python3 bench.py rp5 readfail "$C" "BT3_TEST_READ_FAIL=2000"
echo AUTO-DONE
