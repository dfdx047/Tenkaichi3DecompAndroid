"""Diagnostic actual bind to the already registered retail EE sound server."""
import ctypes,hashlib,json,struct,time
from capture_sio2_dma import BASE,SCRATCH,CODE,pack
IMPORT=0x4D030+0x11B40

def hook(target):
    return pack([0x3C18001D,0xAF1F001C,0xAF1C0020,0xAF1D0024,
       0x3C04001D,0x24840200,0x3C058000,0x34A50704,0x00003021,
       0x0C000000|(target>>2),0,0x3C18001D,0xAF020018,
       0x24190001,0xAF190000,0x1000FFFF,0])

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
        if first>>26!=2 or delay!=0x24000021 or before[SCRATCH:SCRATCH+0x400]!=pack([0x400D]*256):raise ValueError('START import/scratch differs')
        bindword,binddelay=struct.unpack_from('<2I',before,0x88630+0x1244)
        if bindword>>26!=2 or binddelay!=0x2400000F:raise ValueError('Bind API import differs')
        target=(bindword&0x3FFFFFF)<<2
        write(ip+SCRATCH,bytes(0x400));write(ip+CODE,hook(target));write(ip+IMPORT,pack([0x08000000|(CODE>>2),0]))
        report=inspect(original);seg=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=seg['file_offset']+address-seg['virtual_address'];write(ee_pointer+address,original[off:off+8])
        loop=FRESH_BIND_STUB+len(texture_stub('rpc-mc-query-before'))-8;write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('Diagnostic real EE bind return not reached')
        ram=reader.read(ip,2*1024*1024);values=struct.unpack_from('<19I',ram,SCRATCH)
        if values[6]!=0 or reader.read(ip+SCRATCH,76)!=ram[SCRATCH:SCRATCH+76]:raise ValueError('Delay publication changed')
        client=list(struct.unpack_from('<10I',ram,SCRATCH+0x200))
        ee_server=list(struct.unpack('<17I',reader.read(ee_pointer+0x3A6D18,68)))
        (folder/'iop-ee-bind-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'return':values[6],'client':client,'ee_server':ee_server,'target':target,'iop_sha256':hashlib.sha256(ram).hexdigest(),'limitations':'Diagnostic new scratch client invokes actual synchronous bind against the already registered EE server at the card-delay point. Not the earlier SDRDRV bind timing or RPC ID.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'iop-ee-bind.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(meta,indent=2))
    finally:k.CloseHandle(handle)
