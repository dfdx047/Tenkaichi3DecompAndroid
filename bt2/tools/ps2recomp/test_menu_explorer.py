import unittest
import menu_explorer


class MenuExplorerTests(unittest.TestCase):
    def test_pad_script_presses_one_button_per_step(self):
        self.assertEqual(menu_explorer.pad_script(['down','cross'],800,60),'800:down,808:-,860:cross,868:-')
        self.assertEqual(menu_explorer.pad_script([],800,60),'')

    def test_paths_grow_when_new_code_ran_or_after_a_few_moves(self):
        b=['cross','down']
        self.assertEqual(menu_explorer.extensions([],0,b,3),[['cross'],['down']])
        self.assertEqual(menu_explorer.extensions(['cross'],5,b,3),[['cross','cross'],['cross','down']])
        self.assertEqual(menu_explorer.extensions(['cross'],0,b,3),[['cross','cross'],['cross','down']])
        self.assertEqual(menu_explorer.extensions(['triangle'],0,b,3),[])
        self.assertEqual(menu_explorer.extensions(['cross','down'],0,b,3),[['cross','down','cross'],['cross','down','down']])
        self.assertEqual(menu_explorer.extensions(['down','down','down'],0,b,3),[])
        self.assertEqual(menu_explorer.extensions(['down','cross','cross','cross'],0,b,3),[['down','cross','cross','cross','cross']])
        self.assertEqual(menu_explorer.extensions(['down']+['cross']*12,0,b,3),[])
        self.assertEqual(menu_explorer.extensions(['down']+['cross']*12,0,b,3,20),[['down']+['cross']*13])

    def test_explore_is_breadth_first_and_stops_at_the_run_budget(self):
        # A menu where cross enters new code once, and a stopped run is not extended.
        def run(path):
            if path==['down']:return {'result':'stop'},{1}
            return {'result':'ok'},{1,2} if path[:1]==['cross'] else {1}
        order=[]
        seen,done=menu_explorer.explore(10,run,['cross','down'],5,1,2,lambda path,row,new:order.append((path,new)))
        self.assertEqual(seen,{1,2})
        self.assertEqual(order[:3],[([],1),(['cross'],1),(['down'],0)])
        self.assertEqual(done,5)


if __name__=='__main__':unittest.main()
