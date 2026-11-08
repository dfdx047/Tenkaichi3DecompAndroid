#!/bin/bash
# bt2run.sh <rp5|cube> <seconds between pictures> <pictures> "<pad script>": BT2 with that script, pictures in logs/f<n>.png
D=$1; [ $D = rp5 ] && SER=192.168.1.89:5555 || SER=192.168.1.183:5555
F=/sdcard/Android/data/com.dfdx047.dragonrage/files; T=$(mktemp)
adb -s $SER shell am force-stop com.dfdx047.dragonrage
adb -s $SER shell cat $F/bt2/user/android | grep -v '^pad_script\|^sample' > $T; echo "pad_script = $4" >> $T
adb -s $SER push $T $F/bt2/user/android >/dev/null; rm -f $T
adb -s $SER shell monkey -p com.dfdx047.dragonrage -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 4
python3 ui.py $D tap 'BUDOKAI TENKAICHI 2' >/dev/null; sleep 2; python3 ui.py $D tap 'Jogar BT2' >/dev/null
for n in $(seq 1 $3); do sleep $2; adb -s $SER exec-out screencap -p > logs/f$n.png; done
