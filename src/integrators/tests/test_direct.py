import pytest
import drjit as dr
import mitsuba as mi

def test01_delta_emitter_samples_weighted(variants_all_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'sensor': {
            'type': 'perspective',
            'to_world': mi.ScalarTransform4f().look_at(
                origin=(0, 0, 5),
                target=(0, 0, 0),
                up=(0, 1, 0),
            ),
            'sampler': {
                'type': 'independent',
                'sample_count': 1
            },
            'film': {
                'type': 'hdrfilm',
                'width': 128, 'height': 128,
                'crop_offset_x': 64,
                'crop_offset_y': 64,
                'crop_width': 1,
                'crop_height': 1
            },
        },
        'emitter' : {
            'type': 'point',
            'position': [0.0, 0.0, 2.0],
            'intensity': {
                'type': 'rgb',
                'value': 1.0,
            }
        },
        'sphere' : {
            'type': 'sphere',
            'center': [0.0, 0.0, 0.0],
            'radius': 1,
            'bsdf': {
                'type': 'diffuse'
            }
        }
    })

    integrator = mi.load_dict({
        'type': 'direct',
        'emitter_samples': 1,
        'bsdf_samples': 0,
    })
    single_sample_img = mi.render(scene, integrator=integrator)

    integrator = mi.load_dict({
        'type': 'direct',
        'emitter_samples': 10,
        'bsdf_samples': 0,
    })
    multi_sample_img = mi.render(scene, integrator=integrator)
    # The number of emitter samples should not affect a scene with a single
    # point light source.
    diff = single_sample_img.array - multi_sample_img.array
    assert dr.allclose(diff, 0)