"""Free For All spawn spacing, terrain safety and rematch determinism."""
import argparse
import os
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[2]
PROBE=re.compile(r"tagspawn tick=(\d+) actor=(\d+) pos=([-\d.]+),([-\d.]+),([-\d.]+)")
def main():
    p=argparse.ArgumentParser(description=__doc__)
    # Native stage id 35 is the menu's Random entry, not a playable map.
    p.add_argument("--stage",type=int,choices=range(35),action="append")
    p.add_argument("--size",choices=range(2,11),type=int,default=10)
    p.add_argument("--scale",choices=(100,200,300),type=int,default=100)
    p.add_argument("--renderer",choices=("none","gpu"),default="none")
    p.add_argument("--shot",type=int,default=0)
    args=p.parse_args()
    failures=[]
    for stage in args.stage or range(35):
        out=ROOT/"port/build/tag-spawn-tests"/f"{stage}-{args.size}-{args.scale}"
        out.mkdir(parents=True,exist_ok=True)
        settings=out/"settings.txt";settings.write_text(f"tag_extra_intros=0\ntag_map_size={args.scale}\n")
        env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
        size=args.size//2
        env.update(BT3_GS=args.renderer,BT3_DEMO="1",BT3_TAG_TEAM="1",BT3_TAG_PLAY=str(min(args.size,4)),
                   BT3_TAG_ROSTER=f"{size},{args.size-size}",BT3_TAG_TEST_STAGE="1",BT3_TAG_TEST_SPAWN="1",
                   BT3_TEST_STAGE=str(stage),BT3_SETTINGS=str(settings),BT3_SAVES=str(out/"saves"),
                   SDL_AUDIO_DRIVER="dummy")
        if args.shot:
            shot=(out/"frame.ppm").relative_to(ROOT).as_posix()
            env.update(BT3_UI_SHOT=f"{args.shot}:{shot}",BT3_WINDOW="960x720")
        log=out/"runtime.log"
        with log.open("w") as f:
            r=subprocess.run([str(ROOT/"port/build/bt3.exe")],cwd=ROOT,env=env,
                             stdout=f,stderr=subprocess.STDOUT,timeout=90)
        text=log.read_text(errors="replace")
        if r.returncode!=0 or "PASS check=99" not in text:
            failures.append(str(log));print(f"FAIL stage {stage}: {log}",flush=True);continue
        first={a:(x,y,z) for tick,a,x,y,z in PROBE.findall(text) if tick=="1"}
        again={a:(x,y,z) for tick,a,x,y,z in PROBE.findall(text) if tick=="40"}
        assert len(first)==args.size and first==again,(first,again)
        print(f"PASS stage {stage}: {args.size} FFA spawns at {args.scale}%",flush=True)
    assert not failures,"\n".join(failures)
if __name__=="__main__":main()
