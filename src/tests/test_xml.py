from mitsuba.core.xml import load_string
from mitsuba.render import Scene
from mitsuba.core import Thread, EWarn
import pytest


def test01_invalid_xml():
    with pytest.raises(Exception):
        load_string('<?xml version="1.0"?>')


def test02_invalid_root_node():
    with pytest.raises(Exception):
        load_string('<?xml version="1.0"?><invalid></invalid>')


def test03_invalid_root_node():
    with pytest.raises(Exception) as e:
        load_string('<?xml version="1.0"?><integer name="a" '
                   'value="10"></integer>')
    e.match('root element "integer" must be an object')


def test04_valid_root_node():
    obj1 = load_string('<?xml version="1.0"?>\n<scene version="0.4.0">'
                      '</scene>')
    obj2 = load_string('<scene version="0.4.0"></scene>')
    assert type(obj1) is Scene
    assert type(obj2) is Scene


def test05_duplicateID():
    with pytest.raises(Exception) as e:
        load_string("""
        <scene version="0.4.0">
            <shape type="ply" id="myID"/>
            <shape type="ply" id="myID"/>
        </scene>
        """)
    e.match('Error while loading "<string>" \(at line 4, col 14\):'
        ' "shape" has duplicate id "myID" \(previous was at line 3,'
        ' col 14\)')


def test06_reserved_iD():
    with pytest.raises(Exception) as e:
        load_string('<scene version="0.4.0">' +
                   '<shape type="ply" id="_test"/></scene>')
    e.match('invalid id "_test" in "shape": leading underscores '
        'are reserved for internal identifiers')


def test06_reserved_name():
    with pytest.raises(Exception) as e:
        load_string('<scene version="0.4.0">' +
                   '<shape type="ply">' +
                   '<integer name="_test" value="1"/></shape></scene>')
    e.match('invalid parameter name "_test" in "integer": '
        'leading underscores are reserved for internal identifiers')


def test06_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <shape type="ply"/>
                   </integer></shape></scene>""")
    e.match('node "shape" cannot occur as child of a property')


def test07_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <float name="value" value="1"/>
                   </integer></shape></scene>""")
    e.match('node "float" cannot occur as child of a property')


def test08_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply">
                   <translate name="value" x="0" y="1" z="2"/>
                   </shape></scene>""")
    e.match('transform operations can only occur in a transform node')


def test09_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply">
                   <transform name="toWorld">
                   <integer name="value" value="10"/>
                   </transform>
                   </shape></scene>""")
    e.match('transform nodes can only contain transform operations')


def test10_unknown_id():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <ref id="unknown"/>
                   </scene>""")
    e.match('reference to unknown object "unknown"')


def test11_unknown_attribute():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply" param2="abc">
                   </shape></scene>""")
    e.match('unexpected attribute "param2" in "shape"')


def test12_missing_attribute():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <integer name="a"/></scene>""")
    e.match('missing attribute "value" in "integer"')


def test13_duplicate_parameter():
    logger = Thread.thread().logger()
    l = logger.error_level()
    try:
        logger.set_error_level(EWarn)
        with pytest.raises(Exception) as e:
            load_string("""<scene version="0.4.0">
                       <integer name="a" value="1"/>
                       <integer name="a" value="1"/>
                       </scene>""")
    finally:
        logger.set_error_level(l)
    e.match('Property "a" was specified multiple times')


def test14_missing_parameter():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply"/>
                   </scene>""")
    e.match('Property "filename" has not been specified')


def test15_incorrect_parameter_type():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <shape type="ply">
                      <float name="filename" value="1.0"/>
                   </shape></scene>""")
    e.match('The property "filename" has the wrong type'
        ' \(expected <string>\).')


def test16_invalid_integer():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <integer name="10" value="a"/>
                   </scene>""")
    e.match('Could not parse integer value "a"')


def test17_invalid_float():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <float name="10" value="a"/>
                   </scene>""")
    e.match('Could not parse floating point value "a"')


def test18_invalid_boolean():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <boolean name="10" value="a"/>
                   </scene>""")
    e.match('Could not parse boolean value "a"'
            ' -- must be "true" or "false"')


def test19_invalid_vector():
    err_str = 'Could not parse floating point value "a"'
    err_str2 = '"value" attribute must have exactly 3 elements'
    err_str3 = 'Can\'t mix and match "value" and "x"/"y"/"z" attributes'

    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <vector name="10" x="a" y="b" z="c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <vector name="10" value="a, b, c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <vector name="10" value="1, 2"/>
                   </scene>""")
    e.match(err_str2)

    with pytest.raises(Exception) as e:
        load_string("""<scene version="0.4.0">
                   <vector name="10" value="1, 2, 3" x="4"/>
                   </scene>""")
    e.match(err_str3)
