"""Tests common to all integrator implementations."""

import numpy as np
import os
import pytest

import mitsuba
from mitsuba.scalar_spectral.core import Bitmap, Struct
from mitsuba.scalar_rgb.core.xml import load_string as load_string_scalar_rgb
from mitsuba.test.scenes import SCENES, make_integrator

import importlib

def get_modes_load_string_func():
    load_string_list = []

    def add_load_string(mode):
        try:
            f = importlib.import_module('mitsuba.%s.core.xml' % mode).load_string
            load_string_list.append((mode, f))
        except ImportError:
            pass

    add_load_string('scalar_rgb')
    add_load_string('packet_rgb')
    add_load_string('gpu_rgb')

    return load_string_list

modes = [
    'load_string_func', get_modes_load_string_func()
]

integrators = [
    'int_name', [
        "depth",
        "direct",
        "path",
    ]
]

parameters = [ (integrator, mode[0], mode[1]) for integrator in integrators[1] for mode in modes[1] ]

scene_i = 0
def _save(film, int_name, suffix=''):
    """Quick utility to save rendered scenes for debugging."""
    global scene_i
    fname = os.path.join("/tmp",
                         "scene_{}_{}{}.exr".format(scene_i, int_name, suffix))
    film.set_destination_file(fname)
    film.develop()
    print('Saved debug image to: ' + fname)
    scene_i += 1

def check_scene(int_name, scene_name, mode_name, load_string_func = load_string_scalar_rgb, is_empty = False):
    integrator = make_integrator(int_name, "", load_string_func)
    scene = SCENES[scene_name]['factory'](load_string_func=load_string_func)
    integrator_type = {
        'direct': 'direct',
        'depth':  'depth',
        # All other integrators: 'full'
    }.get(int_name, 'full')
    sensor = scene.sensors()[0]

    avg = SCENES[scene_name][integrator_type]
    film = sensor.film()

    status = integrator.render(scene, sensor)
    assert status, "Rendering ({}) failed".format(mode_name)

    if False:
        _save(film, int_name, suffix='_' + mode_name)

    converted = film.bitmap().convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
    values    = np.array(converted, copy=False)
    means     = np.mean(values, axis=(0, 1))
    # Very noisy images, so we add a tolerance
    assert np.allclose(means, avg, rtol=5e-2), \
           "Mismatch: {} integrator, {} scene, {}".format(int_name, scene_name, mode_name)

    return np.array(film.bitmap(), copy=True)


@pytest.mark.parametrize(*integrators)
def test01_create(int_name):
    integrator = make_integrator(int_name)
    assert integrator is not None

    if int_name == "direct":
        # These properties should be queried
        integrator = make_integrator(int_name, xml="""
            <integer name="emitter_samples" value="3"/>
            <integer name="bsdf_samples" value="12"/>
        """)
        # Cannot specify both shading_samples and (emitter_samples | bsdf_samples)
        with pytest.raises(RuntimeError):
            integrator = make_integrator(int_name, xml="""
                <integer name="shading_samples" value="3"/>
                <integer name="emitter_samples" value="5"/>
            """)
    elif int_name == "path":
        # These properties should be queried
        integrator = make_integrator(int_name, xml="""
            <integer name="rr_depth" value="5"/>
            <integer name="max_depth" value="-1"/>
        """)
        # Cannot use a negative `max_depth`, except -1 (unlimited depth)
        with pytest.raises(RuntimeError):
            integrator = make_integrator(int_name, xml="""
                <integer name="max_depth" value="-2"/>
            """)


@pytest.mark.parametrize("int_name, mode_name, load_string_func", parameters)
def test02_render_empty_scene(int_name, mode_name, load_string_func):
    check_scene(int_name, 'empty', mode_name, load_string_func, is_empty=True)

@pytest.mark.parametrize("int_name, mode_name, load_string_func", parameters)
def test03_render_teapot(int_name, mode_name, load_string_func):
    check_scene(int_name, 'teapot', mode_name, load_string_func)

@pytest.mark.parametrize("int_name, mode_name, load_string_func", parameters)
def test04_render_box(int_name, mode_name, load_string_func):
    check_scene(int_name, 'box', mode_name, load_string_func)

@pytest.mark.parametrize("int_name, mode_name, load_string_func", parameters)
def test05_render_museum_plane(int_name, mode_name, load_string_func):
    check_scene(int_name, 'museum_plane', mode_name, load_string_func)


@pytest.mark.skipif(mitsuba.DEBUG, reason="Timeout is unreliable in debug mode.")
@pytest.mark.parametrize(*integrators)
def test06_render_timeout(int_name):
    from timeit import timeit

    # Very long rendering job, but interrupted by a short timeout
    timeout = 0.5
    integrator = make_integrator(int_name,
                                 """<float name="timeout" value="{}"/>""".format(timeout))
    scene = SCENES['teapot']['factory'](spp=100000)
    sensor = scene.sensors()[0]

    def wrapped_scalar():
        assert integrator.render(scene, sensor) is True
    def wrapped_vector():
        assert integrator.render(scene, sensor) is True

    sensor.film().clear()
    effective_s = timeit(wrapped_scalar, number=1)
    sensor.film().clear()
    effective_v = timeit(wrapped_vector, number=1)

    # Check that timeout is respected +/- 0.5s.
    assert np.allclose(timeout, effective_s, atol=0.5)
    assert np.allclose(timeout, effective_v, atol=0.5)


def make_reference_renders():
    """Produces reference images for the test scenes, printing the per-channel
    average pixel values to update `scenes.SCENE_AVERAGES` when needed."""
    integrators = {
        'depth':  make_integrator('depth'),
        'direct': make_integrator('direct'),
        'full':   make_integrator('path'),
    }

    spp = 32
    averages = { n: {} for n in SCENES }
    for int_name, integrator in integrators.items():
        for scene_name, props in SCENES.items():
            scene = props['factory'](spp=spp)
            sensor = scene.sensors()[0]
            film = sensor.film()

            status = integrator.render(scene, sensor)

            # Extract per-channel averages
            converted = film.bitmap().convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
            values    = np.array(converted, copy=False)
            averages[scene_name][int_name] = np.mean(values, axis=(0,1))

            # Save files
            fname = "scene_{}_{}_reference.exr".format(scene_name, int_name)
            film.set_destination_file(os.path.join("/tmp", fname))
            film.develop()

    print('========== Test scenes: per channel averages ==========')
    for k, avg in averages.items():
        print("'{}': {{".format(k))
        for int_name, v in avg.items():
            print("    {:<12}{},".format("'{}':".format(int_name), list(v)))
        print('},')


if __name__ == '__main__':
    make_reference_renders()
