"""Terminal local capture of the first reached LIBSD status read; no success injected."""
import ctypes,hashlib,json,struct,time
from pathlib import Path
BASE=0x82C30;HOOK=BASE+0x4694;SCRATCH=0x1D0000;CODE=SCRATCH+0x2000
REGISTERS=list(range(0,0x800,2))

def hook():
    words=[0x3C18001D,0xAF020004,0xAF1F0008,0xAF1D000C,0x3C19BF90,0x270F0010,0x240E0400,
           0x972D0000,0,0xADED0000,0x27390002,0x25EF0004,0x25CEFFFF,0x15C0FFF9,0,
           0x24190001,0xAF190000,0x1000FFFF,0]
    return struct.pack('<19I',*words)

def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from inspect_elf import inspect
    kernel=reader.kernel
    kernel.OpenProcess.restype=ctypes.c_void_p
    handle=kernel.OpenProcess(0x438,False,reader.pid)
    if not handle:raise ctypes.WinError(ctypes.get_last_error())
    kernel.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,data):
        got=ctypes.c_size_t();buffer=ctypes.create_string_buffer(data)
        if not kernel.WriteProcessMemory(handle,p,buffer,len(data),ctypes.byref(got)) or got.value!=len(data):raise ctypes.WinError(ctypes.get_last_error())
    try:
        iop_pointer,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8))
        before=reader.read(iop_pointer,2*1024*1024)
        expected=struct.pack('<2I',0x03E00008,0x27BD0020)
        if before[HOOK:HOOK+8]!=expected:raise ValueError('Loaded LIBSD sceSdInit return differs')
        if before[SCRATCH:SCRATCH+0x3000]!=struct.pack('<I',0x400D)*3072:raise ValueError('Diagnostic scratch initial fill differs')
        trampoline=struct.pack('<2I',0x08000000|(CODE>>2),0)
        write(iop_pointer+SCRATCH,bytes(0x3000));write(iop_pointer+CODE,hook());write(iop_pointer+HOOK,trampoline)
        # Restore the original EE caller bytes, allowing its subsequent module request.
        report=inspect(original)
        seg=next(p for p in report['load_segments'] if p['virtual_address']<=address< p['virtual_address']+p['file_size'])
        offset=seg['file_offset']+address-seg['virtual_address']
        # The stopped EE PC is at +24. Redirect it through the restored setup first.
        write(ee_pointer+address,original[offset:offset+24]+struct.pack("<2I",0x08000000|(address>>2),0))
        time.sleep(.05)
        write(ee_pointer+address+24,original[offset+24:offset+32])
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(iop_pointer+SCRATCH,4)==struct.pack('<I',1):break
            time.sleep(.02)
        else:raise TimeoutError('sceSdInit return not reached')
        time.sleep(.1)
        ram=reader.read(iop_pointer,2*1024*1024);publication=ram[SCRATCH:SCRATCH+0x1010]
        if reader.read(iop_pointer+SCRATCH,0x1010)!=publication:raise ValueError('SPU2 publication changed')
        if ram[HOOK:HOOK+8]!=trampoline or ram[CODE:CODE+len(hook())]!=hook():raise ValueError('IOP hook changed')
        ee=reader.read(ee_pointer,32*1024*1024)
        (folder/'spu2-initialized-iop.bin').write_bytes(ram);(folder/'spu2-initialized-ee.bin').write_bytes(ee)
        with args.disc.open('rb') as disc_stream:disc_sha=hashlib.file_digest(disc_stream,'sha256').hexdigest()
        fields=dict(zip(REGISTERS,struct.unpack_from('<1024I',publication,16)))
        if fields[0x7C0]!=0xC032:raise ValueError('Initialized register publication differs')
        metadata={'capture':'terminal first LIBSD sceSdInit return before JR','result':struct.unpack_from('<I',publication,4)[0],
                  'return_address':struct.unpack_from('<I',publication,8)[0],'stack':struct.unpack_from('<I',publication,12)[0],
                  'registers':{hex(p):v for p,v in fields.items()},
                  'source_sha256':hashlib.sha256(original).hexdigest(),'disc_sha256':disc_sha,
                  'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),
                  'hook_sha256':hashlib.sha256(hook()).hexdigest(),
                  'limitations':'Terminal before original JR; records actual return V0 and halfword register reads. Dynamic registers may change during snapshot; compare only retained initialization configuration.'}
        (folder/'spu2-initialized.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        print('sceSdInit actual return:',metadata['result'],'ATTR:',hex(fields[0x19A]),hex(fields[0x59A]),'SPDIF:',hex(fields[0x7C0]))
    finally:kernel.CloseHandle(handle)
