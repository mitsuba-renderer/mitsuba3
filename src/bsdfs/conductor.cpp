#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SmoothConductor final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SmoothConductor, BSDF)
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    SmoothConductor(const Properties &props) : Base(props) {
        m_flags = BSDFFlags::DeltaReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);

        m_specular_reflectance = props.texture<Texture>("specular_reflectance", 1.f);

        m_eta = props.texture<Texture>("eta", 0.f);
        m_k   = props.texture<Texture>("k",   1.f);
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si, Float /*sample1*/,
           const Point2f & /*sample2*/, Mask active) const override {
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs;
        Spectrum value(0.f);
        if (unlikely(none_or<false>(active) || !ctx.is_enabled(BSDFFlags::DeltaReflection)))
            return { bs, value };

        bs.sampled_component = 0;
        bs.sampled_type = +BSDFFlags::DeltaReflection;
        bs.wo  = reflect(si.wi);
        bs.eta = 1.f;
        bs.pdf = 1.f;

        Complex<UnpolarizedSpectrum> eta(m_eta->eval(si, active),
                                         m_k->eval(si, active));
        value = m_specular_reflectance->eval(si, active) *
                fresnel_conductor(UnpolarizedSpectrum(cos_theta_i), eta);

        return { bs, value };
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    std::vector<ref<Object>> children() override {
        return { m_specular_reflectance.get(), m_eta.get(), m_k.get() };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothConductor[" << std::endl
            << "  eta = " << string::indent(m_eta) << "," << std::endl
            << "  k = "   << string::indent(m_k)   << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << std::endl
            << "]";
        return oss.str();
    }

private:
    ref<Texture> m_specular_reflectance;
    ref<Texture> m_eta, m_k;
};

MTS_EXPORT_PLUGIN(SmoothConductor, "Smooth conductor")

NAMESPACE_END(mitsuba)
