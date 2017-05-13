import unittest
import inventor


class NodeTest(unittest.TestCase):
    
    def test_node(self):
        sphere = inventor.Sphere("radius 4")
        self.assertEqual(sphere.radius, 4)
        sphere.radius = 2
        self.assertEqual(sphere.radius, 2)

    def test_group(self):
        group = inventor.Group()
        self.assertIsNotNone(group)
        group[0] = inventor.Cone()
        self.assertEqual(len(group), 1)
        group += inventor.Sphere()
        self.assertEqual(len(group), 2)
        group.append(inventor.Cube())
        self.assertEqual(len(group), 3)
        self.assertEqual([c.get_type() for c in group], [ 'Cone', 'Sphere', 'Cube'])
        del (group[1])
        self.assertEqual([c.get_type() for c in group], [ 'Cone', 'Cube'])

    def test_compare(self):
        c1 = inventor.Cone(name="cone1")
        c2 = inventor.Cone(name="cone2")
        c3 = inventor.create_object(name="cone1")
        self.assertFalse(c1 == c2)
        self.assertTrue(c1 == c3)


class SensorTest(unittest.TestCase):

    def setUp(self):
        self.callback_info = ["", "", "", ""]
        self.view = inventor.SceneManager()
        self.view.scene = self.create_scene()
        self.view.resize(256, 256)

    def create_scene(self):
        '''Returns a simple scene (light, camera, manip, material, cone)'''
        root = inventor.Separator()
        root += inventor.DirectionalLight()
        root += inventor.OrthographicCamera()
        root += inventor.Selection("policy SINGLE")
        root[-1] += inventor.Material("diffuseColor 1 0 0")
        root[-1] += inventor.Cone()
        return root

    def selection_callback(self, path):
        self.callback_info[0] = path[-1].get_type()

    def deselection_callback(self, path):
        self.callback_info[1] = path[-1].get_type()

    def start_callback(self, node):
        self.callback_info[2] = node.get_type()

    def finish_callback(self, node):
        self.callback_info[3] = node.get_type()

    def test_selection(self):
        selectionNode = inventor.search(self.view.scene, type="Selection", first = True)[-1]
        selectionSensor = inventor.Sensor(selectionNode, "selection", self.selection_callback)
        deselectionSensor = inventor.Sensor(selectionNode, "deselection", self.deselection_callback)
        startSensor = inventor.Sensor(selectionNode, "start", self.start_callback)
        finishSensor = inventor.Sensor(selectionNode, "finish", self.finish_callback)

        # click on cone to select
        self.callback_info = ["", "", "", ""]
        self.view.mouse_button(0, 0, 128, 128)
        self.view.mouse_button(0, 1, 128, 128)
        self.assertEqual(self.callback_info, ['Cone', '', 'Selection', 'Selection'])

        # click outside to deselect
        self.callback_info = ["", "", "", ""]
        self.view.mouse_button(0, 0, 1, 1)
        self.view.mouse_button(0, 1, 1, 1)
        self.assertEqual(self.callback_info, ['', 'Cone', 'Selection', 'Selection'])


if __name__ == '__main__':
    unittest.main()
