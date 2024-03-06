import pytest
import drjit as dr
import mitsuba as mi


def make_simple_scene(res=1, integrator="path"):
    return mi.load_dict({
        'type' : 'scene',
        "integrator" : { "type" : integrator },
        "mysensor" : {
            "type" : "perspective",
            "near_clip": 0.1,
            "far_clip": 1000.0,
            "to_world" : mi.ScalarTransform4f().look_at(origin=[0, 0, 4],
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


if hasattr(dr, 'JitFlag'):
    jit_flags_options = [
        {dr.JitFlag.VCallRecord : False, dr.JitFlag.VCallOptimize : False, dr.JitFlag.LoopRecord : False},
        {dr.JitFlag.VCallRecord : True, dr.JitFlag.VCallOptimize : False, dr.JitFlag.LoopRecord : False},
        {dr.JitFlag.VCallRecord : True, dr.JitFlag.VCallOptimize : True, dr.JitFlag.LoopRecord : False},
    ]
else:
    jit_flags_options = []


@pytest.mark.parametrize("jit_flags", jit_flags_options)
@pytest.mark.parametrize("spp", [1, 4, 44])
def test01_bsdf_reflectance_backward(variants_all_ad_rgb, jit_flags, spp):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    # Test correctness of the gradients taking one step of gradient descent on linear function
    scene = make_simple_scene()

    # Parameter to differentiate
    key = 'rect.bsdf.reflectance.value'

    # Enable gradients
    params = mi.traverse(scene)
    dr.enable_grad(params[key])

    # Forward rendering - first time
    img_1 = scene.integrator().render(scene, seed=0, spp=spp)

    # Backward pass and gradient descent step
    loss = dr.sum(img_1, axis=None)
    dr.backward(loss)

    grad = dr.grad(params[key])

    lr = 0.01
    params[key][0] += lr
    params.update()

    # Forward rendering - second time
    img_2 = scene.integrator().render(scene, seed=0, spp=spp)

    new_loss = dr.sum(img_2, axis=None)

    assert dr.allclose(loss, new_loss - lr * grad[0])


@pytest.mark.parametrize("jit_flags", jit_flags_options)
@pytest.mark.parametrize("spp", [1, 4])
def test02_bsdf_reflectance_forward(variants_all_ad_rgb, jit_flags, spp):
# def test02_bsdf_reflectance_forward(variant_cuda_ad_rgb, jit_flags, spp):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    # Test correctness of the gradients taking one step of gradient descent on linear function
    scene = make_simple_scene()

    # Parameter to differentiate
    key = 'rect.bsdf.reflectance.value'

    # Enable gradients
    params = mi.traverse(scene)

    # Add differential value to the BSDF reflectance
    X = mi.Float(0.1)
    dr.enable_grad(X)
    params[key] += X
    params.update()

    # Forward rendering - first time
    img_1 = scene.integrator().render(scene, seed=0, spp=spp)

    # Compute forward gradients
    assert dr.all(dr.grad(img_1) == 0.0, axis=None)
    dr.forward(X)
    grad = dr.grad(img_1)

    # Modify reflectance value and re-render
    lr = 0.1
    params[key] += lr
    params.update()

    img_2 = scene.integrator().render(scene, seed=0, spp=spp)

    assert dr.allclose(img_1, img_2 - lr * grad)


@pytest.mark.parametrize("spp", [8])
@pytest.mark.parametrize("res", [3])
@pytest.mark.parametrize("opt_conf", [('SGD', [250.0, 0.8])])
def test03_optimizer(variants_all_ad_rgb, spp, res, opt_conf):
    scene = make_simple_scene(res=res, integrator="direct")

    key = 'rect.bsdf.reflectance.value'

    params = mi.traverse(scene)
    param_ref = mi.Color3f(params[key])
    params.keep(key)

    image_ref = scene.integrator().render(scene, seed=0, spp=spp)

    opt_type, opt_args = opt_conf
    opt = getattr(mi.ad, opt_type)(*opt_args, params=params)

    opt[key] = mi.Color3f(0.1)
    dr.set_label(opt[key], key)
    params.update(opt)

    avrg_params = mi.Color3f(0)

    # Total iteration count
    N = 100
    # Average window size
    W = 10

    for it in range(N):
        # Perform a differentiable rendering of the scene
        image = scene.integrator().render(scene, seed=0, spp=spp)
        dr.set_label(image, 'image')

        # Objective: MSE between 'image' and 'image_ref'
        ob_val = dr.sum(dr.square(image - image_ref), axis=None) / len(image.array)
        dr.set_label(ob_val, 'ob_val')

        # print(dr.graphviz_str(Float(1)))

        # Back-propagate errors to input parameters
        dr.backward(ob_val)

        # Optimizer: take a gradient step
        opt.step()

        # Optimizer: Update the scene parameters
        params.update(opt)

        err_ref = dr.sum(dr.detach(dr.square(param_ref - params[key])))[0]
        print('Iteration %03i: error=%g' % (it, err_ref))

        if it >= N - W:
            avrg_params += params[key]

    avrg_params /= W

    assert dr.allclose(avrg_params, param_ref, atol=1e-3)


@pytest.mark.parametrize("eval_grad", [False, True])
@pytest.mark.parametrize("N", [1, 10])
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test04_vcall_autodiff_bsdf_single_inst_and_masking(variants_all_ad_rgb, eval_grad, N, jit_flags):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    bsdf = mi.load_dict({
        'type' : 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [1.0, 0.0, 0.0]
        }
    })

    # Enable gradients
    bsdf_params = mi.traverse(bsdf)
    p = bsdf_params['reflectance.value']
    dr.enable_grad(p)
    dr.set_label(p, "albedo_1")
    bsdf_params.update()

    mask = dr.arange(mi.UInt32, N) & 1 == 0
    bsdf_ptr = dr.select(mask, mi.BSDFPtr(bsdf), dr.zeros(mi.BSDFPtr))

    si    = dr.zeros(mi.SurfaceInteraction3f, N)
    si.t  = 0.0
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    theta = 0.5 * (dr.pi / 2)
    wo = [dr.sin(theta), 0, dr.cos(theta)]

    # Evaluate BSDF (using vcalls)
    dr.set_label(si, "si")
    dr.set_label(wo, "wo")
    v_eval = bsdf_ptr.eval(ctx, si, wo)
    dr.set_label(v_eval, "v_eval")

    # Check against reference value
    v = mi.Float(dr.cos(theta) * dr.inv_pi)
    v_ref = dr.select(mask, mi.Color3f(v, 0, 0), mi.Color3f(0))
    assert dr.allclose(v_eval, v_ref)

    loss = dr.sum(v_eval)

    # Backpropagate through vcall
    if eval_grad:
        loss_grad = dr.arange(mi.Float, N)
        dr.eval(loss_grad)
    else:
        loss_grad = mi.Float(1)

    dr.set_grad(loss, loss_grad)
    dr.enqueue(dr.ADMode.Backward, loss)
    dr.traverse(dr.ADMode.Backward, dr.ADFlag.ClearVertices)

    # Check gradients
    grad = dr.grad(bsdf_params['reflectance.value'])

    assert dr.allclose(grad, v * dr.sum(dr.select(mask, loss_grad, 0.0)))

    # Forward propagate gradients to loss
    dr.forward(bsdf_params['reflectance.value'], dr.ADFlag.ClearVertices)
    assert dr.allclose(dr.grad(loss), dr.select(mask, 3 * v, 0.0))


@pytest.mark.parametrize("mode", ['backward', 'forward'])
@pytest.mark.parametrize("eval_grad", [False, True])
@pytest.mark.parametrize("N", [1, 10])
@pytest.mark.parametrize("jit_flags", jit_flags_options)
@pytest.mark.parametrize("indirect", [False, True])
def test05_vcall_autodiff_bsdf(variants_all_ad_rgb, mode, eval_grad, N, jit_flags, indirect):
    # Set drjit JIT flags
    for k, v in jit_flags.items():
        dr.set_flag(k, v)

    bsdf1 = mi.load_dict({
        'type' : 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [1.0, 0.0, 0.0]
        }
    })

    bsdf2 = mi.load_dict({
        'type' : 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [0.0, 0.0, 1.0]
        }
    })

    # Enable gradients
    bsdf1_params = mi.traverse(bsdf1)
    p1 = bsdf1_params['reflectance.value']
    dr.enable_grad(p1)
    dr.set_label(p1, "albedo_1")
    bsdf1_params.update()

    bsdf2_params = mi.traverse(bsdf2)
    if indirect:
        p2 = mi.Color3f(1e-10, 1e-10, 1)
        dr.enable_grad(p2)
        dr.set_label(p2, "albedo_2_indirect")
        p2_2 = dr.minimum(p2, mi.Color3f(100))
        bsdf2_params['reflectance.value'] = p2_2
    else:
        p2 = bsdf2_params['reflectance.value']
        dr.enable_grad(p2)
        dr.set_label(p2, "albedo_2")
    bsdf2_params.update()

    mask = (dr.arange(mi.UInt32, N) & 1) == 0
    bsdf_ptr = dr.select(mask, mi.BSDFPtr(bsdf1), mi.BSDFPtr(bsdf2))

    si    = dr.zeros(mi.SurfaceInteraction3f, N)
    si.t  = 0.0
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    theta = 0.5 * (dr.pi / 2)
    wo = [dr.sin(theta), 0, dr.cos(theta)]

    # Evaluate BSDF (using vcalls)
    dr.set_label(si, "si")
    dr.set_label(wo, "wo")
    v_eval = bsdf_ptr.eval(ctx, si, wo)
    dr.set_label(v_eval, "v_eval")

    # Check against reference value
    v = dr.cos(theta) * dr.inv_pi
    v_ref = dr.select(mask, mi.Color3f(v, 0, 0), mi.Color3f(0, 0, v))
    assert dr.allclose(v_eval, v_ref)

    # Make their gradients a bit different
    mult1 = mi.Float(0.5)
    mult2 = mi.Float(4.0)
    v_eval *= dr.select(mask, mult1, mult2)

    loss = dr.sum(v_eval)
    dr.set_label(loss, "loss")

    if mode == "backward":
        if eval_grad:
            loss_grad = dr.arange(mi.Float, N)
            dr.eval(loss_grad)
        else:
            loss_grad = mi.Float(1)

        dr.set_grad(loss, loss_grad)
        dr.enqueue(dr.ADMode.Backward, loss)
        dr.traverse(dr.ADMode.Backward, dr.ADFlag.ClearVertices)

        # Check gradients
        grad1 = dr.grad(p1)
        grad2 = dr.grad(p2)

        assert dr.allclose(grad1, v * mult1 * dr.sum(dr.select(mask,  loss_grad, 0.0)))
        assert dr.allclose(grad2, v * mult2 * dr.sum(dr.select(~mask, loss_grad, 0.0)))

    if mode == "forward":
        # Forward propagate gradients to loss, one BSDF at a time
        dr.set_grad(p1, 1)
        dr.set_grad(p2, 0)
        dr.enqueue(dr.ADMode.Forward, p1)
        dr.enqueue(dr.ADMode.Forward, p2)
        dr.traverse(dr.ADMode.Forward, dr.ADFlag.ClearVertices)

        dr.set_grad(p1, 0)
        dr.set_grad(p2, 1)
        dr.enqueue(dr.ADMode.Forward, p1)
        dr.enqueue(dr.ADMode.Forward, p2)
        dr.traverse(dr.ADMode.Forward, dr.ADFlag.ClearVertices)

        assert dr.allclose(dr.grad(loss), 3 * v * dr.select(mask, mult1, mult2))


def test06_optimizer_state(variants_all_ad_rgb):
    def ensure_iterable(x):
        if not isinstance(x, (tuple, list)):
            return (x,)
        return x

    key = 'some_param'
    init = mi.Float([1.0, 2.0, 3.0])

    for cls in [(lambda: mi.ad.SGD(lr=2e-2, momentum=0.1)),
                (lambda: mi.ad.Adam(lr=2e-2))]:
        opt = cls()
        assert key not in opt.variables
        assert key not in opt.state
        opt[key] = mi.Float(init)
        assert key in opt.variables

        for _ in range(3):
            dr.set_grad(opt[key], mi.Float([-1, 1, 2]))
            opt.step()

        assert key in opt.state
        state_before = ensure_iterable(opt.state[key])
        for s in state_before:
            assert dr.all(s != 0)

        # A value change should not affect the state
        opt[key] = dr.clip(opt[key], 0, 2)
        state_after = ensure_iterable(opt.state[key])
        for a, b in zip(state_before, state_after):
            assert dr.allclose(a, b)

        # A size change should reset the state
        opt[key] = mi.Float([1.0, 2.0])
        state_after = ensure_iterable(opt.state[key])
        for s in state_after:
            assert dr.all(s == 0)


@pytest.mark.parametrize('opt', ['SGD', 'Adam'])
def test07_masked_updates(variants_all_ad_rgb, opt):
    def ensure_iterable(v):
        if isinstance(v, (tuple, list)):
            return v
        else:
            return [v]

    n = 5
    x = dr.full(mi.Float, 1.0, n)
    params = { 'x': x }

    if opt == 'SGD':
        opt = mi.ad.SGD(lr=1.0, momentum=0.8, params=params, mask_updates=True)
    else:
        assert opt == 'Adam'
        opt = mi.ad.Adam(lr=1.0, params=params, mask_updates=True)

    # Build momentum for a few iterations
    g1 = dr.full(mi.Float, -1.0, n)
    for _ in range(5):
        dr.set_grad(params['x'], g1)
        opt.step()
        params.update(opt)
    assert dr.all(params['x'] > 2.4)

    # Masked updates: parameters and state should only
    # be updated where gradients are nonzero.
    prev_x = mi.Float(params['x'])
    prev_state = [mi.Float(vv) for vv in ensure_iterable(opt.state['x'])]
    for zero_i in range(n):
        is_zero = dr.arange(mi.UInt32, n) == zero_i
        g2 = dr.select(is_zero, 0, mi.Float(g1))

        dr.set_grad(params['x'], g2)
        opt.step()
        params.update(opt)

        assert dr.all((params['x'] == prev_x) | ~is_zero), 'Param should not be updated where grad == 0'
        assert dr.all((params['x'] != prev_x) | is_zero), 'Param should be updated where grad != 0'
        for v1, v2 in zip(ensure_iterable(opt.state['x']), prev_state):
            assert dr.all((v1 == v2) | ~is_zero), 'State should not be updated where grad == 0'
            assert dr.all((v1 != v2) | is_zero), 'State should be updated where grad != 0'

        prev_x = mi.Float(params['x'])
        prev_state = [mi.Float(vv) for vv in ensure_iterable(opt.state['x'])]
