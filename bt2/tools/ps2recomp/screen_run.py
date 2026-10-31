"""Screen runs (WHOLE_GAME_COVERAGE_PLAN.md, step 6): the main menu's modes through their battles, without input.

Starts each screen with BT2_START_SCREEN=<mode> and a generated pad script: cross every --cross-every fields
from field 800 (menus, messages, loads, a battle's results) and, with --down-every, down held for 60 fields
that often (it stops a cross press due then). The screen starter gives the battles of Ultimate Battle Z and
the Dragon Tournament to the CPU on both sides, takes Ultimate Battle Z's courses in turn (side 1 with one
health bar) and answers the save flow's "Exit saving?" with Yes (no run saves); in Ultimate Training (mode 61) side 0 stays with the pad, the
starter takes the 21 lessons in turn, and a lesson's practice is cleared by the game's own task report (the
pad's down reaches Movement; the starter reports the lesson's other tasks after a while) and ended as the
pause menu's Exit training does. Each run prints its result and the battles the starter recorded (kind,
rule, the field the battle started, the lesson in Training, the result flags); pictures every --pictures
flips go to <out>/mode<mode>-<flip>.png. Runs go --jobs at a time (each about one CPU core).

    python tools/ps2recomp/screen_run.py --disc <iso> --modes 61 --flips 90000 --down-every 400
"""
import argparse,concurrent.futures,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import battle_sweep
ROOT=battle_sweep.ROOT
FIRST_FIELD=800 # The screens are up by about field 730.


def pad_script(fields,cross_every,down_every=0):
    """Cross for 8 fields every cross_every fields from FIRST_FIELD; with down_every, down for 60 fields that
    often from field 1000, replacing the cross steps it overlaps."""
    steps={}
    for f in range(FIRST_FIELD,fields,cross_every):steps[f]='cross';steps[f+8]='-'
    if down_every:
        for f in range(1000,fields,down_every):
            for g in range(f,f+60):steps.pop(g,None)
            steps[f]='down';steps[f+60]='-'
    return ','.join(f'{f}:{steps[f]}' for f in sorted(steps))


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--modes',default='10,24,61',help='BT2_START_SCREEN modes (default: %(default)s: Ultimate Battle Z, Dragon Tournament, Ultimate Training).')
    p.add_argument('--flips',type=int,default=30000,help='Frame budget per run (default: %(default)s).')
    p.add_argument('--cross-every',type=int,default=120)
    p.add_argument('--down-every',type=int,default=0,help='Down presses for lessons (Training: 400).')
    p.add_argument('--pictures',type=int,default=2000,help='Flips between pictures (0: none).')
    p.add_argument('--jobs',type=int,default=3)
    p.add_argument('--out',type=Path,default=ROOT/'work/coverage/modes')
    a=p.parse_args()
    a.out.mkdir(parents=True,exist_ok=True)
    disc=a.disc.resolve()
    # Fields run ahead of flips (two per flip on 30 frame-per-second screens), so the script covers twice the flips.
    script=pad_script(a.flips*2+2000,a.cross_every,a.down_every)
    pictures=tuple(range(a.pictures,a.flips+1,a.pictures)) if a.pictures else ()
    def one(mode):
        found={}
        def passed(report):
            found['battles']=report.get('starter_mode_battles');return bool(report.get('frame_budget_reached'))
        row,interpreted=battle_sweep.run_case(mode,disc,a.out,a.flips,1200,pictures,'BT2_START_SCREEN',f'mode{mode}',passed,script)
        return mode,row,interpreted,found.get('battles')
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
        for mode,row,interpreted,battles in pool.map(one,a.modes.split(',')):
            print(f"mode {mode}: {row['result']}, {row.get('flips')} flips, {row.get('entered')} functions entered, "
                  f"{len(interpreted)} interpreted {row.get('stop','')}".rstrip(),flush=True)
            print('  battles',json.dumps(battles),flush=True)


if __name__=='__main__':
    main()
