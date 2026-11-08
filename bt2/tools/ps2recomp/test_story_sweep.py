import unittest
import story_sweep


class StorySweepTests(unittest.TestCase):
    def test_cases_take_the_story_marker_and_list_parts_without_one(self):
        parts=[{'episode':0,'part':0,'story_marker':14},{'episode':0,'part':1,'story_marker':None},{'episode':2,'part':0,'story_marker':3},
               {'episode':2,'part':1,'story_marker':None,'direct_story':True}]
        self.assertEqual(story_sweep.cases(parts),(['0,0,marker:14','2,0,marker:3','2,1'],['0,1']))
        self.assertEqual(story_sweep.cases(parts,{2}),(['2,0,marker:3','2,1'],[]))

    def test_optional_cases_take_each_optional_scene_once(self):
        parts=[{'episode':0,'part':0,'first_scene':33,'markers':{'14':34,'11':664,'5':727}},
               {'episode':0,'part':1,'first_scene':36,'markers':{'9':37,'11':664,'6':728}}]
        self.assertEqual(story_sweep.optional_cases(parts),['0,0,marker:5','0,0,marker:11','0,1,marker:6'])

    def test_name_and_completion(self):
        self.assertEqual(story_sweep.name('3,12,marker:7'),'e03-p12-m07')
        self.assertEqual(story_sweep.name('3,2'),'e03-p02')
        self.assertEqual(story_sweep.outcome({'starter_parts_completed':1,'starter_battles_cleared':1}),'cleared')
        self.assertEqual(story_sweep.outcome({'starter_parts_completed':1,'starter_battles_lost':1}),'lost')
        self.assertEqual(story_sweep.outcome({'starter_stuck':0x255580}),'stuck: 00255580')
        self.assertEqual(story_sweep.outcome({'frame_budget_reached':1}),'')
        self.assertEqual(story_sweep.outcome({'starter_town_done':True}),'town')

    def test_battle_text(self):
        won={'rule':3,'ball':1,'ball_picked':0,'rewards':[411,-1],'sagas':[411]}
        lost={'rule':0,'ball':2,'ball_picked':2,'rewards':[-1,-1],'sagas':[]}
        self.assertEqual(story_sweep.battle_text([won,lost]),'rule 3 ball 1/0 rewards 411 sagas 411; rule 0 ball 2/2 rewards none')


if __name__=='__main__':unittest.main()
