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

    assert p.type('prop_1') == mi.Properties.Type.Integer
    assert p.type('prop_2') == mi.Properties.Type.String
    assert p.type('prop_3') == mi.Properties.Type.Bool
    assert p.type('prop_4') == mi.Properties.Type.Float
    assert p.type('prop_5') == mi.Properties.Type.Vector
    assert p.type('prop_6') == mi.Properties.Type.Color
    assert p.type('prop_7') == mi.Properties.Type.Object

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
    assert type(p['prop_7']) is Array3f64


def test03_management_of_properties(variant_scalar_rgb):
    p = mi.Properties()
    fill_properties(p)
    # Existence
    assert 'prop_1' in p
    # Test deprecated method (with warning suppression)
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        assert p.has_property('prop_1')
        assert not p.has_property('random_unset_property')

    # Removal - test both old and new ways
    # Old way (deprecated)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        assert p.remove_property('prop_2')
        assert not p.has_property('prop_2')

    # New way (Pythonic)
    p['prop_2'] = 'restored'  # Restore for next test
    assert 'prop_2' in p
    del p['prop_2']
    assert 'prop_2' not in p

    # Update
    p['prop_1'] = 42
    assert 'prop_1' in p
    del p['prop_1']
    assert 'prop_1' not in p


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

    assert mi.variant() == it.variant_name()
    assert 'PathIntegrator' in str(it)
    assert 'max_depth = 4' in str(it)


def test10_transforms3(variant_scalar_rgb):
    p = mi.Properties()
    p["transform"] = mi.ScalarTransform3d().translate([2,4])

    # In Python, transforms are always returned as 4x4
    assert type(p["transform"]) is mi.ScalarTransform4d


def test11_tensor(variant_scalar_rgb):
    props = mi.Properties()

    # Check copy is set to properties
    a = dr.zeros(mi.TensorXf, shape = [30, 30, 30])
    props['foo'] = a
    a = None
    assert props['foo'].shape == (30, 30, 30)

def test12_nanobind_object_storage(variant_scalar_rgb):
    """Test storing and retrieving nanobind-bound C++ objects via Any"""
    props = mi.Properties()

    # Test with TensorXf - this should use our catch-all setter since there's no specific TensorXf setter
    import numpy as np
    tensor = mi.TensorXf(np.array([[1, 2], [3, 4]], dtype=np.float32))
    props['tensor'] = tensor

    # Verify this is stored as 'any' type since TensorXf doesn't have a specific property type
    assert props.type('tensor') == mi.Properties.Type.Any

    # Verify we can retrieve the same object
    retrieved_tensor = props['tensor']
    assert type(retrieved_tensor) is mi.TensorXf
    assert np.array_equal(np.array(retrieved_tensor), np.array([[1, 2], [3, 4]]))

    # Test that non-nanobind objects are rejected
    with pytest.raises(RuntimeError, match="could not assign an object of type 'dict' to key 'invalid', as the value was not convertible any of the supported property types"):
        props['invalid'] = {'key': 'value'}  # Python dict


def test13_object_identity_preservation(variant_scalar_rgb):
    """Test that Object-derived objects preserve identity when stored/retrieved"""
    props = mi.Properties()

    # Test with a BSDF (Object-derived class)
    bsdf = mi.load_dict({'type': 'diffuse', 'reflectance': 0.5})
    props['bsdf'] = bsdf

    # Verify it's stored as Object type
    assert props.type('bsdf') == mi.Properties.Type.Object

    # Verify identity preservation - should be the same Python object
    retrieved_bsdf = props['bsdf']
    assert retrieved_bsdf is bsdf


def test14_invalid_object_type_assignment(variant_scalar_rgb):
    """Test that assigning wrong object types produces useful error messages"""

    # Test case: BSDF expects a Texture for reflectance, but we give it another BSDF
    other_bsdf = mi.load_dict({'type': 'diffuse', 'reflectance': 0.5})

    with pytest.raises(RuntimeError, match=r"incompatible object type.*expected Texture.*got BSDF"):
        mi.load_dict({
            'type': 'diffuse',
            'reflectance': other_bsdf  # Should be a texture, not a BSDF
        })


def test15_plugin_manager_plugin_type(variant_scalar_rgb):
    """Test PluginManager.plugin_type() method functionality"""
    pmgr = mi.PluginManager.instance()
    # Test known plugin types
    assert pmgr.plugin_type('diffuse') == mi.ObjectType.BSDF
    assert pmgr.plugin_type('path') == mi.ObjectType.Integrator
    assert pmgr.plugin_type('area') == mi.ObjectType.Emitter
    assert pmgr.plugin_type('perspective') == mi.ObjectType.Sensor
    assert pmgr.plugin_type('independent') == mi.ObjectType.Sampler
    assert pmgr.plugin_type('sphere') == mi.ObjectType.Shape
    assert pmgr.plugin_type('bitmap') == mi.ObjectType.Texture
    assert pmgr.plugin_type('hdrfilm') == mi.ObjectType.Film
    assert pmgr.plugin_type('box') == mi.ObjectType.ReconstructionFilter
    # Test unknown/non-existent plugin types
    assert pmgr.plugin_type('nonexistent_plugin') == mi.ObjectType.Unknown
    assert pmgr.plugin_type('') == mi.ObjectType.Unknown
    # Test special case: scene
    assert pmgr.plugin_type('scene') == mi.ObjectType.Scene


def test16_dictionary_like_interface(variant_scalar_rgb):
    """Test new dictionary-like methods and deprecation warnings"""
    from drjit.scalar import Array3f
    import warnings

    p = mi.Properties()
    p['name'] = 'test'
    p['value'] = 42
    p['vector'] = Array3f(1, 2, 3)
    p['flag'] = True

    # Test keys() method
    keys = p.keys()
    expected_keys = ['name', 'value', 'vector', 'flag']
    assert set(keys) == set(expected_keys)
    assert len(keys) == 4

    # Test items() method
    items = p.items()
    assert len(items) == 4

    # Verify items are tuples of (key, value)
    items_dict = dict(items)
    assert items_dict['name'] == 'test'
    assert items_dict['value'] == 42
    assert dr.all(items_dict['vector'] == Array3f(1, 2, 3))
    assert items_dict['flag'] == True

    # Test iteration over items
    for key, value in p.items():
        assert key in expected_keys
        assert dr.all(p[key] == value)

    # Test that deprecated methods show warnings
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")

        # These should trigger deprecation warnings
        _ = p.has_property('name')
        _ = p.property_names()
        p.remove_property('flag')

        # Verify we got deprecation warnings
        assert len(w) == 3
        assert all(issubclass(warning.category, DeprecationWarning) for warning in w)
        assert 'has_property() is deprecated' in str(w[0].message)
        assert 'property_names() is deprecated' in str(w[1].message)
        assert 'remove_property() is deprecated' in str(w[2].message)

def test17_insertion_order_with_deletion():
    """Test that Properties maintains insertion order even after deletion"""
    props = mi.Properties()
    props.set_plugin_name("test")

    # Add properties in a specific order
    props["first"] = 1.0
    props["second"] = "hello"
    props["third"] = mi.ScalarVector3f(1, 2, 3)
    props["fourth"] = True
    props["fifth"] = 42

    # Verify insertion order
    keys = list(props.keys())
    assert keys == ["first", "second", "third", "fourth", "fifth"]

    # Delete a property in the middle
    del props["third"]

    # Verify remaining properties maintain insertion order
    keys = list(props.keys())
    assert keys == ["first", "second", "fourth", "fifth"]

    # Verify values are still correct
    assert props["first"] == 1.0
    assert props["second"] == "hello"
    assert props["fourth"] == True
    assert props["fifth"] == 42

    # Add a new property and verify it's added at the end
    props["sixth"] = mi.ScalarColor3f(0.5, 0.5, 0.5)
    keys = list(props.keys())
    assert keys == ["first", "second", "fourth", "fifth", "sixth"]


def test18_range(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match=r'Property "bsdf_samples": value -3 is out of bounds, must be in the range \[0, 18446744073709551615\]'):
        mi.load_dict({'type': 'direct', 'bsdf_samples': -3})
    mi.load_dict({'type': 'direct', 'bsdf_samples': 10})


def test19_numpy_scalars(variant_scalar_rgb):
    """Test that NumPy scalars are correctly converted without silent rounding"""
    np = pytest.importorskip("numpy")

    props = mi.Properties()

    # float32
    props['a'] = np.float32(1.5)
    assert props['a'] == 1.5
    assert props.type('a') == mi.Properties.Type.Float

    # float64
    props['c'] = np.float64(2.7)
    assert props['c'] == 2.7
    assert props.type('c') == mi.Properties.Type.Float

    # int32
    props['d'] = np.int32(42)
    assert props['d'] == 42
    assert props.type('d') == mi.Properties.Type.Integer

    # int64
    props['e'] = np.int64(-100)
    assert props['e'] == -100
    assert props.type('e') == mi.Properties.Type.Integer

    # uint32
    props['f'] = np.uint32(99)
    assert props['f'] == 99
    assert props.type('f') == mi.Properties.Type.Integer

    # uint64
    props['g'] = np.uint64(123)
    assert props['g'] == 123
    assert props.type('g') == mi.Properties.Type.Integer

    # bool
    props['g'] = np.bool_(True)
    assert props['g'] == True
    assert props.type('g') == mi.Properties.Type.Bool

    # Test that complex types are rejected
    with pytest.raises(RuntimeError, match=r"numpy scalars with a.*'c' data type are not supported"):
        props['j'] = np.complex64(1 + 2j)
