"""Prepare local runtime sources and a bounded guest image from validated generation.

Route emitted direct tail calls through the runtime so missing guest code is a
checked boundary, not an unresolved native symbol or an invented function stub.
"""

import hashlib
import importlib.util
import json
import re
from run_pipeline import ROOT, verify_generated


def detach_generated_headers(text,own):
    # The generator's declaration headers list every mapped function and are rewritten whenever the map
    # changes; including them would recompile every prepared source on each map change. A prepared source
    # reaches other functions only through the dispatcher, so it needs neither header.
    text=text.replace('#include <ps2_recompiled_functions.h>\n','').replace('#include <ps2_recompiled_stubs.h>\n','')
    foreign=set(re.findall(r'\bFUN_[0-9A-F]{8}_0x[0-9a-f]+\b',text))-{own}
    if foreign:raise ValueError(f'Prepared source {own} still names {sorted(foreign)[:3]}')
    return text


def write_prepared(path,text):
    text=detach_generated_headers(text,path.stem)
    # Unchanged prepared sources keep their timestamps, so incremental builds skip them.
    if not path.exists() or path.read_text(encoding='utf-8')!=text:
        path.write_text(text,encoding='utf-8')


def route_tail_calls(source):
    pattern = r"\bFUN_[0-9A-Fa-f]{8}_0x([0-9A-Fa-f]+)\(rdram, ctx, runtime\); return;"
    return re.subn(pattern, lambda match:
        f'(void)runtime->dispatchGuestBranch(rdram, ctx, 0x{match[1]}u, ctx->branch_pc, 0u, '
        'PS2Runtime::GuestBranchKind::TailJump, "generated tail call"); return;', source)


def route_direct_jumps(source):
    # A J to another function (emitted as an inline DirectJump dispatch when the target is outside
    # this generated program, e.g. an overlay jumping into the main executable) ends the path: the
    # target returns to this function's caller. Fallthrough after it would overwrite that return PC.
    pattern=(r'if \(!runtime->dispatchGuestBranch\(rdram, ctx, (0x[0-9A-Fa-f]+u), (0x[0-9A-Fa-f]+u), 0x0u, '
             r'PS2Runtime::GuestBranchKind::DirectJump, "J"\)\) \{\s*return;\s*\}')
    return re.subn(pattern,lambda m:f'(void)runtime->dispatchGuestBranch(rdram, ctx, {m[1]}, {m[2]}, 0u, '
                   'PS2Runtime::GuestBranchKind::TailJump, "generated tail jump"); return;',source)


def route_vu_status(source):
    pattern=r"SET_GPR_U32\(ctx, (\d+), ctx->vu0_vpu_stat\);"
    text,count=re.subn(pattern,lambda m:f'SET_GPR_U32(ctx, {m[1]}, runtime->readVuStatus(ctx));',source)
    if 'ctx->vu0_vpu_stat' in text:raise ValueError('Unrecognized generated VU status access')
    return text,count

# VU0 macro instructions by their effect on the status flags (mnemonic prefixes): those that leave them
# alone, and those whose effects are reported to the runtime between a flag clear and read.
VU_QUIET=('vnop','vwaitq','vftoi','vitof','vmove','vmr32','vabs','vclip','sqc2','lqc2','qmfc2','qmtc2','cfc2','ctc2')
VU_FLAG_SETTING=('vadd','vsub','vmul','vmadd','vmsub','vopm','vdiv','vsqrt','vrsqrt','vmax','vmini')

def route_cop2_condition(source):
    # BC2T/BC2F test the VU condition; the runtime owns which VU work exists. CFC2/CTC2 of the status
    # flag register go to the runtime as well (macro-mode arithmetic does not maintain the flags).
    text,count=re.subn(r'\(\(ctx->vu0_status & 0x1\)\)',r'(runtime->cop2Condition(ctx))',source)
    text,reads=re.subn(r'SET_GPR_U32\(ctx, (\d+), ctx->vu0_status\);',r'SET_GPR_U32(ctx, \1, runtime->readVuStatusFlag(ctx));',text)
    text,writes=re.subn(r'ctx->vu0_status = static_cast<uint16_t>\(GPR_U32\(ctx, (\d+)\) & (0x[0-9A-Fa-f]+u?)\);',r'runtime->writeVuStatusFlag(ctx, GPR_U32(ctx, \1) & \2);',text)
    count+=reads+writes
    # Between a flag clear and the flag read, every flag-setting VU instruction first reports its
    # effects to the runtime (from the instruction word in the generator's comment).
    lines=text.split('\n');out=[];armed=False
    for line in lines:
        out.append(line)
        if 'runtime->writeVuStatusFlag(' in line:armed=True
        elif 'runtime->readVuStatusFlag(' in line:armed=False
        elif armed:
            m=re.match(r'(\s*)// 0x[0-9a-f]+: 0x([0-9a-f]+)  (v[a-z0-9]+)',line)
            if m and not m[3].startswith(VU_QUIET):
                if not m[3].startswith(VU_FLAG_SETTING):
                    raise ValueError(f'Unrecognized VU instruction {m[3]} between a flag clear and read')
                out.append(f'{m[1]}runtime->vu0UpdateFlags(ctx, 0x{m[2]}u);');count+=1
    text='\n'.join(out)
    if 'ctx->vu0_status' in text:raise ValueError('Unrecognized generated COP2 condition')
    return text,count

def route_vu_control(source):
    pattern=r"ctx->vu0_fbrst = GPR_U32\(ctx, (\d+)\) & 0x00000C0Cu;"
    text,count=re.subn(pattern,lambda m:f'runtime->writeVuFbrst(ctx, GPR_U32(ctx, {m[1]}));',source)
    if 'ctx->vu0_fbrst =' in text:raise ValueError('Unrecognized generated VU control write')
    return text,count


def route_data_cache(source):
    pattern = (r'(// 0x[0-9a-f]+: 0x[0-9a-f]+  cache       0x18, 0x([0-9A-Fa-f]+)\(\$t0\)'
               r'\s+ctx->pc = 0x[0-9A-Fa-f]+u;\s+)// CACHE instruction \(ignored\)')
    return re.subn(pattern, lambda match: match[1] +
        f'runtime->dataCacheLine(rdram, ADD32(GPR_U32(ctx, 8), 0x{match[2]}u));', source)


def route_indexed_cache(source):
    pattern=(r'(// 0x[0-9a-f]+: 0x[0-9a-f]+  cache       0x(10|14|16), 0x([01])\(\$a2\)'
             r'\s+ctx->pc = 0x[0-9A-Fa-f]+u;\s+)// CACHE instruction \(ignored\)')
    text,count=re.subn(pattern,lambda m:m[1]+
        f'runtime->indexedDataCache(rdram, ctx, 0x{m[2]}u, ADD32(GPR_U32(ctx, 6), 0x{m[3]}u));',source)
    text,reads=re.subn(r'SET_GPR_S32\(ctx, (\d+), \(int32_t\)ctx->cop0_taglo\);',
        lambda m:f'SET_GPR_S32(ctx, {m[1]}, (int32_t)runtime->readCacheTagLo(ctx));',text)
    if 'ctx->cop0_taglo' in text:raise ValueError('Unrecognized generated TagLo access')
    if count or reads:
        if 'CACHE instruction (ignored)' in text:raise ValueError('Unsupported CACHE mixed with indexed TagLo')
    return text,count,reads


def route_fast_halfword_reads(source):
    # Generated constant-address loads retain the checked owned-RAM read path.
    text,count=re.subn(r'\bFAST_READ16\((0x[0-9A-Fa-f]+u)\)',r'READ16(\1)',source)
    if 'FAST_READ16(' in text:raise ValueError('Unrecognized generated fast halfword read')
    return text,count


def route_fast_byte_reads(source):
    # Keep the generated LB/LBU cast; only route its literal address through checked RAM.
    text,count=re.subn(r'\bFAST_READ8\((0x[0-9A-Fa-f]+u)\)',r'READ8(\1)',source)
    if 'FAST_READ8(' in text:raise ValueError('Unrecognized generated fast byte read')
    return text,count


def route_recursive_calls(source,entry):
    # A JAL to the function's own entry is emitted as an internal jump with RA set; its JR RA then leaves
    # the native function with a return address inside it. Make it a real guest call through the
    # dispatcher (same call accounting and depth limit as any other call).
    pattern=re.compile(r'(SET_GPR_U32\(ctx, 31, (0x[0-9A-Fa-f]+)u\);(?:(?!SET_GPR_U32\(ctx, 31,|goto ).)*?ctx->pc = (0x[0-9A-Fa-f]+)u;\s+)goto label_[0-9a-f]+;(\s+ctx->pc = \2u;)',re.S)
    def call(m):
        if int(m[3],16)!=entry:return m[0]
        return (m[1]+f'if (!runtime->dispatchGuestBranch(rdram, ctx, {m[3]}u, ctx->branch_pc, {m[2]}u, '
                'PS2Runtime::GuestBranchKind::DirectCall, "JAL")) {\n        return;\n    }'+m[4])
    text,count=pattern.subn(call,source)
    return text,sum(1 for m in pattern.finditer(source) if int(m[3],16)==entry)


def route_vu_memory(source):
    # VLQI/VSQI/VLQD/VSQD address VU0 data memory; the generator's output swaps their register fields
    # and reads main RAM. The runtime executes them from the instruction word instead.
    pattern=re.compile(r'(// 0x[0-9a-f]+: 0x([0-9a-f]+)  v(?:lqi|sqi|lqd|sqd)[^\n]*\n(?:\s*ctx->pc = 0x[0-9a-f]+u;\n)?)(\s*)\{[^\n]*\}\n')
    text,count=pattern.subn(lambda m:f'{m[1]}{m[3]}runtime->vu0MemoryInstruction(ctx, 0x{m[2]}u);\n',source)
    if re.search(r'ctx->vi\[(1[6-9]|[2-9][0-9])\]',text) or 'uint32_t addr = ((uint32_t)(ctx->vi[' in text:
        raise ValueError('Unrecognized generated VU0 memory access')
    return text,count


def route_vu_random(source):
    # VRNEXT/VRGET/VRINIT/VRXOR: the generator's output follows another sequence than the register's
    # and never writes VRNEXT's destination. The runtime executes them from the instruction word.
    pattern=re.compile(r'(// 0x[0-9a-f]+: 0x([0-9a-f]+)  v(?:rnext|rget|rinit|rxor)[^\n]*\n(?:\s*ctx->pc = 0x[0-9a-f]+u;\n)?)([ \t]*)\{[^{}]*\}\n')
    text,count=pattern.subn(lambda m:f'{m[1]}{m[3]}runtime->vu0RandomInstruction(ctx, 0x{m[2]}u);\n',source)
    if 'vu0_r' in text:raise ValueError('Unrecognized generated VU0 random number access')
    return text,count


def sign_extend_float_moves(source):
    # MFC1 (and the COP2 moves of Q's bits) sign-extend the 32-bit pattern into the 64-bit register on
    # the R5900. The generator zero-extends it, so every sign test of a float done on its bits with a
    # 64-bit branch or comparison (the C library's sinf, atanf, floorf ...) takes a negative value for
    # positive.
    pattern=re.compile(r'(\{ uint32_t bits; std::memcpy\(&bits, &ctx->(?:f\[\d+\]|vu0_q), sizeof\(bits\)\); )SET_GPR_U32\(ctx, (\d+), bits\); \}')
    text,count=pattern.subn(r'\1SET_GPR_S32(ctx, \2, (int32_t)bits); }',source)
    if re.search(r'SET_GPR_U32\(ctx, \d+, bits\)',text):raise ValueError('Unrecognized generated move of float bits')
    return text,count


def fix_sqrt_operand(source):
    # SQRT.S on the R5900 takes its operand from ft (the fs field is 0): fd = sqrt(ft). The generator
    # emits sqrt(fs), the root of $f0. Operand and destination are restored from the instruction word.
    pattern=re.compile(r'(// 0x[0-9a-f]+: 0x([0-9a-f]+)  [^\n]*\n(?:\s*ctx->pc = 0x[0-9a-f]+u;\n)?\s*)ctx->f\[\d+\] = FPU_SQRT_S\(ctx->f\[\d+\]\);')
    def fix(m):
        word=int(m[2],16)
        if word>>21!=0x230 or word&0x3F!=4:raise ValueError('FPU_SQRT_S emitted for another instruction')
        return f'{m[1]}ctx->f[{(word>>6)&31}] = FPU_SQRT_S(ctx->f[{(word>>16)&31}]);'
    text,count=pattern.subn(fix,source)
    if text.count('FPU_SQRT_S(')!=count:raise ValueError('Unrecognized generated SQRT.S')
    return text,count


def fix_clip(source):
    # VCLIP sets, per x, y and z of fs, the low bit when it is above +|w| of ft and the high bit when it
    # is below -|w|. The generator compares with w instead of |w| and swaps the two bits, so a loop that
    # moves a point back inside by the flags (the demo fight's object at 0x145DF0) moves it away forever.
    pattern=re.compile(r'\{ __m128 fs = ctx->vu0_vf\[(\d+)\]; __m128 ft = _mm_shuffle_ps\(ctx->vu0_vf\[(\d+)\], ctx->vu0_vf\[\2\], _MM_SHUFFLE\(3,3,3,3\)\); '
                       r'__m128 neg_ft = [^;]*; __m128 gt = [^;]*; __m128 lt = [^;]*; uint32_t gt_mask = [^;]*; uint32_t lt_mask = [^;]*; uint32_t flags = [^;]*; '
                       r'(ctx->vu0_clip_flags = \(\(ctx->vu0_clip_flags << 6\) \| \(flags & 0x3F\)\) & 0xFFFFFF; \})')
    def fix(m):
        return (f'{{ __m128 fs = ctx->vu0_vf[{m[1]}]; __m128 limit = _mm_and_ps(_mm_shuffle_ps(ctx->vu0_vf[{m[2]}], ctx->vu0_vf[{m[2]}], _MM_SHUFFLE(3,3,3,3)), _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF))); '
                f'uint32_t above = (uint32_t)_mm_movemask_ps(_mm_cmpgt_ps(fs, limit)); uint32_t below = (uint32_t)_mm_movemask_ps(_mm_cmplt_ps(fs, _mm_xor_ps(limit, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))))); '
                f'uint32_t flags = (above & 0x1) | ((below & 0x1) << 1) | ((above & 0x2) << 1) | ((below & 0x2) << 2) | ((above & 0x4) << 2) | ((below & 0x4) << 3); {m[3]}')
    text,count=pattern.subn(fix,source)
    if 'neg_ft' in text:raise ValueError('Unrecognized generated VCLIP')
    return text,count


def route_local_indirect_jumps(source):
    # A JR the generator could not resolve (a jump table it did not recognize) is emitted as a
    # dispatch to another function. When the target is one of this function's own entry labels (the
    # generator emits one per instruction for such functions), execution continues there through the
    # function's entry switch; any other target still goes to the dispatcher, which refuses it.
    pattern=re.compile(r'([ \t]*)(if \(!runtime->dispatchGuestBranch\(rdram, ctx, jumpTarget, 0x[0-9A-Fa-f]+u, 0x0u, PS2Runtime::GuestBranchKind::IndirectJump, "JR"\)\))')
    if not pattern.search(source):return source,0
    head=re.search(r'\n([ \t]*)switch \(ctx->pc\) \{\n(.*?)\n[ \t]*default: break;',source,re.S)
    if not head:return source,0 # No entry labels: every target is another function's.
    labels=re.findall(r'case (0x[0-9a-f]+u): goto label_[0-9a-f]+;',head[2])
    if not labels or len(labels)!=head[2].count('case '):raise ValueError('Unrecognized generated entry switch')
    cases=' '.join(f'case {label}:' for label in labels)
    text,count=pattern.subn(lambda m:f'{m[1]}switch (jumpTarget) {{ {cases} goto bt2_reenter; default: break; }}\n{m[1]}{m[2]}',source)
    return text.replace(head[0],f'\nbt2_reenter:{head[0]}',1),count


def keep_vf0_constant(source):
    # VF0 is hard-wired to (0, 0, 0, 1): an instruction naming it as destination only has its flag
    # effects (the game's clip tests subtract into VF0). The generator emits the register write, which
    # corrupts every later use of VF0; the value is computed and dropped instead.
    text,count=re.subn(r'ctx->vu0_vf\[0\] = ',r'(void)',source)
    if re.search(r'ctx->vu0_vf\[0\]\s*(=[^=]|[-+*/|&^]=)',text) or '&ctx->vu0_vf[0]' in text:raise ValueError('Unrecognized generated write to VF0')
    return text,count


def patchable_code():
    """The instruction words optional game patches change (config/game_patches.json): address ->
    (original, patched). Only an immediate of LUI may change, so the rewrite stays one instruction."""
    words={}
    for name,patch in json.loads((ROOT/'config/game_patches.json').read_text(encoding='utf-8'))['patches'].items():
        for address,original,patched in patch['code']:
            address,original,patched=int(address,16),int(original,16),int(patched,16)
            if original>>26!=0x0F or (original^patched)>>16:raise ValueError(f'Patch {name} at {address:#x} changes more than a LUI immediate')
            if words.get(address,(original,))[0]!=original:raise ValueError(f'Patches disagree on the original word at {address:#x}')
            words[address]=(original,patched)
    return words


def read_patchable_immediates(source,words=None):
    # A LUI an optional patch changes reads its immediate from the instruction word in guest memory,
    # so applying the patch (runtime/ee/game_patches.cpp) is a memory write, as on the PS2.
    words=patchable_code() if words is None else words
    pattern=re.compile(r'(// 0x([0-9a-f]+): 0x([0-9a-f]+)  lui[^\n]*\n(?:[ \t]*ctx->pc = 0x[0-9a-f]+u;\n)?)'
                       r'([ \t]*)SET_GPR_S32\(ctx, (\d+), \(int32_t\)\(\(uint32_t\)(\d+) << 16\)\);\n')
    count=0
    def replace(m):
        nonlocal count
        address=int(m[2],16)
        if address not in words:return m[0]
        original=words[address][0]
        if int(m[3],16)!=original or int(m[6])!=original&0xFFFF or int(m[5])!=(original>>16)&31:
            raise ValueError(f"The instruction at {address:#x} is not the patch's original word")
        count+=1
        return f'{m[1]}{m[4]}SET_GPR_S32(ctx, {m[5]}, (int32_t)(READ32(0x{address:x}u) << 16));\n'
    return pattern.sub(replace,source),count


def widen_sign_branches(source):
    # BLTZ/BGEZ/BLEZ/BGTZ (and their likely/link forms) compare the full 64-bit register on the R5900;
    # the generator emits a 32-bit test, which misreads e.g. a 64-bit BUSY flag in bit 63.
    text,count=re.subn(r'(const bool branch_taken_0x[0-9a-f]+ = \()GPR_S32(\(ctx, \d+\) (?:<|>=|<=|>) 0\);)',r'\1GPR_S64\2',source)
    if re.search(r'branch_taken_0x[0-9a-f]+ = \(GPR_S32',text):raise ValueError('Unrecognized generated sign branch')
    return text,count


def prepare_text(text,address):
    """Every rewrite a generated function's source gets, in order; returns the text and the count of
    each kind of rewrite. The interpreter's handlers (generate_interpreter.py) get the same."""
    text,count=route_tail_calls(text)
    text,jump_count=route_direct_jumps(text);count+=jump_count
    counts={'tail_dispatch_hooks':count}
    text,counts['vu_status_hooks']=route_vu_status(text)
    text,counts['vu_control_hooks']=route_vu_control(text)
    text,counts['cop2_condition_hooks']=route_cop2_condition(text)
    text,counts['indexed_cache_hooks'],counts['taglo_hooks']=route_indexed_cache(text)
    text,counts['checked_halfword_hooks']=route_fast_halfword_reads(text)
    text,counts['checked_byte_hooks']=route_fast_byte_reads(text)
    text,counts['sign_branch_hooks']=widen_sign_branches(text)
    text,counts['recursive_call_hooks']=route_recursive_calls(text,address)
    text,counts['vu_memory_hooks']=route_vu_memory(text)
    text,counts['vu_random_hooks']=route_vu_random(text)
    text,counts['float_moves_sign_extended']=sign_extend_float_moves(text)
    text,counts['sqrt_operand_fixes']=fix_sqrt_operand(text)
    text,counts['clip_fixes']=fix_clip(text)
    text,counts['vf0_writes_dropped']=keep_vf0_constant(text)
    text,counts['local_indirect_jumps']=route_local_indirect_jumps(text)
    text,counts['patchable_immediates']=read_patchable_immediates(text)
    return text,counts


def build_memory(data, metadata, size=32 * 1024 * 1024):
    memory = bytearray(size)
    occupied = []
    for load in metadata["load_segments"]:
        address, length = load["virtual_address"], load["memory_size"]
        if not length:
            continue
        if address < 0 or address + length > size or load["file_size"] > length:
            raise ValueError("ELF load exceeds bounded guest RAM.")
        if any(address < end and start < address + length for start, end in occupied):
            raise ValueError("Overlapping ELF loads need explicit policy.")
        occupied.append((address, address + length))
        offset, file_size = load["file_offset"], load["file_size"]
        if offset < 0 or offset + file_size > len(data):
            raise ValueError("ELF load payload is out of file bounds.")
        memory[address:address + file_size] = data[offset:offset + file_size]
    return memory


def main():
    verify_generated()
    folder = ROOT / "work/bt2-generation"
    patched = ROOT / "game/generated/bt2-runtime"
    patched.mkdir(parents=True, exist_ok=True)
    prepared_sources = []
    tail_sources = {
        "FUN_00100008_0x100008.cpp": 1,
        "FUN_00338520_0x338520.cpp": 1,
        "FUN_0033A238_0x33a238.cpp": 1,
        "FUN_00321E50_0x321e50.cpp": 1,
        "FUN_00321D90_0x321d90.cpp": 1,
        "FUN_00331FE8_0x331fe8.cpp": 2,
        "FUN_00331888_0x331888.cpp": 3,
    }
    for filename, expected_count in tail_sources.items():
        source = ROOT / "game/generated/bt2" / filename
        text, count = route_tail_calls(source.read_text(encoding="utf-8"))
        if count != expected_count:
            raise ValueError(f"Unexpected emitted tail call count in {filename}; review changed output.")
        text, _ = widen_sign_branches(text)
        destination = patched / source.name
        write_prepared(destination,text)
        prepared_sources.append({"source": filename,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "prepared_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
            "tail_dispatch_hooks": count})
    # Every actually selected extra entry gets the same deterministic tail routing.
    # Preserve raw generation and record hashes; no handwritten per-function stub list.
    from expand_startup import verify_extra,verify_overlays
    import overlay_programs
    selected=[(address,ROOT/'game/generated/bt2'/f'FUN_{address:08X}_0x{address:x}.cpp',patched) for address in verify_extra()]
    # Selected overlay entries: same deterministic routing into each overlay's own prepared folder.
    for program,functions in verify_overlays().items():
        files=overlay_programs.paths(program);files['runtime'].mkdir(parents=True,exist_ok=True)
        selected+=[(row['start'],files['output']/f"FUN_{row['start']:08X}_0x{row['start']:x}.cpp",files['runtime']) for row in functions]
    for address,source,prepared in selected:
        text,counts=prepare_text(source.read_text(encoding='utf-8'),address)
        if address in (0x32F4B0,0x32F5F0) and (counts['indexed_cache_hooks'],counts['taglo_hooks'])!=(4,2):
            raise ValueError('Reached indexed cache helper shape/count differs')
        destination=prepared/source.name
        write_prepared(destination,text)
        prepared_sources.append({'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
                                 'prepared_sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),**counts})
    # Every instruction an optional patch changes is in a prepared function, and only once.
    patchable=sum(row.get('patchable_immediates',0) for row in prepared_sources)
    if patchable!=len(patchable_code()):raise ValueError(f'{patchable} of {len(patchable_code())} patchable instructions found in the prepared functions')
    source = ROOT / 'game/generated/bt2/FUN_00331F38_0x331f38.cpp'
    text, count = route_data_cache(source.read_text(encoding='utf-8'))
    if count != 9 or 'CACHE instruction (ignored)' in text:
        raise ValueError('Unexpected generated data cache instruction shape/count.')
    text, _ = widen_sign_branches(text)
    destination = patched / source.name
    write_prepared(destination, text)
    prepared_sources.append({'source': source.name,
        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'prepared_sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
        'data_cache_hooks': count})
    spec = importlib.util.spec_from_file_location("bt2_elf", ROOT / "tools/inspect_elf/inspect_elf.py")
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    data = (ROOT / "rom/SLUS_214.41").read_bytes()
    memory = build_memory(data, inspector.inspect(data))
    image = folder / "guest-memory.bin"
    if not image.exists() or image.read_bytes() != memory:
        image.write_bytes(memory)
    (folder / "runtime-preparation.json").write_text(json.dumps({
        "prepared_sources": prepared_sources, "memory_size": len(memory),
        "memory_sha256": hashlib.sha256(memory).hexdigest(), "scope": "ELF PT_LOAD image, main RAM only; no BIOS/MMIO/TLB/IOP"}, indent=2) + "\n", encoding="utf-8")
    print("Prepared entry dispatch hook and bounded ELF memory image; originals unchanged.")


if __name__ == "__main__":
    main()
