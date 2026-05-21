#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Field<Float, Spectrum>::Field(const Properties &props)
    : JitObject<Field>(props.id()) { }

MI_VARIANT FieldValueType Field<Float, Spectrum>::out_type() const {
    return FieldValueType::Features;
}

MI_VARIANT FieldDomain Field<Float, Spectrum>::domain() const {
    return FieldDomain::SurfaceAndInteraction;
}

MI_VARIANT uint32_t Field<Float, Spectrum>::out_dim() const {
    switch (out_type()) {
        case FieldValueType::Float: return 1;
        case FieldValueType::Spectrum:
            return (uint32_t) dr::size_v<UnpolarizedSpectrum>;
        case FieldValueType::Color3: return 3;
        case FieldValueType::Array2: return 2;
        case FieldValueType::Array3: return 3;
        case FieldValueType::Features: return 0;
        default: return 0;
    }
}

MI_VARIANT uint32_t Field<Float, Spectrum>::args_dim() const {
    return 0;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_scalar() const {
    return true;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_jit() const {
    return true;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_surface_queries() const {
    FieldDomain d = domain();
    return d == FieldDomain::Surface || d == FieldDomain::SurfaceAndInteraction;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_interaction_queries() const {
    FieldDomain d = domain();
    return d == FieldDomain::Interaction || d == FieldDomain::SurfaceAndInteraction;
}

MI_VARIANT typename Field<Float, Spectrum>::FloatStorage
Field<Float, Spectrum>::eval(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT typename Field<Float, Spectrum>::FloatStorage
Field<Float, Spectrum>::eval(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT Float
Field<Float, Spectrum>::eval_1(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval_1");
}

MI_VARIANT Float
Field<Float, Spectrum>::eval_1(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval_1");
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_color3(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval_color3");
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_color3(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval_color3");
}

MI_VARIANT typename Field<Float, Spectrum>::Array2f
Field<Float, Spectrum>::eval_array2(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval_array2");
}

MI_VARIANT typename Field<Float, Spectrum>::Array2f
Field<Float, Spectrum>::eval_array2(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval_array2");
}

MI_VARIANT typename Field<Float, Spectrum>::Array3f
Field<Float, Spectrum>::eval_array3(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval_array3");
}

MI_VARIANT typename Field<Float, Spectrum>::Array3f
Field<Float, Spectrum>::eval_array3(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval_array3");
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval_spec(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval_spec");
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval_spec(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval_spec");
}

MI_VARIANT typename Field<Float, Spectrum>::Array6f
Field<Float, Spectrum>::eval_array6(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval_array6");
}

MI_VARIANT typename Field<Float, Spectrum>::Array6f
Field<Float, Spectrum>::eval_array6(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval_array6");
}

MI_VARIANT void
Field<Float, Spectrum>::eval_n(const SurfaceInteraction3f &, Float *, uint32_t,
                               Args, Mask) const {
    NotImplementedError("eval_n");
}

MI_VARIANT void
Field<Float, Spectrum>::eval_n(const Interaction3f &, Float *, uint32_t,
                               Args, Mask) const {
    NotImplementedError("eval_n");
}

MI_INSTANTIATE_CLASS(Field)
NAMESPACE_END(mitsuba)
