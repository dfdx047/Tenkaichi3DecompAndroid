#!/bin/bash
# bt4cube.sh <name> [settings k=v ...] : the Cube from the launcher (no BT4 save) to a spectator duel; log in logs/bt4_cube_<name>.txt
C=192.168.1.183:5555; N=$1; shift
B=/sdcard/Android/data/com.dfdx047.dragonrage/files/bt4
cd "$(dirname "$0")"
python3 -c "import lab,sys; lab.settings_set('$C', sys.argv[1:], base=lab.F+'/bt4')" "$@"
adb -s $C shell am force-stop com.dfdx047.dragonrage
echo BT3_GS_VERBOSE=1 | adb -s $C shell "cat > $B/bt3_env.txt"
adb -s $C shell rm -f $B/bt3_log.txt $B/bt3_log.1.txt $B/bt3_log.2.txt
adb -s $C shell monkey -p com.dfdx047.dragonrage -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1; sleep 4
python3 ui.py cube tap JOGAR >/dev/null || { python3 ui.py cube tap "BUDOKAI TENKAICHI 4" >/dev/null; sleep 3; python3 ui.py cube tap JOGAR >/dev/null; }
sleep 48
python3 pad.py seq "start wait:3000 start wait:3000 start wait:4000 wait:3000 start wait:4000"
python3 pad.py seq "cross wait:4000 cross wait:3000 left wait:600 cross wait:5000"
python3 pad.py seq "down wait:500 down wait:500 down wait:500 cross wait:3000 down wait:500 down wait:500 cross wait:4000 cross wait:5000"
for i in 1 2 3 4 5 6 7 8 9; do python3 pad.py seq "cross wait:3000"; done
sleep ${WAIT:-60}
adb -s $C exec-out screencap -p > logs/bt4_cube_$N.png
adb -s $C shell "cat $B/bt3_log.2.txt $B/bt3_log.1.txt $B/bt3_log.txt 2>/dev/null" > logs/bt4_cube_$N.txt
L=logs/bt4_cube_$N.txt
echo "== $N: $(grep -m1 -E 'GPU renderer|renderer:' $L) | $(grep -m1 'internal resolution' $L)"
grep -E "speed [0-9]+%" $L | tail -2 | cut -c1-300
grep -E "\[fps\]" $L | tail -1 | cut -c1-120
grep logicrate $L | tail -1
grep -iE "fatal|abort|signal [0-9]|crash" $L | head -3
