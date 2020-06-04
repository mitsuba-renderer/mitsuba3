
import numpy as np
import pytest

import mitsuba
mitsuba.set_variant("scalar_rgb")
from mitsuba.core import math, warp, Bitmap, Struct
from mitsuba.core.math import Pi
from mitsuba.core.xml import load_string
from mitsuba.render import (PhaseFunction, PhaseFunctionContext, PhaseFunctionFlags,
                            MediumInteraction3f, register_phasefunction, has_flag)
from mitsuba.python.test.util import fresolver_append_path


@pytest.fixture(scope='module')
def create_phasefunction():
    class MyIsotropicPhaseFunction(PhaseFunction):
        def __init__(self, props):
            PhaseFunction.__init__(self, props)
            self.m_flags = PhaseFunctionFlags.Isotropic

        def sample(self, ctx, mi, sample1, active=True):
            wo = warp.square_to_uniform_sphere(sample1)
            pdf = warp.square_to_uniform_sphere_pdf(wo)
            return (wo, pdf)

        def eval(self, ctx, mi, wo, active=True):
            return warp.square_to_uniform_sphere_pdf(wo)

        def to_string(self):
            return "MyIsotropicPhaseFunction[]"

    register_phasefunction("myisotropic", lambda props: MyIsotropicPhaseFunction(props))


@fresolver_append_path
def create_medium_scene(phase_function='isotropic', spp=8):
    scene = load_string(f"""
        <scene version='2.0.0'>
            <integrator type="volpathmis"/>
            <sensor type="perspective">
                <transform name="to_world">
                    <lookat target="0.0,   0.0, 1.0"
                            origin="0.0, 14.0, 1.0"
                            up    ="0.0,   0.0, 1.0"/>
                </transform>
                <film type="hdrfilm">
                    <integer name="width" value="32"/>
                    <integer name="height" value="32"/>
                    <string name="pixel_format" value="rgb" />
                </film>
                <sampler type="independent">
                    <integer name="sample_count" value="{spp}"/>
                </sampler>
            </sensor>
            <emitter type="constant" />
            <shape type="ply">
                <string name="filename" value="resources/data/common/meshes/teapot.ply"/>
                <bsdf type="null" />
                <medium name="interior" type="homogeneous">
                    <rgb name="sigma_t" value="1.0, 1.0, 1.0"/>
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

    assert has_flag(p.m_flags, PhaseFunctionFlags.Isotropic)
    assert not has_flag(p.m_flags, PhaseFunctionFlags.Anisotropic)
    assert not has_flag(p.m_flags, PhaseFunctionFlags.Microflake)

    ctx = PhaseFunctionContext(None)
    mi = MediumInteraction3f()
    for theta in np.linspace(0, np.pi / 2, 4):
        for ph in np.linspace(0, np.pi, 4):
            wo = [np.sin(theta), 0, np.cos(theta)]
            v_eval = p.eval(ctx, mi, wo)
            assert np.allclose(v_eval, 1.0 / (4 * Pi))


def test02_render_scene(create_phasefunction):
    scene = create_medium_scene('myisotropic')
    status = scene.integrator().render(scene, scene.sensors()[0])
    assert status, "Rendering failed"

    film = scene.sensors()[0].film()
    converted = film.bitmap(raw=True).convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
    trampoline_np = np.array(converted, copy=False)

    # Reference image: rendered using isotropic phase function
    scene = create_medium_scene('isotropic')
    status = scene.integrator().render(scene, scene.sensors()[0])
    assert status, "Reference rendering failed"

    film = scene.sensors()[0].film()
    converted = film.bitmap(raw=True).convert(Bitmap.PixelFormat.RGBA, Struct.Type.Float32, False)
    ref_np = np.array(converted, copy=False)

    diff = np.mean((ref_np - trampoline_np) ** 2)
    assert diff < 0.001 # TODO: Replace this by a T-Test
