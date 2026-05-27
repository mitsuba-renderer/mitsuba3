#include <mitsuba/render/volume.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! Volume base implementation
// =======================================================================

MI_VARIANT VolumeField<Float, Spectrum>::VolumeField(const Properties &props)
    : Base(props) {
    m_to_local = props.get<ScalarAffineTransform4f>("to_world", ScalarAffineTransform4f()).inverse();
    m_channel_count = 0;
    update_bbox();
}

MI_VARIANT FieldValueType VolumeField<Float, Spectrum>::out_type() const {
    switch (m_channel_count) {
        case 1: return FieldValueType::Float;
        case 3: return FieldValueType::Array3;
        case 6: return FieldValueType::Features;
        default: return FieldValueType::Features;
    }
}

MI_VARIANT FieldDomain VolumeField<Float, Spectrum>::domain() const {
    return FieldDomain::Interaction;
}

MI_VARIANT uint32_t VolumeField<Float, Spectrum>::out_dim() const {
    return m_channel_count;
}

MI_VARIANT uint32_t VolumeField<Float, Spectrum>::args_dim() const {
    return 0;
}

MI_VARIANT bool VolumeField<Float, Spectrum>::supports_scalar() const {
    return true;
}

MI_VARIANT bool VolumeField<Float, Spectrum>::supports_jit() const {
    return true;
}

MI_VARIANT bool VolumeField<Float, Spectrum>::supports_surface_queries() const {
    return false;
}

MI_VARIANT bool VolumeField<Float, Spectrum>::supports_interaction_queries() const {
    return true;
}

MI_VARIANT typename VolumeField<Float, Spectrum>::FloatStorage
VolumeField<Float, Spectrum>::eval(const SurfaceInteraction3f &, Args, Mask) const {
    Throw("Volume field query domain mismatch: expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::FloatStorage
VolumeField<Float, Spectrum>::eval(const Interaction3f &it,
                              Args args,
                              Mask active) const {
    if (args.size != 0)
        Throw("Volume field query expected args_dim=0, got %u.", args.size);
    uint32_t dim = out_dim();
    if (dim == 0)
        Throw("Volume field query has unknown output dimension.");

    FloatStorage result = dr::empty<FloatStorage>(dim);
    eval_n(it, result.data(), dim, Args{}, active);
    return result;
}

MI_VARIANT Float
VolumeField<Float, Spectrum>::eval_1(const SurfaceInteraction3f &, Args,
                                     Mask active) const {
    if (dr::none_or<false>(active))
        return 0.f;
    Throw("Volume::eval_1(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT Float
VolumeField<Float, Spectrum>::eval_1(const Interaction3f &it,
                                Args args,
                                Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_1(): expected args_dim=0, got %u.", args.size);
    if (dr::none_or<false>(active))
        return 0.f;
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    switch (type) {
        case FieldValueType::Float:
            if (dim == 1)
                return eval_1(it, active);
            break;

        case FieldValueType::Spectrum:
            return dr::mean(eval(it, active));

        case FieldValueType::Color3:
        case FieldValueType::Array3: {
            if (dim == 3) {
                Vector3f value = eval_3(it, active);
                return luminance(Color3f(value.x(), value.y(), value.z()));
            }
            break;
        }

        default:
            break;
    }

    Throw("Volume::eval_1(): expected scalar-compatible output, got %s[%u].",
          field_value_type_name(type), dim);
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Color3f
VolumeField<Float, Spectrum>::eval_color3(const SurfaceInteraction3f &, Args,
                                          Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Color3f>();
    Throw("Volume::eval_color3(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Color3f
VolumeField<Float, Spectrum>::eval_color3(const Interaction3f &it,
                                     Args args,
                                     Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_color3(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Color3f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Color3 || dim != 3)
        Throw("Volume::eval_color3(): expected Color3[3], got %s[%u].",
              field_value_type_name(type), dim);
    Vector3f value = eval_3(it, active);
    return Color3f(value.x(), value.y(), value.z());
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Array2f
VolumeField<Float, Spectrum>::eval_array2(const SurfaceInteraction3f &, Args,
                                          Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array2f>();
    Throw("Volume::eval_array2(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Array2f
VolumeField<Float, Spectrum>::eval_array2(const Interaction3f &, Args args,
                                          Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_array2(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Array2f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    Throw("Volume::eval_array2(): expected Array2[2], got %s[%u].",
          field_value_type_name(type), dim);
    return dr::zeros<Array2f>();
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Array3f
VolumeField<Float, Spectrum>::eval_array3(const SurfaceInteraction3f &, Args,
                                          Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array3f>();
    Throw("Volume::eval_array3(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Array3f
VolumeField<Float, Spectrum>::eval_array3(const Interaction3f &it,
                                     Args args,
                                     Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_array3(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Array3f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Array3 || dim != 3)
        Throw("Volume::eval_array3(): expected Array3[3], got %s[%u].",
              field_value_type_name(type), dim);
    return eval_3(it, active);
}

MI_VARIANT typename VolumeField<Float, Spectrum>::UnpolarizedSpectrum
VolumeField<Float, Spectrum>::eval_spec(const SurfaceInteraction3f &, Args,
                                        Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<UnpolarizedSpectrum>();
    Throw("Volume::eval_spec(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::UnpolarizedSpectrum
VolumeField<Float, Spectrum>::eval_spec(const Interaction3f &it,
                                   Args args,
                                   Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_spec(): expected args_dim=0, got %u.", args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<UnpolarizedSpectrum>();
    uint32_t expected_dim = (uint32_t) dr::size_v<UnpolarizedSpectrum>;
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Spectrum || dim != expected_dim)
        Throw("Volume::eval_spec(): expected Spectrum[%u], got %s[%u].",
              expected_dim, field_value_type_name(type), dim);
    return eval(it, active);
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Array6f
VolumeField<Float, Spectrum>::eval_array6(const SurfaceInteraction3f &, Args,
                                          Mask active) const {
    if (dr::none_or<false>(active))
        return dr::zeros<Array6f>();
    Throw("Volume::eval_array6(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Array6f
VolumeField<Float, Spectrum>::eval_array6(const Interaction3f &it,
                                     Args args,
                                     Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_array6(): expected args_dim=0, got %u.",
              args.size);
    if (dr::none_or<false>(active))
        return dr::zeros<Array6f>();
    FieldValueType type = out_type();
    uint32_t dim = out_dim();
    if (type != FieldValueType::Features || dim != 6)
        Throw("Volume::eval_array6(): expected Features[6], got %s[%u].",
              field_value_type_name(type), dim);
    return eval_6(it, active);
}

MI_VARIANT void
VolumeField<Float, Spectrum>::eval_n(const SurfaceInteraction3f &, Float *,
                                uint32_t, Args, Mask active) const {
    if (dr::none_or<false>(active))
        return;
    Throw("Volume::eval_n(): domain mismatch, expected Interaction, got "
          "Surface interaction.");
}

MI_VARIANT void
VolumeField<Float, Spectrum>::eval_n(const Interaction3f &it,
                                Float *out,
                                uint32_t count,
                                Args args,
                                Mask active) const {
    if (args.size != 0)
        Throw("Volume::eval_n(): expected args_dim=0, got %u.", args.size);
    if (dr::none_or<false>(active))
        return;
    uint32_t dim = out_dim();
    FieldValueType type = out_type();
    if (count != dim)
        Throw("Volume::eval_n(): count (%u) must match out_dim (%u).",
              count, dim);
    switch (type) {
        case FieldValueType::Float: {
            out[0] = eval_1(it, active);
            return;
        }

        case FieldValueType::Spectrum: {
            UnpolarizedSpectrum value = eval(it, active);
            for (uint32_t i = 0; i < count; ++i)
                out[i] = value.entry(i);
            return;
        }

        case FieldValueType::Color3:
        case FieldValueType::Array3: {
            Vector3f value = eval_3(it, active);
            out[0] = value.x();
            out[1] = value.y();
            out[2] = value.z();
            return;
        }

        case FieldValueType::Features:
            if (count == 6) {
                Array6f value = eval_6(it, active);
                for (uint32_t i = 0; i < 6; ++i)
                    out[i] = value.entry(i);
                return;
            }
            break;

        default:
            break;
    }

    Throw("Volume::eval_n(): unsupported volume field output %s[%u].",
          field_value_type_name(type), dim);
}

MI_VARIANT typename VolumeField<Float, Spectrum>::UnpolarizedSpectrum
VolumeField<Float, Spectrum>::eval(const Interaction3f &, Mask) const {
    NotImplementedError("eval");
}

MI_VARIANT Float VolumeField<Float, Spectrum>::eval_1(const Interaction3f &, Mask) const {
    NotImplementedError("eval_1");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::Vector3f
VolumeField<Float, Spectrum>::eval_3(const Interaction3f &, Mask) const {
    NotImplementedError("eval_3");
}

MI_VARIANT dr::Array<Float, 6>
VolumeField<Float, Spectrum>::eval_6(const Interaction3f &, Mask) const {
    NotImplementedError("eval_6");
}

MI_VARIANT void
VolumeField<Float, Spectrum>::eval_n(const Interaction3f & /*it*/,
                                Float * /*out*/,
                                Mask /*active*/) const {
    NotImplementedError("eval_n");
}

MI_VARIANT std::pair<typename VolumeField<Float, Spectrum>::UnpolarizedSpectrum,
                     typename VolumeField<Float, Spectrum>::Vector3f>
VolumeField<Float, Spectrum>::eval_gradient(const Interaction3f & /*it*/, Mask /*active*/) const {
    NotImplementedError("eval_gradient");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::ScalarFloat
VolumeField<Float, Spectrum>::max() const { NotImplementedError("max"); }

MI_VARIANT void
VolumeField<Float, Spectrum>::max_per_channel(ScalarFloat * /*out*/) const {
    NotImplementedError("max_per_channel");
}

MI_VARIANT typename VolumeField<Float, Spectrum>::ScalarVector3i
VolumeField<Float, Spectrum>::resolution() const {
    return ScalarVector3i(1, 1, 1);
}

MI_VARIANT typename VolumeField<Float, Spectrum>::ScalarVector3i
VolumeField<Float, Spectrum>::resolution_3d() const {
    return resolution();
}

MI_INSTANTIATE_CLASS(VolumeField)
NAMESPACE_END(mitsuba)
