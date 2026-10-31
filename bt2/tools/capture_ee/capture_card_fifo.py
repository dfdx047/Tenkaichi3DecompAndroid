"""Observe actual first absent-card FIFO packet and diagnostic receive bytes."""
import ctypes,hashlib,json,struct,time
from capture_sio2_dma import BASE,SCRATCH,CODE,pack
IMPORT=BASE+0x144

def hook(target=0):
    w=[0x3C01BF81,0xA0248260,0x3C18001D,0x8F190004,0];labels={};fix=[]
    def branch(op,rs,rt,label):fix.append((len(w),op,rs,rt,label));w.extend([0,0])
    branch(5,25,0,'record');w += [0x24030081];branch(5,4,3,'return')
    w += [0x3C030005,0x8C63CF08,0,0xAF030008,0x8C630028,0,0xAF03000C,
          0x00031A02,0x306303FF,0xAF030010]
    labels['record']=len(w)
    w += [0x03381821,0xA0640800,0x27390001,0xAF190004,0x8F030010,0]
    branch(5,25,3,'return')
    # Read the actual device FIFO into owned scratch, after the real last transmit store.
    w += [0x3C01BF80,0x34218264,0x00001821,0x27090880]
    labels['read']=len(w);w += [0x902A0000,0,0xA12A0000,0x25290001,0x24630001]
    branch(5,3,25,'read')
    w += [0x3C01BF81,0x8C23826C,0,0xAF030014,0x24030001,0xAF030000,0x1000FFFF,0]
    labels['return']=len(w);w += [0x03E00008,0]
    for i,op,rs,rt,label in fix:w[i]=(op<<26)|(rs<<21)|(rt<<16)|((labels[label]-i-1)&0xFFFF)
    return pack(w)

def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from capture_ee import FRESH_BIND_STUB,texture_stub
    from inspect_elf import inspect
    k=reader.kernel;k.OpenProcess.restype=ctypes.c_void_p;handle=k.OpenProcess(0x438,False,reader.pid)
    if not handle:raise ctypes.WinError(ctypes.get_last_error())
    k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,b):
        count=ctypes.c_size_t();buffer=ctypes.create_string_buffer(b)
        if not k.WriteProcessMemory(handle,p,buffer,len(b),ctypes.byref(count)) or count.value!=len(b):raise ctypes.WinError(ctypes.get_last_error())
    try:
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024)
        first,delay=struct.unpack_from('<2I',before,IMPORT)
        if first!=0x3C01BF81 or delay!=0xA0248260 or before[SCRATCH:SCRATCH+0xA00]!=pack([0x400D]*640):raise ValueError('FIFO store/scratch differs')
        target=0
        write(ip+SCRATCH,bytes(0xA00));write(ip+CODE,hook(target));write(ip+IMPORT,pack([0x08000000|(CODE>>2),0]))
        report=inspect(original);seg=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=seg['file_offset']+address-seg['virtual_address'];write(ee_pointer+address,original[off:off+8])
        loop=FRESH_BIND_STUB+len(texture_stub('rpc-mc-query-before'))-8;write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('First absent-card FIFO packet not reached')
        ram=reader.read(ip,2*1024*1024);values=struct.unpack_from('<19I',ram,SCRATCH)
        if values[1]!=3 or reader.read(ip+SCRATCH,76)!=ram[SCRATCH:SCRATCH+76]:raise ValueError('FIFO publication changed')
        payload=ram[SCRATCH+0x800:SCRATCH+0x800+values[1]];reply=ram[SCRATCH+0x880:SCRATCH+0x880+values[1]]
        (folder/'card-fifo-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'transmit':list(payload),'reply':list(reply),'descriptor':values[2],'command':values[3],'status':values[5],'iop_sha256':hashlib.sha256(ram).hexdigest(),'limitations':'Actual first absent-card FIFO stores and direct diagnostic receive reads; stopped before control START/IRQ. No reply is substituted into guest execution.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'card-fifo.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(meta,indent=2))
    finally:k.CloseHandle(handle)
