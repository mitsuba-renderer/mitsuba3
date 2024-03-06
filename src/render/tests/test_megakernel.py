import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import find_resource

def write_kernels(*args, output_dir='kernels', scene_fname=None):
    import os
    import json
    assert mi.variant() == 'cuda_rgb'

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
                f.write(f'// Variant: {mi.variant()}\n')
                f.write('// Hash: {}\n'.format(hex(safe_entries['hash'])[2:]))
                f.write(f'// Kernel: {meta}\n\n\n')
                f.write(ptx)


integrator_name = ['path']
if hasattr(dr, 'cuda.ad'):
    integrator_name.append('prb')

@pytest.mark.parametrize('integrator_name', integrator_name)
def test01_kernel_launches_path(variants_vec_rgb, integrator_name):
    """
    Tests that forward rendering launches the correct number of kernels
    """
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'))
    film_size = scene.sensors()[0].film().crop_size()
    spp = 2

    integrator = mi.load_dict({
        'type': integrator_name,
        'max_depth': 3
    })

    with dr.scoped_set_flag(dr.JitFlag.KernelHistory, True):
        # Perform 3 rendering in a row
        integrator.render(scene, seed=0, spp=spp)
        history_1 = dr.kernel_history([dr.KernelType.JIT])
        integrator.render(scene, seed=0, spp=spp)
        history_2 = dr.kernel_history([dr.KernelType.JIT])
        integrator.render(scene, seed=0, spp=spp)
        history_3 = dr.kernel_history([dr.KernelType.JIT])

        # Should only be 2 kernels (rendering, film develop)
        assert len(history_1) == 2
        assert len(history_2) == 2
        assert len(history_3) == 2

        # print(history_1[0]['ir'].read())
        # print(history_2[0]['ir'].read())

        # Only the rendering kernel should use OptiX
        if mi.variant().startswith('cuda'):
            assert [e['uses_optix'] for e in history_1] == [True, False]
            assert [e['uses_optix'] for e in history_2] == [True, False]
            assert [e['uses_optix'] for e in history_3] == [True, False]

        # Check rendering wavefront size
        render_wavefront_size = dr.prod(film_size) * spp
        assert history_1[0]['size'] == render_wavefront_size
        assert history_2[0]['size'] == render_wavefront_size
        assert history_3[0]['size'] == render_wavefront_size

        # Check film development wavefront size
        film_wavefront_size = dr.prod(film_size) * 3 #(RGB)
        assert history_1[1]['size'] == film_wavefront_size
        assert history_2[1]['size'] == film_wavefront_size
        assert history_3[1]['size'] == film_wavefront_size

        # TODO First run produces different kernels for some reason
        # Kernels should all be identical (reused from the cached)
        for i in range(len(history_1)):
            # assert history_1[i]['hash'] == history_2[i]['hash'] # TODO
            assert history_3[i]['hash'] == history_2[i]['hash']


@pytest.mark.parametrize('scene_fname', [
    'resources/data/scenes/cbox/cbox.xml',
    'resources/data/tests/scenes/various_emitters/test_various_emitters_ptracer.xml'])
def test02_kernel_launches_ptracer(variants_vec_rgb, scene_fname):
    """
    Tests that forward rendering launches the correct number of kernels
    for the particle tracer integrator
    """
    scene = mi.load_file(find_resource(scene_fname))
    film_size = scene.sensors()[0].film().crop_size()
    spp = 2

    integrator = mi.load_dict({
        'type': 'ptracer',
        'max_depth': 5
    })

    # Area emitters using bitmaps lazily compute scalar 2D distribution used
    # for importance sampling. That can trigger a sync_thread during sampling
    # because associated variables may not have been evaluated yet
    # Hence, first render may invoke a higher number of kernels
    integrator.render(scene, seed=0, spp=spp)

    with dr.scoped_set_flag(dr.JitFlag.KernelHistory, True):
        # Perform 3 rendering in a row
        integrator.render(scene, seed=0, spp=spp)
        history_1 = dr.kernel_history([dr.KernelType.JIT])
        integrator.render(scene, seed=0, spp=spp)
        history_2 = dr.kernel_history([dr.KernelType.JIT])
        integrator.render(scene, seed=0, spp=spp)
        history_3 = dr.kernel_history([dr.KernelType.JIT])

        # Role of each kernel
        # 0. Render visible emitters, trace and connect paths from the light source to the sensor
        # 1. Developing the film
        assert len(history_1) == 2
        assert len(history_2) == 2
        assert len(history_3) == 2

        # TODO First run produces different kernels for some reason
        # Kernels should all be identical (reused from the cached)
        for i in range(len(history_1)):
            # assert history_1[i]['hash'] == history_2[i]['hash'] # TODO
            assert history_3[i]['hash'] == history_2[i]['hash']

        # Only the rendering kernels should use optix
        if mi.variant().startswith('cuda'):
            assert [e['uses_optix'] for e in history_1] == [True, False]
            assert [e['uses_optix'] for e in history_2] == [True, False]
            assert [e['uses_optix'] for e in history_3] == [True, False]

        # Check rendering wavefront size
        render_wavefront_size = dr.prod(film_size) * spp
        assert history_1[0]['size'] == render_wavefront_size
        assert history_2[0]['size'] == render_wavefront_size
        assert history_3[0]['size'] == render_wavefront_size

        # Check film development wavefront size
        film_wavefront_size = dr.prod(film_size) * 3 #(RGB)
        assert history_1[1]['size'] == film_wavefront_size
        assert history_2[1]['size'] == film_wavefront_size
        assert history_3[1]['size'] == film_wavefront_size


@pytest.mark.skip('TODO')
def test03_kernel_launches_optimization(variants_all_ad_rgb):
    """
    Check the history of kernel launches during a simple optimization loop
    using render_adjoint.
    """
    scene = mi.load_file(find_resource('resources/data/scenes/cbox/cbox.xml'))
    film_size = scene.sensors()[0].film().crop_size()
    spp = 4

    film_wavefront_size = dr.prod(film_size) * 3 #(RGB)
    wavefront_size = dr.prod(film_size) * spp

    integrator = mi.load_dict({
        'type': 'prb',
        'max_depth': 3
    })

    with dr.scoped_set_flag(dr.JitFlag.KernelHistory, True):
        image_ref = integrator.render(scene, seed=0, spp=spp)
        dr.kernel_history_clear()

        # dr.set_log_level(3)

        params = mi.traverse(scene)
        key = 'red.reflectance.value'
        params.keep([key])
        params[key] = mi.Color3f(0.1, 0.1, 0.1)
        params.update()

        # Updating the scene here shouldn't produce any kernel launch
        assert len(dr.kernel_history([dr.KernelType.JIT])) == 0

        opt = mi.ad.SGD(lr=0.05, params=params)
        params.update(opt)

        # Creating the optimizer shouldn't produce any kernel launch
        assert len(dr.kernel_history([dr.KernelType.JIT])) == 0

        # Use a different spp for the optimization loop (produce different kernels)
        spp = 2
        wavefront_size = dr.prod(film_size) * spp

        histories = []
        for it in range(3):
            # print(f"\nITERATION {it}\n")

            # print(f"\n----- PRIMAL\n")

            # Primal rendering of the scene
            with dr.suspend_grad():
                image = integrator.render(scene, seed=0, spp=spp)

            history_primal = dr.kernel_history([dr.KernelType.JIT])
            assert len(history_primal) == 2 # (render, develop film)
            assert history_primal[0]['size'] == wavefront_size
            assert history_primal[1]['size'] == film_wavefront_size

            # print(f"\n----- LOSS\n")

            # Compute adjoint image
            dr.enable_grad(image)
            ob_val = dr.sum(dr.sqr(image - image_ref)) / len(image)
            dr.backward(ob_val)
            image_adj = dr.grad(image)
            dr.set_grad(image, 0.0)

            history_loss = dr.kernel_history([dr.KernelType.JIT])
            assert len(history_loss) == 1 # (horizontal reduction with hsum)
            assert history_loss[0]['size'] == film_wavefront_size

            # print(f"\n----- ADJOINT\n")

            # Adjoint rendering of the scene
            integrator.render_backward(scene, params, image_adj, seed=0, spp=spp)

            history_adjoint = dr.kernel_history([dr.KernelType.JIT])
            assert len(history_adjoint) == 1 # (gather rays weights in image_adj)
            assert history_adjoint[0]['size'] == film_wavefront_size

            # print(f"\n----- STEP\n")

            # Optimizer: take a gradient step
            opt.step()
            history_step = dr.kernel_history([dr.KernelType.JIT])
            assert len(history_step) == 2 # (adjoint render, eval optimizer state)
            assert history_step[0]['size'] == wavefront_size
            assert history_step[1]['size'] == 1

            # print(f"\n----- UPDATE\n")

            # Optimizer: Update the scene parameters
            opt.update()
            history_update = dr.kernel_history([dr.KernelType.JIT])
            assert len(history_update) == 0

            histories.append(history_primal + history_loss + history_adjoint + history_step)

        # Ensures all evaluation have finished
        dr.sync_thread()

        # Make sure that kernels are reused after the 2nd iteration
        for k in range(len(histories[1])):
            assert histories[1][k]['hash'] == histories[2][k]['hash']
