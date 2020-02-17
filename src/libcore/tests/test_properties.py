import enoki as ek
import pytest
import mitsuba


def fill_properties(p):
    """Sets up some properties with various types"""
    p['prop_1'] = 1
    p['prop_2'] = '1'
    p['prop_3'] = False
    p['prop_4'] = 3.14


def test01_name_and_id(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    p.set_id("magic")
    p.set_plugin_name("unicorn")
    assert p.id() == "magic"
    assert p.plugin_name() == "unicorn"

    p2 = Prop(p.plugin_name())
    assert p.plugin_name() == p2.plugin_name()


def test02_type_is_preserved(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    fill_properties(p)
    assert p['prop_1'] == 1
    assert p['prop_2'] == '1'
    assert p['prop_3'] == False
    assert ek.abs(p['prop_4']-3.14) < 1e-6

    # Updating an existing property but using a different type
    p['prop_2'] = 2
    assert p['prop_2'] == 2


def test03_management_of_properties(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    fill_properties(p)
    # Existence
    assert 'prop_1' in p
    assert p.has_property('prop_1')
    assert not p.has_property('random_unset_property')
    # Removal
    assert p.remove_property('prop_2')
    assert not p.has_property('prop_2')
    # Update
    p['prop_1'] = 42
    assert p.has_property('prop_1')
    del p['prop_1']
    assert not p.has_property('prop_1')


def test04_queried_properties(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    fill_properties(p)
    # Make some queries
    _ = p['prop_1']
    _ = p['prop_2']
    # Check queried status
    assert p.was_queried('prop_1')
    assert p.was_queried('prop_2')
    assert not p.was_queried('prop_3')
    assert p.unqueried(), ['prop_3' == 'prop_4']

    # Mark field as queried explicitly
    p.mark_queried('prop_3')
    assert p.was_queried('prop_3')
    assert p.unqueried() == ['prop_4']


def test05_copy_and_merge(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    fill_properties(p)

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


def test06_equality(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    fill_properties(p)

    # Equality should encompass properties, their type,
    # the instance's plugin_name and id properties
    p2 = Prop()
    p2['prop_1'] = 1
    p2['prop_2'] = '1'
    p2['prop_3'] = False
    assert not p == p2

    p2['prop_4'] = 3.14
    assert p == p2

    p2.set_plugin_name("some_name")
    assert not p == p2
    p2.set_plugin_name("")
    p2.set_id("some_id")
    p.set_id("some_id")
    assert p == p2


def test07_printing(variant_scalar_rgb):
    from mitsuba.core import Properties as Prop

    p = Prop()
    p.set_plugin_name('some_plugin')
    p.set_id('some_id')
    p['prop_1'] = 1
    p['prop_2'] = 'hello'

    assert str(p) == \
"""Properties[
  plugin_name = "some_plugin",
  id = "some_id",
  elements = {
    "prop_1" -> 1,
    "prop_2" -> "hello"
  }
]
"""


@pytest.mark.skip("TODO fix AnimatedTransform")
def test08_animated_transforms(variant_scalar_rgb):
    """An AnimatedTransform can be built from a given Transform."""
    from mitsuba.core import Properties as Prop, Transform4f, AnimatedTransform

    p = Prop()
    p["trafo"] = Transform4f.translate([1, 2, 3])

    atrafo = AnimatedTransform()
    atrafo.append(0, Transform4f.translate([-1, -1, -2]))
    atrafo.append(1, Transform4f.translate([4, 3, 2]))
    p["atrafo"] = atrafo

    assert type(p["trafo"]) is Transform4f
    assert type(p["atrafo"]) is AnimatedTransform

