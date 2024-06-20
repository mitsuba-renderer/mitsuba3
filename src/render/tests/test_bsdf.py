import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def test01_ctx_construct(variant_scalar_rgb):
    ctx = mi.BSDFContext()
    assert ctx.type_mask == +mi.BSDFFlags.All
    assert ctx.component == np.iinfo(np.uint32).max
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

def test03_bsdf_attributes(variants_vec_backends_once_rgb):

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [0.5, 0.5]

    color = [0.5, 0.6, 0.7]

    bsdf = mi.load_dict({
        'type': 'diffuse',
        'reflectance': { 'type': 'rgb', 'value': color }
    })

    assert dr.all(bsdf.has_attribute('reflectance'))
    assert not dr.all(bsdf.has_attribute('foo'))
    assert dr.allclose(color, bsdf.eval_attribute('reflectance', si))
    assert dr.allclose(0.0, bsdf.eval_attribute('foo', si))

    # Now with a custom BSDF

    class DummyBSDF(mi.BSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tint = props['tint']

        def traverse(self, callback):
            callback.put_object('tint', self.tint, mi.ParamFlags.Differentiable)

    mi.register_bsdf('dummy', DummyBSDF)

    bsdf = mi.load_dict({
        'type': 'dummy',
        'tint': { 'type': 'rgb', 'value': color }
    })
    assert dr.all(bsdf.has_attribute('tint'))
    assert not dr.all(bsdf.has_attribute('foo'))
    assert dr.allclose(color, bsdf.eval_attribute('tint', si))
    assert dr.allclose(0.0, bsdf.eval_attribute('foo', si))