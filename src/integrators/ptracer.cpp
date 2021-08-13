#include <random>

#include <enoki/morton.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/progress.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scatteringintegrator.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-ptracer:

Path tracer (:monosp:`ptracer`)
-------------------------------

.. pluginparameters::

 * - max_depth
   - |int|
   - Specifies the longest path depth in the generated output image (where -1 corresponds to
     :math:`\infty`). A value of 1 will only render directly visible light sources. 2 will lead
     to single-bounce (direct-only) illumination, and so on. (Default: -1)
 * - rr_depth
   - |int|
   - Specifies the minimum path depth, after which the implementation will start to use the
     *russian roulette* path termination criterion. (Default: 5)
 * - hide_emitters
   - |bool|
   - Hide directly visible emitters. (Default: no, i.e. |false|)
 * - samples_per_pass
   - |bool|
   - If specified, divides the workload in successive passes with :paramtype:`samples_per_pass`
     samples per pixel.


This integrator traces rays starting from light sources and attempts to connect them
to the sensor at each bounce.
It does not support media (volumes).

Usually, this is a relatively useless rendering technique due to its high variance, but there
are some cases where it excels. In particular, it does a good job on scenes where most scattering
events are directly visible to the camera.

Note that unlike sensor-based integrators such as :ref:`path <integrator-path>`, it is not
possible to divide the workload in image-space tiles. The :paramtype:`samples_per_pass` parameter
allows splitting work in successive passes of the given sample count per pixel. It is particularly
useful in wavefront mode.
 */
template <typename Float, typename Spectrum>
class ParticleTracerIntegrator : public ScatteringIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ScatteringIntegrator, m_samples_per_pass, m_hide_emitters,
                    m_rr_depth, m_max_depth)
    MTS_IMPORT_TYPES(Scene, Sensor, Film, Sampler, ImageBlock, Emitter,
                     EmitterPtr, BSDF, BSDFPtr)

    ParticleTracerIntegrator(const Properties &props) : Base(props) { }

    void sample(const Scene *scene, const Sensor *sensor, Sampler *sampler,
                ImageBlock *block, Mask /*active*/) const override {
        if (likely(m_max_depth != 0) && !m_hide_emitters) {
            // Account for emitters directly visible from the sensor
            sample_visible_emitters(scene, sensor, sampler, block);
        }

        // Primary & further bounces illumination
        auto [ray, throughput] = prepare_ray(scene, sensor, sampler);
        trace_light_ray(ray, scene, sensor, sampler, throughput, block);
    }


    /**
     * Samples an emitter in the scene and connects it directly to the sensor,
     * splatting the emitted radiance to the given image block.
     */
    Spectrum sample_visible_emitters(const Scene *scene, const Sensor *sensor,
                                     Sampler *sampler, ImageBlock *block,
                                     Mask active = true) const {
        // 1. Time sampling
        Float time = sensor->shutter_open();
        if (sensor->shutter_open_time() > 0)
            time += sampler->next_1d(active) * sensor->shutter_open_time();

        // 2. Emitter sampling (select one emitter)
        auto [emitter_idx, emitter_idx_weight, _] =
            scene->sample_emitter(sampler->next_1d(active), active);
        EmitterPtr emitter =
            ek::gather<EmitterPtr>(scene->emitters_ek(), emitter_idx, active);

        // Don't connect delta emitters with sensor (both position and direction)
        active &= !has_flag(emitter->flags(), EmitterFlags::Delta);

        // 3. Emitter position sampling
        Spectrum emitter_weight = ek::zero<Spectrum>();
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();

        // 3.a. Infinite emitters
        Mask is_infinite = has_flag(emitter->flags(), EmitterFlags::Infinite);
        Mask active_e = active && is_infinite;
        if (ek::any_or<true>(active_e)) {
            /* We are sampling a direction toward an envmap emitter starting
            * from the center of the scene. This is because the sensor is
            * not part of the scene's bounding box, which could cause issues.
            */
            Interaction3f ref_it(0.f, time, ek::zero<Wavelength>(),
                                sensor->world_transform().translation());

            auto [ds, dir_weight] = emitter->sample_direction(
                ref_it, sampler->next_2d(active), active_e);
            /* Note: `dir_weight` already includes the emitter radiance, but
            * that will be accounted for again when sampling the wavelength
            * below. Instead, we recompute just the factor due to the PDF.
            * Also, convert to area measure. */
            emitter_weight[active_e] =
                ek::select(ds.pdf > 0.f, 1.f / ds.pdf, 0.f) * ek::sqr(ds.dist);
            si[active_e] = SurfaceInteraction3f(ds, ref_it.wavelengths);
        }

        // 3.b. Finite emitters
        active_e = active && !is_infinite;
        if (ek::any_or<true>(active_e)) {
            auto [ps, pos_weight] =
                emitter->sample_position(time, sampler->next_2d(active), active_e);
            emitter_weight[active_e] = pos_weight;
            si[active_e] = SurfaceInteraction3f(ps, ek::zero<Wavelength>());
        }

        /* 4. Connect to the sensor.
        * Query sensor for a direction connecting to `si.p`. This also gives
        * us UVs on the sensor (for splatting). The resulting direction points
        * from si.p (on the emitter) toward the sensor. */
        auto [sensor_ds, sensor_weight] = sensor->sample_direction(si, sampler->next_2d(), active);
        si.wi = sensor_ds.d;

        // 5. Sample spectrum of the emitter (accounts for its radiance)
        auto [wavelengths, wav_weight] =
            emitter->sample_wavelengths(si, sampler->next_1d(active), active);
        si.wavelengths = wavelengths;
        si.shape       = emitter->shape();

        Spectrum weight = emitter_idx_weight * emitter_weight * wav_weight * sensor_weight;

        // No BSDF passed (should not evaluate it since there's no scattering)
        return connect_sensor(scene, si, sensor_ds, nullptr, weight, block, active);
    }

    /**
     * Samples a ray from a random emitter in the scene.
     */
    std::pair<Ray3f, Spectrum> prepare_ray(const Scene *scene,
                                           const Sensor *sensor,
                                           Sampler *sampler,
                                           Mask active = true) const {
        Float time = sensor->shutter_open();
        if (sensor->shutter_open_time() > 0)
            time += sampler->next_1d(active) * sensor->shutter_open_time();

        /* ---------------------- Sample ray from emitter ------------------- */
        // Prepare random samples.
        Float wavelength_sample  = sampler->next_1d(active);
        Point2f direction_sample = sampler->next_2d(active);
        Point2f position_sample  = sampler->next_2d(active);
        // Sample one ray from an emitter in the scene.
        auto [ray, ray_weight, emitter] = scene->sample_emitter_ray(
            time, wavelength_sample, direction_sample, position_sample, active);
        return std::make_pair(ray, ray_weight);
    }

    /**
     * Intersects the given ray with the scene and recursively trace using
     * BSDF sampling. The given `throughput` should account for emitted
     * radiance from the sampled light source, wavelengths sampling weights,
     * etc. At each interaction, we attempt connecting to the sensor and add
     * the current radiance to the given `block`.
     *
     * Note: this will *not* account for directly visible emitters, since
     * they require a direct connection from the emitter to the sensor. See
     * \ref sample_visible_emitters.
     *
     * \return The radiance along the ray and an alpha value.
     */
    std::pair<Spectrum, Float>
    trace_light_ray(Ray3f ray, const Scene *scene, const Sensor *sensor,
                    Sampler *sampler, Spectrum throughput, ImageBlock *block,
                    Mask active = true) const {
        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);

        Int32 depth = 1;

        /* ---------------------- Path construction ------------------------- */
        // First intersection from the emitter to the scene
        SurfaceInteraction3f si = scene->ray_intersect(ray, active);

        active &= si.is_valid();
        if (m_max_depth >= 0)
            active &= depth < m_max_depth;

        /* Set up an Enoki loop (optimizes away to a normal loop in scalar mode,
           generates wavefront or megakernel renderer based on configuration).
           Register everything that changes as part of the loop here */
        ek::Loop<Float> loop("Particle Tracer Integrator");
        loop.put(active, depth, ray, throughput, si, eta);

        // The internal sampler state is also modified by the loop
        sampler->loop_register(loop);
        loop.init();

        ek::eval(block->data());

        // Incrementally build light path using BSDF sampling.
        while (loop(ek::detach(active))) {
            BSDFPtr bsdf = si.bsdf(ray);

            // Connect to sensor and splat if successful.
            // Sample a direction from the sensor to the current surface point.
            auto [sensor_ds, sensor_weight] =
                sensor->sample_direction(si, sampler->next_2d(), active);
            connect_sensor(scene, si, sensor_ds, bsdf,
                           throughput * sensor_weight, block, active);

            /* ----------------------- BSDF sampling ------------------------ */
            // Sample BSDF * cos(theta).
            BSDFContext ctx(TransportMode::Importance);
            auto [bs, bsdf_val] =
                bsdf->sample(ctx, si, sampler->next_1d(active),
                             sampler->next_2d(active), active);
            // Using geometric normals (wo points to the camera)
            Float wi_dot_geo_n = ek::dot(si.n, -ray.d),
                  wo_dot_geo_n = ek::dot(si.n, si.to_world(bs.wo));
            // Prevent light leaks due to shading normals
            active &= (wi_dot_geo_n * Frame3f::cos_theta(si.wi) > 0.f) &&
                      (wo_dot_geo_n * Frame3f::cos_theta(bs.wo) > 0.f);
            // Adjoint BSDF for shading normals -- [Veach, p. 155]
            Float correction = ek::abs((Frame3f::cos_theta(si.wi) * wo_dot_geo_n) /
                                       (Frame3f::cos_theta(bs.wo) * wi_dot_geo_n));
            throughput *= bsdf_val * correction;
            eta *= bs.eta;

            active &= ek::any(ek::neq(unpolarized_spectrum(throughput), 0.f));
            if (ek::none_or<false>(active))
                break;

            // Intersect the BSDF ray against scene geometry (next vertex).
            ray = si.spawn_ray(si.to_world(bs.wo));
            si = scene->ray_intersect(ray, active);

            depth++;
            if (m_max_depth >= 0)
                active &= depth < m_max_depth;
            active &= si.is_valid();

            // Russian Roulette
            Mask use_rr = depth > m_rr_depth;
            if (ek::any_or<true>(use_rr)) {
                Float q = ek::min(
                    ek::hmax(unpolarized_spectrum(throughput)) * ek::sqr(eta), 0.95f);
                ek::masked(active, use_rr) &= sampler->next_1d(active) < q;
                ek::masked(throughput, use_rr) *= ek::rcp(q);
            }
        }

        return { throughput, 1.f };
    }

    /**
     * Attempt connecting the given point to the sensor.
     *
     * If the point to connect is on the surface (non-null `bsdf` values),
     * evaluate the BSDF in the direction of the sensor.
     *
     * Finally, splat `weight` (with all appropriate factors) to the
     * given image block.
     *
     * \return The quantity that was accumulated to the block.
     */
    Spectrum connect_sensor(const Scene *scene, const SurfaceInteraction3f &si,
                            const DirectionSample3f &sensor_ds,
                            const BSDFPtr &bsdf, const Spectrum &weight,
                            ImageBlock *block, Mask active) const {
        active &= (sensor_ds.pdf > 0.f) &&
                  ek::any(ek::neq(unpolarized_spectrum(weight), 0.f));
        if (ek::none_or<false>(active))
            return 0.f;

        // Check that sensor is visible from current position (shadow ray).
        Ray3f sensor_ray = si.spawn_ray_to(sensor_ds.p);
        active &= !scene->ray_test(sensor_ray, active);
        if (ek::none_or<false>(active))
            return 0.f;

        // Foreshortening term and BSDF value for that direction (for surface interactions).
        Spectrum result = 0.f;
        Spectrum surface_weight = 1.f;
        Vector3f local_d        = si.to_local(sensor_ray.d);
        Mask on_surface         = active && ek::neq(si.shape, nullptr);
        if (ek::any_or<true>(on_surface)) {
            // Note that foreshortening is only missing for directly visible
            // emitters associated with a shape. Otherwise it's included in the BSDF.
            // Clamp negative cosines (zero value if behind the surface).
            surface_weight[on_surface && ek::eq(bsdf, nullptr)] *=
                ek::max(0.f, Frame3f::cos_theta(local_d));

            on_surface &= ek::neq(bsdf, nullptr);
            if (ek::any_or<true>(on_surface)) {
                BSDFContext ctx(TransportMode::Importance);
                // Using geometric normals
                Float wi_dot_geo_n = ek::dot(si.n, si.to_world(si.wi)),
                      wo_dot_geo_n = ek::dot(si.n, sensor_ray.d);
                // Prevent light leaks due to shading normals
                Mask valid = (wi_dot_geo_n * Frame3f::cos_theta(si.wi) > 0.f) &&
                             (wo_dot_geo_n * Frame3f::cos_theta(local_d) > 0.f);

                // Adjoint BSDF for shading normals -- [Veach, p. 155]
                Float correction = ek::select(
                    valid,
                    ek::abs((Frame3f::cos_theta(si.wi) * wo_dot_geo_n) /
                               (Frame3f::cos_theta(local_d) * wi_dot_geo_n)),
                    0.f);

                surface_weight[on_surface] *=
                    correction * bsdf->eval(ctx, si, local_d, on_surface);
            }
        }

        // Even if the ray is not coming from a surface (no foreshortening),
        // we still don't want light coming from behind the emitter.
        Mask not_on_surface = active && ek::eq(si.shape, nullptr) && ek::eq(bsdf, nullptr);
        if (ek::any_or<true>(not_on_surface)) {
            Mask invalid_side = Frame3f::cos_theta(local_d) <= 0.f;
            surface_weight[not_on_surface && invalid_side] = 0.f;
        }

        result = weight * surface_weight;

        // Splatting
        Float alpha = ek::select(ek::neq(bsdf, nullptr), 1.f, 0.f);
        block->put(sensor_ds.uv, si.wavelengths, result, alpha, active);

        return result;
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("ParticleTracerIntegrator[\n"
                           "  max_depth = %i,\n"
                           "  rr_depth = %i\n"
                           "]",
                           m_max_depth, m_rr_depth);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(ParticleTracerIntegrator, ScatteringIntegrator);
MTS_EXPORT_PLUGIN(ParticleTracerIntegrator, "Particle Tracer integrator");
NAMESPACE_END(mitsuba)
