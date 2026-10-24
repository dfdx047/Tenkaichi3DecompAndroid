"""Ten-fighter Namek FFA: real projectiles, aura cleanup and sustained combat."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]

def check_blind_picture(path):
    # This is the engine's framebuffer capture, not a desktop screenshot.
    with path.open("rb") as f:
        assert f.readline().strip()==b"P6",path
        width,height=map(int,f.readline().split())
        assert f.readline().strip()==b"255",path
        pixels=f.read()
    assert len(pixels)==width*height*3,path
    white=[]
    for view in range(4):
        x0=(view%2)*width//2
        y0=(view//2)*height//2
        total=lit=0
        # Avoid health bars and split borders; target markers occupy few pixels.
        for y in range(y0+height//8,y0+height*9//20,3):
            for x in range(x0+width//20,x0+width*9//20,3):
                rgb=pixels[(y*width+x)*3:(y*width+x)*3+3]
                lit+=min(rgb)>245
                total+=1
        white.append(lit/total)
    assert white[2]>.95 and max(white[i] for i in (0,1,3))<.8,(path,white)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--case",choices=("effects","combat","blind"),default="effects")
    p.add_argument("--renderer",choices=("none","gpu"),default="none")
    args=p.parse_args()
    out=ROOT/"port/build/tag-ffa-tests"/args.case
    out.mkdir(parents=True,exist_ok=True)
    settings=out/"settings.txt"
    settings.write_text("tag_extra_intros=0\ntag_map_size=200\ntag_shared_clash=0\n")
    env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS=args.renderer,BT3_DEMO="1",BT3_TAG_TEAM="1",
               BT3_TAG_ROSTER="5,5",BT3_TAG_TEST_SPAWN="1",BT3_TEST_STAGE="2",
               BT3_SETTINGS=str(settings),BT3_SAVES=str(out/"saves"),
               BT3_UPDATE_CHECK="0",BT3_TAG_TRACE="1",SDL_AUDIO_DRIVER="dummy")
    if args.case=="effects":
        env["BT3_TAG_TEST_STAGE"]="4"
    if args.case=="blind":
        if args.renderer=="gpu":
            (out/"frame.ppm").unlink(missing_ok=True)
        env.update(BT3_TAG_TEST_STAGE="5",BT3_TAG_PLAY="4",BT3_TAG_VIEWS="4",
                   BT3_WINDOW="960x720",BT3_UI_SHOT=f"700:{(out/'frame.ppm').as_posix()}")
    log=out/"runtime.log"
    with log.open("w") as f:
        r=subprocess.run([str(ROOT/"port/build/bt3.exe")],cwd=ROOT,env=env,
                         stdout=f,stderr=subprocess.STDOUT,timeout=180)
    text=log.read_text(errors="replace")
    expected="bt3: battle finished" if args.case=="combat" else "PASS check=99"
    assert r.returncode==0 and expected in text,(r.returncode,log)
    if args.case=="blind" and args.renderer=="gpu":
        check_blind_picture(out/"frame.ppm")
    print(f"PASS ten-fighter Namek 200% FFA {args.case}",flush=True)

if __name__=="__main__":main()
