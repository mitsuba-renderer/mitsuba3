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

/*
 * \brief This macro should be used in the definition of BSDF
 * plugins to instantiate concrete versions of the the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_INTEGRATOR()                                             \
    Spectrumf Li(const RayDifferential3f &r,                                   \
                 RadianceSample3f &r_rec) const override {                     \
        return Li_impl(r, r_rec, true);                                        \
    }                                                                          \
    SpectrumfP Li(const RayDifferential3fP &r, RadianceSample3fP &r_rec,       \
                  const mask_t<FloatP> &active) const override {               \
        return Li_impl(r, r_rec, active);                                      \
    }

/**
 * \brief Abstract integrator base-class; does not make any assumptions on
 * how radiance is computed.
 *
 * In Mitsuba, the different rendering techniques are collectively referred to as
 * \a integrators, since they perform integration over a high-dimensional
 * space. Each integrator represents a specific approach for solving
 * the light transport equation---usually favored in certain scenarios, but
 * at the same time affected by its own set of intrinsic limitations.
 * Therefore, it is important to carefully select an integrator based on
 * user-specified accuracy requirements and properties of the scene to be
 * rendered.
 *
 * This is the base class of all integrators; it does not make any assumptions on
 * how radiance is computed, which allows for many different kinds of implementations
 * ranging from software-based path tracing and Markov-Chain based techniques such
 * as Metropolis Light Transport up to hardware-accelerated rasterization.
 *
 * \ingroup librender
 */
class MTS_EXPORT_RENDER Integrator : public Object {
public:
    /**
     * \brief Render the scene as seen by its default sensor.
     */
    template <bool Vectorize = true>
    bool render(Scene *scene) {
        if (Vectorize) return render_vector(scene);
        else return render_scalar(scene);
    }

    /**
     * \brief Cancel a running render job
     *
     * This function can be called asynchronously to cancel a running render
     * job. In this case, \ref render() will quit with a return value of
     * \c false.
     */
    virtual void cancel() = 0;

    /**
     * \brief Configure the sample generator for use with this integrator
     *
     * This function is called once after instantiation and can be used to
     * inform the sampler implementation about specific sample requirements
     * of this integrator.
     */
    virtual void configure_sampler(const Scene *scene, Sampler *sampler);

    // /// Serialize this integrator to a binary data stream
    // void serialize(Stream *stream, InstanceManager *manager) const;

    MTS_DECLARE_CLASS()
protected:
    /// Create an integrator
    Integrator(const Properties &props);

    // /// Unserialize an integrator
    // Integrator(Stream *stream, InstanceManager *manager);

    /// Virtual destructor
    virtual ~Integrator() { }

    /// \brief Render the scene as seen by its default sensor.
    virtual bool render_scalar(Scene *scene) = 0;
    /// Vector version of \ref render_scalar.
    virtual bool render_vector(Scene *scene) = 0;
};

/** \brief Abstract base class, which describes integrators
 * capable of computing samples of the scene's radiance function.
 * \ingroup librender
 */
class MTS_EXPORT_RENDER SamplingIntegrator : public Integrator {
public:
    /**
     * \brief Sample the incident radiance along a ray. The record passed is
     * used to store extra information about the result, if appropriate.
     */
    virtual Spectrumf Li(const RayDifferential3f &ray,
                         RadianceSample3f &r_rec) const = 0;
    /// \see \ref Li.
    Spectrumf Li(const RayDifferential3f &ray, RadianceSample3f &r_rec,
                 const bool &/*unused*/) const {
        return Li(ray, r_rec);
    }
    /// Vectorized variant of \ref Li.
    virtual SpectrumfP Li(const RayDifferential3fP &ray,
                          RadianceSample3fP &r_rec,
                          const mask_t<FloatP> &active) const = 0;

    /**
     * This can be called asynchronously to cancel a running render job.
     * In this case, <tt>render()</tt> will quit with a return value of
     * <tt>false</tt>.
     */
    void cancel() override;

    /**
     * This method does the main work of <tt>render()</tt> and
     * can be called in parallel, to work concurrently on different image blocks.
     *
     * \param scene
     *    Pointer to the underlying scene
     * \param sensor
     *    Pointer to the sensor used to render the image
     * \param sampler
     *    Pointer to the sampler used to render the image
     * \param block
     *    Pointer to the image block to be rendered
     * \param stop
     *    Reference to a boolean, which will be set to true when
     *    the user has requested that the program be stopped
     * \param points
     *    Specifies the traversal order, e.g. using a space-filling
     *    curve. To limit the size of the array, it is currently assumed
     *    that the block size is smaller than 256x256
     */
    template <bool Vectorize = true>
    void render_block(const Scene *scene, const Sensor *sensor,
                      Sampler *sampler, ImageBlock *block, const bool &stop,
                      const std::vector<Point2u> &points) const {
        if (Vectorize)
            return render_block_vector(scene, sensor, sampler, block, stop, points);
        else
            return render_block_scalar(scene, sensor, sampler, block, stop, points);
    }

    // /// Serialize this integrator to a binary data stream
    // void serialize(Stream *stream, InstanceManager *manager) const;

    MTS_DECLARE_CLASS()
protected:
    /// Create an integrator
    SamplingIntegrator(const Properties &props);

    // /// Unserialize an integrator
    // SamplingIntegrator(Stream *stream, InstanceManager *manager);

    /// Virtual destructor
    virtual ~SamplingIntegrator() { }

    /**
     * \brief Perform the main rendering task
     *
     * The work is automatically parallelized to multiple cores and
     * remote machines. The default implementation uniformly generates
     * samples on the sensor aperture and image plane as specified by
     * the used sampler. The average of the estimated radiance along the
     * associated rays in a pixel region is then taken as an approximation
     * of that pixel's radiance value. For adaptive strategies, have a look at
     * the \c adaptive plugin, which is an extension of this class.
     */
    bool render_scalar(Scene *scene) override;
    /// Vector version of \ref render_scalar.
    bool render_vector(Scene *scene) override;

    /// Scalar implementation of \ref render_block.
    virtual void render_block_scalar(const Scene *scene, const Sensor *sensor,
        Sampler *sampler, ImageBlock *block, const bool &stop,
        const std::vector<Point2u> &points) const;
    /// Vector implementation of \ref render_block.
    virtual void render_block_vector(const Scene *scene, const Sensor *sensor,
        Sampler *sampler, ImageBlock *block, const bool &stop,
        const std::vector<Point2u> &points) const;

private:
    /// See \ref render_scalar.
    template <bool Vectorize>
    bool render_impl(Scene *scene);
};

/*
 * \brief Base class of all recursive Monte Carlo integrators, which compute
 * unbiased solutions to the rendering equation (and optionally
 * the radiative transfer equation).
 * \ingroup librender
 */
class MTS_EXPORT_RENDER MonteCarloIntegrator : public SamplingIntegrator {
public:
    // /// Serialize this integrator to a binary data stream
    // void serialize(Stream *stream, InstanceManager *manager) const;

    MTS_DECLARE_CLASS()
protected:
    /// Create an integrator
    MonteCarloIntegrator(const Properties &props);

    // /// Unserialize an integrator
    // MonteCarloIntegrator(Stream *stream, InstanceManager *manager);

    /// Virtual destructor
    virtual ~MonteCarloIntegrator() { }
protected:
    int m_max_depth;
    int m_rr_depth;
    bool m_strict_normals;
    bool m_hide_emitters;
};

NAMESPACE_END(mitsuba)
