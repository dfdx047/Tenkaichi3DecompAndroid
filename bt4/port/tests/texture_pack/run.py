from pathlib import Path
import os,subprocess,tempfile,zipfile,struct
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='bt4-dds-') as folder:
 kw={'creationflags':0x08000000} if os.name=='nt' else {}
 # A valid ZIP64 local-offset extra, with ordinary 32-bit entry sizes. This
 # exercises the large-pack representation without writing a multi-GB fixture.
 archive=Path(folder)/'offset64.zip'
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  z.writestr('texture.bin',b'BT4 ZIP64 OK')
  z.writestr('SLUS-21978/','')
  z.writestr('SLUS-21978/replacements/','')
 data=archive.read_bytes();end=data.rfind(b'PK\x05\x06');start=struct.unpack_from('<I',data,end+16)[0]
 central=bytearray(data[start:end]);name,extra=struct.unpack_from('<HH',central,28)
 struct.pack_into('<H',central,6,45);struct.pack_into('<H',central,30,extra+12)
 struct.pack_into('<I',central,42,0xffffffff)
 at=46+name+extra;central[at:at]=struct.pack('<HHQ',1,8,0)
 tail=bytearray(data[end:]);struct.pack_into('<I',tail,12,len(central))
 archive.write_bytes(data[:start]+central+tail)
 exe=Path(folder)/('zip.exe' if os.name=='nt' else 'zip')
 subprocess.run([os.environ.get('CC','gcc' if os.name=='nt' else 'cc'),'-O1','-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES','-DMINIZ_NO_ARCHIVE_WRITING_APIS','-DMINIZ_NO_TIME',str(root/'port/tests/texture_pack/test_zip.c'),str(root/'port/third_party/miniz/miniz.c'),'-o',str(exe)],check=True,**kw)
 subprocess.run([str(exe),str(archive)],check=True,**kw)
 print('ZIP64 large-archive offset regression passed')
