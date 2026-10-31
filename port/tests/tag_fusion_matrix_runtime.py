"""Native fusion model loads across pairs, costumes, CPU consent and KOs."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
CASES=[
    ("gogeta",3,34,0,53,4,False,False),
    ("gogeta-costume2",3,34,1,53,4,False,False),
    ("gogeta-cpu",3,34,0,53,1,False,False),
    ("gotenks",46,44,0,48,4,False,False),
    ("ss4-gogeta",10,38,0,54,4,False,False),
    ("simultaneous",3,34,0,53,4,True,False),
    ("potara-cpu",3,34,0,51,1,False,False),
    ("fused-ko",3,34,0,53,4,False,True),
    ("fused-rematch",3,34,0,51,4,False,False),
]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--case",action="append")
    args=p.parse_args()
    out=ROOT/"port/build/tag-fusion-matrix";out.mkdir(parents=True,exist_ok=True)
    settings=out/"settings.txt";settings.write_text("tag_extra_intros=0\n")
    passed=[]
    for name,a,b,costume,result,players,double,ko in CASES:
        if args.case and name not in args.case:continue
        env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
        env.update(BT3_GS="none",BT3_DEMO="1",BT3_SETTINGS=str(settings),
                   BT3_SAVES=str(out/"saves"),BT3_TAG_TEAM="1",BT3_TAG_PLAY=str(players),
                   BT3_TAG_VIEWS="4",BT3_TAG_ROSTER="3,2" if ko else "2,2",
                   BT3_TAG_TEST_FUSION="2",BT3_TAG_FUSION_PAIR=f"{a},{b},{costume}",
                   BT3_TAG_FUSION_RESULT=str(result),BT3_TAG_TRACE="1",SDL_AUDIO_DRIVER="dummy")
        if double:env["BT3_TAG_FUSION_DOUBLE"]="1"
        if ko:env["BT3_TAG_FUSION_KO"]="1"
        if name=="fused-rematch":env["BT3_TAG_FUSION_REMATCH"]="1"
        log=out/f"{name}.log"
        with log.open("w") as f:
            r=subprocess.run([str(ROOT/"port/build/bt3.exe")],cwd=ROOT,env=env,
                             stdout=f,stderr=subprocess.STDOUT,timeout=120)
        text=log.read_text(errors="replace")
        assert r.returncode==0 and "PASS check=99" in text,log
        print(f"PASS {name}",flush=True);passed.append(name)
    (out/"results.json").write_text(json.dumps(passed,indent=2)+"\n")
if __name__=="__main__":main()
