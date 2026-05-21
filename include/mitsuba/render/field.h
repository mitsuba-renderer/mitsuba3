#pragma once

#include <mitsuba/core/profiler.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <drjit/call.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Semantic type of float channels produced by a Field.
 */
enum class FieldValueType : uint32_t {
    Float,
    Spectrum,
    Color3,
    Array2,
    Array3,
    Features
};

/**
 * \brief Structured interaction domains supported by a Field.
 */
enum class FieldDomain : uint32_t {
    Surface,
    Interaction,
    SurfaceAndInteraction
};

/**
 * \brief Lightweight view of optional float features supplied to Field queries.
 */
template <typename Float> struct FieldArgs {
    const Float *data = nullptr;
    uint32_t size = 0;

    FieldArgs() = default;
    FieldArgs(const Float *data, uint32_t size) : data(data), size(size) { }
};

/**
 * \brief Base class of render-layer storage/query field implementations.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Field : public JitObject<Field<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES()

    using FloatStorage = DynamicBuffer<Float>;
    using Array2f      = dr::Array<Float, 2>;
    using Array3f      = dr::Array<Float, 3>;
    using Array6f      = dr::Array<Float, 6>;
    using Args         = FieldArgs<Float>;

    /// Return the semantic output type of this field.
    virtual FieldValueType out_type() const;

    /// Return the structured query domain supported by this field.
    virtual FieldDomain domain() const;

    /// Return the number of output channels.
    virtual uint32_t out_dim() const;

    /// Return the number of optional argument channels.
    virtual uint32_t args_dim() const;

    /// Return whether this field supports scalar Mitsuba variants.
    virtual bool supports_scalar() const;

    /// Return whether this field supports JIT Mitsuba variants.
    virtual bool supports_jit() const;

    /// Return whether this field supports SurfaceInteraction queries.
    virtual bool supports_surface_queries() const;

    /// Return whether this field supports Interaction queries.
    virtual bool supports_interaction_queries() const;

    /// Generic dynamic-output evaluation at a surface interaction.
    virtual FloatStorage eval(const SurfaceInteraction3f &si,
                              Args args = {},
                              Mask active = true) const;

    /// Generic dynamic-output evaluation at an interaction.
    virtual FloatStorage eval(const Interaction3f &it,
                              Args args = {},
                              Mask active = true) const;

    /// Fixed one-channel evaluation at a surface interaction.
    virtual Float eval_1(const SurfaceInteraction3f &si,
                         Args args = {},
                         Mask active = true) const;

    /// Fixed one-channel evaluation at an interaction.
    virtual Float eval_1(const Interaction3f &it,
                         Args args = {},
                         Mask active = true) const;

    /// Fixed color evaluation at a surface interaction.
    virtual Color3f eval_color3(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed color evaluation at an interaction.
    virtual Color3f eval_color3(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed two-channel evaluation at a surface interaction.
    virtual Array2f eval_array2(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed two-channel evaluation at an interaction.
    virtual Array2f eval_array2(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed three-channel evaluation at a surface interaction.
    virtual Array3f eval_array3(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed three-channel evaluation at an interaction.
    virtual Array3f eval_array3(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed spectral evaluation at a surface interaction.
    virtual UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si,
                                          Args args = {},
                                          Mask active = true) const;

    /// Fixed spectral evaluation at an interaction.
    virtual UnpolarizedSpectrum eval_spec(const Interaction3f &it,
                                          Args args = {},
                                          Mask active = true) const;

    /// Fixed six-channel evaluation at a surface interaction.
    virtual Array6f eval_array6(const SurfaceInteraction3f &si,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed six-channel evaluation at an interaction.
    virtual Array6f eval_array6(const Interaction3f &it,
                                Args args = {},
                                Mask active = true) const;

    /// Fixed n-channel evaluation at a surface interaction.
    virtual void eval_n(const SurfaceInteraction3f &si,
                        Float *out,
                        uint32_t count,
                        Args args = {},
                        Mask active = true) const;

    /// Fixed n-channel evaluation at an interaction.
    virtual void eval_n(const Interaction3f &it,
                        Float *out,
                        uint32_t count,
                        Args args = {},
                        Mask active = true) const;

    MI_DECLARE_PLUGIN_BASE_CLASS(Field)

protected:
    Field(const Properties &props);

    MI_TRAVERSE_CB(Object)
};

MI_EXTERN_CLASS(Field)
NAMESPACE_END(mitsuba)

DRJIT_CALL_TEMPLATE_BEGIN(mitsuba::Field)
    DRJIT_CALL_METHOD(eval)
    DRJIT_CALL_METHOD(eval_1)
    DRJIT_CALL_METHOD(eval_color3)
    DRJIT_CALL_METHOD(eval_array2)
    DRJIT_CALL_METHOD(eval_array3)
    DRJIT_CALL_METHOD(eval_spec)
    DRJIT_CALL_METHOD(eval_array6)
DRJIT_CALL_END()
