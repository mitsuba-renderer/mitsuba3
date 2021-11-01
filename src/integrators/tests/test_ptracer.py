import enoki as ek
import mitsuba
import pytest

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def create_test_scene(max_depth=4, emitter='area',
                      shape=None, hide_emitters=False, crop_window=None):
    from mitsuba.core import ScalarTransform4f
    from mitsuba.core.xml import load_dict
    integrator = {
        'type': 'ptracer',
        'samples_per_pass': 16,
        'rr_depth': 9,
        'max_depth': max_depth,
        'hide_emitters': hide_emitters,
    }
    scene = {
        'type': 'scene',
        'integrator': integrator,
        'sensor': {
            'type': 'perspective',
            'to_world': ScalarTransform4f.look_at(
                origin=(2, 0, 0),
                target=(0, 0, 0),
                up=(0, 1, 0),
            ),
            'sampler': {
                'type': 'independent'
            },
            'film': {
                'type': 'hdrfilm',
                'width': 8, 'height': 8,
                # 'width': 128, 'height': 128,
                'rfilter': {'type': 'box'}
            },
        }
    }
    if crop_window:
        f = scene['sensor']['film']
        f['crop_offset_x'] = crop_window[0][0]
        f['crop_offset_y'] = crop_window[0][1]
        f['crop_width'] = crop_window[1][0]
        f['crop_height'] = crop_window[1][1]

    if emitter in ('area', 'texturedarea'):
        emitter = {
            'type': 'rectangle',
            'to_world': ScalarTransform4f.look_at(
                origin=(0, 0, 0),
                target=(1, 0, 0),
                up=(0, 1, 0),
            ),
            'area_emitter': {
                'type': 'area',
                'radiance': (
                    {'type': 'rgb', 'value': (1.0, 0.5, 0.2)}
                    if emitter == 'area' else {
                        'type': 'bitmap',
                        'filename': 'resources/data/common/textures/flower.bmp',
                    }
                ),
            },
        }
    elif emitter == 'directionalarea':
        emitter = {
            'type': 'rectangle',
            'to_world': ScalarTransform4f.look_at(
                origin=(4, 1, 0),
                target=(0, 0, 0),
                up=(0, 1, 0),
            ),
            'area_emitter': {
                'type': 'directionalarea',
                'radiance': {'type': 'rgb', 'value': (1.0, 0.5, 0.2)},
            },
        }
        # Need a receiver plane to bounce light from that emitter
        shape = 'receiver'
    elif emitter == 'constant':
        emitter = {'type': 'constant'}
    elif emitter == 'directional':
        emitter = {
            'type': 'directional',
            'to_world': ScalarTransform4f.rotate(axis=(0, 1, 0), angle=240)
        }
        # Need a receiver plane to bounce light from that emitter
        shape = 'receiver'
    elif emitter == 'envmap':
        emitter = {
            'type': 'envmap',
            'filename': 'resources/data/common/textures/museum.exr',
        }
    if emitter is not None:
        scene['emitter'] = emitter

    if shape == 'receiver':
        scene['receiver'] = {
            'type': 'rectangle',
            'to_world': ScalarTransform4f.look_at(
                origin=(0, 0, 0),
                target=(1, 0, 0),
                up=(0, 1, 0),
            ),
            'bsdf': {'type': 'diffuse'},
        }

    scene = load_dict(scene)
    return scene, scene.integrator()


@pytest.mark.parametrize('emitter', ['area', 'directionalarea', 'texturedarea'])
def test01_render_simple(variants_all, emitter):
    from mitsuba.render import ScatteringIntegrator

    scene, integrator = create_test_scene(emitter=emitter)
    assert isinstance(integrator, ScatteringIntegrator)
    image = integrator.render(scene, seed=0, spp=2, develop_film=True)
    assert ek.count(ek.ravel(image) > 0) >= 0.3 * ek.hprod(ek.shape(image))


@pytest.mark.slow
@pytest.mark.parametrize('emitter', ['constant', 'envmap', 'directional'])
@pytest.mark.parametrize('shape', [None, 'receiver'])
def test02_render_infinite_emitter(variants_all, emitter, shape):
    """Even if the scene has no shape, only the sensor and an infinite emitter,
    we should still be able to render it."""
    scene, integrator = create_test_scene(emitter=emitter, shape=shape)
    image = integrator.render(scene, seed=0, spp=4, develop_film=True)
    # Infinite emitters are sampled very inefficiently, so with such low spp
    # it's possible that the vast majority of pixels are still zero.
    assert ek.count(ek.ravel(image) > 0) >= 0.03 * ek.hprod(ek.shape(image))


@pytest.mark.parametrize('emitter', ['constant', 'envmap', 'area', 'texturedarea'])
def test03_render_hide_emitters(variants_all, emitter):
    """Infinite emitters and directly visible emitters should not
    be visible when hide_emitters is enabled."""
    scene, integrator = create_test_scene(emitter=emitter, shape=None, hide_emitters=True)
    image = integrator.render(scene, seed=0, spp=4, develop_film=True)
    assert ek.all(ek.ravel(image) == 0)

@pytest.mark.slow
@pytest.mark.parametrize('develop', [False, True])
def test04_render_no_emitters(variants_all, develop):
    """It should be possible to render a scene with no emitter to get a black image."""
    scene, integrator = create_test_scene(emitter=None, shape='receiver')

    image = integrator.render(scene, seed=0, spp=2, develop_film=develop)
    if not develop:
        assert len(ek.shape(image)) == 0
        image = scene.sensors()[0].film().develop()
    assert ek.all((image == 0) | ek.isnan(image))


def test05_render_crop_film(variants_all):
    """Infinite emitters and directly visible emitters should not
    be visible when hide_emitters is enabled."""
    offset, size = (2, 3), (6, 4)
    # offset, size = (16, 8), (64, 24)

    # 1. Render full, then crop the resulting image
    scene, integrator = create_test_scene(emitter='texturedarea')
    image1 = integrator.render(scene, seed=0, spp=4, develop_film=True)
    image1_cropped = image1[offset[1]:offset[1]+size[1], offset[0]:offset[0]+size[0], :]
    ek.eval(image1_cropped)

    # 2. Use the film's crop window via the Python API
    film = scene.sensors()[0].film()
    if True:
        film.set_crop_window(offset, size)
        # TODO: ideally, we shouldn't need to call this explicitly
        scene.sensors()[0].parameters_changed()
    image2 = integrator.render(scene, seed=0, spp=4, develop_film=True)
    ek.eval(image2)

    # 3. Specify the crop window at scene loading time
    scene, integrator = create_test_scene(emitter='texturedarea', crop_window=(offset, size))
    image3 = integrator.render(scene, seed=0, spp=4, develop_film=True)
    ek.eval(image3)

    if False:
        from mitsuba.core import Bitmap
        Bitmap(image1).write(f'test05_{mitsuba.variant()}_full.exr')
        Bitmap(image1_cropped).write(f'test05_{mitsuba.variant()}_full_cropped.exr')
        Bitmap(image2).write(f'test05_{mitsuba.variant()}_cropped.exr')
        Bitmap(image3).write(f'test05_{mitsuba.variant()}_loaded.exr')

    # Since we trace the exact same rays regardless of crops, even the noise
    # pattern should be identical
    assert ek.allclose(ek.ravel(image1_cropped), ek.ravel(image2), atol=1e-4), \
        'Cropping the result should be the same as setting a crop window'
    assert ek.allclose(ek.ravel(image2), ek.ravel(image3), atol=1e-4), \
        'Cropping at runtime should be equivalent to cropping at load time'


@pytest.mark.slow
def test06_ptracer_gradients(variants_all_ad_rgb):
    from mitsuba.python.util import traverse
    from mitsuba.python.ad import SGD

    scene, integrator = create_test_scene(emitter='area')
    params = traverse(scene)
    key = 'emitter.emitter.radiance.value'
    params.keep([key])
    params[key] = type(params[key])(1.0)
    params.update()
    opt = SGD(lr=1e-2, params=params)
    opt.load()
    opt.update()

    with ek.suspend_grad():
        image = integrator.render(scene, seed=0, spp=4)

    ek.enable_grad(image)
    loss = ek.hmean_async(image)
    ek.backward(loss)
    adjoint = ek.grad(image)
    ek.set_grad(image, 0.0)

    integrator.render_backward(scene, opt, adjoint, seed=3, spp=4)
    g = ek.grad(params[key])
    assert ek.shape(g) == ek.shape(params[key])
    assert ek.allclose(g, 0.3269110321)
