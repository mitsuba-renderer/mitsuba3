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
            self.tint = props.get_texture('tint')

        def traverse(self, cb):
            cb.put('tint', self.tint, mi.ParamFlags.Differentiable)

    mi.register_bsdf('dummy', DummyBSDF)

    bsdf = mi.load_dict({
        'type': 'dummy',
        'tint': { 'type': 'rgb', 'value': color }
    })
    assert dr.all(bsdf.has_attribute('tint'))
    assert not dr.all(bsdf.has_attribute('foo'))
    assert dr.allclose(color, bsdf.eval_attribute('tint', si))
    assert dr.allclose(0.0, bsdf.eval_attribute('foo', si))


def test04_bsdf_attribute_direct_fields_skip_texture_metadata(variants_vec_backends_once_rgb):
    class DirectAttributeField(mi.Field):
        def __init__(self, supports_surface=True):
            mi.Field.__init__(self, mi.Properties("direct_attribute_field"))
            self._supports_surface = supports_surface

        def out_type(self):
            raise RuntimeError("out_type should not be queried")

        def out_dim(self):
            raise RuntimeError("out_dim should not be queried")

        def domain(self):
            return mi.FieldDomain.Surface

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return self._supports_surface

        def supports_interaction_queries(self):
            return False

        def eval_3(self, si, active=True):
            return mi.Color3f(si.uv.x, si.uv.y, 0.25)

    class FieldAttributeBSDF(mi.BSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.attr = props["attr"]

        def traverse(self, cb):
            cb.put("attr", self.attr, mi.ParamFlags.Differentiable)

    plugin_name = "field_attribute_bsdf_" + mi.variant().replace("_", "")
    try:
        mi.register_bsdf(plugin_name, FieldAttributeBSDF)
    except RuntimeError as exc:
        if "already" not in str(exc).lower():
            raise

    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [0.2, 0.7]

    bsdf = mi.load_dict({
        "type": plugin_name,
        "attr": DirectAttributeField(),
    })
    assert dr.all(bsdf.has_attribute("attr"))
    assert dr.allclose(bsdf.eval_attribute_3("attr", si), [0.2, 0.7, 0.25])

    bad_bsdf = mi.load_dict({
        "type": plugin_name,
        "attr": DirectAttributeField(supports_surface=False),
    })
    with pytest.raises(RuntimeError, match="supports surface queries"):
        bad_bsdf.eval_attribute_3("attr", si)
