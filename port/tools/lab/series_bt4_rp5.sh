#!/bin/bash
# series_bt4_rp5.sh <version>: BT4 on the RP5, both renderers
cd "$(dirname "$0")"; V=$1; S=192.168.1.89:5555; O=$HOME/DragonRageRefs/$V/logs
for r in 1:armsx2 0:dr; do n=${r%%:*}; t=${r#*:}
  python3 -c "import lab,sys; lab.settings_set('$S', sys.argv[1:], base=lab.F+'/bt4')" bt4_renderer=$n meter=1
  WAIT=${WAIT:-75} ./bt4fight.sh $S rp5_$t BT3_GS_VERBOSE=1
  cp logs/bt4_rp5_$t.* $O/
done
