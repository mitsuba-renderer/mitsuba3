import mitsuba
import pytest
import enoki as ek


def make_simple_scene(res=1):
    from mitsuba.core import xml, ScalarTransform4f

    return xml.load_dict({
        'type' : 'scene',
        "integrator" : { "type" : "path" },
        "mysensor" : {
            "type" : "perspective",
            "near_clip": 0.1,
            "far_clip": 1000.0,
            "to_world" : ScalarTransform4f.look_at(origin=[0, 0, 4],
                                                   target=[0, 0, 0],
                                                   up=[0, 1, 0]),
            "myfilm" : {
                "type" : "hdrfilm",
                "rfilter" : { "type" : "box"},
                "width" : res,
                "height" : res,
            },
            "mysampler" : {
                "type" : "independent",
                "sample_count" : 1,
            },
        },
        'rect' : {
            'type' : 'rectangle',
            'id' : 'rect',
            "bsdf" : {
                "type" : "diffuse",
                "reflectance" : {
                    "type" : "rgb",
                    "value" : [0.6, 0.6, 0.6]
                }
            }
        },
        "emitter" : {
            "type" : "point",
            "position" : [0, 0, 5]
        }
    })


@pytest.mark.parametrize("spp", [1, 4])
def test01_bsdf_reflectance_backward(variants_all_autodiff_rgb, spp):
    # Test correctness of the gradients taking one step of gradient descent on linear function
    from mitsuba.python.util import traverse
    from mitsuba.python.autodiff import render

    scene = make_simple_scene()

    # Parameter to differentiate
    key = 'rect.bsdf.reflectance.value'

    # Enable gradients
    params = traverse(scene)
    ek.enable_grad(params[key])

    # Forward rendering - first time
    scene.sensors()[0].sampler().seed(0)
    img_1 = render(scene, spp=spp)

    # Backward pass and gradient descent step
    loss = ek.hsum_async(img_1)
    ek.backward(loss)

    grad = ek.grad(params[key])

    lr = 0.01
    params[key][0] += lr
    params.update()

    # Forward rendering - second time
    scene.sensors()[0].sampler().seed(0)
    img_2 = render(scene, spp=spp)

    new_loss = ek.hsum_async(img_2)

    assert ek.allclose(loss, new_loss - lr * grad[0])


@pytest.mark.parametrize("spp", [1, 4])
def test02_bsdf_reflectance_forward(variants_all_autodiff_rgb, spp):
    # Test correctness of the gradients taking one step of gradient descent on linear function
    from mitsuba.core import Float
    from mitsuba.python.util import traverse
    from mitsuba.python.autodiff import render

    scene = make_simple_scene()

    # Parameter to differentiate
    key = 'rect.bsdf.reflectance.value'

    # Enable gradients
    params = traverse(scene)

    # Add differential value to the BSDF reflectance
    X = Float(0.1)
    ek.enable_grad(X)
    params[key] += X
    params.update()

    # Forward rendering - first time
    scene.sensors()[0].sampler().seed(0)
    img_1 = render(scene, spp=spp)

    # Compute forward gradients
    assert len(ek.grad(img_1)) == 0
    ek.forward(X)
    grad = ek.grad(img_1)

    # Modify reflectance value and re-render
    lr = 0.1
    params[key] += lr
    params.update()

    scene.sensors()[0].sampler().seed(0)
    img_2 = render(scene, spp=spp)

    assert ek.allclose(img_1, img_2 - lr * grad)


from mitsuba.python.autodiff import Adam, SGD

@pytest.mark.parametrize("spp", [8])
@pytest.mark.parametrize("unbiased", [False]) # TODO refactoring
@pytest.mark.parametrize("opt_conf", [(SGD, [200.5, 0.9])])
def test03_optimizer(variants_all_autodiff_rgb, spp, unbiased, opt_conf):
    from mitsuba.core import Float, Color3f
    from mitsuba.python.util import traverse
    from mitsuba.python.autodiff import render

    scene = make_simple_scene(res=3)

    key = 'rect.bsdf.reflectance.value'

    params = traverse(scene)
    param_ref = Color3f(params[key])

    scene.sensors()[0].sampler().seed(0)
    image_ref = render(scene, spp=spp)

    opt_type, opt_args = opt_conf
    opt = opt_type(*opt_args, params=params)

    opt[key] = Color3f(0.1)
    opt.update()

    avrg_params = Color3f(0)

    # Total iteration count
    N = 100
    # Average window size
    W = 10

    for it in range(N):
        # Perform a differentiable rendering of the scene
        scene.sensors()[0].sampler().seed(0)
        image = render(scene, optimizer=opt, unbiased=unbiased, spp=spp)

        # Objective: MSE between 'image' and 'image_ref'
        ob_val = ek.hsum_async(ek.sqr(image - image_ref)) / len(image)

        # Back-propagate errors to input parameters
        ek.backward(ob_val)

        # Optimizer: take a gradient step
        opt.step()

        # Optimizer: Update the scene parameters
        opt.update()

        err_ref = ek.hsum(ek.detach(ek.sqr(param_ref - params[key])))[0]
        print('Iteration %03i: error=%g' % (it, err_ref))

        if it >= N - W:
            avrg_params += params[key]

    avrg_params /= W

    assert ek.allclose(avrg_params, param_ref, atol=1e-3)
