import hashlib,json,struct,unittest
from pathlib import Path
from unittest.mock import patch
import verify_post_sif as v
from capture_ee import MARKER,EXPECTED_SHA,capture_trap

class PostSifEvidenceTests(unittest.TestCase):
    def fixture(self, mutate=None):
        env=dict(pcsx2_sha256='pcsx2',bios_sha256='bios',disc_sha256='disc'); fixtures={}
        for point in v.POINTS:
            after=point.endswith('after'); ee=bytearray(32*1024*1024); ir=bytearray(2*1024*1024)
            address,trap,marker=capture_trap(point); ee[address:address+len(trap)]=trap
            struct.pack_into('<I',ee,MARKER,marker)
            pub=(1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640) if after else (0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640)
            struct.pack_into('<9I',ee,MARKER+8,*pub)
            struct.pack_into('<4I',ir,0x1E700,0x1E640,0x1E6C0,0x3A94C0 if after else 0,0x1E730)
            struct.pack_into('<8I',ee,0x3A9598,0x203A94C0,0x203A9540,0x1E640 if after else 0x19600,0x3A95C0,32,0,0,0x3A9740)
            if after:
                struct.pack_into('<4I',ee,0x3A9800,16,0,0x80000002,1)
                struct.pack_into('<6I',ee,0x3A94C0,0,0,0x80000001,0,0,1)
                struct.pack_into('<I',ee,0x3A9740,1)
            m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture','patch':dict(address=address,size=len(trap),authored_words=list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),marker_address=MARKER,marker_value=marker)}
            if mutate: mutate(point,ee,ir,m)
            m.update(capture_sha256=hashlib.sha256(ee).hexdigest(),iop_capture_sha256=hashlib.sha256(ir).hexdigest())
            fixtures[point]=(ee,ir,m)
        points={p:(a,b,hashlib.sha256(bytes(b-a)).hexdigest()) for p,(a,b,_) in v.POINTS.items()}
        with patch.object(v,'POINTS',points),patch.object(Path,'read_text',lambda p,**kw:json.dumps(fixtures[p.parent.name][2])),patch.object(Path,'read_bytes',lambda p:fixtures[p.parent.name][1 if p.name=='iop-ram.bin' else 0]):
            return v.verify(Path('fixtures'),env)
    def test_bundle(self): self.assertEqual(len(self.fixture()[0]),136)
    def test_stale_ownership(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:struct.pack_into('<I',i,0x1E700,0x19600))
    def test_response(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:struct.pack_into('<I',e,0x3A9740,0))
    def test_environment(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:m.update(bios_sha256='wrong'))
    def test_prefix(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:e.__setitem__(v.POINTS[p][0],255))
    def test_trap(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:e.__setitem__(capture_trap(p)[0],255))
    def test_ee_owner(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:struct.pack_into('<I',e,0x3A9598+8,0))
    def test_opt_one(self):
        with self.assertRaises(ValueError): self.fixture(lambda p,e,i,m:struct.pack_into('<I',e,0x3A9800,20))
