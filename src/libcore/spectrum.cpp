#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <numeric>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name DiscreteSpectrum implementation
// =======================================================================

std::tuple<DiscreteSpectrum, DiscreteSpectrum, DiscreteSpectrum>
ContinuousSpectrum::sample(Float) const {
    NotImplementedError("sample");
}

std::tuple<DiscreteSpectrumP, DiscreteSpectrumP, DiscreteSpectrumP>
ContinuousSpectrum::sample(FloatP) const {
    NotImplementedError("sample");
}

DiscreteSpectrum ContinuousSpectrum::pdf(DiscreteSpectrum) const {
    NotImplementedError("pdf");
}

DiscreteSpectrumP ContinuousSpectrum::pdf(DiscreteSpectrumP) const {
    NotImplementedError("pdf");
}

Float ContinuousSpectrum::integral() const {
    NotImplementedError("integral");
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name InterpolatedSpectrum implementation
// =======================================================================

InterpolatedSpectrum::InterpolatedSpectrum(Float lambda_min, Float lambda_max,
                                           size_t size, const Float *data)
    : m_data(data, data + size), m_lambda_min(lambda_min),
      m_lambda_max(lambda_max) {

    m_interval_size = Float((double(lambda_max) - double(lambda_min)) / (size - 1));
    m_inv_interval_size = Float((size - 1) / (double(lambda_max) - double(lambda_min)));
    m_size_minus_2 = uint32_t(size - 2);

    m_cdf.resize(size);
    m_cdf[0] = 0.f;

    /* Compute a probability mass function for each interval */
    double scale = 0.5 * (double(lambda_max) - double(lambda_min)) / (size - 1),
           accum = 0.0;
    for (size_t i = 1; i < size; ++i) {
        accum += scale * (double(m_data[i - 1]) + double(m_data[i]));
        m_cdf[i] = Float(accum);
    }

    /* Store the normalization factor */
    m_integral = Float(accum);
    m_normalization = Float(1.0 / accum);
}

template <typename Value> auto InterpolatedSpectrum::eval(Value lambda) const {
    using Mask = mask_t<Value>;
    using Index = int_array_t<Value>;

    Value t = (lambda - m_lambda_min) * m_inv_interval_size;
    Mask mask_valid = lambda >= m_lambda_min & lambda <= m_lambda_max;

    Index i0 = min(max(Index(t), zero<Index>()), Index(m_size_minus_2));
    Index i1 = i0 + 1;

    Value v0 = gather<Value>(m_data.data(), i0, mask_valid);
    Value v1 = gather<Value>(m_data.data(), i1, mask_valid);

    Value w1 = t - Value(i0);
    Value w0 = (Float) 1 - w1;

    return (w0 * v0 + w1 * v1) & mask_valid;
}

template <typename Value, typename Float>
auto InterpolatedSpectrum::sample(Float sample_) const {
    using Index = int_array_t<Value>;
    using Mask = mask_t<Value>;

    Value sample = sample_shifted<Value>(sample_) * m_cdf[m_cdf.size() - 1];

    Index i0 = math::find_interval(m_cdf.size(),
        [&](Index idx, Mask active) {
            return gather<Value>(m_cdf.data(), idx, active) <= sample;
        },
        Mask(true)
    );
    Index i1 = i0 + 1;

    Value f0 = gather<Value>(m_data.data(), i0);
    Value f1 = gather<Value>(m_data.data(), i1);

    /* Reuse the sample */
    sample = (sample - gather<Value>(m_cdf.data(), i0)) * m_inv_interval_size;

    /* Importance sample the linear interpolant */
    Value t_linear =
        (f0 - safe_sqrt(f0 * f0 + 2 * sample * (f1 - f0))) / (f0 - f1);
    Value t_const  = sample / f0;
    Value t = select(neq(f0, f1), t_linear, t_const);

    return std::make_tuple(
        m_lambda_min + (Value(i0) + t) * m_interval_size,
        Value(m_integral),
        (1 - t) * f0 + t * f1
    );
}

DiscreteSpectrum InterpolatedSpectrum::eval(DiscreteSpectrum lambda) const {
    return eval<DiscreteSpectrum>(lambda);
}

DiscreteSpectrumP InterpolatedSpectrum::eval(DiscreteSpectrumP lambda) const {
    return eval<DiscreteSpectrumP>(lambda);
}

DiscreteSpectrum InterpolatedSpectrum::pdf(DiscreteSpectrum lambda) const {
    return eval<DiscreteSpectrum>(lambda) * m_normalization;
}

DiscreteSpectrumP InterpolatedSpectrum::pdf(DiscreteSpectrumP lambda) const {
    return eval<DiscreteSpectrumP>(lambda) * m_normalization;
}

std::tuple<DiscreteSpectrum, DiscreteSpectrum, DiscreteSpectrum>
InterpolatedSpectrum::sample(Float sample_) const {
    return sample<DiscreteSpectrum>(sample_);
}

std::tuple<DiscreteSpectrumP, DiscreteSpectrumP, DiscreteSpectrumP>
InterpolatedSpectrum::sample(FloatP sample_) const {
    return sample<DiscreteSpectrumP>(sample_);
}

Float InterpolatedSpectrum::integral() const {
    return m_integral;
}

//! @}
// =======================================================================

// =======================================================================
//! @{ \name CIE 1931 2 degree observer implementation
// =======================================================================

/**
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 */
const Float cie1931_data[7][4] = {
    {  0.362f, 442.0f, 0.0624f, 0.0374f }, // x0
    {  1.056f, 599.8f, 0.0264f, 0.0323f }, // x1
    { -0.065f, 501.1f, 0.0490f, 0.0382f }, // x2
    {  0.821f, 568.8f, 0.0213f, 0.0247f }, // y0
    {  0.286f, 530.9f, 0.0613f, 0.0322f }, // y1
    {  1.217f, 437.0f, 0.0845f, 0.0278f }, // z0
    {  0.681f, 459.0f, 0.0385f, 0.0725f }  // z1
};

template MTS_EXPORT_CORE std::tuple<DiscreteSpectrum, DiscreteSpectrum, DiscreteSpectrum>
cie1931_xyz(DiscreteSpectrum lambda);
template MTS_EXPORT_CORE std::tuple<DiscreteSpectrumP, DiscreteSpectrumP, DiscreteSpectrumP>
cie1931_xyz(DiscreteSpectrumP lambda);

template MTS_EXPORT_CORE DiscreteSpectrum cie1931_y(DiscreteSpectrum lambda);
template MTS_EXPORT_CORE DiscreteSpectrumP cie1931_y(DiscreteSpectrumP lambda);

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_ALIAS(ContinuousSpectrum, "spectrum", Object)
MTS_IMPLEMENT_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
NAMESPACE_END(mitsuba)
