import enoki as ek
import mitsuba
import pytest


@pytest.mark.parametrize('integrator_name', ['rb', 'path'])
def test01_kernel_launches_path(variants_vec_rgb, integrator_name):
    """
    Tests that forward rendering launches the correct number of kernels
    """
    from mitsuba.core import xml

    scene = xml.load_file('../resources/data/scenes/cbox/cbox.xml')
    film_size = scene.sensors()[0].film().crop_size()
    spp = 32

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': 3
    })

    ek.set_flag(ek.JitFlag.KernelHistory, True)

    # Perform 3 rendering in a row
    integrator.render(scene, spp=spp)
    history_1 = ek.kernel_history()
    integrator.render(scene, spp=spp)
    history_2 = ek.kernel_history()
    integrator.render(scene, spp=spp)
    history_3 = ek.kernel_history()

    # 1st run: should only be 3 kernels (sampler seeding, rendering, film develop)
    assert len(history_1) == 3
    # 2nd and 3rd run: no sample seeding kernel as we don't reseed
    assert len(history_2) == 2
    assert len(history_3) == 2

    # Only the rendering kernel should use optix
    if mitsuba.variant().startswith('cuda'):
        assert [e['uses_optix'] for e in history_1] == [False, True, False]
        assert [e['uses_optix'] for e in history_2] == [True, False]
        assert [e['uses_optix'] for e in history_3] == [True, False]

    # Check rendering wavefront size
    render_wavefront_size = ek.hprod(film_size) * spp
    assert history_1[1]['size'] == render_wavefront_size
    assert history_2[0]['size'] == render_wavefront_size
    assert history_3[0]['size'] == render_wavefront_size

    # Check film development wavefront size
    film_wavefront_size = ek.hprod(film_size) * 3 #(RGB)
    assert history_1[2]['size'] == film_wavefront_size
    assert history_2[1]['size'] == film_wavefront_size
    assert history_3[1]['size'] == film_wavefront_size

    # Film development kernel should be identical
    assert history_2[1]['hash'] == history_1[2]['hash']
    assert history_3[1]['hash'] == history_1[2]['hash']

    # 2nd and 3rd run should reuse the cached kernels
    assert history_2[0]['hash'] == history_3[0]['hash']
    assert history_2[1]['hash'] == history_3[1]['hash']


@pytest.mark.parametrize('integrator_name', ['rb', 'path'])
def test02_kernel_launches_path_reseed(variants_vec_rgb, integrator_name):
    """
    Similar to previous test, but this time we re-seed between each rendering.
    Hence we should always be able to fully reuse all the kernels
    """
    from mitsuba.core import xml

    scene = xml.load_file('../resources/data/scenes/cbox/cbox.xml')
    film_size = scene.sensors()[0].film().crop_size()
    spp = 32

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': 3
    })

    ek.set_flag(ek.JitFlag.KernelHistory, True)

    film_wavefront_size = ek.hprod(film_size) * 3 #(RGB)
    wavefront_size = ek.hprod(film_size) * spp

    histories = []

    # Perform 3 rendering in a row
    scene.sensors()[0].sampler().seed(0, wavefront_size)
    integrator.render(scene, spp=spp)
    histories.append(ek.kernel_history())

    scene.sensors()[0].sampler().seed(0, wavefront_size)
    integrator.render(scene, spp=spp)
    histories.append(ek.kernel_history())

    scene.sensors()[0].sampler().seed(0, wavefront_size)
    integrator.render(scene, spp=spp)
    histories.append(ek.kernel_history())

    # Various check on the launched kernels (similar to previous test)
    for history in histories:
        assert len(history) == 3
        if mitsuba.variant().startswith('cuda'):
            assert [e['uses_optix'] for e in history] == [False, True, False]
        assert history[1]['size'] == wavefront_size
        assert history[2]['size'] == film_wavefront_size

    # Make sure we fully re-use all the kernels from the cache
    for i in range(2):
        for k in range(3):
            assert histories[0][k]['hash'] == histories[i][k]['hash']


def test03_kernel_launches_optimization(variants_vec_rgb):
    """
    Check the history of kernel launches during a simple optimization loop
    using render_adjoint.
    """
    from mitsuba.core import xml, Color3f
    from mitsuba.python.util import traverse

    scene = xml.load_file('../resources/data/scenes/cbox/cbox.xml')
    film_size = scene.sensors()[0].film().crop_size()
    spp = 32

    film_wavefront_size = ek.hprod(film_size) * 3 #(RGB)
    wavefront_size = ek.hprod(film_size) * spp

    integrator = xml.load_dict({
        'type': 'rb',
        'max_depth': 3
    })

    ek.set_flag(ek.JitFlag.KernelHistory, True)

    image_ref = integrator.render(scene, spp=spp)
    ek.kernel_history_clear()

    # ek.set_log_level(3)

    params = traverse(scene)
    key = 'red.reflectance.value'
    params.keep([key])
    params[key] = Color3f(0.1, 0.1, 0.1)
    params.update()

    # Updating the scene here shouldn't produce any kernel launch
    assert len(ek.kernel_history()) == 0

    opt = mitsuba.python.ad.SGD(lr=0.05, params=params)
    opt.load(key)
    opt.update()

    # Creating the optimizer shouldn't produce any kernel launch
    assert len(ek.kernel_history()) == 0

    spp = 16
    wavefront_size = ek.hprod(film_size) * spp

    histories = []
    for it in range(3):
        # print(f"\nITERATION {it}\n")

        # print(f"\n----- PRIMAL\n")

        # Primal rendering of the scene
        with opt.suspend_gradients():
            image = integrator.render(scene, spp=spp)

        history_primal = ek.kernel_history()
        if it == 0:
            assert len(history_primal) == 3 # (seed, render, develop film)
        else:
            assert len(history_primal) == 2 # (render, develop film)
        assert history_primal[-2]['size'] == wavefront_size
        assert history_primal[-1]['size'] == film_wavefront_size

        # print(f"\n----- LOSS\n")

        # Compute adjoint image
        ek.enable_grad(image)
        ob_val = ek.hsum_async(ek.sqr(image - image_ref)) / len(image)
        ek.backward(ob_val)
        image_adj = ek.grad(image)
        ek.set_grad(image, 0.0)

        history_loss = ek.kernel_history()
        assert len(history_loss) == 1 # (horizontal reduction with hsum)
        assert history_loss[0]['size'] == film_wavefront_size

        # print(f"\n----- ADJOINT\n")

        # Adjoint rendering of the scene
        integrator.render_backward(scene, opt, image_adj, spp=spp)

        history_adjoint = ek.kernel_history()
        assert len(history_adjoint) == 1 # (gather rays weights in image_adj)
        assert history_adjoint[0]['size'] == film_wavefront_size

        # print(f"\n----- STEP\n")

        # Optimizer: take a gradient step
        opt.step()
        history_step = ek.kernel_history()
        assert len(history_step) == 2 # (adjoint render, eval optimizer state)
        assert history_step[0]['size'] == wavefront_size
        assert history_step[1]['size'] == 1

        # print(f"\n----- UPDATE\n")

        # Optimizer: Update the scene parameters
        opt.update()
        history_update = ek.kernel_history()
        assert len(history_update) == 0

        histories.append(history_primal + history_loss + history_adjoint + history_step)

    # Make sure that kernels are reused after the 2nd iteration
    for k in range(len(histories[1])):
        assert histories[1][k]['hash'] == histories[2][k]['hash']
