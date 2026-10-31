"""Bounded local ROMDIR inventory; no extraction, loading or execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[2]


def inspect(data):
    if len(data)<48 or data[:10]!=b'RESET'+bytes(5) or data[16:26]!=b'ROMDIR'+bytes(4):
        raise ValueError('Expected RESET/ROMDIR header.')
    directory_size=struct.unpack_from('<I',data,28)[0]
    if directory_size<64 or directory_size%16 or directory_size>len(data) or directory_size>65536:
        raise ValueError('Invalid ROMDIR extent.')
    rows=[];offset=0;extension=0;names=set();terminated=False
    for index in range(0,directory_size,16):
        raw,extra,size=struct.unpack_from('<10sHI',data,index)
        if raw==bytes(10):
            if extra or size or any(data[index:directory_size]):raise ValueError('Malformed terminator.')
            terminated=True;break
        name=raw.split(b'\0')[0].decode('ascii')
        if not name or any(not(c.isupper() or c.isdigit() or c=='_') for c in name) or name in names:
            raise ValueError('Invalid/duplicate ROMDIR name.')
        names.add(name)
        if offset+size>len(data):raise ValueError('ROMDIR payload exceeds image.')
        payload=data[offset:offset+size]
        row={'name':name,'offset':offset,'size':size,'extinfo_size':extra,'sha256':hashlib.sha256(payload).hexdigest()}
        if payload.startswith(b'\x7fELF'):
            if len(payload)<52 or payload[4:7]!=bytes((1,1,1)) or struct.unpack_from('<H',payload,18)[0]!=8:
                raise ValueError('Invalid ELF32 little-endian MIPS module.')
            phoff=struct.unpack_from('<I',payload,28)[0];phsize,phcount=struct.unpack_from('<HH',payload,42)
            if phcount and (phsize!=32 or phoff+phsize*phcount>size):raise ValueError('Invalid module program table.')
            row['elf_entry']=struct.unpack_from('<I',payload,24)[0]
            row['elf_type']=struct.unpack_from('<H',payload,16)[0]
        rows.append(row);offset=(offset+size+15)&~15;extension+=extra
    if not terminated or [r['name'] for r in rows[:3]]!=['RESET','ROMDIR','EXTINFO']:
        raise ValueError('Incomplete ROMDIR metadata.')
    if rows[0]['size'] or rows[1]['offset']!=0 or rows[1]['size']!=directory_size or extension!=rows[2]['size']:
        raise ValueError('ROMDIR metadata/extinfo mismatch.')
    return {'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'entries':rows,
            'limitations':'ROMDIR inventory and bounded ELF header metadata only; no import resolution or execution.'}


def disc_image(iso,filename=b'IOPRP300.IMG;1',directory=b'IRX',limit=16*1024*1024):
    if not filename or any(c not in b'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.;' for c in filename+directory):raise ValueError('Unsupported local disc filename')
    with iso.open('rb') as stream:
        length=iso.stat().st_size
        def read(offset,size):
            if offset<0 or size<0 or offset+size>length:raise ValueError('ISO extent outside disc.')
            stream.seek(offset);data=stream.read(size)
            if len(data)!=size:raise ValueError('Truncated ISO extent.')
            return data
        def extent(record):
            if len(record)<34 or record[1] or record[26] or record[27] or record[25]&128:
                raise ValueError('Unsupported ISO record.')
            lba,size=struct.unpack_from('<I',record,2)[0],struct.unpack_from('<I',record,10)[0]
            if (lba,size)!=(struct.unpack_from('>I',record,6)[0],struct.unpack_from('>I',record,14)[0]):
                raise ValueError('ISO paired fields differ.')
            return lba*2048,size
        pvd=read(16*2048,2048)
        if pvd[:7]!=b'\x01CD001\x01' or pvd[128:132]!=b'\x00\x08\x08\x00':raise ValueError('Expected ISO9660.')
        record=pvd[156:156+pvd[156]]
        for name,is_directory in ((directory,True),(filename,False)):
            start,size=extent(record)
            if size>16*1024*1024:raise ValueError('Unreasonable directory.')
            directory=read(start,size);pos=0;matches=[]
            while pos<len(directory):
                n=directory[pos]
                if not n:pos=(pos//2048+1)*2048;continue
                if n<34 or pos+n>len(directory) or pos%2048+n>2048:raise ValueError('Invalid directory record.')
                candidate=directory[pos:pos+n];count=candidate[32]
                if 33+count>n:raise ValueError('Invalid ISO filename.')
                if candidate[33:33+count]==name:matches.append(candidate)
                pos+=n
            if len(matches)!=1 or bool(matches[0][25]&2)!=is_directory:raise ValueError('Missing/ambiguous IOP image path.')
            record=matches[0]
        start,size=extent(record)
        if size>limit:raise ValueError('Unreasonable disc file size.')
        return read(start,size)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,default=ROOT/'rom/IRX/IOPRP300.IMG')
    parser.add_argument('--disc',type=Path,required=True)
    args=parser.parse_args();data=args.image.read_bytes()
    if disc_image(args.disc)!=data:raise ValueError('Local image differs from embedded disc image.')
    report=inspect(data)
    with args.disc.open('rb') as stream:report['disc_sha256']=hashlib.file_digest(stream,'sha256').hexdigest()
    folder=ROOT/'work/bt2-generation';folder.mkdir(parents=True,exist_ok=True)
    (folder/'iop-image.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f"Validated local/embedded image: {len(report['entries'])-3} payloads; metadata remains ignored.")

if __name__=='__main__':main()
