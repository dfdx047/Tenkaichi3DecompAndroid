"""Exercise the native mod menus with real disc assets and scripted pad input.

No prepared matches or emulator states. Saves and settings belong to this test.
Run from the repository root with --case settings, back, or match.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
CASES = {
    "settings": {
        "pad": "150:select,180:down,210:down,240:down,270:down,300:down,320:down,380:cross,440:cross,500:down,560:right,680:triangle,740:triangle,800:select",
        "end": r"frame=\d+ page=4 state=0,0,1,3,0",
        "after": 800,
        "states": [(4, 4), (4, 5), (4, 1), (4, 0)],
    },
    "back": {
        "pad": "150:select,210:cross,270:down,310:down,350:cross,530:start,650:triangle,800:triangle,860:triangle,920:triangle",
        "end": r"frame=\d+ page=4 state=0,0,1,3,0",
        "after": 920,
        "states": [(4, 2), (4, 3), (40, 0), (4, 3), (4, 2), (4, 1), (4, 0)],
    },
    "match": {
        "pad": "150:select,210:cross,270:cross,450:start,540:cross,600:cross,660:cross,720:cross,780:right,840:cross,900:cross,960:cross,1020:cross,1080:cross,1140:cross,1200:cross,1260:cross,1320:right,1380:cross,1440:cross,1500:cross,1560:cross,1700:cross",
        "end": r"tagintro actor=3 .*state=1",
        "after": 0,
        "states": [(4, 1), (4, 2), (4, 3), (40, 0), (40, 1)],
    },
}
def roster_pad(left=2,right=2,duplicates=False,cpu_only=False,cpu_steps=0):
    events=["150:select","210:cross","270:cross","450:start"]
    tick=540
    def press(key):
        nonlocal tick
        events.append(f"{tick}:{key}")
        tick+=60
    for side,count in enumerate((left,right)):
        for member in range(count):
            if member:
                press("cross")  # open next free slot
                for _ in range(0 if duplicates else (0,1,2,4,5)[member]): press("right")
            # Side 2 starts one column to the right (Kid Gohan). Its third
            # and fifth choices open Vegeta/Trunks's form picker first.
            for _ in range(4 if not duplicates and side==1 and member in (2,4) else 3):
                press("cross") # fighter, optional form, equipment, colour
            if cpu_only or side!=0 or member!=0:
                for _ in range(cpu_steps): press("down")
                press("cross") # CPU difficulty
        if count<5: press("right") # End is optional, not forced after two
        press("cross")
    tick+=80
    press("cross") # stage
    return ",".join(events)

def bt4_roster_pad(left=2,right=2,cpu_only=False,cpu_steps=0):
    # Both first BT4 grid cells have a form picker. Repeated fighters also
    # exercise the native mod's duplicate-selection rule.
    events=["150:select","210:cross","270:cross","450:start"];tick=540
    for side,count in enumerate((left,right)):
        for member in range(count):
            for _ in range(4+(member>0)):
                events.append(f"{tick}:cross");tick+=60
            if cpu_only or side!=0 or member!=0:
                for key in ['down']*cpu_steps+['cross']:
                    events.append(f"{tick}:{key}");tick+=60
        for key in (('right','cross') if count<5 else ('cross',)):
            events.append(f"{tick}:{key}");tick+=60
    tick+=80;events.append(f"{tick}:cross")
    return ','.join(events)

def adapt_bt4_roster(pad, sizes, duplicates=False,cpu_only=False,cpu_steps=0):
    """Replace only character selection, keeping mode/setup and later inputs.

    CPU-only selection starts later; return/result cases have later actions
    which must move by the same duration as BT4's extra form confirmations.
    """
    def parse(value):
        return [(int(t),key) for t,key in (event.split(':',1) for event in value.split(',') if event)]
    events=parse(pad)
    before=parse(roster_pad(*sizes,duplicates=duplicates,cpu_only=cpu_only,cpu_steps=cpu_steps))[4:]
    after=parse(bt4_roster_pad(*sizes,cpu_only=cpu_only,cpu_steps=cpu_steps))[4:]
    for start in range(len(events)-len(before)+1):
        offset=events[start][0]-before[0][0]
        if events[start:start+len(before)]==[(t+offset,k) for t,k in before]:
            delta=after[-1][0]-before[-1][0]
            events=(events[:start]+[(t+offset,k) for t,k in after]+
                    [(t+delta,k) for t,k in events[start+len(before):]])
            return ','.join(f'{t}:{k}' for t,k in events)
    return pad
CASES["match"]["pad"]=roster_pad()
CASES["camera-views"] = {
    "pad": "150:select,180:down,210:down,240:down,270:down,300:down,330:down,350:cross,"
           "410:down,450:down,490:down,550:cross,"
           "610:down,650:down,690:down,750:left,850:triangle,910:triangle,970:select",
    "end": r"frame=\d+ page=4 state=0,0,1,3,0",
    "after":970, "states":[(4,4),(4,8),(4,4),(4,1),(4,0)],
}
CASES["revival-ring"] = {
    "pad": "150:select,180:down,210:down,240:down,270:down,300:down,330:down,350:cross,410:cross,"
           "470:down,510:down,550:down,590:down,630:down,670:down,710:down,770:right,"
           "830:down,890:right,1010:triangle,1070:triangle,1130:select",
    "end": r"frame=\d+ page=4 state=0,0,1,3,0",
    "after":1130,"states":[(4,4),(4,5),(4,4),(4,1),(4,0)],
    "env":{"BT3_UI_SHOT":"950:port/build/tag-menu-tests/revival-ring/menu.ppm"},
}
CASES["fusion-settings"] = {
    # The last group and its Back row use the native second label sheet.
    "pad": "150:select,180:down,210:down,240:down,270:down,300:down,330:down,350:cross,"
           "410:down,450:down,490:down,530:down,570:down,610:down,650:down,710:cross,"
           "770:down,810:down,850:down,890:down,930:down,990:right,"
           "1050:down,1090:down,1130:down,1190:cross,1250:up,1310:cross,1370:select",
    "end": r"frame=\d+ page=4 state=0,0,1,3,0",
    "after":1370,"states":[(4,4),(4,12),(4,4),(4,1),(4,0)],
    "env":{"BT3_UI_SHOT":"1150:port/build/tag-menu-tests/fusion-settings/menu.ppm"},
}
CASES["stage-settings"] = {
    "pad": "150:select,180:down,210:down,240:down,270:down,300:down,330:down,350:cross,"+
           ",".join(f"{410+i*40}:down" for i in range(10))+
           ",830:cross,890:right,950:down,1010:cross,1070:triangle,1130:select",
    "end": r"frame=\d+ page=4 state=0,0,1,3,0",
    "after":1130,"states":[(4,4),(4,15),(4,4),(4,1),(4,0)],
    "env":{"BT3_UI_SHOT":"920:port/build/tag-menu-tests/stage-settings/menu.ppm"},
}
CASES["team-layout"] = {
    "pad": "150:select,210:cross,270:down,310:down,350:down,390:cross,"
           "450:cross,510:right,570:cross,630:down,690:cross,750:left,810:cross,870:start,1040:triangle",
    "end": r"frame=\d+ page=4 state=3,0,0,0,0",
    "after":1040,"states":[(4,3),(45,1),(40,0),(4,3)],
    "assignments":[(1,1,0,1,4),(1,0,0,1,4)],
}
CASES["cpu-swap"] = {
    "pad":"150:select,210:cross,270:cross,330:cross,390:right,450:cross,510:start,670:triangle",
    "end":r"frame=\d+ page=4 state=3,0,0,0,0",
    "after":670,"states":[(4,3),(45,1),(40,0),(4,3)],
    "assignments":[(1,0,0,0,1)],
}
CASES["unequal"] = dict(CASES["match"],pad=roster_pad(4,2),end=r"tagintro actor=5 .*state=1",roster=(4,2))
CASES["full-roster"] = dict(CASES["match"],pad=roster_pad(5,5),end=r"tagintro actor=9 .*state=1",roster=(5,5))
CASES["duplicates"] = dict(CASES["match"],pad=roster_pad(3,2,True),end=r"tagintro actor=4 .*state=1",roster=(3,2))
CASES["cpu-label"] = {
    "pad":"150:select,210:cross,270:down,310:down,350:down,390:down,650:cross",
    "end":r"frame=\d+ page=4 state=3,0,0,0,0","after":650,
    "states":[(4,2),(4,3)],"env":{"BT3_UI_SHOT":"500:port/build/tag-menu-tests/cpu-label/menu.ppm"},
}
CASES["setup-edit-cancel"] = {
    # Only the timer is shown here. Triangle cancels its edit; Cross saves.
    "pad":"150:select,210:cross,270:down,310:down,350:down,390:down,450:cross,"
          "510:cross,570:right,630:triangle,690:cross,750:right,810:cross,930:triangle",
    "end":r"frame=\d+ page=4 state=2,","after":930,
    "states":[(4,3),(4,2)],
    "env":{"BT3_UI_SHOT":"900:port/build/tag-menu-tests/setup-edit-cancel/menu.ppm"},
}
for mode, navigation in (("free-for-all", "180:down,"), ("training", "175:down,195:down,")):
    CASES[mode] = dict(CASES["match"], pad=CASES["match"]["pad"].replace("210:cross", navigation + "210:cross"))
for mode, navigation, mode_id in (("team", "", 0), ("free-for-all", "180:down,", 1), ("training", "175:down,195:down,", 2)):
    # Set the timer here, and each CPU's difficulty after their outfit.
    pad="150:select,"+navigation+"210:cross,270:down,310:down,350:down,390:down,450:cross,"
    pad+="510:cross,"+("550:right," if mode=="training" else "550:right,590:right,")
    pad+="630:cross,690:start"
    for event in roster_pad(cpu_only=True,cpu_steps=2).split(",")[4:]:
        tick,key=event.split(":");pad+=f",{int(tick)+330}:{key}"
    CASES["cpu-"+mode] = dict(CASES["match"],pad=pad,
        end=r"tagteam frame=150 fighter=3",cpu=True,mode_id=mode_id,cpu_steps=2,
        env={"BT3_TAG_DEBUG":"1","BT3_BGM_DEBUG":"1"})
CASES["pause"] = {
    "pad": "", "after": 0, "end": r"page=91 state=3,0,",
    "states": [(90, 2), (91, 2), (90, 3), (91, 3)],
    "env": {"BT3_DEMO": "1", "BT3_TAG_PLAY": "4", "BT3_TAG_ROSTER": "2,2", "BT3_TAG_INPUT_TRACE": "1",
            "BT3_UI_SHOT": "1100:port/build/tag-menu-tests/pause/menu.ppm",
            "BT3_PAD_3": "900:start,960:down,1020:cross,1140:triangle,1200:start",
            # The native pause menu remembers the selected Skill List row.
            "BT3_PAD_4": "1260:start,1380:cross,1500:triangle,1560:start"},
}
for destination, downs, page, state in (("characters", "", 40, 0), ("main", "3830:down,", 4, 1)):
    CASES["return-" + destination] = {
        # Wait for all four voiced entrances and Ready/Fight before pausing.
        "pad": CASES["match"]["pad"] + ",3700:start,3760:down,3800:down," + downs + "3860:cross,3920:up,3980:cross",
        "end": rf"frame=\d+ page={page} state={state},",
        "after": 3700,
        "states": CASES["match"]["states"] + [(90, 0), (page, state)],
    }


for loss in (False, True):
    for destination,downs,page,state in (
        ("retry","",92,4),("characters","4140:down,",40,0),
        ("main","4100:down,4140:down,",4,1)):
        CASES[("loss-" if loss else "win-")+destination]={
            "pad":roster_pad()+","+downs+"4200:cross,4300:up,4400:cross",
            "end":rf"frame=\d+ page={page} state={state},","after":4200,
            "states":CASES["match"]["states"]+[(90,0),(page,state)],
            "settings":"tag_extra_intros=0\n",
            "env":{"BT3_TAG_TEST_RESULT":"2" if loss else "1","BT3_UNCAPPED":"1"},
        }


def run(case, exe, renderer, data=None):
    spec = dict(CASES[case])
    if renderer=='none':
        raise ValueError('Scripted menu input requires --renderer gpu.')
    marker=Path(data)/'native-disc.json' if data else None
    if marker and marker.exists() and json.loads(marker.read_text()).get('game')=='bt4':
        sizes=spec.get('roster',(2,2))
        spec['pad']=adapt_bt4_roster(spec['pad'],sizes,case=='duplicates',spec.get('cpu',False),spec.get('cpu_steps',0))
    output = ROOT / "port/build/tag-menu-tests" / ((Path(data).name+"-" if data else "")+case)
    output.mkdir(parents=True, exist_ok=True)
    settings = output / "settings.txt"
    # These cases exercise menu routing with scripted logical pads. Physical
    # controller check-in has its own SDL integration fixture.
    settings.write_text("tag_revive_seconds=3\ntag_controller_check=0\n"+spec.get("settings",""), encoding="utf-8")
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS=renderer, BT3_TAG_TEAM="1", BT3_TAG_MENU_TEST="main",
               BT3_MENU_LANG="en", BT3_UPDATE_CHECK="0", BT3_TAG_TRACE="1",
               BT3_LOAD_TRACE="1", BT3_SAVES=str(output / "saves"),
               BT3_SETTINGS=str(settings), BT3_MENU_ITEM4="0", BT3_PAD=spec["pad"],
               BT3_PACED="1", SDL_AUDIO_DRIVER="dummy")
    env.update(spec.get("env", {}))
    if data: env['BT3_DATA']=str(Path(data).resolve())
    if data: env.pop('BT3_PACED',None)
    if env.get("BT3_UNCAPPED") == "1":env.pop("BT3_PACED",None)
    if renderer=="none":env.pop("BT3_PACED",None)
    path = output / "runtime.log"
    with path.open("w", encoding="utf-8") as log:
        p = subprocess.Popen([str(exe)], cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 120
        try:
            while time.monotonic() < deadline:
                text = path.read_text(encoding="utf-8", errors="replace")
                assert "crashed:" not in text, f"Native crash: {path}"
                matches = list(re.finditer(spec["end"], text))
                if any(spec["after"] == 0 or int(re.search(r"frame=(\d+)", m[0])[1]) > spec["after"] for m in matches):
                    break
                assert p.poll() is None, f"Game exited before completion: {path}"
                time.sleep(0.1)
            else:
                raise AssertionError(f"Timed out: {path}")
        finally:
            if p.poll() is None:
                p.terminate()
            p.wait(timeout=10)
    transitions = [(int(a), int(b)) for a, b in re.findall(r"tagmenu frame=\d+ page=(\d+) state=(\d+),", text)]
    remaining = iter(transitions)
    for expected in spec["states"]:
        assert expected in remaining, f"Missing ordered transition {expected}: {path}"
    assert "page=80" not in text, "Standalone setup page was entered"
    if "assignments" in spec:
        actual=[tuple(map(int,m.split(","))) for m in re.findall(r"page=45 state=([\d,]+)",text)]
        assert actual==spec["assignments"],actual
    if spec.get("cpu"):
        # BT4's highest selectable difficulty is Z Hard, level 99.
        level=99 if marker and marker.exists() and json.loads(marker.read_text()).get('game')=='bt4' else 29
        assert f"page=46 state=1,{level},0,{spec['mode_id']},{level}" in text, "Chosen timer/per-fighter difficulty did not reach the loaded battle"
        assert not re.search(r"page=47 state=1,",text), "Hidden global CPU difficulty was edited"
        if marker and marker.exists() and json.loads(marker.read_text()).get('game')=='bt4':
            playlists=[list(map(int,line.split())) for line in re.findall(r"bt4 music list:([^\r\n]+)",text)]
            assert playlists and all(len(ids)==len(set(ids)) and ids.count(0)==1 and ids.count(24)==1 for ids in playlists),playlists
        controls=re.findall(r"tagteam control fighter=(\d+) control=(\d+) inject=(\d+)",text)
        assert controls and all(control=="2" and inject=="1" for _,control,inject in controls),controls
    if "rosters" in spec:
        rosters = [tuple(map(int, m.split(","))) for m in re.findall(r"page=45 state=([\d,]+)", text)]
        assert rosters == spec["rosters"], rosters
        for a, b, left, right, humans in rosters:
            players = [(packed >> (slot * 4)) & 15 for count, packed in ((a, left), (b, right)) for slot in range(count)]
            assert sorted(p for p in players if p) == list(range(1, humans + 1)), players
    if case == "settings":
        assert "tag_revive_seconds=4\n" in settings.read_text(), "Setting did not persist"
    if case == "revival-ring":
        assert "tag_revive_wave_height=9\n" in settings.read_text(), "Wave height did not persist"
        assert "tag_revive_wave_speed=125\n" in settings.read_text(), "Wave speed did not persist"
    if case=="setup-edit-cancel":
        changes=[tuple(map(int,m.split(","))) for m in re.findall(r"page=47 state=([-\d,]+)",text)]
        assert changes==[(0,1,4,2,0),(0,-1,3,2,0),(0,1,4,2,0)],changes
    if case == "camera-views":
        assert "tag_three_wide=0\n" in settings.read_text(), "Three-player layout did not persist"
    if case == "fusion-settings":
        assert "tag_fusion_swap_seconds=25\n" in settings.read_text(), "Fusion handoff interval did not persist"
    if case == "stage-settings":
        assert "tag_map_size=125\n" in settings.read_text(), "Expanded map size did not persist"
    if case == "pause":
        assert {int(p) for p in re.findall(r"page=93 state=(\d+),", text)} == {2, 3}, "Controller scripts leaked between players"
        for pad in (2, 3):
            assert re.search(rf"page=90 state={pad},0,2,2,{pad}", text), f"P{pad+1} skill list did not open"
    if case in ("match", "free-for-all", "training", "unequal", "full-roster", "duplicates"):
        roster=spec.get("roster",(2,2)); actors=set(range(sum(roster)))
        assert {int(n) for n in re.findall(r"tagteam frame=150 fighter=(\d+)", text)} == actors
        assert {int(n) for n in re.findall(r"tagintro actor=(\d+)", text)} == actors
        assert {int(n) for n in re.findall(r"tagload batch=(\d+)", text)} == {1}
        actual_charas={int(a):int(c) for a,c in re.findall(r"tagteam frame=150 fighter=(\d+) chara=(\d+)",text)}
        intro_charas={int(a):int(c) for a,c in re.findall(r"tagintro actor=(\d+) chara=(\d+)",text)}
        assert intro_charas==actual_charas,(intro_charas,actual_charas)
        if case=="duplicates":
            assert len({actual_charas[i] for i in (0,2,4)})==1,actual_charas
        teams = {int(actor): int(team) for actor, team in re.findall(r"tagteam frame=150 fighter=(\d+) chara=\d+ team=(\d+)", text)}
        expected=[t for m in range(5) for t in range(2) if m<roster[t]]
        assert teams == ({i:i for i in actors} if case == "free-for-all" else dict(enumerate(expected))), teams
    return {"case": case, "passed": True, "log": str(path.relative_to(ROOT))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default="port/build/bt3.exe")
    parser.add_argument("--renderer", choices=("none", "gpu"), default="gpu")
    parser.add_argument("--case", choices=tuple(CASES), action="append")
    parser.add_argument("--data", type=Path)
    args = parser.parse_args()
    results = []
    for case in args.case or CASES:
        result = run(case, (ROOT / args.exe).resolve(), args.renderer,args.data)
        results.append(result)
        print(json.dumps(result), flush=True)
    (ROOT / "port/build/tag-menu-tests/results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
