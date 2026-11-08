"""Isolated first MCSERV SIO2 slice setup; actual API, no DMA completion claimed."""
import ctypes,hashlib,json,struct,time
BASE=0x4BF30;SCRATCH=0x1D0000;CODE=SCRATCH+0x100;IMPORT=BASE+0xE28

def pack(w):return struct.pack('<'+str(len(w))+'I',*w)
def hook(target):
    w=[0x3C18001D,0xAF040004,0xAF050008,0xAF06000C,0xAF070010,0x8FB90010,0,0xAF190014,0xAF1F001C,0xAF1C0020,0xAF1D0024,
       0x0C000000|(target>>2),0,0x3C18001D,0xAF020018]
    for reg,offset in ((0x1540,40),(0x1544,44),(0x1548,48)):
        w += [0x3C19BF80,0x8F390000|reg,0,0xAF190000|offset]
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
        if first>>26!=2 or delay!=0x2400001C or before[SCRATCH:SCRATCH+0x400]!=pack([0x400D]*256):raise ValueError('Slice import/scratch differs')
        target=(first&0x3FFFFFF)<<2
        write(ip+SCRATCH,bytes(0x400));write(ip+CODE,hook(target));write(ip+IMPORT,pack([0x08000000|(CODE>>2),0]))
        report=inspect(original);seg=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=seg['file_offset']+address-seg['virtual_address'];write(ee_pointer+address,original[off:off+8])
        loop=FRESH_BIND_STUB+len(texture_stub('rpc-mc-query-before'))-8;write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('Initial card slice setup not reached')
        ram=reader.read(ip,2*1024*1024);values=struct.unpack_from('<13I',ram,SCRATCH)
        source=values[2]&0xFFFFFF;payload=ram[source:source+144]
        if values[1]!=11 or values[3:7]!=(36,1,1,1) or values[10:13]!=(source,0x10024,0x201):raise ValueError('Actual slice setup/readback differs')
        if len(payload)!=144 or payload!=bytes([0x81,0x11])+bytes(142) or reader.read(ip+source,144)!=payload:raise ValueError('First card command payload differs')
        if reader.read(ip+SCRATCH,52)!=ram[SCRATCH:SCRATCH+52]:raise ValueError('Slice publication changed')
        (folder/'sio2-slice-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'arguments':list(values[1:6]),'return':values[6],'registers':list(values[10:13]),'target':target,'payload_sha256':hashlib.sha256(payload).hexdigest(),'iop_sha256':hashlib.sha256(ram).hexdigest(),'limitations':'Actual slice programming only; stopped before StartDMA. No bytes transferred, device reply, callback or completion inferred.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'sio2-slice.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print('Actual first card slice setup verified:',meta['arguments'],meta['registers'])
    finally:k.CloseHandle(handle)
