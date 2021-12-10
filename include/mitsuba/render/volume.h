#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/// Abstract base class for 3D volumes.
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Volume : public Object {
public:
    MI_IMPORT_TYPES(Texture)

    // ======================================================================
    //! @{ \name Volume interface
    // ======================================================================

    /// Evaluate the volume at the given surface interaction, with color processing.
    virtual UnpolarizedSpectrum eval(const Interaction3f &it, Mask active = true) const;

    /// Evaluate this volume as a single-channel quantity.
    virtual Float eval_1(const Interaction3f &it, Mask active = true) const;

    /// Evaluate this volume as a three-channel quantity with no color processing (e.g. velocity field).
    virtual Vector3f eval_3(const Interaction3f &it, Mask active = true) const;

   /**
     * Evaluate this volume as a six-channel quantity with no color processing
     * This interface is specifically intended to encode the parameters of an SGGX phase function.
     */
    virtual dr::Array<Float, 6> eval_6(const Interaction3f &it, Mask active = true) const;

    /**
     * Evaluate this volume as a n-channel quantity with no color processing
     * This interface is specifically intended to encode a variable number of parameters
     */
    virtual std::vector<Spectrum> eval_generic(const Interaction3f &it, Mask active = true) const;
    virtual std::vector<Float> eval_generic_1(const Interaction3f &it, Mask active = true) const;

    /**
     * Evaluate the volume at the given surface interaction,
     * and compute the gradients of the linear interpolant as well.
     */
    virtual std::pair<UnpolarizedSpectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                                   Mask active = true) const;

    /// Returns the maximum value of the volume over all dimensions.
    virtual ScalarFloat max() const;

    /// Returns the maximum value of the volume over all dimensions per channel.
    virtual std::vector<ScalarFloat> max_generic() const;

    /// Returns the bounding box of the volume
    ScalarBoundingBox3f bbox() const { return m_bbox; }

    /**
     * \brief Returns the resolution of the volume, assuming that it is based
     * on a discrete representation.
     *
     * The default implementation returns <tt>(1, 1, 1)</tt>
     */
    virtual ScalarVector3i resolution() const;

    //! @}
    // ======================================================================

    /// Returns a human-reable summary
    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Volume[" << std::endl
            << "  to_local = " << m_to_local << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    Volume(const Properties &props);
    virtual ~Volume() {}

    void update_bbox() {
        ScalarTransform4f to_world = m_to_local.inverse();
        ScalarPoint3f a = to_world * ScalarPoint3f(0.f, 0.f, 0.f);
        ScalarPoint3f b = to_world * ScalarPoint3f(1.f, 1.f, 1.f);
        m_bbox = ScalarBoundingBox3f(a, b);
    }

protected:
    /// Used to bring points in world coordinates to local coordinates.
    ScalarTransform4f m_to_local;
    /// Bounding box
    ScalarBoundingBox3f m_bbox;
};

MI_EXTERN_CLASS(Volume)
NAMESPACE_END(mitsuba)
