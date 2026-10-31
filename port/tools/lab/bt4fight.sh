#!/bin/bash
# bt4fight.sh <serial> <name> [env lines...]: BT4 from the launcher to a spectator duel, then 60 s of fight; log in logs/bt4_<name>.txt
S=$1; N=$2; shift 2
B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4
cd "$(dirname "$0")"
adb -s $S shell am force-stop com.dfdx047.dragonrage
if [ $# -gt 0 ]; then printf '%s\n' "$@" | adb -s $S shell "cat > $B/bt3_env.txt"; else adb -s $S shell rm -f $B/bt3_env.txt; fi
adb -s $S shell rm -f $B/bt3_log.txt $B/bt3_log.1.txt $B/bt3_log.2.txt
adb -s $S shell monkey -p com.dfdx047.dragonrage -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 4
D=$([ $S = 192.168.1.89:5555 ] && echo rp5 || echo cube)
python3 ui.py $D tap JOGAR >/dev/null || { python3 ui.py $D tap "BUDOKAI TENKAICHI 4" >/dev/null; sleep 3; python3 ui.py $D tap JOGAR >/dev/null; }
sleep 30
P="python3 kpad.py $S"; [ $D = cube ] && P="python3 cpad.py $S"
$P "start wait:3000 start wait:3000 start wait:4000 start wait:3000 cross wait:4000 start wait:3000 cross wait:4000"
$P "down wait:500 down wait:500 down wait:500 cross wait:3000 down wait:500 down wait:500 cross wait:4000 cross wait:5000"
for i in 1 2 3 4 5 6 7 8 9; do $P "cross wait:3000"; done
sleep ${WAIT:-75}
adb -s $S exec-out screencap -p > logs/bt4_$N.png
adb -s $S shell "cat $B/bt3_log.2.txt $B/bt3_log.1.txt $B/bt3_log.txt 2>/dev/null" > logs/bt4_$N.txt
echo "$N: $(grep -c 'stallprobe' logs/bt4_$N.txt) stall lines, readback: $(grep 'readback' logs/bt4_$N.txt | head -2 | tr '\n' ' ' | cut -c1-160), logicrate: $(grep logicrate logs/bt4_$N.txt | tail -1)"
