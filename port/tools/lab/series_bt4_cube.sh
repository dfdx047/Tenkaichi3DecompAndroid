#!/bin/bash
# series_bt4_cube.sh <version>: BT4 on the Cube, both renderers, as it is and as a G52 MC1
cd "$(dirname "$0")"; V=$1; S=192.168.1.183:5555; O=$HOME/DragonRageRefs/$V/logs
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}
  python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=$n meter=1
  ./bt4cube.sh $t; cp logs/bt4_cube_$t.* $O/ 2>/dev/null
done
bash lowend.sh on 512; sleep 15; bash lowend.sh show
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}
  python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=$n meter=1
  ./bt4cube.sh low_$t; cp logs/bt4_cube_low_$t.* $O/ 2>/dev/null
done
bash lowend.sh off; sleep 3; bash lowend.sh show
