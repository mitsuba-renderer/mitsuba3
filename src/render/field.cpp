#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

NAMESPACE_BEGIN(mitsuba)

static const char *field_value_type_name(FieldValueType type) {
    switch (type) {
        case FieldValueType::Float: return "Float";
        case FieldValueType::Spectrum: return "Spectrum";
        case FieldValueType::Color3: return "Color3";
        case FieldValueType::Array2: return "Array2";
        case FieldValueType::Array3: return "Array3";
        case FieldValueType::Features: return "Features";
        default: return "Unknown";
    }
}

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
    return false;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_jit() const {
    return false;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_surface_queries() const {
    return false;
}

MI_VARIANT bool Field<Float, Spectrum>::supports_interaction_queries() const {
    return false;
}

MI_VARIANT typename Field<Float, Spectrum>::FloatStorage
Field<Float, Spectrum>::eval(const SurfaceInteraction3f &, Args, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT typename Field<Float, Spectrum>::FloatStorage
Field<Float, Spectrum>::eval(const Interaction3f &, Args, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval(const SurfaceInteraction3f &si, Mask active) const {
    FieldValueType type = out_type();

    switch (type) {
        case FieldValueType::Float:
            return eval_1(si, active);

        case FieldValueType::Spectrum:
            return eval_spec(si, active);

        case FieldValueType::Color3: {
            Color3f value = eval_color3(si, active);
            if constexpr (is_monochromatic_v<Spectrum>)
                return luminance(value);
            else if constexpr (is_spectral_v<Spectrum>)
                Throw("Field::eval(): cannot convert Color3 field output to a "
                      "spectral value.");
            else
                return value;
        }

        case FieldValueType::Array3: {
            Array3f value = eval_array3(si, Args{}, active);
            Color3f color(value.x(), value.y(), value.z());
            if constexpr (is_monochromatic_v<Spectrum>)
                return luminance(color);
            else if constexpr (is_spectral_v<Spectrum>)
                Throw("Field::eval(): cannot convert Array3 field output to a "
                      "spectral value.");
            else
                return color;
        }

        default: {
            uint32_t dim = out_dim();
            Throw("Field::eval(): unsupported field output %s[%u].",
                  field_value_type_name(type), dim);
        }
    }
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval(const Interaction3f &it, Mask active) const {
    FieldValueType type = out_type();

    switch (type) {
        case FieldValueType::Float:
            return eval_1(it, active);

        case FieldValueType::Spectrum:
            return eval_spec(it, active);

        case FieldValueType::Color3: {
            Color3f value = eval_color3(it, active);
            if constexpr (is_monochromatic_v<Spectrum>)
                return luminance(value);
            else if constexpr (is_spectral_v<Spectrum>)
                Throw("Field::eval(): cannot convert Color3 field output to a "
                      "spectral value.");
            else
                return value;
        }

        case FieldValueType::Array3: {
            Array3f value = eval_array3(it, Args{}, active);
            Color3f color(value.x(), value.y(), value.z());
            if constexpr (is_monochromatic_v<Spectrum>)
                return luminance(color);
            else if constexpr (is_spectral_v<Spectrum>)
                Throw("Field::eval(): cannot convert Array3 field output to a "
                      "spectral value.");
            else
                return color;
        }

        default: {
            uint32_t dim = out_dim();
            Throw("Field::eval(): unsupported field output %s[%u].",
                  field_value_type_name(type), dim);
        }
    }
}

MI_VARIANT Float
Field<Float, Spectrum>::eval_1(const SurfaceInteraction3f &si,
                               Args args,
                               Mask active) const {
    if (dr::none_or<false>(active))
        return 0.f;
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Float || dim != 1)
        Throw("Field::eval_1(): expected Float[1], got %s[%u].",
              field_value_type_name(type), dim);
    Float result;
    eval_n(si, &result, 1, args, active);
    return result;
}

MI_VARIANT Float
Field<Float, Spectrum>::eval_1(const SurfaceInteraction3f &si,
                               Mask active) const {
    return eval_1(si, Args{}, active);
}

MI_VARIANT Float
Field<Float, Spectrum>::eval_1(const Interaction3f &it,
                               Args args,
                               Mask active) const {
    if (dr::none_or<false>(active))
        return 0.f;
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Float || dim != 1)
        Throw("Field::eval_1(): expected Float[1], got %s[%u].",
              field_value_type_name(type), dim);
    Float result;
    eval_n(it, &result, 1, args, active);
    return result;
}

MI_VARIANT Float
Field<Float, Spectrum>::eval_1(const Interaction3f &it,
                               Mask active) const {
    return eval_1(it, Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_color3(const SurfaceInteraction3f &si,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Color3f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Color3 || dim != 3)
        Throw("Field::eval_color3(): expected Color3[3], got %s[%u].",
              field_value_type_name(type), dim);
    Color3f result;
    eval_n(si, result.data(), 3, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_color3(const SurfaceInteraction3f &si,
                                    Mask active) const {
    return eval_color3(si, Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_color3(const Interaction3f &it,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Color3f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Color3 || dim != 3)
        Throw("Field::eval_color3(): expected Color3[3], got %s[%u].",
              field_value_type_name(type), dim);
    Color3f result;
    eval_n(it, result.data(), 3, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_color3(const Interaction3f &it,
                                    Mask active) const {
    return eval_color3(it, Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::Array2f
Field<Float, Spectrum>::eval_array2(const SurfaceInteraction3f &si,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array2f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Array2 || dim != 2)
        Throw("Field::eval_array2(): expected Array2[2], got %s[%u].",
              field_value_type_name(type), dim);
    Array2f result;
    eval_n(si, result.data(), 2, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::Array2f
Field<Float, Spectrum>::eval_array2(const Interaction3f &it,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array2f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Array2 || dim != 2)
        Throw("Field::eval_array2(): expected Array2[2], got %s[%u].",
              field_value_type_name(type), dim);
    Array2f result;
    eval_n(it, result.data(), 2, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::Array3f
Field<Float, Spectrum>::eval_array3(const SurfaceInteraction3f &si,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array3f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Array3 || dim != 3)
        Throw("Field::eval_array3(): expected Array3[3], got %s[%u].",
              field_value_type_name(type), dim);
    Array3f result;
    eval_n(si, result.data(), 3, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::Array3f
Field<Float, Spectrum>::eval_array3(const Interaction3f &it,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array3f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Array3 || dim != 3)
        Throw("Field::eval_array3(): expected Array3[3], got %s[%u].",
              field_value_type_name(type), dim);
    Array3f result;
    eval_n(it, result.data(), 3, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval_spec(const SurfaceInteraction3f &si,
                                  Args args,
                                  Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<UnpolarizedSpectrum>();
    uint32_t expected_dim = (uint32_t) dr::size_v<UnpolarizedSpectrum>;
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Spectrum || dim != expected_dim)
        Throw("Field::eval_spec(): expected Spectrum[%u], got %s[%u].",
              expected_dim, field_value_type_name(type), dim);
    UnpolarizedSpectrum result;
    eval_n(si, result.data(), expected_dim, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval_spec(const SurfaceInteraction3f &si,
                                  Mask active) const {
    return eval_spec(si, Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval_spec(const Interaction3f &it,
                                  Args args,
                                  Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<UnpolarizedSpectrum>();
    uint32_t expected_dim = (uint32_t) dr::size_v<UnpolarizedSpectrum>;
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Spectrum || dim != expected_dim)
        Throw("Field::eval_spec(): expected Spectrum[%u], got %s[%u].",
              expected_dim, field_value_type_name(type), dim);
    UnpolarizedSpectrum result;
    eval_n(it, result.data(), expected_dim, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::UnpolarizedSpectrum
Field<Float, Spectrum>::eval_spec(const Interaction3f &it,
                                  Mask active) const {
    return eval_spec(it, Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::Array6f
Field<Float, Spectrum>::eval_array6(const SurfaceInteraction3f &si,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array6f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Features || dim != 6)
        Throw("Field::eval_array6(): expected Features[6], got %s[%u].",
              field_value_type_name(type), dim);
    Array6f result;
    eval_n(si, result.data(), 6, args, active);
    return result;
}

MI_VARIANT typename Field<Float, Spectrum>::Array6f
Field<Float, Spectrum>::eval_array6(const Interaction3f &it,
                                    Args args,
                                    Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array6f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Features || dim != 6)
        Throw("Field::eval_array6(): expected Features[6], got %s[%u].",
              field_value_type_name(type), dim);
    Array6f result;
    eval_n(it, result.data(), 6, args, active);
    return result;
}

MI_VARIANT void
Field<Float, Spectrum>::eval_n(const SurfaceInteraction3f &si, Float *out,
                               uint32_t count, Args args, Mask active) const {
    if (dr::none_or<false>(active))
        return;
    uint32_t dim = out_dim();
    if (count != dim)
        Throw("Field::eval_n(): count (%u) must match out_dim (%u).",
              count, dim);
    FloatStorage result = eval(si, args, active);
    if (result.size() != count)
        Throw("Field::eval_n(): generic eval returned %zu channel(s), "
              "expected %u.", result.size(), count);
    for (uint32_t i = 0; i < count; ++i)
        out[i] = result.entry(i);
}

MI_VARIANT void
Field<Float, Spectrum>::eval_n(const Interaction3f &it, Float *out,
                               uint32_t count, Args args, Mask active) const {
    if (dr::none_or<false>(active))
        return;
    uint32_t dim = out_dim();
    if (count != dim)
        Throw("Field::eval_n(): count (%u) must match out_dim (%u).",
              count, dim);
    FloatStorage result = eval(it, args, active);
    if (result.size() != count)
        Throw("Field::eval_n(): generic eval returned %zu channel(s), "
              "expected %u.", result.size(), count);
    for (uint32_t i = 0; i < count; ++i)
        out[i] = result.entry(i);
}

MI_VARIANT void
Field<Float, Spectrum>::eval_n(const Interaction3f &it, Float *out,
                               Mask active) const {
    eval_n(it, out, out_dim(), Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::Color3f
Field<Float, Spectrum>::eval_3(const SurfaceInteraction3f &si,
                               Mask active) const {
    FieldValueType type = out_type();
    if (type == FieldValueType::Color3)
        return eval_color3(si, Args{}, active);
    if (type == FieldValueType::Array3) {
        Array3f value = eval_array3(si, Args{}, active);
        return Color3f(value.x(), value.y(), value.z());
    }
    uint32_t dim = out_dim();
    Throw("Field::eval_3(): expected Color3[3] or Array3[3], got %s[%u].",
          field_value_type_name(type), dim);
}

MI_VARIANT typename Field<Float, Spectrum>::Vector3f
Field<Float, Spectrum>::eval_3(const Interaction3f &it,
                               Mask active) const {
    FieldValueType type = out_type();
    if (type == FieldValueType::Array3) {
        Array3f value = eval_array3(it, Args{}, active);
        return Vector3f(value.x(), value.y(), value.z());
    }
    if (type == FieldValueType::Color3) {
        Color3f value = eval_color3(it, Args{}, active);
        return Vector3f(value.x(), value.y(), value.z());
    }
    uint32_t dim = out_dim();
    Throw("Field::eval_3(): expected Array3[3] or Color3[3], got %s[%u].",
          field_value_type_name(type), dim);
}

MI_VARIANT typename Field<Float, Spectrum>::Array6f
Field<Float, Spectrum>::eval_6(const Interaction3f &it,
                               Mask active) const {
    return eval_array6(it, Args{}, active);
}

MI_VARIANT typename Field<Float, Spectrum>::Vector2f
Field<Float, Spectrum>::eval_1_grad(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_1_grad");
}

MI_VARIANT std::pair<typename Field<Float, Spectrum>::Wavelength,
                     typename Field<Float, Spectrum>::UnpolarizedSpectrum>
Field<Float, Spectrum>::sample_spectrum(const SurfaceInteraction3f &,
                                        const Wavelength &, Mask) const {
    NotImplementedError("sample_spectrum");
}

MI_VARIANT typename Field<Float, Spectrum>::Wavelength
Field<Float, Spectrum>::pdf_spectrum(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("pdf_spectrum");
}

MI_VARIANT std::pair<typename Field<Float, Spectrum>::Point2f, Float>
Field<Float, Spectrum>::sample_position(const Point2f &sample,
                                        Mask active) const {
    return { sample, dr::select(active, Float(1.f), Float(0.f)) };
}

MI_VARIANT Float
Field<Float, Spectrum>::pdf_position(const Point2f &, Mask active) const {
    return dr::select(active, Float(1.f), Float(0.f));
}

MI_VARIANT Float Field<Float, Spectrum>::mean() const {
    return Float(.5f);
}

MI_VARIANT typename Field<Float, Spectrum>::ScalarFloat
Field<Float, Spectrum>::max() const {
    NotImplementedError("max");
}

MI_VARIANT typename Field<Float, Spectrum>::ScalarVector2i
Field<Float, Spectrum>::resolution_2d() const {
    return ScalarVector2i(1, 1);
}

MI_VARIANT typename Field<Float, Spectrum>::ScalarFloat
Field<Float, Spectrum>::spectral_resolution() const {
    NotImplementedError("spectral_resolution");
}

MI_VARIANT typename Field<Float, Spectrum>::ScalarVector2f
Field<Float, Spectrum>::wavelength_range() const {
    return ScalarVector2f(MI_CIE_MIN, MI_CIE_MAX);
}

MI_VARIANT bool Field<Float, Spectrum>::is_spatially_varying() const {
    return supports_surface_queries();
}

MI_VARIANT std::pair<typename Field<Float, Spectrum>::UnpolarizedSpectrum,
                     typename Field<Float, Spectrum>::Vector3f>
Field<Float, Spectrum>::eval_gradient(const Interaction3f &, Mask) const {
    NotImplementedError("eval_gradient");
}

MI_VARIANT void
Field<Float, Spectrum>::max_per_channel(ScalarFloat *) const {
    NotImplementedError("max_per_channel");
}

MI_VARIANT typename Field<Float, Spectrum>::ScalarBoundingBox3f
Field<Float, Spectrum>::bbox() const {
    NotImplementedError("bbox");
}

MI_VARIANT typename Field<Float, Spectrum>::ScalarVector3i
Field<Float, Spectrum>::resolution_3d() const {
    return ScalarVector3i(1, 1, 1);
}

MI_VARIANT uint32_t Field<Float, Spectrum>::channel_count() const {
    return 0;
}

MI_VARIANT ref<Field<Float, Spectrum>>
Field<Float, Spectrum>::D65(ScalarFloat scale) {
    Properties props("d65");
    props.set("scale", scale);
    ref<Object> object = create_texture_role_object_for_variant(
        props, Variant);
    std::vector<ref<Object>> children = object->expand();
    if (!children.empty())
        object = children[0];

    Field *field = dynamic_cast<Field *>(object.get());
    if (!field)
        Throw("Field::D65(): expected a field expansion.");
    return ref<Field>(field);
}

MI_VARIANT ref<Field<Float, Spectrum>>
Field<Float, Spectrum>::D65(ref<Field> field) {
    if constexpr (!is_spectral_v<Spectrum>) {
        return field;
    } else {
        Properties props("d65");
        props.set("nested", ref<Object>(field.get()));
        ref<Object> object = create_texture_role_object_for_variant(
            props, Variant);
        std::vector<ref<Object>> children = object->expand();
        if (!children.empty())
            object = children[0];

        Field *result = dynamic_cast<Field *>(object.get());
        if (!result)
            Throw("Field::D65(): expected a field expansion.");
        return ref<Field>(result);
    }
}

MI_INSTANTIATE_CLASS(Field)
NAMESPACE_END(mitsuba)
