import struct,unittest
import story_scenes


def section(tag,body,extra=0):
    return tag+struct.pack('<3I',16,len(body),extra)+body


def script(blocks,values=()):
    """A synthetic GSCF file: an empty header section, the code blocks and the values section."""
    code=b''.join(section(b'GSAC',struct.pack(f'<{len(words)}I',*words),number)+b'EOFC'+struct.pack('<3I',16,0,0)
                  for number,words in blocks)
    body=section(b'GSHD',b'')+section(b'GSCD',code,1)+section(b'GSDT',struct.pack(f'<{len(values)}i',*values))
    return b'GSCF'+struct.pack('<3I',16,len(body),1)+body


class StorySceneTests(unittest.TestCase):
    def test_instructions_take_their_argument_words(self):
        words=struct.pack('<6I',0x0FA10301,0x0A,0x20A,0x30A,0x2AF90001,0)
        self.assertEqual(story_scenes.instructions(words),[(4001,[(0,10),(2,10),(3,10)]),(11001,[])])

    def test_scene_blocks_and_values(self):
        data=script([(0xFFFFFFFF,[0x3A990101,0x0A,0,0]),(1000,[0x03F40101,0x10A])],[0,40])
        blocks=story_scenes.scene(data)
        self.assertEqual(blocks[0xFFFFFFFF],[(15001,[(0,10)])])
        self.assertEqual(blocks[1000],[(1012,[(1,10)])])
        self.assertEqual(story_scenes.values(data),[0,40])
        with self.assertRaises(ValueError):story_scenes.scene(b'XXXX'+data[4:])

    def test_survey_resolves_markers_and_scenes_through_the_values(self):
        class Files:
            def __init__(self,files):self.files=files
            def entry(self,n):
                if n not in self.files:raise ValueError('missing')
                return self.files[n]
        # Scene 33: marker 14 leads to scene 34 (value 1), marker 5 to 700 (value 2); block 1000 goes to 40 (value 3).
        first=script([(2014,[0x03EA0101,0x10A]),(2005,[0x03EA0101,0x20A]),(1000,[0x03F40101,0x30A])],[0,34,700,40])
        files={33:first,40:script([(1000,[0x2AF90001])]),36:script([])}
        parts=story_scenes.survey(Files(files),[2])
        self.assertEqual(parts[0]['markers'],{14:34,5:700})
        self.assertEqual(parts[0]['story_marker'],14)
        self.assertEqual(parts[0]['scenes'],[33,40])
        self.assertEqual(parts[0]['commands'],{1002:2,1012:1,11001:1})
        self.assertFalse(parts[0]['direct_story'])
        self.assertIsNone(parts[1]['story_marker'])
        self.assertEqual(parts[1]['first_scene'],36)

    def test_survey_reads_3000_marker_blocks_and_direct_story_scenes(self):
        class Files:
            def __init__(self,files):self.files=files
            def entry(self,n):return self.files[n]
        # Scene 33: block 3014 leads to scene 34; scene 36 goes to 37 itself from block 1001 (command 1012);
        # scene 39 goes to 40 after a chase (command 1013).
        files={33:script([(3014,[0x03EA0101,0x10A])],[0,34]),36:script([(1001,[0x03F40101,0x10A])],[0,37]),
               39:script([(1001,[0x03F50101,0x10A])],[0,40]),34:script([]),37:script([]),40:script([])}
        parts=story_scenes.survey(Files(files),[3])
        self.assertEqual(parts[0]['markers'],{14:34})
        self.assertEqual(parts[0]['story_marker'],14)
        self.assertIsNone(parts[1]['story_marker'])
        self.assertTrue(parts[1]['direct_story'])
        self.assertFalse(parts[1]['chase'])
        self.assertTrue(parts[2]['direct_story'] and parts[2]['chase'])


if __name__=='__main__':unittest.main()
