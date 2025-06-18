import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path

def test01_ss_repr(variants_vec_backends_once_rgb):
    # Test that the class is bound and prints itself
    ss = dr.zeros(mi.SilhouetteSample3f, 3)
    expected = """SilhouetteSample[
  p=[[0, 0, 0],
     [0, 0, 0],
     [0, 0, 0]],
  n=[[0, 0, 0],
     [0, 0, 0],
     [0, 0, 0]],
  uv=[[0, 0],
      [0, 0],
      [0, 0]],
  time=[0, 0, 0],
  pdf=[0, 0, 0],
  delta=[0, 0, 0],
  discontinuity_type=[0, 0, 0],
  d=[[0, 0, 0],
     [0, 0, 0],
     [0, 0, 0]],
  silhouette_d=[[0, 0, 0],
                [0, 0, 0],
                [0, 0, 0]],
  prim_index=[0, 0, 0],
  scene_index=[0, 0, 0],
  flags=[0, 0, 0],
  projection_index=[0, 0, 0],
  shape=[0x0, 0x0, 0x0],
  foreshortening=[0, 0, 0],
  offset=[0, 0, 0]
]
            """
    assert expected.strip() == str(ss)


def test02_shape_ptr(variants_vec_backends_once_rgb):
    scene = mi.load_dict(mi.cornell_box())
    shapes = scene.shapes_dr()

    shapes_int = dr.reinterpret_array(mi.UInt32, shapes)
    print(shapes_int)
    assert isinstance(shapes_int, mi.UInt32)

    shapes_ptr = dr.reinterpret_array(mi.ShapePtr, shapes_int)
    print(shapes_ptr)
    assert isinstance(shapes_ptr, mi.ShapePtr)


@fresolver_append_path
def test03_shape_set_bsdf(variants_all_backends_once):
    """Check that the BSDF can be correctly updated on shapes."""

    scene = mi.load_dict(mi.cornell_box())
    new_bsdf = mi.load_dict({ "type": "conductor" })

    # Updating on a single shape (analytic or mesh, already part of a scene or not)
    shapes = [
        mi.load_dict({ "type": "sphere" }),
        scene.shapes()[0],
        mi.load_dict({
            "type": "ply",
            "filename" : "resources/data/tests/ply/rectangle_uv.ply",
        }),
    ]
    for shape in shapes:
        assert not mi.has_flag(shape.bsdf().flags(), mi.BSDFFlags.DeltaReflection)
        shape.set_bsdf(new_bsdf)
        assert mi.has_flag(shape.bsdf().flags(), mi.BSDFFlags.DeltaReflection)

    # Cannot set the BSDF to None
    with pytest.raises(TypeError, match="incompatible function arguments"):
        shapes[-1].set_bsdf(None)
    assert shapes[-1].bsdf() is not None

    if dr.is_jit_v(mi.Float):
        shapes_ptr = scene.shapes_dr()
        # Cannot set BSDFs via a ShapePtr
        with pytest.raises(AttributeError, match="has no attribute 'set_bsdf'"):
            shapes_ptr.set_bsdf(new_bsdf)

        # But the new BSDF should be reflected when getting a BSDFPtr
        bsdfs_ptr = shapes_ptr.bsdf()
        assert dr.count(bsdfs_ptr == new_bsdf) == 1

    # New BSDF should be found via `traverse`
    props = mi.traverse(shapes[0])
    assert "bsdf.eta.value" in props


    # We should be able to override `set_bsdf()` in custom Mesh plugins
    class MyMesh(mi.Mesh):
        def __init__(self, props):
            super().__init__(props)
            self.called = False

        def set_bsdf(self, bsdf):
            super().set_bsdf(bsdf)
            self.called = True

    mi.register_shape("mymesh", MyMesh)

    custom_shape = mi.load_dict({
        "type": "mymesh"
    })
    assert not mi.has_flag(custom_shape.bsdf().flags(), mi.BSDFFlags.DeltaReflection)
    custom_shape.set_bsdf(new_bsdf)
    assert mi.has_flag(custom_shape.bsdf().flags(), mi.BSDFFlags.DeltaReflection)
    assert custom_shape.called
