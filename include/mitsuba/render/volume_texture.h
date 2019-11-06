#pragma once

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/// Holds metadata about a volume, e.g. when loaded from a Mitsuba binary volume file.
struct VolumeMetadata {
    // TODO: how to parametrize over single / double precision?
    using Vector3i = Vector<int32_t, 3>;
    using Point3f = Point<float, 3>;
    using BoundingBox3f = BoundingBox<Point3f>;
    using Transform4f = Transform<float, 4>;

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

/**
 * Base class for 3D textures based on trilinearly interpolated volume data.
 */
template <typename Float, typename Spectrum>
class Grid3DBase : public Texture3D<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(Grid3DBase, Texture3D)
    MTS_IMPORT_TYPES()
    MTS_USING_BASE(Texture3D, update_bbox, m_world_to_local)

    explicit Grid3DBase(const Properties &props) : Base(props) {}

    void set_metadata(const VolumeMetadata &meta, bool use_grid_bbox) {
        m_metadata = meta;
        m_size     = hprod(m_metadata.shape);
        if (use_grid_bbox) {
            m_world_to_local = m_metadata.transform * m_world_to_local;
            update_bbox();
        }

#if defined(MTS_ENABLE_AUTODIFF)
        // Avoids resolution being treated as a literal
        // TODO: isn't there an Enoki helper for this?
        m_shape_d.x() = UInt32C::copy(&m_metadata.shape.x(), 1);
        m_shape_d.y() = UInt32C::copy(&m_metadata.shape.y(), 1);
        m_shape_d.z() = UInt32C::copy(&m_metadata.shape.z(), 1);
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
            m_size       = new_size;
        }
    }

    virtual size_t data_size() const = 0;
#endif

    Float mean() const override { return Float(m_metadata.mean); }
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

protected:
    VolumeMetadata m_metadata;
    size_t m_size;
};



NAMESPACE_END(mitsuba)
