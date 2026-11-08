"""Terminal capture of a reached original CDVDFSV import call/return."""
import ctypes,hashlib,json,struct,time
BASE=0x3DD30;SCRATCH=0x1D0000;CODE=SCRATCH+0x100
POINTS={'search-afs2':(0x4BB0,84),'search-op':(0x4BB0,84),'search-ed':(0x4BB0,84),'search-logo':(0x4BB0,84),'search-afs':(0x4BB0,84),'read-final':(0x4A3C,7),'read-completion':(0x4A3C,7),'read-tail':(0x4A3C,7),'sync-after':(0x4AF0,11),'read-disable':(0x48A4,7),'read-alarm-cancel':(0x4988,37),'read-alarm':(0x4980,35),'read-transfer-poll':(0x4A44,8),'read-transfer':(0x4A3C,7),'read-callback-after':(0xB00,0),'read-callback':(0xB00,0),'sc-media':(0x4B68,50),'sc-read2':(0x4B68,50),'sc-key':(0x4B68,50),'sc-dual':(0x4B68,50),'sc-lsn':(0x4B68,50),'sc-timeout':(0x4B68,50),'read-rv':(0x4B80,63),'read':(0x4B78,62),'sync':(0x4AF0,11),'error':(0x4AD8,8)}
def pack(w):return struct.pack('<'+str(len(w))+'I',*w)
def hook(target,kind):
    if kind=='read-callback-after':return pack([0x3C18001D,0xAF1F000C,0xAF1D001C,0xAF040020,0xAF050024,0xAF060028,0xAF07002C,0x0C000000|((CODE+0x100)>>2),0,0x3C18001D,0xAF020008,0x24190001,0xAF190000,0x1000FFFF,0])
    if kind=='read-callback':return pack([0x3C18001D,0xAF1F000C,0xAF1D001C,0xAF040020,0xAF050024,0xAF060028,0xAF07002C,0x24190001,0xAF190000,0x1000FFFF,0])
    w=[]
    if kind.startswith('sc-'):w=[0x24180000|({'sc-media':0xFFF4,'sc-read2':0xFFFD,'sc-key':0xFFFC,'sc-dual':0xFFEA,'sc-lsn':0xFFE9,'sc-timeout':0xFFF1}[kind]),0x14980000,0]
    if kind=='read-completion':w=[0x8C990004,0x3C180036,0x371894C0,0x17380000,0]
    if kind.startswith('search-'):
        value={'search-afs':0x46412E31,'search-afs2':0x46412E32,'search-op':0x46532E50,'search-ed':0x46532E44,'search-logo':0x4446532E}[kind];w=[0x8CB8000C,0x3C190000|(value>>16),0x37390000|(value&65535),0x17190000,0]
    if kind=='read-final':w=[0x8C980000,0,0x8F180000,0x24191000,0x17190000,0]
    if kind=='read-tail':w=[0x24180001,0x14B80000,0]
    if kind=='sync-after':w=[0x24180005,0x14980000,0]
    w += [0x3C18001D,0xAF1F000C,0xAF1D001C]
    for n in range(4):w += [0xAF040020|(n<<16)|n*4]
    w += [0x0C000000|(target>>2),0,0x3C18001D,0xAF020008,0x24190001,0xAF190000,0x1000FFFF,0]
    if kind.startswith('sc-') or kind in ('sync-after','read-tail'):w[1]|=len(w)-2;w += [0x08000000|(target>>2),0]
    if kind=='read-completion' or kind.startswith('search-'):w[3]|=len(w)-4;w += [0x08000000|(target>>2),0]
    if kind=='read-final':w[4]|=len(w)-5;w += [0x08000000|(target>>2),0]
    return pack(w)
def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from inspect_elf import inspect
    from capture_ee import FRESH_BIND_STUB,texture_stub
    kind=args.capture_disc_api;off,ordinal=POINTS[kind]
    k=reader.kernel;k.OpenProcess.restype=ctypes.c_void_p;h=k.OpenProcess(0x438,False,reader.pid)
    if not h:raise ctypes.WinError(ctypes.get_last_error())
    k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,b):
        n=ctypes.c_size_t();v=ctypes.create_string_buffer(b)
        if not k.WriteProcessMemory(h,p,v,len(b),ctypes.byref(n)) or n.value!=len(b):raise ctypes.WinError(ctypes.get_last_error())
    try:
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024);first,delay=struct.unpack_from('<2I',before,BASE+off)
        if (ordinal and (first>>26!=2 or delay!=0x24000000|ordinal)) or (not ordinal and (first,delay)!=(0x27BDFFE8,0x3C040004)) or before[SCRATCH:SCRATCH+(0x300 if kind=='read-callback-after' else 0x200)]!=pack([0x400D]*(192 if kind=='read-callback-after' else 128)):raise ValueError('API import/scratch differs')
        target=((first&0x3FFFFFF)<<2) if ordinal else BASE+off;write(ip+SCRATCH,bytes(0x300 if kind=='read-callback-after' else 0x200));
        if kind=='read-callback-after':write(ip+CODE+0x100,pack([first,delay,0x08000000|((target+8)>>2),0]))
        write(ip+CODE,hook(target,kind));write(ip+BASE+off,pack([0x08000000|(CODE>>2),0]))
        r=inspect(original);s=next(s for s in r['load_segments'] if s['virtual_address']<=address<s['virtual_address']+s['file_size']);at=s['file_offset']+address-s['virtual_address'];write(ee_pointer+address,original[at:at+8]);stub=texture_stub(args.capture_point);write(ee_pointer+FRESH_BIND_STUB+(56 if args.capture_point=='rpc-ncmd-call-sema' else len(stub)-8),pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('Disc API return not reached')
        ram=reader.read(ip,2*1024*1024);pub=ram[SCRATCH:SCRATCH+48]
        if reader.read(ip+SCRATCH,len(pub))!=pub:raise ValueError('API publication changed')
        (folder/'disc-api-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'iop_sha256':hashlib.sha256(ram).hexdigest(),'kind':kind,'target':target,'arguments':list(struct.unpack_from('<4I',pub,32)),'result':None if kind=='read-callback' else struct.unpack_from('<I',pub,8)[0],'return_pc':struct.unpack_from('<I',pub,12)[0],'limitations':'Terminal at original callback entry; no callback/request completion inferred.' if kind=='read-callback' else 'Terminal after first reached original API; no whole request completion inferred.'}
        if kind.startswith('search-'):
            ee=reader.read(ee_pointer,32*1024*1024);(folder/'disc-api-ee.bin').write_bytes(ee);meta['ee_sha256']=hashlib.sha256(ee).hexdigest()
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'disc-api.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8');print(kind,meta['arguments'],'result',meta['result'])
    finally:k.CloseHandle(h)
