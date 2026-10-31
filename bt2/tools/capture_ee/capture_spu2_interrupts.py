"""Capture three real linked LIBSD DisableIntr returns with retained caller context."""
import ctypes,hashlib,json,struct,time
from pathlib import Path
BASE=0x82C30;HOOK=BASE+0x478C;SCRATCH=0x1D0000;CODE=SCRATCH+0x100
def hook(target):
    words=[0x27BDFFD0,0xAFBF002C,0xAFA40000,0xAFA50004,0x0C000000|(target>>2),0,
           0xAFB80008,0xAFB9000C,0xAFA30010,0x3C18001D,0x8F190000,0,
           0x0019C900,0x27390010,0x0319C821,0x8FA30000,0,0xAF230000,
           0x8FA30004,0,0xAF230004,0xAF220008,0x8C630000,0,0xAF23000C,
           0x8F190000,0,0x27390001,0xAF190000,0x24030003,0x17230003,0,0x1000FFFF,0,
           0x8FB80008,0x8FB9000C,0x8FA30010,0x8FBF002C,0x27BD0030,0x03E00008,0]
    return struct.pack('<'+str(len(words))+'I',*words)

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
        linked,ordinal=struct.unpack_from('<2I',before,HOOK)
        if linked>>26!=2 or ordinal!=0x24000007:raise ValueError('Linked LIBSD DisableIntr stub differs')
        target=(linked&0x3FFFFFF)<<2
        expected=struct.pack('<2I',linked,ordinal)
        if before[HOOK:HOOK+8]!=expected:raise ValueError('Linked LIBSD interrupt stub differs')
        if before[SCRATCH:SCRATCH+0x200]!=struct.pack('<I',0x400D)*128:raise ValueError('Diagnostic scratch initial fill differs')
        trampoline=struct.pack('<2I',0x08000000|(CODE>>2),0)
        write(iop_pointer+SCRATCH,bytes(0x200));write(iop_pointer+CODE,hook(target));write(iop_pointer+HOOK,trampoline)
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
            if reader.read(iop_pointer+SCRATCH,4)==struct.pack('<I',3):break
            time.sleep(.02)
        else:raise TimeoutError('Three LIBSD interrupt disables not reached')
        time.sleep(.1)
        ram=reader.read(iop_pointer,2*1024*1024);publication=ram[SCRATCH:SCRATCH+0x50]
        if reader.read(iop_pointer+SCRATCH,0x50)!=publication:raise ValueError('SPU2 publication changed')
        if ram[HOOK:HOOK+8]!=trampoline or ram[CODE:CODE+len(hook(target))]!=hook(target):raise ValueError('IOP hook changed')
        ee=reader.read(ee_pointer,32*1024*1024)
        (folder/'spu2-interrupt-iop.bin').write_bytes(ram);(folder/'spu2-interrupt-ee.bin').write_bytes(ee)
        with args.disc.open('rb') as disc_stream:disc_sha=hashlib.file_digest(disc_stream,'sha256').hexdigest()
        records=[dict(zip(['irq','result_pointer','return','old_mask'],struct.unpack_from('<4I',publication,16+i*16))) for i in range(3)]
        if [r['irq'] for r in records]!=[36,40,9]:raise ValueError('Reached interrupt sequence differs')
        metadata={'capture':'three real LIBSD DisableIntr returns','records':records,'target':target,
                  'source_sha256':hashlib.sha256(original).hexdigest(),'disc_sha256':disc_sha,
                  'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),
                  'hook_sha256':hashlib.sha256(hook(target)).hexdigest(),
                  'limitations':'Actual linked kernel calls; first two return with hook scratch state restored. Third call terminal stop; other workers live, timing not validated.'}
        (folder/'spu2-interrupts.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        print('LIBSD interrupt disables:',records)
    finally:kernel.CloseHandle(handle)
