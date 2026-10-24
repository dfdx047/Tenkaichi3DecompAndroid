#!/usr/bin/env python3
"""Exercise the actual native battle loader, CPU fight, timeout and cleanup.

Requires the user's extracted USA disc data and a built executable. No emulator,
mock game memory, or savestate is used. Run from any directory, for example:
    python port/tests/tag_team_runtime.py --exe port/build/bt3.exe
"""
import argparse
import collections
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
TRACE = re.compile(r"tagteam frame=(\d+) fighter=(\d+) chara=(\d+) team=(\d+) target=(\d+) hp=(-?\d+) action=(\d+)")


def run_case(exe, output, roster):
    name = "baseline-1v1" if roster is None else "v".join(map(str, roster))
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"))
    if roster:
        env.update(BT3_TAG_TEAM="1", BT3_TAG_TRACE="1", BT3_TAG_ROSTER=",".join(map(str, roster)))
    start = time.monotonic()
    log = output / (name + ".log")
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0, f"{name}: exit {result.returncode}; see {log}"
    assert "bt3: battle finished" in text, f"{name}: no completed battle"
    report = {"case": name, "seconds": round(time.monotonic() - start, 2), "log": str(log.relative_to(ROOT))}
    if roster:
        rows = [tuple(map(int, row)) for row in TRACE.findall(text)]
        by_actor = collections.defaultdict(list)
        for row in rows:
            by_actor[row[1]].append(row)
        assert set(by_actor) == set(range(sum(roster))), f"{name}: incomplete roster"
        teams = {actor: values[0][3] for actor, values in by_actor.items()}
        assert collections.Counter(teams.values()) == {0: roster[0], 1: roster[1]}
        for actor, values in by_actor.items():
            assert all(teams[row[4]] != teams[actor] for row in values), f"{name}: friendly target"
            assert len({row[6] for row in values}) > 1, f"{name}: actor {actor} never changes actions"
        assert any(min(row[5] for row in values) < values[0][5] for values in by_actor.values()), f"{name}: no damage"
        report["fighters"] = [{"actor": actor, "team": teams[actor], "chara": values[0][2],
                               "last_hp": values[-1][5], "different_actions": len({row[6] for row in values})}
                              for actor, values in sorted(by_actor.items())]
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default="port/build/bt3.exe" if os.name == "nt" else "port/build/bt3_64")
    parser.add_argument("--audio-only", action="store_true", help="Only run the real SDL mixer check")
    parser.add_argument("--clash-only", action="store_true", help="Only run native clash participants and interruption checks")
    parser.add_argument("--controls-only", action="store_true", help="Run native lock-off, teammate targeting and takeover checks")
    parser.add_argument("--beams-only", action="store_true", help="Fire real beams and check their collision and recovery")
    parser.add_argument("--assist-only", action="store_true", help="Exercise beam assists with their default settings")
    parser.add_argument("--giants-only", action="store_true", help="Exercise real giant models and collision volumes")
    parser.add_argument("--cameras-only", action="store_true", help="Exercise native grab cameras and recovery")
    parser.add_argument("--checkin-only", action="store_true", help="Exercise four real SDL virtual controllers and packet routing")
    parser.add_argument("--fusion-only", action="store_true", help="Exercise teammate consent, native fusion and timed separation")
    parser.add_argument("--cpu-only", action="store_true", help="Exercise target commitment, retaliation and cinematic guards")
    args = parser.parse_args()
    exe = Path(args.exe)
    if not exe.is_absolute():
        exe = ROOT / exe
    platform = "windows" if exe.suffix.lower() == ".exe" else "linux"
    output = ROOT / "port/build/tagteam-runtime-tests" / platform
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    if args.cpu_only:
        run_cpu(exe, output)
        return
    if args.giants_only:
        run_giants(exe, output)
        return
    if args.assist_only:
        run_assist(exe, output)
        return
    if args.fusion_only:
        run_fusion(exe, output)
        return
    if args.cameras_only:
        run_cameras(exe, output)
        return
    if args.checkin_only:
        run_checkin(exe, output)
        return
    if args.beams_only:
        run_beams(exe, output)
        return
    if args.controls_only:
        run_controls(exe, output)
        return
    if args.clash_only:
        run_clash(exe, output)
        return
    if args.audio_only:
        run_audio(exe, output)
        return
    for roster in [None, (2, 2), (1, 2), (2, 1), (1, 3), (3, 1), (4, 2), (2, 4), (5, 5), (5, 1), (1, 5)]:
        report = run_case(exe, output, roster)
        reports.append(report)
        print(f"PASS {report['case']} ({report['seconds']}s)", flush=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_ROSTER="2,2", BT3_TAG_TEST_REVIVE="1")
    log = output / "revival.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Revival failed; see {log}"
    reports.append({"case": "revival", "log": str(log.relative_to(ROOT))})
    print("PASS revival, interruption, blocking, movement and ring-out", flush=True)
    reports.append(run_audio(exe, output))
    reports.append(run_clash(exe, output))
    reports.append(run_beams(exe, output))
    reports.append(run_controls(exe, output))
    reports.append(run_cameras(exe, output))
    reports.append(run_fusion(exe, output))
    reports.append(run_cpu(exe, output))
    (output / "results.json").write_text(json.dumps(reports, indent=2) + "\n", encoding="utf-8")


def run_cpu(exe, output):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="1", BT3_TAG_ROSTER="2,2",
               BT3_TAG_TEST_CPU="1", BT3_TAG_CPU_TRACE="1")
    log = output / "cpu-targeting.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"CPU targeting failed; see {log}"
    print("PASS CPU target commitment, damage retaliation, pause, queued damage, KO, cinematic and rematch guards", flush=True)
    return {"case": "cpu-targeting", "log": str(log.relative_to(ROOT))}


def run_fusion(exe, output):
    cases=[(4,0,2),(4,0,1),(4,1,3),(4,2,3),(4,0,3),(4,1,2),
           (4,3,1),(2,1,0),(3,0,1),(3,0,2),(3,1,2)]
    for views,a,b in cases:
        run_fusion_case(exe,output,views,a,b)
    run_fusion_case(exe,output,2,0,1,result=51)
    return {"case": "fusion", "layouts": len(cases), "potara": True}


def run_fusion_case(exe, output, views, a, b, result=53):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    settings = output / "fusion-settings.txt"
    settings.write_text("tag_extra_intros=0\n", encoding="utf-8")
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS=str(settings), BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_VIEWS=str(views), BT3_TAG_ROSTER="2,2",
               BT3_TAG_FUSION_SEATS=f"{a},{b}",
               BT3_TAG_FUSION_RESULT=str(result),
               BT3_TAG_TEST_FUSION="1", BT3_TAG_TRACE="1")
    log = output / f"fusion-{views}views-{a+1}-{b+1}-form{result}.log"
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert process.returncode == 0 and "PASS check=99" in text, f"Fusion checks failed; see {log}"
    print(f"PASS {views} views P{a+1}+P{b+1}: consent, fusion layout, control switching, " +
          ("permanent Potara and switch setting" if result==51 else "defusion, placement and view recovery"), flush=True)
    return {"case": "fusion", "log": str(log.relative_to(ROOT))}


def run_cameras(exe, output):
    for backstep in (False, True):
        run_camera_case(exe, output, backstep)
    return {"case": "cameras", "backstep": True}


def run_camera_case(exe, output, backstep):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    settings = output / "camera-settings.txt"
    settings.write_text("", encoding="utf-8")
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS=str(settings), BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_ROSTER="2,2", BT3_TAG_TEST_CAMERAS="2" if backstep else "1")
    log = output / ("cameras-backstep.log" if backstep else "cameras.log")
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Camera checks failed; see {log}"
    print(f"PASS native grab {'during backstep' if backstep else 'from standing'}, actual camera owner, shared-camera setting and recovery", flush=True)
    return {"case": "cameras", "log": str(log.relative_to(ROOT))}


def run_checkin(exe, output):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="gpu", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_MENU_TEST="main", BT3_TAG_CHECKIN_TEST="1", BT3_UPDATE_CHECK="0")
    log = output / "checkin.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=60)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Controller check-in failed; see {log}"
    print("PASS shuffled joins, duplicate prevention, leave/rejoin, buttons, sticks, disconnect and cancel", flush=True)
    return {"case": "checkin", "log": str(log.relative_to(ROOT))}


def run_controls(exe, output):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="1", BT3_TAG_ROSTER="2,2", BT3_TAG_TEST_CONTROLS="1")
    log = output / "controls.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Controls failed; see {log}"
    env.update(BT3_TAG_PLAY="0",BT3_TAG_TEST_CONTROLS="2")
    spectator_log=output / "spectator-controls.log"
    with spectator_log.open("w",encoding="utf-8") as stream:
        result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    assert result.returncode==0 and "PASS check=99" in spectator_log.read_text(encoding="utf-8",errors="replace"), \
        f"CPU spectator controls failed; see {spectator_log}"
    print("PASS lock-off, downed targets, camera-centred relock, CPU takeover and actual clash contact", flush=True)
    return {"case": "controls", "log": str(log.relative_to(ROOT))}


def run_clash(exe, output):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_ROSTER="2,2", BT3_TAG_TEST_CLASH="1")
    log = output / "clash.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Clash failed; see {log}"
    print("PASS independent clash pairs, vanish exchanges, struggle actions and interruption", flush=True)
    return {"case": "clashes", "log": str(log.relative_to(ROOT))}


def run_beams(exe, output):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_SOUND="1", SDL_AUDIO_DRIVER="dummy", BT3_PACED="1", BT3_DEMO="1",
               BT3_SETTINGS="", BT3_SAVES=str(output / "saves"), BT3_TAG_TEAM="1", BT3_TAG_PLAY="4",
               BT3_TAG_ROSTER="2,2", BT3_TAG_TEST_BEAMS="1", BT3_TAG_TRACE="1", BT3_TAG_BEAM_TRACE="1")
    log = output / "real-beams.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Real beam collision failed; see {log}"
    assert all(f"PASS check={n}" in text for n in range(82,87)), f"Incomplete beam checks; see {log}"
    # The native technique timer must stay stopped for the actual participants.
    # A live clash can otherwise outlast its beams while still passing recovery.
    for actor in (2, 3):
        sustained = re.findall(rf"beam actor={actor} phase=2 flags=70 expire=0 ", text)
        assert len(sustained) >= 6, f"Actor {actor}'s beam expired during the struggle; see {log}"
    intros = re.findall(r"tagintro actor=(\d+) chara=\d+ line=\d+ state=(\d+)", text)
    assert sorted((int(a),int(s)) for a,s in intros) == [(a,s) for a in range(4) for s in (0,1)], \
        f"Missing, repeated or timed-out intro: {intros}"
    print("PASS real beam emission, collision, struggle, recovery and four voiced entrances", flush=True)
    env["BT3_TAG_TEST_MULTI_BEAMS"]="1"
    log = output / "simultaneous-beams.log"
    with log.open("w",encoding="utf-8") as stream:
        result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    text=log.read_text(encoding="utf-8",errors="replace")
    assert result.returncode==0 and all(f"PASS check={n}" in text for n in (88,89,90,99)), \
        f"Independent beam struggles failed; see {log}"
    print("PASS two simultaneous beam struggles and independent endings",flush=True)
    run_assist(exe, output)
    return {"case": "real-beams-and-intros", "log": str(log.relative_to(ROOT))}


def run_assist(exe, output):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_ROSTER="2,2",
               BT3_TAG_TEST_BEAMS="1", BT3_TAG_TEST_BEAM_ASSIST="1", BT3_TAG_TRACE="1")
    log=output / "beam-assist.log"
    with log.open("w",encoding="utf-8") as stream:
        result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    text=log.read_text(encoding="utf-8",errors="replace")
    assert result.returncode==0 and all(f"PASS check={n}" in text for n in (*range(92,98),99)), \
        f"Beam assist failed; see {log}"
    print("PASS teammate assist input, native pose, stock cost, power and control recovery",flush=True)
    return {"case": "beam-assist", "log": str(log.relative_to(ROOT))}


def run_giants(exe, output):
    # Every native giant body: apes, giant Namekians and Hirudegarn.
    for chara in (12,30,72,74,76,78,81,122,124,139,143):
        env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
        env.update(BT3_GS="none", BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
                   BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_ROSTER="2,2",
                   BT3_CHARA=str(chara), BT3_TAG_TEST_GIANTS="1", BT3_TAG_TRACE="1")
        log=output / f"giants-{chara}.log"
        with log.open("w",encoding="utf-8") as stream:
            result=subprocess.run([str(exe)],cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        text=log.read_text(encoding="utf-8",errors="replace")
        assert result.returncode==0 and "PASS check=99" in text, f"Giant checks failed; see {log}"
        print(f"PASS giant {chara}: scale, collision, motion, damage, armor, camera and disabling",flush=True)


def run_audio(exe, output, roster="5,5"):
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_SOUND="1", SDL_AUDIO_DRIVER="dummy", BT3_SND_VERBOSE="1",
               BT3_DEMO="1", BT3_SETTINGS="", BT3_SAVES=str(output / "saves"),
               BT3_TAG_TEAM="1", BT3_TAG_PLAY="4", BT3_TAG_ROSTER=roster, BT3_TAG_TEST_AUDIO="1")
    log = output / "audio.log"
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(exe)], cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=120)
    text = log.read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0 and "PASS check=99" in text, f"Audio failed; see {log}"
    assert "sound: dummy" in text, "No SDL audio device was exercised"
    print(f"PASS {sum(map(int,roster.split(',')))} independent voices, sample banks, streamed effects, pause and cleanup", flush=True)
    return {"case": "audio", "log": str(log.relative_to(ROOT))}


if __name__ == "__main__":
    main()
