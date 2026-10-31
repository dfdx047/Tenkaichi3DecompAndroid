import hashlib
import io
import struct
import unittest
from verify_disc import hash_stream, root_executable


class DiscTests(unittest.TestCase):
    def test_bounded_hash_and_truncation(self):
        self.assertEqual(hash_stream(io.BytesIO(b'abcxyz'), 3)['sha256'], hashlib.sha256(b'abc').hexdigest())
        with self.assertRaises(ValueError):
            hash_stream(io.BytesIO(b'abc'), 4)

    def fixture(self):
        data=bytearray(24*2048)
        pvd=16*2048
        data[pvd:pvd+7]=b'\x01CD001\x01'
        data[pvd+128:pvd+132]=b'\x00\x08\x08\x00'
        def record(lba,size,name):
            r=bytearray(34+len(name))
            r[0]=len(r)
            struct.pack_into('<I',r,2,lba);struct.pack_into('>I',r,6,lba)
            struct.pack_into('<I',r,10,size);struct.pack_into('>I',r,14,size)
            r[32]=len(name);r[33:33+len(name)]=name
            return r
        r=record(20,2048,b'\0');r[25]=2
        data[pvd+156:pvd+156+len(r)]=r
        r=record(21,100,b'SLUS_214.41;1')
        data[20*2048:20*2048+len(r)]=r
        return data

    def test_root_executable_extent(self):
        data=self.fixture()
        self.assertEqual(root_executable(io.BytesIO(data),len(data)),(21*2048,100))

    def test_bad_paired_extent_rejected(self):
        data=self.fixture();data[20*2048+6]^=1
        with self.assertRaises(ValueError):
            root_executable(io.BytesIO(data),len(data))

    def test_outside_disc_rejected(self):
        data=self.fixture()
        struct.pack_into('<I',data,20*2048+2,100)
        struct.pack_into('>I',data,20*2048+6,100)
        with self.assertRaises(ValueError):
            root_executable(io.BytesIO(data),len(data))
