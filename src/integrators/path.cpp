#include <random>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-path:

Path tracer (:monosp:`path`)
----------------------------

.. pluginparameters::

 * - max_depth
   - |int|
   - Specifies the longest path depth in the generated output image (where -1
     corresponds to :math:`\infty`). A value of |1| will only render directly
     visible light sources. |2| will lead to single-bounce (direct-only)
     illumination, and so on. (Default: |-1|)
 * - rr_depth
   - |int|
   - Specifies the path depth, at which the implementation will begin to use
     the *russian roulette* path termination criterion. For example, if set to
     |1|, then path generation many randomly cease after encountering directly
     visible surfaces. (Default: |5|)
 * - hide_emitters
   - |bool|
   - Hide directly visible emitters. (Default: no, i.e. |false|)

This integrator implements a basic path tracer and is a **good default choice**
when there is no strong reason to prefer another method.

To use the path tracer appropriately, it is instructive to know roughly how
it works: its main operation is to trace many light paths using *random walks*
starting from the sensor. A single random walk is shown below, which entails
casting a ray associated with a pixel in the output image and searching for
the first visible intersection. A new direction is then chosen at the intersection,
and the ray-casting step repeats over and over again (until one of several
stopping criteria applies).

.. image:: ../../resources/data/docs/images/integrator/integrator_path_figure.png
    :width: 95%
    :align: center

At every intersection, the path tracer tries to create a connection to
the light source in an attempt to find a *complete* path along which
light can flow from the emitter to the sensor. This of course only works
when there is no occluding object between the intersection and the emitter.

This directly translates into a category of scenes where a path tracer can be
expected to produce reasonable results: this is the case when the emitters are
easily "accessible" by the contents of the scene. For instance, an interior
scene that is lit by an area light will be considerably harder to render when
this area light is inside a glass enclosure (which effectively counts as an
occluder).

Like the :ref:`direct <integrator-direct>` plugin, the path tracer internally
relies on multiple importance sampling to combine BSDF and emitter samples. The
main difference in comparison to the former plugin is that it considers light
paths of arbitrary length to compute both direct and indirect illumination.

.. _sec-path-strictnormals:

.. Commented out for now
.. Strict normals
   --------------

.. Triangle meshes often rely on interpolated shading normals
   to suppress the inherently faceted appearance of the underlying geometry. These
   "fake" normals are not without problems, however. They can lead to paradoxical
   situations where a light ray impinges on an object from a direction that is
   classified as "outside" according to the shading normal, and "inside" according
   to the true geometric normal.

.. The :paramtype:`strict_normals` parameter specifies the intended behavior when such cases arise. The
   default (|false|, i.e. "carry on") gives precedence to information given by the shading normal and
   considers such light paths to be valid. This can theoretically cause light "leaks" through
   boundaries, but it is not much of a problem in practice.

.. When set to |true|, the path tracer detects inconsistencies and ignores these paths. When objects
   are poorly tesselated, this latter option may cause them to lose a significant amount of the
   incident radiation (or, in other words, they will look dark).

.. note:: This integrator does not handle participating media

 */

template <typename Float, typename Spectrum>
class PathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth)
    MTS_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PathIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Bool> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Bool active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        if (unlikely(m_max_depth == 0))
            return { 0.f, false };

        // --------------------- Configure loop state ----------------------

        Ray3f ray                     = Ray3f(ray_);
        Spectrum throughput           = 1.f;
        Spectrum result               = 0.f;
        Float eta                     = 1.f;
        UInt32 depth                  = 0;

        // Variables caching information from the previous bounce
        Interaction3f prev_si         = ek::zero<Interaction3f>();
        Float         prev_bsdf_pdf   = 1.f;
        Bool          prev_bsdf_delta = true;
        BSDFContext   bsdf_ctx;

        /* Set up an Enoki loop. This optimizes away to a normal loop in scalar
           mode, and it generates either a a megakernel (default) or
           wavefront-style renderer in JIT variants. This can be controlled by
           passing the '-W' command line flag to the mitsuba binary or
           enabling/disabling the JitFlag.LoopRecord bit in Enoki-JIT.

           The first argument identifies the loop by name, which is helpful for
           debugging. The subsequent list registers all variables that encode
           the loop state variables. This is crucial: omitting a variable may
           lead to undefined behavior. */
        ek::Loop<Bool> loop("Path Tracer", sampler, ray, throughput, result, eta,
                            depth, prev_si, prev_bsdf_pdf, prev_bsdf_delta, active);

        /* Inform the loop about the maximum number of loop iterations.
           This accelerates wavefront-style rendering by avoiding costly
           synchronization points that check the 'active' flag. */
        loop.set_max_iterations(m_max_depth);

        while (loop(active)) {
            /* ek::Loop implicitly masks all code in the loop using the 'active'
               flag, so there is no need to pass it to every function */

            SurfaceInteraction3f si =
                scene->ray_intersect(ray,
                                     /* ray_flags = */ +RayFlags::All,
                                     /* coherent = */ ek::eq(depth, 0u));

            /* ek.none_or<false> returns false in JIT variants (i.e. the test is
               skipped), and it enables a potential early exit in scalar
               variants */
            if (ek::none_or<false>(si.is_valid()))
                break;

            // ---------------------- Direct emission ----------------------

            if (ek::any_or<true>(si.shape->is_emitter())) {
                DirectionSample3f ds(scene, si, prev_si);
                Float em_pdf = 0;

                if (ek::any_or<true>(!prev_bsdf_delta))
                    em_pdf = scene->pdf_emitter_direction(prev_si, ds,
                                                          !prev_bsdf_delta);

                // Compute MIS weight for emitter sample from previous bounce
                Float mis_bsdf = mis_weight(prev_bsdf_pdf, em_pdf);

                // Accumulate, being careful with polarization (see spec_fma)
                result = spec_fma(
                    throughput,
                    ds.emitter->eval(si, prev_bsdf_pdf > 0.f) * mis_bsdf,
                    result);
            }

            BSDFPtr bsdf = si.bsdf(ray);

            // Perform emitter sampling?
            Bool active_next = (depth + 1 < m_max_depth) && si.is_valid(),
                 active_em   = active_next && has_flag(bsdf->flags(), BSDFFlags::Smooth);

            if (ek::none_or<false>(active_next))
                break; // early exit for scalar mode

            if (ek::any_or<true>(active_em)) {
                // Sample the emitter
                auto [ds, emitter_val] = scene->sample_emitter_direction(
                    si, sampler->next_2d(), true, active_em);
                active_em &= ek::neq(ds.pdf, 0.f);

                // Evalute BRDF * cos(theta)
                Vector3f wo = si.to_local(ds.d);
                auto [bsdf_val, bsdf_pdf] =
                    bsdf->eval_pdf(bsdf_ctx, si, wo, active_em);
                bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                // Compute the MIS weight
                Float mis_em =
                    mis_weight(ek::select(ds.delta, 0.f, ds.pdf), bsdf_pdf);

                // Accumulate, being careful with polarization (see spec_fma)
                result = spec_fma(throughput, bsdf_val * emitter_val * mis_em, result);
            }

            // ---------------------- BSDF sampling ----------------------

            Float sample_1 = sampler->next_1d();
            Point2f sample_2 = sampler->next_2d();

            auto [bsdf_sample, bsdf_weight] =
                bsdf->sample(bsdf_ctx, si, sample_1, sample_2, active_next);
            bsdf_weight = si.to_world_mueller(bsdf_weight, -bsdf_sample.wo, si.wi);

            // ---- Update loop variables based on current interaction -----
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo));
            eta *= bsdf_sample.eta;
            throughput = throughput * bsdf_weight;

            // Information about the current vertex needed by the next iteration
            prev_si = si;
            prev_bsdf_pdf = bsdf_sample.pdf;
            prev_bsdf_delta = has_flag(bsdf_sample.sampled_type, BSDFFlags::Delta);

            // -------------------- Stopping criterion ---------------------

            ek::masked(depth, si.is_valid()) += 1;

            Float throughput_max = ek::hmax(unpolarized_spectrum(throughput));

            Float rr_prob = ek::min(throughput_max * ek::sqr(eta), .95f);
            Mask rr_active = depth >= m_rr_depth,
                 rr_continue = sampler->next_1d() < rr_prob;

            /* Differentiable variants of the renderer require the the russian
               roulette sampling weight to be detached to avoid bias. This is a
               no-op in non-differentiable variants. */
            throughput[rr_active] *= ek::rcp(ek::detach(rr_prob));

            active = active_next && (!rr_active || rr_continue) &&
                     ek::neq(throughput_max, 0.f);
        }

        return {
            /* spec  = */ result,
            /* valid = */ ek::neq(depth, 0u)
        };
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("PathIntegrator[\n"
            "  max_depth = %u,\n"
            "  rr_depth = %u\n"
            "]", m_max_depth, m_rr_depth);
    }

    /// Compute a multiple importance sampling weight using the power heuristic
    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        Float w = pdf_a / (pdf_a + pdf_b);
        return ek::select(ek::isfinite(w), w, 0.f);
    }

    /**
     * \brief Perform a Mueller matrix multiplication in polarized modes, and a
     * fused multiply-add otherwise.
     */
    Spectrum spec_fma(const Spectrum &a, const Spectrum &b,
                      const Spectrum &c) const {
        if constexpr (is_polarized_v<Spectrum>)
            return a * b + c;
        else
            return ek::fmadd(a, b, c);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(PathIntegrator, MonteCarloIntegrator)
MTS_EXPORT_PLUGIN(PathIntegrator, "Path Tracer integrator");
NAMESPACE_END(mitsuba)
