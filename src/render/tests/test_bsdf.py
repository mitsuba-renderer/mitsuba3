import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def test01_ctx_construct(variant_scalar_rgb):
    ctx = mi.BSDFContext()
    assert ctx.type_mask == +mi.BSDFFlags.All
    assert ctx.component == np.uint32(-1)
    assert ctx.mode == mi.TransportMode.Radiance
    ctx.reverse()
    assert ctx.mode == mi.TransportMode.Importance

    # By default, all components and types are queried
    assert ctx.is_enabled(mi.BSDFFlags.DeltaTransmission)
    assert ctx.is_enabled(mi.BSDFFlags.Delta, 1)
    assert ctx.is_enabled(mi.BSDFFlags.DiffuseTransmission, 6)
    assert ctx.is_enabled(mi.BSDFFlags.Glossy, 10)


def test02_bs_construct(variant_scalar_rgb):
    wo = [1, 0, 0]
    bs = mi.BSDFSample3f(wo)
    assert dr.allclose(bs.wo, wo)
    assert dr.allclose(bs.pdf, 0.0)
    assert dr.allclose(bs.eta, 1.0)
    assert bs.sampled_type == 0
