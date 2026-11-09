"""Capture actual DBCMAN DMA submission/polls with an isolated local interpreter."""
import ctypes,hashlib,json,struct,time
BASE=0x77C30;SCRATCH=0x1D0000;SUBMIT=SCRATCH+0x300;POLL=SCRATCH+0x600

def pack(words):return struct.pack('<'+str(len(words))+'I',*words)
def submit_hook(target):
    w=[0x3C18001D,0xAF040004,0xAF050008,0xAF1F000C]
    for n in range(4):w += [0x8C990000|n*4,0,0xAF190020|n*4]
    w += [0x8C990000,0] # actual source pointer
    for n in range(32):w += [0x8F280000|n*4,0,0xAF080040|n*4]
    # Immutable diagnostic source: redirect only this descriptor before real submission.
    w += [0x3C19001D,0x37390040,0xAC990000]
    w += [0x0C000000|(target>>2),0,0x3C18001D,0xAF020010,0x8F1F000C,0,0x03E00008,0]
    return pack(w)
def poll_hook(target):
    # Preserve real token/poll return; terminal only once the original poll is negative.
    w=[0x3C18001D,0xAF1F0018,0xAF04001C,0x0C000000|(target>>2),0,
       0x3C18001D,0x8F190014,0,0x27390001,0xAF190014,0xAF0200C0,
       0x04400005,0,0x8F1F0018,0,0x03E00008,0,
       0x24190001,0xAF190000,0x1000FFFF,0]
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
        for off,ordinal in ((0x21DC,7),(0x21E4,8)):
            first,delay=struct.unpack_from('<2I',before,BASE+off)
            if first>>26!=2 or delay!=0x24000000|ordinal:raise ValueError('Loaded DBCMAN import stub differs')
            targets.append((first&0x3FFFFFF)<<2)
        # Check actual loaded descriptor and handler belong to the captured local module.
        if struct.unpack_from('<I',before,BASE+0x2DE0)[0]!=0x80001300:raise ValueError('DBCMAN service ownership differs')
        write(ip+SCRATCH,bytes(0x1000));write(ip+SUBMIT,submit_hook(targets[0]));write(ip+POLL,poll_hook(targets[1]))
        write(ip+BASE+0x21DC,pack([0x08000000|(SUBMIT>>2),0]));write(ip+BASE+0x21E4,pack([0x08000000|(POLL>>2),0]))
        report=inspect(original);segment=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=segment['file_offset']+address-segment['virtual_address'];write(ee_pointer+address,original[off:off+8])
        stub=texture_stub('rpc-dbc-setup-before');loop=FRESH_BIND_STUB+len(stub)-8
        write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('First DBCMAN DMA completion poll not reached')
        ram=reader.read(ip,2*1024*1024);pub=ram[SCRATCH:SCRATCH+0xC4];ee=reader.read(ee_pointer,32*1024*1024)
        descriptor=struct.unpack_from('<4I',pub,32);source=pub[64:192]
        if descriptor!=(BASE+0x2778,0x3A27C0,128,0) or struct.unpack_from('<I',pub,8)[0]!=1:raise ValueError('First DBCMAN transfer descriptor differs')
        if struct.unpack_from('<I',pub,192)[0]!=0xFFFFFFFF:raise ValueError('Original completion poll did not return -1')
        (folder/'dbc-dma-diagnostic-iop.bin').write_bytes(ram);(folder/'dbc-dma-diagnostic-ee.bin').write_bytes(ee)
        if ee[0x3A27C0:0x3A2840]!=source or reader.read(ee_pointer+0x3A27C0,128)!=source:raise ValueError('First EE destination differs or changed')
        if reader.read(ip+SCRATCH,len(pub))!=pub:raise ValueError('DMA publication changed')
        (folder/'dbc-dma-iop.bin').write_bytes(ram);(folder/'dbc-dma-ee.bin').write_bytes(ee)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'descriptor':list(descriptor),'count':1,'token':struct.unpack_from('<I',pub,16)[0],'polls':struct.unpack_from('<I',pub,20)[0],'poll_return':0xFFFFFFFF,'targets':targets,'source_sha256':hashlib.sha256(source).hexdigest(),'diagnostic_source_redirect':SCRATCH+64,'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),'limitations':'Original submit/polls executed with descriptor source redirected to immutable diagnostic copy to isolate concurrent producer writes. Terminal at first negative poll before handler return. No repeated DMA, controller traffic or physical timing equivalence.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'dbc-dma.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print('First reverse DMA verified:',descriptor,'polls:',meta['polls'],'destination matches source.')
    finally:k.CloseHandle(handle)
