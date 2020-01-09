#pragma once

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/// Holds metadata about a volume, e.g. when loaded from a Mitsuba binary volume file.
struct VolumeMetadata {
    using Float = float;
    MTS_IMPORT_CORE_TYPES()

    std::string filename;
    uint8_t version;
    int32_t data_type;
    Vector3i shape;
    size_t channel_count;
    BoundingBox3f bbox;
    Transform4f transform;

    double mean = 0.;
    float max;
};

NAMESPACE_END(mitsuba)
