"""Byte parity between the shipped WASM kernel and its production native wrapper.

Both execute one generated binary event stream. No game data, clock sleeps,
network sockets, installed client or desktop surface are used.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,struct,subprocess,tempfile

ROOT=Path(__file__).resolve().parents[3]
PROTOCOL=0x54540009
NEUTRAL=bytes([0,0,0,0,127,127]+[0]*12)

def word(packet,offset,value):struct.pack_into('<I',packet,offset,value&0xffffffff)
def number(packet,offset):return struct.unpack_from('<I',packet,offset)[0]
def request(command,epoch,x=0,y=0,z=0,pad=None):
    packet=bytearray(512);struct.pack_into('<6I',packet,0,PROTOCOL,command,epoch,x,y,z)
    if pad is not None:packet[16:34]=pad
    return bytes(packet)

def controls(frame,seat):
    if frame==0 and seat==0:return struct.pack('<I',0x200)+bytes([127,127,1,0,1]+[0]*9)
    pad=bytearray(NEUTRAL);word(pad,0,1<<((frame//7+seat)%20))
    pad[4]=(frame*7+seat*31)%255;pad[5]=(frame*13+seat*17)%255
    pad[6]=int(frame%53==0);pad[8]=int(frame%71==0)
    return bytes(pad)

def make_events(armed,one_human,humans,count,frames):
    epoch=number(armed,8);events=[];descriptions=[]
    def add(op,member=101,packet=None,expected=None,label='',frame=None):
        events.append(struct.pack('<II',op,member)+(packet or bytes(512)))
        descriptions.append(dict(op=op,expected=expected,label=label,frame=frame,member=member))
    invalid=[]
    for name,offset,value in [('running-snapshot',28,5),('error-snapshot',16,9),('delay',40,2),
                              ('bad-protocol',0,PROTOCOL^1),('bad-slot-count',472,11),('bad-host',24,9001)]:
        packet=bytearray(armed);word(packet,offset,value);invalid.append((name,bytes(packet)))
    for name,mask in [('reserved-flags',1024),('unconfirmed',-2),('unloaded',-3),('unarmed',-5)]:
        packet=bytearray(armed);flags=number(packet,104)
        word(packet,104,flags|mask if mask>0 else flags&mask);invalid.append((name,bytes(packet)))
    packet=bytearray(armed);packet[56]^=1;invalid.append(('load-digest',bytes(packet)))
    packet=bytearray(armed);word(packet,112,101);invalid.append(('duplicate-member',bytes(packet)))
    invalid.append(('only-one-human',one_human))
    add(7,expected=0,label='null-import')
    for name,packet in invalid:add(5,packet=packet,expected=0,label='reject-'+name)
    add(5,packet=armed,expected=1,label='import-armed')
    add(4,label='armed-snapshot')
    add(1,102,request(9,epoch),expected=4,label='only-host-can-start')
    add(1,101,request(9,epoch),expected=0,label='start')
    disconnected=humans-1
    for frame in range(frames):
        if frame==333:
            for name,packet in invalid:add(5,packet=packet,expected=0,label='running-reject-'+name,frame=frame)
        if frame==1199:
            # Already-in-flight input from a peer that disconnects next frame.
            add(1,101+disconnected,request(11,epoch,1202,pad=controls(1202,disconnected)),0,'prefetched-before-disconnect',frame)
        if frame==1200:add(3,101+disconnected,label='disconnect',frame=frame)
        if frame==1201:add(1,101+disconnected,request(11,epoch,1203,pad=controls(1203,disconnected)),2,'inactive-input-rejected',frame)
        if frame%23==0 and frame+2<frames and humans>1 and frame<1198:
            add(1,102,request(11,epoch,frame+2,pad=controls(frame+2,1)),0,'reordered-future-input',frame)
        for seat in range(humans):
            if seat==disconnected and frame>=1200:continue
            if seat==0 and 1<=frame<=8:continue
            if seat==disconnected and frame%97 in (31,32,33,34,35):continue
            packet=request(11,epoch,frame,pad=controls(frame,seat))
            add(1,101+seat,packet,0,'input',frame)
            if frame%11==0:add(1,101+seat,packet,0,'duplicate-input',frame)
            if frame%53==0:
                conflicting=bytearray(packet);conflicting[16]^=0x80
                add(1,101+seat,bytes(conflicting),9,'conflicting-duplicate',frame)
        if frame%101==0:
            add(1,9001,request(11,epoch,frame,pad=NEUTRAL),2,'unknown-member',frame)
            add(1,101,request(11,epoch-1,frame,pad=NEUTRAL),8,'old-epoch',frame)
            add(1,101,request(11,epoch+1,frame,pad=NEUTRAL),8,'future-epoch',frame)
            add(1,101,request(11,epoch,frame+512,pad=NEUTRAL),8,'outside-history',frame)
        add(2,label='commit',frame=frame)
        if frame%17==0:add(1,101,request(11,epoch,frame,pad=controls(frame+13,0)),0,'late-input-ignored',frame)
        if frame in (8,334,1203,1999):add(4,label='state-snapshot',frame=frame)
    add(6,label='abort');add(4,label='aborted-snapshot')
    add(1,101,request(11,epoch,frames,pad=NEUTRAL),1,'no-input-after-abort')
    return b''.join(events),descriptions

def compare(native,wasm,label):
    if native==wasm:return None
    at=next((i for i,(a,b) in enumerate(zip(native,wasm)) if a!=b),min(len(native),len(wasm)))
    return dict(stream=label,byte_offset=at,native_size=len(native),wasm_size=len(wasm),
                native_byte=native[at] if at<len(native) else None,wasm_byte=wasm[at] if at<len(wasm) else None)

def validate(trace,wire,description,humans,count,frames):
    errors=[];wire_at=0;committed=[];expected_pending=0;previous_state=(0,0,0)
    for index,event in enumerate(description):
        op,value,phase,epoch,pending=struct.unpack_from('<5I',trace,index*20)
        if event['expected'] is not None and value!=event['expected']:
            errors.append(dict(event=index,label=event['label'],frame=event['frame'],expected=event['expected'],actual=value))
        if op in (5,7) and event['expected']==0 and (phase,epoch,pending)!=previous_state:
            errors.append(dict(event=index,label='rejected-import-mutated-state',before=previous_state,after=(phase,epoch,pending)))
        if op==2:
            packet=wire[wire_at:wire_at+512];wire_at+=512;committed.append(packet)
            frame=len(committed)-1;expected_pending+=1
            if not value&1 or number(packet,0)!=PROTOCOL or number(packet,4)!=101 or number(packet,12)!=frame or pending!=expected_pending:
                errors.append(dict(event=index,label='commit-sequence',frame=frame,pending=pending,value=value))
            for seat in range(humans,count):
                if packet[16+seat*18:34+seat*18]!=NEUTRAL:errors.append(dict(label='CPU-neutral',frame=frame,seat=seat))
        elif op==4:wire_at+=512
        previous_state=(phase,epoch,pending)
    for frame in range(1,9):
        expected=bytearray(controls(0,0) if frame<=3 else NEUTRAL);expected[6]=expected[8]=0
        actual=committed[frame][16:34]
        if actual!=expected:errors.append(dict(label='held-for-three-then-neutral',frame=frame,expected=bytes(expected).hex(),actual=actual.hex()))
    disconnected=humans-1
    for frame in range(1200,frames):
        actual=committed[frame][16+disconnected*18:34+disconnected*18]
        if actual!=NEUTRAL:errors.append(dict(label='disconnected-seat-must-be-neutral',frame=frame,seat=disconnected,actual=actual.hex()))
    if len(committed)!=frames:errors.append(dict(label='frame-count',expected=frames,actual=len(committed)))
    return errors

def run(output,frame_count=2000):
    output.mkdir(parents=True,exist_ok=True);env=dict(os.environ)
    cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc');node=shutil.which('node')
    if not cc or not node:raise RuntimeError('A native C compiler and Node.js are required')
    hidden={'creationflags':0x08000000} if os.name=='nt' else {}
    native=output/('native-kernel.exe' if os.name=='nt' else 'native-kernel')
    command=[cc,'-std=c99','-O2','-I'+str(ROOT/'port/include'),str(Path(__file__).with_name('native_fixture.c')),
             str(ROOT/'port/src/gs/tag_team_authority.c'),str(ROOT/'port/src/tag_team.c'),str(ROOT/'port/src/tag_team_wire.c'),'-o',str(native)]
    subprocess.run(command,env=env,check=True,**hidden)
    one_human=output/'one-human.armed';subprocess.run([str(native),'--fixture',str(one_human),'1','4'],check=True,**hidden)
    rows=[]
    for humans,count,ffa in [(2,2,0),(2,4,0),(2,10,0),(4,4,0),(4,6,0),(4,10,0),(10,10,0),(6,6,1)]:
        name=f'{humans}-humans-{count-humans}-CPU'+('-FFA' if ffa else '');folder=output/name;folder.mkdir(exist_ok=True)
        armed=folder/'armed.bin';subprocess.run([str(native),'--fixture',str(armed),str(humans),str(count),str(ffa)],check=True,**hidden)
        events,description=make_events(armed.read_bytes(),one_human.read_bytes(),humans,count,frame_count)
        path=folder/'events.bin';path.write_bytes(events)
        subprocess.run([str(native),str(path),str(folder/'native.trace'),str(folder/'native.wire')],check=True,**hidden)
        subprocess.run([node,str(Path(__file__).with_name('wasm_fixture.mjs')),str(ROOT/'cloudflare/kernel.wasm'),str(path),str(folder/'wasm.trace'),str(folder/'wasm.wire')],check=True,**hidden)
        nt=(folder/'native.trace').read_bytes();wt=(folder/'wasm.trace').read_bytes()
        nw=(folder/'native.wire').read_bytes();ww=(folder/'wasm.wire').read_bytes()
        mismatches=[x for x in [compare(nt,wt,'command-results-and-state'),compare(nw,ww,'wire-packets')] if x]
        wire_events=[event for event in description if event['op'] in (2,4)]
        for mismatch in mismatches:
            event_index=mismatch['byte_offset']//(20 if mismatch['stream']=='command-results-and-state' else 512)
            candidates=description if mismatch['stream']=='command-results-and-state' else wire_events
            if event_index<len(candidates):mismatch['event']=candidates[event_index]
        errors=validate(nt,nw,description,humans,count,frame_count)
        row=dict(case=name,frames=frame_count,events=len(description),byte_parity=not mismatches,
                 wire_bytes=len(nw),trace_bytes=len(nt),wire_sha256=hashlib.sha256(nw).hexdigest(),
                 mismatches=mismatches,semantic_errors=errors,passed=not mismatches and not errors)
        (folder/'RESULT.json').write_text(json.dumps(row,indent=2)+'\n');rows.append(row)
        print(json.dumps({k:row[k] for k in ['case','frames','events','byte_parity','passed','semantic_errors']}),flush=True)
    report=dict(passed=all(row['passed'] for row in rows),scenarios=rows,
                wasm_sha256=hashlib.sha256((ROOT/'cloudflare/kernel.wasm').read_bytes()).hexdigest(),
                native_kernel_sha256=hashlib.sha256((ROOT/'cloudflare/kernel.c').read_bytes()).hexdigest(),
                production_sources={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
                    ['cloudflare/kernel.c','cloudflare/sync_kernel.inc','port/src/gs/tag_team_authority.c',
                     'port/src/tag_team.c','port/src/tag_team_wire.c']},
                scope='Input coordination kernel parity and fixture assertions, without native gameplay or Internet traffic')
    (output/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['passed']:raise AssertionError('Kernel parity or semantic fixture failed; see '+str(output/'RESULTS.json'))
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:run(args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='bt4-kernel-parity-') as directory:run(Path(directory))
