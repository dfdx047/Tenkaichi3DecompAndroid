#!/bin/bash
# auto_cube.sh <ver>: BT4 on the weak profile with no renderer chosen: ARMSX2's, then Dragon Rage's from the next start
cd "$(dirname "$0")"; V=$1; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
S=192.168.1.183:5555; B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4
adb -s $S install -r -d ../1.0-test11.apk | tail -1
adb -s $S shell "sed -i '/^bt4_renderer/d' $B/bt3_settings.txt"
bash lowend.sh on 512; sleep 15; bash lowend.sh show
SLOW=2 BOOT=110 WAIT=60 ./bt4cube.sh auto1; cp logs/bt4_cube_auto1.* "$BENCH_OUT/"
adb -s $S shell am force-stop com.dfdx047.dragonrage
SLOW=2 BOOT=110 WAIT=60 ./bt4cube.sh auto2; cp logs/bt4_cube_auto2.* "$BENCH_OUT/"
adb -s $S shell am force-stop com.dfdx047.dragonrage
bash lowend.sh off; sleep 3; bash lowend.sh show
adb -s $S shell "grep bt4_renderer $B/bt3_settings.txt"
echo AUTO-DONE
