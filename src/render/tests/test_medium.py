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
