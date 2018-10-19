#include <mitsuba/core/properties.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/tensor.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>

/// Set to 1 to fall back to cosine-weighted sampling (for debugging)
#define MTS_SAMPLE_DIFFUSE     0

/// Sample the luminance map before warping by the NDF/VNDF?
#define MTS_SAMPLE_LUMINANCE   1

NAMESPACE_BEGIN(mitsuba)

class Measured final : public BSDF {
public:
    using Warp2D0 = warp::Marginal2D<0>;
    using Warp2D2 = warp::Marginal2D<2>;
    using Warp2D3 = warp::Marginal2D<3>;

    Measured(const Properties &props) {
        m_flags = EGlossyReflection | EFrontSide;

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();

        ref<TensorFile> tf = new TensorFile(file_path);

        auto theta_i = tf->field("theta_i");
        auto phi_i = tf->field("phi_i");
        auto ndf = tf->field("ndf");
        auto sigma = tf->field("sigma");
        auto vndf = tf->field("vndf");
        auto spectra = tf->field("spectra");
        auto luminance = tf->field("luminance");
        auto wavelengths = tf->field("wavelengths");
        auto description = tf->field("description");
        auto jacobian = tf->field("jacobian");

        if (!(description.shape.size() == 1 &&
              description.dtype == Struct::EUInt8 &&

              theta_i.shape.size() == 1 &&
              theta_i.dtype == Struct::EFloat32 &&

              phi_i.shape.size() == 1 &&
              phi_i.dtype == Struct::EFloat32 &&

              wavelengths.shape.size() == 1 &&
              wavelengths.dtype == Struct::EFloat32 &&

              ndf.shape.size() == 2 &&
              ndf.dtype == Struct::EFloat32 &&

              sigma.shape.size() == 2 &&
              sigma.dtype == Struct::EFloat32 &&

              vndf.shape.size() == 4 &&
              vndf.dtype == Struct::EFloat32 &&
              vndf.shape[0] == phi_i.shape[0] &&
              vndf.shape[1] == theta_i.shape[0] &&

              luminance.shape.size() == 4 &&
              luminance.dtype == Struct::EFloat32 &&
              luminance.shape[0] == phi_i.shape[0] &&
              luminance.shape[1] == theta_i.shape[0] &&
              luminance.shape[2] == luminance.shape[3] &&

              spectra.dtype == Struct::EFloat32 &&
              spectra.shape.size() == 5 &&
              spectra.shape[0] == phi_i.shape[0] &&
              spectra.shape[1] == theta_i.shape[0] &&
              spectra.shape[2] == wavelengths.shape[0] &&
              spectra.shape[3] == spectra.shape[4] &&

              luminance.shape[2] == spectra.shape[3] &&
              luminance.shape[3] == spectra.shape[4] &&

              jacobian.shape.size() == 1 &&
              jacobian.shape[0] == 1 &&
              jacobian.dtype == Struct::EUInt8))
              Throw("Invalid file structure: %s", tf->to_string());

        m_isotropic = phi_i.shape[0] <= 2;
        m_jacobian  = ((uint8_t *) jacobian.data)[0];

        if (!m_isotropic) {
            Float *phi_i_data = (Float *) phi_i.data;
            m_reduction = (int) std::rint((2 * math::Pi) /
                (phi_i_data[phi_i.shape[0] - 1] - phi_i_data[0]));
        }

        /* Construct NDF interpolant data structure */
        m_ndf = Warp2D0(
            Vector2u(ndf.shape[1], ndf.shape[0]),
            (Float *) ndf.data,
            { }, { }, false, false
        );

        /* Construct projected surface area interpolant data structure */
        m_sigma = Warp2D0(
            Vector2u(sigma.shape[1], sigma.shape[0]),
            (Float *) sigma.data,
            { }, { }, false, false
        );

        /* Construct VNDF warp data structure */
        m_vndf = Warp2D2(
            Vector2u(vndf.shape[3], vndf.shape[2]),
            (Float *) vndf.data,
            {{ (uint32_t) phi_i.shape[0],
               (uint32_t) theta_i.shape[0] }},
            {{ (const Float *) phi_i.data,
               (const Float *) theta_i.data }}
        );

        /* Construct Luminance warp data structure */
        m_luminance = Warp2D2(
            Vector2u(luminance.shape[3], luminance.shape[2]),
            (Float *) luminance.data,
            {{ (uint32_t) phi_i.shape[0],
               (uint32_t) theta_i.shape[0] }},
            {{ (const Float *) phi_i.data,
               (const Float *) theta_i.data }}
        );

        /* Construct spectral interpolant */
        m_spectra = Warp2D3(
            Vector2u(spectra.shape[4], spectra.shape[3]),
            (Float *) spectra.data,
            {{ (uint32_t) phi_i.shape[0],
               (uint32_t) theta_i.shape[0],
               (uint32_t) wavelengths.shape[0] }},
            {{ (const Float *) phi_i.data,
               (const Float *) theta_i.data,
               (const Float *) wavelengths.data }},
            false, false
        );

        std::string description_str(
            (const char *) description.data,
            (const char *) description.data + description.shape[0]
        );

        Log(EInfo, "Loaded material \"%s\" (resolution %i x %i x %i x %i x %i)",
            description_str, spectra.shape[0], spectra.shape[1],
            spectra.shape[3], spectra.shape[4], spectra.shape[2]);
    }

    template <typename Vector3> auto cos_theta(const Vector3f &d) {
        auto dist = std::sqrt(sqr(d.x()) + sqr(d.y()) + sqr(d.z() - 1.f));
        return 2.f * safe_asin(.5f * dist);
    }

    template <typename SurfaceInteraction, typename Value, typename Point2,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                const SurfaceInteraction &si,
                                                Value /* sample1 */,
                                                const Point2 &sample2,
                                                mask_t<Value> active) const {
        using Vector2 = Vector<Value, 2>;
        using Vector3 = Vector<Value, 3>;
        using Frame = Frame<Vector3>;

        BSDFSample bs;
        Vector3 wi = si.wi;
        active &= Frame::cos_theta(wi) > 0;

        if (!ctx.is_enabled(EGlossyReflection) || none(active))
            return { bs, 0.f };

        Value sx = -1.f, sy = -1.f;

        if (m_reduction >= 2) {
            sy = wi.y();
            sx = (m_reduction == 4) ? wi.x() : sy;
            wi.x() = mulsign_neg(wi.x(), sx);
            wi.y() = mulsign_neg(wi.y(), sy);
        }

        Value theta_i = cos_theta(wi),
              phi_i   = atan2(wi.y(), wi.x());

        Value params[2] = { phi_i, theta_i };
        Vector2 u_wi(theta2u(theta_i), phi2u(phi_i));

        Vector2 sample;

        #if MTS_SAMPLE_DIFFUSE == 0
            sample = Vector2(sample2.y(), sample2.x());
            Value pdf = 1.f;

            #if MTS_SAMPLE_LUMINANCE == 1
                std::tie(sample, pdf) =
                    m_luminance.sample(sample, params, active);
            #endif

            Vector2 u_m;
            Value ndf_pdf;
            std::tie(u_m, ndf_pdf) =
                m_vndf.sample(sample, params, active);

            Value phi_m   = u2phi(u_m.y()),
                  theta_m = u2theta(u_m.x());

            if (m_isotropic)
                phi_m += phi_i;

            /* Spherical -> Cartesian coordinates */
            Value sin_phi_m, cos_phi_m, sin_theta_m, cos_theta_m;
            std::tie(sin_phi_m, cos_phi_m) = sincos(phi_m);
            std::tie(sin_theta_m, cos_theta_m) = sincos(theta_m);

            Vector3 m(
                cos_phi_m * sin_theta_m,
                sin_phi_m * sin_theta_m,
                cos_theta_m
            );

            Value jacobian = enoki::max(2.f * sqr(math::Pi) * u_m.x() *
                                        sin_theta_m, 1e-6f) * 4.f * dot(wi, m);

            bs.wo = fmsub(m, 2.f * dot(m, wi), wi);
            bs.pdf = ndf_pdf * pdf / jacobian;
        #else // MTS_SAMPLE_DIFFUSE
            bs.wo = warp::square_to_cosine_hemisphere(sample2);
            bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);

            Vector3 m = normalize(bs.wo + wi);

            /* Cartesian -> spherical coordinates */
            Value theta_m = cos_theta(m),
                  phi_m   = atan2(m.y(), m.x());

            Vector2 u_m(theta2u(theta_m),
                        phi2u(m_isotropic ? (phi_m - phi_i) : phi_m));

            u_m[1] = u_m[1] - floor(u_m[1]);

            Value unused;
            std::tie(sample, unused) =
                m_vndf.invert(u_m, params, active);
        #endif // MTS_SAMPLE_DIFFUSE

        bs.eta = 1.f;
        bs.sampled_type = (uint32_t) EGlossyReflection;
        bs.sampled_component = 0;

        Spectrum spec;
        for (size_t i = 0; i < MTS_WAVELENGTH_SAMPLES; ++i) {
            Value params_spec[3] = { phi_i, theta_i, si.wavelengths[i] };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        if (m_jacobian)
            spec *= m_ndf.eval(u_m, params, active) /
                    (4 * m_sigma.eval(u_wi, params, active));

        bs.wo.x() = mulsign_neg(bs.wo.x(), sx);
        bs.wo.y() = mulsign_neg(bs.wo.y(), sy);

        active &= Frame::cos_theta(bs.wo) > 0;

        return { bs, select(active, spec / bs.pdf, Spectrum(0.f)) };
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo_, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;
        using Vector2 = Vector<Value, 2>;

        Vector3 wi = si.wi, wo = wo_;

        active &= Frame::cos_theta(wi) > 0.f &&
                  Frame::cos_theta(wo) > 0.f;

        if (none(active) || !ctx.is_enabled(EGlossyReflection))
            return Spectrum(0.f);

        if (m_reduction >= 2) {
            Value sy = wi.y(),
                  sx = (m_reduction == 4) ? wi.x() : sy;

            wi.x() = mulsign_neg(wi.x(), sx);
            wi.y() = mulsign_neg(wi.y(), sy);
            wo.x() = mulsign_neg(wo.x(), sx);
            wo.y() = mulsign_neg(wo.y(), sy);
        }

        Vector3 m = normalize(wo + wi);

        /* Cartesian -> spherical coordinates */
        Value theta_i = cos_theta(wi),
              phi_i   = atan2(wi.y(), wi.x()),
              theta_m = cos_theta(m),
              phi_m   = atan2(m.y(), m.x());

        /* Spherical coordinates -> unit coordinate system */
        Vector2 u_wi(theta2u(theta_i), phi2u(phi_i)),
                u_m (theta2u(theta_m), phi2u(
                     m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - floor(u_m[1]);

        Vector2 sample;
        Value unused, params[2] = { phi_i, theta_i };
        std::tie(sample, unused) = m_vndf.invert(u_m, params, active);

        Spectrum spec;
        for (size_t i = 0; i < MTS_WAVELENGTH_SAMPLES; ++i) {
            Value params_spec[3] = { phi_i, theta_i, si.wavelengths[i] };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        if (m_jacobian)
            spec *= m_ndf.eval(u_m, params, active) /
                    (4 * m_sigma.eval(u_wi, params, active));

        return spec & active;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo_, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        Vector3 wi = si.wi, wo = wo_;

        active &= Frame::cos_theta(wi) > 0.f &&
                  Frame::cos_theta(wo) > 0.f;

        if (none(active) || !ctx.is_enabled(EGlossyReflection))
            return 0.f;

        if (m_reduction >= 2) {
            Value sy = wi.y(),
                  sx = (m_reduction == 4) ? wi.x() : sy;

            wi.x() = mulsign_neg(wi.x(), sx);
            wi.y() = mulsign_neg(wi.y(), sy);
            wo.x() = mulsign_neg(wo.x(), sx);
            wo.y() = mulsign_neg(wo.y(), sy);
        }

        #if MTS_SAMPLE_DIFFUSE == 1
            return select(active, warp::square_to_cosine_hemisphere_pdf(wo), 0.f);
        #else // MTS_SAMPLE_DIFFUSE
            using Vector2 = Vector<Value, 2>;
            Vector3 m = normalize(wo + wi);

            /* Cartesian -> spherical coordinates */
            Value theta_i = cos_theta(wi),
                  phi_i   = atan2(wi.y(), wi.x()),
                  theta_m = cos_theta(m),
                  phi_m   = atan2(m.y(), m.x());

            /* Spherical coordinates -> unit coordinate system */
            Vector2 u_wi(theta2u(theta_i), phi2u(phi_i)),
                    u_m (theta2u(theta_m), phi2u(
                         m_isotropic ? (phi_m - phi_i) : phi_m));

            u_m[1] = u_m[1] - floor(u_m[1]);

            Vector2 sample;
            Value vndf_pdf, params[2] = { phi_i, theta_i };
            std::tie(sample, vndf_pdf) = m_vndf.invert(u_m, params, active);

            Value pdf = 1.f;
            #if MTS_SAMPLE_LUMINANCE == 1
                pdf = m_luminance.eval(sample, params, active);
            #endif

            Value jacobian = enoki::max(2.f * sqr(math::Pi) * u_m.x() *
                                        Frame::sin_theta(m), 1e-6f) * 4.f * dot(wi, m);

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

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()

private:
    template <typename Value> Value u2theta(Value u) const {
        return sqr(u) * (math::Pi / 2.f);
    }

    template <typename Value> Value u2phi(Value u) const {
        return (2.f * u - 1.f) * math::Pi;
    }

    template <typename Value> Value theta2u(Value theta) const {
        return sqrt(theta * (2.f / math::Pi));
    }

    template <typename Value> Value phi2u(Value phi) const {
        return (phi + math::Pi) * math::InvTwoPi;
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

MTS_IMPLEMENT_CLASS(Measured, BSDF)
MTS_EXPORT_PLUGIN(Measured, "Measured material")

NAMESPACE_END(mitsuba)
