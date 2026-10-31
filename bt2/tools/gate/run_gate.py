"""Pre-commit gate: Debug and Release builds, CTest, Python suites, the Debug probe runs with the original
comparisons, the Release battle run (battle_run.py: the scripted fight against its recorded baseline) and
the map closure check (map_gaps.py --check: nothing the programs name or build is left without a function
or label).

Independent steps run concurrently (each probe run writes its own report), so the gate takes about as
long as its longest probe run plus the build. Exit status is non-zero when any step fails.
"""
import argparse,os,subprocess,sys,time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'work/bt2-generation'
SUITES=('capture_ee','ps2recomp','iop','ghidra','graphics/textures','audio','perf','vu1','saves','gate','build')
# Comparisons against captures taken before the end of the full run: each needs the probe stopped at
# the same point (BT2_PHASE_STOP).
PHASES={'311860':('dbzp_load',),'310518':('frame_vsync',),
        '28b218':('overlay_dispenv','display_reset','cri_autodma','dbc_rpc','mc_rpc','sound_bank'),'24b81c':('frame_gs_reset',)}
# Not part of the gate: comparisons that need their own capture inputs or output paths, an earlier stop
# than the gate takes, or that differ as recorded in docs/PROG.md. Run them by hand when their area changes.
NOT_IN_GATE=('cd_init','heap_allocation','heap_bind','heap_copy','rpc_bind','spu2_initialization','spu2_transfer',
             'cri_preemption','cri_rpc','cri_second_channel','disc_read','ee_main_status','second_heap','startup_workers','vblank_wake')
def phase_only():return {name for names in PHASES.values() for name in names}
def verifier_command(name,report,disc):
    cmd=[sys.executable,str(ROOT/'tools/capture_ee'/f'verify_{name}.py'),'--native',str(report)]
    if name in ('disc_command','disc_read'):cmd+=['--disc',str(disc)]
    if name=='disc_read':cmd+=['--workspace','0x1D4B00']
    if name=='ee_sound_dispatch':cmd+=['--queued']
    return cmd
def full_run_verifiers():
    names=sorted(p.stem[len('verify_'):] for p in (ROOT/'tools/capture_ee').glob('verify_*.py'))
    return [n for n in names if n not in phase_only() and n not in NOT_IN_GATE]
def run(label,cmd,env=None):
    start=time.monotonic();r=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,errors='replace')
    return label,r.returncode==0,time.monotonic()-start,(r.stdout+r.stderr).strip()
def last_line(text):return text.splitlines()[-1][:160] if text else ''
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--jobs',type=int,default=os.cpu_count() or 4);p.add_argument('--skip-build',action='store_true');a=p.parse_args()
    disc=a.disc.resolve();temp=ROOT/'work/test-temp';temp.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,TEMP=str(temp),TMP=str(temp));failed=[];began=time.monotonic()
    def report(results,verbose=True):
        for label,ok,seconds,text in results:
            if not ok:failed.append(label)
            if verbose or not ok:print(f"{'ok  ' if ok else 'FAIL'} {label} ({seconds:.0f} s){': '+last_line(text) if not ok or label.startswith('battle run') else ''}",flush=True)
    if not a.skip_build:
        msvc=[sys.executable,str(ROOT/'tools/build/msvc.py'),'cmake','--build','build/probe','--config','Debug']
        release=[sys.executable,str(ROOT/'tools/build/msvc.py'),'cmake','--build','build/probe','--config','Release']
        report([run('build Debug',msvc,env),run('build Debug entry probe',msvc+['--target','bt2-entry-probe'],env),run('build Release',release,env)])
        if failed:sys.exit('Gate failed: '+', '.join(failed))
    probe=[sys.executable,str(ROOT/'tools/ps2recomp/run_startup_probe.py'),'--disc',str(disc)]
    steps=[('ctest',['ctest','--test-dir','build/probe','-C','Debug','-j',str(a.jobs),'--output-on-failure'],env),
           ('probe: sound worker',probe+['--sound-worker'],env),('probe: full run',probe,env)]
    steps+=[(f'probe: phase {ph}',probe+['--report',str(WORK/f'phase-{ph}/entry-stop.json')],dict(env,BT2_PHASE_STOP=ph)) for ph in PHASES]
    steps.append(('battle run (Release)',[sys.executable,str(ROOT/'tools/gate/battle_run.py'),'--disc',str(disc)],env))
    # The map holds everything the programs name or build, and every address inside a function has a label.
    steps.append(('map closure',[sys.executable,str(ROOT/'tools/ps2recomp/map_gaps.py'),'--check'],env))
    steps+=[(f'python: {s}',[sys.executable,'-m','unittest','discover','-s',f'tools/{s}'],env) for s in SUITES]
    with ThreadPoolExecutor(a.jobs) as pool:
        report(pool.map(lambda s:run(*s),steps))
        native=WORK/'entry-stop.json'
        checks=[(f'compare: {n}',verifier_command(n,native,disc),env) for n in full_run_verifiers()]
        checks+=[(f'compare at {ph}: {n}',verifier_command(n,WORK/f'phase-{ph}/entry-stop.json',disc),env) for ph,names in PHASES.items() for n in names]
        results=list(pool.map(lambda s:run(*s),checks));report(results,verbose=False)
        print(f'{sum(ok for _,ok,_,_ in results)}/{len(results)} comparisons pass')
    print(f"{'GATE FAILED: '+', '.join(failed) if failed else 'GATE PASSED'} ({time.monotonic()-began:.0f} s)")
    sys.exit(1 if failed else 0)
if __name__=='__main__':main()
