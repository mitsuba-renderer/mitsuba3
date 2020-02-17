import mitsuba
import pytest
import enoki as ek
import numpy as np


def test01_ctx_construct(variant_scalar_rgb):
    from mitsuba.render import BSDFContext, BSDFFlags, TransportMode
    ctx = BSDFContext()
    assert ctx.type_mask == +BSDFFlags.All
    assert ctx.component == np.uint32(-1)
    assert ctx.mode == TransportMode.Radiance
    ctx.reverse()
    assert ctx.mode == TransportMode.Importance

    # By default, all components and types are queried
    assert ctx.is_enabled(BSDFFlags.DeltaTransmission)
    assert ctx.is_enabled(BSDFFlags.Delta, 1)
    assert ctx.is_enabled(BSDFFlags.DiffuseTransmission, 6)
    assert ctx.is_enabled(BSDFFlags.Glossy, 10)


def test02_bs_construct(variant_scalar_rgb):
    from mitsuba.render import BSDFSample3f
    wo = [1, 0, 0]
    bs = BSDFSample3f(wo)
    assert ek.allclose(bs.wo, wo)
    assert ek.allclose(bs.pdf, 0.0)
    assert ek.allclose(bs.eta, 1.0)
    assert bs.sampled_type == 0
