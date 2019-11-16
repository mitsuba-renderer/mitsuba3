#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/tls.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract integrator base-class; does not make any assumptions on
 * how radiance is computed.
 *
 * In Mitsuba, the different rendering techniques are collectively referred to
 * as \a integrators, since they perform integration over a high-dimensional
 * space. Each integrator represents a specific approach for solving the light
 * transport equation---usually favored in certain scenarios, but at the same
 * time affected by its own set of intrinsic limitations. Therefore, it is
 * important to carefully select an integrator based on user-specified accuracy
 * requirements and properties of the scene to be rendered.
 *
 * This is the base class of all integrators; it does not make any assumptions
 * on how radiance is computed, which allows for many different kinds of
 * implementations.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Integrator : public Object {
public:
    MTS_DECLARE_CLASS_VARIANT(Integrator, Object, "integrator")
    MTS_IMPORT_TYPES(Scene, Sensor)

    /// Perform the main rendering job. Returns \c true upon success
    virtual bool render(Scene *scene, Sensor *sensor) = 0;

    /**
     * \brief Cancel a running render job
     *
     * This function can be called asynchronously to cancel a running render
     * job. In this case, \ref render() will quit with a return value of \c
     * false.
     */
    virtual void cancel() = 0;

protected:
    /// Create an integrator
    Integrator(const Properties & /*props*/) {}

    /// Virtual destructor
    virtual ~Integrator() { }
};

/** \brief Abstract base class, which describes integrators
 * capable of computing samples of the scene's radiance function.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER SamplingIntegrator : public Integrator<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SamplingIntegrator, Integrator)
    MTS_IMPORT_TYPES()
    using Base       = Integrator<Float, Spectrum>;
    using Film       = typename RenderAliases::Film;
    using ImageBlock = typename RenderAliases::ImageBlock;
    using Sampler    = typename RenderAliases::Sampler;
    using Scene      = typename RenderAliases::Scene;
    using Sensor     = typename RenderAliases::Sensor;

    /**
     * \brief Sample the incident radiance along a ray.
     *
     * \param scene
     *    The underlying scene in which the radiance function should be sampled
     *
     * \param sampler
     *    A source of (pseudo-/quasi-) random numbers
     *
     * \param ray
     *    A ray, optionally with differentials
     *
     * \param active
     *    A mask that indicates which SIMD lanes are active
     *
     * \return
     *    A pair containing a spectrum and a mask specifying whether a surface
     *    or medium interaction was sampled. False mask entries indicate that
     *    the ray "escaped" the scene, in which case the the returned spectrum
     *    contains the contribution of environment maps, if present.
     */
    virtual std::pair<Spectrum, Mask> sample(const Scene *scene, Sampler *sampler,
                                             const RayDifferential3f &ray, Mask active) const;

    // =========================================================================
    //! @{ \name Integrator interface implementation
    // =========================================================================

    bool render(Scene *scene, Sensor *sensor) override;
    void cancel() override;

    /**
     * Indicates whether \ref cancel() or a timeout have occured. Should be
     * checked regularly in the integrator's main loop so that timeouts are
     * enforced accurately.
     *
     * Note that accurate timeouts rely on \ref m_render_timer, which needs
     * to be reset at the beginning of the rendering phase.
     */
    bool should_stop() const {
        return m_stop || (m_timeout > 0.f &&
                          m_render_timer.value() > 1000.f * m_timeout);
    }

    //! @}
    // =========================================================================

protected:
    SamplingIntegrator(const Properties &props);
    virtual ~SamplingIntegrator();

    virtual void render_block_scalar(const Scene *scene,
                                     const Sensor *sensor,
                                     Sampler *sampler,
                                     ImageBlock *block,
                                     size_t sample_count = size_t(-1)) const;

protected:
    /// Integrators should stop all work when this flag is set to true.
    bool m_stop;

    /// Size of (square) image blocks to render per core.
    uint32_t m_block_size;

    /**
     * \brief Number of samples to compute for each pass over the image blocks.
     *
     * Must be a multiple of the total sample count per pixel.
     * If set to (size_t) -1, all the work is done in a single pass (default).
     */
    uint32_t m_samples_per_pass;

    /** Maximum amount of time to spend rendering (excluding scene parsing).
     * Specified in seconds. A negative values indicates no timeout. */
    float m_timeout;

    /// Timer used to enforce the timeout.
    Timer m_render_timer;
};

/*
 * \brief Base class of all recursive Monte Carlo integrators, which compute
 * unbiased solutions to the rendering equation (and optionally the radiative
 * transfer equation).
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER MonteCarloIntegrator : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(MonteCarloIntegrator, SamplingIntegrator)
    MTS_IMPORT_BASE(SamplingIntegrator)

protected:
    /// Create an integrator
    MonteCarloIntegrator(const Properties &props);

    /// Virtual destructor
    virtual ~MonteCarloIntegrator();

protected:
    int m_max_depth;
    int m_rr_depth;
};

MTS_EXTERN_CLASS(Integrator)
MTS_EXTERN_CLASS(SamplingIntegrator)
MTS_EXTERN_CLASS(MonteCarloIntegrator)
NAMESPACE_END(mitsuba)
