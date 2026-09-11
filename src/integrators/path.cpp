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
     corresponds to :math:`\infty`). A value of 1 will only render directly
     visible light sources. 2 will lead to single-bounce (direct-only)
     illumination, and so on. (Default: -1)

 * - rr_depth
   - |int|
   - Specifies the path depth, at which the implementation will begin to use
     the *russian roulette* path termination criterion. For example, if set to
     1, then path generation may randomly cease after encountering directly
     visible surfaces. (Default: 5)

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

.. note:: This integrator does not handle participating media

.. tabs::
    .. code-tab::  xml
        :name: path-integrator

        <integrator type="path">
            <integer name="max_depth" value="8"/>
        </integrator>

    .. code-tab:: python

        'type': 'path',
        'max_depth': 8

 */

template <typename Float, typename Spectrum>
class PathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PathIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Bool> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const Ray3f &ray_,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Bool active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        if (unlikely(m_max_depth == 0))
            return { 0.f, false };

        // --------------------- Configure loop state ----------------------

        Ray3f ray                     = ray_;
        Spectrum throughput           = 1.f;
        Spectrum result               = 0.f;
        Float eta                     = 1.f;
        PreliminaryIntersection3f pi  = dr::zeros<PreliminaryIntersection3f>();
        Mask valid_ray                = false;
        BSDFContext bsdf_ctx;

        // Null tests fold to literals in scenes without null shapes
        bool has_null = scene->has_null_shapes();

        // State of the last scattering vertex of the path. Null crossings
        // are not path vertices and leave it unchanged.
        struct Vertex {
            Interaction3f si;
            Float bsdf_pdf;
            Bool bsdf_delta;
            UInt32 depth;
            UInt32 ray_mask;
            DRJIT_STRUCT(Vertex, si, bsdf_pdf, bsdf_delta, depth, ray_mask)
        } vertex = { dr::zeros<Interaction3f>(), 1.f, true, 0,
                     +RayMask::Primary };

        // Set up a Dr.Jit loop. This optimizes away to a normal loop in scalar
        // mode, and it generates either a megakernel (default) or
        // wavefront-style renderer in JIT variants. This can be controlled by
        // passing the '-W' command line flag to the mitsuba binary or
        // enabling/disabling the JitFlag.LoopRecord bit in Dr.Jit.
        struct LoopState {
            Ray3f ray;
            PreliminaryIntersection3f pi;
            Spectrum throughput;
            Spectrum result;
            Float eta;
            Mask valid_ray;
            Vertex vertex;
            Bool active;
            Sampler* sampler;

            DRJIT_STRUCT(LoopState, ray, pi, throughput, result, eta,
                         valid_ray, vertex, active, sampler)
        } ls = {
            ray,
            pi,
            throughput,
            result,
            eta,
            valid_ray,
            vertex,
            active,
            sampler
        };

        // First bounce is usually coherent - don't reorder threads. The
        // camera mask hides emitters marked as invisible.
        ls.pi = scene->ray_intersect_preliminary(ls.ray,
                                                 /* coherent = */ true,
                                                 +RayMask::Primary,
                                                 ls.active);

        dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
            [](const LoopState& ls) { return ls.active; },
            [this, scene, bsdf_ctx, has_null](LoopState& ls) {

            // dr::while_loop implicitly masks all code in the loop using the
            // 'active' flag, so there is no need to pass it to every function

            // Fill out all information of the interaction
            SurfaceInteraction3f si = scene->compute_surface_interaction(
                ls.ray, ls.pi, +RayFlags::Default);

            // ---------------------- Direct emission ----------------------

            // The emitter lookup uses the mask of the ray that produced 'si'
            EmitterPtr emitter = si.emitter(scene, ls.vertex.ray_mask);
            ls.valid_ray |= emitter != nullptr;

            if (dr::any_or<true>(emitter != nullptr)) {
                DirectionSample3f ds(scene, si, ls.vertex.si,
                                     ls.vertex.ray_mask);
                Float em_pdf = 0.f;

                if (dr::any_or<true>(!ls.vertex.bsdf_delta))
                    em_pdf = scene->pdf_emitter_direction(ls.vertex.si, ds,
                                                          !ls.vertex.bsdf_delta);

                // Compute MIS weight for emitter sample from previous bounce
                Float mis_bsdf = mis_weight(ls.vertex.bsdf_pdf, em_pdf);

                // Accumulate, being careful with polarization (see spec_fma)
                ls.result = spec_fma(ls.throughput,
                                     emitter->eval(si) * mis_bsdf, ls.result);
            }

            if (dr::none_or<false>(si.is_valid())) {
                ls.active = false;
                return; // early exit for scalar mode
            }

            BSDFPtr bsdf = si.bsdf();
            UInt32 bsdf_flags = bsdf->flags();

            // Continue tracing the path at this point? Null crossings don't
            // count when checking for the max_depth cutoff.
            Bool depth_ok  = ls.vertex.depth + 1 < m_max_depth,
                 can_cross = has_null ? has_flag(bsdf_flags, BSDFFlags::Null)
                                      : Mask(false),
                 active_next = si.is_valid() && (depth_ok || can_cross);

            if (dr::none_or<false>(active_next)) {
                ls.active = false;
                return; // early exit for scalar mode
            }

            // ---------------------- Emitter sampling ----------------------

            // Perform emitter sampling?
            Mask active_em = active_next && depth_ok &&
                             has_flag(bsdf_flags, BSDFFlags::Smooth);

            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            Spectrum em_weight = dr::zeros<Spectrum>();
            Vector3f wo = dr::zeros<Vector3f>();

            if (dr::any_or<true>(active_em)) {
                // Sample the emitter. The sample is detached, and its weight
                // is attached to the emitter and to the motion of 'si'.
                std::tie(ds, em_weight) = scene->sample_emitter_direction(
                    si, ls.sampler->next_2d(), true, active_em);
                active_em &= (ds.pdf != 0.f);

                wo = si.to_local(ds.d);
            }

            // ------ Evaluate BSDF * cos(theta) and sample direction -------

            Float sample_1 = ls.sampler->next_1d();
            Point2f sample_2 = ls.sampler->next_2d();

            auto [bsdf_val, bsdf_pdf, bsdf_sample, bsdf_weight]
                = bsdf->eval_pdf_sample(bsdf_ctx, si, wo, sample_1, sample_2);

            // --------------- Emitter sampling contribution ----------------

            if (dr::any_or<true>(active_em)) {
                bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                // Compute the MIS weight
                Float mis_em =
                    dr::select(ds.delta, 1.f, mis_weight(ds.pdf, bsdf_pdf));

                // Accumulate, being careful with polarization (see spec_fma)
                ls.result[active_em] = spec_fma(
                    ls.throughput, bsdf_val * em_weight * mis_em, ls.result);
            }

            // ---------------------- BSDF sampling ----------------------

            bsdf_weight = si.to_world_mueller(bsdf_weight, -bsdf_sample.wo, si.wi);

            ls.ray = si.spawn_ray(si.to_world(bsdf_sample.wo));

            // A null crossing is not a path vertex
            Mask scattered = has_null ? !bsdf_sample.is_null() : Mask(true);

            // When the path tracer is differentiated, we must be careful that
            // the generated Monte Carlo samples are detached (i.e. don't track
            // derivatives) to avoid bias resulting from the combination of moving
            // samples and discontinuous visibility. We need to re-evaluate the
            // BSDF differentiably with the detached sample in that case. A
            // null crossing keeps the attached weight of the null lobe.
            if (dr::grad_enabled(ls.ray)) {
                ls.ray = dr::detach(ls.ray);

                // Recompute 'wo' to propagate derivatives to cosine term
                Vector3f wo_2 = si.to_local(ls.ray.d);
                auto [bsdf_val_2, bsdf_pdf_2] = bsdf->eval_pdf(bsdf_ctx, si, wo_2);
                bsdf_weight[bsdf_pdf_2 > 0.f && scattered] =
                    bsdf_val_2 / dr::detach(bsdf_pdf_2);
            }

            // ------ Update loop variables based on current interaction ------

            ls.throughput *= bsdf_weight;
            ls.eta *= bsdf_sample.eta;

            ls.valid_ray |= scattered && si.is_valid();
            dr::masked(ls.vertex, scattered) = Vertex{
                Interaction3f(si), bsdf_sample.pdf,
                bsdf_sample.is_delta(),
                ls.vertex.depth + 1,
                +RayMask::Secondary };

            // -------------------- Stopping criterion ---------------------

            Float throughput_max = dr::max(unpolarized_spectrum(ls.throughput));

            Float rr_prob = dr::minimum(throughput_max * dr::square(ls.eta), .95f);
            Mask rr_active = ls.vertex.depth >= m_rr_depth,
                 rr_continue = ls.sampler->next_1d() < rr_prob;

            // Differentiable variants of the renderer require the russian
            // roulette sampling weight to be detached to avoid bias. This is a
            // no-op in non-differentiable variants.
            ls.throughput[rr_active] *= dr::rcp(dr::detach(rr_prob));

            ls.active = active_next && (depth_ok || !scattered) &&
                        (!rr_active || rr_continue) && (throughput_max != 0.f);

            // Reorder threads based on the shape they hit.
            ls.pi = scene->ray_intersect_preliminary(ls.ray,
                                                     /* coherent = */ false,
                                                     ls.vertex.ray_mask,
                                                     ls.active,
                                                     /* reorder = */ jit_flag(JitFlag::LoopRecord));
        });

        return {
            /* spec  = */ dr::select(ls.valid_ray, ls.result, 0.f),
            /* valid = */ ls.valid_ray
        };
    }

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
        return dr::detach(dr::select(dr::isfinite(w), w, 0.f));
    }

    /**
     * Perform a Mueller matrix multiplication in polarized modes, and a
     * fused multiply-add otherwise.
     */
    Spectrum spec_fma(const Spectrum &a, const Spectrum &b,
                      const Spectrum &c) const {
        if constexpr (is_polarized_v<Spectrum>)
            return a * b + c;
        else
            return dr::fmadd(a, b, c);
    }

    MI_DECLARE_CLASS(PathIntegrator)
};

MI_EXPORT_PLUGIN(PathIntegrator)
NAMESPACE_END(mitsuba)
