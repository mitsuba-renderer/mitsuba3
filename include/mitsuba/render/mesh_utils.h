#pragma once

#include <mitsuba/core/platform.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/transform.h>
#include <drjit/quaternion.h>
#include <drjit/unique_buffer.h>
#include <drjit-core/hash.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// Number of 32-bit words per packed face record
constexpr uint32_t MeshFaceStride = 4;

/// Number of 32-bit words per packed vertex record
constexpr uint32_t MeshVertexStride = 8;

/// Offset of the position (3 floats) in a packed vertex record
constexpr uint32_t PackedPositionOffset = 0;

/// Offset of the normal or shading frame in a packed vertex record
constexpr uint32_t PackedFrameOffset = 3;

/// Offset of the texture coordinates (2 floats)
constexpr uint32_t PackedTexcoordOffset = 6;

/// Bit flag which indicates a face with flipped UVs
constexpr uint32_t FaceUVFlipped = 0x80000000u;

/// Bit mask used to encode the BSDF index
constexpr uint32_t FaceBSDFIndexMask = 0x7fffffffu;

/// Content of the packed records of a `Mesh`
enum class Layout : uint32_t {
    Positions = 0x0,  ///< Every vertex record carries positions
    Normals   = 0x1,  ///< Shading normals
    Tangents  = 0x2,  ///< Shading tangents
    Texcoords = 0x4,  ///< Texture coordinates
    FaceBSDFs = 0x8,  ///< The face records carry per-face BSDF indices
};

MI_DECLARE_ENUM_OPERATORS(Layout)

/// Assemble the `Layout` flags of the packed records
constexpr Layout make_layout(bool normals, bool texcoords,
                             bool tangents = false, bool face_bsdfs = false) {
    return (Layout) ((normals    ? (uint32_t) Layout::Normals   : 0u) |
                     (texcoords  ? (uint32_t) Layout::Texcoords : 0u) |
                     (tangents   ? (uint32_t) Layout::Tangents  : 0u) |
                     (face_bsdfs ? (uint32_t) Layout::FaceBSDFs : 0u));
}

/**
 * Identifies a set of mutually mergeable meshes
 *
 * The fields are the state that `Mesh.merge()` inherits from its first
 * input, so a difference in any of them would misrepresent the remaining
 * inputs. Obtain the key of a mesh through `Mesh.merge_key()`.
 */
struct MergeKey {
    const Object *bsdf, *emitter, *sensor, *interior_medium, *exterior_medium;

    /**
     * Packed record layout, without the ``FaceBSDFs`` bit that a merge
     * unions
     *
     * The face-normal setting needs no separate field, since a built mesh
     * carries the ``Normals`` bit exactly when it shades with vertex normals.
     */
    Layout layout;

    bool operator==(const MergeKey &k) const {
        return bsdf == k.bsdf && emitter == k.emitter && sensor == k.sensor &&
               interior_medium == k.interior_medium &&
               exterior_medium == k.exterior_medium && layout == k.layout;
    }

    bool operator!=(const MergeKey &k) const { return !operator==(k); }
};

/// Hash function of a ``MergeKey``, for use with ``tsl::robin_map``
struct MergeKeyHasher {
    size_t operator()(const MergeKey &k) const {
        uint64_t h = (uint64_t) k.layout;
        for (const Object *p : { k.bsdf, k.emitter, k.sensor,
                                 k.interior_medium, k.exterior_medium })
            h = fmix64(h ^ (uintptr_t) p);
        return (size_t) h;
    }
};

/**
 * Encode a unit normal ``n`` and a tangent ``s`` into three floats
 *
 * Together with the (implied) bitangent, this is an element of ``SO(3)``,
 * which this function parameterizes using 3 parameters by stereographically
 * projecting the frame's unit quaternion (the *modified Rodrigues
 * parameters* of Terzakis et al. :cite:`Terzakis2018Rodrigues`).
 *
 * After flipping the quaternion to a non-negative real part ``w``, the result
 * is ``imag(q) / (1 + w)``, of length ``tan(angle/4) <= 1``.
 *
 * The parameterization has a (benign) seam at ``angle=pi``. Antipodal points
 * of the unit sphere decode to the same frame.
 */
template <typename Value>
Vector<Value, 3> frame_encode(const Normal<Value, 3> &n,
                              const Vector<Value, 3> &s) {
    Vector<Value, 3> t = dr::cross(n, s);

    dr::Matrix<Value, 3> R(s.x(), t.x(), n.x(),
                           s.y(), t.y(), n.y(),
                           s.z(), t.z(), n.z());

    dr::Quaternion<Value> q = dr::matrix_to_quat(R);
    Value w = dr::real(q);

    return Vector<Value, 3>(dr::imag(q)) *
           dr::mulsign(dr::rcp(1.f + dr::abs(w)), w);
}

/// Decode a frame produced by `frame_encode()` into normal and tangent
template <typename Value>
std::pair<Normal<Value, 3>, Vector<Value, 3>>
frame_decode(const Vector<Value, 3> &p) {
    // Inverting the stereographic projection with ``a = 1/(1 + |p|^2)``
    // recovers the quaternion ``(imag, real) = (2 a p, 2 a - 1)``. The
    // rotation matrix is assembled from the products ``2 imag_i imag_j``
    // and ``2 real imag_i``, which collapse to ``A p_i p_j`` and ``B p_i``.

    Value a  = dr::rcp(1.f + dr::squared_norm(p)),
          A  = 8.f * dr::square(a),
          B  = dr::fnmadd(4.f, a, A),
          X  = A * p.x(), Y = A * p.y(), Z = A * p.z(),
          yy = p.y() * Y, xy = p.x() * Y,
          xz = p.x() * Z, yz = p.y() * Z,
          u  = 1.f - yy;

    // The third and first column of the rotation matrix
    return { Normal<Value, 3>(dr::fmadd (B, p.y(), xz),
                              dr::fnmadd(B, p.x(), yz),
                              dr::fnmadd(p.x(), X, u)),
             Vector<Value, 3>(dr::fnmadd(p.z(), Z, u),
                              dr::fmadd (B, p.z(), xy),
                              dr::fnmadd(B, p.y(), xz)) };
}

/**
 * Helper data structure to efficiently construct and upload
 * the internal `Mesh` data structure.
 *
 * Loaders fill this structure on the host and pass it to
 * `Mesh.from_packed`(PackedMesh &&), which uploads or adopts each
 * buffer exactly once.
 *
 * The constructor allocates staging memory of the flavor appropriate for
 * the target backend (host-pinned on CUDA, shared on Metal, plain host
 * memory otherwise), so the subsequent transfer is a single asynchronous
 * copy per buffer, or an in-place adoption on CPU backends.
 *
 * When ``position_count`` / ``normal_count`` are nonzero, they indicate the
 * size of the ``*_index`` maps (see `Mesh` for details).
 */
struct MI_EXPORT_LIB PackedMesh {
    using ScalarPoint3f  = Point<float, 3>;
    using ScalarNormal3f = Normal<float, 3>;
    using ScalarVector2f = Vector<float, 2>;
    using ScalarVector3f = Vector<float, 3>;
    using ScalarVector3u = Vector<uint32_t, 3>;
    using ScalarAffineTransform4f = AffineTransform<Point<float, 4>>;

    /// Custom mesh attribute (see `Mesh.add_attribute()`).
    struct Attribute {
        std::string name;
        size_t dim = 0;
        /// Upsample RGB colors to color spectra?
        bool upsample_srgb = true;
        drjit::unique_buffer<float> values;
    };

    PackedMesh() = default;
    PackedMesh(JitBackend backend, size_t vertex_count, size_t face_count,
               Layout layout, size_t position_count = 0,
               size_t normal_count = 0);

    PackedMesh(PackedMesh &&) = default;
    PackedMesh &operator=(PackedMesh &&) = default;
    PackedMesh(const PackedMesh &) = delete;
    PackedMesh &operator=(const PackedMesh &) = delete;

    /**
     * Bake a placement and orientation into the mesh
     *
     * Call this method to apply a to-world transformation to any mesh data
     * that is subsequently filled via ``set_face()`` and ``set_vertex()``.
     * Mirroring transformations and ``flip_normals`` may also reverse the
     * winding order.
     */
    void set_transform(const ScalarAffineTransform4f &to_world,
                       bool flip_normals = false);

    /**
     * Transform mesh data written so far
     *
     * Producers that fill ``PackedMesh`` directly without ``set_vertex()``
     * and ``set_face()`` should call this method at the end.
     */
    void transform_records();

    /// Write one vertex record and grow `bbox`
    void set_vertex(size_t i, const ScalarPoint3f &p,
                    const ScalarNormal3f &n = { 0.f, 0.f, 0.f },
                    const ScalarVector2f &uv = { 0.f, 0.f });

    /// Write one face record, checking that its indices are in bounds
    void set_face(size_t i,
                  const ScalarVector3u &indices,
                  uint32_t bsdf = 0);

    /**
     * Allocate a custom attribute and return a pointer to its buffer
     *
     * The name must be prefixed ``vertex_`` or ``face_``. Spectral variants
     * turn 3-channel ``*color*`` attributes into sRGB upsampling
     * coefficients while adopting the buffer. Clear ``upsample_srgb`` when
     * the producer already stores coefficients.
     */
    float *add_attribute(std::string_view name, size_t dim,
                         bool upsample_srgb = true);

    /**
     * Generate tangents for a mesh with normals and texture coordinates
     *
     * This commit populates the packed mesh data with vertex tangent frames,
     * matching the behavior of `Mesh::compute_tangents()`. In contrast to
     * this method, the computation is done on the host machine.
     * Call this after every face and vertex record has been written and
     * transformed.
     */
    void add_tangents();

    /// Dr.Jit backend of the allocated buffers
    JitBackend backend = JitBackend::None;

    /// Content of the vertex records
    Layout layout = Layout::Positions;

    /// Sizes of the vertex/face/position/normal arrays
    size_t vertex_count = 0, face_count = 0;
    size_t position_count = 0, normal_count = 0;

    /// Bounding box computed from the mesh positions
    BoundingBox<ScalarPoint3f> bbox;

    /// Reverse the corner order of the face records? (``set_transform()``)
    bool reverse_winding = false;

    drjit::unique_buffer<float>    vertices;
    drjit::unique_buffer<uint32_t> faces;
    drjit::unique_buffer<uint32_t> position_index, normal_index;
    std::vector<Attribute> attrs;

private:
    ScalarAffineTransform4f m_to_world;
    bool m_transform = false;
    bool m_negate_normals = false;

    bool m_written = false;
};

/**
 * Per-corner values of one attribute, see ``CornerMesh``
 *
 * ``data`` points to ``value_count x dim`` records. When ``indices`` is
 * null, the records align with the corner records of the ``CornerMesh``.
 * Otherwise, ``indices`` supplies one record index per corner record, and
 * ``UINT32_MAX`` marks a missing entry that resolves to zeros.
 */
struct CornerAttribute {
    /// Attribute name, which custom attributes must prefix with ``vertex_``
    std::string_view name;

    /// Number of channels (1 to 4 for custom attributes)
    size_t dim = 0;

    /// Attribute records (``value_count * dim`` floats)
    const float *data = nullptr;
    size_t value_count = 0;

    /// Optional indices into ``data`` (``CornerMesh.record_count`` entries)
    const uint32_t *indices = nullptr;
};

/**
 * Corner-indexed mesh description
 *
 * Many DCC applications and file formats store positions per vertex, while
 * normals, texture coordinates and colors live per *face corner* so that
 * they can be discontinuous across edges. This structure is a non-owning
 * view of such data: `positions` holds one entry per source vertex, the
 * faces are triangles or arbitrary polygons (``face_offsets``), and the
 * per-corner attributes are read either directly or through the record
 * indirection of ``corner_index``.
 *
 * ``corner_to_packed_mesh()`` turns this description into the split-vertex
 * representation used by `Mesh`.
 */
struct CornerMesh {
    /// Number of source vertices
    size_t vertex_count = 0;

    /// Vertex positions (``vertex_count * 3`` floats)
    const float *positions = nullptr;

    /// Number of face corners
    size_t corner_count = 0;

    /**
     * Optional map from face corner to corner record
     *
     * When given (``corner_count`` entries), ``corner_vertex`` and the
     * ``CornerAttribute`` records hold ``record_count`` entries that
     * corner ``c`` reads at ``corner_index[c]``, instead of one per corner.
     */
    const uint32_t *corner_index = nullptr;

    /// Number of corner records. Zero implies a value of ``corner_count``.
    size_t record_count = 0;

    /// Vertex referenced by each corner record (``record_count`` entries)
    const uint32_t *corner_vertex = nullptr;

    /// Number of faces (only used together with ``face_offsets``)
    size_t face_count = 0;

    /**
     * Optional polygonal face topology (``face_count + 1`` entries)
     *
     * Face ``i`` spans corners ``[face_offsets[i], face_offsets[i+1])``
     * and fans into triangles. The array must be non-decreasing, start at 0,
     * and end at ``corner_count``. Null input implies pure triangles.
     */
    const uint32_t *face_offsets = nullptr;

    /// Optional per-face BSDF indices (one entry per input face,
    /// replicated when a polygon fans into several triangles)
    const uint32_t *bsdf_index = nullptr;

    /// Shading normals (3 channels); optional
    CornerAttribute normals { "normals", 3 };

    /// Texture coordinates (2 channels); optional
    CornerAttribute texcoords { "texcoords", 2 };

    /// Custom ``vertex_*`` attributes (``attr_count`` entries)
    const CornerAttribute *attrs = nullptr;
    size_t attr_count = 0;
};

/**
 * Convert a corner-indexed mesh description into a packed mesh
 * compatible with the internal representation of `Mesh`
 *
 * This function triangulates polygonal faces and then welds the corners of
 * each source vertex: corners that agree on all per-corner data collapse
 * into a single vertex, while differing ones yield separate copies. The
 * generated ``position_index`` and ``normal_index`` maps record which of
 * these copies still share a position or a normal, so that seams do not
 * turn into geometric cuts (see the `Mesh` documentation). Corners also
 * split when their triangles disagree on the orientation of the texture
 * parameterization, since such triangles cannot share a tangent frame.
 *
 * Unreferenced source vertices are dropped, the remaining ones keep their
 * relative order, and split copies follow their original vertex.
 *
 * ``name`` identifies the mesh in error messages. ``face_normals``
 * announces that the mesh will use per-face normals, in which case any
 * supplied shading normals are ignored. The ``flip_normals`` and ``to_world``
 * arguments are baked into the result, see ``PackedMesh.set_transform()``.
 */
extern PackedMesh
corner_to_packed_mesh(JitBackend backend, const CornerMesh &desc,
                      std::string_view name = "",
                      bool face_normals = false,
                      bool flip_normals = false,
                      const PackedMesh::ScalarAffineTransform4f &to_world = {});

/// Magic number and current version of the ``.serialized`` mesh encoding,
/// whose reader and writer are ``SerializedMesh::load_v5()`` and
/// ``Mesh::write_serialized(Stream*)``. Keep the two in step.
constexpr uint16_t SerializedMagic   = 0x041C;
constexpr uint16_t SerializedVersion = 0x0005;

/// Flag word of a ``.serialized`` file. The low bits store the
/// `Layout` of the vertex records verbatim.
enum class SerializedFlags : uint32_t {
    LayoutMask      = 0x000F,
    FaceNormals     = 0x0010,
    SinglePrecision = 0x1000,
};

NAMESPACE_END(mitsuba)
