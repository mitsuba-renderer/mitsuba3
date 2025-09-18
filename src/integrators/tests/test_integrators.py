import pytest
import drjit as dr
import mitsuba as mi

def test01_trampoline_id(variants_vec_backends_once_rgb):
    class DummyIntegrator(mi.SamplingIntegrator):
        def __init__(self, props):
            mi.SamplingIntegrator.__init__(self, props)
            self.depth = props['depth']

        def traverse(self, cb):
            cb.put('depth', self.depth, mi.ParamFlags.NonDifferentiable)

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


def test02_path_directly_visible(variants_all_rgb):
    scene_description = mi.cornell_box()
    # Look only at light
    scene_description['sensor']['film']['crop_offset_x'] = 124
    scene_description['sensor']['film']['crop_offset_y'] = 36
    scene_description['sensor']['film']['crop_width'] = 1
    scene_description['sensor']['film']['crop_height'] = 1
    scene = mi.load_dict(scene_description)

    # Should only see the emitter contribution
    integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 1,
        'hide_emitters': False,
    })
    img = mi.render(scene, integrator=integrator)
    assert dr.allclose(img.array, [18.387, 13.9873, 6.75357])

    # Should be compeltely black (we ignore the emitter)
    integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 1,
        'hide_emitters': True,
    })
    img = mi.render(scene, integrator=integrator)
    assert dr.allclose(img.array, 0)
