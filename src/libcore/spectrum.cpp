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

const Float cie1931_x_data[95] = {
    0.000129900000f, 0.000232100000f, 0.000414900000f, 0.000741600000f,
    0.001368000000f, 0.002236000000f, 0.004243000000f, 0.007650000000f,
    0.014310000000f, 0.023190000000f, 0.043510000000f, 0.077630000000f,
    0.134380000000f, 0.214770000000f, 0.283900000000f, 0.328500000000f,
    0.348280000000f, 0.348060000000f, 0.336200000000f, 0.318700000000f,
    0.290800000000f, 0.251100000000f, 0.195360000000f, 0.142100000000f,
    0.095640000000f, 0.057950010000f, 0.032010000000f, 0.014700000000f,
    0.004900000000f, 0.002400000000f, 0.009300000000f, 0.029100000000f,
    0.063270000000f, 0.109600000000f, 0.165500000000f, 0.225749900000f,
    0.290400000000f, 0.359700000000f, 0.433449900000f, 0.512050100000f,
    0.594500000000f, 0.678400000000f, 0.762100000000f, 0.842500000000f,
    0.916300000000f, 0.978600000000f, 1.026300000000f, 1.056700000000f,
    1.062200000000f, 1.045600000000f, 1.002600000000f, 0.938400000000f,
    0.854449900000f, 0.751400000000f, 0.642400000000f, 0.541900000000f,
    0.447900000000f, 0.360800000000f, 0.283500000000f, 0.218700000000f,
    0.164900000000f, 0.121200000000f, 0.087400000000f, 0.063600000000f,
    0.046770000000f, 0.032900000000f, 0.022700000000f, 0.015840000000f,
    0.011359160000f, 0.008110916000f, 0.005790346000f, 0.004109457000f,
    0.002899327000f, 0.002049190000f, 0.001439971000f, 0.000999949300f,
    0.000690078600f, 0.000476021300f, 0.000332301100f, 0.000234826100f,
    0.000166150500f, 0.000117413000f, 0.000083075270f, 0.000058706520f,
    0.000041509940f, 0.000029353260f, 0.000020673830f, 0.000014559770f,
    0.000010253980f, 0.000007221456f, 0.000005085868f, 0.000003581652f,
    0.000002522525f, 0.000001776509f, 0.000001251141f
};

const Float cie1931_y_data[95] = {
    0.000003917000f, 0.000006965000f, 0.000012390000f, 0.000022020000f,
    0.000039000000f, 0.000064000000f, 0.000120000000f, 0.000217000000f,
    0.000396000000f, 0.000640000000f, 0.001210000000f, 0.002180000000f,
    0.004000000000f, 0.007300000000f, 0.011600000000f, 0.016840000000f,
    0.023000000000f, 0.029800000000f, 0.038000000000f, 0.048000000000f,
    0.060000000000f, 0.073900000000f, 0.090980000000f, 0.112600000000f,
    0.139020000000f, 0.169300000000f, 0.208020000000f, 0.258600000000f,
    0.323000000000f, 0.407300000000f, 0.503000000000f, 0.608200000000f,
    0.710000000000f, 0.793200000000f, 0.862000000000f, 0.914850100000f,
    0.954000000000f, 0.980300000000f, 0.994950100000f, 1.000000000000f,
    0.995000000000f, 0.978600000000f, 0.952000000000f, 0.915400000000f,
    0.870000000000f, 0.816300000000f, 0.757000000000f, 0.694900000000f,
    0.631000000000f, 0.566800000000f, 0.503000000000f, 0.441200000000f,
    0.381000000000f, 0.321000000000f, 0.265000000000f, 0.217000000000f,
    0.175000000000f, 0.138200000000f, 0.107000000000f, 0.081600000000f,
    0.061000000000f, 0.044580000000f, 0.032000000000f, 0.023200000000f,
    0.017000000000f, 0.011920000000f, 0.008210000000f, 0.005723000000f,
    0.004102000000f, 0.002929000000f, 0.002091000000f, 0.001484000000f,
    0.001047000000f, 0.000740000000f, 0.000520000000f, 0.000361100000f,
    0.000249200000f, 0.000171900000f, 0.000120000000f, 0.000084800000f,
    0.000060000000f, 0.000042400000f, 0.000030000000f, 0.000021200000f,
    0.000014990000f, 0.000010600000f, 0.000007465700f, 0.000005257800f,
    0.000003702900f, 0.000002607800f, 0.000001836600f, 0.000001293400f,
    0.000000910930f, 0.000000641530f, 0.000000451810f
};

const Float cie1931_z_data[95] = {
    0.000606100000f, 0.001086000000f, 0.001946000000f, 0.003486000000f,
    0.006450001000f, 0.010549990000f, 0.020050010000f, 0.036210000000f,
    0.067850010000f, 0.110200000000f, 0.207400000000f, 0.371300000000f,
    0.645600000000f, 1.039050100000f, 1.385600000000f, 1.622960000000f,
    1.747060000000f, 1.782600000000f, 1.772110000000f, 1.744100000000f,
    1.669200000000f, 1.528100000000f, 1.287640000000f, 1.041900000000f,
    0.812950100000f, 0.616200000000f, 0.465180000000f, 0.353300000000f,
    0.272000000000f, 0.212300000000f, 0.158200000000f, 0.111700000000f,
    0.078249990000f, 0.057250010000f, 0.042160000000f, 0.029840000000f,
    0.020300000000f, 0.013400000000f, 0.008749999000f, 0.005749999000f,
    0.003900000000f, 0.002749999000f, 0.002100000000f, 0.001800000000f,
    0.001650001000f, 0.001400000000f, 0.001100000000f, 0.001000000000f,
    0.000800000000f, 0.000600000000f, 0.000340000000f, 0.000240000000f,
    0.000190000000f, 0.000100000000f, 0.000049999990f, 0.000030000000f,
    0.000020000000f, 0.000010000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f, 0.000000000000f,
    0.000000000000f, 0.000000000000f, 0.000000000000f
};

/**
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 */
const Float cie1931_fits[7][4] = {
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

template MTS_EXPORT_CORE DiscreteSpectrum  rgb_spectrum(const Color3f &, DiscreteSpectrum lambda);
template MTS_EXPORT_CORE DiscreteSpectrumP rgb_spectrum(const Color3f &, DiscreteSpectrumP lambda);

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_ALIAS(ContinuousSpectrum, "spectrum", Object)
MTS_IMPLEMENT_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
NAMESPACE_END(mitsuba)
