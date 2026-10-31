import unittest
import struct
from prepare_map import select_candidates, has_tail_nop_gap, has_branch_nop_gaps, has_switch_case_gaps


class CandidateMapTests(unittest.TestCase):
    metadata = {"sections": [{"name": ".text", "flags": 6, "occupies_file_bytes": True,
                              "address": 0x1000, "size": 0x100}]}

    def row(self, start, end):
        return {"start": start, "end_exclusive": end, "body_size": end-start, "contiguous_aligned": True}

    def test_unreachable_branch_padding_and_control_rejections(self):
        metadata={'sections':[{**self.metadata['sections'][0],'file_offset':0}]}
        words=[0x10000007,0,0,0x24020001,0x10000003,0,0,0x24020002,0x03E00008,0]
        row={'start':0x1000,'end_exclusive':0x1028,'body_size':32,'contiguous_aligned':False,'body_ranges':['1000-1007','100c-1017','101c-1027']}
        data=struct.pack('<10I',*words)
        self.assertTrue(has_branch_nop_gaps(row,metadata,data))
        for i,value in [(2,1),(0,0x14000007),(0,0x10000001),(4,0x10000001),(3,0x03200008),(1,0x03E00008),(3,0x45000002)]:
            changed=words.copy();changed[i]=value
            self.assertFalse(has_branch_nop_gaps(row,metadata,struct.pack('<10I',*changed)))
        self.assertFalse(has_branch_nop_gaps(row,metadata,data[:-1]))
        selected,_=select_candidates({'functions':[row,self.row(0x1008,0x1010),self.row(0x1040,0x1048)]},metadata,data=data)
        self.assertEqual(selected,[{'start':0x1040,'end':0x1048}])

    def test_tail_jump_padding_before_another_path(self):
        metadata={'sections':[{**self.metadata['sections'][0],'file_offset':0}]}
        words=[0x14000004,0,0x08000420,0x27BD0020,0,0x0C000420,0,0x03E00008,0]
        row={'start':0x1000,'end_exclusive':0x1024,'body_size':32,'contiguous_aligned':False,'body_ranges':['1000-100f','1014-1023']}
        data=struct.pack('<9I',*words)
        self.assertTrue(has_branch_nop_gaps(row,metadata,data))
        indirect=words.copy();indirect[5]=0x0040F809
        self.assertTrue(has_branch_nop_gaps(row,metadata,struct.pack('<9I',*indirect)))
        indirect[5]=0x00401009
        self.assertFalse(has_branch_nop_gaps(row,metadata,struct.pack('<9I',*indirect)))
        for index,value in [(2,0x0C000420),(2,0x03200008),(2,0x08000404),(2,0x08000800),(0,0x14000003),(4,1),(3,0x03E00008)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(row,metadata,struct.pack('<9I',*changed)))

    def test_overlap_group_quarantines_all_members(self):
        report = {"functions": [self.row(0x1000, 0x1020), self.row(0x1010, 0x1030),
                                self.row(0x1020, 0x1040), self.row(0x1040, 0x1048)]}
        selected, excluded = select_candidates(report, self.metadata)
        self.assertEqual(selected, [{"start": 0x1040, "end": 0x1048}])
        self.assertEqual(len(excluded), 3)

    def test_size_section_and_body_filters(self):
        invalid = self.row(0x1060, 0x1068)
        invalid["body_size"] = 4
        selected, excluded = select_candidates({"functions": [self.row(0x1000, 0x1008),
            self.row(0x1010, 0x1050), self.row(0x2000, 0x2008), invalid]}, self.metadata, max_size=16)
        self.assertEqual(len(selected), 1)
        self.assertEqual({row["reason"] for row in excluded},
                         {"oversized", "outside_file_backed_ee_text", "noncontiguous_or_invalid"})

    def test_single_tail_padding_gap_and_rejections(self):
        metadata = {"sections": [{**self.metadata["sections"][0], "file_offset": 0}]}
        words = [0x14000004, 0, 0x08000800, 0x27BD0010, 0, 0x03E00008, 0x27BD0010]
        data = struct.pack("<7I", *words)
        candidate = {"start": 0x1000, "end_exclusive": 0x101C, "body_size": 24,
                     "contiguous_aligned": False, "body_ranges": ["1000-100f", "1014-101b"]}
        self.assertTrue(has_tail_nop_gap(candidate, metadata, data))
        self.assertEqual(select_candidates({"functions": [candidate]}, metadata, data=data)[0],
                         [{"start": 0x1000, "end": 0x101C}])
        for index, replacement in ((4, 1), (2, 0x0C000800), (2, 0x08000404),
                                   (0, 0x14000003), (0, 0x03200008), (0, 0x45000003),
                                   (3, 0x03E00008), (5, 0)):
            changed = words.copy()
            changed[index] = replacement
            self.assertFalse(has_tail_nop_gap(candidate, metadata, struct.pack("<7I", *changed)))
        self.assertFalse(has_tail_nop_gap(candidate, metadata, data[:-1]))
        overlapping = self.row(0x1010, 0x1018)
        selected, excluded = select_candidates({"functions": [candidate, overlapping, self.row(0x1040, 0x1048)]},
                                               metadata, data=data)
        self.assertEqual(selected, [{"start": 0x1040, "end": 0x1048}])
        self.assertEqual(len(excluded), 2)


class BoundedSwitchTests(unittest.TestCase):
    def test_bounded_table_and_changed_bounds_targets_instructions(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x48,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        words=[0,0,0x2623FFE0,0x2C620002,0x10400009,0x00031080,0x3C030000,0x00621821,0x8C631040,0x00600008,0,0x10000003,0,0,0x03E00008,0,0x1000,0x1038]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':60,'contiguous_aligned':False,'body_ranges':['1000-1033','1038-103f']}
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*words)))
        for index,value in [(3,0x2C620000),(3,0x2C620003),(4,0x14400009),(5,0x000310C0),(8,0x8C631041),(16,0x1034),(16,0x1040),(17,0x1039),(10,0x24020001)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*changed)))
        self.assertFalse(has_branch_nop_gaps(candidate,metadata,struct.pack('<17I',*words[:-1])))
        # Register saves between the bound and the guard keep the proof (the bound moved up two words,
        # two stores in between); an instruction writing the condition register there breaks it.
        apart=words.copy();apart[1]=0x2C620002;apart[2]=0xFFB00000;apart[3]=0xE7B40010
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*apart)))
        apart[3]=0x24020001
        self.assertFalse(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*apart)))
        # The bound in the delay slot of a conditional branch on the index (BLTZ v1) is still proven;
        # after an unconditional branch, a likely branch or a jump it is not.
        signed=words.copy();signed[2]=0x0460000B
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*signed)))
        for value in (0x1000000B,0x0462000B,0x08000400):
            changed=words.copy();changed[2]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*changed)))

    def test_moved_index_beql_switch_with_padding_after_jump(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # Index producer / SLTIU / BEQL / annulled LD delay / SLL / LUI / ADDU / LW / JR v1 / delay, NOP gap.
        words=[0,0x0040182D,0x2C620002,0x50400008,0xDFB00000,0x00031080,0x3C030000,0x00621821,0x8C631048,0x00600008,0,0,
               0x10000002,0,0x03E00008,0,0,0,0x1030,0x1038]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':60,'contiguous_aligned':False,'body_ranges':['1000-102b','1030-103f']}
        pack=lambda w:struct.pack('<20I',*w)
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(words)))
        for index,value in [(1,0x03E00008),(2,0x2C820002),(3,0x54400008),(4,0x03E00008),(5,0x000310C0),(19,0x102C),(18,0x1040)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,pack(changed)))

    def test_switch_that_begins_its_function(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x48,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # The function's first instruction is the bound, the index an incoming argument (seen at
        # 0x41CAD8 of the overlay): SLTIU v0,a0,2 / BEQ v0,zero / SLL v0,a0,2 (delay) / LUI v1 /
        # ADDU v1,v1,v0 / LW v1 / JR v1 / delay; two case blocks the analysis did not follow, the
        # second running into the function's end.
        words=[0x2C820002,0x1040000C,0x00041080,0x3C030000,0x00621821,0x8C631040,0x00600008,0,
               0x3C020043,0x10000004,0x24040001,0,0x3C020043,0x24040002,0x03E00008,0,0x1020,0x1030]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':40,'contiguous_aligned':False,'body_ranges':['1000-101f','1038-103f']}
        pack=lambda w:struct.pack('<18I',*w)
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(words)))
        # Without the bound, with a bound on another register, or with a target outside the function it stays unproven.
        for index,value in [(0,0x24020001),(0,0x2CA20002),(16,0x1044)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(changed)))

    def test_beql_switch_with_swapped_index_and_condition_registers(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # Index in v0, bound condition in v1: SLTIU v1,v0,2 / BEQL v1,zero / annulled LW v0 / SLL v0,v0,2 /
        # LUI v1 / ADDU v1,v1,v0 / LW v1 / JR v1 (seen at 0x26DD30).
        words=[0,0x0060102D,0x2C430002,0x50600008,0x8FA20004,0x00021080,0x3C030000,0x00621821,0x8C631048,0x00600008,0,0,
               0x10000002,0,0x03E00008,0,0,0,0x1030,0x1038]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':60,'contiguous_aligned':False,'body_ranges':['1000-102b','1030-103f']}
        pack=lambda w:struct.pack('<20I',*w)
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(words)))
        # SLL of a register other than the bounded index, a bound whose condition is the index, or a
        # guard branch on another register stay unproven.
        for index,value in [(5,0x00031080),(2,0x2C420002),(3,0x50400008)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,pack(changed)))

    def test_beq_switch_with_index_preserving_delay_slot(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # As the BEQL layout, but a plain BEQ whose delay slot (LW v0, both paths) precedes the SLL.
        words=[0,0x0040182D,0x2C620002,0x10400008,0x8E421C00,0x00031080,0x3C030000,0x00621821,0x8C631048,0x00600008,0,0,
               0x10000002,0,0x03E00008,0,0,0,0x1030,0x1038]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':60,'contiguous_aligned':False,'body_ranges':['1000-102b','1030-103f']}
        pack=lambda w:struct.pack('<20I',*w)
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(words)))
        # Delay slot writing the index (LW v1 / ADDU v1), a control delay slot, or a wrong bound stay unproven.
        # A store in the delay slot writes no register (SW zero,0(sp), seen at 0x2A1AB4).
        stored=words.copy();stored[4]=0xAFA00000
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(stored)))
        for index,value in [(4,0x8E431C00),(4,0x00431821),(4,0x03E00008),(2,0x2C620000)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,pack(changed)))
        # FPU arithmetic writes no integer register: allowed in the delay slot and between the bound
        # and the guard (seen at 0x12001C); MFC1 into the index or the condition is not.
        fpu=words.copy();fpu[1],fpu[2],fpu[4]=0x2C620002,0x46001D42,0x46011D02
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(fpu)))
        for index,value in [(4,0x44030000),(2,0x44020000),(2,0x44030000)]:
            changed=fpu.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,pack(changed)))

    def test_external_jump_target_for_overlay_programs(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x40,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # J into another program's code / delay / NOP gap / JR RA path.
        words=[0x08100000,0,0,0x03E00008,0]+[0]*11
        candidate={'start':0x1000,'end_exclusive':0x1014,'body_size':16,'contiguous_aligned':False,'body_ranges':['1000-1007','100c-1013']}
        data=struct.pack('<16I',*words)
        self.assertFalse(has_branch_nop_gaps(candidate,metadata,data))
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,data,((0x400000,0x500000),)))
        self.assertFalse(has_branch_nop_gaps(candidate,metadata,data,((0x500000,0x600000),)))

    def test_delay_slot_lui_switch_through_other_register(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # ADDIU a0 / SLTIU v1,a0,2 / BEQ v1 / LUI v0 (delay) / SLL v1,a0,2 / ADDIU v0 / ADDU / LW a0 / JR a0 / delay;
        # unconditional BEQ + delay / NOP gap / two case paths; table after the body.
        words=[0x2444FFFE,0x2C830002,0x10600007,0x3C020000,0x00041880,0x24421040,0x00621821,0x8C640000,0x00800008,0,
               0x10000003,0,0,0x03E00008,0,0,0x1034,0x1038]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':60,'contiguous_aligned':False,'body_ranges':['1000-102f','1034-103f']}
        pack=lambda w:struct.pack('<18I',*w)
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(words)))
        for index,value in [(1,0x2C830000),(1,0x2C830003),(2,0x14600007),(3,0x3C040000),(4,0x000418C0),(4,0x00051880),(5,0x24431040),
                            (7,0x8C640004),(7,0x8C650000),(16,0x1030),(17,0x1040)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,pack(changed)))

    def test_coprocessor_arithmetic_and_condition_branches(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x40,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # BEQ to the tail path / slot 2 varies / JR RA + delay / NOP gap / tail J + delay.
        words=[0x10000005,0,0x24020001,0x03E00008,0,0,0x08000400,0]
        candidate={'start':0x1000,'end_exclusive':0x1020,'body_size':28,'contiguous_aligned':False,'body_ranges':['1000-1013','1018-101f']}
        pack=lambda w:struct.pack('<8I',*w)
        for value,accepted in [(0x46000836,True),   # COP1 compare (rs 16): data flow.
                               (0x4A000001,True),   # COP2 macro operation (rs 16).
                               (0x45000003,True),   # BC1F to the tail path.
                               (0x45000002,False),  # BC1F into the padding.
                               (0x42000018,False)]: # ERET.
            changed=words.copy();changed[2]=value
            self.assertEqual(has_branch_nop_gaps(candidate,metadata,pack(changed)),accepted,hex(value))

    def test_padding_after_return_before_tail_path(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x40,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        # BEQ to the tail path / return path ending JR RA + delay / NOP gap / tail J + delay.
        words=[0x10000005,0,0x24020001,0x03E00008,0,0,0x08000400,0]
        candidate={'start':0x1000,'end_exclusive':0x1020,'body_size':28,'contiguous_aligned':False,'body_ranges':['1000-1013','1018-101f']}
        pack=lambda w:struct.pack('<8I',*w)
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,pack(words)))
        for index,value in [(0,0x10000004),(3,0x03200008),(4,0x10000000),(5,0x24020002)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,pack(changed)))

if __name__ == "__main__":
    unittest.main()


class SwitchCaseGapTests(unittest.TestCase):
    # The reached shape (0x2B3C30): SLTIU on the argument index, BEQ to the default, table jump, and a
    # case block (JR RA / DADDU) reached only through the table, left out of the analysis body.
    words=[0x2C820006,0x1040000A,0,0x00041080,0x3C030000,0x00621821,0x8C631038,0x00600008,0,
           0,0x03E00008,0x0000102D,0x03E00008,0x24020001,0x1030,0x1030,0x1028,0x1028,0x1030,0x1030]
    metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
    candidate={'start':0x1000,'end_exclusive':0x1038,'body_size':44,'contiguous_aligned':False,'body_ranges':['1000-1023','1030-1037']}

    def test_case_block_may_call(self):
        pack=lambda w:struct.pack('<20I',*w)
        called=self.words.copy();called[10]=0x0C000400;called[11]=0 # JAL 0x1000 with a NOP delay slot.
        self.assertTrue(has_switch_case_gaps(self.candidate,self.metadata,pack(called)))

    def test_case_block_may_branch_on_an_fpu_condition(self):
        # A case block with BC1TL into the candidate and a NOP delay slot, falling through into the
        # next span (seen at 0x1E6C74); the same branch leaving the candidate is refused.
        pack=lambda w:struct.pack('<20I',*w)
        fpu=self.words.copy();fpu[9],fpu[10],fpu[11],fpu[16],fpu[17]=0x45030002,0,0,0x1024,0x1024
        self.assertTrue(has_switch_case_gaps(self.candidate,self.metadata,pack(fpu)))
        fpu[9]=0x45030040
        self.assertFalse(has_switch_case_gaps(self.candidate,self.metadata,pack(fpu)))

    def test_switch_bound_and_index_kept_in_the_stack_frame(self):
        # ADDIU sp / SW a0,0x10(sp) / LW v0,0x10(sp) / SLTIU v0,v0,2 / SW v0,0x14(sp) / LW v0,0x14(sp) /
        # BEQ v0,zero,default / LW a0,0x10(sp) (delay) / table jump / NOP; a case block in the hole
        # (0x1519A8 shape). Each slot is stored once and the frame is fixed.
        words=[0x27BDFFE0,0xAFA40010,0x8FA20010,0x2C420002,0xAFA20014,0x8FA20014,0x10400009,0x8FA40010,
               0x00041080,0x3C030000,0x00621821,0x8C631048,0x00600008,0,0x03E00008,0x27BD0020,0x03E00008,0x27BD0020,0x1038,0x1040]
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        candidate={'start':0x1000,'end_exclusive':0x1048,'body_size':64,'contiguous_aligned':False,'body_ranges':['1000-1037','1040-1047']}
        pack=lambda w:struct.pack('<20I',*w)
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(words)))
        # A second store to the condition slot, the index slot's address taken, a bound on another
        # slot's value, a frame that changes, or a doubleword store over the index slot: unproven.
        for index,value in [(13,0xAFA20014),(13,0x27A50010),(2,0x8FA20018),(13,0x27BDFFF8),(13,0xFFA20010)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(changed)))

    def test_switch_bound_kept_in_a_callee_saved_register(self):
        # SW a0,0x10(sp) / LW v1,0x10(sp) / SLTIU s1,v1,2 / ... / BEQ s1,zero,default / LW a0,0x10(sp)
        # (delay) / table jump (0x23AE34 shape): s1 keeps the condition up to the guard.
        words=[0x27BDFFE0,0xAFA40010,0x8FA30010,0x2C710002,0,0,0x12200009,0x8FA40010,
               0x00041080,0x3C030000,0x00621821,0x8C631048,0x00600008,0,0x03E00008,0x27BD0020,0x03E00008,0x27BD0020,0x1038,0x1040]
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x50,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        candidate={'start':0x1000,'end_exclusive':0x1048,'body_size':64,'contiguous_aligned':False,'body_ranges':['1000-1037','1040-1047']}
        pack=lambda w:struct.pack('<20I',*w)
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(words)))
        called=words.copy();called[4]=0x0C000400 # A call in between leaves s1 intact.
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(called)))
        # s1 rewritten on the way, a second SLTIU into s1, a bound on another slot's value, or the
        # condition in a register calls may change (v1 through SLTIU v1 / BEQ v1): unproven.
        for changes in [{4:0x0000882D},{4:0x2C710003},{2:0x8FA30018},{3:0x2C630002,6:0x10600009,9:0x3C020000,10:0x00431021,11:0x8C421048,12:0x00400008,8:0x00041880}]:
            changed=words.copy()
            for index,value in changes.items():changed[index]=value
            self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(changed)))

    def test_case_block_continues_at_its_own_branch_target(self):
        # A case block: BNE a0,zero to the other arm / NOP / JR RA / NOP, then that arm (JR RA / DADDU),
        # reached only by the branch (0x14C988 shape). A branch into the arm's delay slot is refused.
        words=[0x2C820006,0x1040000D,0,0x00041080,0x3C030000,0x00621821,0x8C631044,0x00600008,0,
               0x14800003,0,0x03E00008,0,0x03E00008,0x0000102D,0x03E00008,0x24020001,
               0x103C,0x103C,0x1024,0x1024,0x103C,0x103C]
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x5C,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        candidate={'start':0x1000,'end_exclusive':0x1044,'body_size':44,'contiguous_aligned':False,'body_ranges':['1000-1023','103c-1043']}
        pack=lambda w:struct.pack('<23I',*w)
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(words)))
        slot=words.copy();slot[9]=0x14800004
        self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(slot)))
        # Without the branch the arm is unexplained code in the hole.
        plain=words.copy();plain[9]=0
        self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(plain)))

    def test_fpu_load_in_the_guard_delay_slot(self):
        # The base shape with LWC1 in the guard's delay slot (it writes no integer register; seen at
        # 0x1514E8) and one more padding NOP before the case block.
        words=[0x2C820006,0x1040000B,0xC6990008,0x00041080,0x3C030000,0x00621821,0x8C63103C,0x00600008,0,
               0,0,0x03E00008,0x0000102D,0x03E00008,0x24020001,0x1034,0x1034,0x102C,0x102C,0x1034,0x1034]
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x54,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        candidate={'start':0x1000,'end_exclusive':0x103C,'body_size':44,'contiguous_aligned':False,'body_ranges':['1000-1023','1034-103b']}
        pack=lambda w:struct.pack('<21I',*w)
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(words)))
        # LW a0 in the delay slot rewrites the index: the bound no longer proves the table's length.
        # The table is then read by scanning (six aligned entries inside the candidate) and the
        # holes still pass their own checks; with fewer than four entries nothing is accepted.
        loaded=words.copy();loaded[2]=0x8E840008
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(loaded)))
        short=loaded.copy();short[18]=short[19]=short[20]=0x2000
        self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(short)))

    def test_switch_candidate_may_also_have_a_padding_hole(self):
        # As above, plus a second hole that is only a NOP after JR RA and its delay slot (0x23CDF8 shape).
        words=[0x2C820006,0x1040000A,0,0x00041080,0x3C030000,0x00621821,0x8C631044,0x00600008,0,
               0,0x03E00008,0x0000102D,0x03E00008,0x24020001,0,0x03E00008,0,
               0x1030,0x1030,0x1028,0x1028,0x103C,0x103C]
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x5C,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        candidate={'start':0x1000,'end_exclusive':0x1044,'body_size':52,'contiguous_aligned':False,'body_ranges':['1000-1023','1030-1037','103c-1043']}
        pack=lambda w:struct.pack('<23I',*w)
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(words)))
        # A table target inside the first case block (a case falling through) is accepted, its delay
        # slot is not.
        inner=words.copy();inner[9]=0x24020005;inner[19]=0x1024
        self.assertTrue(has_switch_case_gaps(candidate,metadata,pack(inner)))
        inner[19]=0x102C
        self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(inner)))
        # A case block may contain a conditional branch into the candidate (here BEQ v0 to the default
        # at 0x1030 with a NOP delay slot, replacing the padding and shifting nothing); a branch out of
        # the candidate is refused.
        branchy=words.copy();branchy[19]=0x1024;branchy[20]=0x1024
        branchy[9]=0x10400002;branchy[10]=0;branchy[11]=0x03E00008
        self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(branchy))) # JR RA last: its delay slot leaves the hole.
        outside=words.copy();outside[19]=0x1024;outside[20]=0x1024;outside[9]=0x10400040
        self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(outside)))
        # The padding hole must be NOPs, follow an unconditional transfer, and not be a branch target.
        for index,value in [(14,0x24020001),(12,0x24020002),(1,0x1040000C)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_switch_case_gaps(candidate,metadata,pack(changed)))

    def test_case_block_reached_only_by_the_table(self):
        pack=lambda w:struct.pack('<20I',*w)
        self.assertTrue(has_switch_case_gaps(self.candidate,self.metadata,pack(self.words)))
        self.assertEqual(select_candidates({'functions':[self.candidate]},self.metadata,data=pack(self.words))[0],[{'start':0x1000,'end':0x1038}])
        # A case block may end in a tail jump (J) to code of the program; a jump outside it is refused.
        tail=self.words.copy();tail[10]=0x08000400
        self.assertTrue(has_switch_case_gaps(self.candidate,self.metadata,pack(tail)))
        tail[10]=0x08000800
        self.assertFalse(has_switch_case_gaps(self.candidate,self.metadata,pack(tail)))
        # A last block without a return falls through into the span after the hole: accepted.
        fall=self.words.copy();fall[10]=0
        self.assertTrue(has_switch_case_gaps(self.candidate,self.metadata,pack(fall)))
        # ... but not a branch at the hole's end, whose delay slot would leave the hole.
        fall[11]=0x1000FFFE
        self.assertFalse(has_switch_case_gaps(self.candidate,self.metadata,pack(fall)))
        # A broken bound or guard leaves the table to be read by scanning, which this fixture's table
        # is long enough for; the holes are still checked.
        for index,value in [(0,0x2C820000),(1,0x1440000A)]:
            changed=self.words.copy();changed[index]=value
            self.assertTrue(has_switch_case_gaps(self.candidate,self.metadata,pack(changed)))
        # Each of these still breaks it: a table target outside, padding that is not NOP, a control
        # instruction in the delay slot, no table target in the hole.
        for index,value in [(16,0x1040),(9,0x24020001),(11,0x03E00008),(16,0x1030)]:
            changed=self.words.copy();changed[index]=value
            if index==16:changed[17]=value
            self.assertFalse(has_switch_case_gaps(self.candidate,self.metadata,pack(changed)))
