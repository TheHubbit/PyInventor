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

    def test_compare(self):
        c1 = inventor.Cone(name="cone1")
        c2 = inventor.Cone(name="cone2")
        c3 = inventor.create_object(name="cone1")
        self.assertFalse(c1 == c2)
        self.assertTrue(c1 == c3)

if __name__ == '__main__':
    unittest.main()
