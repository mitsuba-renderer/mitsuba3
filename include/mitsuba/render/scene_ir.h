/*
    scene_ir.h -- Backend-neutral IR to feed acceleration structure builders.
*/

#pragma once

#include <mitsuba/render/fwd.h>
#include <cstddef>
#include <cstdint>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

struct ShapeIR {
    /// Mitsuba bundles each of the following geometry kinds into its own BLAS.
    /// Instance must remain last (see \ref NumGeometryKinds).
    enum class Kind : uint32_t {
        Triangles,        ///< double-sided meshes
        TrianglesCulled,  ///< back-face-culled meshes (EllipsoidsMesh)
        BSplineCurve,     ///< cubic B-spline curve
        LinearCurve,      ///< linear curve
        Custom,           ///< AABB/implicit: sphere, disk, cylinder, sdfgrid, ellipsoids
        Instance          ///< ShapeGroup instance, carries no geometry and is never bucketed
    };

    Kind kind = Kind::Custom;
    ShapeType type{};

    // --- Custom (implicit / bounding-box) shapes ---

    /// Number of AABBs / primitives this shape contributes.
    size_t prim_count = 1;

    /// Per-primitive POD size in bytes (0 if the shape writes none).
    size_t pdata_size = 0;

    /// Total per-shape POD size in bytes. 0 means ``prim_count * pdata_size``.
    /// Set explicitly for custom layouts that deviate (e.g. SDFGrid, Ellipsoids).
    size_t data_size = 0;

    /// Writes ``prim_count * 6`` floats (min/max interleaved) to ``out``.
    void (*fill_aabbs)(const void *ctx, void *out) = nullptr;

    /// Writes ``data_size`` bytes of primitive data to ``out``.
    void (*fill_data)(const void *ctx, void *out) = nullptr;

    /// Precomputed device AABB buffer for zero-copy builds (OptiX only; Metal
    /// always copies via \ref fill_aabbs).
    const void *aabb_buffer = nullptr;

    /// Opaque context passed to the fill callbacks (the owning Shape).
    const void *ctx = nullptr;

    /// Stable per-shape storage index assigned by \ref SceneIRBuilder (OptiX
    /// only; Metal uses its own per-BLAS lookup table).
    uint32_t data_slot = (uint32_t) -1;

    // --- Triangles (mesh, ellipsoidsmesh) ---

    /// Backend-appropriate buffer handles (host pointer on CPU/Embree, device
    /// pointer on Metal/OptiX).
    const void *vertex_ptr = nullptr;
    const void *index_ptr = nullptr;
    size_t vertex_count = 0, face_count = 0;

    // --- Curve (linear / b-spline) ---

    /// Interleaved (x, y, z, radius) control points and uint32 segment indices.
    const void *cp_ptr = nullptr;
    size_t cp_count = 0;
    const void *seg_ptr = nullptr;
    size_t seg_count = 0;

    // --- Instance (instance, shapegroup) ---

    /// Column-major 3x4 affine (four columns of three floats).
    float to_world[12] = { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                           0.f, 0.f, 1.f, 0.f, 0.f, 0.f };

    /// BLAS-set cache key (shared by all instances of one ShapeGroup).
    const void *group_id = nullptr;

    /// Resolved per-shape POD byte count (see \ref data_size).
    size_t data_size_bytes() const {
        return data_size ? data_size : prim_count * pdata_size;
    }
};

/// Number of geometry kinds and the bucket-array size for BLAS partitioning.
/// Instance is excluded, hence it must remain the last \ref ShapeIR::Kind.
static constexpr size_t NumGeometryKinds = (size_t) ShapeIR::Kind::Instance;

// ---------------------------------------------------------------------------
//  Whole-scene descriptor
// ---------------------------------------------------------------------------

/// \ref InstanceEntry::owner_registry_id for a top-level BLAS.
static constexpr uint32_t SCENE_IR_NO_OWNER = (uint32_t) -1;

/// One bottom-level acceleration structure holding same-kind geometry.
struct BlasEntry {
    ShapeIR::Kind        kind;
    std::vector<ShapeIR> geoms;
};

/// One flattened TLAS/IAS instance: a transformed reference to a \ref BlasEntry.
struct InstanceEntry {
    /// Index into \ref SceneIR::blases.
    uint32_t blas_index = 0;

    /// Column-major 3x4 affine, identity for a top-level BLAS.
    float    to_world[12] = { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                              0.f, 0.f, 1.f, 0.f, 0.f, 0.f };

    /// JIT registry ID of the ShapeGroup, or ``SCENE_IR_NO_OWNER``.
    uint32_t owner_registry_id = SCENE_IR_NO_OWNER;
};

/// Scene description consumed by acceleration-structure builders.
struct SceneIR {
    /// BLAS entries in build order: top-level first, then ShapeGroup entries.
    std::vector<BlasEntry>     blases;

    /// Flattened TLAS/IAS instances referencing entries in \ref blases.
    std::vector<InstanceEntry> instances;

    /// Indices in \ref blases that belong to top-level scene geometry.
    std::vector<uint32_t>      top_blases;

    /// BLAS indices generated for each ShapeGroup's children, indexed in the
    /// same order as \c scene->shapegroups(). Backends use entry \c i to
    /// rebuild the scene's i-th ShapeGroup.
    std::vector<std::vector<uint32_t>> group_blases;
};

/// Lower variant-specific scenes to backend-neutral \ref SceneIR.
template <typename Float, typename Spectrum>
struct MI_EXPORT_LIB SceneIRBuilder {
    /**
     * \brief Walk the \c scene once and lower it to a \ref SceneIR.
     *
     * 1. Visit top-level shapes first, then ShapeGroup children. Describe each
     *    shape exactly once and assign it a stable \c data_slot in that order.
     *    Backends use the slot as the persistent index for per-shape storage
     *    such as custom primitive data buffers.
     *
     * 2. Partition non-instance geometry by \ref ShapeIR::Kind. Each non-empty
     *    bucket becomes one \ref BlasEntry. Emit top-level BLASes first, then
     *    one shared BLAS set per ShapeGroup.
     *
     * 3. Flatten the TLAS/IAS instance list: one identity \ref InstanceEntry per
     *    top-level BLAS, then one transformed entry for every \c Instance and
     *    every BLAS of its referenced ShapeGroup.
     */
    static SceneIR build(Scene<Float, Spectrum> *scene);
};

MI_EXTERN_STRUCT(SceneIRBuilder)

NAMESPACE_END(mitsuba)
