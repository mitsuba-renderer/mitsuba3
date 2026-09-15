/*
    optix/accel.h -- Backend-agnostic interface to the OptiX acceleration
    structure builder (src/render/optix/accel.cpp).

    Consumes the `SceneIR` that scene_optix.inl lowers the scene into and
    builds the OptiX objects: one GAS per BLAS, the shader binding-table hit
    records, and the IAS. It is non-templated because everything it needs lives
    in the descriptors.
*/

#pragma once

#if defined(MI_ENABLE_CUDA)

#include "common.h"
#include <mitsuba/render/scene_ir.h>
#include <mitsuba/core/logger.h>

#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// One built GAS per `BlasEntry`, in BLAS order. A `ShapeGroup` owns one
/// of these (its per-group GAS); the scene builder owns one for the top-level
/// BLASes. The destructor (freeing the device buffers) is in
/// src/render/optix/accel.cpp.
struct MiOptixAccelData {
    struct HandleData {
        OptixTraversableHandle handle = 0ull;
        void *buffer = nullptr;
    };
    /// One handle per BlasEntry built into this scope, index-aligned with the
    /// index list passed to `build_gas()`.
    std::vector<HandleData> gas;

    ~MiOptixAccelData();
};

/// Slot of a geometry kind's program group index: triangles, then the two
/// curve types. Custom shapes bring their own program groups (see below).
inline uint32_t optix_pg_slot(ShapeIR::Kind kind) {
    switch (kind) {
        case ShapeIR::Kind::Triangles:
        case ShapeIR::Kind::TrianglesCulled: return 0;
        case ShapeIR::Kind::BSplineCurve:    return 1;
        case ShapeIR::Kind::LinearCurve:     return 2;
        default: Throw("optix_pg_slot(): geometry kind has no program group!");
    }
}

/**
 * Packs the HitGroupSbtRecords for every geom of every BLAS in ``blases`` into
 * ``out``. The BLAS list is already in canonical (kind, slot) order, so the
 * resulting SBT layout is contiguous per BLAS and matches the offsets
 * `prepare_ias()` assigns. ``pg_index`` maps the slots of ``optix_pg_slot()``
 * to entries of ``pg``.
 *
 * The records of custom shapes (those with a ``ShapeIR::isect_func``) only
 * receive their registry id: Dr.Jit writes their header and data pointer for
 * the bound intersection routine before each launch (see jit_isect_bind()).
 */
extern MI_EXPORT_LIB void
fill_hitgroup_records(const std::vector<BlasEntry> &blases,
                      HitGroupSbtRecord *out,
                      const OptixProgramGroup *pg,
                      const uint32_t *pg_index);

/**
 * Build one OptiX geometry acceleration structure (GAS) per BLAS, storing
 * the handles in ``out_accel.gas`` (index-aligned with ``indices``).
 *
 * ``indices`` selects the BLASes (indices into ``blases``) built into this
 * scope: `SceneIR::top_blases` for the top level, or a group's
 * `SceneIR::group_blases` entry for an instanced ShapeGroup. Each BLAS
 * holds only same-kind geometry. The build-input type derives from each
 * geom's `ShapeIR::Kind`. Set ``compact`` to run OptiX GAS compaction after
 * the build.
 */
extern MI_EXPORT_LIB void
build_gas(const OptixDeviceContext &context,
          const std::vector<BlasEntry> &blases,
          const std::vector<uint32_t> &indices,
          MiOptixAccelData &out_accel,
          bool compact);

/// Fills the `OptixInstance` array (one per `SceneIR::instances` entry)
/// into ``out`` (sized ``sd.instances.size()``). ``blas_handle`` and
/// ``blas_sbt_offset`` are indexed by global BLAS index: the GAS traversable
/// and the SBT base offset of each BLAS. The per-instance face-cull flag
/// derives from the referenced BLAS's `ShapeIR::Kind`.
extern MI_EXPORT_LIB void
prepare_ias(const SceneIR &sd,
            const std::vector<OptixTraversableHandle> &blas_handle,
            const std::vector<uint32_t> &blas_sbt_offset,
            OptixInstance *out);

NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
