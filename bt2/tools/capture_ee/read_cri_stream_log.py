"""Print the CRI stream log recorded by capture_ee.py --cri-stream-log (non-empty uploads and replies of every channel)."""
import argparse,struct
from pathlib import Path

STUB_DATA=0x01000300;ENTRY_WORDS=12


def entries(ram):
    count=struct.unpack_from('<I',ram,STUB_DATA+0xC)[0]
    for index in range(count):
        yield struct.unpack_from('<12I',ram,STUB_DATA+0x100+index*ENTRY_WORDS*4)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True,help='Capture folder with ee-ram.bin.')
    ram=(parser.parse_args().capture/'ee-ram.bin').read_bytes()
    for tag,count,sequence,*words in entries(ram):
        records=' | '.join(' '.join(f'{word:X}' for word in words[start:start+4]) for start in range(0,min(count,2)*4,4))
        print(f"{'reply ' if tag&3==1 else 'upload'} {tag&~3:06X} seq {sequence:5} count {count} {records}")


if __name__=='__main__':
    main()
