"""Scoped first SIO2 FIFO reply capture in the isolated interpreter process."""
import ctypes,hashlib,json,struct,time
from pathlib import Path

BASE=0x4BF30;HOOK=BASE+0x154;SCRATCH=0x1D0000;CODE=SCRATCH+0x100

def hook(count=5):
    # Preserve scratch registers and SP; perform the original byte read exactly once.
    words=[0x27BDFFF0,0xAFA30000,0xAFA80004,0xAFA90008,
           0x3C02BF81,0x90428264,0,0x3C03001D,0x8C680000,0,
           0x00684821,0xA1220004,0x25080001,0xAC680000]
    for offset,publication in [(0x8268,0x20),(0x826C,0x24),(0x8270,0x28),(0x8274,0x2C),(0x8280,0x30),(0x10F0,0x34),(0x1570,0x38)]:
        words += [0x3C09BF81 if offset>=0x8000 else 0x3C09BF80,0x8D290000|offset,0,0xAC690000|publication]
    words += [0x2D090000|count,0x15200003,0,0x1000FFFF,0,
              0x8FA30000,0x8FA80004,0x8FA90008,0x27BD0010,0x03E00008,0]

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
        expected=struct.pack('<4I',0x3C02BF81,0x90428264,0x03E00008,0)
        if before[HOOK:HOOK+16]!=expected:raise ValueError('Loaded FIFO read helper differs')
        if before[SCRATCH:SCRATCH+0x200]!=struct.pack('<I',0x400D)*128:raise ValueError('Diagnostic scratch initial fill differs')
        trampoline=struct.pack('<2I',0x08000000|(CODE>>2),0)
        write(iop_pointer+SCRATCH,bytes(0x200));write(iop_pointer+CODE,hook(args.pad_poll_read_count));write(iop_pointer+HOOK,trampoline)
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
            if reader.read(iop_pointer+SCRATCH,4)==struct.pack('<I',args.pad_poll_read_count):break
            time.sleep(.02)
        else:raise TimeoutError('Requested FIFO replies not reached')
        time.sleep(.1)
        ram=reader.read(iop_pointer,2*1024*1024);publication=ram[SCRATCH:SCRATCH+0x3C]
        if reader.read(iop_pointer+SCRATCH,0x3C)!=publication:raise ValueError('FIFO publication changed')
        if ram[HOOK:HOOK+8]!=trampoline or ram[CODE:CODE+len(hook(args.pad_poll_read_count))]!=hook(args.pad_poll_read_count):raise ValueError('IOP hook changed')
        ee=reader.read(ee_pointer,32*1024*1024)
        (folder/'pad-poll-iop.bin').write_bytes(ram);(folder/'pad-poll-ee.bin').write_bytes(ee)
        with args.disc.open('rb') as disc_stream:disc_sha=hashlib.file_digest(disc_stream,'sha256').hexdigest()
        metadata={'capture':'first SIO2 FIFO reads after SIO2D return','read_count':args.pad_poll_read_count,'reply':list(publication[4:4+args.pad_poll_read_count]),'registers':dict(zip(['control','command_status','port_status','fifo_status','irq_status','dma_control_1','dma_control_2'],struct.unpack_from('<7I',publication,0x20))),
                  'iop_sha256':hashlib.sha256(ram).hexdigest(),'ee_sha256':hashlib.sha256(ee).hexdigest(),
                  'source_sha256':hashlib.sha256(original).hexdigest(),'hook_sha256':hashlib.sha256(hook(args.pad_poll_read_count)).hexdigest(),
                  'disc_sha256':disc_sha,
                  'limitations':'Isolated EE/IOP interpreter profile; final requested FIFO byte read then worker halted before returning it. Other IOP workers live. Timing and completion are not validated.'}
        (folder/'pad-poll.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
        print('First FIFO response:',publication[4:4+args.pad_poll_read_count].hex())
    finally:kernel.CloseHandle(handle)
