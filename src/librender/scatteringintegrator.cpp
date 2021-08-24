#include <mutex>
#include <cmath>  // For std::rint

#include <enoki-thread/thread.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/progress.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/util.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scatteringintegrator.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

// -----------------------------------------------------------------------------

MTS_VARIANT ScatteringIntegrator<Float, Spectrum>::ScatteringIntegrator(const Properties &props)
    : Base(props) {

    m_samples_per_pass = (uint32_t) props.size_("samples_per_pass", (size_t) -1);

    m_rr_depth = props.int_("rr_depth", 5);
    if (m_rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    m_max_depth = props.int_("max_depth", -1);
    if (m_max_depth < 0 && m_max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value >= 0");
}

MTS_VARIANT ScatteringIntegrator<Float, Spectrum>::~ScatteringIntegrator() {}

MTS_VARIANT typename ScatteringIntegrator<Float, Spectrum>::TensorXf
ScatteringIntegrator<Float, Spectrum>::render(Scene *scene, uint32_t seed,
                                              uint32_t sensor_index,
                                              bool develop_film) {
    m_stop = false;

    ref<Sensor> sensor = scene->sensors()[sensor_index];
    ref<Film> film = sensor->film();
    ScalarVector2i film_size = film->size();
    ScalarVector2i crop_size = film->crop_size();

    // Figure out how to divide up samples into passes, if needed
    size_t samples_per_pixel = sensor->sampler()->sample_count();
    size_t samples_per_pass_per_pixel =
        (m_samples_per_pass == (size_t) -1)
            ? samples_per_pixel
            : std::min((size_t) m_samples_per_pass, samples_per_pixel);
    if ((samples_per_pixel % samples_per_pass_per_pixel) != 0)
        Throw("sample_count (%d) must be a multiple of samples_per_pass (%d).",
                samples_per_pixel, samples_per_pass_per_pixel);

    /* Multiply sample count by pixel count to obtain a similar scale
     * to the standard path tracer.
     * When crop is enabled, in order to get comparable convergence and
     * brightness as a with apath tracer, we still trace a number of
     * rays corresponding to the full sensor size.
     */
    size_t samples_per_pass = samples_per_pass_per_pixel * ek::hprod(film_size);
    size_t n_passes =
        (samples_per_pixel * ek::hprod(film_size) + samples_per_pass - 1) / samples_per_pass;
    size_t total_samples = samples_per_pass * n_passes;

    std::vector<std::string> channels = aov_names();
    bool has_aovs = !channels.empty();
    if (has_aovs)
        Throw("Not supported yet: AOVs in ScatteringIntegrator");
    // Insert default channels and set up the film
    for (size_t i = 0; i < 5; ++i)
        channels.insert(channels.begin() + i, std::string(1, "RGBAW"[i]));
    film->prepare(channels);

    // Special case: no emitters present in the scene.
    if (unlikely(scene->emitters().empty())) {
        Log(Info, "Rendering finished (no emitters found, returning black image).");
        TensorXf result;
        if (develop_film) {
            result = film->develop();
            ek::schedule(result);
        } else {
            film->schedule_storage();
        }
        return result;
    }

    size_t total_samples_done = 0;
    m_render_timer.reset();
    if constexpr (!ek::is_jit_array_v<Float>) {
        size_t n_threads = __global_thread_count;

        Log(Info, "Starting render job (%ix%i, %i sample%s,%s %i thread%s)",
            crop_size.x(), crop_size.y(),
            total_samples, total_samples == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(" %d passes,", n_passes) : "",
            n_threads, n_threads == 1 ? "" : "s");
        if (m_timeout > 0.f)
            Log(Info, "Timeout specified: %.2f seconds.", m_timeout);

        // Split up all samples between threads
        size_t grain_count = std::max(
            (size_t) 1, (size_t) std::rint(samples_per_pass /
                                            ScalarFloat(n_threads * 2)));

        ThreadEnvironment env;
        std::mutex mutex;
        ref<ProgressReporter> progress = new ProgressReporter("Rendering");
        size_t update_threshold = std::max(grain_count / 10, 10000lu);

        ek::parallel_for(
            ek::blocked_range<size_t>(0, total_samples, grain_count),
            [&](const ek::blocked_range<size_t> &range) {
                ScopedSetThreadEnvironment set_env(env);

                ref<Sampler> sampler = sensor->sampler()->clone();
                ref<ImageBlock> block = new ImageBlock(
                    crop_size, channels.size(), film->reconstruction_filter(),
                    /* warn_negative */ !has_aovs && !is_spectral_v<Spectrum>,
                    /* warn_invalid */ true, /* border */ false,
                    /* normalize */ true);
                block->set_offset(film->crop_offset());

                size_t samples_done = 0;
                block->clear();
                sampler->seed(seed + (size_t) range.begin());

                for (auto i = range.begin(); i != range.end() && !should_stop(); ++i) {
                    sample(scene, sensor, sampler, block);

                    // TODO: it is possible that there are more than 1 sample / it ?
                    samples_done += 1;
                    if (samples_done > update_threshold) {
                        std::lock_guard<std::mutex> lock(mutex);
                        total_samples_done += samples_done;
                        samples_done = 0;
                        progress->update(total_samples_done / (ScalarFloat) total_samples);
                    }
                }

                // When all samples are done for this range, commit to the film
                /* locked */ {
                    std::lock_guard<std::mutex> lock(mutex);
                    total_samples_done += samples_done;
                    progress->update(total_samples_done / (ScalarFloat) total_samples);

                    film->put(block);
                }
            }
        );
    } else {
        // Wavefront rendering
        Log(Info, "Starting render job (%ix%i, %i sample%s,%s)",
            crop_size.x(), crop_size.y(),
            total_samples, total_samples == 1 ? "" : "s",
            n_passes > 1 ? tfm::format(" %d passes", n_passes) : "");

        ref<Sampler> sampler = sensor->sampler();
        // Implicitly, the sampler expects samples per pixel per pass.
        sampler->set_samples_per_wavefront((uint32_t) samples_per_pass_per_pixel);

        ScalarUInt32 wavefront_size = (uint32_t) samples_per_pass;
        sampler->seed(seed, wavefront_size);

        /* Note: we disable warnings because they trigger a horizontal
           reduction which is can be expensive, or even impossible in
           symbolic modes. */
        ref<ImageBlock> block = new ImageBlock(
            crop_size, channels.size(), film->reconstruction_filter(),
            /* warn_negative */ false,
            /* warn_invalid */ false, /* border */ false,
            /* normalize */ true);
        block->set_offset(film->crop_offset());
        block->clear();

        for (size_t i = 0; i < n_passes; i++) {
            sample(scene, sensor, sampler, block);

            total_samples_done += samples_per_pass;

            sampler->schedule_state();

            if (n_passes > 1) {
                ek::eval(block->data());
                ek::sync_thread();
            }
        }

        film->put(block);
    }

    /* Enoki does not guarantee that subsequent `scatter_add`
     * and `scatter` of different sizes will be executed in
     * order, so we force evaluation. */
    ek::eval();
    ek::sync_thread();

    // Apply proper normalization.
    film->overwrite_channel("W", samples_per_pixel * ek::hprod(film_size) /
                                     ScalarFloat(ek::hprod(crop_size)));

    TensorXf result;
    if (develop_film) {
        result = film->develop();
        ek::schedule(result);
    } else {
        film->schedule_storage();
    }
    ek::eval();
    ek::sync_thread();  // To get an accurate timing below

    if (!m_stop) {
        Assert(total_samples == total_samples_done);
        Log(Info, "Rendering finished. (took %s)",
            util::time_string((float) m_render_timer.value(), true));
    }

    return result;
}

MTS_IMPLEMENT_CLASS_VARIANT(ScatteringIntegrator, Integrator)
MTS_INSTANTIATE_CLASS(ScatteringIntegrator)

NAMESPACE_END(mitsuba)
