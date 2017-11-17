#include <enoki/morton.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/spiral.h>
#include <tbb/tbb.h>

NAMESPACE_BEGIN(mitsuba)

Integrator::Integrator(const Properties &/*props*/)
    : Object() { }

// Integrator::Integrator(Stream *stream, InstanceManager *manager)
//  : Object(stream, manager) { }

// void Integrator::serialize(Stream *stream, InstanceManager * /*manager*/) const {
//     Object::serialize(stream, manager);
// }

void Integrator::configure_sampler(const Scene * scene, Sampler *sampler) {
    // Prepare the sampler for bucket-based rendering
    sampler->set_film_resolution(
        scene->film()->crop_size(),
        class_()->derives_from(MTS_CLASS(SamplingIntegrator))
    );
}

SamplingIntegrator::SamplingIntegrator(const Properties &props)
    : Integrator(props) { }

// SamplingIntegrator::SamplingIntegrator(Stream *stream, InstanceManager *manager)
//  : Integrator(stream, manager) { }

// void SamplingIntegrator::serialize(Stream *stream,
//                                    InstanceManager *manager) const {
//     Integrator::serialize(stream, manager);
// }

void SamplingIntegrator::cancel() {
    NotImplementedError("cancel");  // Well, crashing is one way to cancel.
}

template <bool Vectorize>
bool SamplingIntegrator::render_impl(Scene *scene) {
    ref<Sensor> sensor = scene->sensor();
    ref<Film> film = sensor->film();
    ref<Sampler> sampler = sensor->sampler();

    size_t n_cores = util::core_count();
    size_t sample_count = sampler->sample_count();
    film->clear();

    Log(EInfo, "Starting render job (%ix%i, %i sample%s, %i core%s)",
        film->crop_size().x(), film->crop_size().y(),
        sample_count, sample_count == 1 ? "" : "s",
        n_cores, n_cores == 1 ? "" : "s");

    // Prepare image blocks to be given to the threads.
    // TODO: tweak interface to allow reusing the blocks?
    Spiral spiral(film);
    std::vector<ref<ImageBlock>> blocks;
    ref<ImageBlock> b = spiral.next_block();
    do {
        blocks.push_back(b);
        b = spiral.next_block();
    } while (b);

    // Hack to make sure that each block has a sampler seeded differently
    // (in a thread-safe way).
    // TODO: refactor / do this more elegantly
    std::vector<ref<Sampler>> samplers;
    for (size_t i = 0; i < blocks.size(); ++i) {
        // Perturb sampler's state before cloning it.
        sampler->next_1d();
        samplers.push_back(sampler->clone());
    }

    // Render image blocks in parallel.
    // TODO: proper parallelization support (resources, distributed, etc).
    bool stop = false;
    tbb::parallel_for(
        // Minimum workload: 1 image block per thread.
        tbb::blocked_range<size_t>(0, blocks.size(), 1),
        [&](const tbb::blocked_range<size_t> &range) {
            for (auto block_i = range.begin(); block_i != range.end(); ++block_i) {
                auto &block = blocks[block_i];
                auto local_sampler = samplers[block_i];


                // Pixels of the block will be handled in Morton order.
                // TODO: avoid re-generating the most common morton order over
                // and over again (at least for the standard image block size,
                // it is always the same).
                size_t i_max = math::round_to_power_of_two(
                        std::max(block->width(), block->height()));
                i_max *= i_max;

                std::vector<Point2u> points;
                points.reserve(i_max);
                for (value_t<Point2u> i = 0; i < i_max; ++i) {
                    const auto p = enoki::morton_decode<Point2u>(i);
                    if (p.x() >= block->width() || p.y() >= block->height())
                        continue;

                    points.push_back(p);
                }

                // Start rendering the image block.
                render_block<Vectorize>(scene, sensor.get(), local_sampler,
                                        block.get(), stop, points);
                // Add the block's results to the film.
                // TODO: double-check this is thread-safe.
                film->put(block.get());
            }
        }
    );
    Log(EInfo, "Committed %i blocks to the film.", blocks.size());
    return true;
}
bool SamplingIntegrator::render_scalar(Scene *scene) {
    return render_impl<false>(scene);
}
bool SamplingIntegrator::render_vector(Scene *scene) {
    return render_impl<true>(scene);
}

void SamplingIntegrator::render_block_scalar(
        const Scene *scene, const Sensor *sensor, Sampler *sampler,
        ImageBlock *block, const bool &stop,
        const std::vector<Point2u> &points) const {

    Float diff_scale_factor = 1.0f /
        std::sqrt((Float) sampler->sample_count());

    bool needs_aperture_sample = sensor->needs_aperture_sample();
    bool needs_time_sample = sensor->needs_time_sample();

    RadianceSample3f r_rec(scene, sampler);
    Point2f aperture_sample(0.5f);
    Float time_sample = 0.5f;
    RayDifferential3f sensor_ray;

    block->clear();
    // For each pixel of the block
    for (size_t i = 0; i < points.size(); ++i) {
        Point2i offset = Point2i(points[i]) + Vector2i(block->offset());
        if (stop)
            break;

        sampler->generate(offset);

        for (size_t j = 0; j < sampler->sample_count(); j++) {
            r_rec.new_query(sensor->medium());
            Point2f sample_pos(Point2f(offset)
                               + Vector2f(r_rec.next_sample_2d()));

            if (needs_aperture_sample)
                aperture_sample = r_rec.next_sample_2d();
            if (needs_time_sample)
                time_sample = r_rec.next_sample_1d();

            Spectrumf spec;
            RayDifferential3f sensor_ray;
            std::tie(sensor_ray, spec) = sensor->sample_ray_differential(
                sample_pos, aperture_sample, time_sample);

            sensor_ray.scale_differential(diff_scale_factor);

            spec *= Li(sensor_ray, r_rec);
            block->put(sample_pos, spec, r_rec.alpha, true);
            sampler->advance();
        }
    }
}

void SamplingIntegrator::render_block_vector(
        const Scene *scene, const Sensor *sensor, Sampler *sampler,
        ImageBlock *block, const bool &stop,
        const std::vector<Point2u> &points) const {

    Float diff_scale_factor = 1.0f /
        std::sqrt((Float) sampler->sample_count());

    bool needs_aperture_sample = sensor->needs_aperture_sample();
    bool needs_time_sample = sensor->needs_time_sample();

    RadianceSample3fP r_rec(scene, sampler);
    Point2fP aperture_sample(0.5f);
    FloatP time_sample = 0.5f;
    RayDifferential3fP sensor_ray;

    block->clear();
    // For each pixel of the block
    for (size_t i = 0; i < points.size(); ++i) {
        Point2i offset = Point2i(points[i]) + Vector2i(block->offset());
        if (stop)
            break;

        sampler->generate(offset);

        // Generate and handle PacketSize samples at once.
        // TODO: use dynamic arrays instead.
        size_t n_packets = (size_t)std::ceil(
                sampler->sample_count() / (Float)PacketSize);
        for (size_t j = 0; j < n_packets; j++) {
            r_rec.new_query(sensor->medium());

            Point2fP sample_pos(Point2fP(offset)
                                + Vector2fP(r_rec.next_sample_2d()));

            if (needs_aperture_sample)
                aperture_sample = r_rec.next_sample_2d();
            if (needs_time_sample)
                time_sample = r_rec.next_sample_1d();

            SpectrumfP spec;
            RayDifferential3fP sensor_ray;
            std::tie(sensor_ray, spec) = sensor->sample_ray_differential(
                sample_pos, aperture_sample, time_sample);

            sensor_ray.scale_differential(diff_scale_factor);

            spec *= Li(sensor_ray, r_rec, true);
            block->put(sample_pos, spec, r_rec.alpha, true);
            sampler->advance();
        }
    }
}

MonteCarloIntegrator::MonteCarloIntegrator(const Properties &props)
    : SamplingIntegrator(props) {
    /// Depth to begin using russian roulette
    m_rr_depth = props.int_("rr_depth", 5);
    if (m_rr_depth <= 0) {
        Log(EError, "'rr_depth' must be set to a value greater than zero!"
                    " Found %i instead.", m_rr_depth);
    }

    /** Longest visualized path depth (\c -1 = infinite).
      * A value of \c 1 will visualize only directly visible light sources.
      * \c 2 will lead to single-bounce (direct-only) illumination, and so on.
      */
    m_max_depth = props.int_("max_depth", -1);
    if (m_max_depth <= 0 && m_max_depth != -1) {
        Log(EError, "'max_depth' must be set to -1 (infinite) or a value"
                    " greater than zero! Found %i instead.", m_max_depth);
    }

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

    /**
     * When this flag is set to true, contributions from directly
     * visible emitters will not be included in the rendered image
     */
    m_hide_emitters = props.bool_("hide_emitters", false);
}

// MonteCarloIntegrator::MonteCarloIntegrator(Stream *stream, InstanceManager *manager)
//     : SamplingIntegrator(stream, manager) {
//     m_rr_depth = stream->readInt();
//     m_max_depth = stream->readInt();
//     m_strict_normals = stream->readBool();
//     m_hide_emitters = stream->readBool();
// }

// void MonteCarloIntegrator::serialize(Stream *stream, InstanceManager *manager) const {
//     SamplingIntegrator::serialize(stream, manager);
//     stream->writeInt(m_rr_depth);
//     stream->writeInt(m_max_depth);
//     stream->writeBool(m_strict_normals);
//     stream->writeBool(m_hide_emitters);
// }

MTS_IMPLEMENT_CLASS(Integrator, Object)
MTS_IMPLEMENT_CLASS(SamplingIntegrator, Integrator)
MTS_IMPLEMENT_CLASS(MonteCarloIntegrator, SamplingIntegrator)

// =============================================================
//! @{ \name Explicit template instantiations
// =============================================================
template MTS_EXPORT_RENDER bool Integrator::render<false>(Scene *);
template MTS_EXPORT_RENDER bool Integrator::render<true>(Scene *);
template MTS_EXPORT_RENDER void SamplingIntegrator::render_block<false>(
    const Scene *, const Sensor *, Sampler *, ImageBlock *, const bool &,
    const std::vector<Point2u> &) const;
template MTS_EXPORT_RENDER void SamplingIntegrator::render_block<true>(
    const Scene *, const Sensor *, Sampler *, ImageBlock *, const bool &,
    const std::vector<Point2u> &) const;

//! @}
// =============================================================

NAMESPACE_END(mitsuba)
