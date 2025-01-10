import pytest
import drjit as dr
import mitsuba as mi


def test01_trampoline(variants_vec_backends_once_rgb):
    class DummyMedium(mi.Medium):
        def __init__(self, props):
            mi.Medium.__init__(self, props)
            self.m_is_homogeneous = True

        def to_string(self):
            return f"DummyMedium ({self.m_is_homogeneous})"

    mi.register_medium('dummy_medium', DummyMedium)
    medium = mi.load_dict({
        'type': 'dummy_medium'
    })

    assert str(medium) == "DummyMedium (True)"


def test02_override(variants_vec_backends_once_rgb):
    class OverrideMedium(mi.Medium):
        def __init__(self, props):
            mi.Medium.__init__(self, props)
            self.m_is_homogeneous = False
            self.interaction_probability_trampoline_count = 0

        def get_interaction_probabilities(self, radiance, mei, throughput):
            self.interaction_probability_trampoline_count += 1
            return (
                (dr.zeros(mi.UnpolarizedSpectrum), dr.ones(mi.UnpolarizedSpectrum)),
                (dr.zeros(mi.UnpolarizedSpectrum), dr.ones(mi.UnpolarizedSpectrum)),
            )

        def to_string(self):
            return f"OverrideMedium ({self.m_is_homogeneous})"

    mi.register_medium('override_medium', OverrideMedium)
    medium = mi.load_dict({
        'type': 'override_medium'
    })

    rad, mei, throughput = dr.zeros(mi.Spectrum), dr.zeros(mi.MediumInteraction3f), dr.zeros(mi.Spectrum)
    test_output = medium.get_interaction_probabilities(rad, mei, throughput)

    assert medium.interaction_probability_trampoline_count == 1
    assert dr.allclose(test_output[0][0], dr.zeros(mi.UnpolarizedSpectrum))
    assert dr.allclose(test_output[1][0], dr.zeros(mi.UnpolarizedSpectrum))
    assert dr.allclose(test_output[0][1], dr.ones(mi.UnpolarizedSpectrum))
    assert dr.allclose(test_output[1][1], dr.ones(mi.UnpolarizedSpectrum))
