#include <mitsuba/render/sunsky.h>


NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class TimedSunskyEmitter final: public BaseSunskyEmitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BaseSunskyEmitter,
        m_turbidity, m_sky_scale, m_sun_scale, m_albedo,
        m_sun_half_aperture, m_sky_rad_dataset, m_tgmm_tables,
        m_sky_params_dataset, m_sun_radiance,
        CHANNEL_COUNT, m_to_world
    )

    using typename Base::FloatStorage;

    using typename Base::SkyRadData;
    using typename Base::SkyParamsData;

    using typename Base::USpec;
    using typename Base::USpecUInt32;
    using typename Base::USpecMask;

    using FullSpectrum = unpolarized_spectrum_t<mitsuba::Spectrum<Float, WAVELENGTH_COUNT>>;

    MI_IMPORT_TYPES()

    TimedSunskyEmitter(const Properties &props) : Base(props) {

        ScalarFloat window_start_time = props.get<ScalarFloat>("window_start_time", 7.f);
        if (window_start_time < 0.f || window_start_time > 24.f)
            Log(Error, "Start hour: %f is out of range [0, 24]", window_start_time);
        m_window_start_time = window_start_time;

        ScalarFloat window_end_time = props.get<ScalarFloat>("window_end_time", 19.f);
        if (window_end_time < 0.f || window_end_time > 24.f)
            Log(Error, "Start hour: %f is out of range [0, 24]", window_end_time);
        m_window_end_time = window_end_time;

        if (window_start_time > window_end_time)
            Log(Error, "The given start time is greater than the end time");

        m_location = LocationRecord<Float>{props};
        DateTimeRecord<ScalarFloat> start_date, end_date;
        start_date.year = props.get<ScalarInt32>("start_year", 2025);
        start_date.month = props.get<ScalarInt32>("start_month", 1);
        start_date.day = props.get<ScalarInt32>("start_day", 1);

        end_date.year = props.get<ScalarInt32>("end_year", start_date.year + 1);
        end_date.month = props.get<ScalarInt32>("end_month", start_date.month);
        end_date.day = props.get<ScalarInt32>("end_day", start_date.day);

        m_start_date = start_date;
        m_end_date = end_date;
        dr::make_opaque(
            m_window_start_time, m_window_end_time,
            m_start_date, m_end_date, m_location
        );

        m_nb_days = dr::maximum(
            DateTimeRecord<Float>::get_days_between(m_start_date, m_end_date, m_location) - 1,
            1
        );

        m_sky_rad = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
        m_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);

        const TensorFile sampling_datasets(
            file_resolver()->resolve(DATABASE_PATH + "sampling_data.bin")
        );

        m_sky_irrad_dataset = Base::template load_field<TensorXf32>(sampling_datasets, "sky_irradiance");
        m_sun_irrad_dataset = Base::template load_field<TensorXf32>(sampling_datasets, "sun_irradiance");

        m_sky_sampling_weights = compute_sky_sampling_weights();
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
        cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
        cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);
        cb->put("window_start_time", m_window_start_time, ParamFlags::NonDifferentiable);
        cb->put("window_end_time", m_window_start_time, ParamFlags::NonDifferentiable);
        cb->put("start_year", m_start_date.year, ParamFlags::NonDifferentiable);
        cb->put("start_month", m_start_date.month, ParamFlags::NonDifferentiable);
        cb->put("start_day", m_start_date.day, ParamFlags::NonDifferentiable);
        cb->put("end_year", m_end_date.year, ParamFlags::NonDifferentiable);
        cb->put("end_month", m_end_date.month, ParamFlags::NonDifferentiable);
        cb->put("end_day", m_end_date.day, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);

        dr::make_opaque(
            m_location,
            m_window_start_time, m_window_end_time,
            m_start_date, m_end_date
        );

        m_nb_days = dr::maximum(
            DateTimeRecord<Float>::get_days_between(m_start_date, m_end_date, m_location) - 1,
            1
        );

        m_sky_rad = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
        m_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);

        m_sky_sampling_weights = compute_sky_sampling_weights();
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        if constexpr (!is_spectral_v<Spectrum>) {
            DRJIT_MARK_USED(si);
            DRJIT_MARK_USED(sample);
            DRJIT_MARK_USED(active);
            NotImplementedError("sample_wavelengths");
        } else {
            static constexpr ScalarFloat
                    min_w = std::max(MI_CIE_MIN, WAVELENGTHS<ScalarFloat>[0]),
                    max_w = std::min(MI_CIE_MAX, WAVELENGTHS<ScalarFloat>[WAVELENGTH_COUNT - 1]);

            Wavelength w_sample = math::sample_shifted<Wavelength>(sample);
                       w_sample = min_w + (max_w - min_w) * w_sample;

            SurfaceInteraction3f new_si(si);
            new_si.wavelengths = w_sample;

            return { w_sample, Base::eval(new_si, active) / (max_w - min_w) };
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunskyEmitter[" << std::endl
            << "  turbidity = " << string::indent(m_turbidity) << std::endl
            << "  sky_scale = " << string::indent(m_sky_scale) << std::endl
            << "  sun_scale = " << string::indent(m_sun_scale) << std::endl
            << "  albedo = " << string::indent(m_albedo) << std::endl
            << "  sun aperture (°) = " << string::indent(dr::rad_to_deg(2 * m_sun_half_aperture)) << std::endl;
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(TimedSunskyEmitter)

private:

    FloatStorage compute_sky_sampling_weights() const {
        using UInt32Storage = dr::uint32_array_t<FloatStorage>;
        using FullSpectrumStorage = unpolarized_spectrum_t<
            mitsuba::Spectrum<FloatStorage, WAVELENGTH_COUNT>
        >;

        UInt32Storage elevation_idx = dr::arange<UInt32Storage>(ELEVATION_CTRL_PTS);

        FullSpectrumStorage wavelengths = {
            320, 360, 400, 440, 480, 520, 560, 600, 640, 680, 720
        };

        FloatStorage sky_irrad_data = dr::take_interp(m_sky_irrad_dataset, m_turbidity),
                     sun_irrad_data = dr::take_interp(m_sun_irrad_dataset, m_turbidity);

        FullSpectrumStorage sky_irrad = dr::gather<FullSpectrumStorage>(sky_irrad_data, elevation_idx),
                            sun_irrad = dr::gather<FullSpectrumStorage>(sun_irrad_data, elevation_idx);

        FloatStorage sky_lum = m_sky_scale * luminance(sky_irrad, wavelengths),
                     sun_lum = m_sun_scale * luminance(sun_irrad, wavelengths);

        return sky_lum / (sky_lum + sun_lum);
    }

    Point2f get_sun_angles(const Float& time) const override {
        DateTimeRecord<Float> date_time = dr::zeros<DateTimeRecord<Float>>();
        date_time.year = m_start_date.year;
        date_time.month = m_start_date.month;

        Float day = dr::clip(time, 0.f, 1.f) * (m_nb_days - dr::Epsilon<Float>);
        Int32 int_day = dr::floor2int<Int32>(day);

        date_time.day = m_start_date.day + int_day;
        date_time.hour = m_window_start_time + (m_window_end_time - m_window_start_time) * (day - int_day);

        const auto [sun_elevation, sun_azimuth] = Base::sun_coordinates(date_time, m_location);
        return {sun_azimuth, sun_elevation};
    }

    Float get_sky_sampling_weight(const Point2f& sun_angles, const Mask& active) const override {
        Mask valid_elevation = active & (sun_angles.y() <= 0.5f * dr::Pi<Float>);
        Float sun_idx = 0.5f * dr::Pi<Float> - sun_angles.y();
              sun_idx = (sun_idx - 2.f) / 3.f;
              sun_idx = dr::clip(sun_idx, 0.f, ELEVATION_CTRL_PTS - 1.f - dr::Epsilon<Float>);

        UInt32 sun_idx_low = dr::floor2int<UInt32>(sun_idx),
               sun_idx_high = sun_idx_low + 1;

        Float low_weight = dr::gather<Float>(m_sky_sampling_weights, sun_idx_low, valid_elevation),
              high_weight = dr::gather<Float>(m_sky_sampling_weights, sun_idx_high, valid_elevation);

        Float res = dr::lerp(low_weight, high_weight, sun_idx - sun_idx_low);

        return dr::select(res == 0.f, 1.f, res);
    }

    std::pair<UInt32, Float> sample_reuse_tgmm(const Float& sample, const Point2f& sun_angles, const Mask& active) const override {
        const auto [ lerp_w, tgmm_idx ] = Base::get_tgmm_data(sun_angles);

        Mask active_loop = active;
        Float last_cdf = 0.f, cdf = 0.f;
        UInt32 res_gaussian_idx = 0;
        for (uint32_t mixture_idx = 0; mixture_idx < 4; ++mixture_idx) {
            for (uint32_t gaussian_idx = 0; gaussian_idx < TGMM_COMPONENTS; ++gaussian_idx) {
                dr::masked(last_cdf, active_loop) = cdf;

                dr::masked(res_gaussian_idx, active_loop) =
                    tgmm_idx[mixture_idx] + gaussian_idx;

                Float gaussian_w = lerp_w[mixture_idx] * dr::gather<Float>(m_tgmm_tables,
                    res_gaussian_idx * TGMM_GAUSSIAN_PARAMS + (TGMM_GAUSSIAN_PARAMS - 1),
                    active_loop
                );

                // Gathered weight is 0 if inactive
                cdf += gaussian_w;

                active_loop &= cdf < sample;

            }
        }

        return std::make_pair(res_gaussian_idx, (sample - last_cdf) / (cdf - last_cdf));
    }

    std::pair<SkyRadData, SkyParamsData>
    get_datasets(const Point2f& sun_angles, const USpecUInt32& channel_idx, const USpecMask& active) const override {
        const Float sun_eta = 0.5f * dr::Pi<Float> - sun_angles.y();
        USpecMask active_dataset = active & (sun_eta >= 0.f);

        return std::make_pair(
            bezier_interp<SkyRadData>(m_sky_rad, channel_idx, sun_eta, active_dataset),
            bezier_interp<SkyParamsData>(m_sky_params, channel_idx, sun_eta, active_dataset)
        );
    }

    template<typename Dataset>
    Dataset bezier_interp(const TensorXf& dataset, const USpecUInt32& channel_idx, const Float& eta, const USpecMask& active) const {

        Dataset res = dr::zeros<Dataset>();
        Float x = dr::cbrt(2 * dr::InvPi<Float> * eta);
        constexpr dr::scalar_t<Float> coefs[SKY_CTRL_PTS] = {1, 5, 10, 10, 5, 1};

        Float x_pow = 1.f, x_pow_inv = dr::pow(1.f - x, SKY_CTRL_PTS - 1);
        Float x_pow_inv_scale = dr::rcp(1.f - x);

        for (uint32_t ctrl_pt = 0; ctrl_pt < SKY_CTRL_PTS; ++ctrl_pt) {
            TensorXf temp = dr::take(dataset, ctrl_pt);
            Dataset data_ctrl_pt = dr::gather<Dataset>(
                temp.array(), channel_idx, active
            );

            res += data_ctrl_pt * coefs[ctrl_pt] * x_pow * x_pow_inv;

            x_pow *= x;
            x_pow_inv *= x_pow_inv_scale;
        }

        return res;
    }

    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    Float m_window_start_time, m_window_end_time;
    DateTimeRecord<Float> m_start_date, m_end_date;
    LocationRecord<Float> m_location;

    Int32 m_nb_days;

    // ========= Radiance parameters =========
    TensorXf m_sky_rad;
    TensorXf m_sky_params;

    // ========= Sampling parameters =========
    /// Sampling weights (sun vs sky) for each elevation
    FloatStorage m_sky_sampling_weights;

    // ========= Irradiance datasets loaded from file =========
    // Contains irradiance values for the 10 turbidites,
    // 30 elevations and 11 wavelengths
    TensorXf m_sky_irrad_dataset;
    TensorXf m_sun_irrad_dataset;

};

MI_EXPORT_PLUGIN(TimedSunskyEmitter)
NAMESPACE_END(mitsuba)
