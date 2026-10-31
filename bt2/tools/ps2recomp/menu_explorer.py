"""Menu explorer (WHOLE_GAME_COVERAGE_PLAN.md, step 6): the main menu's screens beyond their first page.

Starts a screen with BT2_START_SCREEN=<mode> and presses a path of buttons with a generated pad script, one
button every --step fields from --first-field on, then lets the run go on --settle flips. A path is
extended by each button of --buttons when its run entered functions no earlier run of that mode entered;
a path ending in a cursor move or cross is also extended without that, up to --idle-moves of them in a row,
since a move may only prepare the next selection and a screen may take several confirmations. Paths run --jobs at a time, breadth first, up to --runs per mode.

Findings are the runs that stopped before their frame budget (a stop, a stall) and the code they
interpreted. Results go to <out>/<mode>/summary.tsv (path, result, new functions, entered, interpreted,
stop) and <out>/<mode>/entered.txt (every function the mode's runs entered); pictures of a run that
stopped are kept. The game's data and the paths' pictures stay in ignored work/.
"""
import argparse,concurrent.futures,json,sys,threading
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import battle_sweep
ROOT=battle_sweep.ROOT
CROSSES=12 # Default of --crosses: crosses in a row a path may take without new code, for message chains.
MOVES={'up','down','left','right','cross'} # Buttons a path may repeat without new code: cursor moves, and cross (messages, loads).


def pad_script(path,first,step):
    """A pad script pressing each button of the path for 8 fields, one every step fields."""
    return ','.join(f'{first+i*step}:{button},{first+i*step+8}:-' for i,button in enumerate(path))


def extensions(path,new,buttons,idle_moves,crosses_allowed=CROSSES):
    """The paths to try after this one: every button added when it found new functions, or when it ends in
    fewer than idle_moves cursor moves or crosses in a row; after that, cross alone up to crosses_allowed in a row
    (a chain of messages, each taking a cross without new code)."""
    trailing=0
    for button in reversed(path):
        if button not in MOVES:break
        trailing+=1
    if new or (path and path[-1] in MOVES and trailing<idle_moves) or not path:return [path+[b] for b in buttons]
    crosses=len(path)-len(''.join('x' if b=='cross' else '-' for b in path).rstrip('x'))
    if crosses and crosses<crosses_allowed and 'cross' in buttons:return [path+['cross']]
    return []


def explore(mode,run,buttons,runs,idle_moves,jobs,record,crosses_allowed=CROSSES):
    """Breadth first over paths; run(path) returns (row, entered set). record(path,row,new) is called per run."""
    seen=set();frontier=[[]];done=0;lock=threading.Lock()
    while frontier and done<runs:
        batch=frontier[:max(0,min(len(frontier),runs-done))];frontier=frontier[len(batch):]
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            results=list(pool.map(run,batch))
        for path,(row,entered) in zip(batch,results):
            with lock:
                new=len(entered-seen);seen|=entered;done+=1
            record(path,row,new)
            if row['result']=='ok':frontier+=extensions(path,new,buttons,idle_moves,crosses_allowed)
    return seen,done


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--modes',default='10,24,61,53,5,38,48,43',help='BT2_START_SCREEN modes (default: %(default)s).')
    p.add_argument('--buttons',default='cross,down,right,triangle,circle',help='Buttons a path is extended by (default: %(default)s).')
    p.add_argument('--runs',type=int,default=120,help='Runs per mode (default: %(default)s).')
    p.add_argument('--idle-moves',type=int,default=3)
    p.add_argument('--crosses',type=int,default=CROSSES,help='Crosses in a row a path may take without new code (default: %(default)s; long message chains need more).')
    p.add_argument('--first-field',type=int,default=800,help='Field of the first press (the screens are up by about field 730).')
    p.add_argument('--step',type=int,default=60,help='Fields between presses (default: %(default)s).')
    p.add_argument('--settle',type=int,default=400,help='Flips after the last press (default: %(default)s).')
    p.add_argument('--jobs',type=int,default=6)
    p.add_argument('--out',type=Path,default=ROOT/'work/coverage/menus')
    a=p.parse_args()
    buttons=a.buttons.split(',')
    for mode in battle_sweep.numbers(a.modes):
        out=a.out/str(mode);out.mkdir(parents=True,exist_ok=True)
        rows=[];lock=threading.Lock()
        def run(path):
            found={}
            def passed(report):
                found['entered']={int(x,16) if isinstance(x,str) else x for x in report.get('ee_entered_addresses',[])}
                return bool(report.get('frame_budget_reached'))
            label='-'.join(path) or 'none'
            frames=a.first_field-430+len(path)*a.step+a.settle # Fields run about 430 ahead of flips on these screens.
            row,interpreted=battle_sweep.run_case(str(mode),a.disc.resolve(),out,frames,600,(),'BT2_START_SCREEN',label[:120],passed,
                                                  pad_script(path,a.first_field,a.step))
            row['interpreted_entries']=interpreted
            return row,found.get('entered',set())
        def record(path,row,new):
            with lock:
                rows.append((path,row,new))
                print(f'mode {mode} [{len(rows)}] {"-".join(path) or "none"}: {row["result"]} new {new} entered {row.get("entered","-")} '
                      f'interpreted {row.get("interpreted","-")} {row.get("stop","")}',flush=True)
        seen,done=explore(mode,run,buttons,a.runs,a.idle_moves,a.jobs,record,a.crosses)
        (out/'entered.txt').write_text(''.join(f'{x:08X}\n' for x in sorted(seen)))
        with (out/'summary.tsv').open('w',encoding='utf-8') as f:
            f.write('path\tresult\tnew\tentered\tinterpreted\tstop\n')
            for path,row,new in rows:f.write(f'{"-".join(path) or "none"}\t{row["result"]}\t{new}\t{row.get("entered","")}\t{row.get("interpreted","")}\t{row.get("stop","")}\n')
        stops=[r for _,r,_ in rows if r['result']!='ok'];interpreted={x for _,r,_ in rows for x in r.get('interpreted_entries',[])}
        print(f'mode {mode}: {done} runs, {len(seen)} functions entered, {len(stops)} stopped, {len(interpreted)} interpreted entries',flush=True)


if __name__=='__main__':main()
