#!/bin/bash
# ab_bt4.sh <a.apk> <b.apk> <rounds>: BT4's Dragon Rage renderer on the weak profile, the two versions in turn, same session
cd "$(dirname "$0")"; export OUT=$HOME/DragonRageRefs/ab/logs; mkdir -p "$OUT"
C=192.168.1.183:5555
bt4set() { python3 -c "import lab,sys; lab.settings_set('$C', sys.argv[1:], base=lab.F+'/bt4')" "$@"; }
bash lowend.sh on 512; sleep 15; bash lowend.sh show
for i in $(seq 1 $3); do
  for v in a:$1 b:$2; do t=${v%%:*}; a=${v#*:}
    adb -s $C install -r -d "$a" 2>/dev/null | tail -1
    bt4set bt4_renderer=0 meter=1 dr_renderer=1 gpu_api=0
    SLOW=2 BOOT=110 WAIT=60 ./bt4cube.sh low_dr_$t$i; cp logs/bt4_cube_low_dr_$t$i.* "$OUT/"
  done
done
adb -s $C shell am force-stop com.dfdx047.dragonrage
bash lowend.sh off; sleep 3; bash lowend.sh show
echo AB-BT4-DONE
