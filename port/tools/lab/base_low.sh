#!/bin/bash
# base_low.sh: 0.9.0 on the weak profile, BT3 and BT4 (both renderers), beside the 1.0's numbers; then the test APK back
cd "$(dirname "$0")"; V=0.9.0b; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
S=192.168.1.183:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
L="fx_off=48 frame_skip=1 interp=0 meter=1 present=0 frag_depth=0 smooth_2d=0 filter=0 dr_renderer=1 gpu_api=0 scale4=4 scale=1"
bt4set() { python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=$1 meter=1; }
adb -s $S install -r -d ~/AndroidStudioProjects/DragonRage/DragonRage-0.9.0.apk | tail -1
adb -s $S push ../fight.rec $F/fight.rec >/dev/null
bash lowend.sh on 512; sleep 15; bash lowend.sh show
python3 bench.py cube low_vk1x "$L dr_lite=0" ""
python3 bench.py cube low_vk1x_lite "$L dr_lite=1" ""
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}; bt4set $n; SLOW=2 BOOT=110 WAIT=60 ./bt4cube.sh low_$t; cp logs/bt4_cube_low_$t.* "$BENCH_OUT/"; done
adb -s $S shell am force-stop com.dfdx047.dragonrage
adb -s $S install -r -d ../1.0-test11.apk | tail -1
adb -s $S push ../fight4.rec $F/fight.rec >/dev/null
python3 bench.py cube low_vk1x "$L dr_lite=0" "" 
python3 bench.py cube low_vk1x_lite "$L dr_lite=1" ""
adb -s $S shell am force-stop com.dfdx047.dragonrage
bash lowend.sh off; sleep 3; bash lowend.sh show
echo BASE-DONE
