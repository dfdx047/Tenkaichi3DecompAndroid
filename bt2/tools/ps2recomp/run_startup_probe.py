"""Run the current bounded native startup checkpoint using ignored local inputs."""
import argparse,json,subprocess,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
# Default controller input for the measured checkpoint (logical IOP fields): with no memory card the
# game asks "Continue anyway?" (shown by field 373); select Yes and confirm. Repeatable by construction.
DEFAULT_PAD_SCRIPT='400:left,408:-,420:cross,428:-'
INPUTS=('guest-memory.bin','kernel-snapshot.bin','timer3-mode.bin','timer2-mode.bin','intc-mask.bin','timer2-readback.bin','osd-config.bin','osd-readback.bin','dma-status.bin','dma5-control.bin','sif-command.bin','sif-dma.bin','sif-rpc-state.bin','sif-bind.bin','cd-init.bin','scmd-bind.bin','heap-bind.bin','heap-allocation.bin','texture-startup.bin','ee-thread-start.bin','disc-bind.bin','ncmd-bind.bin')
def command(root,disc,config='Debug'):
    folder=root/'work/bt2-generation';binary=root/'build/probe'/config/'bt2-entry-probe.exe'
    paths=[binary,disc,*[folder/name for name in INPUTS]]
    missing=[str(p) for p in paths if not p.is_file()]
    if missing:raise ValueError('Missing local startup inputs: '+', '.join(missing))
    # Existing C++ CLI places report after memory and disc before the final bind bundle.
    return [str(binary),str(folder/INPUTS[0]),str(folder/'entry-stop.json'),*[str(folder/name) for name in INPUTS[1:-1]],str(disc),str(folder/INPUTS[-1])]
def summary(report):
    for key in ('distinct_ee_entered','distinct_ee_completed','distinct_kernel_entered','distinct_kernel_completed'):
        if key not in report:raise ValueError('Probe needs rebuilding with distinct-entry reporting')
    return (f"Distinct EE entries: {report['distinct_ee_entered']} entered / {report['distinct_ee_completed']} completed; "
            f"kernel entries: {report['distinct_kernel_entered']} / {report['distinct_kernel_completed']}.\n"
            f"EE call/return events: {report['calls']} / {report['returns']}.\n"
            f"CRI calls: {report['cri_rpc_calls']}; heaps: {report['heap_calls']}; "
            f"boundary selector={report['selector']:#x}, PC={report['pc']:#x}, target={report['target']:#x}.\n"
            "Completion counts are observed invocation returns, not full gameplay correctness or startup completion.")
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--disc',type=Path,required=True);p.add_argument('--config',choices=('Debug','Release'),default='Debug',help='Probe build configuration; Release runs several times faster for selection loops.');p.add_argument('--sound-worker',action='store_true',help='Diagnostic retained sound-worker/reverse-bind path; main priority remains pending.');p.add_argument('--viewer',action='store_true',help='Present each display flip in a window; no frame budget unless --frames is given.');p.add_argument('--frames',type=int,help='Stop after this many display flips (0: no budget).');p.add_argument('--play',action='store_true',help='Play with the keyboard and a gamepad: the viewer with sound and live input.');p.add_argument('--record',type=Path,help='With --play: write the buttons played as a pad script to this file.');p.add_argument('--sound',action='store_true',help='Play the audio on the default audio device; the device then paces the run at the game speed.');p.add_argument('--scale',choices=('integer','stretch'),default='integer',help='Viewer scaling.');p.add_argument('--report',type=Path,help='Report path (default: work/bt2-generation/entry-stop.json); the dumps are written next to it. Separate paths let runs execute concurrently.');p.add_argument('--pad-script',default=DEFAULT_PAD_SCRIPT,help='Scripted controller input: FIELD:buttons steps (default: %(default)s; empty for none).');a=p.parse_args()
    cmd=command(ROOT,a.disc.resolve(),a.config);env=dict(os.environ);env.pop('BT2_EE_SOUND_DISPATCH_PROBE',None)
    report=a.report.resolve() if a.report else ROOT/'work/bt2-generation'/('sound-worker-stop.json' if a.sound_worker else 'entry-stop.json')
    report.parent.mkdir(parents=True,exist_ok=True);cmd[2]=str(report)
    if a.sound_worker:env['BT2_EE_SOUND_DISPATCH_PROBE']='1'
    env.pop('BT2_VIEWER',None);env.pop('BT2_AUDIO_DEVICE',None);env.pop('BT2_HOST_INPUT',None);env.pop('BT2_PAD_RECORD',None);env.pop('BT2_FRAME_LIMIT',None);env.pop('BT2_VIEWER_SCALE',None);env.pop('BT2_PAD_SCRIPT',None)
    if a.pad_script:env['BT2_PAD_SCRIPT']=a.pad_script
    if a.play:a.viewer=a.sound=True;env['BT2_HOST_INPUT']='1'
    if a.viewer:env['BT2_VIEWER']='1';env['BT2_VIEWER_SCALE']=a.scale
    if a.record:env['BT2_PAD_RECORD']=str(a.record.resolve())
    if a.sound:env['BT2_AUDIO_DEVICE']='1'
    if a.frames is not None:env['BT2_FRAME_LIMIT']=str(a.frames)
    # The GPU renderer draws every picture; a build without it runs headless.
    env.pop('BT2_GPU_GS',None)
    if gpu_build(a.config):env['BT2_GPU_GS']='on'
    subprocess.run(cmd,cwd=ROOT,env=env,check=True)
    print(summary(json.loads(report.read_text(encoding='utf-8'))))
    if a.sound_worker:print('Diagnostic sound-worker scope only: main priority request is pending; the EE stack is cancelled after the checked wait.')
def gpu_build(config):
    # The probe build links paraLLEl-GS when configured with BT2_GPU_GS (build/probe/CMakeCache.txt).
    cache=ROOT/'build/probe/CMakeCache.txt'
    return cache.is_file() and 'BT2_GPU_GS:BOOL=ON' in cache.read_text(encoding='utf-8',errors='replace')
if __name__=='__main__':main()
