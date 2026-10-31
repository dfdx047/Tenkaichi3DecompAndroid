"""Overlapping authored rush/ultimate animations and native transformations."""
import os
import argparse
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument("--exe",default="port/build/bt3.exe");a=p.parse_args()
out=ROOT/"port/build/tag-cinema-matrix";out.mkdir(parents=True,exist_ok=True)
settings=out/"settings.txt";settings.write_text("tag_extra_intros=0\n")
for kind,name in [(1,"kaioken-rush"),(2,"spirit-bomb"),(3,"dragon-fist"),(12,"shared-ultimate-transform")]:
    env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none",BT3_DEMO="1",BT3_SETTINGS=str(settings),
               BT3_SAVES=str(out/"saves"),BT3_TAG_TEAM="1",BT3_TAG_PLAY="4",
               BT3_TAG_ROSTER="2,2",BT3_TAG_TEST_CINEMA=str(kind),SDL_AUDIO_DRIVER="dummy")
    log=out/f"{name}.log"
    with log.open("w") as f:
        r=subprocess.run([str(ROOT/a.exe)],cwd=ROOT,env=env,
                         stdout=f,stderr=subprocess.STDOUT,timeout=90)
    text=log.read_text(errors="replace")
    assert r.returncode==0 and "PASS check=99" in text,log
    print(f"PASS {name}: actual target, concurrent transformation, damage and camera recovery",flush=True)
