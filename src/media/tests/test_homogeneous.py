import pytest
import drjit as dr
import mitsuba as mi


def test01_phase_function_accessors(variants_vec_rgb):
    medium = mi.load_dict ({
        'type': 'homogeneous',
        'albedo': {
            'type': 'rgb',
            'value': [0.99, 0.9, 0.96]
        }
    })
    medium_ptr = mi.MediumPtr(medium)

    assert type(medium.phase_function()) == mi.PhaseFunction
    assert type(medium_ptr.phase_function()) == mi.PhaseFunctionPtr


def test02_surface_field_objects_can_initialize_constant_volume_slots(variant_scalar_rgb):
    medium = mi.load_dict({
        "type": "homogeneous",
        "albedo": {
            "type": "srgb",
            "color": mi.Color3f(0.2, 0.4, 0.6),
        },
        "sigma_t": 2.0,
    })

    mei = mi.MediumInteraction3f()
    sigma_s, sigma_n, sigma_t = medium.get_scattering_coefficients(mei)
    assert dr.allclose(sigma_s, [0.4, 0.8, 1.2])
    assert dr.allclose(sigma_n, 0.0)
    assert dr.allclose(sigma_t, 2.0)


def test03_interaction_fields_without_volume_metadata_are_rejected(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="Volume role|VolumeField"):
        mi.load_dict({
            "type": "homogeneous",
            "albedo": {
                "type": "sinusoidalfield",
                "input_dim": 3,
                "out_dim": 6,
            },
        })


def test04_sigma_t_forwards_inactive_mask(variants_all_ad_rgb_unpolarized):
    class InactiveSensitiveValue(mi.Field):
        def __init__(self):
            mi.Field.__init__(self, mi.Properties("inactive_sensitive_value"))
            self.last_active = None

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_1(self, si, args=None, active=True):
            self.last_active = active
            return dr.select(active, 2.0, 0.0)

        def max(self):
            return 2.0

    value = InactiveSensitiveValue()
    medium = mi.load_dict({
        "type": "homogeneous",
        "albedo": 1.0,
        "sigma_t": {
            "type": "constvolume",
            "value": value,
        },
    })

    mei = dr.zeros(mi.MediumInteraction3f, 1)
    active = mi.Bool(False)
    assert dr.allclose(medium.get_majorant(mei, active), 0.0)
    assert value.last_active is not None
    assert not dr.any(value.last_active)
