import pytest
import drjit as dr
import mitsuba as mi


def fill_properties(p):
    """Sets up some properties with various types"""
    from drjit.scalar import Array3f

    p['prop_1'] = 1
    p['prop_2'] = '1'
    p['prop_3'] = False
    p['prop_4'] = 1.25
    p['prop_5'] = Array3f(1, 2, 3)
    p['prop_6'] = mi.ScalarColor3f(1, 2, 3)
    p['prop_7'] = mi.Object()


def test01_name_and_id(variant_scalar_rgb):
    p = mi.Properties()
    p.set_id("magic")
    p.set_plugin_name("unicorn")
    assert p.id() == "magic"
    assert p.plugin_name() == "unicorn"

    p2 = mi.Properties(p.plugin_name())
    assert p.plugin_name() == p2.plugin_name()


def test02_type_is_preserved(variant_scalar_rgb):
    from drjit.scalar import Array3f, Array3f64

    p = mi.Properties()
    fill_properties(p)

    assert isinstance(p['prop_1'], int)
    assert isinstance(p['prop_2'], str)
    assert isinstance(p['prop_3'], bool)
    assert isinstance(p['prop_4'], float)
    assert type(p['prop_5']) is Array3f64
    assert type(p['prop_6']) is mi.ScalarColor3d

    assert p['prop_1'] == 1
    assert p['prop_2'] == '1'
    assert p['prop_3'] == False
    assert p['prop_4'] == 1.25
    assert dr.all(p['prop_5'] == Array3f(1, 2, 3))
    assert dr.all(p['prop_6'] == mi.ScalarColor3f(1, 2, 3))

    # Updating an existing property but using a different type
    p['prop_2'] = 2
    assert p['prop_2'] == 2

    p['prop_7'] = [1, 2, 3]
    print()
    assert type(p['prop_7']) is Array3f64


def test03_management_of_properties(variant_scalar_rgb):
    p = mi.Properties()
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
    p = mi.Properties()
    fill_properties(p)
    # Make some queries
    _ = p['prop_1']
    _ = p['prop_2']
    # Check queried status
    assert p.was_queried('prop_1')
    assert p.was_queried('prop_2')
    assert not p.was_queried('prop_3')
    assert p.unqueried() == ['prop_3', 'prop_4', 'prop_5', 'prop_6', 'prop_7']

    # Mark field as queried explicitly
    p.mark_queried('prop_4')
    assert p.was_queried('prop_4')
    assert p.unqueried() == ['prop_3', 'prop_5', 'prop_6', 'prop_7']


def test05_copy_and_merge(variant_scalar_rgb):
    p = mi.Properties()
    fill_properties(p)

    # Merge with dinstinct sets
    p2 = mi.Properties()
    p2['hello'] = 42
    p.merge(p2)
    assert p['hello'] == 42
    assert p['prop_3'] == False
    assert p2['hello'] == 42 # p2 unchanged

    # Merge with override (and different type)
    p3 = mi.Properties()
    p3['hello'] = 'world'
    p2.merge(p3)
    assert p2['hello'] == 'world'
    assert p2['hello'] == 'world' # p3 unchanged


def test06_equality(variant_scalar_rgb):
    p = mi.Properties()
    fill_properties(p)
    del p['prop_5']
    del p['prop_6']
    del p['prop_7']

    # Equality should encompass properties, their type,
    # the instance's plugin_name and id properties
    p2 = mi.Properties()
    p2['prop_1'] = 1
    p2['prop_2'] = '1'
    p2['prop_3'] = False
    assert not p == p2

    p2['prop_4'] = 1.25
    assert p == p2

    p2.set_plugin_name("some_name")
    assert not p == p2
    p2.set_plugin_name("")
    p2.set_id("some_id")
    p.set_id("some_id")
    assert p == p2


def test07_printing(variant_scalar_rgb):
    p = mi.Properties()
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


def test08_get_default(variant_scalar_rgb):
    p = mi.Properties()
    assert p.get('foo') == None
    assert p.get('foo', 4.0) == 4.0
    assert p.get('foo', 4) == 4
    assert type(p.get('foo', 4)) == int
    assert p.get('foo', 'foo') == 'foo'

    p['foo'] = 'test'
    assert p.get('foo', 4.0) == 'test'


def test09_create_object(variants_all_rgb):
    props = mi.Properties()
    props.set_plugin_name('path')
    props['max_depth'] = 4

    it = mi.PluginManager.instance().create_object(props)

    assert mi.variant() == it.class_().variant()
    assert it.class_().name() == 'PathIntegrator'
    assert 'max_depth = 4' in str(it)


@pytest.mark.skip("TODO fix AnimatedTransform")
def test10_animated_transforms(variant_scalar_rgb):
    """An AnimatedTransform can be built from a given Transform."""
    p = mi.Properties()
    p["trafo"] = mi.Transform4f().translate([1, 2, 3])

    atrafo = mi.AnimatedTransform()
    atrafo.append(0, mi.Transform4f().translate([-1, -1, -2]))
    atrafo.append(1, mi.Transform4f().translate([4, 3, 2]))
    p["atrafo"] = atrafo

    assert type(p["trafo"]) is mi.Transform4d
    assert type(p["atrafo"]) is mi.AnimatedTransform

def test10_transforms3(variant_scalar_rgb):
    p = mi.Properties()
    p["transform"] = mi.ScalarTransform3d().translate([2,4])

    assert type(p["transform"] is mi.ScalarTransform3d)


def test11_tensor(variant_scalar_rgb):
    props = mi.Properties()

    # Check copy is set to properties
    a = dr.zeros(mi.TensorXf, shape = [30, 30, 30])
    props['foo'] = a
    a = None
    assert props['foo'].shape == (30, 30, 30)

    # Check temporary can be set
    props['moo'] =  dr.zeros(mi.TensorXf, shape = [30, 30, 30])
    assert props['moo'].shape == (30, 30, 30)

    # Check numpy
    import numpy as np
    props['goo'] = mi.TensorXf(np.zeros((2, 3, 4)))
    assert props['goo'].shape == (2, 3, 4)


def test11_tensor_cuda(variant_cuda_ad_rgb):
    props = mi.Properties()

    ### Check PyTorch
    torch = pytest.importorskip("torch")
    props['goo'] = mi.TensorXf(torch.zeros(2, 3, 4))
    assert props['goo'].shape == (2, 3, 4)

def test12_large_integer_keys(variant_scalar_rgb):
    key1 = '5c930abab5cb692464249420000000000000000000000004'
    key2 = '5c930abab5cb692464249420000000000000000000000001'

    props = mi.Properties()
    props[key1] = 4.0
    props[key2] = 8.0

    assert len(props.property_names()) == 2
    assert props[key1] == 4.0
    assert props[key2] == 8.0

def test13_trailing_zeros_keys(variant_scalar_rgb):
    key1 = 'aa_1'
    key2 = 'aa_0001'

    props = mi.Properties()
    props[key1] = 4.0
    props[key2] = 8.0

    assert len(props.property_names()) == 2
    assert props[key1] == 4.0
    assert props[key2] == 8.0
