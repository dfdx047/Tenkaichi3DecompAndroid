"""Observe the original first CDVDFSV disc-type read; no return is injected."""
import ctypes,hashlib,json,struct,time
BASE=0x3DD30;HOOK=BASE+0x3B1C;SCRATCH=0x1D0000;CODE=SCRATCH+0x100

def pack(w):return struct.pack('<'+str(len(w))+'I',*w)
def hook(ready=False):
    if ready:return pack([0x3C18001D,0xAF030004,0xAF02000C,0x90630000,0x90420000,0,0xAF030008,0xAF020010,0x24190001,0xAF190000,0x1000FFFF,0])
    return pack([0x3C18001D,0xAF020004,0x90420000,0,0xAF020008,0x24190001,0xAF190000,0x1000FFFF,0])
def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from inspect_elf import inspect
    from capture_ee import FRESH_BIND_STUB,texture_stub
    k=reader.kernel;k.OpenProcess.restype=ctypes.c_void_p;h=k.OpenProcess(0x438,False,reader.pid)
    if not h:raise ctypes.WinError(ctypes.get_last_error())
    k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,b):
        n=ctypes.c_size_t();v=ctypes.create_string_buffer(b)
        if not k.WriteProcessMemory(h,p,v,len(b),ctypes.byref(n)) or n.value!=len(b):raise ctypes.WinError(ctypes.get_last_error())
    try:
        ready=args.capture_point=='rpc-ncmd-call-sema';site=BASE+0x3A34 if ready else HOOK
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024)
        if before[site:site+8]!=pack([0x90630000,0x90420000] if ready else [0x90420000,0x8FA30030]) or before[SCRATCH:SCRATCH+0x200]!=pack([0x400D]*128):raise ValueError('Disc read/scratch differs')
        write(ip+SCRATCH,bytes(0x200));write(ip+CODE,hook(ready));write(ip+site,pack([0x08000000|(CODE>>2),0]))
        r=inspect(original);s=next(s for s in r['load_segments'] if s['virtual_address']<=address<s['virtual_address']+s['file_size']);off=s['file_offset']+address-s['virtual_address']
        write(ee_pointer+address,original[off:off+8]);stub=texture_stub(args.capture_point);write(ee_pointer+FRESH_BIND_STUB+(56 if ready else len(stub)-8),pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('Disc type read not reached')
        ram=reader.read(ip,2*1024*1024);pub=ram[SCRATCH:SCRATCH+(20 if ready else 12)]
        if reader.read(ip+SCRATCH,len(pub))!=pub or struct.unpack_from('<I',pub,4)[0]!=0xBF40200F:raise ValueError('Disc type publication differs')
        (folder/'disc-type-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'iop_sha256':hashlib.sha256(ram).hexdigest(),'register':0xBF40200F,'value':struct.unpack_from('<I',pub,8)[0],'limitations':'Terminal after original register reads; no sector completion observed.'}
        if ready:meta['status_register']=struct.unpack_from('<I',pub,12)[0];meta['status_value']=struct.unpack_from('<I',pub,16)[0]
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'disc-type.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8');print('Actual CDVDFSV disc type:',hex(meta['value']))
    finally:k.CloseHandle(h)
