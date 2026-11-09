import hashlib,json,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from capture_ee import EXPECTED_SHA,MARKER,capture_trap,ROOT
from verify_loadfile import verify

class LoadfileTests(unittest.TestCase):
    def setUp(self):
        self.caller=patch('verify_loadfile.CALLER_SHA',hashlib.sha256(bytes(0x34)).hexdigest())
        self.caller.start();self.addCleanup(self.caller.stop)

    def fixtures(self,folder):
        environment={k:k for k in ('pcsx2_sha256','bios_sha256','disc_sha256')}
        for point in ('loadfile-bind-after','loadfile-version-after','loadfile-module-before'):
            ee=bytearray(32*1024*1024);iop=bytearray(2*1024*1024);address,trap,marker=capture_trap(point)
            ee[address:address+len(trap)]=trap;struct.pack_into('<I',ee,MARKER,marker)
            struct.pack_into('<3I',ee,MARKER+8,0,0,0x1FFECA0 if point.endswith('before') else 0x1FFEC50)
            struct.pack_into('<4I',iop,0x1E700,0x1E640,0x1E6C0,0x3A94C0,0x1E730)
            struct.pack_into('<3I',iop,0x22568,0x80000006,0x20DAC,0x225B0);struct.pack_into('<I',iop,0x22568+64,0x22550);struct.pack_into('<I',iop,0x22550+8,0x22568)
            struct.pack_into('<10I',ee,0x3ACC00,0,7,7,0,0,0x225B0,0,0,0,0x22568)
            reply=(0,0,0x80000008,0,0,0x203A97C0,0,0x3ACC00,0x80000009,0x22568,0x225B0,0,0,0,0,0) if point=='loadfile-bind-after' else (0x400,0x3ACA00,0x80000008,0,0,0,0,0x3ACC00,0x8000000A,0,0,0,0,0,0,0)
            struct.pack_into('<16I',ee,0x3A94C0,*reply)
            if point.endswith('before'):ee[0x3ACA08:0x3ACA22]=b'cdrom0:\\IRX\\SIO2MAN.IRX;1\0'
            else:ee[0x3ACA00:0x3ACA04]=b'3000'
            path=folder/point;path.mkdir();(path/'ee-ram.bin').write_bytes(ee);(path/'iop-ram.bin').write_bytes(iop)
            m={**environment,'source_elf_sha256':EXPECTED_SHA,'capture_point':point+':test','capture_sha256':hashlib.sha256(ee).hexdigest(),'iop_capture_sha256':hashlib.sha256(iop).hexdigest(),'patch':{'address':address,'size':len(trap),'authored_words':list(struct.unpack('<8I',trap)),'marker_address':MARKER,'marker_value':marker}}
            (path/'capture.json').write_text(json.dumps(m),encoding='utf-8')
        return environment
    def test_bundle_and_environment_rejection(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as temporary:
            folder=Path(temporary);env=self.fixtures(folder);bundle,_=verify(folder,env);self.assertEqual(len(bundle),132)
            env['disc_sha256']='different'
            with self.assertRaises(ValueError):verify(folder,env)
    def test_modified_version_and_stale_server_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as temporary:
            folder=Path(temporary);env=self.fixtures(folder)
            original=(folder/'loadfile-version-after'/'ee-ram.bin').read_bytes()
            for offset,value in ((0x3ACA00,0),(0x3ACC24,0x43388),(0x100400,1)):
                path=folder/'loadfile-version-after';data=bytearray(original);struct.pack_into('<I',data,offset,value);(path/'ee-ram.bin').write_bytes(data)
                m=json.loads((path/'capture.json').read_text(encoding='utf-8'));m['capture_sha256']=hashlib.sha256(data).hexdigest();(path/'capture.json').write_text(json.dumps(m),encoding='utf-8')
                with self.subTest(offset=offset),self.assertRaises(ValueError):verify(folder,env)
if __name__=='__main__':unittest.main()
