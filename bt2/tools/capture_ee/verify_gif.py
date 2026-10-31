"""Validate first GIF helper checkpoints and inspect a single PACKED A+D packet.

No transfer completion or rendering is implemented by this diagnostic validator.
"""
import hashlib,json,struct
from capture_ee import EXPECTED_SHA,MARKER,capture_trap

POINTS={
 'gif-idle':(0x3100E4,0x3100E8,'508177e956be083f4d721748bdc650f4dd641f6a457bafb2da96caebe071bb42',0,0x1FFEAF0),
 'gif-qwc':(0x310124,0x31014C,'ffe503d3590f1692a40cae19dcb60a3440c1aeb6b7e102d1548bc23a7b1432b3',15,0x1FFEAF0),
 'gif-after':(0x1002A4,0x1002AC,'3eda120ffe28afa6f7ffb63a867ae1e0f113c9d5e1963c52384e8905d1aad151',0,0x1FFEB00)}
IDLE=struct.pack('<9I',0,0,0x1FFEAF0,0,0,0,1,0x800040,0x1201)

def inspect_packet(packet):
    if len(packet)<16 or len(packet)%16:raise ValueError('GIF packet alignment/size differs.')
    tag,regs=struct.unpack_from('<QQ',packet)
    loops=tag&0x7fff
    if not loops or not tag&0x8000 or (tag>>46)&1 or (tag>>58)&3 or (tag>>60)&15!=1 or regs!=14:
        raise ValueError('Only one complete PACKED A+D tag without PRE is inspected.')
    if len(packet)!=(loops+1)*16:raise ValueError('GIF packet payload count differs.')
    addresses=[struct.unpack_from('<Q',packet,offset+8)[0] for offset in range(16,len(packet),16)]
    if any(reg>0x7f for reg in addresses):raise ValueError('GIF A+D address reserved bits differ.')
    return {'format':'single PACKED A+D','qwc':loops+1,'register_writes':loops,
            'register_addresses':addresses,'vertex_kicks':sum(reg in (4,5) for reg in addresses),
            'sha256':hashlib.sha256(packet).hexdigest(),
            'scope':'Packet metadata only; no GS register, vertex, transfer or rendering effects executed.'}

def verify(folder,environment):
    manifests={};packets={};idle=None
    for point,(start,end,prefix,value,sp) in POINTS.items():
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();address,trap,marker=capture_trap(point);p=m['patch']
        if (len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
            or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap)
            or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or ee[address:address+len(trap)]!=trap or hashlib.sha256(ee).hexdigest()!=m['capture_sha256']
            or hashlib.sha256(ee[start:end]).hexdigest()!=prefix
            or struct.unpack_from('<I',ee,MARKER)[0]!=marker
            or struct.unpack_from('<QI',ee,MARKER+8)!=(value,sp)):
            raise ValueError('GIF checkpoint publication/prefix differs: '+point)
        if any(m.get(key)!=environment[key] for key in ('pcsx2_sha256','bios_sha256','disc_sha256')):
            raise ValueError('GIF reference environment differs.')
        if point=='gif-idle':
            idle=ee[MARKER+8:MARKER+44]
            if idle!=IDLE:raise ValueError('First GIF idle/channel/enable checkpoint differs.')
        packet_address=sp+(80 if point=='gif-after' else 96)
        packets[point]=ee[packet_address:packet_address+240]
        manifests[point]=m
    packet=packets['gif-idle'];metadata=inspect_packet(packet)
    if metadata['qwc']!=15 or any(other!=packet for other in packets.values()):
        raise ValueError('First GIF packet differs across checkpoints.')
    return idle+struct.pack('<QQ',15,0)+packet,{'captures':manifests,'packet':metadata,
            'limitations':'Independent reached points; reference helper return is not cycle-accurate completion evidence. Command consumption and completion need separate runtime support.'}

SYNC_POINTS={
 'gif-sync-before':(0x310548,0x310564,'6635a4b0144b8fe1707a40f55369e859da0ef010ee54da17623bcadabc19bbc0'),
 'gif-sync-poll':(0x3105B4,0x3105D0,'05fab687bf37f8f80421796171fa580eb10fff820badf3c5716b682482b151ce'),
 'gif-vu-idle':(0x31062C,0x310630,'c1468d7e1805d24da4a96b775c7cc271d16fd8b8dc3d6d70c139ca8e97ec08c5'),
 'gif-path-after':(0x1002B0,0x1002B8,'414e7f0c663c6d8eae2eada558d753e9f3529a2c8fb9ce2e4f93dcd2a85c8678'),
 'gif-sync-after':(0x3105CC,0x3105DC,'c90a7ea86c33f10878b5b0737b091c535199e3377d0108d56fe8b1cf14e0a13a'),
}

def verify_sync(folder,environment):
    publications=[];manifests={}
    for point in ('gif-sync-before','gif-sync-poll','gif-sync-after','gif-vu-idle','gif-path-after'):
        start,end,prefix=SYNC_POINTS[point]
        path=folder/point/'capture.json';m=json.loads(path.read_text(encoding='utf-8'))
        ee=path.with_name('ee-ram.bin').read_bytes();address,trap,marker=capture_trap(point);p=m['patch']
        if (len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA
            or not m['capture_point'].startswith(point+':') or p['address']!=address or p['size']!=len(trap)
            or p['authored_words']!=list(struct.unpack('<'+str(len(trap)//4)+'I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or ee[address:address+len(trap)]!=trap or hashlib.sha256(ee).hexdigest()!=m['capture_sha256']
            or hashlib.sha256(ee[start:end]).hexdigest()!=prefix
            or struct.unpack_from('<I',ee,MARKER)[0]!=marker):raise ValueError('GIF synchronization provenance differs')
        if any(m.get(key)!=environment[key] for key in ('pcsx2_sha256','bios_sha256','disc_sha256')):
            raise ValueError('GIF synchronization environment differs')
        if point in ('gif-vu-idle','gif-path-after'):
            sp=0x1FFEAF0 if point=='gif-vu-idle' else 0x1FFEB00
            pub=ee[MARKER+8:MARKER+20]
            if struct.unpack('<QI',pub)!=(0,sp):raise ValueError('GIF idle/return publication differs')
            publications.append(pub);manifests[point]=m
            if point=='gif-path-after':
                second=ee[sp+320:sp+560]
                if inspect_packet(second)['qwc']!=15:raise ValueError('Second packet count differs')
                publications.append(second)
            continue
        pub=ee[MARKER+8:MARKER+44]
        expected={'gif-sync-before':(0,0,0x1FFEAF0,0,0x101,0,0x1FFEC40,0x40,0xE00),
                  'gif-sync-poll':(1,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0),
                  'gif-sync-after':(0x1000A000,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0)}[point]
        if struct.unpack('<9I',pub)!=expected:raise ValueError('GIF synchronization publication differs')
        publications.append(pub);manifests[point]=m
    return b''.join(publications),{'captures':manifests,'limitations':'Independent checkpoints establish first completion poll; no continuous or cycle-accurate timing. VU status is an explicit idle checkpoint; no VU execution or timing established. The second source packet is independently captured.'}
