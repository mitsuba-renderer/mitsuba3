from mitsuba.scalar_rgb.render import BSDF, BSDFSample3f, BSDFContext, BSDFFlags, TransportMode
import numpy as np


def test01_ctx_construct():
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


def test02_bs_construct():
    wo = [1, 0, 0]
    bs = BSDFSample3f(wo)
    assert np.allclose(bs.wo, wo)
    assert np.allclose(bs.pdf, 0.0)
    assert np.allclose(bs.eta, 1.0)
    assert bs.sampled_type == 0
