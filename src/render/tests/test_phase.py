import pytest
import drjit as dr
import mitsuba as mi


def test01_trampoline(variants_vec_backends_once_rgb):
    class DummyPhaseFunction(mi.PhaseFunction):
        def __init__(self, props):
            mi.PhaseFunction.__init__(self, props)
            self.m_flags = mi.PhaseFunctionFlags.Anisotropic

        def to_string(self):
            return f"DummyPhaseFunction ({self.m_flags})"

    mi.register_phasefunction('dummy_phase', DummyPhaseFunction)
    phase = mi.load_dict({
        'type': 'dummy_phase'
    })

    assert str(phase) == "DummyPhaseFunction (2)"
