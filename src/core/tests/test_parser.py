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
    assert len(state.versions) == 1
    assert state.versions[0].major_version == 1
    assert state.versions[0].minor_version == 2
    assert state.versions[0].patch_version == 3


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

    # Check child node
    assert len(state.root.children) == 1
    child_name, child_idx = state.root.children[0]
    assert child_name == "mysensor"
    assert child_idx == 1
    child = state.nodes[child_idx]
    assert child.props.plugin_name() == "perspective"
    # Property name is already checked above (child_name == "mysensor")


def test03_error_invalid_xml(variant_scalar_rgb):
    """Test error handling for invalid XML"""
    xml = '<phase type="foo" version="invalid"/>'

    with pytest.raises(RuntimeError, match="Invalid version number"):
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
    assert dr.allclose(shape_node.props["radius"], 2.5)
    assert "bar" in shape_node.props
    assert dr.allclose(shape_node.props["bar"], -3.14)

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
    shape_name, shape_idx = state.root.children[0]
    shape_node = state.nodes[shape_idx]
    assert shape_node.props.plugin_name() == "sphere"
    assert shape_name == "myshape"

    # Check float properties in the shape node
    assert "radius" in shape_node.props
    assert dr.allclose(shape_node.props["radius"], 2.5)
    assert "bar" in shape_node.props
    assert dr.allclose(shape_node.props["bar"], -3.14)


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
    assert shape.props["samples"] == 16

    # Check string property
    assert "filename" in shape.props
    assert shape.props["filename"] == "mesh.obj"

    # Check vector properties
    assert "direction" in shape.props
    direction = shape.props["direction"]
    assert dr.allclose(direction, [1, 0, 0])

    assert "up" in shape.props
    up = shape.props["up"]
    assert dr.allclose(up, [0, 1, 0])

    # Check point property
    assert "position" in shape.props
    position = shape.props["position"]
    assert dr.allclose(position, [0, 2, -1])

    # Check RGB property
    assert "color" in shape.props
    color = shape.props["color"]
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
    assert shape.props["samples"] == 16
    assert shape.props["filename"] == "mesh.obj"
    assert dr.allclose(shape.props["direction"], [1, 0, 0])
    assert dr.allclose(shape.props["position"], [0, 2, -1])
    assert dr.allclose(shape.props["color"], [0.5, 0.7, 0.9])


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
    assert dr.allclose(shape_node.props["radius"], 2.5)
    assert shape_node.props["material"] == "material_gold"

    # Test undefined parameter error
    xml2 = '<phase type="$undefined_param" version="3.0.0"/>'
    with pytest.raises(RuntimeError, match="Undefined parameter: \\$undefined_param"):
        mi.parser.parse_string(xml2)

    # Test parameter with multiple occurrences in same value
    xml3 = '<shape type="sphere" id="$prefix_$suffix" version="3.0.0"/>'
    state3 = mi.parser.parse_string(xml3, prefix="start", suffix="end")
    assert state3.root.props.id() == "start_end"


def test13_parameter_substitution_warnings(variant_scalar_rgb):
    """Test unused parameter handling with different log levels"""
    xml = '<phase type="isotropic" version="3.0.0"/>'

    # By default (Error level), unused parameters should throw an exception
    with pytest.raises(RuntimeError, match="Unused parameter"):
        mi.parser.parse_string(xml, unused_param="value")

    # Explicitly test that Error level throws an exception
    config = mi.parser.ParserConfig()
    config.unused_parameters = mi.LogLevel.Error
    with pytest.raises(RuntimeError, match="Unused parameter"):
        mi.parser.parse_string(xml, config, unused_param="value")

    # With config.unused_parameters=Warn, it should generate a warning instead of throwing
    config.unused_parameters = mi.LogLevel.Warn
    # This should not throw an exception
    state = mi.parser.parse_string(xml, config, unused_param="value")
    assert len(state.nodes) == 1  # Verify parsing succeeded

    # With config.unused_parameters=Debug, it should log at debug level
    config.unused_parameters = mi.LogLevel.Debug
    # This should also not throw an exception
    state = mi.parser.parse_string(xml, config, unused_param="value")
    assert len(state.nodes) == 1  # Verify parsing succeeded

    # With config.unused_parameters=Info, it should be silent
    config.unused_parameters = mi.LogLevel.Info
    state = mi.parser.parse_string(xml, config, unused_param="value")
    assert len(state.nodes) == 1  # Verify parsing succeeded


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


def test15_child_plugins_xml(variant_scalar_rgb):
    """Test parsing plugins with child plugins from XML"""
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

    state = mi.parser.parse_string(xml)

    # Should have: root, shape, bsdf, emitter
    assert len(state.nodes) == 4

    # Check root
    assert state.root.props.plugin_name() == "scene"
    assert len(state.root.children) == 1

    # Check shape
    _, shape_idx = state.root.children[0]
    shape = state.nodes[shape_idx]
    assert shape.props.plugin_name() == "sphere"
    assert shape.props.id() == "myshape"
    assert dr.allclose(shape.props["radius"], 2.0)
    assert len(shape.children) == 2  # bsdf and emitter

    # Check BSDF (first child)
    bsdf_name, bsdf_idx = shape.children[0]
    bsdf = state.nodes[bsdf_idx]
    assert bsdf.props.plugin_name() == "diffuse"
    assert bsdf.props.id() == "mybsdf"
    assert bsdf_name == "_arg_0"  # Auto-generated name
    # RGB properties should be stored directly
    assert "reflectance" in bsdf.props
    assert dr.allclose(bsdf.props["reflectance"], [0.8, 0.2, 0.2])

    # Check emitter (second child)
    emitter_name, emitter_idx = shape.children[1]
    emitter = state.nodes[emitter_idx]
    assert emitter.props.plugin_name() == "area"
    assert emitter_name == "_arg_1"  # Auto-generated name
    assert "radiance" in emitter.props
    assert dr.allclose(emitter.props["radiance"], [10, 10, 10])


def test16_child_plugins_dict(variant_scalar_rgb):
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

    state = mi.parser.parse_dict(dict_scene)

    # Should have: root, shape, bsdf, emitter
    assert len(state.nodes) == 4

    # Check shape - in dict parser, shape is named "myshape"
    shape_name, shape_idx = state.root.children[0]
    shape = state.nodes[shape_idx]
    assert shape.props.plugin_name() == "sphere"
    assert shape_name == "myshape"
    assert dr.allclose(shape.props["radius"], 2.0)
    assert len(shape.children) == 2

    # Find BSDF by checking children
    bsdf_found = False
    emitter_found = False

    for child_name, child_idx in shape.children:
        child = state.nodes[child_idx]
        if child.props.plugin_name() == "diffuse":
            assert child_name == "mybsdf"
            assert dr.allclose(child.props["reflectance"], [0.8, 0.2, 0.2])
            bsdf_found = True
        elif child.props.plugin_name() == "area":
            assert child_name == "myemitter"
            assert dr.allclose(child.props["radiance"], [10, 10, 10])
            emitter_found = True

    assert bsdf_found and emitter_found


def test18_xml_include_basic(variant_scalar_rgb, tmp_path):
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
        <include filename="{include_file}"/>
    </scene>'''

    state = mi.parser.parse_string(main_xml)

    # Should have scene + shape
    assert len(state.nodes) == 2
    assert state.root.props.plugin_name() == "scene"

    # Check included shape
    assert len(state.root.children) == 1
    _, shape_idx = state.root.children[0]
    shape = state.nodes[shape_idx]
    assert shape.props.plugin_name() == "rectangle"
    assert shape.props["width"] == 2.0
    assert shape.props["height"] == 3.0


def test19_xml_include_scene_merge(variant_scalar_rgb, tmp_path):
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
        <include filename="{include_file}"/>
    </scene>'''

    state = mi.parser.parse_string(main_xml)

    # Should have: main scene + rectangle + sphere + cube = 4 nodes
    # (The included scene root is skipped, only its children are merged)
    assert len(state.nodes) == 4
    assert len(state.root.children) == 3

    # Verify all shapes
    shapes = [state.nodes[idx] for name, idx in state.root.children]
    shape_types = [s.props.plugin_name() for s in shapes]
    assert "rectangle" in shape_types
    assert "sphere" in shape_types
    assert "cube" in shape_types


def test20_xml_include_relative_path(variant_scalar_rgb, tmp_path):
    """Test relative path resolution in includes"""
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
    main_xml = '''<scene version="3.5.0">
        <include filename="subdir/shape.xml"/>
    </scene>'''
    main_file.write_text(main_xml)

    # Parse from file to test relative path resolution
    state = mi.parser.parse_file(str(main_file))

    assert len(state.nodes) == 2
    _, shape_idx = state.root.children[0]
    shape = state.nodes[shape_idx]
    assert shape.props.plugin_name() == "sphere"
    assert shape.props["radius"] == 0.5


def test21_xml_include_nested(variant_scalar_rgb, tmp_path):
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
        <include filename="{deep_file}"/>
    </shape>'''
    middle_file.write_text(middle_xml)

    # Main XML
    main_xml = f'''<scene version="3.5.0">
        <include filename="{middle_file}"/>
    </scene>'''

    state = mi.parser.parse_string(main_xml)

    # Should have scene + cube + sphere
    assert len(state.nodes) == 3

    # Find the cube
    _, cube_idx = state.root.children[0]
    cube = state.nodes[cube_idx]
    assert cube.props.plugin_name() == "cube"

    # Find the sphere (child of cube)
    assert len(cube.children) == 1
    _, sphere_idx = cube.children[0]
    sphere = state.nodes[sphere_idx]
    assert sphere.props.plugin_name() == "sphere"
    assert sphere.props["radius"] == 0.25


def test22_xml_include_errors(variant_scalar_rgb, tmp_path):
    """Test error handling in includes"""
    # Test missing file
    xml = '''<scene version="3.5.0">
        <include filename="nonexistent.xml"/>
    </scene>'''

    with pytest.raises(Exception, match="not found"):
        mi.parser.parse_string(xml)

    # Test circular include
    file1 = tmp_path / "file1.xml"
    file2 = tmp_path / "file2.xml"

    file1_xml = f'''<scene version="3.5.0">
        <include filename="{file2}"/>
    </scene>'''
    file1.write_text(file1_xml)

    file2_xml = f'''<scene version="3.5.0">
        <include filename="{file1}"/>
    </scene>'''
    file2.write_text(file2_xml)

    with pytest.raises(Exception, match="Exceeded maximum include recursion depth"):
        mi.parser.parse_file(str(file1))

    # Test include without filename
    xml = '''<scene version="3.5.0">
        <include/>
    </scene>'''

    with pytest.raises(Exception, match="Missing required attribute 'filename'"):
        mi.parser.parse_string(xml)


def test23_xml_include_parameter_substitution(variant_scalar_rgb, tmp_path):
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
        <include filename="{include_file}"/>
    </scene>'''

    # Parse with parameters
    state = mi.parser.parse_string(main_xml, radius="2.5", suffix="test")

    # Check that parameters were substituted
    _, shape_idx = state.root.children[0]
    shape = state.nodes[shape_idx]
    assert shape.props["radius"] == 2.5
    assert shape.props.id() == "sphere_test"


def test24_xml_include_error_chain(variant_scalar_rgb, tmp_path):
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
        <include filename="{error_file}"/>
    </scene>'''
    middle_file.write_text(middle_xml)
    
    # Create main file that includes the middle file
    main_file = tmp_path / "main.xml"
    main_xml = f'''<scene version="3.5.0">
        <include filename="{middle_file}"/>
    </scene>'''
    main_file.write_text(main_xml)
    
    # Test the error chain
    try:
        mi.parser.parse_file(str(main_file))
        assert False, "Should have raised an exception"
    except Exception as e:
        error_msg = str(e)
        # Remove the tmp_path prefix to make the test deterministic
        error_msg = error_msg.replace(str(tmp_path) + "/", "")
        
        # Remove unstable parts: [parser.cpp:XXX] prefixes and zero-width spaces
        import re
        error_msg = error_msg.replace('\u200b', '')  # Remove zero-width spaces
        error_msg = re.sub(r'\[parser\.cpp:\d+\]\s*', '', error_msg)  # Remove [parser.cpp:XXX]
        
        # The expected error message with full include chain
        expected = (
            "Error while parsing main.xml (line 2, col 10): "
            "while processing <include>:\n"
            "Error while parsing middle.xml (line 2, col 10): "
            "while processing <include>:\n"
            "Error while parsing error.xml (line 3, col 14): "
            "Undefined parameter: $undefined_param"
        )
        
        assert error_msg == expected


def test25_xml_unexpected_attributes(variant_scalar_rgb):
    """Test that unexpected attributes are rejected"""
    # Test unexpected attribute on vector tag
    xml = '''<scene version="3.5.0">
        <shape type="sphere">
            <vector name="position" x="1" y="2" z="3" foo="bar"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Unexpected attribute"):
        mi.parser.parse_string(xml)

    # Test unexpected attribute on float tag
    xml = '''<scene version="3.5.0">
        <shape type="sphere">
            <float name="radius" value="1.0" invalid="true"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Unexpected attribute"):
        mi.parser.parse_string(xml)

    # Test unexpected attribute on shape tag
    xml = '''<scene version="3.5.0">
        <shape type="sphere" unknown="value">
            <float name="radius" value="1.0"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Unexpected attribute"):
        mi.parser.parse_string(xml)


def test25_transform_upgrade_called(variant_scalar_rgb):
    """Test that transform_upgrade is called during parsing"""
    # For now, we just verify that parsing completes successfully
    # The actual verification would require modifying transform_upgrade
    # to have observable side effects
    xml = '<scene version="3.5.0"/>'
    state = mi.parser.parse_string(xml)

    # Basic check that parsing succeeded
    assert len(state.nodes) == 1
    assert state.root.props.plugin_name() == "scene"


def test26_transform_tags_xml(variant_scalar_rgb):
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

    state = mi.parser.parse_string(xml)
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


def test27_transform_translate(variant_scalar_rgb):
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

    state = mi.parser.parse_string(xml)
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


def test28_transform_rotate(variant_scalar_rgb):
    """Test rotate transform tag"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="r1">
                <rotate angle="90" x="1" y="0" z="0"/>
            </transform>
            <transform name="r2">
                <rotate angle="45" value="0 0 1"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)
    shape = state.nodes[1]

    # Test rotation around X axis
    r1 = shape.props["r1"]
    assert isinstance(r1, mi.Transform4d)
    expected1 = mi.Transform4d().rotate([1, 0, 0], 90)
    assert dr.allclose(r1.matrix, expected1.matrix)

    # Test rotation around Z axis with value attribute
    r2 = shape.props["r2"]
    assert isinstance(r2, mi.Transform4d)
    expected2 = mi.Transform4d().rotate([0, 0, 1], 45)
    assert dr.allclose(r2.matrix, expected2.matrix)

    # Test missing angle
    xml_bad = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="r">
                <rotate x="1" y="0" z="0"/>
            </transform>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Missing required attribute 'angle'"):
        mi.parser.parse_string(xml_bad)


def test29_transform_scale(variant_scalar_rgb):
    """Test scale transform tag"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="s1">
                <scale x="2" y="3" z="4"/>
            </transform>
            <transform name="s2">
                <scale value="5"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)
    shape = state.nodes[1]

    # Test non-uniform scale
    s1 = shape.props["s1"]
    assert isinstance(s1, mi.Transform4d)
    expected1 = mi.Transform4d().scale([2, 3, 4])
    assert dr.allclose(s1.matrix, expected1.matrix)

    # Test uniform scale with value attribute
    s2 = shape.props["s2"]
    assert isinstance(s2, mi.Transform4d)
    expected2 = mi.Transform4d().scale([5, 5, 5])
    assert dr.allclose(s2.matrix, expected2.matrix)


def test30_transform_lookat(variant_scalar_rgb):
    """Test lookat transform tag"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="l1">
                <lookat origin="0 0 0" target="0 0 1" up="0 1 0"/>
            </transform>
            <transform name="l2">
                <lookat origin="1 2 3" target="4 5 6"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)
    shape = state.nodes[1]

    # Test lookat with explicit up vector
    l1 = shape.props["l1"]
    assert isinstance(l1, mi.Transform4d)
    expected1 = mi.Transform4d().look_at(
        origin=[0, 0, 0],
        target=[0, 0, 1],
        up=[0, 1, 0]
    )
    assert dr.allclose(l1.matrix, expected1.matrix)

    # Test lookat with auto-generated up vector
    l2 = shape.props["l2"]
    assert isinstance(l2, mi.Transform4d)
    # When up is not specified, parser generates one perpendicular to view direction
    # View direction is (4-1, 5-2, 6-3) = (3, 3, 3), normalized = (1,1,1)/sqrt(3)
    # Parser will choose an up vector perpendicular to this

    # Test missing required attributes
    xml_bad = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="l">
                <lookat origin="0 0 0"/>
            </transform>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Missing required attribute 'target'"):
        mi.parser.parse_string(xml_bad)


def test31_transform_matrix(variant_scalar_rgb):
    """Test matrix transform tag"""
    # Test 4x4 identity matrix
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="m1">
                <matrix value="1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)
    shape = state.nodes[1]
    m1 = shape.props["m1"]
    assert isinstance(m1, mi.Transform4d)
    # Should be identity
    assert dr.allclose(m1.matrix, mi.Transform4d().matrix)

    # Test 4x4 custom matrix (90 degree rotation around Y)
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="m2">
                <matrix value="0 0 1 0  0 1 0 0  -1 0 0 0  0 0 0 1"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)
    shape = state.nodes[1]
    m2 = shape.props["m2"]
    expected2 = mi.Transform4d().rotate([0, 1, 0], 90)
    assert dr.allclose(m2.matrix, expected2.matrix)

    # Test 3x3 matrix (gets extended to 4x4)
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="m3">
                <matrix value="2 0 0  0 2 0  0 0 2"/>
            </transform>
        </shape>
    </scene>'''

    state = mi.parser.parse_string(xml)
    shape = state.nodes[1]
    m3 = shape.props["m3"]
    expected3 = mi.Transform4d().scale([2, 2, 2])
    assert dr.allclose(m3.matrix, expected3.matrix)

    # Test invalid matrix size
    xml_bad = '''<scene version="3.0.0">
        <shape type="sphere">
            <transform name="m">
                <matrix value="1 2 3 4 5"/>
            </transform>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="Matrix must have 9 or 16 values"):
        mi.parser.parse_string(xml_bad)


def test32_transform_composition(variant_scalar_rgb):
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

    state = mi.parser.parse_string(xml)
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


def test33_transform_outside_transform_tag(variant_scalar_rgb):
    """Test that transform operations outside transform tags produce errors"""
    xml = '''<scene version="3.0.0">
        <shape type="sphere">
            <translate x="1" y="0" z="0"/>
        </shape>
    </scene>'''

    with pytest.raises(RuntimeError, match="can only appear inside a <transform> tag"):
        mi.parser.parse_string(xml)


def test34_xml_writing(variant_scalar_rgb):
    """Test comprehensive XML writing functionality"""
    # Create a scene with various property types and structures
    # Using values that are exactly representable in binary floating-point:
    # - Powers of 2: 0.5, 0.25, 0.125, 2, 4, 8
    # - Small integers: 1, 3, 5
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
    state = mi.parser.parse_string(xml_input)
    
    # Write to string
    xml_output = mi.parser.write_string(state)
    
    # Expected output with exact formatting
    # Note: - pugixml uses self-closing tags with space before />
    #       - No encoding attribute in XML declaration  
    #       - Transforms are written as matrices
    #       - translate(4,0,0) followed by scale(2,4,8) produces:
    #         [2 0 0 8]  (translation is scaled: 4*2=8)
    #         [0 4 0 0]
    #         [0 0 8 0]
    #         [0 0 0 1]
    #       - 0.5, 0.25, 0.125 are exactly representable in binary
    expected = '''<?xml version="1.0"?>
<scene version="3.5.0">
    <sensor type="perspective" id="camera">
        <float name="fov" value="45" />
        <integer name="sample_count" value="128" />
        <string name="filter" value="gaussian" />
        <boolean name="active" value="true" />
        <transform name="to_world">
            <matrix value="1 0 0 0 0 1 0 5 0 0 1 10 0 0 0 1" />
        </transform>
    </sensor>
    <bsdf type="diffuse" id="red_mat">
        <rgb name="reflectance" value="0.5 0.25 0.125" />
    </bsdf>
    <shape type="sphere" id="sphere1">
        <float name="radius" value="2.5" />
        <vector name="center" x="0" y="1" z="0" />
        <ref id="red_mat" name="material" />
    </shape>
    <shape type="cube">
        <transform name="to_world">
            <matrix value="2 0 0 8 0 4 0 0 0 0 8 0 0 0 0 1" />
        </transform>
        <bsdf type="conductor" id="metal">
            <string name="material" value="Au" />
        </bsdf>
    </shape>
</scene>
'''
    
    assert xml_output == expected


def test34_dict_boolean_support(variant_scalar_rgb):
    """Test parsing boolean values in dictionary"""
    dict_scene = {
        "type": "scene",
        "sensor": {
            "type": "perspective",
            "active": True,
            "use_cache": False
        }
    }

    state = mi.parser.parse_dict(dict_scene)

    # Check boolean properties
    sensor_node = state.nodes[1]
    assert "active" in sensor_node.props
    assert sensor_node.props["active"] == True
    assert "use_cache" in sensor_node.props
    assert sensor_node.props["use_cache"] == False


def test35_dict_key_validation(variant_scalar_rgb):
    """Test that dictionary keys with dots are rejected"""
    dict_scene = {
        "type": "scene",
        "my.sensor": {  # Invalid key with dot
            "type": "perspective"
        }
    }

    with pytest.raises(RuntimeError, match="contains a '.' character"):
        mi.parser.parse_dict(dict_scene)

    # Also test for property names
    dict_scene2 = {
        "type": "scene",
        "sensor": {
            "type": "perspective",
            "focal.length": 50  # Invalid property name with dot
        }
    }

    with pytest.raises(RuntimeError, match="contains a '.' character"):
        mi.parser.parse_dict(dict_scene2)


def test36_dict_transform_support(variant_scalar_rgb):
    """Test parsing Transform4f values in dictionary"""
    # Create a transform matrix
    transform = mi.Transform4f().translate(mi.Point3f(1, 2, 3)) @ mi.Transform4f().scale(mi.Point3f(2, 2, 2))
    
    dict_scene = {
        "type": "scene",
        "shape": {
            "type": "sphere",
            "to_world": transform
        }
    }

    state = mi.parser.parse_dict(dict_scene)

    # Check transform property
    shape_node = state.nodes[1]
    assert "to_world" in shape_node.props
    stored_transform = shape_node.props["to_world"]
    assert dr.allclose(stored_transform.matrix, transform.matrix)


def test37_dict_color3f_support(variant_scalar_rgb):
    """Test parsing Color3f values in dictionary"""
    color = mi.Color3f(0.5, 0.7, 0.9)
    
    dict_scene = {
        "type": "scene",
        "bsdf": {
            "type": "diffuse",
            "reflectance": color
        }
    }

    state = mi.parser.parse_dict(dict_scene)

    # Check color property
    bsdf_node = state.nodes[1]
    assert "reflectance" in bsdf_node.props
    stored_color = bsdf_node.props["reflectance"]
    assert dr.allclose(stored_color, [0.5, 0.7, 0.9])


def test38_dict_rgb_dictionary_support(variant_scalar_rgb):
    """Test RGB dictionary support"""
    # Test RGB dictionary
    dict_scene = {
        "type": "scene",
        "bsdf": {
            "type": "diffuse",
            "reflectance": {
                "type": "rgb",
                "value": [0.5, 0.7, 0.9]
            }
        }
    }

    state = mi.parser.parse_dict(dict_scene)
    bsdf_node = state.nodes[1]
    
    # The reflectance property should be set as a Color3f
    assert "reflectance" in bsdf_node.props
    color = bsdf_node.props["reflectance"]
    assert dr.allclose(color, [0.5, 0.7, 0.9])

    # Test RGB dictionary with Color3f object
    dict_scene2 = {
        "type": "scene",
        "bsdf": {
            "type": "diffuse",
            "reflectance": {
                "type": "rgb",
                "value": mi.Color3f(0.1, 0.2, 0.3)
            }
        }
    }

    state2 = mi.parser.parse_dict(dict_scene2)
    bsdf_node2 = state2.nodes[1]
    assert "reflectance" in bsdf_node2.props
    color2 = bsdf_node2.props["reflectance"]
    assert dr.allclose(color2, [0.1, 0.2, 0.3])


def test39_dict_reference_support(variant_scalar_rgb):
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

    state = mi.parser.parse_dict(dict_scene)
    
    # Check that the material has an ID
    material_node = state.nodes[1]
    assert material_node.props.id() == "my_material"
    
    # Check that the shape has a reference child
    shape_node = state.nodes[2]
    assert len(shape_node.children) == 1
    
    # Check the reference node
    ref_name, ref_idx = shape_node.children[0]
    assert ref_name == "bsdf"
    
    ref_node = state.nodes[ref_idx]
    assert ref_node.props.plugin_name() == "ref"
    assert ref_node.props.id() == "my_material"
    assert ref_node.property_name == "bsdf"


def test40_dict_spectrum_support(variant_scalar_rgb):
    """Test spectrum dictionary support"""
    # Test uniform spectrum
    dict_scene1 = {
        "type": "scene",
        "emitter": {
            "type": "point",
            "intensity": {
                "type": "spectrum",
                "value": 2.5
            }
        }
    }

    state1 = mi.parser.parse_dict(dict_scene1)
    emitter_node = state1.nodes[1]
    
    # Uniform spectrum should be stored as a float
    assert "intensity" in emitter_node.props
    assert dr.allclose(emitter_node.props["intensity"], 2.5)

    # Test spectrum with wavelength-value pairs
    dict_scene2 = {
        "type": "scene",
        "emitter": {
            "type": "point",
            "intensity": {
                "type": "spectrum",
                "value": [[400, 0.5], [500, 1.0], [600, 0.8], [700, 0.3]]
            }
        }
    }

    # This should parse without error, even if full implementation is pending
    state2 = mi.parser.parse_dict(dict_scene2)


def test41_dict_hierarchical_error_paths(variant_scalar_rgb):
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

    with pytest.raises(RuntimeError, match=r"\[scene\.integrator\.nested\].*missing 'type' attribute"):
        mi.parser.parse_dict(dict_scene1)

    # Test invalid RGB value with path
    dict_scene2 = {
        "type": "scene",
        "material": {
            "type": "diffuse",
            "reflectance": {
                "type": "rgb",
                "value": [0.5, 0.7]  # Only 2 components
            }
        }
    }

    with pytest.raises(RuntimeError, match=r"\[scene\.material\.reflectance\].*must have exactly 3 components"):
        mi.parser.parse_dict(dict_scene2)

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

    with pytest.raises(RuntimeError, match=r"\[scene\.sensor\.film\.pixel\.filter\].*contains a '\.' character"):
        mi.parser.parse_dict(dict_scene3)


def test42_dict_mitsuba_object_support(variant_scalar_rgb):
    """Test that Mitsuba objects can be passed directly in the dictionary parser"""
    # Create a Mitsuba texture object
    custom_texture = mi.load_dict({
        "type": "checkerboard",
        "color0": mi.Color3f(0, 0, 0),
        "color1": mi.Color3f(1, 1, 1)
    })
    
    # Use it in a scene dictionary
    dict_scene = {
        "type": "scene",
        "material": {
            "type": "diffuse",
            "reflectance": custom_texture  # Pass the texture object directly
        }
    }
    
    state = mi.parser.parse_dict(dict_scene)
    
    # Check that the material node was created
    material_node = state.nodes[1]
    assert material_node.props.plugin_name() == "diffuse"
    
    # The texture should be stored as an Object property
    assert "reflectance" in material_node.props
    assert material_node.props.type("reflectance") == mi.Properties.Type.Object
    
    # Test with another Mitsuba object type (e.g., a BSDF)
    custom_bsdf = mi.load_dict({
        "type": "diffuse",
        "reflectance": mi.Color3f(0.8, 0.8, 0.8)
    })
    
    dict_scene2 = {
        "type": "scene",
        "shape": {
            "type": "sphere",
            "bsdf": custom_bsdf  # Pass BSDF object directly
        }
    }
    
    state2 = mi.parser.parse_dict(dict_scene2)
    shape_node = state2.nodes[1]
    assert "bsdf" in shape_node.props
    assert shape_node.props.type("bsdf") == mi.Properties.Type.Object


def test43_dict_arbitrary_object_support(variant_scalar_rgb):
    """Test that arbitrary nanobind-bound objects can be stored via Any type"""
    # Create a Properties object (which is nanobind-bound)
    custom_props = mi.Properties("test")
    custom_props["value"] = 42
    custom_props["name"] = "test_props"
    
    dict_scene = {
        "type": "scene",
        "integrator": {
            "type": "path",
            "custom_data": custom_props  # Pass Properties object as arbitrary data
        }
    }
    
    state = mi.parser.parse_dict(dict_scene)
    integrator_node = state.nodes[1]
    
    # The Properties object should be stored as an Any type
    assert "custom_data" in integrator_node.props
    assert integrator_node.props.type("custom_data") == mi.Properties.Type.Any
    
    # Test with a FileResolver object (another nanobind-bound type)
    resolver = mi.FileResolver()
    resolver.append("/tmp")
    
    dict_scene2 = {
        "type": "scene",
        "sensor": {
            "type": "perspective",
            "file_resolver": resolver  # Pass FileResolver as arbitrary data
        }
    }
    
    state2 = mi.parser.parse_dict(dict_scene2)
    sensor_node = state2.nodes[1]
    assert "file_resolver" in sensor_node.props
    assert sensor_node.props.type("file_resolver") == mi.Properties.Type.Any



