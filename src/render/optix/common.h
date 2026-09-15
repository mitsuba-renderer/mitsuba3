#pragma once

#include <cstdint>
#include <mitsuba/render/optix_api.h>

/// Stores information about a `Shape` on the Optix side
struct OptixHitGroupData {
    /// Shape id in Dr.Jit's pointer registry
    uint32_t shape_registry_id;
    /// Data block of the shape's recorded intersection routine (see jit_isect_bind())
    void* data;
};

template <typename T>
struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
    SbtRecord(const T &data) : data(data) { }
};

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) EmptySbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

using RayGenSbtRecord   = EmptySbtRecord;
using MissSbtRecord     = EmptySbtRecord;
using HitGroupSbtRecord = SbtRecord<OptixHitGroupData>;
