#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
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
 * This is the base class of all integrators; it does not make any assumptions on
 * how radiance is computed, which allows for many different kinds of implementations
 * ranging from software-based path tracing and Markov-Chain based techniques such
 * as Metropolis Light Transport up to hardware-accelerated rasterization.
 */
class MTS_EXPORT_RENDER Integrator : public Object {
public:
    /// Perform the main rendering job
    virtual bool render(Scene *scene, bool vectorize) = 0;

    /**
     * \brief Cancel a running render job
     *
     * This function can be called asynchronously to cancel a running render
     * job. In this case, \ref render() will quit with a return value of
     * \c false.
     */
    virtual void cancel() = 0;

    MTS_DECLARE_CLASS()
protected:
    /// Create an integrator
    Integrator(const Properties &props);

    /// Virtual destructor
    virtual ~Integrator() { }
};

/** \brief Abstract base class, which describes integrators
 * capable of computing samples of the scene's radiance function.
 */
class MTS_EXPORT_RENDER SamplingIntegrator : public Integrator {
public:
    /**
     * \brief Sample the incident radiance along a ray. The record passed is
     * used to store additional information about the result.
     */
    virtual Spectrumf eval(const RayDifferential3f &ray,
                           RadianceSample3f &rs) const = 0;

    /// Vectorized variant of \ref eval.
    virtual SpectrumfP eval(const RayDifferential3fP &ray,
                            RadianceSample3fP &rs,
                            MaskP active = true) const = 0;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref eval()
    Spectrumf eval(const RayDifferential3f &ray, RadianceSample3f &rs,
                   bool /* unused */) const {
        return eval(ray, rs);
    }

    // =========================================================================
    //! @{ \name Integrator interface implementation
    // =========================================================================

    bool render(Scene *scene, bool vectorize) override;
    void cancel() override;

    //! @}
    // =========================================================================

    MTS_DECLARE_CLASS()
protected:
    SamplingIntegrator(const Properties &props);
    virtual ~SamplingIntegrator();

    void render_block_scalar(const Scene *scene, Sampler *sampler,
                             ImageBlock *block) const;

    void render_block_vector(const Scene *scene, Sampler *sampler,
                             ImageBlock *block, Point2fX &points) const;

protected:
    bool m_stop;
    size_t m_block_size;
};

/*
 * \brief Base class of all recursive Monte Carlo integrators, which compute
 * unbiased solutions to the rendering equation (and optionally the radiative
 * transfer equation).
 */
class MTS_EXPORT_RENDER MonteCarloIntegrator : public SamplingIntegrator {
public:
    MTS_DECLARE_CLASS()
protected:
    /// Create an integrator
    MonteCarloIntegrator(const Properties &props);

    /// Virtual destructor
    virtual ~MonteCarloIntegrator();
protected:
    int m_max_depth;
    int m_rr_depth;
    bool m_strict_normals;
};

/// Instantiates concrete scalar and packet versions of the endpoint plugin API
#define MTS_IMPLEMENT_INTEGRATOR()                                             \
    Spectrumf eval(const RayDifferential3f &ray, RadianceSample3f &rs)         \
        const override {                                                       \
        return eval_impl(ray, rs, true);                                       \
    }                                                                          \
    SpectrumfP eval(const RayDifferential3fP &ray, RadianceSample3fP &rs,      \
                    MaskP active) const override {                             \
        return eval_impl(ray, rs, active);                                     \
    }

NAMESPACE_END(mitsuba)
