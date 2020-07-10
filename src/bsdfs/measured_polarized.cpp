#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/tensor.h>
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/microfacet.h>

/* Set the weight for cosine hemisphere sampling in relation to GGX sampling.
   Set to 1.0 in order to fully fall back to cosine sampling. */
#define COSINE_HEMISPHERE_PDF_WEIGHT 0.1f

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-measured_polarized:

Measured polarized material (:monosp:`measured_polarized`)
-------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the material data file to be loaded
 * - alpha_sample
   - |float|
   - Specifies which roughness value should be used for the internal Microfacet
     importance sampling routine. (Default: 0.1)
 * - wavelength
   - |float|
   - Specifies if the material should only be rendered for just one specific wavelength.
     The valid range is between 450 and 650 nm.
     A value of -1 means the full spectrally-varying pBRDF will be used.
     (Default: -1, i.e. all wavelengths.)

This plugin allows rendering of polarized materials (pBRDFs) acquired as part of
"Image-Based Acquisition and Modeling of Polarimetric Reflectance" by Baek et al.
2020 (:cite:`Baek2020Image`). The required files for each material can
be found in the `corresponding database <http://vclab.kaist.ac.kr/siggraph2020/pbrdfdataset/kaistdataset.html>`_.

The dataset is made out of isotropic pBRDFs spanning a wide range of appearances:
diffuse/specular, metallic/dielectric, rough/smooth, and different color albedos,
captured in five wavelength ranges covering the visible spectrum from 450 to 650 nm.

Here are two example materials of gold, and a dielectric "fake" gold, together with
visualizations of the resulting Stokes vectors rendered with the
:ref:`Stokes <integrator-stokes>` integrator:

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_gold.jpg
   :caption: Measured gold material
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_gold_stokes_s1.jpg
   :caption: ":math:`\mathbf{s}_1`": horizontal vs. vertical polarization
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_gold_stokes_s2.jpg
   :caption: ":math:`\mathbf{s}_2`": positive vs. negative diagonal polarization
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_gold_stokes_s3.jpg
   :caption: ":math:`\mathbf{s}_3`": right vs. left circular polarization
.. subfigend::
   :label: fig-stokes

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_fakegold.jpg
   :caption: Measured "fake" gold material
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_fakegold_stokes_s1.jpg
   :caption: ":math:`\mathbf{s}_1`": horizontal vs. vertical polarization
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_fakegold_stokes_s2.jpg
   :caption: ":math:`\mathbf{s}_2`": positive vs. negative diagonal polarization
.. subfigure:: ../../resources/data/docs/images/render/bsdf_measured_polarized_fakegold_stokes_s3.jpg
   :caption: ":math:`\mathbf{s}_3`": right vs. left circular polarization
.. subfigend::
   :label: fig-stokes

In the following example, the measured gold BSDF from the dataset is setup:

.. code-block:: xml

    <bsdf type="measured_polarized">
        <string name="filename" value="6_gold_inpainted.pbsdf"/>
        <float name="alpha_sample" value="0.02"/>
    </bsdf>

Internally, a sampling routine from the GGX Microfacet model is used in order to
importance sampling outgoing directions. The used GGX roughness value is exposed
here as a user parameter `alpha_sample` and should be set according to the
approximate roughness of the material to be rendered. Note that any value here
will result in a correct rendering but the level of noise can vary significantly.

*/
template <typename Float, typename Spectrum>
class MeasuredPolarized final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture, MicrofacetDistribution)

    using Interpolator = Marginal2D<Float, 4, true>;

    MeasuredPolarized(const Properties &props) : Base(props) {
        if constexpr (!is_spectral_v<Spectrum>)
            Throw("The measured polarized BSDF model is only supported in spectral modes!");

        m_flags = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);

        m_alpha_sample = props.float_("alpha_sample", 0.1f);
        m_wavelength = props.float_("wavelength", -1.f);

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();

        ref<TensorFile> tf = new TensorFile(file_path);

        auto theta_h = tf->field("theta_h");
        auto theta_d = tf->field("theta_d");
        auto phi_d   = tf->field("phi_d");
        auto wvls    = tf->field("wvls");
        auto pbrdf   = tf->field("M");

        if (!(theta_h.shape.size() == 2 &&
              theta_h.dtype == Struct::Type::Float32 &&

              theta_d.shape.size() == 2 &&
              theta_d.dtype == Struct::Type::Float32 &&

              phi_d.shape.size() == 2 &&
              phi_d.dtype == Struct::Type::Float32 &&

              wvls.shape.size() == 1 &&
              wvls.dtype == Struct::Type::UInt16 &&

              pbrdf.dtype == Struct::Type::Float32 &&
              pbrdf.shape.size() == 6 &&
              pbrdf.shape[0] == phi_d.shape[1] &&
              pbrdf.shape[1] == theta_d.shape[1] &&
              pbrdf.shape[2] == theta_h.shape[1] &&
              pbrdf.shape[3] == wvls.shape[0]) &&
              pbrdf.shape[4] == 4 &&
              pbrdf.shape[5] == 4) {
            Throw("Invalid file structure: %s", tf->to_string());
        }

        ScalarFloat wavelengths[5];
        for (size_t i = 0; i < 5; ++i) {
            wavelengths[i] = ScalarFloat(((uint16_t *) wvls.data)[i]);
        }

        m_interpolator = Interpolator(
            (ScalarFloat *) pbrdf.data,
            ScalarVector2u(4, 4),
            {{ (uint32_t) phi_d.shape[1],
               (uint32_t) theta_d.shape[1],
               (uint32_t) theta_h.shape[1],
               (uint32_t) wvls.shape[0] }},
            {{ (const ScalarFloat *) phi_d.data,
               (const ScalarFloat *) theta_d.data,
               (const ScalarFloat *) theta_h.data,
               (const ScalarFloat *) wavelengths }},
            false, false
        );
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs;
        if (unlikely(ek::none_or<false>(active) || !ctx.is_enabled(BSDFFlags::GlossyReflection)))
            return { bs, 0.f };

        MicrofacetDistribution distr(MicrofacetType::GGX,
                                     m_alpha_sample, m_alpha_sample, true);

        Float lobe_pdf_diffuse = COSINE_HEMISPHERE_PDF_WEIGHT;
        Mask sample_diffuse    = active && sample1 < lobe_pdf_diffuse,
             sample_microfacet = active && !sample_diffuse;

        Vector3f wo_diffuse    = warp::square_to_cosine_hemisphere(sample2);
        auto [m, unused] = distr.sample(si.wi, sample2);
        Vector3f wo_microfacet = reflect(si.wi, m);

        bs.wo[sample_diffuse]    = wo_diffuse;
        bs.wo[sample_microfacet] = wo_microfacet;

        bs.pdf = pdf(ctx, si, bs.wo, active);

        bs.sampled_component = 0;
        bs.sampled_type = +BSDFFlags::GlossyReflection;
        bs.eta = 1.f;

        Spectrum value = eval(ctx, si, bs.wo, active);
        return { bs, ek::select(active && bs.pdf > 0, value / bs.pdf, 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);
        active &= (cos_theta_i > 0.f && cos_theta_o > 0.f);

        if (unlikely(ek::none_or<false>(active) || !ctx.is_enabled(BSDFFlags::GlossyReflection)))
            return 0.f;

        /* Due to lack of reciprocity in polarization-aware pBRDFs, they are
           always evaluated w.r.t. the actual light propagation direction, no
           matter the transport mode. In the following, 'wi_hat' is toward the
           light source. */
        Vector3f wi_hat = ctx.mode == TransportMode::Radiance ? wo : si.wi,
                 wo_hat = ctx.mode == TransportMode::Radiance ? si.wi : wo;

        /* We now transform both directions to the standard frame defined in
           Figure 3. Here, one of the directions is aligned with the x-axis. */
        Float phi_std = phi(wo_hat);
        Vector3f wi_std = rotate_vector(wi_hat, Vector3f(0,0,1), -phi_std),
                 wo_std = rotate_vector(wo_hat, Vector3f(0,0,1), -phi_std);

        /* This representation can be turned into the (isotropic) Rusinkiewicz
           parameterization. */
        auto [phi_d, theta_h, theta_d] = directions_to_rusinkiewicz(wi_std, wo_std);

        Spectrum value;
        if constexpr (is_spectral_v<Spectrum>) {
            if constexpr (is_polarized_v<Spectrum>) {
                /* The Stokes reference frame vector of this matrix lies in the plane
                   of reflection. See Figure 4. */
                Vector3f zi_std = -wi_std,
                         ti_std = normalize(cross(wi_std - wo_std, zi_std)),
                         yi_std = normalize(cross(ti_std, zi_std)),
                         xi_std = cross(yi_std, zi_std),
                         zo_std = wo_std,
                         to_std = normalize(cross(wo_std - wi_std, zo_std)),
                         yo_std = normalize(cross(to_std, zo_std)),
                         xo_std = cross(yo_std, zo_std);

                if (m_wavelength == -1.f) {
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            UnpolarizedSpectrum tmp(0.f);
                            for (size_t k = 0; k < ek::array_size_v<UnpolarizedSpectrum>; ++k) {
                                Float params[4] = {
                                    phi_d, theta_d, theta_h,
                                    si.wavelengths[k]
                                };
                                tmp[k] = m_interpolator.eval(Point2f(Float(j)/3.f, Float(i)/3.f), params, active);
                            }
                            value(i, j) = tmp;
                        }
                    }
                } else {
                    for (int i = 0; i < 4; ++i) {
                        for (int j = 0; j < 4; ++j) {
                            Float params[4] = {
                                phi_d, theta_d, theta_h,
                                Float(m_wavelength)
                            };
                            value(i,j) = m_interpolator.eval(Point2f(Float(j)/3.f, Float(i)/3.f), params, active);
                        }
                    }
                }

                /* Invalid configurations such as transmission directions are encoded as NaNs.
                   Make sure these values don't end up in the interpolated value. */
                ek::masked(value, any(isnan(value(0,0)))) = 0.f;

                // Make sure intensity is non-negative
                value(0,0) = max(0.f, value(0,0));

                // Reverse phi rotation from above on Stokes reference frames
                Vector3f xi_hat = rotate_vector(xi_std, Vector3f(0,0,1), phi_std),
                         xo_hat = rotate_vector(xo_std, Vector3f(0,0,1), phi_std);

                /* Rotate in/out reference vector of value s.t. it aligns with the
                   implicit Stokes bases of -wi_hat & wo_hat. */
                value = mueller::rotate_mueller_basis(value,
                                                      -wi_hat, xi_hat, mueller::stokes_basis(-wi_hat),
                                                       wo_hat, xo_hat, mueller::stokes_basis(wo_hat));
            } else {
                if (m_wavelength == -1.f) {
                    for (size_t k = 0; k < ek::array_size_v<UnpolarizedSpectrum>; ++k) {
                        Float params[4] = {
                            phi_d, theta_d, theta_h,
                            si.wavelengths[k]
                        };
                        value[k] = m_interpolator.eval(Point2f(0.f, 0.f), params, active);
                    }
                } else {
                    Float params[4] = {
                        phi_d, theta_d, theta_h,
                        Float(m_wavelength)
                    };
                    Float value_ = m_interpolator.eval(Point2f(0.f, 0.f), params, active);
                    value = Spectrum(value_);
                }

                // Make sure BRDF is non-negative
                value = max(0.f, value);
            }
        }

        return (value * cos_theta_o) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        if (unlikely(ek::none_or<false>(active) || !ctx.is_enabled(BSDFFlags::GlossyReflection)))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        MicrofacetDistribution distr(MicrofacetType::GGX,
                                     m_alpha_sample, m_alpha_sample, true);

        Vector3f H = normalize(wo + si.wi);

        Float pdf_diffuse = warp::square_to_cosine_hemisphere_pdf(wo);
        Float pdf_microfacet = distr.pdf(si.wi, H) / (4.f * dot(wo, H));

        Float pdf = 0.f;
        pdf += pdf_diffuse * COSINE_HEMISPHERE_PDF_WEIGHT;
        pdf += pdf_microfacet * (1.f - COSINE_HEMISPHERE_PDF_WEIGHT);

        return ek::select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MeasuredPolarized[" << std::endl
            << "  name = " << m_name << std::endl
            << "]";
        return oss.str();
    }

private:
    template <typename Vector3,
              typename Value = ek::value_t<Vector3>>
    Value phi(const Vector3 &v) const {
        Value p = atan2(v.y(), v.x());
        ek::masked(p, p < 0) += 2.f*ek::Pi<Float>;
        return p;
    }

    template <typename Vector3, typename Value = ek::value_t<Vector3>>
    MTS_INLINE
    Vector3 rotate_vector(const Vector3 &v, const Vector3 &axis_, Value angle) const {
        Vector3 axis = normalize(axis_);
        auto [sin_angle, cos_angle] = ek::sincos(angle);
        return v*cos_angle + axis*dot(v, axis)*(1.f - cos_angle) + sin_angle*cross(axis, v);
    }

    template <typename Vector3, typename Value = ek::value_t<Vector3>>
    MTS_INLINE
    std::tuple<Value, Value, Value> directions_to_rusinkiewicz(const Vector3 &i, const Vector3 &o) const {
        Vector3 h = normalize(i + o);

        Vector3 n(0, 0, 1);
        Vector3 b = normalize(cross(n, h)),
                t = normalize(cross(b, h));

        Value td = ek::safe_acos(dot(h, i)),
              th = ek::safe_acos(dot(n, h));

        Vector3 i_prj = normalize(i - dot(i, h)*h);
        Value cos_phi_d = ek::clamp(dot(t, i_prj), -1.f, 1.f),
              sin_phi_d = ek::clamp(dot(b, i_prj), -1.f, 1.f);

        Value pd = atan2(sin_phi_d, cos_phi_d);

        return std::make_tuple(pd, th, td);
    }

    MTS_DECLARE_CLASS()
private:
    std::string m_name;
    ScalarFloat m_wavelength;
    ScalarFloat m_alpha_sample;
    Interpolator  m_interpolator;
};

MTS_IMPLEMENT_CLASS_VARIANT(MeasuredPolarized, BSDF)
MTS_EXPORT_PLUGIN(MeasuredPolarized, "Measured polarized material")
NAMESPACE_END(mitsuba)
