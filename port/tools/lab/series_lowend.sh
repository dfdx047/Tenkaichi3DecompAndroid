# series_lowend.sh <tag>: BT3 (recorded fight) and BT4 (duel) on the Cube as it is set up now (lowend.sh)
cd "$(dirname "$0")"; T=$1; S=192.168.1.183:5555
C="fx_off=0 interp=0 meter=1 present=0 texture_pack=0"
bash lowend.sh show
python3 bench.py cube ${T}_gl_1x_fs0 "$C frame_skip=0 scale4=4 scale=1 dr_renderer=2 gpu_api=1" ""
python3 bench.py cube ${T}_gl_1x_fs1 "$C frame_skip=1 scale4=4 scale=1 dr_renderer=2 gpu_api=1" ""
[ -n "$BT3ONLY" ] || python3 bench.py cube ${T}_vk_1x_fs0 "$C frame_skip=0 scale4=4 scale=1 dr_renderer=1 gpu_api=0" ""
for ra in ${BT4RUNS:-"1 0" "1 1" "0 0"}; do set -- $ra; r=$1; a=$2
  N=cube_${T}_r${r}_a${a}
  python3 -c "import lab; lab.settings_set('$S', ['bt4_fps60=0','meter=1','bt4_renderer=$r','gpu_api=$a','dr_renderer=$((a+1))','scale4=4','scale=1'], base=lab.F+'/bt4')"
  WAIT=60 bash bt4fight.sh $S $N > /dev/null 2>&1
  python3 - logs/bt4_$N.txt $N <<'PY'
import re, sys
t = open(sys.argv[1], errors="replace").read()
lr = [float(x) for x in re.findall(r"\[logicrate\] ([\d.]+)", t)]
gm = [float(x) for x in re.findall(r"guest_ms=([\d.]+)", t)]
print(f"{sys.argv[2]}: fight updates/s {sum(lr)/max(1,len(lr)):.1f} ({len(lr)} x 5 s, min {min(lr) if lr else 0}), guest_ms {sum(gm[len(gm)//2:])/max(1,len(gm)-len(gm)//2):.1f}")
PY
done
bash lowend.sh show
