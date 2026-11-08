"""Isolated actual SIO2 channel-11/12 START capture; stop after receive API."""
import ctypes,hashlib,json,struct,time
from capture_sio2_dma import BASE,SCRATCH,CODE,pack
IMPORT=BASE+0xE30

def hook(target):
    w=[0x3C18001D,0xAF040004,0xAF1F001C,0xAF1C0020,0xAF1D0024,0xAF120050,
       0x0C000000|(target>>2),0,0x3C18001D,0x8F190004,0,0x241A000B]
    # First call (send): return to real caller; second (receive): stop.
    branch=len(w);w += [0,0]
    for reg,offset in ((0x1540,40),(0x1544,44),(0x1548,48)):
        w += [0x3C19BF80,0x8F390000|reg,0,0xAF190000|offset]
    w += [0x8F1F001C,0,0x03E00008,0]
    receive=len(w)
    w[branch]=(5<<26)|(25<<21)|(26<<16)|(receive-branch-1)
    for reg,offset in ((0x1550,52),(0x1554,56),(0x1558,60),(0x8200,64),(0x826C,68),(0x8268,72)):
        upper=0xBF80
        w += [0x3C190000|upper,0x8F390000|reg,0,0xAF190000|offset]
    w += [0x24190001,0xAF190000,0x1000FFFF,0]
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
        if first>>26!=2 or delay!=0x24000020 or before[SCRATCH:SCRATCH+0x400]!=pack([0x400D]*256):raise ValueError('START import/scratch differs')
        target=(first&0x3FFFFFF)<<2
        write(ip+SCRATCH,bytes(0x400));write(ip+CODE,hook(target));write(ip+IMPORT,pack([0x08000000|(CODE>>2),0]))
        report=inspect(original);seg=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=seg['file_offset']+address-seg['virtual_address'];write(ee_pointer+address,original[off:off+8])
        loop=FRESH_BIND_STUB+len(texture_stub('rpc-mc-query-before'))-8;write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('First receive DMA START not reached')
        ram=reader.read(ip,2*1024*1024);values=struct.unpack_from('<19I',ram,SCRATCH)
        if values[1]!=12 or reader.read(ip+SCRATCH,76)!=ram[SCRATCH:SCRATCH+76]:raise ValueError('START publication changed')
        (folder/'sio2-start-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'send_registers':list(values[10:13]),'receive_registers':list(values[13:16]),'raw_pre_control_readbacks':list(values[16:19]),'device_profile':'both memory-card slots disabled','transfer_descriptor':struct.unpack_from('<I',ram,SCRATCH+80)[0],'target':target,'iop_sha256':hashlib.sha256(ram).hexdigest(),'limitations':'Both actual START APIs invoked. Instrumented stop after first receive START; no IRQ scheduling or callback equivalence claimed.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'sio2-start.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(meta,indent=2))
    finally:k.CloseHandle(handle)
