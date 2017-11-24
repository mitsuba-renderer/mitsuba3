#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: this is an MVP port, most features remain unimplemented. An interface
// rework is required for proper vectorization support.
class MTS_EXPORT_RENDER Sampler : public Object {
public:
    /**
     * \brief Create a clone of this sampler
     *
     * The clone is allowed to be different to some extent, e.g. a pseudorandom
     * generator should be based on a different random seed compared to the
     * original. All other parameters, are copied exactly.
     *
     * The default implementation throws an exception.
     */
    virtual ref<Sampler> clone() = 0;

    /**
     * \brief Generate new samples
     *
     * This function is called initially and every time the generated
     * samples have been exhausted. When used in conjunction with a
     * \ref SamplingIntegrator, this will be called before starting to
     * render each pixel, and the argument denotes the pixel position.
     * Otherwise, some dummy value should be provided, e.g. Point2i(-1).
     */
    virtual void generate(const Point2i &/*offset*/) {
        m_sample_index = 0;
        m_dimension_1d_array = m_dimension_2d_array = 0;
        m_dimension_1d_array_p = m_dimension_2d_array_p = 0;
    }

    /// Advance to the next sample
    virtual void advance() {
        m_sample_index++;
        m_dimension_1d_array = m_dimension_2d_array = 0;
        m_dimension_1d_array_p = m_dimension_2d_array_p = 0;
    }

    /// Retrieve the next component value from the current sample
    virtual Float next_1d() = 0;
    /// Retrieve the next packet of values from the current sample
    virtual FloatP next_1d_p(const mask_t<FloatP> &active = true) = 0;
    /// Retrieve the next two component values from the current sample
    virtual Point2f next_2d() = 0;
    /// Retrieve the next packet of 2D values from the current sample
    virtual Point2fP next_2d_p(const mask_t<FloatP> &active = true) = 0;
    /**
     * Automatically selects the right variant of <tt>next_...</tt> based on
     * the desired return type.
     *
     * Note that the mask type should always be 1D, even
     * when requesting 2D points. In particular, \c next<Point2f> takes
     * a \c mask_t<FloatP>, not \c mask_t<Point2f>.
     */
    template <typename T, typename Mask> T next(const Mask &active = true);


    /// See \ref request_2d_array.
    // TODO: vectorized variants
    void request_1d_array(size_t size);
    void request_1d_array_p(size_t size);

    /**
     * \brief Request that a 2D array will be made available for
     * later consumption by \ref next_2d_array().
     *
     * This function must be called before \ref generate().
     * See \ref next_2d_array() for a more detailed description
     * of this feature.
     */
    void request_2d_array(size_t size);
    void request_2d_array_p(size_t size);

    /**
     * Automatically selects the right variant among the request_*_array_*
     * methods, based on the template type.
     */
    template <typename T> void request_array(size_t size);

    /// See \ref next_2d_array.
    Float *next_1d_array(size_t size);
    /// Vectorized variant of \ref next_1d_array.
    FloatP *next_1d_array_p(size_t /*size*/);

    /**
     * \brief Retrieve the next 2D array of values from the current sample.
     *
     * Note that this is different from just calling \ref next_2d()
     * repeatedly - this function will generally return a set of 2D vectors,
     * which are not only well-laid out over all samples at the current pixel,
     * but also with respect to each other. Note that this 2D array has to be
     * requested initially using \ref request_2d_array() and later, they have
     * to be retrieved in the same same order and size configuration as the
     * requests. An exception is thrown when a mismatch is detected.
     *
     * This function is useful to support things such as a direct illumination
     * rendering technique with "n" pixel samples and "m" shading samples,
     * while ensuring that the "n*m" sampled positions on an area light source
     * are all well-stratified with respect to each other.
     */
    Point2f *next_2d_array(size_t size);
    /// Vectorized variant of \ref next_2d_array.
    Point2fP *next_2d_array_p(size_t /*size*/);

    /**
     * Automatically selects the right variant among the next_*_array_*
     * methods, based on the template type.
     */
    template <typename T> auto next_array(size_t size);

    // =============================================================
    //! @{ \name Accessors & misc
    // =============================================================
    size_t sample_count() const { return m_sample_count; }
    size_t sample_index() const { return m_sample_index; }

    /// Manually set the current sample index
    virtual void set_sample_index(size_t sample_index) {
        m_sample_index = sample_index;
        m_dimension_1d_array = m_dimension_2d_array = 0;
        m_dimension_1d_array_p = m_dimension_2d_array_p = 0;
    }

    virtual void set_film_resolution(const Vector2i &/*res*/, bool /*blocked*/) {
        NotImplementedError("set_film_resolution");
    }

    virtual std::string to_string() const override = 0;
    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Sampler(const Properties &/*props*/)
        : Object(), m_sample_count(0), m_sample_index(0) { }

    virtual ~Sampler();

protected:
    size_t m_sample_count;
    size_t m_sample_index;

    std::vector<size_t> m_requests_1d, m_requests_2d,
                        m_requests_1d_p, m_requests_2d_p;
    std::vector<Float *> m_samples_1d;
    std::vector<FloatP *> m_samples_1d_p;
    std::vector<Point2f *> m_samples_2d;
    std::vector<Point2fP *> m_samples_2d_p;
    size_t m_dimension_1d_array, m_dimension_1d_array_p,
           m_dimension_2d_array, m_dimension_2d_array_p;
};

// =============================================================
//! @{ \name Template specializations
// =============================================================
template <> inline Float Sampler::next<Float>(const mask_t<Float> &) {
    return next_1d();
}
template <> inline FloatP Sampler::next<FloatP>(const mask_t<FloatP> &active) {
    return next_1d_p(active);
}
template <> inline FloatX Sampler::next<FloatX>(const mask_t<FloatX> &/*active*/) {
    NotImplementedError("next<FloatX>()");
    return FloatX();
}
template <> inline Point2f Sampler::next<Point2f>(const mask_t<Float> &) {
    return next_2d();
}
template <> inline Point2fP Sampler::next<Point2fP>(const mask_t<FloatP> &active) {
    return next_2d_p(active);
}
template <> inline Point2fX Sampler::next<Point2fX>(const mask_t<FloatX> &/*active*/) {
    NotImplementedError("next<Point2fX>()");
    return Point2fX();
}

template <> inline void Sampler::request_array<Float>(size_t size) { return request_1d_array(size); }
template <> inline void Sampler::request_array<FloatP>(size_t size) { return request_1d_array_p(size); }
template <> inline void Sampler::request_array<Point2f>(size_t size) { return request_2d_array(size); }
template <> inline void Sampler::request_array<Point2fP>(size_t size) { return request_2d_array_p(size); }

template <> inline auto Sampler::next_array<Float>(size_t size) { return next_1d_array(size); }
template <> inline auto Sampler::next_array<FloatP>(size_t size) { return next_1d_array_p(size); }
template <> inline auto Sampler::next_array<Point2f>(size_t size) { return next_2d_array(size); }
template <> inline auto Sampler::next_array<Point2fP>(size_t size) { return next_2d_array_p(size); }
//! @}
// =============================================================

NAMESPACE_END(mitsuba)
