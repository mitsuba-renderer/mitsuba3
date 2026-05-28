#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/field.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Base class of fields that provide the surface texture role
 *
 * This class implements a generic texture map that supports evaluation at
 * arbitrary surface positions and wavelengths (if compiled in spectral mode).
 * It can be used to provide both intensities (e.g. for light sources) and
 * unitless reflectance parameters (e.g. an albedo of a reflectance model).
 *
 * The spectrum can be evaluated at arbitrary (continuous) wavelengths, though
 * the underlying function is not required to be smooth or even continuous.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB SurfaceField : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES()

    using FloatStorage = typename Field<Float, Spectrum>::FloatStorage;
    using Args         = typename Field<Float, Spectrum>::Args;
    using Array2f      = typename Field<Float, Spectrum>::Array2f;
    using Array3f      = typename Field<Float, Spectrum>::Array3f;
    using Array6f      = typename Field<Float, Spectrum>::Array6f;
    using FieldType    = typename Field<Float, Spectrum>::FieldType;

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
    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active = true) const override;

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
    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si,
                    const Wavelength &sample,
                    Mask active = true) const override;

    /**
     * \brief Evaluate the density function of the \ref sample_spectrum()
     * method as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily overrides this function. The default
     * implementation throws an exception.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     A density value for each wavelength in <tt>si.wavelengths</tt>
     *     (hence the \ref Wavelength type).
     */
    Wavelength pdf_spectrum(const SurfaceInteraction3f &si,
                            Mask active = true) const override;

    /**
     * \brief Importance sample a surface position proportional to the
     * overall spectral reflectance or intensity of the texture
     *
     * This function assumes that the texture is implemented as a mapping from
     * 2D UV positions to texture values, which is not necessarily true for all
     * textures (e.g. 3D noise functions, mesh attributes, etc.). For this
     * reason, not every plugin will provide a specialized implementation, and
     * the default implementation simply returns the input sample (i.e. uniform
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
    std::pair<Point2f, Float> sample_position(const Point2f &sample,
                                              Mask active = true) const override;

    /// Returns the probability per unit area of \ref sample_position().
    Float pdf_position(const Point2f &p, Mask active = true) const override;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Specialized evaluation routines
    // =============================================================

    /**
     * \brief Monochromatic evaluation of the texture at the given surface
     * interaction
     *
     * This function differs from \ref eval() in that it provides raw access to
     * scalar intensity/reflectance values without any color processing (e.g.
     * spectral upsampling). This is useful in parts of the renderer that
     * encode scalar quantities using textures, e.g. a height field.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     A scalar intensity or reflectance value
     */
    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active = true) const override;

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
    Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                         Mask active = true) const override;

    /**
     * \brief Trichromatic evaluation of the texture at the given surface
     * interaction
     *
     * This function differs from \ref eval() in that it provides raw access to
     * RGB intensity/reflectance values without any additional color processing
     * (e.g. RGB-to-spectral upsampling). This is useful in parts of the
     * renderer that encode 3D quantities using textures, e.g. a normal map.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \return
     *     A trichromatic intensity or reflectance value
     */
    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active = true) const override;

    /**
     * Return the mean value of the spectrum over the support
     * (MI_WAVELENGTH_MIN..MI_WAVELENGTH_MAX)
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    Float mean() const override;

    /**
     * \brief Returns the resolution of the texture, assuming that it is based
     * on a discrete representation.
     *
     * The default implementation returns <tt>(1, 1)</tt>
     */
    virtual ScalarVector2i resolution() const;

    ScalarVector2i resolution_2d() const override;

    /**
     * \brief Returns the resolution of the spectrum in nanometers (if discretized)
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    ScalarFloat spectral_resolution() const override;

    /**
     * \brief Returns the range of wavelengths covered by the spectrum
     *
     * The default implementation returns <tt>(MI_CIE_MIN, MI_CIE_MAX)</tt>
     */
    ScalarVector2f wavelength_range() const override;

    /**
     * Return the maximum value of the spectrum
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    ScalarFloat max() const override;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Field compatibility interface
    // =============================================================

    FieldValueType out_type() const override;
    FieldDomain domain() const override;
    uint32_t out_dim() const override;
    uint32_t args_dim() const override;
    bool supports_scalar() const override;
    bool supports_jit() const override;
    bool supports_surface_queries() const override;
    bool supports_interaction_queries() const override;

    FloatStorage eval(const SurfaceInteraction3f &si,
                      Args args,
                      Mask active) const override;
    FloatStorage eval(const Interaction3f &it,
                      Args args,
                      Mask active) const override;
    Float eval_1(const SurfaceInteraction3f &si,
                 Args args,
                 Mask active) const override;
    Float eval_1(const Interaction3f &it,
                 Args args,
                 Mask active) const override;
    Color3f eval_color3(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Color3f eval_color3(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    Array2f eval_array2(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Array2f eval_array2(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    Array3f eval_array3(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Array3f eval_array3(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si,
                                  Args args,
                                  Mask active) const override;
    UnpolarizedSpectrum eval_spec(const Interaction3f &it,
                                  Args args,
                                  Mask active) const override;
    Array6f eval_array6(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Array6f eval_array6(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    void eval_n(const SurfaceInteraction3f &si,
                Float *out,
                uint32_t count,
                Args args,
                Mask active) const override;
    void eval_n(const Interaction3f &it,
                Float *out,
                uint32_t count,
                Args args,
                Mask active) const override;

    //! @}
    // ======================================================================

    /// Does this texture evaluation depend on the UV coordinates.
    bool is_spatially_varying() const override { return false; }

    MI_DECLARE_CLASS(SurfaceField)
    static constexpr const char *Variant =
        detail::variant<Float, Spectrum>::name;
    static constexpr const char *Domain = "Field";
    static constexpr ObjectType Type = ObjectType::Field;
    std::string_view variant_name() const override { return Variant; }
    ObjectType type() const override { return Type; }

protected:
    SurfaceField(const Properties &);

    MI_TRAVERSE_CB(Object)
};

MI_EXTERN_CLASS(SurfaceField)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enables vectorized method calls on Dr.Jit arrays of surface fields
// -----------------------------------------------------------------------

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::SurfaceField)
    DRJIT_CALL_METHOD(eval)
    DRJIT_CALL_METHOD(sample_spectrum)
    DRJIT_CALL_METHOD(pdf_spectrum)
    DRJIT_CALL_METHOD(sample_position)
    DRJIT_CALL_METHOD(pdf_position)
    DRJIT_CALL_METHOD(eval_1)
    DRJIT_CALL_METHOD(eval_1_grad)
    DRJIT_CALL_METHOD(eval_3)
    DRJIT_CALL_METHOD(mean)

    DRJIT_CALL_GETTER(max)
    DRJIT_CALL_GETTER(is_spatially_varying)
DRJIT_CALL_END()
