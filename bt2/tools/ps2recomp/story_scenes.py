"""Lists the story mode's scene scripts per episode part (docs/plans/ee/STORY_MODE_PLAN.md).

Reads the scene files from the disc's first archive (file numbers below 13,234) and decodes their script
(GSCF: sections GSHD, GSCD with GSAC blocks ending in EOFC, GSDT with the script's values; a command's
arguments index the values). For each episode part (first scene 33 + 3 x the parts before it, the part
counts read from the game's executable) it lists the map markers of the first scene with the scene each
leads to (block 2000 + marker, or 3000 + marker on some maps; command 1002), the marker that leads to the
part's own story scene (first + 1, for BT2_START_EPISODE=<episode>,<part>,marker:<m>), whether the first
scene goes there itself without a marker (direct_story; chase when it does so once the enemy chased on the map
is caught, command 1013), the scenes reached and the commands used. Output: a JSON file in ignored work/ (the game's data stays local).
"""
import argparse,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

FIRST_ARCHIVE=(b'DATA',b'ZS2US_1.AFS;1')
SECOND_ARCHIVE_START=13234
GO_TO_SCENE={1004,1007,1012,1013}  # Event kinds 2 to 4: the next scene is the argument.
MARKER_SCENE=1002                  # In a marker's block: the scene the marker leads to.
CHASE_SCENE=1013                   # Event kind 3: the scene after the chase on the map (the enemy caught).
MARKER_BLOCKS=(*range(2001,2026),*range(3001,3026)) # Block 2000 + marker, or 3000 + marker on maps with the flag 0x2AD1F0 tests.
EPISODE_TABLE=0x344F30             # In the executable: 16 bytes per episode, the part count at +4.
EPISODES=25


class Disc:
    def __init__(self,path):
        self.stream=open(path,'rb')

    def read(self,offset,size):
        self.stream.seek(offset);data=self.stream.read(size)
        if len(data)!=size:raise ValueError('Truncated read from the disc image')
        return data

    def file(self,path):
        """(offset, size) of a file in the ISO 9660 image, by its path of directory and file names."""
        pvd=self.read(16*2048,2048)
        if pvd[1:6]!=b'CD001':raise ValueError('Not an ISO 9660 image')
        record=pvd[156:156+pvd[156]]
        for name in path:
            start,size=struct.unpack_from('<I',record,2)[0]*2048,struct.unpack_from('<I',record,10)[0]
            directory=self.read(start,size);pos=0;found=None
            while pos<len(directory):
                n=directory[pos]
                if not n:pos=(pos//2048+1)*2048;continue
                if directory[pos+33:pos+33+directory[pos+32]]==name:found=directory[pos:pos+n];break
                pos+=n
            if found is None:raise ValueError(f'{name!r} not on the disc')
            record=found
        return struct.unpack_from('<I',record,2)[0]*2048,struct.unpack_from('<I',record,10)[0]


class Archive:
    def __init__(self,disc,name):
        self.disc=disc;self.offset,self.size=disc.file(name)
        head=disc.read(self.offset,8)
        if head[:3]!=b'AFS':raise ValueError('Not an AFS archive')
        self.count=struct.unpack_from('<I',head,4)[0]
        self.table=disc.read(self.offset+8,self.count*8)

    def entry(self,index):
        if not 0<=index<self.count:raise ValueError(f'Archive entry {index} outside 0 to {self.count-1}')
        start,size=struct.unpack_from('<2I',self.table,index*8)
        return self.disc.read(self.offset+start,size)


def instructions(code):
    """Decodes a GSAC block's words: (opcode, arguments); an argument word holds (value << 8) | type."""
    out=[];words=list(struct.unpack_from(f'<{len(code)//4}I',code));k=0
    while k<len(words):
        word=words[k];k+=1
        if not word:continue
        opcode,count=word>>16,(word>>8)&0xFF
        arguments=[]
        for _ in range(count):
            if k>=len(words):break
            arguments.append((words[k]>>8,words[k]&0xFF));k+=1
        out.append((opcode,arguments))
    return out


def sections(data):
    if data[:4]!=b'GSCF':raise ValueError('Not a scene script')
    a=struct.unpack_from('<I',data,4)[0];found={}
    while a+16<=len(data):
        tag,head,size=data[a:a+4],*struct.unpack_from('<2I',data,a+4)
        if tag!=b'EOFC':found[tag]=(a+head,size)
        a+=head+size
    return found


def values(data):
    """The script's values (section GSDT), which command arguments index."""
    start,size=sections(data).get(b'GSDT',(0,0))
    return list(struct.unpack_from(f'<{size//4}i',data,start))


def scene(data):
    """The blocks of a scene script: {block number: [(opcode, arguments)]}."""
    start,size=sections(data)[b'GSCD'];blocks={};a=start
    while a+16<=start+size:
        tag,head,length,number=data[a:a+4],*struct.unpack_from('<3I',data,a+4)
        if tag==b'GSAC':blocks[number]=instructions(data[a+head:a+head+length]);a+=head+length
        elif tag==b'EOFC':a+=head
        else:break
    return blocks


def survey(archive,part_counts):
    """Per episode and part: the scenes reached from its first scene and the commands used in them."""
    result=[];before=0
    for episode,parts in enumerate(part_counts):
        for part in range(parts):
            first=33+3*(before+part);seen=set();todo=[first];commands={};markers={};direct=chase=False
            while todo:
                number=todo.pop()
                if number in seen or number>=SECOND_ARCHIVE_START:continue
                seen.add(number)
                try:
                    data=archive.entry(number);blocks=scene(data);table=values(data)
                except ValueError:continue
                value=lambda arguments:table[arguments[0][0]] if arguments and 0<=arguments[0][0]<len(table) else None
                for block,code in blocks.items():
                    for opcode,arguments in code:
                        if opcode in GO_TO_SCENE and value(arguments) is not None:todo.append(value(arguments))
                        if number==first and block in MARKER_BLOCKS and opcode==MARKER_SCENE and value(arguments) is not None:
                            markers[block%1000]=value(arguments)
                        if number==first and block<2000 and opcode in GO_TO_SCENE and value(arguments)==first+1:
                            direct=True;chase=chase or opcode==CHASE_SCENE
                        commands[opcode]=commands.get(opcode,0)+1
            story=[m for m,s in markers.items() if s==first+1]
            result.append({'episode':episode,'part':part,'first_scene':first,'markers':markers,'story_marker':story[0] if story else None,'direct_story':direct,'chase':chase,
                           'scenes':sorted(seen),'commands':commands})
        before+=parts
    return result


def part_counts(executable):
    """Part counts from the executable's episode table."""
    elf=executable.read_bytes()
    off,count=struct.unpack_from('<I',elf,0x1C)[0],struct.unpack_from('<H',elf,0x2C)[0]
    for n in range(count):
        kind,offset,vaddr,_,size,_=struct.unpack_from('<6I',elf,off+n*32)
        if kind==1 and vaddr<=EPISODE_TABLE<vaddr+size:
            base=offset+EPISODE_TABLE-vaddr
            return [struct.unpack_from('<I',elf,base+16*k+4)[0] for k in range(EPISODES)]
    raise ValueError('Episode table not in a loaded segment')


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--disc',type=Path,required=True)
    p.add_argument('--executable',type=Path,default=ROOT/'rom/SLUS_214.41')
    p.add_argument('--output',type=Path,default=ROOT/'work/coverage/story/scenes.json')
    a=p.parse_args()
    archive=Archive(Disc(a.disc),FIRST_ARCHIVE)
    parts=survey(archive,part_counts(a.executable))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(parts,indent=1),encoding='utf-8')
    marked=sum(p['story_marker'] is not None for p in parts);direct=sum(p['direct_story'] for p in parts);chases=sum(p['chase'] for p in parts)
    print(f'{len(parts)} parts, {marked} with a marker to their story scene, {direct} going there without one ({chases} after a chase), {sum(len(p["scenes"]) for p in parts)} scenes visited; {a.output}')


if __name__=='__main__':main()
