"""Terminal local capture of the reached CRI low-memory argument read; no success injected."""
import ctypes,hashlib,json,struct,time
from pathlib import Path
BASE=0xB7F30;HOOK=BASE+0x1394;SCRATCH=0x1D0000;CODE=SCRATCH+0x200
def hook():
    words=[0x3C18001D,0xAF020004,0xAF040008,0xAF05000C,0xAF060010,
           0x8C440000,0,0xAF040014,0x8C590004,0,0xAF190018,0x8C590008,0,0xAF19001C,
           0x24190001,0xAF190000,0x1000FFFF,0]
    return struct.pack('<18I',*words)

def loading_hook():
    words=[0x27BDFFF0,0xAFB80000,0xAFB90004,0x3C18001D,0x24190001,0xAF1F0004,0xAF190000,
           0x1000FFFF,0,0x8FB80000,0x8FB90004,0x27BD0010,0x03E00008,0x27BD0020]
    return struct.pack('<14I',*words)

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
        initial=0x82C30+0x4694;initial_code=SCRATCH+0x100
        if before[initial:initial+8]!=struct.pack('<2I',0x03E00008,0x27BD0020):raise ValueError('LIBSD loading checkpoint differs')
        if before[SCRATCH:SCRATCH+0x400]!=struct.pack('<I',0x400D)*256:raise ValueError('Diagnostic scratch initial fill differs')
        initial_trampoline=struct.pack('<2I',0x08000000|(initial_code>>2),0)
        write(iop_pointer+SCRATCH,bytes(0x400));write(iop_pointer+initial_code,loading_hook());write(iop_pointer+initial,initial_trampoline)
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
        else:raise TimeoutError('LIBSD return did not load CRI')
        actual_return=struct.unpack('<I',reader.read(iop_pointer+SCRATCH+4,4))[0]
        actual_base=actual_return-0x2C;actual_hook=actual_base+0x1394
        expected=struct.pack('<2I',0x8C440000,0x8C450004)
        if reader.read(iop_pointer+actual_hook,8)!=expected:raise ValueError('Reached CRI argument read differs')
        trampoline=struct.pack('<2I',0x08000000|(CODE>>2),0)
        write(iop_pointer+SCRATCH,bytes(0x20));write(iop_pointer+CODE,hook());write(iop_pointer+actual_hook,trampoline)
        write(iop_pointer+initial_code+7*4,struct.pack('<2I',0x08000000|((initial_code+9*4)>>2),0))
        while time.monotonic()<deadline:
            if reader.read(iop_pointer+SCRATCH,4)==struct.pack('<I',1):break
            time.sleep(.02)
        else:raise TimeoutError('CRI low-memory read not reached')
        time.sleep(.1)
        ram=reader.read(iop_pointer,2*1024*1024);publication=ram[SCRATCH:SCRATCH+0x20]
        if reader.read(iop_pointer+SCRATCH,0x20)!=publication:raise ValueError('SPU2 publication changed')
        if ram[actual_hook:actual_hook+8]!=trampoline or ram[CODE:CODE+len(hook())]!=hook():raise ValueError('IOP hook changed')
        ee=reader.read(ee_pointer,32*1024*1024)
        (folder/'cri-low-read-iop.bin').write_bytes(ram);(folder/'cri-low-read-ee.bin').write_bytes(ee)
        with args.disc.open('rb') as disc_stream:disc_sha=hashlib.file_digest(disc_stream,'sha256').hexdigest()
        reached_address=struct.unpack_from('<I',publication,4)[0]
        words=list(struct.unpack_from('<3I',publication,20))
        if reached_address!=0 or words!=list(struct.unpack_from('<3I',ram,0)):raise ValueError('Reached low-memory operands differ')
        metadata={'hook_address':actual_hook,'module_base':actual_base,'capture':'terminal actual CRI low-memory argument read','address':reached_address,'words':words,
                  'arguments':list(struct.unpack_from('<3I',publication,8)),
                  'source_sha256':hashlib.sha256(original).hexdigest(),'disc_sha256':disc_sha,
                  'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),
                  'hook_sha256':hashlib.sha256(hook()).hexdigest(),
                  'limitations':'Original first LW executed once; two following operands published without resuming worker. No general kernel memory ownership established.'}
        (folder/'cri-low-read.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        print('CRI actual low read:',hex(reached_address),words,'arguments:',metadata['arguments'])
    finally:kernel.CloseHandle(handle)
