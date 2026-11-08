#!/bin/bash
# numbers_armsx2.sh <ver> <cube|rp5>: BT4 with ARMSX2's renderer (normal, then the weak profile); the log must not say it fell back
cd "$(dirname "$0")"; V=$1; export BENCH_OUT=$HOME/DragonRageRefs/$V/logs; mkdir -p "$BENCH_OUT"
if [ "$2" = cube ]; then S=192.168.1.183:5555; else S=192.168.1.89:5555; fi
python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=1 meter=1
if [ "$2" = cube ]; then
  WAIT=60 ./bt4cube.sh armsx2; cp logs/bt4_cube_armsx2.* "$BENCH_OUT/"
  adb -s $S shell am force-stop com.dfdx047.dragonrage; bash lowend.sh on 512; sleep 15; bash lowend.sh show
  SLOW=2 BOOT=110 WAIT=60 ./bt4cube.sh low_armsx2; cp logs/bt4_cube_low_armsx2.* "$BENCH_OUT/"
  adb -s $S shell am force-stop com.dfdx047.dragonrage; bash lowend.sh off; sleep 3; bash lowend.sh show
else
  WAIT=75 ./bt4fight.sh $S rp5_armsx2 BT3_GS_VERBOSE=1; cp logs/bt4_rp5_armsx2.* "$BENCH_OUT/"
  WAIT=75 ./bt4fight.sh $S rp5_c480_armsx2 BT3_GS_VERBOSE=1 BT3_TEST_CPUS=0-5; cp logs/bt4_rp5_c480_armsx2.* "$BENCH_OUT/"
  adb -s $S shell am force-stop com.dfdx047.dragonrage
fi
echo ARMSX2-DONE
