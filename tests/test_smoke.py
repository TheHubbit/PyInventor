import unittest
import inventor

class SmokeTest(unittest.TestCase):
    
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

if __name__ == '__main__':
    unittest.main()
