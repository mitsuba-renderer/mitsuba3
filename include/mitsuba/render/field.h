#pragma once

#include <mitsuba/core/profiler.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <drjit/call.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Type of float channel tuple returned by a \ref Field
 */
enum class FieldValueType : uint32_t {
    /// A single scalar channel
    Float,

    /// An unpolarized spectral value
    Spectrum,

    /// A three-channel color value
    Color3,

    /// A two-channel float array
    Array2,

    /// A three-channel float array
    Array3,

    /// A variable-length float array
    Features
};

/**
 * \brief Interaction record types accepted by a \ref Field
 */
enum class FieldDomain : uint32_t {
    /// Surface interactions only
    Surface,

    /// Generic 3D interactions only
    Interaction,

    /// Both surface and generic 3D interactions
    SurfaceAndInteraction
};

extern MI_EXPORT_LIB const char *field_value_type_name(FieldValueType type);

/**
 * \brief View of optional argument channels passed to a \ref Field query
 */
template <typename Float> struct FieldArgs {
    /// Pointer to the first argument channel, or \c nullptr when empty
    const Float *data = nullptr;

    /// Number of argument channels
    uint32_t size = 0;

    FieldArgs() = default;
    FieldArgs(const Float *data, uint32_t size) : data(data), size(size) { }
};

/**
 * \brief Base class of all field implementations
 *
 * A field evaluates float-valued data at renderer interaction records. The
 * input can be a \ref SurfaceInteraction3f, a generic \ref Interaction3f, and
 * an optional array of user-provided float arguments.
 *
 * The generic \ref eval() overloads return a dynamic array of output channels.
 * Fixed-size routines are provided for callers that know the desired semantic
 * output type and want to avoid dynamic storage in performance-sensitive code.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Field : public JitObject<Field<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES()

    using FloatStorage = dr::DynamicArray<Float>;
    using Array2f      = dr::Array<Float, 2>;
    using Array3f      = dr::Array<Float, 3>;
    using Array6f      = dr::Array<Float, 6>;
    using Args         = FieldArgs<Float>;
    using FieldType    = Field<Float, Spectrum>;

    // =============================================================
    //! @{ \name Field metadata
    // =============================================================

    /// Return the semantic output type of this field
    virtual FieldValueType out_type() const;

    /// Return the interaction record type accepted by this field
    virtual FieldDomain domain() const;

    /// Return the number of output channels
    virtual uint32_t out_dim() const;

    /// Return the number of optional argument channels
    virtual uint32_t args_dim() const;

    /// Return whether this field supports scalar Mitsuba variants
    virtual bool supports_scalar() const;

    /// Return whether this field supports JIT Mitsuba variants
    virtual bool supports_jit() const;

    /// Return whether this field supports \ref SurfaceInteraction3f queries
    virtual bool supports_surface_queries() const;

    /// Return whether this field supports \ref Interaction3f queries
    virtual bool supports_interaction_queries() const;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Generic evaluation interface
    // =============================================================

    /**
     * \brief Evaluate the field at a surface interaction
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \param args
     *     Optional field-specific argument channels
     *
     * \return
     *     A dynamic array containing \ref out_dim() float channels
     */
    virtual FloatStorage eval(const SurfaceInteraction3f &si,
                              Args args,
                              Mask active = true) const;

    /**
     * \brief Evaluate the field at a generic 3D interaction
     *
     * \param it
     *     An interaction record describing the associated 3D position
     *
     * \param args
     *     Optional field-specific argument channels
     *
     * \return
     *     A dynamic array containing \ref out_dim() float channels
     */
    virtual FloatStorage eval(const Interaction3f &it,
                              Args args,
                              Mask active = true) const;

    /// Evaluate an argument-free surface field as an unpolarized spectrum.
    virtual UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                                     Mask active = true) const;

    /// Evaluate an argument-free interaction field as an unpolarized spectrum.
    virtual UnpolarizedSpectrum eval(const Interaction3f &it,
                                     Mask active = true) const;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Specialized evaluation routines
    // =============================================================

    /// Evaluate the field as a single-channel quantity at a surface interaction.
    virtual Float eval_1(const SurfaceInteraction3f &si,
                         Args args = {},
                         Mask active = true) const;

    /// Evaluate an argument-free single-channel field at a surface interaction.
    virtual Float eval_1(const SurfaceInteraction3f &si,
                         Mask active) const;

    /// Evaluate the field as a single-channel quantity at a generic 3D interaction.
    virtual Float eval_1(const Interaction3f &it,
                         Args args = {},
                         Mask active = true) const;

    /// Evaluate an argument-free single-channel field at a generic 3D interaction.
    virtual Float eval_1(const Interaction3f &it,
                         Mask active) const;

    /// Evaluate the field as a three-channel color value at a surface interaction.
    virtual Color3f eval_color3(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate an argument-free color field at a surface interaction.
    virtual Color3f eval_color3(const SurfaceInteraction3f &si,
                                Mask active) const;

    /// Evaluate the field as a three-channel color value at a generic 3D interaction.
    virtual Color3f eval_color3(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate an argument-free color field at a generic 3D interaction.
    virtual Color3f eval_color3(const Interaction3f &it,
                                Mask active) const;

    /// Evaluate the field as a two-channel array at a surface interaction.
    virtual Array2f eval_array2(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate the field as a two-channel array at a generic 3D interaction.
    virtual Array2f eval_array2(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate the field as a three-channel array at a surface interaction.
    virtual Array3f eval_array3(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate the field as a three-channel array at a generic 3D interaction.
    virtual Array3f eval_array3(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate the field as an unpolarized spectrum at a surface interaction.
    virtual UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si,
                                          Args args = {},
                                          Mask active = true) const;

    /// Evaluate an argument-free spectrum field at a surface interaction.
    virtual UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si,
                                          Mask active) const;

    /// Evaluate the field as an unpolarized spectrum at a generic 3D interaction.
    virtual UnpolarizedSpectrum eval_spec(const Interaction3f &it,
                                          Args args = {},
                                          Mask active = true) const;

    /// Evaluate an argument-free spectrum field at a generic 3D interaction.
    virtual UnpolarizedSpectrum eval_spec(const Interaction3f &it,
                                          Mask active) const;

    /// Evaluate the field as a six-channel array at a surface interaction.
    virtual Array6f eval_array6(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Evaluate the field as a six-channel array at a generic 3D interaction.
    virtual Array6f eval_array6(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /**
     * \brief Evaluate the field as an n-channel float array at a surface
     * interaction
     *
     * Pointer allocation and deallocation must be performed by the caller.
     */
    virtual void eval_n(const SurfaceInteraction3f &si,
                        Float *out,
                        uint32_t count,
                        Args args = {},
                        Mask active = true) const;

    /**
     * \brief Evaluate the field as an n-channel float array at a generic 3D
     * interaction
     *
     * Pointer allocation and deallocation must be performed by the caller.
     */
    virtual void eval_n(const Interaction3f &it,
                        Float *out,
                        uint32_t count,
                        Args args = {},
                        Mask active = true) const;

    /// Evaluate all channels at a generic 3D interaction.
    virtual void eval_n(const Interaction3f &it,
                        Float *out,
                        Mask active = true) const;

    /// Texture-style trichromatic evaluation at a surface interaction.
    virtual Color3f eval_3(const SurfaceInteraction3f &si,
                           Mask active = true) const;

    /// Volume-style three-channel evaluation at a generic 3D interaction.
    virtual Vector3f eval_3(const Interaction3f &it,
                            Mask active = true) const;

    /// Volume-style six-channel evaluation at a generic 3D interaction.
    virtual Array6f eval_6(const Interaction3f &it,
                           Mask active = true) const;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Surface role interface
    // =============================================================

    /// Evaluate the UV-space gradient of a scalar surface field.
    virtual Vector2f eval_1_grad(const SurfaceInteraction3f &si,
                                 Mask active = true) const;

    /// Importance sample wavelengths proportional to the surface spectrum.
    virtual std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si,
                    const Wavelength &sample,
                    Mask active = true) const;

    /// Return the wavelength PDF associated with \ref sample_spectrum().
    virtual Wavelength pdf_spectrum(const SurfaceInteraction3f &si,
                                    Mask active = true) const;

    /// Importance sample a UV-space position.
    virtual std::pair<Point2f, Float> sample_position(const Point2f &sample,
                                                      Mask active = true) const;

    /// Return the UV-space position PDF associated with \ref sample_position().
    virtual Float pdf_position(const Point2f &p,
                               Mask active = true) const;

    /// Return the mean value of a surface field, if available.
    virtual Float mean() const;

    /// Return the maximum value of a field, if available.
    virtual ScalarFloat max() const;

    /// Return the 2D resolution of a discrete surface field.
    virtual ScalarVector2i resolution_2d() const;

    /// Return the wavelength spacing of a discretized spectrum.
    virtual ScalarFloat spectral_resolution() const;

    /// Return the wavelength range covered by a spectrum.
    virtual ScalarVector2f wavelength_range() const;

    /// Return whether evaluation varies over surface position.
    virtual bool is_spatially_varying() const;

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Volume role interface
    // =============================================================

    /// Evaluate a volume field and its spatial gradient, if available.
    virtual std::pair<UnpolarizedSpectrum, Vector3f>
    eval_gradient(const Interaction3f &it,
                  Mask active = true) const;

    /// Return per-channel maxima for a multi-channel volume field.
    virtual void max_per_channel(ScalarFloat *out) const;

    /// Return the world-space bounding box of a volume field.
    virtual ScalarBoundingBox3f bbox() const;

    /// Return the 3D resolution of a discrete volume field.
    virtual ScalarVector3i resolution_3d() const;

    /// Return the number of stored volume channels, if available.
    virtual uint32_t channel_count() const;

    //! @}
    // ======================================================================

    /// Convenience function returning the standard D65 illuminant field.
    static ref<Field> D65(ScalarFloat scale = 1.f);

    /// Convenience function to create a product field with the D65 illuminant.
    static ref<Field> D65(ref<Field> field);

    MI_DECLARE_PLUGIN_BASE_CLASS(Field)

protected:
    Field(const Properties &props);

    MI_TRAVERSE_CB(Object)
};

/// Validate optional role methods that are required by specific consumers.
template <typename FieldT>
MI_INLINE void require_field_eval_1_grad(const FieldT *field,
                                         std::string_view param_name) {
    try {
        typename FieldT::SurfaceInteraction3f si =
            dr::zeros<typename FieldT::SurfaceInteraction3f>();
        (void) field->eval_1_grad(si);
    } catch (const std::exception &e) {
        Throw("Field parameter \"%s\": requires eval_1_grad(): %s",
              param_name, e.what());
    }
}

template <typename FieldT>
MI_INLINE void require_field_mean(const FieldT *field,
                                  std::string_view param_name) {
    try {
        (void) field->mean();
    } catch (const std::exception &e) {
        Throw("Field parameter \"%s\": requires mean(): %s",
              param_name, e.what());
    }
}

template <typename FieldT>
MI_INLINE void require_field_max(const FieldT *field,
                                 std::string_view param_name) {
    try {
        (void) field->max();
    } catch (const std::exception &e) {
        Throw("Field parameter \"%s\": requires max(): %s",
              param_name, e.what());
    }
}

template <typename FieldT>
MI_INLINE void require_field_sample_spectrum(const FieldT *field,
                                             std::string_view param_name) {
    try {
        typename FieldT::SurfaceInteraction3f si =
            dr::zeros<typename FieldT::SurfaceInteraction3f>();
        typename FieldT::Wavelength sample =
            dr::full<typename FieldT::Wavelength>(.5f);
        (void) field->sample_spectrum(si, sample);
    } catch (const std::exception &e) {
        Throw("Field parameter \"%s\": requires sample_spectrum(): %s",
              param_name, e.what());
    }
}

template <typename FieldT>
MI_INLINE void require_field_spectral_metadata(const FieldT *field,
                                               std::string_view param_name) {
    try {
        (void) field->wavelength_range();
        (void) field->spectral_resolution();
    } catch (const std::exception &e) {
        Throw("Field parameter \"%s\": requires spectral metadata: %s",
              param_name, e.what());
    }
}

template <typename FieldT>
MI_INLINE void require_field_spectral_evaluable(const FieldT *field,
                                                std::string_view param_name) {
    FieldValueType type = field->out_type();
    if (type == FieldValueType::Color3 || type == FieldValueType::Array3)
        Throw("Field parameter \"%s\": cannot use %s[%u] output as a "
              "spectral value.",
              param_name, field_value_type_name(type), field->out_dim());
}

MI_EXTERN_CLASS(Field)
NAMESPACE_END(mitsuba)

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Field)
    DRJIT_CALL_GETTER(out_dim)
    DRJIT_CALL_GETTER(args_dim)
    DRJIT_CALL_METHOD(eval)
    DRJIT_CALL_METHOD(eval_1)
    DRJIT_CALL_METHOD(eval_color3)
    DRJIT_CALL_METHOD(eval_array2)
    DRJIT_CALL_METHOD(eval_array3)
    DRJIT_CALL_METHOD(eval_spec)
    DRJIT_CALL_METHOD(eval_array6)
    DRJIT_CALL_METHOD(eval_3)
    DRJIT_CALL_METHOD(eval_6)
    DRJIT_CALL_METHOD(eval_1_grad)
    DRJIT_CALL_METHOD(sample_spectrum)
    DRJIT_CALL_METHOD(pdf_spectrum)
    DRJIT_CALL_METHOD(sample_position)
    DRJIT_CALL_METHOD(pdf_position)
    DRJIT_CALL_METHOD(mean)
    DRJIT_CALL_METHOD(max)
    DRJIT_CALL_METHOD(is_spatially_varying)
DRJIT_CALL_END()
