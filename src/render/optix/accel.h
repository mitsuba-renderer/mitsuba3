/*
    optix/accel.h -- Backend-agnostic interface to the OptiX acceleration
    structure builder (src/render/optix/accel.cpp).

    Consumes the \ref SceneIR that scene_optix.inl lowers the scene into and
    builds the OptiX objects: one GAS per BLAS, the shader binding-table hit
    records, and the IAS. It is non-templated because everything it needs lives
    in the descriptors.
*/

#pragma once

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/scene_ir.h>
#include <mitsuba/core/logger.h>

#include <drjit/array_router.h> // dr::tzcnt
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// One built GAS per \ref BlasEntry, in BLAS order. A \ref ShapeGroup owns one
/// of these (its per-group GAS); the scene builder owns one for the top-level
/// BLASes. The destructor (freeing the device buffers) is in
/// src/render/optix/accel.cpp.
struct MiOptixAccelData {
    struct HandleData {
        OptixTraversableHandle handle = 0ull;
        void *buffer = nullptr;
    };
    /// One handle per BlasEntry built into this scope, index-aligned with the
    /// index list passed to \ref build_gas().
    std::vector<HandleData> gas;

    ~MiOptixAccelData();
};

/// Per-shape SBT data buffers (device), indexed by \ref ShapeIR::data_slot.
/// Allocated once, refilled in place so the SBT records stay valid.
using ShapeDataBuffers = std::vector<void *>;

/// Number of ShapeType bit positions (the highest, ShapeGroup, is bit 11), which
/// sizes the lookup table below. Only geometry types are ever stored or queried.
#define MI_SHAPE_TYPE_NUM_BITS 12

/// Map a \ref ShapeType to an OptiX program group index, keyed by the type's
/// lowest set bit.
struct OptixProgramGroupMapping {
    uint32_t mapping[MI_SHAPE_TYPE_NUM_BITS];

    OptixProgramGroupMapping() {
        for (uint32_t i = 0; i < MI_SHAPE_TYPE_NUM_BITS; ++i)
            mapping[i] = (uint32_t) -1;
    }

    uint32_t index(ShapeType type) const {
        uint32_t index = dr::tzcnt((uint32_t) type);
        if (index >= MI_SHAPE_TYPE_NUM_BITS)
            Throw("OptixProgramGroupMapping: invalid shape type!");
        return index;
    }

    const uint32_t &operator[](ShapeType type) const { return mapping[index(type)]; }
    uint32_t &operator[](ShapeType type) { return mapping[index(type)]; }

    uint32_t at(ShapeType type) const {
        uint32_t value = operator[](type);
        if (value == (uint32_t) -1)
            Throw("OptixProgramGroupMapping: shape type not mapped!");
        return value;
    }
};

/// Counts the HitGroupSbtRecords the BLAS list contributes (one per geom, since
/// every geom packs exactly one record). \c blases is the full
/// \ref SceneIR::blases (top-level BLASes first, then groups, in slot order).
extern MI_EXPORT_LIB size_t
count_hitgroup_records(const std::vector<BlasEntry> &blases);

/// Packs the HitGroupSbtRecords for every geom of every BLAS in \c blases into
/// \c out, advancing \c cursor. The BLAS list is already in canonical (kind,
/// slot) order, so the resulting SBT layout is contiguous per BLAS and matches
/// the offsets \ref prepare_ias assigns. Allocates the referenced
/// custom-primitive data buffers in \c data_buffers (without filling them: the
/// records only need the stable pointer); \ref optix_refresh_shape_data writes
/// their contents.
extern MI_EXPORT_LIB void
fill_hitgroup_records(const std::vector<BlasEntry> &blases,
                      HitGroupSbtRecord *out, size_t &cursor,
                      const OptixProgramGroup *pg,
                      const OptixProgramGroupMapping &pg_mapping,
                      ShapeDataBuffers &data_buffers);

/// Refill every custom shape's SBT data buffer in place (used when parameters
/// change: the SBT keeps pointing at the same buffers, so updated per-primitive
/// values reach the device without rebuilding the SBT).
extern MI_EXPORT_LIB void
optix_refresh_shape_data(const SceneIR &sd, ShapeDataBuffers &data_buffers);

/**
 * \brief Build one OptiX geometry acceleration structure (GAS) per BLAS, storing
 * the handles in \c out_accel.gas (index-aligned with \c indices).
 *
 * \c indices selects the BLASes (indices into \c blases) built into this scope:
 * \ref SceneIR::top_blases for the top level, or a group's
 * \ref SceneIR::group_blases entry for an instanced ShapeGroup. Each BLAS
 * holds only same-kind geometry. The build-input type derives from each geom's
 * \ref ShapeIR::Kind "kind". Set \c compact to run OptiX GAS compaction after
 * the build.
 */
extern MI_EXPORT_LIB void
build_gas(const OptixDeviceContext &context,
          const std::vector<BlasEntry> &blases,
          const std::vector<uint32_t> &indices,
          MiOptixAccelData &out_accel,
          bool compact);

/// Fills the \ref OptixInstance array (one per \ref SceneIR::instances entry)
/// into \c out (sized \c sd.instances.size()). \c blas_handle and
/// \c blas_sbt_offset are indexed by global BLAS index: the GAS traversable and
/// the SBT base offset of each BLAS. The per-instance face-cull flag derives
/// from the referenced BLAS's \ref ShapeIR::Kind "kind".
extern MI_EXPORT_LIB void
prepare_ias(const SceneIR &sd,
            const std::vector<OptixTraversableHandle> &blas_handle,
            const std::vector<uint32_t> &blas_sbt_offset,
            OptixInstance *out);

NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
