#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/medium.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract integrator base class, which does not make any assumptions
 * with regards to how radiance is computed.
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
class MI_EXPORT_LIB Integrator : public Object {
public:
    MI_IMPORT_TYPES(Scene, Sensor)

    /**
     * \brief Render the scene
     *
     * This function renders the scene from the viewpoint of \c sensor. All
     * other parameters are optional and control different aspects of the
     * rendering process. In particular:
     *
     * \param seed
     *     This parameter controls the initialization of the random number
     *     generator. It is crucial that you specify different seeds (e.g., an
     *     increasing sequence) if subsequent \c render() calls should produce
     *     statistically independent images.
     *
     * \param spp
     *     Set this parameter to a nonzero value to override the number of
     *     samples per pixel. This value then takes precedence over whatever
     *     was specified in the construction of <tt>sensor->sampler()</tt>.
     *     This parameter may be useful in research applications where an image
     *     must be rendered multiple times using different quality levels.
     *
     * \param develop
     *     If set to \c true, the implementation post-processes the data stored
     *     in <tt>sensor->film()</tt>, returning the resulting image as a
     *     \ref TensorXf. Otherwise, it returns an empty tensor.
     *
     * \param evaluate
     *     This parameter is only relevant for JIT variants of Mitsuba (LLVM,
     *     CUDA). If set to \c true, the rendering step evaluates the generated
     *     image and waits for its completion. A log message also denotes the
     *     rendering time. Otherwise, the returned tensor
     *     (<tt>develop=true</tt>) or modified film (<tt>develop=false</tt>)
     *     represent the rendering task as an unevaluated computation graph.
     */
    virtual TensorXf render(Scene *scene,
                            Sensor *sensor,
                            uint32_t seed = 0,
                            uint32_t spp = 0,
                            bool develop = true,
                            bool evaluate = true) = 0;

    /**
     * \brief Render the scene
     *
     * This function is just a thin wrapper around the previous \ref render()
     * overload. It accepts a sensor *index* instead and renders the scene
     * using sensor 0 by default.
     */
    TensorXf render(Scene *scene,
                    uint32_t sensor_index = 0,
                    uint32_t seed = 0,
                    uint32_t spp = 0,
                    bool develop = true,
                    bool evaluate = true);

    /// \brief Cancel a running render job (e.g. after receiving Ctrl-C)
    virtual void cancel();

    /**
     * Indicates whether \ref cancel() or a timeout have occurred. Should be
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

    /**
     * For integrators that return one or more arbitrary output variables
     * (AOVs), this function specifies a list of associated channel names. The
     * default implementation simply returns an empty vector.
     */
    virtual std::vector<std::string> aov_names() const;

    MI_DECLARE_CLASS()
protected:
    /// Create an integrator
    Integrator(const Properties & props);

    /// Virtual destructor
    virtual ~Integrator() { }

protected:
    /// Integrators should stop all work when this flag is set to true.
    bool m_stop;

    /**
     * \brief Maximum amount of time to spend rendering (excluding scene parsing).
     *
     * Specified in seconds. A negative values indicates no timeout.
     */
    float m_timeout;

    /// Timer used to enforce the timeout.
    Timer m_render_timer;

    /// Flag for disabling direct visibility of emitters
    bool m_hide_emitters;
};

/** \brief Abstract integrator that performs Monte Carlo sampling starting from
 * the sensor
 *
 * Subclasses of this interface must implement the \ref sample() method, which
 * performs Monte Carlo integration to return an unbiased statistical estimate
 * of the radiance value along a given ray.
 *
 * The \ref render() method then repeatedly invokes this estimator to compute
 * all pixels of the image.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB SamplingIntegrator : public Integrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Integrator, should_stop, aov_names,
                    m_stop, m_timeout, m_render_timer, m_hide_emitters)
    MI_IMPORT_TYPES(Scene, Sensor, Film, ImageBlock, Medium, Sampler)

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
     * \param medium
     *    If the ray is inside a medium, this parameter holds a pointer to that
     *    medium
     *
     * \param aov
     *    Integrators may return one or more arbitrary output variables (AOVs)
     *    via this parameter. If \c nullptr is provided to this argument, no
     *    AOVs should be returned. Otherwise, the caller guarantees that space
     *    for at least <tt>aov_names().size()</tt> entries has been allocated.
     *
     * \param active
     *    A mask that indicates which SIMD lanes are active
     *
     * \return
     *    A pair containing a spectrum and a mask specifying whether a surface
     *    or medium interaction was sampled. False mask entries indicate that
     *    the ray "escaped" the scene, in which case the the returned spectrum
     *    contains the contribution of environment maps, if present. The mask
     *    can be used to estimate a suitable alpha channel of a rendered image.
     *
     * \remark
     *    In the Python bindings, this function returns the \c aov output
     *    argument as an additional return value. In other words:
     *    <pre>
     *        (spec, mask, aov) = integrator.sample(scene, sampler, ray, medium, active)
     *    </pre>
     */
    virtual std::pair<Spectrum, Mask> sample(const Scene *scene,
                                             Sampler *sampler,
                                             const RayDifferential3f &ray,
                                             const Medium *medium = nullptr,
                                             Float *aovs = nullptr,
                                             Mask active = true) const;

    // =========================================================================
    //! @{ \name Integrator interface implementation
    // =========================================================================

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    uint32_t seed = 0,
                    uint32_t spp = 0,
                    bool develop = true,
                    bool evaluate = true) override;

    //! @}
    // =========================================================================

    MI_DECLARE_CLASS()
protected:
    SamplingIntegrator(const Properties &props);
    virtual ~SamplingIntegrator();

    virtual void render_block(const Scene *scene,
                              const Sensor *sensor,
                              Sampler *sampler,
                              ImageBlock *block,
                              Float *aovs,
                              uint32_t sample_count,
                              uint32_t seed,
                              uint32_t block_id,
                              uint32_t block_size) const;

    void render_sample(const Scene *scene,
                       const Sensor *sensor,
                       Sampler *sampler,
                       ImageBlock *block,
                       Float *aovs,
                       const Vector2f &pos,
                       ScalarFloat diff_scale_factor,
                       Mask active = true) const;

protected:

    /// Size of (square) image blocks to render in parallel (in scalar mode)
    uint32_t m_block_size;

    /**
     * \brief Number of samples to compute for each pass over the image blocks.
     *
     * Must be a multiple of the total sample count per pixel.
     * If set to (uint32_t) -1, all the work is done in a single pass (default).
     */
    uint32_t m_samples_per_pass;
};

/** \brief Abstract integrator that performs *recursive* Monte Carlo sampling
 * starting from the sensor
 *
 * This class is almost identical to \ref SamplingIntegrator. It stores two
 * additional fields that are helpful for recursive Monte Carlo techniques:
 * the maximum path depth, and the depth at which the Russian Roulette path
 * termination technique should start to become active.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB MonteCarloIntegrator
    : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)

protected:
    /// Create an integrator
    MonteCarloIntegrator(const Properties &props);

    /// Virtual destructor
    virtual ~MonteCarloIntegrator();

    MI_DECLARE_CLASS()
protected:
    uint32_t m_max_depth;
    uint32_t m_rr_depth;
};

/** \brief Abstract adjoint integrator that performs Monte Carlo sampling
 * starting from the emitters.
 *
 * Subclasses of this interface must implement the \ref sample() method, which
 * performs recursive Monte Carlo integration starting from an emitter and
 * directly accumulates the product of radiance and importance into the film.
 * The \ref render() method then repeatedly invokes this estimator to compute
 * the rendered image.
 *
 * \remark The adjoint integrator does not support renderings with arbitrary
 * output variables (AOVs).
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB AdjointIntegrator : public Integrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Integrator, should_stop, aov_names, m_stop, m_timeout,
                    m_render_timer, m_hide_emitters)
    MI_IMPORT_TYPES(Scene, Sensor, Film, BSDF, BSDFPtr, ImageBlock, Sampler,
                     EmitterPtr)

    /**
     * \brief Sample the incident importance and splat the product of
     * importance and radiance to the film.
     *
     * \param scene
     *    The underlying scene
     *
     * \param sensor
     *    A sensor from which rays should be sampled
     *
     * \param sampler
     *    A source of (pseudo-/quasi-) random numbers
     *
     * \param block
     *    An image block that will be updated during the sampling process
     *
     * \param sample_scale
     *    A scale factor that must be applied to each sample to account
     *    for the film resolution and number of samples.
     */
    virtual void sample(const Scene *scene, const Sensor *sensor,
                        Sampler *sampler, ImageBlock *block,
                        ScalarFloat sample_scale) const = 0;

    // =========================================================================
    //! @{ \name Integrator interface implementation
    // =========================================================================

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    uint32_t seed = 0,
                    uint32_t spp = 0,
                    bool develop = true,
                    bool evaluate = true) override;

    //! @}
    // =========================================================================

    MI_DECLARE_CLASS()

protected:
    /// Create an integrator
    AdjointIntegrator(const Properties &props);

    /// Virtual destructor
    virtual ~AdjointIntegrator();

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


MI_EXTERN_CLASS(Integrator)
MI_EXTERN_CLASS(SamplingIntegrator)
MI_EXTERN_CLASS(MonteCarloIntegrator)
MI_EXTERN_CLASS(AdjointIntegrator)
NAMESPACE_END(mitsuba)
