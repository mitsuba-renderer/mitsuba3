import pytest
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
