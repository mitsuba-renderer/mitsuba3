#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/fwd.h>
#include <drjit/dynamic.h>

NAMESPACE_BEGIN(mitsuba)

/// Per-vertex boundary and defect flags reported by `DirectedEdge`
enum class VertexFlags : uint32_t {
    /// An edge incident to the vertex is unpaired
    Boundary = 0x1,

    /// The vertex is an endpoint of an edge with more than two faces
    NonManifoldEdge = 0x2,

    /// The faces around the vertex form more than one ring (e.g. a bowtie)
    NonManifoldVertex = 0x4,

    /// The vertex is an endpoint of an edge whose two faces wind the same way
    InconsistentOrientation = 0x8
};

MI_DECLARE_ENUM_OPERATORS(VertexFlags)

/**
 * Immutable half-edge adjacency for an indexed triangle mesh
 *
 * This class derives edge and vertex adjacency from a flat triangle index
 * buffer ``F`` using the directed-edge representation of Campagna et al.
 * :cite:`Campagna1998DirectedEdges`.
 * It supports queries such as finding the triangle across an edge, traversing
 * the triangles around a vertex, processing each mesh edge once, and
 * identifying boundaries or malformed connectivity.
 *
 * The result is purely combinatorial and immutable. It stores neither vertex
 * positions nor a copy of the index buffer.
 *
 * .. rubric:: Representation
 *
 * ``F`` stores the three vertex indices of each triangle consecutively and in
 * winding order. Triangle ``f`` therefore contributes three half-edges
 * ``e = 3*f + i``, where ``i`` is in ``{0, 1, 2}``. Their basic relationships
 * are:
 *
 * .. code-block:: python
 *
 *    next(e)             = 3*f + (i + 1) % 3
 *    prev(e)             = 3*f + (i + 2) % 3
 *    face(e)             = e / 3
 *    corner(e)           = e % 3
 *    source(e)           = F[e]
 *    target(e)           = F[next(e)]
 *    opposing_vertex(e)  = F[prev(e)]
 *
 * Hence, `next()` advances along the triangle's winding direction and
 * `prev()` goes in the opposite direction; neither operation leaves the
 * triangle.
 *
 * The structure provides four principal arrays:
 *
 * * `E2E()` maps each half-edge to the oppositely oriented half-edge of
 *   the adjacent triangle, or to ``Invalid`` when the result is ambiguous.
 *
 * * `V2E()` maps each vertex to a canonical outgoing half-edge, or to
 *   ``Invalid`` when no triangle with three distinct indices contains it.
 *
 * * `valence()` records how many triangles with three distinct indices
 *   contain each vertex.
 *
 * * `flags()` records per-vertex boundary and connectivity diagnostics.
 *
 * On a consistently oriented manifold triangle mesh, each interior edge is
 * shared by two triangles, which contribute oppositely oriented half-edges.
 * ``E2E`` pairs these half-edges. A boundary edge belongs to only one triangle,
 * and its half-edge maps to ``Invalid``.
 *
 * ``V2E[v]`` provides a starting point for walking around vertex ``v``. For
 * an interior vertex, any outgoing half-edge would work; ``V2E[v]`` contains
 * the smallest one for deterministic results. At a boundary vertex,
 * ``V2E[v] = next(b)``, where ``b`` is the incoming boundary half-edge.
 * Starting there, repeatedly applying ``e = next(opposite(e))`` visits the
 * complete fan of faces. The walk stops at the other boundary edge, where
 * ``opposite(e)`` is ``Invalid``.
 *
 * The examples below use Python-style scalar pseudocode.
 *
 * .. rubric:: Example 1: querying the neighborhood of an edge
 *
 * The following snippet obtains the two faces ``f0`` and ``f1`` that share
 * an interior manifold edge (which is the case when ``o != Invalid``).
 *
 * .. code-block:: python
 *
 *    f0 = de.face(e)
 *    o  = de.opposite(e)  # Load E2E[e]
 *    f1 = de.face(o)
 *
 * The two triangles share the edge endpoints ``v0`` and ``v1``. Each triangle
 * also has one vertex opposite the shared edge, denoted by ``c0`` and ``c1``.
 * These are obtained as follows:
 *
 * .. code-block:: python
 *
 *    v0 = F[e]
 *    v1 = F[de.next(e)]
 *    c0 = F[de.prev(e)]
 *    c1 = F[de.prev(o)]
 *
 * This local neighborhood is useful when computing cotangent weights,
 * curvature, etc.
 *
 * .. rubric:: Example 2: traversing the faces around a vertex
 *
 * Starting at `vertex_edge()`, repeatedly crossing the current edge
 * and advancing within the neighboring triangle walks around its source
 * vertex:
 *
 * .. code-block:: python
 *
 *    start = de.vertex_edge(v)  # Load V2E[v]
 *    e, visited = start, 0
 *
 *    while e != de.Invalid:
 *        visit_face(de.face(e))
 *        visited += 1
 *
 *        o = de.opposite(e)
 *        if o == de.Invalid:
 *            break
 *
 *        e = de.next(o)
 *        if e == start:
 *            break
 *
 * A closed fan returns to ``start``. An open fan terminates at an unpaired
 * edge. For a manifold vertex, ``visited`` equals ``valence(v)``.
 *
 * Each visited half-edge starts at ``v``, so its target is one of ``v``'s
 * neighbors. For a closed fan, these targets include every neighbor. For an
 * open fan, the incoming boundary edge is not visited because it points toward
 * ``v``. The missing neighbor is ``F[de.prev(start)]``.
 *
 * These neighborhoods are commonly used by Laplacian and curvature operators.
 *
 * .. rubric:: Robustness
 *
 * The implementation only pairs half-edges when the result is unambiguous.
 * Whenever ``o = E2E[e]`` is not ``Invalid``, the following invariants hold:
 *
 * .. code-block:: python
 *
 *    E2E[o]     == e
 *    F[o]       == F[next(e)]
 *    F[next(o)] == F[e]
 *
 * These invariants remain true in the presence of boundaries and malformed
 * topology.
 *
 * .. rubric:: Boundaries and edge defects
 *
 * On a well-formed mesh, every edge belongs to at most two triangles. An
 * interior edge is shared by two triangles with oppositely oriented
 * half-edges, which ``E2E`` pairs up. A boundary edge belongs to a single
 * triangle, and its half-edge maps to ``Invalid``. Both endpoints of such
 * an edge receive the `VertexFlags.Boundary` flag. Triangles with
 * repeated vertex indices are ignored throughout (see below).
 *
 * Malformed input deviates from this picture in two ways. In both cases
 * there is no valid way to pair the involved half-edges, so all of them
 * remain unpaired and read as boundaries. The endpoints of the affected
 * edge receive `VertexFlags.Boundary` and a flag identifying the
 * defect:
 *
 * - When the two half-edges of a shared edge are oriented the same way,
 *   one triangle is wound backwards relative to the other. The endpoints
 *   receive `VertexFlags.InconsistentOrientation`.
 *
 * - When three or more triangles share an edge (think of a fin attached to
 *   the edge of a box), no pair is preferable to the others. The endpoints
 *   receive `VertexFlags.NonManifoldEdge`.
 *
 * The following table summarizes the classification:
 *
 * ========  ==========  ===========  =========================================
 * Faces     Winding     E2E entries  Flags at both endpoints
 * ========  ==========  ===========  =========================================
 * 1         --          Invalid      ``Boundary``
 * 2         opposite    paired       none
 * 2         same        Invalid      ``Boundary | InconsistentOrientation``
 * >= 3      any         Invalid      ``Boundary | NonManifoldEdge``
 * ========  ==========  ===========  =========================================
 *
 * A vertex accumulates the flags of all edges that touch it, so several
 * bits may be set at once. Test them individually using ``has_flag()``
 * rather than comparing the complete bitmask for equality. The remaining
 * flag, `VertexFlags.NonManifoldVertex`, is not part of the edge
 * classification and is explained next.
 *
 * .. rubric:: Disconnected fans around a vertex
 *
 * `VertexFlags.NonManifoldVertex` marks vertices where the walk of
 * Example 2 reaches fewer faces than `valence()` reports. The
 * typical case is a bowtie: two fans that touch only at their common apex.
 * Every edge of such a configuration may pair normally, and those pairings
 * are preserved. The defect is a property of the vertex, not of its edges.
 * `vertex_edge()` selects a starting point in one of the fans, and the
 * walk covers only that fan.
 *
 * To enumerate every face around a non-manifold vertex, scan ``F`` for
 * half-edges with ``F[e] == v`` (again skipping triangles with repeated
 * indices) instead of walking from `vertex_edge()`.
 *
 * .. rubric:: Triangles with repeated indices
 *
 * A triangle such as ``(a, a, b)`` has no area and is ignored as a whole:
 * it keeps its slots in the half-edge numbering, but its three ``E2E``
 * entries are ``Invalid``, its half-edges never appear in `V2E()`, and
 * it contributes no face counts or flags. The remaining triangles are
 * paired as if it did not exist.
 *
 * One consequence is that ``E2E[e] == Invalid`` alone does not identify a
 * boundary edge: code that scans all half-edges must first skip those of
 * triangles with repeated indices.
 *
 * .. rubric:: Geometric and input limitations
 *
 * This structure only examines vertex indices. It cannot detect zero-area
 * geometry caused by collinear vertices or distinct indices that refer to the
 * same position, nor can it detect self-intersections. The constructor also
 * does not validate index bounds: every entry of ``F`` must be smaller than
 * ``vertex_count``, and violations cause undefined behavior.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB DirectedEdge : public Object {
public:
    MI_IMPORT_CORE_TYPES()

    using IndexBuffer = DynamicBuffer<UInt32>;

    /// Sentinel denoting a half-edge that does not exist
    static constexpr uint32_t Invalid = (uint32_t) -1;

    /**
     * Build the adjacency structure of a triangle mesh
     *
     * The caller must ensure that ``F[i] < vertex_count`` holds for all
     * provided indices. Violations cause undefined behavior.
     *
     * Args:
     *     F: A flat index buffer holding the three vertex indices of each
     *         triangle face, in winding order. Its size must be a multiple of 3.
     *
     *     vertex_count: The number of mesh vertices.
     *
     *     name: An optional name identifying the mesh in log messages.
     *
     *     warn_defects: Report non-manifold or inconsistently wound input in a log message.
     *         Counting the affected vertices requires a device-to-host transfer,
     *         which callers building the structure implicitly may wish to avoid.
     */
    DirectedEdge(const IndexBuffer &F, uint32_t vertex_count,
                 std::string_view name = "", bool warn_defects = true);

    /// Return the next half-edge within the same face
    template <typename Index> static Index next(const Index &e) {
        return dr::select(e % 3u == 2u, e - 2u, e + 1u);
    }

    /// Return the previous half-edge within the same face
    template <typename Index> static Index prev(const Index &e) {
        return dr::select(e % 3u == 0u, e + 2u, e - 1u);
    }

    /// Return the index of the face containing the half-edge
    template <typename Index> static Index face(const Index &e) {
        return e / 3u;
    }

    /// Return the local corner index of the half-edge within its face
    template <typename Index> static Index corner(const Index &e) {
        return e % 3u;
    }

    /// Number of half-edges, i.e. three times the face count
    uint32_t half_edge_count() const { return m_half_edge_count; }

    /// Number of vertices in the numbering used by the per-vertex outputs
    uint32_t vertex_count() const { return m_vertex_count; }

    /// Return the name passed to the constructor
    const std::string &name() const { return m_name; }

    /**
     * Return the half-edge opposite to ``e``, or ``Invalid``
     *
     * This accessor loads ``E2E[e]``.
     */
    MI_INLINE UInt32 opposite(UInt32 e, Mask active = true) const {
        return dr::gather<UInt32>(m_E2E, e, active);
    }

    /**
     * Return the canonical half-edge starting at ``v``, or ``Invalid``
     *
     * This accessor loads ``V2E[v]``.
     *
     * This is the smallest half-edge at ``v`` whose predecessor is unpaired, or
     * the smallest one overall when no such half-edge exists. It is the entry
     * point of the vertex walk described in the class documentation.
     */
    MI_INLINE UInt32 vertex_edge(UInt32 v, Mask active = true) const {
        return dr::gather<UInt32>(m_V2E, v, active);
    }

    /**
     * Return the valence of ``v``
     *
     * This is the number of faces containing ``v``. It excludes degenerate
     * faces with a repeated vertex index.
     */
    MI_INLINE UInt32 vertex_valence(UInt32 v, Mask active = true) const {
        return dr::gather<UInt32>(m_valence, v, active);
    }

    /// Return the bitmask of `VertexFlags` associated with vertex ``v``
    MI_INLINE UInt32 vertex_flags(UInt32 v, Mask active = true) const {
        return dr::gather<UInt32>(m_flags, v, active);
    }

    /// Per-half-edge buffer of opposites or ``Invalid`` entries
    const IndexBuffer &E2E() const { return m_E2E; }

    /// Per-vertex buffer of canonical outgoing half-edges
    const IndexBuffer &V2E() const { return m_V2E; }

    /// Per-vertex buffer of valences, i.e. face counts
    const IndexBuffer &valence() const { return m_valence; }

    /// Per-vertex buffer of `VertexFlags` bitmasks
    const IndexBuffer &flags() const { return m_flags; }

    /**
     * Number of vertices carrying the given single-bit flag
     *
     * The first call synchronizes with the device unless the constructor
     * already counted the flags to report defects.
     */
    uint32_t flag_count(VertexFlags flag) const;

    std::string to_string() const override;

    MI_DECLARE_CLASS(DirectedEdge)

protected:
    /// Single-threaded builder used in scalar variants
    void build_host(const IndexBuffer &F);

    /// Data-parallel builder used in JIT variants
    void build_jit(const IndexBuffer &F);

    /// Populate ``m_flag_counts``, transferring them from the device
    void count_flags() const;

protected:
    uint32_t m_vertex_count = 0;
    uint32_t m_half_edge_count = 0;
    std::string m_name;

    IndexBuffer m_E2E, m_V2E, m_valence, m_flags;

    /// Number of vertices carrying each of the four `VertexFlags`
    mutable uint32_t m_flag_counts[4] { 0, 0, 0, 0 };
    mutable bool m_flag_counts_ready = false;

    MI_TRAVERSE_CB(Object, m_E2E, m_V2E, m_valence, m_flags)
};

MI_EXTERN_CLASS(DirectedEdge)
NAMESPACE_END(mitsuba)
