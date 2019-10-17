#pragma once

/// @file Helper functions for volume data handling.
#include <mitsuba/core/math.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)

template <typename T> T read(std::ifstream &f) {
    T v;
    f.read(reinterpret_cast<char *>(&v), sizeof(v));
    return v;
}

template <typename Float>
MTS_INLINE auto stof(const std::string &s) {
    if constexpr (is_double_v<Float>)
        return std::stod(s);
    else
        return std::stof(s);
}

/// Estimates the transformation from a unit axis-aligned bounding box to the given one.
template <typename BoundingBox>
auto bbox_transform(const BoundingBox &bbox) {
    using Vector3f    = typename BoundingBox::Vector;
    using Float       = value_t<Vector3f>;
    using Transform4f = Transform<Float, 4>;

    Vector3f d        = rcp(bbox.max - bbox.min);
    auto scale_transf = Transform4f::scale(d);
    Vector3f t        = -1.f * Vector3f(bbox.min.x(), bbox.min.y(), bbox.min.z());
    auto translation  = Transform4f::translate(t);
    return scale_transf * translation;
}

NAMESPACE_END(detail)

/**
 * Reads a Mitsuba binary volume file.
 */
// TODO: document data format.
// TODO: what if Float is a GPU array, should we upload to it directly?
template <typename Float, size_t expected_channels>
VolumeMetadata read_binary_volume_data(const std::string &filename,
                                       Vector<FloatBuffer<Float>, expected_channels> *data) {
    using Scalar        = scalar_t<Float>;
    using BoundingBox3f = typename VolumeMetadata::BoundingBox3f;
    using Point3f       = typename BoundingBox3f::Point;

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
    if (meta.channel_count != expected_channels)
        Throw("Expected %d channel in volume data, found %d", expected_channels,
              meta.channel_count);

    // Transform specified in the volume file
    float dims[6];
    f.read(reinterpret_cast<char *>(dims), sizeof(float) * 6);
    meta.bbox =
        BoundingBox3f(Point3f(dims[0], dims[1], dims[2]), Point3f(dims[3], dims[4], dims[5]));
    meta.transform = detail::bbox_transform(meta.bbox);

    for (size_t i = 0; i < expected_channels; ++i)
        set_slices((*data)[i], size);
    meta.mean = 0.;
    meta.max  = -math::Infinity<Float>;
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < expected_channels; ++j) {
            auto val = detail::read<float>(f);
            slice((*data)[j], i) = val;
            meta.mean += (double) val;
            meta.max = std::max(meta.max, (Scalar) val);
        }
    }
    meta.mean /= double(size * meta.channel_count);

    Log(Debug, "Loaded grid volume data from file %s: dimensions %s, mean value %f, max value %f",
        filename, meta.shape, meta.mean, meta.max);

    return meta;
}

NAMESPACE_END(mitsuba)
