"""The gate's battle run: the Release probe plays the scripted fight on the GPU renderer and its
report is checked against the recorded baseline.

The run goes through the films, the title, the menus, character and map selection into a 1P-vs-COM
fight (pad-fight.txt beside this file: button presses by field, written for this project). It runs
with BT2_VU1_VERIFY, so every native VU1 program is also compared with the interpreter. Checked:
the frame budget is reached, the VU1 output hash, pair and start counts equal the baseline, every
start with a native program was verified, and at least as many EE functions were entered as the
baseline run entered. A change that alters the game's 3D output on purpose updates BASELINE here.
"""
import argparse, json, os, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/ps2recomp'))
FRAMES = 5100
BASELINE = {'vu1_kick_hash': '5a4960667d4a3dff', 'vu1_instructions': 724698589, 'vu1_runs': 2140195}
ENTERED_AT_LEAST = 3700 # 3,731 at the baseline; the exact count moves when the map splits functions.


def judge(report):
    """The reasons the run fails the gate; empty when it passes."""
    problems = []
    if not report.get('frame_budget_reached') or report.get('gs_display_flips') != FRAMES:
        problems.append(f"stopped at frame {report.get('gs_display_flips')} of {FRAMES} ({report.get('status')}; pc {report.get('pc', 0):#x}, target {report.get('target', 0):#x})")
    for key, expected in BASELINE.items():
        if report.get(key) != expected:
            problems.append(f'{key} is {report.get(key)}, baseline {expected}')
    if report.get('vu1_native_runs') and report.get('vu1_verified_runs') != report.get('vu1_native_runs'):
        problems.append(f"{report.get('vu1_verified_runs')} of {report.get('vu1_native_runs')} native VU1 starts verified")
    if report.get('distinct_ee_entered', 0) < ENTERED_AT_LEAST:
        problems.append(f"{report.get('distinct_ee_entered')} EE functions entered, at least {ENTERED_AT_LEAST} expected")
    # The interpreter only bridges code the map lacks; the gate's runs must run everything compiled.
    if report.get('ee_interpreted_instructions', 0):
        problems.append(f"{report['ee_interpreted_instructions']} instructions of unmapped code interpreted "
                        f"(entries {[hex(e['entry']) for e in report.get('ee_interpreted_entries', [])][:8]}; map them with discovery_loop.py)")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--report', type=Path, default=ROOT / 'work/bt2-generation/gate-battle.json')
    args = parser.parse_args()
    import run_startup_probe
    if not run_startup_probe.gpu_build('Release'):
        print('Battle run skipped: the Release build has no GPU renderer, and only it draws the 3D frames.')
        return 0
    script = (Path(__file__).with_name('pad-fight.txt')).read_text(encoding='utf-8').strip()
    env = dict(os.environ, BT2_VU1_VERIFY='1')
    for name in ('BT2_CARD_FOLDER', 'BT2_PLAYER_SETTINGS', 'BT2_VU1_INTERPRET', 'BT2_VU1_NO_HAND_WRITTEN', 'BT2_GPU_GS_SNAPSHOT'):
        env.pop(name, None)
    done = subprocess.run([sys.executable, str(ROOT / 'tools/ps2recomp/run_startup_probe.py'), '--disc', str(args.disc), '--config', 'Release',
                           '--frames', str(FRAMES), '--pad-script', script, '--report', str(args.report)],
                          cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
    if not args.report.exists():
        print((done.stdout + done.stderr).strip()[-600:]); print('Battle run failed: no report.')
        return 1
    report = json.loads(args.report.read_text(encoding='utf-8'))
    problems = judge(report)
    for line in (done.stdout + done.stderr).splitlines():
        if problems and ('differs from the interpreter' in line or 'Branch target' in line or 'stopped:' in line or 'failed' in line):
            print(line[:300])
    print('Battle run: ' + ('; '.join(problems) if problems else
          f"{FRAMES} frames, {report['distinct_ee_entered']} EE functions entered, {report['vu1_runs']} VU1 starts ({report['vu1_verified_runs']} verified against the interpreter), hash {report['vu1_kick_hash']}"))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
