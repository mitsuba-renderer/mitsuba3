"""
Tests of the DirectedEdge half-edge adjacency structure: golden values on
hand-checkable meshes, the guarantees of its contract on generated geometry up
to a few million faces, agreement between the scalar and the data-parallel
builder, and its lifetime inside a Mesh.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import fresolver_append_path

I = 0xffffffff

# Fixtures: faces, vertex count, E2E, V2E, and the four flag sets
FIXTURES = {
    # Two triangles sharing one edge, open elsewhere
    'T1': ([(0, 1, 2), (2, 1, 3)], 4,
           [I, 3, I, 1, I, I], [0, 1, 3, 5],
           {0, 1, 2, 3}, set(), set(), set()),

    # Open fan
    'T2': ([(0, 1, 2), (0, 2, 3), (0, 3, 4)], 5,
           [I, I, 3, 2, I, 6, 5, I, I], [6, 1, 2, 5, 8],
           {0, 1, 2, 3, 4}, set(), set(), set()),

    # Closed tetrahedron: fully paired, no flags
    'T3': ([(0, 2, 1), (0, 1, 3), (0, 3, 2), (1, 2, 3)], 4,
           [8, 9, 3, 2, 11, 6, 5, 10, 0, 1, 7, 4], [0, 2, 1, 5],
           set(), set(), set(), set()),

    # Non-manifold fin: three faces on one edge
    'T4': ([(0, 1, 2), (0, 1, 3), (1, 0, 4)], 5,
           [I] * 9, [0, 1, 2, 5, 8],
           {0, 1, 2, 3, 4}, {0, 1}, {0, 1}, set()),

    # Bowtie: E2E stays fully valid, only the ring at the apex is split
    'T5': ([(0, 1, 2), (0, 2, 3), (0, 3, 1),
            (0, 4, 5), (0, 5, 6), (0, 6, 4)], 7,
           [8, I, 3, 2, I, 6, 5, I, 0, 17, I, 12, 11, I, 15, 14, I, 9],
           [0, 8, 2, 5, 17, 11, 14],
           {1, 2, 3, 4, 5, 6}, set(), {0}, set()),

    # Inconsistent winding
    'T6': ([(0, 1, 2), (0, 1, 3)], 4,
           [I] * 6, [0, 1, 2, 5],
           {0, 1, 2, 3}, set(), {0, 1}, {0, 1}),

    # Degenerate face plus isolated vertex
    'T7': ([(0, 1, 2), (3, 3, 4)], 6,
           [I] * 6, [0, 1, 2, I, I, I],
           {0, 1, 2}, set(), set(), set()),

    # One closed and one open fan at vertex 0. V2E[0] = 12 lies in the OPEN
    # fan, although the smallest half-edge at vertex 0 lies in the closed one
    'T8': ([(0, 1, 2), (0, 2, 3), (0, 3, 1), (0, 4, 5), (0, 5, 6)], 7,
           [8, I, 3, 2, I, 6, 5, I, 0, I, I, 12, 11, I, I],
           [12, 8, 2, 5, 10, 11, 14],
           {0, 1, 2, 3, 4, 5, 6}, set(), {0}, set()),

    # Nothing but degenerate faces: every vertex is isolated
    'T9': ([(0, 0, 0), (1, 1, 2)], 4,
           [I] * 6, [I] * 4,
           set(), set(), set(), set()),

    # The same face twice. All three edges carry two half-edges that agree on
    # their direction, so none of them pairs.
    'T10': ([(0, 1, 2), (0, 1, 2)], 3,
            [I] * 6, [0, 1, 2],
            {0, 1, 2}, set(), {0, 1, 2}, {0, 1, 2}),

    # Two open fans at vertex 0, whose ring starts are 3 and 9. V2E[0] is the
    # smaller of the two, and the walk from it covers only its own fan.
    'T11': ([(0, 1, 2), (0, 2, 3), (0, 4, 5), (0, 5, 6)], 7,
            [I, I, 3, 2, I, I, I, I, 9, 8, I, I],
            [3, 1, 2, 5, 7, 8, 11],
            {0, 1, 2, 3, 4, 5, 6}, set(), {0}, set()),
}


def build(F, vertex_count, module=None):
    return (module or mi).DirectedEdge(
        np.array(F, np.uint32).ravel(), vertex_count)


def flag_set(dedge, flag):
    flags = np.array(dedge.flags())
    return set(np.flatnonzero(flags & int(flag)).tolist())


def grid_mesh(n):
    """An open square patch: ``2 * (n - 1)**2`` triangles, ``4 * (n - 1)``
    boundary vertices"""
    i, j = np.meshgrid(np.arange(n - 1), np.arange(n - 1), indexing='ij')
    return quads(i * n + j, i * n + j + 1, (i + 1) * n + j + 1,
                 (i + 1) * n + j), n * n


def torus_mesh(n):
    """A closed torus: ``2 * n**2`` triangles, no boundary and no defects"""
    i, j = np.meshgrid(np.arange(n), np.arange(n), indexing='ij')
    idx = lambda a, b: (a % n) * n + (b % n)
    return quads(idx(i, j), idx(i, j + 1), idx(i + 1, j + 1),
                 idx(i + 1, j)), n * n


def cylinder_mesh(n):
    """An open tube: ``2 * n * (n - 1)`` triangles and two boundary loops"""
    i, j = np.meshgrid(np.arange(n - 1), np.arange(n), indexing='ij')
    idx = lambda a, b: a * n + (b % n)
    return quads(idx(i, j), idx(i, j + 1), idx(i + 1, j + 1),
                 idx(i + 1, j)), n * n


def quads(a, b, c, d):
    """Split a grid of quads into a consistently wound triangle index buffer"""
    a, b, c, d = (x.ravel().astype(np.uint32) for x in (a, b, c, d))
    faces = np.empty((2 * len(a), 3), np.uint32)
    faces[0::2] = np.stack([a, b, c], 1)
    faces[1::2] = np.stack([a, c, d], 1)
    return faces.ravel()


MESHES = { 'grid': grid_mesh, 'torus': torus_mesh, 'cylinder': cylinder_mesh }


def check_topology(dedge, faces, vertex_count):
    """Assert every guarantee of the class except the ring walk, which needs a
    loop over the vertices. Vectorized, so it also runs on large meshes."""
    E2E = np.array(dedge.E2E())
    V2E = np.array(dedge.V2E())
    valence = np.array(dedge.valence())
    flags = np.array(dedge.flags())

    faces = np.asarray(faces, np.uint32).reshape(-1, 3)
    D = 3 * len(faces)
    src = faces.ravel()
    dst = faces[:, [1, 2, 0]].ravel()
    e = np.arange(D, dtype=np.uint32)
    prv = np.where(e % 3 == 0, e + 2, e - 1)

    degenerate = ((faces[:, 0] == faces[:, 1]) | (faces[:, 1] == faces[:, 2]) |
                  (faces[:, 0] == faces[:, 2]))
    alive = ~np.repeat(degenerate, 3)

    paired = E2E != I
    assert np.all(E2E[paired] < D)

    # Involution, reversal, irreflexivity, non-degeneracy
    assert np.all(E2E[E2E[paired]] == e[paired])
    assert np.all(src[E2E[paired]] == dst[paired])
    assert np.all(dst[E2E[paired]] == src[paired])
    assert np.all(E2E[paired] != e[paired])
    assert not np.any(paired & ~alive)

    assert np.array_equal(valence,
                          np.bincount(src[alive], minlength=vertex_count))

    # Canonical start: the smallest half-edge with an unpaired predecessor,
    # falling back to the smallest one at the vertex
    def smallest(mask):
        out = np.full(vertex_count, I, np.uint32)
        np.minimum.at(out, src[mask], e[mask])
        return out

    ring_start = smallest(alive & (E2E[prv] == I))
    assert np.array_equal(V2E, np.where(ring_start != I, ring_start,
                                        smallest(alive)))

    # Boundary is defined over the whole star, not over one ring
    boundary = np.zeros(vertex_count, bool)
    unpaired = alive & ~paired
    boundary[src[unpaired]] = True
    boundary[dst[unpaired]] = True
    assert np.array_equal(boundary,
                          (flags & int(mi.VertexFlags.Boundary)) != 0)


def check_invariants(dedge, faces, vertex_count):
    """Assert the guarantees documented on the DirectedEdge class"""
    check_topology(dedge, faces, vertex_count)

    E2E = np.array(dedge.E2E())
    V2E = np.array(dedge.V2E())
    valence = np.array(dedge.valence())
    flags = np.array(dedge.flags())

    faces = np.asarray(faces, np.uint32).reshape(-1, 3)
    src = faces.ravel()
    e = np.arange(3 * len(faces), dtype=np.uint32)
    nxt = np.where(e % 3 == 2, e - 2, e + 1)

    degenerate = ((faces[:, 0] == faces[:, 1]) | (faces[:, 1] == faces[:, 2]) |
                  (faces[:, 0] == faces[:, 2]))
    alive = ~np.repeat(degenerate, 3)

    # Ring closure: the walk stays in the star, never repeats, and covers the
    # star exactly when the vertex is not flagged as non-manifold
    for v in range(vertex_count):
        star = e[alive & (src == v)]
        if len(star) == 0:
            assert V2E[v] == I
            continue

        visited, cur = [], V2E[v]
        while True:
            assert cur in star and cur not in visited
            visited.append(cur)
            if E2E[cur] == I:
                break
            cur = nxt[E2E[cur]]
            if cur == V2E[v]:
                break

        assert ((len(visited) != valence[v]) ==
                bool(flags[v] & int(mi.VertexFlags.NonManifoldVertex)))


@pytest.mark.parametrize("name", list(FIXTURES))
def test01_fixtures(variants_all_rgb, name):
    """Golden values on small meshes covering every branch of the
    classification: closed, open, non-manifold edge, non-manifold vertex,
    inconsistent winding and degenerate faces."""
    faces, V, E2E, V2E, boundary, nm_edge, nm_vertex, inconsistent = \
        FIXTURES[name]
    dedge = build(faces, V)

    assert dedge.half_edge_count() == 3 * len(faces)
    assert dedge.vertex_count() == V
    assert list(dedge.E2E()) == E2E
    assert list(dedge.V2E()) == V2E
    assert flag_set(dedge, mi.VertexFlags.Boundary) == boundary
    assert flag_set(dedge, mi.VertexFlags.NonManifoldEdge) == nm_edge
    assert flag_set(dedge, mi.VertexFlags.NonManifoldVertex) == nm_vertex
    assert flag_set(dedge, mi.VertexFlags.InconsistentOrientation) == \
        inconsistent

    for flag, expected in [(mi.VertexFlags.Boundary, boundary),
                           (mi.VertexFlags.NonManifoldEdge, nm_edge),
                           (mi.VertexFlags.NonManifoldVertex, nm_vertex),
                           (mi.VertexFlags.InconsistentOrientation,
                            inconsistent)]:
        assert dedge.flag_count(flag) == len(expected)


@pytest.mark.parametrize("name", list(FIXTURES))
def test02_invariants(variants_all_rgb, name):
    """The contract of the class, asserted exhaustively on the fixtures."""
    faces, V = FIXTURES[name][:2]
    check_invariants(build(faces, V), faces, V)


@fresolver_append_path
@pytest.mark.parametrize("filename", ["cbox_smallbox", "rectangle_uv",
                                      "triangle"])
def test03_invariants_ply(variants_all_rgb, filename):
    """The same invariants on real geometry."""
    mesh = mi.load_dict({
        "type": "ply",
        "filename": f"resources/data/tests/ply/{filename}.ply",
    })
    faces = np.array(mesh.geometric_faces())
    dedge = build(faces, mesh.position_count())
    check_invariants(dedge, faces, mesh.position_count())


def test04_pathological_topology(variants_all_rgb):
    """Two inputs on which a naive grouping degrades to quadratic work. They
    would not fail on a regression, they would hang."""
    # A closed fan of N triangles. Half-edges join the list of their
    # endpoint with fewer incident faces, so the 2N edges of the shared
    # center vertex spread over the outer vertices.
    N = 20000
    i = np.arange(1, N + 1, dtype=np.uint32)
    j = np.where(i == N, 1, i + 1).astype(np.uint32)
    faces = np.stack([np.zeros(N, np.uint32), i, j], 1).ravel()

    dedge = build(faces, N + 1)
    E2E = np.array(dedge.E2E())
    paired = E2E != I

    assert np.all(E2E[E2E[paired]] == np.arange(len(E2E))[paired])
    assert np.array(dedge.valence())[0] == N
    # Every spoke is shared; only the rim is a boundary, and it closes
    assert np.count_nonzero(paired) == 2 * N
    assert dedge.flag_count(mi.VertexFlags.Boundary) == N
    assert dedge.flag_count(mi.VertexFlags.NonManifoldVertex) == 0

    # A 'book': N faces on the single edge (0, 1). Every entry of that list
    # matches, so the scan has to give up once the classification is settled.
    N = 20000
    i = np.arange(2, N + 2, dtype=np.uint32)
    faces = np.stack([np.zeros(N, np.uint32), np.ones(N, np.uint32), i],
                     1).ravel()

    dedge = build(faces, N + 2)
    assert list(np.unique(np.array(dedge.E2E()))) == [I]
    assert dedge.flag_count(mi.VertexFlags.NonManifoldEdge) == 2


@pytest.mark.parametrize("mesh", ["grid", "torus", "cylinder"])
def test05_builders_agree(variants_vec_rgb, mesh):
    """The data-parallel builder of the JIT variants and the scalar one group
    equal keys by unrelated mechanisms, a comparison sort against atomically
    threaded lists, so this checks the grouping design rather than its
    transcription. It also pins that the list order, which races by design,
    does not reach the output."""
    import mitsuba.scalar_rgb as mi_s

    faces, V = MESHES[mesh](120)
    fast = build(faces, V)
    ref = build(faces, V, mi_s)

    for a, b in [(fast.E2E(), ref.E2E()), (fast.V2E(), ref.V2E()),
                 (fast.valence(), ref.valence()),
                 (fast.flags(), ref.flags())]:
        assert np.array_equal(np.array(a), np.array(b))


@pytest.mark.parametrize("mesh", ["grid", "torus", "cylinder", "doubled"])
def test06_large_meshes(variants_vec_rgb, mesh):
    """Over a million faces of generated geometry, in four topologies whose
    boundary and defect counts are known exactly. The torus pins that pairing
    is complete, since a builder returning nothing would also satisfy the
    involution."""
    n, V = 800, 800 * 800
    if mesh == "doubled":
        # Every face of the torus twice, so every edge carries four half-edges
        faces = np.tile(torus_mesh(n)[0], 2)
    else:
        faces = MESHES[mesh](n)[0]
    assert len(faces) // 3 > 10**6

    dedge = build(faces, V)
    check_topology(dedge, faces, V)

    # Boundary, non-manifold edge, non-manifold vertex, inconsistent winding
    expected = { 'grid':     (4 * (n - 1), 0, 0, 0),
                 'torus':    (0,           0, 0, 0),
                 'cylinder': (2 * n,       0, 0, 0),
                 'doubled':  (V,           V, V, 0) }[mesh]

    for flag, count in zip([mi.VertexFlags.Boundary,
                            mi.VertexFlags.NonManifoldEdge,
                            mi.VertexFlags.NonManifoldVertex,
                            mi.VertexFlags.InconsistentOrientation], expected):
        assert dedge.flag_count(flag) == count


def test07_invalid_input(variants_all_rgb):
    """A mesh always has at least one face and one vertex, so degenerate input
    is rejected rather than silently accepted."""
    with pytest.raises(RuntimeError, match="not a multiple of 3"):
        mi.DirectedEdge(np.array([0, 1], np.uint32), 2)

    with pytest.raises(RuntimeError, match="at least one face"):
        build([], 3)

    with pytest.raises(RuntimeError, match="at least one face"):
        build([(0, 1, 2)], 0)


def test08_defects_are_reported(variants_all_rgb, capfd):
    """Broken input yields a sound but degraded structure, and says so."""
    faces, V = FIXTURES['T4'][:2]
    build(faces, V)
    assert "non-manifold" in capfd.readouterr().out

    faces, V = FIXTURES['T3'][:2]
    build(faces, V)
    assert "non-manifold" not in capfd.readouterr().out


def test09_accessors(variants_all_rgb):
    """The indexed accessors agree with the raw buffers."""
    faces, V, E2E, V2E = FIXTURES['T5'][:4]
    dedge = build(faces, V)
    valence = np.array(dedge.valence())

    for i in range(len(E2E)):
        assert dr.all(dedge.opposite(mi.UInt32(i)) == E2E[i])
    for v in range(V):
        assert dr.all(dedge.vertex_edge(mi.UInt32(v)) == V2E[v])
        assert dr.all(dedge.valence(mi.UInt32(v)) == int(valence[v]))
        # The apex carries the split ring, every other vertex the boundary
        expected = (mi.VertexFlags.NonManifoldVertex if v == 0
                    else mi.VertexFlags.Boundary)
        assert dr.all(dedge.vertex_flags(mi.UInt32(v)) == int(expected))

    # The half-edges of a face are consecutive and wind with it
    assert dr.all(mi.DirectedEdge.next(mi.UInt32(0)) == 1)
    assert dr.all(mi.DirectedEdge.next(mi.UInt32(2)) == 0)
    assert dr.all(mi.DirectedEdge.prev(mi.UInt32(0)) == 2)
    assert dr.all(mi.DirectedEdge.prev(mi.UInt32(4)) == 3)
    assert dr.all(mi.DirectedEdge.face(mi.UInt32(2)) == 0)
    assert dr.all(mi.DirectedEdge.face(mi.UInt32(3)) == 1)
    assert dr.all(mi.DirectedEdge.corner(mi.UInt32(2)) == 2)
    assert dr.all(mi.DirectedEdge.corner(mi.UInt32(3)) == 0)


def test10_mesh_lifetime(variants_all_rgb):
    """The structure is built on demand, survives position-value edits, and is
    discarded when the position count or topology changes."""
    mesh = mi.Mesh("quad", faces=[[0, 1, 2], [0, 2, 3]],
                   positions=np.float32([[0, 0, 0], [1, 0, 0],
                                         [1, 1, 0], [0, 1, 0]]))

    # Before construction the JIT-facing accessor must not build anything
    assert dr.all(mesh.opposite_dedge(mi.UInt32(2)) == I)

    dedge = mesh.directed_edges()
    assert list(dedge.E2E()) == [I, I, 3, 2, I, I]
    assert mesh.directed_edges() is dedge
    assert dr.all(mesh.opposite_dedge(mi.UInt32(2)) == 3)

    params = mi.traverse(mesh)
    params['positions'] = mi.TensorXf(np.float32([[0, 0, 0], [2, 0, 0],
                                                  [2, 2, 0], [0, 2, 0]]))
    params.update()
    assert mesh.directed_edges() is dedge

    params['positions'] = mi.TensorXf(np.float32([
        [0, 0, 0], [2, 0, 0], [2, 2, 0], [0, 2, 0], [3, 3, 0]
    ]))
    params.update()
    assert mesh.position_count() == 5
    assert dr.all(mesh.opposite_dedge(mi.UInt32(2)) == I)

    resized_dedge = mesh.directed_edges()
    assert resized_dedge is not dedge
    assert resized_dedge.vertex_count() == 5
    assert list(resized_dedge.V2E()) == [3, 1, 2, 5, I]

    params['faces'] = mi.TensorXu(np.uint32([[0, 1, 2], [2, 1, 3]]))
    params.update()
    assert dr.all(mesh.opposite_dedge(mi.UInt32(2)) == I)
    assert list(mesh.directed_edges().E2E()) == [I, 3, I, 1, I, I]


@fresolver_append_path
def test11_seamed_mesh(variants_all_rgb):
    """Pairing runs on the geometric topology, so a closed mesh whose faces are
    separate UV islands has no unpaired half-edge."""
    positions = np.float32([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]])
    tris = [(0, 2, 1), (0, 1, 3), (0, 3, 2), (1, 2, 3)]
    uv = np.float32([(10 * f + i, f) for f in range(4) for i in range(3)])

    mesh = mi.Mesh("tet")
    mesh.from_corners(positions=positions,
                      corner_vertex=np.uint32(tris).ravel(), texcoords=uv)
    assert mesh.vertex_count() == 12 and mesh.position_count() == 4

    dedge = mesh.directed_edges()
    assert dedge.vertex_count() == 4
    assert I not in list(dedge.E2E())
    assert dedge.flag_count(mi.VertexFlags.Boundary) == 0


def test12_frozen(variants_vec_rgb):
    """The structure participates in Dr.Jit's object traversal, so a frozen
    function that gathers through it sees its buffers as dependencies."""
    mesh = mi.Mesh("quad", faces=[[0, 1, 2], [0, 2, 3]],
                   positions=np.float32([[0, 0, 0], [1, 0, 0],
                                         [1, 1, 0], [0, 1, 0]]))
    mesh.directed_edges()

    @dr.freeze
    def func(m, e):
        return m.opposite_dedge(e)

    index = dr.arange(mi.UInt32, 6)
    expected = mi.UInt32([I, I, 3, 2, I, I])
    for _ in range(3):
        assert dr.all(func(mi.MeshPtr(mesh), index) == expected)
    assert func.n_recordings == 1
