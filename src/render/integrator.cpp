#include <mutex>

#include <drjit/morton.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/progress.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/spiral.h>
#include <nanothread/nanothread.h>

NAMESPACE_BEGIN(mitsuba)

// -----------------------------------------------------------------------------

MI_VARIANT Integrator<Float, Spectrum>::Integrator(const Properties & props)
    : m_stop(false), m_id(props.id()) {
    m_timeout = props.get<ScalarFloat>("timeout", -1.f);

    // Disable direct visibility of emitters if needed
    m_hide_emitters = props.get<bool>("hide_emitters", false);
}

MI_VARIANT typename Integrator<Float, Spectrum>::TensorXf
Integrator<Float, Spectrum>::render(Scene *scene,
                                    uint32_t sensor_index,
                                    uint32_t seed,
                                    uint32_t spp,
                                    bool develop,
                                    bool evaluate) {
    if (sensor_index >= scene->sensors().size())
        Throw("Scene::render(): sensor index %i is out of bounds!", sensor_index);

    return render(scene, scene->sensors()[sensor_index].get(),
                  seed, spp, develop, evaluate);
}

MI_VARIANT typename Integrator<Float, Spectrum>::TensorXf
Integrator<Float, Spectrum>::render_forward(Scene* scene,
                                            void* /*params*/,
                                            Sensor *sensor,
                                            uint32_t seed,
                                            uint32_t spp) {
    auto forward_gradients = [&]() -> TensorXf {
        auto image = render(scene, sensor, seed, spp, true, false);
        dr::forward_to(image);
        return TensorXf(dr::grad(image.array()), 3, image.shape().data());
    };

    if constexpr (dr::is_jit_v<Float>) {
        // Recorded loops cannot be differentiated, so let's disable them
        dr::scoped_set_flag scope(JitFlag::LoopRecord, false);
        return forward_gradients();
    } else {
        return forward_gradients();
    }
}

MI_VARIANT void
Integrator<Float, Spectrum>::render_backward(Scene* scene,
                                             void* /*params */,
                                             const TensorXf& grad_in,
                                             Sensor* sensor,
                                             uint32_t seed,
                                             uint32_t spp) {
    auto backward_gradients = [&]() -> void {
        auto image = render(scene, sensor, seed, spp, true, false);
        dr::backward_from((image * grad_in).array());
    };

    if constexpr (dr::is_jit_v<Float>) {
        // Recorded loops cannot be differentiated, so let's disable them
        dr::scoped_set_flag scope(JitFlag::LoopRecord, false);
        backward_gradients();
    } else {
        backward_gradients();
    }
}

MI_VARIANT std::vector<std::string> Integrator<Float, Spectrum>::aov_names() const {
    return { };
}

MI_VARIANT void Integrator<Float, Spectrum>::cancel() {
    m_stop = true;
}

// -----------------------------------------------------------------------------

MI_VARIANT SamplingIntegrator<Float, Spectrum>::SamplingIntegrator(const Properties &props)
    : Base(props) {

    m_block_size = props.get<uint32_t>("block_size", 0);

    // If a block size is specified, ensure that it is a power of two
    uint32_t block_size = math::round_to_power_of_two(m_block_size);
    if (m_block_size > 0 && block_size != m_block_size) {
        Log(Warn, "Setting block size from %i to next higher power of two: %i", m_block_size,
            block_size);
        m_block_size = block_size;
    }

    m_samples_per_pass = props.get<uint32_t>("samples_per_pass", (uint32_t) -1);
    if (m_samples_per_pass != (uint32_t) -1) {
        Log(Warn, "The 'samples_per_pass' is deprecated, as a poor choice of "
                  "this parameter can have a detrimental effect on performance. "
                  "Please leave it undefined; Mitsuba will then automatically "
                  "choose the necessary number of passes.");
    }
}

MI_VARIANT SamplingIntegrator<Float, Spectrum>::~SamplingIntegrator() { }

MI_VARIANT typename SamplingIntegrator<Float, Spectrum>::TensorXf
SamplingIntegrator<Float, Spectrum>::render(Scene *scene,
                                            Sensor *sensor,
                                            uint32_t seed,
                                            uint32_t spp,
                                            bool develop,
                                            bool evaluate) {
    ScopedPhase sp(ProfilerPhase::Render);
    m_stop = false;

    // Render on a larger film if the 'high quality edges' feature is enabled
    Film *film = sensor->film();
    ScalarVector2u film_size = film->crop_size();
    if (film->sample_border())
        film_size += 2 * film->rfilter()->border_size();

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
    size_t n_channels = film->prepare(aov_names());

    // Start the render timer (used for timeouts & log messages)
    m_render_timer.reset();

    TensorXf result;
    if constexpr (!dr::is_jit_v<Float>) {
        // Render on the CPU using a spiral pattern
        uint32_t n_threads = (uint32_t) Thread::thread_count();

        Log(Info, "Starting render job (%ux%u, %u sample%s,%s %u thread%s)",
            film_size.x(), film_size.y(), spp, spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(" %u passes,", n_passes) : "", n_threads,
            n_threads == 1 ? "" : "s");

        if (m_timeout > 0.f)
            Log(Info, "Timeout specified: %.2f seconds.", m_timeout);

        // If no block size was specified, find size that is good for parallelization
        uint32_t block_size = m_block_size;
        if (block_size == 0) {
            block_size = MI_BLOCK_SIZE; // 32x32
            while (true) {
                // Ensure that there is a block for every thread
                if (block_size == 1 || dr::prod((film_size + block_size - 1) /
                                                 block_size) >= n_threads)
                    break;
                block_size /= 2;
            }
        }

        Spiral spiral(film_size, film->crop_offset(), block_size, n_passes);

        std::mutex mutex;
        ref<ProgressReporter> progress;
        Logger* logger = mitsuba::Thread::thread()->logger();
        if (logger && Info >= logger->log_level())
            progress = new ProgressReporter("Rendering");

        // Total number of blocks to be handled, including multiple passes.
        uint32_t total_blocks = spiral.block_count() * n_passes,
                 blocks_done = 0;

        // Grain size for parallelization
        uint32_t grain_size = std::max(total_blocks / (4 * n_threads), 1u);

        // Avoid overlaps in RNG seeding RNG when a seed is manually specified
        seed *= dr::prod(film_size);

        ThreadEnvironment env;
        dr::parallel_for(
            dr::blocked_range<uint32_t>(0, total_blocks, grain_size),
            [&](const dr::blocked_range<uint32_t> &range) {
                ScopedSetThreadEnvironment set_env(env);
                // Fork a non-overlapping sampler for the current worker
                ref<Sampler> sampler = sensor->sampler()->fork();

                ref<ImageBlock> block = film->create_block(
                    ScalarVector2u(block_size) /* size */,
                    false /* normalize */,
                    true /* border */);

                std::unique_ptr<Float[]> aovs(new Float[n_channels]);

                // Render up to 'grain_size' image blocks
                for (uint32_t i = range.begin();
                     i != range.end() && !should_stop(); ++i) {
                    auto [offset, size, block_id] = spiral.next_block();
                    Assert(dr::prod(size) != 0);

                    if (film->sample_border())
                        offset -= film->rfilter()->border_size();

                    block->set_size(size);
                    block->set_offset(offset);

                    render_block(scene, sensor, sampler, block, aovs.get(),
                                 spp_per_pass, seed, block_id, block_size);

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
        size_t wavefront_size = (size_t) film_size.x() *
                                (size_t) film_size.y() * (size_t) spp_per_pass,
               wavefront_size_limit = 0xffffffffu;

        if (wavefront_size > wavefront_size_limit) {
            spp_per_pass /=
                (uint32_t)((wavefront_size + wavefront_size_limit - 1) /
                           wavefront_size_limit);
            n_passes       = spp / spp_per_pass;
            wavefront_size = (size_t) film_size.x() * (size_t) film_size.y() *
                             (size_t) spp_per_pass;

            Log(Warn,
                "The requested rendering task involves %zu Monte Carlo "
                "samples, which exceeds the upper limit of 2^32 = 4294967296 "
                "for this variant. Mitsuba will instead split the rendering "
                "task into %zu smaller passes to avoid exceeding the limits.",
                wavefront_size, n_passes);
        }

        dr::sync_thread(); // Separate from scene initialization (for timings)

        Log(Info, "Starting render job (%ux%u, %u sample%s%s)",
            film_size.x(), film_size.y(), spp, spp == 1 ? "" : "s",
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

        // Only use the ImageBlock coalescing feature when rendering enough samples
        block->set_coalesce(block->coalesce() && spp_per_pass >= 4);

        // Compute discrete sample position
        UInt32 idx = dr::arange<UInt32>((uint32_t) wavefront_size);

        // Try to avoid a division by an unknown constant if we can help it
        uint32_t log_spp_per_pass = dr::log2i(spp_per_pass);
        if ((1u << log_spp_per_pass) == spp_per_pass)
            idx >>= dr::opaque<UInt32>(log_spp_per_pass);
        else
            idx /= dr::opaque<UInt32>(spp_per_pass);

        // Compute the position on the image plane
        Vector2i pos;
        pos.y() = idx / film_size[0];
        pos.x() = dr::fnmadd(film_size[0], pos.y(), idx);

        if (film->sample_border())
            pos -= film->rfilter()->border_size();

        pos += film->crop_offset();

        // Scale factor that will be applied to ray differentials
        ScalarFloat diff_scale_factor = dr::rsqrt((ScalarFloat) spp);

        Timer timer;
        std::unique_ptr<Float[]> aovs(new Float[n_channels]);

        // Potentially render multiple passes
        for (size_t i = 0; i < n_passes; i++) {
            render_sample(scene, sensor, sampler, block, aovs.get(), pos,
                          diff_scale_factor);

            if (n_passes > 1) {
                sampler->advance(); // Will trigger a kernel launch of size 1
                sampler->schedule_state();
                dr::eval(block->tensor());
            }
        }

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

MI_VARIANT void SamplingIntegrator<Float, Spectrum>::render_block(const Scene *scene,
                                                                   const Sensor *sensor,
                                                                   Sampler *sampler,
                                                                   ImageBlock *block,
                                                                   Float *aovs,
                                                                   uint32_t sample_count,
                                                                   uint32_t seed,
                                                                   uint32_t block_id,
                                                                   uint32_t block_size) const {

    if constexpr (!dr::is_array_v<Float>) {
        uint32_t pixel_count = block_size * block_size;

        // Avoid overlaps in RNG seeding RNG when a seed is manually specified
        seed += block_id * pixel_count;

        // Scale down ray differentials when tracing multiple rays per pixel
        Float diff_scale_factor = dr::rsqrt((Float) sample_count);

        // Clear block (it's being reused)
        block->clear();

        for (uint32_t i = 0; i < pixel_count && !should_stop(); ++i) {
            sampler->seed(seed + i);

            Point2u pos = dr::morton_decode<Point2u>(i);
            if (dr::any(pos >= block->size()))
                continue;

            Point2f pos_f = Point2f(Point2i(pos) + block->offset());
            for (uint32_t j = 0; j < sample_count && !should_stop(); ++j) {
                render_sample(scene, sensor, sampler, block, aovs, pos_f,
                              diff_scale_factor);
                sampler->advance();
            }
        }
    } else {
        DRJIT_MARK_USED(scene);
        DRJIT_MARK_USED(sensor);
        DRJIT_MARK_USED(sampler);
        DRJIT_MARK_USED(block);
        DRJIT_MARK_USED(aovs);
        DRJIT_MARK_USED(sample_count);
        DRJIT_MARK_USED(seed);
        DRJIT_MARK_USED(block_id);
        DRJIT_MARK_USED(block_size);
        Throw("Not implemented for JIT arrays.");
    }
}

MI_VARIANT void
SamplingIntegrator<Float, Spectrum>::render_sample(const Scene *scene,
                                                   const Sensor *sensor,
                                                   Sampler *sampler,
                                                   ImageBlock *block,
                                                   Float *aovs,
                                                   const Vector2f &pos,
                                                   ScalarFloat diff_scale_factor,
                                                   Mask active) const {
    const Film *film = sensor->film();
    const bool has_alpha = has_flag(film->flags(), FilmFlags::Alpha);
    const bool box_filter = film->rfilter()->is_box_filter();

    ScalarVector2f scale = 1.f / ScalarVector2f(film->crop_size()),
                   offset = -ScalarVector2f(film->crop_offset()) * scale;

    Vector2f sample_pos   = pos + sampler->next_2d(active),
             adjusted_pos = dr::fmadd(sample_pos, scale, offset);

    Point2f aperture_sample(.5f);
    if (sensor->needs_aperture_sample())
        aperture_sample = sampler->next_2d(active);

    Float time = sensor->shutter_open();
    if (sensor->shutter_open_time() > 0.f)
        time += sampler->next_1d(active) * sensor->shutter_open_time();

    Float wavelength_sample = 0.f;
    if constexpr (is_spectral_v<Spectrum>)
        wavelength_sample = sampler->next_1d(active);

    auto [ray, ray_weight] = sensor->sample_ray_differential(
        time, wavelength_sample, adjusted_pos, aperture_sample);

    if (ray.has_differentials)
        ray.scale_differential(diff_scale_factor);

    const Medium *medium = sensor->medium();

    auto [spec, valid] = sample(scene, sampler, ray, medium,
               aovs + (has_alpha ? 5 : 4) /* skip R,G,B,[A],W */, active);

    UnpolarizedSpectrum spec_u = unpolarized_spectrum(ray_weight * spec);

    if (unlikely(has_flag(film->flags(), FilmFlags::Special))) {
        film->prepare_sample(spec_u, ray.wavelengths, aovs,
                             /*weight*/ 1.f,
                             /*alpha */ dr::select(valid, Float(1.f), Float(0.f)),
                             valid);
    } else {
        Color3f rgb;
        if constexpr (is_spectral_v<Spectrum>)
            rgb = spectrum_to_srgb(spec_u, ray.wavelengths, active);
        else if constexpr (is_monochromatic_v<Spectrum>)
            rgb = spec_u.x();
        else
            rgb = spec_u;

        aovs[0] = rgb.x();
        aovs[1] = rgb.y();
        aovs[2] = rgb.z();

        if (likely(has_alpha)) {
            aovs[3] = dr::select(valid, Float(1.f), Float(0.f));
            aovs[4] = 1.f;
        } else {
            aovs[3] = 1.f;
        }
    }

    // With box filter, ignore random offset to prevent numerical instabilities
    block->put(box_filter ? pos : sample_pos, aovs, active);
}

MI_VARIANT std::pair<Spectrum, typename SamplingIntegrator<Float, Spectrum>::Mask>
SamplingIntegrator<Float, Spectrum>::sample(const Scene * /* scene */,
                                            Sampler * /* sampler */,
                                            const RayDifferential3f & /* ray */,
                                            const Medium * /* medium */,
                                            Float * /* aovs */,
                                            Mask /* active */) const {
    NotImplementedError("sample");
}

// -----------------------------------------------------------------------------

MI_VARIANT MonteCarloIntegrator<Float, Spectrum>::MonteCarloIntegrator(const Properties &props)
    : Base(props) {
    /*  Longest visualized path depth (``-1 = infinite``). A value of \c 1 will
        visualize only directly visible light sources. \c 2 will lead to
        single-bounce (direct-only) illumination, and so on. */
    int max_depth = props.get<int>("max_depth", -1);
    if (max_depth < 0 && max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");

    m_max_depth = (uint32_t) max_depth; // This maps -1 to 2^32-1 bounces

    // Depth to begin using russian roulette
    int rr_depth = props.get<int>("rr_depth", 5);
    if (rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    m_rr_depth = (uint32_t) rr_depth;
}

MI_VARIANT MonteCarloIntegrator<Float, Spectrum>::~MonteCarloIntegrator() { }

// -----------------------------------------------------------------------------

MI_VARIANT AdjointIntegrator<Float, Spectrum>::AdjointIntegrator(const Properties &props)
    : Base(props) {

    m_samples_per_pass = props.get<uint32_t>("samples_per_pass", (uint32_t) -1);

    m_rr_depth = props.get<int>("rr_depth", 5);
    if (m_rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    m_max_depth = props.get<int>("max_depth", -1);
    if (m_max_depth < 0 && m_max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");
}

MI_VARIANT AdjointIntegrator<Float, Spectrum>::~AdjointIntegrator() { }

MI_VARIANT typename AdjointIntegrator<Float, Spectrum>::TensorXf
AdjointIntegrator<Float, Spectrum>::render(Scene *scene,
                                           Sensor *sensor,
                                           uint32_t seed,
                                           uint32_t spp,
                                           bool develop,
                                           bool evaluate) {
    ScopedPhase sp(ProfilerPhase::Render);
    m_stop = false;

    Film *film = sensor->film();
    ScalarVector2u film_size = film->size(),
                   crop_size = film->crop_size();

    // Potentially adjust the number of samples per pixel if spp != 0
    Sampler *sampler = sensor->sampler();
    if (spp)
        sampler->set_sample_count(spp);
    spp = sampler->sample_count();

    // Figure out how to divide up samples into passes, if needed
    uint32_t spp_per_pass = (m_samples_per_pass == (uint32_t) -1)
                                ? spp
                                : std::min(m_samples_per_pass, spp);

    if ((spp % spp_per_pass) != 0)
        Throw("sample_count (%d) must be a multiple of samples_per_pass (%d).",
              spp, spp_per_pass);

    uint32_t n_passes = spp / spp_per_pass;

    size_t samples_per_pass =
        (size_t) film_size.x() * (size_t) film_size.y() * (size_t) spp_per_pass;

    std::vector<std::string> aovs = aov_names();
    if (!aovs.empty())
        Throw("AOVs are not supported in the AdjointIntegrator!");
    film->prepare(aovs);

    // Special case: no emitters present in the scene.
    if (unlikely(scene->emitters().empty())) {
        Log(Info, "Rendering finished (no emitters found, returning black image).");
        TensorXf result;
        if (develop) {
            result = film->develop();
            dr::schedule(result);
        } else {
            film->schedule_storage();
        }
        return result;
    }

    ScalarFloat sample_scale =
        dr::prod(crop_size) / ScalarFloat(spp * dr::prod(film_size));

    TensorXf result;
    if constexpr (!dr::is_jit_v<Float>) {
        size_t n_threads = Thread::thread_count();

        Log(Info, "Starting render job (%ux%u, %u sample%s,%s %u thread%s)",
            crop_size.x(), crop_size.y(), spp, spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(" %d passes,", n_passes) : "", n_threads,
            n_threads == 1 ? "" : "s");

        if (m_timeout > 0.f)
            Log(Info, "Timeout specified: %.2f seconds.", m_timeout);

        // Split up all samples between threads
        size_t grain_size =
            std::max(samples_per_pass / (4 * n_threads), (size_t) 1);

        std::mutex mutex;
        ref<ProgressReporter> progress = new ProgressReporter("Rendering");

        size_t total_samples = samples_per_pass * n_passes;

        seed *= (uint32_t) total_samples / (uint32_t) grain_size;
        std::atomic<size_t> samples_done(0);

        // Start the render timer (used for timeouts & log messages)
        m_render_timer.reset();

        ThreadEnvironment env;
        dr::parallel_for(
            dr::blocked_range<size_t>(0, total_samples, grain_size),
            [&](const dr::blocked_range<size_t> &range) {
                ScopedSetThreadEnvironment set_env(env);

                // Fork a non-overlapping sampler for the current worker
                ref<Sampler> sampler = sensor->sampler()->clone();

                ref<ImageBlock> block = film->create_block(
                    ScalarVector2u(0) /* use crop size */,
                    true /* normalize */,
                    false /* border */);

                block->set_offset(film->crop_offset());

                // Clear block (it's being reused)
                block->clear();

                sampler->seed(seed +
                              (uint32_t) range.begin() / (uint32_t) grain_size);

                size_t ctr = 0;
                for (auto i = range.begin(); i != range.end() && !should_stop(); ++i) {
                    sample(scene, sensor, sampler, block, sample_scale);
                    sampler->advance();

                    ctr++;
                    if (ctr > 10000) {
                        std::lock_guard<std::mutex> lock(mutex);
                        samples_done += ctr;
                        ctr = 0;
                        progress->update(samples_done / (ScalarFloat) total_samples);
                    }
                }
                total_samples += ctr;

                // When all samples are done for this range, commit to the film
                /* locked */ {
                    std::lock_guard<std::mutex> lock(mutex);
                    progress->update(samples_done / (ScalarFloat) total_samples);
                    film->put_block(block);
                }
            }
        );

        if (develop)
            result = film->develop();
    } else {
        if (n_passes > 1 && !evaluate) {
            Log(Warn, "render(): forcing 'evaluate=true' since multi-pass "
                      "rendering was requested.");
            evaluate = true;
        }

        constexpr size_t wavefront_size_limit = 0xffffffffu;
        if (samples_per_pass > wavefront_size_limit) {
            spp_per_pass /=
                (uint32_t)((samples_per_pass + wavefront_size_limit - 1) /
                           wavefront_size_limit);
            n_passes = spp / spp_per_pass;
            samples_per_pass = (size_t) film_size.x() * (size_t) film_size.y() *
                               (size_t) spp_per_pass;

            Log(Warn,
                "The requested rendering task involves %zu Monte Carlo "
                "samples, which exceeds the upper limit of 2^32 = 4294967296 "
                "for this variant. Mitsuba will instead split the rendering "
                "task into %zu smaller passes to avoid exceeding the limits.",
                samples_per_pass, n_passes);
        }

        Log(Info, "Starting render job (%ux%u, %u sample%s%s)",
            crop_size.x(), crop_size.y(), spp, spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(", %u passes", n_passes) : "");

        // Inform the sampler about the passes (needed in vectorized modes)
        sampler->set_samples_per_wavefront(spp_per_pass);

        // Seed the underlying random number generators, if applicable
        sampler->seed(seed, (uint32_t) samples_per_pass);

        ref<ImageBlock> block = film->create_block(
            ScalarVector2u(0) /* use crop size */,
            true /* normalize */,
            false /* border */);

        block->set_offset(film->crop_offset());

        /* Disable coalescing of atomic writes performed within the ImageBlock
           (they are highly irregular in any particle tracing-based method) */
        block->set_coalesce(false);

        Timer timer;
        for (size_t i = 0; i < n_passes; i++) {
            sample(scene, sensor, sampler, block, sample_scale);

            if (n_passes > 1) {
                sampler->advance(); // Will trigger a kernel launch of size 1
                sampler->schedule_state();
                dr::eval(block->tensor());
            }
        }

        film->put_block(block);

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

// -----------------------------------------------------------------------------

MI_IMPLEMENT_CLASS_VARIANT(Integrator, Object, "integrator")
MI_IMPLEMENT_CLASS_VARIANT(SamplingIntegrator, Integrator)
MI_IMPLEMENT_CLASS_VARIANT(MonteCarloIntegrator, SamplingIntegrator)
MI_IMPLEMENT_CLASS_VARIANT(AdjointIntegrator, Integrator)

MI_INSTANTIATE_CLASS(Integrator)
MI_INSTANTIATE_CLASS(SamplingIntegrator)
MI_INSTANTIATE_CLASS(MonteCarloIntegrator)
MI_INSTANTIATE_CLASS(AdjointIntegrator)

NAMESPACE_END(mitsuba)
