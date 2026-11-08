#!/bin/bash
# finals2.sh: BT3 GL on the RP5, BT4 Dragon Rage on GL on both (dr_renderer=2 gpu_api=1: GL is really used)
cd "$(dirname "$0")"; export BENCH_OUT=$HOME/DragonRageRefs/1.0n/logs
K="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=2 gpu_api=1"
R=192.168.1.89:5555; C=192.168.1.183:5555
bt4set() { python3 -c "import lab,sys; lab.settings_set(sys.argv[1], sys.argv[2:], base=lab.F+'/bt4')" "$@"; }
( python3 bench.py rp5 gl2x_real "$K scale4=8 scale=2" ""
  bt4set $R bt4_renderer=0 meter=1 dr_renderer=2 gpu_api=1; WAIT=60 ./bt4fight.sh $R rp5_dr_glreal BT3_GS_VERBOSE=1; cp logs/bt4_rp5_dr_glreal.* "$BENCH_OUT/"
  bt4set $R bt4_renderer=1 meter=1 dr_renderer=1 gpu_api=0; adb -s $R shell am force-stop com.dfdx047.dragonrage; echo RP5-FIN2 ) &
( bt4set $C bt4_renderer=0 meter=1 dr_renderer=2 gpu_api=1; WAIT=60 ./bt4cube.sh dr_glreal; cp logs/bt4_cube_dr_glreal.* "$BENCH_OUT/"
  bt4set $C bt4_renderer=1 meter=1 dr_renderer=1 gpu_api=0; adb -s $C shell am force-stop com.dfdx047.dragonrage; echo CUBE-FIN2 ) &
wait; echo FIN2
