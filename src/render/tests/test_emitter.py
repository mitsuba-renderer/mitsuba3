import pytest
import drjit as dr
import mitsuba as mi


def test01_sampling_weight_getter(variants_vec_rgb):
    emitter0 = mi.load_dict({
        'type': 'constant',
        'sampling_weight': 0.1
    })
    emitter1 = mi.load_dict({
        'type': 'constant',
        'sampling_weight': 0.3
    })

    emitter0_ptr = mi.EmitterPtr(emitter0)
    emitter1_ptr = mi.EmitterPtr(emitter1)

    emitters = dr.select(mi.Bool([True, False, False]), emitter0_ptr, emitter1_ptr)

    weight = emitters.sampling_weight()
    assert dr.allclose(weight, [0.1, 0.3, 0.3])

    params = mi.traverse(emitter1)
    params['sampling_weight'] = 0.7
    params.update()

    weight = emitters.sampling_weight()
    assert dr.allclose(weight, [0.1, 0.7, 0.7])


def test02_trampoline(variants_vec_backends_once_rgb):
    class DummyEmitter(mi.Emitter):
        def __init__(self, props):
            mi.Emitter.__init__(self, props)
            self.m_flags = mi.EmitterFlags.SpatiallyVarying

        def to_string(self):
            return f"DummyEmitter ({self.m_flags})"

    mi.register_emitter('dummy_emitter', DummyEmitter)
    emitter = mi.load_dict({
        'type': 'dummy_emitter'
    })

    assert str(emitter) == "DummyEmitter (16)"
