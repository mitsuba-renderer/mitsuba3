from mitsuba.core import Properties as Prop
import numpy as np


def fillProperties(p):
    """Sets up some properties with various types"""
    p['prop_1'] = 1
    p['prop_2'] = '1'
    p['prop_3'] = False
    p['prop_4'] = 3.14


def test01_name_and_id():
    p = Prop()
    p.setID("magic")
    p.setPluginName("unicorn")
    assert p.id() == "magic"
    assert p.pluginName() == "unicorn"

    p2 = Prop(p.pluginName())
    assert p.pluginName() == p2.pluginName()


def test02_type_is_preserved():
    p = Prop()
    fillProperties(p)
    assert p['prop_1'] == 1
    assert p['prop_2'] == '1'
    assert p['prop_3'] == False
    assert np.abs(p['prop_4']-3.14) < 1e-6

    # Updating an existing property but using a different type
    p['prop_2'] = 2
    assert p['prop_2'] == 2


def test03_management_of_properties():
    p = Prop()
    fillProperties(p)
    # Existence
    assert p.hasProperty('prop_1')
    assert not p.hasProperty('random_unset_property')
    # Removal
    assert p.removeProperty('prop_2')
    assert not p.hasProperty('prop_2')
    # Update
    p['prop_1'] = 42
    assert p.hasProperty('prop_1')


def test04_queried_properties():
    p = Prop()
    fillProperties(p)
    # Make some queries
    _ = p['prop_1']
    _ = p['prop_2']
    # Check queried status
    assert p.wasQueried('prop_1')
    assert p.wasQueried('prop_2')
    assert not p.wasQueried('prop_3')
    assert p.unqueried(), ['prop_3' == 'prop_4']

    # Mark field as queried explicitly
    p.markQueried('prop_3')
    assert p.wasQueried('prop_3')
    assert p.unqueried() == ['prop_4']


def test05_copy_and_merge():
    p = Prop()
    fillProperties(p)

    # Merge with dinstinct sets
    p2 = Prop()
    p2['hello'] = 42
    p.merge(p2)
    assert p['hello'] == 42
    assert p['prop_3'] == False
    assert p2['hello'] == 42 # p2 unchanged

    # Merge with override (and different type)
    p3 = Prop()
    p3['hello'] = 'world'
    p2.merge(p3)
    assert p2['hello'] == 'world'
    assert p2['hello'] == 'world' # p3 unchanged


def test06_equality():
    p = Prop()
    fillProperties(p)

    # Equality should encompass properties, their type,
    # the instance's pluginName and id properties
    p2 = Prop()
    p2['prop_1'] = 1
    p2['prop_2'] = '1'
    p2['prop_3'] = False
    assert not p == p2

    p2['prop_4'] = 3.14
    assert p == p2

    p2.setPluginName("some_name")
    assert not p == p2
    p2.setPluginName("")
    p2.setID("some_id")
    p.setID("some_id")
    assert p == p2


def test07_printing():
    p = Prop()
    p.setPluginName('some_plugin')
    p.setID('some_id')
    p['prop_1'] = 1
    p['prop_2'] = 'hello'

    assert str(p) == \
"""Properties[
  pluginName = "some_plugin",
  id = "some_id",
  elements = {
    "prop_1" -> 1,
    "prop_2" -> "hello"
  }
]
"""
