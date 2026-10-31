#!/bin/bash
# tagroom.sh: BT3 with the Tag Team on, both devices: the RP5 opens an online room, the Cube goes to "Join a room"
cd "$(dirname "$0")"; C=192.168.1.183:5555; R=192.168.1.89:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
for s in $C $R; do adb -s $s shell am force-stop com.dfdx047.dragonrage; python3 -c "import lab; lab.settings_set('$s',['dr_tag_team=1'])"; adb -s $s shell am start -n com.dfdx047.dragonrage/.PlayShortcutActivity >/dev/null; done
sleep 22
for i in 1 2 3 4 5 6; do timeout 6 python3 kpad.py $R "start wait:1500"; done
timeout 6 python3 kpad.py $R "cross wait:6000 select:300 wait:2500"
timeout 15 python3 kpad.py $R "down wait:400 down wait:400 down wait:400 down wait:400 down wait:400 cross wait:2500 cross wait:2500"
adb -s $R shell input keyevent KEYCODE_ENTER
sleep 2
for i in 1 2 3 4; do timeout 8 python3 pad.py seq "start wait:2000"; done
timeout 8 python3 pad.py seq "start wait:2000 cross wait:6000 select:300 wait:2500"
timeout 15 python3 pad.py seq "down wait:400 down wait:400 down wait:400 down wait:400 down wait:400 cross wait:2500 down wait:500 cross wait:2500"
