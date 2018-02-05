from mitsuba.render import BSDF, BSDFSample3f, BSDFContext
from mitsuba.render import EImportance, ERadiance
import numpy as np
import os
import pytest


def test01_ctx_construct():
    ctx = BSDFContext()
    assert ctx.type_mask == BSDF.EAll
    assert ctx.component == np.uint32(-1)
    assert ctx.mode == ERadiance
    ctx.reverse()
    assert ctx.mode == EImportance

    # By default, all components and types are queried
    assert ctx.is_enabled(BSDF.EDeltaTransmission)
    assert ctx.is_enabled(BSDF.EDelta, 1)
    assert ctx.is_enabled(BSDF.EDiffuseTransmission, 6)
    assert ctx.is_enabled(BSDF.EGlossy, 10)

def test02_bs_construct():
    wo = [1, 0, 0]
    wi = [0, 1, 0]
    bs = BSDFSample3f(wi, wo)
    assert np.allclose(bs.wi, wi)
    assert np.allclose(bs.wo, wo)
    assert np.allclose(bs.pdf, 0.0)
    assert np.allclose(bs.eta, 1.0)
    assert bs.sampled_type == 0
