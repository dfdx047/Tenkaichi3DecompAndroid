"""Observe absent-card traffic/control; stop before actual secondary SIF DMA."""
import ctypes,hashlib,json,struct,time
from capture_sio2_dma import BASE,SCRATCH,CODE,pack
IMPORT=BASE+0xE30

def hook(target):
    w=[0x3C18001D,0xAF040004,0xAF050008,0xAF06000C,0xAF1F001C,0xAF1C0020,0xAF1D0024]
    for reg,offset in ((0x1540,40),(0x1544,44),(0x1548,48),(0x1550,52),(0x1554,56),(0x1558,60)):
        w += [0x3C19BF80,0x8F390000|reg,0,0xAF190000|offset]
    w += [0x24190001,0xAF190000,0x1000FFFF,0]
    return pack(w)

def control_hook():
    w=[0x3C02BF80,0x34428268,0xAC440000,0x3C18001D,0x8F190054,0,0x2F230020,0,0,
       0x0019C8C0,0x0338C821,0xAF240800,0x8C430000,0,0xAF230804,
       0x8F190054,0,0x27390001,0xAF190054,0x03E00008,0]
    w[7]=(4<<26)|(3<<21)|(19-7-1) # At capacity, preserve the real store and return.
    return pack(w)

def send_hook(target):
    # Tail-call actual START after recording the owned transmit buffer's first words.
    w=[0x2418000B,0,0,0x3C18001D,0x8F190058,0,0x2F230040,0,0,
       0x0019C900,0x0338C821,0x3C03BF80,0x8C631540,0,0x8C620000,0,0xAF220900,
       0x8C620004,0,0xAF220904,0x8E420028,0,0xAF220908,
       0x8F190058,0,0x27390001,0xAF190058,0x08000000|(target>>2),0]
    w[1]=(5<<26)|(4<<21)|(24<<16)|(27-1-1)
    w[7]=(4<<26)|(3<<21)|(27-7-1)
    return pack(w)

def count_hook(target):
    return pack([0x3C18001D,0x8F190060,0,0x27390001,0xAF190060,0xAF04006C,0x08000000|(target>>2),0])

def fifo_hook():
    w=[0x3C01BF81,0xA0248260,0x3C18001D,0x3C030005,0x8C63CF08,0,0x8C630028,0,
       0x3C19000C,0x37390342,0,0,0x8F190064,0,0x2F230060,0,0,
       0x03381821,0xA0640E00,0x27390001,0xAF190064,0x03E00008,0]
    w[10]=(5<<26)|(3<<21)|(25<<16)|(21-10-1)
    w[15]=(4<<26)|(3<<21)|(21-15-1)
    return pack(w)

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
        server=struct.unpack('<I',reader.read(ee_pointer+0x3A6FA4,4))[0];mcbase=server-0x36F8;import_address=mcbase+0x1064
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024)
        first,delay=struct.unpack_from('<2I',before,import_address)
        if first>>26!=2 or delay!=0x24000007 or before[SCRATCH:SCRATCH+0x1000]!=pack([0x400D]*1024):raise ValueError('START import/scratch differs')
        target=(first&0x3FFFFFF)<<2
        write(ip+SCRATCH,bytes(0x400));write(ip+CODE,hook(target));write(ip+import_address,pack([0x08000000|(CODE>>2),0]))
        if before[BASE:BASE+20]!=pack([0x3C02BF80,0x34428268,0xAC440000,0x03E00008,0]):raise ValueError('Control setter differs')
        write(ip+BASE,pack([0x08000000|((CODE+0x200)>>2),0]));write(ip+CODE+0x200,control_hook());write(ip+SCRATCH+0x54,pack([0]));write(ip+SCRATCH+0x800,bytes(0x800))
        startword,startdelay=struct.unpack_from('<2I',before,BASE+0xE30)
        if startword>>26!=2 or startdelay!=0x24000020:raise ValueError('START import differs')
        starttarget=(startword&0x3FFFFFF)<<2
        write(ip+BASE+0xE30,pack([0x08000000|((CODE+0x400)>>2),0]));write(ip+CODE+0x400,send_hook(starttarget));write(ip+SCRATCH+88,pack([0]))
        delayword,delaytag=struct.unpack_from('<2I',before,0x4D030+0x11B40)
        if delayword>>26!=2 or delaytag!=0x24000021:raise ValueError('Delay import differs')
        delaytarget=(delayword&0x3FFFFFF)<<2
        write(ip+0x4D030+0x11B40,pack([0x08000000|((CODE+0x600)>>2),0]));write(ip+CODE+0x600,count_hook(delaytarget))
        if before[BASE+0x144:BASE+0x14C]!=pack([0x3C01BF81,0xA0248260]):raise ValueError('FIFO byte store differs')
        write(ip+BASE+0x144,pack([0x08000000|((CODE+0x680)>>2),0]));write(ip+CODE+0x680,fifo_hook());write(ip+SCRATCH+0x60,bytes(16))
        report=inspect(original);seg=next(p for p in report['load_segments'] if p['virtual_address']<=address<p['virtual_address']+p['file_size'])
        off=seg['file_offset']+address-seg['virtual_address'];write(ee_pointer+address,original[off:off+8])
        loop=FRESH_BIND_STUB+len(texture_stub('rpc-mc-query-before'))-8;write(ee_pointer+loop,pack([0x08000000|(address>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:
            (folder/'flow-timeout-iop.bin').write_bytes(reader.read(ip,2*1024*1024))
            raise TimeoutError('Secondary SIF submission not reached')
        ram=reader.read(ip,2*1024*1024);values=struct.unpack_from('<19I',ram,SCRATCH)
        if values[2]!=1 or reader.read(ip+SCRATCH,76)!=ram[SCRATCH:SCRATCH+76]:raise ValueError('START publication changed')
        count=struct.unpack_from('<I',ram,SCRATCH+0x54)[0]
        if count>32:raise ValueError('Control log overflow')
        controls=[list(struct.unpack_from('<2I',ram,SCRATCH+0x800+n*8)) for n in range(count)]
        send_count=struct.unpack_from('<I',ram,SCRATCH+88)[0]
        if send_count>64:raise ValueError('Send trace overflow')
        sends=[list(struct.unpack_from('<3I',ram,SCRATCH+0x900+n*16)) for n in range(send_count)]
        fifo_count=struct.unpack_from('<I',ram,SCRATCH+0x64)[0]
        if fifo_count>96:raise ValueError('FIFO trace overflow')
        fifo=list(ram[SCRATCH+0xE00:SCRATCH+0xE00+fifo_count])
        delay_count=struct.unpack_from('<I',ram,SCRATCH+0x60)[0]
        descriptor=list(struct.unpack_from('<4I',ram,values[1]))
        if reader.read(ip+SCRATCH,0x1000)!=ram[SCRATCH:SCRATCH+0x1000]:raise ValueError('Flow publication changed')
        (folder/'sio2-flow-iop.bin').write_bytes(ram)
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),'device_profile':'both memory-card slots disabled','final_dma_registers':list(values[10:16]),'controls':controls,'fifo':fifo,'delay_count':delay_count,'delay_argument':struct.unpack_from('<I',ram,SCRATCH+0x6C)[0],'delay_target':delaytarget,'sends':sends,'start_target':starttarget,'descriptor_pointer':values[1],'descriptor':descriptor,'import_address':import_address,'target':target,'iop_sha256':hashlib.sha256(ram).hexdigest(),'limitations':'Actual control stores/readbacks and actual pending secondary SIF descriptor; SIF submission stopped before transport.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'sio2-flow.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(meta,indent=2))
    finally:k.CloseHandle(handle)
