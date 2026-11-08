"""Capture actual first CDVDFSV metadata DMA submission with an isolated local interpreter."""
import ctypes,hashlib,json,struct,time
BASE=0x3DD30;SCRATCH=0x1D0000;SUBMIT=SCRATCH+0x300

def pack(words):return struct.pack('<'+str(len(words))+'I',*words)
def submit_hook(target):
    # Log all calls and retain the reached 36-byte payload without redirecting it.
    w=[0x3C18001D,0xAF1F000C,0x8F190000,0,0x00194080,0x01194021,0x00084080,0x01084021,0x27080040,0xAD050010]
    for n in range(4):w += [0x8C890000|n*4,0,0xAD090000|n*4]
    w += [0x27390001,0xAF190000,0x8C880004,0,0x3C09003A,0x35298600,0x15090000,0]
    skip=len(w)-2
    w += [0x8C880000,0]
    for n in range(9):w += [0x8D090000|n*4,0,0xAF090800|n*4]
    tail=len(w);w[skip]|=tail-skip-1
    w += [0x0C000000|(target>>2),0,0x3C18001D,0xAF020010,0x24190001,0xAF190018,0x1000FFFF,0]
    return pack(w)


def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from inspect_elf import inspect
    from capture_ee import FRESH_BIND_STUB,texture_stub
    k=reader.kernel;k.OpenProcess.restype=ctypes.c_void_p;handle=k.OpenProcess(0x438,False,reader.pid)
    if not handle:raise ctypes.WinError(ctypes.get_last_error())
    k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,b):
        count=ctypes.c_size_t();buffer=ctypes.create_string_buffer(b)
        if not k.WriteProcessMemory(handle,p,buffer,len(b),ctypes.byref(count)) or count.value!=len(b):raise ctypes.WinError(ctypes.get_last_error())
    try:
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024)
        if before[SCRATCH:SCRATCH+0x1000]!=pack([0x400D]*1024):raise ValueError('Diagnostic scratch initial fill differs')
        targets=[]
        for off,ordinal in ((0x4A3C,7),(0x4A44,8)):
            first,delay=struct.unpack_from('<2I',before,BASE+off)
            if first>>26!=2 or delay!=0x24000000|ordinal:raise ValueError('Loaded CDVDFSV import stub differs')
            targets.append((first&0x3FFFFFF)<<2)
        # Check actual loaded descriptor and handler belong to the captured local module.
        if struct.unpack_from('<I',before,0x433D0)[0]!=0x80000597:raise ValueError('CDVDFSV service ownership differs')
        write(ip+SCRATCH,bytes(0x1000));write(ip+SUBMIT,submit_hook(targets[0]))
        write(ip+BASE+0x4A3C,pack([0x08000000|(SUBMIT>>2),0]));# Original poll stub is untouched; this capture terminates at submit return.
        report=inspect(original);segment=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=segment['file_offset']+address-segment['virtual_address'];write(ee_pointer+address,original[off:off+8])
        stub=texture_stub('rpc-disc-command-before');loop=FRESH_BIND_STUB+len(stub)-8
        write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH+24,4)==pack([1]):break
            time.sleep(.02)
        else:
            (folder/'disc-dma-timeout-iop.bin').write_bytes(reader.read(ip,2*1024*1024))
            raise TimeoutError('First CDVDFSV DMA submission return not reached')
        ram=reader.read(ip,2*1024*1024);pub=ram[SCRATCH:SCRATCH+0xC4];ee=reader.read(ee_pointer,32*1024*1024)
        count=struct.unpack_from('<I',pub)[0]
        if count!=1:raise ValueError('Unexpected first transfer submission count')
        rows=[struct.unpack_from('<5I',ram,SCRATCH+64+20*n) for n in range(count)]
        matching=[r for r in rows if r[1]==0x3A8600]
        if len(matching)!=1:raise ValueError('Metadata destination transfer missing/ambiguous')
        descriptor=matching[0][:4];source=ram[SCRATCH+0x800:SCRATCH+0x824]
        if descriptor!=(BASE+0x6498,0x3A8600,36,0) or matching[0][4]!=1:raise ValueError('First CDVDFSV transfer descriptor differs')
        # This terminal observes submission only; independent after-return capture verifies the result.
        (folder/'disc-dma-diagnostic-iop.bin').write_bytes(ram);(folder/'disc-dma-diagnostic-ee.bin').write_bytes(ee)
        if ee[0x3A8600:0x3A8624]!=source or reader.read(ee_pointer+0x3A8600,36)!=source:raise ValueError('First EE destination differs or changed')
        if reader.read(ip+SCRATCH,len(pub))!=pub:raise ValueError('DMA publication changed')
        (folder/'disc-dma-iop.bin').write_bytes(ram);(folder/'disc-dma-ee.bin').write_bytes(ee)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'descriptor':list(descriptor),'count':1,'token':struct.unpack_from('<I',pub,16)[0],'polls':None,'poll_return':None,'targets':targets,'source_sha256':hashlib.sha256(source).hexdigest(),'diagnostic_source_redirect':None,'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),'limitations':'Original first submission executed without source redirection. Source retained at submission; terminal immediately after submit return. Poll and handler completion are not observed here. No repeated DMA, controller traffic or physical timing equivalence.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'disc-dma.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print('First reverse DMA verified:',descriptor,'polls:',meta['polls'],'destination matches source.')
    finally:k.CloseHandle(handle)
