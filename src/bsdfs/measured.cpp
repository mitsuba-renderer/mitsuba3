#include <mitsuba/core/properties.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/tensor.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <array>
#include <cmath>

/// Set to 1 to fall back to cosine-weighted sampling (for debugging)
#define MI_SAMPLE_DIFFUSE     0

/// Sample the luminance map before warping by the NDF/VNDF?
#define MI_SAMPLE_LUMINANCE   1

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-measured:

Measured material (:monosp:`measured`)
--------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the material data file to be loaded

This plugin implements the data-driven material model described in the paper `An
Adaptive Parameterization for Efficient Material Acquisition and Rendering
<http://rgl.epfl.ch/publications/Dupuy2018Adaptive>`__. A database containing
compatible materials is are `here <http://rgl.epfl.ch/materials>`__.

Simply click on a material on this page and then download the *RGB* or
*spectral* ``.bsdf`` file and pass it to the ``filename`` parameter of the
plugin. Note that the spectral data files can only be used in a spectral
variant of Mitsuba, and the RGB-based approximations require an RGB variant.
The original measurements are spectral and cover the 360--1000nm range, hence
it is strongly recommended that you use a spectral workflow. (Many colors
cannot be reliably represented in RGB, as they are outside of the color gamut)

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_aniso_morpho_melenaus.jpg
   :caption: Iridescent butterfly (Dorsal *Morpho Melenaus* wing, anisotropic)
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_aniso_cc_nothern_aurora.jpg
   :caption: Vinyl car wrap material (TeckWrap RD05 *Northern Aurora*, isotropic)
.. subfigend::
   :label: fig-measured

Note that this material is one-sided---that is, observed from the back side, it
will be completely black. If this is undesirable, consider using the
:ref:`twosided <bsdf-twosided>` BRDF adapter plugin. The following XML snippet
describes how to import one of the spectral measurements from the database.

.. tabs::
    .. code-tab:: xml
        :name: lst-measured

        <bsdf type="measured">
            <string name="filename" value="cc_nothern_aurora_spec.bsdf"/>
        </bsdf>

    .. code-tab:: python

        'type': 'measured',
        'filename': 'cc_nothern_aurora_spec.bsdf'

*/
template <typename Float, typename Spectrum>
class Measured final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES()

    using Warp2D0 = Marginal2D<Float, 0, true>;
    using Warp2D2 = Marginal2D<Float, 2, true>;
    using Warp2D3 = Marginal2D<Float, 3, true>;

    Measured(const Properties &props) : Base(props) {
        m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);
        m_flags = m_components[0];

        auto fs            = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name             = file_path.filename().string();

        ref<TensorFile> tf = new TensorFile(file_path);
        using Field = TensorFile::Field;

        const Field &theta_i       = tf->field("theta_i");
        const Field &phi_i         = tf->field("phi_i");
        const Field &ndf           = tf->field("ndf");
        const Field &sigma         = tf->field("sigma");
        const Field &vndf          = tf->field("vndf");
        const Field &luminance     = tf->field("luminance");
        const Field &description   = tf->field("description");
        const Field &jacobian      = tf->field("jacobian");

        Field spectra, wavelengths;
        bool is_spectral = tf->has_field("wavelengths");

        const ScalarFloat rgb_wavelengths[3] = { 0, 1, 2 };
        if (is_spectral) {
            spectra = tf->field("spectra");
            wavelengths = tf->field("wavelengths");
            if constexpr (!is_spectral_v<Spectrum>)
                Throw("Measurements in spectral format require the use of a spectral variant of Mitsuba!");
        } else {
            spectra = tf->field("rgb");
            if constexpr (!is_rgb_v<Spectrum>)
                Throw("Measurements in RGB format require the use of a RGB variant of Mitsuba!");

            wavelengths.shape.push_back(3);
            wavelengths.data = rgb_wavelengths;
        }

        if (!(description.shape.size() == 1 &&
              description.dtype == Struct::Type::UInt8 &&

              theta_i.shape.size() == 1 &&
              theta_i.dtype == Struct::Type::Float32 &&

              phi_i.shape.size() == 1 &&
              phi_i.dtype == Struct::Type::Float32 &&

              (!is_spectral || (
                  wavelengths.shape.size() == 1 &&
                  wavelengths.dtype == Struct::Type::Float32
              )) &&

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
              spectra.shape[2] == (is_spectral ? wavelengths.shape[0] : 3) &&
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
            m_reduction = (int) std::rint((2 * dr::Pi<ScalarFloat>) /
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
        auto dist = dr::sqrt(dr::square(d.x()) + dr::square(d.y()) + dr::square(d.z() - 1.f));
        return 2.f * dr::safe_asin(.5f * dist);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /*sample1*/,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        Vector3f wi = si.wi;
        active &= Frame3f::cos_theta(wi) > 0;

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active))
            return { bs, 0.f };

        Float sx = -1.f, sy = -1.f;

        if (m_reduction >= 2) {
            sy = wi.y();
            sx = (m_reduction == 4) ? wi.x() : sy;
            wi.x() = dr::mulsign_neg(wi.x(), sx);
            wi.y() = dr::mulsign_neg(wi.y(), sy);
        }

        Float theta_i = elevation(wi),
            phi_i   = dr::atan2(wi.y(), wi.x());

        Float params[2] = { phi_i, theta_i };
        Vector2f u_wi(theta2u(theta_i), phi2u(phi_i));

        Vector2f sample;

#if MI_SAMPLE_DIFFUSE == 0
        sample = Vector2f(sample2.y(), sample2.x());
        Float pdf = 1.f;

        #if MI_SAMPLE_LUMINANCE == 1
        std::tie(sample, pdf) = m_luminance.sample(sample, params, active);
        #endif

        auto [u_m, ndf_pdf] = m_vndf.sample(sample, params, active);

        Float phi_m   = u2phi(u_m.y()),
              theta_m = u2theta(u_m.x());

        if (m_isotropic)
            phi_m += phi_i;

        // Spherical -> Cartesian coordinates
        auto [sin_phi_m, cos_phi_m] = dr::sincos(phi_m);
        auto [sin_theta_m, cos_theta_m] = dr::sincos(theta_m);

        Vector3f m(
            cos_phi_m * sin_theta_m,
            sin_phi_m * sin_theta_m,
            cos_theta_m
        );

        Float jacobian = dr::maximum(2.f * dr::square(dr::Pi<Float>) * u_m.x() *
                                    sin_theta_m, 1e-6f) * 4.f * dr::dot(wi, m);

        bs.wo = dr::fmsub(m, 2.f * dr::dot(m, wi), wi);
        bs.pdf = ndf_pdf * pdf / jacobian;
#else // MI_SAMPLE_DIFFUSE
        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);

        Vector3f m = dr::normalize(bs.wo + wi);

        // Cartesian -> spherical coordinates
        Float theta_m = elevation(m),
              phi_m   = dr::atan2(m.y(), m.x());

        Vector2f u_m(theta2u(theta_m),
                     phi2u(m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - dr::floor(u_m[1]);

    std::tie(sample, std::ignore) = m_vndf.invert(u_m, params, active);
#endif // MI_SAMPLE_DIFFUSE

        bs.eta               = 1.f;
        bs.sampled_type      = +BSDFFlags::GlossyReflection;
        bs.sampled_component = 0;

        UnpolarizedSpectrum spec;
        for (size_t i = 0; i < dr::size_v<UnpolarizedSpectrum>; ++i) {
            Float params_spec[3] = { phi_i, theta_i,
                is_spectral_v<Spectrum> ? si.wavelengths[i] : Float((float) i) };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        if (m_jacobian)
            spec *= m_ndf.eval(u_m, params, active) /
                    (4 * m_sigma.eval(u_wi, params, active));

        bs.wo.x() = dr::mulsign_neg(bs.wo.x(), sx);
        bs.wo.y() = dr::mulsign_neg(bs.wo.y(), sy);

        active &= Frame3f::cos_theta(bs.wo) > 0;

        return { bs, (depolarizer<Spectrum>(spec) / bs.pdf) & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo_, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Vector3f wi = si.wi, wo = wo_;

        active &= Frame3f::cos_theta(wi) > 0.f &&
                  Frame3f::cos_theta(wo) > 0.f;

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active))
            return Spectrum(0.f);

        if (m_reduction >= 2) {
            Float sy = wi.y(),
                sx = (m_reduction == 4) ? wi.x() : sy;

            wi.x() = dr::mulsign_neg(wi.x(), sx);
            wi.y() = dr::mulsign_neg(wi.y(), sy);
            wo.x() = dr::mulsign_neg(wo.x(), sx);
            wo.y() = dr::mulsign_neg(wo.y(), sy);
        }

        Vector3f m = dr::normalize(wo + wi);

        // Cartesian -> spherical coordinates
        Float theta_i = elevation(wi),
              phi_i   = dr::atan2(wi.y(), wi.x()),
              theta_m = elevation(m),
              phi_m   = dr::atan2(m.y(), m.x());

        // Spherical coordinates -> unit coordinate system
        Vector2f u_wi(theta2u(theta_i), phi2u(phi_i)),
                 u_m (theta2u(theta_m), phi2u(
                     m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - dr::floor(u_m[1]);

        Float params[2] = { phi_i, theta_i };
        auto [sample, unused] = m_vndf.invert(u_m, params, active);

        UnpolarizedSpectrum spec;
        for (size_t i = 0; i < dr::size_v<UnpolarizedSpectrum>; ++i) {
            Float params_spec[3] = { phi_i, theta_i,
                is_spectral_v<Spectrum> ? si.wavelengths[i] : Float((float) i) };
            spec[i] = m_spectra.eval(sample, params_spec, active);
        }

        if (m_jacobian)
            spec *= m_ndf.eval(u_m, params, active) /
                    (4 * m_sigma.eval(u_wi, params, active));

        return depolarizer<Spectrum>(spec) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo_, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Vector3f wi = si.wi, wo = wo_;

        active &= Frame3f::cos_theta(wi) > 0.f &&
                  Frame3f::cos_theta(wo) > 0.f;

        if (!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active))
            return 0.f;

        if (m_reduction >= 2) {
            Float sy = wi.y(),
                sx = (m_reduction == 4) ? wi.x() : sy;

            wi.x() = dr::mulsign_neg(wi.x(), sx);
            wi.y() = dr::mulsign_neg(wi.y(), sy);
            wo.x() = dr::mulsign_neg(wo.x(), sx);
            wo.y() = dr::mulsign_neg(wo.y(), sy);
        }

#if MI_SAMPLE_DIFFUSE == 1
        return dr::select(active, warp::square_to_cosine_hemisphere_pdf(wo), 0.f);
#else // MI_SAMPLE_DIFFUSE
        Vector3f m = dr::normalize(wo + wi);

        // Cartesian -> spherical coordinates
        Float theta_i = elevation(wi),
              phi_i   = dr::atan2(wi.y(), wi.x()),
              theta_m = elevation(m),
              phi_m   = dr::atan2(m.y(), m.x());

        // Spherical coordinates -> unit coordinate system
        Vector2f u_wi(theta2u(theta_i), phi2u(phi_i));
        Vector2f u_m (theta2u(theta_m),
                      phi2u(m_isotropic ? (phi_m - phi_i) : phi_m));

        u_m[1] = u_m[1] - dr::floor(u_m[1]);

        Float params[2] = { phi_i, theta_i };
        auto [sample, vndf_pdf] = m_vndf.invert(u_m, params, active);

        Float pdf = 1.f;
        #if MI_SAMPLE_LUMINANCE == 1
        pdf = m_luminance.eval(sample, params, active);
        #endif

        Float jacobian =
            dr::maximum(2.f * dr::square(dr::Pi<Float>) * u_m.x() * Frame3f::sin_theta(m), 1e-6f) * 4.f *
            dr::dot(wi, m);

        pdf = vndf_pdf * pdf / jacobian;

        return dr::select(active, pdf, 0.f);
#endif // MI_SAMPLE_DIFFUSE
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

    MI_DECLARE_CLASS()
private:
    template <typename Value> Value u2theta(Value u) const {
        return dr::square(u) * (dr::Pi<Float> / 2.f);
    }

    template <typename Value> Value u2phi(Value u) const {
        return (2.f * u - 1.f) * dr::Pi<Float>;
    }

    template <typename Value> Value theta2u(Value theta) const {
        return dr::sqrt(theta * (2.f / dr::Pi<Float>));
    }

    template <typename Value> Value phi2u(Value phi) const {
        return (phi + dr::Pi<Float>) * dr::InvTwoPi<Float>;
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

MI_IMPLEMENT_CLASS_VARIANT(Measured, BSDF)
MI_EXPORT_PLUGIN(Measured, "Measured material")
NAMESPACE_END(mitsuba)
