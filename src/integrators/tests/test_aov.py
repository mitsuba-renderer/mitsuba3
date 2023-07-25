import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import find_resource

def test01_construct(variants_all):
    # Single AOV
    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'dd.y:depth',
        'my_image': {
            'type': 'path'
        }
    })

    assert aov_integrator is not None

    # Mutliple AOVs
    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'dd.y:depth,nn:sh_normal',
        'my_image': {
            'type': 'path'
        }
    })

    assert aov_integrator is not None

    # Error if missing AOVs
    with pytest.raises(RuntimeError):
        mi.load_dict({
            'type': 'aov',
            'my_image': {
                'type': 'path'
            }
        })

def test02_radiance_consistent(variants_all_rgb):
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'))

    path_integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 6
    })

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'dd.y:depth,nn:sh_normal',
        'my_image': path_integrator
    })

    spp = 16
    path_image = path_integrator.render(scene, seed=0, spp=spp)
    aovs_image = aov_integrator.render(scene, seed=0, spp=spp)

    # Make sure radiance is consistent
    assert(dr.allclose(path_image, aovs_image[:,:,:3]))

def test03_supports_multiple_inner_integrators(variants_all_rgb):
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'))

    path_integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 6
    })

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'dd.y:depth,nn:sh_normal',
        'my_image': path_integrator,
        'my_image2' : path_integrator
    })

    spp = 16
    aovs_image = aov_integrator.render(scene, seed=0, spp=spp)

    # Make sure radiance is consistent between two inner integrators
    assert(dr.allclose(aovs_image[:,:,:3], aovs_image[:,:, 3:6]))

def test04_check_aov_correct(variants_all_rgb):

    albedo = 0.4
    camera_offset = 1
    plane_offset = -1

    plane = {
        'type' : 'rectangle',
        'material': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': albedo
            }
        },
        'to_world' : mi.ScalarTransform4f.scale([10.0, 10.0, 1.0]).translate([0,0,plane_offset])
    }

    path_integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 6
    })

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'nn:sh_normal,ab:albedo,pos:position',
        'my_image': path_integrator,
    })

    scene = mi.load_dict({
        'type': 'scene',
        'sensor': {
            'type': 'orthographic',
            'to_world': mi.ScalarTransform4f.look_at(
                origin=(0, 0, camera_offset),
                target=(0, 0, plane_offset),
                up=(0, 1, 0),
            ),
            'sampler': {
                'type': 'independent'
            },
            'film': {
                'type': 'hdrfilm',
                'width': 128, 'height': 128,
                'rfilter': {'type': 'box'}
            },
        },
        'emitter' : {
            'type': 'constant',
            'radiance': {
                'type': 'rgb',
                'value': 1.0,
            }
        },
        'plane' : plane
    })

    image = aov_integrator.render(scene, seed=0, spp=32)

    image_dim = image.shape[0] * image.shape[1]

    # Check normal
    assert(dr.allclose(image[:,:,3:6].array, [0,0,1] * image_dim))

    # Check albedo
    assert(dr.allclose(image[:,:, 6:9].array, [albedo, albedo, albedo] * image_dim))

    # Check z-pos
    assert(dr.allclose(image[:,:, -1:].array, [plane_offset] * image_dim))