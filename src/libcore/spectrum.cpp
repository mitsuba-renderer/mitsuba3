#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <numeric>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name Spectrum implementation (just throws exceptions)
// =======================================================================

std::tuple<Spectrumf, Spectrumf, Spectrumf>
ContinuousSpectrum::sample(const Spectrumf &) const { NotImplementedError("sample"); }

std::tuple<SpectrumfP, SpectrumfP, SpectrumfP>
ContinuousSpectrum::sample(const SpectrumfP &, const Mask &) const { NotImplementedError("sample"); }

Spectrumf ContinuousSpectrum::pdf(const Spectrumf &) const { NotImplementedError("pdf"); }

SpectrumfP ContinuousSpectrum::pdf(const SpectrumfP &, const Mask &) const { NotImplementedError("pdf"); }

Float ContinuousSpectrum::integral() const { NotImplementedError("integral"); }

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

template <typename Value>
ENOKI_INLINE auto InterpolatedSpectrum::eval_impl(Value lambda,
                                                  mask_t<Value> active) const {
    using Index = uint_array_t<Value>;

    Value t = (lambda - m_lambda_min) * m_inv_interval_size;
    active &= (lambda >= m_lambda_min) & (lambda <= m_lambda_max);

    Index i0 = min(max(Index(t), zero<Index>()), Index(m_size_minus_2));
    Index i1 = i0 + 1;

    Value v0 = gather<Value>(m_data.data(), i0, active);
    Value v1 = gather<Value>(m_data.data(), i1, active);

    Value w1 = t - Value(i0);
    Value w0 = (Float) 1 - w1;

    auto r1 = (w0 * v0 + w1 * v1);
    auto r2 = r1 & active;
    return r2;
}

template <typename Value>
ENOKI_INLINE auto InterpolatedSpectrum::sample_impl(Value sample,
                                                    mask_t<Value> active) const {
    using Index = int_array_t<Value>;
    using Mask = mask_t<Value>;

    sample *= m_cdf[m_cdf.size() - 1];

    Index i0 = math::find_interval(m_cdf.size(),
        [&](Index idx, Mask active) {
            return gather<Value>(m_cdf.data(), idx, active) <= sample;
        },
        active
    );

    Index i1 = i0 + 1;

    Value f0 = gather<Value>(m_data.data(), i0, active);
    Value f1 = gather<Value>(m_data.data(), i1, active);

    /* Reuse the sample */
    sample = (sample - gather<Value>(m_cdf.data(), i0)) * m_inv_interval_size;

    /* Importance sample the linear interpolant */
    Value t_linear =
        (f0 - safe_sqrt(f0 * f0 + 2 * sample * (f1 - f0))) / (f0 - f1);
    Value t_const  = sample / f0;
    Value t = select(eq(f0, f1), t_const, t_linear);

    return std::make_tuple(
        m_lambda_min + (Value(i0) + t) * m_interval_size,
        Value(m_integral),
        ((1 - t) * f0 + t * f1) * m_normalization
    );
}

Spectrumf InterpolatedSpectrum::eval(const Spectrumf &lambda) const {
    return eval_impl(lambda);
}

SpectrumfP InterpolatedSpectrum::eval(const SpectrumfP &lambda, const Mask &active) const {
    return eval_impl(lambda, active);
}

Spectrumf InterpolatedSpectrum::pdf(const Spectrumf &lambda) const {
    return eval_impl(lambda) * m_normalization;
}

SpectrumfP InterpolatedSpectrum::pdf(const SpectrumfP &lambda, const Mask &active) const {
    return eval_impl(lambda, active) * m_normalization;
}

std::tuple<Spectrumf, Spectrumf, Spectrumf>
InterpolatedSpectrum::sample(const Spectrumf &sample) const {
    return sample_impl(sample);
}

std::tuple<SpectrumfP, SpectrumfP, SpectrumfP>
InterpolatedSpectrum::sample(const SpectrumfP &sample, const Mask &active) const {
    return sample_impl(sample, active);
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
    Float(0.000129900000), Float(0.000232100000), Float(0.000414900000), Float(0.000741600000),
    Float(0.001368000000), Float(0.002236000000), Float(0.004243000000), Float(0.007650000000),
    Float(0.014310000000), Float(0.023190000000), Float(0.043510000000), Float(0.077630000000),
    Float(0.134380000000), Float(0.214770000000), Float(0.283900000000), Float(0.328500000000),
    Float(0.348280000000), Float(0.348060000000), Float(0.336200000000), Float(0.318700000000),
    Float(0.290800000000), Float(0.251100000000), Float(0.195360000000), Float(0.142100000000),
    Float(0.095640000000), Float(0.057950010000), Float(0.032010000000), Float(0.014700000000),
    Float(0.004900000000), Float(0.002400000000), Float(0.009300000000), Float(0.029100000000),
    Float(0.063270000000), Float(0.109600000000), Float(0.165500000000), Float(0.225749900000),
    Float(0.290400000000), Float(0.359700000000), Float(0.433449900000), Float(0.512050100000),
    Float(0.594500000000), Float(0.678400000000), Float(0.762100000000), Float(0.842500000000),
    Float(0.916300000000), Float(0.978600000000), Float(1.026300000000), Float(1.056700000000),
    Float(1.062200000000), Float(1.045600000000), Float(1.002600000000), Float(0.938400000000),
    Float(0.854449900000), Float(0.751400000000), Float(0.642400000000), Float(0.541900000000),
    Float(0.447900000000), Float(0.360800000000), Float(0.283500000000), Float(0.218700000000),
    Float(0.164900000000), Float(0.121200000000), Float(0.087400000000), Float(0.063600000000),
    Float(0.046770000000), Float(0.032900000000), Float(0.022700000000), Float(0.015840000000),
    Float(0.011359160000), Float(0.008110916000), Float(0.005790346000), Float(0.004109457000),
    Float(0.002899327000), Float(0.002049190000), Float(0.001439971000), Float(0.000999949300),
    Float(0.000690078600), Float(0.000476021300), Float(0.000332301100), Float(0.000234826100),
    Float(0.000166150500), Float(0.000117413000), Float(0.000083075270), Float(0.000058706520),
    Float(0.000041509940), Float(0.000029353260), Float(0.000020673830), Float(0.000014559770),
    Float(0.000010253980), Float(0.000007221456), Float(0.000005085868), Float(0.000003581652),
    Float(0.000002522525), Float(0.000001776509), Float(0.000001251141)
};

const Float cie1931_y_data[95] = {
    Float(0.000003917000), Float(0.000006965000), Float(0.000012390000), Float(0.000022020000),
    Float(0.000039000000), Float(0.000064000000), Float(0.000120000000), Float(0.000217000000),
    Float(0.000396000000), Float(0.000640000000), Float(0.001210000000), Float(0.002180000000),
    Float(0.004000000000), Float(0.007300000000), Float(0.011600000000), Float(0.016840000000),
    Float(0.023000000000), Float(0.029800000000), Float(0.038000000000), Float(0.048000000000),
    Float(0.060000000000), Float(0.073900000000), Float(0.090980000000), Float(0.112600000000),
    Float(0.139020000000), Float(0.169300000000), Float(0.208020000000), Float(0.258600000000),
    Float(0.323000000000), Float(0.407300000000), Float(0.503000000000), Float(0.608200000000),
    Float(0.710000000000), Float(0.793200000000), Float(0.862000000000), Float(0.914850100000),
    Float(0.954000000000), Float(0.980300000000), Float(0.994950100000), Float(1.000000000000),
    Float(0.995000000000), Float(0.978600000000), Float(0.952000000000), Float(0.915400000000),
    Float(0.870000000000), Float(0.816300000000), Float(0.757000000000), Float(0.694900000000),
    Float(0.631000000000), Float(0.566800000000), Float(0.503000000000), Float(0.441200000000),
    Float(0.381000000000), Float(0.321000000000), Float(0.265000000000), Float(0.217000000000),
    Float(0.175000000000), Float(0.138200000000), Float(0.107000000000), Float(0.081600000000),
    Float(0.061000000000), Float(0.044580000000), Float(0.032000000000), Float(0.023200000000),
    Float(0.017000000000), Float(0.011920000000), Float(0.008210000000), Float(0.005723000000),
    Float(0.004102000000), Float(0.002929000000), Float(0.002091000000), Float(0.001484000000),
    Float(0.001047000000), Float(0.000740000000), Float(0.000520000000), Float(0.000361100000),
    Float(0.000249200000), Float(0.000171900000), Float(0.000120000000), Float(0.000084800000),
    Float(0.000060000000), Float(0.000042400000), Float(0.000030000000), Float(0.000021200000),
    Float(0.000014990000), Float(0.000010600000), Float(0.000007465700), Float(0.000005257800),
    Float(0.000003702900), Float(0.000002607800), Float(0.000001836600), Float(0.000001293400),
    Float(0.000000910930), Float(0.000000641530), Float(0.000000451810)
};

const Float cie1931_z_data[95] = {
    Float(0.000606100000), Float(0.001086000000), Float(0.001946000000), Float(0.003486000000),
    Float(0.006450001000), Float(0.010549990000), Float(0.020050010000), Float(0.036210000000),
    Float(0.067850010000), Float(0.110200000000), Float(0.207400000000), Float(0.371300000000),
    Float(0.645600000000), Float(1.039050100000), Float(1.385600000000), Float(1.622960000000),
    Float(1.747060000000), Float(1.782600000000), Float(1.772110000000), Float(1.744100000000),
    Float(1.669200000000), Float(1.528100000000), Float(1.287640000000), Float(1.041900000000),
    Float(0.812950100000), Float(0.616200000000), Float(0.465180000000), Float(0.353300000000),
    Float(0.272000000000), Float(0.212300000000), Float(0.158200000000), Float(0.111700000000),
    Float(0.078249990000), Float(0.057250010000), Float(0.042160000000), Float(0.029840000000),
    Float(0.020300000000), Float(0.013400000000), Float(0.008749999000), Float(0.005749999000),
    Float(0.003900000000), Float(0.002749999000), Float(0.002100000000), Float(0.001800000000),
    Float(0.001650001000), Float(0.001400000000), Float(0.001100000000), Float(0.001000000000),
    Float(0.000800000000), Float(0.000600000000), Float(0.000340000000), Float(0.000240000000),
    Float(0.000190000000), Float(0.000100000000), Float(0.000049999990), Float(0.000030000000),
    Float(0.000020000000), Float(0.000010000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000), Float(0.000000000000),
    Float(0.000000000000), Float(0.000000000000), Float(0.000000000000)
};

template MTS_EXPORT_CORE std::tuple<Spectrumf, Spectrumf, Spectrumf>
cie1931_xyz(const Spectrumf &lambda, const mask_t<Spectrumf> &);
template MTS_EXPORT_CORE std::tuple<SpectrumfP, SpectrumfP, SpectrumfP>
cie1931_xyz(const SpectrumfP &lambda, const mask_t<SpectrumfP> &);

template MTS_EXPORT_CORE Spectrumf  cie1931_y(const Spectrumf  &lambda, const mask_t<Spectrumf> &);
template MTS_EXPORT_CORE SpectrumfP cie1931_y(const SpectrumfP &lambda, const mask_t<SpectrumfP> &);

template MTS_EXPORT_CORE Spectrumf  rgb_spectrum(const Color3f &, const Spectrumf  &lambda);
template MTS_EXPORT_CORE SpectrumfP rgb_spectrum(const Color3f &, const SpectrumfP &lambda);

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_ALIAS(ContinuousSpectrum, "spectrum", Object)
MTS_IMPLEMENT_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
NAMESPACE_END(mitsuba)
