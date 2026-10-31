r"""Compare main's first file load (cdrom0:\BIN\DBZP.BIN;1) with the original.

Original observer point: `rpc-dbzp-loaded` (main's first frame step after closing the file,
0x10036C). The C library's open, two seeks, read and close run through boot FILEIO's server and
worker threads natively; the loaded image is compared byte for byte at its link address. FILEIO's
IOP read buffer is a model-local placement and is not compared.
"""
import argparse,json
from pathlib import Path
from verify_vblank_wake import original
from verify_sound_rpc import require

MARKER={16:0x1FFED20,48:0x38BDF0}
IMAGE=(0x3B3100,0x7D48F)

def native_scope(r):
    # Bind-time initialization request, then open, lseek (end), lseek (start), read, close; every
    # file operation completes through one IOP->EE SIF command.
    require(r['fileio_binds']==1 and r['fileio_calls']>=6 and r['fileio_completions']>=5
        and r['fileio_completions']==min(r['fileio_calls'],6)-1,'Native FILEIO lifecycle differs')

def verify(root,native):
    ee=original(root,'rpc-dbzp-loaded',MARKER)
    r=json.loads(native.read_text(encoding='utf-8'));native_scope(r)
    ne=Path(str(native)+'.ee.bin').read_bytes()
    a,n=IMAGE;require(ne[a:a+n]==ee[a:a+n],'Loaded DBZP.BIN image differs')
    print('Verified the loaded DBZP.BIN image (0x7D48F bytes at 0x3B3100) after the native FILEIO open/seek/read/close.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args()
    verify(Path(__file__).resolve().parents[2],a.native)
