import enoki as ek
import mitsuba
import pytest


@pytest.mark.parametrize('integrator_name', ['rb', 'path'])
def test01_kernel_launches_path(variants_vec_rgb, integrator_name):
    """Tests that forward rendering launches the correct number of kernels"""
    from mitsuba.core import xml

    ek.set_flag(ek.JitFlag.KernelHistory, False)

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

    ek.set_flag(ek.JitFlag.KernelHistory, False)

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
