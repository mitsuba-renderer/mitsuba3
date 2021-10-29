#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/render/integrator.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Base class for integrators that start paths from the light source and scatter
 * their radiance to sensors, as opposed integrators starting from sensors and
 * gathering radiance from light sources.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER ScatteringIntegrator : public Integrator<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Integrator, should_stop, aov_names, m_stop, m_timeout,
                    m_render_timer, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sensor, Film, BSDF, BSDFPtr, ImageBlock, Sampler, EmitterPtr)

    explicit ScatteringIntegrator(const Properties &props);
    virtual ~ScatteringIntegrator();

    /// Perform the main rendering job
    TensorXf render(Scene *scene, uint32_t seed, Sensor *sensor,
                    bool develop_film = true) override;

    /**
     * Samples a light path starting from a light source and attempts to
     * connect it to the given sensor at each surface interaction.
     * If the connection is successful, the corresponding radiance is
     * splatted directly to the given image block at the right position.
     */
    // TODO: support AOVs.
    // TODO: support media.
    virtual void sample(const Scene *scene, const Sensor *sensor,
                        Sampler *sampler, ImageBlock *block,
                        Mask active = true) const = 0;

    MTS_DECLARE_CLASS()

protected:
    /**
     * \brief Number of samples to compute for each pass over the image blocks.
     *
     * Must be a multiple of the total sample count per pixel.
     * If set to (size_t) -1, all the work is done in a single pass (default).
     */
    uint32_t m_samples_per_pass;

    /**
     * Longest visualized path depth (\c -1 = infinite).
     * A value of \c 1 will visualize only directly visible light sources.
     * \c 2 will lead to single-bounce (direct-only) illumination, and so on.
     */
    int m_max_depth;

    /// Depth to begin using russian roulette
    int m_rr_depth;
};

MTS_EXTERN_CLASS_RENDER(ScatteringIntegrator)
NAMESPACE_END(mitsuba)
