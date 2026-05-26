#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! Texture base implementation
// =======================================================================

namespace {
    const char *texture_field_value_type_name(FieldValueType type) {
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
}

MI_VARIANT SurfaceField<Float, Spectrum>::SurfaceField(const Properties &props)
    : Base(props) { }

MI_VARIANT FieldValueType SurfaceField<Float, Spectrum>::out_type() const {
    return FieldValueType::Spectrum;
}

MI_VARIANT FieldDomain SurfaceField<Float, Spectrum>::domain() const {
    return FieldDomain::Surface;
}

MI_VARIANT uint32_t SurfaceField<Float, Spectrum>::out_dim() const {
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

MI_VARIANT uint32_t SurfaceField<Float, Spectrum>::args_dim() const {
    return 0;
}

MI_VARIANT bool SurfaceField<Float, Spectrum>::supports_scalar() const {
    return true;
}

MI_VARIANT bool SurfaceField<Float, Spectrum>::supports_jit() const {
    return true;
}

MI_VARIANT bool SurfaceField<Float, Spectrum>::supports_surface_queries() const {
    return true;
}

MI_VARIANT bool SurfaceField<Float, Spectrum>::supports_interaction_queries() const {
    return false;
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::FloatStorage
SurfaceField<Float, Spectrum>::eval(const SurfaceInteraction3f &si,
                               Args args,
                               Mask active) const {
    if (args.size != 0)
        Throw("Texture field query expected args_dim=0, got %u.", args.size);

    uint32_t dim = out_dim();
    if (dim == 0)
        Throw("Texture field query has unknown output dimension.");

    FloatStorage result = dr::empty<FloatStorage>(dim);
    eval_n(si, result.data(), dim, Args{}, active);
    return result;
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::FloatStorage
SurfaceField<Float, Spectrum>::eval(const Interaction3f &, Args, Mask) const {
    Throw("Texture field query domain mismatch: expected Surface interaction, "
          "got Interaction.");
}

MI_VARIANT Float
SurfaceField<Float, Spectrum>::eval_1(const SurfaceInteraction3f &si,
                                 Args args,
                                 Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_1(): expected args_dim=0, got %u.", args.size);
    if (dr::none_or<false>(active))
        return 0.f;
    if (out_type() != FieldValueType::Float || out_dim() != 1)
        Throw("Texture::eval_1(): expected Float[1], got %s[%u].",
              texture_field_value_type_name(out_type()), out_dim());
    return eval_1(si, active);
}

MI_VARIANT Float
SurfaceField<Float, Spectrum>::eval_1(const Interaction3f &, Args,
                                      Mask active) const {
    if (dr::none_or<false>(active))
        return 0.f;
    Throw("Texture::eval_1(): domain mismatch, expected Surface interaction, "
          "got Interaction.");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Color3f
SurfaceField<Float, Spectrum>::eval_color3(const SurfaceInteraction3f &si,
                                      Args args,
                                      Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_color3(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Color3f>();
    if (out_type() != FieldValueType::Color3 || out_dim() != 3)
        Throw("Texture::eval_color3(): expected Color3[3], got %s[%u].",
              texture_field_value_type_name(out_type()), out_dim());
    return eval_3(si, active);
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Color3f
SurfaceField<Float, Spectrum>::eval_color3(const Interaction3f &, Args,
                                           Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Color3f>();
    Throw("Texture::eval_color3(): domain mismatch, expected Surface "
          "interaction, got Interaction.");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Array2f
SurfaceField<Float, Spectrum>::eval_array2(const SurfaceInteraction3f &,
                                      Args args,
                                      Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_array2(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Array2f>();
    Throw("Texture::eval_array2(): expected Array2[2], got %s[%u].",
          texture_field_value_type_name(out_type()), out_dim());
    return dr::zeros<Array2f>();
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Array2f
SurfaceField<Float, Spectrum>::eval_array2(const Interaction3f &, Args,
                                           Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array2f>();
    Throw("Texture::eval_array2(): domain mismatch, expected Surface "
          "interaction, got Interaction.");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Array3f
SurfaceField<Float, Spectrum>::eval_array3(const SurfaceInteraction3f &si,
                                      Args args,
                                      Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_array3(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Array3f>();
    if (out_type() != FieldValueType::Array3 || out_dim() != 3)
        Throw("Texture::eval_array3(): expected Array3[3], got %s[%u].",
              texture_field_value_type_name(out_type()), out_dim());
    return eval_3(si, active);
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Array3f
SurfaceField<Float, Spectrum>::eval_array3(const Interaction3f &, Args,
                                           Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array3f>();
    Throw("Texture::eval_array3(): domain mismatch, expected Surface "
          "interaction, got Interaction.");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::UnpolarizedSpectrum
SurfaceField<Float, Spectrum>::eval_spec(const SurfaceInteraction3f &si,
                                    Args args,
                                    Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_spec(): expected args_dim=0, got %u.", args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<UnpolarizedSpectrum>();
    uint32_t dim = (uint32_t) dr::size_v<UnpolarizedSpectrum>;
    if (out_type() != FieldValueType::Spectrum || out_dim() != dim)
        Throw("Texture::eval_spec(): expected Spectrum[%u], got %s[%u].",
              dim, texture_field_value_type_name(out_type()), out_dim());
    return eval(si, active);
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::UnpolarizedSpectrum
SurfaceField<Float, Spectrum>::eval_spec(const Interaction3f &, Args,
                                         Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<UnpolarizedSpectrum>();
    Throw("Texture::eval_spec(): domain mismatch, expected Surface "
          "interaction, got Interaction.");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Array6f
SurfaceField<Float, Spectrum>::eval_array6(const SurfaceInteraction3f &,
                                      Args args,
                                      Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_array6(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Array6f>();
    Throw("Texture::eval_array6(): expected Features[6], got %s[%u].",
          texture_field_value_type_name(out_type()), out_dim());
    return dr::zeros<Array6f>();
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Array6f
SurfaceField<Float, Spectrum>::eval_array6(const Interaction3f &, Args,
                                           Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array6f>();
    Throw("Texture::eval_array6(): domain mismatch, expected Surface "
          "interaction, got Interaction.");
}

MI_VARIANT void
SurfaceField<Float, Spectrum>::eval_n(const SurfaceInteraction3f &si,
                                 Float *out,
                                 uint32_t count,
                                 Args args,
                                 Mask active) const {
    if (args.size != 0)
        Throw("Texture::eval_n(): expected args_dim=0, got %u.", args.size);
    if (dr::none_or<false>(active))
        return;
    if (count != out_dim())
        Throw("Texture::eval_n(): count (%u) must match out_dim (%u).",
              count, out_dim());

    if (out_type() == FieldValueType::Float) {
        out[0] = eval_1(si, active);
    } else if (out_type() == FieldValueType::Color3) {
        Color3f value = eval_3(si, active);
        out[0] = value.x();
        out[1] = value.y();
        out[2] = value.z();
    } else if (out_type() == FieldValueType::Array3) {
        Array3f value = eval_array3(si, args, active);
        out[0] = value.x();
        out[1] = value.y();
        out[2] = value.z();
    } else if (out_type() == FieldValueType::Spectrum) {
        UnpolarizedSpectrum value = eval(si, active);
        for (uint32_t i = 0; i < count; ++i)
            out[i] = value.entry(i);
    } else {
        Throw("Texture::eval_n(): unsupported texture field output %s[%u].",
              texture_field_value_type_name(out_type()), out_dim());
    }
}

MI_VARIANT void
SurfaceField<Float, Spectrum>::eval_n(const Interaction3f &, Float *, uint32_t,
                                 Args, Mask) const {
    Throw("Texture::eval_n(): domain mismatch, expected Surface interaction, "
          "got Interaction.");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::UnpolarizedSpectrum
SurfaceField<Float, Spectrum>::eval(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT std::pair<typename SurfaceField<Float, Spectrum>::Wavelength,
                     typename SurfaceField<Float, Spectrum>::UnpolarizedSpectrum>
SurfaceField<Float, Spectrum>::sample_spectrum(const SurfaceInteraction3f &,
                                          const Wavelength &, Mask) const {
    NotImplementedError("sample_spectrum");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Wavelength
SurfaceField<Float, Spectrum>::pdf_spectrum(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("pdf_spectrum");
}

MI_VARIANT Float SurfaceField<Float, Spectrum>::eval_1(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_1");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Vector2f
SurfaceField<Float, Spectrum>::eval_1_grad(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_1_grad");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::Color3f
SurfaceField<Float, Spectrum>::eval_3(const SurfaceInteraction3f &, Mask) const {
    NotImplementedError("eval_3");
}

MI_VARIANT Float
SurfaceField<Float, Spectrum>::mean() const {
    NotImplementedError("mean");
}

MI_VARIANT
std::pair<typename SurfaceField<Float, Spectrum>::Point2f, Float>
SurfaceField<Float, Spectrum>::sample_position(const Point2f &sample,
                                          Mask) const {
    return { sample, 1.f };
}

MI_VARIANT
Float SurfaceField<Float, Spectrum>::pdf_position(const Point2f &, Mask) const {
    return 1;
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::ScalarVector2i
SurfaceField<Float, Spectrum>::resolution() const {
    return ScalarVector2i(1, 1);
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::ScalarVector2i
SurfaceField<Float, Spectrum>::resolution_2d() const {
    return resolution();
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::ScalarFloat
SurfaceField<Float, Spectrum>::spectral_resolution() const {
    NotImplementedError("spectral_resolution");
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::ScalarVector2f
SurfaceField<Float, Spectrum>::wavelength_range() const {
    return ScalarVector2f(MI_CIE_MIN, MI_CIE_MAX);
}

MI_VARIANT typename SurfaceField<Float, Spectrum>::ScalarFloat
SurfaceField<Float, Spectrum>::max() const {
    NotImplementedError("max");
}

MI_INSTANTIATE_CLASS(SurfaceField)
NAMESPACE_END(mitsuba)
