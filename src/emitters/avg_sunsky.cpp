#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sunsky.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class AvgSunskyEmitter final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MI_IMPORT_TYPES(Scene, Shape, Texture)

    using EnvEmitter = Emitter<Float, Spectrum>;
    using FloatStorage = DynamicBuffer<Float>;
    using ScalarFloatStorage = DynamicBuffer<ScalarFloat>;
    using FullSpectrum = std::conditional_t<
        is_spectral_v<Spectrum>,
        unpolarized_spectrum_t<mitsuba::Spectrum<Float, WAVELENGTH_COUNT>>,
        unpolarized_spectrum_t<Spectrum>>;

    AvgSunskyEmitter(const Properties &props) : Base(props) {
        if constexpr (!(is_rgb_v<Spectrum> || is_spectral_v<Spectrum>))
            Throw("Unsupported spectrum type, can only render in Spectral or RGB modes!");

        constexpr bool IS_RGB = is_rgb_v<Spectrum>;

        init_from_props(props);

        // Extract albedo from texture
        ScalarFloatStorage albedo = extract_albedo(m_albedo);

        // ================= GET SKY RADIANCE =================
        m_sky_params_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyParams));
        m_sky_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SkyRadiance));

        // ================= GET SUN RADIANCE =================
        m_sun_rad_dataset = sunsky_array_from_file<Float64, Float>(
            path_to_dataset<IS_RGB>(Dataset::SunRadiance));

        // Only used in spectral mode since limb darkening is baked in the RGB dataset
        if constexpr (is_spectral_v<Spectrum>)
            m_sun_ld = sunsky_array_from_file<Float64, Float>(
                path_to_dataset<IS_RGB>(Dataset::SunLimbDarkening));

        // =============== ENVMAP INSTANTIATION ===============
        // Permute axis for envmap's convention
        ScalarMatrix4f permute_axis {
            ScalarVector4f{0, 0, -1, 0},
            ScalarVector4f{1, 0,  0, 0},
            ScalarVector4f{0, 1,  0, 0},
            ScalarVector4f{0, 0,  0, 1}
        };
        ScalarTransform4f envmap_transform { permute_axis };
        ref<Object> average_sky = compute_avg({256, 512});

        Properties envmap_props = Properties("envmap");
        envmap_props.set("bitmap", average_sky);
        envmap_props.set("to_world", envmap_transform);
        m_envmap = PluginManager::instance()->create_object<EnvEmitter>(envmap_props);

        m_flags = +EmitterFlags::Infinite | +EmitterFlags::SpatiallyVarying;
    }

    ~AvgSunskyEmitter() override {
        m_envmap.reset();
        delete m_bitmap_data;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("turbidity", m_turbidity, ParamFlags::NonDifferentiable);
        cb->put("sky_scale", m_sky_scale, ParamFlags::NonDifferentiable);
        cb->put("sun_scale", m_sun_scale, ParamFlags::NonDifferentiable);
        cb->put("albedo",    m_albedo,    ParamFlags::NonDifferentiable);
        cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
        cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
        cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);

        cb->put("to_world", m_to_world, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &) override {
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);
        if (dr::any(m_turbidity < 1.f || 10.f < m_turbidity))
            Log(Error, "Turbidity value %f is out of range [1, 10]", m_turbidity);
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!",
                dr::rad_to_deg(2 * m_sun_half_aperture));
        if (m_albedo->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        #define CHANGED(word) string::contains(keys, word)

        //bool changed_atmosphere = CHANGED("albedo") || CHANGED("turbidity");
        //bool changed_time_record = !keys.empty() && m_active_record && (
        //        CHANGED("timezone") || CHANGED("year") ||
        //        CHANGED("day") || CHANGED("month") || CHANGED("hour") ||
        //        CHANGED("minute") || CHANGED("second") || CHANGED("latitude") ||
        //        CHANGED("longitude")
        //    );
        //bool changed_sun_dir = (!m_active_record && CHANGED("sun_direction")) || changed_time_record;


        #undef CHANGED
    }

    MI_INLINE void set_scene(const Scene *scene) override {
        m_envmap->set_scene(scene);
    }

    MI_INLINE Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_envmap->eval(si, active);
    }

    MI_INLINE std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                      const Point2f &sample2,
                                      const Point2f &sample3,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        return m_envmap->sample_ray(time, wavelength_sample, sample2, sample3, active);
    }

    MI_INLINE std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        return m_envmap->sample_direction(it, sample, active);
    }

    MI_INLINE Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_envmap->pdf_direction(it, ds, active);
    }

    MI_INLINE Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        return m_envmap->eval_direction(it, ds, active);
    }

    MI_INLINE std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_envmap->sample_wavelengths(si, sample, active);
    }

    MI_INLINE std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f & sample,
                    Mask active) const override {
        return m_envmap->sample_position(time, sample, active);
    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunskyEmitter[" << std::endl
            << "  turbidity = " << string::indent(m_turbidity) << std::endl
            << "  sky_scale = " << string::indent(m_sky_scale) << std::endl
            << "  sun_scale = " << string::indent(m_sun_scale) << std::endl
            << "  albedo = " << string::indent(m_albedo) << std::endl
            << "  sun aperture (°) = " << string::indent(dr::rad_to_deg(2 * m_sun_half_aperture))
            << "  location = " << m_location.to_string()
            << "]" << std::endl;
        return oss.str();
    }

    MI_DECLARE_CLASS(AvgSunskyEmitter)

private:

    DateTimeRecord<FloatStorage> compute_times(
        ScalarFloat start_time, ScalarFloat end_time,
        ScalarUInt32 year, ScalarUInt32 day_resolution) const {
        using UInt32Storage = DynamicBuffer<UInt32>;

        ScalarFloat delta_hour = (end_time - start_time) / static_cast<ScalarFloat>(day_resolution);

        bool is_leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        ScalarUInt32 nb_samples = day_resolution * (is_leap_year ? 366 : 365);
        UInt32Storage index = dr::arange<UInt32Storage>(nb_samples);

        UInt32Storage days = index / day_resolution;
        FloatStorage  hours = start_time + delta_hour * (index % day_resolution);
        DateTimeRecord<FloatStorage> times = dr::zeros<DateTimeRecord<FloatStorage>>(nb_samples);

        times.year = year;
        times.day = days;
        times.hour = hours;

        return times;
    }


    ref<Object> compute_avg(const ScalarPoint2i& resolution) {
        ScalarFloatStorage albedo = extract_albedo(m_albedo);
        m_bitmap_data = new float[resolution.x() * resolution.y() * 3];
        memset(m_bitmap_data, 0, resolution.x() * resolution.y() * 3 * sizeof(float));

        //DateTimeRecord<FloatStorage> times = compute_times(8, 17, 2025, 2);
        //                             times = dr::migrate(times, AllocType::Host);
        //if constexpr (dr::is_jit_v<Float>)
        //    dr::sync_thread();

        const uint32_t nb_samples = 300;
        const float start_hour = 8, end_hour = 17;
        const float dt = (end_hour - start_hour) / nb_samples;


        DateTimeRecord<ScalarFloat> time;
        for (uint32_t i = 0; i < nb_samples; ++i) {
            time.hour = start_hour + i * dt;

            const auto [sun_elevation, sun_azimuth] = sun_coordinates(time, m_location);
            const ScalarFloat sun_eta = 0.5f * dr::Pi<ScalarFloat> - sun_elevation;
            const ScalarVector3f sun_dir = sph_to_dir(sun_elevation, sun_azimuth);

            ScalarFloatStorage sun_radiance = sun_params<SUN_DATASET_SIZE>(m_sun_rad_dataset, m_turbidity);
            ScalarFloatStorage sky_params = sky_radiance_params<SKY_DATASET_SIZE>(m_sky_params_dataset, albedo, m_turbidity, sun_eta);
            ScalarFloatStorage sky_radiance = sky_radiance_params<SKY_DATASET_RAD_SIZE>(m_sky_rad_dataset, albedo, m_turbidity, sun_eta);

            // Iterate over top half of the image
            for (int32_t pixel_idx = 0; pixel_idx < resolution.x() * resolution.y() / 2; ++pixel_idx) {
                ScalarFloat pixel_u = static_cast<ScalarFloat>(pixel_idx % resolution.x()) / static_cast<ScalarFloat>(resolution.x());
                ScalarFloat pixel_v = static_cast<ScalarFloat>(pixel_idx / resolution.x()) / static_cast<ScalarFloat>(resolution.y());

                ScalarVector3f ray_dir = sph_to_dir(
                    dr::Pi<ScalarFloat> * pixel_v,
                    dr::TwoPi<ScalarFloat> * pixel_u
                );
                ScalarFloat gamma = dr::unit_angle(ray_dir, sun_dir);

                ScalarColor3f res = 0.;
                res += eval_sky<ScalarColor3f>({0, 1, 2}, ray_dir.z(), gamma, sky_params, sky_radiance);
                res += (gamma < m_sun_half_aperture ? 1.f : 0.f) * SPEC_TO_RGB_SUN_CONV *
                    eval_sun<ScalarColor3f>({0, 1, 2}, ray_dir.z(), gamma, sun_radiance, m_sun_half_aperture) * get_area_ratio(m_sun_half_aperture);
                res *= static_cast<ScalarFloat>(MI_CIE_Y_NORMALIZATION) / static_cast<ScalarFloat>(nb_samples);

                m_bitmap_data[3 * pixel_idx + 0] += res.r();
                m_bitmap_data[3 * pixel_idx + 1] += res.g();
                m_bitmap_data[3 * pixel_idx + 2] += res.b();

            }

        }

        return new Bitmap(Bitmap::PixelFormat::RGB, Struct::Type::Float32, resolution, 3, {}, reinterpret_cast<uint8_t *>(m_bitmap_data));
    } 


    void init_from_props(const Properties& props) {
        m_sun_scale = props.get<ScalarFloat>("sun_scale", 1.f);
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);

        m_sky_scale = props.get<ScalarFloat>("sky_scale", 1.f);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);

        ScalarFloat turb = props.get<ScalarFloat>("turbidity", 3.f);
        if (turb < 1.f || 10.f < turb)
            Log(Error, "Turbidity value %f is out of range [1, 10]", turb);
        m_turbidity = turb;
        dr::make_opaque(m_turbidity);

        m_sun_half_aperture = dr::deg_to_rad(0.5f * (float) props.get<ScalarFloat>("sun_aperture", 0.5358));
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!", dr::rad_to_deg(2 * m_sun_half_aperture));

        m_albedo = props.get_texture<Texture>("albedo", 0.3f);
        if (m_albedo->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        m_location.latitude  = props.get<ScalarFloat>("latitude", 35.6894f);
        m_location.longitude = props.get<ScalarFloat>("longitude", 139.6917f);
        m_location.timezone  = props.get<ScalarFloat>("timezone", 9);

        dr::make_opaque(m_location.latitude, m_location.longitude, m_location.timezone);
    }

    /**
     * \brief Extract the albedo values for the required wavelengths/channels
     *
     * \param albedo_tex
     *      Texture to extract the albedo from
     * \return
     *      The buffer with the extracted albedo values for the needed wavelengths/channels
     */
    ScalarFloatStorage extract_albedo(const ref<Texture>& albedo_tex) const {
        using FloatStorage = DynamicBuffer<Float>;
        FloatStorage albedo = dr::zeros<FloatStorage>(CHANNEL_COUNT);
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        if constexpr (is_rgb_v<Spectrum>) {
            albedo = dr::ravel(albedo_tex->eval(si));

        } else if constexpr (dr::is_array_v<Float> && is_spectral_v<Spectrum>) {
            si.wavelengths = dr::load<FloatStorage>(WAVELENGTHS<ScalarFloat>, CHANNEL_COUNT);
            albedo = albedo_tex->eval(si)[0];
        } else if (!dr::is_array_v<Float> && is_spectral_v<Spectrum>) {
            for (ScalarUInt32 i = 0; i < CHANNEL_COUNT; ++i) {
                if constexpr (!is_monochromatic_v<Spectrum>)
                    si.wavelengths = WAVELENGTHS<ScalarFloat>[i];
                dr::scatter(albedo, albedo_tex->eval(si)[0], (UInt32) i);
            }
        }

        if (dr::any(albedo < 0.f || albedo > 1.f))
            Log(Error, "Albedo values must be in [0, 1], got: %f", albedo);

        return albedo;
    }

    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    /// Number of channels used in the skylight model
    static constexpr uint32_t CHANNEL_COUNT =
        is_spectral_v<Spectrum> ?
        WAVELENGTH_COUNT :
        (is_monochromatic_v<Spectrum> ? 1 : 3);

    // Dataset sizes
    static constexpr uint32_t SKY_DATASET_SIZE =
        TURBITDITY_LVLS * ALBEDO_LVLS * SKY_CTRL_PTS * CHANNEL_COUNT *
        SKY_PARAMS;
    static constexpr uint32_t SKY_DATASET_RAD_SIZE =
        TURBITDITY_LVLS * ALBEDO_LVLS * SKY_CTRL_PTS * CHANNEL_COUNT;
    static constexpr uint32_t SUN_DATASET_SIZE =
        TURBITDITY_LVLS * CHANNEL_COUNT * SUN_SEGMENTS * SUN_CTRL_PTS *
        (is_spectral_v<Spectrum> ? 1 : SUN_LD_PARAMS);
    static constexpr uint32_t TGMM_DATA_SIZE =
        (TURBITDITY_LVLS - 1) * ELEVATION_CTRL_PTS * TGMM_COMPONENTS *
        TGMM_GAUSSIAN_PARAMS;

    ref<EnvEmitter> m_envmap;
    float* m_bitmap_data;

    // ========= Common parameters =========
    ScalarFloat m_turbidity;
    ScalarFloat m_sky_scale;
    ScalarFloat m_sun_scale;
    ref<Texture> m_albedo;

    // ========= Sun parameters =========

    ScalarFloat m_sun_half_aperture;
    /// Indicates if the plugin was initialized with a location/time record
    LocationRecord<ScalarFloat> m_location;

    // Permanent datasets loaded from files/memory
    ScalarFloatStorage m_sky_rad_dataset;
    ScalarFloatStorage m_sky_params_dataset;
    ScalarFloatStorage m_sun_ld; // Not initialized in RGB mode
    ScalarFloatStorage m_sun_rad_dataset;
};

MI_EXPORT_PLUGIN(AvgSunskyEmitter)
NAMESPACE_END(mitsuba)
