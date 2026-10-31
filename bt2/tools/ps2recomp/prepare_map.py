"""Prepare a provisional EE map automatically; quarantine ambiguous candidates.

No function purpose or boundary is certified. Raw maps/reports remain ignored.
"""

from collections import Counter
import hashlib
import importlib.util
import json
import struct
from pathlib import Path

from generate_game import ROOT, LOCAL_CANDIDATE_SHA256
import overlay_programs


def has_tail_nop_gap(candidate, metadata, data):
    """Accept one unreachable NOP between a direct tail jump and alternate JR RA."""
    start, end = candidate["start"], candidate["end_exclusive"]
    ranges = candidate.get("body_ranges", [])
    if start % 4 or end % 4 or end - start > 0x5000 or len(ranges) != 2:
        return False
    try:
        spans = [(int(a, 16), int(b, 16) + 1) for a, b in (r.split("-") for r in ranges)]
    except (ValueError, TypeError):
        return False
    gap = spans[0][1]
    if spans != [(start, gap), (gap + 4, end)] or end != gap + 12 or gap - start < 8:
        return False
    if candidate["body_size"] != end - start - 4 or gap % 4:
        return False
    sections = [row for row in metadata["sections"] if row["name"] == ".text"
                and row["flags"] & 4 and row["occupies_file_bytes"]
                and row["address"] <= start < end <= row["address"] + row["size"]]
    if len(sections) != 1:
        return False
    offset = sections[0]["file_offset"] + start - sections[0]["address"]
    if offset < 0 or offset + end - start > len(data):
        return False
    def word(address):
        return struct.unpack_from("<I", data, offset + address - start)[0]
    jump = word(gap - 8)
    destination = ((gap - 4) & 0xF0000000) | ((jump & 0x03FFFFFF) << 2)
    if word(gap) != 0 or jump >> 26 != 2 or start <= destination < end or word(gap + 4) != 0x03E00008:
        return False
    def control(ins):
        op = ins >> 26
        return op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and ins & 63 in (8, 9))
    if control(word(gap - 4)) or control(word(gap + 8)):
        return False
    for address in range(start, end, 4):
        ins = word(address)
        op = ins >> 26
        if (op == 0 and ins & 63 in (8, 9) and ins != 0x03E00008) or (op in (16, 17, 18) and (ins >> 21) & 31 >= 8):
            return False
        if op in (1, 4, 5, 6, 7, 20, 21, 22, 23):
            immediate = ins & 0xFFFF
            if immediate & 0x8000:
                immediate -= 0x10000
            if address + 4 + immediate * 4 == gap:
                return False
        elif op in (2, 3) and (((address + 4) & 0xF0000000) | ((ins & 0x03FFFFFF) << 2)) == gap:
            return False
    return True


def cop_branch(ins):
    """BC0/BC1/BC2 condition branches: an immediate target like other conditional branches."""
    return ins>>26 in (16,17,18) and (ins>>21)&31==8


def cop_control(ins):
    """Coprocessor instructions that change control flow or are not proven safe to bridge over:
    condition branches, and COP0 CO-group operations (ERET and friends). COP1/COP2 arithmetic
    (rs >= 16) is ordinary data flow."""
    op,rs=ins>>26,(ins>>21)&31
    return cop_branch(ins) or (op==16 and rs>=16) or (op in (16,17,18) and 8<rs<16)


def fpu_only(ins):
    """COP1 instructions that write no integer register: S/W-format arithmetic, conversions and
    compares (rs 16 or 20) and MTC1 (rs 4)."""
    return (ins>>26==17 and (ins>>21)&31 in (4,16,20)) or ins>>26 in (0x31,0x36) # Also LWC1 and LQC2 loads.


def spilled_bound(address,start,end,c,i,word,control,writes):
    """The bound of a switch whose index is reloaded from the stack frame in the guard's delay slot:
    BEQ c,zero / LW i,I(sp) (delay) / SLL ... / JR at `address`. Returns the SLTIU word.

    Proven from the whole candidate: the frame is fixed (ADDIU sp,sp,-N first; sp otherwise only
    restored) and slot I is stored once, before any branch. The condition is SLTIU r,x,N of x
    reloaded from slot I, and reaches the guard in one of two ways:
    - through slot C (LW c,C(sp) right before the guard, seen at 0x1519A8): stored once, from r;
    - in a callee-saved register (r = c, seen at 0x23AE34): the only SLTIU into c, nothing between it
      and the guard writes c, no branch from outside enters that stretch, and every other table jump
      of the candidate lies inside it.
    No other store overlaps a slot or takes its address. Writes through pointers to other locals are
    assumed not to reach the slots."""
    sign=lambda v:v-65536 if v&32768 else v
    guard=address-24
    load_c,load_i,frame=word(address-28),word(address-20),word(start)
    if load_i>>16!=(0x8FA0|i) or frame>>16!=0x27BD:return None
    slot_i,size=sign(load_i&65535),-sign(frame&65535)
    slot_c=sign(load_c&65535) if load_c>>16==(0x8FA0|c) else None
    slots=[slot_i]+([slot_c] if slot_c is not None else [])
    if size<=0 or len(set(slots))!=len(slots) or min(slots)<0 or max(slots)+4>size or any(x%4 for x in slots):return None
    call=lambda ins:ins>>26==3 or (ins>>26==0 and ins&0x1FFFFF==0xF809 and (ins>>21)&31!=0)
    widths={0x28:1,0x29:2,0x2B:4,0x39:4,0x3F:8,0x3D:8,0x1F:16,0x3E:16}
    stores={slot:[] for slot in slots}
    for at in range(start+4,end,4):
        ins=word(at);op,rs,rt=ins>>26,(ins>>21)&31,(ins>>16)&31
        if op in (0,28) and (ins>>11)&31==29 and ins:return None # R-type or MMI result in sp.
        if rt==29 and (op in (8,9,10,11,12,13,14,15,24,25,26,27,0x37) or 0x20<=op<=0x27):
            if not (op==9 and rs==29 and sign(ins&65535)==size):return None # Only the frame's release.
        if op==9 and rs==29 and rt!=29 and sign(ins&65535) in stores:return None # Address of a slot taken.
        if rs!=29 or not (op in widths or op in (0x2A,0x2C,0x2D,0x2E)):continue
        offset=sign(ins&65535)
        low,high=(offset,offset+widths[op]) if op in widths else (offset-7,offset+8) # Unaligned stores: both ways.
        for slot in stores:
            if low<slot+4 and slot<high:
                if op!=0x2B or offset!=slot:return None
                stores[slot].append(at)
    if any(len(found)!=1 for found in stores.values()):return None
    if any(control(word(at)) and not call(word(at)) for at in range(start,stores[slot_i][0]+4,4)):return None
    def from_index_slot(back,bound):
        # x of SLTIU r,x,N at `back` was just reloaded from slot I.
        x=(bound>>21)&31
        for earlier in range(back-4,max(back-20,start),-4):
            ins=word(earlier)
            if ins==(0x8FA00000|(x<<16)|slot_i):return True
            # The SLTIU may sit in the delay slot of a conditional, non-likely branch: it runs either way.
            slot_of_branch=earlier==back-4 and (ins>>26 in (4,5,6,7) or (ins>>26==1 and (ins>>16)&31 in (0,1)))
            if not slot_of_branch and (control(ins) or writes(ins,x)):return False
        return False
    if slot_c is not None:
        at=stores[slot_c][0];r=(word(at)>>16)&31
        for back in range(at-4,max(at-20,start),-4):
            bound=word(back)
            if bound>>26==11 and (bound>>16)&31==r:break
            if control(bound) or writes(bound,r):return None
        else:return None
        return bound if from_index_slot(back,bound) else None
    if c not in (16,17,18,19,20,21,22,23,30):return None
    bounds=[at for at in range(start,end,4) if word(at)>>26==11 and (word(at)>>16)&31==c]
    if len(bounds)!=1 or not bounds[0]<guard or not from_index_slot(bounds[0],word(bounds[0])):return None
    back=bounds[0]
    for at in range(back+4,guard,4):
        ins=word(at)
        if not control(ins) and writes(ins,c):return None # Calls and branches leave a callee-saved register.
    for at in range(start,end,4):
        ins=word(at);op=ins>>26
        if op in (1,4,5,6,7,20,21,22,23) or cop_branch(ins):target=at+4+sign(ins&65535)*4
        elif op==2:target=((at+4)&0xF0000000)|((ins&0x03FFFFFF)<<2)
        else:
            if op==0 and ins&0xFC1FFFFF==8 and ins!=0x03E00008 and at!=address and not back<at<guard:return None
            continue
        if back<target<=guard and not back-4<=at<=guard:return None # A branch with the SLTIU in its delay slot passes it.
    return word(back)


def bounded_word_switch(address,start,end,spans,metadata,data,word):
    return switch_targets(address,start,end,spans,metadata,data,word) is not None


def switch_targets(address,start,end,spans,metadata,data,word):
    """Prove the reached sltiu/beq/sll/lui/addu/lw/jr table idiom.

    Safety rests on the SLTIU c,i,N bound and its guard branch on c; the instruction producing the
    index i only has to be non-control. With BEQ, SLL is its delay slot, or a non-control delay slot that
    leaves the index register intact precedes SLL (it runs on both paths). With BEQL, the delay slot
    runs only on the out-of-range branch, so it may be any non-control instruction and SLL follows it."""
    def control(ins):
        op=ins>>26
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9,12,13)) or cop_control(ins)
    sign=lambda v:v-65536 if v&32768 else v
    # The shortest idiom is the whole head of a function: SLTIU first, the JR 24 bytes after it.
    if address-start<24 or address+8>end or word(address+4)!=0:return None
    jr=word(address)
    tail=[word(address+n) for n in (-16,-12,-8,-4)] if address-start>=16 else [0]*4
    # First layout: SLTIU c,i,N / BEQ(L) c,zero / [delay] / SLL s,i,2 / LUI b / ADDU b,b,s / LW j,d(b) / JR j.
    # Registers are decoded (seen with index v1 -> v0 and index v0 -> v1 roles).
    sll,lui,addu,load=tail
    first=(jr&0xFC1FFFFF==8 and sll&0xFFE0003F==0 and (sll>>6)&31==2 and lui&0xFFE00000==0x3C000000
        and addu&0xFC0007FF==0x21 and (addu>>11)&31==(lui>>16)&31 and {(addu>>21)&31,(addu>>16)&31}=={(lui>>16)&31,(sll>>11)&31}
        and load>>26==35 and (load>>21)&31==(lui>>16)&31 and (load>>16)&31==(jr>>21)&31)
    if first:
        i,sreg,b=(sll>>16)&31,(sll>>11)&31,(lui>>16)&31
        if 0 in (i,sreg,b) or b==sreg:return None
        spilled=False
        def keeps_index(ins):
            # A delay slot that runs on both paths must not write the index: I-type ALU/load
            # write rt, R-type ALU writes rd, stores (SB/SH/SW/SD/SQ) write no register. Anything
            # else is not proven.
            op=ins>>26
            if fpu_only(ins):return True
            if op==0:return ins&63 in (0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2A,0x2B,0x2C,0x2D,0x00,0x02,0x03) and (ins>>11)&31!=i
            if op in (0x28,0x29,0x2B,0x3F,0x1F,0x39,0x3D):return True # Stores, including SWC1/SDC1.
            return op in (8,9,10,11,12,13,14,15,0x20,0x21,0x23,0x24,0x25,0x37) and (ins>>16)&31!=i
        def beq_zero(ins,opcode):return ins>>26==opcode and (ins>>16)&31==0
        if beq_zero(word(address-20),4):branch=address-20
        elif address-start>=32 and beq_zero(word(address-24),20) and not control(word(address-20)):branch=address-24
        elif address-start>=28 and beq_zero(word(address-24),4) and not control(word(address-20)) and keeps_index(word(address-20)):
            branch=address-24 # BEQ whose delay slot (both paths) leaves the index intact; SLL follows it.
        elif address-start>=32 and beq_zero(word(address-24),4):
            branch=address-24;spilled=True # Bound and index reloaded from the stack frame; proven below.
        else:return None
        # The index producer precedes the bound; at the function start the index is an incoming argument.
        # The bound may instead sit in the delay slot of a conditional, non-likely branch (a sign test
        # of the index): it runs before the fall-through path reaches the guard.
        # Between the bound and the guard only instructions proven not to write the condition or the
        # index may stand (register saves in a prologue): stores write no integer register, R-type ALU
        # writes rd, I-type ALU and loads write rt. Anything else is not proven.
        def writes(ins,reg):
            op=ins>>26
            if op in (0x28,0x29,0x2B,0x3F,0x1F,0x39,0x3D) or fpu_only(ins):return False
            if op==0 and ins&63 in (0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2A,0x2B,0x2C,0x2D,0x00,0x02,0x03):return (ins>>11)&31==reg
            if op in (8,9,10,11,12,13,14,15,0x20,0x21,0x23,0x24,0x25,0x37):return (ins>>16)&31==reg
            return True
        c=(word(branch)>>21)&31
        if c in (0,i):return None
        if spilled:
            bound=spilled_bound(address,start,end,c,i,word,control,writes)
            if bound is None:return None
        else:
            at=branch-4
            while True:
                if at<start or branch-at>64:return None
                bound=word(at)
                if bound>>26==11 and (bound>>21)&31==i and (bound>>16)&31==c:break
                if control(bound) or writes(bound,c) or writes(bound,i):return None
                at-=4
            index=word(at-4) if at-4>=start else 0
            op,rs,rt=index>>26,(index>>21)&31,(index>>16)&31
            falls_through=op in (5,6,7) or (op==4 and rs!=rt) or (op==1 and rt in (0,1))
            if control(index) and not falls_through:return None
        hi,disp=lui&65535,sign(load&65535)
    else:
        # Second layout: SLTIU c,i,N / BEQ c,zero / LUI b (delay) / SLL s,i,2 / ADDIU b,b,lo /
        # ADDU a,s,b / LW j,0(a) / JR j. Registers are decoded and must keep the index intact.
        jr=word(address)
        if address-start<32 or jr&0xFC1FFFFF!=8:return None
        j=(jr>>21)&31;branch=address-24
        bound,beq,lui,sll,addiu,addu,load=(word(address+n) for n in (-28,-24,-20,-16,-12,-8,-4))
        c,i=(bound>>16)&31,(bound>>21)&31;b=(lui>>16)&31;sreg=(sll>>11)&31;a=(addu>>11)&31
        if bound>>26!=11 or beq&0xFFFF0000!=0x10000000|(c<<21) or lui&0xFFE00000!=0x3C000000 or sll&0xFFE0003F!=0 or (sll>>16)&31!=i or (sll>>6)&31!=2:return None
        if addiu>>26!=9 or (addiu>>21)&31!=b or (addiu>>16)&31!=b or addu&0xFC0007FF!=0x21 or {(addu>>21)&31,(addu>>16)&31}!={sreg,b}:return None
        if load&0xFFFF0000!=0x8C000000|(a<<21)|(j<<16) or 0 in (c,i,b,sreg,a,j) or b in (i,sreg) or c==i:return None
        hi,disp=lui&65535,sign(addiu&65535)+sign(load&65535) # LW displacement is part of the table address.
    count=bound&65535
    # The bound itself, the table lying in file-backed bytes and every target inside the function
    # are what make a table safe; its length is not (302 cases seen in the overlay).
    if not 1<=count<=4096:return None
    immediate=word(branch)&65535;immediate=immediate-65536 if immediate&32768 else immediate
    default=branch+4+immediate*4
    if not any(a<=default<b for a,b in spans):return None
    table=(hi<<16)+disp
    regions=[x for x in metadata['sections'] if x.get('occupies_file_bytes') and x['address']<=table and table+count*4<=x['address']+x['size']]
    if len(regions)!=1 or table%4:return None
    at=regions[0]['file_offset']+table-regions[0]['address']
    if at<0 or at+count*4>len(data):return None
    targets=struct.unpack_from('<'+str(count)+'I',data,at)
    return targets if all(t%4==0 and any(a<=t<b for a,b in spans) for t in targets) else None


def scanned_table_targets(address,start,end,metadata,data,word):
    """The table of a JR whose bound is not next to it (the guard is reached from elsewhere in the
    function, or the index comes back from the stack): SLL s,i,2 / LUI b / ADDU b,b,s / LW j,d(b) /
    JR j, with the table read as far as its words are aligned addresses inside the candidate.

    Without the bound the length is not proven, so the result may hold more words than the game
    uses; that only names more places of the candidate as possible targets. What makes it safe to
    take the holes as this function's code is checked by the caller as for a proven switch: every
    hole starts at a table target and holds straight-line case blocks, and nothing else branches in.
    At least four entries are required, so that stray data is not taken for a table."""
    if address-start<16 or address+8>end or word(address+4)!=0:return None
    jr=word(address);sll,lui,addu,load=(word(address+n) for n in (-16,-12,-8,-4))
    if not (jr&0xFC1FFFFF==8 and sll&0xFFE0003F==0 and (sll>>6)&31==2 and lui&0xFFE00000==0x3C000000
            and addu&0xFC0007FF==0x21 and (addu>>11)&31==(lui>>16)&31 and {(addu>>21)&31,(addu>>16)&31}=={(lui>>16)&31,(sll>>11)&31}
            and load>>26==35 and (load>>21)&31==(lui>>16)&31 and (load>>16)&31==(jr>>21)&31):return None
    disp=load&65535;disp=disp-65536 if disp&32768 else disp
    table=((lui&65535)<<16)+disp
    regions=[x for x in metadata['sections'] if x.get('occupies_file_bytes') and x['address']<=table<x['address']+x['size']]
    if len(regions)!=1 or table%4:return None
    targets=[]
    while len(targets)<4096 and table+len(targets)*4+4<=regions[0]['address']+regions[0]['size']:
        at=regions[0]['file_offset']+table-regions[0]['address']+len(targets)*4
        if at<0 or at+4>len(data):break
        entry=struct.unpack_from('<I',data,at)[0]
        if entry%4 or not start<=entry<end:break
        targets.append(entry)
    return tuple(targets) if len(targets)>=4 else None


def has_branch_nop_gaps(candidate, metadata, data, external_text=()):
    """Accept isolated unreachable NOPs after an unconditional branch/jump and delay slot.

    external_text: extra (start, end) code ranges a J may target, e.g. the main executable's .text
    for an overlay program (cross-program tail jumps)."""
    start,end=candidate['start'],candidate['end_exclusive']
    try:spans=[(int(a,16),int(b,16)+1) for a,b in (r.split('-') for r in candidate.get('body_ranges',[]))]
    except (ValueError,TypeError):return False
    if start%4 or end%4 or end-start>0x5000 or len(spans)<2 or spans[0][0]!=start or spans[-1][1]!=end:return False
    if any(a%4 or b%4 or b<=a for a,b in spans):return False
    if any(right[0]!=left[1]+4 for left,right in zip(spans,spans[1:])):return False
    gaps={b for a,b in spans[:-1]}
    if candidate['body_size']!=end-start-4*len(gaps):return False
    sections=[x for x in metadata['sections'] if x['name']=='.text' and x['flags']&4 and x['occupies_file_bytes'] and x['address']<=start<end<=x['address']+x['size']]
    if len(sections)!=1:return False
    offset=sections[0]['file_offset']+start-sections[0]['address']
    if offset<0 or offset+end-start>len(data):return False
    def word(address):return struct.unpack_from('<I',data,offset+address-start)[0]
    def control(ins):
        op=ins>>26
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9)) or cop_control(ins)
    for gap in gaps:
        if gap-start<8 or word(gap)!=0 or control(word(gap-4)):return False
        transfer=word(gap-8)
        if transfer&0xFFFF0000==0x10000000:
            immediate=transfer&65535;immediate=immediate-65536 if immediate&32768 else immediate
            target=gap-4+immediate*4
            if not start<=target<end:return False
        elif transfer>>26==2:
            target=((gap-4)&0xF0000000)|((transfer&0x03FFFFFF)<<2)
            if not (sections[0]['address']<=target<sections[0]['address']+sections[0]['size'] or any(a<=target<b for a,b in external_text)):return False
        elif transfer==0x03E00008:
            continue # JR RA returns; the body scan below still rejects any branch into the gap.
        elif bounded_word_switch(gap-8,start,end,spans,metadata,data,word):
            continue # Every proven table target lies inside the body; none is the gap.
        else:return False
        if target in gaps:return False
    for address in range(start,end,4):
        ins=word(address);op=ins>>26
        # Standard JALR ra,rs is a call with a local continuation. Unknown
        # tail jumps/link registers remain quarantined; runtime still checks targets.
        standard_call=op==0 and ins&0x1FFFFF==0xF809 and (ins>>21)&31!=0
        if (op==0 and ins&63 in (8,9) and ins!=0x03E00008 and not standard_call and not bounded_word_switch(address,start,end,spans,metadata,data,word)) or (cop_control(ins) and not cop_branch(ins)):return False
        if op in (1,4,5,6,7,20,21,22,23) or cop_branch(ins):
            immediate=ins&65535;immediate=immediate-65536 if immediate&32768 else immediate
            if address+4+immediate*4 in gaps:return False
        elif op in (2,3) and (((address+4)&0xF0000000)|((ins&0x03FFFFFF)<<2)) in gaps:return False
    return True


def has_switch_case_gaps(candidate, metadata, data):
    """Accept holes that hold case blocks of a proven jump-table switch Ghidra did not follow.

    The body must contain at least one switch: bounded (switch_targets, with every table target inside
    the candidate) or, where the bound is not next to the jump, with its table read by scanning
    (scanned_table_targets); several switches share the same rules. Each hole must contain at least one table target; its words are NOP padding before
    the first target, then straight-line case blocks (calls included), each starting exactly at a table
    target (or inside the previous block when that one falls through into it, never in a delay slot) and ending in JR RA, a tail jump (J) into this program's code or an unconditional branch to the candidate, with a non-control delay slot,
    all inside the hole (conditional branches into the candidate may occur inside a block); the last block may instead fall through into the span after the hole. Between blocks only NOP padding is allowed. Nothing else may branch into a hole."""
    start,end=candidate['start'],candidate['end_exclusive']
    try:spans=[(int(a,16),int(b,16)+1) for a,b in (r.split('-') for r in candidate.get('body_ranges',[]))]
    except (ValueError,TypeError):return False
    if start%4 or end%4 or end-start>0x5000 or len(spans)<2 or spans[0][0]!=start or spans[-1][1]!=end:return False
    if any(a%4 or b%4 or b<=a for a,b in spans) or any(right[0]<=left[1] for left,right in zip(spans,spans[1:])):return False
    if candidate['body_size']!=sum(b-a for a,b in spans):return False
    sections=[x for x in metadata['sections'] if x['name']=='.text' and x['flags']&4 and x['occupies_file_bytes'] and x['address']<=start<end<=x['address']+x['size']]
    if len(sections)!=1:return False
    offset=sections[0]['file_offset']+start-sections[0]['address']
    if offset<0 or offset+end-start>len(data):return False
    def word(address):return struct.unpack_from('<I',data,offset+address-start)[0]
    def control(ins):
        op=ins>>26
        return op in (1,2,3,4,5,6,7,20,21,22,23) or (op==0 and ins&63 in (8,9,12,13)) or cop_control(ins)
    whole=[(start,end)]
    switches=[(a,t) for a in range(start,end,4) if any(x<=a<y for x,y in spans) and word(a)&0xFC1FFFFF==8 and word(a)!=0x03E00008
              for t in [switch_targets(a,start,end,whole,metadata,data,word) or scanned_table_targets(a,start,end,metadata,data,word)] if t is not None]
    if not switches:return False
    jumps={address for address,_ in switches};targets={t for _,table in switches for t in table}
    holes=[(left[1],right[0]) for left,right in zip(spans,spans[1:])]
    hole_words=set();walked=set();all_slots=set();inner_branches=[]
    for h0,h1 in holes:
        inside=sorted(t for t in targets if h0<=t<h1)
        if not inside:
            # A hole without case blocks: only NOP padding after an unconditional transfer (B into the
            # candidate, J, JR RA or a proven switch jump) and its non-control delay slot. The scan below rejects branches into it.
            before=word(h0-8) if h0-start>=8 else 0
            local=before&0xFFFF0000==0x10000000 and start<=h0-4+((before&65535)-(65536 if before&32768 else 0))*4<end
            if any(word(a)!=0 for a in range(h0,h1,4)) or control(word(h0-4)) or not (local or before>>26==2 or before==0x03E00008 or h0-8 in jumps):return False
            hole_words.update(range(h0,h1,4));continue
        # Blocks start at the table targets and at the targets of branches inside this hole's own
        # blocks (the other arm of a condition inside a case, placed after the arm's closing branch).
        address=h0;slots=set();block_starts=set(inside);visited=set()
        while block_starts-visited:
            target=min(block_starts-visited);visited.add(target)
            if target<address:
                # A target inside an earlier block: a case that the previous one falls through into.
                # It is another entry of that straight-line code unless it is a delay slot.
                if target in slots:return False
                continue
            if any(word(a)!=0 for a in range(address,target,4)):return False # Only padding before a block.
            address=target
            while True:
                if address==h1:break # Falls through into the span after the hole.
                ins=word(address)
                if control(ins) and address+4>=h1:return False # Its delay slot would leave the hole.
                walked.add(address)
                if not control(ins):address+=4;continue
                # A call (JAL, or JALR rd=ra) returns after its delay slot: straight-line within the block.
                call=ins>>26==3 or (ins>>26==0 and ins&0x1FFFFF==0xF809 and (ins>>21)&31!=0)
                slots.add(address+4);all_slots.add(address+4)
                if call and not control(word(address+4)):address+=8;continue
                # A conditional branch into the candidate keeps the block going after its delay slot; a
                # target inside a hole must be an instruction of a case block (checked below).
                if (ins>>26 in (5,6,7,21,22,23) or (ins>>26 in (4,20) and (ins>>21)&31!=(ins>>16)&31) or (ins>>26==1 and (ins>>16)&31 in (0,1,2,3)) or cop_branch(ins)) and not control(word(address+4)):
                    immediate=ins&65535;immediate=immediate-65536 if immediate&32768 else immediate
                    if not start<=address+4+immediate*4<end:return False
                    inner_branches.append(address+4+immediate*4)
                    if h0<=address+4+immediate*4<h1:block_starts.add(address+4+immediate*4)
                    address+=8;continue
                if ins==0x03E00008:pass
                elif ins&0xFFFF0000==0x10000000:
                    immediate=ins&65535;immediate=immediate-65536 if immediate&32768 else immediate
                    if not start<=address+4+immediate*4<end:return False
                    inner_branches.append(address+4+immediate*4)
                    if h0<=address+4+immediate*4<h1:block_starts.add(address+4+immediate*4)
                elif ins>>26==2:
                    # A tail jump (J) to code of this program ends the block like a return.
                    target=((address+4)&0xF0000000)|((ins&0x03FFFFFF)<<2)
                    if not sections[0]['address']<=target<sections[0]['address']+sections[0]['size']:return False
                else:return False
                if control(word(address+4)):return False
                address+=8;break
        if any(word(a)!=0 for a in range(address,h1,4)):return False
        hole_words.update(range(h0,h1,4))
    if any(target in hole_words and (target not in walked or target in all_slots) for target in inner_branches):return False
    # The spans: no transfer into a hole except the switch itself; no other indirect jump.
    for address in range(start,end,4):
        if address in hole_words:continue
        ins=word(address);op=ins>>26
        standard_call=op==0 and ins&0x1FFFFF==0xF809 and (ins>>21)&31!=0
        if (op==0 and ins&63 in (8,9) and ins!=0x03E00008 and not standard_call and address not in jumps) or (cop_control(ins) and not cop_branch(ins)):return False
        if op in (1,4,5,6,7,20,21,22,23) or cop_branch(ins):
            immediate=ins&65535;immediate=immediate-65536 if immediate&32768 else immediate
            if address+4+immediate*4 in hole_words:return False
        elif op in (2,3) and (((address+4)&0xF0000000)|((ins&0x03FFFFFF)<<2)) in hole_words:return False
    return True


def split_merged(candidate, metadata, data, owned):
    """The separate pieces of a candidate the analysis merged from code that is not one function.

    A body range that ends in a completed transfer (J or JR RA with its delay slot) followed by a
    gap that is not NOP padding ends a piece: what the analysis attached beyond it (a shared tail the
    function jumps to, often far away, before or after it) is code of its own. The piece at the
    candidate's start is the function; another piece becomes a candidate only where no other candidate's extent holds its start
    (`owned`: sorted (start, end) extents), so a jump into the middle of another function adds
    nothing. Returns the pieces as candidates, or None when the candidate is not such a merge."""
    try:
        spans = sorted((int(a, 16), int(b, 16) + 1) for a, b in (r.split("-") for r in candidate.get("body_ranges", [])))
    except (ValueError, TypeError):
        return None
    if len(spans) < 2 or all(a != candidate["start"] for a, b in spans) or any(a % 4 or b % 4 for a, b in spans):
        return None
    sections = [row for row in metadata["sections"] if row.get("occupies_file_bytes")]
    def word(address):
        for row in sections:
            if row["address"] <= address <= row["address"] + row["size"] - 4:
                at = row["file_offset"] + address - row["address"]
                return struct.unpack_from("<I", data, at)[0] if 0 <= at <= len(data) - 4 else None
        return None
    pieces = [[spans[0]]]
    for previous, span in zip(spans, spans[1:]):
        transfer = word(previous[1] - 8) if previous[1] - previous[0] >= 8 else None
        gap = [word(at) for at in range(previous[1], span[0], 4)]
        completed = transfer is not None and (transfer == 0x03E00008 or transfer >> 26 == 2)
        padding = len(gap) <= 3 and all(w == 0 for w in gap)
        if completed and not padding and span[0] > previous[1]:
            pieces.append([span])
        else:
            pieces[-1].append(span)
    if len(pieces) < 2:
        return None
    def held(address):
        import bisect
        at = bisect.bisect_right(owned, (address, 1 << 62)) - 1
        return at >= 0 and owned[at][0] <= address < owned[at][1] and owned[at] != (candidate["start"], candidate["end_exclusive"])
    result = []
    if all(piece[0][0] != candidate["start"] for piece in pieces):
        return None # The start lies inside a piece: not the simple merge this takes apart.
    for piece in pieces:
        start, end = piece[0][0], piece[-1][1]
        if start != candidate["start"] and held(start):
            continue
        size = sum(b - a for a, b in piece)
        result.append({"start": start, "end_exclusive": end, "body_size": size, "contiguous_aligned": len(piece) == 1,
                       "body_ranges": ["%08x-%08x" % (a, b - 1) for a, b in piece]})
    return result


def select_candidates(report, metadata, max_size=0x5000, data=None, external_text=()):
    rows, excluded = [], []
    functions = report["functions"]
    if data is not None:
        # Candidates merged from separate code are taken apart first; a piece that another candidate
        # already starts at is not added twice.
        owned = sorted((c["start"], c["end_exclusive"]) for c in functions if 0 < c["end_exclusive"] - c["start"] <= max_size)
        starts = {c["start"] for c in functions}
        expanded = []
        for candidate in functions:
            pieces = None if candidate.get("contiguous_aligned") else split_merged(candidate, metadata, data, owned)
            if pieces is None:
                expanded.append(candidate)
                continue
            for piece in pieces:
                if piece["start"] != candidate["start"] and piece["start"] in starts:
                    continue
                starts.add(piece["start"])
                expanded.append(piece)
        functions = expanded
    for candidate in functions:
        start, end = candidate["start"], candidate["end_exclusive"]
        reason = None
        padding = data is not None and (has_tail_nop_gap(candidate, metadata, data) or has_branch_nop_gaps(candidate, metadata, data, external_text)
                                        or has_switch_case_gaps(candidate, metadata, data))
        if not padding and (not candidate["contiguous_aligned"] or start % 4 or end % 4 or end <= start or candidate["body_size"] != end - start):
            reason = "noncontiguous_or_invalid"
        elif not any(section["name"] == ".text" and section["flags"] & 4 and section["occupies_file_bytes"]
                     and section["address"] <= start < end <= section["address"] + section["size"] for section in metadata["sections"]):
            reason = "outside_file_backed_ee_text"
        elif end - start > max_size:
            reason = "oversized"
        if reason:
            excluded.append({"start": start, "end": end, "reason": reason})
        else:
            rows.append({"start": start, "end": end})
    # Quarantine every member of an overlapping group, rather than choosing a guessed winner.
    rows.sort(key=lambda row: (row["start"], row["end"]))
    selected, group, group_end = [], [], 0
    def finish():
        if len(group) == 1:
            selected.extend(group)
        else:
            excluded.extend({**row, "reason": "overlap_or_duplicate"} for row in group)
    for row in rows:
        if group and row["start"] >= group_end:
            finish()
            group = []
        group.append(row)
        group_end = max(row["end"], group_end) if len(group) > 1 else row["end"]
    if group:
        finish()
    if not selected:
        raise ValueError("No eligible EE candidates.")
    return selected, excluded


def add_entries(selected, entries, metadata, data, max_size=0x5000):
    """Functions at entry addresses the analysis did not make functions of: targets of calls, jumps
    and pointers found by tools/ps2recomp/map_gaps.py, and addresses a run stopped at.

    An entry in unmapped code becomes a function from there to its end: instructions are followed in
    address order, conditional and unconditional branches must stay at or after the entry, and the
    function ends after the first completed transfer (JR RA or J with its delay slot) that no
    earlier branch reaches beyond; it must end before the next function begins, except a stub whose
    last instruction before the next function is a SYSCALL (a call that does not return). An entry inside a
    selected function splits it there, provided the code before the entry is complete (a transfer
    and its delay slot, then at most three NOPs) and no branch crosses the entry in either
    direction. Anything else is left alone and reported. Returns (rows, notes): the map with the new
    rows, and for each entry what was done."""
    sign = lambda v: v - 65536 if v & 32768 else v
    sections = [row for row in metadata["sections"] if row["name"] == ".text" and row["flags"] & 4 and row["occupies_file_bytes"]]
    def word(address):
        for row in sections:
            if row["address"] <= address <= row["address"] + row["size"] - 4:
                at = row["file_offset"] + address - row["address"]
                return struct.unpack_from("<I", data, at)[0] if 0 <= at <= len(data) - 4 else None
        return None
    def branch_target(at, ins):
        op = ins >> 26
        if op in (4, 5, 6, 7, 20, 21, 22, 23) or (op == 1 and (ins >> 16) & 31 in (0, 1, 2, 3, 16, 17, 18, 19)) or cop_branch(ins):
            return at + 4 + sign(ins & 65535) * 4
        return None
    def complete_before(address, lowest):
        at = address
        for _ in range(3):
            if at - 4 >= lowest and word(at - 4) == 0 and word(at - 8) not in (0x03E00008,) and (word(at - 8) or 0) >> 26 != 2:
                at -= 4
            else:
                break
        transfer = word(at - 8) if at - 8 >= lowest else None
        return transfer is not None and (transfer == 0x03E00008 or transfer >> 26 == 2)
    rows = sorted(({"start": r["start"], "end": r["end"]} for r in selected), key=lambda r: r["start"])
    notes = []
    for entry in sorted(set(entries)):
        starts = [r["start"] for r in rows]
        import bisect
        index = bisect.bisect_right(starts, entry) - 1
        inside = rows[index] if index >= 0 and rows[index]["start"] <= entry < rows[index]["end"] else None
        if entry % 4 or word(entry) is None:
            notes.append({"entry": entry, "result": "not aligned code of this program"}); continue
        if inside and inside["start"] == entry:
            notes.append({"entry": entry, "result": "already a function"}); continue
        if inside:
            if not complete_before(entry, inside["start"]):
                notes.append({"entry": entry, "result": "inside a function whose code runs into it"}); continue
            crossing = False
            for at in range(inside["start"], inside["end"], 4):
                target = branch_target(at, word(at))
                if target is not None and (at < entry) != (target < entry):
                    crossing = True; break
            if crossing:
                notes.append({"entry": entry, "result": "inside a function that branches across it"}); continue
            rows.insert(index + 1, {"start": entry, "end": inside["end"]}); inside["end"] = entry
            notes.append({"entry": entry, "result": "split from the function at 0x%08X" % inside["start"]}); continue
        limit = rows[index + 1]["start"] if index + 1 < len(rows) else None
        end, reach, at, failure = None, entry, entry, None
        while True:
            if limit is not None and at >= limit and at > entry and (word(at - 4) or 0) & 0xFC00003F == 0x0C and reach <= at:
                # A stub ending in a system call that does not return (a thread's exit): it ends where
                # the next function begins; were the call to return, the generated function would end
                # at its own end and dispatch would refuse the return.
                end = at; break
            if (limit is not None and at >= limit) or at - entry >= max_size:
                failure = "runs into the next function" if limit is not None and at >= limit else "too long"; break
            ins = word(at)
            if ins is None:
                failure = "leaves the code section"; break
            target = branch_target(at, ins)
            if target is not None:
                if target < entry or (limit is not None and target >= limit):
                    failure = "branches outside itself"; break
                reach = max(reach, target)
            if ins == 0x03E00008 or ins >> 26 == 2:
                if at + 8 > reach:
                    end = at + 8; break
            at += 4
        if end is None or (limit is not None and end > limit):
            notes.append({"entry": entry, "result": "not taken: " + (failure or "runs into the next function")}); continue
        rows.insert(index + 1, {"start": entry, "end": end})
        notes.append({"entry": entry, "result": "new function of %u bytes" % (end - entry)})
    return rows, notes


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", choices=sorted(overlay_programs.PROGRAMS), help="Prepare a wrapped overlay's separate map")
    args = parser.parse_args()
    if args.program:
        _, data = overlay_programs.load(args.program)
        files = overlay_programs.paths(args.program)
        identity, export, folder = hashlib.sha256(data).hexdigest(), files["exports"] / "analysis.json", files["generation"]
    else:
        data = (ROOT / "rom/SLUS_214.41").read_bytes()
        identity, export, folder = LOCAL_CANDIDATE_SHA256, ROOT / "work/ghidra/exports/analysis.json", ROOT / "work/bt2-generation"
        if hashlib.sha256(data).hexdigest() != LOCAL_CANDIDATE_SHA256:
            raise ValueError("Local executable hash differs from the inspected candidate.")
    report = json.loads(export.read_text(encoding="utf-8"))
    if report["language"] != "r5900:LE:32:default" or report["executable_sha256"].lower() != identity:
        raise ValueError("Export language/hash differs from the executable.")
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    metadata = inspector.inspect(data)
    external_text=()
    if args.program:
        # Overlay code may tail-jump into the main executable: its .text is an accepted J target.
        main_data=(ROOT / "rom/SLUS_214.41").read_bytes()
        external_text=tuple((x['address'],x['address']+x['size']) for x in inspector.inspect(main_data)['sections'] if x['name']=='.text')
    selected, excluded = select_candidates(report, metadata, data=data, external_text=external_text)
    folder.mkdir(parents=True, exist_ok=True)
    # Entry addresses to make functions of (map_gaps.py, discovery_loop.py): a list of numbers.
    entries_path = folder / "entries.json"
    entries = json.loads(entries_path.read_text(encoding="utf-8")) if entries_path.exists() else []
    # A candidate the rules above refused is tried the same way: its start is an entry address.
    selected, entry_notes = add_entries(selected, sorted(set(entries) | {row["start"] for row in excluded}), metadata, data)
    taken_starts = {row["start"] for row in selected}
    excluded = [row for row in excluded if row["start"] not in taken_starts]
    path = folder / "candidates.csv"
    text = "Name,Start,End,Size\n" + "".join(
        f"FUN_{row['start']:08X},0x{row['start']:08X},0x{row['end']:08X},{row['end']-row['start']}\n" for row in selected)
    path.write_text(text, encoding="utf-8")
    manifest = {"status": "automated provisional EE map", "executable_sha256": identity, "program": args.program or "executable",
                "map_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "export_sha256": hashlib.sha256(export.read_bytes()).hexdigest(), "language": report["language"],
                "selected_count": len(selected), "covered_bytes": sum(row["end"]-row["start"] for row in selected),
                "excluded_counts": dict(Counter(row["reason"] for row in excluded)), "excluded": excluded,
                "nop_padding_candidates": [row["start"] for row in report["functions"]
                    if has_tail_nop_gap(row, metadata, data)],
                "branch_nop_padding_candidates": [row["start"] for row in report["functions"] if has_branch_nop_gaps(row,metadata,data,external_text)],
                "entries": entry_notes, "max_function_bytes": 0x5000, "limitations": "Boundaries are provisional. Upstream may discover additional functions. No VU map or overlay assumed."}
    path.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    taken = sum(1 for note in entry_notes if note["result"].startswith(("new function", "split")))
    for note in entry_notes:
        if not note["result"].startswith(("new function", "split", "already")):
            print(f"Entry 0x{note['entry']:08X}: {note['result']}")
    print(f"Prepared {len(selected)} provisional EE functions ({taken} from entry addresses); quarantined {len(excluded)}. Map: {path}")


if __name__ == "__main__":
    main()
