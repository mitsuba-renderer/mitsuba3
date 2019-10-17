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

NAMESPACE_END(mitsuba)
