#include <enoki/stl.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-direct:

Direct illumination integrator (:monosp:`direct`)
-------------------------------------------------

.. pluginparameters::

 * - shading_samples
   - |int|
   - This convenience parameter can be used to set both :code:`emitter_samples` and
     :code:`bsdf_samples` at the same time.
 * - emitter_samples
   - |int|
   - Optional more fine-grained parameter: specifies the number of samples that should be generated
     using the direct illumination strategies implemented by the scene's emitters.
     (Default: set to the value of :monosp:`shading_samples`)
 * - bsdf_samples
   - |int|
   - Optional more fine-grained parameter: specifies the number of samples that should be generated
     using the BSDF sampling strategies implemented by the scene's surfaces.
     (Default: set to the value of :monosp:`shading_samples`)
 * - hide_emitters
   - |bool|
   - Hide directly visible emitters.
     (Default: no, i.e. |false|)

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/integrator_direct_bsdf.jpg
   :caption: (**a**) BSDF sampling only
   :label: fig-direct-bsdf
.. subfigure:: ../../resources/data/docs/images/render/integrator_direct_lum.jpg
   :caption: (**b**) Emitter sampling only
   :label: fig-direct-lum
.. subfigure:: ../../resources/data/docs/images/render/integrator_direct_both.jpg
   :caption: (**c**) MIS between both sampling strategies
   :label: fig-direct-both
.. subfigend::
   :width: 0.32
   :label: fig-direct

This integrator implements a direct illumination technique that makes use
of *multiple importance sampling*: for each pixel sample, the
integrator generates a user-specifiable number of BSDF and emitter
samples and combines them using the power heuristic. Usually, the BSDF
sampling technique works very well on glossy objects but does badly
everywhere else (**a**), while the opposite is true for the emitter sampling
technique (**b**). By combining these approaches, one can obtain a rendering
technique that works well in both cases (**c**).

The number of samples spent on either technique is configurable, hence
it is also possible to turn this plugin into an emitter sampling-only
or BSDF sampling-only integrator.

.. note:: This integrator does not handle participating media or indirect illumination.

 */

template <typename Float, typename Spectrum>
class DirectIntegrator : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(SamplingIntegrator, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    // =============================================================
    //! @{ \name Constructors
    // =============================================================
    DirectIntegrator(const Properties &props) : Base(props) {
        if (props.has_property("shading_samples")
            && (props.has_property("emitter_samples") ||
                props.has_property("bsdf_samples"))) {
            Throw("Cannot specify both 'shading_samples' and"
                  " ('emitter_samples' and/or 'bsdf_samples').");
        }

        /// Number of shading samples -- this parameter is a shorthand notation
        /// to set both 'emitter_samples' and 'bsdf_samples' at the same time
        size_t shading_samples = props.size_("shading_samples", 1);

        /// Number of samples to take using the emitter sampling technique
        m_emitter_samples = props.size_("emitter_samples", shading_samples);

        /// Number of samples to take using the BSDF sampling technique
        m_bsdf_samples = props.size_("bsdf_samples", shading_samples);

        if (m_emitter_samples + m_bsdf_samples == 0)
            Throw("Must have at least 1 BSDF or emitter sample!");

        size_t sum    = m_emitter_samples + m_bsdf_samples;
        m_weight_bsdf = 1.f / (ScalarFloat) m_bsdf_samples;
        m_weight_lum  = 1.f / (ScalarFloat) m_emitter_samples;
        m_frac_bsdf   = m_bsdf_samples / (ScalarFloat) sum;
        m_frac_lum    = m_emitter_samples / (ScalarFloat) sum;
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        SurfaceInteraction3f si = scene->ray_intersect(ray, active);
        Mask valid_ray = si.is_valid();

        Spectrum result(0.f);

        // ----------------------- Visible emitters -----------------------
        if (!m_hide_emitters) {
            EmitterPtr emitter_vis = si.emitter(scene, active);
            if (any_or<true>(neq(emitter_vis, nullptr)))
                result += emitter_vis->eval(si, active);
        }

        active &= si.is_valid();
        if (none_or<false>(active))
            return { result, valid_ray };

        // ----------------------- Emitter sampling -----------------------

        BSDFContext ctx;
        BSDFPtr bsdf = si.bsdf(ray);
        Mask sample_emitter = active && has_flag(bsdf->flags(), BSDFFlags::Smooth);

        if (any_or<true>(sample_emitter)) {
            for (size_t i = 0; i < m_emitter_samples; ++i) {
                Mask active_e = sample_emitter;
                DirectionSample3f ds;
                Spectrum emitter_val;
                std::tie(ds, emitter_val) = scene->sample_emitter_direction(
                    si, sampler->next_2d(active_e), true, active_e);
                active_e &= neq(ds.pdf, 0.f);
                if (none_or<false>(active_e))
                    continue;

                // Query the BSDF for that emitter-sampled direction
                Vector3f wo = si.to_local(ds.d);

                Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);
                bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                /* Determine probability of having sampled that same
                   direction using BSDF sampling. */
                Float bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);

                Float mis = select(ds.delta, Float(1.f), mis_weight(
                    ds.pdf * m_frac_lum, bsdf_pdf * m_frac_bsdf) * m_weight_lum);
                result[active_e] += mis * bsdf_val * emitter_val;
            }
        }

        // ------------------------ BSDF sampling -------------------------

        for (size_t i = 0; i < m_bsdf_samples; ++i) {
            auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active),
                                               sampler->next_2d(active), active);
            bsdf_val = si.to_world_mueller(bsdf_val, -bs.wo, si.wi);

            Mask active_b = active && any(neq(depolarize(bsdf_val), 0.f));

            // Trace the ray in the sampled direction and intersect against the scene
            SurfaceInteraction si_bsdf =
                scene->ray_intersect(si.spawn_ray(si.to_world(bs.wo)), active_b);

            // Retain only rays that hit an emitter
            EmitterPtr emitter = si_bsdf.emitter(scene, active_b);
            active_b &= neq(emitter, nullptr);

            if (any_or<true>(active_b)) {
                Spectrum emitter_val = emitter->eval(si_bsdf, active_b);
                Mask delta = has_flag(bs.sampled_type, BSDFFlags::Delta);

                /* Determine probability of having sampled that same
                   direction using Emitter sampling. */
                DirectionSample3f ds(si_bsdf, si);
                ds.object = emitter;

                Float emitter_pdf =
                    select(delta, 0.f, scene->pdf_emitter_direction(si, ds, active_b));

                result[active_b] +=
                    bsdf_val * emitter_val *
                    mis_weight(bs.pdf * m_frac_bsdf, emitter_pdf * m_frac_lum) *
                    m_weight_bsdf;
            }
        }

        return { result, valid_ray };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DirectIntegrator[" << std::endl
            << "  emitter_samples = " << m_emitter_samples << "," << std::endl
            << "  bsdf_samples = " << m_bsdf_samples << std::endl
            << "]";
        return oss.str();
    }

    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        return select(pdf_a > 0.f, pdf_a / (pdf_a + pdf_b), Float(0.f));
    }

    MTS_DECLARE_CLASS()
private:
    size_t m_emitter_samples;
    size_t m_bsdf_samples;
    ScalarFloat m_frac_bsdf, m_frac_lum;
    ScalarFloat m_weight_bsdf, m_weight_lum;
};

MTS_IMPLEMENT_CLASS_VARIANT(DirectIntegrator, SamplingIntegrator)
MTS_EXPORT_PLUGIN(DirectIntegrator, "Direct integrator");
NAMESPACE_END(mitsuba)
