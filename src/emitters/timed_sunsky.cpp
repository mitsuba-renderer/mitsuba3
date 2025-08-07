#include <drjit/local.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/sunsky.h>


NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class TimedSunskyEmitter final: public BaseSunskyEmitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BaseSunskyEmitter,
        m_turbidity, m_sky_scale, m_sun_scale, m_albedo,
        m_sun_half_aperture, m_sky_rad_dataset,
        m_sky_params_dataset,
        CHANNEL_COUNT, m_to_world
    )

    using typename Base::FloatStorage;

    MI_IMPORT_TYPES()

private:
    using RadLocal = dr::Local<Float, CHANNEL_COUNT>;
    using ParamsLocal = dr::Local<Float, SKY_PARAMS * CHANNEL_COUNT>;

public:

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

        dr::make_opaque(m_window_start_time, m_window_end_time,
                        m_start_date, m_end_date, m_location);


        m_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);
        m_sky_radiance = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
        cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
        cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);
        NotImplementedError()
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);
        NotImplementedError()
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        using Spec = unpolarized_spectrum_t<Spectrum>;
        using SpecUInt32 = dr::uint32_array_t<Spec>;

        // These typedefs concatenate the discrete spectrae together
        // to make it easier to iterate over them for interpolation
        using ExtendedSpec = dr::Array<Float, (is_spectral_v<Spec> ? 2 : 1) * dr::size_v<Spec>>;
        using ExtendedSpecUInt32 = dr::uint32_array_t<ExtendedSpec>;
        using ExtendedSpecMask   = dr::mask_t<ExtendedSpec>;

        // Compute interpolation coefficients and indices for the wavelengths
        ExtendedSpec lerp_factor;
        ExtendedSpecUInt32 channel_idx;
        ExtendedSpecMask valid_idx = active;
        if constexpr (is_rgb_v<Spec>) {
            lerp_factor = 1.f;
            channel_idx = {0, 1, 2};
        } else if constexpr (is_spectral_v<Spec>) {
            Wavelength normalized_wavelengths =
                (si.wavelengths - WAVELENGTHS<ScalarFloat>[0]) / WAVELENGTH_STEP;

            SpecUInt32 query_idx_low = dr::floor2int<SpecUInt32>(normalized_wavelengths);
            channel_idx = dr::concat(query_idx_low, query_idx_low + 1);

            Wavelength lerp_factor_wavelength = normalized_wavelengths - query_idx_low;
            lerp_factor = dr::concat(lerp_factor_wavelength, 1.f - lerp_factor_wavelength);

            valid_idx &= (0.f <= lerp_factor) & (lerp_factor <= 1.f);
        }

        Datasets datasets = compute_dataset(si.time);

        // Compute angles
        Vector3f local_wo = m_to_world.value().inverse() * (-si.wi);
        Float cos_theta = Frame3f::cos_theta(local_wo),
              gamma = dr::unit_angle(datasets.sun_dir, local_wo);

        active &= cos_theta >= 0 && Frame3f::cos_theta(datasets.sun_dir) > 0.f;
        Mask hit_sun = dr::dot(datasets.sun_dir, local_wo) >= dr::cos(m_sun_half_aperture);

        // Evaluate the model channel by channel
        Spec res = 0.f;
        for (uint32_t idx = 0; idx < dr::size_v<ExtendedSpec>; ++idx) {
            Float sky_rad = m_sky_scale * eval_sky(channel_idx[idx], cos_theta, gamma, datasets.sky_rad, datasets.sky_params, valid_idx[idx]);

            Float sun_rad = m_sun_scale * Base::get_area_ratio(m_sun_half_aperture) *
                Base::template eval_sun<Float>(channel_idx[idx], cos_theta, gamma,hit_sun & valid_idx[idx]);

            if constexpr (is_rgb_v<Spec>) {
                sun_rad *= SPEC_TO_RGB_SUN_CONV;
            } else if constexpr (is_spectral_v<Spec>) {
                // TODO sort sun limb darkening
                sun_rad *= 1.f;
            }

            res[idx % dr::size_v<Spec>] += lerp_factor[idx] * (sky_rad + sun_rad) * (is_rgb_v<Spec> ? ScalarFloat(MI_CIE_Y_NORMALIZATION) : 1.f);
        }

        return depolarizer<Spectrum>(res) & active;
    }


    Float eval_sky(const UInt32 &channel_idx,
        const Float &cos_theta, const Float &gamma,
        const RadLocal& sky_rad, const ParamsLocal& sky_params,
        const Mask &active) const {

        using SpecSkyParams = dr::Array<Float, SKY_PARAMS>;
        SpecSkyParams needed_sky_params = dr::zeros<SpecSkyParams>();
        for (uint32_t param_idx = 0; param_idx < SKY_PARAMS; ++param_idx)
            needed_sky_params[param_idx] = sky_params.read(channel_idx * SKY_PARAMS + param_idx, active);

        return Base::template eval_sky<Float>(cos_theta, gamma, needed_sky_params, sky_rad.read(channel_idx, active));
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                      const Point2f &sample2,
                                      const Point2f &sample3,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        DRJIT_MARK_USED(time);
        DRJIT_MARK_USED(wavelength_sample);
        DRJIT_MARK_USED(sample2);
        DRJIT_MARK_USED(sample3);
        NotImplementedError()
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        DRJIT_MARK_USED(it);
        DRJIT_MARK_USED(sample);
        NotImplementedError()
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        DRJIT_MARK_USED(ds);
        NotImplementedError()
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        DRJIT_MARK_USED(si);
        DRJIT_MARK_USED(sample);
        DRJIT_MARK_USED(active);
        NotImplementedError()
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

    std::pair<UInt32, Float> sample_reuse_tgmm(const Float&, const Float&, const Mask&) const override {
        return std::make_pair(0, 0.f);
    }
    Point2f get_sun_angles(const Float&, const Mask&) const override {
        return 0.f;
    }
    std::pair<Vector4f, Vector4u> get_tgmm_data(const Float&, const Mask&) const override {
        return std::make_pair(0.f, 0);
    }

    struct Datasets {
        Vector3f sun_dir;
        RadLocal sky_rad;
        ParamsLocal sky_params;
    };


    /**
     * Computes the sky datasets associated with a given time
     *
     * @param time Time value in [0, 1]
     * @return The ``Datasets`` instance containing the sky parameters, radiance
     * and associated direction
     */
    Datasets compute_dataset(const Float& time) const {
        DateTimeRecord<Float> date_time = dr::zeros<DateTimeRecord<Float>>();
        date_time.year = m_start_date.year;
        date_time.month = m_start_date.month;

        Float day = time * DateTimeRecord<Float>::get_days_between(m_start_date, m_end_date, m_location);

        date_time.day = m_start_date.day + dr::floor2int<Int32>(day);
        date_time.hour = m_window_start_time + (m_window_end_time - m_window_start_time) * dr::fmod(day, 1.f);

        const auto [sun_elevation, sun_azimuth] = Base::sun_coordinates(date_time, m_location);
        const Float sun_eta = 0.5f * dr::Pi<Float> - sun_elevation;

        using ArrayXf = dr::DynamicArray<Float>;
        ArrayXf sky_rad = Base::template bezier_interp<ArrayXf>(m_sky_radiance, sun_eta);
        ArrayXf sky_params = Base::template bezier_interp<ArrayXf>(m_sky_params, sun_eta);

        Datasets result = {
            sph_to_dir(sun_elevation, sun_azimuth),
            RadLocal(sun_eta), ParamsLocal(sun_eta),
        };

        for (uint32_t channel_idx = 0; channel_idx < CHANNEL_COUNT; ++channel_idx) {
            result.sky_rad.write(channel_idx, sky_rad[channel_idx]);
            for (uint32_t param_idx = 0; param_idx < SKY_PARAMS; ++param_idx) {
                result.sky_params.write(channel_idx * SKY_PARAMS + param_idx, sky_params[channel_idx * SKY_PARAMS + param_idx]);
            }
        }

        return result;
    }


    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    Float m_window_start_time, m_window_end_time;
    DateTimeRecord<Float> m_start_date, m_end_date;
    LocationRecord<Float> m_location;

    // ========= Radiance parameters =========
    TensorXf m_sky_params;
    TensorXf m_sky_radiance;
    FloatStorage m_sun_radiance;

};

MI_EXPORT_PLUGIN(TimedSunskyEmitter)
NAMESPACE_END(mitsuba)
