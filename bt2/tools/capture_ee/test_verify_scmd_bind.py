import struct
import unittest
from verify_scmd_bind import validate_state, validate_mmode_state

class ScmdBindTests(unittest.TestCase):
    def setUp(self):
        self.ee=bytearray(32*1024*1024); self.iop=bytearray(2*1024*1024)
        def put(data,address,*values):
            struct.pack_into('<'+str(len(values))+'I',data,address,*values)
        put(self.ee,0x387E80,0xB713,0,0x1FFECB0)
        put(self.ee,0x1FFEC40,0x370000,1,0,0,2,0x3838D8)
        put(self.ee,0x369F40,0,4,7,0,0,0x3F6D8,0,0,0,0x3F690)
        request=[0,0,0x80000009,0,5,0x203A97C0,4,0x369F40,0x80000593,4,0x3695C0,16,1,0x3F570,0,0]
        put(self.iop,0x19600,*request)
        request[0],request[4],request[6]=64,4,0
        put(self.ee,0x3A97C0,*request)
        put(self.ee,0x3A94C0,0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x3F690,0x3F6D8)
        put(self.iop,0x3F690,0x80000593,0x3D4E8,0x3F6D8)
        put(self.iop,0x3F6D0,0x3F558)
        put(self.iop,0x3F680,0x3F690)
        put(self.iop,0x3F5A8,0x3F648)

    def test_authored_protocol_fixture(self):
        validate_state(self.ee,self.iop)

    def test_changed_evidence_rejected(self):
        for data,address in ((self.ee,0x1FFEC50),(self.ee,0x369F48),(self.ee,0x3A94E4),
                             (self.iop,0x19620),(self.iop,0x3F690),(self.iop,0x3F680)):
            with self.subTest(address=hex(address)):
                data[address]^=1
                with self.assertRaises(ValueError): validate_state(self.ee,self.iop)
                data[address]^=1

    def test_wrong_size_rejected(self):
        with self.assertRaises(ValueError): validate_state(self.ee[:-1],self.iop)


class MediaModeTests(unittest.TestCase):
    def setUp(self):
        self.ee=bytearray(32*1024*1024);self.iop=bytearray(2*1024*1024)
        def put(data,address,*values): struct.pack_into('<'+str(len(values))+'I',data,address,*values)
        put(self.ee,0x387E80,0xB714,0,0x1FFECF0)
        put(self.ee,0x1FFEC30,0,1,0,0,0x370000,0x3838E8)
        put(self.ee,0x369F40,0,5,7,0,0,0x3F6D8,0x38BDF0,0,0,0x3F690)
        request=[0x400,0x3F6D8,0x8000000A,0,5,0x203A97C0,5,0x369F40,34,4,0x3695C0,4,1,0x3F690,0,0]
        put(self.iop,0x19600,*request);request[0],request[4],request[6]=0x440,4,0
        put(self.ee,0x3A97C0,*request)
        put(self.ee,0x3A94C0,0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A)
        put(self.ee,0x369A00,2);put(self.iop,0x3F6D8,2)
        put(self.iop,0x3F690,0x80000593,0x3D4E8,0x3F6D8,4,0,0,0,0x369F40,0x203A97C0,34,0x3695C0,4,1)
        put(self.iop,0x3F6D0,0x3F558)

    def test_authored_media_mode_fixture(self): validate_mmode_state(self.ee,self.iop)

    def test_altered_input_reply_service_rejected(self):
        for data,address in ((self.ee,0x369A00),(self.ee,0x3695C0),(self.ee,0x369F48),
                             (self.iop,0x19620),(self.iop,0x3F690),(self.ee,0x3A94C4)):
            with self.subTest(address=hex(address)):
                data[address]^=1
                with self.assertRaises(ValueError): validate_mmode_state(self.ee,self.iop)
                data[address]^=1

    def test_wrong_size_rejected(self):
        with self.assertRaises(ValueError): validate_mmode_state(self.ee,self.iop[:-1])
