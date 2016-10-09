from mitsuba.core.xml import loadString
from mitsuba.render import Scene
from mitsuba.core import Thread, EWarn
import pytest


def test01_invalidXML():
    with pytest.raises(Exception):
        loadString('<?xml version="1.0"?>')


def test02_invalidRootNode():
    with pytest.raises(Exception):
        loadString('<?xml version="1.0"?><invalid></invalid>')


def test03_invalidRootNode():
    with pytest.raises(Exception) as e:
        loadString('<?xml version="1.0"?><integer name="a" '
                   'value="10"></integer>')
    e.match('root element "integer" must be an object')


def test04_validRootNode():
    obj1 = loadString('<?xml version="1.0"?>\n<scene version="0.4.0">'
                      '</scene>')
    obj2 = loadString('<scene version="0.4.0"></scene>')
    assert type(obj1) is Scene
    assert type(obj2) is Scene


def test05_duplicateID():
    with pytest.raises(Exception) as e:
        loadString("""
        <scene version="0.4.0">
            <shape type="ply" id="myID"/>
            <shape type="ply" id="myID"/>
        </scene>
        """)
    e.match('Error while loading "<string>" \(at line 4, col 14\):'
        ' "shape" has duplicate id "myID" \(previous was at line 3,'
        ' col 14\)')


def test06_reservedID():
    with pytest.raises(Exception) as e:
        loadString('<scene version="0.4.0">' +
                   '<shape type="ply" id="_test"/></scene>')
    e.match('invalid id "_test" in "shape": leading underscores '
        'are reserved for internal identifiers')


def test06_reservedName():
    with pytest.raises(Exception) as e:
        loadString('<scene version="0.4.0">' +
                   '<shape type="ply">' +
                   '<integer name="_test" value="1"/></shape></scene>')
    e.match('invalid parameter name "_test" in "integer": '
        'leading underscores are reserved for internal identifiers')


def test06_incorrectNesting():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <shape type="ply"/>
                   </integer></shape></scene>""")
    e.match('node "shape" cannot occur as child of a property')


def test07_incorrectNesting():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <float name="value" value="1"/>
                   </integer></shape></scene>""")
    e.match('node "float" cannot occur as child of a property')


def test08_incorrectNesting():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply">
                   <translate name="value" x="0" y="1" z="2"/>
                   </shape></scene>""")
    e.match('transform operations can only occur in a transform node')


def test09_incorrectNesting():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply">
                   <transform name="toWorld">
                   <integer name="value" value="10"/>
                   </transform>
                   </shape></scene>""")
    e.match('transform nodes can only contain transform operations')


def test10_unknownID():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <ref id="unknown"/>
                   </scene>""")
    e.match('reference to unknown object "unknown"')


def test11_unknownAttribute():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply" param2="abc">
                   </shape></scene>""")
    e.match('unexpected attribute "param2" in "shape"')


def test12_missingAttribute():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <integer name="a"/></scene>""")
    e.match('missing attribute "value" in "integer"')


def test13_duplicateParameter():
    logger = Thread.thread().logger()
    l = logger.errorLevel()
    try:
        logger.setErrorLevel(EWarn)
        with pytest.raises(Exception) as e:
            loadString("""<scene version="0.4.0">
                       <integer name="a" value="1"/>
                       <integer name="a" value="1"/>
                       </scene>""")
    finally:
        logger.setErrorLevel(l)
    e.match('Property "a" was specified multiple times')


def test14_missingParameter():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply"/>
                   </scene>""")
    e.match('Property "filename" has not been specified')


def test15_incorrectParameterType():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <shape type="ply">
                      <float name="filename" value="1.0"/>
                   </shape></scene>""")
    e.match('The property "filename" has the wrong type'
        ' \(expected <string>\).')


def test16_invalidInteger():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <integer name="10" value="a"/>
                   </scene>""")
    e.match('Could not parse integer value "a"')


def test17_invalidFloat():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <float name="10" value="a"/>
                   </scene>""")
    e.match('Could not parse floating point value "a"')


def test18_invalidBoolean():
    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <boolean name="10" value="a"/>
                   </scene>""")
    e.match('Could not parse boolean value "a"'
            ' -- must be "true" or "false"')


def test19_invalidVector():
    err_str = 'Could not parse floating point value "a"'
    err_str2 = '"value" attribute must have exactly 3 elements'
    err_str3 = 'Can\'t mix and match "value" and "x"/"y"/"z" attributes'

    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <vector name="10" x="a" y="b" z="c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <vector name="10" value="a, b, c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <vector name="10" value="1, 2"/>
                   </scene>""")
    e.match(err_str2)

    with pytest.raises(Exception) as e:
        loadString("""<scene version="0.4.0">
                   <vector name="10" value="1, 2, 3" x="4"/>
                   </scene>""")
    e.match(err_str3)
