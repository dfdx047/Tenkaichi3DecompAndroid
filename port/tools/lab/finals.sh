#!/bin/bash
# finals.sh: the rest of the battery on the final build: GL and Lite on each device, BT4 on GL, BT4 at 60 on the RP5
cd "$(dirname "$0")"; V=1.0n; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1"
L="fx_off=48 frame_skip=1 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=1 dr_renderer=1 gpu_api=0"
R=192.168.1.89:5555; K=192.168.1.183:5555
bt4set() { python3 -c "import lab,sys; lab.settings_set(sys.argv[1], sys.argv[2:], base=lab.F+'/bt4')" "$@"; }
( python3 bench.py rp5 gl2x "$C gpu_api=1 scale4=8 scale=2" ""
  python3 bench.py rp5 lite2x "$L scale4=8 scale=2" ""
  bt4set $R bt4_renderer=0 meter=1 gpu_api=1; WAIT=60 ./bt4fight.sh $R rp5_dr_gl BT3_GS_VERBOSE=1; cp logs/bt4_rp5_dr_gl.* "$BENCH_OUT/"
  bt4set $R bt4_renderer=1 meter=1 gpu_api=1; WAIT=60 ./bt4fight.sh $R rp5_armsx2_gl BT3_GS_VERBOSE=1; cp logs/bt4_rp5_armsx2_gl.* "$BENCH_OUT/"
  bt4set $R bt4_renderer=1 meter=1 gpu_api=0 bt4_fps60=1; WAIT=75 ./bt4fight.sh $R rp5_armsx2_60 BT3_GS_VERBOSE=1 BT4_FPS60_KEEP=1; cp logs/bt4_rp5_armsx2_60.* "$BENCH_OUT/"
  bt4set $R bt4_fps60=0 bt4_renderer=1
  adb -s $R shell am force-stop com.dfdx047.dragonrage; echo RP5-FIN ) &
( python3 bench.py cube gl1x "$C gpu_api=1 scale4=4 scale=1" ""
  python3 bench.py cube lite1x "$L scale4=4 scale=1" ""
  bt4set $K bt4_renderer=0 meter=1 gpu_api=1; WAIT=60 ./bt4cube.sh dr_gl; cp logs/bt4_cube_dr_gl.* "$BENCH_OUT/"
  bt4set $K bt4_renderer=1 meter=1 gpu_api=0
  adb -s $K shell am force-stop com.dfdx047.dragonrage; echo CUBE-FIN ) &
wait; echo FIN
