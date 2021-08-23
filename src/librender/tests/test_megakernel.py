import enoki as ek
import mitsuba
import pytest

def write_kernels(*args, output_dir='kernels', scene_fname=None):
    import os
    import json
    assert mitsuba.variant() == 'cuda_rgb'

    os.makedirs(output_dir, exist_ok=True)
    for i, h in enumerate(args):
        for j, entry in enumerate(h):
            fname = os.path.join(output_dir, f'run_{i}_kernel_{j}.ptx')
            ptx = entry['ir'].getvalue()

            safe_entries = {k: v for k, v in entry.items() if k not in ('backend', 'ir')}

            with open(fname, 'w') as f:
                meta = '\n// '.join(json.dumps(safe_entries, indent=2).split('\n'))
                f.write(f'// Run {i}, kernel {j} ({len(h)} kernels total)\n')
                if scene_fname:
                    f.write(f'// Scene: {scene_fname}\n')
                f.write(f'// Variant: {mitsuba.variant()}\n')
                f.write('// Hash: {}\n'.format(hex(safe_entries['hash'])[2:]))
                f.write(f'// Kernel: {meta}\n\n\n')
                f.write(ptx)


@pytest.mark.parametrize('integrator_name', ['rb', 'path'])
def test01_kernel_launches_path(variants_vec_rgb, gc_collect, integrator_name):
    """
    Tests that forward rendering launches the correct number of kernels
    """
    from mitsuba.core import xml

    scene = xml.load_file('../resources/data/scenes/cbox/cbox.xml')
    film_size = scene.sensors()[0].film().crop_size()
    spp = 2

    integrator = xml.load_dict({
        'type': integrator_name,
        'max_depth': 3
    })

    ek.set_flag(ek.JitFlag.KernelHistory, True)

    # Perform 3 rendering in a row
    integrator.render(scene, seed=0, spp=spp)
    history_1 = ek.kernel_history()
    integrator.render(scene, seed=0, spp=spp)
    history_2 = ek.kernel_history()
    integrator.render(scene, seed=0, spp=spp)
    history_3 = ek.kernel_history()

    # Should only be 3 kernels (sampler seeding, rendering, film develop)
    assert len(history_1) == 3
    assert len(history_2) == 3
    assert len(history_3) == 3

    # Only the rendering kernel should use optix
    if mitsuba.variant().startswith('cuda'):
        assert [e['uses_optix'] for e in history_1] == [False, True, False]
        assert [e['uses_optix'] for e in history_2] == [False, True, False]
        assert [e['uses_optix'] for e in history_3] == [False, True, False]

    # Check rendering wavefront size
    render_wavefront_size = ek.hprod(film_size) * spp
    assert history_1[1]['size'] == render_wavefront_size
    assert history_2[1]['size'] == render_wavefront_size
    assert history_3[1]['size'] == render_wavefront_size

    # Check film development wavefront size
    film_wavefront_size = ek.hprod(film_size) * 3 #(RGB)
    assert history_1[2]['size'] == film_wavefront_size
    assert history_2[2]['size'] == film_wavefront_size
    assert history_3[2]['size'] == film_wavefront_size

    # Kernels should all be identical (reused from the cached)
    for i in range(len(history_1)):
        assert history_2[i]['hash'] == history_1[i]['hash']
        assert history_3[i]['hash'] == history_1[i]['hash']


@pytest.mark.parametrize('scene_fname', [
    '../resources/data/scenes/cbox/cbox.xml',
    '../resources/data/tests/scenes/various_emitters/test_various_emitters.xml'])
def test02_kernel_launches_ptracer(variants_vec_rgb, gc_collect, scene_fname):
    """
    Tests that forward rendering launches the correct number of kernels
    for the particle tracer integrator
    """
    from mitsuba.core import xml

    scene = xml.load_file(scene_fname)
    film_size = scene.sensors()[0].film().crop_size()
    spp = 2

    integrator = xml.load_dict({
        'type': 'ptracer',
        'max_depth': 5
    })

    ek.set_flag(ek.JitFlag.KernelHistory, True)
    # Perform 3 rendering in a row
    integrator.render(scene, seed=0, spp=spp)
    history_1 = ek.kernel_history()
    integrator.render(scene, seed=0, spp=spp)
    history_2 = ek.kernel_history()
    integrator.render(scene, seed=0, spp=spp)
    history_3 = ek.kernel_history()

    # Role of each kernel
    # 0. Seeding the sampler
    # 1. Render visible emitters, trace and connect paths from the light source to the sensor
    # 2. Normalization (overwrite weight channel)
    # 3. Developing the film
    assert len(history_1) == 4
    assert len(history_2) == 4
    assert len(history_3) == 4

    # Kernels should all be identical (reused from the cached)
    for i in range(len(history_1)):
        assert history_2[i]['hash'] == history_1[i]['hash']
        assert history_3[i]['hash'] == history_1[i]['hash']

    # Only the rendering kernels should use optix
    if mitsuba.variant().startswith('cuda'):
        assert [e['uses_optix'] for e in history_1] == [False, True, False, False]
        assert [e['uses_optix'] for e in history_2] == [False, True, False, False]
        assert [e['uses_optix'] for e in history_3] == [False, True, False, False]

    # Check seeding & rendering wavefront size
    render_wavefront_size = ek.hprod(film_size) * spp
    for i in range(2):
        assert history_1[i]['size'] == render_wavefront_size
        assert history_2[i]['size'] == render_wavefront_size
        assert history_3[i]['size'] == render_wavefront_size

    # Check film renormalization wavefront size
    normalization_wavefront_size = ek.hprod(film_size) * 1 #(W)
    assert history_1[2]['size'] == normalization_wavefront_size
    assert history_2[2]['size'] == normalization_wavefront_size
    assert history_3[2]['size'] == normalization_wavefront_size

    # Check film development wavefront size
    film_wavefront_size = ek.hprod(film_size) * 3 #(RGB)
    assert history_1[3]['size'] == film_wavefront_size
    assert history_2[3]['size'] == film_wavefront_size
    assert history_3[3]['size'] == film_wavefront_size


def test03_kernel_launches_optimization(variants_all_ad_rgb, gc_collect):
    """
    Check the history of kernel launches during a simple optimization loop
    using render_adjoint.
    """
    from mitsuba.core import xml, Color3f
    from mitsuba.python.util import traverse

    scene = xml.load_file('../resources/data/scenes/cbox/cbox.xml')
    film_size = scene.sensors()[0].film().crop_size()
    spp = 4

    film_wavefront_size = ek.hprod(film_size) * 3 #(RGB)
    wavefront_size = ek.hprod(film_size) * spp

    integrator = xml.load_dict({
        'type': 'rb',
        'max_depth': 3
    })

    ek.set_flag(ek.JitFlag.KernelHistory, True)

    image_ref = integrator.render(scene, seed=0, spp=spp)
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

    # Use a different spp for the optimization loop (produce different kernels)
    spp = 2
    wavefront_size = ek.hprod(film_size) * spp

    histories = []
    for it in range(3):
        # print(f"\nITERATION {it}\n")

        # print(f"\n----- PRIMAL\n")

        # Primal rendering of the scene
        with opt.suspend_gradients():
            image = integrator.render(scene, seed=0, spp=spp)

        history_primal = ek.kernel_history()
        assert len(history_primal) == 3 # (seed, render, develop film)
        assert history_primal[1]['size'] == wavefront_size
        assert history_primal[2]['size'] == film_wavefront_size

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
        integrator.render_backward(scene, opt, image_adj, seed=0, spp=spp)

        history_adjoint = ek.kernel_history()
        assert len(history_adjoint) == 2 # (seed, gather rays weights in image_adj)
        assert history_adjoint[1]['size'] == film_wavefront_size

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

    # Ensures all evaluation have finished
    ek.sync_thread()

    # Make sure that kernels are reused after the 2nd iteration
    for k in range(len(histories[1])):
        assert histories[1][k]['hash'] == histories[2][k]['hash']
