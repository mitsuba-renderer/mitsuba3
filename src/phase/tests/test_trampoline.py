import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


@pytest.fixture(scope='module')
def create_phasefunction():
    class MyIsotropicPhaseFunction(mi.scalar_rgb.PhaseFunction):
        def __init__(self, props):
            mi.PhaseFunction.__init__(self, props)
            self.m_flags = mi.PhaseFunctionFlags.Isotropic

        def sample(self, ctx, mei, sample1, sample2, active=True):
            wo = mi.warp.square_to_uniform_sphere(sample2)
            pdf = mi.warp.square_to_uniform_sphere_pdf(wo)
            return (wo, pdf)

        def eval(self, ctx, mei, wo, active=True):
            return mi.warp.square_to_uniform_sphere_pdf(wo)

        def to_string(self):
            return "MyIsotropicPhaseFunction[]"

    mi.scalar_rgb.register_phasefunction("myisotropic", lambda props: MyIsotropicPhaseFunction(props))


@fresolver_append_path
def create_medium_scene(phase_function='isotropic', spp=8):
    scene = mi.load_string(f"""
        <scene version='3.0.0'>
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


def test01_create_and_eval(variant_scalar_rgb, create_phasefunction):
    p = mi.load_string("<phase version='3.0.0' type='myisotropic'/>")
    assert p is not None

    assert mi.has_flag(p.m_flags, mi.PhaseFunctionFlags.Isotropic)
    assert not mi.has_flag(p.m_flags, mi.PhaseFunctionFlags.Anisotropic)
    assert not mi.has_flag(p.m_flags, mi.PhaseFunctionFlags.Microflake)

    ctx = mi.PhaseFunctionContext(None)
    mei = mi.MediumInteraction3f()
    theta = dr.linspace(mi.Float, 0, dr.Pi / 2, 4)
    ph = dr.linspace(mi.Float, 0, dr.Pi, 4)

    wo = [dr.sin(theta), 0, dr.cos(theta)]
    v_eval = p.eval(ctx, mei, wo)

    dr.allclose(v_eval, 1.0 / (4 * dr.Pi))


def test02_render_scene(variant_scalar_rgb, create_phasefunction):
    import numpy as np

    scene = create_medium_scene('myisotropic')
    bitmap = scene.render()
    trampoline_np = np.array(bitmap, copy=False)

    # Reference image: rendered using isotropic phase function
    scene = create_medium_scene('isotropic')
    bitmap = scene.render()
    ref_np = np.array(bitmap, copy=False)

    diff = np.mean((ref_np - trampoline_np) ** 2)
    assert diff < 0.001 # TODO: Replace this by a T-Test
