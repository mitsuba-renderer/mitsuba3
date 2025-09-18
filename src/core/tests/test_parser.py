import pytest
import mitsuba as mi
import drjit as dr
import re

from mitsuba.scalar_rgb.test.util import fresolver_append_path

def xml_escape(path):
    """Escape dollar signs in paths for XML parameter substitution"""
    return str(path).replace('$', '\\$')

def error_string(msg):
    """Clean error messages by removing unstable parts like [file.cpp:line] prefixes"""
    if isinstance(msg, Exception):
        msg = str(msg)
    msg = re.sub(r'\[\w+\.cpp:\d+\]\s*', '', msg)  # Remove [file.cpp:line]
    return msg


def get_children(state, node):
    """Helper to get child nodes of a SceneNode"""
    result = []
    for name in node.props.keys():
        if node.props.type(name) == mi.Properties.Type.ResolvedReference:
            result.append((name, state.nodes[node.props[name].index()]))
    return result

config = mi.parser.ParserConfig('scalar_rgb')

def test01_basic_xml_parsing(variant_scalar_rgb):
    """Test parsing a simple XML snippet without instantiation"""

    xml = '<phase type="foo" version="1.2.3"/>'

    state = mi.parser.parse_string(config, xml)

    # Check basic structure
    assert len(state.nodes) == 1
    assert state.root.props.plugin_name() == "foo"
    assert state.root.type == mi.ObjectType.PhaseFunction

    # Version should be parsed
    assert len(state.versions) == 1
    assert state.versions[0].major_version == 1
    assert state.versions[0].minor_version == 2
    assert state.versions[0].patch_version == 3

def test02_basic_dict_parsing(variant_scalar_rgb):
    """Test parsing a simple dictionary without instantiation"""

    dict_scene = {
        "type": "scene",
        "mysensor": { "type": "perspective" }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    # Check basic structure
    assert len(state.nodes) == 2  # root + mysensor
    assert state.root.props.plugin_name() == "scene"

    # Check child node via Properties
    assert "mysensor" in state.root.props
    assert state.root.props.type("mysensor") == mi.Properties.Type.ResolvedReference
    ref = state.root.props["mysensor"]
    assert ref.index() == 1
    child = state.nodes[ref.index()]
    assert child.props.plugin_name() == "perspective"


def test03_float_boolean_attributes_xml(variant_scalar_rgb):
    """Test parsing float and boolean attributes in XML"""
    xml = '''<scene version="3.0.0">
        <shape type="plugin_name" id="plugin_id">
            <float name="radius" value="2.5"/>
            <float name="bar" value="-3.14"/>
            <boolean name="active" value="true"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)

    # Should have root + shape nodes
    assert len(state.nodes) == 2

    # Check shape node
    shape_node = state.nodes[1]
    assert shape_node.props.plugin_name() == "plugin_name"
    assert shape_node.props.id() == "plugin_id"

    # Check float and boolean properties with type verification
    assert shape_node.props.type("radius") == mi.Properties.Type.Float
    assert dr.allclose(shape_node.props["radius"], 2.5)
    assert shape_node.props.type("bar") == mi.Properties.Type.Float
    assert dr.allclose(shape_node.props["bar"], -3.14)
    assert shape_node.props.type("active") == mi.Properties.Type.Bool
    assert shape_node.props["active"] == True

    # Test invalid float parsing
    xml_invalid = '''<scene version="3.0.0">
        <shape type="plugin_name">
            <float name="radius" value="foo"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match='could not parse floating point value "foo"'):
        mi.parser.parse_string(config, xml_invalid)


def test04_float_boolean_attributes_dict(variant_scalar_rgb):
    """Test parsing float and boolean attributes in dictionary"""
    dict_scene = {
        "type": "scene",
        "myshape": {
            "type": "plugin_name",
            "id": "plugin_id",
            "radius": 2.5,
            "active": True
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    # Should have root + shape nodes
    assert len(state.nodes) == 2

    # Check shape node via Properties
    assert "myshape" in state.root.props
    ref = state.root.props["myshape"]
    shape_node = state.nodes[ref.index()]
    assert shape_node.props.plugin_name() == "plugin_name"
    assert shape_node.props.id() == "plugin_id"
    # Check float and boolean properties with type verification
    assert shape_node.props.type("radius") == mi.Properties.Type.Float
    assert dr.allclose(shape_node.props["radius"], 2.5)
    assert shape_node.props.type("active") == mi.Properties.Type.Bool
    assert shape_node.props["active"] == True

def test05_xml_parse_error_reporting(variant_scalar_rgb):
    """Test that XML parse errors include line/column information"""

    xml = '''<scene version="3.0.0">
    <shape type="sphere">
        <float name="radius" value="1.0"
    </shape>
</scene>'''

    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.parse_string(config, xml)
    assert error_string(excinfo.value) == 'XML parsing failed: Error parsing start element tag (line 4, col 5)'

def test06_error_bad_version(variant_scalar_rgb):
    """Test error reporting when an invalid version number is specified"""

    with pytest.raises(RuntimeError, match="Invalid version number"):
        mi.parser.parse_string(config, '<phase type="foo" version="invalid"/>')

    with pytest.raises(RuntimeError, match="Invalid version number"):
        mi.parser.parse_string(config, '<phase type="foo" version="1.2"/>')

    with pytest.raises(RuntimeError, match="Invalid version number"):
        mi.parser.parse_string(config, '<phase type="foo" version="1.2.4.5"/>')

def test07_missing_float_attributes_xml(variant_scalar_rgb):
    """Test error handling for missing attributes in XML"""
    # Missing value attribute
    xml1 = '''<scene version="3.0.0">
        <shape type="sphere">
            <float name="radius"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match='missing required attribute "value"'):
        mi.parser.parse_string(config, xml1)

    # Missing name attribute
    xml2 = '''<scene version="3.0.0">
        <shape type="sphere">
            <float value="1.0"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match='missing required attribute "name"'):
        mi.parser.parse_string(config, xml2)

def test08_file_location(variant_scalar_rgb, tmp_path):
    """Test file location reporting with line:column calculation"""
    # Create a temporary file with known content
    # Note: Using explicit indentation to control exact byte positions
    xml_content = """<scene version="3.0.0">
    <!-- This is a comment -->
    <shape type="sphere">
        <float name="radius" value="1.0"/>
    </shape>
</scene>"""

    temp_file = tmp_path / "test.xml"
    with open(temp_file, 'w', newline='\n') as f:
        f.write(xml_content)

    # No try/finally needed - pytest's tmp_path automatically cleans up
    state = mi.parser.ParserState()

    # Test 1: Node at beginning of file (byte 0)
    node1 = mi.parser.SceneNode()
    node1.file_index = 0
    node1.offset = 0  # First character '<'
    node1.props.set_id("node_at_start")

    # Test 2: Node on second line at the '!' of comment
    node2 = mi.parser.SceneNode()
    node2.file_index = 0
    node2.offset = 28  # Offset 28 is at position 5 of line 2
    node2.props.set_id("node_on_line2")

    # Test 3: Node on line 3 at the 's' of <shape>
    node3 = mi.parser.SceneNode()
    node3.file_index = 0
    node3.offset = 60  # Offset 60 is at position 6 of line 3
    node3.props.set_id("node_on_line3")

    # Add nodes to state
    state.nodes = [node1, node2, node3]
    state.files = [str(temp_file)]

    # Check line:column reporting
    location1 = mi.parser.file_location(state, node1)
    assert "line 1, col 1" in location1
    assert str(temp_file) in location1

    location2 = mi.parser.file_location(state, node2)
    assert "line 2, col 5" in location2

    location3 = mi.parser.file_location(state, node3)
    assert r"line 3, col 6" in location3

    # Test node without file info (file_index out of bounds)
    node4 = mi.parser.SceneNode()
    node4.file_index = 999  # Out of bounds
    node4.props.set_id("no_file_info")
    state.nodes = [node1, node2, node3, node4]

    location4 = mi.parser.file_location(state, node4)
    assert "no_file_info" in location4
    assert "line" not in location4

def test09_invalid_xml(variant_scalar_rgb):
    """Test parsing incomplete XML"""
    with pytest.raises(RuntimeError, match="XML parsing failed: No document element found"):
        mi.parser.parse_string(config, '<?xml version="1.0"?>')


def test10_invalid_root_node(variant_scalar_rgb):
    """Test parsing invalid root node"""
    with pytest.raises(RuntimeError, match='encountered an unsupported XML element: <invalid>'):
        mi.parser.parse_string(config, '<?xml version="1.0"?><invalid version="3.0.0"></invalid>')


def test11_duplicate_id(variant_scalar_rgb):
    """Test duplicate ID error"""
    with pytest.raises(RuntimeError, match='duplicate ID: "my_id" \\(previous was at'):
        mi.parser.parse_string(config, """
        <scene version="3.0.0">
            <shape type="ply" id="my_id"/>
            <shape type="ply" id="my_id"/>
        </scene>
        """)


def test12_incorrect_nesting_object_in_property(variant_scalar_rgb):
    """Test incorrect nesting - object inside property"""
    with pytest.raises(RuntimeError, match='<shape> element cannot occur as child of a property'):
        mi.parser.parse_string(config, """<scene version="3.0.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <shape type="ply"/>
                   </integer></shape></scene>""")


def test13_incorrect_nesting_property_in_property(variant_scalar_rgb):
    """Test incorrect nesting - property inside property"""
    with pytest.raises(RuntimeError, match='<float> element cannot occur as child of a property'):
        mi.parser.parse_string(config, """<scene version="3.0.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <float name="value" value="1"/>
                   </integer></shape></scene>""")


def test14_incorrect_nesting_property_in_transform(variant_scalar_rgb):
    """Test incorrect nesting - non-transform in transform node"""
    with pytest.raises(RuntimeError, match='unexpected <integer> element inside <transform>'):
        mi.parser.parse_string(config, """<scene version="3.0.0">
                   <shape type="ply">
                   <transform name="to_world">
                   <integer name="value" value="10"/>
                   </transform>
                   </shape></scene>""")


def test15_duplicate_parameter(variant_scalar_rgb):
    """Test duplicate parameter error"""
    with pytest.raises(RuntimeError, match='Property "a" was specified multiple times'):
        mi.parser.parse_string(config, """<scene version="3.0.0">
                       <integer name="a" value="1"/>
                       <integer name="a" value="1"/>
                       </scene>""")


def test16_invalid_boolean(variant_scalar_rgb):
    """Test invalid boolean value"""
    with pytest.raises(RuntimeError, match='could not parse boolean value "a" -- must be "true" or "false"'):
        mi.parser.parse_string(config, """<scene version="3.0.0">
                   <boolean name="10" value="a"/>
                   </scene>""")


def test17_invalid_vector_mixed_attributes(variant_scalar_rgb):
    """Test invalid vector with mixed value and x/y/z attributes"""
    with pytest.raises(RuntimeError, match='Cannot mix "value" and "x"/"y"/"z" attributes'):
        mi.parser.parse_string(config, """<scene version="3.0.0">
                   <vector name="10" value="1, 2, 3" x="4"/>
                   </scene>""")



def test18_error_location_reporting(variant_scalar_rgb):
    """Test that error messages include line:column information"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <float name="radius" value="invalid"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.parse_string(config, xml)
    error_msg = error_string(excinfo.value)
    assert 'Error while loading string (line 3, col 14): could not parse floating point value "invalid"' == error_msg


def test19_error_missing_type_dict(variant_scalar_rgb):
    """Test comprehensive error handling for dictionary parsing"""

    # Test missing type attribute with hierarchical path
    dict_scene1 = {
        "type": "scene",
        "mysensor": {
            "fov": 45
        }
    }

    with pytest.raises(RuntimeError, match=r"\[mysensor\] missing 'type' attribute"):
        mi.parser.parse_dict(config, dict_scene1)

    # Test invalid key with dot
    dict_scene2 = {
        "type": "scene",
        "my.sensor": {  # Invalid key with dot
            "type": "perspective"
        }
    }

    with pytest.raises(RuntimeError, match=r"The object key 'my.sensor' contains a '.' character, which is reserved as a delimiter in object paths. Please use '_' instead"):
        mi.parser.parse_dict(config, dict_scene2)


def test20_dict_hierarchical_error_paths(variant_scalar_rgb):
    """Test that error messages include hierarchical paths"""

    # Test missing type in nested object
    dict_scene1 = {
        "type": "scene",
        "integrator": {
            "type": "path",
            "nested": {
                # Missing type
                "value": 5
            }
        }
    }

    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.parse_dict(config, dict_scene1)
    error_msg = error_string(excinfo.value)
    assert "[integrator.nested] missing 'type' attribute" == error_msg

    # Test invalid RGB value with path
    dict_scene2 = {
        "type": "scene",
        "material": {
            "type": "diffuse",
            "reflectance": [0.5, 0.7]  # Only 2 components
        }
    }

    with pytest.raises(RuntimeError, match=r"\[material\.reflectance\] could not assign an object of type 'list' to key 'reflectance'"):
        mi.parser.parse_dict(config, dict_scene2)

    # Test invalid property name with path
    dict_scene3 = {
        "type": "scene",
        "sensor": {
            "type": "perspective",
            "film": {
                "type": "hdrfilm",
                "pixel.filter": {  # Invalid name with dot
                    "type": "gaussian"
                }
            }
        }
    }

    with pytest.raises(RuntimeError, match=r"\[sensor.film\] The object key 'pixel.filter' contains a '.' character, which is reserved as a delimiter in object paths. Please use '_' instead."):
        mi.parser.parse_dict(config, dict_scene3)


def test21_basic_properties_xml(variant_scalar_rgb):
    """Test parsing integer, string, vector, point, and RGB properties from XML"""

    xml = '''<scene version="3.0.0">
        <shape type="foo">
            <integer name="samples" value="16"/>
            <string name="filename" value="mesh.obj"/>
            <vector name="direction" value="1 0 0"/>
            <point name="position" value="0 2 -1"/>
            <vector name="up" x="0" y="1" z="0"/>
            <rgb name="color" value="0.5 0.7 0.9"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    assert len(state.nodes) == 2

    shape = state.nodes[1]

    # Verify property types and values
    assert shape.props.type("samples") == mi.Properties.Type.Integer
    assert shape.props["samples"] == 16
    assert shape.props.type("filename") == mi.Properties.Type.String
    assert shape.props["filename"] == "mesh.obj"
    assert shape.props.type("direction") == mi.Properties.Type.Vector
    assert dr.allclose(shape.props["direction"], [1, 0, 0])
    assert shape.props.type("up") == mi.Properties.Type.Vector
    assert dr.allclose(shape.props["up"], [0, 1, 0])
    assert shape.props.type("position") == mi.Properties.Type.Vector
    assert dr.allclose(shape.props["position"], [0, 2, -1])
    assert shape.props.type("color") == mi.Properties.Type.Color
    assert dr.allclose(shape.props["color"], [0.5, 0.7, 0.9])


def test22_basic_properties_dict(variant_scalar_rgb):
    """Test parsing integer, string, vector, and RGB properties from dictionaries"""

    dict_scene = {
        "type": "scene",
        "myshape": {
            "type": "foo",
            "samples": 16,
            "filename": "mesh.obj",
            "direction": [1, 0, 0],
            "position": [0, 2, -1],
            "color": mi.ScalarColor3f(0.5, 0.7, 0.9)
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)
    assert len(state.nodes) == 2

    # Verify property types and values
    shape = state.nodes[1]
    assert shape.props.type("samples") == mi.Properties.Type.Integer
    assert shape.props["samples"] == 16
    assert shape.props.type("filename") == mi.Properties.Type.String
    assert shape.props["filename"] == "mesh.obj"
    assert shape.props.type("direction") == mi.Properties.Type.Vector
    assert dr.allclose(shape.props["direction"], [1, 0, 0])
    assert shape.props.type("position") == mi.Properties.Type.Vector
    assert dr.allclose(shape.props["position"], [0, 2, -1])
    assert shape.props.type("color") == mi.Properties.Type.Color
    assert dr.allclose(shape.props["color"], [0.5, 0.7, 0.9])


def test23_spectrum_xml(variant_scalar_rgb):
    """Test parsing of spectrum tags in XML"""
    xml = '<phase type="foo" version="3.0.0"><spectrum name="value" value="0.5"/></phase>'
    state = mi.parser.parse_string(config, xml)

    assert len(state.nodes) == 1
    assert state.root.props.type("value") == mi.Properties.Type.Spectrum
    spectrum_data = state.root.props["value"]
    assert spectrum_data.is_uniform()
    assert spectrum_data.values == [0.5]
    assert len(spectrum_data.wavelengths) == 0

    xml = '<phase type="foo" version="3.0.0"><spectrum name="value" value="400:0.1 500:0.5 600:0.9"/></phase>'
    state = mi.parser.parse_string(config, xml)

    assert len(state.nodes) == 1
    assert state.root.props.type("value") == mi.Properties.Type.Spectrum
    spectrum_data = state.root.props["value"]
    assert not spectrum_data.is_uniform()
    assert spectrum_data.wavelengths == [400.0, 500.0, 600.0]
    assert spectrum_data.values == [0.1, 0.5, 0.9]

    # Test failure modes
    # Single wavelength entry should fail
    with pytest.raises(RuntimeError, match="Spectrum must have at least two entries"):
        mi.parser.parse_string(config, '''<scene version="3.0.0">
            <spectrum name="value" value="400:0.5"/>
        </scene>''')

    # Non-increasing wavelengths should fail
    with pytest.raises(RuntimeError, match="Wavelengths must be specified in increasing order"):
        mi.parser.parse_string(config, '''<scene version="3.0.0">
            <spectrum name="value" value="500:0.5, 400:0.1, 600:0.9"/>
        </scene>''')

    # Invalid tokens should fail
    with pytest.raises(RuntimeError, match='Invalid spectrum value "foo" \\(expected wavelength:value pairs\\)'):
        mi.parser.parse_string(config, '''<scene version="3.0.0">
            <spectrum name="value" value="foo bar"/>
        </scene>''')


def test24_spectrum_dict(variant_scalar_rgb):
    """Test parsing uniform spectrum from dictionary"""
    dict_scene = {
        "type": "phase",
        "plugin_type": "foo",
        "value": {"type": "spectrum", "value": 0.5}
    }

    state = mi.parser.parse_dict(config, dict_scene)

    assert len(state.nodes) == 1
    assert state.root.props.type("value") == mi.Properties.Type.Spectrum
    spectrum_data = state.root.props["value"]
    assert spectrum_data.is_uniform()
    assert spectrum_data.values == [0.5]
    assert len(spectrum_data.wavelengths) == 0

    # Test spectrum with multiple wavelengths
    dict_scene2 = {
        "type": "phase",
        "plugin_type": "foo",
        "value": {"type": "spectrum", "value": [[400, 0.1], [500, 0.5], [600, 0.9]]}
    }

    state2 = mi.parser.parse_dict(config, dict_scene2)

    assert len(state2.nodes) == 1
    assert state2.root.props.type("value") == mi.Properties.Type.Spectrum
    spectrum_data2 = state2.root.props["value"]
    assert not spectrum_data2.is_uniform()
    assert spectrum_data2.wavelengths == [400.0, 500.0, 600.0]
    assert spectrum_data2.values == [0.1, 0.5, 0.9]


def test25_rgb_dict(variant_scalar_rgb):
    """Test parsing RGB spectrum from nested dictionary with type: rgb"""
    dict_scene = {
        "type": "phase",
        "plugin_type": "foo",
        "value": {"type": "rgb", "value": [0.1, 0.2, 0.3]}
    }

    state = mi.parser.parse_dict(config, dict_scene)

    assert len(state.nodes) == 1
    assert state.root.props.type("value") == mi.Properties.Type.Color
    color_data = state.root.props["value"]
    assert dr.allclose(color_data, [0.1, 0.2, 0.3])


def test26_parameter_substitution(variant_scalar_rgb):
    """Test parameter substitution in XML"""
    xml = '<phase type="$mytype" version="3.0.0"/>'
    state = mi.parser.parse_string(config, xml, mytype="isotropic")
    assert state.root.props.plugin_name() == "isotropic"


def test27_parameter_substitution_complex(variant_scalar_rgb):
    """Test more complex cases of parameter substitution"""

    xml = '''<scene version="3.0.0">
        <shape type="$shapetype" id="$shapeid">
            <float name="radius" value="$radius"/>
            <string name="material" value="material_$material_name"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml,
                                   shapetype="sphere",
                                   shapeid="mysphere",
                                   radius="2.5",
                                   material_name="gold")

    assert len(state.nodes) == 2
    shape_node = state.nodes[1]
    assert shape_node.props.plugin_name() == "sphere"
    assert shape_node.props.id() == "mysphere"
    assert "radius" in shape_node.props
    assert dr.allclose(shape_node.props["radius"], 2.5)
    assert shape_node.props["material"] == "material_gold"

    # Test multiple substitutions
    xml2 = '<shape type="sphere" id="$prefix_$suffix_$prefix" version="3.0.0"/>'
    state2 = mi.parser.parse_string(config, xml2, prefix="start", suffix="end")
    assert state2.root.props.id() == "start_end_start"

    # Test undefined parameter error
    xml3 = '<phase type="$undefined_param" version="3.0.0"/>'
    with pytest.raises(RuntimeError, match="undefined parameter: \\$undefined_param"):
        mi.parser.parse_string(config, xml3)


def test28_parameter_substitution_warnings(variant_scalar_rgb):
    """Test unused parameter handling with different log levels"""
    xml = '<phase type="isotropic" version="3.0.0"/>'

    # Test multiple unused parameters - check exact error message format
    config = mi.parser.ParserConfig('scalar_rgb')
    config.unused_parameters = mi.LogLevel.Error

    # Parameter keys are sorted by size. By using keys of different lengths,
    # we can assert that the error message is always formatted in the same way.
    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.parse_string(config, xml, param001="value1", param02="value2", param3="value3")

    # Check exact error message format
    expected = "Found unused parameters:\n  - $param001=value1\n  - $param02=value2\n  - $param3=value3"
    assert error_string(excinfo.value) == expected

    # With config.unused_parameters=Debug, the operation should succeed
    config.unused_parameters = mi.LogLevel.Debug
    state = mi.parser.parse_string(config, xml, unused_param="value")
    assert len(state.nodes) == 1  # Verify parsing succeeded


def test29_child_plugins_xml(variant_scalar_rgb):
    """Test XML parsing of scenes with nested plugins"""

    xml = '''<scene version="3.0.0">
        <shape type="sphere" id="myshape">
            <float name="radius" value="2.0"/>
            <bsdf type="diffuse" id="mybsdf">
                <rgb name="reflectance" value="0.8 0.2 0.2"/>
            </bsdf>
            <emitter type="area">
                <rgb name="radiance" value="10 10 10"/>
            </emitter>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)

    # Should have: root, shape, bsdf, emitter
    assert len(state.nodes) == 4

    # Check root
    assert state.root.props.plugin_name() == "scene"
    root_children = get_children(state, state.root)
    assert len(root_children) == 1

    # Check shape
    shape_name, shape = root_children[0]
    assert shape.props.plugin_name() == "sphere"
    assert shape.props.id() == "myshape"
    assert dr.allclose(shape.props["radius"], 2.0)

    # Check shape's children
    shape_children = get_children(state, shape)
    assert len(shape_children) == 2

    # Find BSDF and emitter by type
    children_by_type = {child.type: (name, child) for name, child in shape_children}

    # Check BSDF
    bsdf_name, bsdf = children_by_type[mi.ObjectType.BSDF]
    assert bsdf.props.plugin_name() == "diffuse"
    assert bsdf.props.id() == "mybsdf"
    assert bsdf_name == "_arg_0"
    assert "reflectance" in bsdf.props
    assert dr.allclose(bsdf.props["reflectance"], [0.8, 0.2, 0.2])

    # Check emitter
    emitter_name, emitter = children_by_type[mi.ObjectType.Emitter]
    assert emitter.props.plugin_name() == "area"
    assert emitter_name == "_arg_1"
    assert "radiance" in emitter.props
    assert dr.allclose(emitter.props["radiance"], [10, 10, 10])


def test30_child_plugins_dict(variant_scalar_rgb):
    """Test parsing plugins with child plugins from dictionary"""
    dict_scene = {
        "type": "scene",
        "myshape": {
            "type": "sphere",
            "radius": 2.0,
            "mybsdf": {
                "type": "diffuse",
                "reflectance": [0.8, 0.2, 0.2]
            },
            "myemitter": {
                "type": "area",
                "radiance": [10, 10, 10]
            }
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    # Should have: root, shape, bsdf, emitter
    assert len(state.nodes) == 4

    # Check shape - in dict parser, shape is named "myshape"
    root_children = get_children(state, state.root)
    assert len(root_children) == 1
    shape_name, shape = root_children[0]
    assert shape.props.plugin_name() == "sphere"
    assert shape_name == "myshape"
    assert "radius" in shape.props
    assert dr.allclose(shape.props["radius"], 2.0)

    # Check shape's children
    shape_children = get_children(state, shape)
    assert len(shape_children) == 2

    # Find BSDF and emitter by name
    children_by_name = {name: child for name, child in shape_children}

    # Check BSDF
    assert "mybsdf" in children_by_name
    bsdf = children_by_name["mybsdf"]
    assert bsdf.props.plugin_name() == "diffuse"
    assert "reflectance" in bsdf.props
    assert dr.allclose(bsdf.props["reflectance"], [0.8, 0.2, 0.2])

    # Check emitter
    assert "myemitter" in children_by_name
    emitter = children_by_name["myemitter"]
    assert emitter.props.plugin_name() == "area"
    assert "radiance" in emitter.props
    assert dr.allclose(emitter.props["radiance"], [10, 10, 10])


def test31_xml_include_basic(variant_scalar_rgb, tmp_path):
    """Test basic file inclusion in XML"""
    # Create included file
    include_file = tmp_path / "include.xml"
    include_xml = '''<shape type="rectangle" version="3.5.0">
        <float name="width" value="2.0"/>
        <float name="height" value="3.0"/>
    </shape>'''
    include_file.write_text(include_xml)

    # Main XML with include
    main_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(include_file)}"/>
    </scene>'''

    state = mi.parser.parse_string(config, main_xml)

    # Should have scene + shape
    assert len(state.nodes) == 2
    assert state.root.props.plugin_name() == "scene"

    # Check included shape
    root_children = get_children(state, state.root)
    assert len(root_children) == 1
    _, shape = root_children[0]
    assert shape.props.plugin_name() == "rectangle"
    assert "width" in shape.props
    assert shape.props["width"] == 2.0
    assert "height" in shape.props
    assert shape.props["height"] == 3.0


def test32_xml_include_scene_merge(variant_scalar_rgb, tmp_path):
    """Test that included scene children are merged into parent"""
    # Create included file with scene root
    include_file = tmp_path / "scene_fragment.xml"
    include_xml = '''<scene version="3.5.0">
        <shape type="sphere">
            <float name="radius" value="1.0"/>
        </shape>
        <shape type="cube">
            <float name="size" value="2.0"/>
        </shape>
    </scene>'''
    include_file.write_text(include_xml)

    # Main XML
    main_xml = f'''<scene version="3.5.0">
        <shape type="rectangle">
            <float name="width" value="5.0"/>
        </shape>
        <include filename="{xml_escape(include_file)}"/>
    </scene>'''

    state = mi.parser.parse_string(config, main_xml)

    # Should have: main scene + rectangle + sphere + cube = 4 nodes
    # The included scene root is merged, so its children become children of the main scene
    assert len(state.nodes) == 4
    root_children = get_children(state, state.root)

    # All shapes should be children of the root scene
    assert len(root_children) == 3

    # Verify all shapes are present
    shape_types = [child.props.plugin_name() for _, child in root_children]
    assert "rectangle" in shape_types
    assert "sphere" in shape_types
    assert "cube" in shape_types


@pytest.mark.parametrize("absolute_path", [True, False])
def test33_xml_include_path_resolution(absolute_path, variant_scalar_rgb, tmp_path):
    """Test both absolute and relative path resolution in includes"""
    # Create subdirectory
    subdir = tmp_path / "subdir"
    subdir.mkdir()

    # Create included file in subdirectory
    include_file = subdir / "shape.xml"
    include_xml = '''<shape type="sphere" version="3.5.0">
        <float name="radius" value="0.5"/>
    </shape>'''
    include_file.write_text(include_xml)

    # Main file in parent directory
    main_file = tmp_path / "main.xml"
    if absolute_path:
        include_path = str(include_file)
    else:
        include_path = "subdir/shape.xml"

    main_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(include_path)}"/>
    </scene>'''
    main_file.write_text(main_xml)

    # Parse from file to test path resolution
    state = mi.parser.parse_file(config, str(main_file))

    assert len(state.nodes) == 2
    root_children = get_children(state, state.root)
    assert len(root_children) == 1
    _, shape = root_children[0]
    assert shape.props.plugin_name() == "sphere"
    assert shape.props["radius"] == 0.5


def test34_xml_include_nested(variant_scalar_rgb, tmp_path):
    """Test nested includes"""
    # Create deeply included file
    deep_file = tmp_path / "deep.xml"
    deep_xml = '''<shape type="sphere" version="3.5.0">
        <float name="radius" value="0.25"/>
    </shape>'''
    deep_file.write_text(deep_xml)

    # Create middle include file
    middle_file = tmp_path / "middle.xml"
    middle_xml = f'''<shape type="cube" version="3.5.0">
        <float name="size" value="1.0"/>
        <include filename="{xml_escape(deep_file)}"/>
    </shape>'''
    middle_file.write_text(middle_xml)

    # Main XML
    main_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(middle_file)}"/>
    </scene>'''

    state = mi.parser.parse_string(config, main_xml)

    # Should have scene + cube + sphere
    assert len(state.nodes) == 3

    # Find the cube
    _, cube = get_children(state, state.root)[0]
    assert cube.props.plugin_name() == "cube"

    # Find the sphere (child of cube)
    cube_children = get_children(state, cube)
    assert len(cube_children) == 1
    _, sphere = cube_children[0]
    assert sphere.props.plugin_name() == "sphere"
    assert sphere.props["radius"] == 0.25


def test35_xml_include_errors(variant_scalar_rgb, tmp_path):
    """Test error handling in includes"""

    # Test missing file
    xml = '''<scene version="3.5.0">
        <include filename="nonexistent.xml"/>
    </scene>'''

    with pytest.raises(Exception, match="not found"):
        mi.parser.parse_string(config, xml)

    # Test circular include
    file1 = tmp_path / "file1.xml"
    file2 = tmp_path / "file2.xml"

    file1_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(file2)}"/>
    </scene>'''
    file1.write_text(file1_xml)

    file2_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(file1)}"/>
    </scene>'''
    file2.write_text(file2_xml)

    with pytest.raises(Exception, match="Exceeded maximum include recursion depth"):
        mi.parser.parse_file(config, str(file1))

    # Test include without filename
    xml = '''<scene version="3.5.0">
        <include/>
    </scene>'''

    with pytest.raises(Exception, match='missing required attribute "filename"'):
        mi.parser.parse_string(config, xml)


def test36_xml_include_parameter_substitution(variant_scalar_rgb, tmp_path):
    """Test that parameter substitution works in included files"""
    # Create included file with parameters
    include_file = tmp_path / "param_shape.xml"
    include_xml = '''<scene version="3.5.0">
        <shape type="sphere" id="sphere_$suffix">
            <float name="radius" value="$radius"/>
        </shape>
    </scene>'''
    include_file.write_text(include_xml)

    # Main XML with include
    main_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(include_file)}"/>
    </scene>'''

    # Parse with parameters
    state = mi.parser.parse_string(config, main_xml, radius="2.5", suffix="test")

    # Check that parameters were substituted
    _, shape = get_children(state, state.root)[0]
    assert shape.props["radius"] == 2.5
    assert shape.props.id() == "sphere_test"


def test37_xml_include_error_chain(variant_scalar_rgb, tmp_path):
    """Test that errors in nested includes show the full include chain"""

    # Create a file with an error (undefined parameter)
    error_file = tmp_path / "error.xml"
    error_xml = '''<scene version="3.5.0">
        <shape type="sphere">
            <float name="radius" value="$undefined_param"/>
        </shape>
    </scene>'''
    error_file.write_text(error_xml)

    # Create middle file that includes the error file
    middle_file = tmp_path / "middle.xml"
    middle_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(error_file)}"/>
    </scene>'''
    middle_file.write_text(middle_xml)

    # Create main file that includes the middle file
    main_file = tmp_path / "main.xml"
    main_xml = f'''<scene version="3.5.0">
        <include filename="{xml_escape(middle_file)}"/>
    </scene>'''
    main_file.write_text(main_xml)

    # Test the error chain
    with pytest.raises(Exception) as excinfo:
        mi.parser.parse_file(config, str(main_file))
    # Remove the tmp_path prefix to make the test deterministic
    error_msg = error_string(excinfo.value).replace(str(tmp_path) + "/", "").replace(str(tmp_path) + "\\", "")

    # The expected error message with full include chain
    expected = (
        "Error while loading main.xml (line 2, col 10): while processing <include>:\n"
        "Error while loading middle.xml (line 2, col 10): while processing <include>:\n"
        "Error while loading error.xml (line 3, col 14): undefined parameter: $undefined_param"
    )

    assert error_msg == expected


def test38_xml_unexpected_attributes(variant_scalar_rgb):
    """Test that unexpected attributes are rejected"""

    # Test unexpected attribute on vector tag
    xml = '''<scene version="3.5.0">
        <shape type="sphere">
            <vector name="position" x="1" y="2" z="3" foo="bar"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="unexpected attribute"):
        mi.parser.parse_string(config, xml)

    # Test unexpected attribute on float tag
    xml = '''<scene version="3.5.0">
        <shape type="sphere">
            <float name="radius" value="1.0" invalid="true"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="unexpected attribute"):
        mi.parser.parse_string(config, xml)

    # Test unexpected attribute on shape tag
    xml = '''<scene version="3.5.0">
        <shape type="sphere" unknown="value">
            <float name="radius" value="1.0"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="unexpected attribute"):
        mi.parser.parse_string(config, xml)


def test39_transform_upgrade_called(variant_scalar_rgb):
    """Test transform_upgrade functionality with camelCase conversion and specific renames"""

    # Test v1.0 scene with camelCase properties and diffuse BSDF
    xml_str = """<scene version="1.0.0">
        <shape type="sphere">
            <float name="myRadius" value="1.0"/>
            <float name="someValue" value="2.0"/>
        </shape>
        <bsdf type="diffuse">
            <rgb name="diffuseReflectance" value="0.5 0.3 0.1"/>
        </bsdf>
    </scene>"""

    state = mi.parser.parse_string(config, xml_str)

    # Before upgrade - verify original property names
    shape_node = state.nodes[1]
    bsdf_node = state.nodes[2]
    assert "myRadius" in shape_node.props
    assert "someValue" in shape_node.props
    assert "diffuseReflectance" in bsdf_node.props

    # Apply upgrade transformation
    mi.parser.transform_upgrade(config, state)

    # Reload nodes after transformations
    shape_node = state.nodes[1]
    bsdf_node = state.nodes[2]

    # After upgrade - verify camelCase conversion
    assert "myRadius" not in shape_node.props
    assert "someValue" not in shape_node.props
    assert "my_radius" in shape_node.props
    assert "some_value" in shape_node.props
    assert shape_node.props.get("my_radius") == 1.0
    assert shape_node.props.get("some_value") == 2.0

    # Verify diffuse_reflectance -> reflectance rename
    assert "diffuseReflectance" not in bsdf_node.props
    assert "diffuse_reflectance" not in bsdf_node.props
    assert "reflectance" in bsdf_node.props
    color = bsdf_node.props.get("reflectance")
    assert abs(color[0] - 0.5) < 1e-6

    # Test that v3.0 scenes are not modified
    xml_v3 = """<scene version="3.0.0">
        <shape type="sphere">
            <float name="myRadius" value="1.0"/>
        </shape>
    </scene>"""

    state_v3 = mi.parser.parse_string(config, xml_v3)
    mi.parser.transform_upgrade(config, state_v3)
    # Should remain unchanged
    assert "myRadius" in state_v3.nodes[1].props
    assert "my_radius" not in state_v3.nodes[1].props


def test40_transform_tags_xml(variant_scalar_rgb):
    """Test parsing transform tags from XML"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="to_world">
                <translate x="1" y="2" z="3"/>
                <rotate angle="90" x="0" y="1" z="0"/>
                <scale value="2"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    assert len(state.nodes) == 2

    shape = state.nodes[1]
    assert "to_world" in shape.props
    transform = shape.props["to_world"]

    # Verify it's a Transform4d (parser uses doubles internally)
    assert isinstance(transform, mi.Transform4d)

    # The transform should be: scale(2) * rotate(90, y) * translate(1, 2, 3)
    # Note: transforms are applied right-to-left, so this translates first
    expected = mi.Transform4d().scale([2, 2, 2]) @ \
               mi.Transform4d().rotate([0, 1, 0], 90) @ \
               mi.Transform4d().translate([1, 2, 3])

    assert dr.allclose(transform.matrix, expected.matrix)


def test41_transform_translate(variant_scalar_rgb):
    """Test translate transform tag"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="t1">
                <translate x="1" y="2" z="3"/>
            </transform>
            <transform name="t2">
                <translate value="4 5 6"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    shape = state.nodes[1]

    # Test first translate
    t1 = shape.props["t1"]
    assert isinstance(t1, mi.Transform4d)
    expected1 = mi.Transform4d().translate([1, 2, 3])
    assert dr.allclose(t1.matrix, expected1.matrix)

    # Test second translate with value attribute
    t2 = shape.props["t2"]
    assert isinstance(t2, mi.Transform4d)
    expected2 = mi.Transform4d().translate([4, 5, 6])
    assert dr.allclose(t2.matrix, expected2.matrix)

def test42_transform_perspective_camera(variant_scalar_rgb):
    """Test perspective camera with to_world transform"""
    xml = '''<scene version="3.0.0">
        <sensor type="perspective">
            <transform name="to_world">
                <lookat origin="0 0 5" target="0 0 0" up="0 1 0"/>
            </transform>
        </sensor>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    sensor = state.nodes[1]

    # Test camera's to_world transform
    assert "to_world" in sensor.props
    to_world = sensor.props["to_world"]
    assert isinstance(to_world, mi.Transform4d)
    expected = mi.Transform4d().look_at(
        origin=[0, 0, 5],
        target=[0, 0, 0],
        up=[0, 1, 0]
    )
    assert dr.allclose(to_world.matrix, expected.matrix)


def test43_transform_matrix(variant_scalar_rgb):
    """Test matrix transform tag"""

    # Test 4x4 custom matrix (90 degree rotation around Y)
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="m1">
                <matrix value="0 0 1 0  0 1 0 0  -1 0 0 0  0 0 0 1"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    shape = state.nodes[1]
    m1 = shape.props["m1"]
    expected = mi.Transform4d().rotate([0, 1, 0], 90)
    assert dr.allclose(m1.matrix, expected.matrix)

    # Test invalid matrix size
    xml_bad = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="m">
                <matrix value="1 2 3 4 5"/>
            </transform>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="matrix must have 9 or 16 values"):
        mi.parser.parse_string(config, xml_bad)

def test44_transform_composition(variant_scalar_rgb):
    """Test composing multiple transforms"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="composite">
                <translate x="1" y="0" z="0"/>
                <rotate angle="90" x="0" y="1" z="0"/>
                <scale value="2"/>
                <translate x="0" y="1" z="0"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    shape = state.nodes[1]

    composite = shape.props["composite"]
    assert isinstance(composite, mi.Transform4d)

    # The transforms are applied in order (each new transform multiplies from the left)
    # So the final transform is: translate(0,1,0) * scale(2) * rotate(90,y) * translate(1,0,0)
    expected = mi.Transform4d().translate([0, 1, 0]) @ \
               mi.Transform4d().scale([2, 2, 2]) @ \
               mi.Transform4d().rotate([0, 1, 0], 90) @ \
               mi.Transform4d().translate([1, 0, 0])

    assert dr.allclose(composite.matrix, expected.matrix)


def test45_transform_outside_transform_tag(variant_scalar_rgb):
    """Test that transform operations outside transform tags produce errors"""

    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <translate x="1" y="0" z="0"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="can only appear inside a <transform> tag"):
        mi.parser.parse_string(config, xml)


def test46_dict_transform_support(variant_scalar_rgb):
    """Test parsing Transform4f values in dictionary"""

    transform = mi.Transform4f().translate(mi.Point3f(1, 2, 3)) @ mi.Transform4f().scale(mi.Point3f(2, 2, 2))

    dict_scene = {
        "type": "scene",
        "shape": {
            "type": "sphere",
            "to_world": transform
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    # Check transform property
    shape_node = state.nodes[1]
    assert "to_world" in shape_node.props
    stored_transform = shape_node.props["to_world"]
    assert dr.allclose(stored_transform.matrix, transform.matrix)


def test47_dict_reference_support(variant_scalar_rgb):
    """Test reference dictionary support"""
    dict_scene = {
        "type": "scene",
        "material": {
            "type": "diffuse",
            "id": "my_material",
            "reflectance": mi.Color3f(0.8, 0.6, 0.4)
        },
        "shape": {
            "type": "sphere",
            "bsdf": {
                "type": "ref",
                "id": "my_material"
            }
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    # Check that the material has an ID
    material_node = state.nodes[1]
    assert material_node.props.id() == "my_material"

    # Check that the shape has a reference property
    shape_node = state.nodes[2]
    assert "bsdf" in shape_node.props
    assert shape_node.props.type("bsdf") == mi.Properties.Type.Reference

    # The reference should point to the material
    ref = shape_node.props["bsdf"]
    assert ref.id() == "my_material"

def test48_dict_scene_reference(variant_scalar_rgb):
    """Test that dictionary objects can be referenced by both their key and explicit ID"""
    # Ported from test09_dict_scene_reference in test_dict.py
    scene = mi.load_dict({
        "type": "scene",
        # reference using its key
        "bsdf1_key": {"type": "conductor"},
        # reference using its key or its id
        "bsdf2_key": {
            "type": "roughdielectric",
            "id": "bsdf2_id"
        },
        "texture1_id": {"type": "checkerboard"},

        "shape0": {
            "type": "sphere",
            "bsdf": {
                "type": "ref",
                "id": "bsdf1_key"
            }
        },

        "shape1": {
            "type": "sphere",
            "bsdf": {
                "type": "ref",
                "id": "bsdf2_id"
            }
        },

        "shape2": {
            "type": "sphere",
            "bsdf": {
                "type": "ref",
                "id": "bsdf2_key"
            }
        },

        "shape3": {
            "type": "sphere",
            "bsdf": {
                "type": "diffuse",
                "reflectance": {
                    "type": "ref",
                    "id": "texture1_id"
                }
            }
        },
    })

    # Create reference objects to compare against
    bsdf1 = mi.load_dict({"type": "conductor"})
    bsdf2 = mi.load_dict({
        "type": "roughdielectric",
        "id": "bsdf2_id"
    })
    texture = mi.load_dict({"type": "checkerboard"})
    bsdf3 = mi.load_dict({
        "type": "diffuse",
        "reflectance": texture
    })

    # Verify all shapes have the correct BSDFs
    assert str(scene.shapes()[0].bsdf()) == str(bsdf1)
    assert str(scene.shapes()[1].bsdf()) == str(bsdf2)
    assert str(scene.shapes()[2].bsdf()) == str(bsdf2)  # Both reference the same BSDF
    assert str(scene.shapes()[3].bsdf()) == str(bsdf3)

    # Test duplicate ID error
    with pytest.raises(RuntimeError, match="has duplicate ID"):
        mi.load_dict({
            "type": "scene",
            "shape0": {
                "type": "sphere",
                "id": "shape_id",
            },
            "shape1": {
                "type": "sphere",
                "id": "shape_id",
            },
        })

    # Test missing reference error
    with pytest.raises(RuntimeError, match='unresolved reference to "bsdf2_id"'):
        mi.load_dict({
            "type": "scene",
            "bsdf1_id": {"type": "conductor"},
            "shape1": {
                "type": "sphere",
                "bsdf": {
                    "type": "ref",
                    "id": "bsdf2_id"  # This ID doesn't exist
                }
            },
        })

def test49_dict_mitsuba_and_arbitrary_object_support(variant_scalar_rgb):
    """Test that Mitsuba objects and arbitrary nanobind-based objects can be passed in dictionaries"""

    # Mitsuba object
    value_obj = mi.load_dict({
        "type": "checkerboard",
        "color0": mi.Color3f(0, 0, 0),
        "color1": mi.Color3f(1, 1, 1)
    })

    # Arbitrary object
    value_any = mi.TensorXf([1, 2, 3, 4])

    dict_scene = {
        "type": "scene",
        "entry": {
            "type": "foo",
            "value_obj": value_obj,  # Mitsuba object -> Type.Object
            "value_any": value_any
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)
    props = state.nodes[1].props

    assert "value_obj" in props
    assert props.type("value_obj") == mi.Properties.Type.Object
    assert props["value_obj"] is value_obj

    assert "value_any" in props
    assert props.type("value_any") == mi.Properties.Type.Any
    assert props["value_any"] is value_any


def test50_transform_resolve(variant_scalar_rgb):
    """Test the transform_resolve pass"""

    xml_str = """<scene version="3.0.0">
        <bsdf type="diffuse" id="mat1">
            <rgb name="reflectance" value="0.8 0.8 0.8"/>
        </bsdf>
        <shape type="sphere">
            <ref id="mat1" name="bsdf"/>
        </shape>
    </scene>"""

    state = mi.parser.parse_string(config, xml_str)

    # Before resolution, the shape should have a Reference property
    shape_node = state.nodes[2]  # Root, bsdf, shape
    assert "bsdf" in shape_node.props
    assert shape_node.props.type("bsdf") == mi.Properties.Type.Reference

    # Apply reference resolution
    mi.parser.transform_resolve(config, state)

    # Re-fetch the node after transformation
    shape_node = state.nodes[2]

    # After resolution, it should be a ResolvedReference
    assert shape_node.props.type("bsdf") == mi.Properties.Type.ResolvedReference

    # Test with dictionaries
    dict_scene = {
        "type": "scene",
        "mat2": {
            "type": "diffuse",
            "id": "mat2",
            "reflectance": mi.Color3f(0.5, 0.5, 0.5)
        },
        "shape": {
            "type": "sphere",
            "bsdf": {"type": "ref", "id": "mat2"}
        }
    }

    state2 = mi.parser.parse_dict(config, dict_scene)

    # Before resolution
    shape_node2 = state2.nodes[2]  # Root, mat2, shape
    assert shape_node2.props.type("bsdf") == mi.Properties.Type.Reference

    # Apply resolution
    mi.parser.transform_resolve(config, state2)

    # Re-fetch the node after transformation
    shape_node2 = state2.nodes[2]
    assert shape_node2.props.type("bsdf") == mi.Properties.Type.ResolvedReference

    # Test error case - reference to non-existent object
    xml_bad = """<scene version="3.0.0">
        <shape type="sphere">
            <ref id="nonexistent" name="bsdf"/>
        </shape>
    </scene>"""

    state_bad = mi.parser.parse_string(config, xml_bad)
    with pytest.raises(RuntimeError, match='unresolved reference to "nonexistent"'):
        mi.parser.transform_resolve(config, state_bad)


@pytest.mark.parametrize("parallel", [False, True])
def test51_parallel_instantiation(parallel, variant_scalar_rgb):
    """Test single-threaded/parallel instantiation of 100 objects"""

    # Create a large scene with many objects and dependencies
    scene = { "type": "scene" }

    # Add 100 spheres with materials in a single loop
    for i in range(100):
        scene[f"shape_{i}"] = {
            "type": "sphere",
            "id": f"sphere_{i}",
            "center": [i * 0.1, 0, 0],  # Linearly increasing position
            "radius": 0.4,
            "bsdf": {
                "type": "diffuse",
                "id": f"mat_{i}",
                "reflectance": {
                    "type": "rgb",
                    "value": [i * 0.01, i * 0.01, i * 0.01]  # Linearly increasing intensity
                }
            }
        }

    # Add a sensor
    scene["sensor"] = {
        "type": "perspective",
        "id": "sensor",
        "fov": 45,
        "film": {
            "type": "hdrfilm",
            "width": 1024,
            "height": 768
        }
    }

    # Parse the scene
    state = mi.parser.parse_dict(config, scene)

    # Apply transformations
    mi.parser.transform_resolve(config, state)

    # Instantiate the scene with parallel flag
    config.parallel = parallel
    scene = mi.parser.instantiate(config, state)

    # Verify the result
    assert scene is not None
    assert isinstance(scene, mi.Scene)

    # Get scene parameters using traverse
    params = mi.traverse(scene)

    # Verify all 100 spheres
    for i in range(100):
        to_world = params[f"sphere_{i}.to_world"]
        # Extract the translation from the transform
        assert dr.allclose(to_world.translation(), mi.Point3f(i * 0.1, 0, 0))

        # Check material reflectance value
        expected_color = mi.Color3f(i * 0.01, i * 0.01, i * 0.01)
        reflectance_value = params[f"sphere_{i}.bsdf.reflectance.value"]
        assert dr.allclose(reflectance_value, expected_color, atol=1e-5)


def test52_unqueried_properties_error_formatting(variant_scalar_rgb):
    """Test that unqueried properties error messages are well-formatted"""
    xml_str = """<scene version="3.0.0">
        <shape type="sphere">
            <float name="radius" value="2.0"/>
            <string name="unused_string" value="hello"/>
            <integer name="unused_int" value="42"/>
            <boolean name="unused_bool" value="true"/>
            <vector name="unused_vector" x="1" y="2" z="3"/>
            <rgb name="unused_color" value="0.5 0.7 0.9"/>
            <transform name="unused_transform">
                <translate x="1" y="0" z="0"/>
            </transform>
        </shape>
    </scene>"""

    config = mi.parser.ParserConfig('scalar_rgb')
    state = mi.parser.parse_string(config, xml_str)

    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.instantiate(config, state)

    expected = '''While loading string (line 2, col 10): Found 6 unreferenced properties in shape plugin of type "sphere":
  - string "unused_string": "hello"
  - integer "unused_int": 42
  - boolean "unused_bool": true
  - vector "unused_vector": [1, 2, 3]
  - rgb "unused_color": [0.5, 0.7, 0.9]
  - transform "unused_transform": [[1, 0, 0, 1],
 [0, 1, 0, 0],
 [0, 0, 1, 0],
 [0, 0, 0, 1]]'''

    assert error_string(excinfo.value) == expected

    config.unused_properties = mi.LogLevel.Debug

    # Now, the following should work without complaints
    mi.parser.instantiate(config, state)


def test53_instantiation_failure_location_xml(variant_scalar_rgb):
    """Test that instantiation failures include proper file location information for XML"""

    # XML with invalid string radius that should cause instantiation to fail
    xml_str = """<scene version="3.0.0">
        <shape type="sphere">
            <string name="radius" value="invalid_radius"/>
        </shape>
    </scene>"""

    state = mi.parser.parse_string(config, xml_str)
    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.instantiate(config, state)

    expected = 'At string (line 2, col 10): failed to instantiate shape plugin of type "sphere": The property "radius" has the wrong type (expected float, got string)'
    assert error_string(excinfo.value) == expected


def test54_instantiation_failure_location_dict(variant_scalar_rgb):
    """Test that instantiation failures include proper node information for dictionaries"""

    # Dictionary with invalid string radius
    dict_scene = {
        "type": "scene",
        "myshape": {
            "type": "sphere",
            "radius": "invalid_radius"  # Should be float
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    # Try to instantiate - this should fail with node information
    with pytest.raises(RuntimeError) as excinfo:
        mi.parser.instantiate(config, state)

    error_msg = error_string(excinfo.value)

    # For dictionary parsing, we should get the hierarchical path with periods
    expected = 'At dictionary node "myshape": failed to instantiate shape plugin of type "sphere": The property "radius" has the wrong type (expected float, got string)'
    assert error_msg == expected


def test55_dict_auto_id_generation(variant_scalar_rgb):
    """Test automatic ID generation from dictionary keys"""

    # Test that objects get auto-generated IDs from dictionary keys with nested structure
    dict_scene = {
        "type": "scene",
        "my_shape": {
            "type": "sphere",
            "nested_emitter": {
                "type": "area",
                "radiance": {
                    "type": "rgb",
                    "value": [1.0, 0.5, 0.2]
                }
            }
        }
    }

    state = mi.parser.parse_dict(config, dict_scene)

    assert state.nodes[1].props.id() == "my_shape"
    assert state.nodes[2].props.id() == "nested_emitter"

    # Verify the nested nodes have the full path
    assert state.node_paths[1] == "my_shape"
    assert state.node_paths[2] == "my_shape.nested_emitter"

    # Test that explicit IDs take precedence
    dict_scene2 = {
        "type": "scene",
        "dict_key": {
            "type": "diffuse",
            "id": "explicit_id"  # This should override "dict_key"
        }
    }

    state2 = mi.parser.parse_dict(config, dict_scene2)
    assert state2.nodes[1].props.id() == "explicit_id"


def test56_transform_merge_equivalent(variant_scalar_rgb):
    """Test node deduplication with transform_merge_equivalent"""

    scene_dict = {
        "type": "scene",
        # normalmap1 and normalmap2 have identical nested diffuse materials
        # so they should be completely merged (both child and parent)
        "normalmap1": {
            "type": "normalmap",
            "id": "normalmap1",
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "rgb", "value": [0.8, 0.2, 0.2]}
            }
        },
        "normalmap2": {
            "type": "normalmap",
            "id": "normalmap2",
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "rgb", "value": [0.8, 0.2, 0.2]}
            }
        },
        # normalmap3 has a different nested diffuse material
        "normalmap3": {
            "type": "normalmap",
            "id": "normalmap3",
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "rgb", "value": [0.2, 0.8, 0.2]}
            }
        },
        # Shapes using the materials
        "sphere": {
            "type": "sphere",
            "bsdf": {"type": "ref", "id": "normalmap1"}
        },
        "cube": {
            "type": "cube",
            "bsdf": {"type": "ref", "id": "normalmap2"}
        },
        "rectangle": {
            "type": "rectangle",
            "bsdf": {"type": "ref", "id": "normalmap3"}
        }
    }

    state = mi.parser.parse_dict(config, scene_dict)
    mi.parser.transform_resolve(config, state)

    # Find the normalmap nodes and their nested diffuse children
    # normalmap1 is at index 1, its child is at index 2
    # normalmap2 is at index 3, its child is at index 4
    # normalmap3 is at index 5, its child is at index 6
    normalmap1_idx = 1
    normalmap2_idx = 3
    normalmap3_idx = 5

    # Get the child diffuse indices
    diffuse1_idx = state.nodes[normalmap1_idx].props["bsdf"].index()
    diffuse2_idx = state.nodes[normalmap2_idx].props["bsdf"].index()
    diffuse3_idx = state.nodes[normalmap3_idx].props["bsdf"].index()

    assert diffuse1_idx == 2
    assert diffuse2_idx == 4
    assert diffuse3_idx == 6

    # Before optimization, all are separate nodes
    assert normalmap1_idx != normalmap2_idx
    assert diffuse1_idx != diffuse2_idx

    # Apply optimization
    mi.parser.transform_merge_equivalent(config, state)

    # After optimization:
    # 1. The identical child diffuse materials should be merged
    diffuse1_idx_opt = state.nodes[normalmap1_idx].props["bsdf"].index()
    diffuse2_idx_opt = state.nodes[normalmap2_idx].props["bsdf"].index()
    diffuse3_idx_opt = state.nodes[normalmap3_idx].props["bsdf"].index()

    assert diffuse1_idx_opt == diffuse2_idx_opt  # Child diffuse materials merged!
    assert diffuse1_idx_opt != diffuse3_idx_opt  # Different properties, not merged

    # 2. The normalmap BSDFs should also be merged since they now reference the same child
    sphere = state.nodes[7]
    cube = state.nodes[8]
    rect = state.nodes[9]

    normalmap1_idx_opt = sphere.props["bsdf"].index()
    normalmap2_idx_opt = cube.props["bsdf"].index()
    normalmap3_idx_opt = rect.props["bsdf"].index()

    assert normalmap1_idx_opt == normalmap2_idx_opt  # Parent normalmap materials merged!
    assert normalmap1_idx_opt != normalmap3_idx_opt  # Different child, not merged


def test57_dict_expand(variant_scalar_spectral):
    """Test that instantiation expands objects"""

    spectrum = mi.load_dict({"type" : "d65"})
    assert spectrum.class_name() == "RegularSpectrum"


def test58_transform_merge_meshes(variant_scalar_rgb):
    """Test transform_merge_meshes wraps shapes into a merge shape"""

    # Create simple scene with two shapes
    scene_dict = {
        "type": "scene",
        "shape1": {
            "type": "sphere",
            "radius": 1.0
        },
        "shape2": {
            "type": "cube"
        }
    }

    state = mi.parser.parse_dict(config, scene_dict)
    mi.parser.transform_resolve(config, state)

    # Before transform: root has 2 shape references
    assert len(state.nodes) == 3  # root + 2 shapes
    root_children = get_children(state, state.root)
    assert len(root_children) == 2
    assert root_children[0][0] == "shape1"
    assert root_children[1][0] == "shape2"

    # Apply transform_merge_meshes
    mi.parser.transform_merge_meshes(config, state)

    # After transform: root has 1 merge shape reference
    assert len(state.nodes) == 4  # root + 2 shapes + merge shape
    root_children_after = get_children(state, state.root)
    assert len(root_children_after) == 1
    assert root_children_after[0][0] == "_arg_0"

    # Verify the merge shape contains the original shapes
    merge_shape = root_children_after[0][1]
    assert merge_shape.type == mi.ObjectType.Shape
    assert merge_shape.props.plugin_name() == "merge"

    merge_children = get_children(state, merge_shape)
    assert len(merge_children) == 2
    assert merge_children[0][0] == "shape1"
    assert merge_children[1][0] == "shape2"


def test59_uoffset_voffset_uscale_vscale_upgrade(variant_scalar_rgb):
    """Test that uoffset/voffset/uscale/vscale from version 1.x are converted to transforms"""

    # Test with all four properties
    xml = '''<scene version="1.9.0">
        <texture type="bitmap">
            <float name="uscale" value="2.0"/>
            <float name="vscale" value="4.0"/>
            <float name="uoffset" value="0.5"/>
            <float name="voffset" value="-0.25"/>
            <string name="filename" value="texture.jpg"/>
        </texture>
    </scene>'''

    state = mi.parser.parse_string(config, xml)
    mi.parser.transform_upgrade(config, state)

    texture_node = state.nodes[1]

    # Check that old properties are removed
    assert "uoffset" not in texture_node.props
    assert "voffset" not in texture_node.props
    assert "uscale" not in texture_node.props
    assert "vscale" not in texture_node.props

    # Check that to_uv transform was created
    assert "to_uv" in texture_node.props
    transform = texture_node.props["to_uv"]
    assert isinstance(transform, mi.Transform4d)

    # The transform should be scale(2,4,1) * translate(0.5,-0.25,0)
    expected = mi.Transform4d().scale([2.0, 4.0, 1.0]) @ mi.Transform4d().translate([0.5, -0.25, 0.0])
    assert dr.allclose(transform.matrix, expected.matrix)


def test59_alias_support(variant_scalar_rgb):
    """Test alias tag functionality"""

    # Create a scene with an object and an alias to it
    xml = '''<scene version="3.0.0">
        <bsdf type="diffuse" id="mat_original">
            <rgb name="reflectance" value="0.8 0.2 0.2"/>
        </bsdf>

        <!-- Create an alias -->
        <alias id="mat_original" as="mat_alias"/>

        <!-- Use both the original and the alias -->
        <shape type="sphere">
            <ref id="mat_original" name="bsdf"/>
        </shape>

        <shape type="cube">
            <ref id="mat_alias" name="bsdf"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml)

    # Check that both IDs exist in id_to_index
    assert "mat_original" in state.id_to_index
    assert "mat_alias" in state.id_to_index

    # Both should point to the same node index
    assert state.id_to_index["mat_original"] == state.id_to_index["mat_alias"]

    # Apply reference resolution
    mi.parser.transform_resolve(config, state)

    # Both shapes should reference the same BSDF node
    sphere = state.nodes[2]  # Root, bsdf, sphere
    cube = state.nodes[3]    # Root, bsdf, sphere, cube

    assert sphere.props["bsdf"].index() == cube.props["bsdf"].index()

    # Test error case - alias to non-existent ID
    xml_bad1 = '''<scene version="3.0.0">
        <alias id="nonexistent" as="mat_alias"/>
    </scene>'''

    with pytest.raises(RuntimeError, match='alias source ID "nonexistent" not found'):
        mi.parser.parse_string(config, xml_bad1)

    # Test error case - alias destination already exists
    xml_bad2 = '''<scene version="3.0.0">
        <bsdf type="diffuse" id="mat1"/>
        <bsdf type="diffuse" id="mat2"/>
        <alias id="mat1" as="mat2"/>
    </scene>'''

    with pytest.raises(RuntimeError, match='alias destination ID "mat2" already exists'):
        mi.parser.parse_string(config, xml_bad2)


def test60_default_parameter_values(variant_scalar_rgb):
    """Test default parameter value functionality"""

    xml = '''<scene version="3.0.0">
        <default name="radius" value="2.5"/>
        <shape type="sphere">
            <float name="radius" value="$radius"/>
        </shape>
    </scene>'''

    # Test 1: Use default value when parameter is not provided
    state1 = mi.parser.parse_string(config, xml)
    assert state1.nodes[1].props["radius"] == 2.5

    # Test 2: Explicit parameter overrides default
    state2 = mi.parser.parse_string(config, xml, radius="5.0")
    assert state2.nodes[1].props["radius"] == 5.0

    # Test 3: Empty parameter name should fail
    xml_bad = '''<scene version="3.0.0">
        <default name="" value="test"/>
    </scene>'''

    with pytest.raises(RuntimeError, match="<default>: name must be non-empty"):
        mi.parser.parse_string(config, xml_bad)

    # Test 4: Unused default parameters are acceptable
    xml_unused = '''<scene version="3.0.0">
        <default name="unused_param" value="123"/>
    </scene>'''
    mi.parser.parse_string(config, xml_unused)


def test61_circular_references(variant_scalar_rgb):
    """Test detection of circular references"""

    # Test case 1: Two shapes that reference each other
    xml_circular = '''<scene version="3.0.0">
        <shape type="sphere" id="shape1">
            <ref id="shape2" name="child"/>
        </shape>

        <shape type="cube" id="shape2">
            <ref id="shape1" name="child"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml_circular)
    with pytest.raises(RuntimeError, match="circular reference detected: shape1 -> shape2 -> shape1"):
        mi.parser.transform_resolve(config, state)

    # Test case 2: Self reference
    xml_self_ref = '''<scene version="3.0.0">
        <shape type="sphere" id="shape1">
            <ref id="shape1" name="child"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml_self_ref)
    with pytest.raises(RuntimeError, match="circular reference detected: shape1 -> shape1"):
        mi.parser.transform_resolve(config, state)

    # Test case 3: Longer cycle (A -> B -> C -> A)
    xml_long_cycle = '''<scene version="3.0.0">
        <shape type="sphere" id="A">
            <ref id="B" name="child"/>
        </shape>

        <shape type="cube" id="B">
            <ref id="C" name="child"/>
        </shape>

        <shape type="cube" id="C">
            <ref id="A" name="child"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml_long_cycle)
    with pytest.raises(RuntimeError, match="circular reference detected: A -> B -> C -> A"):
        mi.parser.transform_resolve(config, state)


@fresolver_append_path
def test62_transform_relocate(variant_scalar_rgb, tmp_path):
    """Test the transform_relocate functionality for organizing scene files."""

    # Create temporary source files with fake content
    source_dir = tmp_path / "source"
    source_dir.mkdir()

    # Create fake texture files
    texture1 = source_dir / "wood.jpg"
    texture2 = source_dir / "metal.png"
    texture1.write_text("fake texture content")
    texture2.write_text("fake texture content 2")

    # Create fake mesh files
    mesh1 = source_dir / "bunny.ply"
    mesh2 = source_dir / "teapot.obj"
    mesh1.write_text("fake mesh content")
    mesh2.write_text("fake mesh content 2")

    # Create duplicate filename to test conflict resolution
    duplicate_mesh = source_dir / "bunny.ply"
    # Note: This will overwrite above, so let's use a different approach
    duplicate_dir = source_dir / "subdir"
    duplicate_dir.mkdir()
    duplicate_mesh2 = duplicate_dir / "bunny.ply"
    duplicate_mesh2.write_text("different bunny content")

    # Create output directory where files will be relocated
    output_dir = tmp_path / "output"
    output_dir.mkdir()

    # Create scene with file paths
    scene_xml = f'''
    <scene version="2.1.0">
        <texture type="bitmap" name="wood_tex">
            <string name="filename" value="{xml_escape(texture1)}"/>
        </texture>

        <texture type="bitmap" name="metal_tex">
            <string name="filename" value="{xml_escape(texture2)}"/>
        </texture>

        <shape type="ply" name="bunny_shape">
            <string name="filename" value="{xml_escape(mesh1)}"/>
        </shape>

        <shape type="obj" name="teapot_shape">
            <string name="filename" value="{xml_escape(mesh2)}"/>
        </shape>

        <!-- Test string properties that are file paths -->
        <emitter type="envmap" name="env">
            <string name="filename" value="{xml_escape(texture1)}"/>
        </emitter>

        <!-- Test duplicate filename from different directory -->
        <shape type="ply" name="bunny2_shape">
            <string name="filename" value="{xml_escape(duplicate_mesh2)}"/>
        </shape>
    </scene>
    '''

    # Parse the scene
    config = mi.parser.ParserConfig("scalar_rgb")
    state = mi.parser.parse_string(config, scene_xml)

    # Apply standard transformations first
    mi.parser.transform_upgrade(config, state)
    mi.parser.transform_resolve(config, state)

    # Verify files exist before relocation
    assert texture1.exists()
    assert texture2.exists()
    assert mesh1.exists()
    assert mesh2.exists()
    assert duplicate_mesh2.exists()

    # Apply file relocation transformation
    mi.parser.transform_relocate(config, state, mi.filesystem.path(str(output_dir)))

    # Check that subdirectories were created
    textures_dir = output_dir / "textures"
    meshes_dir = output_dir / "meshes"

    assert textures_dir.exists()
    assert meshes_dir.exists()

    # Check that files were copied to correct directories
    assert (textures_dir / "wood.jpg").exists()
    assert (textures_dir / "metal.png").exists()
    assert (meshes_dir / "bunny.ply").exists()
    assert (meshes_dir / "teapot.obj").exists()

    # Check that duplicate filename was handled (should have (1) suffix)
    assert (meshes_dir / "bunny (1).ply").exists()

    # Verify content was copied correctly
    assert (textures_dir / "wood.jpg").read_text() == "fake texture content"
    assert (textures_dir / "metal.png").read_text() == "fake texture content 2"
    assert (meshes_dir / "bunny.ply").read_text() == "fake mesh content"
    assert (meshes_dir / "teapot.obj").read_text() == "fake mesh content 2"
    assert (meshes_dir / "bunny (1).ply").read_text() == "different bunny content"

    # Verify that properties were updated with relative paths
    # Collect all filename properties that should have been updated
    updated_filenames = []
    for node in state.nodes:
        if "filename" in node.props:
            updated_filenames.append(node.props.get("filename"))

    # Should have 6 filename properties total
    assert len(updated_filenames) == 6

    # Check that all paths were updated to relative paths
    expected_paths = {
        "textures/wood.jpg",    # wood texture (appears twice - texture and emitter)
        "textures/metal.png",   # metal texture
        "meshes/bunny.ply",     # bunny mesh
        "meshes/teapot.obj",    # teapot mesh
        "meshes/bunny (1).ply"   # duplicate bunny mesh
    }

    # Verify all expected relative paths are present
    for path in expected_paths:
        assert path in updated_filenames, f"Expected path {path} not found in {updated_filenames}"

    # Verify that wood.jpg appears twice (texture and emitter should both reference it)
    assert updated_filenames.count("textures/wood.jpg") == 2

    # Create a separate mesh file for the second test to avoid conflicts
    mesh3 = source_dir / "sphere.ply"
    mesh3.write_text("sphere mesh content")

    # Test that non-existent files are ignored
    scene_xml_nonexistent = '''
    <scene version="2.1.0">
        <texture type="bitmap" name="missing_tex">
            <string name="filename" value="/nonexistent/path/texture.jpg"/>
        </texture>
        <shape type="ply" name="existing_shape">
            <string name="filename" value="''' + xml_escape(str(mesh3)) + '''"/>
        </shape>
    </scene>
    '''

    # Parse scene with non-existent file
    state2 = mi.parser.parse_string(config, scene_xml_nonexistent)
    mi.parser.transform_upgrade(config, state2)
    mi.parser.transform_resolve(config, state2)

    # Clear output directory for clean test
    import shutil
    shutil.rmtree(output_dir)
    output_dir.mkdir()

    # Apply relocation - should not fail, just skip non-existent files
    mi.parser.transform_relocate(config, state2, mi.filesystem.path(str(output_dir)))

    # Check that only existing files were processed
    meshes_dir = output_dir / "meshes"
    assert meshes_dir.exists()
    assert (meshes_dir / "sphere.ply").exists()
    assert not (output_dir / "textures").exists()  # No textures copied due to non-existent file

    # Verify that only the existing file was relocated
    relocated_filenames = []
    for node in state2.nodes:
        if "filename" in node.props:
            relocated_filenames.append(node.props.get("filename"))

    # Should have 2 filename properties: one unchanged (non-existent), one updated
    assert len(relocated_filenames) == 2
    assert "/nonexistent/path/texture.jpg" in relocated_filenames  # Unchanged
    assert "meshes/sphere.ply" in relocated_filenames  # Updated


def test63_resource_path_management(variant_scalar_rgb, tmp_path):
    """Test resource path management (<path> tag)"""
    import shutil
    from mitsuba.test.util import find_resource

    # Create a data subdirectory in temp path
    data_dir = tmp_path / 'data'
    data_dir.mkdir()

    # Copy carrot.png to data/image_file.png
    carrot_src = mi.file_resolver().resolve(find_resource("resources/data/common/textures/carrot.png"))
    image_file = data_dir / 'image_file.png'
    shutil.copy(str(carrot_src), str(image_file))

    # Create a scene XML file
    scene_file = tmp_path / 'scene.xml'

    # Test 1: Without <path> tag, image_file.png should not be found
    xml_without_path = '''<scene version="3.0.0">
        <texture type="bitmap" id="tex">
            <string name="filename" value="image_file.png"/>
        </texture>
    </scene>'''

    scene_file.write_text(xml_without_path)

    # Parse the file first
    state = mi.parser.parse_file(config, str(scene_file))
    mi.parser.transform_all(config, state)

    # This should fail because image_file.png can't be found during instantiation
    with pytest.raises(RuntimeError) as excinfo:
        scene = mi.parser.instantiate(config, state)
    assert "No such file or directory" in str(excinfo.value) or " does not exist!" in str(excinfo.value)

    # Test 2: With <path> tag pointing to data directory
    xml_with_path = '''<scene version="3.0.0">
        <path value="data"/>
        <texture type="bitmap" id="tex">
            <string name="filename" value="image_file.png"/>
        </texture>
    </scene>'''

    scene_file.write_text(xml_with_path)

    # This should succeed because <path> adds data/ to search paths
    state = mi.parser.parse_file(config, str(scene_file))

    # Verify the texture node exists
    assert len(state.nodes) == 2  # root + texture
    assert state.nodes[1].props.plugin_name() == "bitmap"
    assert state.nodes[1].props["filename"] == "image_file.png"

    # Test 3: Multiple <path> tags
    # Create another directory with a different file
    data2_dir = tmp_path / 'data2'
    data2_dir.mkdir()
    image_file2 = data2_dir / 'other_image.png'
    shutil.copy(str(carrot_src), str(image_file2))

    xml_multiple_paths = '''<scene version="3.0.0">
        <path value="data"/>
        <path value="data2"/>
        <texture type="bitmap" id="tex1">
            <string name="filename" value="image_file.png"/>
        </texture>
        <texture type="bitmap" id="tex2">
            <string name="filename" value="other_image.png"/>
        </texture>
    </scene>'''

    scene_file.write_text(xml_multiple_paths)

    # This should succeed - both files should be found
    state = mi.parser.parse_file(config, str(scene_file))
    assert len(state.nodes) == 3  # root + 2 textures

    # Test 4: <path> must be direct child of root
    xml_nested_path = '''<scene version="3.0.0">
        <shape type="sphere">
            <path value="data"/>
        </shape>
    </scene>'''

    scene_file.write_text(xml_nested_path)

    with pytest.raises(RuntimeError, match="<path>: path can only be child of root"):
        mi.parser.parse_file(config, str(scene_file))

    # Test 5: <path> with non-existent directory
    xml_bad_path = '''<scene version="3.0.0">
        <path value="nonexistent_directory"/>
    </scene>'''

    scene_file.write_text(xml_bad_path)

    with pytest.raises(RuntimeError, match='<path>: folder ".*nonexistent_directory" not found'):
        mi.parser.parse_file(config, str(scene_file))


def test63_transform_reorder(variant_scalar_rgb):
    """Test reordering of scene elements by type"""

    # Create a comprehensive test scene with all object types
    # The ordering within each type group is now stable (preserves insertion order)
    xml = '''<scene version="3.0.0">
        <!-- Mixed order of elements -->
        <shape type="rectangle"/>
        <emitter type="point"/>
        <sensor type="perspective"/>
        <bsdf type="diffuse" id="mat1"/>
        <integrator type="path"/>
        <texture type="bitmap" id="tex1"/>
        <shape type="sphere">
            <emitter type="area"/>
        </shape>
        <medium type="homogeneous" id="med1"/>
        <emitter type="envmap"/>
        <shape type="cube"/>
        <volume type="constvolume" id="vol1"/>
        <bsdf type="conductor" id="mat2"/>
        <shape type="cylinder">
            <bsdf type="diffuse"/>
            <medium type="homogeneous"/>
        </shape>
        <texture type="checkerboard" id="tex2"/>
    </scene>'''

    # Create config with merge_meshes disabled for this test
    config = mi.parser.ParserConfig('scalar_rgb')
    config.merge_meshes = False

    state = mi.parser.parse_string(config, xml)
    mi.parser.transform_all(config, state)
    mi.parser.transform_reorder(config, state)

    # Check the reordered elements
    root_children = get_children(state, state.root)

    # Extract types and plugin names for verification
    types_and_names = [(child.type, child.props.plugin_name()) for _, child in root_children]

    # Expected order preserves insertion order within each priority group:
    # Priority 0: Integrator (path)
    # Priority 1: Sensor (perspective)
    # Priority 2: Textures in insertion order (bitmap, checkerboard)
    # Priority 3: BSDFs in insertion order (diffuse, conductor)
    # Priority 4: Emitters and area light shapes in insertion order (point, sphere, envmap)
    # Priority 5: Regular shapes in insertion order (rectangle, cube, cylinder)
    # Priority 6: Volumes (constvolume)
    # Priority 7: Media (homogeneous)

    expected_types_and_names = [
        (mi.ObjectType.Integrator, "path"),
        (mi.ObjectType.Sensor, "perspective"),
        (mi.ObjectType.Texture, "bitmap"),
        (mi.ObjectType.Texture, "checkerboard"),
        (mi.ObjectType.BSDF, "diffuse"),
        (mi.ObjectType.BSDF, "conductor"),
        (mi.ObjectType.Emitter, "point"),
        (mi.ObjectType.Shape, "sphere"),      # Has area light, classified with emitters
        (mi.ObjectType.Emitter, "envmap"),
        (mi.ObjectType.Shape, "rectangle"),
        (mi.ObjectType.Shape, "cube"),
        (mi.ObjectType.Shape, "cylinder"),
        (mi.ObjectType.Volume, "constvolume"),
        (mi.ObjectType.Medium, "homogeneous"),
    ]

    assert types_and_names == expected_types_and_names

    # Verify that the sphere has an area light
    sphere = next(child for _, child in root_children if child.props.plugin_name() == "sphere")
    sphere_children = get_children(state, sphere)
    assert len(sphere_children) == 1
    assert sphere_children[0][1].props.plugin_name() == "area"

def test64_logger_deadlock():
    """Test that logging does not cause a deadlock when loading a scene in parallel"""
    log_level = dr.log_level()
    dr.set_log_level(dr.LogLevel.Debug)
    scene = mi.load_dict(mi.cornell_box(), parallel=True)
    dr.set_log_level(log_level)

def test65_tensorxf_lookup():
    """Test that TensorXf values can be correctly passed to plugins"""
    class DummyEmitter(mi.Emitter):
        def __init__(self, props) -> None:
            super().__init__(props)
            self.foo = props.get('foo')
            assert type(self.foo) is mi.TensorXf
            print(self.foo)

    mi.register_emitter('dummy_emitter', lambda props: DummyEmitter(props))

    obj1 = mi.load_dict({
        'type': 'dummy_emitter',
        'foo': dr.zeros(mi.TensorXf, shape=(4, 4, 4))
    })


def test66_escaped_dollar_sign(variant_scalar_rgb):
    """Test that escaped dollar signs (\\$) are preserved as literal $"""

    # Test escaped dollar sign
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <string name="price" value="Cost: \\$100"/>
            <string name="mixed" value="\\$50 for $count items"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(config, xml, count="10")
    shape = state.nodes[1]

    # Escaped \$ should become $
    assert shape.props["price"] == "Cost: $100"
    # Mix of escaped and parameter substitution
    assert shape.props["mixed"] == "$50 for 10 items"
