"""Expanded stages through the real loader, collision queries and rematch."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]
PROBE=re.compile(r"tagstageprobe phase=(\d+) stage=(\d+) scale=([-\d.]+) radius=([-\d.]+) top=([-\d.]+) bottom=([-\d.]+) water=([-\d.]+) ground=([-\d.]+)")

def run(exe,stage,percent,data=None):
    out=ROOT/"port/build/tag-stage-tests"/("bt4" if data else "bt3")/f"{stage}-{percent}"
    out.mkdir(parents=True,exist_ok=True)
    settings=out/"settings.txt"
    settings.write_text(f"tag_map_size={percent}\ntag_extra_intros=0\n")
    env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none",BT3_DEMO="1",BT3_TAG_TEAM="1",BT3_TAG_PLAY="4",
               BT3_TAG_ROSTER="2,2",BT3_TEST_STAGE=str(stage),BT3_TAG_TEST_STAGE="1",
               BT3_SETTINGS=str(settings),BT3_SAVES=str(out/"saves"),SDL_AUDIO_DRIVER="dummy")
    if data:env["BT3_DATA"]=str(Path(data).resolve())
    log=out/"runtime.log"
    with log.open("w") as f:
        result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=60)
    text=log.read_text(errors="replace")
    assert result.returncode==0 and "PASS check=99" in text,log
    probes=[list(map(float,m)) for m in PROBE.findall(text)]
    assert len(probes)==3 and all(int(p[1])==stage for p in probes),probes
    assert probes[0][2:]==probes[-1][2:],"Rematch changed stage dimensions"
    if percent>100:
        assert f"tagstage: scale={percent}" in text and "unsupported" not in text,log
    return probes[0][2:]

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe",default="port/build/bt3.exe")
    p.add_argument("--stage",type=int,choices=range(35),action="append")
    args=p.parse_args();exe=(ROOT/args.exe).resolve();results=[]
    for stage in args.stage or [0,2,3,7,9,12,15,26,33,34]:
        baseline=run(exe,stage,100);expanded=run(exe,stage,200)
        for a,b in zip(baseline,expanded):
            assert abs(a*2-b)<.1,(stage,baseline,expanded)
        results.append(dict(stage=stage,normal=baseline,expanded=expanded))
        print(f"PASS stage {stage}: arena, water, collision and rematch",flush=True)
    (ROOT/"port/build/tag-stage-tests/results.json").write_text(json.dumps(results,indent=2)+"\n")
