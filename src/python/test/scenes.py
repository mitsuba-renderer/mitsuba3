"""Test fixtures containing common scenes."""
import pytest
from mitsuba.core.xml import load_string
from mitsuba.test.util import fresolver_append_path

@pytest.fixture
def empty_scene():
    scene = load_string("""
        <scene version='2.0.0'>
            <sensor type="perspective">
                <film type="hdrfilm">
                    <integer name="width" value="151"/>
                    <integer name="height" value="146"/>
                </film>

                <sampler type="independent"/>
            </sensor>
        </scene>
    """)
    assert scene is not None
    return scene

@pytest.fixture
@fresolver_append_path
def teapot_scene():
    scene = load_string("""
        <scene version='2.0.0'>
            <sensor type="perspective">
                <float name="near_clip" value="1"/>
                <float name="far_clip" value="1000"/>

                <transform name="to_world">
                    <lookat target="0.0, 0.0, 1.5"
                            origin="0.0, -12.0, 1.5"
                            up    ="0.0, 0.0, 1.0"/>
                </transform>

                <film type="hdrfilm">
                    <integer name="width" value="151"/>
                    <integer name="height" value="146"/>
                </film>

                <sampler type="independent">
                    <integer name="sample_count" value="32"/>
                </sampler>
            </sensor>

            <shape type="ply">
                <string name="filename"
                        value="resources/data/ply/teapot.ply"/>

                <bsdf type="diffuse">
                    <spectrum name="reflectance" value="0.4f, 0.1f, 0.8f, 0.5f"/>
                </bsdf>
            </shape>

            <emitter type="point">
                <point name="position" x="2" y="-6.0" z="4.5"/>
                <spectrum name="intensity" value="3.0f, 3.0f, 1.0f, 0.5f"/>
            </emitter>
            <emitter type="point">
                <point name="position" x="-3" y="-3.0" z="-0.5"/>
                <spectrum name="intensity" value="1.0f, 3.0f, 3.0f, 0.5f"/>
            </emitter>
        </scene>
    """)
    assert scene is not None
    return scene

def make_integrator(kind, xml = ""):
    integrator = load_string("<integrator version='2.0.0' type='%s'>"
                             "%s</integrator>" % (kind, xml))
    assert integrator is not None
    return integrator
