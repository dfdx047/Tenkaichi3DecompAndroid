#!/bin/bash
# numbers_rp5.sh <ver>: the RP5's numbers of a version: BT3 pixels and speed at 2x, BT4 both renderers, then held to 2 big + 4 small processors (BT3_TEST_CPUS, no GPU ceiling without root)
cd "$(dirname "$0")"; V=$1; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
S=192.168.1.89:5555
C="fx_off=0 frame_skip=0 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=0 dr_renderer=1 gpu_api=0"
bt4set() { python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=$1 meter=1; }
bash compare_env.sh $V rp5 2 shot ""
python3 bench.py rp5 vk2x "$C scale4=8 scale=2" ""
python3 bench.py rp5 c480_vk1x "$C scale4=4 scale=1" "BT3_TEST_CPUS=0-5"
python3 bench.py rp5 c480_vk1x_lite "fx_off=48 frame_skip=1 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_lite=1 dr_renderer=1 gpu_api=0 scale4=4 scale=1" "BT3_TEST_CPUS=0-5"
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}; bt4set $n; WAIT=75 ./bt4fight.sh $S rp5_$t BT3_GS_VERBOSE=1; cp logs/bt4_rp5_$t.* "$BENCH_OUT/"; done
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}; bt4set $n; WAIT=75 ./bt4fight.sh $S rp5_c480_$t BT3_GS_VERBOSE=1 BT3_TEST_CPUS=0-5; cp logs/bt4_rp5_c480_$t.* "$BENCH_OUT/"; done
adb -s $S shell am force-stop com.dfdx047.dragonrage
bt4set 1
echo RP5-DONE
