"""Story sweep (STORY_MODE_PLAN.md): every part's story scene and battle without input.

Runs the Release probe once per part with BT2_START_EPISODE=<episode>,<part>,marker:<m>: the map reports the player at the part's story marker, the starter answers the waits for a button
(message boxes, the pre-battle screen, the results, the save questions without saving) and ends the run
300 flips after the part's after-battle step. A part's result is 'cleared' or 'lost' (the battle's
outcome, with the CPU on both sides), or 'stop' (a stop, a stall, a
waiting screen that kept getting presses, or the frame budget ran out first). --jobs runs several
parts at once (each run uses about one CPU core; a stated exception to one run at a time, for sweeps). The parts and their markers come from story_scenes.py's
scenes.json (a part whose first scene goes to its story scene itself starts without a marker); other parts
are listed as skipped. Results go to <out>/summary.tsv
(resumable, as battle_sweep.py), pictures at the chosen flips to <out>/<case>-<flip>.png, and each part's battle
records (the starter's report: rule kind, Dragon Ball placed and picked up, rewards, saga items unlocked) to
<out>/battles.json; with --watch, the listed functions each part entered go there too.
"""
import argparse,concurrent.futures,json,sys,threading
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import battle_sweep
ROOT=battle_sweep.ROOT


def cases(parts,episodes=None):
    """BT2_START_EPISODE texts for the parts with a story marker or whose first scene goes to the story
    scene itself, and the other parts."""
    todo,skipped=[],[]
    for part in parts:
        if episodes is not None and part['episode'] not in episodes:continue
        start=f'{part["episode"]},{part["part"]}'
        if part['story_marker'] is not None:todo.append(f'{start},marker:{part["story_marker"]}')
        elif part.get('direct_story'):todo.append(start)
        else:skipped.append(start)
    return todo,skipped


def optional_cases(parts,episodes=None):
    """One BT2_START_EPISODE text per scene that an optional marker leads to (a town with its characters,
    its shop or its fights): the first part and marker that lead there."""
    seen=set();todo=[]
    for part in parts:
        if episodes is not None and part['episode'] not in episodes:continue
        for marker,scene in sorted(part['markers'].items(),key=lambda item:int(item[0])):
            if scene==part['first_scene']+1 or scene in seen:continue
            seen.add(scene);todo.append(f'{part["episode"]},{part["part"]},marker:{marker}')
    return todo


def name(case):
    episode,part,*marker=case.split(',')
    return f'e{int(episode):02}-p{int(part):02}'+(f'-m{int(marker[0][7:]):02}' if marker else '')


def outcome(report):
    """'cleared' or 'lost' once the part's battle is over, 'town' once every character of a town was
    talked to or touched, 'stuck: <function>' when a waiting screen kept getting presses, '' otherwise."""
    if report.get('starter_stuck'):return f'stuck: {report["starter_stuck"]:08X}'
    if report.get('starter_parts_completed',0)<1:return 'town' if report.get('starter_town_done') else ''
    return 'cleared' if report.get('starter_battles_cleared') else 'lost'


def battle_text(battles):
    """A part's battles for the summary: rule kind, the ball placed and picked up, rewards, sagas unlocked."""
    return '; '.join(f'rule {b["rule"]} ball {b["ball"]}/{b["ball_picked"]} rewards {"-".join(str(r) for r in b["rewards"] if r>=0) or "none"}'
                     +(f' sagas {"-".join(map(str,b["sagas"]))}' if b['sagas'] else '') for b in battles)


def run_part(case,disc,out,frames,stall,pictures,watch=()):
    """One part's run; the row's result is its outcome, or 'stop' / 'failed'."""
    found={}
    def passed(report):
        found['outcome']=outcome(report)
        found['scenes']=report.get('starter_scenes',[]);found['blocks']=report.get('starter_town_blocks',0)
        found['battles']=report.get('starter_battles',[])
        entered={int(x,16) if isinstance(x,str) else x for x in report.get('ee_entered_addresses',[])}
        found['watched']=[f'{x:08X}' for x in watch if x in entered]
        return found['outcome'] in ('cleared','lost','town')
    row,interpreted=battle_sweep.run_case(case,disc,out,frames,stall,pictures,'BT2_START_EPISODE',name(case),passed)
    if row['result']=='ok':
        row['result']=found['outcome']
        row['stop']=f'scenes {"-".join(map(str,found["scenes"]))}; {found["blocks"]} town blocks'+(f'; {battle_text(found["battles"])}' if found['battles'] else '')
    elif found.get('outcome'):row['stop']=found['outcome']
    elif row['result']=='stop' and not row['stop']:row['stop']='the part did not end within the frame budget'
    row['battles']=found.get('battles',[])
    if watch:row['watched']=found.get('watched',[])
    return row,interpreted


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--scenes',type=Path,default=ROOT/'work/coverage/story/scenes.json',help='story_scenes.py output.')
    p.add_argument('--episodes',help='Episode numbers, e.g. 0-3,7 (default: all).')
    p.add_argument('--case',action='append',default=[],help='A BT2_START_EPISODE text to run instead (repeatable).')
    p.add_argument('--optional',action='store_true',help="Run the optional markers' scenes (towns, shops, fights) instead, one case per scene; default --out work/coverage/story/optional.")
    p.add_argument('--frames',type=int,default=15000,help='Frame budget per part (default: %(default)s).')
    p.add_argument('--pictures',default='2500,4000,5500',help='Flips at which pictures are saved (default: %(default)s; empty: none).')
    p.add_argument('--stall-fields',type=int,default=600)
    p.add_argument('--out',type=Path)
    p.add_argument('--again',action='store_true',help='Rerun cases already in the summary.')
    p.add_argument('--watch',default='',help='Hex function addresses whose entry per part goes to battles.json, e.g. 274338,2425D8.')
    p.add_argument('--jobs',type=int,default=1,help='Parts run at the same time (default: %(default)s; each run uses about one CPU core).')
    a=p.parse_args()
    parts=json.loads(a.scenes.read_text(encoding='utf-8'))
    episodes=set(battle_sweep.numbers(a.episodes)) if a.episodes else None
    todo,skipped=(optional_cases(parts,episodes),[]) if a.optional else cases(parts,episodes)
    a.out=a.out or ROOT/'work/coverage/story'/('optional' if a.optional else 'sweep')
    if a.case:todo,skipped=a.case,[]
    pictures=tuple(battle_sweep.numbers(a.pictures)) if a.pictures else ()
    a.out.mkdir(parents=True,exist_ok=True)
    summary=a.out/'summary.tsv';rows=battle_sweep.read_summary(summary)
    seen_path=a.out/'interpreted.txt'
    seen={int(l,16) for l in seen_path.read_text().split()} if seen_path.exists() else set()
    todo=[case for case in todo if case not in rows or a.again]
    battles_path=a.out/'battles.json'
    battles=json.loads(battles_path.read_text(encoding='utf-8')) if battles_path.exists() else {}
    watch=[int(x,16) for x in a.watch.split(',') if x]
    lock=threading.Lock();done=[0]
    def one(case):
        row,interpreted=run_part(case,a.disc.resolve(),a.out,a.frames,a.stall_fields,pictures,watch)
        with lock:
            battles[case]=row.pop('battles')
            if watch:battles[case+' entered']=row.pop('watched')
            battles_path.write_text(json.dumps(battles,indent=1),encoding='utf-8')
            rows[case]=row;battle_sweep.write_summary(summary,rows)
            seen.update(interpreted);seen_path.write_text(''.join(f'{x:08X}\n' for x in sorted(seen)))
            done[0]+=1
            print(f'[{done[0]}/{len(todo)}] {case}: {row["result"]} flips {row.get("flips","-")} entered {row.get("entered","-")} '
                  f'interpreted {row.get("interpreted","-")} ({row["seconds"]} s) {row.get("stop","")}',flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        list(pool.map(one,todo))
    results=[r['result'] for r in rows.values()]
    print(f'{results.count("cleared")} cleared, {results.count("lost")} lost, {results.count("stop")} stopped, {results.count("failed")} failed, '
          f'{len(skipped)} parts without a way to their story scene skipped; summary {summary}')
    records=[b for key,found in battles.items() if not key.endswith(' entered') for b in found]
    print(f'{len(records)} battles: {sum(b["rule"]==3 for b in records)} with rule 3, sagas unlocked {sorted({s for b in records for s in b["sagas"]})}, '
          f'Dragon Balls picked up in {sum(b["ball_picked"]>0 for b in records)}')


if __name__=='__main__':main()
