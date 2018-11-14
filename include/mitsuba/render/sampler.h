#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Sampler : public Object {
public:
    /**
     * \brief Create a clone of this sampler
     *
     * The clone is allowed to be different to some extent, e.g. a pseudorandom
     * generator should be based on a different random seed compared to the
     * original. All other parameters are copied exactly.
     *
     * May throw an exception if not supported. Cloning may also change the
     * state of the original sampler (e.g. by using the next 1D sample as a
     * seed for the clone).
     */
    virtual ref<Sampler> clone() = 0;

    /// Deterministically seed the underlying RNG, if any
    virtual void seed(size_t seed_value);

    /// Retrieve the next component value from the current sample
    virtual Float next_1d();

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_1d()
    Float next_1d(bool /* unused */) { return next_1d(); }

    /// Retrieve the next packet of values from the current sample
    virtual FloatP next_1d_p(MaskP active = true);

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref next_1d()
    virtual FloatD next_1d_d(MaskD active = true);
#endif

    /// Retrieve the next two component values from the current sample
    virtual Point2f next_2d();

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_2d()
    Point2f next_2d(bool /* unused */) { return next_2d(); }

    /// Retrieve the next packet of 2D values from the current sample
    virtual Point2fP next_2d_p(MaskP active = true);

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref next_2d()
    virtual Point2fD next_2d_d(MaskD active = true);
#endif

    template <typename T> using MaskType =
        std::conditional_t<(array_size_v<T> == 2), mask_t<value_t<T>>, mask_t<T>>;

    /**
     * \brief Automatically selects the right variant of <tt>next_...</tt> based on
     * the desired return type.
     */
    template <typename T> T next(MaskType<T> active = true);

    /// Return the number of samples per pixel
    size_t sample_count() const { return m_sample_count; }

    MTS_DECLARE_CLASS()

protected:
    Sampler(const Properties &props);
    virtual ~Sampler();

protected:
    size_t m_sample_count;
};

// =============================================================
//! @{ \name Template specializations
// =============================================================

template<> MTS_INLINE Float    Sampler::next<Float>(bool /* active */)   { return next_1d(); }
template<> MTS_INLINE Point2f  Sampler::next<Point2f>(bool /* active */) { return next_2d(); }

template<> MTS_INLINE FloatP   Sampler::next<FloatP>(MaskP active)       { return next_1d_p(active); }
template<> MTS_INLINE Point2fP Sampler::next<Point2fP>(MaskP active)     { return next_2d_p(active); }

#if defined(MTS_ENABLE_AUTODIFF)
    template<> MTS_INLINE FloatD   Sampler::next<FloatD>(MaskD active)       { return next_1d_d(active); }
    template<> MTS_INLINE Point2fD Sampler::next<Point2fD>(MaskD active)     { return next_2d_d(active); }
#endif

//! @}
// =============================================================

NAMESPACE_END(mitsuba)
