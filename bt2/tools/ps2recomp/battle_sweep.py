"""CPU-against-CPU battle sweep (WHOLE_GAME_COVERAGE_PLAN.md, step 6).

Runs the Release probe once per case with BT2_START_BATTLE (a battle without input), one run at a time,
and records each case in <out>/summary.tsv: whether it reached its frame budget, the flips, the functions
entered, the code the interpreter ran and the stop. A case already in the summary is skipped, so a sweep
can be resumed; --again reruns them. Each case's picture at a chosen flip is kept for a look. A stopped case keeps its report and log as the reproducer (the
switch is the case's BT2_START_BATTLE text); the memory dumps of passing cases are deleted. Interpreted
entries are collected in <out>/interpreted.txt for the discovery loop. Everything stays in ignored work/.
"""
import argparse,json,os,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).resolve().parent))
import run_startup_probe

FIELDS=('case','result','flips','entered','interpreted','seconds','stop')
DUMPS=('.ee.bin','.iop.bin')


def numbers(text):
    """'0-3,7,9-10' -> [0, 1, 2, 3, 7, 9, 10]."""
    values=[]
    for part in filter(None,text.split(',')):
        low,_,high=part.partition('-')
        values.extend(range(int(low),int(high or low)+1))
    return values


def cases(characters,variants,opponent,stages):
    """BT2_START_BATTLE texts: every character and variant against the opponent (default: the next
    character), the stages taken in turn."""
    result=[]
    for n,(character,variant) in enumerate((c,v) for c in characters for v in variants):
        other=opponent if opponent is not None else character+1
        result.append(f'{character}:{variant},{other},{stages[n%len(stages)]}')
    return result


def name(case):
    first,second,stage=case.split(',')[:3]
    return 'c'+first.replace(':','v')+'-c'+second.replace(':','v')+'-s'+stage


def stop_line(lines):
    """The line that names why a run stopped, if any."""
    keys=('budget exceeded','Unsupported','Branch target','Syscall','stopped','failed','Unexpected','refused','error')
    found=[l for l in lines if any(k in l for k in keys) and not l.startswith('[')]
    return found[-1][:240] if found else ''


def to_png(ppm):
    """The picture as PNG beside it (kept as PPM when Pillow is missing)."""
    try:from PIL import Image
    except ImportError:return
    Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()


def read_summary(path):
    if not path.exists():return {}
    rows=[l.split('\t') for l in path.read_text(encoding='utf-8').splitlines()[1:] if l]
    return {r[0]:dict(zip(FIELDS,r)) for r in rows}


def write_summary(path,rows):
    lines=['\t'.join(FIELDS)]+['\t'.join(str(rows[k].get(f,'')) for f in FIELDS) for k in sorted(rows)]
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8')


def run_case(case,disc,out,frames,stall,pictures=(),switch='BT2_START_BATTLE',label=None,passed=None,pad_script=''):
    """One run with <switch>=<case> (and a pad script, default none); passed(report) decides 'ok' (default:
    the frame budget reached)."""
    label=label or name(case)
    report=out/(label+'.json');log=out/(label+'.log')
    cmd=run_startup_probe.command(ROOT,disc,'Release');cmd[2]=str(report)
    env=dict(os.environ)
    for key in ('BT2_VIEWER','BT2_AUDIO_DEVICE','BT2_HOST_INPUT','BT2_PAD_RECORD','BT2_PAD_SCRIPT','BT2_PLAYER_SETTINGS',
                'BT2_START_BATTLE','BT2_START_SCREEN','BT2_START_EPISODE'):env.pop(key,None)
    env.update({switch:case,'BT2_FRAME_LIMIT':str(frames),'BT2_STALL_FIELDS':str(stall),'BT2_PAD_SCRIPT':pad_script})
    if isinstance(pictures,int):pictures=(pictures,) if pictures else ()
    ppms=[out/(label+(f'-{flip}' if len(pictures)>1 else '')+'.ppm') for flip in pictures]
    for ppm in ppms:ppm.unlink(missing_ok=True);ppm.with_suffix('.png').unlink(missing_ok=True) # No stale pictures from an earlier run.
    if pictures:env['BT2_GPU_GS_SNAPSHOT']=';'.join(f'{flip}:{ppm.as_posix()}' for flip,ppm in zip(pictures,ppms))
    if run_startup_probe.gpu_build('Release'):env['BT2_GPU_GS']='on'
    start=time.time()
    with log.open('w',encoding='utf-8',errors='replace') as stream:
        subprocess.run(cmd,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT)
    seconds=round(time.time()-start)
    lines=[l for l in log.read_text(encoding='utf-8',errors='replace').splitlines() if not l.startswith(('[INFO]','[WARN]'))]
    if not report.exists():
        return {'case':case,'result':'failed','seconds':seconds,'stop':stop_line(lines) or 'no report'},[]
    data=json.loads(report.read_text(encoding='utf-8'))
    interpreted=[e['entry'] for e in data.get('ee_interpreted_entries',[])]
    ok=passed(data) if passed else bool(data.get('frame_budget_reached'))
    row={'case':case,'result':'ok' if ok else 'stop','flips':data.get('gs_display_flips',0),
         'entered':data.get('distinct_ee_entered',0),'interpreted':len(interpreted),'seconds':seconds,
         'stop':'' if ok else stop_line(lines)}
    for suffix in DUMPS:Path(str(report)+suffix).unlink(missing_ok=True)
    for ppm in ppms:
        if ppm.exists():to_png(ppm)
    if ok and not interpreted:report.unlink();log.unlink()
    return row,interpreted


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--characters',default='0-4',help='Character numbers, e.g. 0-120 (default: %(default)s).')
    p.add_argument('--variants',default='0',help='Variant numbers 0-8 (default: %(default)s).')
    p.add_argument('--opponent',type=int,help='The other side\'s character (default: the next character).')
    p.add_argument('--stages',default='0',help='Stage numbers, used in turn (default: %(default)s).')
    p.add_argument('--case',action='append',default=[],help='A BT2_START_BATTLE text to run (instead of the ranges; repeatable).')
    p.add_argument('--frames',type=int,default=4500,help='Frame budget per battle (default: %(default)s; one battle to K.O. took about 4,000).')
    p.add_argument('--picture',type=int,default=2500,help='Flip at which each battle\'s picture is saved as <case>.png for a look (0: none; default: %(default)s).')
    p.add_argument('--stall-fields',type=int,default=600,help='Fields without a display flip that end a run (default: %(default)s).')
    p.add_argument('--out',type=Path,default=ROOT/'work/coverage/battles')
    p.add_argument('--again',action='store_true',help='Rerun cases already in the summary.')
    a=p.parse_args()
    todo=a.case or cases(numbers(a.characters),numbers(a.variants),a.opponent,numbers(a.stages))
    a.out.mkdir(parents=True,exist_ok=True)
    summary=a.out/'summary.tsv';rows=read_summary(summary)
    seen_path=a.out/'interpreted.txt'
    seen={int(l,16) for l in seen_path.read_text().split()} if seen_path.exists() else set()
    for n,case in enumerate(todo,1):
        if case in rows and not a.again:continue
        row,interpreted=run_case(case,a.disc.resolve(),a.out,a.frames,a.stall_fields,a.picture)
        rows[case]=row;write_summary(summary,rows)
        seen.update(interpreted);seen_path.write_text(''.join(f'{x:08X}\n' for x in sorted(seen)))
        print(f'[{n}/{len(todo)}] {case}: {row["result"]} flips {row.get("flips","-")} entered {row.get("entered","-")} '
              f'interpreted {row.get("interpreted","-")} ({row["seconds"]} s) {row.get("stop","")}',flush=True)
    results=[r['result'] for r in rows.values()]
    print(f'{results.count("ok")} ok, {results.count("stop")} stopped, {results.count("failed")} failed; summary {summary}')


if __name__=='__main__':main()
