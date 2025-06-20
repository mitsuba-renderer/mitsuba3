import pytest
import mitsuba as mi


def test01_basic_xml_parsing():
    """Test parsing a simple XML snippet without instantiation"""
    xml = '<phase type="foo" version="1.2.3"/>'

    state = mi.parser.parse_string(xml)

    # Check basic structure
    assert len(state.nodes) == 1
    assert state.root.props.plugin_name() == "scene"
    assert state.root.props.id() == "root"

    # Version should be parsed
    assert state.version.major() == 1
    assert state.version.minor() == 2
    assert state.version.patch() == 0


def test02_basic_dict_parsing():
    """Test parsing a simple dictionary without instantiation"""
    dict_scene = {
        "type": "scene",
        "mysensor": {
            "type": "perspective"
        }
    }

    state = mi.parser.parse_dict(dict_scene)

    # Check basic structure
    assert len(state.nodes) == 1
    assert state.root.props.plugin_name() == "scene"
    assert state.root.props.id() == "root"


def test03_error_invalid_xml():
    """Test error handling for invalid XML"""
    xml = '<phase type="foo" version="invalid"/>'

    with pytest.raises(RuntimeError, match="Invalid version"):
        mi.parser.parse_string(xml)


def test04_error_missing_type_dict():
    """Test error handling for dictionary without type"""
    dict_scene = {
        "mysensor": {
            "fov": 45
        }
    }

    with pytest.raises(RuntimeError, match="missing.*type"):
        mi.parser.parse_dict(dict_scene)


def test05_parameter_substitution():
    """Test parameter substitution in XML"""
    xml = '<phase type="$mytype" version="3.0.0"/>'

    state = mi.parser.parse_string(xml, mytype="isotropic")

    # CLAUDE: check type here as well
    # For now, just check it doesn't crash
    assert len(state.nodes) == 1

def test06_file_location():
    """Test file location reporting"""
    # CLAUDE: the test doesn't make sense as designed.
    # file_location() should either open file associated with the given node and then
    # walk through it character by character to determine the row and column. Or if
    # the file is given in string form, then it can do the same with "node.content"
    # The only thing that would make sense here is to make a temporary file with
    # several lines and then check if the row/column is computed correctly

    state = mi.parser.ParserState()

    # Add a node with file info
    node = mi.parser.SceneNode()
    node.file_index = 0
    node.offset = 50
    node.props.set_id("test_node")
    state.nodes.append(node)
    state.files.append("/path/to/test.xml")

    location = mi.parser.file_location(state, node)
    assert "/path/to/test.xml:50" in location

    # Test node without file info
    node2 = mi.parser.SceneNode()
    node2.props.set_id("another_node")
    state.nodes.append(node2)

    location2 = mi.parser.file_location(state, node2)
    assert "another_node" in location2


