import pytest
import drjit as dr
import mitsuba as mi

def test01_ss_repr(variants_vec_backends_once_rgb):
    # Test that the class is bound and prints itself
    ss = dr.zeros(mi.SilhouetteSample3f, 3)
    expected = """SilhouetteSample[
  p = [[0, 0, 0],
       [0, 0, 0],
       [0, 0, 0]],
  d = [[0, 0, 0],
       [0, 0, 0],
       [0, 0, 0]],
  sil_dir = [[0, 0, 0],
             [0, 0, 0],
             [0, 0, 0]],
  n = [[0, 0, 0],
       [0, 0, 0],
       [0, 0, 0]],
  prim_index = [0, 0, 0],
  project_index = [0, 0, 0],
  uv = [[0, 0],
        [0, 0],
        [0, 0]],
  pdf = [0, 0, 0],
  delta = [0, 0, 0],
  shape = [0x0, 0x0, 0x0],
  foreshortening = [0, 0, 0],
  offset = [0, 0, 0],
]
            """
    assert expected.strip() == str(ss)

