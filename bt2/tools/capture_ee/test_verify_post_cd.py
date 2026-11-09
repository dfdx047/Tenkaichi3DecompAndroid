import hashlib,json,struct,unittest
from pathlib import Path
from unittest.mock import patch
import verify_post_cd as v
from capture_ee import capture_trap,fresh_bind_stub,FRESH_BIND_STUB,MARKER,EXPECTED_SHA

class FreshCdTests(unittest.TestCase):
    def fixture(self,mutation=None):
        env=dict(pcsx2_sha256='pcsx2',bios_sha256='bios',disc_sha256='disc');fixtures={}
        for point in v.POINTS:
            ee=bytearray(32*1024*1024);iop=bytearray(2*1024*1024)
            addr,trap,marker=capture_trap(point);ee[addr:addr+len(trap)]=trap;struct.pack_into('<I',ee,MARKER,marker)
            call=point=='post-cd-after';bound=point!='post-cd-bind-after';server=0x43388 if bound else 0;buf=0x441B8 if bound else 0
            struct.pack_into('<3I',ee,MARKER+8,1 if call else 0,0,0x1FFED40 if call else 0x1FFEC90)
            struct.pack_into('<4I',iop,0x1E700,0x1E640,0x1E6C0,0x3A94C0,0x1E730)
            struct.pack_into('<3I',iop,0x43388,0x80000592,0x3E36C,0x441B8);struct.pack_into('<I',iop,0x43388+64,0x43358);struct.pack_into('<I',iop,0x43358+8,0x43388)
            struct.pack_into('<I',ee,0x3A87A8+36,server);struct.pack_into('<I',ee,0x3A87A8+20,buf)
            reply=(0x1000,0x3695C0,0x80000008,0,0,0,0,0x3A87A8,0x8000000A,0,0,0,0,0,0,0) if call else (0,0,0x80000008,0,0,0x203A97C0,0,0x3A87A8,0x80000009,server,buf,0,0,0,0,0)
            struct.pack_into('<16I',ee,0x3A94C0,*reply)
            if call:struct.pack_into('<4I',ee,0x3695C0,1,0x225,0x225,0)
            m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture','patch':dict(address=addr,size=len(trap),authored_words=list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),marker_address=MARKER,marker_value=marker)}
            if not call:
                stub=fresh_bind_stub(point.endswith('success'));ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]=stub
                m['authored_extra_segment']=dict(address=FRESH_BIND_STUB,words=list(struct.unpack('<'+str(len(stub)//4)+'I',stub)))
            if mutation:mutation(point,ee,iop,m)
            m.update(capture_sha256=hashlib.sha256(ee).hexdigest(),iop_capture_sha256=hashlib.sha256(iop).hexdigest());fixtures[point]=(ee,iop,m)
        with patch.object(v,'CALLER_SHA',hashlib.sha256(bytes(8)).hexdigest()),patch.object(Path,'read_text',lambda p,**kw:json.dumps(fixtures[p.parent.name][2])),patch.object(Path,'read_bytes',lambda p:fixtures[p.parent.name][1 if p.name=='iop-ram.bin' else 0]):
            return v.verify(Path('fixtures'),env)
    def test_bundle(self):self.assertEqual(len(self.fixture()[0]),208)
    def test_old_service(self):
        with self.assertRaises(ValueError):self.fixture(lambda p,e,i,m:struct.pack_into('<I',e,0x3A87A8+36,0x3F570))
    def test_bad_queue(self):
        with self.assertRaises(ValueError):self.fixture(lambda p,e,i,m:struct.pack_into('<I',i,0x43358+8,0))
    def test_result(self):
        with self.assertRaises(ValueError):self.fixture(lambda p,e,i,m:struct.pack_into('<I',e,0x3695C0,0))
    def test_environment(self):
        with self.assertRaises(ValueError):self.fixture(lambda p,e,i,m:m.update(disc_sha256='wrong'))
    def test_stub(self):
        with self.assertRaises(ValueError):self.fixture(lambda p,e,i,m:e.__setitem__(FRESH_BIND_STUB,255))
    def test_caller(self):
        with self.assertRaises(ValueError):self.fixture(lambda p,e,i,m:e.__setitem__(0x1003E8,255))
    def test_conditional_stub_paths(self):
        for success in (False,True):
            words=struct.unpack('<'+str(len(fresh_bind_stub(success))//4)+'I',fresh_bind_stub(success))
            early=6+(words[5]&65535)
            self.assertEqual(words[early:early+2],(0x04A30003,0x8E220024))
            self.assertEqual((words[early+2]&0x3FFFFFF)<<2,0x31F990)
            self.assertEqual((words[early+4]&0x3FFFFFF)<<2,0x31F9D4)
            if success:self.assertEqual(11+(words[10]&65535),early)

class ConditionalInstrumentationTests(unittest.TestCase):
    def test_authored_segment_and_displaced_branch(self):
        from capture_ee import instrument
        for point in ('post-cd-bind-success','post-scmd-bind-success'):
            addr,trap,_=capture_trap(point);data=bytearray(256)
            struct.pack_into('<I',data,28,52);struct.pack_into('<HH',data,42,32,1)
            struct.pack_into('<8I',data,52,1,0,0,0,0,0,7,4096)
            words=(0x04430013,0x8E020024) if 'scmd' in point else (0x0040282D,0x04A30012,0x8E220024)
            struct.pack_into('<'+str(len(words))+'I',data,200,*words)
            meta=dict(sha256=EXPECTED_SHA,load_segments=[dict(flags=5,virtual_address=addr,file_size=len(words)*4,file_offset=200,memory_size=len(words)*4)])
            with patch('capture_ee.inspect',return_value=meta):patched,offset=instrument(bytes(data),point)
            ph=struct.unpack_from('<8I',patched,52)
            self.assertEqual(offset,200);self.assertEqual(patched[200:200+len(trap)],trap)
            self.assertEqual(ph[2:4],(FRESH_BIND_STUB,FRESH_BIND_STUB));self.assertEqual(ph[6:],(5,4096))
            self.assertEqual(patched[ph[1]:],fresh_bind_stub(True,'scmd' in point))
            data[200]=255
            with patch('capture_ee.inspect',return_value=meta),self.assertRaises(ValueError):instrument(bytes(data),point)
    def test_scmd_branch_targets(self):
        words=struct.unpack('<'+str(len(fresh_bind_stub(True,True))//4)+'I',fresh_bind_stub(True,True))
        early=6+(words[5]&65535)
        self.assertEqual(words[early:early+2],(0x04430003,0x8E020024))
        self.assertEqual((words[early+2]&0x3FFFFFF)<<2,0x31F794)
        self.assertEqual((words[early+4]&0x3FFFFFF)<<2,0x31F7DC)

class FreshScmdTests(unittest.TestCase):
    def fixture(self,mutation=None):
        env=dict(pcsx2_sha256='pcsx2',bios_sha256='bios',disc_sha256='disc');fixtures={}
        for point in ('post-scmd-bind-success','post-mmode-after'):
            bind=point=='post-scmd-bind-success';ee=bytearray(32*1024*1024);iop=bytearray(2*1024*1024)
            addr,trap,marker=capture_trap(point);ee[addr:addr+len(trap)]=trap;struct.pack_into('<I',ee,MARKER,marker)
            struct.pack_into('<3I',ee,MARKER+8,0 if bind else 1,0,0x1FFECB0 if bind else 0x1FFED40)
            struct.pack_into('<4I',iop,0x1E700,0x1E640,0x1E6C0,0x3A94C0,0x1E730)
            struct.pack_into('<3I',iop,0x434F0,0x80000593,0x41BC8,0x439A8);struct.pack_into('<I',iop,0x434F0+64,0x43358)
            struct.pack_into('<I',iop,0x43388+56,0x43460);struct.pack_into('<I',iop,0x43460+56,0x434F0)
            struct.pack_into('<10I',ee,0x369F40,0,5 if bind else 6,7,0,0,0x439A8,0,0,0,0x434F0)
            reply=(0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x434F0,0x439A8,0,0,0,0,0) if bind else (0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0)
            struct.pack_into('<16I',ee,0x3A94C0,*reply)
            if not bind:
                struct.pack_into('<I',ee,0x3695C0,1);struct.pack_into('<I',ee,0x369A00,2);struct.pack_into('<I',iop,0x439A8,2)
            m={**env,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+': fixture','patch':dict(address=addr,size=len(trap),authored_words=list(struct.unpack('<'+str(len(trap)//4)+'I',trap)),marker_address=MARKER,marker_value=marker)}
            if bind:
                stub=fresh_bind_stub(True,True);ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]=stub;m['authored_extra_segment']=dict(address=FRESH_BIND_STUB,words=list(struct.unpack('<'+str(len(stub)//4)+'I',stub)))
            if mutation:mutation(point,ee,iop,m)
            m.update(capture_sha256=hashlib.sha256(ee).hexdigest(),iop_capture_sha256=hashlib.sha256(iop).hexdigest());fixtures[point]=(ee,iop,m)
        with patch.object(v,'SCMD_CALLER_SHA',hashlib.sha256(bytes(8)).hexdigest()),patch.object(Path,'read_text',lambda p,**kw:json.dumps(fixtures[p.parent.name][2])),patch.object(Path,'read_bytes',lambda p:fixtures[p.parent.name][1 if p.name=='iop-ram.bin' else 0]):return v.verify_scmd(Path('fixtures'),env)
    def test_bundle(self):self.assertEqual(len(self.fixture()[0]),132)
    def test_chain_and_result(self):
        for mutate in (lambda p,e,i,m:struct.pack_into('<I',i,0x43460+56,0),lambda p,e,i,m:struct.pack_into('<I',e,0x3695C0,0)):
            with self.assertRaises(ValueError):self.fixture(mutate)
    def test_stale_client_and_environment(self):
        for mutate in (lambda p,e,i,m:struct.pack_into('<I',e,0x369F64,0x3F690),lambda p,e,i,m:m.update(pcsx2_sha256='wrong')):
            with self.assertRaises(ValueError):self.fixture(mutate)
