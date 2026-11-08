import hashlib,json,struct,unittest
from pathlib import Path
from unittest.mock import patch
import verify_gs
from capture_ee import capture_trap,MARKER,EXPECTED_SHA

class GsEvidenceTests(unittest.TestCase):
    def check(self,alter=None,point="gs-csr-reset"):
        value,sp={"gs-csr-reset":(0x551B4000,0x1FFEAB0),"gs-imr-after":(0xFF00,0x1FFEAB0),"gs-crt-after":(0,0x1FFEB00),"gs-intc-after":(8,0x1FFEAE0),"gs-csr-field":(0x551B6004,0x1FFEAE0)}[point]
        address,trap,marker=capture_trap(point);ee=bytearray(32*1024*1024)
        ee[address:address+len(trap)]=trap
        struct.pack_into('<I',ee,MARKER,marker);struct.pack_into('<Q',ee,MARKER+8,value)
        struct.pack_into('<I',ee,MARKER+16,sp)
        env={'pcsx2_sha256':'pcsx2','bios_sha256':'bios','disc_sha256':'disc'}
        m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture',
           'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),
                    'marker_address':MARKER,'marker_value':marker}}
        if alter:alter(ee,m)
        m['capture_sha256']=hashlib.sha256(ee).hexdigest()
        with patch.object(verify_gs,'PREFIX_SHA',hashlib.sha256(bytes(32)).hexdigest()), \
             patch.object(verify_gs,'EXTRA_PREFIXES',{name:(start,end,hashlib.sha256(bytes(end-start)).hexdigest()) for name,(start,end,_) in verify_gs.EXTRA_PREFIXES.items()}), \
             patch.object(Path,'read_text',lambda path,**kw:json.dumps(m)), \
             patch.object(Path,'read_bytes',lambda path:ee):
            return verify_gs.verify(Path('authored/capture.json'),env,point)

    def test_authored_publication(self):
        for point,value in (('gs-csr-reset',0x551B4000),('gs-imr-after',0xFF00),('gs-crt-after',0),('gs-intc-after',8),('gs-csr-field',0x551B6004)):
            with self.subTest(point=point):self.assertEqual(self.check(point=point)[0],struct.pack('<Q',value))
    def test_wrong_csr_or_stack_rejected(self):
        for offset in (8,16):
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                self.check(lambda ee,m:ee.__setitem__(MARKER+offset,ee[MARKER+offset]^1))
    def test_wrong_disc_rejected(self):
        with self.assertRaises(ValueError):self.check(lambda ee,m:m.update(disc_sha256='another-disc'))
    def test_original_reset_prefix_mutation_rejected(self):
        with self.assertRaises(ValueError):self.check(lambda ee,m:ee.__setitem__(0x30FE44,1))

class InterruptEvidenceTests(unittest.TestCase):
    def check(self,mutation=None):
        env={'pcsx2_sha256':'pcsx2','bios_sha256':'bios','disc_sha256':'disc'};fixtures={}
        for point,(start,end,prefix,status,result) in verify_gs.INTERRUPT_POINTS.items():
            address,trap,marker=capture_trap(point);ee=bytearray(32*1024*1024)
            ee[address:address+len(trap)]=trap;struct.pack_into('<I',ee,MARKER,marker)
            struct.pack_into('<4I',ee,MARKER+8,status,0x1802,0x1FFEAD0,result)
            m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture',
               'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<14I',trap)),
                        'marker_address':MARKER,'marker_value':marker}}
            if mutation:mutation(point,ee,m)
            m['capture_sha256']=hashlib.sha256(ee).hexdigest();fixtures[point]=(ee,m)
        with patch.object(verify_gs,'INTERRUPT_POINTS',{
                name:(start,end,hashlib.sha256(bytes(end-start)).hexdigest(),status,result)
                for name,(start,end,_,status,result) in verify_gs.INTERRUPT_POINTS.items()}), \
             patch.object(Path,'read_text',lambda path,**kw:json.dumps(fixtures[path.parent.name][1])), \
             patch.object(Path,'read_bytes',lambda path:fixtures[path.parent.name][0]):
            return verify_gs.verify_interrupts(Path('fixtures'),env)
    def test_ordering_publications(self):
        bundle,_=self.check();self.assertEqual(len(bundle),64)
        self.assertEqual([struct.unpack_from('<I',bundle,i)[0] for i in range(0,64,16)],[12,8,12,8])
    def test_wrong_status_mask_stack_result_and_environment(self):
        for offset in (8,12,16,20):
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                self.check(lambda point,ee,m:ee.__setitem__(MARKER+offset,ee[MARKER+offset]^1))
        with self.assertRaises(ValueError):self.check(lambda point,ee,m:m.update(disc_sha256='wrong'))
    def test_direct_mmio_address_and_preserved_event_loop(self):
        for point in verify_gs.INTERRUPT_POINTS:
            address,trap,_=capture_trap(point);words=struct.unpack('<14I',trap)
            self.assertEqual(words[3:5],(0x3C191000,0x3739F000))
            self.assertEqual(words[5],0x8F220000);self.assertEqual(words[7],0x8F220010)
            self.assertEqual(words[12],0x08000000|((address+48)>>2));self.assertEqual(words[13],0)
        self.assertGreaterEqual(capture_trap('gs-intc-event')[0],0x32F14C)
