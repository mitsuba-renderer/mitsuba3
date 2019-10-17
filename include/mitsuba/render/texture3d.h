#pragma once

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Abstract base class for spatially-varying 3D textures.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Texture3D : public Object {
public:
    MTS_IMPORT_TYPES()

    explicit Texture3D(const Properties &props) {
        m_world_to_local = props.transform("to_world", Transform4f()).inverse();
        update_bbox();
    }

    // ======================================================================
    //! @{ \name 3D Texture interface
    // ======================================================================

    /// Evaluate the texture at the given surface interaction
    virtual Spectrum eval(const Interaction3f &it, Mask active = true) const = 0;

    /**
     * Evaluate the texture at the given surface interaction,
     * and compute the gradients of the linear interpolant as well.
     */
    virtual std::pair<Spectrum, Vector3f> eval_gradient(const Interaction3f & /*it*/,
                                                        Mask /*active*/ = true) const {
        NotImplementedError("eval_gradient");
    }

    /**
     * \brief Returns the (possibly approximate) mean value of the texture over all
     * dimensions.
     *
     * Not guaranteed to be implemented. The default implementation throws an
     * exception.
     */
    virtual Float mean() const { NotImplementedError("mean"); }

    /// Returns the maximum value of the texture over all dimensions.
    virtual Float max() const { NotImplementedError("max"); }

    /// Returns the bounding box of the 3d texture
    BoundingBox3f bbox() const { return m_bbox; }

    /// Returns the resolution of the texture, defaults to "1"
    virtual Vector3i resolution() const { return Vector3i(1, 1, 1); }

    //! @}
    // ======================================================================

    /// Returns a human-reable summary
    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Texture3D[" << std::endl
            << "  world_to_local = " << m_world_to_local << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    virtual ~Texture3D() {}

    Mask is_inside(const Interaction3f &it, Mask active) const;

    void update_bbox() {
        Point3f a(0.0f, 0.0f, 0.0f);
        Point3f b(1.0f, 1.0f, 1.0f);
        a      = m_world_to_local.inverse() * a;
        b      = m_world_to_local.inverse() * b;
        m_bbox = BoundingBox3f(a);
        m_bbox.expand(b);
    }

protected:
    /// Used to bring points in world coordinates to local coordinates.
    Transform4f m_world_to_local;
    /// Bounding box
    BoundingBox3f m_bbox;

};

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
    MTS_IMPORT_TYPES()
    using Base = Texture3D<Float, Spectrum>;
    using Base::update_bbox;
    using Base::m_world_to_local;

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

    MTS_DECLARE_CLASS()

protected:
    VolumeMetadata m_metadata;
    size_t m_size;
};



NAMESPACE_END(mitsuba)
