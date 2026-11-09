import struct
import tempfile
import unittest
from pathlib import Path
from inspect_image import inspect,disc_image,ROOT

class ImageTests(unittest.TestCase):
    def fixture(self):
        data=bytearray(84)
        for index,(name,size) in enumerate((('RESET',0),('ROMDIR',80),('EXTINFO',0),('TEST',4))):
            struct.pack_into('<10sHI',data,index*16,name.encode(),0,size)
        data[80:84]=b'test';return data

    def test_derived_payload_offset(self):
        report=inspect(self.fixture());self.assertEqual(report['entries'][3]['offset'],80)
        self.assertEqual(report['entries'][3]['size'],4)

    def test_truncation_duplicate_and_extent_rejected(self):
        for mutate in ('truncated','duplicate','extent','terminator'):
            data=self.fixture()
            if mutate=='truncated':data=data[:-1]
            elif mutate=='duplicate':data[48:58]=b'ROMDIR'+bytes(4)
            elif mutate=='extent':struct.pack_into('<I',data,60,10000)
            else:data[64]=65
            with self.subTest(mutate=mutate),self.assertRaises(ValueError):inspect(data)

    def test_invalid_elf_header_rejected(self):
        data=self.fixture();data[80:84]=b'\x7fELF'
        with self.assertRaises(ValueError):inspect(data)

    def iso(self):
        data=bytearray(24*2048)
        def record(name,lba,size,directory):
            n=33+len(name);n+=n%2;r=bytearray(n);r[0]=n
            struct.pack_into('<I',r,2,lba);struct.pack_into('>I',r,6,lba)
            struct.pack_into('<I',r,10,size);struct.pack_into('>I',r,14,size)
            r[25]=2 if directory else 0;r[32]=len(name);r[33:33+len(name)]=name
            return r
        data[16*2048:16*2048+7]=b'\x01CD001\x01'
        data[16*2048+128:16*2048+132]=b'\x00\x08\x08\x00'
        root=record(b'\0',20,2048,True);data[16*2048+156:16*2048+156+len(root)]=root
        child=record(b'IRX',21,2048,True);data[20*2048:20*2048+len(child)]=child
        item=record(b'IOPRP300.IMG;1',22,84,False);data[21*2048:21*2048+len(item)]=item
        data[22*2048:22*2048+84]=self.fixture();return data

    def test_local_iso_path(self):
        with tempfile.TemporaryDirectory(dir=ROOT/"work") as folder:
            path=Path(folder)/'test.iso';path.write_bytes(self.iso())
            self.assertEqual(disc_image(path),self.fixture())

    def test_iso_endian_mismatch_rejected(self):
        data=self.iso();data[20*2048+6]^=1
        with tempfile.TemporaryDirectory(dir=ROOT/"work") as folder:
            path=Path(folder)/'test.iso';path.write_bytes(data)
            with self.assertRaises(ValueError):disc_image(path)
