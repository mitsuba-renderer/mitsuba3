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
  p_err=[0, 0, 0],
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


def make_python_spheres():
    """A row of spheres implemented as one Python shape with several
    primitives. The JIT variants intersect it through the recording of its
    ``ray_intersect_preliminary()``, which receives the primitive index."""
    class PySpheres(mi.Shape):
        def __init__(self, props):
            super().__init__(props)
            self.count = props.get('count', 1)
            self.radius_s = props.get('radius', 1.0)

            # Opaque parameters are captured by the recording instead of
            # being baked into the compiled intersection code
            self.radius = mi.Float(self.radius_s)
            dr.make_opaque(self.radius)

        def primitive_count(self):
            return self.count

        def bbox(self, prim_index=None):
            lo = 0 if prim_index is None else 2 * prim_index
            hi = 2 * (self.count - 1) if prim_index is None else lo
            r = self.radius_s
            return mi.ScalarBoundingBox3f([lo - r, -r, -r], [hi + r, r, r])

        def center(self, prim_index):
            return mi.Point3f(2 * mi.Float(prim_index), 0, 0)

        def ray_intersect_preliminary(self, ray, prim_index=0, active=True):
            o = ray.o - self.center(prim_index)
            a = dr.squared_norm(ray.d)
            b = 2 * dr.dot(o, ray.d)
            c = dr.squared_norm(o) - self.radius**2
            disc = b*b - 4*a*c
            sq = dr.sqrt(dr.maximum(disc, 0))
            t0, t1 = (-b - sq) / (2*a), (-b + sq) / (2*a)
            t = dr.select(t0 >= 0, t0, t1)

            pi = dr.zeros(mi.PreliminaryIntersection3f)
            pi.valid = active & (disc >= 0) & (t >= 0) & (t <= ray.maxt)
            pi.t = dr.select(pi.valid, t, dr.inf)
            pi.prim_index = prim_index
            return pi

        def compute_surface_interaction(self, ray, pi, ray_flags, active):
            si = dr.zeros(mi.SurfaceInteraction3f)
            si.t = dr.select(active, pi.t, dr.inf)
            si.p = ray(si.t)
            si.n = si.sh_frame.n = dr.normalize(si.p - self.center(pi.prim_index))
            si.prim_index = pi.prim_index
            si.shape = pi.shape
            return si

    mi.register_shape('pyspheres', PySpheres)


def test04_python_shape(variants_vec_rgb):
    """The backend intersects each primitive of a Python shape"""
    make_python_spheres()
    r = 0.7
    scene = mi.load_dict({'type': 'scene', 'shape': {
        'type': 'pyspheres', 'count': 3, 'radius': r}})

    shape = scene.shapes()[0]
    bounds = mi.Shape.bbox(shape, prim_index=1)
    assert dr.allclose(bounds.min, [2 - r, -r, -r])
    assert dr.allclose(bounds.max, [2 + r, r, r])
    clip = mi.ScalarBoundingBox3f([2, 0, 0], [3, 1, 1])
    clipped = mi.Shape.bbox(shape, prim_index=1, clip=clip)
    assert dr.allclose(clipped.min, [2, 0, 0])
    assert dr.allclose(clipped.max, [2 + r, r, r])

    # Rays along +z past the spheres at x = 0, 2, 4
    x = dr.linspace(mi.Float, -1, 5, 61)
    ray = mi.Ray3f(mi.Point3f(x, 0.2, -5), mi.Vector3f(0, 0, 1))
    index = dr.clip(dr.round(x / 2), 0, 2)
    d = dr.sqrt((x - 2 * index)**2 + 0.2**2)
    hit = d < r
    t_ref = 5 - dr.safe_sqrt(r**2 - d**2)

    # The second round runs after the compiled callbacks were dropped
    for i in range(2):
        si = scene.ray_intersect(ray)
        assert dr.all(si.is_valid() == hit)
        assert dr.allclose(dr.select(hit, si.t, t_ref), t_ref)
        assert dr.all(dr.select(hit, si.prim_index, mi.UInt32(index)) == mi.UInt32(index))
        assert dr.all(scene.ray_test(ray) == hit)
        dr.flush_kernel_cache()


def test05_python_shape_side_effect(variants_vec_backends_once_rgb):
    """The recorded intersection routine must not scatter"""
    class BadShape(mi.Shape):
        def __init__(self, props):
            super().__init__(props)
            self.buf = dr.zeros(mi.Float, 1)

        def bbox(self, *args):
            return mi.ScalarBoundingBox3f([-1, -1, -1], [1, 1, 1])

        def ray_intersect_preliminary(self, ray, prim_index=0, active=True):
            dr.scatter(self.buf, ray.maxt, 0)
            return dr.zeros(mi.PreliminaryIntersection3f)

    mi.register_shape('badshape', BadShape)
    with pytest.raises(RuntimeError, match='side effect'):
        mi.load_dict({'type': 'scene', 'shape': {'type': 'badshape'}})


def test06_to_world_and_animated_to_world(variant_scalar_rgb):
    """Shape.to_world() and to_world_scalar() evaluate the animation at a time"""
    at = mi.AnimatedTransform4f({
        0.0: mi.ScalarAffineTransform4f.translate([0, 0, 0]),
        1.0: mi.ScalarAffineTransform4f.translate([10, 0, 0])
    })

    scene = mi.load_dict({
        'type': 'scene',
        'sg': {
            'type': 'shapegroup',
            'sphere': {'type': 'sphere'}
        },
        'inst': {
            'type': 'instance',
            'shapegroup': {'type': 'ref', 'id': 'sg'},
            'to_world': at
        }
    })

    shape = scene.shapes()[0]
    assert shape.animated_to_world().is_animated()
    assert dr.allclose(shape.to_world().translation(), [0, 0, 0])
    assert dr.allclose(shape.to_world(0.5).translation(), [5, 0, 0])
    assert dr.allclose(shape.to_world(1.0).translation(), [10, 0, 0])
    assert dr.allclose(shape.to_world_scalar().translation(), [0, 0, 0])
    assert dr.allclose(shape.to_world_scalar(0.5).translation(), [5, 0, 0])
    assert dr.allclose(shape.to_world_scalar(1.0).translation(), [10, 0, 0])
