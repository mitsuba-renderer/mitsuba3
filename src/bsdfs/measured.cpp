#include <mitsuba/core/properties.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/tensor.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>

/// Defines the details of the parameterization. Do not change this.
#define MTS_MARGINAL_WARP      1
#define MTS_REVERSE_AXES       0
#define MTS_WARP_VNDF          1

/// Set to 1 to fall back to cosine-weighted sampling (for debugging)
#define MTS_SAMPLE_DIFFUSE     0

/// Sample the luminance map before warping by the NDF/VNDF?
#define MTS_SAMPLE_LUMINANCE   1

NAMESPACE_BEGIN(mitsuba)

class Measured final : public BSDF {
public:
    #if MTS_MARGINAL_WARP == 1
        using Warp2D0 = warp::Marginal2D<0>;
        using Warp2D2 = warp::Marginal2D<2>;
        using Warp2D3 = warp::Marginal2D<3>;
    #else
        using Warp2D0 = warp::Hierarchical2D<0>;
        using Warp2D2 = warp::Hierarchical2D<2>;
        using Warp2D3 = warp::Hierarchical2D<3>;
    #endif

    Measured(const Properties &props) {
        m_flags = EGlossyReflection | EFrontSide;

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();

        ref<TensorFile> tf = new TensorFile(file_path);

        auto theta_i = tf->field("theta_i");
        auto phi_i = tf->field("phi_i");
        auto vndf = tf->field("vndf");
        auto ndf = tf->field("ndf");
        auto spectra = tf->field("spectra");
        auto luminance = tf->field("luminance");
        auto wavelengths = tf->field("wavelengths");
        auto description = tf->field("description");

        if (!(description.shape.size() == 1 &&
              description.dtype == Struct::EUInt8 &&

              theta_i.shape.size() == 1 &&
              theta_i.dtype == Struct::EFloat32 &&

              phi_i.shape.size() == 1 &&
              phi_i.dtype == Struct::EFloat32 &&

              wavelengths.shape.size() == 1 &&
              wavelengths.dtype == Struct::EFloat32 &&

              vndf.shape.size() == 4 &&
              vndf.dtype == Struct::EFloat32 &&
              vndf.shape[0] == phi_i.shape[0] &&
              vndf.shape[1] == theta_i.shape[0] &&

              ndf.shape.size() == 2 &&
              ndf.dtype == Struct::EFloat32 &&

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

              luminance.shape[2] == spectra.shape[3]))
              Throw("Invalid file structure: %s", tf->to_string());

        m_phi_relative = phi_i.shape[0] <= 2;

        /* Construct VNDF warp data structure */
        m_ndf = Warp2D0(Vector2u(ndf.shape[1], ndf.shape[0]), (Float *) ndf.data);

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
        if (!ctx.is_enabled(EGlossyReflection))
            return { bs, 0.f };

        active &= Frame::cos_theta(si.wi) > 0;

        Value theta_i = safe_acos(Frame::cos_theta(si.wi)),
              phi_i   = atan2(si.wi.y(), si.wi.x());

        Value phi_i_p = phi_i;
        masked(phi_i_p, phi_i_p < 0.f) +=
            m_phi_relative ? math::Pi : (2.f * math::Pi);

        Value params[2] = { phi_i_p, theta_i };

        Vector2 luminance_sample, ndf_sample;
        Value luminance_pdf, ndf_pdf;

        #if MTS_SAMPLE_DIFFUSE == 0
            #if MTS_SAMPLE_LUMINANCE == 1
                std::tie(luminance_sample, luminance_pdf) =
                    m_luminance.sample(Vector2(sample2.y(), sample2.x()), params, active);
            #else
                luminance_sample = Vector2(sample2.y(), sample2.x());
                luminance_pdf = 1.f;
            #endif

            #if MTS_WARP_VNDF == 1
                std::tie(ndf_sample, ndf_pdf) =
                    m_vndf.sample(luminance_sample, params, active);
            #else
                std::tie(ndf_sample, ndf_pdf) =
                    m_ndf.sample(luminance_sample, params, active);
            #endif

            #if MTS_REVERSE_AXES == 1
                std::swap(ndf_sample.x(), ndf_sample.y());
            #endif

            Value phi_m   = ndf_sample.y() * (2.f * math::Pi),
                  theta_m = sqr(ndf_sample.x()) * (math::Pi / 2.f);

            if (m_phi_relative)
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

            Value jacobian = enoki::max(2.f * sqr(math::Pi) * ndf_sample.x() *
                                        sin_theta_m, 1e-3f) * 4.f * dot(si.wi, m);

            bs.wo = fmsub(m, 2.f * dot(m, si.wi), si.wi);
            bs.pdf = ndf_pdf * luminance_pdf / jacobian;
        #else // MTS_SAMPLE_DIFFUSE
            (void) ndf_pdf; (void) ndf_sample;

            bs.wo = warp::square_to_cosine_hemisphere(sample2);
            bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);

            Vector3 m = normalize(bs.wo + si.wi);

            /* Cartesian -> spherical coordinates */
            Value theta_m = safe_acos(Frame::cos_theta(m)),
                  phi_m   = atan2(m.y(), m.x());

            Vector2 u(
                sqrt(2.f / math::Pi * theta_m),
                (phi_m - phi_i) * math::InvTwoPi
            );

            u[1] = u[1] - floor(u[1]);

            #if MTS_REVERSE_AXES == 1
                std::swap(u[0], u[1]);
            #endif

            #if MTS_WARP_VNDF == 1
                std::tie(luminance_sample, luminance_pdf) =
                    m_vndf.invert(u, params, active);
            #else
                std::tie(luminance_sample, luminance_pdf) =
                    m_ndf.invert(u, params, active);
            #endif
        #endif // MTS_SAMPLE_DIFFUSE

        bs.eta = 1.f;
        bs.sampled_type = (uint32_t) EGlossyReflection;
        bs.sampled_component = 0;

        Spectrum spec;
        for (size_t i = 0; i < MTS_WAVELENGTH_SAMPLES; ++i) {
            Value params_spec[3] = { phi_i_p, theta_i, si.wavelengths[i] };
            spec[i] = m_spectra.eval(luminance_sample, params_spec, active);
        }

        active &= Frame::cos_theta(bs.wo) > 0;

        return { bs, select(active, spec / bs.pdf, Spectrum(0.f)) };
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;
        using Vector2 = Vector<Value, 2>;

        active &= Frame::cos_theta(si.wi) > 0.f &&
                  Frame::cos_theta(wo) > 0.f;

        if (none(active) || !ctx.is_enabled(EGlossyReflection))
            return Spectrum(0.f);

        Vector3 m = normalize(wo + si.wi);

        /* Cartesian -> spherical coordinates */
        Value theta_i = safe_acos(Frame::cos_theta(si.wi)),
              phi_i   = atan2(si.wi.y(), si.wi.x()),
              theta_m = safe_acos(Frame::cos_theta(m)),
              phi_m   = atan2(m.y(), m.x());

        Vector2 u(
            sqrt(2.f / math::Pi * theta_m),
            (m_phi_relative ? (phi_m - phi_i) : phi_m) * math::InvTwoPi
        );

        u[1] = u[1] - floor(u[1]);
        masked(phi_i, phi_i < 0.f) += m_phi_relative ? math::Pi : (2.f * math::Pi);

        #if MTS_REVERSE_AXES == 1
            std::swap(u[0], u[1]);
        #endif

        Vector2 sample;
        Value pdf;
        Value params[2] = { phi_i, theta_i };
        #if MTS_WARP_VNDF == 1
            std::tie(sample, pdf) = m_vndf.invert(u, params, active);
        #else
            std::tie(sample, pdf) = m_ndf.invert(u, params, active);
        #endif

        Spectrum spec;
        for (size_t i = 0; i < MTS_WAVELENGTH_SAMPLES; ++i) {
            Value params_spec[3] = { phi_i, theta_i, si.wavelengths[i] };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        return spec & active;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        active &= Frame::cos_theta(si.wi) > 0.f &&
                  Frame::cos_theta(wo) > 0.f;

        if (none(active) || !ctx.is_enabled(EGlossyReflection))
            return 0.f;

        #if MTS_SAMPLE_DIFFUSE == 1
            return select(active, warp::square_to_cosine_hemisphere_pdf(wo), 0.f);
        #else // MTS_SAMPLE_DIFFUSE
            using Vector2 = Vector<Value, 2>;
            Vector3 m = normalize(wo + si.wi);

            /* Cartesian -> spherical coordinates */
            Value theta_i = safe_acos(Frame::cos_theta(si.wi)),
                  phi_i   = atan2(si.wi.y(), si.wi.x()),
                  theta_m = safe_acos(Frame::cos_theta(m)),
                  phi_m   = atan2(m.y(), m.x());

            Vector2 u(
                sqrt(2.f / math::Pi * theta_m),
                (m_phi_relative ? (phi_m - phi_i) : phi_m) * math::InvTwoPi
            );

            u[1] = u[1] - floor(u[1]);
            masked(phi_i, phi_i < 0.f) +=
                m_phi_relative ? math::Pi : (2.f * math::Pi);

            Vector2 ndf_sample;
            Value ndf_pdf, luminance_pdf;

            #if MTS_REVERSE_AXES == 1
                std::swap(u[0], u[1]);
            #endif

            Value params[2] = { phi_i, theta_i };
            #if MTS_WARP_VNDF == 1
                std::tie(ndf_sample, ndf_pdf) = m_vndf.invert(u, params, active);
            #else
                std::tie(ndf_sample, ndf_pdf) = m_ndf.invert(u, params, active);
            #endif

            #if MTS_SAMPLE_LUMINANCE == 1
                luminance_pdf = m_luminance.eval(ndf_sample, params, active);
            #else
                luminance_pdf = 1.f;
            #endif

            #if MTS_REVERSE_AXES == 0
                Value u_theta = u.x();
            #else
                Value u_theta = u.y();
            #endif

            Value jacobian = enoki::max(2.f * sqr(math::Pi) * u_theta *
                                        Frame::sin_theta(m), 1e-3f) *
                4.f * dot(si.wi, m);

            Value pdf = ndf_pdf * luminance_pdf / jacobian;

            return select(active, pdf, 0.f);
        #endif // MTS_SAMPLE_DIFFUSE
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Measured[" << std::endl
            << "  filename = \"" << m_name << "\"," << std::endl
            << "  ndf = " << string::indent(m_ndf.to_string()) << "," << std::endl
            << "  vndf = " << string::indent(m_vndf.to_string()) << "," << std::endl
            << "  luminance = " << string::indent(m_luminance.to_string()) << "," << std::endl
            << "  spectra = " << string::indent(m_spectra.to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()

private:
    std::string m_name;
    Warp2D0 m_ndf;
    Warp2D2 m_vndf;
    Warp2D2 m_luminance;
    Warp2D3 m_spectra;
    bool m_phi_relative;
};

MTS_IMPLEMENT_CLASS(Measured, BSDF)
MTS_EXPORT_PLUGIN(Measured, "Measured BRDF")

NAMESPACE_END(mitsuba)
