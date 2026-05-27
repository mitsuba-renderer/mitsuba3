import pytest
import drjit as dr
import mitsuba as mi


def test01_constant_construct(variant_scalar_rgb):
    vol = mi.load_dict({
        "type": "constvolume",
        "to_world": mi.ScalarTransform4f().scale([2, 0.2, 1]),
        "value": {
            "type": "regular",
            "wavelength_min": 500,
            "wavelength_max": 600,
            "values": "3.0, 3.0"
        }
    })

    assert vol is not None
    assert vol.bbox() == mi.BoundingBox3f([0, 0, 0], [2, 0.2, 1])


def test02_constant_eval(variant_scalar_rgb):
    vol = mi.load_dict({
        "type": "constvolume",
        "value": {
            "type" : "srgb",
            "color" : mi.Color3f([0.5, 1.0, 0.3])
        }
    })

    it = dr.zeros(mi.Interaction3f, 1)
    assert dr.allclose(vol.eval(it), mi.Color3f(0.5, 1.0, 0.3))
    assert vol.bbox() == mi.BoundingBox3f([0, 0, 0], [1, 1, 1])


def test03_constant_defers_generic_field_summary_failures(variant_scalar_rgb):
    class ScalarFieldNoSummary(mi.Field):
        def __init__(self, props):
            mi.Field.__init__(self, props)

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval(self, si, args=None, active=True):
            return mi.ArrayXf([self.eval_1(si, args=args, active=active)])

        def eval_1(self, si, args=None, active=True):
            return dr.select(active, 0.5, 0.0)

    class ScalarFieldMaxOnly(ScalarFieldNoSummary):
        def max(self):
            return 0.5

    class ScalarFieldWithSummary(ScalarFieldMaxOnly):
        def mean(self):
            return 0.5

    for name, cls in [
        ("scalar_field_no_summary", ScalarFieldNoSummary),
        ("scalar_field_max_only", ScalarFieldMaxOnly),
        ("scalar_field_with_summary", ScalarFieldWithSummary),
    ]:
        try:
            mi.register_field(name, cls)
        except RuntimeError as exc:
            if "already" not in str(exc).lower():
                raise

    vol_no_summary = mi.load_dict({
        "type": "constvolume",
        "value": {
            "type": "scalar_field_no_summary",
        },
    })
    with pytest.raises(RuntimeError, match="max"):
        vol_no_summary.max()

    vol_max_only = mi.load_dict({
        "type": "constvolume",
        "value": {
            "type": "scalar_field_max_only",
        },
    })
    assert dr.allclose(vol_max_only.max(), 0.5)
    with pytest.raises(RuntimeError, match="mean"):
        vol_max_only.mean()

    vol = mi.load_dict({
        "type": "constvolume",
        "value": {
            "type": "scalar_field_with_summary",
        },
    })
    assert dr.allclose(vol.eval_1(dr.zeros(mi.Interaction3f)), 0.5)
