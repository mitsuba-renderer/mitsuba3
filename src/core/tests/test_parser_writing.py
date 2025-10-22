import pytest
import drjit as dr
import mitsuba as mi
import os

from mitsuba.test.util import fresolver_append_path

config = mi.parser.ParserConfig('scalar_rgb')

def check_roundtrip(scene_dict):
    """Helper function to check that a scene can be serialized to XML and reconstituted"""
    state_1 = mi.parser.parse_dict(config, scene_dict)
    xml_str = mi.parser.write_string(state_1)
    state_2 = mi.parser.parse_string(config, xml_str)
    assert state_1 == state_2
    return xml_str, state_1, state_2


def check_roundtrip_approx(scene_dict, atol=1e-6):
    """Helper function to check roundtrip with approximate equality for transforms"""
    state_1 = mi.parser.parse_dict(config, scene_dict)
    xml_str = mi.parser.write_string(state_1)
    state_2 = mi.parser.parse_string(config, xml_str)

    # First check that the states have the same structure
    assert len(state_1.nodes) == len(state_2.nodes)

    for i in range(len(state_1.nodes)):
        node1, node2 = state_1.nodes[i], state_2.nodes[i]

        # Check all transform properties in node1
        for prop_name in list(node1.props):
            if node1.props.type(prop_name) == mi.Properties.Type.Transform:
                t1 = node1.props.get(prop_name)
                if prop_name in node2.props:
                    # Both have it - check approximately equal
                    t2 = node2.props.get(prop_name)
                    assert dr.allclose(t1.matrix, t2.matrix, atol=atol)
                    del node2.props[prop_name]
                else:
                    # Only node1 has it - must be identity (optimized away in XML)
                    assert dr.allclose(t1.matrix, mi.ScalarTransform4f().matrix, atol=atol)
                del node1.props[prop_name]

    # Now check exact equality with transforms removed
    assert state_1 == state_2
    return xml_str, state_1, state_2


def test01_xml_export_plugin(variant_scalar_rgb):
    """Test basic plugin export and round-trip loading"""

    check_roundtrip({
        "type": "sphere",
        "center": [0, 0, -10],
        "radius": 10.0,
    })


def test02_xml_point(variant_scalar_rgb):
    """Test vector/point property export with different input types"""
    check_roundtrip({ # Test with list
        'type': 'scene',
        'light': {
            'type': 'point',
            'position': [0.0, 1.0, 2.0]
        }
    })

    check_roundtrip({ # Test with ScalarPoint3f
        'type': 'scene',
        'light': {
            'type': 'point',
            'position': mi.ScalarPoint3f(0, 1, 2)
        }
    })

    np = pytest.importorskip("numpy")
    check_roundtrip({ # Test with NumPy array
        'type': 'scene',
        'light': {
            'type': 'point',
            'position': np.array([0, 1, 2])
        }
    })


def test03_xml_rgb(variants_all_scalar):
    """Test RGB value export with different input formats"""
    check_roundtrip({ # Test Color3f input
        'type': 'scene',
        'point': {
            "type": "point",
            "intensity": {
                "type": "rgb",
                "value": mi.ScalarColor3f(0.5, 0.2, 0.5)
            }
        }
    })

    check_roundtrip({ # Test list input
        'type': 'scene',
        'point': {
            "type": "point",
            "intensity": {
                "type": "rgb",
                "value": [0.5, 0.2, 0.5]
            }
        }
    })

    check_roundtrip({
        'type': 'scene', # Test uniform value
        'point': {
            "type": "point",
            "intensity": {
                "type": "rgb",
                "value": 0.5  # Uniform gray
            }
        }
    })


@fresolver_append_path
def test04_xml_spectrum(variants_all_scalar):
    """Test spectrum export with different formats"""
    check_roundtrip({ # Test wavelength-value pairs
        'type': 'scene',
        'light': {
            "type": "point",
            "intensity": {
                "type": "spectrum",
                "value": [(400, 0.5), (500, 0.25), (600, 1), (700, 3)]
            }
        }
    })

    check_roundtrip({ # Test uniform spectrum
        'type': 'scene',
        'light': {
            "type": "point",
            "intensity": {
                "type": "spectrum",
                "value": 0.75
            }
        }
    })

    # Test spectrum sourced from a file
    check_roundtrip({
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : mi.file_resolver().resolve('resources/data/ior/Al.eta.spd')
            }
        }
    })



def test05_xml_references(variant_scalar_rgb):
    """Test nesting and references"""
    check_roundtrip({
        'type': 'scene',
        'bsdf1': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [1.0, 0.0, 0.0]
            }
        },
        'texture1': {
            'type': 'checkerboard'
        },
        'bsdf4': {
            'type': 'dielectric',
            'specular_reflectance': {
                'type': 'ref',
                'id': 'texture1'
            },
            'id': 'bsdf2'
        },
        'shape1': {
            'type': 'sphere',
            'bsdf': {
                'type': 'ref',
                'id': 'bsdf1'
            }
        },
        'shape2': {
            'type': 'cylinder',
            'bsdf': {
                'type': 'ref',
                'id': 'bsdf2'
            }
        }
    })

def test06_xml_defaults(variant_scalar_rgb):
    """Test default parameter system with parameter substitution"""
    # Test basic parameter substitution with scene defaults
    xml_str, *unused = check_roundtrip({
        'type': 'scene',
        'cam': {
            'type': 'perspective',
            'fov_axis': 'x',
            'fov': 35,
            'sampler': {
                'type': 'independent',
                'sample_count': 250  # Default spp
            },
            'film': {
                'type': 'hdrfilm',
                'width': 1920,  # Default resx
                'height': 1080  # Default resy
            }
        }
    })

    # Test that XML contains parameter substitution patterns
    assert '<integer name="sample_count" value="$spp"/>' in xml_str
    assert '<integer name="width" value="$resx"/>' in xml_str
    assert '<integer name="height" value="$resy"/>' in xml_str

    # Parse with parameters and verify it works
    state = mi.parser.parse_string(config, xml_str, spp="512", resx="1280", resy="720")
    assert len(state.nodes) == 4  # scene + sensor + sampler + film

    # Check that the substituted values were used correctly
    sampler_node = state.nodes[2]  # sampler is the 3rd node
    film_node = state.nodes[3]     # film is the 4th node

    assert sampler_node.props.get("sample_count") == 512
    assert film_node.props.get("width") == 1280
    assert film_node.props.get("height") == 720


@pytest.mark.parametrize("object_type,expected_tag", [("shape", "<matrix"), ("sensor", "<lookat") ])
def test08_xml_decompose_transform(variant_scalar_rgb, object_type, expected_tag):
    """Test that transformations are decomposed when the sensor is a camera"""
    scene_dict = {
        'type': 'scene',
        'obj' : {
            'type': 'sphere' if object_type == "shape" else 'perspective',
            'to_world': mi.ScalarTransform4f().look_at([15, 42.3, 25], [1.0, 0.0, 0.5], [1.0, 0.0, 0.0])
        }
    }

    # Use approximate roundtrip check since lookat decomposition has numerical precision limits
    xml_str, state_1, state_2 = check_roundtrip_approx(scene_dict)
    assert expected_tag in xml_str


def test09_xml_transform_decomposition_comprehensive(variant_scalar_rgb):
    """Comprehensive test of all transform decomposition cases supported by decompose_transform"""

    # Test cases that should decompose to specific forms
    test_cases = [
        # (name, transform, expected_tags, object_type)

        # Case 1: Identity transform (should be omitted entirely)
        ("identity", mi.ScalarTransform4f(), [], "shape"),

        # Case 2: Pure translation
        ("translate", mi.ScalarTransform4f().translate([1, 2, 3]), ["<translate"], "shape"),

        # Case 3: Uniform scale only
        ("uniform_scale", mi.ScalarTransform4f().scale(2.0), ["<scale"], "shape"),

        # Case 4: Non-uniform scale only
        ("non_uniform_scale", mi.ScalarTransform4f().scale([1, 2, 3]), ["<scale"], "shape"),

        # Case 5: Scale + translate
        ("scale_translate", mi.ScalarTransform4f().scale(2.0).translate([1, 2, 3]),
         ["<scale", "<translate"], "shape"),

        # Case 6: Single rotation (X-axis)
        ("rotate_x", mi.ScalarTransform4f().rotate([1, 0, 0], 45), ["<rotate x"], "shape"),

        # Case 7: Single rotation (Y-axis)
        ("rotate_y", mi.ScalarTransform4f().rotate([0, 1, 0], 30), ["<rotate y"], "shape"),

        # Case 8: Single rotation (Z-axis)
        ("rotate_z", mi.ScalarTransform4f().rotate([0, 0, 1], 60), ["<rotate z"], "shape"),

        # Case 9: Single rotation + translate
        ("rotate_translate", mi.ScalarTransform4f().rotate([1, 0, 0], 45).translate([1, 2, 3]),
         ["<rotate x", "<translate"], "shape"),

        # Case 10: Scale + single rotation + translate
        ("scale_rotate_translate",
         mi.ScalarTransform4f().scale(2.0).rotate([0, 1, 0], 30).translate([1, 2, 3]),
         ["<scale", "<rotate y", "<translate"], "shape"),

        # Case 11: Multiple rotations for shape (should use matrix)
        ("multi_rotate_shape",
         mi.ScalarTransform4f().rotate([1, 0, 0], 45).rotate([0, 1, 0], 30),
         ["<matrix"], "shape"),

        # Case 12: Multiple rotations for sensor (should use lookat)
        ("multi_rotate_sensor",
         mi.ScalarTransform4f().look_at([5, 10, 15], [0, 0, 0], [1, 1, 0]),
         ["<lookat"], "sensor"),

        # Case 13: Scale + multiple rotations (should use matrix even for sensor)
        ("scale_multi_rotate",
         mi.ScalarTransform4f().scale(2.0).rotate([1, 0, 0], 45).rotate([0, 1, 0], 30),
         ["<matrix"], "sensor"),

        # Case 14: Non-diagonal scale (shear) - should use matrix
        ("shear", mi.ScalarTransform4f([[1, 0.5, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]),
         ["<matrix"], "shape"),
    ]

    for name, transform, expected_tags, obj_type in test_cases:
        scene_dict = {
            'type': 'scene',
            'obj': {
                'type': 'sphere' if obj_type == 'shape' else 'perspective',
                'to_world': transform
            }
        }

        # Use approximate check for all transforms (handles identity removal and numerical precision)
        xml_str, _, _ = check_roundtrip_approx(scene_dict, atol=1e-5)

        # Verify expected tags are present
        for tag in expected_tags:
            assert tag in xml_str, f"Test '{name}': Expected '{tag}' not found in XML"

        # Verify transform tags are omitted for identity/tiny transforms
        if not expected_tags:
            assert '<transform' not in xml_str, f"Test '{name}': Transform should be omitted"

        # For matrix fallback cases, verify specific decomposition tags are NOT present
        if '<matrix' in expected_tags:
            for tag in ['<translate', '<scale', '<rotate', '<lookat']:
                assert tag not in xml_str, f"Test '{name}': '{tag}' should not appear when using matrix"


def test07_complex(variant_scalar_rgb):
    """
    Test the ability to write an XML scene containing various standard constructs
    - floats/vectors/references/booleans
    - hierarchies
    - transformations
    """

    xml_input = '''<scene version="3.5.0">
        <sensor type="perspective" id="camera">
            <float name="fov" value="45.0"/>
            <integer name="sample_count" value="128"/>
            <string name="filter" value="gaussian"/>
            <boolean name="active" value="true"/>
            <transform name="to_world">
                <translate x="0" y="5" z="10"/>
            </transform>
        </sensor>
        <bsdf type="diffuse" id="red_mat">
            <rgb name="reflectance" value="0.5 0.25 0.125"/>
        </bsdf>
        <shape type="sphere" id="sphere1">
            <float name="radius" value="2.5"/>
            <vector name="center" x="0" y="1" z="0"/>
            <ref id="red_mat" name="material"/>
        </shape>
        <shape type="cube">
            <transform name="to_world">
                <translate x="4" y="0" z="0"/>
                <scale x="2" y="4" z="8"/>
            </transform>
            <bsdf type="conductor" id="metal">
                <string name="material" value="Au"/>
            </bsdf>
        </shape>
    </scene>'''

    # Parse the XML
    state = mi.parser.parse_string(config, xml_input)

    # Write to string
    xml_output = mi.parser.write_string(state)

    # Expected output with optimizations:
    # - Default extraction for sample_count -> $spp
    # - Unreferenced IDs (camera, sphere1, metal) are removed
    # - Referenced ID (red_mat) is preserved
    expected = '''<scene version="3.5.0">
    <default name="spp" value="128"/>

    <sensor type="perspective">
        <float name="fov" value="45"/>
        <integer name="sample_count" value="$spp"/>
        <string name="filter" value="gaussian"/>
        <boolean name="active" value="true"/>

        <transform name="to_world">
            <translate y="5" z="10"/>
        </transform>
    </sensor>

    <bsdf type="diffuse" id="red_mat">
        <rgb name="reflectance" value="0.5 0.25 0.125"/>
    </bsdf>

    <shape type="sphere">
        <float name="radius" value="2.5"/>
        <vector name="center" x="0" y="1" z="0"/>
        <ref name="material" id="red_mat"/>
    </shape>

    <shape type="cube">
        <transform name="to_world">
            <scale x="2" y="4" z="8"/>
            <translate x="8"/>
        </transform>

        <bsdf type="conductor">
            <string name="material" value="Au"/>
        </bsdf>
    </shape>
</scene>
'''

    assert xml_output == expected
