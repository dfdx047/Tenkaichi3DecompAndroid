import hashlib
import struct
import unittest
from unittest.mock import patch
import verify_sif_reset as verifier
from capture_ee import MARKER, capture_trap

class SifResetEvidenceTests(unittest.TestCase):
    def fixture(self,point):
        # Entirely authored RAM fixture; no kernel or executable bytes.
        ee=bytearray(32*1024*1024)
        marker=capture_trap(point)[2]
        struct.pack_into('<III',ee,MARKER,marker,0 if point=='sif-stop-after' else 0x19600,0x1FFEC80)
        struct.pack_into('<I',ee,0x14EAC,0x80006380)
        return ee

    def check(self,ee,point):
        with patch.object(verifier,'KERNEL_STOP_SHA',hashlib.sha256(bytes(44)).hexdigest()):
            return verifier.validate_publication(ee,point)

    def test_stop_and_query_publications(self):
        for point,result in (('sif-stop-after',0),('sif-reset-subaddr',0x19600)):
            self.assertEqual(self.check(self.fixture(point),point),struct.pack('<I',result))

    def test_altered_publication_table_or_kernel_rejected(self):
        for address in (MARKER,MARKER+4,MARKER+8,0x14EAC,0x6380):
            ee=self.fixture('sif-stop-after');ee[address]^=1
            with self.subTest(address=hex(address)),self.assertRaises(ValueError):self.check(ee,'sif-stop-after')

    def test_truncated_capture_rejected(self):
        with self.assertRaises(ValueError):self.check(self.fixture('sif-stop-after')[:-1],'sif-stop-after')


class SmflagEvidenceTests(unittest.TestCase):
    def fixture(self,point):
        ee=bytearray(32*1024*1024)
        marker=capture_trap(point)[2]
        struct.pack_into('<4I',ee,MARKER,marker,0 if point=='reset-smflag-before' else 0x30000,0x1FFEC80,0x30000)
        return ee

    def test_before_and_after_publications(self):
        for point in ('reset-smflag-before','reset-smflag-after'):
            verifier.validate_smflag_publication(self.fixture(point),point)

    def test_wrong_marker_result_stack_or_flag_rejected(self):
        for offset in (0,4,8,12):
            ee=self.fixture('reset-smflag-after');ee[MARKER+offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                verifier.validate_smflag_publication(ee,'reset-smflag-after')

    def test_truncated_capture_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_smflag_publication(self.fixture('reset-smflag-before')[:-1],'reset-smflag-before')


class RebootSubmissionEvidenceTests(unittest.TestCase):
    def check_chain(self, mutation=None):
        from pathlib import Path
        import json
        points = [('reset-submit-after',0xE01,None),('reset-cmd-clear',0x20000,0x20000),
                  ('reset-flags-after',0,0),('reset-rpc-clear',0,0),('reset-subaddr-clear',0,0)]
        captures={}; manifests={}
        for point,result,flag in points:
            address,trap,marker=capture_trap(point)
            ee=bytearray(32*1024*1024)
            ee[address:address+len(trap)]=trap
            struct.pack_into('<4I',ee,MARKER,marker,result,0x1FFEC80,flag or 0)
            struct.pack_into('<6I',ee,0x3ACC40,104,0,0x80000003,0,36,0)
            ee[0x3ACC58:0x3ACC7D]=b'rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1\0'
            m={'source_elf_sha256':verifier.EXPECTED_SHA,'capture_point':point+': authored fixture',
               'pcsx2_sha256':'authored-pcsx2','bios_sha256':'authored-bios','disc_sha256':'authored-disc',
               'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),
                        'marker_address':MARKER,'marker_value':marker}}
            if mutation:mutation(point,ee,m)
            m['capture_sha256']=hashlib.sha256(ee).hexdigest()
            captures[point]=ee;manifests[point]=m
        with patch.object(Path,'read_text',lambda path,**kw:json.dumps(manifests[path.parent.name])), \
             patch.object(Path,'read_bytes',lambda path:captures[path.parent.name]):
            return verifier.verify_submission_chain(Path('authored'),{'disc_sha256':'authored-disc'})

    def test_authored_chain(self):
        self.assertEqual(self.check_chain()[0],struct.pack('<5I',0xE01,0x20000,0,0,0))

    def test_wrong_disc_rejected(self):
        def mutate(point,ee,m):
            if point=='reset-submit-after':m['disc_sha256']='another-disc'
        with self.assertRaises(ValueError):self.check_chain(mutate)

    def test_wrong_flag_readback_rejected(self):
        def mutate(point,ee,m):
            if point=='reset-cmd-clear':struct.pack_into('<I',ee,MARKER+12,0x30000)
        with self.assertRaises(ValueError):self.check_chain(mutate)


class ReadyEvidenceTests(unittest.TestCase):
    def check(self, change=None):
        from pathlib import Path
        import json
        address,trap,marker=capture_trap('reset-ready')
        ee=bytearray(32*1024*1024);iop=bytes(2*1024*1024)
        ee[address:address+len(trap)]=trap
        struct.pack_into('<3I',ee,MARKER,marker,0x40000,0x1FFED30)
        m={'source_elf_sha256':verifier.EXPECTED_SHA,'capture_point':'reset-ready: fixture',
           'pcsx2_sha256':'pcsx2','bios_sha256':'bios','disc_sha256':'disc',
           'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<8I',trap)),
                    'marker_address':MARKER,'marker_value':marker}}
        if change:change(ee,m)
        m['capture_sha256']=hashlib.sha256(ee).hexdigest();m['iop_capture_sha256']=hashlib.sha256(iop).hexdigest()
        with patch.object(verifier,'READY_BRANCH_SHA',hashlib.sha256(bytes(32)).hexdigest()), \
             patch.object(Path,'read_text',lambda path,**kw:json.dumps(m)), \
             patch.object(Path,'read_bytes',lambda path:iop if path.name=='iop-ram.bin' else ee):
            return verifier.verify_ready(Path('fixture/capture.json'),{'reset-submit-after':
                {'pcsx2_sha256':'pcsx2','bios_sha256':'bios','disc_sha256':'disc'}})

    def test_ready_branch_preserves_not_ready_return(self):
        address,trap,_=capture_trap('reset-ready')
        self.assertLessEqual(address+len(trap),0x33814C)
        self.assertEqual(struct.unpack('<8I',trap)[2],0xAF030004)
        self.assertEqual(self.check()[0],struct.pack('<I',0x40000))

    def test_wrong_ready_publication_rejected(self):
        def change(ee,m):struct.pack_into('<I',ee,MARKER+4,0)
        with self.assertRaises(ValueError):self.check(change)

    def test_wrong_ready_disc_rejected(self):
        def change(ee,m):m['disc_sha256']='another-disc'
        with self.assertRaises(ValueError):self.check(change)
