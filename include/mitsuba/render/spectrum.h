#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract continuous spectral power distribution data type,
 * which supports evaluation at arbitrary wavelengths.
 *
 * \remark The term 'continuous' does not imply that the underlying spectrum
 * must be continuous, but rather emphasizes that it is a function defined on
 * the set of real numbers (as opposed to the discretely sampled spectrum,
 * which only stores samples at a finite set of wavelengths).
 *
 * A continuous spectrum can also be vary with respect to a spatial position.
 * The (optional) texture interface at the bottom can be implemented to support
 * this. The default implementation strips the position information and falls
 * back the non-textured implementation.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER ContinuousSpectrum : public Object {
public:
    MTS_DECLARE_CLASS_VARIANT(ContinuousSpectrum, Object, "spectrum")
    MTS_IMPORT_TYPES()

    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param wavelengths
     *     List of wavelengths specified in nanometers
     */
    virtual Spectrum eval(const Wavelength &wavelengths, Mask active = true) const;

    /**
     * \brief Importance sample the spectral power distribution
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param sample
     *     A uniform variate for each desired wavelength.
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *
     *     2. The Monte Carlo importance weight (Spectral power
     *        density value divided by the sampling density)
     */
    virtual std::pair<Wavelength, Spectrum>
    sample(const Wavelength &sample, Mask active = true) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrum pdf(const Wavelength &wavelengths, Mask active = true) const;

    // ======================================================================
    //! @{ \name Texture interface implementation
    //!
    //! The texture interface maps a given surface position and set of
    //! wavelengths to spectral reflectance values in the [0, 1] range.
    //!
    //! The default implementations simply ignore the spatial information
    //! and fall back to the above non-textured implementations.
    // ======================================================================

    /// Evaluate the texture at the given surface interaction, with color processing.
    virtual Spectrum eval(const SurfaceInteraction3f &si, Mask active = true) const;

    /// Evaluate this texture as a three-channel quantity with no color processing (e.g. normal map).
    virtual Vector3f eval3(const SurfaceInteraction3f &si, Mask active = true) const;

    /// Evaluate this texture as a single-channel quantity.
    virtual Float eval1(const SurfaceInteraction3f &si, Mask active = true) const;

    /**
     * \brief Importance sample the (textured) spectral power distribution
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \param sample
     *     A uniform variate for each desired wavelength.
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *
     *     2. The Monte Carlo importance weight (Spectral power
     *        distribution value divided by the sampling density)
     */
    virtual std::pair<Wavelength, Spectrum>
    sample(const SurfaceInteraction3f &si, const Spectrum &sample, Mask active = true) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrum pdf(const SurfaceInteraction3f &si, Mask active = true) const;

    //! @}
    // ======================================================================

    /**
     * Return the mean value of the spectrum over the support
     * (MTS_WAVELENGTH_MIN..MTS_WAVELENGTH_MAX)
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    virtual ScalarFloat mean() const;

    /**
     * Convenience method returning the standard D65 illuminant.
     */
    static ref<ContinuousSpectrum> D65(ScalarFloat scale = 1.f);

protected:
    virtual ~ContinuousSpectrum();
};


template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Texture : public ContinuousSpectrum<Float, Spectrum> {
public:
    // TODO: should this define an interface?
    MTS_DECLARE_CLASS_VARIANT(Texture, ContinuousSpectrum, "texture")
    MTS_IMPORT_TYPES();

protected:
    virtual ~Texture();
};


/**
 * Abstract base class for spatially-varying 3D textures.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Texture3D : public ContinuousSpectrum<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(Texture3D, ContinuousSpectrum)
    MTS_IMPORT_TYPES()
    MTS_USING_BASE(ContinuousSpectrum, eval, eval3, eval1)

    Texture3D(const Properties &props);

    // ======================================================================
    //! @{ \name 3D Texture interface
    // ======================================================================

    /// Evaluate the texture at the given surface interaction, with color processing.
    virtual Spectrum eval(const Interaction3f &si, Mask active = true) const = 0;

    /// Evaluate this texture as a three-channel quantity with no color processing (e.g. normal map).
    virtual Vector3f eval3(const Interaction3f &si, Mask active = true) const = 0;

    /// Evaluate this texture as a single-channel quantity.
    virtual Float eval1(const Interaction3f &si, Mask active = true) const = 0;

    /**
     * Evaluate the texture at the given surface interaction,
     * and compute the gradients of the linear interpolant as well.
     */
    virtual std::pair<Spectrum, Vector3f> eval_gradient(const Interaction3f & /*it*/,
                                                        Mask /*active*/ = true) const {
        NotImplementedError("eval_gradient");
    }


    // ======================================================================
    //! @{ \name Compatibility with 2D texture interface
    // ======================================================================

    Spectrum eval(const SurfaceInteraction3f &si, Mask active = true) const override {
        return eval(static_cast<Interaction3f>(si), active);
    }

    Vector3f eval3(const SurfaceInteraction3f &si, Mask active = true) const override {
        return eval3(static_cast<Interaction3f>(si), active);
    }

    Float eval1(const SurfaceInteraction3f &si, Mask active = true) const override {
        return eval1(static_cast<Interaction3f>(si), active);
    }

    //! @}
    // ======================================================================

    /**
     * \brief Returns the (possibly approximate) mean value of the texture over all
     * dimensions.
     *
     * Not guaranteed to be implemented. The default implementation throws an
     * exception.
     */
    Float mean() const override { NotImplementedError("mean"); }

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
