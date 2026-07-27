// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// needed for render() and render_sample():
#include <nanothread/nanothread.h>
#include <mutex>
#include <mitsuba/core/progress.h>
// needed for sample():
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-acoustic_path:

Acoustic Path Tracer (:monosp:`acoustic_path`)
-----------------------------------------------

.. pluginparameters::

 * - speed_of_sound
   - |float|
   - Speed of sound in meters per second. (Default: 343.0)

 * - max_time
   - |float|
   - Stopping criterion for the maximum propagation time in seconds.
     Paths whose accumulated travel distance exceeds ``max_time *
     speed_of_sound`` are terminated.

 * - max_depth
   - |int|
   - Specifies the longest path depth (where -1
     corresponds to :math:`\infty`). A value of 1 will only render directly
     audible sound sources. 2 will lead to first-order reflections, and so on.
     (Default: -1)

 * - rr_depth
   - |int|
   - Russian roulette path termination is not yet supported. Setting this
     to any value other than the default raises an error. Use
     ``max_energy_loss`` instead. (Default: 100000)

 * - hide_emitters
   - |bool|
   - Hide directly visible emitters, i.e. skip the direct (line-of-sight)
     contribution from sound sources. (Default: no, i.e. |false|)

 * - max_energy_loss
   - |float|
   - Maximum energy loss in dB before a path is terminated. When the path
     throughput drops below the corresponding threshold, the path is stopped.
     Set to -1 to disable this criterion. (Default: 60.0)

This integrator implements an acoustic path tracer that simulates sound
propagation in a scene by tracing paths from the sensor (microphone) to
the emitters (sound sources). It computes an energy-based impulse response
(echogram) by accumulating path contributions into time bins determined by the
total path length and the speed of sound.

At each surface interaction, the integrator uses multiple importance sampling
(MIS) to combine BSDF and emitter samples, analogous to the optical
:ref:`path tracer <integrator-path>`. The key difference is that energy
transport is not assumed to be instantaneous, but at the speed of sound. Instead
of producing an image, the output is stored in a ``Tape``, where the first axis
corresponds to frequency bins and the second axis to time bins.

Sound paths are terminated when any of the following conditions are met:

- The maximum path depth (``max_depth``) is reached.
- The accumulated path distance exceeds ``max_time * speed_of_sound``.
- The path throughput drops below the energy loss threshold (``max_energy_loss``).

.. note:: This integrator does not handle participating media or polarized
   rendering. It requires a ``Microphone`` sensor with a ``Tape`` film type.

.. tabs::
    .. code-tab::  xml
        :name: acoustic-path-integrator

        <integrator type="acoustic_path">
            <float name="max_time" value="1.0"/>
            <float name="speed_of_sound" value="343.0"/>
            <integer name="max_depth" value="-1"/>
        </integrator>

    .. code-tab:: python

        'type': 'acoustic_path',
        'max_time': 1.0,
        'speed_of_sound': 343.0,
        'max_depth': -1,

 */

template <typename Float, typename Spectrum>
class AcousticPathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_samples_per_pass, m_max_depth, m_rr_depth, m_hide_emitters, m_render_timer, m_stop, m_timeout)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr, Sensor, Film, ImageBlock)

    AcousticPathIntegrator(const Properties &props) : Base(props) {
        Log(Debug, "Loading acoustic Path Integrator ..");

        m_max_time    = props.get<float>("max_time");
        m_speed_of_sound = props.get<float>("speed_of_sound", 343.f);
        if (m_max_time <= 0.f || m_speed_of_sound <= 0.f)
            Throw("\"max_time\" and \"speed_of_sound\" must be set to a value greater than zero!");

        int max_depth = props.get<int>("max_depth", -1);
        if (max_depth < 0 && max_depth != -1)
            Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");

        m_max_depth = (uint32_t) max_depth; // This maps -1 to 2^32-1 bounces

        // Depth to begin using russian roulette
        int rr_depth = props.get<int>("rr_depth", 100000);
        if (rr_depth <= 0)
            Throw("\"rr_depth\" must be set to a value greater than zero!");
        if (rr_depth != 100000)
            Throw("Russian Roulette is not yet implemented! Please use energy loss stopping criterion instead.");
        m_rr_depth = (uint32_t) rr_depth;

        float max_energy_loss = props.get<float>("max_energy_loss", 60.f);
        if (max_energy_loss <= 0.f && max_energy_loss != -1.f)
            Throw("\"max_energy_loss\" must be set to -1 (disabled) or a value > 0 (in dB)");
        // When -1, disable the criterion by using a threshold of 0
        m_throughput_threshold = (max_energy_loss == -1.f)
            ? 0.f
            : dr::pow(10.f, -max_energy_loss / 10.f);
    }

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    UInt32 seed,
                    uint32_t spp,
                    bool develop,
                    bool evaluate) override {
        ScopedPhase sp(ProfilerPhase::Render);
        Log(Debug, "Running render() ..");
        m_stop = false;

        Film *film = sensor->film();
        ScalarVector2u film_size = film->crop_size();
        if (film->sample_border())
            Throw("sample_border not compatible with acoustic rendering.");

        // Potentially adjust the number of samples per pixel if spp != 0
        Sampler *sampler = sensor->sampler();
        if (spp)
            sampler->set_sample_count(spp);
        spp = sampler->sample_count();

        uint32_t spp_per_pass = (m_samples_per_pass == (uint32_t) -1)
                                    ? spp
                                    : std::min(m_samples_per_pass, spp);


        if ((spp % spp_per_pass) != 0)
            Throw("sample_count (%d) must be a multiple of spp_per_pass (%d).",
                  spp, spp_per_pass);

        uint32_t n_passes = spp / spp_per_pass;


        // Determine output channels and prepare the film with this information
        size_t n_channels = film->prepare({});
        Log(Debug, "Rendering with %u channel%s", n_channels, n_channels == 1 ? "" : "s");

        if (has_flag(sensor->film()->flags(), FilmFlags::Spectral)) {
            NotImplementedError("Rendering in continuous spectral bands not implemented.");
        } else {
            Log(Debug, "No spectral film detected, iterating through wavelength bins.");
        }

        // Start the render timer (used for timeouts & log messages)
        m_render_timer.reset();

        ScalarFloat sample_scale = 1.f / (ScalarFloat) spp;

        TensorXf result;
        if constexpr (!dr::is_jit_v<Float>) {
            // Render on the CPU using a spiral pattern
            uint32_t n_threads = (uint32_t) (pool_size() + 1);

            Log(Info, "Starting render job (%ux%u, %u sample%s,%s %u thread%s)",
                film_size[0], film_size.y(), spp, spp == 1 ? "" : "s",
                n_passes > 1 ? tfm::format(" %u passes,", n_passes) : "", n_threads,
                n_threads == 1 ? "" : "s");

            if (m_timeout > 0.f)
                Log(Info, "Timeout specified: %.2f seconds.", m_timeout);

            std::mutex mutex;
            ref<ProgressReporter> progress;
            Logger* logger = mitsuba::Thread::thread()->logger();
            if (logger && Info >= logger->log_level())
                progress = new ProgressReporter("Rendering");

            // Total number of blocks to be handled, including multiple passes.
            uint32_t total_blocks = film_size[0] * n_passes,
                     blocks_done = 0;
            Log(Debug, "Total blocks: %u", total_blocks);

            // Avoid overlaps in RNG seeding RNG when a seed is manually specified
            seed *= dr::prod(film_size);

            dr::parallel_for(
                dr::blocked_range<uint32_t>(0, total_blocks, 1),
                [&](const dr::blocked_range<uint32_t> &range) {
                    // Fork a non-overlapping sampler for the current worker
                    ref<Sampler> sampler = sensor->sampler()->fork();

                    ref<ImageBlock> block = film->create_block(
                        ScalarVector2u(1, film_size.y())  /* size */,
                        false /* normalize */,
                        false /* border */);
                    Log(Trace, "ImageBlock allocated: %s", block);

                    std::unique_ptr<Float[]> aovs(new Float[n_channels]);

                    for (uint32_t i = range.begin(); i != range.end(); ++i) {
                        sampler->seed(seed * i);

                        /* The first index is the frequency index, the second
                        index (time) is unused. */
                        Vector2i pos(i / n_passes, 0);

                        block->set_offset(ScalarPoint2u(pos[0], 0));

                        if constexpr (dr::is_array_v<Float>) {
                            Throw("Not implemented for JIT arrays.");
                        } else {
                            block->clear();

                            for (uint32_t j = 0; j < spp_per_pass; ++j) {
                                Log(Debug, "Rendering sample %u of %u", j+1, spp_per_pass);
                                render_sample(scene, sensor, film, sampler, block, aovs.get(), pos);
                                sampler->advance();
                            }
                        }

                        block->tensor() = block->tensor() * sample_scale;

                        film->put_block(block);

                        /* Critical section: update progress bar */
                        if (progress) {
                            std::lock_guard<std::mutex> lock(mutex);
                            blocks_done++;
                            progress->update(blocks_done / (float) total_blocks);
                        }
                    }
                }
            );

            if (develop)
                result = film->develop();
         } else {
            //                               freq_bins    * samples per pixel
            size_t wavefront_size = (size_t) film_size[0] * (size_t) spp_per_pass,
                   wavefront_size_limit = 0xffffffffu;

            if (wavefront_size > wavefront_size_limit) {
                spp_per_pass /=
                    (uint32_t)((wavefront_size + wavefront_size_limit - 1) /
                               wavefront_size_limit);
                n_passes       = spp / spp_per_pass;
                wavefront_size = (size_t) film_size[0] * (size_t) spp_per_pass;

                Log(Warn,
                    "The requested rendering task involves %zu Monte Carlo "
                    "samples, which exceeds the upper limit of 2^32 = 4294967296 "
                    "for this variant. Mitsuba will instead split the rendering "
                    "task into %zu smaller passes to avoid exceeding the limits.",
                    wavefront_size, n_passes);
            }

            dr::sync_thread(); // Separate from scene initialization (for timings)

            Log(Info, "Starting render job (%ux%u, %u sample%s%s)",
                film_size[0], film_size.y(), spp, spp == 1 ? "" : "s",
                n_passes > 1 ? tfm::format(", %u passes", n_passes) : "");

            if (n_passes > 1 && !evaluate) {
                Log(Warn, "render(): forcing 'evaluate=true' since multi-pass "
                          "rendering was requested.");
                evaluate = true;
            }

            // Inform the sampler about the passes (needed in vectorized modes)
            sampler->set_samples_per_wavefront(spp_per_pass);

            // Seed the underlying random number generators, if applicable
            sampler->seed(seed, (uint32_t) wavefront_size);

            // Allocate a large image block that will receive the entire rendering
            ref<ImageBlock> block = film->create_block();
            block->set_offset(film->crop_offset());

            UInt32 idx = dr::arange<UInt32>((uint32_t) wavefront_size);

            // Try to avoid a division by an unknown constant if we can help it
            uint32_t log_spp_per_pass = dr::log2i(spp_per_pass);
            if ((1u << log_spp_per_pass) == spp_per_pass)
                idx >>= dr::opaque<UInt32>(log_spp_per_pass);
            else
                idx /= dr::opaque<UInt32>(spp_per_pass);

            Vector2u pos(idx, 0 * idx);

            Timer timer;
            std::unique_ptr<Float[]> aovs(new Float[n_channels]);

            // Potentially render multiple passes
            for (size_t i = 0; i < n_passes; i++) {
                render_sample(scene, sensor, film, sampler, block, aovs.get(), pos);

                if (n_passes > 1) {
                    sampler->advance(); // Will trigger a kernel launch of size 1
                    sampler->schedule_state();
                    dr::eval(block->tensor());
                }
            }

            block->tensor() = block->tensor() * sample_scale;

            film->put_block(block);

            if (n_passes == 1 && jit_flag(JitFlag::VCallRecord) &&
                jit_flag(JitFlag::LoopRecord)) {
                Log(Info, "Computation graph recorded. (took %s)",
                    util::time_string((float) timer.reset(), true));
            }

            if (develop) {
                result = film->develop();
                dr::schedule(result);
            } else {
                film->schedule_storage();
            }

            if (evaluate) {
                dr::eval();

                if (n_passes == 1 && jit_flag(JitFlag::VCallRecord) &&
                    jit_flag(JitFlag::LoopRecord)) {
                    Log(Info, "Code generation finished. (took %s)",
                        util::time_string((float) timer.value(), true));

                    /* Separate computation graph recording from the actual
                       rendering time in single-pass mode */
                    m_render_timer.reset();
                }

                dr::sync_thread();
            }
        }

        if (!m_stop && (evaluate || !dr::is_jit_v<Float>))
            Log(Info, "Rendering finished. (took %s)",
                util::time_string((float) m_render_timer.value(), true));

        return result;
    }

    // default function signature for proper inheritance
    std::pair<Spectrum, Bool> sample(const Scene *,
                                     Sampler *,
                                     const RayDifferential3f &,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Bool /*active */) const override {
        NotImplementedError("AcousticPathIntegrator::sample default arguments");
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const Film *film,
                                     const Vector2f &pos,
                                     const RayDifferential3f &ray_,
                                     ImageBlock *block,
                                     Float *aovs /* this stores the values that are put into the ImageBlock, see film::prepare_sample() */,
                                     Bool active) const {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);
        Log(Debug, "Running acoustic_path::sample() ..");

        if (unlikely(m_max_depth == 0))
            return { 0.f, false };

        // --------------------- Configure loop state ----------------------

        Ray3f ray                      = Ray3f(ray_);
        Spectrum throughput            = 1.f;
        Float eta                      = 1.f;
        UInt32 depth                   = 0;
        Float distance                 = 0.f;
        const ScalarFloat max_distance = m_max_time * m_speed_of_sound;

        // If m_hide_emitters == true, directly visible emitters are hidden
        Mask valid_ray                 = !m_hide_emitters;

        // Variables caching information from the previous bounce
        Interaction3f prev_si          = dr::zeros<Interaction3f>();
        Float         prev_bsdf_pdf    = 1.f;
        Bool          prev_bsdf_delta  = true;
        BSDFContext   bsdf_ctx;

        /* Set up a Dr.Jit loop. This optimizes away to a normal loop in scalar
           mode, and it generates either a a megakernel (default) or
           wavefront-style renderer in JIT variants. This can be controlled by
           passing the '-W' command line flag to the mitsuba binary or
           enabling/disabling the JitFlag.LoopRecord bit in Dr.Jit.

           The first argument identifies the loop by name, which is helpful for
           debugging. The subsequent list registers all variables that encode
           the loop state variables. This is crucial: omitting a variable may
           lead to undefined behavior. */
        // TODO: should loop keep track of imageblock data?
        struct LoopState {
            Ray3f ray;
            Spectrum throughput;
            Float eta;
            UInt32 depth;
            Float distance;
            Float time_bin;
            Mask valid_ray;
            Interaction3f prev_si;
            Float prev_bsdf_pdf;
            Bool prev_bsdf_delta;
            Bool active;
            Sampler* sampler;

            DRJIT_STRUCT(LoopState, ray, throughput, eta, depth, distance, \
                time_bin, valid_ray, prev_si, prev_bsdf_pdf, prev_bsdf_delta,
                active, sampler)
        } ls = {
            ray,
            throughput,
            eta,
            depth,
            distance,
            0.f, /* time_bin */
            valid_ray,
            prev_si,
            prev_bsdf_pdf,
            prev_bsdf_delta,
            active,
            sampler
        };

        dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
            [](const LoopState& ls) { return ls.active; },
            [this, scene, bsdf_ctx, block, aovs, pos, ray, film, max_distance](LoopState& ls) {

            Float tau     = 0;
            Float tau_dir = 0;

            Log(Debug, "Starting loop.");
            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "Tracing ray with origin %s m, direction %s, frequency %f Hz.", ray.o, ray.d, ray.wavelengths);
            /* dr::while_loop implicitly masks all code in the loop using the
               'active' flag, so there is no need to pass it to every function */

            SurfaceInteraction3f si =
                scene->ray_intersect(ls.ray,
                                     /* ray_flags = */ +RayFlags::All,
                                     /* coherent = */ ls.depth == 0u);

            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "Intersection found with distance %f m.", si.t);

            /*
            Calculate path segment length. spawn_ray() offsets rays by a small
            epsilon to prevent self-intersection. This moves the origin towards
            the intersection point and reduces si.t slightly.
            Use true geometric distance instead:
            */
            tau = dr::select(ls.depth == 0u,
                                dr::norm(si.p - ls.ray.o),
                                dr::norm(si.p - ls.prev_si.p)
                            );

            // ---------------------- Direct emission ----------------------

            /* dr::any_or() checks for active entries in the provided boolean
               array. JIT/Megakernel modes can't do this test efficiently as
               each Monte Carlo sample runs independently. In this case,
               dr::any_or<..>() returns the template argument (true) which means
               that the 'if' statement is always conservatively taken. */
            if (dr::any_or<true>(si.emitter(scene) != nullptr)) {
                DirectionSample3f ds(scene, si, ls.prev_si);
                Float em_pdf = 0.f;

                if (dr::any_or<true>(!ls.prev_bsdf_delta))
                    em_pdf = scene->pdf_emitter_direction(ls.prev_si, ds,
                                                          !ls.prev_bsdf_delta);

                // Compute MIS weight for emitter sample from previous bounce
                Float mis_bsdf = mis_weight(ls.prev_bsdf_pdf, em_pdf);

                ls.time_bin = ((ls.distance + tau) / max_distance) * block->size()[1];
                Float result = (ls.throughput * ds.emitter->eval(si, ls.prev_bsdf_pdf > 0.f) * mis_bsdf)[0];

                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "ls.throughput: %f, result = %s", ls.throughput, result);

                if (likely(has_flag(film->flags(), FilmFlags::Special))) {
                    film->prepare_sample(result, ray.wavelengths, aovs,
                        /*weight*/ 1.f,
                        /*alpha is ignored*/ 1.f,
                        /*Mask is ignored*/ true);
                } else {
                    Throw("AcousticPathIntegrator only supports Tape and SpecTape films");
                }

                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "valid_ray: %s", ls.valid_ray);
                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "putting value %f into block at position [%f, %f]", ls.valid_ray ? aovs[0] : 0.f , pos[0], ls.time_bin);

                block->put({ pos[0], ls.time_bin }, aovs, result > 0.f && ls.valid_ray && ls.active);
            }

            // Continue tracing the path at this point?

            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "si.is_valid() = %s", si.is_valid());
            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "ls.distance <= max_distance = %s", ls.distance <= max_distance);
            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "ls.depth < m_max_depth = %s", ls.depth < m_max_depth);
            Bool active_next = si.is_valid() && ls.distance <= max_distance
                            && (ls.depth + 1 < m_max_depth);
            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "Continue tracing the path at this point? %s", active_next);

            if (dr::none_or<false>(active_next)) {
                ls.active = active_next;
                return; // early exit for scalar mode
            }

            BSDFPtr bsdf = si.bsdf(ls.ray);

            // ---------------------- Emitter sampling ----------------------

            // Perform emitter sampling?
            Mask active_em = active_next && has_flag(bsdf->flags(), BSDFFlags::Smooth);
            if constexpr (!dr::is_jit_v<Float>) {
                Log(Trace, "active_next = %s, BSDFFlags::DiffuseReflection? %s, BSDFFlags::Smooth? %s",
                    active_next,
                    has_flag(bsdf->flags(), BSDFFlags::DiffuseReflection),
                    has_flag(bsdf->flags(), BSDFFlags::Smooth));
                Log(Trace, "Perform Emitter sampling? %s", active_em);
            }

            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            Spectrum em_weight = dr::zeros<Spectrum>();
            Vector3f wo = dr::zeros<Vector3f>();

            if (dr::any_or<true>(active_em)) {
                // Sample the emitter
                std::tie(ds, em_weight) = scene->sample_emitter_direction(
                    si, ls.sampler->next_2d(), true, active_em);
                active_em &= (ds.pdf != 0.f);

                /* Given the detached emitter sample, recompute its contribution
                   with AD to enable light source optimization. */
                if (dr::grad_enabled(si.p)) {
                    ds.d = dr::normalize(ds.p - si.p);
                    Spectrum em_val = scene->eval_emitter_direction(si, ds, active_em);
                    em_weight = dr::select(ds.pdf != 0, em_val / ds.pdf, 0);
                }

                wo = si.to_local(ds.d);
            }

            // ------ Evaluate BSDF * cos(theta) and sample direction -------

            Float sample_1 = ls.sampler->next_1d();
            Point2f sample_2 = ls.sampler->next_2d();

            auto [bsdf_val, bsdf_pdf, bsdf_sample, bsdf_weight]
                = bsdf->eval_pdf_sample(bsdf_ctx, si, wo, sample_1, sample_2);

            // --------------- Emitter sampling contribution ----------------

            if constexpr (!dr::is_jit_v<Float>) Log(Trace, "calculating Emitter sampling contribution at frequency %f Hz.",si.wavelengths);
            if (dr::any_or<true>(active_em)) {
                bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);
                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "bsdf_val: %s", bsdf_val);

                // Compute the MIS weight
                Float mis_em =
                    dr::select(ds.delta, 1.f, mis_weight(ds.pdf, bsdf_pdf));
                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "mis_em: %f", mis_em);


                if constexpr (!dr::is_jit_v<Float>) Log(Trace,"block->size(): %s,  block->size()[0]: %f, block->size()[1]: %f",
                        block->size(), block->size()[0], block->size()[1]);

                tau_dir = dr::norm(ds.p - si.p);
                ls.time_bin = ((ls.distance + tau + tau_dir) / max_distance) * block->size()[1];
                if constexpr (!dr::is_jit_v<Float>) Log(Trace,
                    "ls.distance: %f, max_distance: %f, time bin: %f.",
                    ls.distance, max_distance, ls.time_bin);
                Float result = (ls.throughput * bsdf_val * em_weight * mis_em)[0];
                active_em &= result > 0.f;
                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "result: %f, active_em: %s",
                    result, active_em);

                if (likely(has_flag(film->flags(), FilmFlags::Special))) {
                    film->prepare_sample(result, ray.wavelengths, aovs,
                                        /*weight*/ 1.f,
                                        /*alpha */ dr::select(active_em, Float(1.f), Float(0.f)),
                                        active_em);
                } else {
                    Throw("AcousticPathIntegrator only supports Tape and SpecTape films");
                }
                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "valid_ray: %s, active_em: %s", ls.valid_ray, active_em);
                if constexpr (!dr::is_jit_v<Float>) Log(Trace, "putting value %f into block at position [%f, %f]", ls.valid_ray ? aovs[0] : 0.f , pos[0], ls.time_bin);
                block->put({ pos[0], ls.time_bin }, aovs, result > 0.f && ls.valid_ray && active_em);
            }

            // ---------------------- BSDF sampling ----------------------

            bsdf_weight = si.to_world_mueller(bsdf_weight, -bsdf_sample.wo, si.wi);

            ls.ray = si.spawn_ray(si.to_world(bsdf_sample.wo));

            /* When the path tracer is differentiated, we must be careful that
               the generated Monte Carlo samples are detached (i.e. don't track
               derivatives) to avoid bias resulting from the combination of moving
               samples and discontinuous visibility. We need to re-evaluate the
               BSDF differentiably with the detached sample in that case. */
            if (dr::grad_enabled(ls.ray)) {
                ls.ray = dr::detach<true>(ls.ray);

                // Recompute 'wo' to propagate derivatives to cosine term
                Vector3f wo_2 = si.to_local(ls.ray.d);
                auto [bsdf_val_2, bsdf_pdf_2] = bsdf->eval_pdf(bsdf_ctx, si, wo_2, ls.active);
                bsdf_weight[bsdf_pdf_2 > 0.f] = bsdf_val_2 / dr::detach(bsdf_pdf_2);
            }

            // ------ Update loop variables based on current interaction ------

            ls.throughput *= bsdf_weight;
            ls.eta *= bsdf_sample.eta;
            ls.distance += tau;
            ls.valid_ray |= ls.active && si.is_valid() &&
                         !has_flag(bsdf_sample.sampled_type, BSDFFlags::Null);

            // Information about the current vertex needed by the next iteration
            ls.prev_si = si;
            ls.prev_bsdf_pdf = bsdf_sample.pdf;
            ls.prev_bsdf_delta = has_flag(bsdf_sample.sampled_type, BSDFFlags::Delta);

            // -------------------- Stopping criterion ---------------------

            Float throughput_max = dr::max(unpolarized_spectrum(ls.throughput));

            active_next &= (throughput_max != 0.f);
            active_next &= throughput_max >= m_throughput_threshold;
            active_next &= ls.distance <= max_distance;

            // Russian roulette stopping probability (must cancel out ior^2
            // to obtain unitless throughput, enforces a minimum probability)
            Float rr_prob = dr::minimum(throughput_max * dr::square(ls.eta), .95f);
            Mask rr_active = ls.depth >= m_rr_depth,
                 rr_continue = ls.sampler->next_1d() < rr_prob;

            /* Differentiable variants of the renderer require the the russian
               roulette sampling weight to be detached to avoid bias. This is a
               no-op in non-differentiable variants. */
            ls.throughput[rr_active] *= dr::rcp(dr::detach(rr_prob));

            active_next &= (!rr_active || rr_continue);

            dr::masked(ls.depth, si.is_valid()) += 1;
            ls.active = active_next;
            if constexpr (!dr::is_jit_v<Float>) {
                Log(Trace, "active_next: %s, rr_active: %s, rr_continue: %s, throughput_max: %f, distance: %f, max_distance: %f, active: %s",
                    active_next, rr_active, rr_continue, throughput_max, ls.distance, max_distance, ls.active);
                Log(Trace, "Perform next iteration? %s", ls.active);
            }
        });

            Log(Trace, "max: %s, mean: %s, var: %s",
                dr::max(depth), dr::mean(depth), dr::mean(dr::square(depth - dr::mean(depth))));

        return {
            /* spec  = */ dr::select(ls.valid_ray, ls.throughput, 0.f),
            /* valid = */ ls.valid_ray
        };
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AcousticPathIntegrator["
            << "\n  speed_of_sound = " << m_speed_of_sound
            << "\n  max_time = " << m_max_time << "\n  max_depth = "
            << (m_max_depth == (uint32_t) -1 ? std::string("disabled")
                                             : std::to_string(m_max_depth))
            << "\n  rr_depth = " << m_rr_depth << "\n  max_energy_loss = "
            << (m_throughput_threshold == 0.f
                    ? std::string("disabled")
                    : (std::to_string(-10.0f * static_cast<float>(log10(m_throughput_threshold))) +
                       " dB"))
            << "\n  hide_emitters = " << m_hide_emitters << "\n]";
        return oss.str();
    }

    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        Float w = pdf_a / (pdf_a + pdf_b);
        return dr::detach<true>(dr::select(dr::isfinite(w), w, 0.f));
    }

    /**
     * \brief Perform a Mueller matrix multiplication in polarized modes, and a
     * fused multiply-add otherwise.
     */
    Spectrum spec_fma(const Spectrum &a, const Spectrum &b,
                      const Spectrum &c) const {
        if constexpr (is_polarized_v<Spectrum>)
            Throw("AcousticPathIntegrator not compatible with polarized rendering.");
        else
            return dr::fmadd(a, b, c);
    }

    MI_DECLARE_CLASS(AcousticPathIntegrator)

protected:

    void render_sample(const Scene *scene,
                       const Sensor *sensor,
                       const Film *film,
                       Sampler *sampler,
                       ImageBlock *block,
                       Float *aovs, /* just passed through towards sample(), putting data into the block needs to happen inside sample() to avoid storing copies of entire histograms. */
                       const Vector2f &pos,
                       Mask active = true) const {
        Log(Debug, "Running render_sample() ..");

        Point2f aperture_sample(.0f);
        if (sensor->needs_aperture_sample())
            aperture_sample = sampler->next_2d(active);

        ScalarVector2f scale  = 1.f / ScalarVector2f(sensor->film()->crop_size());
        Vector2f adjusted_pos = pos * scale;
        if constexpr (!dr::is_jit_v<Float>) Log(Trace, "sensor->film()->crop_size(): %s", sensor->film()->crop_size());
        if constexpr (!dr::is_jit_v<Float>) Log(Trace, "scale: %s, pos: %s, adjusted_pos: %s", scale, pos, adjusted_pos);

        // check if the film is a tape. In the future, this could be changed by implementing two acoustic variants:
        // acoustic, with "spectrum": "Color<Float, 1>" and acoustic_spectral, with "spectrum": "Spectrum<Float, 1>"
        Float frequency_sample = 0.f;
        if (has_flag(sensor->film()->flags(), FilmFlags::Spectral)) {
            frequency_sample = sampler->next_1d(active);
        }
        else {
            frequency_sample = pos[0];

        }

        auto [ray, ray_weight] = sensor->sample_ray_differential(
            0.f, frequency_sample, adjusted_pos, aperture_sample);
        if constexpr (!dr::is_jit_v<Float>) Log(Trace, "Uniform frequency sample: %f, rendering at frequency %f", frequency_sample, ray.wavelengths);
        sample(scene, sampler, film, pos, ray, block, aovs, active);
    }

protected:
    float m_max_time;
    float m_speed_of_sound;
    float m_throughput_threshold;
};

MI_EXPORT_PLUGIN(AcousticPathIntegrator)
NAMESPACE_END(mitsuba)
