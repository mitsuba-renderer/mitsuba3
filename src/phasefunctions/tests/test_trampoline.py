
import numpy as np
import pytest

import mitsuba
from mitsuba.scalar_rgb.core import math, warp, Bitmap, Struct
from mitsuba.scalar_rgb.core.math import Pi
from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.render import (PhaseFunction, register_phasefunction)
from mitsuba.test.util import fresolver_append_path


@pytest.fixture(scope='module')
def create_phasefunction():
    class MyIsotropicPhaseFunction(PhaseFunction):
        def __init__(self, props):
            PhaseFunction.__init__(self, props)

        def sample(self, mi, sample1, active=True):
            return warp.square_to_uniform_sphere(sample1)

        def eval(self, wo, active=True):
            return warp.square_to_uniform_sphere_pdf(wo)

        def to_string(self):
            return "MyIsotropicPhaseFunction[]"

    register_phasefunction("myisotropic", lambda props: MyIsotropicPhaseFunction(props))


@fresolver_append_path
def create_medium_scene(phase_function='isotropic', spp=8):
    scene = load_string(f"""
        <scene version='2.0.0'>
            <integrator type="volpath"/>
            <sensor type="perspective">
                <transform name="to_world">
                    <lookat target="0.0,   0.0, 1.0"
                            origin="0.0, 14.0, 1.0"
                            up    ="0.0,   0.0, 1.0"/>
                </transform>
                <film type="hdrfilm">
                    <integer name="width" value="100"/>
                    <integer name="height" value="100"/>
                    <string name="pixel_format" value="rgb" />
                </film>
                <sampler type="independent">
                    <integer name="sample_count" value="{spp}"/>
                </sampler>
            </sensor>
            <emitter type="constant" />
            <shape type="ply">
                <string name="filename" value="resources/data/ply/teapot.ply"/>
                <bsdf type="null" />
                <medium name="interior" type="homogeneous">
                    <texture3d type="constant3d" name="sigma_t">
                        <rgb name="color" value="1.0, 1.0, 1.0"/>
                    </texture3d>
                    <rgb name="albedo" value="0.99, 0.4, 0.4"/>
                    <phase type="{phase_function}"/>
                    <boolean name="sample_emitters" value="true"/>
                </medium>
            </shape>
        </scene>
    """)
    assert scene is not None
    return scene


def test01_create_and_eval(create_phasefunction):
    p = load_string("<phase version='2.0.0' type='myisotropic'/>")
    assert p is not None

    for theta in np.linspace(0, np.pi / 2, 4):
        for ph in np.linspace(0, np.pi, 4):
            wo = [np.sin(theta), 0, np.cos(theta)]
            v_eval = p.eval(wo)
            assert np.allclose(v_eval, 1.0 / (4 * Pi))


def test02_render_scene(create_phasefunction):
    scene = create_medium_scene('myisotropic')
    status = scene.integrator().render(scene, scene.sensors()[0])
    assert status, "Rendering failed"

    film = scene.sensors()[0].film()
    converted = film.bitmap().convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
    mean_trampoline = np.mean(np.array(converted, copy=False), (0,  1))

    # Reference image: rendered using isotropic phase function
    scene = create_medium_scene('isotropic')
    status = scene.integrator().render(scene, scene.sensors()[0])
    assert status, "Reference rendering failed"

    film = scene.sensors()[0].film()
    converted = film.bitmap().convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
    mean_ref = np.mean(np.array(converted, copy=False), (0, 1))

    # The implementation of "myisotropic" follows the C++ implementation of the "isotropic" plugin
    # Therefore, we can expect the image values to be identical (using the same seed for rendering)
    assert np.allclose(mean_trampoline, mean_ref)
