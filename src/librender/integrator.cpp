#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/spiral.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/progress.h>
#include <enoki/morton.h>

NAMESPACE_BEGIN(mitsuba)

Integrator::Integrator(const Properties &/*props*/) { }

SamplingIntegrator::SamplingIntegrator(const Properties &props)
    : Integrator(props) {
    m_block_size = props.size_("block_size", MTS_BLOCK_SIZE);
    size_t b = math::round_to_power_of_two(m_block_size);

    if (b != m_block_size) {
        m_block_size = b;
        Log(EWarn, "Setting block size to next higher power of two: %i", m_block_size);
    }
}

SamplingIntegrator::~SamplingIntegrator() { }

void SamplingIntegrator::cancel() {
    m_stop = true;
}

bool SamplingIntegrator::render(Scene *scene, bool vectorize) {
    ScopedPhase sp(EProfilerPhase::ERender);
    ref<Sensor> sensor = scene->sensor();
    ref<Film> film = sensor->film();

    size_t n_cores = util::core_count();
    size_t sample_count = scene->sampler()->sample_count();
    film->clear();
    m_stop = false;

    Log(EInfo, "Starting render job (%ix%i, %i sample%s, %i core%s)",
        film->crop_size().x(), film->crop_size().y(),
        sample_count, sample_count == 1 ? "" : "s",
        n_cores, n_cores == 1 ? "" : "s");

    Spiral spiral(film, m_block_size);

    Timer timer;
    ThreadEnvironment env;
    ref<ProgressReporter> progress = new ProgressReporter("Rendering");
    tbb::spin_mutex mutex;
    size_t blocks_done = 0;
    size_t grain_count = std::max(
        (size_t) 1, (size_t) std::rint(spiral.block_count() / Float(n_cores * 2)));

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, spiral.block_count(), grain_count),
        [&](const tbb::blocked_range<size_t> &range) {
            ScopedSetThreadEnvironment set_env(env);
            ref<Sampler> sampler = scene->sampler()->clone();
            ref<ImageBlock> block =
                new ImageBlock(Bitmap::EXYZAW, Vector2i(m_block_size), film->reconstruction_filter());
            Point2fX points;
            for (auto i = range.begin(); i != range.end() && !m_stop; ++i) {
                Vector2i offset, size;
                std::tie(offset, size) = spiral.next_block();

                // Ensure that the sample generation is fully deterministic
                sampler->seed((size_t) offset.x() +
                              (size_t) offset.y() * (size_t) film->crop_size().x());

                if (hprod(size) == 0)
                    Throw("Internal error -- generated empty image block!");

                if (size != block->size())
                    block = new ImageBlock(Bitmap::EXYZAW, size, film->reconstruction_filter());

                block->set_offset(offset);

                if (vectorize)
                    render_block_vector(scene, sampler, block, points);
                else
                    render_block_scalar(scene, sampler, block);

                film->put(block);

                /* locked */ {
                    tbb::spin_mutex::scoped_lock lock(mutex);
                    blocks_done++;
                    progress->update(blocks_done / (Float) spiral.block_count());
                }
            }
        }
    );

    if (!m_stop)
        Log(EInfo, "Rendering finished. (took %s)",
            util::time_string(timer.value(), true));

    return !m_stop;
}

void SamplingIntegrator::render_block_scalar(const Scene *scene,
                                             Sampler *sampler,
                                             ImageBlock *block) const {
    const Sensor *sensor = scene->sensor();
    block->clear();
    uint32_t pixel_count = m_block_size * m_block_size,
             sample_count = sampler->sample_count();

    RadianceSample3f rs(scene, sampler);
    bool needs_aperture_sample = sensor->needs_aperture_sample();
    bool needs_time_sample = sensor->shutter_open_time() > 0;

    Float diff_scale_factor = rsqrt((Float) sampler->sample_count());

    Point2f aperture_sample(.5f);
    Vector2f inv_resolution = 1.f / sensor->film()->crop_size();

    for (uint32_t i = 0; i < pixel_count && !m_stop; ++i) {
        Point2u p = enoki::morton_decode<Point2u>(i);
        if (any(p >= block->size()))
            continue;

        p += block->offset();
        for (uint32_t j = 0; j < sample_count && !m_stop; ++j) {
            Vector2f position_sample = p + rs.next_2d();

            if (needs_aperture_sample)
                aperture_sample = rs.next_2d();

            Float time = sensor->shutter_open();
            if (needs_time_sample)
                time += rs.next_1d() * sensor->shutter_open_time();

            Float wavelength_sample = rs.next_1d();

            RayDifferential3f ray;
            Spectrumf ray_weight;
            std::tie(ray, ray_weight) = sensor->sample_ray_differential(
                time, wavelength_sample, position_sample * inv_resolution,
                aperture_sample);

            ray.scale_differential(diff_scale_factor);

            Spectrumf result;
            /* Integrator::eval */ {
                ScopedPhase sp(EProfilerPhase::ESamplingIntegratorEval);
                result = eval(ray, rs);
            }
            /* ImageBlock::put */ {
                ScopedPhase sp(EProfilerPhase::EImageBlockPut);
                block->put(position_sample, ray.wavelengths,
                           ray_weight * result, rs.alpha);
            }
        }
    }
}

void SamplingIntegrator::render_block_vector(const Scene *scene,
                                             Sampler *sampler,
                                             ImageBlock *block,
                                             Point2fX &points) const {
    const Sensor *sensor = scene->sensor();
    block->clear();
    uint32_t sample_count = sampler->sample_count();

    if (block->size().x() != block->size().y()
        || (uint32_t) hprod(block->size()) * sample_count
           != (uint32_t) slices(points)) {

        size_t max_pixel_count = m_block_size * m_block_size,
               n_entries = 0;
        set_slices(points, max_pixel_count * sample_count);
        for (uint32_t i = 0; i < max_pixel_count; ++i) {
            Point2u p = enoki::morton_decode<Point2u>(i);
            if (any(p >= block->size()))
                continue;
            Point2f pf = Point2f(p);

            for (uint32_t j = 0; j < sample_count; ++j)
                slice(points, n_entries++) = pf;
        }
        set_slices(points, n_entries);
    }

    Vector2f inv_resolution = 1.f / sensor->film()->crop_size();

    Vector2fP offset = block->offset(),
              aperture_sample = .5f;

    RadianceSample3fP rs(scene, sampler);

    bool needs_aperture_sample = sensor->needs_aperture_sample(),
         needs_time_sample = sensor->shutter_open_time() == 0;

    Float diff_scale_factor = rsqrt((Float) sampler->sample_count());
    UInt32P index = index_sequence<UInt32P>();

    for (size_t i = 0; i < packets(points) && !m_stop; ++i) {
        MaskP active = index < slices(points);
        Vector2fP position_sample =
            packet(points, i) + offset + rs.next_2d(active);

        if (needs_aperture_sample)
            aperture_sample = rs.next_2d(active);

        FloatP time = sensor->shutter_open();
        if (needs_time_sample)
            time += rs.next_1d(active) * sensor->shutter_open_time();

        FloatP wavelength_sample = rs.next_1d(active);

        RayDifferential3fP ray;
        SpectrumfP ray_weight;
        std::tie(ray, ray_weight) = sensor->sample_ray_differential(
            time, wavelength_sample, position_sample * inv_resolution,
            aperture_sample, active);

        ray.scale_differential(diff_scale_factor);

        SpectrumfP result;
        /* Integrator::eval */ {
            ScopedPhase p(EProfilerPhase::ESamplingIntegratorEvalP);
            result = eval(ray, rs);
        }
        /* ImageBlock::put */ {
            ScopedPhase p(EProfilerPhase::EImageBlockPutP);
            block->put(position_sample, ray.wavelengths,
                       ray_weight * result, rs.alpha);
        }

        index += (uint32_t) PacketSize;
    }
}

MonteCarloIntegrator::MonteCarloIntegrator(const Properties &props)
    : SamplingIntegrator(props) {
    /// Depth to begin using russian roulette
    m_rr_depth = props.int_("rr_depth", 5);
    if (m_rr_depth <= 0)
        Throw("\"rr_depth\" must be set to a value greater than zero!");

    /** Longest visualized path depth (\c -1 = infinite).
      * A value of \c 1 will visualize only directly visible light sources.
      * \c 2 will lead to single-bounce (direct-only) illumination, and so on.
      */
    m_max_depth = props.int_("max_depth", -1);
    if (m_max_depth <= 0 && m_max_depth != -1)
        Throw("\"max_depth\" must be set to -1 (infinite) or a value"
              " greater than zero!");

    /**
     * This parameter specifies the action to be taken when the geometric
     * and shading normals of a surface don't agree on whether a ray is on
     * the front or back-side of a surface.
     *
     * When \c strict_normals is set to \c false, the shading normal has
     * precedence, and rendering proceeds normally at the risk of
     * introducing small light leaks (this is the default).
     *
     * When \c strict_normals is set to \c true, the random walk is
     * terminated when encountering such a situation. This may
     * lead to silhouette darkening on badly tesselated meshes.
     */
    m_strict_normals = props.bool_("strict_normals", false);
}

MonteCarloIntegrator::~MonteCarloIntegrator() { }

MTS_IMPLEMENT_CLASS(Integrator, Object)
MTS_IMPLEMENT_CLASS(SamplingIntegrator, Integrator)
MTS_IMPLEMENT_CLASS(MonteCarloIntegrator, SamplingIntegrator)

NAMESPACE_END(mitsuba)
