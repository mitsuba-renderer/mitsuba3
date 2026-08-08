#pragma once

#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/mesh_utils.h>
#include <mitsuba/render/dedge.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/struct.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/properties.h>
#include <drjit/dynamic.h>
#include <drjit/tensor.h>
#include <drjit/quaternion.h>
#include <drjit/array_traverse.h>
#include <map>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Triangle mesh
 *
 * This class represents indexed triangle meshes, in which each triangle
 * references three vertices that carry data like positions, shading normals,
 * texture coordinates, and tangents that the renderer interpolates across
 * faces. Tangents orient the shading frame for use with normal maps and
 * anisotropic appearance models (e.g., brushed metal) and are computed
 * following the MikkTSpace (https://github.com/mmikk/MikkTSpace) standard. The
 * class can also store and interpolate arbitrary user-provided data with 1 to
 * 4 dimensions.
 *
 * Constructing a mesh involves two steps: the \ref Mesh() constructor to create
 * an empty mesh followed by a call to one of \ref from_fields() (for indexed
 * triangle data), \ref from_corners() (for corner-indexed data from 3D
 * modeling tools like Blender or interchange formats like OBJ), or
 * \ref from_packed() (for packed data explained below). There is also a
 * ``Mesh(name, faces, positions, ...)`` convenience constructor that
 * directly calls \ref from_fields(). Subsequent changes to the mesh data go
 * through the parameter interface described below, or through one of the
 * dedicated methods \ref transform(), \ref add_attribute() and
 * \ref remove_attribute().
 *
 * Interpolation makes mesh attributes vary smoothly along the surface, but
 * many meshes in practice also need deliberate *discontinuities*:
 *
 * - Shading normals across sharp edges (e.g. of a cube) must be discontinuous,
 *   which is incompatible with smooth interpolation.
 *
 * - Closed surfaces cannot be flattened into a 2D texture domain without
 *   cutting them open. Texture coordinates must therefore be able to represent
 *   seams, where the two sides of a curve on the surface map to different
 *   parts of the texture.
 *
 * - Tangents follow the texture parameterization and inherit its seams.
 *
 * The \ref Mesh class uses an indexed representation that expresses such a jump
 * by splitting a vertex, so that the faces on either side can reference
 * separate copies with their own values. However, splitting alone is not
 * sufficient because it turns seams into geometric cuts that have undesirable
 * side effects like creases in recomputed normals, spurious silhouettes in a
 * differentiable renderer, and split vertices drifting apart when the mesh is
 * optimized.
 *
 * The class therefore records which copies belong together, by storing each
 * attribute at the coarsest level at which it is constant. A mesh with ``F``
 * faces and ``V`` vertices consists of ``P`` surface positions and ``N``
 * normal groups, where each level subdivides the previous one (``P <= N <=
 * V``). In particular, the \ref Mesh exposes the following set of arrays:
 *
 * .. code-block:: text
 *
 *    Name              Type          Shape     Range     Optional
 *    ------------------------------------------------------------
 *    faces             TensorXu32    (F, 3)    [0, V)
 *    position_index    UInt32 array  V         [0, P)       x
 *    normal_index      UInt32 array  V         [0, N)       x
 *    positions         TensorXf32    (P, 3)
 *    normals           TensorXf32    (N, 3)                 x
 *    texcoords         TensorXf32    (V, 2)                 x
 *    bsdf_index        UInt32 array  F         [0, B)       x
 *
 * The fields have the following roles:
 *
 * - ``faces``: stores the 3 vertex indices for each triangular face.
 *
 * - ``position_index`` (optional): maps from *vertex index* to *position
 *   index*. When not present, the map is implicitly the identity and vertex
 *   and position indices coincide.
 *
 * - ``normal_index`` (optional): maps from *vertex index* to *normal
 *   index*. When not present, the map is implicitly the identity and vertex
 *   and normal indices coincide.
 *
 * - ``positions``: surface positions.
 *
 * - ``normals`` (optional): surface normals.
 *
 * - ``bsdf_index`` (optional): per-face index into a set of ``B`` materials.
 *
 * ## Example usage
 *
 * Texture coordinates and tangents vary *per vertex*. For example,
 * reading the UV coordinate for corner ``c`` of face ``f`` involves
 *
 * .. code-block:: python
 *
 *    vertex_idx = faces[f][c]
 *    texcoord   = texcoords[vertex_idx]
 *
 * Positions and normals require one further indirection
 *
 * .. code-block:: python
 *
 *    vertex_idx   = faces[f][c]
 *    position_idx = position_index[vertex_idx] if len(position_index) > 0 else vertex_idx
 *    position     = positions[position_idx]
 *
 * and analogously for normals through ``normal_index``.
 *
 * Split vertices may reference different attributes while sharing a position
 * and/or normal. This permits splitting a mesh into different parameterization
 * charts or adding creases without the problems mentioned earlier.
 *
 * ## Parameter interface
 *
 * The mesh exposes the fields listed above via \ref traverse(), and all of them
 * can be written. Custom mesh attributes appear as further ``vertex_*`` and
 * ``face_*`` entries with 1 to 4 channels. The mesh size (i.e., the number of
 * faces or vertices) may be changed as well. Following a change to the
 * parameters, call \ref Object::parameters_changed(), which will validate the
 * size of all fields, and refresh dependent state (bounding box, sampling
 * tables, tangents, acceleration structures).
 *
 * ## Packed layout
 *
 * The representation explained above is expressive but not particularly
 * efficient in a ray tracer, since it involves several indirections and
 * data spread out over many different buffers.
 *
 * To address this, the \ref Mesh class internally encodes data into a *packed*
 * layout, which uses 16 bytes per face and 32 bytes per vertex (plus custom
 * attributes). This is an implementation detail that is only exposed through
 * \ref from_packed() and the low-level \ref packed_vertices(),
 * ``packed_face()`` and ``packed_vertex()`` accessors. Access to the per-field
 * representation explained earlier implicitly reconstructs it from the packed
 * state.
 *
 * Using the packed representation, a ray intersection can then fetch all
 * per-corner triangle data using 4 packet loads, which map to a single
 * hardware instruction each on recent GPUs with 256-bit loads (NVIDIA
 * Blackwell), as opposed to ~28 scalar loads spread across multiple buffers.
 *
 * ## Orientation
 *
 * The face winding order defines the orientation of the surface. In
 * particular, the geometric normal of a face follows from the right hand rule
 * applied to its positions. Shading normals are expected to lie in the same
 * hemisphere, and the \ref Mesh class preserves this invariant by potentially
 * changing the winding order depending on `Shape` parameters like ``flip_normals``,
 * ``to_world``, and subsequent transformations via \ref transform().
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Mesh : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(BSDF, DirectedEdge)
    MI_IMPORT_BASE(Shape, m_to_world, mark_dirty, m_emitter, m_sensor, m_bsdf,
                   m_interior_medium, m_exterior_medium, m_is_instance,
                   m_discontinuity_types, m_shape_type, m_initialized,
                   get_children_string)

    // Mesh is always stored in single precision
    using InputFloat    = float;
    using InputPoint3f  = Point<InputFloat, 3>;
    using InputVector2f = Vector<InputFloat, 2>;
    using InputVector3f = Vector<InputFloat, 3>;
    using InputNormal3f = Normal<InputFloat, 3>;

    using FloatBuffer  = DynamicBuffer<Float32>;
    using IndexBuffer  = DynamicBuffer<UInt32>;

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::Index;

    // =========================================================================
    // Constructors and destructor
    // =========================================================================

    /** \brief Create an empty mesh
     *
     * The resulting object is not ready for use. You must call
     * \ref from_fields(), \ref from_corners() or \ref from_packed() to
     * initialize its storage.
     */
    Mesh(const Properties &props = Properties());

    /** \brief Create an empty mesh
     *
     * Analogous to the previous constructor, except that attributes
     * are specified manually and not through a `Properties` object.
     *
     * The resulting object is not ready for use. You must call
     * \ref from_fields(), \ref from_corners() or \ref from_packed() to
     * initialize its storage.
     */
    Mesh(std::string_view name,
         bool face_normals = false,
         bool flip_normals = false);

    /** \brief Create a mesh from a per-field representation
     *
     * Convenience constructor that chains the previous constructor and
     * \ref from_fields(). See this function for details.
     */
    Mesh(std::string_view name,
         const TensorXu32 &faces,
         const TensorXf32 &positions,
         const TensorXf32 &normals = TensorXf32(),
         const TensorXf32 &texcoords = TensorXf32(),
         const IndexBuffer &position_index = IndexBuffer(),
         const IndexBuffer &normal_index = IndexBuffer(),
         const IndexBuffer &bsdf_index = IndexBuffer(),
         bool face_normals = false,
         bool flip_normals = false);

    // =========================================================================

    // =========================================================================
    // Mesh construction
    // =========================================================================

    /**
     * \brief Build the mesh from a per-field representation
     *
     * This function initializes the mesh using device-resident field
     * tensors as explained in the Mesh class documentation. A mesh can be
     * built only once: a second ``from_*`` call raises an exception, and
     * later mesh changes must go through the parameter interface instead.
     *
     * The function checks the tensor shapes for consistency but trusts that
     * any specified indices are in-bounds (see \ref validate()).
     *
     * In differentiable variants, derivatives will propagate between the
     * supplied tensors and the resulting mesh.
     *
     * On a mesh constructed with ``face_normals`` set, the ``normals`` and
     * ``normal_index`` parameters are silently ignored.
     *
     * \param faces
     *     ``(F, 3)`` tensor of vertex indices in ``[0, V)``
     *
     * \param positions
     *     ``(P, 3)`` tensor of surface positions
     *
     * \param normals
     *     Optional ``(N, 3)`` tensor of shading normals, which must be of
     *     unit length. When empty, smooth normals are derived from the
     *     positions and regenerate after later position edits (unless the
     *     mesh uses face normals).
     *
     * \param texcoords
     *     Optional ``(V, 2)`` tensor of texture coordinates
     *
     * \param position_index
     *     Optional map from vertex index to surface position, in which case
     *     ``positions`` has one entry per surface position rather than per
     *     vertex. An empty map encodes the identity.
     *
     * \param normal_index
     *     Optional map from vertex index to normal group. An empty map
     *     encodes the identity, or the position map when the normal count
     *     matches the surface position count.
     *
     * \param bsdf_index
     *     Optional per-face material index. An empty buffer stands for zeros.
     */
    void from_fields(const TensorXu32 &faces,
                     const TensorXf32 &positions,
                     const TensorXf32 &normals = TensorXf32(),
                     const TensorXf32 &texcoords = TensorXf32(),
                     const IndexBuffer &position_index = IndexBuffer(),
                     const IndexBuffer &normal_index = IndexBuffer(),
                     const IndexBuffer &bsdf_index = IndexBuffer());

    /**
     * \brief Build the mesh from a packed representation
     *
     * This function initializes the mesh using device-resident tensors
     * that are already in the packed format that is internally used by
     * the Mesh class. A mesh can be built only once: a second ``from_*``
     * call raises an exception; later mesh changes must go through
     * the parameter interface.
     *
     * The function checks the tensor shapes for consistency but trusts that
     * any specified indices are in-bounds and that the per-face UV
     * orientation bits of a tangent layout are consistent with the stored
     * texture coordinates.
     *
     * The operation is differentiable in the sense that derivatives propagate
     * between function parameters and the resulting mesh state.
     *
     * \param layout
     *     Describes the content of the vertex records.
     *
     * \param packed_faces
     *     ``(F, 4)`` tensor of packed face records
     *
     * \param packed_vertices
     *     ``(V, 8)`` tensor of packed vertex records
     *
     * \param position_index
     *     Optional map from vertex index to surface position
     *
     * \param normal_index
     *     Optional map from vertex index to normal group. It may simply
     *     alias ``position_index`` when the normals are stored at surface
     *     position granularity.
     *
     * \param position_count
     *     Number of surface positions. Only needed when ``position_index``
     *     is nonempty.
     *
     * \param normal_count
     *     Number of normal groups. Only needed when ``normal_index`` is
     *     nonempty.
     */
    void from_packed(Layout layout,
                     const TensorXu32 &packed_faces,
                     const TensorXf32 &packed_vertices,
                     const IndexBuffer &position_index = IndexBuffer(),
                     const IndexBuffer &normal_index = IndexBuffer(),
                     size_t position_count = 0,
                     size_t normal_count = 0,
                     const ScalarBoundingBox3f *bbox = nullptr);

    /**
     * \brief Build the mesh from host-side staging data
     *
     * The ``PackedMesh`` data structure can be used to incrementally build
     * a packed mesh in a device-shared staging buffer while validating
     * inputs. This method then consumes the resulting ``PackedMesh`` and
     * efficiently blits it to the target device. A mesh can be built only
     * once: a second ``from_*`` call raises an exception; later mesh
     * changes must go through the parameter interface.
     *
     * Any transformations (e.g. ``flip_normals``, ``to_world`` parameters)
     * should already have been baked into the packed mesh data by the caller.
     * (The ``PackedMesh`` class provides an API for this.)
     */
    void from_packed(PackedMesh &&data);

    /**
     * \brief Build the mesh from corner-indexed data
     *
     * A number of DCC applications and mesh formats store positions per
     * vertex but normals, UVs and colors per *face corner*, so that they
     * can be discontinuous across edges. This function constructs a Mesh
     * from such input by splitting vertices with incompatible state.
     *
     * A mesh can be built only once: a second ``from_*`` call raises an
     * exception; later mesh changes must go through the parameter
     * interface instead.
     */
    void from_corners(const CornerMesh &desc);

    /**
     * \brief Return the compatibility key of this mesh
     *
     * The key collects the state that \ref merge() copies from its first
     * input, which is the attachments (BSDF, emitter, sensor, media) and the
     * packed record layout. Meshes are mergeable when their keys agree and
     * none of them carries custom attributes.
     */
    MergeKey merge_key() const;

    /**
     * \brief Merge several meshes into one
     *
     * All meshes must share the same \ref merge_key() and none of them may
     * carry custom attributes.
     *
     * The method raises an exception when called with incompatible inputs.
     */
    static ref<Mesh> merge(const std::vector<Shape<Float, Spectrum> *> &shapes);

    // =========================================================================

    // =========================================================================
    // Element counts
    // =========================================================================

    /// Return the total number of faces
    ScalarSize face_count() const { return m_face_count; }
    /// Return the total number of vertices
    ScalarSize vertex_count() const { return m_vertex_count; }
    /// Return the number of surface positions
    ScalarSize position_count() const { return m_position_count; }
    /// Return the number of normal groups
    ScalarSize normal_count() const { return m_normal_count; }

    // =========================================================================

    // =========================================================================
    // Field state of the vertex and face data
    // =========================================================================

    /// Return the vertex index triplets as an ``(F, 3)`` tensor
    const TensorXu32 &faces() const {
        ensure_views();
        return m_faces;
    }

    /**
     * \brief Return an ``(F, 3)`` tensor encoding the geometric topology
     *
     * This function returns \ref faces() re-indexed into surface position space,
     * i.e., the geometric topology of the mesh without UV/normal-related
     * seams. It is used by features like \ref dedge() and the
     * mesh Laplacian in ``largesteps.py``.
     */
    TensorXu32 geometric_faces() const;

    /// Return the per-face BSDF index (size \ref face_count()). An empty
    /// buffer stands for zeros, see ``has_face_bsdfs()``.
    const IndexBuffer &bsdf_index() const {
        ensure_views();
        return m_bsdf_index;
    }

    /// Return the surface position positions as a ``(P, 3)`` tensor
    const TensorXf32 &positions() const {
        ensure_views();
        return m_positions;
    }

    /// Return the shading normal group values as an ``(N, 3)`` tensor
    const TensorXf32 &normals() const {
        ensure_views();
        return m_normals;
    }

    /// Return the texture coordinates as a ``(V, 2)`` tensor
    const TensorXf32 &texcoords() const {
        ensure_views();
        return m_texcoords;
    }

    /// Return the shading tangents as a ``(V, 3)`` tensor
    const TensorXf32 &tangents() const {
        ensure_views();
        if (m_tangents.array().empty() && has_tangents())
            const_cast<Mesh *>(this)->m_tangents = compute_tangents();
        return m_tangents;
    }

    /// Return the vertex index -> surface position index map. An empty
    /// map encodes the identity.
    const IndexBuffer &position_index() const { return m_position_index; }

    /// Return the vertex index -> normal index map. An empty map
    /// encodes the identity.
    const IndexBuffer &normal_index() const { return m_normal_index; }

    // =========================================================================

    // =========================================================================
    // Feature queries
    // =========================================================================

    /// Does the mesh provide interpolated normals?
    bool has_normals() const { return has_flag(m_layout, Layout::Normals); }

    /// Does the mesh provide interpolated texture coordinates?
    bool has_texcoords() const { return has_flag(m_layout, Layout::Texcoords); }

    /// Does the mesh provide interpolated tangents?
    bool has_tangents() const { return has_normals() && has_texcoords(); }

    /// Does this mesh have a per-face BSDF assignment?
    bool has_face_bsdfs() const { return has_flag(m_layout, Layout::FaceBSDFs); }

    /// Does the mesh store per-vertex tangents?
    bool packs_tangent() const { return has_flag(m_layout, Layout::Tangents); }

    /// Does this mesh have additional mesh attributes?
    bool has_mesh_attributes() const { return m_mesh_attributes.size() != 0; }

    /// Does this mesh use face normals?
    bool has_face_normals() const { return m_face_normals; }

    // =========================================================================

    // =========================================================================
    // Geometry queries
    // =========================================================================

    /** \brief Returns the world-space position of the vertex with index
     * \c index
     *
     * The index type is generic because the host-side kd-tree paths
     * (``bbox()`` and ``ray_intersect_triangle_scalar()``) read
     * vertices one at a time with a plain \c uint32_t, while the packet
     * variants of \ref ray_intersect_triangle() pass a \c dr::Packet.
     */
    template <typename Index>
    MI_INLINE auto vertex_position(Index index,
                                   dr::mask_t<Index> active = true) const {
        using Value = dr::replace_scalar_t<Index, InputFloat>;
        Index base = index * MeshVertexStride + PackedPositionOffset;
        return Point<Value, 3>(
            dr::gather<Value>(m_packed_vertices, base, active),
            dr::gather<Value>(m_packed_vertices, base + 1u, active),
            dr::gather<Value>(m_packed_vertices, base + 2u, active));
    }

    /// Returns the normal direction of the vertex with index \c index
    MI_INLINE Normal<Float32, 3> vertex_normal(UInt32 index,
                                               Mask active = true) const {
        UInt32 base = index * MeshVertexStride + PackedFrameOffset;
        Vector<Float32, 3> f(
            dr::gather<Float32>(m_packed_vertices, base, active),
            dr::gather<Float32>(m_packed_vertices, base + 1u, active),
            dr::gather<Float32>(m_packed_vertices, base + 2u, active));
        return packs_tangent() ? frame_decode(f).first : Normal<Float32, 3>(f);
    }

    /// Returns the UV texture coordinates of the vertex with index \c index
    MI_INLINE Point<Float32, 2> vertex_texcoord(UInt32 index,
                                                Mask active = true) const {
        UInt32 base = index * MeshVertexStride + PackedTexcoordOffset;
        return Point<Float32, 2>(
            dr::gather<Float32>(m_packed_vertices, base, active),
            dr::gather<Float32>(m_packed_vertices, base + 1u, active));
    }

    /// Returns the vertex indices associated with triangle \c index
    template <typename Index>
    MI_INLINE auto face_indices(Index index,
                                dr::mask_t<Index> active = true) const {
        using UInt32 = dr::uint32_array_t<Index>;
        auto rec = packed_face(index, active);
        return Vector<UInt32, 3>(rec[0], rec[1], rec[2]);
    }

    /**
     * \brief Returns the vertex indices of the directed edge \c index
     *
     * The three components are the source vertex, the target vertex, and the
     * vertex opposing the edge within its face. They correspond to ``F[e]``,
     * ``F[next(e)]``, and ``F[prev(e)]`` in the notation of \ref DirectedEdge.
     */
    MI_INLINE Vector3u dedge_indices(UInt32 index, Mask active = true) const {
        UInt32 base     = DirectedEdge::face(index) * MeshFaceStride,
               source   = DirectedEdge::corner(index),
               target   = dr::select(source == 2u, 0u, source + 1u),
               opposing = dr::select(source == 0u, 2u, source - 1u);

        return Vector3u(
            dr::gather<UInt32>(m_packed_faces, base + source, active),
            dr::gather<UInt32>(m_packed_faces, base + target, active),
            dr::gather<UInt32>(m_packed_faces, base + opposing, active));
    }

    /// Returns the normal direction of a face with the given vertex positions
    template <typename Point3>
    static MI_INLINE auto face_normal(const Point3 &p0, const Point3 &p1,
                                      const Point3 &p2) {
        return dr::normalize(dr::cross(p1 - p0, p2 - p0));
    }

    /// Returns the normal direction of the face with index \c index
    MI_INLINE Vector3f face_normal(UInt32 index, Mask active = true) const {
        Vector3u vi = face_indices(index, active);
        Vector3f p0 = vertex_position(vi[0], active),
                 p1 = vertex_position(vi[1], active),
                 p2 = vertex_position(vi[2], active);

        return face_normal(p0, p1, p2);
    }

    /**
     * Returns the opposite edge index associated with directed edge \c index
     *
     * This is one of four accessors forwarding to the \ref DirectedEdge
     * structure, which \ref dedge() builds on first use. A returned \c
     * DirectedEdge::Invalid therefore always describes the topology, here the
     * absence of a neighboring face.
     */
    MI_INLINE UInt32 dedge_opposite(UInt32 index, Mask active = true) const {
        return dedge()->opposite(index, active);
    }

    /// Returns the canonical half-edge starting at vertex \c index
    MI_INLINE UInt32 dedge_vertex_edge(UInt32 index, Mask active = true) const {
        return dedge()->vertex_edge(index, active);
    }

    /// Returns the number of faces containing vertex \c index
    MI_INLINE UInt32 dedge_vertex_valence(UInt32 index,
                                          Mask active = true) const {
        return dedge()->vertex_valence(index, active);
    }

    /// Returns the \ref VertexFlags bitmask of vertex \c index
    MI_INLINE UInt32 dedge_vertex_flags(UInt32 index,
                                        Mask active = true) const {
        return dedge()->vertex_flags(index, active);
    }

    Point3f barycentric_coordinates(const SurfaceInteraction3f &si,
                                    Mask active = true) const;

    // =========================================================================

    // =========================================================================
    // Custom mesh attributes
    // =========================================================================

    /// Return the mesh attribute \c name as a ``(rows, dim)`` tensor
    const TensorXf32 &attribute(std::string_view name) const;

    /**
     * \brief Add the mesh attribute \c name
     *
     * The name must start with ``vertex_`` or ``face_``, which selects
     * the domain and hence the expected row count of the ``(rows, dim)``
     * tensor \c values. Attributes may carry 1 to 4 dimensions.
     */
    void add_attribute(std::string_view name, const TensorXf32 &values);

    /**
     * Remove an attribute with the given \c name.
     *
     * Affects both mesh and texture attributes.
     *
     * Throws an exception if the attribute was not previously registered.
     */
    void remove_attribute(std::string_view name) override;

    // =========================================================================

    // =========================================================================
    // Packed record layout
    // =========================================================================

    /// One packed vertex record, as returned by ``packed_vertex()``
    using PackedVertex = dr::Array<Float32, MeshVertexStride>;

    /// One packed face record, as returned by ``packed_face()``
    template <typename Index = UInt32>
    using PackedFace = Vector<dr::uint32_array_t<Index>, MeshFaceStride>;

    // =========================================================================

    // =========================================================================
    // Functions to access the packed vertex state
    // =========================================================================

    /// Return the packed per-vertex buffer
    FloatBuffer& packed_vertices() { return m_packed_vertices; }

    /// Const variant of \ref packed_vertices().
    const FloatBuffer& packed_vertices() const { return m_packed_vertices; }

    /// Returns the packed face record of triangle \c index
    template <typename Index>
    MI_INLINE PackedFace<Index>
    packed_face(Index index, dr::mask_t<Index> active = true) const {
        return dr::gather<PackedFace<Index>>(m_packed_faces, index, active);
    }

    /**
     * \brief Returns the packed vertex data for vertex index \c index
     *
     * When \c detach is \c true, the read is detached from the AD graph.
     */
    MI_INLINE PackedVertex packed_vertex(UInt32 index, Mask active = true,
                                         bool detach = false) const {
        DRJIT_MARK_USED(detach);
        if constexpr (dr::is_diff_v<Float>)
            return dr::gather<PackedVertex>(
                detach ? dr::detach(m_packed_vertices) : m_packed_vertices,
                index, active);
        else
            return dr::gather<PackedVertex>(m_packed_vertices, index, active);
    }

    // =========================================================================

    // =========================================================================
    // Recomputation of derived data
    // =========================================================================

    /// (Re-) compute smooth interpolated normals from the positions
    void recompute_normals();

    /**
     * \brief Transform the mesh geometry in place
     *
     * Maps positions and tangents through \c t and shading normals through its
     * inverse transpose. A mirroring \c t additionally reverses the face
     * winding, so that the geometric normals remain consistent with respect to
     * the shading normals. The method also refreshes dependent state (bounding
     * box, sampling tables, field views). In differentiable variants,
     * derivatives propagate from \c t to the resulting mesh state.
     */
    void transform(const AffineTransform4f &t);

    /** \brief Return a data structure describing the half-edge adjacency
     *
     * This function returns a `DirectedEdge` data structure that enables
     * geometric queries such as finding the triangle across an edge or
     * traversing faces surrounding a vertex.
     *
     * The data structure is built on demand and uses the geometric
     * connectivity specified by \ref geometric_faces() so that attribute
     * discontinuities do not introduce artificial geometric boundaries.
     * The result is immutable and invariant to changes in mesh positions,
     * normals and materials. The `Mesh` class automatically deletes the
     * cached instance when the index buffer, the position index map, or the
     * position count changes.
     *
     * The on-demand construction does not lock a mutex to protect from
     * concurrent calls to this function. This matches the expected usage (JIT
     * variants), where a single thread orchestrates the parallel computation.
     */
    const DirectedEdge *dedge() const;

    /**
     * \brief Check the field views for consistency
     *
     * By default, this function cheaply checks tensor ranks and shapes of the
     * mesh state for consistency. When \c check_bounds is set, the function
     * also verifies that every index is in range. This is relatively expensive
     * because it requires several device reductions.
     */
    void validate(bool check_bounds = false) const;

    // =========================================================================

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * Write the mesh to a binary PLY file
     *
     * \param filename
     *    Target file path on disk
     */
    void write_ply(const fs::path &filename) const;

    /**
     * Write the mesh encoded in binary PLY format to a stream
     *
     * \param stream
     *    Target stream that will receive the encoded output
     */
    void write_ply(Stream *stream) const;

    /**
     * \brief Write the mesh to a ``.serialized`` file
     *
     * This function writes the packed mesh state to an efficient
     * compressed file representation.
     *
     * \param filename
     *    Target file path on disk
     */
    void write_serialized(const fs::path &filename) const;

    /**
     * \brief Write the mesh in ``.serialized`` encoding to a stream
     *
     * Appends a single self-contained mesh segment at the current stream
     * position, without the trailing dictionary that indexes multiple
     * meshes within one file; \ref write_serialized()
     * adds it. Segments of several meshes may be concatenated by calling
     * this method repeatedly, recording the byte offsets, and appending
     * one ``uint64`` offset per mesh followed by a ``uint32`` mesh count.
     *
     * \param stream
     *    Target stream that will receive the encoded output
     */
    void write_serialized(Stream *stream) const;

    // =========================================================================

    // =========================================================================
    // Ray-triangle intersection
    // =========================================================================

    /** \brief Ray-triangle intersection test
     *
     * Uses the algorithm by Moeller and Trumbore discussed at
     * <tt>http://www.acm.org/jgt/papers/MollerTrumbore97/code.html</tt>.
     *
     * \param index
     *    Index of the triangle to be intersected.
     * \param ray
     *    The ray segment to be used for the intersection query.
     * \return
     *    A tuple <tt>(valid, t, uv)</tt> where
     *
     *    - \c valid indicates whether an intersection was found.
     *
     *    - \c t contains the distance from the ray origin to the
     *      intersection point.
     *
     *    - \c uv contains the first two barycentric coordinates.
     */
    template <typename T, typename Ray3>
    std::tuple<dr::mask_t<T>, T, Point<T, 2>>
    ray_intersect_triangle_impl(const dr::uint32_array_t<T> &index,
                                const Ray3 &ray,
                                dr::mask_t<T> active = true) const {
        using Point3T = Point<T, 3>;
        using Faces = dr::Array<dr::uint32_array_t<T>, 3>;

        Faces fi;
        Point3T p0, p1, p2;
#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
        // Ensure we don't rely on drjit-core when called from an LLVM kernel
        if constexpr (!dr::is_array_v<T> && dr::is_llvm_v<Float>) {
            auto rec = dr::gather<dr::Array<dr::uint32_array_t<T>, 4>>(
                m_packed_faces_ptr, index, active);
            fi = Faces(rec[0], rec[1], rec[2]);
            auto packed_position = [&](uint32_t v) {
                uint32_t base = v * MeshVertexStride;
                return InputPoint3f(
                    dr::gather<InputFloat>(m_packed_vertices_ptr, base, active),
                    dr::gather<InputFloat>(m_packed_vertices_ptr, base + 1, active),
                    dr::gather<InputFloat>(m_packed_vertices_ptr, base + 2, active));
            };
            p0 = packed_position(fi[0]);
            p1 = packed_position(fi[1]);
            p2 = packed_position(fi[2]);
        } else
#endif
        {
            fi = face_indices(index, active);
            p0 = vertex_position(fi[0], active),
            p1 = vertex_position(fi[1], active),
            p2 = vertex_position(fi[2], active);
        }

        auto [t, uv, hit] = moeller_trumbore(ray, p0, p1, p2, active);
        return { hit, dr::select(hit, t, dr::Infinity<T>), uv };
    }

    MI_INLINE PreliminaryIntersection3f
    ray_intersect_triangle(const UInt32 &index, const Ray3f &ray,
                           Mask active = true) const {
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();
        std::tie(pi.valid, pi.t, pi.prim_uv) =
            ray_intersect_triangle_impl<Float>(index, ray, active);
        pi.prim_index = index;
        pi.shape = this;
        return pi;
    }

    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;
    MI_INLINE std::tuple<bool, ScalarFloat, ScalarPoint2f>
    ray_intersect_triangle_scalar(const ScalarUInt32 &index, const ScalarRay3f &ray) const {
        return ray_intersect_triangle_impl<ScalarFloat>(index, ray, true);
    }

#define MI_DECLARE_RAY_INTERSECT_TRI_PACKET(N)                             \
    using FloatP##N   = dr::Packet<dr::scalar_t<Float>, N>;                \
    using MaskP##N    = dr::mask_t<FloatP##N>;                             \
    using UInt32P##N  = dr::uint32_array_t<FloatP##N>;                     \
    using Point2fP##N = Point<FloatP##N, 2>;                               \
    using Point3fP##N = Point<FloatP##N, 3>;                               \
    using Ray3fP##N   = Ray<Point3fP##N, Spectrum>;                        \
    virtual std::tuple<MaskP##N, FloatP##N, Point2fP##N>                   \
    ray_intersect_triangle_packet(const UInt32P##N &index,                 \
                                  const Ray3fP##N &ray,                    \
                                  MaskP##N active = true) const {          \
        return ray_intersect_triangle_impl<FloatP##N>(index, ray, active); \
    }

    MI_DECLARE_RAY_INTERSECT_TRI_PACKET(4)
    MI_DECLARE_RAY_INTERSECT_TRI_PACKET(8)
    MI_DECLARE_RAY_INTERSECT_TRI_PACKET(16)

    // =========================================================================

    // =========================================================================
    // Shape interface implementation
    // =========================================================================

    /// Set the shape's `BSDF`
    void set_bsdf(BSDF *bsdf) override;

    ScalarBoundingBox3f bbox() const override;

    ScalarBoundingBox3f bbox(ScalarIndex index) const override;

    ScalarBoundingBox3f bbox(ScalarIndex index,
                             const ScalarBoundingBox3f &clip) const override;

    ScalarSize primitive_count() const override;

    Float surface_area() const override;

    PositionSample3f sample_position(Float time,
                                     const Point2f &sample,
                                     Mask active = true) const override;

    Float pdf_position(const PositionSample3f &ps, Mask active = true) const override;

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth = 0,
                                                     Mask active = true) const override;

    Mask has_attribute(std::string_view name, Mask active = true) const override;

    UnpolarizedSpectrum eval_attribute(std::string_view name,
                                       const SurfaceInteraction3f &si,
                                       Mask active = true) const override;

    Float eval_attribute_1(std::string_view name,
                           const SurfaceInteraction3f &si,
                           Mask active = true) const override;

    Color3f eval_attribute_3(std::string_view name,
                             const SurfaceInteraction3f &si,
                             Mask active = true) const override;

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags = +RayFlags::Default,
                                               Mask active = true) const override;

    SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                         uint32_t flags,
                                         Mask active) const override;

    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active) const override;

    Point3f differential_motion(const SurfaceInteraction3f &si,
                                Mask active = true) const override;

    SilhouetteSample3f primitive_silhouette_projection(const Point3f &viewpoint,
                                                       const SurfaceInteraction3f &si,
                                                       uint32_t flags,
                                                       Float sample,
                                                       Mask active = true) const override;

    std::tuple<IndexBuffer, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f &viewpoint) const override;

    SilhouetteSample3f sample_precomputed_silhouette(const Point3f &viewpoint,
                                                     Index sample1,
                                                     Float sample2,
                                                     Mask active = true) const override;

    void describe(ShapeIR &g) const override;

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;
    bool parameters_grad_enabled() const override;

    /// Return a human-readable string representation of the shape contents.
    std::string to_string() const override;

    // =========================================================================

    // =========================================================================
    // Miscellaneous
    // =========================================================================

    void set_scene(Scene<Float, Spectrum> *scene) { m_scene = scene; }

    size_t vertex_data_bytes() const;
    size_t face_data_bytes() const;

    // =========================================================================

protected:
    /**
     * \brief Build the table for sampling the surface uniformly wrt. area
     *
     * Computes the surface area and sets up \c m_area_pmf.
     *
     * Emitter and sensor meshes sample positions during rendering, so
     * ``refresh()`` builds their table eagerly.
     */
    void build_pmf();

    /**
     * \brief Return the sampling density over edges that could contribute to
     * the indirect discontinuous integral
     *
     * The distribution excludes concave edges and flat surfaces. It depends on
     * the vertex positions, so \ref refresh() discards it and this accessor
     * rebuilds it on the next use.
     */
    const DiscreteDistribution<Float> &sil_dedge_pmf() const;

    /**
     * \brief Initialize the \c m_parameterization field for mapping UV
     * coordinates to positions
     *
     * Internally, the function creates a nested scene to leverage optimized
     * ray tracing functionality in \ref eval_parameterization()
     */
    void build_parameterization();

    /// Does a spatially varying emitter require \c m_parameterization?
    bool needs_parameterization() const;

    // Ensures that the sampling table are ready.
    DRJIT_INLINE void ensure_pmf_built() const {
        if (unlikely(m_area_pmf.empty()))
            const_cast<Mesh *>(this)->build_pmf();
    }

    /**
     * \brief Does this mesh need tangents?
     *
     * True when the mesh can supply tangents (\ref has_tangents()) and the
     * attached material declares \ref BSDFFlags::NeedsTangents. This is
     * the layout that the next repack will write, see \ref packs_tangent().
     */
    bool needs_tangents() const;

    /**
     * \brief Reverse the corner order of every face, which flips the
     * geometric normals
     *
     * The orientation of each face's UV triangle reverses along with it, so
     * the packed tangent frames are updated to match. The caller is
     * responsible for the subsequent ``refresh()``.
     */
    void flip_winding();

    /**
     * \brief Compile the field views into the packed records
     *
     * This is the one mutation path of a built mesh. It validates the
     * views, derives the element counts and the vertex layout, generates
     * shading normals (when \c regenerate_normals is set, discarding the
     * current values while preserving their grouping) and MikkTSpace
     * tangents (when the attached material consumes them), writes both
     * packed buffers in a single pass, and ends in ``refresh()``.
     *
     * The \c flip_normals flag turns the surface inside out as the records
     * are written, which \ref from_fields() uses to bake the property of
     * the same name. See ``validate_impl()`` for \c updating.
     */
    void pack(bool regenerate_normals, bool flip_normals = false,
              bool updating = false);

    /**
     * \brief Implementation of \ref validate()
     *
     * When \c updating is set, the mesh is checking a
     * \ref Object::parameters_changed() batch, and shape mismatches also report how to
     * rewrite the offending field.
     */
    void validate_impl(bool check_bounds, bool updating) const;

    /**
     * \brief Regenerate everything downstream of the packed state
     *
     * Every mutation ends with a call to this method. It rebuilds the
     * bounding box (adopting \c bbox when given), the area sampling table
     * of emitter/sensor meshes, the UV parameterization of spatially
     * varying emitters, and the silhouette structures of gradient-enabled
     * meshes, refreshes the raw data pointers, marks the scene
     * acceleration structure dirty, and rebinds the field views unless
     * they are dormant. The directed edge structure is not touched here:
     * it is expensive and purely topological, so \ref Object::parameters_changed()
     * clears it only when a topology write occurs.
     */
    void refresh(const ScalarBoundingBox3f *bbox = nullptr);

    /// (Re-)compute the bounding box from the packed positions
    void recompute_bbox();

    /// Release the field views and make the packed state authoritative
    void drop_views();

    /// Derive the field views from the packed state, preserving AD identity
    void build_views();

    /// Materialize the views if they are dormant
    void ensure_views() const;

    /// Compute smooth shading normals from the field views, accumulated
    /// over the ``m_normal_index`` grouping, as an ``(N, 3)`` tensor
    TensorXf32 compute_normals() const;

    /// Compute MikkTSpace shading tangents from the field views as a
    /// ``(V, 3)`` tensor
    TensorXf32 compute_tangents() const;

    /** \brief Moeller and Trumbore algorithm for computing ray-triangle
     * intersection
     *
     * Discussed at
     * <tt>http://www.acm.org/jgt/papers/MollerTrumbore97/code.html</tt>.
     *
     * \param ray
     *    The ray segment to be used for the intersection query.
     * \param p0
     *    First vertex position of the triangle
     * \param p1
     *    Second vertex position of the triangle
     * \param p2
     *    Third vertex position of the triangle
     * \return
     *    A tuple <tt>(t, uv, mask)</tt> where
     *
     *    - \c t contains the distance from the ray origin to the
     *      intersection point.
     *
     *    - \c uv contains the first two barycentric coordinates.
     *
     *    - \c mask indicates whether an intersection was found.
     */
    template <typename T, typename Ray3>
    std::tuple<T, Point<T, 2>, dr::mask_t<T>>
    moeller_trumbore(const Ray3 &ray, const Point<T, 3> &p0,
                     const Point<T, 3> &p1, const Point<T, 3> &p2,
                     dr::mask_t<T> active = true) const {
        using Vector3T = Vector<T, 3>;
        Vector3T e1 = p1 - p0, e2 = p2 - p0;

        Vector3T pvec = dr::cross(ray.d, e2);
        T inv_det = dr::rcp(dr::dot(e1, pvec));

        Vector3T tvec = ray.o - p0;
        T u = dr::dot(tvec, pvec) * inv_det;
        active &= u >= 0.f && u <= 1.f;

        Vector3T qvec = dr::cross(tvec, e1);
        T v = dr::dot(ray.d, qvec) * inv_det;
        active &= v >= 0.f && u + v <= 1.f;

        T t = dr::dot(e2, qvec) * inv_det;
        active &= t >= 0.f && t <= ray.maxt;

        return { t, { u, v }, active };
    }

    MI_DECLARE_CLASS(Mesh)

protected:
    struct MeshAttribute {
        uint32_t dim;

        /// Interleaved ``(rows, dim)`` attribute records
        TensorXf32 data;

        MeshAttribute migrate(JitBackend backend) const {
            return MeshAttribute {
                dim,
                TensorXf32(dr::migrate(data.array(), backend), 2,
                           data.shape().data())
            };
        }

        DRJIT_ARRAY_DEFAULTS(MeshAttribute)
        DRJIT_TRAVERSE(MeshAttribute, data);
    };

    /// Does the attribute \c name live on the vertices rather than the faces?
    static bool is_vertex_attribute(std::string_view name) {
        return string::starts_with(name, "vertex_");
    }

    /// Do the records of the attribute \c name hold RGB2Spec upsampling coefficients?
    static bool holds_rgb2spec_coeffs(std::string_view name, size_t dim);

    /// Convert \c rows RGB triplets in place into RGB2Spec upsampling coefficients.
    static void to_rgb2spec_coeffs(InputFloat *data, size_t rows);

    /**
     * \brief Read the attribute \c attr at the interaction \c si
     *
     * With ``Raw``, the result holds the stored values (\c Float or \c
     * Color3f). Otherwise, the read may perform variant-specific
     * conversions (e.g., spectral upsampling)
     */
    template <uint32_t Size, bool Raw>
    auto interpolate_attribute(const MeshAttribute &attr,
                               std::string_view name,
                               const SurfaceInteraction3f &si,
                               Mask active) const;

    /// Return the mesh attribute \c name or NULL
    const MeshAttribute *find_attribute(std::string_view name) const;

    /// Shared body of \ref eval_attribute_1() and \ref eval_attribute_3()
    template <uint32_t Size>
    auto eval_attribute_n(std::string_view name,
                          const SurfaceInteraction3f &si, Mask active) const;

protected:
    /// Resolved path of the source file when the mesh was constructed
    fs::path m_source_path;

    /// Short human-readable label used in log and error messages.
    std::string m_filename;

    /// Bounding box of the mesh positions
    ScalarBoundingBox3f m_bbox;

    ScalarSize m_vertex_count = 0;
    ScalarSize m_face_count = 0;
    ScalarSize m_position_count = 0;
    ScalarSize m_normal_count = 0;

    /// Potentially flip the normals once at construction time
    bool m_flip_normals = false;

    /// Is the mesh flat shaded, without stored shading normals?
    bool m_face_normals = false;

    /// Set by the first successful build; construction is one-shot
    bool m_built = false;

    /// Packed faces, material IDs and UV orientation bits (4 x UInt32 per face)
    IndexBuffer m_packed_faces;

    /// Packed per-vertex state (8 x Float32 per vertex)
    FloatBuffer m_packed_vertices;

    /// Content of the packed vertex records
    Layout m_layout = Layout::Positions;

    /// Vertex index to position index map. Optional.
    IndexBuffer m_position_index;

    /// Vertex index to normal index map. Optional.
    IndexBuffer m_normal_index;

    /// Inverses of the two index maps above, mapping each group to a
    /// representative vertex
    mutable IndexBuffer m_position_rep, m_normal_rep;

    // Views of the packed state (see \ref build_views())
    TensorXf32 m_positions;
    TensorXf32 m_normals;
    TensorXf32 m_texcoords;
    TensorXu32 m_faces;
    IndexBuffer m_bsdf_index;
    TensorXf32 m_tangents;

    /// Half-edge adjacency, null until \ref dedge() builds it
    mutable ref<DirectedEdge> m_dedge;

    /// Sampling density of silhouette edges, null until \ref sil_dedge_pmf()
    mutable DiscreteDistribution<Float> m_sil_dedge_pmf;

#if defined(MI_ENABLE_LLVM) && !defined(MI_ENABLE_EMBREE)
    // Fast explicit access to data pointers for use with LLVM and Embree
    float* m_packed_vertices_ptr;
    uint32_t* m_packed_faces_ptr;
#endif

    /// Custom mesh attributes. The use of a node-based map is intentional
    /// as this provides stable references.
    std::map<std::string, MeshAttribute, std::less<>> m_mesh_attributes;

    // Surface area distribution -- generated on demand when
    // prepare_area_pmf() is first called.
    DiscreteDistribution<Float> m_area_pmf;
    std::mutex m_mutex;

    /// Optional: used in \ref eval_parameterization()
    ref<Scene<Float, Spectrum>> m_parameterization;

    /// Pointer to the scene that owns this mesh
    Scene<Float, Spectrum>* m_scene = nullptr;

    MI_DECLARE_TRAVERSE_CB(m_packed_vertices, m_packed_faces,
                           m_positions, m_normals, m_texcoords,
                           m_position_index, m_normal_index,
                           m_dedge, m_sil_dedge_pmf, m_mesh_attributes,
                           m_area_pmf, m_parameterization)
};

MI_EXTERN_CLASS(Mesh)
NAMESPACE_END(mitsuba)


// -----------------------------------------------------------------------
// Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_INHERITED_BEGIN(mitsuba::Mesh, mitsuba::Shape)
    DRJIT_CALL_METHOD(face_indices)
    DRJIT_CALL_METHOD(dedge_indices)
    DRJIT_CALL_METHOD(vertex_position)
    DRJIT_CALL_METHOD(vertex_normal)
    DRJIT_CALL_METHOD(vertex_texcoord)
    DRJIT_CALL_METHOD(face_normal)
    DRJIT_CALL_METHOD(dedge_opposite)
    DRJIT_CALL_METHOD(dedge_vertex_edge)
    DRJIT_CALL_METHOD(dedge_vertex_valence)
    DRJIT_CALL_METHOD(dedge_vertex_flags)
    DRJIT_CALL_METHOD(ray_intersect_triangle)

    DRJIT_CALL_GETTER(vertex_count)
    DRJIT_CALL_GETTER(face_count)
    DRJIT_CALL_GETTER(has_normals)
    DRJIT_CALL_GETTER(has_texcoords)
    DRJIT_CALL_GETTER(has_mesh_attributes)
    DRJIT_CALL_GETTER(has_face_normals)
DRJIT_CALL_END()

// -----------------------------------------------------------------------
