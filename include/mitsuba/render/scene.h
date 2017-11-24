#pragma once

#include <iostream>
#include <vector>
#include <mitsuba/core/ddistr.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/sensor.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Scene : public Object {
public:
    Scene(const Properties &props);

    // =============================================================
    //! @{ \name Ray tracing
    // =============================================================
    /**
     * Shoot a ray and get full information about any resulting intersection.
     *
     * \warning All fields of \c its may be overwritten, independently of the
     * \c active mask.
     */
    template <typename Ray, typename Value = typename Ray::Value>
    auto ray_intersect(const Ray &ray, const Value &mint, const Value &maxt,
                       SurfaceInteraction<typename Ray::Point> &its,
                       const mask_t<Value> &active) const {
        MTS_MAKE_KD_CACHE(cache);

        auto result = m_kdtree->ray_intersect_pbrt<false>(ray, mint, maxt,
                                                          (void*) cache, active);
        if (any(active & result.first)) {
            m_kdtree->fill_surface_interaction(ray, (void*) cache, its,
                                               active & result.first);
        }

        return result;
    }

    /// Shoot a shadow ray: only a boolean is returned (true iff there was an
    /// intersection).
    template <typename Ray, typename Value = typename Ray::Value>
    auto ray_intersect(const Ray &ray, const Value &mint, const Value &maxt,
                       const mask_t<Value> &active = true) const {
        return m_kdtree->ray_intersect_pbrt<true>(ray, mint, maxt, nullptr,
                                                  active).first;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling interface
    // =============================================================
    /**
     * \brief Direct illumination sampling routine
     *
     * Given an arbitrary reference point in the scene, this method samples a
     * position on an emitter that has a nonzero contribution towards that point.
     *
     * Ideally, the implementation should importance sample the product of
     * the emission profile and the geometry term between the reference point
     * and the position on the emitter.
     *
     * \param d_rec
     *    A direct illumination sampling record that specifies the
     *    reference point and a time value. After the function terminates,
     *    it will be populated with the position sample and related information
     *
     * \param sample
     *    A uniformly distributed 2D vector
     *
     * \param test_visibility
     *    When set to \c true, a shadow ray will be cast to ensure that the
     *    sampled emitter position and the reference point are mutually visible.
     *
     * \return
     *    An importance weight given by the radiance received along
     *    the sampled ray divided by the sample probability.
     */
    Spectrumf sample_emitter_direct(DirectSample3f &d_rec,
                                    const Point2f &sample,
                                    bool test_visibility = true) const;
    Spectrumf sample_emitter_direct(
            DirectSample3f &d_rec, const Point2f &sample, bool test_visibility,
            bool /*unused*/) const {
        return sample_emitter_direct(d_rec, sample, test_visibility);
    }
    /// Vectorized variant of \ref sample_emitter_direct
    SpectrumfP sample_emitter_direct(
            DirectSample3fP &d_rec, const Point2fP &sample,
            bool test_visibility, const mask_t<FloatP> &active) const;


    /**
     * \brief Evaluate the probability density of the \a direct sampling
     * method implemented by the \ref sample_emitter_direct() method.
     *
     * \param d_rec
     *    A direct sampling record, which specifies the query
     *    location. Note that this record need not be completely
     *    filled out. The important fields are \c p, \c n, \c ref,
     *    \c dist, \c d, \c measure, and \c uv.
     *
     * \param p
     *    The world-space position that would have been passed to \ref
     *    sample_emitter_direct()
     *
     * \return
     *    The density expressed with respect to the requested measure
     *    (usually \ref ESolidAngle)
     */
    Float pdf_emitter_direct(const DirectSample3f &/*d_rec*/) const {
        NotImplementedError("pdf_emitter_direct");
        return 0.0f;
    }

    Spectrumf sample_attenuated_emitter_direct(
            DirectSample3f &/*d_rec*/, const SurfaceInteraction3f &/*its*/,
            const Medium * /*medium*/, int &/*interactions*/, const Point2f &/*sample*/,
            Sampler * /*sampler*/ = nullptr) const {
        NotImplementedError("sample_attenuated_emitter_direct");
        return Spectrumf(0.0f);
    }

    /**
     * \brief Return the environment radiance for a ray that did not intersect
     * any of the scene objects.
     *
     * This is primarily meant for path tracing-style integrators.
     */
    template <typename RayDifferential,
              typename Spectrum = Spectrum<typename RayDifferential::Value>>
    Spectrum eval_environment(const RayDifferential &/*ray*/) const {
        // TODO: support environment emitters.
        return Spectrum(0.0f);
    }
    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Accessors
    // =============================================================
    /// Return the scene's KD-tree
    const ShapeKDTree *kdtree() const { return m_kdtree; }
    /// Return the scene's KD-tree
    ShapeKDTree *kdtree() { return m_kdtree; }

    /// Return the current sensor
    ref<Sensor> sensor() { return m_sensors.front(); }
    /// Return the current sensor
    const ref<Sensor> sensor() const { return m_sensors.front(); }

    /// Return the current emitter
    std::vector<ref<Emitter>> &emitters() { return m_emitters; }
    /// Return the current emitter
    const std::vector<ref<Emitter>> &emitters() const { return m_emitters; }

    /// Return the current sensor's film
    Film *film() { return m_sensors.front()->film(); }
    /// Return the current sensor's film
    const Film *film() const { return m_sensors.front()->film(); }

    /// Return the scene's sampler
    ref<Sampler> sampler() { return m_sampler; }
    /// Return the scene's sampler
    const ref<Sampler> sampler() const { return m_sampler; }

    /// Return the scene's integrator
    ref<Integrator> integrator() { return m_integrator; }
    /// Return the scene's integrator
    const ref<Integrator> integrator() const { return m_integrator; }

    /// Return the environment emitter, or nullptr is there is none.
    ref<Emitter> environment_emitter() {
        // Log(EWarn, "Scene::environment_emitter not implemented yet.");
        return nullptr;
    }
    /// Return the environment emitter, or nullptr is there is none.
    const ref<Emitter> environment_emitter() const {
        // Log(EWarn, "Scene::environment_emitter not implemented yet.");
        return nullptr;
    }
    //! @}
    // =============================================================

    /// Return a human-readable string representation of the scene contents.
    virtual std::string to_string() const override;

    MTS_DECLARE_CLASS()
protected:
    virtual ~Scene();

    ref<ShapeKDTree> m_kdtree;
    // TODO: properly handle multiple sensors ("active sensor index")
    std::vector<ref<Sensor>> m_sensors;
    std::vector<ref<Emitter>> m_emitters;
    ref<Sampler> m_sampler;
    ref<Integrator> m_integrator;

    /// Precomputed distribution of emitters' intensity.
    DiscreteDistribution m_emitters_pdf;

protected:
    template <typename DirectSample, typename Value = typename DirectSample::Value>
    auto sample_emitter_direct_impl(
        DirectSample &d_rec, const Point<Value, 2> &sample_,
        bool test_visibility, mask_t<Value> active) const;
};

NAMESPACE_END(mitsuba)

// This header needs to be included after the declaration of Scene to avoid a
// circular dependency.
#include <mitsuba/render/records.inl>
