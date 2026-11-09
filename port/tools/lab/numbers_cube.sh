#!/bin/bash
# numbers_cube.sh <ver>: the Cube's numbers of a version: BT3 pixels and speed at 1x, BT4 both renderers, then all again as a G52 MC1; back to normal
cd "$(dirname "$0")"; V=$1; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
S=192.168.1.183:5555
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1"
L="fx_off=48 frame_skip=1 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1"
bt4set() { python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=$1 meter=1; }
bash compare_env.sh $V cube 1 shot ""
python3 bench.py cube vk1x "$C" ""
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}; bt4set $n; WAIT=60 ./bt4cube.sh $t; cp logs/bt4_cube_$t.* "$BENCH_OUT/"; done
for i in 1 2 3 4 5 6; do sleep 6; adb -s $S exec-out screencap -p > "$BENCH_OUT/bt4_cube_dr_view$i.png"; done
adb -s $S shell am force-stop com.dfdx047.dragonrage
bash lowend.sh on 512; sleep 15; bash lowend.sh show
python3 bench.py cube low_vk1x "$L dr_lite=0" ""
python3 bench.py cube low_vk1x_lite "$L dr_lite=1" ""
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}; bt4set $n; SLOW=2 BOOT=110 WAIT=60 ./bt4cube.sh low_$t; cp logs/bt4_cube_low_$t.* "$BENCH_OUT/"; done
adb -s $S shell am force-stop com.dfdx047.dragonrage
bash lowend.sh off; sleep 3; bash lowend.sh show
bt4set 1
echo CUBE-DONE
