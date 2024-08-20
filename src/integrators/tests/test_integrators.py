import pytest
import drjit as dr
import mitsuba as mi

def test01_trampoline_id(variants_vec_backends_once_rgb):
    class DummyIntegrator(mi.SamplingIntegrator):
        def __init__(self, props):
            mi.SamplingIntegrator.__init__(self, props)
            self.depth = props['depth']

        def traverse(self, callback):
            callback.put_parameter('depth', self.depth, mi.ParamFlags.NonDifferentiable)

    mi.register_integrator('dummy_integrator', DummyIntegrator)

    scene_description = mi.cornell_box()
    del scene_description['integrator']
    scene_description['my_integrator'] = {
        'type': 'dummy_integrator',
        'depth': 3.1
    }
    scene = mi.load_dict(scene_description)

    params = mi.traverse(scene)
    assert 'my_integrator.depth' in params
