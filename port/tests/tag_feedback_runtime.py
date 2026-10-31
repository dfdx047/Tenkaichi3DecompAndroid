"""Render nearby revival feedback with the player locked off, using real HUD code."""
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
out=ROOT/"port/build/tag-feedback-tests"
out.mkdir(parents=True,exist_ok=True)
settings=out/"settings.txt"
settings.write_text("tag_extra_intros=0\n",encoding="utf-8")
env={k:v for k,v in os.environ.items() if not k.startswith("BT3_")}
env.update(BT3_GS="gpu",BT3_DEMO="1",BT3_TAG_TEAM="1",BT3_TAG_PLAY="4",BT3_TAG_ROSTER="2,2",
           BT3_TAG_TEST_CONTROLS="3",BT3_SETTINGS=str(settings),BT3_SAVES=str(out/"saves"),
           BT3_TEST_STAGE="0",
           BT3_UI_SHOT="800:port/build/tag-feedback-tests/frame.ppm",BT3_WINDOW="960x720")
log=out/"runtime.log"
with log.open("w") as f:
    r=subprocess.run([str(ROOT/"port/build/bt3.exe")],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=75)
assert r.returncode==0 and "PASS check=99" in log.read_text(errors="replace"),log
assert (out/"frame.ppm").exists(),"Frame not captured"
print("PASS locked-off enemy HUD and nearby revival feedback",flush=True)
