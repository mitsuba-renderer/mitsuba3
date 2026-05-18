import pytest

import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path


def example_mesh(**kwargs):
    return {
        "type" : "ply",
        "filename" : "resources/data/tests/ply/triangle.ply",
        "face_normals" : True,
        **kwargs
    }


def example_mesh_obj(bsdf=None):
    """Load an example mesh as a Python object, optionally sharing a BSDF."""
    d = example_mesh()
    if bsdf is not None:
        d["bsdf"] = bsdf
    return mi.load_dict(d)


@fresolver_append_path
def test01_merge_single_shape(variants_all_backends_once):
    # One shape on its own, no BSDF
    m = mi.load_dict({
        "type": "merge",
        "child1": example_mesh(),
    })
    assert isinstance(m, mi.Mesh)
    assert m.id() == "child1"

    # One shape in a scene, no BSDF
    m = mi.load_dict({
        "type": "scene",
        "parent": {
            "type": "merge",
            "child1": example_mesh(),
        },
    })
    assert len(m.shapes()) == 1
    assert m.shapes()[0].id() == "parent"

    # One shape in a scene, with a BSDF
    m = mi.load_dict({
        "type": "scene",
        "parent": {
            "type": "merge",
            "child1": example_mesh(bsdf={ "type": "diffuse" }),
        },
    })
    assert len(m.shapes()) == 1
    assert m.shapes()[0].id() == "parent"

    # Non-mesh --> should be just passed through
    m = mi.load_dict({
        "type": "merge",
        "child1": {
            "type": "sphere",
        },
    })
    assert isinstance(m, mi.Shape)
    assert m.id() == "child1"


@fresolver_append_path
def test02_two_shapes(variants_all_rgb):
    # No BSDF --> doesn't merge
    m = mi.load_dict({
        "type": "merge",
        "child1": example_mesh(),
        "child2": example_mesh(),
    })
    assert len(m) == 2
    assert set([m[0].id(), m[1].id()]) == {"child1", "child2"}

    # No BSDF --> doesn't merge
    m = mi.load_dict({
        "type": "merge",
        "child1": example_mesh(bsdf={ "type": "diffuse" }),
        "child2": example_mesh(),
    })
    assert len(m) == 2
    assert set([m[0].id(), m[1].id()]) == {"child1", "child2"}

    # Same BSDF: merge
    m = mi.load_dict({
        "type": "scene",
        "bsdf1": { "type": "diffuse" },
        "parent": {
            "type": "merge",
            "child1": example_mesh(bsdf={ "type": "ref", "id": "bsdf1" }),
            "child2": example_mesh(bsdf={ "type": "ref", "id": "bsdf1" }),
        }
    })
    assert len(m.shapes()) == 1
    assert m.shapes()[0].id() == "parent"

    # Non-mesh --> doesn't merge
    m = mi.load_dict({
        "type": "scene",
        "bsdf1": { "type": "diffuse" },
        "parent": {
            "type": "merge",
            "child1": example_mesh(bsdf={ "type": "ref", "id": "bsdf1" }),
            "child2": {
                "type": "sphere",
                "bsdf": { "type": "ref", "id": "bsdf1" }
            },
        }
    })
    assert len(m.shapes()) == 2
    assert set([m.shapes()[0].id(), m.shapes()[1].id()]) == {"parent", "child2"}


@fresolver_append_path
def test03_merge_many_pair(variants_all_backends_once):
    """`Mesh.merge_many([m1, m2])` returns a merged mesh whose vertex/face
    counts are the sum of the inputs."""
    bsdf = mi.load_dict({"type": "diffuse"})
    m1 = example_mesh_obj(bsdf)
    m2 = example_mesh_obj(bsdf)

    merged = mi.Mesh.merge_many([m1, m2])
    assert isinstance(merged, mi.Mesh)
    assert merged.vertex_count() == m1.vertex_count() + m2.vertex_count()
    assert merged.face_count()   == m1.face_count()   + m2.face_count()


@fresolver_append_path
def test04_merge_many_n(variants_all_backends_once):
    """Same, but with five inputs."""
    bsdf = mi.load_dict({"type": "diffuse"})
    meshes = [example_mesh_obj(bsdf) for _ in range(5)]
    merged = mi.Mesh.merge_many(meshes)
    assert merged.vertex_count() == sum(m.vertex_count() for m in meshes)
    assert merged.face_count()   == sum(m.face_count()   for m in meshes)


@fresolver_append_path
def test05_merge_many_singleton(variants_all_backends_once):
    """A 1-element input is returned unchanged (no merge work)."""
    m = example_mesh_obj()
    assert mi.Mesh.merge_many([m]) is m


def test06_merge_many_empty(variants_all_backends_once):
    """Calling on an empty list raises an exception."""
    with pytest.raises(RuntimeError):
        mi.Mesh.merge_many([])


@fresolver_append_path
def test07_merge_many_incompatible(variants_all_backends_once):
    """Meshes with different attached BSDFs cannot be merged."""
    # Each `mi.load_dict` here creates its own default BSDF, so the two
    # meshes don't share the bsdf pointer that the compatibility check uses.
    m1 = example_mesh_obj()
    m2 = example_mesh_obj()
    with pytest.raises(RuntimeError):
        mi.Mesh.merge_many([m1, m2])


@fresolver_append_path
def test08_merge_many_face_indices(variants_all_backends_once):
    """After a merge, the second input's face indices must be rewritten to
    point into the second half of the merged vertex buffer. We verify the
    weaker, robust property that every face index is in bounds."""
    import drjit as dr
    bsdf = mi.load_dict({"type": "diffuse"})
    m1 = example_mesh_obj(bsdf)
    m2 = example_mesh_obj(bsdf)

    merged = mi.Mesh.merge_many([m1, m2])
    faces = merged.faces_buffer()
    assert dr.all(faces < merged.vertex_count())


@fresolver_append_path
def test09_pairwise_merge(variants_all_backends_once):
    """The instance pairwise `m.merge(other)` still works and produces the
    same counts as the batched 2-input call."""
    bsdf = mi.load_dict({"type": "diffuse"})
    m1 = example_mesh_obj(bsdf)
    m2 = example_mesh_obj(bsdf)

    merged_pair  = m1.merge(m2)
    merged_batch = mi.Mesh.merge_many([m1, m2])
    assert merged_pair.vertex_count()  == merged_batch.vertex_count()
    assert merged_pair.face_count()    == merged_batch.face_count()


@fresolver_append_path
def test10_merge_many_vertex_positions(variants_all_backends_once):
    """The merged `vertex_positions` buffer is the concatenation of the
    inputs' position buffers."""
    bsdf = mi.load_dict({"type": "diffuse"})
    m1 = example_mesh_obj(bsdf)
    m2 = example_mesh_obj(bsdf)

    merged = mi.Mesh.merge_many([m1, m2])
    p1 = list(m1.vertex_positions_buffer())
    p2 = list(m2.vertex_positions_buffer())
    assert list(merged.vertex_positions_buffer()) == p1 + p2


@fresolver_append_path
def test11_merge_many_faces_offset(variants_all_backends_once):
    """The merged `faces` buffer is the concatenation of the inputs' face
    buffers, with the second input's indices shifted by `m1.vertex_count()`
    so they reference the second half of the merged vertex buffer."""
    bsdf = mi.load_dict({"type": "diffuse"})
    m1 = example_mesh_obj(bsdf)
    m2 = example_mesh_obj(bsdf)

    merged = mi.Mesh.merge_many([m1, m2])
    f1 = list(m1.faces_buffer())
    f2 = list(m2.faces_buffer())
    vc1 = m1.vertex_count()
    assert list(merged.faces_buffer()) == f1 + [i + vc1 for i in f2]


@fresolver_append_path
def test12_merge_many_vertex_normals(variants_all_backends_once):
    """The merged `vertex_normals` buffer is the concatenation of the
    inputs' normal buffers when both inputs carry per-vertex normals."""
    bsdf = mi.load_dict({"type": "diffuse"})
    # `face_normals=False` makes the PLY loader generate per-vertex normals.
    d = example_mesh()
    d["face_normals"] = False
    d["bsdf"] = bsdf
    m1 = mi.load_dict(d)
    m2 = mi.load_dict(d)
    assert m1.has_vertex_normals() and m2.has_vertex_normals()

    merged = mi.Mesh.merge_many([m1, m2])
    n1 = list(m1.vertex_normals_buffer())
    n2 = list(m2.vertex_normals_buffer())
    assert list(merged.vertex_normals_buffer()) == n1 + n2
