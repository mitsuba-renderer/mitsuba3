#include <cstring>

#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT BSDF<Float, Spectrum>::BSDF(const Properties &props)
    : m_flags(+BSDFFlags::Empty), m_id(props.id()) {
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_put(dr::backend_v<Float>, "mitsuba::BSDF", this);
}

MI_VARIANT BSDF<Float, Spectrum>::~BSDF() {
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_remove(this);
}

MI_VARIANT std::pair<Spectrum, Float>
BSDF<Float, Spectrum>::eval_pdf(const BSDFContext &ctx,
                                const SurfaceInteraction3f &si,
                                const Vector3f &wo,
                                Mask active) const {
    return { eval(ctx, si, wo, active), pdf(ctx, si, wo, active) };
}

MI_VARIANT std::tuple<Spectrum, Float, typename BSDF<Float, Spectrum>::BSDFSample3f, Spectrum>
BSDF<Float, Spectrum>::eval_pdf_sample(const BSDFContext &ctx,
                                       const SurfaceInteraction3f &si,
                                       const Vector3f &wo,
                                       Float sample1,
                                       const Point2f &sample2,
                                       Mask active) const {
        auto [e_val, pdf_val] = eval_pdf(ctx, si, wo, active);
        auto [bs, bsdf_weight] = sample(ctx, si, sample1, sample2, active);
        return { e_val, pdf_val, bs, bsdf_weight };
}

MI_VARIANT Spectrum BSDF<Float, Spectrum>::eval_null_transmission(
    const SurfaceInteraction3f & /* si */, Mask /* active */) const {
    return 0.f;
}

MI_VARIANT Spectrum BSDF<Float, Spectrum>::eval_diffuse_reflectance(
    const SurfaceInteraction3f &si, Mask active) const {
    Vector3f wo = Vector3f(0.0f, 0.0f, 1.0f);
    BSDFContext ctx;
    return eval(ctx, si, wo, active) * dr::Pi<Float>;
}

template <typename Texture, typename Type>
struct AttributeCallback : public TraversalCallback {
    using F1 = std::function<Type(Texture *)>;

    AttributeCallback(std::string name, F1 func_object):
        name(name), found(false), result(0.f), func_object(func_object) { }

    void put_object(const std::string &name, Object *obj, uint32_t) override {
        if (this->name == name) {
            Texture *texture = dynamic_cast<Texture *>(obj);
            if (texture) {
                result = func_object(texture);
                found = true;
            }
        }
    };

    void put_parameter_impl(const std::string &name, void *val, uint32_t, const std::type_info &type) override {
        if (this->name == name) {
            if (strcmp(type.name(), typeid(Type).name()) == 0)
                result = *((Type *) val);
            found = true;
        }
    };

    std::string name;
    bool found;
    Type result;
    F1 func_object;
};

MI_VARIANT typename BSDF<Float, Spectrum>::Mask
BSDF<Float, Spectrum>::has_attribute(const std::string& name, Mask /*active*/) const {
    AttributeCallback<Texture, float> cb(name, [&](Texture *) { return 0.f; });
    const_cast<BSDF<Float, Spectrum>*>(this)->traverse((TraversalCallback *) &cb);
    return cb.found;
}

MI_VARIANT typename BSDF<Float, Spectrum>::UnpolarizedSpectrum
BSDF<Float, Spectrum>::eval_attribute(const std::string &name,
                                      const SurfaceInteraction3f &si,
                                      Mask active) const {
    AttributeCallback<Texture, UnpolarizedSpectrum> cb(
        name,
        [&](Texture *texture) { return texture->eval(si, active); }
    );
    const_cast<BSDF<Float, Spectrum>*>(this)->traverse((TraversalCallback *) &cb);

    if (!cb.found) {
        if constexpr (!dr::is_jit_v<Float>)
            Throw("Invalid attribute requested %s.", name.c_str());
    }

    return cb.result;
}

MI_VARIANT Float
BSDF<Float, Spectrum>::eval_attribute_1(const std::string& name,
                                        const SurfaceInteraction3f & si,
                                        Mask active) const {
    AttributeCallback<Texture, Float> cb(
        name,
        [&](Texture *texture) { return texture->eval_1(si, active); }
    );
    const_cast<BSDF<Float, Spectrum>*>(this)->traverse((TraversalCallback *) &cb);

    if (!cb.found) {
        if constexpr (!dr::is_jit_v<Float>)
            Throw("Invalid attribute requested %s.", name.c_str());
    }

    return cb.result;
}

MI_VARIANT typename BSDF<Float, Spectrum>::Color3f
BSDF<Float, Spectrum>::eval_attribute_3(const std::string& name,
                                        const SurfaceInteraction3f & si,
                                        Mask active) const {
    AttributeCallback<Texture, Color3f> cb(
        name,
        [&](Texture *texture) { return texture->eval_3(si, active); }
    );
    const_cast<BSDF<Float, Spectrum>*>(this)->traverse((TraversalCallback *) &cb);

    if (!cb.found) {
        if constexpr (!dr::is_jit_v<Float>)
            Throw("Invalid attribute requested %s.", name.c_str());
    }

    return cb.result;
}

template <typename Index>
std::string type_mask_to_string(Index type_mask) {
    std::ostringstream oss;

    bool value_before = false;
    auto add_separator = [&]() {
        if (value_before)
            oss << "| ";
        value_before = true;
    };

    auto is_subset = [](BSDFFlags flag, Index flags) {
        return (((uint32_t) flag & flags) == (uint32_t) flag);
    };

    oss << "{ ";
    if (is_subset(BSDFFlags::All, type_mask)) {
        add_separator();
        oss << "all ";
        type_mask = type_mask & ~BSDFFlags::All;
    }
    if (is_subset(BSDFFlags::Reflection, type_mask)) {
        add_separator();
        oss << "reflection ";
        type_mask = type_mask & ~BSDFFlags::Reflection;
    }
    if (is_subset(BSDFFlags::Transmission, type_mask)) {
        add_separator();
        oss << "transmission ";
        type_mask = type_mask & ~BSDFFlags::Transmission;
    }
    if (is_subset(BSDFFlags::Smooth, type_mask)) {
        add_separator();
        oss << "smooth ";
        type_mask = type_mask & ~BSDFFlags::Smooth;
    }
    if (is_subset(BSDFFlags::Diffuse, type_mask)) {
        add_separator();
        oss << "diffuse ";
        type_mask = type_mask & ~BSDFFlags::Diffuse;
    }
    if (is_subset(BSDFFlags::Glossy, type_mask)) {
        add_separator();
        oss << "glossy ";
        type_mask = type_mask & ~BSDFFlags::Glossy;
    }
    if (is_subset(BSDFFlags::Delta, type_mask)) {
        add_separator();
        oss << "delta";
        type_mask = type_mask & ~BSDFFlags::Delta;
    }
    if (is_subset(BSDFFlags::Delta1D, type_mask)) {
        add_separator();
        oss << "delta_1d ";
        type_mask = type_mask & ~BSDFFlags::Delta1D;
    }
    if (has_flag(type_mask, BSDFFlags::DiffuseReflection)) {
        add_separator();
        oss << "diffuse_reflection ";
        type_mask = type_mask & ~BSDFFlags::DiffuseReflection;
    }
    if (has_flag(type_mask, BSDFFlags::DiffuseTransmission)) {
        add_separator();
        oss << "diffuse_transmission ";
        type_mask = type_mask & ~BSDFFlags::DiffuseTransmission;
    }
    if (has_flag(type_mask, BSDFFlags::GlossyReflection)) {
        add_separator();
        oss << "glossy_reflection ";
        type_mask = type_mask & ~BSDFFlags::GlossyReflection;
    }
    if (has_flag(type_mask, BSDFFlags::GlossyTransmission)) {
        add_separator();
        oss << "glossy_transmission ";
        type_mask = type_mask & ~BSDFFlags::GlossyTransmission;
    }
    if (has_flag(type_mask, BSDFFlags::DeltaReflection)) {
        add_separator();
        oss << "delta_reflection ";
        type_mask = type_mask & ~BSDFFlags::DeltaReflection;
    }
    if (has_flag(type_mask, BSDFFlags::DeltaTransmission)) {
        add_separator();
        oss << "delta_transmission ";
        type_mask = type_mask & ~BSDFFlags::DeltaTransmission;
    }
    if (has_flag(type_mask, BSDFFlags::Delta1DReflection)) {
        add_separator();
        oss << "delta_1d_reflection ";
        type_mask = type_mask & ~BSDFFlags::Delta1DReflection;
    }
    if (has_flag(type_mask, BSDFFlags::Delta1DTransmission)) {
        add_separator();
        oss << "delta_1d_transmission ";
        type_mask = type_mask & ~BSDFFlags::Delta1DTransmission;
    }
    if (has_flag(type_mask, BSDFFlags::Null)) {
        add_separator();
        oss << "null ";
        type_mask = type_mask & ~BSDFFlags::Null;
    }
    if (has_flag(type_mask, BSDFFlags::Anisotropic)) {
        add_separator();
        oss << "anisotropic ";
        type_mask = type_mask & ~BSDFFlags::Anisotropic;
    }
    if (has_flag(type_mask, BSDFFlags::FrontSide)) {
        add_separator();
        oss << "front_side ";
        type_mask = type_mask & ~BSDFFlags::FrontSide;
    }
    if (has_flag(type_mask, BSDFFlags::BackSide)) {
        add_separator();
        oss << "back_side ";
        type_mask = type_mask & ~BSDFFlags::BackSide;
    }
    if (has_flag(type_mask, BSDFFlags::SpatiallyVarying)) {
        add_separator();
        oss << "spatially_varying ";
        type_mask = type_mask & ~BSDFFlags::SpatiallyVarying;
    }
    if (has_flag(type_mask, BSDFFlags::NonSymmetric)) {
        add_separator();
        oss << "non_symmetric ";
        type_mask = type_mask & ~BSDFFlags::NonSymmetric;
    }
    if (has_flag(type_mask, BSDFFlags::NeedsDifferentials)) {
        add_separator();
        oss << "needs_differentials ";
        type_mask = type_mask & ~BSDFFlags::NeedsDifferentials;
    }

    Assert(type_mask == 0);
    oss << "}";
    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const BSDFContext& ctx) {
    os << "BSDFContext[" << std::endl
        << "  mode = " << ctx.mode << "," << std::endl
        << "  type_mask = " << type_mask_to_string(ctx.type_mask) << "," << std::endl
        << "  component = ";
    if (ctx.component == (uint32_t) -1)
        os << "all";
    else
        os << ctx.component;
    os  << std::endl << "]";
    return os;
}

std::ostream &operator<<(std::ostream &os, const TransportMode &mode) {
    switch (mode) {
        case TransportMode::Radiance:   os << "radiance"; break;
        case TransportMode::Importance: os << "importance"; break;
        default:                        os << "invalid"; break;
    }
    return os;
}

MI_IMPLEMENT_CLASS_VARIANT(BSDF, Object, "bsdf")
MI_INSTANTIATE_CLASS(BSDF)
NAMESPACE_END(mitsuba)
