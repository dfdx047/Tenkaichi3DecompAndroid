"""Observe original CRI higher-priority StartThread and first WaitSema, no return injected."""
import ctypes,hashlib,json,struct,time
from pathlib import Path
BASE=0xB7F30;START=BASE+0xC5F0;WAIT=BASE+0xC63C;SCRATCH=0x1D0000;CODE=SCRATCH+0x200

def pack(words):return struct.pack('<'+str(len(words))+'I',*words)
def start_hook(target,terminal=True):
    words=[0x3C18001D,0xAF040004,0xAF050008,0xAF1F000C,0xAF1C0010,0xAF1D0014,
        0x0C000000|(target>>2),0,0x3C18001D,0xAF020028,0xAF1C002C,0xAF1D0030,
        0x24190001,0xAF190000,0x1000FFFF,0]
    if not terminal:words[12:]=[0x8F1F000C,0,0x03E00008,0]
    return pack(words)
def wait_hook(target):
    return pack([0x3C18001D,0xAF040018,0xAF1F001C,0xAF1C0020,0xAF1D0024,
        0x08000000|(target>>2),0x24000008])
def dma_hook():
    return pack([0x3C18001D,0x8E620000,0,0xAF020034,0xAF130038,
        0xAF04003C,0xAF050040,0xAF060044,0xAF070048,0xAF1F004C,
        0x24190001,0xAF190000,0x1000FFFF,0])
def adma_hook():
    # Publish the original requested halfword store without performing it.
    return pack([0x3C18001D,0xAF020034,0xAF100038,0xAF04003C,0xAF050040,
        0xAF060044,0xAF070048,0xAF1F004C,0x24190001,0xAF190000,0x1000FFFF,0])
def program_hook():
    # Execute displaced delay instruction before publishing the preceding CHCR load.
    return pack([0x02801021,0xAFA30010,0x3C18001D,0xAF030034,0xAF130038,
        0xAF11003C,0xAF140040,0xAF1F0044,0x3C19BF80,0x8F2A10C0,0,
        0xAF0A0048,0x8F2A10C4,0,0xAF0A004C,0x8F2A1014,0,0xAF0A0050,
        0x24190001,0xAF190000,0x1000FFFF,0])
def transfer_hook(target):
    # Preserve the original stack/arguments; call the linked export once and record its return.
    return pack([0x3C18001D,0xAF040034,0xAF050038,0xAF06003C,0xAF070040,
        0xAF1F0044,0xAF1C0048,0xAF1D004C,0x0C000000|(target>>2),0,
        0x3C18001D,0xAF020050,0xAF1C0054,0xAF1D0058,
        0x24190001,0xAF190000,0x1000FFFF,0])
def irq_hook():
    # Terminal observer before the original handler body; no callback return is supplied.
    return pack([0x3C18001D,0xAF040034,0xAF1F0038,0xAF1C003C,0xAF1D0040,
        0x3C19BF80,0x8F2A10C0,0,0xAF0A0044,0x8F2A10C4,0,0xAF0A0048,
        0x8F2A10C8,0,0xAF0A004C,0x8C8A0000,0,0xAF0A0050,
        0x24190001,0xAF190000,0x1000FFFF,0])
def volume_hook():
    # Execute every original parameter store. Publish only the two reached input volumes.
    return pack([0xA4450000,0x3059FFFF,0x2418076C,0x13380004,0,
        0x2418076E,0x17380017,0,0x3C18001D,0x8F190034,0,0x00194100,
        0x25190038,0x0319C821,0xAF220000,0xAF250004,0xAF240008,0xAF3F000C,
        0x8F190034,0,0x27390001,0xAF190034,0x24180002,0x17380006,0,
        0x3C18001D,0x24190001,0xAF190000,0x1000FFFF,0,0x03E00008,0])
def capture(reader,ee_pointer,iop_rva,original,folder,address,args):
    from capture_ee import FRESH_BIND_STUB,texture_stub
    k=reader.kernel;k.OpenProcess.restype=ctypes.c_void_p;handle=k.OpenProcess(0x438,False,reader.pid)
    if not handle:raise ctypes.WinError(ctypes.get_last_error())
    k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    def write(p,b):
        count=ctypes.c_size_t();buffer=ctypes.create_string_buffer(b)
        if not k.WriteProcessMemory(handle,p,buffer,len(b),ctypes.byref(count)) or count.value!=len(b):raise ctypes.WinError(ctypes.get_last_error())
    try:
        ip,=struct.unpack('<Q',reader.read(reader.base+iop_rva,8));before=reader.read(ip,2*1024*1024)
        targets=[]
        for hook,ordinal in ((START,6),(WAIT,8)):
            first,delay=struct.unpack_from('<2I',before,hook)
            if first>>26!=2 or delay!=0x24000000|ordinal:raise ValueError('CRI original import differs')
            targets.append((first&0x3FFFFFF)<<2)
        if before[SCRATCH:SCRATCH+0x400]!=pack([0x400D]*256):raise ValueError('CRI observer scratch fill differs')
        if struct.unpack_from('<2I',before,BASE+0x5890)!=(0x27BDFFD8,0xAFB00010):raise ValueError('CRI child entry differs')
        write(ip+SCRATCH,bytes(0x400));write(ip+CODE,start_hook(targets[0],args.capture_cri_thread=='start'));write(ip+CODE+0x100,wait_hook(targets[1]))
        write(ip+START,pack([0x08000000|(CODE>>2),0]));write(ip+WAIT,pack([0x08000000|((CODE+0x100)>>2),0]))
        if args.capture_cri_thread in ('dma','adma','program','transfer','irq','volume'):
            mode=args.capture_cri_thread
            if mode=='transfer':
                dma=BASE+0xC434;first,delay=struct.unpack_from('<2I',before,dma)
                if first>>26!=2 or delay!=0x24000012:raise ValueError('Original LIBSD transfer import differs')
                targets.append((first&0x3FFFFFF)<<2);observer=transfer_hook(targets[-1])
            else:
                dma=0x82C30+{'dma':0x2250,'adma':0x2DB8,'program':0x2E38,'irq':0x27E8,'volume':0x36E4}[mode]
                expected={'dma':(0x8E620000,0x3C030100),'adma':(0xA6020000,0x3C02BF80),'program':(0x02801021,0xAFA30010),'irq':(0x27BDFFD8,0xAFB20020),'volume':(0x03E00008,0xA4450000)}[mode]
                if struct.unpack_from('<2I',before,dma)!=expected:raise ValueError('Original LIBSD DMA instruction differs')
                observer={'dma':dma_hook,'adma':adma_hook,'program':program_hook,'irq':irq_hook,'volume':volume_hook}[mode]()
            write(ip+CODE+0x140,observer);write(ip+dma,pack([0x08000000|((CODE+0x140)>>2),0]))
        # Resume the existing displaced original EE prologue, not a replacement result.
        stub=texture_stub('rpc-cri-third-before');halt=FRESH_BIND_STUB+len(stub)-24;fallback=FRESH_BIND_STUB+len(stub)-16
        if reader.read(ee_pointer+FRESH_BIND_STUB,len(stub))!=stub:raise ValueError('CRI EE observer differs')
        write(ee_pointer+halt,pack([0x08000000|(fallback>>2),0]))
        deadline=time.monotonic()+args.timeout
        while time.monotonic()<deadline:
            if reader.read(ip+SCRATCH,4)==pack([1]):break
            time.sleep(.02)
        else:raise TimeoutError('Selected original CRI/LIBSD boundary not reached')
        ram=reader.read(ip,2*1024*1024);publication=ram[SCRATCH:SCRATCH+{'start':0x34,'dma':0x50,'adma':0x50,'program':0x54,'transfer':0x5C,'irq':0x54,'volume':0x58}[args.capture_cri_thread]]
        if reader.read(ip+SCRATCH,len(publication))!=publication:raise ValueError('CRI thread publication changed')
        ee=reader.read(ee_pointer,32*1024*1024)
        (folder/'cri-thread-iop.bin').write_bytes(ram);(folder/'cri-thread-ee.bin').write_bytes(ee)
        values=list(struct.unpack('<'+str(len(publication)//4)+'I',publication))
        meta={'source_elf_sha256':hashlib.sha256(original).hexdigest(),'pcsx2_sha256':hashlib.sha256(args.pcsx2.read_bytes()).hexdigest(),'bios_sha256':hashlib.sha256(args.bios.read_bytes()).hexdigest(),
              'mode':args.capture_cri_thread,'values':values,'targets':targets,'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),'limitations':'Original StartThread and first WaitSema run; terminal selected instruction/API boundary. No continuous scheduling or audio equivalence.'}
        with args.disc.open('rb') as f:meta['disc_sha256']=hashlib.file_digest(f,'sha256').hexdigest()
        (folder/'cri-thread.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8');print('Original CRI/LIBSD publication:',[hex(x) for x in values])
    finally:k.CloseHandle(handle)
