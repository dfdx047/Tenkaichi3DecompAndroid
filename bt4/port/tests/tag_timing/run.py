from pathlib import Path
import json,os,shutil,subprocess
root=Path(__file__).resolve().parents[3]
out=root/'port/build/test_tag_timing';out.mkdir(exist_ok=True)
prefix=Path(os.environ.get('SDL3_PREFIX',str(root/'port/build/win/SDL3-3.4.12/x86_64-w64-mingw32')))
cc=os.environ.get('CC') or shutil.which('gcc')
if not cc:raise SystemExit('Set CC to a MinGW-w64 C compiler.')
args=[cc,'-std=gnu99','-O2',str(Path(__file__).with_name('test_tag_timing.c')),
      '-I'+str(prefix/'include'),'-L'+str(prefix/'lib'),'-lSDL3','-lwinhttp','-lws2_32','-ladvapi32','-o',str(out/'test.exe')]
p=subprocess.run(args,capture_output=True,text=True,creationflags=0x08000000);assert p.returncode==0,p.stderr
shutil.copy2(prefix/'bin/SDL3.dll',out/'SDL3.dll')
p=subprocess.run([str(out/'test.exe')],cwd=out,capture_output=True,text=True,creationflags=0x08000000);assert p.returncode==0,p.stdout+p.stderr
result=json.loads(p.stdout)
assert result['cpu_delay']==result['cpu_ping']==result['network_schedule_holes']==0
assert result['cpu_first_frame_ready']==1 and result['cpu_local_frames']==240
assert result['cpu_input_packets_sent']==0 and result['transport_rtt']==40
print(json.dumps(result,indent=2))
