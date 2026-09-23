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
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'), res=32)

    path_integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 6
    })

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'dd.y:depth,nn:sh_normal',
        'my_image': path_integrator
    })

    spp = 4
    path_image = path_integrator.render(scene, seed=0, spp=spp)
    aovs_image = aov_integrator.render(scene, seed=0, spp=spp)

    # The first nested integrator sees the same random numbers as when
    # rendering alone, hence the radiance must match
    assert(dr.allclose(path_image, aovs_image[:,:,:3]))


def test03_supports_multiple_inner_integrators(variants_all_rgb):
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'), res=32)

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

    spp = 32
    aovs_image = aov_integrator.render(scene, seed=0, spp=spp)

    # The inner integrators share the sampler and therefore have independent
    # noise. Their mean radiance must agree.
    import numpy as np
    image = np.array(aovs_image)
    assert np.allclose(image[:, :, :3].mean(axis=(0, 1)),
                       image[:, :, 3:6].mean(axis=(0, 1)), rtol=2e-2)

    # The first inner integrator occupies the base channels of the film, the
    # second one receives a group of channels named after it
    film = scene.sensors()[0].film()
    assert film.channels() == ['R', 'G', 'B', 'my_image2.R', 'my_image2.G',
                               'my_image2.B', 'dd.y.T', 'nn.X', 'nn.Y', 'nn.Z']
    assert dr.allclose(aovs_image, film.develop())


# With a wide filter or a nested integrator that has its own pass structure,
# correct AOVs require that all sources share sample positions and weights
@pytest.mark.parametrize('rfilter,samples_per_pass', [
    ('box', None), ('gaussian', None), ('gaussian', 1)])
def test04_check_aov_correct(variants_all_rgb, rfilter, samples_per_pass):
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
        'to_world' : mi.ScalarTransform4f().scale([10.0, 10.0, 1.0]).translate([0,0,plane_offset])
    }

    path_dict = {
        'type': 'path',
        'max_depth': 6
    }
    if samples_per_pass is not None:
        path_dict['samples_per_pass'] = samples_per_pass
    path_integrator = mi.load_dict(path_dict)

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'nn:sh_normal,ab:albedo,pos:position',
        'my_image': path_integrator,
    })

    scene = mi.load_dict({
        'type': 'scene',
        'sensor': {
            'type': 'orthographic',
            'to_world': mi.ScalarTransform4f().look_at(
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
                'rfilter': {'type': rfilter}
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


def test05_check_aov_film(variants_all_rgb):
    import numpy as np
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'), res=32)

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

    film = scene.sensors()[0].film()

    path_integrator.render(scene, seed=0, spp=spp)
    bitmap_path = film.bitmap()

    _ = aov_integrator.render(scene, seed=0, spp=spp)
    bitmap_aov = film.bitmap()

    # Make sure radiance is consistent
    assert(np.allclose(bitmap_aov.split()[0][1],bitmap_path.split()[0][1]))


def test06_test_aov_ad_forward(variants_all_ad_rgb):
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'), res=32)

    path_integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 6
    })

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'ab:albedo,dd.y:depth,nn:sh_normal',
        'my_image': path_integrator
    })

    spp = 16

    key = 'red.reflectance.value'
    params = mi.traverse(scene)

    dr.enable_grad(params[key])
    params.update()

    image = mi.render(scene, params, integrator=path_integrator, spp=spp)
    dr.forward(params[key])
    grad_image = dr.grad(image)
    dr.eval(grad_image)

    aov_image = mi.render(scene, params, integrator=aov_integrator, spp=spp)
    dr.forward(params[key])
    grad_aov_image = dr.grad(aov_image)
    dr.eval(grad_aov_image)

    assert dr.allclose(grad_image, grad_aov_image[:,:,:3])


def test06_test_aov_ad_backward(variants_all_ad_rgb):
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'), res=32)

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

    key = 'red.reflectance.value'
    params = mi.traverse(scene)

    dr.enable_grad(params[key])
    params.update()

    image = mi.render(scene, params, integrator=path_integrator, spp=spp)
    dr.backward(image)
    grad_path = dr.grad(params[key])
    dr.eval(grad_path)

    dr.set_grad(params[key], 0) # Reset to zero
    params.update()

    aov_image = mi.render(scene, params, integrator=aov_integrator, spp=spp)
    dr.backward_from(aov_image[:,:,:3])
    grad_aov = dr.grad(params[key])
    dr.eval(grad_aov)

    assert dr.allclose(grad_path, grad_aov)


def test07_test_aov_normalmap(variants_all_ad_rgb):
    camera_offset = 1
    plane_offset = -1
    n = dr.normalize(mi.Float([0.0,0.5,1.0]))

    plane = {
        'type' : 'rectangle',
        'material': {
            'type': 'normalmap',
            'normalmap' : {
                'type': 'bitmap',
                 # Normalmap expects data in domain [0,1] rather than [-1, 1]
                'data': mi.TensorXf([[(n + 1) * 0.5]]),
                'raw': True,
                'filter_type': 'nearest'
            },
            'bsdf' : {
                'type': 'diffuse',
                'reflectance': {
                    'type': 'rgb',
                    'value': 0.4
                }
            }
        },
        'to_world' : mi.ScalarTransform4f().
            scale([10.0, 10.0, 1.0]).
            translate([0,0,plane_offset])
    }

    path_integrator = mi.load_dict({
        'type': 'path',
        'max_depth': 6
    })

    aov_integrator = mi.load_dict({
        'type': 'aov',
        'aovs': 'nn:sh_normal',
        'my_image': path_integrator,
    })

    scene = mi.load_dict({
        'type': 'scene',
        'sensor': {
            'type': 'orthographic',
            'to_world': mi.ScalarTransform4f().look_at(
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

    image = aov_integrator.render(scene, seed=0, spp=4)

    assert(dr.allclose(
        image[:,:,3:6].array,
        dr.tile(n, image.shape[0] * image.shape[1])))


def test08_nested_aov_integrators(variants_all_rgb):
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'), res=32)

    direct_integrator = mi.load_dict({'type': 'direct'})
    outer_aov = mi.load_dict({
        'type': 'aov',
        'aovs': 'dd:depth',
        'inner_aov': {
            'type': 'aov',
            'aovs': 'nn:sh_normal',
            'inner_direct': direct_integrator,
        },
    })

    spp = 4
    direct_image = direct_integrator.render(scene, seed=0, spp=spp)
    aov_image = outer_aov.render(scene, seed=0, spp=spp)
    # The image of the innermost integrator occupies the base channels of the
    # film at every level of nesting
    expected_aov_names = [
        'inner_aov.nn.X',
        'inner_aov.nn.Y',
        'inner_aov.nn.Z',
        'dd.T',
    ]
    assert outer_aov.aov_names(scene.sensors()[0].film()) == expected_aov_names
    # Developed RGB image contains 7 channels (3 base + 3 normals + 1 depth)
    assert aov_image.shape[2] == 7
    assert dr.allclose(direct_image[:, :, :3], aov_image[:, :, :3], atol=1e-2)
    assert dr.any(aov_image[:, :, 3:6] != 0.0) # non-zero normal values
    assert dr.any(aov_image[:, :, 6] > 0.0)  # positive depth values

    # The developed image is what the film holds
    film = scene.sensors()[0].film()
    assert film.channels() == ['R', 'G', 'B'] + expected_aov_names
    assert dr.allclose(aov_image, film.develop())

    # Verify that bitmap.split() cleanly splits channels into expected layer sub-bitmaps
    split_layers = dict(film.bitmap().split())
    assert split_layers.keys() == {
        '<root>',
        'inner_aov.nn',
        'dd',
    }

    # <root> layer contains the developed base RGB image
    root_layer = mi.TensorXf(split_layers['<root>'])
    assert dr.allclose(direct_image[:, :, :3], root_layer[:, :, :3], atol=1e-2)
