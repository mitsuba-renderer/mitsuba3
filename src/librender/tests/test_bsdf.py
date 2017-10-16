from mitsuba.render import BSDFSample3f
from mitsuba.render import SurfaceInteraction3f
import numpy as np
import os
import pytest


def test01_bs_reverse():
    its = SurfaceInteraction3f()
    wo  = [1, 0, 0]
    wi  = [0, 1, 0]
    bs  = BSDFSample3f(its, wi, wo)
    bs.reverse()
    assert((bs.wi == wo).all())
    assert((bs.wo == wi).all())

@pytest.mark.skip("BSDF interface will evolve, components are not implemented yet.")
def test02_component():
    # TODO implement this test
    assert(false)


