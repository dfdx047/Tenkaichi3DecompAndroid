#!/bin/bash
# bt2room.sh: BT2 on both devices from the launcher to the Online Mode's three ways in (the RP5 creates a room; the code is read from its screen)
cd "$(dirname "$0")"; C=192.168.1.183:5555; R=192.168.1.89:5555
for d in cube rp5; do s=$C; [ $d = rp5 ] && s=$R; adb -s $s shell am force-stop com.dfdx047.dragonrage; adb -s $s shell am start -n com.dfdx047.dragonrage/.MainActivity >/dev/null; sleep 3; python3 ui.py $d tap "BUDOKAI TENKAICHI 2" >/dev/null; sleep 3; python3 ui.py $d tap "Jogar BT2" >/dev/null; done
sleep 16
for i in 1 2 3 4 5 6 7 8 9; do python3 bt2key.py $R "start wait:1600" & python3 bt2key.py $C "start wait:1600"; wait; done
sleep 2
python3 bt2key.py $R "start wait:3000 start wait:3000 cross wait:5000" & python3 bt2key.py $C "cross wait:5000"; wait
python3 bt2key.py $R "up wait:700 cross wait:3500 cross wait:2000" & python3 bt2key.py $C "up wait:700 cross wait:3500 cross wait:2000"; wait
python3 bt2key.py $R "cross wait:5000"
