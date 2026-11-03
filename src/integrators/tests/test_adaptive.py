import drjit as dr
import mitsuba as mi
import pytest

def test_adaptive_sampling(variants_vec_rgb):
    pytest.skip()
    scene = mi.load_dict(mi.cornell_box())

    adaptive_integrator = mi.load_dict({
        'type': 'adaptive_sampling',
        'threshold': 0.05,
        'pass_spp': 16,
        'nested': scene.integrator()
    })

    def callback(pass_i: int,
                 accum_spp: int,
                 pass_spp: int,
                 img: mi.TensorXf,
                 odd_img: mi.TensorXf,
                 error: mi.TensorXf,
                 active: mi.TensorXf):
        print(f'Adaptive sampling pass {pass_i}, rendering {dr.count(active)} pixels with {pass_spp} spp.')
        return True

    adaptive_integrator.set_callback(callback)

    img = mi.render(scene, integrator=adaptive_integrator, spp=512)
    # img = mi.render(scene, integrator=scene.integrator(), spp=512)
    # mi.Bitmap(img).write('output.exr')
