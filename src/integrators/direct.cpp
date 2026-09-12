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

.. tabs::
    .. code-tab::  xml
        :name: direct-integrator

        <integrator type="direct"/>

    .. code-tab:: python

        'type': 'direct'

 */

template <typename Float, typename Spectrum>
class DirectIntegrator : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    DirectIntegrator(const Properties &props) : Base(props) {
        if (props.has_property("shading_samples")
            && (props.has_property("emitter_samples") ||
                props.has_property("bsdf_samples"))) {
            Throw("Cannot specify both 'shading_samples' and"
                  " ('emitter_samples' and/or 'bsdf_samples').");
        }

        /// Number of shading samples -- this parameter is a shorthand notation
        /// to set both 'emitter_samples' and 'bsdf_samples' at the same time
        size_t shading_samples = props.get<size_t>("shading_samples", 1);

        /// Number of samples to take using the emitter sampling technique
        m_emitter_samples = props.get<size_t>("emitter_samples", shading_samples);

        /// Number of samples to take using the BSDF sampling technique
        m_bsdf_samples = props.get<size_t>("bsdf_samples", shading_samples);

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
                                     const Ray3f &ray,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        // The camera mask hides emitters marked as invisible
        SurfaceInteraction3f si = scene->ray_intersect(
            ray, +RayFlags::Default, /* coherent = */ true, +RayMask::Primary,
            active);

        Spectrum result(0.f);

        // ----------------------- Visible emitters -----------------------

        // The emitter lookup reuses the camera mask so that escaped rays
        // ignore a hidden environment emitter
        EmitterPtr emitter_vis = si.emitter(scene, +RayMask::Primary, active);
        if (dr::any_or<true>(emitter_vis != nullptr))
            result += emitter_vis->eval(si, active);

        Mask valid_ray = active && si.is_valid();

        active &= si.is_valid();
        if (dr::none_or<false>(active))
            return { result, valid_ray };

        // ----------------------- Emitter sampling -----------------------

        BSDFContext ctx;
        BSDFPtr bsdf = si.bsdf();
        Mask sample_emitter = active && bsdf->has_flag(BSDFFlags::Smooth);

        if (dr::any_or<true>(sample_emitter)) {
            for (size_t i = 0; i < m_emitter_samples; ++i) {
                Mask active_e = sample_emitter;
                DirectionSample3f ds;
                Spectrum emitter_val;
                std::tie(ds, emitter_val) = scene->sample_emitter_direction(
                    si, sampler->next_2d(active_e), true, active_e);
                active_e &= ds.pdf != 0.f;
                if (dr::none_or<false>(active_e))
                    continue;

                // Query the BSDF for that emitter-sampled direction
                Vector3f wo = si.to_local(ds.d);

                // Determine BSDF value and probability of having sampled
                // that same direction using BSDF sampling.
                auto [bsdf_val, bsdf_pdf] = bsdf->eval_pdf(ctx, si, wo, active_e);
                bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                Float mis = dr::select(ds.delta, Float(1.f), mis_weight(
                    ds.pdf * m_frac_lum, bsdf_pdf * m_frac_bsdf));
                result[active_e] += mis * bsdf_val * emitter_val * m_weight_lum;
            }
        }

        // ------------------------ BSDF sampling -------------------------

        // Null tests fold to literals in scenes without null shapes
        bool has_null = scene->has_null_shapes();

        for (size_t i = 0; i < m_bsdf_samples; ++i) {
            auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active),
                                               sampler->next_2d(active), active);
            bsdf_val = si.to_world_mueller(bsdf_val, -bs.wo, si.wi);

            Mask active_b = active && dr::any(unpolarized_spectrum(bsdf_val) != 0.f);

            // A ray that leaves through a null lobe remains a camera ray
            Mask null = has_null ? bs.is_null() : Mask(false);
            UInt32 ray_mask = dr::select(null, +RayMask::Primary,
                                         +RayMask::Secondary);

            Ray3f ray_b = si.spawn_ray(si.to_world(bs.wo));

            // Differentiation requires a detached sample direction and a
            // re-evaluation of the BSDF, as explained in the path tracer. A
            // null crossing keeps the attached weight of the null lobe.
            if (dr::grad_enabled(ray_b)) {
                ray_b = dr::detach(ray_b);
                Vector3f wo_2 = si.to_local(ray_b.d);
                auto [bsdf_val_2, bsdf_pdf_2] = bsdf->eval_pdf(ctx, si, wo_2);
                bsdf_val[bsdf_pdf_2 > 0.f && !null] =
                    bsdf_val_2 / dr::detach(bsdf_pdf_2);
            }

            // Trace a ray in the sampled direction and keep track of reduced
            // transmittance due to null surfaces
            auto [si_bsdf, tr] = scene->ray_intersect_tr(
                ray_b, +RayFlags::Default, /* coherent = */ false, ray_mask,
                active_b);

            // Retain only rays that hit an emitter
            EmitterPtr emitter = si_bsdf.emitter(scene, ray_mask, active_b);
            active_b &= (emitter != nullptr);

            if (dr::any_or<true>(active_b)) {
                Spectrum emitter_val = emitter->eval(si_bsdf, active_b);
                Mask delta = bs.is_delta();

                // Determine probability of having sampled that same
                // direction using Emitter sampling.
                DirectionSample3f ds(scene, si_bsdf, si);

                Float emitter_pdf =
                    dr::select(delta, 0.f, scene->pdf_emitter_direction(si, ds, active_b));

                result[active_b] +=
                    bsdf_val * tr * emitter_val *
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

    /// Compute a multiple importance sampling weight using the power heuristic
    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        Float w = pdf_a / (pdf_a + pdf_b);
        return dr::detach(dr::select(dr::isfinite(w), w, 0.f));
    }

    MI_DECLARE_CLASS(DirectIntegrator)
private:
    size_t m_emitter_samples;
    size_t m_bsdf_samples;
    ScalarFloat m_frac_bsdf, m_frac_lum;
    ScalarFloat m_weight_bsdf, m_weight_lum;

    MI_TRAVERSE_CB(Base, m_frac_bsdf, m_frac_lum, m_weight_bsdf, m_weight_lum)
};

MI_EXPORT_PLUGIN(DirectIntegrator)
NAMESPACE_END(mitsuba)
