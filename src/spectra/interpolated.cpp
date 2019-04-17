#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/math.h>

NAMESPACE_BEGIN(mitsuba)

/// Linear interpolant of a regularly sampled spectrum
class InterpolatedSpectrum final : public ContinuousSpectrum {
#if defined(MTS_ENABLE_AUTODIFF)
    using FloatVector = std::vector<Float, enoki::cuda_host_allocator<Float>>;
#else
    using FloatVector = std::vector<Float>;
#endif

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
            m_data = FloatVector(values, values + size);
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
#if defined(MTS_ENABLE_AUTODIFF)
        // Copy parsed data over to the GPU
        m_data_d = CUDAArray<Float>::copy(m_data.data(), m_data.size());
#endif
        parameters_changed();
    }

    /// Note: this assumes that the wavelengths and number of entries have not changed.
    void parameters_changed() override {
#if defined(MTS_ENABLE_AUTODIFF)
        // TODO: copy values to CPU to compute CDF?
#endif

        // Update CDF, normalization and integral from the new values
        m_cdf[0] = 0.f;
        size_t size = m_data.size();

        // Compute a probability mass function for each interval
        double scale = 0.5 * (double(m_lambda_max) - double(m_lambda_min)) / (size - 1),
               accum = 0.0;
        for (size_t i = 1; i < size; ++i) {
            accum += scale * (double(m_data[i - 1]) + double(m_data[i]));
            m_cdf[i] = Float(accum);
        }

        // Store the normalization factor
        m_integral = Float(accum);
        m_normalization = Float(1.0 / accum);

#if defined(MTS_ENABLE_AUTODIFF)
        m_integral_d = m_integral;
        m_normalization_d = m_normalization;
        m_cdf_d = CUDAArray<Float>::copy(m_cdf.data(), m_cdf.size());
#endif
    }

    template <typename Value, typename Index, typename Mask>
    auto data_gather(const Index &index, const Mask &active) const {
        if constexpr (!is_diff_array_v<Index>) {
            return gather<Value>(m_data.data(), index, active);
        }
#if defined(MTS_ENABLE_AUTODIFF)
        else {
            return gather<Value>(m_data_d, index, active);
        }
#endif
    }
    template <typename Value, typename Index, typename Mask>
    auto cdf_gather(const Index &index, const Mask &active) const {
        if constexpr (!is_diff_array_v<Index>) {
            return gather<Value>(m_cdf.data(), index, active);
        }
#if defined(MTS_ENABLE_AUTODIFF)
        else {
            return gather<Value>(m_cdf_d, index, active);
        }
#endif
    }

    template <typename Value>
    ENOKI_INLINE Value eval_impl(Value lambda, mask_t<Value> active) const {
        using Index = uint_array_t<Value>;

        Value t = (lambda - m_lambda_min) * m_inv_interval_size;
        active &= lambda >= m_lambda_min && lambda <= m_lambda_max;

        Index i0 = clamp(Index(t), zero<Index>(), Index(m_size_minus_2)),
              i1 = i0 + Index(1);

        Value v0 = data_gather<Value>(i0, active),
              v1 = data_gather<Value>(i1, active);

        Value w1 = t - Value(i0),
              w0 = (Float) 1 - w1;

        return (w0 * v0 + w1 * v1) & active;
    }

    template <typename Value>
    ENOKI_INLINE Value pdf_impl(Value lambda, mask_t<Value> active) const {
        return eval_impl(lambda, active) * normalization<Value>();
    }

    template <typename Value>
    ENOKI_INLINE std::pair<Value, Value> sample_impl(Value sample, mask_t<Value> active) const {
        using Index = int_array_t<Value>;
        using Mask = mask_t<Value>;

        sample *= integral<Value>();

        // TODO: find_interval on differentiable m_cdf?
        Index i0 = math::find_interval(m_cdf.size(),
            [&](Index idx, Mask active) {
                return cdf_gather<Value>(idx, active) <= sample;
            },
            active
        );

        Index i1 = i0 + Index(1);

        Value f0 = data_gather<Value>(i0, active),
              f1 = data_gather<Value>(i1, active);

        // Reuse the sample
        sample = (sample - cdf_gather<Value>(i0, active)) * m_inv_interval_size;

        // Importance sample the linear interpolant
        Value t_linear =
            (f0 - safe_sqrt(f0 * f0 + 2.0f * sample * (f1 - f0))) / (f0 - f1);
        Value t_const  = sample / f0;
        Value t = select(eq(f0, f1), t_const, t_linear);

        return {
            m_lambda_min + (Value(i0) + t) * m_interval_size,
            Value(integral<Value>())
        };
    }

    Float mean() const override {
        return m_integral / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "data", m_data_d);
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "InterpolatedSpectrum[" << std::endl
            << "  size = " << m_data.size() << "," << std::endl
            << "  lambda_min = " << m_lambda_min << "," << std::endl
            << "  lambda_max = " << m_lambda_max << "," << std::endl
            << "  interval_size = " << m_interval_size << "," << std::endl
            << "  integral = " << m_integral << "," << std::endl
            << "  normalization = " << m_normalization << "," << std::endl
            << "  data = " << m_data << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SPECTRUM_ALL()
    MTS_AUTODIFF_GETTER(data, m_data, m_data_d)
    MTS_AUTODIFF_GETTER(integral, m_integral, m_integral_d)
    MTS_AUTODIFF_GETTER(normalization, m_normalization, m_normalization_d)
    MTS_DECLARE_CLASS()

private:
    FloatVector m_data, m_cdf;
    uint32_t m_size_minus_2;
    Float m_lambda_min;
    Float m_lambda_max;
    Float m_interval_size;
    Float m_inv_interval_size;
    Float m_integral;
    Float m_normalization;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_data_d, m_cdf_d, m_integral_d, m_normalization_d;
#endif
};

MTS_IMPLEMENT_CLASS(InterpolatedSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(InterpolatedSpectrum, "Interpolated spectrum")

NAMESPACE_END(mitsuba)
