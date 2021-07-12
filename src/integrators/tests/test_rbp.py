import enoki as ek
import mitsuba
import pytest


def make_simple_scene(res=1):
    from mitsuba.core import xml, ScalarTransform4f as T

    return xml.load_dict({
        'type': 'scene',
        'integrator': { 'type': 'path' },
        'mysensor': {
            'type': 'perspective',
            'near_clip': 0.1,
            'far_clip': 1000.0,
            'to_world': T.look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
            'myfilm': {
                'type': 'hdrfilm',
                'rfilter': { 'type': 'box'},
                'width': res,
                'height': res,
            },
            'mysampler': {
                'type': 'independent',
                'sample_count': 1,
            },
        },
        'whitewall': { 'type': 'rectangle' },
        'redwall': {
            'type': 'rectangle',
            'bsdf' : {
                'type' : 'diffuse',
                'reflectance' : { 'type' : 'rgb', 'value' : [0.99, 0.1, 0.1] }
            },
            'to_world': T.translate([-1, 0, 0]) * T.rotate([0, 1, 0], 90)
        },
        # 'arealight': {
        #     'type': 'rectangle',
        #     'bsdf' : {
        #         'type' : 'diffuse',
        #         'reflectance' : { 'type' : 'rgb', 'value' : [0.0, 0.0, 0.0] }
        #     },
        #     'to_world': T.translate([1, 0, 0]) * T.rotate([0, 1, 0], -90),
        #     'emitter': { 'type': 'area' }
        # },
        'emitter': {
            'type': 'point',
            'position': [0, 0, 1]
        }
    })

# if hasattr(ek, 'JitFlag'):
#     jit_flags_options = [
#         {ek.JitFlag.VCallRecord: 0, ek.JitFlag.LoopRecord: 0},
#         {ek.JitFlag.VCallRecord: 1, ek.JitFlag.LoopRecord: 0},
#         {ek.JitFlag.VCallRecord: 1, ek.JitFlag.LoopRecord: 1},
#     ]
# else:
#     jit_flags_options = []

# def test_gradients(variants_all_ad_rgb):
@pytest.mark.skip()
def test_gradients(variant_cuda_ad_rgb):
    from mitsuba.core import xml, Color3f
    from mitsuba.python.util import traverse
    from mitsuba.python.autodiff import write_bitmap

    scene = make_simple_scene(res=16)
    crop_size = scene.sensors()[0].film().crop_size()

    spp_primal = 4024 * 1
    spp_adjoint = 4024 * 1
    max_depth = 3

    # ek.set_flag(ek.JitFlag.VCallRecord, False)
    # ek.set_flag(ek.JitFlag.LoopRecord,  False)
    # ek.set_flag(ek.JitFlag.LoopOptimize,  False)

    rbp_integrator = xml.load_dict({
        'type': 'rbp',
        'max_depth': max_depth
    })
    path_integrator = xml.load_dict({
        'type': 'path',
        'max_depth': max_depth
    })

    # Rendering reference image
    scene.sensors()[0].sampler().seed(0)
    image_ref = rbp_integrator.render(scene, spp=spp_primal)
    write_bitmap('path_ref.exr', image_ref, crop_size)

    params = traverse(scene)

    # Update scene parameter
    key = 'redwall.bsdf.reflectance.value'
    params.keep([key])
    params[key] = Color3f([0.1, 0.9, 0.9])
    params.update()

    # Render primal image
    scene.sensors()[0].sampler().seed(0)
    image = rbp_integrator.render(scene, spp=spp_primal)
    write_bitmap('path_init.exr', image, crop_size)

    # Compute adjoint image (MSE objective function)
    ek.enable_grad(image)
    ob_val = ek.hsum_async(ek.sqr(image - image_ref)) / len(image)
    ek.backward(ob_val)
    image_adj = ek.grad(image)
    ek.set_grad(image, 0.0)
    rgb_image_adj = ek.unravel(Color3f, ek.detach(image_adj))
    write_bitmap('path_adj.exr', image_adj, crop_size)

    # print('Adjoint rendering ----')
    # scene.sensors()[0].sampler().seed(0)
    rbp_integrator.render_adjoint(scene, params, image_adj, spp=spp_adjoint)
    grad_backward = ek.grad(params[key])

    # # Forward render with path tracer
    # print('Forward rendering ----')

    ek.set_flag(ek.JitFlag.VCallRecord, False)
    ek.set_flag(ek.JitFlag.LoopRecord,  False)

    params = traverse(scene)
    ek.set_grad(params[key], 0.0)
    ek.enable_grad(params[key])
    # scene.sensors()[0].sampler().seed(0)
    image = path_integrator.render(scene, spp=spp_primal)
    fwd_image = ek.unravel(Color3f, image)
    ek.forward(params[key])
    fwd_diff_grad_image_rgb = ek.grad(fwd_image)

    grad_forward = Color3f([ek.dot(fwd_diff_grad_image_rgb[i], rgb_image_adj[i]) for i in range(3)])

    print(f'grad_forward:  {grad_forward}')
    print(f'grad_backward: {grad_backward}')
    # assert ek.allclose(grad_forward, grad_backward)
