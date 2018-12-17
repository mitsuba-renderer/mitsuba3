#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/math.h>

NAMESPACE_BEGIN(mitsuba)

/// Linear interpolant of a regularly sampled spectrum
class InterpolatedSpectrum final : public ContinuousSpectrum {
public:
    /**
     * \brief Construct a linearly interpolated spectrum
     *
     * \param lambda_min
     *      Lowest wavelength value associated with a sample
     *
     * \param lambda_max
     *      Largest wavelength value associated with a sample
     *
     * \param size
     *      Number of sample values
     *
     * \param values
     *      Pointer to the sample values. The data is copied,
     *      hence there is no need to keep 'data' alive.
     */
    InterpolatedSpectrum(const Properties &props) {
        m_lambda_min = props.float_("lambda_min");
        m_lambda_max = props.float_("lambda_max");

        if (props.type("values") == Properties::EString) {
            std::vector<std::string> values_str =
                string::tokenize(props.string("values"), " ,");

            m_data.resize(values_str.size());
            int ctr = 0;
            for (const auto &s : values_str) {
                try {
                    m_data[ctr++] = (Float) std::stod(s);
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", s);
                }
            }
        } else {
            size_t size = props.size_("size");
            const Float *values = (Float *) props.pointer("values");
            m_data = std::vector<Float>(values, values + size);
        }

        if (m_data.size() < 2)
            Throw("InterpolatedSpectrum must have at least 2 entries!");

        size_t size = m_data.size();
        m_interval_size = Float((double(m_lambda_max) - double(m_lambda_min)) / (size - 1));

        if (m_interval_size <= 0)
            Throw("InterpolatedSpectrum: interval size must be positive!");

        bool expanded = false;
        while (m_lambda_min > MTS_WAVELENGTH_MIN) {
            m_data.insert(m_data.begin(), m_data[0]);
            m_lambda_min -= m_interval_size;
            size += 1;
            expanded = true;
        }

        while (m_lambda_max < MTS_WAVELENGTH_MAX) {
            m_data.push_back(m_data[m_data.size() - 1]);
            m_lambda_max += m_interval_size;
            size += 1;
            expanded = true;
        }

        if (expanded)
            Log(EWarn, "InterpolatedSpectrum was expanded to cover wavelength range [%.1f, %.1f]",
                       MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX);

        m_inv_interval_size = Float((size - 1) / (double(m_lambda_max) - double(m_lambda_min)));
        m_size_minus_2 = uint32_t(size - 2);

        m_cdf.resize(size);
        m_cdf[0] = 0.f;

        /* Compute a probability mass function for each interval */
        double scale = 0.5 * (double(m_lambda_max) - double(m_lambda_min)) / (size - 1),
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
    ENOKI_INLINE Value eval_impl(Value lambda, mask_t<Value> active) const {
        using Index = uint_array_t<Value>;

        Value t = (lambda - m_lambda_min) * m_inv_interval_size;
        active  = active && (lambda >= m_lambda_min) && (lambda <= m_lambda_max);

        Index i0 = min(max(Index(t), zero<Index>()), Index(m_size_minus_2)),
              i1 = i0 + Index(1);

        Value v0 = gather<Value>(m_data.data(), i0, active),
              v1 = gather<Value>(m_data.data(), i1, active);

        Value w1 = t - Value(i0),
              w0 = (Float) 1 - w1;

        return (w0 * v0 + w1 * v1) & active;
    }

    template <typename Value>
    ENOKI_INLINE Value pdf_impl(Value lambda, mask_t<Value> active) const {
        return eval_impl(lambda, active) * m_normalization;
    }

    template <typename Value>
    ENOKI_INLINE std::pair<Value, Value> sample_impl(Value sample, mask_t<Value> active) const {
        using Index = int_array_t<Value>;
        using Mask = mask_t<Value>;

        sample *= m_cdf[m_cdf.size() - 1];

        Index i0 = math::find_interval(m_cdf.size(),
            [&](Index idx, Mask active) {
                return gather<Value>(m_cdf.data(), idx, active) <= sample;
            },
            active
        );

        Index i1 = i0 + Index(1);

        Value f0 = gather<Value>(m_data.data(), i0, active),
              f1 = gather<Value>(m_data.data(), i1, active);

        /* Reuse the sample */
        sample = (sample - gather<Value>(m_cdf.data(), i0)) * m_inv_interval_size;

        /* Importance sample the linear interpolant */
        Value t_linear =
            (f0 - safe_sqrt(f0 * f0 + 2.0f * sample * (f1 - f0))) / (f0 - f1);
        Value t_const  = sample / f0;
        Value t = select(eq(f0, f1), t_const, t_linear);

        return {
            m_lambda_min + (Value(i0) + t) * m_interval_size,
            Value(m_integral)
        };
    }

    Float mean() const override {
        return m_integral / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "InterpolatedSpectrum[" << std::endl
            << "  size = " << m_data.size() << "," << std::endl
            << "  lambda_min = " << m_lambda_min << "," << std::endl
            << "  lambda_max = " << m_lambda_max << "," << std::endl
            << "  interval_size = " << m_interval_size << "," << std::endl
            << "  integral = " << m_integral << "," << std::endl
            << "  normalization = " << m_normalization << "," << std::endl
            << "  data = " << "[ " << m_data << " ]" << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SPECTRUM()
    MTS_DECLARE_CLASS()

private:
    std::vector<Float> m_data, m_cdf;
    uint32_t m_size_minus_2;
    Float m_lambda_min;
    Float m_lambda_max;
    Float m_interval_size;
    Float m_inv_interval_size;
    Float m_integral;
    Float m_normalization;
};

MTS_IMPLEMENT_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(InterpolatedSpectrum, "Interpolated spectrum")

NAMESPACE_END(mitsuba)
