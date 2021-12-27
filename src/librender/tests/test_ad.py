import mitsuba
import pytest
import enoki as ek


def make_simple_scene(res=1, integrator="path"):
    from mitsuba.core import load_dict, ScalarTransform4f

    return load_dict({
        'type' : 'scene',
        "integrator" : { "type" : integrator },
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


if hasattr(ek, 'JitFlag'):
    jit_flags_options = [
        {ek.JitFlag.VCallRecord : 0, ek.JitFlag.VCallOptimize : 0, ek.JitFlag.LoopRecord : 0},
        {ek.JitFlag.VCallRecord : 1, ek.JitFlag.VCallOptimize : 0, ek.JitFlag.LoopRecord : 0},
        {ek.JitFlag.VCallRecord : 1, ek.JitFlag.VCallOptimize : 1, ek.JitFlag.LoopRecord : 0},
    ]
else:
    jit_flags_options = []


@pytest.mark.parametrize("jit_flags", jit_flags_options)
@pytest.mark.parametrize("spp", [1, 4, 44])
def test01_bsdf_reflectance_backward(variants_all_ad_rgb, jit_flags, spp):
    # Set enoki JIT flags
    for k, v in jit_flags.items():
        ek.set_flag(k, v)

    # Test correctness of the gradients taking one step of gradient descent on linear function
    from mitsuba.python.util import traverse

    scene = make_simple_scene()

    # Parameter to differentiate
    key = 'rect.bsdf.reflectance.value'

    # Enable gradients
    params = traverse(scene)
    ek.enable_grad(params[key])

    # Forward rendering - first time
    img_1 = scene.integrator().render(scene, seed=0, spp=spp)

    # Backward pass and gradient descent step
    loss = ek.hsum_async(img_1)
    ek.backward(loss)

    grad = ek.grad(params[key])

    lr = 0.01
    params[key][0] += lr
    params.update()

    # Forward rendering - second time
    img_2 = scene.integrator().render(scene, seed=0, spp=spp)

    new_loss = ek.hsum_async(img_2)

    assert ek.allclose(loss, new_loss - lr * grad[0])


@pytest.mark.parametrize("jit_flags", jit_flags_options)
@pytest.mark.parametrize("spp", [1, 4])
def test02_bsdf_reflectance_forward(variants_all_ad_rgb, jit_flags, spp):
# def test02_bsdf_reflectance_forward(variant_cuda_ad_rgb, jit_flags, spp):
    # Set enoki JIT flags
    for k, v in jit_flags.items():
        ek.set_flag(k, v)

    # Test correctness of the gradients taking one step of gradient descent on linear function
    from mitsuba.core import Float
    from mitsuba.python.util import traverse

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
    img_1 = scene.integrator().render(scene, seed=0, spp=spp)

    # Compute forward gradients
    assert ek.grad(img_1) == 0.0
    ek.forward(X)
    grad = ek.grad(img_1)

    # Modify reflectance value and re-render
    lr = 0.1
    params[key] += lr
    params.update()

    img_2 = scene.integrator().render(scene, seed=0, spp=spp)

    assert ek.allclose(img_1, img_2 - lr * grad)


@pytest.mark.parametrize("spp", [8])
@pytest.mark.parametrize("res", [3])
@pytest.mark.parametrize("opt_conf", [('SGD', [250.0, 0.8])])
def test03_optimizer(variants_all_ad_rgb, spp, res, opt_conf):
    from mitsuba.core import Float, Color3f
    from mitsuba.python.util import traverse

    scene = make_simple_scene(res=res, integrator="direct")

    key = 'rect.bsdf.reflectance.value'

    params = traverse(scene)
    param_ref = Color3f(params[key])

    image_ref = scene.integrator().render(scene, seed=0, spp=spp)

    opt_type, opt_args = opt_conf
    opt = getattr(mitsuba.python.ad, opt_type)(*opt_args, params=params)

    opt[key] = Color3f(0.1)
    ek.set_label(opt[key], key)
    opt.update()

    avrg_params = Color3f(0)

    # Total iteration count
    N = 100
    # Average window size
    W = 10

    for it in range(N):
        # Perform a differentiable rendering of the scene
        image = scene.integrator().render(scene, seed=0, spp=spp)
        ek.set_label(image, 'image')

        # Objective: MSE between 'image' and 'image_ref'
        ob_val = ek.hsum_async(ek.sqr(image - image_ref)) / len(image)
        ek.set_label(ob_val, 'ob_val')

        # print(ek.graphviz_str(Float(1)))

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


@pytest.mark.parametrize("eval_grad", [False, True])
@pytest.mark.parametrize("N", [1, 10])
@pytest.mark.parametrize("jit_flags", jit_flags_options)
def test04_vcall_autodiff_bsdf_single_inst_and_masking(variants_all_ad_rgb, eval_grad, N, jit_flags):
    # Set enoki JIT flags
    for k, v in jit_flags.items():
        ek.set_flag(k, v)

    from mitsuba.core import load_dict, Float, UInt32, Color3f, Frame3f
    from mitsuba.render import SurfaceInteraction3f, BSDFPtr, BSDFContext
    from mitsuba.python.util import traverse

    bsdf = load_dict({
        'type' : 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [1.0, 0.0, 0.0]
        }
    })

    # Enable gradients
    bsdf_params = traverse(bsdf)
    p = bsdf_params['reflectance.value']
    ek.enable_grad(p)
    ek.set_label(p, "albedo_1")
    bsdf_params.update()

    mask = ek.eq(ek.arange(UInt32, N) & 1, 0)
    bsdf_ptr = ek.select(mask, BSDFPtr(bsdf), ek.zero(BSDFPtr))

    si    = ek.zero(SurfaceInteraction3f, N)
    si.t  = 0.0
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()

    theta = 0.5 * (ek.Pi / 2)
    wo = [ek.sin(theta), 0, ek.cos(theta)]

    # Evaluate BSDF (using vcalls)
    ek.set_label(si, "si")
    ek.set_label(wo, "wo")
    v_eval = bsdf_ptr.eval(ctx, si, wo)
    ek.set_label(v_eval, "v_eval")

    # Check against reference value
    v = Float(ek.cos(theta) * ek.InvPi)
    v_ref = ek.select(mask, Color3f(v, 0, 0), Color3f(0))
    assert ek.allclose(v_eval, v_ref)

    loss = ek.hsum(v_eval)

    # Backpropagate through vcall
    if eval_grad:
        loss_grad = ek.arange(Float, N)
        ek.eval(loss_grad)
    else:
        loss_grad = Float(1)

    ek.set_grad(loss, loss_grad)
    ek.enqueue(ek.ADMode.Backward, loss)
    ek.traverse(Float, ek.ADMode.Backward, ek.ADFlag.ClearVertices)

    # Check gradients
    grad = ek.grad(bsdf_params['reflectance.value'])

    assert ek.allclose(grad, v * ek.hsum(ek.select(mask, loss_grad, 0.0)))

    # Forward propagate gradients to loss
    ek.forward(bsdf_params['reflectance.value'], ek.ADFlag.ClearVertices)
    assert ek.allclose(ek.grad(loss), ek.select(mask, 3 * v, 0.0))


@pytest.mark.parametrize("mode", ['backward', 'forward'])
@pytest.mark.parametrize("eval_grad", [False, True])
@pytest.mark.parametrize("N", [1, 10])
@pytest.mark.parametrize("jit_flags", jit_flags_options)
@pytest.mark.parametrize("indirect", [False, True])
def test05_vcall_autodiff_bsdf(variants_all_ad_rgb, mode, eval_grad, N, jit_flags, indirect):
    # Set enoki JIT flags
    for k, v in jit_flags.items():
        ek.set_flag(k, v)

    from mitsuba.core import load_dict, Float, UInt32, Color3f, Frame3f
    from mitsuba.render import SurfaceInteraction3f, BSDFPtr, BSDFContext
    from mitsuba.python.util import traverse

    bsdf1 = load_dict({
        'type' : 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [1.0, 0.0, 0.0]
        }
    })

    bsdf2 = load_dict({
        'type' : 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [0.0, 0.0, 1.0]
        }
    })

    # Enable gradients
    bsdf1_params = traverse(bsdf1)
    p1 = bsdf1_params['reflectance.value']
    ek.enable_grad(p1)
    ek.set_label(p1, "albedo_1")
    bsdf1_params.update()

    bsdf2_params = traverse(bsdf2)
    if indirect:
        p2 = Color3f(1e-10, 1e-10, 1)
        ek.enable_grad(p2)
        ek.set_label(p2, "albedo_2_indirect")
        p2_2 = ek.min(p2, Color3f(100))
        bsdf2_params['reflectance.value'] = p2_2
    else:
        p2 = bsdf2_params['reflectance.value']
        ek.enable_grad(p2)
        ek.set_label(p2, "albedo_2")
    bsdf2_params.update()

    mask = ek.eq(ek.arange(UInt32, N) & 1, 0)
    bsdf_ptr = ek.select(mask, BSDFPtr(bsdf1), BSDFPtr(bsdf2))

    si    = ek.zero(SurfaceInteraction3f, N)
    si.t  = 0.0
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = Frame3f(si.n)

    ctx = BSDFContext()

    theta = 0.5 * (ek.Pi / 2)
    wo = [ek.sin(theta), 0, ek.cos(theta)]

    # Evaluate BSDF (using vcalls)
    ek.set_label(si, "si")
    ek.set_label(wo, "wo")
    v_eval = bsdf_ptr.eval(ctx, si, wo)
    ek.set_label(v_eval, "v_eval")

    # Check against reference value
    v = ek.cos(theta) * ek.InvPi
    v_ref = ek.select(mask, Color3f(v, 0, 0), Color3f(0, 0, v))
    assert ek.allclose(v_eval, v_ref)

    # Make their gradients a bit different
    mult1 = Float(0.5)
    mult2 = Float(4.0)
    v_eval *= ek.select(mask, mult1, mult2)

    loss = ek.hsum(v_eval)
    ek.set_label(loss, "loss")

    if mode == "backward":
        if eval_grad:
            loss_grad = ek.arange(Float, N)
            ek.eval(loss_grad)
        else:
            loss_grad = Float(1)

        ek.set_grad(loss, loss_grad)
        ek.enqueue(ek.ADMode.Backward, loss)
        ek.traverse(Float, ek.ADMode.Backward, ek.ADFlag.ClearVertices)

        # Check gradients
        grad1 = ek.grad(p1)
        grad2 = ek.grad(p2)

        assert ek.allclose(grad1, v * mult1 * ek.hsum(ek.select(mask,  loss_grad, 0.0)))
        assert ek.allclose(grad2, v * mult2 * ek.hsum(ek.select(~mask, loss_grad, 0.0)))

    if mode == "forward":
        # Forward propagate gradients to loss, one BSDF at a time
        ek.set_grad(p1, 1)
        ek.set_grad(p2, 0)
        ek.enqueue(ek.ADMode.Forward, p1)
        ek.enqueue(ek.ADMode.Forward, p2)
        ek.traverse(Float, ek.ADMode.Forward, ek.ADFlag.ClearVertices)

        ek.set_grad(p1, 0)
        ek.set_grad(p2, 1)
        ek.enqueue(ek.ADMode.Forward, p1)
        ek.enqueue(ek.ADMode.Forward, p2)
        ek.traverse(Float, ek.ADMode.Forward, ek.ADFlag.ClearVertices)

        assert ek.allclose(ek.grad(loss), 3 * v * ek.select(mask, mult1, mult2))


def test06_optimizer_state(variants_all_ad_rgb):
    from mitsuba.core import Float
    from mitsuba.python.ad import SGD, Adam

    def ensure_iterable(x):
        if not isinstance(x, (tuple, list)):
            return (x,)
        return x

    key = 'some_param'
    init = Float([1.0, 2.0, 3.0])

    for cls in [(lambda: SGD(lr=2e-2, momentum=0.1)),
                (lambda: Adam(lr=2e-2))]:
        opt = cls()
        assert key not in opt.variables
        assert key not in opt.state
        opt[key] = Float(init)
        assert key in opt.variables

        for _ in range(3):
            ek.set_grad(opt[key], Float([-1, 1, 2]))
            opt.step()

        assert key in opt.state
        state_before = ensure_iterable(opt.state[key])
        for s in state_before:
            assert ek.all(ek.neq(s, 0))

        # A value change should not affect the state
        opt[key] = ek.clamp(opt[key], 0, 2)
        state_after = ensure_iterable(opt.state[key])
        for a, b in zip(state_before, state_after):
            assert ek.allclose(a, b)

        # A size change should reset the state
        opt[key] = Float([1.0, 2.0])
        state_after = ensure_iterable(opt.state[key])
        for s in state_after:
            assert ek.all(ek.eq(s, 0))


@pytest.mark.parametrize('opt', ['SGD', 'Adam'])
def test07_masked_updates(variants_all_ad_rgb, opt):
    from mitsuba.core import Float, UInt32
    from mitsuba.python.ad.optimizers import Adam, SGD

    def ensure_iterable(v):
        if isinstance(v, (tuple, list)):
            return v
        else:
            return [v]

    n = 5
    x = ek.full(Float, 1.0, n)
    params = {'x': x}

    if opt == 'SGD':
        opt = SGD(lr=1.0, momentum=0.8, params=params, mask_updates=True)
    else:
        assert opt == 'Adam'
        opt = Adam(lr=1.0, params=params, mask_updates=True)
    opt.load()

    # Build momentum for a few iterations
    g1 = ek.full(Float, -1.0, n)
    for _ in range(5):
        ek.set_grad(params['x'], g1)
        opt.step()
        opt.update()
    assert ek.all(params['x'] > 2.4)

    # Masked updates: parameters and state should only
    # be updated where gradients are nonzero.
    prev_x = Float(params['x'])
    prev_state = [Float(vv) for vv in ensure_iterable(opt.state['x'])]
    for zero_i in range(n):
        is_zero = ek.eq(ek.arange(UInt32, n), zero_i)
        g2 = ek.select(is_zero, 0, Float(g1))

        ek.set_grad(params['x'], g2)
        opt.step()
        opt.update()

        assert ek.all(ek.eq(params['x'], prev_x) | ~is_zero), 'Param should not be updated where grad == 0'
        assert ek.all(ek.neq(params['x'], prev_x) | is_zero), 'Param should be updated where grad != 0'
        for v1, v2 in zip(ensure_iterable(opt.state['x']), prev_state):
            assert ek.all(ek.eq(v1, v2) | ~is_zero), 'State should not be updated where grad == 0'
            assert ek.all(ek.neq(v1, v2) | is_zero), 'State should be updated where grad != 0'

        prev_x = Float(params['x'])
        prev_state = [Float(vv) for vv in ensure_iterable(opt.state['x'])]
