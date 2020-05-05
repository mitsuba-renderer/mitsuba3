#include <mitsuba/core/properties.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/tensor.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>

/// Set to 1 to fall back to cosine-weighted sampling (for debugging)
#define MTS_SAMPLE_DIFFUSE     0

/// Sample the luminance map before warping by the NDF/VNDF?
#define MTS_SAMPLE_LUMINANCE   1

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Measured final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES()

    using Warp2D0 = Marginal2D<Float, 0, true>;
    using Warp2D2 = Marginal2D<Float, 2, true>;
    using Warp2D3 = Marginal2D<Float, 3, true>;

    Measured(const Properties &props) : Base(props) {
        if constexpr (is_polarized_v<Spectrum>)
            Throw("The measured BSDF model requires that rendering takes place in spectral mode!");

        m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);
        m_flags = m_components[0];

        auto fs            = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name             = file_path.filename().string();

        ref<TensorFile> tf = new TensorFile(file_path);
        auto theta_i       = tf->field("theta_i");
        auto phi_i         = tf->field("phi_i");
        auto ndf           = tf->field("ndf");
        auto sigma         = tf->field("sigma");
        auto vndf          = tf->field("vndf");
        auto spectra       = tf->field("spectra");
        auto luminance     = tf->field("luminance");
        auto wavelengths   = tf->field("wavelengths");
        auto description   = tf->field("description");
        auto jacobian      = tf->field("jacobian");

        if (!(description.shape.size() == 1 &&
              description.dtype == Struct::Type::UInt8 &&

              theta_i.shape.size() == 1 &&
              theta_i.dtype == Struct::Type::Float32 &&

              phi_i.shape.size() == 1 &&
              phi_i.dtype == Struct::Type::Float32 &&

              wavelengths.shape.size() == 1 &&
              wavelengths.dtype == Struct::Type::Float32 &&

              ndf.shape.size() == 2 &&
              ndf.dtype == Struct::Type::Float32 &&

              sigma.shape.size() == 2 &&
              sigma.dtype == Struct::Type::Float32 &&

              vndf.shape.size() == 4 &&
              vndf.dtype == Struct::Type::Float32 &&
              vndf.shape[0] == phi_i.shape[0] &&
              vndf.shape[1] == theta_i.shape[0] &&

              luminance.shape.size() == 4 &&
              luminance.dtype == Struct::Type::Float32 &&
              luminance.shape[0] == phi_i.shape[0] &&
              luminance.shape[1] == theta_i.shape[0] &&
              luminance.shape[2] == luminance.shape[3] &&

              spectra.dtype == Struct::Type::Float32 &&
              spectra.shape.size() == 5 &&
              spectra.shape[0] == phi_i.shape[0] &&
              spectra.shape[1] == theta_i.shape[0] &&
              spectra.shape[2] == wavelengths.shape[0] &&
              spectra.shape[3] == spectra.shape[4] &&

              luminance.shape[2] == spectra.shape[3] &&
              luminance.shape[3] == spectra.shape[4] &&

              jacobian.shape.size() == 1 &&
              jacobian.shape[0] == 1 &&
              jacobian.dtype == Struct::Type::UInt8))
              Throw("Invalid file structure: %s", tf);

        m_isotropic = phi_i.shape[0] <= 2;
        m_jacobian  = ((uint8_t *) jacobian.data)[0];

        if (!m_isotropic) {
            ScalarFloat *phi_i_data = (ScalarFloat *) phi_i.data;
            m_reduction = (int) std::rint((2 * math::Pi<ScalarFloat>) /
                (phi_i_data[phi_i.shape[0] - 1] - phi_i_data[0]));
        }

        // Construct NDF interpolant data structure
        m_ndf = Warp2D0(
            (ScalarFloat *) ndf.data,
            ScalarVector2u(ndf.shape[1], ndf.shape[0]),
            { }, { }, false, false
        );

        // Construct projected surface area interpolant data structure
        m_sigma = Warp2D0(
            (ScalarFloat *) sigma.data,
            ScalarVector2u(sigma.shape[1], sigma.shape[0]),
            { }, { }, false, false
        );

        // Construct VNDF warp data structure
        m_vndf = Warp2D2(
            (ScalarFloat *) vndf.data,
            ScalarVector2u(vndf.shape[3], vndf.shape[2]),
            {{ (uint32_t) phi_i.shape[0],
               (uint32_t) theta_i.shape[0] }},
            {{ (const ScalarFloat *) phi_i.data,
               (const ScalarFloat *) theta_i.data }}
        );

        // Construct Luminance warp data structure
        m_luminance = Warp2D2(
            (ScalarFloat *) luminance.data,
            ScalarVector2u(luminance.shape[3], luminance.shape[2]),
            {{ (uint32_t) phi_i.shape[0],
               (uint32_t) theta_i.shape[0] }},
            {{ (const ScalarFloat *) phi_i.data,
               (const ScalarFloat *) theta_i.data }}
        );

        // Construct spectral interpolant
        m_spectra = Warp2D3(
            (ScalarFloat *) spectra.data,
            ScalarVector2u(spectra.shape[4], spectra.shape[3]),
            {{ (uint32_t) phi_i.shape[0],
               (uint32_t) theta_i.shape[0],
               (uint32_t) wavelengths.shape[0] }},
            {{ (const ScalarFloat *) phi_i.data,
               (const ScalarFloat *) theta_i.data,
               (const ScalarFloat *) wavelengths.data }},
            false, false
        );

        std::string description_str(
            (const char *) description.data,
            (const char *) description.data + description.shape[0]
        );

        Log(Info, "Loaded material \"%s\" (resolution %i x %i x %i x %i x %i)",
            description_str, spectra.shape[0], spectra.shape[1],
            spectra.shape[3], spectra.shape[4], spectra.shape[2]);
    }

    /**
     * Numerically stable method computing the elevation of the given
     * (normalized) vector in the local frame.
     * Conceptually equivalent to:
     *     safe_acos(Frame3f::cos_theta(d))
     */
    auto elevation(const Vector3f &d) const {
        auto dist = sqrt(sqr(d.x()) + sqr(d.y()) + sqr(d.z() - 1.f));
        return 2.f * safe_asin(.5f * dist);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /*sample1*/,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = zero<BSDFSample3f>();
        Vector3f wi = si.wi;
        active &= Frame3f::cos_theta(wi) > 0;

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) || none_or<false>(active))
            return { bs, 0.f };

        Float sx = -1.f, sy = -1.f;

        if (m_reduction >= 2) {
            sy = wi.y();
            sx = (m_reduction == 4) ? wi.x() : sy;
            wi.x() = mulsign_neg(wi.x(), sx);
            wi.y() = mulsign_neg(wi.y(), sy);
        }

        Float theta_i = elevation(wi),
            phi_i   = atan2(wi.y(), wi.x());

        Float params[2] = { phi_i, theta_i };
        Vector2f u_wi(theta2u(theta_i), phi2u(phi_i));

        Vector2f sample;

#if MTS_SAMPLE_DIFFUSE == 0
        sample = Vector2f(sample2.y(), sample2.x());
        Float pdf = 1.f;

        #if MTS_SAMPLE_LUMINANCE == 1
        std::tie(sample, pdf) = m_luminance.sample(sample, params, active);
        #endif

        auto [u_m, ndf_pdf] = m_vndf.sample(sample, params, active);

        Float phi_m   = u2phi(u_m.y()),
            theta_m = u2theta(u_m.x());

        if (m_isotropic)
            phi_m += phi_i;

        // Spherical -> Cartesian coordinates
        auto [sin_phi_m, cos_phi_m] = sincos(phi_m);
        auto [sin_theta_m, cos_theta_m] = sincos(theta_m);

        Vector3f m(
            cos_phi_m * sin_theta_m,
            sin_phi_m * sin_theta_m,
            cos_theta_m
        );

        Float jacobian = enoki::max(2.f * sqr(math::Pi<Float>) * u_m.x() *
                                    sin_theta_m, 1e-6f) * 4.f * dot(wi, m);

        bs.wo = fmsub(m, 2.f * dot(m, wi), wi);
        bs.pdf = ndf_pdf * pdf / jacobian;
#else // MTS_SAMPLE_DIFFUSE
        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);

        Vector3f m = normalize(bs.wo + wi);

        // Cartesian -> spherical coordinates
        Float theta_m = elevation(m),
            phi_m   = atan2(m.y(), m.x());

        Vector2f u_m(theta2u(theta_m),
                    phi2u(m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - floor(u_m[1]);

    std::tie(sample, std::ignore) = m_vndf.invert(u_m, params, active);
#endif // MTS_SAMPLE_DIFFUSE

        bs.eta               = 1.f;
        bs.sampled_type      = +BSDFFlags::GlossyReflection;
        bs.sampled_component = 0;

        UnpolarizedSpectrum spec;
        for (size_t i = 0; i < array_size_v<UnpolarizedSpectrum>; ++i) {
            Float params_spec[3] = { phi_i, theta_i, si.wavelengths[i] };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        if (m_jacobian)
            spec *= m_ndf.eval(u_m, params, active) /
                    (4 * m_sigma.eval(u_wi, params, active));

        bs.wo.x() = mulsign_neg(bs.wo.x(), sx);
        bs.wo.y() = mulsign_neg(bs.wo.y(), sy);

        active &= Frame3f::cos_theta(bs.wo) > 0;

        return { bs, select(active, unpolarized<Spectrum>(spec) / bs.pdf, Spectrum(0.f)) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo_, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Vector3f wi = si.wi, wo = wo_;

        active &= Frame3f::cos_theta(wi) > 0.f &&
                Frame3f::cos_theta(wo) > 0.f;

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) || none_or<false>(active))
            return Spectrum(0.f);

        if (m_reduction >= 2) {
            Float sy = wi.y(),
                sx = (m_reduction == 4) ? wi.x() : sy;

            wi.x() = mulsign_neg(wi.x(), sx);
            wi.y() = mulsign_neg(wi.y(), sy);
            wo.x() = mulsign_neg(wo.x(), sx);
            wo.y() = mulsign_neg(wo.y(), sy);
        }

        Vector3f m = normalize(wo + wi);

        // Cartesian -> spherical coordinates
        Float theta_i = elevation(wi),
            phi_i   = atan2(wi.y(), wi.x()),
            theta_m = elevation(m),
            phi_m   = atan2(m.y(), m.x());

        // Spherical coordinates -> unit coordinate system
        Vector2f u_wi(theta2u(theta_i), phi2u(phi_i)),
                u_m (theta2u(theta_m), phi2u(
                    m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - floor(u_m[1]);

        Float params[2] = { phi_i, theta_i };
        auto [sample, unused] = m_vndf.invert(u_m, params, active);

        UnpolarizedSpectrum spec;
        for (size_t i = 0; i < array_size_v<UnpolarizedSpectrum>; ++i) {
            Float params_spec[3] = { phi_i, theta_i, si.wavelengths[i] };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        if (m_jacobian)
            spec *= m_ndf.eval(u_m, params, active) /
                    (4 * m_sigma.eval(u_wi, params, active));

        return unpolarized<Spectrum>(spec) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo_, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Vector3f wi = si.wi, wo = wo_;

        active &= Frame3f::cos_theta(wi) > 0.f &&
                Frame3f::cos_theta(wo) > 0.f;

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) || none_or<false>(active))
            return 0.f;

        if (m_reduction >= 2) {
            Float sy = wi.y(),
                sx = (m_reduction == 4) ? wi.x() : sy;

            wi.x() = mulsign_neg(wi.x(), sx);
            wi.y() = mulsign_neg(wi.y(), sy);
            wo.x() = mulsign_neg(wo.x(), sx);
            wo.y() = mulsign_neg(wo.y(), sy);
        }

#if MTS_SAMPLE_DIFFUSE == 1
        return select(active, warp::square_to_cosine_hemisphere_pdf(wo), 0.f);
#else // MTS_SAMPLE_DIFFUSE
        Vector3f m = normalize(wo + wi);

        // Cartesian -> spherical coordinates
        Float theta_i = elevation(wi),
            phi_i   = atan2(wi.y(), wi.x()),
            theta_m = elevation(m),
            phi_m   = atan2(m.y(), m.x());

        // Spherical coordinates -> unit coordinate system
        Vector2f u_wi(theta2u(theta_i), phi2u(phi_i));
        Vector2f u_m (theta2u(theta_m),
                    phi2u(m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - floor(u_m[1]);

        Float params[2] = { phi_i, theta_i };
        auto [sample, vndf_pdf] = m_vndf.invert(u_m, params, active);

        Float pdf = 1.f;
        #if MTS_SAMPLE_LUMINANCE == 1
        pdf = m_luminance.eval(sample, params, active);
        #endif

        Float jacobian =
            enoki::max(2.f * sqr(math::Pi<Float>) * u_m.x() * Frame3f::sin_theta(m), 1e-6f) * 4.f *
            dot(wi, m);

        pdf = vndf_pdf * pdf / jacobian;

        return select(active, pdf, 0.f);
#endif // MTS_SAMPLE_DIFFUSE
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Measured[" << std::endl
            << "  filename = \"" << m_name << "\"," << std::endl
            << "  ndf = " << string::indent(m_ndf.to_string()) << "," << std::endl
            << "  sigma = " << string::indent(m_sigma.to_string()) << "," << std::endl
            << "  vndf = " << string::indent(m_vndf.to_string()) << "," << std::endl
            << "  luminance = " << string::indent(m_luminance.to_string()) << "," << std::endl
            << "  spectra = " << string::indent(m_spectra.to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    template <typename Value> Value u2theta(Value u) const {
        return sqr(u) * (math::Pi<Float> / 2.f);
    }

    template <typename Value> Value u2phi(Value u) const {
        return (2.f * u - 1.f) * math::Pi<Float>;
    }

    template <typename Value> Value theta2u(Value theta) const {
        return sqrt(theta * (2.f / math::Pi<Float>));
    }

    template <typename Value> Value phi2u(Value phi) const {
        return (phi + math::Pi<Float>) * math::InvTwoPi<Float>;
    }

private:
    std::string m_name;
    Warp2D0 m_ndf;
    Warp2D0 m_sigma;
    Warp2D2 m_vndf;
    Warp2D2 m_luminance;
    Warp2D3 m_spectra;
    bool m_isotropic;
    bool m_jacobian;
    int m_reduction;
};

MTS_IMPLEMENT_CLASS_VARIANT(Measured, BSDF)
MTS_EXPORT_PLUGIN(Measured, "Measured material")
NAMESPACE_END(mitsuba)
