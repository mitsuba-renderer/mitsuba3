try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core import Thread
from mitsuba.core.xml import loadString
import numpy as np
import os


class ShapeTest(unittest.TestCase):
    def setUp(self):
        fres = Thread.thread().fileResolver()
        fres.append(os.path.dirname(os.path.realpath(__file__)))

    def test01_ply_triangle(self):
        shape = loadString("""
            <scene version="0.4.0">
                <shape type="ply">
                    <string name="filename" value="data/triangle.ply"/>
                </shape>
            </scene>
        """).kdtree()[0]

        vertices, faces = shape.vertices(), shape.faces()
        self.assertTrue(vertices.ndim == 1)
        self.assertTrue(vertices.shape == (3, ))
        self.assertTrue(np.all(vertices['x'] == np.float32([0, 0, 0])))
        self.assertTrue(np.all(vertices['y'] == np.float32([0, 0, 1])))
        self.assertTrue(np.all(vertices['z'] == np.float32([0, 1, 0])))
        self.assertTrue(faces.ndim == 1)
        self.assertTrue(faces.shape == (1, ))
        self.assertEqual(faces[0]['vertex_index.i0'], np.uint32(0))
        self.assertEqual(faces[0]['vertex_index.i1'], np.uint32(1))
        self.assertEqual(faces[0]['vertex_index.i2'], np.uint32(2))

if __name__ == '__main__':
    unittest.main()
