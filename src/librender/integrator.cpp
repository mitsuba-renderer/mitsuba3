#include <thread>
#include <mutex>

#include <enoki/morton.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/progress.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/spiral.h>
#include <enoki-thread/thread.h>
#include <mutex>

NAMESPACE_BEGIN(mitsuba)

// -----------------------------------------------------------------------------

MTS_VARIANT SamplingIntegrator<Float, Spectrum>::SamplingIntegrator(const Properties &props)
    : Base(props) {

    m_block_size = (uint32_t) props.size_("block_size", 0);
    uint32_t block_size = math::round_to_power_of_two(m_block_size);
    if (m_block_size > 0 && block_size != m_block_size) {
        Log(Warn, "Setting block size from %i to next higher power of two: %i", m_block_size,
            block_size);
        m_block_size = block_size;
    }

    m_samples_per_pass = (uint32_t) props.size_("samples_per_pass", (size_t) -1);
    m_timeout = props.float_("timeout", -1.f);

    /// Disable direct visibility of emitters if needed
    m_hide_emitters = props.bool_("hide_emitters", false);
}

MTS_VARIANT SamplingIntegrator<Float, Spectrum>::~SamplingIntegrator() { }

MTS_VARIANT void SamplingIntegrator<Float, Spectrum>::cancel() {
    m_stop = true;
}

MTS_VARIANT std::vector<std::string> SamplingIntegrator<Float, Spectrum>::aov_names() const {
    return { };
}

MTS_VARIANT typename SamplingIntegrator<Float, Spectrum>::TensorXf
SamplingIntegrator<Float, Spectrum>::render(Scene *scene,
                                            uint32_t seed,
                                            uint32_t sensor_index,
                                            bool develop_film) {
    ScopedPhase sp(ProfilerPhase::Render);
    m_stop = false;

    TensorXf result;

    ref<Sensor> sensor = scene->sensors()[sensor_index];
    ref<Film> film = sensor->film();
    ScalarVector2i film_size = film->crop_size();

    size_t total_spp        = sensor->sampler()->sample_count();
    size_t samples_per_pass = (m_samples_per_pass == (size_t) -1)
                               ? total_spp : std::min((size_t) m_samples_per_pass, total_spp);
    if ((total_spp % samples_per_pass) != 0)
        Throw("sample_count (%d) must be a multiple of samples_per_pass (%d).",
              total_spp, samples_per_pass);

    size_t n_passes = (total_spp + samples_per_pass - 1) / samples_per_pass;

    std::vector<std::string> channels = aov_names();

    // Insert default channels and set up the film
    for (size_t i = 0; i < 5; ++i)
        channels.insert(channels.begin() + i, std::string(1, "RGBAW"[i]));
    film->prepare(channels);

    m_render_timer.reset();
    if constexpr (!ek::is_jit_array_v<Float>) {
        /// Render on the CPU using a spiral pattern
        size_t n_threads = __global_thread_count;
        Log(Info, "Starting render job (%ix%i, %i sample%s,%s %i thread%s)",
            film_size.x(), film_size.y(),
            total_spp, total_spp == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(" %d passes,", n_passes) : "",
            n_threads, n_threads == 1 ? "" : "s");

        if (m_timeout > 0.f)
            Log(Info, "Timeout specified: %.2f seconds.", m_timeout);

        // Find a good block size to use for splitting up the total workload.
        if (m_block_size == 0) {
            uint32_t block_size = MTS_BLOCK_SIZE;
            while (true) {
                if (block_size == 1 || ek::hprod((film_size + block_size - 1) / block_size) >= n_threads)
                    break;
                block_size /= 2;
            }
            m_block_size = block_size;
        }

        Spiral spiral(film, m_block_size, n_passes);

        ThreadEnvironment env;
        ref<ProgressReporter> progress = new ProgressReporter("Rendering");
        std::mutex mutex;

        // Total number of blocks to be handled, including multiple passes.
        size_t total_blocks = spiral.block_count() * n_passes,
               blocks_done = 0;
        size_t seed_offset = seed * total_spp * ek::hprod(film_size);
        bool has_aovs = !channels.empty();

        ek::parallel_for(
            ek::blocked_range<size_t>(0, total_blocks, 1),
            [&](const ek::blocked_range<size_t> &range) {
                ScopedSetThreadEnvironment set_env(env);
                ref<Sampler> sampler = sensor->sampler()->fork();
                ref<ImageBlock> block = new ImageBlock(m_block_size, channels.size(),
                                                       film->reconstruction_filter(),
                                                       !has_aovs);
                std::unique_ptr<Float[]> aovs(new Float[channels.size()]);

                // For each block
                for (auto i = range.begin(); i != range.end() && !should_stop(); ++i) {
                    auto [offset, size, block_id] = spiral.next_block();
                    Assert(ek::hprod(size) != 0);
                    block->set_size(size);
                    block->set_offset(offset);

                    render_block(scene, sensor, sampler, block, aovs.get(),
                                 samples_per_pass, seed_offset, block_id);

                    film->put(block);

                    /* Critical section: update progress bar */ {
                        std::lock_guard<std::mutex> lock(mutex);
                        blocks_done++;
                        progress->update(blocks_done / (float) total_blocks);
                    }
                }
            }
        );

        if (develop_film)
            result = film->develop();
    } else {
        Log(Info, "Start rendering...");

        ref<Sampler> sampler = sensor->sampler();
        sampler->set_samples_per_wavefront((uint32_t) samples_per_pass);

        ScalarFloat diff_scale_factor = ek::rsqrt((ScalarFloat) sampler->sample_count());
        ScalarUInt32 wavefront_size = ek::hprod(film_size) * (uint32_t) samples_per_pass;
        sampler->seed(seed, wavefront_size);

        UInt32 idx = ek::arange<UInt32>(wavefront_size);
        if (samples_per_pass != 1)
            idx /= (uint32_t) samples_per_pass;

        ref<ImageBlock> block = new ImageBlock(film_size, channels.size(),
                                               film->reconstruction_filter(),
                                               false, false, false, false);
        block->clear();
        Vector2f pos = Vector2f(Float(idx % uint32_t(film_size[0])),
                                Float(idx / uint32_t(film_size[0])));
        std::vector<Float> aovs(channels.size());

        Timer timer;
        for (size_t i = 0; i < n_passes; i++) {
            render_sample(scene, sensor, sampler, block, aovs.data(),
                          pos, diff_scale_factor);
            sampler->schedule_state();

            if (n_passes > 1) {
                ek::eval(block->data());
                ek::sync_thread();
            }
        }
        film->put(block);

        if (n_passes == 1) {
            if (jit_flag(JitFlag::VCallRecord) && jit_flag(JitFlag::LoopRecord)) {
                Log(Info, "Computation graph recorded. (took %s)",
                    util::time_string((float) timer.reset(), true));
            }

            auto &out = Base::m_graphviz_output;
            if (!out.empty()) {
                ref<FileStream> out_stream =
                    new FileStream(out, FileStream::ETruncReadWrite);
                const char *graph = jit_var_graphviz();
                out_stream->write(graph, strlen(graph));
            }
        }

        if (develop_film) {
            result = film->develop();
            ek::schedule(result);
        } else {
            film->schedule_storage();
        }
        ek::eval();

        if (n_passes == 1) {
            if (jit_flag(JitFlag::VCallRecord) && jit_flag(JitFlag::LoopRecord)) {
                Log(Info, "Code generation finished. (took %s)",
                    util::time_string((float) timer.reset(), true));
            }
        }

        ek::sync_thread();
    }

    if (!m_stop)
        Log(Info, "Rendering finished. (took %s)",
            util::time_string((float) m_render_timer.value(), true));

    return result;
}

MTS_VARIANT void SamplingIntegrator<Float, Spectrum>::render_block(const Scene *scene,
                                                                   const Sensor *sensor,
                                                                   Sampler *sampler,
                                                                   ImageBlock *block,
                                                                   Float *aovs,
                                                                   size_t sample_count_,
                                                                   size_t seed_offset,
                                                                   size_t block_id) const {
    block->clear();
    uint32_t pixel_count  = (uint32_t)(m_block_size * m_block_size),
             sample_count = (uint32_t)(sample_count_ == (size_t) -1
                                           ? sampler->sample_count()
                                           : sample_count_);

    ScalarFloat diff_scale_factor = ek::rsqrt((ScalarFloat) sampler->sample_count());

    if constexpr (!ek::is_array_v<Float>) {
        for (uint32_t i = 0; i < pixel_count && !should_stop(); ++i) {
            sampler->seed(block_id * pixel_count + i + seed_offset);

            ScalarPoint2u pos = ek::morton_decode<ScalarPoint2u>(i);
            if (ek::any(pos >= block->size()))
                continue;

            pos += block->offset();
            for (uint32_t j = 0; j < sample_count && !should_stop(); ++j) {
                render_sample(scene, sensor, sampler, block, aovs,
                              pos, diff_scale_factor);
            }
        }
    } else if constexpr (ek::is_array_v<Float> && !ek::is_jit_array_v<Float>) {
        // Ensure that the sample generation is fully deterministic
        sampler->seed(block_id * pixel_count + seed_offset);

        for (auto [index, active] : ek::range<UInt32>(pixel_count * sample_count)) {
            if (should_stop())
                break;
            Point2u pos = ek::morton_decode<Point2u>(index / UInt32(sample_count));
            active &= !any(pos >= block->size());
            pos += block->offset();
            render_sample(scene, sensor, sampler, block, aovs, pos, diff_scale_factor, active);
        }
    } else {
        ENOKI_MARK_USED(scene);
        ENOKI_MARK_USED(sensor);
        ENOKI_MARK_USED(seed_offset);
        ENOKI_MARK_USED(aovs);
        ENOKI_MARK_USED(diff_scale_factor);
        ENOKI_MARK_USED(pixel_count);
        ENOKI_MARK_USED(sample_count);
        ENOKI_MARK_USED(block_id);
        ENOKI_MARK_USED(seed_offset);
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
    Vector2f position_sample = pos + sampler->next_2d(active);

    Point2f aperture_sample(.5f);
    if (sensor->needs_aperture_sample())
        aperture_sample = sampler->next_2d(active);

    Float time = sensor->shutter_open();
    if (sensor->shutter_open_time() > 0.f)
        time += sampler->next_1d(active) * sensor->shutter_open_time();

    Float wavelength_sample = sampler->next_1d(active);

    Vector2f adjusted_position =
        (position_sample - sensor->film()->crop_offset()) /
        sensor->film()->crop_size();

    auto [ray, ray_weight] = sensor->sample_ray_differential(
        time, wavelength_sample, adjusted_position, aperture_sample);

    ray.scale_differential(diff_scale_factor);

    const Medium *medium = sensor->medium();
    std::pair<Spectrum, Mask> result = sample(scene, sampler, ray, medium, aovs + 5, active);
    result.first = ray_weight * result.first;

    UnpolarizedSpectrum spec_u = unpolarized_spectrum(result.first);

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

    block->put(position_sample, aovs, active);

    sampler->advance();
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
    /// Depth to begin using russian roulette
    m_rr_depth = props.int_("rr_depth", 5);
    if (m_rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    /*  Longest visualized path depth (``-1 = infinite``). A value of \c 1 will
        visualize only directly visible light sources. \c 2 will lead to
        single-bounce (direct-only) illumination, and so on. */
    m_max_depth = props.int_("max_depth", -1);
    if (m_max_depth < 0 && m_max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");
}

MTS_VARIANT MonteCarloIntegrator<Float, Spectrum>::~MonteCarloIntegrator() { }

MTS_IMPLEMENT_CLASS_VARIANT(Integrator, Object, "integrator")
MTS_IMPLEMENT_CLASS_VARIANT(SamplingIntegrator, Integrator)
MTS_IMPLEMENT_CLASS_VARIANT(MonteCarloIntegrator, SamplingIntegrator)

MTS_INSTANTIATE_CLASS(Integrator)
MTS_INSTANTIATE_CLASS(SamplingIntegrator)
MTS_INSTANTIATE_CLASS(MonteCarloIntegrator)
NAMESPACE_END(mitsuba)
