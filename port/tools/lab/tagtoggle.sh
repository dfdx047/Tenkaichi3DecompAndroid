#!/bin/bash
# tagtoggle.sh <out folder>: the Tag Team switch of the launcher (dr_tag_team), both ways, on the RP5: BT3 opened by the
# app as a player would, the main menu, SELECT; a picture of each (on: the Tag Team's modes; off: the menu as it was).
cd "$(dirname "$0")"; O=$1; mkdir -p "$O"; R=192.168.1.89:5555; F=/sdcard/Android/data/com.dfdx047.dragonrage/files
adb -s $R shell rm -f $F/bt3_env.txt
for v in 1 0; do
  adb -s $R shell am force-stop com.dfdx047.dragonrage
  python3 -c "import lab; lab.settings_set('$R',['dr_tag_team=$v'])"
  adb -s $R shell am start -n com.dfdx047.dragonrage/.PlayShortcutActivity >/dev/null
  sleep 22
  for i in 1 2 3 4 5 6; do timeout 6 python3 kpad.py $R "start wait:1500"; done
  timeout 8 python3 kpad.py $R "cross wait:6000"
  adb -s $R exec-out screencap -p > "$O/tag${v}_menu.png"
  timeout 6 python3 kpad.py $R "select:300 wait:2500"
  adb -s $R exec-out screencap -p > "$O/tag${v}_select.png"
  adb -s $R shell am force-stop com.dfdx047.dragonrage; sleep 2
  adb -s $R pull $F/bt3_log.txt "$O/tag${v}_log.txt" >/dev/null
  echo "dr_tag_team=$v: crash $(grep -a -c -iE 'SIGSEGV|SIGABRT|fatal' "$O/tag${v}_log.txt"), tag lines $(grep -a -c -iE 'tagteam|tag team|taglink' "$O/tag${v}_log.txt")"
done
python3 -c "import lab; lab.settings_set('$R',['dr_tag_team=0'])"
echo TAG-DONE
