from mitsuba.core import Transform
import numpy as np

def test01_identity():
    assert(Transform() == Transform(np.eye(4, 4)))
