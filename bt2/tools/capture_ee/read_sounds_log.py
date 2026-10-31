"""Print the SOUNDS call log recorded by capture_ee.py --sounds-log (function, command words, reply)."""
import argparse,struct
from pathlib import Path

STUB_DATA=0x01000300;ENTRIES=STUB_DATA+0x400;ENTRY_WORDS=24


def entries(ram):
    for index in range(struct.unpack_from('<I',ram,STUB_DATA+0x10)[0]):
        words=struct.unpack_from('<21I',ram,ENTRIES+index*ENTRY_WORDS*4)
        yield words[0],words[1:5],words[5:21]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True,help='Capture folder with ee-ram.bin.')
    ram=(parser.parse_args().capture/'ee-ram.bin').read_bytes()
    for index,(function,command,reply) in enumerate(entries(ram),1):
        print(f"{index:3} function {function:#x} command {' '.join(f'{w:X}' for w in command)} -> {' '.join(f'{w:X}' for w in reply)}")


if __name__=='__main__':
    main()
