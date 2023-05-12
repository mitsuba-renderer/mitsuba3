#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT BSDF<Float, Spectrum>::BSDF(const Properties &props)
    : m_flags(+BSDFFlags::Empty), m_id(props.id()) { }

MI_VARIANT BSDF<Float, Spectrum>::~BSDF() { }

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
            if (&type == &typeid(Type))
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
    oss << "{ ";

#define is_set(flag) has_flag(type_mask, flag)
    if (is_set(BSDFFlags::All)) {
        oss << "all ";
        type_mask = type_mask & ~BSDFFlags::All;
    }
    if (is_set(BSDFFlags::Reflection)) {
        oss << "reflection ";
        type_mask = type_mask & ~BSDFFlags::Reflection;
    }
    if (is_set(BSDFFlags::Transmission)) {
        oss << "transmission ";
        type_mask = type_mask & ~BSDFFlags::Transmission;
    }
    if (is_set(BSDFFlags::Smooth)) {
        oss << "smooth ";
        type_mask = type_mask & ~BSDFFlags::Smooth;
    }
    if (is_set(BSDFFlags::Diffuse)) {
        oss << "diffuse ";
        type_mask = type_mask & ~BSDFFlags::Diffuse;
    }
    if (is_set(BSDFFlags::Glossy)) {
        oss << "glossy ";
        type_mask = type_mask & ~BSDFFlags::Glossy;
    }
    if (is_set(BSDFFlags::Delta)) {
        oss << "delta";
        type_mask = type_mask & ~BSDFFlags::Delta;
    }
    if (is_set(BSDFFlags::Delta1D)) {
        oss << "delta_1d ";
        type_mask = type_mask & ~BSDFFlags::Delta1D;
    }
    if (is_set(BSDFFlags::DiffuseReflection)) {
        oss << "diffuse_reflection ";
        type_mask = type_mask & ~BSDFFlags::DiffuseReflection;
    }
    if (is_set(BSDFFlags::DiffuseTransmission)) {
        oss << "diffuse_transmission ";
        type_mask = type_mask & ~BSDFFlags::DiffuseTransmission;
    }
    if (is_set(BSDFFlags::GlossyReflection)) {
        oss << "glossy_reflection ";
        type_mask = type_mask & ~BSDFFlags::GlossyReflection;
    }
    if (is_set(BSDFFlags::GlossyTransmission)) {
        oss << "glossy_transmission ";
        type_mask = type_mask & ~BSDFFlags::GlossyTransmission;
    }
    if (is_set(BSDFFlags::DeltaReflection)) {
        oss << "delta_reflection ";
        type_mask = type_mask & ~BSDFFlags::DeltaReflection;
    }
    if (is_set(BSDFFlags::DeltaTransmission)) {
        oss << "delta_transmission ";
        type_mask = type_mask & ~BSDFFlags::DeltaTransmission;
    }
    if (is_set(BSDFFlags::Delta1DReflection)) {
        oss << "delta_1d_reflection ";
        type_mask = type_mask & ~BSDFFlags::Delta1DReflection;
    }
    if (is_set(BSDFFlags::Delta1DTransmission)) {
        oss << "delta_1d_transmission ";
        type_mask = type_mask & ~BSDFFlags::Delta1DTransmission;
    }
    if (is_set(BSDFFlags::Null)) {
        oss << "null ";
        type_mask = type_mask & ~BSDFFlags::Null;
    }
    if (is_set(BSDFFlags::Anisotropic)) {
        oss << "anisotropic ";
        type_mask = type_mask & ~BSDFFlags::Anisotropic;
    }
    if (is_set(BSDFFlags::FrontSide)) {
        oss << "front_side ";
        type_mask = type_mask & ~BSDFFlags::FrontSide;
    }
    if (is_set(BSDFFlags::BackSide)) {
        oss << "back_side ";
        type_mask = type_mask & ~BSDFFlags::BackSide;
    }
    if (is_set(BSDFFlags::SpatiallyVarying)) {
        oss << "spatially_varying ";
        type_mask = type_mask & ~BSDFFlags::SpatiallyVarying;
    }
    if (is_set(BSDFFlags::NonSymmetric)) {
        oss << "non_symmetric ";
        type_mask = type_mask & ~BSDFFlags::NonSymmetric;
    }
#undef is_set

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
