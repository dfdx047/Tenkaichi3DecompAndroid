"""Ordinary specials must not hide or freeze unrelated fighters."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
CASES={4:"beam",5:"rush",6:"shared-beam",7:"ultimate",8:"pause-off",9:"two-beams"}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe",default="port/build/bt3.exe")
    p.add_argument("--case",choices=CASES.values(),action="append")
    p.add_argument("--renderer",choices=("none","gpu"),default="none")
    p.add_argument("--shot",type=int,default=0)
    p.add_argument("--character",type=int)
    p.add_argument("--slot",type=int,choices=(2,3,4))
    p.add_argument("--data",type=Path,help="Extracted native disc directory")
    args=p.parse_args()
    for kind,name in CASES.items():
        if args.case and name not in args.case:continue
        out=ROOT/"port/build/tag-special-tests"/((args.data.name+"-" if args.data else "")+name+(f"-{args.character}-{args.slot}" if args.character is not None else ""));out.mkdir(parents=True,exist_ok=True)
        settings=out/"settings.txt";settings.write_text("tag_extra_intros=0\n")
        env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
        env.update(BT3_GS=args.renderer,BT3_DEMO="1",BT3_TAG_TEAM="1",BT3_TAG_PLAY="4",
                   BT3_TAG_ROSTER="2,2",BT3_TAG_TEST_CINEMA=str(kind),BT3_SETTINGS=str(settings),
                   BT3_SAVES=str(out/"saves"),BT3_UPDATE_CHECK="0",SDL_AUDIO_DRIVER="dummy")
        if args.data:env["BT3_DATA"]=str(args.data.resolve())
        if args.character is not None:env["BT3_TAG_SPECIAL_CHARACTER"]=str(args.character)
        if args.slot is not None:env["BT3_TAG_SPECIAL_SLOT"]=str(args.slot)
        if args.shot:
            shot=(out/"frame.ppm").relative_to(ROOT).as_posix()
            env.update(BT3_UI_SHOT=f"{args.shot}:{shot}",BT3_WINDOW="960x720")
        log=out/"runtime.log"
        with log.open("w") as f:
            r=subprocess.run([str(ROOT/args.exe)],cwd=ROOT,env=env,
                             stdout=f,stderr=subprocess.STDOUT,timeout=100)
        text=log.read_text(errors="replace")
        assert r.returncode==0 and "PASS check=99" in text,log
        print(f"PASS {name}",flush=True)

if __name__=="__main__":main()
