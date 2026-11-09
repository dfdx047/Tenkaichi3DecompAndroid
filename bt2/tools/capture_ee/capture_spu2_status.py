"""Terminal local capture of the first reached LIBSD status read; no success injected."""
import ctypes,hashlib,json,struct,time
from pathlib import Path
BASE=0x82C30;HOOK=BASE+0x1F7C;SCRATCH=0x1D0000;CODE=SCRATCH+0x100
REGISTERS=[0x7C0,0x7C6,0x7C8,0x7CA,0x760,0x762,0x788,0x78A,0x1B0,0x5B0,0x19A,0x59A,0x744]
def hook():
    # Target LHU uses V0 for address/result. T8/T9 are scratch at this terminal stop.
    words=[0x3C18001D,0xAF020004,0x94420000,0,0xAF020008]
    for index,offset in enumerate(REGISTERS):
        words += [0x3C19BF90,0x97390000|offset,0,0xAF190000|(12+index*4)]
    words += [0x24190001,0xAF190000,0x1000FFFF,0]
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
        expected=struct.pack('<2I',0x94420000,0x08000000|((BASE+0x1FAC)>>2))
        if before[HOOK:HOOK+8]!=expected:raise ValueError('Loaded LIBSD first status read differs')
        if before[SCRATCH:SCRATCH+0x200]!=struct.pack('<I',0x400D)*128:raise ValueError('Diagnostic scratch initial fill differs')
        trampoline=struct.pack('<2I',0x08000000|(CODE>>2),0)
        write(iop_pointer+SCRATCH,bytes(0x200));write(iop_pointer+CODE,hook());write(iop_pointer+HOOK,trampoline)
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
        else:raise TimeoutError('SPU2 status read not reached')
        time.sleep(.1)
        ram=reader.read(iop_pointer,2*1024*1024);publication=ram[SCRATCH:SCRATCH+0x48]
        if reader.read(iop_pointer+SCRATCH,0x48)!=publication:raise ValueError('SPU2 publication changed')
        if ram[HOOK:HOOK+8]!=trampoline or ram[CODE:CODE+len(hook())]!=hook():raise ValueError('IOP hook changed')
        ee=reader.read(ee_pointer,32*1024*1024)
        (folder/'spu2-status-iop.bin').write_bytes(ram);(folder/'spu2-status-ee.bin').write_bytes(ee)
        with args.disc.open('rb') as disc_stream:disc_sha=hashlib.file_digest(disc_stream,'sha256').hexdigest()
        if struct.unpack_from('<I',publication,4)[0]!=0xBF900344:raise ValueError('Reached status address differs')
        fields=dict(zip(REGISTERS,struct.unpack_from('<'+str(len(REGISTERS))+'I',publication,12)))
        metadata={'capture':'terminal first LIBSD status read after SOUNDS return','address':struct.unpack_from('<I',publication,4)[0],
                  'value':struct.unpack_from('<I',publication,8)[0],'registers':{hex(p):v for p,v in fields.items()},
                  'source_sha256':hashlib.sha256(original).hexdigest(),'disc_sha256':disc_sha,
                  'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),
                  'hook_sha256':hashlib.sha256(hook()).hexdigest(),
                  'limitations':'One actual LHU executed once; worker halted before its original following branch. Other workers live; no timing/transfer completion or full initialization validated.'}
        (folder/'spu2-status.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        print('First SPU2 status:',hex(metadata['address']),hex(metadata['value']),metadata['registers'])
    finally:kernel.CloseHandle(handle)
