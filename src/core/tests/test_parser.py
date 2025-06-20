import pytest
import mitsuba as mi
import drjit as dr


def test01_basic_xml_parsing(variant_scalar_rgb):
    """Test parsing a simple XML snippet without instantiation"""
    xml = '<phase type="foo" version="1.2.3"/>'

    state = mi.parser.parse_string(xml)

    # Check basic structure
    assert len(state.nodes) == 1
    assert state.root.props.plugin_name() == "foo"
    assert state.root.type == mi.ObjectType.PhaseFunction

    # Version should be parsed
    assert state.version.major_version == 1
    assert state.version.minor_version == 2
    assert state.version.patch_version == 3


def test02_basic_dict_parsing(variant_scalar_rgb):
    """Test parsing a simple dictionary without instantiation"""
    dict_scene = {
        "type": "scene",
        "mysensor": {
            "type": "perspective"
        }
    }

    state = mi.parser.parse_dict(dict_scene)

    # Check basic structure
    assert len(state.nodes) == 2  # root + mysensor
    assert state.root.props.plugin_name() == "scene"
    assert state.root.props.id() == "root"

    # Check child node
    assert len(state.root.children) == 1
    child_idx = state.root.children[0]
    assert child_idx == 1
    child = state.nodes[child_idx]
    assert child.props.plugin_name() == "perspective"
    assert child.property_name == "mysensor"


def test03_error_invalid_xml(variant_scalar_rgb):
    """Test error handling for invalid XML"""
    xml = '<phase type="foo" version="invalid"/>'

    with pytest.raises(RuntimeError, match="Invalid version string"):
        mi.parser.parse_string(xml)


def test04_error_missing_type_dict(variant_scalar_rgb):
    """Test error handling for dictionary without type"""
    dict_scene = {
        "mysensor": {
            "fov": 45
        }
    }

    with pytest.raises(RuntimeError, match="missing 'type' attribute"):
        mi.parser.parse_dict(dict_scene)


def test05_parameter_substitution(variant_scalar_rgb):
    """Test parameter substitution in XML"""
    xml = '<phase type="$mytype" version="3.0.0"/>'

    state = mi.parser.parse_string(xml, mytype="isotropic")

    # Check that parameter substitution worked
    assert len(state.nodes) == 1
    assert state.root.props.plugin_name() == "isotropic"

def test06_float_attributes_xml(variant_scalar_rgb):
    """Test parsing float attributes in XML"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere" id="mysphere">
            <float name="radius" value="2.5"/>
            <float name="bar" value="-3.14"/>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)

    # Should have root + shape nodes
    assert len(state.nodes) == 2

    # Check shape node
    shape_node = state.nodes[1]
    assert shape_node.props.plugin_name() == "sphere"
    assert shape_node.props.id() == "mysphere"

    # Check float properties
    assert "radius" in shape_node.props
    assert dr.allclose(shape_node.props.get("radius"), 2.5)
    assert "bar" in shape_node.props
    assert dr.allclose(shape_node.props.get("bar"), -3.14)

    # Test invalid float parsing
    xml_invalid = '''<scene version="3.0.0">
        <shape type="sphere">
            <float name="radius" value="not_a_number"/>
        </shape>
    </scene>'''
    
    with pytest.raises(RuntimeError, match="Could not parse floating point value"):
        mi.parser.parse_string(xml_invalid)


def test07_float_attributes_dict(variant_scalar_rgb):
    """Test parsing float attributes in dictionary"""
    dict_scene = {
        "type": "scene",
        "myshape": {
            "type": "sphere",
            "radius": 2.5,
            "bar": -3.14
        }
    }

    state = mi.parser.parse_dict(dict_scene)

    # Should have root + shape nodes
    assert len(state.nodes) == 2

    # Check shape node
    shape_node = state.nodes[1]
    assert shape_node.props.plugin_name() == "sphere"
    assert shape_node.property_name == "myshape"

    # Check float properties in the shape node
    assert "radius" in shape_node.props
    assert dr.allclose(shape_node.props.get("radius"), 2.5)
    assert "bar" in shape_node.props
    assert dr.allclose(shape_node.props.get("bar"), -3.14)


def test08_missing_float_attributes_xml(variant_scalar_rgb):
    """Test error handling for missing float attributes in XML"""
    # Missing value attribute
    xml1 = '''<scene version="3.0.0">
        <shape type="sphere">
            <float name="radius"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Missing required attribute 'value'"):
        mi.parser.parse_string(xml1)

    # Missing name attribute
    xml2 = '''<scene version="3.0.0">
        <shape type="sphere">
            <float value="1.0"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Missing required attribute 'name'"):
        mi.parser.parse_string(xml2)


def test09_error_location_reporting(variant_scalar_rgb):
    """Test that error messages include line:column information"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <float name="radius" value="invalid"/>
        </shape>
    </scene>'''

    try:
        mi.parser.parse_string(xml)
        assert False, "Should have raised an exception"
    except RuntimeError as e:
        error_msg = str(e)
        # Should contain line:column information
        assert "line" in error_msg and "col" in error_msg
        # Should mention the invalid value
        assert "invalid" in error_msg


def test10_integer_string_vector_properties_xml(variant_scalar_rgb):
    """Test parsing integer, string, vector, point, and RGB properties from XML"""
    
    xml = '''<scene version="3.0.0">
        <shape type="sphere" id="mysphere">
            <integer name="samples" value="16"/>
            <string name="filename" value="mesh.obj"/>
            <vector name="direction" value="1 0 0"/>
            <point name="position" value="0 2 -1"/>
            <rgb name="color" value="0.5 0.7 0.9"/>
            <vector name="up" x="0" y="1" z="0"/>
        </shape>
    </scene>'''
    
    state = mi.parser.parse_string(xml)
    assert len(state.nodes) == 2
    
    shape = state.nodes[1]
    
    # Check integer property
    assert "samples" in shape.props
    assert shape.props.get("samples") == 16
    
    # Check string property
    assert "filename" in shape.props  
    assert shape.props.get("filename") == "mesh.obj"
    
    # Check vector properties
    assert "direction" in shape.props
    direction = shape.props.get("direction")
    assert dr.allclose(direction, [1, 0, 0])
    
    assert "up" in shape.props
    up = shape.props.get("up")
    assert dr.allclose(up, [0, 1, 0])
    
    # Check point property
    assert "position" in shape.props
    position = shape.props.get("position")
    assert dr.allclose(position, [0, 2, -1])
    
    # Check RGB property
    assert "color" in shape.props
    color = shape.props.get("color")
    assert dr.allclose(color, [0.5, 0.7, 0.9])


def test11_integer_string_vector_properties_dict(variant_scalar_rgb):
    """Test parsing integer, string, vector properties from dictionary"""
    
    dict_scene = {
        "type": "scene",
        "myshape": {
            "type": "sphere",
            "samples": 16,
            "filename": "mesh.obj",
            "direction": [1, 0, 0],
            "position": [0, 2, -1],
            "color": [0.5, 0.7, 0.9]
        }
    }
    
    state = mi.parser.parse_dict(dict_scene)
    assert len(state.nodes) == 2
    
    shape = state.nodes[1]
    
    # Check all properties match
    assert shape.props.get("samples") == 16
    assert shape.props.get("filename") == "mesh.obj"
    assert dr.allclose(shape.props.get("direction"), [1, 0, 0])
    assert dr.allclose(shape.props.get("position"), [0, 2, -1])
    assert dr.allclose(shape.props.get("color"), [0.5, 0.7, 0.9])


def test12_parameter_substitution_comprehensive(variant_scalar_rgb):
    """Test comprehensive parameter substitution scenarios"""
    # Test multiple parameters in different attributes
    xml = '''<scene version="3.0.0">
        <shape type="$shapetype" id="$shapeid">
            <float name="radius" value="$radius"/>
            <string name="material" value="material_$material_name"/>
        </shape>
    </scene>'''
    
    state = mi.parser.parse_string(xml, 
                                  shapetype="sphere",
                                  shapeid="mysphere",
                                  radius="2.5",
                                  material_name="gold")
    
    assert len(state.nodes) == 2
    shape_node = state.nodes[1]
    assert shape_node.props.plugin_name() == "sphere"
    assert shape_node.props.id() == "mysphere"
    assert "radius" in shape_node.props
    assert dr.allclose(shape_node.props.get("radius"), 2.5)
    assert shape_node.props.get("material") == "material_gold"
    
    # Test undefined parameter error
    xml2 = '<phase type="$undefined_param"/>'
    with pytest.raises(RuntimeError, match="Undefined parameter: \\$undefined_param"):
        mi.parser.parse_string(xml2)
    
    # Test parameter with multiple occurrences in same value
    xml3 = '<shape type="sphere" id="$prefix_$suffix"/>'
    state3 = mi.parser.parse_string(xml3, prefix="start", suffix="end")
    assert state3.root.props.id() == "start_end"


def test13_parameter_substitution_warnings(variant_scalar_rgb, caplog):
    """Test warnings for unused parameters"""
    xml = '<phase type="isotropic"/>'
    
    # This should generate a warning for unused parameter
    state = mi.parser.parse_string(xml, unused_param="value")
    
    # Check that a warning was logged
    assert "Unused parameter: 'unused_param'" in caplog.text


def test14_file_location(variant_scalar_rgb, tmp_path):
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
    temp_file.write_text(xml_content)

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

    # Must assign entire vectors
    state.nodes = [node1, node2, node3]
    state.files = [str(temp_file)]

    # Check line:column reporting
    location1 = mi.parser.file_location(state, node1)
    assert "line 1, col 1" in location1
    assert str(temp_file) in location1

    location2 = mi.parser.file_location(state, node2)
    assert "line 2, col 5" in location2

    location3 = mi.parser.file_location(state, node3)
    assert "line 3, col 6" in location3

    # Test node without file info (file_index out of bounds)
    node4 = mi.parser.SceneNode()
    node4.file_index = 999  # Out of bounds
    node4.props.set_id("no_file_info")
    state.nodes = [node1, node2, node3, node4]

    location4 = mi.parser.file_location(state, node4)
    assert "no_file_info" in location4
    assert "line" not in location4


