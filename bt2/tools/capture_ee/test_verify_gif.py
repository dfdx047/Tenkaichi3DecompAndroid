import hashlib,json,struct,unittest
from pathlib import Path
from unittest.mock import patch
import verify_gif
from capture_ee import capture_trap,MARKER,EXPECTED_SHA

# Synthetic authored packet; no retail values used.
def packet():
    return struct.pack('<QQ',14|0x8000|(1<<60),14)+b''.join(struct.pack('<QQ',i,5 if i<2 else 0x47) for i in range(14))

class GifPacketTests(unittest.TestCase):
    def test_packet_metadata(self):
        m=verify_gif.inspect_packet(packet());self.assertEqual(m['qwc'],15)
        self.assertEqual(m['register_writes'],14);self.assertEqual(m['vertex_kicks'],2)
    def test_unsupported_and_truncated_packets(self):
        p=packet()
        for bad in (p[:-1],p[:-16],p+bytes(16),bytes(16),struct.pack('<Q',struct.unpack_from('<Q',p)[0]|(1<<58))+p[8:]):
            with self.assertRaises(ValueError):verify_gif.inspect_packet(bad)
        bad=bytearray(p);struct.pack_into('<Q',bad,24,0x100)
        with self.assertRaises(ValueError):verify_gif.inspect_packet(bad)

class GifEvidenceTests(unittest.TestCase):
    def check(self,mutation=None):
        env={'pcsx2_sha256':'pcsx2','bios_sha256':'bios','disc_sha256':'disc'};fixtures={}
        for point,(start,end,prefix,value,sp) in verify_gif.POINTS.items():
            address,trap,marker=capture_trap(point);ee=bytearray(32*1024*1024)
            ee[address:address+len(trap)]=trap;struct.pack_into('<I',ee,MARKER,marker)
            struct.pack_into('<QI',ee,MARKER+8,value,sp)
            if point=='gif-idle':ee[MARKER+8:MARKER+44]=verify_gif.IDLE
            addr=sp+(80 if point=='gif-after' else 96);ee[addr:addr+240]=packet()
            m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture',
               'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),
                        'marker_address':MARKER,'marker_value':marker}}
            if mutation:mutation(point,ee,m,addr)
            m['capture_sha256']=hashlib.sha256(ee).hexdigest();fixtures[point]=(ee,m)
        with patch.object(verify_gif,'POINTS',{
                name:(start,end,hashlib.sha256(bytes(end-start)).hexdigest(),value,sp)
                for name,(start,end,_,value,sp) in verify_gif.POINTS.items()}), \
             patch.object(Path,'read_text',lambda path,**kw:json.dumps(fixtures[path.parent.name][1])), \
             patch.object(Path,'read_bytes',lambda path:fixtures[path.parent.name][0]):
            return verify_gif.verify(Path('fixtures'),env)
    def test_bundle_identity(self):
        bundle,receipt=self.check();self.assertEqual(len(bundle),292)
        self.assertEqual(bundle[52:],packet());self.assertEqual(receipt['packet']['vertex_kicks'],2)
    def test_wrong_enable_environment_and_packet_rejected(self):
        with self.assertRaises(ValueError):self.check(lambda point,ee,m,addr:ee.__setitem__(MARKER+40,0) if point=='gif-idle' else None)
        with self.assertRaises(ValueError):self.check(lambda point,ee,m,addr:m.update(disc_sha256='wrong'))
        with self.assertRaises(ValueError):self.check(lambda point,ee,m,addr:ee.__setitem__(addr+16,0xFF) if point=='gif-qwc' else None)
    def test_wrong_prefix_and_trap_rejected(self):
        with self.assertRaises(ValueError):self.check(lambda point,ee,m,addr:ee.__setitem__(verify_gif.POINTS[point][0],0xFF))
        with self.assertRaises(ValueError):self.check(lambda point,ee,m,addr:ee.__setitem__(capture_trap(point)[0],0xFF))
    def test_traps_keep_earlier_paths_and_direct_addresses(self):
        addr,trap,_=capture_trap('gif-idle');words=struct.unpack('<26I',trap)
        self.assertEqual(words[4:6],(0x3C191000,0x3739A000))
        self.assertEqual(words[12:14],(0x3C191000,0x3739E000))
        self.assertEqual(words[18:20],(0x3C191000,0x3739F520))
        self.assertEqual(words[24],0x08000000|((addr+96)>>2));self.assertEqual(words[25],0)
        self.assertLessEqual(addr+len(trap),0x310170)
        addr,trap,_=capture_trap('gif-qwc');self.assertLessEqual(addr+len(trap),0x310170)
        addr,trap,_=capture_trap('gif-after');self.assertLessEqual(addr+len(trap),0x1002D8)

class GifSyncEvidenceTests(unittest.TestCase):
    def check_sync(self,mutation=None):
        env={'pcsx2_sha256':'pcsx2','bios_sha256':'bios','disc_sha256':'disc'};fixtures={};points={}
        values={'gif-sync-before':(0,0,0x1FFEAF0,0,0x101,0,0x1FFEC40,0x40,0xE00),
                'gif-sync-poll':(1,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0),
                'gif-sync-after':(0x1000A000,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0)}
        for point,(start,end,_) in verify_gif.SYNC_POINTS.items():
            address,trap,marker=capture_trap(point);ee=bytearray(32*1024*1024)
            ee[address:address+len(trap)]=trap;struct.pack_into('<I',ee,MARKER,marker)
            if point in values:struct.pack_into('<9I',ee,MARKER+8,*values[point])
            else:struct.pack_into('<QI',ee,MARKER+8,0,0x1FFEAF0 if point=='gif-vu-idle' else 0x1FFEB00)
            if point=='gif-path-after':ee[0x1FFEC40:0x1FFED30]=packet()
            m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture',
               'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),
                        'marker_address':MARKER,'marker_value':marker}}
            points[point]=(start,end,hashlib.sha256(ee[start:end]).hexdigest())
            if mutation:mutation(point,ee,m)
            m['capture_sha256']=hashlib.sha256(ee).hexdigest();fixtures[point]=(ee,m)
        with patch.object(verify_gif,'SYNC_POINTS',points), \
             patch.object(Path,'read_text',lambda path,**kw:json.dumps(fixtures[path.parent.name][1])), \
             patch.object(Path,'read_bytes',lambda path:fixtures[path.parent.name][0]):
            return verify_gif.verify_sync(Path('fixtures'),env)
    def test_sync_bundle_and_second_packet(self):
        bundle,receipt=self.check_sync();self.assertEqual(len(bundle),372)
        self.assertEqual(bundle[132:],packet());self.assertEqual(len(receipt['captures']),5)
    def test_sync_mutations_rejected(self):
        with self.assertRaises(ValueError):self.check_sync(lambda point,ee,m:m.update(disc_sha256='wrong'))
        with self.assertRaises(ValueError):self.check_sync(lambda point,ee,m:ee.__setitem__(MARKER+24,0) if point=='gif-sync-before' else None)
        with self.assertRaises(ValueError):self.check_sync(lambda point,ee,m:ee.__setitem__(capture_trap(point)[0],0))
    def test_sync_trap_boundaries(self):
        for point in ('gif-sync-before','gif-sync-poll','gif-sync-after'):
            address,trap,_=capture_trap(point)
            self.assertEqual(len(trap),128);self.assertLess(address+len(trap),0x3106A4)
        address,trap,_=capture_trap('gif-path-after');self.assertEqual(address+len(trap),0x1002D8)
