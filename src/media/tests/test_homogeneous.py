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
