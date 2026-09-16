/*
    scene_ir.h -- Backend-neutral IR to feed acceleration structure builders.
*/

#pragma once

#include <mitsuba/render/fwd.h>
#include <drjit-core/jit.h>
#include <cstddef>
#include <cstdint>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

enum class ShapeVisibility : uint32_t;

/**
 * Mask that the acceleration data structures store for a shape
 *
 * Shapes with null transmission occupy the upper three bits, see `RayMask`
 * for the layout. A ray intersects the shape when its ray mask overlaps
 * this value.
 */
inline uint32_t accel_mask(ShapeVisibility visibility, bool has_null) {
    return (uint32_t) visibility << (has_null ? 3 : 0);
}

/// Decomposed keyframe intermediate representation for hardware acceleration backends.
struct KeyframeIR {
    float time;
    float scale[3];
    /// Rotation quaternion in ``(x, y, z, w)`` order
    float quat[4];
    float trans[3];
};

struct ShapeIR {
    /// Mitsuba bundles each of the following geometry kinds into its own BLAS.
    /// Instance must remain last (see ``NumGeometryKinds``).
    enum class Kind : uint32_t {
        /// Double-sided meshes
        Triangles,

        /// Back-face-culled meshes (EllipsoidsMesh)
        TrianglesCulled,

        /// Cubic B-spline curve
        BSplineCurve,

        /// Linear curve
        LinearCurve,

        /// AABB/implicit: sphere, disk, cylinder, sdfgrid, ellipsoids
        Custom,

        /// ShapeGroup instance, carries no geometry and is never bucketed
        Instance
    };

    Kind kind = Kind::Custom;

    /// Visibility mask (see ``accel_mask()``), filled in by
    /// ``SceneIRBuilder``. Backends with per-instance masks (OptiX, Metal)
    /// rely on same-mask geometry sharing one BLAS.
    uint32_t visibility_mask = 0xFFu;

    // --- Custom (implicit / bounding-box) shapes ---

    /// Number of AABBs / primitives this shape contributes.
    size_t prim_count = 1;

    /// Writes ``prim_count`` boxes of six floats (min xyz, max xyz) to ``out``.
    void (*fill_aabbs)(const void *ctx, void *out) = nullptr;

    /// Device buffer holding the boxes in the same layout. The GPU backends
    /// build from it directly and only call ``fill_aabbs`` when it is null.
    const void *aabb_buffer = nullptr;

    /// Opaque context passed to ``fill_aabbs`` (the owning `Shape`).
    const void *ctx = nullptr;

    /// Intersection function recorded from ``Shape::ray_intersect_preliminary()``
    /// by ``SceneIRBuilder`` (JIT variants), which the backend binds to the
    /// geometry. ``SceneIR::isect_funcs`` owns the handle.
    uint32_t isect_func = 0;

    // --- Triangles (mesh, ellipsoidsmesh) ---

    /// Backend-appropriate buffer handles (host pointer on CPU/Embree, device
    /// pointer on Metal/OptiX).
    const void *vertex_ptr = nullptr;
    const void *index_ptr = nullptr;
    size_t vertex_count = 0, face_count = 0;

    /// Distance between consecutive vertex records in bytes; the position
    /// occupies the first three floats of each record.
    size_t vertex_stride = 3 * sizeof(float);

    /// Distance between consecutive face records in bytes; the three vertex
    /// indices occupy the first three words of each record. Metal inputs
    /// must be tightly packed (12 bytes).
    size_t index_stride = 3 * sizeof(uint32_t);

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

    /// Keyframes for animated instances.
    std::vector<KeyframeIR> keyframes;
};

/// Number of geometry kinds and the bucket-array size for BLAS partitioning.
/// Instance is excluded, hence it must remain the last ``ShapeIR.Kind``.
static constexpr size_t NumGeometryKinds = (size_t) ShapeIR::Kind::Instance;

// ---------------------------------------------------------------------------
//  Whole-scene descriptor
// ---------------------------------------------------------------------------

/// One bottom-level acceleration structure holding geometry that shares one
/// kind and one visibility mask.
struct BlasEntry {
    ShapeIR::Kind        kind;
    uint32_t             visibility_mask;
    std::vector<ShapeIR> geoms;
};

/// One flattened TLAS/IAS instance: a transformed reference to a ``BlasEntry``.
struct InstanceEntry {
    /// Index into ``SceneIR.blases``.
    uint32_t blas_index = 0;

    /// Column-major 3x4 affine, identity for a top-level BLAS.
    float    to_world[12] = { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                              0.f, 0.f, 1.f, 0.f, 0.f, 0.f };

    /// Index + 1 of the owning ``instance``, or 0 for a top-level BLAS.
    uint32_t instance_index = 0;
};

/// Scene description consumed by acceleration-structure builders.
struct SceneIR {
    /// BLAS entries in build order: top-level first, then ShapeGroup entries.
    std::vector<BlasEntry>     blases;

    /// Flattened TLAS/IAS instances referencing entries in ``blases``.
    std::vector<InstanceEntry> instances;

    /// The scene's ``Instance`` shapes in order of appearance, for backends
    /// that instance nested scenes directly (Embree). ``instances`` derives
    /// from these.
    std::vector<ShapeIR> instance_shapes;

    /// Indices in ``blases`` that belong to top-level scene geometry.
    std::vector<uint32_t>      top_blases;

    /// BLAS indices generated for each ShapeGroup's children, indexed in the
    /// same order as ``scene->shapegroups()``. Backends use entry ``i`` to
    /// rebuild the scene's i-th ShapeGroup.
    std::vector<std::vector<uint32_t>> group_blases;

    /// Handle variables of the recorded intersection functions (see
    /// ``ShapeIR::isect_func``), released with the IR. Bindings retain
    /// their own reference.
    std::vector<uint32_t> isect_funcs;

    /// Does any instance have more than one keyframe?
    bool has_motion = false;

    /// Time range spanned by the keyframes of all animated instances
    float time_min = 0.f, time_max = 0.f;

    /// Return the keyframes of the instance that owns ``inst``. The result is
    /// empty for static instances and top-level BLAS entries.
    const std::vector<KeyframeIR> &keyframes(const InstanceEntry &inst) const {
        static const std::vector<KeyframeIR> empty;
        return inst.instance_index
                   ? instance_shapes[inst.instance_index - 1].keyframes
                   : empty;
    }

    SceneIR() = default;
    SceneIR(SceneIR &&) = default;
    SceneIR(const SceneIR &) = delete;
    SceneIR &operator=(const SceneIR &) = delete;
    SceneIR &operator=(SceneIR &&) = delete;
    ~SceneIR() {
        for (uint32_t index : isect_funcs)
            jit_var_dec_ref(index);
    }
};

/// Lower variant-specific scenes to backend-neutral ``SceneIR``.
template <typename Float, typename Spectrum>
struct MI_EXPORT_LIB SceneIRBuilder {
    /**
     * Walk the ``scene`` once and lower it to a ``SceneIR``.
     *
     * 1. Visit top-level shapes first, then ShapeGroup children. Describe each
     *    shape exactly once and, in JIT variants, record the intersection
     *    routine of each custom shape (``ShapeIR::isect_func``).
     *
     * 2. Partition non-instance geometry by ``ShapeIR.Kind`` and visibility
     *    mask. Each non-empty bucket becomes one ``BlasEntry``. Emit top-level
     *    BLASes first, then one shared BLAS set per ShapeGroup.
     *
     * 3. Flatten the TLAS/IAS instance list: one identity ``InstanceEntry`` per
     *    top-level BLAS, then one transformed entry for every ``Instance`` and
     *    every BLAS of its referenced ShapeGroup.
     */
    static SceneIR build(Scene<Float, Spectrum> *scene);
};

MI_EXTERN_STRUCT(SceneIRBuilder)

NAMESPACE_END(mitsuba)
