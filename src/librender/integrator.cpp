#include <mutex>

#include <enoki/morton.h>
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
#include <enoki-thread/thread.h>

NAMESPACE_BEGIN(mitsuba)

// -----------------------------------------------------------------------------

MTS_VARIANT Integrator<Float, Spectrum>::Integrator(const Properties & props)
    : m_stop(false) {
    m_timeout = props.get<ScalarFloat>("timeout", -1.f);

    // Disable direct visibility of emitters if needed
    m_hide_emitters = props.get<bool>("hide_emitters", false);
}

MTS_VARIANT typename Integrator<Float, Spectrum>::TensorXf
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

MTS_VARIANT std::vector<std::string> Integrator<Float, Spectrum>::aov_names() const {
    return { };
}

MTS_VARIANT void Integrator<Float, Spectrum>::cancel() {
    m_stop = true;
}

// -----------------------------------------------------------------------------

MTS_VARIANT SamplingIntegrator<Float, Spectrum>::SamplingIntegrator(const Properties &props)
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
}

MTS_VARIANT SamplingIntegrator<Float, Spectrum>::~SamplingIntegrator() { }

MTS_VARIANT typename SamplingIntegrator<Float, Spectrum>::TensorXf
SamplingIntegrator<Float, Spectrum>::render(Scene *scene,
                                            Sensor *sensor,
                                            uint32_t seed,
                                            uint32_t spp,
                                            bool develop,
                                            bool evaluate) {
    ScopedPhase sp(ProfilerPhase::Render);
    m_stop = false;

    // Render on a larger film if the 'high quality edges' feature is enabled
    ref<Film> film = sensor->film();
    ScalarVector2u film_size = film->crop_size();
    if (film->has_high_quality_edges())
        film_size += 2 * film->reconstruction_filter()->border_size();

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
    std::vector<std::string> aovs = aov_names();
    size_t n_channels = film->prepare(aovs);

    // Start the render timer (used for timeouts & log messages)
    m_render_timer.reset();

    TensorXf result;
    if constexpr (!ek::is_jit_array_v<Float>) {
        // Render on the CPU using a spiral pattern
        uint32_t n_threads = Thread::thread_count();

        Log(Info, "Starting render job (%ux%u, %u sample%s,%s %u thread%s)",
            film_size.x(), film_size.y(), spp, spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(" %u passes,", n_passes) : "", n_threads,
            n_threads == 1 ? "" : "s");

        if (m_timeout > 0.f)
            Log(Info, "Timeout specified: %.2f seconds.", m_timeout);

        // If no block size was specified, find size that is good for parallelization
        uint32_t block_size = m_block_size;
        if (block_size == 0) {
            block_size = MTS_BLOCK_SIZE; // 32x32
            while (true) {
                // Ensure that there is a block for every thread
                if (block_size == 1 || ek::hprod((film_size + block_size - 1) /
                                                 block_size) >= n_threads)
                    break;
                block_size /= 2;
            }
        }

        Spiral spiral(film_size, film->crop_offset(), block_size, n_passes);

        ref<ProgressReporter> progress = new ProgressReporter("Rendering");
        std::mutex mutex;

        // Total number of blocks to be handled, including multiple passes.
        uint32_t total_blocks = spiral.block_count() * n_passes,
                 blocks_done = 0;

        // Grain size for parallelization
        uint32_t grain_size = std::max(total_blocks / (4 * n_threads), 1u);

        // Avoid overlaps in RNG seeding RNG when a seed is manually specified
        uint64_t seed_offset =
            (uint64_t) seed * (uint64_t) ek::hprod(film_size);

        ThreadEnvironment env;
        ek::parallel_for(
            ek::blocked_range<uint32_t>(0, total_blocks, grain_size),
            [&](const ek::blocked_range<uint32_t> &range) {
                ScopedSetThreadEnvironment set_env(env);
                // Fork a non-overlapping sampler for the current worker
                ref<Sampler> sampler = sensor->sampler()->fork();
                ref<ImageBlock> block = film->create_storage(false /* normalization */,
                                                             true /* border */);
                std::unique_ptr<Float[]> aovs(new Float[n_channels]);

                // Render up to 'grain_size' image blocks
                for (uint32_t i = range.begin();
                     i != range.end() && !should_stop(); ++i) {
                    auto [offset, size, block_id] = spiral.next_block();
                    Assert(ek::hprod(size) != 0);

                    if (film->has_high_quality_edges())
                        offset -= film->reconstruction_filter()->border_size();

                    block->set_size(size);
                    block->set_offset(offset);

                    render_block(scene, sensor, sampler, block, aovs.get(),
                                 spp_per_pass, seed_offset, block_id,
                                 block_size);

                    film->put(block);

                    /* Critical section: update progress bar */ {
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
        Log(Info, "Starting render job (%ux%u, %u sample%s%s)",
            film_size.x(), film_size.y(), spp, spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(", %u passes,", n_passes) : "");

        if (n_passes > 1 && !evaluate) {
            Log(Warn, "render(): forcing 'evaluate=true' since multi-pass "
                      "rendering was requested.");
            evaluate = true;
        }

        ref<Sampler> sampler = sensor->sampler();
        sampler->set_samples_per_wavefront(spp_per_pass);

        ScalarFloat diff_scale_factor = ek::rsqrt((ScalarFloat) sampler->sample_count());
        size_t wavefront_size = ek::hprod(film_size) * (size_t) spp_per_pass;
        sampler->seed(seed, wavefront_size);

        ref<ImageBlock> block = film->create_storage();
        block->clear();
        block->set_offset(film->crop_offset());

        // Compute discrete sample position
        UInt32 idx = ek::arange<UInt32>(wavefront_size);
        if (spp_per_pass != 1)
            idx /= ek::opaque<UInt32>(spp_per_pass);

        Vector2u pos;
        pos.y() = idx / film_size[0];
        pos.x() = idx - pos.y() * film_size[0];

        if (film->has_high_quality_edges())
            pos -= film->reconstruction_filter()->border_size();

        pos += film->crop_offset();

        // Cast to floating point, random offset is added in \ref render_sample()
        Vector2f pos_f = Vector2f(pos);

        Timer timer;
        std::unique_ptr<Float[]> aovs(new Float[n_channels]);

        // Potentially render multiple passes
        for (size_t i = 0; i < n_passes; i++) {
            render_sample(scene, sensor, sampler, block,
                          aovs.get(), pos, diff_scale_factor);

            if (n_passes > 1) {
                sampler->advance(); // Will trigger a kernel launch of size 1
                sampler->schedule_state();
                ek::eval(block->data());
            }
        }
        film->put(block);

        if (n_passes == 1 && jit_flag(JitFlag::VCallRecord) &&
            jit_flag(JitFlag::LoopRecord)) {
            Log(Info, "Computation graph recorded. (took %s)",
                util::time_string((float) timer.reset(), true));
        }

        if (develop) {
            result = film->develop();
            ek::schedule(result);
        } else {
            film->schedule_storage();
        }

        if (evaluate) {
            ek::eval();

            if (n_passes == 1 && jit_flag(JitFlag::VCallRecord) &&
                jit_flag(JitFlag::LoopRecord)) {
                Log(Info, "Code generation finished. (took %s)",
                    util::time_string((float) timer.value(), true));

                /* Separate computation graph recording from the actual
                   rendering time in single-pass mode */
                m_render_timer.reset();
            }

            ek::sync_thread();
        }
    }

    if (!m_stop && (evaluate || !ek::is_jit_array_v<Float>))
        Log(Info, "Rendering finished. (took %s)",
            util::time_string((float) m_render_timer.value(), true));

    return result;
}

MTS_VARIANT void SamplingIntegrator<Float, Spectrum>::render_block(const Scene *scene,
                                                                   const Sensor *sensor,
                                                                   Sampler *sampler,
                                                                   ImageBlock *block,
                                                                   Float *aovs,
                                                                   uint32_t sample_count,
                                                                   uint64_t seed_offset,
                                                                   uint32_t block_id,
                                                                   uint32_t block_size) const {

    if constexpr (!ek::is_array_v<Float>) {
        uint32_t pixel_count = block_size * block_size;

        // Avoid overlaps in RNG seeding RNG when a seed is manually specified
        seed_offset += block_id * pixel_count;

        // Scale down ray differentials when tracing multiple rays per pixel
        Float diff_scale_factor = ek::rsqrt((Float) sample_count);

        block->clear();

        for (uint32_t i = 0; i < pixel_count && !should_stop(); ++i) {
            sampler->seed(seed_offset + i);

            Point2u pos = ek::morton_decode<Point2u>(i);
            if (ek::any(pos >= block->size()))
                continue;

            ScalarPoint2f pos_f = ScalarPoint2f(pos + block->offset());
            for (uint32_t j = 0; j < sample_count && !should_stop(); ++j) {
                render_sample(scene, sensor, sampler, block, aovs,
                              pos_f, diff_scale_factor);
                sampler->advance();
            }
        }
    } else {
        ENOKI_MARK_USED(scene);
        ENOKI_MARK_USED(sensor);
        ENOKI_MARK_USED(block);
        ENOKI_MARK_USED(aovs);
        ENOKI_MARK_USED(sample_count);
        ENOKI_MARK_USED(seed_offset);
        ENOKI_MARK_USED(block_id);
        ENOKI_MARK_USED(block_size);
        Throw("Not implemented for JIT arrays.");
    }
}

MTS_VARIANT void
SamplingIntegrator<Float, Spectrum>::render_sample(const Scene *scene,
                                                   const Sensor *sensor,
                                                   Sampler *sampler,
                                                   ImageBlock *block,
                                                   Float *aovs,
                                                   const Vector2f &pos,
                                                   ScalarFloat diff_scale_factor,
                                                   Mask active) const {
    const Film *film = sensor->film();

    ScalarVector2f scale = 1.f / ScalarVector2f(film->crop_size()),
                   offset = -film->crop_offset() * scale;

    Vector2f sample_pos   = pos + sampler->next_2d(active),
             adjusted_pos = ek::fmadd(sample_pos, scale, offset);

    Point2f aperture_sample(.5f);
    if (sensor->needs_aperture_sample())
        aperture_sample = sampler->next_2d(active);

    Float time = sensor->shutter_open();
    if (sensor->shutter_open_time() > 0.f)
        time += sampler->next_1d(active) * sensor->shutter_open_time();

    Float wavelength_sample = sampler->next_1d(active);

    auto [ray, ray_weight] = sensor->sample_ray_differential(
        time, wavelength_sample, adjusted_pos, aperture_sample);

    if (ray.has_differentials)
        ray.scale_differential(diff_scale_factor);

    const Medium *medium = sensor->medium();
    std::pair<Spectrum, Mask> result = sample(
        scene, sampler, ray, medium, aovs + 5 /* skip R,G,B,A,W */, active);
    result.first = ray_weight * result.first;

    UnpolarizedSpectrum spec_u = unpolarized_spectrum(result.first);

    if (unlikely(has_flag(sensor->film()->flags(), FilmFlags::Special))) {
        sensor->film()->prepare_sample(spec_u, ray.wavelengths, aovs, result.second);
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
        aovs[3] = ek::select(result.second, Float(1.f), Float(0.f));
        aovs[4] = 1.f;
    }

    block->put(sample_pos, aovs, active);
}

MTS_VARIANT std::pair<Spectrum, typename SamplingIntegrator<Float, Spectrum>::Mask>
SamplingIntegrator<Float, Spectrum>::sample(const Scene * /* scene */,
                                            Sampler * /* sampler */,
                                            const RayDifferential3f & /* ray */,
                                            const Medium * /* medium */,
                                            Float * /* aovs */,
                                            Mask /* active */) const {
    NotImplementedError("sample");
}

// -----------------------------------------------------------------------------

MTS_VARIANT MonteCarloIntegrator<Float, Spectrum>::MonteCarloIntegrator(const Properties &props)
    : Base(props) {
    // Depth to begin using russian roulette
    m_rr_depth = props.get<int>("rr_depth", 5);
    if (m_rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    /*  Longest visualized path depth (``-1 = infinite``). A value of \c 1 will
        visualize only directly visible light sources. \c 2 will lead to
        single-bounce (direct-only) illumination, and so on. */
    m_max_depth = props.get<int>("max_depth", -1);
    if (m_max_depth < 0 && m_max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");
}

MTS_VARIANT MonteCarloIntegrator<Float, Spectrum>::~MonteCarloIntegrator() { }

// -----------------------------------------------------------------------------

MTS_VARIANT AdjointIntegrator<Float, Spectrum>::AdjointIntegrator(const Properties &props)
    : Base(props) {

    m_samples_per_pass = props.get<uint32_t>("spp_per_pass", (uint32_t) -1);

    m_rr_depth = props.get<int>("rr_depth", 5);
    if (m_rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    m_max_depth = props.get<int>("max_depth", -1);
    if (m_max_depth < 0 && m_max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");
}

MTS_VARIANT AdjointIntegrator<Float, Spectrum>::~AdjointIntegrator() { }

MTS_VARIANT typename AdjointIntegrator<Float, Spectrum>::TensorXf
AdjointIntegrator<Float, Spectrum>::render(Scene *scene,
                                           Sensor *sensor,
                                           uint32_t seed,
                                           uint32_t spp,
                                           bool develop,
                                           bool evaluate) {
    ScopedPhase sp(ProfilerPhase::Render);
    m_stop = false;

    ref<Film> film = sensor->film();
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

    size_t samples_per_pass = spp_per_pass * (size_t) ek::hprod(film_size),
           total_samples = samples_per_pass * n_passes;

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
            ek::schedule(result);
        } else {
            film->schedule_storage();
        }
        return result;
    }

    TensorXf result;
    if constexpr (!ek::is_jit_array_v<Float>) {
        size_t n_threads = Thread::thread_count();

        Log(Info, "Starting render job (%ux%u, %u sample%s,%s %i thread%s)",
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

        uint64_t seed_offset = (uint64_t) seed * (uint64_t) total_samples;
        std::atomic<size_t> samples_done(0);

        // Start the render timer (used for timeouts & log messages)
        m_render_timer.reset();

        ThreadEnvironment env;
        ek::parallel_for(
            ek::blocked_range<size_t>(0, total_samples, grain_size),
            [&](const ek::blocked_range<size_t> &range) {
                ScopedSetThreadEnvironment set_env(env);

                ref<Sampler> sampler = sensor->sampler()->clone();
                ref<ImageBlock> block = film->create_storage();

                block->set_offset(seed_offset + range.begin());
                block->clear();

                sampler->seed(seed_offset + (uint64_t) range.begin());

                size_t ctr = 0;
                for (auto i = range.begin(); i != range.end() && !should_stop(); ++i) {
                    sample(scene, sensor, sampler, block);
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
                    film->put(block);
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

        Log(Info, "Starting render job (%ux%u, %u sample%s%s)",
            crop_size.x(), crop_size.y(), spp, spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(", %u passes,", n_passes) : "");

        ref<Sampler> sampler = sensor->sampler();
        // The sampler expects samples per pixel per pass.
        sampler->set_samples_per_wavefront(spp_per_pass);
        sampler->seed(seed, samples_per_pass);

        /* Note: we disable warnings because they trigger a horizontal
           reduction which is can be expensive, or even impossible in
           symbolic modes. */

        ref<ImageBlock> block = film->create_storage();
        block->set_offset(film->crop_offset());
        block->clear();

        Timer timer;
        for (size_t i = 0; i < n_passes; i++) {
            sample(scene, sensor, sampler, block);

            if (n_passes > 1) {
                sampler->advance(); // Will trigger a kernel launch of size 1
                sampler->schedule_state();
                ek::eval(block->data());
            }
        }

        film->put(block);

        if (develop) {
            result = film->develop();
            ek::schedule(result);
        } else {
            film->schedule_storage();
        }

        if (evaluate) {
            ek::eval();

            if (n_passes == 1 && jit_flag(JitFlag::VCallRecord) &&
                jit_flag(JitFlag::LoopRecord)) {
                Log(Info, "Code generation finished. (took %s)",
                    util::time_string((float) timer.value(), true));

                /* Separate computation graph recording from the actual
                   rendering time in single-pass mode */
                m_render_timer.reset();
            }

            ek::sync_thread();
        }
    }

    if (!m_stop && (evaluate || !ek::is_jit_array_v<Float>))
        Log(Info, "Rendering finished. (took %s)",
            util::time_string((float) m_render_timer.value(), true));

    return result;
}

// -----------------------------------------------------------------------------

MTS_IMPLEMENT_CLASS_VARIANT(Integrator, Object, "integrator")
MTS_IMPLEMENT_CLASS_VARIANT(SamplingIntegrator, Integrator)
MTS_IMPLEMENT_CLASS_VARIANT(MonteCarloIntegrator, SamplingIntegrator)
MTS_IMPLEMENT_CLASS_VARIANT(AdjointIntegrator, Integrator)

MTS_INSTANTIATE_CLASS(Integrator)
MTS_INSTANTIATE_CLASS(SamplingIntegrator)
MTS_INSTANTIATE_CLASS(MonteCarloIntegrator)
MTS_INSTANTIATE_CLASS(AdjointIntegrator)

NAMESPACE_END(mitsuba)
