#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Base class of all surface texture implementations
 *
 * This class implements a generic texture map that supports evaluation at
 * arbitrary surface positions and wavelengths (if compiled in spectral mode).
 * It can be used to provide both intensities (e.g. for light sources) and
 * unitless reflectance parameters (e.g. an albedo of a reflectance model).
 *
 * The spectrum can be evaluated at arbitrary (continuous) wavelengths, though
 * the underlying function it is not required to be smooth or even continuous.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Texture : public Object {
public:
    MTS_IMPORT_TYPES()

    // =============================================================
    //! @{ \name Standard sampling interface
    // =============================================================

    /**
     * \brief Evaluate the texture at the given surface interaction
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     An unpolarized spectral power distribution or reflectance value
     */
    virtual UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                                     Mask active = true) const;

    /**
     * \brief Importance sample a set of wavelengths proportional to the
     * spectrum defined at the given surface position
     *
     * Not every implementation necessarily provides this function, and it is a
     * no-op when compiling non-spectral variants of Mitsuba. The default
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
    virtual std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si,
                    const Wavelength &sample,
                    Mask active = true) const;

    /**
     * \brief Evaluate the density function of the \ref sample_spectrum()
     * method as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     A density value for each wavelength in <tt>si.wavelengths</tt>
     *     (hence the \ref Wavelength type).
     */
    virtual Wavelength pdf_spectrum(const SurfaceInteraction3f &si,
                                    Mask active = true) const;

    /**
     * \brief Importance sample a surface position proportional to the
     * overall spectral reflectance or intensity of the texture
     *
     * This function assumes that the texture is implemented as a mapping from
     * 2D UV positions to texture values, which is not necessarily true for all
     * textures (e.g. 3D noise functions, mesh attributes, etc.). For this
     * reason, not every will plugin provide a specialized implementation, and
     * the default implementation simply return the input sample (i.e. uniform
     * sampling is used).
     *
     * \param sample
     *     A 2D vector of uniform variates
     *
     * \return
     *     1. A texture-space position in the range :math:`[0, 1]^2`
     *
     *     2. The associated probability per unit area in UV space
     */
    virtual std::pair<Point2f, Float> sample_position(const Point2f &sample,
                                                      Mask active = true) const;

    /// Returns the probability per unit area of \ref sample_position()
    virtual Float pdf_position(const Point2f &p, Mask active = true) const;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Specialized evaluation routines
    // =============================================================

    /**
     * \brief Monochromatic evaluation of the texture at the given surface
     * interaction
     *
     * This function differs from \ref eval() in that it provided raw access to
     * scalar intensity/reflectance values without any color processing (e.g.
     * spectral upsampling). This is useful in parts of the renderer that
     * encode scalar quantities using textures, e.g. a height field.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     An scalar intensity or reflectance value
     */
    virtual Float eval_1(const SurfaceInteraction3f &si,
                         Mask active = true) const;

    /**
     * \brief Monochromatic evaluation of the texture gradient at the given
     * surface interaction
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     A (u,v) pair of intensity or reflectance value gradients
     */
    virtual Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                                 Mask active = true) const;

    /**
     * \brief Trichromatic evaluation of the texture at the given surface
     * interaction
     *
     * This function differs from \ref eval() in that it provided raw access to
     * RGB intensity/reflectance values without any additional color processing
     * (e.g. RGB-to-spectral upsampling). This is useful in parts of the
     * renderer that encode 3D quantities using textures, e.g. a normal map.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     An trichromatic intensity or reflectance value
     */
    virtual Color3f eval_3(const SurfaceInteraction3f &si,
                           Mask active = true) const;

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
     * \brief Returns the resolution of the texture, assuming that it is based
     * on a discrete representation.
     *
     * The default implementation returns <tt>(1, 1)</tt>
     */
    virtual ScalarVector2i resolution() const;

    //! @}
    // ======================================================================

    /// Does this texture evaluation depend on the UV coordinates
    virtual bool is_spatially_varying() const { return false; }

    /// Convenience method returning the standard D65 illuminant.
    static ref<Texture> D65(ScalarFloat scale = 1.f);

    /// Return a string identifier
    std::string id() const override { return m_id; }

    MTS_DECLARE_CLASS()

protected:
    Texture(const Properties &);
    virtual ~Texture();

protected:
    std::string m_id;
};


/// Abstract base class for spatially-varying 3D textures.
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Volume : public Object {
public:
    MTS_IMPORT_TYPES(Texture)

    // ======================================================================
    //! @{ \name 3D Texture interface
    // ======================================================================

    /// Evaluate the texture at the given surface interaction, with color processing.
    virtual UnpolarizedSpectrum eval(const Interaction3f &si, Mask active = true) const;

    /// Evaluate this texture as a single-channel quantity.
    virtual Float eval_1(const Interaction3f &si, Mask active = true) const;

    /// Evaluate this texture as a three-channel quantity with no color processing (e.g. normal map).
    virtual Vector3f eval_3(const Interaction3f &si, Mask active = true) const;

    /**
     * Evaluate the texture at the given surface interaction,
     * and compute the gradients of the linear interpolant as well.
     */
    virtual std::pair<UnpolarizedSpectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                                   Mask active = true) const;

    /// Returns the maximum value of the texture over all dimensions.
    virtual ScalarFloat max() const;

    /// Returns the bounding box of the 3d texture
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
            << "  world_to_local = " << m_world_to_local << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    Volume(const Properties &props);
    virtual ~Volume() {}

    virtual Mask is_inside(const Interaction3f &it, Mask active = true) const = 0;

    void update_bbox() {
        ScalarPoint3f a(0.f, 0.f, 0.f);
        ScalarPoint3f b(1.f, 1.f, 1.f);
        a      = m_world_to_local.inverse() * a;
        b      = m_world_to_local.inverse() * b;
        m_bbox = ScalarBoundingBox3f(a);
        m_bbox.expand(b);
    }

protected:
    /// Used to bring points in world coordinates to local coordinates.
    ScalarTransform4f m_world_to_local;
    /// Bounding box
    ScalarBoundingBox3f m_bbox;
};

MTS_EXTERN_CLASS_RENDER(Texture)
MTS_EXTERN_CLASS_RENDER(Volume)
NAMESPACE_END(mitsuba)
