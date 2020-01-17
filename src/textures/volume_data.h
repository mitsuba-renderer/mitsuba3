#pragma once

#include <fstream>
#include <sstream>

/// @file Helper functions for volume data handling.
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/volume_texture.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)

template <typename T> T read(std::ifstream &f) {
    T v;
    f.read(reinterpret_cast<char *>(&v), sizeof(v));
    return v;
}

/// Estimates the transformation from a unit axis-aligned bounding box to the given one.
template <typename Float>
auto bbox_transform(const BoundingBox<Point<Float, 3>> &bbox) {
    MTS_IMPORT_CORE_TYPES()

    ScalarVector3f d  = rcp(bbox.max - bbox.min);
    auto scale_transf = ScalarTransform4f::scale(d);
    ScalarVector3f t  = -1.f * ScalarVector3f(bbox.min.x(), bbox.min.y(), bbox.min.z());
    auto translation  = ScalarTransform4f::translate(t);
    return scale_transf * translation;
}

NAMESPACE_END(detail)

/**
 * Reads a Mitsuba binary volume file.
 */
// TODO: document data format.
// TODO: what if Float is a GPU array, should we upload to it directly?
template <typename Float>
std::pair<VolumeMetadata, std::unique_ptr<scalar_t<Float>[]>>
read_binary_volume_data(const std::string &filename) {
    MTS_IMPORT_CORE_TYPES()

    VolumeMetadata meta;
    auto fs       = Thread::thread()->file_resolver();
    meta.filename = fs->resolve(filename).string();
    std::ifstream f(meta.filename, std::ios::binary);

    char header[3];
    f.read(header, sizeof(char) * 3);
    if (header[0] != 'V' || header[1] != 'O' || header[2] != 'L')
        Throw("Invalid volume file %s", filename);
    meta.version = detail::read<uint8_t>(f);
    if (meta.version != 3)
        Throw("Invalid version, currently only version 3 is supported (found %d)", meta.version);

    meta.data_type = detail::read<int32_t>(f);
    if (meta.data_type != 1)
        Throw("Wrong type, currently only type == 1 (Float32) data is supported (found type = %d)",
              meta.data_type);

    meta.shape.x() = detail::read<int32_t>(f);
    meta.shape.y() = detail::read<int32_t>(f);
    meta.shape.z() = detail::read<int32_t>(f);
    size_t size    = hprod(meta.shape);
    if (size < 8)
        Throw("Invalid grid dimensions: %d x %d x %d < 8 (must have at "
              "least one value at each corner)",
              meta.shape.x(), meta.shape.y(), meta.shape.z());

    meta.channel_count = detail::read<int32_t>(f);

    // Transform specified in the volume file
    float dims[6];
    f.read(reinterpret_cast<char *>(dims), sizeof(float) * 6);
    meta.bbox      = ScalarBoundingBox3f(ScalarPoint3f(dims[0], dims[1], dims[2]),
                                    ScalarPoint3f(dims[3], dims[4], dims[5]));
    meta.transform = detail::bbox_transform(meta.bbox);
    meta.mean      = 0.;
    meta.max       = -math::Infinity<ScalarFloat>;

    auto raw_data = std::unique_ptr<ScalarFloat[]>(new ScalarFloat[size * meta.channel_count]);
    size_t k      = 0;
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < meta.channel_count; ++j) {
            auto val    = detail::read<float>(f);
            raw_data[k] = val;
            meta.mean += (double) val;
            meta.max = std::max(meta.max, val);
            ++k;
        }
    }
    meta.mean /= double(size * meta.channel_count);

    Log(Debug, "Loaded grid volume data from file %s: dimensions %s, mean value %f, max value %f",
        filename, meta.shape, meta.mean, meta.max);

    return { meta, std::move(raw_data) };
}

NAMESPACE_END(mitsuba)
