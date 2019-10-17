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

#if defined(SINGLE_PRECISION)
inline Float stof(const std::string &s) { return std::stof(s); }
#else
inline Float stof(const std::string &s) { return std::stod(s); }
#endif

/// Estimates the transformation from a unit axis-aligned bounding box to the given one.
Transform4f bbox_transform(const BoundingBox3f bbox) {
    Vector3f d        = rcp(bbox.max - bbox.min);
    auto scale_transf = Transform4f::scale(d);
    Vector3f t        = -1.f * Vector3f(bbox.min.x(), bbox.min.y(), bbox.min.z());
    auto translation  = Transform4f::translate(t);
    return scale_transf * translation;
}

NAMESPACE_END(detail)

struct VolumeMetadata {
    std::string filename;
    uint8_t version;
    int32_t data_type;
    Vector3i shape;
    size_t channel_count;
    BoundingBox3f bbox;
    Transform4f transform;

    double mean = 0.;
    Float max;
};

/**
 * Base class for 3D textures based on trilinearly interpolated volume data.
 */
class Grid3DBase : public Texture3D {
public:
    MTS_AUTODIFF_GETTER(nx, m_metadata.shape.x(), m_shape_d.x())
    MTS_AUTODIFF_GETTER(ny, m_metadata.shape.y(), m_shape_d.y())
    MTS_AUTODIFF_GETTER(nz, m_metadata.shape.z(), m_shape_d.z())
    MTS_AUTODIFF_GETTER(z_offset, m_z_offset, m_z_offset_d)

public:
    explicit Grid3DBase(const Properties &props) : Texture3D(props) {}

    void set_metadata(const VolumeMetadata &meta, bool use_grid_bbox) {
        m_metadata = meta;
        m_size     = hprod(m_metadata.shape);
        m_z_offset = m_metadata.shape.y() * m_metadata.shape.x();
        if (use_grid_bbox) {
            m_world_to_local = m_metadata.transform * m_world_to_local;
            update_bbox();
        }

#if defined(MTS_ENABLE_AUTODIFF)
        // Avoids resolution being treated as a literal
        m_shape_d.x() = UInt32C::copy(&m_metadata.shape.x(), 1);
        m_shape_d.y() = UInt32C::copy(&m_metadata.shape.y(), 1);
        m_shape_d.z() = UInt32C::copy(&m_metadata.shape.z(), 1);
        m_z_offset_d  = UInt32C::copy(&m_z_offset, 1);
#endif
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void parameters_changed() override {
        size_t new_size = data_size();
        if (m_size != new_size) {
            // Only support a special case: resolution doubling along all axes
            if (new_size != m_size * 8)
                Throw("Unsupported Grid3DBase data size update: %d -> %d. Expected %d or %d "
                      "(doubling "
                      "the resolution).",
                      m_size, new_size, m_size, m_size * 8);
            m_metadata.shape *= 2;
            m_z_offset = m_metadata.shape.y() * m_metadata.shape.x();
            m_shape_d *= 2;
            m_z_offset_d = m_shape_d.y() * m_shape_d.x();
            m_size       = new_size;
        }
    }

    virtual size_t data_size() const = 0;
#endif

    Float mean() const override { return (Float) m_metadata.mean; }
    Float max() const override { return m_metadata.max; }
    Vector3i resolution() const override { return m_metadata.shape; };

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Grid3DBase[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  dimensions = " << m_metadata.shape << "," << std::endl
            << "  mean = " << m_metadata.mean << "," << std::endl
            << "  max = " << m_metadata.max << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    VolumeMetadata m_metadata;
    size_t m_size, m_z_offset;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Maintain these variables separately to avoid having them baked as literals
    UInt32D m_z_offset_d;
    Vector3iD m_shape_d;
#endif
};

/**
 * Reads a Mitsuba 1 binary volume file.
 */
template <size_t expected_channels, typename Value>
VolumeMetadata read_binary_volume_data(const std::string &filename, Value *data) {
    if constexpr (expected_channels == 1) {
        static_assert(std::is_same_v<FloatX, Value>);
    } else {
        static_assert(is_dynamic_array_v<value_t<Value>>);
        static_assert(Value::Size == expected_channels);
    }

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
        Throw("Wrong type, currently only type == 1 (Float32) data is "
              "supported (found type = %d)",
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

    set_slices(*data, size);
    meta.mean = 0.;
    meta.max  = -math::Infinity;
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < meta.channel_count; ++j) {
            auto val = detail::read<float>(f);
            if constexpr (expected_channels == 1)
                slice(*data, i) = val;
            else
                slice((*data)[j], i) = val;
            meta.mean += (double) val;
            meta.max = std::max(meta.max, (Float) val);
        }
    }
    meta.mean /= (double) (size * meta.channel_count);

    Log(EDebug, "Loaded grid volume data from file %s: dimensions %s, mean value %f, max value %f",
        filename, meta.shape, meta.mean, meta.max);

    return meta;
}

/**
 * Reads a 3D grid of float values from a string (very inefficient, mostly for testing).
 */
VolumeMetadata parse_string_grid(const Properties &props, FloatX *data) {
    VolumeMetadata meta;

    if (props.has_property("side"))
        meta.shape = props.size_("side");
    meta.shape.x() = props.size_("nx", meta.shape.x());
    meta.shape.y() = props.size_("ny", meta.shape.y());
    meta.shape.z() = props.size_("nz", meta.shape.z());
    size_t size    = hprod(meta.shape);
    if (size < 8)
        Throw("Invalid grid dimensions: %d x %d x %d < 8 (must have at "
              "least one value for each corner)",
              meta.shape.x(), meta.shape.y(), meta.shape.z());

    set_slices(*data, size);
    std::vector<std::string> tokens = string::tokenize(props.string("values"), ",");
    if (tokens.size() != size)
        Throw("Invalid token count: expected %d, found %d in "
              "comma-separated list:\n  \"%s\"",
              size, tokens.size(), props.string("values"));

    meta.mean = 0.;
    meta.max  = -math::Infinity;
    for (size_t i = 0; i < size; ++i) {
        Float val       = detail::stof(tokens[i]);
        slice(*data, i) = val;
        meta.mean += (double) val;
        meta.max = std::max(meta.max, (Float) val);
    }
    meta.mean /= (double) size;

    Log(EDebug, "Parsed grid volume data from string: dimensions %s, mean value %f, max value %f",
        meta.shape, meta.mean, meta.max);

    return meta;
}

NAMESPACE_END(mitsuba)
