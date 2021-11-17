import enoki as ek
import mitsuba
import pytest

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def create_test_scene(integ="prpt", max_depth=4, emitter='area',
                      scene_idx=0, hide_emitters=False, crop_window=None):
    from mitsuba.core import ScalarTransform4f
    from mitsuba.core.xml import load_dict
    integrator = {
        'type': integ,
        # 'samples_per_pass': 16,
        'rr_depth': 9,
        'max_depth': max_depth,
        'hide_emitters': hide_emitters,
    }
    scene = {
        'type': 'scene',
        'integrator': integrator,
        'sensor': {
            'type': 'perspective',
            'fov_axis': 'smaller',
            'near_clip': 0.1,
            'far_clip': 100,
            'fov': 80,
            'to_world': ScalarTransform4f.look_at(
                origin=(0, 0, 0),
                target=(0, 0, -1),
                up=(0, 1, 0),
            ),
            'sampler': {
                'type': 'independent',
                'sample_count': 8192
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

    if emitter == 'area':
        emitter = {
            'type': 'obj',
            'filename': 'resources/data/common/meshes/front_back_wall.obj',
            'to_world': ScalarTransform4f.scale([4.0, 4.0, 4.0]),
            'bsdf' : {
                'type': 'twosided',
                'bsdf' : {
                    'type': 'diffuse',
                    'reflectance' : { 'type' : 'rgb', 'value' : [0.4, 0.4, 0.4] }
                }
            },
            'area_emitter': {
                'type': 'area',
                'radiance': {
                    'type': 'rgb',
                    'value': (1.0, 0.5, 0.2)
                },
            },
        }
        scene['shape'] = emitter

    scene = load_dict(scene)
    return scene, scene.integrator()


def test01_render_emitter(variants_all_ad_rgb):
    from mitsuba.render import ScatteringIntegrator
    from mitsuba.core import Bitmap

    scene, integrator = create_test_scene(max_depth=1, hide_emitters=False)
    assert isinstance(integrator, ScatteringIntegrator)
    image = integrator.render(scene, seed=0, spp=8192*4, develop_film=True)
    #Bitmap(image).write(f'test01_{mitsuba.variant()}.exr')
    rgb = [ek.hmean(image[:, :, i]) for i in range(3)]
    assert ek.allclose(rgb, [1.0, 0.5, 0.2], rtol=0.001), "directly visible emitter radiance"

@pytest.mark.parametrize('max_depth', [2, 5, 9])
def test02_render_simple(variants_all_ad_rgb, max_depth):
    from mitsuba.render import ScatteringIntegrator

    scene, integrator_prpt = create_test_scene(max_depth=max_depth, hide_emitters=False)
    scene_, integrator_path = create_test_scene(integ='path', max_depth=max_depth, hide_emitters=False)
    assert isinstance(integrator_prpt, ScatteringIntegrator)
    # ptracer converges slower
    image_prpt = integrator_prpt.render(scene, seed=0, spp=8192*32, develop_film=True)
    image_path = integrator_path.render(scene, seed=0, spp=8192*4, develop_film=True)

    assert ek.allclose(image_path, image_prpt, rtol=0.01), "rendering comparison with path integrator"

def test03_simple_derivative(variants_all_ad_rgb):

    # scene, integrator = create_test_scene(max_depth=5, hide_emitters=False)
    

    # assert ek.allclose(image_path, image_prpt, rtol=0.01), "rendering comparison with path integrator"

def test04_cbox_derivative(variants_all_ad_rgb):

    scene = xml.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'))
    # ptracer converges slower
    #image_prpt = integrator_prpt.render(scene, seed=0, spp=8192*32, develop_film=True)

    #assert ek.allclose(image_path, image_prpt, rtol=0.01), "rendering comparison with path integrator"
