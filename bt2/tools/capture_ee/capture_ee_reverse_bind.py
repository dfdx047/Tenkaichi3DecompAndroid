"""Observe the original SDRDRV reverse BindRpc API and return; no response injected."""
import ctypes,hashlib,json,struct,time
from capture_sio2_dma import SCRATCH,CODE,pack
IMPORT=0x88630+0x1244

def hook(target,terminal=True):
    words=[0x3C18001D,0xAF040004,0xAF050008,0xAF06000C,0xAF1F001C,0xAF1C0020,0xAF1D0024,
       0x40196000,0,0xAF190028,0x0C000000|(target>>2),0,0x3C18001D,
       0xAF020018,0x40196000,0,0xAF19002C,0x24190001,0xAF190000,0x1000FFFF,0]
    if not terminal:words[-4:]=[0x8F1F001C,0,0x03E00008,0]
    return pack(words)

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
        if first>>26!=2 or delay!=0x2400000F or before[SCRATCH:SCRATCH+0x400]!=pack([0x400D]*256):raise ValueError('SDRDRV BindRpc import/scratch differs')
        target=(first&0x3FFFFFF)<<2
        write(ip+SCRATCH,bytes(0x400));write(ip+CODE,hook(target,not args.ee_reverse_bind_wait));write(ip+IMPORT,pack([0x08000000|(CODE>>2),0]))
        if args.ee_reverse_bind_wait:
            first_wait,delay_wait=struct.unpack_from('<2I',before,0x88630+0x1304)
            if first_wait>>26!=2 or delay_wait!=0x24000018:raise ValueError('Original SDRDRV SleepThread import differs')
            observer=pack([0x3C18001D,0xAF1F0034,0xAF1C0038,0xAF1D003C,0x24190002,0xAF190000,0x1000FFFF,0])
            write(ip+CODE+0x100,observer);write(ip+0x88630+0x1304,pack([0x08000000|((CODE+0x100)>>2),0]))
        report=inspect(original);seg=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=seg['file_offset']+address-seg['virtual_address'];write(ee_pointer+address,original[off:off+8])
        loop=FRESH_BIND_STUB+len(texture_stub('sound-init-before'))-8;write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([2 if args.ee_reverse_bind_wait else 1]):break
            time.sleep(.02)
        else:raise TimeoutError('Original SDRDRV reverse BindRpc return not reached')
        ram=reader.read(ip,2*1024*1024);values=struct.unpack_from('<19I',ram,SCRATCH)
        if values[1]!=0x88630+0x1CA0 or values[2]!=0x80000704 or values[3]!=0 or values[6]!=0 or reader.read(ip+SCRATCH,76)!=ram[SCRATCH:SCRATCH+76]:raise ValueError('Reverse bind publication differs')
        (folder/'ee-reverse-bind-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'arguments':list(values[1:4]),'client':list(struct.unpack_from('<10I',ram,values[1])),'wait_observed':args.ee_reverse_bind_wait,'wait_ra':values[13],'wait_gp':values[14],'wait_sp':values[15],'return':values[6],'caller_ra':values[7],'gp':values[8],'sp':values[9],'status_before':values[10],'status_after':values[11],'target':target,'iop_sha256':hashlib.sha256(ram).hexdigest(),'limitations':('Original blocking BindRpc executes once, publishes its return/client and reaches the captured first SleepThread call. The SleepThread call is not executed; no wake/callback completion or general scheduler equivalence.' if args.ee_reverse_bind_wait else 'Original blocking BindRpc API executes once and publishes its actual return/client. Terminal IOP observer; no command, callback completion or general scheduler equivalence.')}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'ee-reverse-bind.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(meta,indent=2))
    finally:k.CloseHandle(handle)
