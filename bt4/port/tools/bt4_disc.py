"""Read and identify a BT4 B14 Rev2 ISO without external archive tools.

Only ISO directory metadata and the three base programs are read here. Native
setup verifies every extracted asset against the selected profile's CRC table.
"""
from pathlib import Path
import hashlib,struct

SECTOR=2048
PROGRAMS=('SLUS_219.78','BIN/DBZP.BIN','BIN/DBZ4.BIN')
PROFILES={
 'es':('2ae9c636de479c01d2b4e9906c25b7ca807e80d9','de91913f40bd675d70ee748aa16a42a96dff5919','989ee93cbd31508717bc245793c1e89c50109e0f'),
 'en':('a4776fbadd2d266f42cf559a3530c2e6454d01a3','1d9f8da80c200bb50e7df3210fff30827e5dae9c','145837274421c39ae7eedc3fa1b697d4212541c2')}

class DiscError(ValueError):pass

def flat_image(elf):
 if len(elf)<52 or elf[:4]!=b'\x7fELF' or elf[4:6]!=b'\x01\x01':
  raise DiscError('SLUS_219.78 is not a little-endian PS2 ELF executable.')
 shoff=struct.unpack_from('<I',elf,32)[0];size,count=struct.unpack_from('<HH',elf,46)
 if size<24 or shoff+size*count>len(elf):raise DiscError('Invalid BT4 executable section table.')
 out=bytearray(0x2ff180-0x100000)
 for i in range(count):
  _,kind,flags,address,offset,length=struct.unpack_from('<6I',elf,shoff+size*i)
  if flags&2 and kind!=8 and length and 0x100000<=address and address+length<=0x2ff180:
   if offset+length>len(elf):raise DiscError('Truncated BT4 executable section.')
   out[address-0x100000:address-0x100000+length]=elf[offset:offset+length]
 return bytes(out)

def read_programs(path):
 path=Path(path);total=path.stat().st_size
 with path.open('rb') as f:
  def read(offset,length):
   if offset<0 or length<0 or offset+length>total:raise DiscError('ISO extent is outside the image.')
   f.seek(offset);data=f.read(length)
   if len(data)!=length:raise DiscError('Truncated ISO image.')
   return data
  volume=read(16*SECTOR,SECTOR)
  if volume[:7]!=b'\x01CD001\x01' or volume[156]<34:raise DiscError('Expected an ISO 9660 PS2 disc image.')
  root=volume[156:156+volume[156]];files={};seen=set()
  def directory(record,prefix='',depth=0):
   extent,length=struct.unpack_from('<I',record,2)[0],struct.unpack_from('<I',record,10)[0]
   if depth>16 or length>16*1024*1024 or extent in seen:raise DiscError('Invalid ISO directory structure.')
   seen.add(extent);data=read(extent*SECTOR,length);at=0
   while at<len(data):
    n=data[at]
    if not n:at=(at//SECTOR+1)*SECTOR;continue
    row=data[at:at+n]
    if n<34 or len(row)!=n or 33+row[32]>n:raise DiscError('Invalid ISO directory record.')
    name=row[33:33+row[32]];at+=n
    if name in (b'\0',b'\1'):continue
    try:name=name.decode('ascii').split(';',1)[0].upper()
    except UnicodeDecodeError:raise DiscError('Unsupported ISO directory encoding.')
    if not name or '/' in name or name in ('.','..'):raise DiscError('Invalid ISO filename.')
    relative=prefix+name
    if row[25]&2:
     if relative=='BIN':directory(row,relative+'/',depth+1)
    elif relative in PROGRAMS:
     offset,length=struct.unpack_from('<I',row,2)[0]*SECTOR,struct.unpack_from('<I',row,10)[0]
     if length>16*1024*1024:raise DiscError('Unsupported BT4 program size.')
     files[relative]=read(offset,length)
  directory(root)
 missing=[name for name in PROGRAMS if name not in files]
 if missing:raise DiscError('Not a BT4 B14 Rev2 ISO. Missing: '+', '.join(missing))
 return files

def identify(path):
 programs=read_programs(path)
 hashes=(hashlib.sha1(flat_image(programs[PROGRAMS[0]])).hexdigest(),
         hashlib.sha1(programs[PROGRAMS[1]]).hexdigest(),hashlib.sha1(programs[PROGRAMS[2]]).hexdigest())
 for language,want in PROFILES.items():
  if hashes==want:return language
 raise DiscError('Unsupported BT4 disc profile. Use the LATINO or English B14 Rev2 ISO with unmodified base programs.')
