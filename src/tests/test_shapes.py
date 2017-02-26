from mitsuba.core import Thread, FileResolver
from mitsuba.core.xml import load_string
import numpy as np
import os


def test01_ply_triangle():
    thread = Thread.thread()
    fres_old = thread.fileResolver()
    fres = FileResolver(fres_old)
    fres.append(os.path.dirname(os.path.realpath(__file__)))
    thread.set_file_resolver(fres)

    shape = load_string("""
        <scene version="0.5.0">
            <shape type="ply">
                <string name="filename" value="data/triangle.ply"/>
            </shape>
        </scene>
    """).kdtree()[0]

    vertices, faces = shape.vertices(), shape.faces()
    assert vertices.ndim == 1
    assert vertices.shape == (3, )
    assert np.all(vertices['x'] == np.float32([0, 0, 0]))
    assert np.all(vertices['y'] == np.float32([0, 0, 1]))
    assert np.all(vertices['z'] == np.float32([0, 1, 0]))
    assert faces.ndim == 1
    assert faces.shape == (1, )
    assert faces[0]['vertex_index.i0'] == np.uint32(0)
    assert faces[0]['vertex_index.i1'] == np.uint32(1)
    assert faces[0]['vertex_index.i2'] == np.uint32(2)
    thread.set_file_resolver(fres_old)
