import struct,unittest
from inspect_irx import inspect,relocate

class IrxTests(unittest.TestCase):
    def fixture(self):
        data=bytearray(400);data[:7]=b'\x7fELF\x01\x01\x01'
        struct.pack_into('<HHIIIIIHHHHHH',data,16,0xFF80,8,1,0,52,256,1,52,32,2,40,3,0)
        struct.pack_into('<8I',data,52,0x70000080,116,0,0,34,0,4,4)
        struct.pack_into('<8I',data,84,1,160,0,0,96,112,7,16)
        struct.pack_into('<6I',data,116,80,0,0,80,16,16);data[142:150]=b'testmod\0'
        struct.pack_into('<4I',data,160,0x0C000002,0,0x03E00008,0)
        struct.pack_into('<IIHH8s',data,192,0x41E00000,0,0x104,0,b'sysclib')
        struct.pack_into('<2I',data,212,0x03E00008,0x24000008)
        struct.pack_into('<10I',data,296,0,1,6,0,160,80,0,0,16,0)
        struct.pack_into('<10I',data,336,0,9,0,0,384,8,0,1,4,8)
        struct.pack_into('<2I',data,384,80,2)
        return data
    def test_bounded_map_and_imports(self):
        report=inspect(self.fixture())
        self.assertEqual(report['memory_size'],112)
        self.assertEqual([r['start'] for r in report['provisional_functions']],[0,8])
        self.assertEqual(report['provisional_functions'][-1]['end'],32)
        self.assertEqual(report['imports'][0]['ordinal'],8)
        self.assertEqual(report['relocation_counts'],{'2':1})
    def test_code_after_export_table_is_preserved(self):
        data=self.fixture();data[160:272]=bytes(112)
        struct.pack_into('<I',data,84+16,112);struct.pack_into('<I',data,84+20,128)
        struct.pack_into('<6I',data,116,96,0,0,96,16,16);struct.pack_into('<I',data,296+20,96)
        struct.pack_into('<4I',data,160,0x0C000002,0,0x03E00008,0)
        struct.pack_into('<IIHH8s',data,176,0x41C00000,0,0x100,0,b'test')
        struct.pack_into('<3I',data,196,0,48,0);struct.pack_into('<2I',data,208,0x03E00008,0)
        struct.pack_into('<IIHH8s',data,216,0x41E00000,0,0x104,0,b'sysclib')
        struct.pack_into('<I',data,384,96)
        report=inspect(data)
        self.assertEqual(report['exports'][0]['slots'],[0,48])
        self.assertEqual(report['provisional_functions'][-1],{'name':'FUN_00000030','start':48,'end':56})
    def test_private_placement_and_relocation(self):
        data=self.fixture();original=bytes(data);report=inspect(data);placed=relocate(data,report,0x10000)
        self.assertEqual(struct.unpack_from('<I',placed,80)[0],0x10000)
        self.assertEqual(placed[96:],bytes(16));self.assertEqual(bytes(data),original)
        for base in (0,0x10001,0x200000):
            with self.subTest(base=base),self.assertRaises(ValueError):relocate(data,report,base)
    def test_jump_and_signed_hi_lo_relocations(self):
        data=self.fixture();struct.pack_into('<3I',data,160,0x0C000004,0x3C018001,0x2421FFFF)
        report=inspect(data);report['relocations']=[{'address':0,'type':4},{'address':4,'type':5},{'address':8,'type':6}]
        placed=relocate(data,report,0x10000)
        self.assertEqual(struct.unpack_from('<3I',placed),(0x0C004004,0x3C018002,0x2421FFFF))
        for rows in ([{'address':4,'type':5}],[{'address':8,'type':6}]):
            report['relocations']=rows
            with self.assertRaises(ValueError):relocate(data,report,0x10000)
    def test_malformed_metadata_rejected(self):
        for offset,value in [(18,9),(84+20,8),(296+20,500),(384,97),(388,255),(212,0x08000000)]:
            data=self.fixture();struct.pack_into('<I' if offset!=18 else '<H',data,offset,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):inspect(data)
    def test_truncated_tables_rejected(self):
        for size in (0,51,115,149,335,387):
            with self.subTest(size=size),self.assertRaises(ValueError):inspect(self.fixture()[:size])
if __name__=='__main__':unittest.main()
