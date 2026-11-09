"""Bounded IRX metadata and provisional neutral map; no loading or execution."""
import argparse,csv,hashlib,json,struct
from pathlib import Path
from inspect_image import disc_image

def inspect(data):
    def read(fmt,offset):
        size=struct.calcsize(fmt)
        if offset<0 or offset+size>len(data):raise ValueError('IRX metadata outside file')
        return struct.unpack_from(fmt,data,offset)
    if len(data)<52 or data[:7]!=b'\x7fELF\x01\x01\x01':raise ValueError('Expected ELF32 little-endian IRX')
    kind,machine,version,entry,phoff,shoff,flags,ehsize,phsize,phcount,shsize,shcount,names=read('<HHIIIIIHHHHHH',16)
    if kind!=0xFF80 or machine!=8 or version!=1 or ehsize!=52 or phsize!=32 or shsize!=40 or not 0<phcount<=16 or not 0<shcount<=128:raise ValueError('Unsupported IRX header')
    programs=[read('<8I',phoff+j*phsize) for j in range(phcount)]
    sections=[read('<10I',shoff+j*shsize) for j in range(shcount)]
    loads=[p for p in programs if p[0]==1];module=[p for p in programs if p[0]==0x70000080]
    if len(loads)!=1 or len(module)!=1:raise ValueError('Expected one IRX load and module header')
    load=loads[0];_,offset,base,physical,filesize,memsize,permissions,alignment=load
    if base!=0 or filesize>memsize or memsize>2*1024*1024 or offset+filesize>len(data) or permissions!=7 or alignment!=16:raise ValueError('Unsupported IRX load extent')
    text=[s for s in sections if s[1]==1 and s[2]&4]
    if len(text)!=1:raise ValueError('Expected one executable section')
    text=text[0];start,end=text[3],text[3]+text[5]
    if start!=0 or text[4]!=offset or end>filesize or end%4 or entry%4 or not start<=entry<end:raise ValueError('IRX text/entry differs')
    for s in sections:
        if s[1]!=8 and s[4]+s[5]>len(data):raise ValueError('IRX section outside file')
        if s[2]&2 and (s[3]+s[5]>memsize):raise ValueError('IRX allocated section outside memory')
    metadata=read('<6I',module[0][1]);name_offset=module[0][1]+26
    terminator=data.find(b'\0',name_offset,module[0][1]+module[0][4])
    if metadata[1]!=entry or sum(metadata[3:6])!=memsize or metadata[3]!=end or terminator<0:raise ValueError('IRX module sizes/name differ')
    name=data[name_offset:terminator].decode('ascii')
    tables=[]
    for address in range(start,end,4):
        magic=read('<I',offset+address)[0]
        if magic in (0x41E00000,0x41C00000):tables.append((address,magic))
    code_end=tables[0][0] if tables else end
    imports=[];exports=[];excluded=[]
    for index,(address,magic) in enumerate(tables):
        limit=tables[index+1][0] if index+1<len(tables) else end
        if address+20>limit:raise ValueError('Truncated IRX library header')
        _,nextptr,ver,mode,raw=read('<IIHH8s',offset+address)
        if nextptr or mode:raise ValueError('Unsupported linked IRX table')
        library=raw.split(b'\0')[0].decode('ascii')
        if not library or any(not(c.isalnum() or c=='_') for c in library):raise ValueError('Invalid IRX library name')
        pos=address+20
        if magic==0x41E00000:
            terminated=False
            while pos+8<=limit:
                jump,ordinal=read('<2I',offset+pos)
                if jump==ordinal==0:
                    terminated=True
                    if any(data[offset+pos:offset+limit]):raise ValueError('Unexpected import table tail')
                    break
                if jump!=0x03E00008 or ordinal>>16!=0x2400:raise ValueError('Unsupported IRX import stub')
                imports.append({'library':library,'version':ver,'ordinal':ordinal&65535,'address':pos});pos+=8
            if not terminated:raise ValueError('Unterminated IRX imports')
            excluded.append((address,limit))
        else:
            slots=[]
            while pos+4<=limit:
                value=read('<I',offset+pos)[0]
                if value%4 or value>=end:break
                slots.append(value);pos+=4
            excluded.append((address,pos))
            while slots and slots[-1]==0:slots.pop()
            if any(v%4 or v>=end for v in slots):raise ValueError('Invalid IRX export pointer')
            exports.append({'address':address,'library':library,'version':ver,'slots':slots,'trailing_zero_ordinals_unknown':True})
    relocations=[];counts={};seen=set()
    for s in sections:
        if s[1]!=9:continue
        if s[9]!=8 or s[5]%8 or s[7]>=len(sections):raise ValueError('Invalid IRX relocation section')
        for at in range(s[4],s[4]+s[5],8):
            address,info=read('<2I',at);relkind=info&255
            if address%4 or address+4>filesize or relkind not in (2,4,5,6) or info>>8:raise ValueError('Unsupported IRX relocation')
            if address in seen:raise ValueError('Duplicate IRX relocation')
            seen.add(address)
            relocations.append({'address':address,'type':relkind});counts[str(relkind)]=counts.get(str(relkind),0)+1
    ranges=[];cursor=0
    for a,b in excluded:
        if cursor<a:ranges.append((cursor,a))
        cursor=b
    if cursor<end:ranges.append((cursor,end))
    def in_code(a):return any(lo<=a<hi for lo,hi in ranges) and a%4==0
    seeds={entry}
    for table in exports:seeds.update(table['slots'])
    for pc in range(0,end,4):
        if not in_code(pc):continue
        word=read('<I',offset+pc)[0]
        if word>>26==3:
            target=((pc+4)&0xF0000000)|((word&0x3FFFFFF)<<2)
            if in_code(target):seeds.add(target)
    if any(not in_code(a) for a in seeds):raise ValueError('IRX entry/export targets inline tables')
    starts=sorted(seeds);rows=[]
    for n,a in enumerate(starts):
        limit=next(hi for lo,hi in ranges if lo<=a<hi)
        rows.append({'name':f'FUN_{a:08X}','start':a,'end':min(starts[n+1],limit) if n+1<len(starts) else limit})
    return {'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),'name':name,'entry':entry,'gp':metadata[2],'memory_size':memsize,'file_size':filesize,'code_end':code_end,'code_ranges':ranges,'imports':imports,'exports':exports,'relocations':relocations,'relocation_counts':counts,'provisional_functions':rows,'limitations':'Structural metadata and entry/export/JAL seeds only. No relocation applied, imports resolved, module loaded or entry executed. Pointer-only callbacks may be undiscovered.'}

def relocate(data,report,base):
    """Prepare a private image at an explicit base; never mutate captured IOP RAM."""
    if base%16 or base<0x1000 or base+report['memory_size']>2*1024*1024:raise ValueError('Invalid scoped IOP placement')
    phoff=struct.unpack_from('<I',data,28)[0];count=struct.unpack_from('<H',data,44)[0]
    loads=[struct.unpack_from('<8I',data,phoff+j*32) for j in range(count) if struct.unpack_from('<I',data,phoff+j*32)[0]==1]
    if len(loads)!=1:raise ValueError('IRX load differs')
    image=bytearray(report['memory_size']);image[:report['file_size']]=data[loads[0][1]:loads[0][1]+report['file_size']]
    pending=[]
    for relocation in report['relocations']:
        address,kind=relocation['address'],relocation['type'];word=struct.unpack_from('<I',image,address)[0]
        if kind==2:word=(word+base)&0xFFFFFFFF
        elif kind==4:
            target=((word&0x3FFFFFF)<<2)+base
            if target>=0x10000000:raise ValueError('IOP jump crosses region')
            word=(word&0xFC000000)|(target>>2)
        elif kind==5:
            pending.append((address,word));continue
        elif kind==6:
            if not pending:raise ValueError('Unpaired IRX LO16')
            low=word&65535;low=low-65536 if low&32768 else low
            for high_address,high in pending:
                value=((high&65535)<<16)+low+base
                struct.pack_into('<I',image,high_address,(high&0xFFFF0000)|(((value+32768)>>16)&65535))
            pending.clear();word=(word&0xFFFF0000)|((low+base)&65535)
        else:raise ValueError('Unsupported IRX relocation type')
        struct.pack_into('<I',image,address,word)
    if pending:raise ValueError('Unpaired IRX HI16')
    return bytes(image)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('module',type=Path);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--disc',type=Path,required=True);parser.add_argument('--base',type=lambda value:int(value,0));parser.add_argument('--iop-capture',type=Path)
    args=parser.parse_args();root=Path(__file__).resolve().parents[2];target=args.output.resolve()
    if not any(target.is_relative_to(root/p) for p in ('work','game_data')):raise ValueError('Derived output must remain under ignored work/ or game_data/')
    data=args.module.read_bytes()
    if disc_image(args.disc,(args.module.name.upper()+';1').encode('ascii'))!=data:raise ValueError('Local IRX differs from the provided disc')
    report=inspect(data)
    if args.base is not None:
        image=relocate(data,report,args.base)
        if args.iop_capture:
            m=json.loads(args.iop_capture.read_text(encoding='utf-8'));iop=args.iop_capture.with_name('iop-ram.bin').read_bytes();ee=args.iop_capture.with_name('ee-ram.bin').read_bytes()
            import sys
            sys.path.insert(0,str(root/'tools/capture_ee'))
            from capture_ee import EXPECTED_SHA,MARKER,capture_trap
            address,trap,marker=capture_trap('loadfile-module-after')
            if len(iop)!=2*1024*1024 or len(ee)!=32*1024*1024 or m['source_elf_sha256']!=EXPECTED_SHA or not m['capture_point'].startswith('loadfile-module-after:') or hashlib.sha256(iop).hexdigest()!=m['iop_capture_sha256'] or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or ee[address:address+len(trap)]!=trap or struct.unpack_from('<I',ee,MARKER)[0]!=marker:raise ValueError('IOP module capture identity differs')
            with args.disc.open('rb') as stream:disc_sha=hashlib.file_digest(stream,'sha256').hexdigest()
            if disc_sha!=m['disc_sha256']:raise ValueError('Module capture used another disc')
            info=struct.unpack_from('<12I',iop,args.base-48);module_id,modres=struct.unpack_from('<2i',ee,0x3ACA00)
            nameptr=info[1]
            if info[3]&65535!=module_id or module_id<=0 or info[4]!=args.base+report['entry'] or info[5]!=args.base+report['gp'] or info[6]!=args.base or nameptr>len(iop)-len(report['name'])-1 or iop[nameptr:nameptr+len(report['name'])+1]!=report['name'].encode()+b'\0':raise ValueError('IOP loaded module ownership differs')
            addresses=[r['address'] for r in report['relocations']]
            if any(image[a:a+4]!=iop[args.base+a:args.base+a+4] for a in addresses):raise ValueError('IOP relocation comparison differs')
            if any(image[a:b]!=iop[args.base+a:args.base+b] for a,b in report['code_ranges']):raise ValueError('IOP code comparison differs')
            report['placement_comparison']={'base':args.base,'module_id':module_id,'reference_module_result':modres,'relocations_verified':len(addresses),'capture':m,'limitations':'Reference loaded-module ownership and relocated code only; no native IOP module executed by this project.'}
        target.parent.mkdir(parents=True,exist_ok=True);target.with_suffix('.placed.bin').write_bytes(image)
    elif args.iop_capture:raise ValueError('IOP comparison needs an explicit placement base')
    target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    with target.with_suffix('.csv').open('w',encoding='utf-8',newline='') as stream:
        writer=csv.writer(stream);writer.writerow(('Name','Start','End','Size'))
        for r in report['provisional_functions']:writer.writerow((r['name'],hex(r['start']),hex(r['end']),r['end']-r['start']))
    print(f"IRX validated: {len(report['provisional_functions'])} provisional functions, {len(report['imports'])} imports; no module executed.")
if __name__=='__main__':main()
