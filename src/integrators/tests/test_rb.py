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
        'whitewall': {
            'type': 'rectangle',
        },
        'redwall': {
            'type': 'rectangle',
            'bsdf' : {
                'type' : 'diffuse',
                'reflectance' : { 'type' : 'rgb', 'value' : [0.99, 0.1, 0.1] }
            },
            'to_world': T.translate([-1, 0, 0]) * T.rotate([0, 1, 0], 90)
        },
        'arealight': {
            'type': 'rectangle',
            'bsdf' : {
                'type' : 'diffuse',
                'reflectance' : { 'type' : 'rgb', 'value' : [0.0, 0.0, 0.0] }
            },
            'to_world': T.translate([1, 0, 0]) * T.rotate([0, 1, 0], -90),
            'emitter': { 'type': 'area' }
        },
        'emitter': {
            'type': 'point',
            'position': [0, 0, 1]
        }
    })

if hasattr(ek, 'JitFlag'):
    jit_flags_options = [
        {ek.JitFlag.VCallRecord: 0, ek.JitFlag.LoopRecord: 0},
        {ek.JitFlag.VCallRecord: 1, ek.JitFlag.LoopRecord: 0},
        {ek.JitFlag.VCallRecord: 1, ek.JitFlag.LoopRecord: 1, ek.JitFlag.LoopOptimize: 0},
        {ek.JitFlag.VCallRecord: 1, ek.JitFlag.LoopRecord: 1, ek.JitFlag.LoopOptimize: 1},
    ]
else:
    jit_flags_options = []

@pytest.mark.slow
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test_gradients(variants_all_ad_rgb, jit_flags):
    from mitsuba.core import xml, Color3f
    from mitsuba.python.util import traverse, write_bitmap

    # Set enoki JIT flags
    for k, v in jit_flags.items():
        ek.set_flag(k, v)

    scene = make_simple_scene(res=1)

    spp_primal = 4024 * 16
    spp_adjoint = 4024 * 16
    max_depth = 3
    epsilon = 1e-2

    ek.set_flag(ek.JitFlag.VCallRecord,   True)
    ek.set_flag(ek.JitFlag.VCallOptimize, True)
    ek.set_flag(ek.JitFlag.LoopRecord,    True)
    ek.set_flag(ek.JitFlag.LoopOptimize,  True)

    rb_integrator = xml.load_dict({
        'type': 'rb',
        'max_depth': max_depth,
        'recursive_li': True
    })

    # Rendering reference image
    image_1 = rb_integrator.render(scene, seed=0, spp=spp_primal)
    # write_bitmap('image_1.exr', image_1)

    # Update scene parameter
    params = traverse(scene)
    key = 'redwall.bsdf.reflectance.value'
    params.keep([key])
    params[key] += epsilon
    params.update()

    # Render primal image
    scene.sensors()[0].sampler().seed(0)
    image_2 = rb_integrator.render(scene, seed=0, spp=spp_primal)
    # write_bitmap('image_2.exr', image_2)

    # Compute adjoint image (MSE objective function)
    ek.enable_grad(image_2)
    ob_val = ek.hsum_async(ek.sqr(image_2 - image_1)) / len(image_2)
    ek.backward(ob_val)
    image_adj = ek.grad(image_2)
    ek.set_grad(image_2, 0.0)
    # write_bitmap('image_adj.exr', image_adj)

    # scene.sensors()[0].sampler().seed(0)
    rb_integrator.render_backward(scene, params, image_adj, seed=0, spp=spp_adjoint)
    grad_backward = ek.grad(params[key])

    # Finite difference
    fd_image_grad = ek.abs(image_1 - image_2) / epsilon
    rgb_image_adj = ek.unravel(Color3f, ek.detach(image_adj.array))
    rgb_fd_image_grad = ek.unravel(Color3f, ek.detach(fd_image_grad.array))
    grad_fd = rgb_fd_image_grad * rgb_image_adj

    # Compare the gradients
    print(f'grad_fd:  {grad_fd}')
    print(f'grad_backward: {grad_backward}')

    assert ek.allclose(grad_fd, grad_backward, rtol=0.05, atol=1e-10)
